#ifndef AC_AST_H
#define AC_AST_H

#include <inttypes.h> /* intmax_t */

#include "re/dstr.h"
#include "location.h"

#ifdef __cplusplus
extern "C" {
#endif

enum ac_ast_type {
    ac_ast_type_UNKNOWN,
    ac_ast_type_BLOCK,
    ac_ast_type_DECLARATION,
    ac_ast_type_EMPTY_STATEMENT,
    ac_ast_type_TOP_LEVEL,
    ac_ast_type_IDENTIFIER,
    ac_ast_type_IF,
    ac_ast_type_LITERAL_BOOL,
    ac_ast_type_LITERAL_FLOAT,
    ac_ast_type_LITERAL_INTEGER,
    ac_ast_type_LITERAL_NULL,
    ac_ast_type_LITERAL_STRING,
    ac_ast_type_TYPE_SPECIFIER,
    ac_ast_type_RETURN,
    ac_ast_type_UNARY
};

struct ac_ast_literal {
    enum ac_ast_type type;
    struct ac_location loc;
    union {
        dstr_view str;
        intmax_t integer;
        double _float;
        bool boolean;
    } u;
};

struct ac_ast_identifier {
    enum ac_ast_type type;
    struct ac_location loc;
    dstr_view name;
};

/* Type specifier can be int, int* etc.
   But right now it can only handle simple "int" so we only consider identifier.
   see more here https://en.cppreference.com/w/c/language/declarations#Specifiers
*/
struct ac_ast_type_specifier {
    enum ac_ast_type type;
    struct ac_location loc;

    struct ac_ast_identifier* identifier;
};

void ac_ast_type_specifier_init(struct ac_ast_type_specifier* node);

struct ac_ast_declaration {
    enum ac_ast_type type;
    struct ac_location loc;

    struct ac_ast_type_specifier* type_specifier;
    struct ac_ast_identifier* ident;

    /* we use an initializer and not an assignment here because those are not the same operations.
       we can initialize a global variable, but we cannot assign it again somewhere else.
    */
    struct ac_ast_expr* initializer;     /* optional, cannot have an 'initializer' and '' at the same time */
    struct ac_ast_block* function_block; /* optional, cannot have an 'function_block' and 'initializer' at the same time */

    union {
        struct c {
            struct ac_ast_declaration* next; /* in C there could be multiple declaration for a type so we keep them in here */
        } c;
    } u;
};

void ac_ast_declaration_init(struct ac_ast_declaration* node);

/* NOTE: Not an AST node
*/
struct ac_ast_expr_list {
    struct ac_ast_expr* value;
    struct ac_ast_expr_list* next;
};

void ac_ast_expr_list_init(struct ac_ast_expr_list* list);

/* block are basically list of expressions*/
struct ac_ast_block {

    struct ac_ast_expr_list parameters; /* parameters for functions, for if/while conditions */

    // body of the block, all the statements
    struct ac_ast_expr_list statements; // @TODO rename body ?
};

struct ac_ast_return {
    enum ac_ast_type type;
    struct ac_location loc;

    struct ac_ast_expr* expr;
};

struct ac_ast_unary {
    enum ac_ast_type type;
    struct ac_location loc;
    enum ac_token_type op;       /* which type of unary operator */
    struct ac_ast_expr* operand; /* primary ast expr */
};

struct ac_ast_binary {
    enum ac_ast_type type;
    struct ac_location loc;
    enum ac_token_type op;     /* which type of binary operator */
    struct ac_ast_expr* left;
    struct ac_ast_expr* right;
};

void ac_ast_unary_init(struct ac_ast_unary* unary);

struct ac_ast_top_level {
    enum ac_ast_type type;
    struct ac_location loc;

    struct ac_ast_expr_list declarations;
};

void ac_ast_top_level_init(struct ac_ast_top_level* file);

struct ac_ast_expr {
    enum ac_ast_type type;
    struct ac_location loc;
};

struct ac_location ac_ast_expr_location(struct ac_ast_expr* expr);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* AC_AST_H */