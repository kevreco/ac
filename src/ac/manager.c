#include "manager.h"

#include "stdbool.h"

#include "re/file.h"
#include "re/path.h"

#include "global.h"

static bool try_get_file_content(const char* filepath, dstr* content);

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

    m->options = *o;
    global_options = o->global;
}

void ac_manager_destroy(ac_manager* m)
{
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
        ac_report_error("w_manager_load_content: file '%s' does not exist.", filepath);
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