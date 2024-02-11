#ifndef AC_PARSER_C_H
#define AC_PARSER_C_H

#include "ast.h" /* ac_ast_block */
#include "lexer.h"
#include "manager.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ac_parser_options
{
    bool debug_verbose;
};

struct ac_parser_c
{
    struct ac_manager* mgr;
    struct ac_lex lex;
    struct ac_parser_options options;

    struct ac_ast_block* current_block;
};

void ac_parser_c_init(struct ac_parser_c* p, struct ac_manager* mgr);
void ac_parser_c_destroy(struct ac_parser_c* p);

bool ac_parser_c_parse(struct ac_parser_c* p, const char* content, size_t size, const char* filepath);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* AC_PARSER_C_H */