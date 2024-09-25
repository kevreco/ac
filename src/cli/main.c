#include <stdio.h>
#include <string.h>

#include <ac/compiler.h>

#include "parse_options.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AC_VERSION "0.0.0-dev"

struct cmd
{
    int (*func)(const struct cmd* cmd, int argc, char** argv);
    strv name;
    const char* usage;
};

int help(const struct cmd* cmd, int argc, char** argv);
int version(const struct cmd* cmd, int argc, char** argv);
int compile(const struct cmd* cmd, int argc, char** argv);
int end_command(const struct cmd* cmd, int argc, char** argv) { AC_UNUSED(cmd);  AC_UNUSED(argc);  AC_UNUSED(argv); return 1; }

static const struct cmd default_command =
{
    compile, STRV("compile"), "ac compile [--config-file <config-file>] <filename>",
};

static const struct cmd commands[] = {
    {help,    STRV("help"),     "ac help"},
    {version, STRV("version"),  "ac version"},
    {end_command, 0, 0, 0},
};

int display_help() { help(0, 0, 0); return 1; }
int display_error(const char* str) { fprintf(stderr, "%s", str); return 1; }

int
help(const struct cmd* cmd, int argc, char** argv )
{
    (void)cmd;
    (void)argc;
    (void)argv;

    const struct cmd* c = commands;

    fprintf(stdout, "AC compiler command line interface.");
    fprintf(stdout, "usage:\n");

    while (c->func != end_command)
    {
        fprintf(stdout, "\n%s", c->usage);
        ++c;
    }

    return 0;
}

int
version(const struct cmd* cmd, int argc, char** argv)
{
    (void)cmd;
    (void)argc;
    (void)argv;
    
    fprintf(stdout, AC_VERSION);

    return 0;
}

#define MAX_FILENAME 2048 /* arbitrary limit for path, adjust this if needed */

char output_file_name[MAX_FILENAME];

int
compile(const struct cmd* cmd, int argc, char** argv)
{
    int result = 1;
    (void)cmd;

    ac_compiler_options options;
    ac_compiler_options_init_default(&options);
    
    if (parse_options(&options, &argc, &argv))
    {
        ac_compiler c;

        ac_compiler_init(&c, options);

        result = ac_compiler_compile(&c) ? 0 : 1;

        ac_compiler_destroy(&c);
    }

    ac_compiler_options_destroy(&options);

    return result;
}

int
main(int argc, char** argv)
{
    int result = 0;
    const struct cmd* c = commands;

    if (!*argv) { /* no argument */
        return display_help();
    }

    /* skip application name */
    pop_args(&argc, &argv);

    if (!*argv) { /* no argument left */
        return display_help();
    }

    char* first_arg = *argv;

    while (c->func != end_command)
    {
        if (strv_equals_str(c->name, first_arg))
        {
            result = c->func(c, argc, argv);
            break;
        }
        ++c;
    }
   
    /* No command was found, execute default command. */
    if (c->func == end_command) {
        default_command.func(&default_command, argc, argv);
    }

    return result;
}

#ifdef __cplusplus
} /* extern "C" */
#endif