#include "lexer.h"

#include "float.h"  /* FLT_MAX */

#include "global.h"

#define AC_EOF ('\0')

enum base_type {
    base2 = 2,
    base8 = 8,
    base10 = 10,
    base16 = 16,
};

static const strv strv_error = {0, 0};
static const strv empty = STRV("");
/* char and string literal prefix. */
static const strv no_prefix = STRV("");
static const strv utf8 = STRV("u8");
static const strv utf16 = STRV("u");
static const strv utf32 = STRV("U");
static const strv wide = STRV("L");

static ac_token_info token_infos[ac_token_type_COUNT];

static bool is_horizontal_whitespace(char c);      /* char is alphanumeric */
static bool is_identifier(char c);                 /* char is allowed in identifier */
static bool is_eof(const ac_lex* l);               /* current char is end of line */
static bool is_char(const ac_lex* l, char c);      /* current char is equal to char  */
static bool is_not_char(const ac_lex* l, char c);  /* current char is not equal to char  */
static bool is_str(const ac_lex* l, const char* str, size_t count); /* current char is equal to string to string */
static bool next_is(const ac_lex* l, char c);      /* next char equal to */
static bool next_next_is(const ac_lex* l, char c); /* next next char equal to */

static int consume_one(ac_lex* l);     /* Goto next char and keep up with location of the token. */
static void skip_newlines(ac_lex* l);  /* Deal with \n, an \r and \r\n. \r\n should be skipped at the same time. */
static bool skip_comment(ac_lex* l);         /* Skip C comment. */
static void skip_inline_comment(ac_lex* l);  /* Skip inline comment. */
static int skip_if_splice(ac_lex* l);        /* Get character after the current splice or return the current character. */
static int next_char_no_splice(ac_lex* l);   /* Get next character ignoring splices. */
static int next_digit(ac_lex* l);         /* Get next digit, ignoring quotes and underscores. Push the digit to the token_buf. */

static ac_token* token_from_text(ac_lex* l, enum ac_token_type type, strv text); /* set current token and got to next */
static ac_token* token_error(ac_lex* l); /* set current token to error and returns it. */
static ac_token* token_eof(ac_lex* l);   /* set current token to eof and returns it. */
static ac_token* token_from_type(ac_lex* l, enum ac_token_type type);
static ac_token* token_from_single_char(ac_lex* l, enum ac_token_type type); /* set current token and got to next */

static double power(double base, unsigned int exponent);
static bool is_binary_digit(char c);
static bool is_octal_digit(char c);
static bool is_decimal_digit(char c);
static bool is_hex_digit(char c);

static bool parse_integer_suffix(ac_lex* l, ac_token_number* num); /* Parse integer suffix like 'uLL' */
static bool parse_float_suffix(ac_lex* l, ac_token_number* num);   /* Parse float suffix like 'f' or 'l' */
static ac_token* token_integer_literal(ac_lex* l, ac_token_number num); /* Create integer token from parsed value. */
static ac_token* token_float_literal(ac_lex* l, ac_token_number num);   /* Create float token from parsed value. */
static ac_token* parse_float_literal(ac_lex* l, ac_token_number num, enum base_type base); /* Parse float after the whole number part trying to parse the dot. */
static ac_token* parse_float_literal_core(ac_lex* l, ac_token_number num, enum base_type base, bool parse_fractional_part); /* Parse float after the whole number part. */
static int hex_string_to_int(const char* c, size_t len);
static ac_token* parse_integer_or_float_literal(ac_lex* l, int previous, int c);

static void* utf8_decode(void* p, int32_t* pc);

static strv string_or_char_literal_to_buffer(ac_lex* l, char quote, dstr* str);
static ac_token* parse_string_literal(ac_lex* l, strv prefix);
static ac_token* token_string(ac_lex* l, strv literal, strv kind);

static ac_token* token_char(ac_lex* l, strv prefix);

static size_t token_str_len(enum ac_token_type type);
static bool token_type_is_literal(enum ac_token_type type);

static void location_increment_row(ac_location* l, int char_count);
static void location_increment_column(ac_location* l, int char_count);

/*
-------------------------------------------------------------------------------
w_lex
-------------------------------------------------------------------------------
*/

void ac_lex_init(ac_lex* l, ac_manager* mgr)
{
    /* Construct. */
    {
        memset(l, 0, sizeof(ac_lex));

        l->mgr = mgr;

        l->options.reject_hex_float = mgr->options.reject_hex_float;
    }

    dstr_init(&l->tok_buf);
    dstr_init(&l->str_buf);
}

void ac_lex_destroy(ac_lex* l)
{
    dstr_destroy(&l->str_buf);
    dstr_destroy(&l->tok_buf);
    memset(l, 0, sizeof(ac_lex));
}

void ac_lex_set_content(ac_lex* l, strv content, strv filepath)
{
    AC_ASSERT(content.data);
    AC_ASSERT(content.size);

    l->filepath = filepath;

    ac_location_init_with_file(&l->location, filepath, content);

    if (content.data && content.size) {
        l->src = content.data;
        l->end = content.data + content.size;
        l->cur = content.data;
        l->len = content.size;
    }

    l->beginning_of_line = true;
}

ac_token* ac_lex_goto_next(ac_lex* l)
{
    memset(&l->token, 0, sizeof(ac_token));

    l->leading_location = l->location;

    /* We need to loop in few occasions:
       - After a splice.
       - After comment (if they need be skipped).
    */
    for (;;)
    {
        int c;
        c = l->cur[0];
        switch (c) {

        case '\\':
            if (!next_is(l, '\n')
                && !next_is(l, '\r')
                && !next_is(l, '\0'))
            {
                return token_from_single_char(l, ac_token_type_BACKSLASH);
            }

            skip_if_splice(l);
            break; /* Will continue on the next char. */
        case ' ':
        case '\t':
        case '\f':
        case '\v': {
            /* Parse group of horizontal whitespace. */
            const char* start = l->cur;
            while (is_horizontal_whitespace(l->cur[0])) {
                consume_one(l);
            }
           
           return token_from_text(l, ac_token_type_HORIZONTAL_WHITESPACE, strv_make_from(start, l->cur - start));
        }

        case '\n':
        case '\r':
        {
            /* Parse single line ending: \n or \r or \r\n */
            const char* start = l->cur;
            skip_newlines(l);
            return token_from_text(l, ac_token_type_NEW_LINE, strv_make_from(start, l->cur - start));
        }

        case '[': return token_from_single_char(l, ac_token_type_SQUARE_L);
        case ']': return token_from_single_char(l, ac_token_type_SQUARE_R);
        case '(': return token_from_single_char(l, ac_token_type_PAREN_L);
        case ')': return token_from_single_char(l, ac_token_type_PAREN_R);
        case '{': return token_from_single_char(l, ac_token_type_BRACE_L);
        case '}': return token_from_single_char(l, ac_token_type_BRACE_R);
        case ':': return token_from_single_char(l, ac_token_type_COLON);
        case ';': return token_from_single_char(l, ac_token_type_SEMI_COLON);
        case ',': return token_from_single_char(l, ac_token_type_COMMA);
        case '?': return token_from_single_char(l, ac_token_type_QUESTION);
        case '@': return token_from_single_char(l, ac_token_type_AT);

        case '#': {
            c = next_char_no_splice(l); /* Skip '#' */
            if (c == '#') {
                c = next_char_no_splice(l); /* Skip '#' */
                return token_from_type(l, ac_token_type_DOUBLE_HASH);
            }
            bool bol = l->beginning_of_line;
            ac_token* t = token_from_type(l, ac_token_type_HASH);
            t->beginning_of_line = bol;
            return t;
        }
        case '=': {
            c = next_char_no_splice(l); /* Skip '=' */
            if (c == '=') {
                c = next_char_no_splice(l); /* Skip '=' */
                return token_from_type(l, ac_token_type_DOUBLE_EQUAL);
            }
            return token_from_type(l, ac_token_type_EQUAL);
        }

        case '!': {
            c = next_char_no_splice(l); /* Skip '!' */
            if (c == '=') {
                c = next_char_no_splice(l); /* Skip '=' */
                return token_from_type(l, ac_token_type_NOT_EQUAL);
            }
            return token_from_type(l, ac_token_type_EXCLAM);
        }

        case '<': {
            c = next_char_no_splice(l); /* Skip '<' */
            if (c == '<') {
                c = next_char_no_splice(l); /* Skip '<' */
                return token_from_type(l, ac_token_type_DOUBLE_LESS);
            }
            else if (c == '=') {
                c = next_char_no_splice(l); /* Skip '=' */
                return token_from_type(l, ac_token_type_LESS_EQUAL);
            }
            return token_from_type(l, ac_token_type_LESS);
        }

        case '>': {
            c = next_char_no_splice(l); /* Skip '>' */
            if (c == '>') {
                c = next_char_no_splice(l); /* Skip '>' */
                return token_from_type(l, ac_token_type_DOUBLE_GREATER);
            }
            else if (c == '=') {
                c = next_char_no_splice(l); /* Skip '=' */
                return token_from_type(l, ac_token_type_GREATER_EQUAL);
            }
            return token_from_type(l, ac_token_type_GREATER);
        }

        case '&': {
            c = next_char_no_splice(l); /* Skip '&' */
            if (c == '&') {
                c = next_char_no_splice(l); /* Skip '&' */
                return token_from_type(l, ac_token_type_DOUBLE_AMP);
            }
            else if (c == '=') {
                c = next_char_no_splice(l); /* Skip '=' */
                return token_from_type(l, ac_token_type_AMP_EQUAL);
            }
            return token_from_type(l, ac_token_type_AMP);
        }

        case '|': {
            c = next_char_no_splice(l); /* Skip '|' */
            if (c == '|') {
                c = next_char_no_splice(l); /* Skip '|' */
                return token_from_type(l, ac_token_type_DOUBLE_PIPE);
            }
            else if (c == '=') {
                c = next_char_no_splice(l); /* Skip '=' */
                return token_from_type(l, ac_token_type_PIPE_EQUAL);
            }
            return token_from_type(l, ac_token_type_PIPE);
        }

        case '+': {
            c = next_char_no_splice(l); /* Skip '+' */
            if (c == '=') {
                c = next_char_no_splice(l); /* Skip '=' */
                return token_from_type(l, ac_token_type_PLUS_EQUAL);
            }
            return token_from_type(l, ac_token_type_PLUS);
        }
        case '-': {
            c = next_char_no_splice(l); /* Skip '-' */
            if (c == '=') {
                c = next_char_no_splice(l); /* Skip '=' */
                return token_from_type(l, ac_token_type_MINUS_EQUAL);
            }
            else if (c == '>') {
                c = next_char_no_splice(l); /* Skip '>' */
                return token_from_type(l, ac_token_type_ARROW);
            }
            return token_from_type(l, ac_token_type_MINUS);
        }

        case '*': {
            c = next_char_no_splice(l); /* Skip '*' */
            if (c == '=') {
                c = next_char_no_splice(l); /* Skip '=' */
                return token_from_type(l, ac_token_type_STAR_EQUAL);
            }
            return token_from_type(l, ac_token_type_STAR);
        }
        case '~': {
            c = next_char_no_splice(l); /* Skip '~' */
            if (c == '=') {
                c = next_char_no_splice(l); /* Skip '=' */
                return token_from_type(l, ac_token_type_TILDE_EQUAL);
            }
            return token_from_type(l, ac_token_type_TILDE);
        }


        case '/': {
            c = next_char_no_splice(l); /* Skip '/' */
            if (c == '=') {
                c = next_char_no_splice(l); /* Skip '=' */
                return token_from_type(l, ac_token_type_SLASH_EQUAL);
            }
            else if (c == '/') {  /* Parse inline comment. */
                const char* start = l->cur - 1;

                skip_inline_comment(l);

                if (l->mgr->options.preserve_comment) {
                    return token_from_text(l, ac_token_type_COMMENT, strv_make_from(start, l->cur - start));
                }
                continue; /* Go to next token. */
            }
            else if (c == '*') {  /* Parse C comment. */
                const char* start = l->cur - 1;

                if (!skip_comment(l)) {
                    return token_error(l);
                }

                if (l->mgr->options.preserve_comment) {
                    return token_from_text(l, ac_token_type_COMMENT, strv_make_from(start, l->cur - start));
                }
                continue; /* Go to next token. */
            }

            return token_from_type(l, ac_token_type_SLASH);
        }

        case '%': {
            c = next_char_no_splice(l); /* Skip '%' */
            if (c == '=') {
                c = next_char_no_splice(l); /* Skip '=' */
                return token_from_type(l, ac_token_type_PERCENT_EQUAL);
            }
            return token_from_type(l, ac_token_type_PERCENT);
        }

        case '^': {
            c = next_char_no_splice(l); /* Skip '^' */
            if (c == '=') {
                c = next_char_no_splice(l); /* Skip '=' */
                return token_from_type(l, ac_token_type_CARET_EQUAL);
            }

            return token_from_type(l, ac_token_type_CARET);
        }

        case '.': {
            dstr_clear(&l->tok_buf);
            dstr_append_char(&l->tok_buf, c);
            c = next_digit(l); /* Skip '.' */

            /* Float can also starts with a dot. */
            if (c >= '0' && c <= '9')
            {
                ac_token_number num = { 0 };
                bool parse_fractional_part = true;
                return parse_float_literal_core(l, num, base10, parse_fractional_part);
            }

            if (c == '.') {
                c = next_char_no_splice(l); /* Skip '.' */
                if (c == '.') {
                    c = next_char_no_splice(l); /* Skip '.' */
                    return token_from_type(l, ac_token_type_TRIPLE_DOT);
                }
                return token_from_type(l, ac_token_type_DOUBLE_DOT);
            }
            return token_from_type(l, ac_token_type_DOT);
        }

        case '"': return parse_string_literal(l, no_prefix);
        case '\'': return token_char(l, no_prefix);
        case '\0': return token_eof(l);

        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
        {
            dstr_clear(&l->tok_buf);
            dstr_append_char(&l->tok_buf, c);
            int next = next_digit(l); /* Skip first number ang get the following one. */
            return parse_integer_or_float_literal(l, c, next);
        }

        case 'a': case 'b': case 'c': case 'd': case 'e':
        case 'f': case 'g': case 'h': case 'i': case 'j':
        case 'k': case 'l': case 'm': case 'n': case 'o':
        case 'p': case 'q': case 'r': case 's': case 't':
        case 'u': case 'v': case 'w': case 'x': case 'y':
        case 'z':
        case 'A': case 'B': case 'C': case 'D': case 'E':
        case 'F': case 'G': case 'H': case 'I': case 'J':
        case 'K': case 'L': case 'M': case 'N': case 'O':
        case 'P': case 'Q': case 'R': case 'S': case 'T':
        case 'U': case 'V': case 'W': case 'X': case 'Y':
        case 'Z':
        case '_':
        {
        parse_identifier:
            AC_ASSERT(is_identifier(l->cur[0]));

            size_t hash = AC_HASH_INIT;

            const char* start = l->cur;
            int n = 0;
            do {
                ++n;
                hash = AC_HASH(hash, l->cur[0]); /* Calculate hash while we parse the identifier. */
                /* @OPT: If this a "bottleneck" of the lexer, it can be improved.
                consume_one can be replaced with a more lightweight function
                that does not care about new_lines. */
                consume_one(l);
            } while (is_identifier(l->cur[0]));

            ac_token token;
            strv ident;
            if (is_char(l, '\\')) /* Stray found. We need to create a new string without it and reparse the identifier. */
            {
                dstr_assign(&l->tok_buf, strv_make_from(start, l->cur - start));
                int c = skip_if_splice(l);

                while (is_identifier(c))
                {
                    hash = AC_HASH(hash, l->cur[0]);
                    dstr_append_char(&l->tok_buf, c);
                    c = next_char_no_splice(l);
                }

                ident = dstr_to_strv(&l->tok_buf);
            }
            else
            {
                ident = strv_make_from(start, n);
            }

            if (l->cur[0] == '\'')  /* Handle char literal */
            {
                if (strv_equals(ident, utf8)) return token_char(l, utf8);
                else if (strv_equals(ident, utf16)) return token_char(l, utf16);
                else if (strv_equals(ident, utf32)) return token_char(l, utf32);
                else if (strv_equals(ident, wide)) return token_char(l, wide);
            }

            if (l->cur[0] == '"') /* Handle string literal. */
            {
                if (strv_equals(ident, utf8)) return parse_string_literal(l, utf8);
                else if (strv_equals(ident, utf16)) return parse_string_literal(l, utf16);
                else if (strv_equals(ident, utf32)) return parse_string_literal(l, utf32);
                else if (strv_equals(ident, wide)) return parse_string_literal(l, wide);
            }

            ac_ident_holder id = ac_create_or_reuse_identifier_h(l->mgr, ident, hash);
            l->token.type = (enum ac_token_type)id.token_type; /* Is and identifier or a keyword. */
            l->token.ident = id.ident;
            return &l->token;
        }

    before_default:
        default: {
            if (l->cur[0] >= 0x80 && l->cur[0] <= 0xFF) /* Start of utf8 identifiers */
            {
                goto parse_identifier;
            }
            ac_report_internal_error("unhandled character: %c", l->cur[0]);
            return token_error(l);

        } /* end default case */

        } /* end switch */
    }
    return token_error(l);
}

ac_token ac_lex_token(ac_lex* l) {
    return l->token;
}

ac_token* ac_lex_token_ptr(ac_lex* l) {
    return &l->token;
}

bool ac_lex_expect(ac_lex* l, enum ac_token_type type) {
    ac_location current_location = l->location;
    ac_token current = l->token;
    if (current.type != type)
    {
        strv expected = ac_token_type_to_strv(type);
        strv actual = ac_token_to_strv(current);

        ac_report_error_loc(current_location, "syntax error: expected '%.*s', actual '%.*s'"
            , expected.size, expected.data
            , actual.size, actual.data
        );

        return false;
    }
    return true;
}

void ac_lex_swap(ac_lex* left, ac_lex* right)
{
    ac_lex tmp = *left;
    *left = *right;
    *right = tmp;
}

ac_lex_state ac_lex_save(ac_lex* l)
{
    ac_lex_state s;

    s.filepath = l->filepath;
    s.src = l->src;
    s.end = l->end;
    s.cur = l->cur;

    s.len = l->len;

    s.token = l->token;
    s.leading_location = l->leading_location;
    s.location = l->location;
    s.beginning_of_line = l->beginning_of_line;

    return s;
}

void ac_lex_restore(ac_lex* l, ac_lex_state* s)
{
    l->filepath = s->filepath;
    l->src = s->src;
    l->end = s->end;
    l->cur = s->cur;

    l->len = s->len;

    l->token = s->token;
    l->leading_location = s->leading_location;
    l->location = s->location;
    l->beginning_of_line = s->beginning_of_line;
}

/*
    NOTE: We want to go as fast as possible to skip preprocessor block.
    However, we still need to tokenize directive identifiers because we need to count nested #if/#endif
    We also need to skip comments and string literal because we don't care about #endif within comments and string literals.

    Example of the issue:
        #if 1
          
           #if 0
           #endif
           //#endif
           "\
           #endif"
           '\
           #endif'

        #endif
*/
ac_token* ac_skip_preprocessor_block(ac_lex* l, bool was_end_of_line)
{
    int c;
    int nesting_level = 0;
    ac_location loc = l->location;

    for (;;)
    {
        c = *l->cur;
        switch (c) {
        case '\0': /* We should not encounter EOF in a preprocessor block. */
            return token_eof(l);
        case '\r':
        case '\n':
            skip_newlines(l);
            was_end_of_line = true;
            continue;
        case '/':
            c = consume_one(l);
            if (c == '*') {
                skip_comment(l);
                break;
            }
            else if (c == '/') {
                skip_inline_comment(l);
                break;
            }
            else
            {
                was_end_of_line = false;
                break;
            }
            /* Fallthrough */
        case ' ':
        case '\t':
        case '\f':
        case '\v':
        {
            consume_one(l);
            continue;
        }
        case '\'':
        case '"':
        {
            consume_one(l); /* Skip '"' or '''. */
            /* Consume the string or char literal but do not create any token. */
            string_or_char_literal_to_buffer(l, c, NULL);
            continue;
        }
        case '#':
            if (was_end_of_line)
            {
                consume_one(l); /* Skip '#'. */
                ac_token* t;

                /* Skip all whitespace and comment. */
                do {
                    t = ac_lex_goto_next(l);
                } while (t->type == ac_token_type_HORIZONTAL_WHITESPACE
                    || t->type == ac_token_type_COMMENT);
                
                
                bool is_ending_token = t->type == ac_token_type_ELSE
                    || t->type == ac_token_type_ELIF
                    || t->type == ac_token_type_ELIFDEF
                    || t->type == ac_token_type_ELIFNDEF
                    || t->type == ac_token_type_ENDIF;

                if (nesting_level == 0 && is_ending_token)
                {
                    return t;
                }

                bool is_starting_token = t->type == ac_token_type_IF
                    || t->type == ac_token_type_IFDEF
                    || t->type == ac_token_type_IFNDEF;
                if (is_starting_token)
                    nesting_level += 1;
                else if (t->type == ac_token_type_ENDIF)
                    nesting_level -= 1;

                continue;
            }
            /* Fallthrough */
        default:
            consume_one(l);
            was_end_of_line = false;
        }
    }
    AC_ASSERT(0 && "Unreachable");
    return token_eof(l);
}

ac_token* ac_parse_include_path(ac_lex* l)
{
    /* Current token is '<' and current char is the one following it. */
    AC_ASSERT(l->token.type == ac_token_type_LESS);

    l->leading_location = l->location;

    strv literal = string_or_char_literal_to_buffer(l, '>', &l->tok_buf);
    if (literal.data == strv_error.data)
    {
        return token_error(l);
    }

    return token_string(l, literal, no_prefix);
}

static ac_token eof = {ac_token_type_EOF};

ac_token* ac_token_eof()
{
    return &eof;
}

ac_token* ac_set_token_error(ac_lex* l)
{
    return token_error(l);
}

static inline bool is_horizontal_whitespace(char c) {
    return (c == ' ' || c == '\t' || c == '\f' || c == '\v');
}

static inline bool is_identifier(char c) {

    return (c >= 'a' && c <= 'z')
        || (c >= 'A' && c <= 'Z')
        || (c >= '0' && c <= '9')
        || (c == '_')
        || (unsigned char)c >= 128; /* Is utf-8 */
}

static inline bool is_eof(const ac_lex* l) {
    return l->cur == l->end || *l->cur == '\0';
}

static bool is_char(const ac_lex* l, char c) {
    return l->cur[0] == c;
}

static bool is_not_char(const ac_lex* l, char c) {
    return l->cur[0] != c;
}

static bool is_str(const ac_lex* l, const char* str, size_t count) {
    return strncmp(l->cur, str, count) == 0;
}

static bool next_is(const ac_lex* l, char c) {
    return l->cur + 1 < l->end
        && *(l->cur + 1) == c;
}

static bool next_next_is(const ac_lex* l, char c) {
    return l->cur + 2 < l->end
        && *(l->cur + 2) == c;
}

/* We consume one char at a time to handle new lines and row/line numbers which change the location of tokens. */
static inline int consume_one(ac_lex* l) {
    l->cur++;
    location_increment_column(&l->location, 1);
    return l->cur[0];
}

static void skip_newlines(ac_lex* l) {

    switch (l->cur[0])
    {
    case '\n':
    {
        l->cur += 1;
        location_increment_row(&l->location, 1);
        break;
    }

    case '\r':
    {
        int count = l->cur[1] == '\n' ? 2 : 1;
        l->cur += count;
        location_increment_row(&l->location, count);
        break;
    }
    default:
        AC_ASSERT(0 && "Unreachable");
    }
}

/* Skip comments and make sure the column number and row number are updated.
   This method is ugly due to efficieny reason.
   We don't want to skip comment being slow.
   The content of a comment can be quite large and the number of comments can also be quite a lot. */
static bool skip_comment(ac_lex* l)
{
    ac_location location = l->location;
    int line = 0;
    int column = location.col;
    const char* anchor = l->cur;

    /* Skip '*' */
    l->cur += 1;
    column += 1;

    bool result = true;
    for(;;)
    {
        /* Skip uninteresting chars. We only care about new lines, EOF and the closing comment tag. */
        while (l->cur[0] != '\0' && l->cur[0] != '\r' && l->cur[0] != '\n' && l->cur[0] != '*')
        {
            l->cur += 1;
            column += 1;
        }

        switch (l->cur[0])
        {
        case '\0':
        {
            ac_report_error_loc(location, "unterminated comment starting with '/*'");

            result = false;
            goto exit;
        }
        case '\r':
        {
            
            if (l->cur[1] == '\n')
            {
                // Advance of the extra \n here, the \r will be handled in the FALLTRHOUGH
                l->cur += 1;
            }
            /* FALLTHROUGH */
        }
        case '\n':
        {
            l->cur += 1;
            /* reset column and increase row. */
            line += 1;
            column = 0;
            break;
        }
        case '*':
        {
            l->cur += 1;
            column += 1;
            if (l->cur[0] == '/')
            {
                l->cur += 1;
                column += 1;
                /* Found end on comment */
                result = true;
                goto exit;
            }
            break;
        }
        }
    }

exit:
    l->location.row += line;
    l->location.col = column;
    l->location.pos += l->cur - anchor;

    return result;
}

static void skip_inline_comment(ac_lex* l)
{
    consume_one(l); /* Skip '/' */

    /* Advance until EOF or end of line */
    while (l->cur[0] != '\0' && l->cur[0] != '\n' && l->cur[0] != '\r')
    {
        consume_one(l);
    }
}

static int skip_if_splice(ac_lex* l)
{
    AC_ASSERT(l->cur[0] == '\\');

    while (l->cur[0] == '\\')
    {
        switch (l->cur[1])
        {
        case '\n': {
            l->cur += 2; /* For the '\' and for the '\n' */
            location_increment_row(&l->location, 2);
            break;
        }
        case '\r': {
            int count = l->cur[2] == '\n' ? 3 : 2;
            l->cur += count;
            location_increment_row(&l->location, count);
            break;
        }
        default: {
            return l->cur[0]; /* Return the '\' */
        }
        }
    }
    return l->cur[0];
}

static int next_char_no_splice(ac_lex* l)
{
    int c;
    c = consume_one(l);
    if (c == '\\') {
        c = skip_if_splice(l);
    }
    return c;
}

static int next_digit(ac_lex* l)
{
    int c = next_char_no_splice(l);

    while (c == '\'' || c == '_') {
        dstr_append_char(&l->tok_buf, c);
        c = next_char_no_splice(l);
    }
    dstr_append_char(&l->tok_buf, c);
    return c;
}

static ac_token* token_from_text(ac_lex* l, enum ac_token_type type, strv text) {
    l->token.type = type;
    l->token.text = text;

    switch (type)
    {
    case ac_token_type_NEW_LINE: {
        l->beginning_of_line = true;
        break;
    }
    case ac_token_type_HORIZONTAL_WHITESPACE:
    case ac_token_type_COMMENT: {
        /* Do nothing, preious_was_end_of_line should not be changed if there are horizontal whitespaces. */
        break;
    }
    default:
        l->beginning_of_line = false;
    }
    
    return &l->token;
}

static ac_token* token_error(ac_lex* l) {
    l->token.type = ac_token_type_EOF;
    l->token.is_premature_eof = true;
    return &l->token;
}

static ac_token* token_eof(ac_lex* l) {
    l->token.type = ac_token_type_EOF;
    return &l->token;
}

static ac_token* token_from_type(ac_lex* l, enum ac_token_type type) {
    return token_from_text(l, type, token_infos[type].ident.text);
}

static ac_token* token_from_single_char(ac_lex* l, enum ac_token_type type) {
    ac_token* t = token_from_type(l, type);
    consume_one(l);
    return t;
}

static double power(double base, unsigned int exponent)
{
    double value = 1;
    for (; exponent; exponent >>= 1) {
        if (exponent & 1)
            value *= base;
        base *= base;
    }
    return value;
}

static bool is_binary_digit(char c) {
    return c == '0' || c == '1';
}
static bool is_octal_digit(char c) {
    return c >= '0' && c <= '7';
}
static bool is_decimal_digit(char c) {
    return c >= '0' && c <= '9';
}
static bool is_hex_digit(char c) {
    return (c >= '0' && c <= '9')
        || (c >= 'a' && c <= 'f')
        || (c >= 'A' && c <= 'F');
}

static bool parse_integer_suffix(ac_lex* l, ac_token_number* num)
{
    int c = l->cur[0];

    int U = 0;
    int L = 0;

    while ((c == 'u' || c == 'U' || c == 'l' || c == 'L')
       && (U + L) <= 3)  /* Maximum number of character is 3 */
    {
        switch (c)
        {
        case 'u':
        case 'U':
        {
            U++;
            if (U > 1) {
                ac_report_error_loc(l->location, "invalid integer suffix. Too many 'u' or 'U'");
                return false;
            }
            num->is_unsigned = true;
            c = next_digit(l);
            break;
        }
        case 'l':
        case 'L':
        {
            L++;
            if (L > 2) {
                ac_report_error_loc(l->location, "invalid integer suffix, too many 'l' or 'L'");
                return false;
            }
            else {
                num->long_depth += 1;
            }

            c = next_digit(l);
        }
        }
    }

    if ((U + L) > 3
        || (c >= '0' && c <= '9')
        || (c >= 'a' && c <= 'z')
        || (c >= 'A' && c <= 'Z'))
    {
        ac_report_error_loc(l->location, "invalid integer suffix: '%c'", c);
        return false;
    }
    return true;
}

/* @TODO handle DF, DD and DL suffixes.
   en.cppreference.com/w/c/language/floating_constant#Suffixes */
static bool parse_float_suffix(ac_lex* l, ac_token_number* num) {
    int c = l->cur[0];

    int F = 0;
    int L = 0;

    while ((c == 'f' || c == 'F' || c == 'l' || c == 'L')
        && (F + L) <= 1)  /* Maximum number of character is 1 */
    {
        switch (c)
        {
        case 'f':
        case 'F':
        {
            F++;
            if (F > 1) {
                ac_report_error_loc(l->location, "invalid float suffix, too many 'f' or 'F'");
                return false;
            }
            num->is_float = true;
            c = next_digit(l);
            break;
        }
        case 'l':
        case 'L':
        {
            L++;
            if (L > 1) {
                ac_report_error_loc(l->location, "invalid float suffix, too many 'l' or 'L'");
                return false;
            }
            else {
                num->is_double = true;
            }

            c = next_digit(l);
        }
        }
    }

    if ((c >= '0' && c <= '9')
        || (c >= 'a' && c <= 'z')
        || (c >= 'A' && c <= 'Z'))
    {
        ac_report_error_loc(l->location, "invalid float suffix: '%c'", c);
        return false;
    }
    return true;
}

static ac_token* token_integer_literal(ac_lex* l, ac_token_number num)
{
    l->token.type = ac_token_type_LITERAL_INTEGER;
    l->token.u.number = num;

    if (!parse_integer_suffix(l, &l->token.u.number))
    {
        return token_error(l);
    }

    strv text = dstr_to_strv(&l->tok_buf);
    text = strv_remove_right(text, 1); /* Remove 1 to remove last character which is not part of the value. */
    l->token.text = ac_create_or_reuse_literal(l->mgr, text);
    return &l->token;
}

static ac_token* token_float_literal(ac_lex* l, ac_token_number num)
{
    l->token.type = ac_token_type_LITERAL_FLOAT;
    l->token.u.number = num;

    if (!parse_float_suffix(l, &l->token.u.number))
    {
        return token_error(l);
    }

    if (l->token.u.number.is_float && l->token.u.number.u.float_value > FLT_MAX)
    {
        l->token.u.number.overflow = true;
    }

    strv text = dstr_to_strv(&l->tok_buf);
    text = strv_remove_right(text, 1); /* Remove 1 to remove last character which is not part of the value. */
    l->token.text = ac_create_or_reuse_literal(l->mgr, text);
    return &l->token;
}

static ac_token* parse_float_literal(ac_lex* l, ac_token_number num, enum base_type base)
{
    int c = l->cur[0];

    bool parse_fractional_part = false;
    if (c == '.') {
        c = next_digit(l);
        parse_fractional_part = true;
    }

    return parse_float_literal_core(l, num, base, parse_fractional_part);
}

static ac_token* parse_float_literal_core(ac_lex* l, ac_token_number num, enum base_type base, bool parse_fractional_part)
{
    AC_ASSERT(base == 10 || base == 16 && "Can only parse decimal floats of hexadicemal floats.");

    double value = num.u.float_value;
    int exponent = 0;

    int c = l->cur[0];

    if (parse_fractional_part) {
        if (c == '.') { return NULL; } /* If there is a dot after a dot then we are not parsing a number. */
        double pow, addend = 0;

        if (base == base10)
        {
            for (pow = 1; is_decimal_digit(c); pow *= base) {
                addend = addend * base + (c - '0');
                c = next_digit(l);
            }
        }
        else /* base == base16 */
        {
            for (pow = 1; ; pow *= base) {
                if (c >= '0' && c <= '9') {
                    addend = addend * base + (c - '0');
                    c = next_digit(l);
                }
                else if (c >= 'a' && c <= 'f') {
                    addend = addend * base + 10 + (c - 'a');
                    c = next_digit(l);
                }
                else if (c >= 'A' && c <= 'F') {
                    addend = addend * base + 10 + (c - 'A');
                    c = next_digit(l);
                }
                else
                    break;
            }
        }

        value += addend / pow;
    }

    if (base == base16) {
        if (c == 'p' || c == 'P') {
            exponent = 1;
            c = next_digit(l); /* Skip 'p' or 'P' */
        }
        else
        {
            ac_report_error_loc(l->location, "invalid exponent in hex float");
            return 0;
        }
    }
    else if (c == 'e' || c == 'E')
    {
        exponent = 1;
        c = next_digit(l); /* Skip 'e' or 'E' */
    }

    int sign = 1;
    if (exponent) {
        unsigned int exponent = 0;
        double power_ = 1;
        if (c == '-' || c == '+')
        {
            sign = '-' ? -1 : 1;
            c = next_digit(l); /* Skip '-' or '+' */
        }
        while (c >= '0' && c <= '9') {
            exponent = exponent * 10 + (c - '0');
            c = next_digit(l);
        }

        power_ = power(base == base10 ? 2 : 10, exponent);
        if (sign)
            value /= power_;
        else
            value *= power_;
    }
    //*q = p;

    num.u.float_value = value;
    return token_float_literal(l, num);
}

static int hex_string_to_int(const char* c, size_t len)
{
    /* @FIXME check for overflow. */
    int n = 0;
    const char* end = c + len;
    for (; c <= end; ++c) {
        if (c[0] >= '0' && c[0] <= '9')
            n = n * 16 + (c[0] - '0');
        else if (c[0] >= 'a' && c[0] <= 'f')
            n = n * 16 + (c[0] - 'a') + 10;
        else if (c[0] >= 'A' && c[0] <= 'F')
            n = n * 16 + (c[0] - 'A') + 10;
        else
            break;
    }
    return n;
}

static ac_token* parse_integer_or_float_literal(ac_lex* l, int previous, int c)
{
    ac_token_number num = { 0 };
    bool leading_zero = previous == '0';

    if (leading_zero)
    {
        /* Need to parse hex integer or float */
        if (c == 'x' || c == 'X') {
            c = next_digit(l); /* Skip 'x' or 'X */
            size_t buffer_size = l->tok_buf.size;
            /* @FIXME check for overflow. */
            int n = 0;
            while (is_hex_digit(c)) {
                n = n * base16 + (c - '0');
                if (c >= '0' && c <= '9')
                    n = n * base16 + (c - '0');
                else if (c >= 'a' && c <= 'f')
                    n = n * base16 + 10 + (c - 'a');
                else if (c >= 'A' && c <= 'F')
                    n = n * base16 + 10 + (c - 'A');
                c = next_digit(l);
            }
            if (!is_eof(l)) {
                if (c == '.' || c == 'p' || c == 'P') {
                    return parse_float_literal(l, num, base16);
                }
            }
            if (buffer_size == l->tok_buf.size) /* Nothing after 0x was parsed */
            {
                ac_report_error_loc(l->leading_location, "invalid hexadecimal value.");
                return token_error(l);
            }
            /* Not float so we return an integer. */
            num.u.int_value = hex_string_to_int(l->tok_buf.data, l->tok_buf.size);
            return token_integer_literal(l, num);
        }
        /* Need to parse binary integer */
        else if (c == 'b' || c == 'B') {
            c = next_digit(l); /* Skip 'b' or 'B' */
            size_t buffer_size = l->tok_buf.size;
            /* @FIXME check for overflow. */
            int n = 0;
            while (is_binary_digit(c)) {
                n = n * base2 + (c - '0');
                c = next_digit(l);
            }
            if (buffer_size == l->tok_buf.size) /* Nothing after 0b was parsed */
            {
                ac_report_error_loc(l->leading_location, "invalid binary value");
                return token_error(l);
            }
            num.u.int_value = n;
            return token_integer_literal(l, num);
        }
    }

    /* Try to parse float starting with decimals (not hexadecimal). */

    int n = 0;
    if (is_decimal_digit(previous)) {
        n = n * base10 + (previous - '0');
        while (is_decimal_digit(c)) {
            n = n * base10 + (c - '0'); /* @FIXME check for overflow. */
            c = next_digit(l);
        }
    }
    if (!is_eof(l)) {
        if (c == '.' || c == 'e' || c == 'E') {
            return parse_float_literal(l, num, base10);
        }
    }

    /* Try to parse octal for buffer since the literal has been eaten */
    if (leading_zero)
    {
        n = 0;
        for (int i = 0; i < l->tok_buf.size; ++i) {
            c = l->tok_buf.data[i];
            /* @FIXME take care of overflow. */
            if (is_octal_digit(c)) {
                n = n * base8 + (c - '0');
            }
        }
    }

    if (is_eof(l) && !l->mgr->options.preprocess) /* Do not display error if we only preprocess. */
    {
        ac_report_error_loc(l->leading_location, "unexpected end of file after number literal");
        return token_error(l);
    }

    /* Return the parsed integer. */
    num.u.int_value = n;
    return token_integer_literal(l, num);
}

static void* utf8_decode(void* p, int32_t* pc)
{
    const int replacement = 0xFFFD;
    const unsigned char* s = (const unsigned char*)p;
    if (s[0] < 0x80) {
        *pc = s[0];
        return *pc ? (char*)p + 1 : p;
    }
    if ((s[0] & 0xE0) == 0xC0) {
        *pc = (int)(s[0] & 0x1F) << 6
            | (int)(s[1] & 0x3F);
        return (char*)p + 2;
    }
    if ((s[0] & 0xF0) == 0xE0) {
        *pc = (int)(s[0] & 0x0F) << 12
            | (int)(s[1] & 0x3F) << 6
            | (int)(s[2] & 0x3F);
        /* Surrogate pairs are not allowed in UTF-8 */
        if (0xD800 <= *pc && *pc <= 0xDFFF)
            *pc = replacement;
        return (char*)p + 3;
    }
    if ((s[0] & 0xF8) == 0xF0 && (s[0] <= 0xF4)) {
        /* Not greater than 0x10FFFF */
        *pc = (int)(s[0] & 0x07) << 18
            | (int)(s[1] & 0x3F) << 12
            | (int)(s[2] & 0x3F) << 6
            | (int)(s[3] & 0x3F);
        return (char*)p + 4;
    }
    *pc = replacement;
    return NULL;
}

/* This function either parse a string or skip a string.
   The string is skipped and buf can be null in case we just want to skip a preprocessor block.
   The function is quite ugly because we consider an optimistic path and a pessimistic one.
   The pessimistic one is when a splice is encountered. */
static strv string_or_char_literal_to_buffer(ac_lex* l, char ending_char, dstr* buf)
{
    strv inner_content = empty;

    if (buf) {
        dstr_clear(buf);
    }

    int c = l->cur[0];
    int previous_for_splice = 0; /* Keep previous character to handle some behavior due to splice and escaped sequences. */

    int n = 0; /* Number of character from the optimistic path. */
    int splice_found = false;

    const char* start = l->cur;

    /* Optimistic path: we assume the string is without any splice, in which case there is no need to add the characters into a buffer. */
    for (;;)
    {
        switch (c)
        {
            case '\n': /* Unterminated string due to new-line */
            case '\r': /* Unterminated string due to new-line */
            case '\0': /* End of file. */
            {
                goto exit_loop; /* Unterminated string. */
            }
            default:
            {
                if (c == ending_char)
                {
                    goto exit_loop; /* Proper end of literal */
                }
                break;
            }
            case  '\\':
            {
                if (next_is(l, '\n') || next_is(l, '\r'))
                {
                    splice_found = true;
                    goto exit_loop;  /* Splice found, we need to place the string in a buffer. */
                }
                else if (next_is(l, '\0'))
                {
                    consume_one(l); /* Skip character before EOF. */
                    goto exit_loop; /* Unterminated string. */
                }
                else
                {
                    n += 1;
                    previous_for_splice = c;
                    c = consume_one(l);

                    if (is_eof(l))
                    {
                        goto exit_loop; /* Unterminated string. */
                    }
                    else if (c == '\\' && (next_is(l, '\n') || next_is(l, '\r')))
                    {
                        splice_found = true;
                        goto exit_loop;  /* Splice found, we need to place the string in a buffer. */
                    }
                }
                break;
            }
        }

        n += 1;
        previous_for_splice = c;
        c = consume_one(l);
    }

exit_loop:

    /* Pessimistic path: splice is found, we add the string to the buffer. */
    if (splice_found)
    {
        AC_ASSERT(c == '\\');
        if (buf) {
            dstr_append(buf, strv_make_from(start, n)); /* Add buffer right before the splice */
        }

        c = skip_if_splice(l); /* Skip new line */
        size_t s = buf ? buf->size : 0;
        for (;;)
        {
            if (is_eof(l))
            {
                break;
            }
            else if (c == ending_char && previous_for_splice != '\\')
            {
                break; /* End of literal */
            }
            else if (c == '\n' || c == '\r')
            {
                /* Error new-line before string termination. */
                break;
            }

            if (buf) {
                dstr_append_char(buf, c);
            }

            /* Special case when two \\ has been found we assume the previous is "0" */
            if (previous_for_splice == '\\' && c == '\\')
                previous_for_splice = 0;
            else
                previous_for_splice = c;

            c = next_char_no_splice(l);
        }

        if (buf) {
            inner_content = dstr_to_strv(buf);
        }
    }
    else
    {
        inner_content = strv_make_from(start, l->cur - start);
    }

    if (c != ending_char) {
        ac_report_error_loc(l->leading_location, "missing terminating char '%c' for literal", ending_char);
        return strv_error;
    }

    c = next_char_no_splice(l); /* consume ending_char */

    return inner_content;
}

static ac_token* parse_string_literal(ac_lex* l, strv prefix) {
    AC_ASSERT(is_char(l, '"'));
    AC_ASSERT(prefix.data == no_prefix.data
        || prefix.data == utf8.data
        || prefix.data == utf16.data
        || prefix.data == utf32.data);

    consume_one(l); /* Skip '"'. */
    strv literal = string_or_char_literal_to_buffer(l, '"', &l->tok_buf);
    if (literal.data == strv_error.data)
        return token_error(l);

    return token_string(l, literal, prefix);
}

static ac_token* token_string(ac_lex* l, strv literal, strv prefix)
{
    l->token.type = ac_token_type_LITERAL_STRING;

    l->token.text = ac_create_or_reuse_literal(l->mgr, literal);

    if (prefix.data == utf8.data)
        l->token.u.str.is_utf8 = true;
    else if (prefix.data == utf16.data)
        l->token.u.str.is_utf16 = true;
    else if (prefix.data == utf32.data)
        l->token.u.str.is_utf32 = true;
    else if (prefix.data == wide.data)
        l->token.u.str.is_wide = true;

    return &l->token;
}

static ac_token* token_char(ac_lex* l, strv prefix)
{
    AC_ASSERT(is_char(l, '\''));

    dstr_assign(&l->tok_buf, prefix);

    consume_one(l); /* Skip '''. */
    strv literal = string_or_char_literal_to_buffer(l, '\'', &l->tok_buf);
    if (literal.data == strv_error.data)
        return token_error(l);

    l->token.type = ac_token_type_LITERAL_CHAR;

    l->token.text = ac_create_or_reuse_literal(l->mgr, literal);

    int32_t c = 0;
    utf8_decode((char*)literal.data, &c);

    if (prefix.data == no_prefix.data) {
        l->token.u.ch.value = (char)c;
    }
    else if (prefix.data == utf8.data) {
        l->token.u.ch.is_utf8 = true;
        l->token.u.ch.value = c;
    }
    else if (prefix.data == utf16.data) {
        l->token.u.ch.is_utf16 = true;
        l->token.u.ch.value = c & 0xffff;
    }
    else if (prefix.data == utf32.data) {
        l->token.u.ch.is_utf32 = true;
        l->token.u.ch.value = c;
    }
    else if (prefix.data == wide.data) {
        l->token.u.ch.is_wide = true;
        l->token.u.ch.value = c;
    }

    return &l->token;
}

/*
-------------------------------------------------------------------------------
ac_token
-------------------------------------------------------------------------------
*/

#define IDENT(value) { STRV(value) }

static ac_token_info token_infos[] = {

    { false, ac_token_type_NONE, IDENT("<none>") },
    { false, ac_token_type_EMPTY, IDENT("") },

    /* Keywords */

    { false, ac_token_type_ALIGNAS, IDENT("alignas") },
    { false, ac_token_type_ALIGNAS2, IDENT("_Alignas") },
    { false, ac_token_type_ALIGNOF, IDENT("alignof") },
    { false, ac_token_type_ALIGNOF2, IDENT("_Alignof") },
    { true,  ac_token_type_AT, IDENT("@") },
    { false, ac_token_type_ATOMIC, IDENT("_Atomic") },
    { false, ac_token_type_AUTO, IDENT("auto") },
    { false, ac_token_type_BOOL, IDENT("bool") },
    { false, ac_token_type_BOOL2, IDENT("_Bool") },
    { false, ac_token_type_BITINT, IDENT("_BitInt") },
    { false, ac_token_type_BREAK, IDENT("break") },
    { false, ac_token_type_CASE, IDENT("case") },
    { true,  ac_token_type_CHAR, IDENT("char") },
    { false, ac_token_type_CONST, IDENT("const") },
    { false, ac_token_type_CONSTEXPR, IDENT("constexpr") },
    { false, ac_token_type_CONTINUE, IDENT("continue") },
    { false, ac_token_type_COMPLEX, IDENT("_Complex") },
    { false, ac_token_type_DECIMAL128, IDENT("_Decimal128") },
    { false, ac_token_type_DECIMAL32, IDENT("_Decimal32") },
    { false, ac_token_type_DECIMAL64, IDENT("_Decimal64") },
    { false, ac_token_type_DEFAULT, IDENT("default") },
    { false, ac_token_type_DO, IDENT("do") },
    { true,  ac_token_type_DOUBLE, IDENT("double") },
    { true, ac_token_type_ELSE, IDENT("else") },
    { false, ac_token_type_ENUM, IDENT("enum") },
    { false, ac_token_type_EXTERN, IDENT("extern") },
    { false, ac_token_type_FALSE, IDENT("false") },
    { true,  ac_token_type_FLOAT, IDENT("float") },
    { false, ac_token_type_FOR, IDENT("for") },
    { false, ac_token_type_GENERIC, IDENT("_Generic") },
    { false, ac_token_type_GOTO, IDENT("goto") },
    { true,  ac_token_type_IF, IDENT("if") },
    { false, ac_token_type_INLINE, IDENT("inline") },
    { true,  ac_token_type_INT, IDENT("int") },
    { false, ac_token_type_IMAGINARY, IDENT("_Imaginary") },
    { true,  ac_token_type_LONG, IDENT("long") },
    { false, ac_token_type_NORETURN, IDENT("_Noreturn") },
    { false, ac_token_type_NULLPTR, IDENT("nullptr") },
    { false, ac_token_type_REGISTER, IDENT("register") },
    { false, ac_token_type_RESTRICT, IDENT("restrict") },
    { true,  ac_token_type_RETURN, IDENT("return") },
    { true,  ac_token_type_SHORT, IDENT("short") },
    { true,  ac_token_type_SIGNED, IDENT("signed") },
    { false, ac_token_type_SIZEOF, IDENT("sizeof") },
    { false, ac_token_type_STATIC, IDENT("static") },
    { false, ac_token_type_STATIC_ASSERT, IDENT("static_assert") },
    { false, ac_token_type_STATIC_ASSERT2, IDENT("_Static_assert") },
    { false, ac_token_type_STRUCT, IDENT("struct") },
    { false, ac_token_type_SWITCH, IDENT("switch") },
    { false, ac_token_type_THREAD_LOCAL, IDENT("thread_local") },
    { false, ac_token_type_THREAD_LOCAL2, IDENT("_Thread_local") },
    { false, ac_token_type_TRUE, IDENT("true") },
    { false, ac_token_type_TYPEDEF, IDENT("typedef") },
    { false, ac_token_type_TYPEOF, IDENT("typeof") },
    { false, ac_token_type_TYPEOF_UNQUAL, IDENT("typeof_unqual") },
    { false, ac_token_type_UNION, IDENT("union") },
    { true,  ac_token_type_UNSIGNED, IDENT("unsigned") },
    { false, ac_token_type_VOID, IDENT("void") },
    { false, ac_token_type_VOLATILE, IDENT("volatile") },
    { false, ac_token_type_WHILE, IDENT("while") },

    /* Preprocessor conditionals */

    { true, ac_token_type_DEFINE, IDENT("define") },
    { true, ac_token_type_DEFINED, IDENT("defined") },
    { true, ac_token_type_ELIF, IDENT("elif") },
    { true, ac_token_type_ELIFDEF, IDENT("elifdef") },
    { true, ac_token_type_ELIFNDEF, IDENT("elifndef") },
    { true, ac_token_type_ENDIF, IDENT("endif") },
    { true, ac_token_type_ERROR, IDENT("error") },
    { true, ac_token_type_EMBED, IDENT("embed") },
    { true, ac_token_type_IFDEF, IDENT("ifdef") },
    { true, ac_token_type_IFNDEF, IDENT("ifndef") },
    { true, ac_token_type_INCLUDE, IDENT("include") },
    { true, ac_token_type_PRAGMA, IDENT("pragma") },
    { true, ac_token_type_LINE, IDENT("line") },
    { true, ac_token_type_UNDEF, IDENT("undef") },
    { true, ac_token_type_WARNING, IDENT("warning") },

    /* Special Macros */

    { true, ac_token_type__FILE__, IDENT("__FILE__") },
    { true, ac_token_type__LINE__, IDENT("__LINE__") },
    { true, ac_token_type__DATE__, IDENT("__DATE__") },
    { true, ac_token_type__TIME__, IDENT("__TIME__") },
    { true, ac_token_type__COUNTER__, IDENT("__COUNTER__") },
    { true, ac_token_type__FUNC__, IDENT("__func__") },
    { true, ac_token_type__FUNCTION__, IDENT("__FUNCTION__") },
    { true, ac_token_type__PRETTY_FUNCTION__, IDENT("__PRETTY_FUNCTION__") },

    /* Symbols */

    { true,  ac_token_type_AMP, IDENT("&") },
    { true,  ac_token_type_AMP_EQUAL, IDENT("&=") },
    { false, ac_token_type_ARROW, IDENT("->") },
    { true,  ac_token_type_BACKSLASH, IDENT("\\") },
    { true,  ac_token_type_BRACE_L, IDENT("{") },
    { true,  ac_token_type_BRACE_R, IDENT("}") },
    { false, ac_token_type_CARET, IDENT("^") },
    { false, ac_token_type_CARET_EQUAL, IDENT("^=") },
    { false, ac_token_type_COLON, IDENT(":") },
    { true,  ac_token_type_COMMA, IDENT(",") },
    { false, ac_token_type_COMMENT, IDENT("<comment>") },
    { false, ac_token_type_DOLLAR, IDENT("$") },
    { false, ac_token_type_DOT, IDENT(".") },
    { false, ac_token_type_DOUBLE_AMP, IDENT("&&") },
    { false, ac_token_type_DOUBLE_DOT, IDENT("..") },
    { true,  ac_token_type_DOUBLE_EQUAL, IDENT("==") },
    { true,  ac_token_type_DOUBLE_GREATER, IDENT(">>") },
    { true,  ac_token_type_DOUBLE_HASH, IDENT("##") },
    { true,  ac_token_type_DOUBLE_LESS, IDENT("<<") },
    { false, ac_token_type_DOUBLE_MINUS, IDENT("--") },
    { true,  ac_token_type_DOUBLE_PIPE, IDENT("||") },
    { false, ac_token_type_DOUBLE_PLUS, IDENT("++") },
    { false, ac_token_type_DOUBLE_QUOTE , IDENT("\"") },
    { true,  ac_token_type_EOF, IDENT("<end-of-line>") },
    { true,  ac_token_type_EQUAL, IDENT("=") },
    { true,  ac_token_type_EXCLAM, IDENT("!") },
    { true,  ac_token_type_GREATER, IDENT(">") },
    { true,  ac_token_type_GREATER_EQUAL, IDENT(">=") },
    { true,  ac_token_type_HASH, IDENT("#") },
    { false, ac_token_type_HORIZONTAL_WHITESPACE, IDENT("<horizontal_whitespace>") },
    { true,  ac_token_type_IDENTIFIER, IDENT("<identifier>") },
    { true,  ac_token_type_LESS, IDENT("<") },
    { true,  ac_token_type_LESS_EQUAL, IDENT("<=") },
    { true,  ac_token_type_LITERAL_CHAR, IDENT("<literal-char>") },
    { true,  ac_token_type_LITERAL_FLOAT, IDENT("<literal-float>") },
    { true,  ac_token_type_LITERAL_INTEGER, IDENT("<literal-integer>") },
    { true,  ac_token_type_LITERAL_STRING, IDENT("<literal-string>") },
    { true,  ac_token_type_MINUS, IDENT("-") },
    { true,  ac_token_type_MINUS_EQUAL, IDENT("-=") },
    { true,  ac_token_type_NEW_LINE, IDENT("<new_line>") },
    { true,  ac_token_type_NOT_EQUAL, IDENT("!=") },
    { true,  ac_token_type_PAREN_L, IDENT("(") },
    { true,  ac_token_type_PAREN_R, IDENT(")") },
    { true,  ac_token_type_PERCENT, IDENT("%") },
    { true,  ac_token_type_PERCENT_EQUAL, IDENT("%=") },
    { true,  ac_token_type_PIPE, IDENT("|") },
    { true,  ac_token_type_PIPE_EQUAL, IDENT("|=") },
    { true,  ac_token_type_PLUS, IDENT("+") },
    { true,  ac_token_type_PLUS_EQUAL, IDENT("+=") },
    { false, ac_token_type_QUESTION, IDENT("?") },
    { false, ac_token_type_QUOTE, IDENT("'") },
    { true,  ac_token_type_SEMI_COLON, IDENT(";") },
    { true,  ac_token_type_SLASH, IDENT("/") },
    { true,  ac_token_type_SLASH_EQUAL, IDENT("/=") },
    { false, ac_token_type_SQUARE_L, IDENT("[") },
    { false, ac_token_type_SQUARE_R, IDENT("]") },
    { true,  ac_token_type_STAR, IDENT("*") },
    { true,  ac_token_type_STAR_EQUAL, IDENT("*=") },
    { true,  ac_token_type_TILDE, IDENT("~") },
    { true,  ac_token_type_TILDE_EQUAL, IDENT("~=") },
    { false, ac_token_type_TRIPLE_DOT, IDENT("...") }
};

const char* ac_token_type_to_str(enum ac_token_type type) {
    return ac_token_type_to_strv(type).data;
}

strv ac_token_type_to_strv(enum ac_token_type type)
{
    return token_infos[type].ident.text;
}

const char* ac_token_to_str(ac_token token) {

    return ac_token_to_strv(token).data;
}

strv ac_token_to_strv(ac_token token) {

    if (token.type == ac_token_type_IDENTIFIER)
    {
        return token.ident->text;
    }
    else if (token.type == ac_token_type_LITERAL_CHAR
        || token.type == ac_token_type_LITERAL_STRING
        || token.type == ac_token_type_LITERAL_INTEGER
        || token.type == ac_token_type_LITERAL_FLOAT
        || token.type == ac_token_type_HORIZONTAL_WHITESPACE
        || token.type == ac_token_type_COMMENT
        || token.type == ac_token_type_NEW_LINE)
    {
        return token.text;
    }
    
    return ac_token_type_to_strv(token.type);
}

void ac_token_fprint(FILE* file, ac_token t)
{
    if (t.previous_was_space)
    {
        fprintf(file, " ");
    }

    strv prefix = ac_token_prefix(t);
    if (prefix.size)
    {
        fprintf(file, "%.*s", (int)prefix.size, prefix.data);
    }
    const char* format = "%.*s";
    if (t.type == ac_token_type_LITERAL_STRING)
        format = "\"%.*s\"";
    else  if (t.type == ac_token_type_LITERAL_CHAR)
        format = "'%.*s'";

    strv s = ac_token_to_strv(t);
    fprintf(file, format, (int)s.size, s.data);
}

void ac_token_sprint(dstr* str, ac_token t)
{
    if (t.previous_was_space)
    {
        dstr_append_f(str, " ");
    }

    strv prefix = ac_token_prefix(t);
    if (prefix.size)
    {
        dstr_append_f(str, "%.*s", (int)prefix.size, prefix.data);
    }
    const char* format = "%.*s";
    if (t.type == ac_token_type_LITERAL_STRING)
        format = "\"%.*s\"";
    else  if (t.type == ac_token_type_LITERAL_CHAR)
        format = "'%.*s'";

    strv s = ac_token_to_strv(t);
    dstr_append_f(str, format, (int)s.size, s.data);
}

ac_token_info* ac_token_infos()
{
    return token_infos;
}

bool ac_token_is_keyword_or_identifier(enum ac_token_type type) {

    return type == ac_token_type_IDENTIFIER
        || (type >= ac_token_type_ALIGNAS && type <= ac_token_type__PRETTY_FUNCTION__);
}

strv ac_token_prefix(ac_token token) {

    if (token.type == ac_token_type_LITERAL_STRING)
    {
        if (token.u.str.is_utf8) return utf8;
        else if (token.u.str.is_utf16) return utf16;
        else if (token.u.str.is_utf32) return utf32;
        else if (token.u.str.is_wide) return wide;
        else return no_prefix;
    }

    if (token.type == ac_token_type_LITERAL_CHAR)
    {
        if (token.u.ch.is_utf8) return utf8;
        else if (token.u.ch.is_utf16) return utf16;
        else if (token.u.ch.is_utf32) return utf32;
        else if (token.u.ch.is_wide) return wide;
        else return no_prefix;
    }
    return no_prefix;
}

static size_t token_str_len(enum ac_token_type type) {
    AC_ASSERT(!token_type_is_literal(type));

    return token_infos[type].ident.text.size;
}

static bool token_type_is_literal(enum ac_token_type type) {
    switch (type) {
    case ac_token_type_TRUE:
    case ac_token_type_FALSE:
    case ac_token_type_LITERAL_CHAR:
    case ac_token_type_LITERAL_INTEGER:
    case ac_token_type_LITERAL_FLOAT:
    case ac_token_type_LITERAL_STRING:
        return true;
    }
    return false;
}

static void location_increment_row(ac_location* l, int char_count) {
    l->row += 1;
    l->col = 0; /* This is 1-based index, but the column after a new line is not '1' the next character should be '1'. */
    l->pos += char_count;
}

static void location_increment_column(ac_location* l, int char_count) {
    l->col += 1;
    l->pos += char_count;
}