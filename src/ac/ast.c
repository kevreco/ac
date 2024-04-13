#include "ast.h"

#include "global.h"

void ac_expr_list_init(struct ac_expr_list* list)
{
    memset(list, 0, sizeof(struct ac_expr_list));
}

void ac_expr_list_add(struct ac_expr_list* list, struct ac_ast_expr* next)
{
    assert(list->head != next);
    assert(list->tail != next);

    // if it's the first value, we initialize the head
    if (list->head == NULL)
    {
        list->head = next;

        list->tail = next;
    }
    else
    {
        list->tail->next = next;
        list->tail = next;
    }
}

void ac_ast_type_specifier_init(struct ac_ast_type_specifier* node)
{
    memset(node, 0, sizeof(struct ac_ast_type_specifier));
    node->type = ac_ast_type_TYPE_SPECIFIER;
}

void ac_ast_array_specifier_init(struct ac_ast_array_specifier* node)
{
    memset(node, 0, sizeof(struct ac_ast_array_specifier));
    node->type = ac_ast_type_ARRAY_SPECIFIER;
}

void ac_ast_declaration_init(struct ac_ast_declaration* node)
{
    memset(node, 0, sizeof(struct ac_ast_declaration));
    node->type = ac_ast_type_DECLARATION_UNKNOWN;
}

bool ac_ast_is_declaration(struct ac_ast_expr* expr)
{
    return expr->type >= ac_ast_type_DECLARATION_BEGIN
        && expr->type < ac_ast_type_DECLARATION_END;
}

void ac_ast_declarator_init(struct ac_ast_declarator* node)
{
    memset(node, 0, sizeof(struct ac_ast_declarator));
    node->type = ac_ast_type_DECLARATOR;
}

void ac_ast_parameter_init(struct ac_ast_parameter* node)
{
    memset(node, 0, sizeof(struct ac_ast_parameter));
    node->type = ac_ast_type_PARAMETER;
}

void ac_ast_parameters_init(struct ac_ast_parameters* node)
{
    memset(node, 0, sizeof(struct ac_ast_parameters));
    node->type = ac_ast_type_PARAMETERS;

    ac_expr_list_init(&node->list);
}

void ac_ast_block_init(struct ac_ast_block* node)
{
    memset(node, 0, sizeof(struct ac_ast_block));
    node->type = ac_ast_type_BLOCK;

    ac_expr_list_init(&node->statements);
}

void ac_ast_return_init(struct ac_ast_return* node)
{
    memset(node, 0, sizeof(struct ac_ast_return));
    node->type = ac_ast_type_RETURN;
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

    ac_ast_block_init(&node->block);
}

void ac_ast_array_empty_size_init(struct ac_ast_array_empty_size* node)
{
    memset(node, 0, sizeof(struct ac_ast_array_empty_size));
    node->type = ac_ast_type_ARRAY_EMPTY_SIZE;
}

struct ac_location ac_ast_expr_location(struct ac_ast_expr* expr)
{
    return expr->loc;
}
