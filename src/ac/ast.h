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
    ac_ast_type_BINARY,
    ac_ast_type_EMPTY_STATEMENT,
    ac_ast_type_DECLARATION_UNKNOWN,
    ac_ast_type_DECLARATION_SIMPLE,               /* const int i  |  int *i  |  int i = 0  |  int i[0]  |  int func() */
    ac_ast_type_DECLARATION_TYPEDEF,              /* @TODO */          /* typedef int my_int */
    ac_ast_type_DECLARATION_FUNCTION_DEFINITION,  /* int func() { } */
    ac_ast_type_DECLARATION_END,
    ac_ast_type_DECLARATOR,                       /* i  |  *i  |  i = 0  |  i[0]  |  func() */
    ac_ast_type_IDENTIFIER,
    ac_ast_type_LITERAL_BOOL,
    ac_ast_type_LITERAL_CHAR,
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
    ac_token token; /* The token itself is stored to have access to the original string. */
};

struct ac_ast_identifier {
    INCLUDE_AST_EXPR_BASE
    strv name;
};

/* Specifiers and qualifiers */
enum ac_specifier
{
    ac_specifier_NONE = 0,

    /* Type modifier (subcategory of type specifier) */

    ac_specifier_SIGNED   = 1 << 0,
    ac_specifier_UNSIGNED = 1 << 1,
    ac_specifier_SHORT = 1 << 2,

    /* Storage specifier. */

    ac_specifier_AUTO     = 1 << 3,
    ac_specifier_EXTERN   = 1 << 4,
    ac_specifier_REGISTER = 1 << 5,
    ac_specifier_STATIC   = 1 << 6,

    /* Special Storage specifier, can be combined with static or extern to adjust linkage. */

    ac_specifier_ATOMIC = 1 << 7, /* Could also be */
    ac_specifier_THREAD_LOCAL = 1 << 8,

    /* Other type specifier. */
   
    ac_specifier_INLINE = 1 << 9,
    ac_specifier_LONG = 1 << 10,
    ac_specifier_LONG_LONG = 1 << 11,

    /* Type qualifiers */
   
    ac_specifier_CONST = 1 << 12,
    ac_specifier_RESTRICT = 1 << 13,
    ac_specifier_VOLATILE = 1 << 14,
};

/* Type specifier can be int, int* etc.
   But right now it can only handle simple "int" so we only consider identifier.
   see more here https://en.cppreference.com/w/c/language/declarations#Specifiers
*/
struct ac_ast_type_specifier {
    INCLUDE_AST_EXPR_BASE

    ac_ast_identifier* identifier;

    enum ac_specifier specifiers;
    /* For bool, int, struct, enum, typedef, etc. This cannot be stacked. */
    enum ac_token_type type_specifier;
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
    ac_ast_array_specifier* array_specifier; /* No member so that we already pre-handle this case */
    bool is_restrict;              /* Optional. */
    /* NOTE: declarator cannot have parameter and initializer at the same time. */
    ac_ast_expr* initializer;      /* Optional. If the variable is initialized with something. */
    ac_ast_parameters* parameters; /* Optional. If it's a function declaration. */
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
    ac_ast_type_specifier* type_specifier;
    bool is_var_args;              /* optional */
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
    enum ac_token_type op; /* Which type of unary operator. */
    ac_ast_expr* operand;  /* Primary ast expression. */
};

void ac_ast_unary_init(ac_ast_unary* node);

struct ac_ast_binary {
    INCLUDE_AST_EXPR_BASE
    enum ac_token_type op; /* Which type of binary operator. */
    ac_ast_expr* left;
    ac_ast_expr* right;
};

void ac_ast_binary_init(ac_ast_binary* node);

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