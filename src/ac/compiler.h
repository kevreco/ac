#ifndef AC_COMPILER_H
#define AC_COMPILER_H

#include "manager.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

enum ac_compilation_step {
    ac_compilation_step_NONE = 1 << 0,
    ac_compilation_step_SEMANTIC = 1 << 2,
    ac_compilation_step_GENERATING = 1 << 3,
    ac_compilation_step_ALL = ac_compilation_step_SEMANTIC | ac_compilation_step_GENERATING,
};

typedef struct ac_compiler_options ac_compiler_options;
struct ac_compiler_options {
    enum ac_compilation_step step;
};

struct ac_compiler {
    ac_compiler_options options;
    struct ac_manager mgr;
};

void ac_compiler_options_init_default(ac_compiler_options* o);

void ac_compiler_init(struct ac_compiler* c, ac_compiler_options o);
void ac_compiler_destroy(struct ac_compiler* c);

bool ac_compiler_compile(struct ac_compiler* c, const char* wood_file, const char* c_file);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* AC_COMPILER_H */