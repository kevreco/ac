#ifndef AC_COMPILER_H
#define AC_COMPILER_H

#include <re/darr.h>

#include "manager.h"

#ifdef __cplusplus
extern "C" {
#endif

enum ac_compilation_step {

    ac_compilation_step_NONE = 1 << 0,
    ac_compilation_step_PARSE = 1 << 1,
    ac_compilation_step_SEMANTIC = 1 << 2,
    ac_compilation_step_GENERATE = 1 << 3,
    ac_compilation_step_ALL = ~0,
};

typedef struct ac_compiler_options ac_compiler_options;
struct ac_compiler_options {

    enum ac_compilation_step step;
                                         
    darrT(const char*) files;            /* Files to compile. */
    strv output_extension;               /* Extension of generated c file. */
    dstr config_file_memory;             /* Config file content with line endings replaced with \0 */
    darrT(const char*) config_file_args; /* Args parsed from the config file. */
};

typedef struct ac_compiler ac_compiler;
struct ac_compiler {
    ac_compiler_options options;
    ac_manager mgr;
};

void ac_compiler_options_init_default(ac_compiler_options* o);
void ac_compiler_options_destroy(ac_compiler_options* o);

void ac_compiler_init(ac_compiler* c, ac_compiler_options o);
void ac_compiler_destroy(ac_compiler* c);

bool ac_compiler_compile(ac_compiler* c);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* AC_COMPILER_H */