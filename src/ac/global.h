#ifndef AC_GLOBAL_H
#define AC_GLOBAL_H

#include "location.h"

#define AC_UNUSED(x) ((void)(x))

#ifndef AC_ASSERT
#include <assert.h>
#define AC_ASSERT assert
#endif

/* djb2 hash. */
#define AC_HASH_INIT (5381)
#define AC_HASH(h, c) ((((h) << 5) + (h)) + (c))

#ifdef __cplusplus
extern "C" {
#endif

typedef struct global_options_t global_options_t;
struct global_options_t {
    bool colored_output;
    bool display_surrounding_lines;
};

/* global_option set at the entry point of compilation. */
extern global_options_t global_options;

typedef struct ac_ast_expr ac_ast_expr;

void ac_report_error(const char* fmt, ...);
void ac_report_warning(const char* fmt, ...);
void ac_report_error_loc(ac_location loc, const char* fmt, ...);
void ac_report_warning_loc(ac_location loc, const char* fmt, ...);
void ac_report_error_expr(ac_ast_expr* expr, const char* fmt, ...);
void ac_report_warning_expr(ac_ast_expr* expr, const char* fmt, ...);

size_t ac_djb2_hash(char* str, size_t size);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* AC_GLOBAL_H */