#ifndef AC_MANAGER_H
#define AC_MANAGER_H

#include "stdbool.h"

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

typedef struct ac_manager ac_manager;
struct ac_manager
{
    /* keep reference to destroy it. */
    ac_source_file source_file;
    ac_ast_top_level* top_level;
};

void ac_manager_init(ac_manager* m);
void ac_manager_destroy(ac_manager* m);

ac_source_file* ac_manager_load_content(ac_manager* m, const char* filepath);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* AC_MANAGER_H */