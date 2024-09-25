#include "lexer.h"

#include "float.h"  /* FLT_MAX */
#include "stdint.h" /* uint64_t */

#include "global.h"

static bool is_end_line(const ac_lex* l);          /* current char is alphanumeric */
static bool is_whitespace(const ac_lex* l);        /* current char is alphanumeric */
static bool is_alpha(const ac_lex* l);             /* current char is alpha */
static bool is_alphanum(const ac_lex* l);          /* current char is alphanumeric */
static bool is_utf8(const ac_lex* l);              /* current char is utf8 */
static bool is_eof(const ac_lex* l);               /* current char is end of line */
static bool is_char(const ac_lex* l, char c);      /* current char is equal to char  */
static bool is_not_char(const ac_lex* l, char c);  /* current char is not equal to char  */
static bool is_str(const ac_lex* l, const char* str, size_t count); /* current char is equal to string to string */
static bool next_is(const ac_lex* l, char c);      /* next char equal to */
static bool next_next_is(const ac_lex* l, char c); /* next next char equal to */

static void consume_one(ac_lex* l);       /* goto next char */
static char get_next(ac_lex* l);          /* goto next char and return it */

static void skipn(ac_lex* l, unsigned n); /* skip n char */

static void skip_whitespace(ac_lex* l);
static void skip_whitespaces_and_comments(ac_lex* l);

static const ac_token* parse_ascii_char_literal(ac_lex* l);
static const ac_token* parse_number(ac_lex* l);

static void set_token_value(ac_lex* l, const char* data, size_t len);
static const ac_token* token_error(ac_lex* l); /* set current token to error and returns it. */
static const ac_token* token_eof(ac_lex* l);   /* set current token to eof and returns it. */
static const ac_token* set_token(ac_lex* l, enum ac_token_type type);       /* set current token and got to next */
static size_t token_str_len(enum ac_token_type type);
static bool try_parse_keyword(const char* str, size_t len, enum ac_token_type* type);

static bool token_type_is_literal(enum ac_token_type type);

static void location_increment_row(ac_location* l);
static void location_increment_column(ac_location* l);

/*
-------------------------------------------------------------------------------
w_lex
-------------------------------------------------------------------------------
*/

void ac_lex_init(ac_lex* l, ac_manager* mgr)
{
    memset(l, 0, sizeof(ac_lex));

    l->mgr = mgr;

    l->options.enable_hex_float = true;


}

void ac_lex_destroy(ac_lex* l)
{
    memset(l, 0, sizeof(ac_lex));
}

void ac_lex_set_source(ac_lex* l, const char* source_content, size_t source_len, const char* filepath)
{
    l->filepath = filepath;

    strv content = strv_make_from(source_content, source_len);
    ac_location_init_with_file(&l->location, filepath, content);

    if (source_content && source_len) {
        l->src = source_content;
        l->end = source_content + source_len;
        l->cur = source_content;
        l->len = source_len;
    }

    /* Go to the first token already */
    ac_lex_goto_next(l);
}

const ac_token* ac_lex_goto_next(ac_lex* l)
{
    if (is_eof(l)) {
        return token_eof(l);
    }

    skip_whitespaces_and_comments(l);

    if (is_eof(l)) {
        return token_eof(l);
    }

    switch (l->cur[0]) {

    case ' ':
    case '\n':
    case '\r':
    case '\t':
    case '\f':
    case '\v': {
        assert(0 && "internal error unreachable. spaces should have been handled differently.");
    }
    case '#': return set_token(l, ac_token_type_HASH);
    case '[': return set_token(l, ac_token_type_SQUARE_L);
    case ']': return set_token(l, ac_token_type_SQUARE_R);
    case '(': return set_token(l, ac_token_type_PAREN_L);
    case ')': return set_token(l, ac_token_type_PAREN_R);
    case '{': return set_token(l, ac_token_type_BRACE_L);
    case '}': return set_token(l, ac_token_type_BRACE_R);
    case ';': return set_token(l, ac_token_type_SEMI_COLON);
    case ',': return set_token(l, ac_token_type_COMMA);
    case '?': return set_token(l, ac_token_type_QUESTION);

    case '=': {
        if (next_is(l, '=')) {
            return set_token(l, ac_token_type_DOUBLE_EQUAL);
        }
        return set_token(l, ac_token_type_EQUAL);
    }

    case '!': {

        if (next_is(l, '=')) {
            return set_token(l, ac_token_type_NOT_EQUAL);
        }
        return set_token(l, ac_token_type_EXCLAM);
    }

    case '<': {
        if (next_is(l, '<')) {
            return set_token(l, ac_token_type_DOUBLE_LESS);
        }
        else if (next_is(l, '=')) {
            return set_token(l,  ac_token_type_LESS_EQUAL);
        }
        return set_token(l,  ac_token_type_LESS);
    }

    case '>': {

        if (next_is(l, '>')) {
            return set_token(l, ac_token_type_DOUBLE_GREATER);
        }
        else if (next_is(l, '=')) {
            return set_token(l, ac_token_type_GREATER_EQUAL);
        }

        return set_token(l, ac_token_type_GREATER);
    }

    case '&': {
        if (next_is(l, '&')) {
            return set_token(l, ac_token_type_DOUBLE_AMP);
        }
        else if (next_is(l, '=')) {
            return set_token(l, ac_token_type_AMP_EQUAL);
        }
        return set_token(l, ac_token_type_AMP);
    }

    case '|': {
        if (next_is(l, '|')) {
            return set_token(l, ac_token_type_DOUBLE_PIPE);
        }
        else if (next_is(l, '=')) {
            return set_token(l, ac_token_type_PIPE_EQUAL);
        }
        return set_token(l, ac_token_type_PIPE);
    }

    case '+': {
        if (next_is(l, '=')) {
            return set_token(l, ac_token_type_PLUS_EQUAL);
        }
        return set_token(l, ac_token_type_PLUS);
    }
    case '-': {
        if (next_is(l, '=')) {
            return set_token(l, ac_token_type_MINUS_EQUAL);
        }
        else if (next_is(l, '>')) {
            return set_token(l, ac_token_type_ARROW);
        }
        return set_token(l, ac_token_type_MINUS);
    }

    case '*': {
        if (next_is(l, '=')) {
            return set_token(l, ac_token_type_STAR_EQUAL);
        }
        return set_token(l, ac_token_type_STAR);
    }

    case '/': {
        if (next_is(l, '=')) {
            return set_token(l, ac_token_type_SLASH_EQUAL);
        }
        else if (next_is(l, '/')) {
            assert(0 && "internal error: unreachable opening comments is handled in another place");
        }
        else if (next_is(l, '*')) {
            assert(0 && "internal error: unreachable opening comments is handled in another place");
        }

        return set_token(l, ac_token_type_SLASH);
    }

    case '%': {
        if (next_is(l, '=')) {
            return set_token(l, ac_token_type_PERCENT_EQUAL);
        }
        return set_token(l, ac_token_type_PERCENT);
    }

    case '^': {
        if (next_is(l, '=')) {
            return set_token(l, ac_token_type_CARET_EQUAL);
        }

        return set_token(l, ac_token_type_CARET);
    }

    case '.': {
        if (next_is(l, '.')) {
            if (next_next_is(l, '.')) {
                return set_token(l, ac_token_type_TRIPLE_DOT);
            }
            return set_token(l, ac_token_type_DOUBLE_DOT);
        }
        return set_token(l, ac_token_type_DOT);
    }

    case ':' : return set_token(l, ac_token_type_COLON);
    case '"' : return parse_ascii_char_literal(l);
    case '\0': return token_eof(l);

    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9': return parse_number(l);

    default: {

        if (is_alpha(l) || is_char(l, '_') || is_utf8(l)) {

            const char* start = l->cur;
            int n = 0;
            do {
                ++n;
                consume_one(l);

            } while ((is_alphanum(l) || is_char(l, '_') || is_utf8(l)));

            set_token_value(l, start, n);

            if (strv_equals_str(l->token.text, "true"))
            {
                l->token.type = ac_token_type_LITERAL_BOOL;
                l->token.u.b.value = true;
            }
            else if (strv_equals_str(l->token.text, "false"))
            {
                l->token.type = ac_token_type_LITERAL_BOOL;
                l->token.u.b.value = false;
            }
            else if (try_parse_keyword(start, n, &l->token.type)) /* if keyword is parsed token type is also retrieved */
            {
                /* do nothing */
            }
            else
            {
                l->token.type = ac_token_type_IDENTIFIER;
            }

            return &l->token;
        } /* end if (is_alpha(l) || is(l, '_') || is_utf8(l)) */
        else
        {
            /* @TODO handle this without assert, likely return a EOF or ERROR token */
            assert(0 && "Illegal char");
        }
        break;
    } /* end default case */

    } /* end switch */

    return token_error(l);
}

static inline bool _is_end_line(char c) {
    return (c == '\n' || c == '\r');
}

static inline bool is_end_line(const ac_lex* l) {
    return _is_end_line(l->cur[0]);
}

static inline bool _is_whitespace(char c) {
    return (c == ' ' || c == '\n'
        || c == '\t' || c == '\r'
        || c == '\f' || c == '\v');
}

static inline bool is_whitespace(const ac_lex* l) {
    return _is_whitespace(l->cur[0]);
}

static inline bool _is_alpha(char c) {
    return (c >= 'a' && c <= 'z')
        || (c >= 'A' && c <= 'Z');
}

static inline bool is_alpha(const ac_lex* l) {
    return _is_alpha(l->cur[0]);
}

static inline bool _is_alphanum(char c) {
    return (c >= 'a' && c <= 'z')
        || (c >= 'A' && c <= 'Z')
        || (c >= '0' && c <= '9');
}
static inline bool is_alphanum(const ac_lex* l) {
    return _is_alphanum(l->cur[0]);
}

static inline bool _is_num(char c) {
    return (c >= '0' && c <= '9');
}

static inline bool is_num(const ac_lex* l) {
    return _is_num(l->cur[0]);
}

static inline bool _is_utf8(char c) {
    /* @TODO do it in a better way */
    return (unsigned char)c >= 128;
}

static inline bool is_utf8(const ac_lex* l) {
    return _is_utf8(l->cur[0]);
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
    return *(l->cur + 1) == c;
}

static bool next_next_is(const ac_lex* l, char c) {
    return *(l->cur + 2) == c;
}

static inline void consume_one(ac_lex* l) {

    if (is_char(l, '\n') || is_char(l, '\r')) {

        /* \r\n is considered as "one" but two characters are skipped */
        /* thanks to stb_c_lexer for this nice trick */
        int num_char = (l->cur[0] + l->cur[1] == '\r' + '\n') ? 2 : 1;
        l->cur += num_char; /* skip newline */

        if (num_char == 1)
            location_increment_row(&l->location);

        else
        {
            l->location.pos++;
            location_increment_row(&l->location);
        }
    }
    else {

        l->cur++;
        location_increment_column(&l->location);
    }
}

static char get_next(ac_lex* l) {

    consume_one(l);

    return *l->cur;
}

static void skipn(ac_lex* l, unsigned n) {

    for (unsigned i = 0; i < n; ++i) {
        consume_one(l);
    }

    if (l->cur > l->end) {
        fprintf(stderr, "skipn, buffer overflow\n");
    }
}

static void skip_whitespace(ac_lex* l) {
    consume_one(l);
    while (is_whitespace(l)) {
        consume_one(l);
    }
}

static void skip_whitespaces_and_comments(ac_lex* l) {

    for (;;) {

        /* Skip Whitespaces */
        if (is_whitespace(l)) {

            skip_whitespace(l);
            continue;
        }

        /* Skip comments */
        if (is_char(l, '/')) {
            /* Skip c comment */
            if (next_is(l, '*')) {
                consume_one(l); /*  Skip '/'  */
                consume_one(l); /*  Skip '*'  */
                while (!(is_char(l, '*') && next_is(l, '/')) && !is_eof(l)) {
                    consume_one(l);
                }

                if (is_char(l, '*') && next_is(l, '/')) {
                    consume_one(l); /* Skip '*' and set the cursor on the char after */
                    consume_one(l); /* Skip '/' and set the cursor on the char after */
                }

                continue;
            }
            else if (next_is(l, '/')) { /* Skip cpp comment */

                while (!is_eof(l) && !is_end_line(l)) {
                    consume_one(l);
                }
                /* We are on the the end of line char, continue will consume this char and  */
                /* potential whitespaces could be skipped */
                continue;
            }
        }
        /* If we reach here there is no more space or comment */
        break;
    } /* end for (;;) */
}

const ac_token* parse_ascii_char_literal(ac_lex* l) {
    assert(is_char(l, '"'));

    consume_one(l); /* ignore the quote char */

    const char* start = l->cur;
    int n = 0;
    while (l->cur[0] && is_not_char(l, '"') && is_not_char(l, '\n')) {
        consume_one(l);
        ++n;
    }

    if (is_not_char(l, '"')) {
        fprintf(stderr, "Lexer Error: literal string is not well terminated with '\"'\n");
        return token_error(l);
    }
    else {
        consume_one(l);
    }

    l->token.type = ac_token_type_LITERAL_STRING;
    if (n == 0) {
        set_token_value(l, 0, 0);  /* clear token data */
    }
    else {
        set_token_value(l, start, n);
    }
    return &l->token;
}

enum base_type
{
    base2 = 2,
    base8 = 8,
    base10 = 10,
    base16 = 16,
};

static inline int atoi_base16(char c) {
    int result = -1;

    if (c >= 'a' && c <= 'f') {
        result = c - 'a' + 10;
    }
    else if (c >= 'A' && c <= 'F') {
        result = c - 'A' + 10;
    }
    else if (c >= '0' && c <= '9') {
        result = c - '0';
    }

    return result;
}

static inline int atoi_base10(char c) {
    int result = -1;

    if (c >= '0' && c <= '9') {
        result = c - '0';
    }

    return result;
}

static inline int atoi_base8(char c) {
    int result = -1;

    if (c >= '0' && c <= '7') {
        result = c - '0';
    }

    return result;
}

static inline int atoi_base2(char c) {
    int result = -1;

    if (c == '0' || c == '1') {
        result = c - '0';
    }

    return result;
}


static inline bool try_atoi_base(char ch, enum base_type base, int* result) {
    switch (base)
    {
    case base16: *result = atoi_base16(ch);
        break;
    case base10: *result = atoi_base10(ch);
        break;
    case base8: *result = atoi_base8(ch);
        break;
    case base2: *result = atoi_base2(ch);
        break;
    }

    return *result != -1;
}

static double stb__clex_pow(double base, unsigned int exponent)
{
    double value = 1;
    for (; exponent; exponent >>= 1) {
        if (exponent & 1)
            value *= base;
        base *= base;
    }
    return value;
}

static const ac_token* parse_number(ac_lex* l) {

    enum base_type base = base10;

    int previous_acc = 0;
    int acc = 0;
    int temp = 0;

    ac_token_float f = {0};

    ac_token_int i = { 0 };

    /* try parse hex integer */
    if (is_str(l, "0x", 2)
        || is_str(l, "0X", 2)) {
        skipn(l, 2); /* skip 0x or 0X */
        base = base16;
    }
    /* try parse binary integer */
    else if (is_str(l, "0b", 2)
        || is_str(l, "0B", 2)) {
        skipn(l, 2); /* skip 0b or 0B */
        base = base2;
    }
    /* try parse octal integer */
    else if (is_str(l, "0o", 2)
        || is_str(l, "0o", 2)) {
        skipn(l, 2); /* skip 0o or 0O */
        base = base8;
    }

    while (try_atoi_base(l->cur[0], base, &temp))
    {
        previous_acc = acc;
        acc = acc * base + temp;

        if (acc < previous_acc)
            f.overflow = true;

        consume_one(l);
    };

    /* try parse float */
    if (is_char(l, '.') && (base == base10 || (l->options.enable_hex_float && base == base16)))
    {
        f.is_double = true;

        uint64_t power_ = 1;
        consume_one(l); /* skip the dot */

        double f_acc = 0;
        double previous_f_acc = 0;
        while (try_atoi_base(l->cur[0], base, &temp))
        {
            previous_f_acc = f_acc;
            f_acc = f_acc * base + (double)temp;
            power_ *= base;

            if (f_acc < previous_f_acc)
                f.overflow = true;

            consume_one(l);
        }

        f_acc = (double)acc + (f_acc * (1.0 / (double)power_));

        bool has_exponent;
        if (l->options.enable_hex_float && base == base16) {
            /* exponent required for hex float literal */
            if (is_char(l, 'p') && is_char(l, 'P')) {
                ac_report_error_loc(l->location, "parse_number: internal error while parsing hex_float exponent\n");
                return token_error(l);
            }
            has_exponent = true;
        }
        else
        {
            has_exponent = (is_char(l, 'e') || is_char(l, 'E'));
        }

        if (has_exponent) {
            int neg = next_is(l, '-');
            unsigned int exponent = 0;
            double power = 1;
            consume_one(l); /* consume exponent p, P, e or E */

            if (is_char(l, '-') || is_char(l, '+'))
                consume_one(l);

            while (try_atoi_base(l->cur[0], base10, &temp))
            {
                exponent = exponent * base10 + temp;
                consume_one(l);
            }

            if (l->options.enable_hex_float && base == base16)
                power = stb__clex_pow(2, exponent);
            else
                power = stb__clex_pow(10, exponent);

            f_acc *= neg ? (1.0 / power) : power;
        }

        if (is_char(l, 'f')) {
            consume_one(l);
            f.is_double = false;

            if (f.value > FLT_MAX)
                f.overflow = true;
        }

        /* assign float value and set it to the current token */
        f.value = f_acc;
        l->token.type = ac_token_type_LITERAL_FLOAT;
        l->token.u.f = f;
        return &l->token;
    }
    else
    {
        i.value = acc;
        l->token.type = ac_token_type_LITERAL_INTEGER;
        l->token.u.i = i;
        return &l->token;
    }
} /* parse_number */

static void set_token_value(ac_lex* l, const char* data, size_t len) {
    l->token.text.data = data;
    l->token.text.size = len;
}

static const ac_token* token_error(ac_lex* l) {
    l->token.type = ac_token_type_ERROR;
    return &l->token;
}

static const ac_token* token_eof(ac_lex* l) {
    l->token.type = ac_token_type_EOF;
    return &l->token;
}

static const ac_token* set_token(ac_lex* l, enum ac_token_type type) {
    l->token.type = type;
    size_t size = token_str_len(type);
    set_token_value(l, l->cur, size);
    skipn(l, size);
    return &l->token;
}

/*
-------------------------------------------------------------------------------
ac_token
-------------------------------------------------------------------------------
*/

struct keyword_item {
    enum ac_token_type type;
    const char* name;
    size_t size;
};

static struct keyword_item keyword_items[] = {

    { ac_token_type_NONE, "<none>", 6 },
    { ac_token_type_AMP, "&", 1 },
    { ac_token_type_AMP_EQUAL, "&=", 2 },
    { ac_token_type_ARROW, "->", 2 },
    { ac_token_type_BACKSLASH, "\\", 1},
    { ac_token_type_BRACE_L, "{", 1},
    { ac_token_type_BRACE_R, "}", 1 },
    { ac_token_type_CARET , "^", 1 },
    { ac_token_type_CARET_EQUAL, "^=", 2 },
    { ac_token_type_COLON, ":", 1 },
    { ac_token_type_COMMA, ",", 1 },
    { ac_token_type_DOLLAR, "$", 1 },
    { ac_token_type_DOT, ".", 1 },
    { ac_token_type_DOUBLE_AMP, "&&", 2 },
    { ac_token_type_DOUBLE_DOT, "..", 2 },
    { ac_token_type_DOUBLE_EQUAL, "==", 2 },
    { ac_token_type_DOUBLE_GREATER, ">>", 2 },
    { ac_token_type_DOUBLE_LESS, "<<", 2 },
    { ac_token_type_DOUBLE_PIPE, "||", 2 },
    { ac_token_type_DOUBLE_QUOTE , "\"", 1 },
    { ac_token_type_ELSE, "else", 4 },
    { ac_token_type_ENUM, "enum", 4 },
    { ac_token_type_EOF, "end-of-line", 11},
    { ac_token_type_EQUAL, "=", 1 },
    { ac_token_type_ERROR, "<error>", 7 },
    { ac_token_type_EXCLAM, "!", 1 },
    { ac_token_type_FOR, "for", 3 },
    { ac_token_type_GREATER, ">", 1 },
    { ac_token_type_GREATER_EQUAL, ">=", 2 },
    { ac_token_type_HASH, "#", 1 },
    { ac_token_type_IDENTIFIER, "<identifier>", 12 },
    { ac_token_type_IF, "if", 2 },
    { ac_token_type_LESS, "<", 1 },
    { ac_token_type_LESS_EQUAL, "<=", 2 },
    { ac_token_type_LITERAL_BOOL, "<literal-bool>", 14 },
    { ac_token_type_LITERAL_CHAR, "<literal-char>", 14 },
    { ac_token_type_LITERAL_FLOAT, "<literal-float>", 14 },
    { ac_token_type_LITERAL_INTEGER, "<literal-integer>", 17 },
    { ac_token_type_LITERAL_STRING, "<literal-string>", 16 },
    { ac_token_type_MINUS, "-", 1 },
    { ac_token_type_MINUS_EQUAL, "-=", 2 },
    { ac_token_type_NOT_EQUAL, "!=", 2 },
    { ac_token_type_PAREN_L, "(", 1 },
    { ac_token_type_PAREN_R, ")", 1 },
    { ac_token_type_PERCENT, "%", 1 },
    { ac_token_type_PERCENT_EQUAL, "%=", 2 },
    { ac_token_type_PIPE, "|", 1 },
    { ac_token_type_PIPE_EQUAL, "|=", 2 },
    { ac_token_type_PLUS, "+", 1 },
    { ac_token_type_PLUS_EQUAL, "+=", 2 },
    { ac_token_type_QUESTION, "?", 1 },
    { ac_token_type_QUOTE, "'", 1 },
    { ac_token_type_RETURN, "return", 6 },
    { ac_token_type_SEMI_COLON, ";", 1 },
    { ac_token_type_SIZEOF, "sizeof", 6 },
    { ac_token_type_SLASH, "/", 1 },
    { ac_token_type_SLASH_EQUAL, "/=", 2 },
    { ac_token_type_SQUARE_L, "[", 1 },
    { ac_token_type_SQUARE_R, "]", 1 },
    { ac_token_type_STAR, "*", 1 },
    { ac_token_type_STAR_EQUAL, "*=", 2 },
    { ac_token_type_STRUCT, "struct", 6 },
    { ac_token_type_TILDE, "~", 1 },
    { ac_token_type_TILDE_EQUAL, "~=", 2 },
    { ac_token_type_TRIPLE_DOT, "...", 3 },
    { ac_token_type_TYPEOF, "typeof", 5 },
    { ac_token_type_WHILE, "while", 5 },
};

const char* ac_token_type_to_str(enum ac_token_type type) {
    return keyword_items[type].name;
}

strv ac_token_type_to_strv(enum ac_token_type type) {

    const struct keyword_item keyword = keyword_items[type];
    return strv_make_from(keyword.name, keyword.size);
}

const char* ac_token_to_str(ac_token token) {

    if (token.type == ac_token_type_IDENTIFIER)
    {
        return "identifier";
    }

    return ac_token_type_to_str(token.type);
}

strv ac_token_to_strv(ac_token token) {

    if (token.type == ac_token_type_IDENTIFIER)
    {
        return (strv){ token.text.size, token.text.data };
    }

    return ac_token_type_to_strv(token.type);
}

static size_t token_str_len(enum ac_token_type type) {
    assert(!token_type_is_literal(type));

    return keyword_items[type].size;
}

static bool try_parse_keyword(const char* str, size_t len, enum ac_token_type* type) {

    const struct keyword_item* keyword_cursor = &keyword_items[0];
    const struct keyword_item* keyword_end = &keyword_items[ac_token_type_COUNT];

    while (keyword_cursor < keyword_end)
    {
        if (len == keyword_cursor->size
            && strncmp(keyword_cursor->name, str, keyword_cursor->size) == 0) {
            *type = keyword_cursor->type;
            return true;
        }
        ++keyword_cursor;
    }

    return false;
}

static bool token_type_is_literal(enum ac_token_type type) {
    switch (type) {
    case ac_token_type_LITERAL_BOOL:
    case ac_token_type_LITERAL_CHAR:
    case ac_token_type_LITERAL_INTEGER:
    case ac_token_type_LITERAL_FLOAT:
    case ac_token_type_LITERAL_STRING:
        return true;
    }
    return false;
}

static void location_increment_row(ac_location* l) {
    l->row++;
    l->col = 0;
    l->pos++;
}

static void location_increment_column(ac_location* l) {
    l->col++;
    l->pos++;
}