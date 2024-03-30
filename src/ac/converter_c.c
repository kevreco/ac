#include "converter_c.h"

#include <stdio.h> /* FILE */
#include <inttypes.h> /* PRIiMAX, PRIuMAX */

#include <re/file/file.h>

#include "ast.h"
#include "manager.h"

#define CAST_TO(type_, ident_, object_) type_ ident_ = (type_)(object_)

static void print_top_level(struct ac_converter_c* c);
static void print_expr(struct ac_converter_c* c, struct ac_ast_expr* expr);
static void print_identifier(struct ac_converter_c* c, struct ac_ast_identifier* identifier);
static void print_declaration(struct ac_converter_c* c, struct ac_ast_declaration* declaration);

static void print_fv(struct ac_converter_c* c, const char* fmt, va_list args);
static void print_f(struct ac_converter_c* c, const char* fmt, ...);
static void print_str(struct ac_converter_c* c, const char* str);
static void print_dstr_view(struct ac_converter_c* c, dstr_view view);

static size_t write_to_file(dstr_view str, FILE* f);
static void push_indent(struct ac_converter_c* c);
static void pop_indent(struct ac_converter_c* c);
static void indent(struct ac_converter_c* c);
static void push_brace(struct ac_converter_c* c);
static void pop_brace(struct ac_converter_c* c);
static void new_line(struct ac_converter_c* c);

void ac_converter_c_init(struct ac_converter_c* c, struct ac_manager* mgr)
{
    memset(c, 0, sizeof(struct ac_converter_c));

    c->mgr = mgr;
    dstr_init(&c->string_buffer);

    c->indent_pattern = "    ";
}

void ac_converter_c_destroy(struct ac_converter_c* c)
{
    dstr_destroy(&c->string_buffer);
}

void ac_converter_c_convert(struct ac_converter_c* c, const char* filepath)
{
    (void)filepath;

    print_top_level(c);

    FILE* f;
    fopen_s(&f, filepath, "wb");
    write_to_file(dstr_to_view(&c->string_buffer), f);
}

static void print_top_level(struct ac_converter_c* c)
{
    struct ac_ast_top_level* top_level = c->mgr->top_level;

    struct ac_ast_expr* current = NULL;
    for(EACH_EXPR(current, top_level->block.statements))
    {
        print_expr(c, current);
    }
}

static void print_expr(struct ac_converter_c* c, struct ac_ast_expr* expr)
{
    if (ac_ast_is_declaration(expr))
    {
        CAST_TO(struct ac_ast_declaration*, declaration, expr);
        print_declaration(c, declaration);
    }
    else if (expr->type == ac_ast_type_LITERAL_INTEGER)
    {
        // @TODO handle unsigned integer
        CAST_TO(struct ac_ast_literal*, literal, expr);
        /* print maximal size possible value of an integer with PRIiMAX */
        print_f(c, "%" PRIiMAX "", literal->u.integer);
    }
    else if (expr->type == ac_ast_type_RETURN)
    {
        CAST_TO(struct ac_ast_return*, return_, expr);
        indent(c);
        print_str(c, "return ");
        print_expr(c, return_->expr);
        print_str(c, ";");
    }
    else
    {
        assert(0 && "Internal error: unhandled ast type.");
    }
}

static void print_identifier(struct ac_converter_c* c, struct ac_ast_identifier* identifier)
{
    print_dstr_view(c, identifier->name);
}

static void print_declaration(struct ac_converter_c* c, struct ac_ast_declaration* declaration)
{

    if (declaration->type == ac_ast_type_DECLARATION_FUNCTION_DEFINITION)
    {
        if (declaration->function_block) new_line(c); /* make extra space if it's a function definition */

        print_identifier(c, declaration->type_specifier->identifier);
        print_str(c, " ");
        print_identifier(c, declaration->ident);
        print_str(c, "(");
        /* @TODO display arguments */
        print_str(c, ")");
        if (declaration->function_block)
        {
            push_brace(c);
            struct ac_ast_expr* next = declaration->function_block->statements.head;
            while (next)
            {
                print_expr(c, next);
                next = next->next;
            }
            pop_brace(c);
        }

        if (declaration->function_block) new_line(c); /* make extra space if it's a function definition */
    }
    // simple declaration 
    else if (declaration->type == ac_ast_type_DECLARATION_SIMPLE)
    {
        indent(c);

        print_identifier(c, declaration->type_specifier->identifier);
        print_str(c, " ");
        print_identifier(c, declaration->ident);
        if (declaration->initializer)
        {
            print_str(c, " = ");
            print_expr(c, declaration->initializer);
        }
        print_str(c, ";");
        print_str(c, "\n");
    }
    else
    {
        assert(0 && "Internal error: unhandled declaration type");
    }
}

static void print_fv(struct ac_converter_c* c, const char* fmt, va_list args)
{
    dstr_append_fv(&c->string_buffer, fmt, args);
}

static void print_f(struct ac_converter_c* c, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    print_fv(c, fmt, args);
    va_end(args);
}

static void print_str(struct ac_converter_c* c, const char* str)
{
    print_f(c, "%s", str);
}

static void print_dstr_view(struct ac_converter_c* c, dstr_view view)
{
    print_f(c, "%.*s", view.size, view.data);
}

static size_t write_to_file(dstr_view str, FILE* f)
{
    return fwrite(str.data, str.size, 1, f);
}

static void push_indent(struct ac_converter_c* c)
{
    c->indentation_level += 1;
}

static void pop_indent(struct ac_converter_c* c)
{
    c->indentation_level -= 1;
}

static void indent(struct ac_converter_c* c)
{
    int remaining = c->indentation_level;
    while (remaining > 0)
    {
        print_f(c, "%s", c->indent_pattern);

        remaining -= 1;
    }
}

static void push_brace(struct ac_converter_c* c)
{
    new_line(c);
    print_str(c, "{");
    push_indent(c);
    new_line(c);
}

static void pop_brace(struct ac_converter_c* c)
{
    pop_indent(c);

    new_line(c);
    print_str(c, "}");
    new_line(c);
}

static void new_line(struct ac_converter_c* c)
{
    print_str(c, "\n");
}