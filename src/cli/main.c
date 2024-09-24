#include <stdio.h>
#include <string.h>

#include <ac/compiler.h>

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
int end_command(const struct cmd* cmd, int argc, char** argv) {}

#define STRV(x)\
    { sizeof(x)-1 , (const char*)(x)  }

static const struct cmd default_command =
{
    compile, STRV("compile"), "ac compile [--config-file <config-file>] <filename>",
};

static const struct cmd commands[] = {
    {help,    STRV("help"),     "ac help"},
    {version, STRV("version"),  "ac version"},
    {end_command, 0, 0, 0},
};

/* Return current argument and go to the next one. */
char*
pop_args(int* argc, char*** argv)
{
    char* current_arg = **argv;
    (*argv) += 1;
    (*argc) -= 1;
    return current_arg;
}

int display_help() { help(0, 0, 0); return -1; }
int display_error(const char* str) { fprintf(stderr, str); return -1; }

int
help(const struct cmd* cmd, int argc, char** argv )
{
    (void)cmd;
    (void)argc;
    (void)argv;

    const struct cmd* c = commands;

    fprintf(stdout, "AC compiler command line interface.");
    fprintf(stdout, "usage:\n");


    while (c->name)
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
    (void)cmd;
    (void)argc;

    const char* filename = pop_args(&argc, &argv);

    const char* extension = ".generated";
    if (argc > 0)
    {
        extension = pop_args(&argc, &argv);
    }

    /*  format path for yhr .c output file */
    {
        if (strlen(filename) >= MAX_FILENAME - 1)
        {
            return display_error("input file name is too long");
        }

        sprintf_s(output_file_name, MAX_FILENAME, "%s%s", filename, extension);
    }

    struct ac_compiler_options options;
    struct ac_compiler c;

    ac_compiler_options_init_default(&options);
    ac_compiler_init(&c, options);

    if (!ac_compiler_compile(&c, filename, output_file_name))
    {
        return -1;
    }

    return 0;
}

int
main(int argc, char** argv)
{
    char* command_name;
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

    /* process first command and go to next */
    command_name = pop_args(&argc, &argv);

    while (c->func != end_command)
    {
        if (strv_equals_str(c->name, command_name))
        {
            result = c->func(c, argc, argv);
            break;
        }
        ++c;
    }
   
    /* No command was found execute default command */
    if (c->func == end_command) {
        default_command.func(&default_command, argc, argv);
    }

    return result;
}

#ifdef __cplusplus
} /* extern "C" */
#endif