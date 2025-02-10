#include "manager.h"

#ifndef _WIN32
#include <sys/stat.h> /* stat */
#include <sys/mman.h> /* mmap, unmap */
#endif

#include "stdbool.h"

#include "re/file.h"
#include "re/path.h"

#include "global.h"
#include "lexer.h"

typedef struct source_file source_file;
struct source_file {
#if _WIN32
    HANDLE handle;
    FILE_ID_INFO info;
#else
    int fd;
    struct stat st;
#endif
    strv filepath;  /* NOTE: View to a null terminated string. */
    strv content;   /* NOTE: View to a null terminated string. */
};

static bool load_source_file(ac_manager* m, char* filepath, source_file* result);
static strv allocate_filepath(ac_manager* m, const char* filepath);

/* mmap the file or get the already mmapped file.
   'filepath' is only used to report more meaningful errors. */
static bool mmap_or_get_source_file(ac_manager* m, source_file* source_file, const char* filepath);
/* Close file handle and unmap the file. */
static bool unmap_source_file(source_file* source_file);

/* Comparer for darr_map. */
static darr_bool source_file_less_predicate(source_file* left, source_file* right);

static ht_hash_t identifier_hash(ac_ident_holder* i);                               /* For hash table. */
static ht_bool identifiers_are_same(ac_ident_holder* left, ac_ident_holder* right); /* For hash table. */
static void swap_identifiers(ac_ident_holder* left, ac_ident_holder* right);        /* For hash table. */
static ht_hash_t literal_hash(strv* sv);                                /* For hash table. */
static ht_bool literals_are_same(strv* left, strv* right);              /* For hash table. */
static void swap_literals(strv* left, strv* right);                     /* For hash table. */

void ac_options_init_default(ac_options* o)
{
    memset(o, 0, sizeof(ac_options));

    o->step = ac_compilation_step_ALL;

    darrT_init(&o->files);
    o->output_extension = strv_make_from_str(".g.c");
    dstr_init(&o->config_file_memory);
    darrT_init(&o->config_file_args);

    o->global.colored_output = true;
    o->global.display_surrounding_lines = true;

    o->debug_parser = false;

    darrT_init(&o->user_includes);
    darrT_init(&o->system_includes);
}

void ac_options_destroy(ac_options* o)
{
    o->step = ac_compilation_step_NONE;

    darrT_destroy(&o->files);

    dstr_destroy(&o->config_file_memory);
    darrT_destroy(&o->config_file_args);

    darrT_destroy(&o->user_includes);
    darrT_destroy(&o->system_includes);
}

void ac_manager_init(ac_manager* m, ac_options* o)
{
    AC_ASSERT(o);
    memset(m, 0, sizeof(ac_manager));

    ac_allocator_arena_init(&m->ast_arena, 16 * 1024);
    ac_allocator_arena_init(&m->identifiers_arena, 16 * 1024);

    ht_init(&m->identifiers,
        sizeof(ac_ident_holder),
        (ht_hash_function_t)identifier_hash,
        (ht_predicate_t)identifiers_are_same,
        (ht_swap_function_t)swap_identifiers,
        0);

    ht_init(&m->literals,
        sizeof(strv),
        (ht_hash_function_t)literal_hash,
        (ht_predicate_t)literals_are_same,
        (ht_swap_function_t)swap_literals,
        0);

    m->options = *o;
    global_options = o->global;

    ac_token_info* infos = ac_token_infos();
    /* Register keywords and known tokens. */
    for (int i = 0; i < ac_token_type_COUNT; i += 1)
    {
        ac_token_info* info = infos + i;
        if (info->is_supported && ac_token_is_keyword_or_identifier(info->type))
        {
            ac_register_known_identifier(m, &info->ident, info->type);
        }
    }

    darr_map_init(&m->opened_files, sizeof(source_file), (darr_predicate_t)source_file_less_predicate);

#if _WIN32
    darrT_init(&m->wchars);
#endif
}

void ac_manager_destroy(ac_manager* m)
{
    ht_destroy(&m->identifiers);
    ht_destroy(&m->literals);

    ac_allocator_arena_destroy(&m->identifiers_arena);
    ac_allocator_arena_destroy(&m->ast_arena);

    /* Release all opened files. */
    for (int i = 0; i < darr_map_size(&m->opened_files); i += 1)
    {
        source_file* src_file = darr_ptr(&m->opened_files.arr,i);
        bool unmmapped = unmap_source_file(src_file);
        AC_ASSERT(unmmapped);
    }

    darr_map_destroy(&m->opened_files);
#if _WIN32
    darrT_destroy(&m->wchars);
#endif
}

bool ac_manager_load_content(ac_manager* m, char* filepath, ac_source_file* result)
{
    if (!re_file_exists_str(filepath))
    {
        ac_report_error("file '%s' does not exist", filepath);
        return false;
    }

    source_file src_file;
    if (!load_source_file(m, filepath, &src_file))
    {
        ac_report_error("could not load file '%s' into memory", filepath);
        return false;
    }

    if (src_file.content.size == 0)
    {
        ac_report_warning("empty file '%s'", filepath);
    }

    result->filepath = src_file.filepath;
    result->content = src_file.content;

    return true;
}

ac_ident_holder ac_create_or_reuse_identifier(ac_manager* m, strv ident)
{
    return ac_create_or_reuse_identifier_h(m, ident, ac_djb2_hash((char*)ident.data, ident.size));
}

ac_ident_holder ac_create_or_reuse_identifier_h(ac_manager* m, strv ident_text, size_t hash)
{
    ac_ident i;
    i.text = ident_text;
    ac_ident_holder ident_to_find = {&i};
    ac_ident_holder* result_ident = (ac_ident_holder*)ht_get_item_h(&m->identifiers, &ident_to_find, hash);

    /* If the identifier is new, a new entry is created. */
    if (result_ident == NULL)
    {
        ac_ident* i = ac_allocator_allocate(&m->identifiers_arena.allocator, sizeof(ac_ident));
        memset(i, 0, sizeof(ac_ident));

        i->text.data = ac_allocator_allocate(&m->identifiers_arena.allocator, ident_text.size);
        memcpy((char*)i->text.data, ident_text.data, ident_text.size);

        i->text.size = ident_text.size;
        ac_ident_holder holder = { .ident = i, .token_type = ac_token_type_IDENTIFIER };
        ht_insert_h(&m->identifiers, &holder, hash);
        return holder;
    }
    else
    {
        return *result_ident;
    }
}

void ac_register_known_identifier(ac_manager* m, ac_ident* id, /* enum ac_token_type */ size_t token_type)
{
    ac_ident_holder holder = { .ident = id, .token_type = token_type };
  
    ht_hash_t h = ac_djb2_hash((char*)id->text.data, id->text.size);
    ht_insert_h(&m->identifiers, &holder, h);
}

strv ac_create_or_reuse_literal(ac_manager* m, strv literal_text)
{
    return ac_create_or_reuse_literal_h(m, literal_text, ac_djb2_hash((char*)literal_text.data, literal_text.size));
}

strv ac_create_or_reuse_literal_h(ac_manager* m, strv literal_text, size_t hash)
{
    strv* result_literal = (strv*)ht_get_item_h(&m->literals, &literal_text, hash);

    /* If the identifier is new, a new entry is created. */
    if (result_literal == NULL)
    {
        strv v;
        v.data = (const char*)ac_allocator_allocate(&m->identifiers_arena.allocator, literal_text.size);
        memcpy((char*)v.data, literal_text.data, literal_text.size);
        v.size = literal_text.size;
        ht_insert_h(&m->literals, &v, hash);
        return v;
    }
    else
    {
        return *result_literal;
    }
}

static bool load_source_file(ac_manager* m, char* filepath, source_file* result)
{
    if (!re_file_exists_str(filepath))
    {
        ac_report_error("file does not exist '%s'", filepath);
        return false;
    }

    /* mmap the file content from the id. */
    if (!mmap_or_get_source_file(m, result, filepath))
    {
        ac_report_error("could not open or read '%s'", filepath);
        return false;
    }
    
    darr_map_set(&m->opened_files, result);

    return true;
}

static strv allocate_filepath(ac_manager* m, const char* filepath)
{
    size_t filepath_size = strlen(filepath);
    void* filepath_memory = ac_allocator_allocate(&m->identifiers_arena.allocator, filepath_size + 1); /* +1 for null-termating char. */
    strncpy(filepath_memory, filepath, filepath_size);
    return strv_make_from(filepath_memory, filepath_size);
}

#ifdef _WIN32
static int convert_utf8_to_wchar(ac_manager* m, const char* chars)
{
    /* Get length of converted string. */
    size_t len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, chars, -1, NULL, 0);
    int result = 0;

    /* Ensure the buffer is big enough. */
    darrT_resize(&m->wchars, len + 1);

    if (len != 0) {
        result = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, chars, -1, m->wchars.arr.data, len); /* Do conversion */
    }
    /* Ensure that the buffer ends with a null terminated wchar. */
    darrT_set(&m->wchars, len, L'0');

    return result;
}

static HANDLE handle_from_filepath(wchar_t* filepath) {

    return CreateFileW(filepath,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS,
        NULL);
}
#endif

static bool mmap_or_get_source_file(ac_manager* m, source_file* src_file, const char* filepath)
{
#ifdef _WIN32

    if (!convert_utf8_to_wchar(m, filepath))
    {
        ac_report_error("could not convert path to wchar_t string: %s", filepath);
        return false;
    }

    FILE_ID_INFO info = { 0 };
    HANDLE handle = handle_from_filepath(m->wchars.arr.data);
    if (handle == INVALID_HANDLE_VALUE
        || !GetFileInformationByHandleEx(handle, FileIdInfo, &info, sizeof(info)))
    {
        ac_report_error("GetFileInformationByHandleEx failed for file: %s", filepath);
        return false;
    }

    source_file lookup = {
        .handle = handle,
        .info = info
    };

    /* Retrieve the content if it's already opened. */
    if (darr_map_get(&m->opened_files, &lookup, src_file))
    {
        return true;
    }

    src_file->filepath = allocate_filepath(m, filepath);

    src_file->handle = handle;
    src_file->info = info;

    /* Get file size */
    LARGE_INTEGER file_size;
    if (!GetFileSizeEx(src_file->handle, &file_size))
    {
        ac_report_error("GetFileSizeEx failed for file: %s", filepath);
        return false;
    }

    /* Handle zero size file as it would make CreateFileMapping to fail. */
    if (file_size.QuadPart == 0)
    {
        src_file->content = strv_make_from_str("");
        return true;
    }

    HANDLE mapping_handle = CreateFileMappingW(src_file->handle, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!mapping_handle)
    {
        ac_report_error("CreateFileMapping failed for file: %s", filepath);
        return false;
    }

    char* memory_ptr = (char*)MapViewOfFile(mapping_handle, FILE_MAP_READ, 0, 0, 0);
    CloseHandle(mapping_handle);

    src_file->content.data = memory_ptr;
    src_file->content.size = (size_t)file_size.QuadPart;
    return true;

#else

    int fd = open(filepath, O_RDONLY | O_NDELAY, 0644);

    if (fd < 0)
    {
        ac_report_internal_error("open failed for file: %s", filepath);
        return false;
    }

    src_file->fd = fd;

    struct stat st;
    if (fstat(fd, &st) != 0)
    {
        ac_report_internal_error("fstat failed for file: %s", filepath);
        return false;
    }

    src_file->st = st;

    source_file lookup = {
        .fd = fd,
        .st = st
    };

    /* Retrieve the content if it's already opened. */
    if (darr_map_get(&m->opened_files, &lookup, src_file))
    {
        return true;
    }

    src_file->filepath = allocate_filepath(m, filepath);

    /* Handle zero size file as it would make mmap to fail. */
    if (st.st_size == 0)
    {
        src_file->content = strv_make_from_str("");
        return true;
    }

    /* mmap */
    char* memory_ptr = (char*)mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);

    if (memory_ptr == MAP_FAILED)
    {
        ac_report_internal_error("mmap failed for file: %s", filepath);
        return false;
    }

    src_file->content.data = memory_ptr;
    src_file->content.size = (size_t)st.st_size;

    return true;
#endif
}

static bool unmap_source_file(source_file* source_file)
{
#if _WIN32
    CloseHandle(source_file->handle);

    if (source_file->content.size != 0)
    {
        return UnmapViewOfFile(source_file->content.data);
    }
    return true;
#else
    close(source_file->fd);
    if (source_file->content.size != 0)
    {
        return munmap((void*)source_file->content.data, source_file->content.size) == 0;
    }
    return true;
#endif
}

static darr_bool source_file_less_predicate(source_file* left, source_file* right)
{
#if _WIN32
    return memcmp(&left->info, &right->info, sizeof(left->info)) < 0;
#else
    return left->st.st_dev < right->st.st_dev
        || left->st.st_ino < right->st.st_ino
        || left->st.st_size < right->st.st_size
        || left->st.st_mtime < right->st.st_mtime;
#endif
}

static ht_hash_t identifier_hash(ac_ident_holder* i)
{
    return ac_djb2_hash((char*)i->ident->text.data, i->ident->text.size);
}

static ht_bool identifiers_are_same(ac_ident_holder* left, ac_ident_holder* right)
{
    return strv_equals(left->ident->text, right->ident->text);
}

static void swap_identifiers(ac_ident_holder* left, ac_ident_holder* right)
{
    ac_ident_holder tmp;
    tmp = *left;
    *left = *right;
    *right = tmp;
}

static ht_hash_t literal_hash(strv* literal)
{
    return ac_djb2_hash((char*)literal->data, literal->size);
}

static ht_bool literals_are_same(strv* left, strv* right)
{
    return strv_equals(*left, *right);
}

static void swap_literals(strv* left, strv* right)
{
    strv tmp;
    tmp = *left;
    *left = *right;
    *right = tmp;
}