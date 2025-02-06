#ifndef AC_PARSER_C_H
#define AC_PARSER_C_H

#include "preprocessor.h"
#include "manager.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ac_ast_block ac_ast_block;

typedef struct ac_parser_c ac_parser_c;
struct ac_parser_c {
    ac_manager* mgr;
    ac_pp pp;

    strv current_function_name;
    ac_ast_block* current_block;
};

void ac_parser_c_init(ac_parser_c* p, ac_manager* mgr, strv content, strv filepath);
void ac_parser_c_destroy(ac_parser_c* p);

bool ac_parser_c_parse(ac_parser_c* p);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* AC_PARSER_C_H */