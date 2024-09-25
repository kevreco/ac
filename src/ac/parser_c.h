#ifndef AC_PARSER_C_H
#define AC_PARSER_C_H

#include "lexer.h"
#include "manager.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ac_ast_block ac_ast_block;

typedef struct ac_parser_options ac_parser_options;
struct ac_parser_options {
    bool debug_verbose;
};

typedef struct ac_parser_c ac_parser_c;
struct ac_parser_c {
    ac_manager* mgr;
    ac_lex lex;
    ac_parser_options options;

    ac_ast_block* current_block;
};

void ac_parser_c_init(ac_parser_c* p, ac_manager* mgr);
void ac_parser_c_destroy(ac_parser_c* p);

bool ac_parser_c_parse(ac_parser_c* p, const char* content, size_t size, const char* filepath);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* AC_PARSER_C_H */