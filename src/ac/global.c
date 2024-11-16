#include "global.h"

#include <stdarg.h>
#include <stdio.h>

#include "ast.h"
#include "location.h"
#include "re_lib.h"

global_options_t global_options;

enum message_type
{
    message_type_NONE,
    message_type_WARNING,
    message_type_ERROR,
};

static void display_message_v(FILE* file, enum message_type type, ac_location location, int surrounding_lines, const char* fmt, va_list args);
static const char* get_message_prefix(enum message_type type);

static bool location_has_row_and_column(ac_location l);
static void print_underline_cursor(FILE* file, strv line, size_t pos);

void ac_report_error(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    ac_location empty_location = ac_location_empty();
    int no_surrounding_lines = 0;
    display_message_v(stderr, message_type_ERROR, empty_location, no_surrounding_lines, fmt, args);

    va_end(args);
}

void ac_report_warning(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    ac_location empty_location = ac_location_empty();
    int no_surrounding_lines = 0;
    display_message_v(stderr, message_type_WARNING, empty_location, no_surrounding_lines, fmt, args);

    va_end(args);
}

void ac_report_error_loc(ac_location loc, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    display_message_v(stderr, message_type_ERROR, loc, global_options.display_surrounding_lines, fmt, args);

    va_end(args);
}

void ac_report_warning_loc(ac_location loc, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    display_message_v(stderr, message_type_WARNING, loc, global_options.display_surrounding_lines, fmt, args);

    va_end(args);
}

void ac_report_error_expr(ac_ast_expr* expr, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    ac_location loc = ac_ast_expr_location(expr);
    display_message_v(stderr, message_type_ERROR, loc, global_options.display_surrounding_lines, fmt, args);

    va_end(args);
}

void ac_report_warning_expr(ac_ast_expr* expr, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    ac_location loc = ac_ast_expr_location(expr);
    display_message_v(stderr, message_type_WARNING, loc, global_options.display_surrounding_lines, fmt, args);

    va_end(args);
}

static void display_message_v(FILE* file, enum message_type type, ac_location location, int surrounding_lines, const char* fmt, va_list args)
{
    AC_ASSERT(surrounding_lines >= 0);

    dstr256 message;
    dstr256_init(&message);

    if (location.filepath)
    {
        dstr256_append_f(&message, "%s:", location.filepath);
    }
    if (location_has_row_and_column(location))
    {
        dstr256_append_f(&message, "%d:%d: ", location.row, location.col);
    }

    if (type != message_type_NONE)
    {
        dstr256_append_f(&message, "%s ", get_message_prefix(type));
    }

    dstr256_append_fv(&message, fmt, args);

    if (surrounding_lines > 0 && location_has_row_and_column(location))
    {
        size_t previous_line_count;
        size_t next_line_count;
        strv partial_source = strv_get_surrounding_lines(
            location.content,
            location.pos,
            surrounding_lines,
            &previous_line_count,
            &next_line_count
        );

        int line_counter = location.row - previous_line_count;
        int max_line_number_text_size = snprintf(0, 0, "%zu", location.row + next_line_count);

        dstr256_append_f(&message, "%s", "\n");

        strv current_line;
        while ((current_line = strv_pop_line(&partial_source)).size != 0)
        {
            dstr256_append_f(&message, "%*d> ", max_line_number_text_size, line_counter);
            dstr256_append_f(&message, "%.*s", (int)current_line.size, current_line.data);

            if (location.row == line_counter)
            {
                /* Print end of line if necessary before the '^' */
                if (!strv_ends_with_str(current_line, "\n") && !strv_ends_with_str(current_line, "\r")) {
                    dstr256_append_f(&message, "%s", "\n");
                }

                /* Print some space until the character we want, based on the colomn count. */
                dstr256_append_f(&message, "%*s^\n", max_line_number_text_size + strlen("> ") + (location.col - 1), "");
            }
            
            ++line_counter;
        }

        /* print end of line if necessary after last line. */
        if (!strv_ends_with_str(current_line, "\n") && !strv_ends_with_str(current_line, "\r")) {
            dstr256_append_f(&message, "%s", "\n");
        }
    }

    /* print end of line if necessary after last line. */
    if (!strv_ends_with_str(dstr_to_strv(&message.dstr), "\n") && !strv_ends_with_str(dstr_to_strv(&message.dstr), "\r")) {
        dstr256_append_f(&message, "%s", "\n");
    }

    fprintf(file, "%s", message.dstr.data);

    dstr256_destroy(&message);
}

static const char* get_message_prefix(enum message_type type)
{
    if (type == message_type_ERROR) return "error:";
    if (type == message_type_WARNING) return "warning:";

    return "";
}

static bool location_has_row_and_column(ac_location l) {
    return l.row > -1 && l.col > -1 && l.pos > -1;
}

static inline bool _is_whitespace(char c) {
    return (c == ' ' || c == '\n'
        || c == '\t' || c == '\r'
        || c == '\f' || c == '\v');
}

static void print_underline_cursor(FILE* file, strv line, size_t pos)
{
    const char* cursor = line.data;
    const char* end = cursor + pos;;
    while (cursor < end)
    {
        char ch = *cursor;
        fprintf(file, "%c", _is_whitespace(ch) ? ch : ' ');
        ++cursor;
    }
    printf("^");
}

size_t ac_djb2_hash(char* str, size_t count)
{
    size_t hash = AC_HASH_INIT;
    size_t i = 0;
    while (i < count)
    {
        hash = AC_HASH(hash, str[i]);
        i++;
    }

    return hash;
}