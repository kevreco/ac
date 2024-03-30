#ifndef AC_AST_H
#define AC_AST_H

#include <inttypes.h> /* intmax_t */

#include "re/dstr.h"
#include "location.h"

#ifdef __cplusplus
extern "C" {
#endif

/* We assume that all expr are nodes, 'next' is used with 'ac_expre_list' */
#define INCLUDE_AST_EXPR_BASE \
    enum ac_ast_type type;  \
    struct ac_location loc; \
    struct ac_ast_expr* next;

enum ac_ast_type {
    ac_ast_type_UNKNOWN,
    ac_ast_type_BLOCK,
    ac_ast_type_EMPTY_STATEMENT,
    ac_ast_type_DECLARATION_BEGIN,
    ac_ast_type_DECLARATION_UNKNOWN = ac_ast_type_DECLARATION_BEGIN,
    ac_ast_type_DECLARATION_SIMPLE = ac_ast_type_DECLARATION_BEGIN,    /* int i;    int i = 0;    int i = 0, j = 0;   int func(); */
    ac_ast_type_DECLARATION_TYPEDEF,              /* @TODO */          /* typedef int my_int */
    ac_ast_type_DECLARATION_FUNCTION_DEFINITION,                       /* int func() { } */
    ac_ast_type_DECLARATION_END,
    ac_ast_type_IDENTIFIER,
    ac_ast_type_LITERAL_BOOL,
    ac_ast_type_LITERAL_FLOAT,
    ac_ast_type_LITERAL_INTEGER,
    ac_ast_type_LITERAL_NULL,
    ac_ast_type_LITERAL_STRING,
    ac_ast_type_RETURN,
    ac_ast_type_TOP_LEVEL,
    ac_ast_type_TYPE_SPECIFIER,
    ac_ast_type_UNARY
};

/* NOTE: Not an AST node */
struct ac_expr_list {
    struct ac_ast_expr* head;
    struct ac_ast_expr* tail;
};

void ac_expr_list_init(struct ac_expr_list* list);
void ac_expr_list_add(struct ac_expr_list* list, struct ac_ast_expr* next);

# 
#define EACH_EXPR(item_, list_) \
    (item_) = list_.head; item_; item_ = item_->next

struct ac_ast_literal {
    INCLUDE_AST_EXPR_BASE
    union {
        dstr_view str;
        intmax_t integer;
        double _float;
        bool boolean;
    } u;
};

struct ac_ast_identifier {
    INCLUDE_AST_EXPR_BASE
    dstr_view name;
};

/* Type specifier can be int, int* etc.
   But right now it can only handle simple "int" so we only consider identifier.
   see more here https://en.cppreference.com/w/c/language/declarations#Specifiers
*/
struct ac_ast_type_specifier {
    INCLUDE_AST_EXPR_BASE

    struct ac_ast_identifier* identifier;
};

void ac_ast_type_specifier_init(struct ac_ast_type_specifier* node);

struct ac_ast_declaration {
    INCLUDE_AST_EXPR_BASE

    struct ac_ast_type_specifier* type_specifier;
    struct ac_ast_identifier* ident;

    /* we use an initializer and not an assignment here because those are not the same operations.
       we can initialize a global variable, but we cannot assign it again somewhere else.
    */
    struct ac_ast_expr* initializer;     /* optional, cannot have an 'initializer' and '' at the same time */
    struct ac_ast_block* function_block; /* optional, cannot have an 'function_block' and 'initializer' at the same time */
};

void ac_ast_declaration_init(struct ac_ast_declaration* node);
bool ac_ast_is_declaration(struct ac_ast_expr* expr);

/* block are basically list of expressions */
struct ac_ast_block {
    INCLUDE_AST_EXPR_BASE
    struct ac_expr_list parameters; /* parameters for functions, for if/while conditions */

    // body of the block, all the statements
    struct ac_expr_list statements; // @TODO rename body ?
};

void ac_ast_block_init(struct ac_ast_block* node);

struct ac_ast_return {
    INCLUDE_AST_EXPR_BASE

    struct ac_ast_expr* expr;
};

void ac_ast_return_init(struct ac_ast_return* node);

struct ac_ast_unary {
    INCLUDE_AST_EXPR_BASE
    enum ac_token_type op;       /* which type of unary operator */
    struct ac_ast_expr* operand; /* primary ast expr */
};

struct ac_ast_binary {
    INCLUDE_AST_EXPR_BASE
    enum ac_token_type op;     /* which type of binary operator */
    struct ac_ast_expr* left;
    struct ac_ast_expr* right;
};

void ac_ast_unary_init(struct ac_ast_unary* unary);

struct ac_ast_top_level {
    INCLUDE_AST_EXPR_BASE

    struct ac_ast_block block;
};

void ac_ast_top_level_init(struct ac_ast_top_level* file);

struct ac_ast_expr {
    INCLUDE_AST_EXPR_BASE
};

struct ac_location ac_ast_expr_location(struct ac_ast_expr* expr);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* AC_AST_H */