#ifndef RE_STRV_EXTENSION_H
#define RE_STRV_EXTENSION_H

/* Unusual functions related to strv. */

/*-----------------------------------------------------------------------*/
/* strv */
/*-----------------------------------------------------------------------*/

/* Ascii to integer. */
STRV_API int strv_atoi(strv sv);

/* Returns a strv with zero size onces there is no next token. */
STRV_API strv strv_tok(strv sv, strv delims);

STRV_API strv strv_line_at(strv sv, size_t pos);

/* Return first line "line" and remove the line from the source "sv" */
STRV_API strv strv_pop_line(strv* sv);

/* Given the source at position 'pos' we get the current line and the N (extra_lines_required) necessary surrounding lines. 
   - previous_line_count contains the previous lines that were extracted before the current one.
   - next_line_count contains the next lines that were extracted after the current one.
*/
STRV_API strv strv_get_surrounding_lines(strv source, size_t pos, size_t extra_lines_required, size_t* previous_line_count, size_t* next_line_count);

/* Should match W3C email regex:
    /^[a-zA-Z0-9.!#$%&’*+/=?^_`{|}~-]+@[a-zA-Z0-9-]+(?:\.[a-zA-Z0-9-]+)*$/
*/
STRV_API bool strv_is_email(strv sv);

/* Compare two string views as if all new lines (\r, \n, and \r\n) were the same
   NOTE: string views are assumed to be null terminated. */
STRV_API bool
strv_equals_newline_insensitive(strv left_str, strv right_str);

/*-----------------------------------------------------------------------*/
/* strv_splitter */
/*-----------------------------------------------------------------------*/

typedef struct strv_splitter strv_splitter;
struct strv_splitter {
    strv str;
    strv delims;
};

STRV_API void strv_splitter_init(strv_splitter* s, strv sv, strv delims);

STRV_API void strv_splitter_init_str(strv_splitter* s, strv sv, const char* delims);

STRV_API strv_splitter strv_splitter_make(strv sv, strv delims);

STRV_API strv_splitter strv_splitter_make_str(strv sv, const char* delims);

STRV_API bool strv_splitter_get_next(strv_splitter* s, strv* res);

#endif /* RE_STRV_EXTENSION_H */

#ifdef STRV_IMPLEMENTATION

static bool char_is_alphanum(char c);

/* Returns true if match regex (.[a-zA-Z0-9_])+ */
static bool parse_email_ending(const char** cursor, const char* end);

static void eat_email_suffix(const char** cursor, const char* end);

/* Returns true if match regex [a-zA-Z0-9_]+ */
static bool parse_email_subdomain(const char** cursor, const char* end);

STRV_API int
strv_atoi(strv sv)
{
    const char* str = sv.data;
    size_t size = sv.size;

    const char* end = str + size;

    int result = 0;

    int bool_is_negative = 0;

    if (*str == '-') {
        bool_is_negative = 1;
        ++str;
    }

    while (*str >= '0' && *str <= '9' && str < end) {
        result = (result * 10) + (*str - '0');
        ++str;
    }

    if (bool_is_negative) {
        result = -result;
    }

    return result;
}

STRV_API strv
strv_tok(strv sv, strv delims)
{
    strv result = strv_make_from(sv.data, 0);

    if (!sv.size) {
        return result;
    }

    const char* start = sv.data;
    const char* end = sv.data + sv.size;

    /* Remove token on the left */
    while (start < end
        && strv_contains_char(delims, start[0]))
    {
        start += 1;
    }

    if (start == end) {
        return result;
    }

    /* Remove token on the right */
    result.data = start;

    while (start < end
        && !strv_contains_char(delims, start[0]))
    {
        start += 1;
        result.size += 1;
    }

    return result;
}

STRV_API strv
strv_line_at(strv sv, size_t pos)
{
    size_t left_index = pos;
    size_t right_index = pos;

    if (sv.size == 0)
    {
        return sv;
    }

    /* If we are on \n and the previous char is \r, we place ourself on the \r instead.
    *  Otherwise the \r will be considered as a previous new line even though it's
    *  within thesame line.
    */
    if (sv.data[left_index] == '\n' && pos > 0 && sv.data[left_index - 1] == '\r')
    {
        left_index -= 1;
    }

    /* Look for the beginning of the string or the previous line. */
    while (left_index > 0)
    {
        if (sv.data[left_index - 1] == '\n' || sv.data[left_index - 1] == '\r')
        {
            break;
        }
        left_index -= 1;
    }

    /* Go to next line ending if we are not already on one */

    while (right_index < sv.size
        && sv.data[right_index] != '\n'
        && sv.data[right_index] != '\r')
    {
        right_index += 1;
    }

    /* We stopped right at the \r or \n or at the end of the line.
    *  If we are on a \r or \n we eat them because we want them in the result.
    */
    if (right_index < sv.size
        && (sv.data[right_index] == '\r' || sv.data[right_index] == '\n'))
    {
        right_index += 1;
    }
    /* If we are on a \n and the previous char was a \r we eat it because want if to be part of the result. */
    if (right_index < sv.size
        && sv.data[right_index - 1] == '\r'
        && sv.data[right_index] == '\n')
    {
        right_index += 1;
    }

    return strv_substr_from(sv, left_index, right_index - left_index);
}

STRV_API strv
strv_pop_line(strv* sv)
{
    size_t pos = 0;
    strv line = strv_line_at(*sv, pos);
    if (line.size)
    {
        sv->data += line.size;
        sv->size -= line.size;
    }
    return line;
}

STRV_API strv
strv_get_surrounding_lines(strv sv, size_t pos, size_t extra_lines_required, size_t* previous_line_count, size_t* next_line_count)
{
    STRV_ASSERT(pos <= sv.size);

    strv line = strv_line_at(sv, pos);
    strv tmp;
    size_t left_pos = line.data - sv.data;
    size_t left_line_count = 0;

    /* Get lines on the left. */
    while (left_pos > 0
        && left_line_count < extra_lines_required)
    {
        tmp = strv_line_at(sv, left_pos - 1);
        left_pos -= tmp.size;
        left_line_count += 1;
    }

    size_t right_pos = line.data - sv.data + line.size;
    size_t right_line_count = 0;

    /* Get lines on the right. */
    while (right_pos < sv.size
        && right_line_count < extra_lines_required)
    {
        tmp = strv_line_at(sv, right_pos + 1);
        right_pos += tmp.size;
        right_line_count += 1;
    }

    *previous_line_count = left_line_count;
    *next_line_count = right_line_count;

    return strv_substr_from(sv, left_pos, right_pos - left_pos);
}

STRV_API bool
strv_is_email(strv sv)
{
    const char* cursor = sv.data;
    const char* end = sv.data + sv.size;

    /* advance to '@' if every character are valid */
    eat_email_suffix(&cursor, end);

    if (cursor >= end) return false;

    if (*cursor != '@') return false; /* must contain '@' */

    ++cursor; /* skip '@' */

    strv view = strv_make_from(cursor, end - cursor);
    size_t first_dot = strv_find_char(view, '.');

    if (first_dot == STRV_NPOS) return false; /* does not contains any dots */

    const char* first_dot_ptr = cursor + first_dot;

    if (!parse_email_subdomain(&cursor, first_dot_ptr)) return false;

    if (cursor == end) return false; /* must not reach the end already */

    if (!parse_email_ending(&cursor, end)) return false;

    if (cursor < end) return false; /* must reach the end */

    return true;
}

STRV_API bool
strv_equals_newline_insensitive(strv left_str, strv right_str)
{
    char* left = (char*)left_str.data;
    char* right = (char*)right_str.data;
    char* left_end = (char*)left_str.data + left_str.size;
    char* right_end = (char*)right_str.data + right_str.size;

    for (;;)
    {
        while ((left < left_end || right < right_end)
            && (left[0] == '\r' || left[0] == '\n')
            && (right[0] == '\r' || right[0] == '\n')) {

            if (left[0] == '\r') ++left;
            if (left[0] == '\n') ++left;

            if (right[0] == '\r') ++right;
            if (right[0] == '\n') ++right;
        }

        if (left >= left_end || right >= right_end) {
            break;
        }
        
        if (*left != *right) {
            return false;
        }

        left += 1;
        right += 1;
    }

    return *left == *right;
}


static bool
char_is_alphanum(char c)
{
    return
        (c >= 'a' && c <= 'z')
        || (c >= 'A' && c <= 'Z')
        || (c >= '0' && c <= '1')
        ;
}


static bool
parse_email_ending(const char** cursor, const char* end)
{
    const char* current = *cursor;

    bool match = 0;
    while (current < end)
    {
        if (*current != '.') return false; /* must contain '.' */
        ++current;  /* skip '.' */

        bool found_char = false;
        while (current < end)
        {
            bool valid = !!(char_is_alphanum(*current)
                || strv_contains_char(strv_make_from_str("-"), *current))
                ;

            if (!valid) {
                break;
            }
            found_char = true;
            match = true;
            ++current;
        }

        if (!found_char)
        {
            break;
        }
    }

    *cursor = current;

    return match;
}

static void
eat_email_suffix(const char** cursor, const char* end)
{
    const char* current = *cursor;
    while (current < end)
    {
        int valid = !!(char_is_alphanum(*current)
            || strv_contains_char(strv_make_from_str(".!#$%&’*+/=?^_`{|}~-"), *current))
            ;

        if (!valid) {
            break;
        }

        ++current;
    }

    *cursor = current;
}

static bool
parse_email_subdomain(const char** cursor, const char* end)
{
    const char* current = *cursor;

    bool match = false;
    while (current < end)
    {
        bool valid = !!(char_is_alphanum(*current)
            || strv_contains_char(strv_make_from_str("-"), *current))
            ;

        if (!valid) {
            break;
        }
        match = true;
        ++current;
    }

    *cursor = current;

    return match;
}

/*-----------------------------------------------------------------------*/
/* strv_splitter */
/*-----------------------------------------------------------------------*/

STRV_API void
strv_splitter_init(strv_splitter* s, strv sv, strv delims)
{
    s->str = sv;
    s->delims = delims;
}

STRV_API void
strv_splitter_init_str(strv_splitter* s, strv sv, const char* delims)
{
    s->str = sv;
    s->delims = strv_make_from_str(delims);
}

STRV_API strv_splitter
strv_splitter_make(strv sv, strv delims)
{
    strv_splitter s;
    strv_splitter_init(&s, sv, delims);
    return s;
}

STRV_API strv_splitter
strv_splitter_make_str(strv sv, const char* delims)
{
    strv_splitter s;
    strv_splitter_init_str(&s, sv, delims);
    return s;
}

STRV_API bool
strv_splitter_get_next(strv_splitter* s, strv* res)
{
    if (s->str.size > 0)
    {
        strv sv = strv_tok(s->str, s->delims);

        s->str.size -= ((sv.data - s->str.data) + sv.size);
        s->str.data = sv.data + sv.size;

        /* If there are remaining char this means a delimiter has been found, in which case we skip it */
        if (s->str.size)
        {
            s->str.size -= 1;
            s->str.data += 1;
        }

        if (sv.size)
        {
            *res = sv;
            return true;
        }
    }

    return false;
}

#endif /* STRV_IMPLEMENTATION */