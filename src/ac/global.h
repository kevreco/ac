#ifndef AC_GLOBAL_H
#define AC_GLOBAL_H

#include "location.h"

#define AC_UNUSED(x) ((void)(x))

#ifndef AC_ASSERT
#include <assert.h>
#define AC_ASSERT assert
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct ac_ast_expr;

void ac_report_error(const char* fmt, ...);
void ac_report_warning(const char* fmt, ...);
void ac_report_error_loc(struct ac_location loc, const char* fmt, ...);
void ac_report_warning_loc(struct ac_location loc, const char* fmt, ...);
void ac_report_error_expr(struct ac_ast_expr* expr, const char* fmt, ...);
void ac_report_warning_expr(struct ac_ast_expr* expr, const char* fmt, ...);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* AC_GLOBAL_H */