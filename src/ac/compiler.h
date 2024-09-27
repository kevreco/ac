#ifndef AC_COMPILER_H
#define AC_COMPILER_H

#include "manager.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ac_compiler ac_compiler;
struct ac_compiler {
    ac_manager mgr;
};

void ac_compiler_init(ac_compiler* c, ac_options* o);
void ac_compiler_destroy(ac_compiler* c);

bool ac_compiler_compile(ac_compiler* c);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* AC_COMPILER_H */