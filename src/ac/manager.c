#include "manager.h"

#include "stdbool.h"

#include "re/file.h"
#include "re/path.h"

#include "global.h"
#include "lexer.h"

static bool try_get_file_content(const char* filepath, dstr* content);

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
}

void ac_options_destroy(ac_options* o)
{
    o->step = ac_compilation_step_NONE;

    darrT_destroy(&o->files);

    dstr_destroy(&o->config_file_memory);
    darrT_destroy(&o->config_file_args);
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
}

void ac_manager_destroy(ac_manager* m)
{
    ht_destroy(&m->identifiers);
    ht_destroy(&m->literals);

    ac_allocator_arena_destroy(&m->identifiers_arena);
    ac_allocator_arena_destroy(&m->ast_arena);

    if (!dstr_empty(&m->source_file.content))
    {
        dstr_destroy(&m->source_file.content);
    }
}

ac_source_file* ac_manager_load_content(ac_manager* m, const char* filepath)
{
    if (m->source_file.content.data != 0)
    {
        ac_report_error("Internal error: @TEMP we can only load a single file per manager instance.");
        return 0;
    }

    if (!re_file_exists_str(filepath))
    {
        ac_report_error("File '%s' does not exist.", filepath);
        return 0;
    }

    dstr content;
    dstr_init(&content);

    if (try_get_file_content(filepath, &content))
    {
        m->source_file.filepath = filepath;
        m->source_file.content = content;

        return &m->source_file;
    }
    else
    {
        ac_report_error("Could not load file into memory.");
        return 0;
    }
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

static bool try_get_file_content(const char* filepath, dstr* content)
{
    if (!re_file_exists_str(filepath)) {
        ac_report_error("File does not exist '%s'.\n", filepath);
        return false;
    }

    if (!re_file_open_and_read(content, filepath))
    {
        ac_report_error("Could not open '%s'\n", filepath);
        return false;
    }
   
    if (content->size == 0) {
        ac_report_warning("Empty file '%s'.\n", filepath);
        return true;
    }

    return true;
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