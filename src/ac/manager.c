#include "manager.h"

#include "stdbool.h"

#include "re/file.h"
#include "re/path.h"

#include "global.h"

static bool try_get_file_content(const char* filepath, dstr* content);

static ht_hash_t identifier_hash(strv* sv);                   /* For the hash table. */
static ht_bool identifiers_are_same(strv* left, strv* right); /* For the hash table. */
static void swap_identifiers(strv* left, strv* right);        /* For the hash table. */

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
        sizeof(strv),
        (ht_hash_function_t)identifier_hash,
        (ht_predicate_t)identifiers_are_same,
        (ht_swap_function_t)swap_identifiers,
        0);

    m->options = *o;
    global_options = o->global;
}

void ac_manager_destroy(ac_manager* m)
{
    ht_destroy(&m->identifiers);
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

static ht_hash_t identifier_hash(strv* sv)
{
    return ac_djb2_hash((char*)sv->data, sv->size);
}

static ht_bool identifiers_are_same(strv* left, strv* right)
{
    return strv_equals(*left, *right);
}

static void swap_identifiers(strv* left, strv* right)
{
    strv tmp;
    tmp = *left;
    *left = *right;
    *right = tmp;
}