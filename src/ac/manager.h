#ifndef AC_MANAGER_H
#define AC_MANAGER_H

#include "stdbool.h"

#include <re/dstr.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ac_ast_top_level;

struct ac_source_file
{
    const char* filepath; /* using dstr instead of dstr_view fopen requires a c string at the end */
    dstr content;
};

struct ac_manager
{
    /* keep reference to destroy it. */
    struct ac_source_file source_file;
    struct ac_ast_top_level* top_level;
};

void ac_manager_init(struct ac_manager* m);
void ac_manager_destroy(struct ac_manager* m);

struct ac_source_file* ac_manager_load_content(struct ac_manager* m, const char* filepath);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* AC_MANAGER_H */