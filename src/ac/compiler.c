#include "compiler.h"

#include "re_lib.h"

#include "global.h"
#include "parser_c.h"
#include "converter_c.h"

static ac_options* options(ac_compiler* c);

void ac_compiler_init(ac_compiler* c, ac_options* options)
{
    AC_ASSERT(c);
    AC_ASSERT(options);
    memset(c, 0, sizeof(ac_compiler));

    ac_manager_init(&c->mgr, options);
}

void ac_compiler_destroy(ac_compiler* c)
{
    ac_manager_destroy(&c->mgr);
}

bool ac_compiler_compile(ac_compiler* c)
{
    AC_ASSERT(darrT_size(&(options(c)->files)));
    AC_ASSERT(darrT_size(&(options(c)->files)) == 1 && "Not supported yet. Cannot compile multiple files.");
    
    /* Load file into memory. */
    const char* source_file = darrT_at(&options(c)->files, 0);

    ac_source_file* source = ac_manager_load_content(&c->mgr, source_file);

    if (!source)
    {
        return 0;
    }

    /*** Preprocess only ***/
    if (options(c)->preprocess)
    {
        ac_pp pp;
        ac_pp_init(&pp, &c->mgr, dstr_to_strv(&source->content), source->filepath);

        /* Print preprocessed tokens in the standard output. */
        const ac_token* token = NULL;
        while ((token = ac_pp_goto_next(&pp)) != NULL
            && token->type != ac_token_type_EOF)
        {
            ac_token_fprint(stdout, *token);
        }

        ac_pp_destroy(&pp);
        return true;
    }
    
    /*** Parsing ***/

    ac_parser_c parser;
    ac_parser_c_init(&parser, &c->mgr, dstr_to_strv(&source->content), source->filepath);

    if (!ac_parser_c_parse(&parser))
    {
        ac_parser_c_destroy(&parser);
        return false;
    }
    ac_parser_c_destroy(&parser);

    /*** Type/semantic check - @TODO ***/

    if ((options(c)->step & ac_compilation_step_SEMANTIC) == 0)
    {
        return true;
    }
    
    /*** Generate ***/

    if ((options(c)->step & ac_compilation_step_GENERATE) == 0)
    {
        return true;
    }

    ac_converter_c conv;

    ac_converter_c_init(&conv, &c->mgr);

    dstr output_file;
    dstr_init(&output_file);
    dstr_assign_str(&output_file, source_file);
    re_path_replace_extension(&output_file, options(c)->output_extension);

    ac_converter_c_convert(&conv, output_file.data);

    ac_converter_c_destroy(&conv);

    dstr_destroy(&output_file);

    return true;
}

static ac_options* options(ac_compiler* c)
{
    return &c->mgr.options;
}