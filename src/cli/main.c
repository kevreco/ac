#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h> /* _O_BINARY */
#endif

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

int
compile(const struct cmd* cmd, int argc, char** argv)
{
    int result = 1;
    (void)cmd;

    ac_options options;
    ac_options_init_default(&options);
    
    if (parse_options(&options, &argc, &argv))
    {
        ac_compiler c;

        ac_compiler_init(&c, &options);

        result = ac_compiler_compile(&c) ? 0 : 1;

        ac_compiler_destroy(&c);
    }

    ac_options_destroy(&options);

    return result;
}

int
main(int argc, char** argv)
{
#ifdef _WIN32
    /* Avoid \n char to be translated into \r\n */
    if (_setmode(fileno(stdout), _O_BINARY) == -1) perror("Cannot set mode to stdout.");
    if (_setmode(fileno(stderr), _O_BINARY) == -1) perror("Cannot set mode to stderr.");

    /* @FIXME: Why is this failing? Can't we set this to binary mode when it's running from a console? */
    (void)_setmode(fileno(stdin), O_BINARY);
#endif

    int result = 1;
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
        result = default_command.func(&default_command, argc, argv);
    }

    return result;
}

#ifdef __cplusplus
} /* extern "C" */
#endif