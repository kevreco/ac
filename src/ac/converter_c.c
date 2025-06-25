#include "converter_c.h"

#include <stdio.h> /* FILE */

#include "ast.h"
#include "manager.h"
#include "re_lib.h"

#define CAST_TO(type_, ident_, object_) type_ ident_ = (type_)(object_)

static void print_top_level(ac_converter_c* c);
static void print_expr(ac_converter_c* c, ac_ast_expr* expr);
static void print_identifier(ac_converter_c* c, ac_ast_identifier* identifier);
static void print_type_specifier(ac_converter_c* c, ac_ast_type_specifier* type_specifier);
static void print_pointers(ac_converter_c* c, int count);
static void print_array_specifier(ac_converter_c* c, ac_ast_array_specifier* array_specifier);
static void print_parameters(ac_converter_c* c, ac_ast_parameters* parameters);
static void print_parameter(ac_converter_c* c, ac_ast_parameter* parameter);
static void print_declaration(ac_converter_c* c, ac_ast_declaration* declaration);
static void print_declarator(ac_converter_c* c, ac_ast_declarator* declarator);
static void print_unary_op(ac_converter_c* c, enum ac_token_type type);
static void print_binary_op(ac_converter_c* c, enum ac_token_type type);

static void print_fv(ac_converter_c* c, const char* fmt, va_list args);
static void print_f(ac_converter_c* c, const char* fmt, ...);
static void print_str(ac_converter_c* c, const char* str);
static void print_strv(ac_converter_c* c, strv view);

static size_t write_to_file(strv str, FILE* f);
static void push_indent(ac_converter_c* c);
static void pop_indent(ac_converter_c* c);
static void indent(ac_converter_c* c);
static void push_brace(ac_converter_c* c);
static void pop_brace(ac_converter_c* c);
static void new_line(ac_converter_c* c);

void ac_converter_c_init(ac_converter_c* c, ac_manager* mgr)
{
    memset(c, 0, sizeof(ac_converter_c));

    c->mgr = mgr;
    dstr_init(&c->string_buffer);

    c->indent_pattern = "    ";
}

void ac_converter_c_destroy(ac_converter_c* c)
{
    dstr_destroy(&c->string_buffer);
}

void ac_converter_c_convert(ac_converter_c* c, const char* filepath)
{
    print_top_level(c);

    FILE* f = re_file_open_readwrite(filepath);
    write_to_file(dstr_to_strv(&c->string_buffer), f);
    re_file_close(f);
}

static void print_top_level(ac_converter_c* c)
{
    ac_ast_top_level* top_level = c->mgr->top_level;

    ac_ast_expr* current = NULL;
    for(EACH_EXPR(current, top_level->block.statements))
    {
        print_expr(c, current);
    }
}

static void print_expr(ac_converter_c* c, ac_ast_expr* expr)
{
    if (ac_ast_is_declaration(expr))
    {
        CAST_TO(ac_ast_declaration*, declaration, expr);
        print_declaration(c, declaration);
    }
    else if (expr->type == ac_ast_type_ARRAY_EMPTY_SIZE)
    {
        /* print nothing */
    }
    else if (expr->type == ac_ast_type_BINARY)
    {
        CAST_TO(ac_ast_binary*, binary, expr);
        print_expr(c, binary->left);
        print_str(c, " ");
        print_binary_op(c, binary->op);
        print_str(c, " ");
        print_expr(c, binary->right);
    }
    else if (expr->type == ac_ast_type_DECLARATOR)
    {
        CAST_TO(ac_ast_declarator*, declarator, expr);

        print_declarator(c, declarator);
    }
    else if (expr->type == ac_ast_type_IDENTIFIER)
    {
        CAST_TO(ac_ast_identifier*, identifier, expr);

        print_identifier(c, identifier);
    }
    else if (
        expr->type == ac_ast_type_LITERAL_CHAR
        || expr->type == ac_ast_type_LITERAL_FLOAT
        || expr->type == ac_ast_type_LITERAL_INTEGER
        || expr->type == ac_ast_type_LITERAL_STRING)
    {
        CAST_TO(ac_ast_literal*, literal, expr);
        if (expr->type == ac_ast_type_LITERAL_STRING
            && literal->token.u.str.is_embed_path)
        {
            print_str(c, "\n#embed ");
            ac_token_sprint(&c->string_buffer, literal->token);
            print_str(c, "\n");
        }
        else
        {
            ac_token_sprint(&c->string_buffer, literal->token);
        }
    }
    else if (expr->type == ac_ast_type_PARAMETER)
    {
        CAST_TO(ac_ast_parameter*, parameter, expr);
        print_parameter(c, parameter);
    }
    else if (expr->type == ac_ast_type_RETURN)
    {
        CAST_TO(ac_ast_return*, return_, expr);
        indent(c);
        print_str(c, "return ");
        print_expr(c, return_->expr);
        print_str(c, ";");
    }
    else if (expr->type == ac_ast_type_TYPE_SPECIFIER)
    {
        CAST_TO(ac_ast_type_specifier*, type_specifier, expr);
        print_type_specifier(c, type_specifier);
    }
    else if (expr->type == ac_ast_type_UNARY)
    {
        CAST_TO(ac_ast_unary*, unary, expr);
        print_unary_op(c, unary->op);
        print_expr(c, unary->operand);
    }
    else
    {
        ac_report_internal_error("unhandled ast expression, cannot print it");
        AC_ASSERT(0);
    }
}

static void print_identifier(ac_converter_c* c, ac_ast_identifier* identifier)
{
    print_strv(c, identifier->name);
}

static void print_type_specifier(ac_converter_c* c, ac_ast_type_specifier* type_specifier)
{
    print_identifier(c, type_specifier->identifier);
}

static void print_pointers(ac_converter_c* c, int count)
{
    if (count)
    {
        print_str(c, "*");

        count -= 1;
    }
}

static void print_array_specifier(ac_converter_c* c, ac_ast_array_specifier* array_specifier)
{
    print_str(c, "[");
    print_expr(c, array_specifier->size_expression);
    print_str(c, "]");
}

static void print_parameters(ac_converter_c* c, ac_ast_parameters* parameters)
{
    print_str(c, "(");

    ac_ast_expr* next = parameters->list.head;
    while (next)
    {
        print_expr(c, next);
        next = next->next;

        if (next)
        {
            print_str(c, ", ");
        }
    }
    print_str(c, ")");
}

static void print_parameter(ac_converter_c* c, ac_ast_parameter* parameter)
{
    print_identifier(c, parameter->type_name);

    if (parameter->is_var_args)
    {
        print_str(c, "...");
    }
    else if (parameter->declarator)
    {
        print_str(c, " ");
        print_declarator(c, parameter->declarator);
    }
}

static void print_declaration(ac_converter_c* c, ac_ast_declaration* declaration)
{
    /* @OPT: ac_ast_type_DECLARATION_SIMPLE and ac_ast_type_DECLARATION_FUNCTION_DEFINITION are really close to each other
       This could be unified if it makes sense once the compiler is more complete.
    */
    if (declaration->type == ac_ast_type_DECLARATION_FUNCTION_DEFINITION)
    {
        if (declaration->function_block) new_line(c); /* make extra space if it's a function definition */

        print_type_specifier(c, declaration->type_specifier);
        print_str(c, " ");
        print_identifier(c, declaration->declarator->ident);
        print_parameters(c, declaration->declarator->parameters);
        if (declaration->function_block)
        {
            push_brace(c);
            ac_ast_expr* next = declaration->function_block->statements.head;
            while (next)
            {
                print_expr(c, next);
                next = next->next;
            }
            pop_brace(c);
        }

        if (declaration->function_block) new_line(c); /* make extra space if it's a function definition */
    }
    /* Simple declaration. */
    else if (declaration->type == ac_ast_type_DECLARATION_SIMPLE)
    {
        indent(c);
        print_type_specifier(c, declaration->type_specifier);
        print_str(c, " ");
        print_declarator(c, declaration->declarator);
        
        print_str(c, ";");
        print_str(c, "\n");
    }
    else
    {
        ac_report_internal_error("unsupported declaration type %d", declaration->type);
        AC_ASSERT(0);
    }
}

static void print_declarator(ac_converter_c* c, ac_ast_declarator* declarator)
{
    if (declarator->pointer_depth)
    {
        print_pointers(c, declarator->pointer_depth);
        print_str(c, " ");
    }

    /* True declarator contains an identifier, however we also use declarator to handle parameters.
       Some parameters are nameless is some scenario. Example "void func(int);".
    */
    if (declarator->ident)
    {
        print_identifier(c, declarator->ident);
    }

    if (declarator->array_specifier)
    {
        print_array_specifier(c, declarator->array_specifier);
    }

    if (declarator->initializer)
    {
        print_str(c, " = ");
        print_expr(c, declarator->initializer);
    }
    else if (declarator->parameters)
    {
        print_parameters(c, declarator->parameters);
    }
}

static void print_unary_op(ac_converter_c* c, enum ac_token_type type)
{
    switch (type)
    {
    case ac_token_type_AMP:
    case ac_token_type_DOUBLE_MINUS:
    case ac_token_type_DOUBLE_PLUS:
    case ac_token_type_EXCLAM:
    case ac_token_type_MINUS:
    case ac_token_type_PLUS:
    case ac_token_type_STAR:
    case ac_token_type_TILDE:
        break;
    default:
        ac_report_internal_error("unsupported unary operator %d", type);
        AC_ASSERT(0);
        return;
    }
    print_strv(c, ac_token_type_to_strv(type));
}

static void print_binary_op(ac_converter_c* c, enum ac_token_type type)
{
    switch (type)
    {
    case ac_token_type_AMP:
    case ac_token_type_CARET:
    case ac_token_type_CARET_EQUAL:
    case ac_token_type_DOUBLE_AMP:
    case ac_token_type_DOUBLE_GREATER:
    case ac_token_type_DOUBLE_EQUAL:
    case ac_token_type_DOUBLE_LESS:
    case ac_token_type_DOUBLE_PIPE:
    case ac_token_type_EQUAL:
    case ac_token_type_GREATER:
    case ac_token_type_GREATER_EQUAL:
    case ac_token_type_LESS:
    case ac_token_type_LESS_EQUAL:
    case ac_token_type_MINUS:
    case ac_token_type_MINUS_EQUAL:
    case ac_token_type_NOT_EQUAL:
    case ac_token_type_PIPE:
    case ac_token_type_PLUS:
    case ac_token_type_PLUS_EQUAL:
    case ac_token_type_SLASH:
    case ac_token_type_SLASH_EQUAL:
    case ac_token_type_STAR:
    case ac_token_type_STAR_EQUAL:
        break;
    default:
        ac_report_internal_error("unsupported unary operator %d", type);
        AC_ASSERT(0);
        return;
    }
    print_strv(c, ac_token_type_to_strv(type));
}

static void print_fv(ac_converter_c* c, const char* fmt, va_list args)
{
    dstr_append_fv(&c->string_buffer, fmt, args);
}

static void print_f(ac_converter_c* c, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    print_fv(c, fmt, args);
    va_end(args);
}

static void print_str(ac_converter_c* c, const char* str)
{
    print_f(c, "%s", str);
}

static void print_strv(ac_converter_c* c, strv view)
{
    print_f(c, "%.*s", view.size, view.data);
}

static size_t write_to_file(strv str, FILE* f)
{
    return fwrite(str.data, str.size, 1, f);
}

static void push_indent(ac_converter_c* c)
{
    c->indentation_level += 1;
}

static void pop_indent(ac_converter_c* c)
{
    c->indentation_level -= 1;
}

static void indent(ac_converter_c* c)
{
    int remaining = c->indentation_level;
    while (remaining > 0)
    {
        print_f(c, "%s", c->indent_pattern);

        remaining -= 1;
    }
}

static void push_brace(ac_converter_c* c)
{
    new_line(c);
    print_str(c, "{");
    push_indent(c);
    new_line(c);
}

static void pop_brace(ac_converter_c* c)
{
    pop_indent(c);

    new_line(c);
    print_str(c, "}");
    new_line(c);
}

static void new_line(ac_converter_c* c)
{
    print_str(c, "\n");
}