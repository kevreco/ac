#include "compiler.h"

#include <re/dstr.h>

#include "parser_c.h"
#include "converter_c.h"

void ac_compiler_options_init_default(struct ac_compiler_options* o)
{
    o->step = ac_compilation_step_ALL;
}

void ac_compiler_init(struct ac_compiler* c, struct ac_compiler_options options)
{
    memset(c, 0, sizeof(struct ac_compiler));

    c->options = options;

    ac_manager_init(&c->mgr);
}

void ac_compiler_destroy(struct ac_compiler* c)
{
    ac_manager_destroy(&c->mgr);
}

bool ac_compiler_compile(struct ac_compiler* c, const char* source_file, const char* c_file)
{
    /* load file into memory */
    struct ac_source_file* source = ac_manager_load_content(&c->mgr, source_file);

    if (!source)
    {
        return 0;
    }

    /*** Parsing ***/
    
    struct ac_parser_c parser;
    ac_parser_c_init(&parser, &c->mgr);

    if (!ac_parser_c_parse(&parser,
        source->content.data,
        source->content.size, 
        source->filepath)
        )
    {
        return false;
    }
    
    /*** Type/semantic check ***/

    if ((c->options.step & ac_compilation_step_SEMANTIC) == 0)
    {
        return true;
    }
    
    /* @TODO */
    
    /*** Generate ***/

    if ((c->options.step & ac_compilation_step_GENERATING) == 0)
    {
        return true;
    }


    struct ac_converter_c conv;

    ac_converter_c_init(&conv, &c->mgr);

    ac_converter_c_convert(&conv, c_file);

    ac_converter_c_destroy(&conv);

    return true;
}