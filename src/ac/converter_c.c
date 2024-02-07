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

void ac_converter_c_init(struct ac_converter_c* c, struct ac_manager* mgr)
{
    memset(c, 0, sizeof(struct ac_converter_c));

    c->mgr = mgr;
    dstr_init(&c->string_buffer);
}

void ac_converter_c_destroy(struct ac_converter_c* c)
{
    (void)c;
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
    struct ac_ast_top_level* top_level = c->mgr->top_level; // @TODO use proper top level

    struct ac_ast_expr_list* current = &top_level->declarations;
    if (current->value)
    {
        do
        {
            print_expr(c, current->value);
            print_str(c, "\n");
        } while ((current = current->next) != 0 && current->value != 0);
    }
}

static void print_expr(struct ac_converter_c* c, struct ac_ast_expr* expr)
{
    if (expr->type == ac_ast_type_DECLARATION)
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
    else
    {
        // @TODO we should report an error here.
        assert(0 && "unhandled ast type.");
    }
}

static void print_identifier(struct ac_converter_c* c, struct ac_ast_identifier* identifier)
{
    print_dstr_view(c, identifier->name);
}

static void print_declaration(struct ac_converter_c* c, struct ac_ast_declaration* declaration)
{
    print_identifier(c, declaration->type_specifier->identifier);
    print_str(c, " ");
    print_identifier(c, declaration->ident);
    if (declaration->initializer)
    {
        print_str(c, " = ");
        print_expr(c, declaration->initializer);
    }
    print_str(c, ";");
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