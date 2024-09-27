#ifndef AC_AST_H
#define AC_AST_H

#include <inttypes.h> /* intmax_t */

#include "re/dstr.h"
#include "lexer.h"
#include "location.h"

#ifdef __cplusplus
extern "C" {
#endif

/* We assume that all expr are nodes, 'next' is used with 'ac_expre_list' */
#define INCLUDE_AST_EXPR_BASE \
    enum ac_ast_type type;  \
    ac_location loc; \
    ac_ast_expr* next;

enum ac_ast_type {
    ac_ast_type_UNKNOWN,
    ac_ast_type_ARRAY_EMPTY_SIZE,
    ac_ast_type_ARRAY_SPECIFIER,
    ac_ast_type_BLOCK,
    ac_ast_type_EMPTY_STATEMENT,
    ac_ast_type_DECLARATION_UNKNOWN,
    ac_ast_type_DECLARATION_BEGIN,
    ac_ast_type_DECLARATION_SIMPLE = ac_ast_type_DECLARATION_BEGIN,    /* const int i  |  int *i  |  int i = 0  |  int i[0]  |  int func() */
    ac_ast_type_DECLARATION_TYPEDEF,              /* @TODO */          /* typedef int my_int */
    ac_ast_type_DECLARATION_FUNCTION_DEFINITION,                       /* int func() { } */
    ac_ast_type_DECLARATION_END,
    ac_ast_type_DECLARATOR,                                            /* i  |  *i  |  i = 0  |  i[0]  |  func() */
    ac_ast_type_IDENTIFIER,
    ac_ast_type_LITERAL_BOOL,
    ac_ast_type_LITERAL_FLOAT,
    ac_ast_type_LITERAL_INTEGER,
    ac_ast_type_LITERAL_NULL,
    ac_ast_type_LITERAL_STRING,
    ac_ast_type_PARAMETERS,
    ac_ast_type_PARAMETER,
    ac_ast_type_RETURN,
    ac_ast_type_TOP_LEVEL,
    ac_ast_type_TYPE_SPECIFIER,
    ac_ast_type_UNARY
};

typedef struct ac_ast_array_specifier ac_ast_array_specifier;
typedef struct ac_ast_binary ac_ast_binary;
typedef struct ac_ast_block ac_ast_block;
typedef struct ac_ast_declaration ac_ast_declaration;
typedef struct ac_ast_declarator ac_ast_declarator;
typedef struct ac_ast_expr ac_ast_expr;
typedef struct ac_ast_identifier ac_ast_identifier;
typedef struct ac_ast_literal ac_ast_literal;
typedef struct ac_ast_parameter ac_ast_parameter;
typedef struct ac_ast_parameters ac_ast_parameters;
typedef struct ac_ast_return ac_ast_return;
typedef struct ac_ast_top_level ac_ast_top_level;
typedef struct ac_ast_type_specifier ac_ast_type_specifier;
typedef struct ac_ast_unary ac_ast_unary;
typedef struct ac_expr_list ac_expr_list;

struct ac_ast_expr {
    INCLUDE_AST_EXPR_BASE
};

/* NOTE: Not an AST node */

struct ac_expr_list {
    ac_ast_expr* head;
    ac_ast_expr* tail;
};

void ac_expr_list_init(ac_expr_list* list);
void ac_expr_list_add(ac_expr_list* list, ac_ast_expr* next);

#define EACH_EXPR(item_, list_) \
    (item_) = list_.head; item_; item_ = item_->next

struct ac_ast_literal {
    INCLUDE_AST_EXPR_BASE
    union {
        strv str;
        intmax_t integer;
        double _float;
        bool boolean;
    } u;
};

struct ac_ast_identifier {
    INCLUDE_AST_EXPR_BASE
    strv name;
};

/* Type specifier can be int, int* etc.
   But right now it can only handle simple "int" so we only consider identifier.
   see more here https://en.cppreference.com/w/c/language/declarations#Specifiers
*/
struct ac_ast_type_specifier {
    INCLUDE_AST_EXPR_BASE

    ac_ast_identifier* identifier;
};

void ac_ast_type_specifier_init(ac_ast_type_specifier* node);

struct ac_ast_array_specifier {
    INCLUDE_AST_EXPR_BASE

    /* There should be an expression resulting into a constant non-negative integer */
    ac_ast_expr* size_expression;
    ac_ast_array_specifier* next_array;
};

void ac_ast_array_specifier_init(ac_ast_array_specifier* node);

struct ac_ast_declarator {
    INCLUDE_AST_EXPR_BASE

    int pointer_depth;
    ac_ast_identifier* ident;
    ac_ast_array_specifier* array_specifier;                  /* dummy member so that we already pre-handle this case */
    ac_ast_expr* initializer;      /* optional */
    ac_ast_parameters* parameters; /* optional */
};

void ac_ast_declarator_init(ac_ast_declarator* node);

struct ac_ast_declaration {
    INCLUDE_AST_EXPR_BASE

    ac_ast_type_specifier* type_specifier;
    ac_ast_declarator* declarator;

    ac_ast_block* function_block; /* optional, only used for ac_ast_type_DECLARATION_FUNCTION_DEFINITION */
};

void ac_ast_declaration_init(ac_ast_declaration* node);
bool ac_ast_is_declaration(ac_ast_expr* expr);

/* function parameters */
struct ac_ast_parameters {
    INCLUDE_AST_EXPR_BASE
    ac_expr_list list;
};

void ac_ast_parameters_init(ac_ast_parameters* node);

/* This represent a function parameter, it's quite similar to a simple declaration */
struct ac_ast_parameter {
    INCLUDE_AST_EXPR_BASE
    ac_ast_identifier* type_name;
    bool is_var_args;                     /* optional */
    ac_ast_declarator* declarator; /* optional */
};

void ac_ast_parameter_init(ac_ast_parameter* node);

/* block are basically list of expressions */
struct ac_ast_block {
    INCLUDE_AST_EXPR_BASE
    /* body of the block */
    ac_expr_list statements;
};

void ac_ast_block_init(ac_ast_block* node);

struct ac_ast_return {
    INCLUDE_AST_EXPR_BASE

    ac_ast_expr* expr;
};

void ac_ast_return_init(ac_ast_return* node);

struct ac_ast_unary {
    INCLUDE_AST_EXPR_BASE
    enum ac_token_type op;       /* which type of unary operator */
    ac_ast_expr* operand; /* primary ast expr */
};

struct ac_ast_binary {
    INCLUDE_AST_EXPR_BASE
    enum ac_token_type op;     /* which type of binary operator */
    ac_ast_expr* left;
    ac_ast_expr* right;
};

void ac_ast_unary_init(ac_ast_unary* unary);

struct ac_ast_top_level {
    INCLUDE_AST_EXPR_BASE

    ac_ast_block block;
};

void ac_ast_top_level_init(ac_ast_top_level* file);

/* For expression's like "int[]" we need a valid value for the array size expression, null means that there was an issue somewhere .
   Hence, this ac_ast_empty_array_size was creaed.
*/
typedef struct ac_ast_array_empty_size ac_ast_array_empty_size;
struct ac_ast_array_empty_size {
    INCLUDE_AST_EXPR_BASE
};

void ac_ast_array_empty_size_init(ac_ast_array_empty_size* node);

ac_location ac_ast_expr_location(ac_ast_expr* expr);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* AC_AST_H */