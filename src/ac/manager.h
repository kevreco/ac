#ifndef AC_MANAGER_H
#define AC_MANAGER_H

#include "stdbool.h"

#include "global.h"

#include <re/darr.h>
#include <re/dstr.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ac_ast_top_level ac_ast_top_level;

typedef struct ac_source_file ac_source_file;
struct ac_source_file
{
    const char* filepath; /* using dstr instead of strv fopen requires a c string at the end */
    dstr content;
};

/* options */

enum ac_compilation_step {

    ac_compilation_step_NONE = 1 << 0,
    ac_compilation_step_PARSE = 1 << 1,
    ac_compilation_step_SEMANTIC = 1 << 2,
    ac_compilation_step_GENERATE = 1 << 3,
    ac_compilation_step_ALL = ~0,
};

typedef struct ac_options ac_options;
struct ac_options {

    enum ac_compilation_step step;

    darrT(const char*) files;            /* Files to compile. */
    strv output_extension;               /* Extension of generated c file. */
    dstr config_file_memory;             /* Config file content with line endings replaced with \0 */
    darrT(const char*) config_file_args; /* Args parsed from the config file. */

    global_options_t global;             /* Some non critical global options. They will be set to a static global_options_t later on. */

    /* @FIXME find a better way to debug the parser.
        I don't remember when I last use it so maybe it should be removed.
    */
    bool debug_parser;                   /* Will print some debugging values in the output. */          
};

void ac_options_init_default(ac_options* o);
void ac_options_destroy(ac_options* o);

/* manager */

typedef struct ac_manager ac_manager;
struct ac_manager
{
    ac_options options;
    /* keep reference to destroy it. */
    ac_source_file source_file;
    ac_ast_top_level* top_level;
};

void ac_manager_init(ac_manager* m, ac_options* o);
void ac_manager_destroy(ac_manager* m);

ac_source_file* ac_manager_load_content(ac_manager* m, const char* filepath);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* AC_MANAGER_H */