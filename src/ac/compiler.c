#include "compiler.h"

#include <re/darr.h>
#include <re/dstr.h>
#include <re/path.h>

#include "global.h"
#include "parser_c.h"
#include "converter_c.h"

void ac_compiler_options_init_default(ac_compiler_options* o)
{
    o->step = ac_compilation_step_ALL;

    darrT_init(&o->files);
    o->output_extension = strv_make_from_str(".g.c");
    dstr_init(&o->config_file_memory);
    darrT_init(&o->config_file_args);
}

void ac_compiler_options_destroy(ac_compiler_options* o)
{
    o->step = ac_compilation_step_NONE;

    darrT_destroy(&o->files);

    dstr_destroy(&o->config_file_memory);
    darrT_destroy(&o->config_file_args);
}

void ac_compiler_init(ac_compiler* c, ac_compiler_options options)
{
    memset(c, 0, sizeof(ac_compiler));

    c->options = options;

    ac_manager_init(&c->mgr);
}

void ac_compiler_destroy(ac_compiler* c)
{
    ac_manager_destroy(&c->mgr);
}

bool ac_compiler_compile(ac_compiler* c)
{
    AC_ASSERT(c->options.files.darr.size > 0);
    AC_ASSERT(c->options.files.darr.size == 1 && "Not supported yet. Cannot compile multiple files.");
    
    /* Load file into memory. */
    const char* source_file = darrT_at(&c->options.files, 0);
    ac_source_file* source = ac_manager_load_content(&c->mgr, source_file);

    if (!source)
    {
        return 0;
    }

    /*** Parsing ***/
    
    ac_parser_c parser;
    ac_parser_c_init(&parser, &c->mgr);

    if (!ac_parser_c_parse(&parser,
        source->content.data,
        source->content.size, 
        source->filepath)
        )
    {
        return false;
    }
    
    /*** Type/semantic check - @TODO ***/

    if ((c->options.step & ac_compilation_step_SEMANTIC) == 0)
    {
        return true;
    }
    
    /*** Generate ***/

    if ((c->options.step & ac_compilation_step_GENERATE) == 0)
    {
        return true;
    }

    struct ac_converter_c conv;

    ac_converter_c_init(&conv, &c->mgr);

    dstr output_file;
    dstr_init(&output_file);
    dstr_assign_str(&output_file, source_file);
    re_path_replace_extension(&output_file, c->options.output_extension);

    ac_converter_c_convert(&conv, output_file.data);

    ac_converter_c_destroy(&conv);

    dstr_destroy(&output_file);

    return true;
}