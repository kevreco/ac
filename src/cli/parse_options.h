#ifndef AC_PARSE_OPTIONS_H
#define AC_PARSE_OPTIONS_H

#include <ac/global.h>
#include <ac/compiler.h>
#include <ac/re_lib.h>

#define STRV(x)\
    { sizeof(x)-1 , (const char*)(x)  }

#ifdef __cplusplus
extern "C" {
#endif

static const struct options {
    strv colored_output;
    strv debug_parser;
    strv display_surrounding_lines;
    strv output_extension;
    strv parse_only;
} cli_options = {
    .colored_output = STRV("--colored-output"),
    .debug_parser     = STRV("--debug-parser"),
    .display_surrounding_lines = STRV("--display-surrounding-lines"),
    .output_extension = STRV("--output-extension"),
    .parse_only       = STRV("--parse-only")
};
/*
    We don't treat the --option-file option the same way.
    We need to load the option file before everything else.
    All following options will override the once loaded from the file.
*/
static strv option_file = STRV("--option-file");

/* Return current argument and go to the next one. */
char*
pop_args(int* argc, char*** argv)
{
    char* current_arg = **argv;
    (*argv) += 1;
    (*argc) -= 1;
    return current_arg;
}

bool
arg_equals(const char* arg, strv sv)
{
    return strv_equals_str(sv, arg);
}

bool
parse_from_arguments(ac_options* o, int* argc, char*** argv)
{
    do {
        const char* arg = pop_args(argc, argv);
        
        if (arg_equals(arg, cli_options.colored_output))
        {
            o->global.colored_output = true;
        }
        else if (arg_equals(arg, cli_options.debug_parser))
        {
            /* @FIXME it's already true by default. We need to read "true" or "false" from the input. */
            o->debug_parser = true;
        }
        else if (arg_equals(arg, cli_options.display_surrounding_lines))
        {
            /* @FIXME it's already true by default. We need to read "true" or "false" from the input. */
            o->global.display_surrounding_lines = true;
        }
        else if (arg_equals(arg, cli_options.output_extension))
        {
            o->output_extension = strv_make_from_str(arg);
        }
        else if (arg_equals(arg, cli_options.parse_only))
        {
            o->step = ac_compilation_step_PARSE;
        }
        /* Ignore --option-file since it has been handled at this point */
        else if (arg_equals(arg, option_file))
        {
            /* Ignore option file value */
            pop_args(argc, argv);
        }
        /* Handle unknown options */
        else if (arg[0] == '-' && arg[1] == '-')
        {
            ac_report_error("Unknown flag: %s", arg);
            return false;
        }
        /* Non-option args are assume to be files. */
        else
        {
            if (!re_file_exists_str(arg))
            {
                ac_report_error("File does not exists : %s", arg);
                return false;
            }

            darrT_push_back(&o->files, arg);
        }
    } while (*argc);

    return true;
}

void
try_parse_from_file(ac_options* o, int argc, char** argv)
{
    char* option_file_path = NULL;
    char* arg;
    while (argc > 0 && ((arg = pop_args(&argc, &argv)) != NULL))
    {
        if (strv_equals_str(option_file, arg))
        {
            if (argc == 0)
            {
                ac_report_error("%s option expect a following value.", option_file.data);
            }

            option_file_path = pop_args(&argc, &argv);
            break;
        }
    }

    if (!option_file_path)
    {
        return;
    }

    if (!re_file_exists_str(option_file_path))
    {
        ac_report_error("File does not exist: %s", option_file_path);
        return;
    }

    if (!re_file_open_and_read(&o->config_file_memory, option_file_path))
    {
        ac_report_error("Could not read file: %s", option_file_path);
        return;
    }

    /* Split file content by line, empty values are ignored. */
    strv_splitter splitter = strv_splitter_make_str(dstr_to_view(&o->config_file_memory), "\n\r");
    strv line;
    while (strv_splitter_get_next(&splitter, &line))
    {
        strv trimmed = strv_trimmed_whitespaces(line);
        
        if (strv_empty(trimmed)
            || strv_begins_with_str(trimmed, "#"))
        {
            continue;
        }

        darrT_push_back(&o->config_file_args, trimmed.data);
        /* Write to the config_file_memory buffer and replace the delimiter with a '\0' */
        ((char*)trimmed.data)[trimmed.size] = '\0';
    }

    int config_argc = (int)o->config_file_args.darr.size;
    char** config_argv = (char**)o->config_file_args.darr.data;

    if (config_argc > 0)
    {
        parse_from_arguments(o, &config_argc, &config_argv);
    }
}

bool
parse_options(ac_options* o, int* argc, char*** argv)
{
    AC_ASSERT(argc && *argc && "There should be at least one argument to parse.");

    try_parse_from_file(o, *argc, *argv);
    return parse_from_arguments(o, argc, argv);
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* AC_PARSE_OPTIONS_H */