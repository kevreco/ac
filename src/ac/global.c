#include "global.h"

#include <stdarg.h>
#include <stdio.h>

#include <re/dstr_util.h>

#include "ast.h"
#include "location.h"

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

static bool os_std_console_setup();
static bool os_std_console_color_enabled();
static const char* os_std_console_color_red_begin();
static const char* os_std_console_color_end();

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
    assert(surrounding_lines >= 0);

    /* if needed setup console so that it can color the output on errors */
    /* @TODO @FIXME we only need to do it once. */
    if (global_options.colored_output) os_std_console_setup();

    dstr256 message;
    dstr256_init(&message);
    dstr256_assign_fv(&message, fmt, args);

    if (location.filepath)
    {
        fprintf(file, "%s: ", location.filepath);
    }
    if (location_has_row_and_column(location))
    {
        fprintf(file, "%d:%d: ", location.row, location.col);
    }

    if (type != message_type_NONE)
    {
        bool emit_color = os_std_console_color_enabled() && type == message_type_ERROR;

        if (emit_color) fprintf(file, "%s", os_std_console_color_red_begin());

        fprintf(file, "%s ", get_message_prefix(type));

        if (emit_color) fprintf(file, "%s", os_std_console_color_end());
    }

    fprintf(file, "%s", dstr_c_str(&message.dstr));

    if (surrounding_lines > 0 && location_has_row_and_column(location))
    {
        size_t previous_line_count;
        size_t next_line_count;
        strv partial_source = strv_get_surrounding_lines(
            location.content,
            location.content.data + location.pos - 1,
            surrounding_lines,
            &previous_line_count,
            &next_line_count
        );

        char margin_format[64];
        static const char* margin_text = "> ";
        int line_number_text_size = snprintf(0, 0, "%zu", next_line_count);
        // if 'lin_number_text_size' is 3 this should write "%3d> %.*s\n"
        snprintf(margin_format, 64, "%%%dd%s%%.*s", line_number_text_size, margin_text);
        int margin_text_size = line_number_text_size + strlen(margin_text);
        int line_counter = location.row - previous_line_count;

        fprintf(file, "\n");
        strv current_line;

        while (strv_pop_line(&partial_source, &current_line))
        {
            fprintf(file, margin_format, line_counter, current_line.size, current_line.data);

            if (location.row == line_counter)
            {
                if (!strv_ends_with_str(current_line, "\n")) printf("\n");  /* print end of line if necessary before the '^' */

                int margin_char_count = margin_text_size;

                while (margin_char_count--) fprintf(file, " "); /* draw margin for under line */
                print_underline_cursor(file, current_line, location.col - 1); /* draw the '^' under the line */
                printf("\n");
            }

            ++line_counter;
        }

        if (!strv_ends_with_str(current_line, "\n")) printf("\n"); /* print end of line if necessary after last line */

        fprintf(file, "\n");
    }
    else
    {
        if (!strv_ends_with_str(dstr_to_view(&message.dstr), "\n")) printf("\n"); /* print end of line if necessary after last line */
    }

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

/*
-------------------------------------------------------------------------------
os_std_console
-------------------------------------------------------------------------------
*/

#if defined(WIN32)

#include <windows.h>
#include <IntSafe.h>
bool os_std_console_setup()
{
    if (!SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), ENABLE_PROCESSED_OUTPUT | 0x0004))
    {
        fprintf(stderr, "SetConsoleMode error\n");
        return false;
    }
    if (!SetConsoleOutputCP(CP_UTF8))
    {
        fprintf(stderr, "SetConsoleOutputCP error\n");
        return false;
    }

    return true;
}

static bool os_std_console_color_enabled()
{
    DWORD mode;
    if (!GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &mode))
    {
        fprintf(stderr, "GetConsoleMode error\n");
        return false;
    }

    return (mode | ENABLE_PROCESSED_OUTPUT)
        && (mode | 0x0004);
}

static const char* os_std_console_color_red_begin()
{
    return "\x1b[31m";
}

static const char* os_std_console_color_end()
{
    return "\x1b[39m";
}

#else

bool os_std_console_setup()
{
    return false;
}

static bool os_std_console_color_enabled()
{
    return false;
}

static const char* os_std_console_color_red_begin()
{
    return "";
}

static const char* os_std_console_color_end()
{
    return "";
}
#endif /* defined(WIN32) */
