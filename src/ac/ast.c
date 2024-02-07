#include "ast.h"

#include "global.h"


void ac_ast_type_specifier_init(struct ac_ast_type_specifier* node)
{
    memset(node, 0, sizeof(struct ac_ast_type_specifier));
    node->type = ac_ast_type_TYPE_SPECIFIER;
}

void ac_ast_declaration_init(struct ac_ast_declaration* node)
{
    memset(node, 0, sizeof(struct ac_ast_declaration));
    node->type = ac_ast_type_DECLARATION;
}

void ac_ast_expr_list_init(struct ac_ast_expr_list* list)
{
    memset(list, 0, sizeof(struct ac_ast_expr_list));
}

void ac_ast_unary_init(struct ac_ast_unary* node)
{
    memset(node, 0, sizeof(struct ac_ast_unary));
    node->type = ac_ast_type_UNARY;

    node->op = ac_ast_type_UNKNOWN;
    node->operand = 0;
}

void ac_ast_top_level_init(struct ac_ast_top_level* node)
{
    memset(node, 0, sizeof(struct ac_ast_top_level));
    node->type = ac_ast_type_TOP_LEVEL;

    ac_ast_expr_list_init(&node->declarations);
}

struct ac_location ac_ast_expr_location(struct ac_ast_expr* expr)
{
    return expr->loc;
}
