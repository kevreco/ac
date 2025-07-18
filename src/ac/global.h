#ifndef AC_GLOBAL_H
#define AC_GLOBAL_H

#include <stdint.h> /* int32_t, int64_t */

#include "re_lib.h"
#include "location.h"

#define AC_UNUSED(x) ((void)(x))

#ifndef AC_ASSERT
#include <assert.h>
#define AC_ASSERT assert
#endif

#define AC_DJB_HASH_INIT (5381)
#define AC_DJB_HASH(h, c) ((((h) << 5) + (h)) + (c))

#define FNV1_PRIME (16777619)
#define FNV1_OFFSET_BASIS (2166136261) 
#define FNV1_HASH(h, c) ((uint32_t)((((unsigned)(c)) ^ (h)) * FNV1_PRIME))

#define AC_HASH_INIT FNV1_OFFSET_BASIS
#define AC_HASH(h, c) FNV1_HASH(h,c)

#define AC_XSTRINGIZE(x) #x
#define AC_STRINGIZE(x) AC_XSTRINGIZE(x)

#ifdef __cplusplus
extern "C" {
#endif

typedef darrT(strv) path_array;

typedef struct global_options_t global_options_t;
struct global_options_t {
    bool colored_output;
    bool display_surrounding_lines;
};

/* global_option set at the entry point of compilation. */
extern global_options_t global_options;

typedef struct ac_ast_expr ac_ast_expr;
typedef struct ac_options ac_options;

void ac_init(ac_options* o);
void ac_terminate();

void ac_add_default_system_includes(path_array* items);

void ac_report_error(const char* fmt, ...);
void ac_report_internal_error(const char* fmt, ...);
void ac_report_warning(const char* fmt, ...);
void ac_report_error_loc(ac_location loc, const char* fmt, ...);
void ac_report_internal_error_loc(ac_location loc, const char* fmt, ...);
void ac_report_warning_loc(ac_location loc, const char* fmt, ...);
void ac_report_pp_warning_loc(ac_location loc, const char* fmt, ...);
void ac_report_pp_error_loc(ac_location loc, const char* fmt, ...);

void ac_report_error_expr(ac_ast_expr* expr, const char* fmt, ...);
void ac_report_warning_expr(ac_ast_expr* expr, const char* fmt, ...);

size_t ac_hash(char* str, size_t size);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* AC_GLOBAL_H */