#include "global.h"

#include <stdarg.h>
#include <stdio.h>

#include "internal.h"
#include "ast.h"

global_options_t global_options;

enum message_type
{
    message_type_NONE,
    message_type_WARNING,
    message_type_ERROR,
    message_type_INTERNAL_ERROR,
    // Preprocessor warning
    message_type_PP_WARNING,
    // Preprocessor error
    message_type_PP_ERROR,
};

static void display_message_v(FILE* file, enum message_type type, ac_location location, int surrounding_lines, const char* fmt, va_list args);
static const char* get_message_prefix(enum message_type type);

static bool location_has_row_and_column(ac_location l);
static void print_underline_cursor(FILE* file, strv line, size_t pos);

#define REPORT_ERROR(message_type) \
    do { \
        va_list args; \
        va_start(args, fmt); \
        ac_location empty_location = ac_location_empty(); \
        int no_surrounding_lines = 0; \
        display_message_v(stderr, message_type, empty_location, no_surrounding_lines, fmt, args); \
        va_end(args); \
    } while (0)
    
#define REPORT_ERROR_LOC(message_type) \
    do { \
        va_list args; \
        va_start(args, fmt); \
        display_message_v(stderr, message_type, loc, global_options.display_surrounding_lines, fmt, args); \
        va_end(args); \
    } while (0)

#ifndef MAX_PATH
#define MAX_PATH 4096
#endif

static char default_system_include[MAX_PATH];

void ac_add_default_system_includes(path_array* items)
{
    if (default_system_include[0] != 0)
    {
        ac_report_internal_error("default system includes already added");
    }

    /* Retrieve default system include path.
       By default, the include/ folder is located next to the binary.
       @TODO: Implement a way to customize it. */
    {
#if _WIN32
        GetModuleFileNameA(NULL, default_system_include, MAX_PATH);
        char* p = strlwr(default_system_include); /* To lower. */
#else
        ssize_t len = readlink("/proc/self/exe", default_system_include, sizeof(default_system_include) - 1);

        if (len == -1)
        {
            ac_report_internal_error("could not get assembly directory");
            return;
        }
        char* p = default_system_include;
#endif 

        p = path_normalize_slashes(p);
        p = path_basename(p);
        int used = p - default_system_include;
        int remaining = MAX_PATH - used;

        if (remaining < strlen("include"))
        {
            ac_report_internal_error("default include path too long.");
            return;
        }

        // Contatenate the executable directory with "include/"
        snprintf(p, remaining, "include");
    }

    darrT_push_back(items, strv_make_from_str(default_system_include));
#if _WIN32
   /* @TODO create and include Windows only headers here. */
#else
    darrT_push_back(items, strv_make_from_str("/usr/local/include"));
    darrT_push_back(items, strv_make_from_str("/usr/include"));
#endif
}

void ac_report_warning(const char* fmt, ...)
{
    REPORT_ERROR(message_type_WARNING);
}

void ac_report_error(const char* fmt, ...)
{
    REPORT_ERROR(message_type_ERROR);
}

void ac_report_internal_error(const char* fmt, ...)
{
    REPORT_ERROR(message_type_INTERNAL_ERROR);
    exit(1);
}

void ac_report_warning_loc(ac_location loc, const char* fmt, ...)
{
    REPORT_ERROR_LOC(message_type_WARNING);
}

void ac_report_error_loc(ac_location loc, const char* fmt, ...)
{
    REPORT_ERROR_LOC(message_type_ERROR);
}

void ac_report_internal_error_loc(ac_location loc, const char* fmt, ...)
{
    REPORT_ERROR_LOC(message_type_INTERNAL_ERROR);
    exit(1);
}

void ac_report_pp_warning_loc(ac_location loc, const char* fmt, ...)
{
    REPORT_ERROR_LOC(message_type_PP_WARNING);
}

void ac_report_pp_error_loc(ac_location loc, const char* fmt, ...)
{
    REPORT_ERROR_LOC(message_type_PP_ERROR);
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

    if (location.filepath.size)
    {
        dstr256_append_f(&message, STRV_FMT ":", STRV_ARG(location.filepath));
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
            dstr256_append_f(&message, "%*d | ", max_line_number_text_size, line_counter);
            dstr256_append_f(&message, STRV_FMT, STRV_ARG(current_line));

            if (location.row == line_counter)
            {
                /* Print end of line if necessary before the '^' */
                if (!strv_ends_with_str(current_line, "\n") && !strv_ends_with_str(current_line, "\r")) {
                    dstr256_append_f(&message, "%s", "\n");
                }

                /* Print some space until the character we want, based on the colomn count. */
                dstr256_append_f(&message, "%*s^\n", max_line_number_text_size + strlen(" | ") + (location.col - 1), "");
            }
            
            ++line_counter;
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
    switch (type)
    {
    case message_type_WARNING: return "warning:";
    case message_type_ERROR: return "error:";
    case message_type_INTERNAL_ERROR: return "internal error:";
    case message_type_PP_WARNING: return "#warning:";
    case message_type_PP_ERROR: return "#error:";
    default: return "";
    }
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

size_t ac_hash(char* str, size_t count)
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