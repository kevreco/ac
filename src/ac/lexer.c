#include "lexer.h"

#include "float.h"  /* FLT_MAX */
#include "stdint.h" /* uint64_t */

#include "global.h"

#define AC_EOF ('\0')

enum base_type {
    base2 = 2,
    base8 = 8,
    base10 = 10,
    base16 = 16,
};

typedef struct token_info token_info;
struct token_info {
    bool is_supported;
    enum ac_token_type type;
    strv name;
};

token_info token_infos[];

static bool is_end_line(const ac_lex* l);          /* current char is alphanumeric */
static bool is_horizontal_whitespace(char c);      /* char is alphanumeric */
static bool is_identifier(char c);          /* char is allowed in identifier */
static bool is_eof(const ac_lex* l);               /* current char is end of line */
static bool is_char(const ac_lex* l, char c);      /* current char is equal to char  */
static bool is_not_char(const ac_lex* l, char c);  /* current char is not equal to char  */
static bool is_str(const ac_lex* l, const char* str, size_t count); /* current char is equal to string to string */
static bool next_is(const ac_lex* l, char c);      /* next char equal to */
static bool next_next_is(const ac_lex* l, char c); /* next next char equal to */

enum consumed_type {
    consumed_char_WAS_SPACE,
    consumed_char_WAS_NOT_SPACE
};

static enum consumed_type consume_one(ac_lex* l); /* goto next char */
static int char_after_stray(ac_lex* l);           /* Get the character after stray such as \\n, \\r or \\r\n */
static int next_char_without_stray(ac_lex* l, int* c);

static void skipn(ac_lex* l, unsigned n); /* skip n char */

static const ac_token* token_from_text(ac_lex* l, enum ac_token_type type, strv text); /* set current token and got to next */
static const ac_token* token_error(ac_lex* l); /* set current token to error and returns it. */
static const ac_token* token_eof(ac_lex* l);   /* set current token to eof and returns it. */
static const ac_token* token_from_type(ac_lex* l, enum ac_token_type type);
static const ac_token* token_from_single_char(ac_lex* l, enum ac_token_type type); /* set current token and got to next */

static double power(double base, unsigned int exponent);
static bool is_binary_digit(char c);
static bool is_octal_digit(char c);
static bool is_decimal_digit(char c);
static bool is_hex_digit(char c);
static const ac_token* parse_ascii_char_literal(ac_lex* l);

static bool parse_integer_suffix(ac_lex* l, ac_token_number* num); /* Parse integer suffix like 'uLL' */
static bool parse_float_suffix(ac_lex* l, ac_token_number* num);   /* Parse float suffix like 'f' or 'l' */
static const ac_token* token_integer_literal(ac_lex* l, ac_token_number num); /* Create integer token from parsed value. */
static const ac_token* token_float_literal(ac_lex* l, ac_token_number num);   /* Create float token from parsed value. */
static const ac_token* parse_float_literal(ac_lex* l, ac_token_number num, enum base_type base); /* Parse float after the whole number part. */
static int hex_to_int(const char* c, size_t len);
static const ac_token* parse_integer_or_float_literal(ac_lex* l);

/* Register keywords or known identifier. It helps to retrieve the type of a token from it's text value. */
static void register_known_identifier(ac_lex* l, strv sv, enum ac_token_type type);
/* We return a ac_token because we want a string view and a token type. */
static ac_token create_or_reuse_identifier(ac_lex* l, strv sv);
static ac_token create_or_reuse_identifier_h(ac_lex* l, strv sv, size_t hash);

static size_t token_str_len(enum ac_token_type type);

static bool token_type_is_literal(enum ac_token_type type);

static void location_increment_row(ac_location* l);
static void location_increment_column(ac_location* l);

/*
-------------------------------------------------------------------------------
w_lex
-------------------------------------------------------------------------------
*/

void ac_lex_init(ac_lex* l, ac_manager* mgr, strv content, const char* filepath)
{
    AC_ASSERT(content.data);
    AC_ASSERT(content.size);

    /* Construct. */
    {
        memset(l, 0, sizeof(ac_lex));

        l->mgr = mgr;

        l->options.reject_hex_float = mgr->options.reject_hex_float;
        l->options.reject_stray = mgr->options.reject_stray;
    }

    /* Initialize source. */
    {
        l->filepath = filepath;

        ac_location_init_with_file(&l->location, filepath, content);

        if (content.data && content.size) {
            l->src = content.data;
            l->end = content.data + content.size;
            l->cur = content.data;
            l->len = content.size;
        }
    }

    dstr_init(&l->tok_buf);

    /* Register keywords and known tokens. */
    int i = 0;
    for (; i < ac_token_type_COUNT; ++i)
    {
        token_info item = token_infos[i];
        if (item.is_supported)
        {
            register_known_identifier(l, item.name, item.type);
        }
    }
}

void ac_lex_destroy(ac_lex* l)
{
    dstr_destroy(&l->tok_buf);
    memset(l, 0, sizeof(ac_lex));
}

#define NEXT_CHAR_NO_STRAY(l, c) next_char_without_stray(l, &c)

const ac_token* ac_lex_goto_next(ac_lex* l)
{
    l->leading_location = l->location;

    if (is_eof(l)) {
        return token_eof(l);
    }

    int c;
switch_start:
    switch (l->cur[0]) {

    case '\\':
        if (l->options.reject_stray)
            goto before_default;
        if (!next_is(l, '\n')
            && !next_is(l, '\r')
            && !next_is(l, '\0'))
        {
            ac_report_error("Stray '\\' in program.");
            return token_eof(l);
        }

        char_after_stray(l); /* Skip stray. */
        goto switch_start;
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
        consume_one(l);
        return token_from_text(l, ac_token_type_NEW_LINE, strv_make_from(start, l->cur - start));
    }
    case '#': return token_from_single_char(l, ac_token_type_HASH);
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

    case '=': {
        NEXT_CHAR_NO_STRAY(l, c); /* Skip '=' */
        if (c == '=') {
            NEXT_CHAR_NO_STRAY(l, c); /* Skip '=' */
            return token_from_type(l, ac_token_type_DOUBLE_EQUAL);
        }
        return token_from_type(l, ac_token_type_EQUAL);
    }

    case '!': {
        NEXT_CHAR_NO_STRAY(l, c); /* Skip '!' */
        if (c == '=') {
            NEXT_CHAR_NO_STRAY(l, c); /* Skip '=' */
            return token_from_type(l, ac_token_type_NOT_EQUAL);
        }
        return token_from_type(l, ac_token_type_EXCLAM);
    }

    case '<': {
        NEXT_CHAR_NO_STRAY(l, c); /* Skip '<' */
        if (c == '<') {
            NEXT_CHAR_NO_STRAY(l, c); /* Skip '<' */
            return token_from_type(l, ac_token_type_DOUBLE_LESS);
        }
        else if (c == '=') {
            NEXT_CHAR_NO_STRAY(l, c); /* Skip '=' */
            return token_from_type(l,  ac_token_type_LESS_EQUAL);
        }
        return token_from_type(l,  ac_token_type_LESS);
    }

    case '>': {
        NEXT_CHAR_NO_STRAY(l, c); /* Skip '>' */
        if (c == '>') {
            NEXT_CHAR_NO_STRAY(l, c); /* Skip '>' */
            return token_from_type(l, ac_token_type_DOUBLE_GREATER);
        }
        else if (c == '=') {
            NEXT_CHAR_NO_STRAY(l, c); /* Skip '=' */
            return token_from_type(l, ac_token_type_GREATER_EQUAL);
        }
        return token_from_type(l, ac_token_type_GREATER);
    }

    case '&': {
        NEXT_CHAR_NO_STRAY(l, c); /* Skip '&' */
        if (c == '&') {
            NEXT_CHAR_NO_STRAY(l, c); /* Skip '&' */
            return token_from_type(l, ac_token_type_DOUBLE_AMP);
        }
        else if (c == '=') {
            NEXT_CHAR_NO_STRAY(l, c); /* Skip '=' */
            return token_from_type(l, ac_token_type_AMP_EQUAL);
        }
        return token_from_type(l, ac_token_type_AMP);
    }

    case '|': {
        NEXT_CHAR_NO_STRAY(l, c); /* Skip '|' */
        if (c == '|') {
            NEXT_CHAR_NO_STRAY(l, c); /* Skip '|' */
            return token_from_type(l, ac_token_type_DOUBLE_PIPE);
        }
        else if (c == '=') {
            NEXT_CHAR_NO_STRAY(l, c); /* Skip '=' */
            return token_from_type(l, ac_token_type_PIPE_EQUAL);
        }
        return token_from_type(l, ac_token_type_PIPE);
    }

    case '+': {
        NEXT_CHAR_NO_STRAY(l, c); /* Skip '+' */
        if (c == '=') {
            NEXT_CHAR_NO_STRAY(l, c); /* Skip '=' */
            return token_from_type(l, ac_token_type_PLUS_EQUAL);
        }
        return token_from_type(l, ac_token_type_PLUS);
    }
    case '-': {
        NEXT_CHAR_NO_STRAY(l, c); /* Skip '-' */
        if (c == '=') {
            NEXT_CHAR_NO_STRAY(l, c); /* Skip '=' */
            return token_from_type(l, ac_token_type_MINUS_EQUAL);
        }
        else if (c == '>') {
            NEXT_CHAR_NO_STRAY(l, c); /* Skip '>' */
            return token_from_type(l, ac_token_type_ARROW);
        }
        return token_from_type(l, ac_token_type_MINUS);
    }

    case '*': {
        NEXT_CHAR_NO_STRAY(l, c); /* Skip '*' */
        if (c == '=') {
            NEXT_CHAR_NO_STRAY(l, c); /* Skip '=' */
            return token_from_type(l, ac_token_type_STAR_EQUAL);
        }
        return token_from_type(l, ac_token_type_STAR);
    }

    case '/': {
        NEXT_CHAR_NO_STRAY(l, c); /* Skip '/' */
        if (c == '=') {
            NEXT_CHAR_NO_STRAY(l, c); /* Skip '=' */
            return token_from_type(l, ac_token_type_SLASH_EQUAL);
        }
        else if (c == '/') {  /* Parse inline comment. */
            const char* start = l->cur - 1;
            consume_one(l); /* Skip '/' */
            
            while (!is_eof(l) && !is_end_line(l)) {
                consume_one(l);
            }
            return token_from_text(l, ac_token_type_COMMENT, strv_make_from(start, l->cur - start));
        }
        else if (c == '*') {  /* Parse C comment. */
            const char* start = l->cur - 1;
            consume_one(l); /* Skip opening comment tag. */
            
            ac_location location = l->location;
            /* Skip until closing comment tag. */
            while (!is_eof(l) && !(is_char(l, '*') && next_is(l, '/'))) {
                consume_one(l);
            }

            if (is_eof(l)) {
                ac_report_error_loc(location, "Cannot find closing comment tag '*/'.\n");
            }
            skipn(l, 2); /* Skip closing comment tag. */
            return token_from_text(l, ac_token_type_COMMENT, strv_make_from(start, l->cur - start));;
        }

        return token_from_type(l, ac_token_type_SLASH);
    }

    case '%': {
        NEXT_CHAR_NO_STRAY(l, c); /* Skip '%' */
        if (c == '=') {
            NEXT_CHAR_NO_STRAY(l, c); /* Skip '=' */
            return token_from_type(l, ac_token_type_PERCENT_EQUAL);
        }
        return token_from_type(l, ac_token_type_PERCENT);
    }

    case '^': {
        NEXT_CHAR_NO_STRAY(l, c); /* Skip '^' */
        if (c == '=') {
            NEXT_CHAR_NO_STRAY(l, c); /* Skip '=' */
            return token_from_type(l, ac_token_type_CARET_EQUAL);
        }

        return token_from_type(l, ac_token_type_CARET);
    }

    case '.': {
       
        /* Float can also starts with a dot. */
        const ac_token* token = parse_integer_or_float_literal(l);
        if (token)
            return token;

        c = l->cur[0];
        if (c == '.') {
            NEXT_CHAR_NO_STRAY(l, c); /* Skip '.' */
            if (c == '.') {
                NEXT_CHAR_NO_STRAY(l, c); /* Skip '.' */
                return token_from_type(l, ac_token_type_TRIPLE_DOT);
            }
            return token_from_type(l, ac_token_type_DOUBLE_DOT);
        }
        return token_from_type(l, ac_token_type_DOT);
    }
    
    case '"' : return parse_ascii_char_literal(l);
    case '\0': return token_eof(l);

    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
        return parse_integer_or_float_literal(l);

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

        if (is_char(l, '\\')) /* Stray found. We need to create a new string without it and reparse the identifier. */
        {
            int c;
            dstr_assign(&l->tok_buf, strv_make_from(start, l->cur - start));
            l->cur--; /* Adjust buffer so the current stray is eaten (if it's indeed a stray). */
            NEXT_CHAR_NO_STRAY(l, c);
            while (is_identifier(c))
            {
                hash = AC_HASH(hash, l->cur[0]);
                dstr_append_char(&l->tok_buf, c);
                NEXT_CHAR_NO_STRAY(l, c);
            }

            token = create_or_reuse_identifier_h(l, dstr_to_strv(&l->tok_buf), hash);
        }
        else
        {
            token = create_or_reuse_identifier_h(l, strv_make_from(start, n), hash);
        }

        return token_from_text(l, token.type, token.text);
    }

    before_default:
    default: {
        if (l->cur[0] >= 0x80 && l->cur[0] <= 0xFF) /* Start of utf8 identifiers */
        {
            goto parse_identifier;
        }
        ac_report_error("Internal error: Unhandled character: %c", l->cur[0]);
        return token_error(l);
        
    } /* end default case */

    } /* end switch */

    return token_error(l);
}

ac_token ac_lex_token(ac_lex* l) {
    return l->token;
}

const ac_token* ac_lex_token_ptr(ac_lex* l) {
    return &l->token;
}

bool ac_lex_expect(ac_lex* l, enum ac_token_type type) {
    ac_location current_location = l->location;
    ac_token current = l->token;
    if (current.type != type)
    {
        strv expected = ac_token_type_to_strv(type);
        strv actual = ac_token_to_strv(current);

        ac_report_error_loc(current_location, "Syntax error: expected '%.*s', actual '%.*s'\n"
            , expected.size, expected.data
            , actual.size, actual.data
        );

        return false;
    }
    return true;
}

static inline bool _is_end_line(char c) {
    return (c == '\n' || c == '\r');
}

static inline bool is_end_line(const ac_lex* l) {
    return _is_end_line(l->cur[0]);
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
static inline enum consumed_type consume_one(ac_lex* l) {

    if (is_char(l, '\n') || is_char(l, '\r')) {

        /* \r\n is considered as "one" but two characters are skipped */
        /* thanks to stb_c_lexer for this nice trick */
        int num_char = (l->cur[0] + l->cur[1] == '\r' + '\n') ? 2 : 1;
        l->cur += num_char; /* skip newline */

        location_increment_row(&l->location);

        if (num_char == 2) /* Increment pos by one extra char. */
            l->location.pos++;

        return consumed_char_WAS_SPACE;
    }
    else {

        l->cur++;
        location_increment_column(&l->location);
        return consumed_char_WAS_NOT_SPACE;
    }
}

static int char_after_stray(ac_lex* l)
{
    AC_ASSERT(l->cur[0] == '\\');

    /* Skip all consecutive backslashes. */
    while (l->cur[0] == '\\')
    {
        consume_one(l); /* Skip '\\' */

        if (consume_one(l) != consumed_char_WAS_SPACE)
        {
            ac_report_error("Stray '\\' in program ");
            return AC_EOF;
        }
    }
    return l->cur[0];
}

static int next_char_without_stray(ac_lex* l, int* c)
{
    consume_one(l);
    *c = *l->cur;
    if (!l->options.reject_stray && *c == '\\') {
        *c = char_after_stray(l);
    }
    return *c;
}

static void skipn(ac_lex* l, unsigned n) {

    for (unsigned i = 0; i < n; ++i) {
        consume_one(l);
    }

    if (l->cur > l->end) {
        fprintf(stderr, "skipn, buffer overflow\n");
    }
}

static const ac_token* token_from_text(ac_lex* l, enum ac_token_type type, strv text) {
    l->token.type = type;
    l->token.text = text;
   
    return &l->token;
}

static const ac_token* token_error(ac_lex* l) {
    l->token.type = ac_token_type_ERROR;
    return &l->token;
}

static const ac_token* token_eof(ac_lex* l) {
    l->token.type = ac_token_type_EOF;
    return &l->token;
}

static const ac_token* token_from_type(ac_lex* l, enum ac_token_type type) {
    return token_from_text(l, type, token_infos[type].name);
}

static const ac_token* token_from_single_char(ac_lex* l, enum ac_token_type type) {
    const ac_token* t = token_from_type(l, type);
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

const ac_token* parse_ascii_char_literal(ac_lex* l) {
    assert(is_char(l, '"'));

    consume_one(l); /* ignore the quote char */

    const char* start = l->cur;
    ac_location loc = l->location;
    int n = 0;
    /* @TODO replace with is_not_eof? */
    while (l->cur[0] && is_not_char(l, '"') && is_not_char(l, '\n')) {
        consume_one(l);
        ++n;
    }

    if (is_not_char(l, '"')) {
        ac_report_error_loc(loc, "String literal is not well terminated with '\"'\n");
        return token_error(l);
    }
    else {
        consume_one(l);
    }
    return token_from_text(l, ac_token_type_LITERAL_STRING, strv_make_from(start, n));
}

/* Get next digit, ignoring quotes and underscores. */
#define NEXT_DIGIT(l, c) \
do { \
    NEXT_CHAR_NO_STRAY(l, c); \
    while (c == '\'' || c == '_') { \
        NEXT_CHAR_NO_STRAY(l, c); \
    } \
    dstr_append_char(&l->tok_buf, c); \
} while (0);

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
                ac_report_error("Invalid integer suffix. Too many 'u' or 'U'");
                return false;
            }
            num->is_unsigned = true;
            NEXT_DIGIT(l, c);
            break;
        }
        case 'l':
        case 'L':
        {
            L++;
            if (L > 2) {
                ac_report_error("Invalid integer suffix. Too many 'l' or 'L'");
                return false;
            }
            else {
                num->long_depth += 1;
            }

            NEXT_DIGIT(l, c);
        }
        }
    }

    if ((U + L) > 3
        || (c >= '0' && c <= '9')
        || (c >= 'a' && c <= 'z')
        || (c >= 'A' && c <= 'Z'))
    {
        ac_report_error("Invalid integer suffix: '%c'", c);
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
                ac_report_error_loc(l->location, "Invalid float suffix. Too many 'f' or 'F'");
                return false;
            }
            num->is_float = true;
            NEXT_DIGIT(l, c);
            break;
        }
        case 'l':
        case 'L':
        {
            L++;
            if (L > 1) {
                ac_report_error_loc(l->location, "Invalid float suffix. Too many 'l' or 'L'");
                return false;
            }
            else {
                num->is_double = true;
            }

            NEXT_DIGIT(l, c);
        }
        }
    }

    if ((c >= '0' && c <= '9')
        || (c >= 'a' && c <= 'z')
        || (c >= 'A' && c <= 'Z'))
    {
        ac_report_error_loc(l->location, "Invalid float suffix: '%c'", c);
        return false;
    }
    return true;
}

static const ac_token* token_integer_literal(ac_lex* l, ac_token_number num)
{
    l->token.type = ac_token_type_LITERAL_INTEGER;
    l->token.number = num;

    if (!parse_integer_suffix(l, &l->token.number))
    {
        return token_eof(l);
    }

    strv text = dstr_to_strv(&l->tok_buf);
    text.size -= 1; /* Remove 1 to remove last character which is not part of the value. */
    const ac_token t = create_or_reuse_identifier(l, text);
    l->token.text = t.text;
    return &l->token;
}

static const ac_token* token_float_literal(ac_lex* l, ac_token_number num)
{
    l->token.type = ac_token_type_LITERAL_FLOAT;
    l->token.number = num;

    if (!parse_float_suffix(l, &l->token.number))
    {
        return token_eof(l);
    }

    if (l->token.number.is_float && l->token.number.u.float_value > FLT_MAX)
    {
        l->token.number.overflow = true;
    }

    strv text = dstr_to_strv(&l->tok_buf);
    text.size -= 1; /* Remove 1 to remove last character which is not part of the value. */
    const ac_token t = create_or_reuse_identifier(l, text);
    l->token.text = t.text;
    return &l->token;
}

static const ac_token* parse_float_literal(ac_lex* l, ac_token_number num, enum base_type base)
{
    AC_ASSERT(base == 10 || base == 16 && "Can only parse decimal floats of hexadicemal floats.");

    double value = num.u.float_value;
    int exponent = 0;

    int c = l->cur[0];

    if (c == '.') {
        NEXT_DIGIT(l, c);

        if (c == '.') { return NULL; } /* If there is a dot after a dot then we are not parsing a number. */
        double pow, addend = 0;

        if (base == base10)
        {
            for (pow = 1; is_decimal_digit(c); pow *= base) {
                addend = addend * base + (c - '0');
                NEXT_DIGIT(l, c);
            }
        }
        else /* base == base16 */
        {
            for (pow = 1; ; pow *= base) {
                if (c >= '0' && c <= '9') {
                    addend = addend * base + (c - '0');
                    NEXT_DIGIT(l, c);
                }
                else if (c >= 'a' && c <= 'f') {
                    addend = addend * base + 10 + (c - 'a');
                    NEXT_DIGIT(l, c);
                }
                else if (c >= 'A' && c <= 'F') {
                    addend = addend * base + 10 + (c - 'A');
                    NEXT_DIGIT(l, c);
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
            NEXT_DIGIT(l, c); /* Skip 'p' or 'P' */
        }
        else
        {
            ac_report_error_loc(l->location, "Invalid exponent in hex float.");
            return 0;
        }
    }
    else if (c == 'e' || c == 'E')
    {
        exponent = 1;
        NEXT_DIGIT(l, c); /* Skip 'e' or 'E' */
    }

    int sign = 1;
    if (exponent) {
        unsigned int exponent = 0;
        double power_ = 1;
        if (c == '-' || c == '+')
        {
            sign = '-' ? -1 : 1;
            NEXT_DIGIT(l, c); /* Skip '-' or '+' */
        }
        while (c >= '0' && c <= '9') {
            exponent = exponent * 10 + (c - '0');
            NEXT_DIGIT(l, c);
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

static int hex_to_int(const char* c, size_t len)
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

static const ac_token* parse_integer_or_float_literal(ac_lex* l)
{
    ac_token_number num = { 0 };
    int c = l->cur[0];

    dstr_clear(&l->tok_buf);
    dstr_append_char(&l->tok_buf, c);

    bool leading_zero = c == '0';

    if (leading_zero)
    {
        NEXT_DIGIT(l, c); /* Skip '0' */
        /* Need to parse hex integer or float */
        if (c == 'x' || c == 'X') {
            NEXT_DIGIT(l, c); /* Skip 'x' or 'X */
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
                NEXT_DIGIT(l, c);
            }
            if (!is_eof(l)) {
                if (c == '.' || c == 'p' || c == 'P') {
                    return parse_float_literal(l, num, base16);
                }
            }
            if (buffer_size == l->tok_buf.size) /* Nothing after 0x was parsed */
            {
                ac_report_error_loc(l->leading_location, "Invalid hexadecimal value.");
                return token_eof(l);
            }
            /* Not float so we return an integer. */
            num.u.int_value = hex_to_int(l->tok_buf.data, l->tok_buf.size);
            return token_integer_literal(l, num);
        }
        /* Need to parse binary integer */
        else if (c == 'b' || c == 'B') {
            NEXT_DIGIT(l, c); /* Skip 'b' or 'B' */
            size_t buffer_size = l->tok_buf.size;
            /* @FIXME check for overflow. */
            int n = 0;
            while (is_binary_digit(c)) {
                n = n * base2 + (c - '0');
                NEXT_DIGIT(l, c);
            }
            if (buffer_size == l->tok_buf.size) /* Nothing after 0b was parsed */
            {
                ac_report_error_loc(l->leading_location, "Invalid binary value.");
                return token_eof(l);
            }
            num.u.int_value = n;
            return token_integer_literal(l, num);
        }
    }

    /* Try to parse float starting with decimals (not hexadecimal). */

    int n = 0;
    while (is_decimal_digit(c)) {
        n = n * base10 + (c - '0'); /* @FIXME check for overflow. */
        NEXT_DIGIT(l, c);
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

    if (is_eof(l))
    {
        ac_report_error_loc(l->leading_location, "Unexpected end of file after number literal.");
        return token_eof(l);
    }

    /* Return the parsed integer. */
    num.u.int_value = n;
    return token_integer_literal(l, num);
}

static void register_known_identifier(ac_lex* l, strv sv, enum ac_token_type type)
{
    ac_token t;
    t.text = sv;
    t.type = type;
    ht_insert_h(&l->mgr->identifiers, &t, ac_djb2_hash((char*)sv.data, sv.size));
}

static ac_token create_or_reuse_identifier(ac_lex* l, strv ident)
{
    return create_or_reuse_identifier_h(l, ident, ac_djb2_hash((char*)ident.data, ident.size));
}

static ac_token create_or_reuse_identifier_h(ac_lex* l, strv ident, size_t hash)
{
    ac_token token_for_search;
    token_for_search.type = ac_token_type_NONE;
    token_for_search.text = ident;
    ac_token* result = (ac_token*)ht_get_item_h(&l->mgr->identifiers, &token_for_search, hash);

    /* If the identifier is new, a new entry is created. */
    if (result == NULL)
    {
        ac_token t;
        t.text.data = ac_allocator_allocate(&l->mgr->identifiers_arena.allocator, ident.size);
        t.text.size = ident.size;
        t.type = ac_token_type_IDENTIFIER;
        memcpy((char*)t.text.data, ident.data, ident.size);

        ht_insert_h(&l->mgr->identifiers, &t, hash);
        return t;
    }

    return *result;
}

/*
-------------------------------------------------------------------------------
ac_token
-------------------------------------------------------------------------------
*/

#define TOKEN_INFO_COUNT (sizeof(token_infos) / sizeof(token_infos[0]))

token_info token_infos[] = {

    { false, ac_token_type_NONE, STRV("<none>") },

    /* Keywords */

    { false, ac_token_type_ALIGNAS, STRV("alignas") },
    { false, ac_token_type_ALIGNAS2, STRV("_Alignas") },
    { false, ac_token_type_ALIGNOF, STRV("alignof") },
    { false, ac_token_type_ALIGNOF2, STRV("_Alignof") },
    { false, ac_token_type_ATOMIC, STRV("_Atomic") },
    { false, ac_token_type_AUTO, STRV("auto") },
    { false, ac_token_type_BOOL, STRV("bool") },
    { false, ac_token_type_BOOL2, STRV("_Bool") },
    { false, ac_token_type_BITINT, STRV("_BitInt") },
    { false, ac_token_type_BREAK, STRV("break") },
    { false, ac_token_type_CASE, STRV("case") },
    { false, ac_token_type_CHAR, STRV("char") },
    { false, ac_token_type_CONST, STRV("const") },
    { false, ac_token_type_CONSTEXPR, STRV("constexpr") },
    { false, ac_token_type_CONTINUE, STRV("continue") },
    { false, ac_token_type_COMPLEX, STRV("_Complex") },
    { false, ac_token_type_DECIMAL128, STRV("_Decimal128") },
    { false, ac_token_type_DECIMAL32, STRV("_Decimal32") },
    { false, ac_token_type_DECIMAL64, STRV("_Decimal64") },
    { false, ac_token_type_DEFAULT, STRV("default") },
    { false, ac_token_type_DO, STRV("do") },
    { false, ac_token_type_DOUBLE, STRV("double") },
    { false, ac_token_type_ELSE, STRV("else") },
    { false, ac_token_type_ENUM, STRV("enum") },
    { false, ac_token_type_EXTERN, STRV("extern") },
    { false, ac_token_type_FALSE, STRV("false") },
    { false, ac_token_type_FLOAT, STRV("float") },
    { false, ac_token_type_FOR, STRV("for") },
    { false, ac_token_type_GENERIC, STRV("_Generic") },
    { false, ac_token_type_GOTO, STRV("goto") },
    { false, ac_token_type_IF, STRV("if") },
    { false, ac_token_type_INLINE, STRV("inline") },
    /* @TODO properly support int. */
    { false, ac_token_type_INT, STRV("int") },
    { false, ac_token_type_IMAGINARY, STRV("_Imaginary") },
    { false, ac_token_type_LONG, STRV("long") },
    { false, ac_token_type_NORETURN, STRV("_Noreturn") },
    { false, ac_token_type_NULLPTR, STRV("nullptr") },
    { false, ac_token_type_REGISTER, STRV("register") },
    { false, ac_token_type_RESTRICT, STRV("restrict") },
    { true,  ac_token_type_RETURN, STRV("return") },
    { false, ac_token_type_SHORT, STRV("short") },
    { false, ac_token_type_SIGNED, STRV("signed") },
    { false, ac_token_type_SIZEOF, STRV("sizeof") },
    { false, ac_token_type_STATIC, STRV("static") },
    { false, ac_token_type_STATIC_ASSERT, STRV("static_assert") },
    { false, ac_token_type_STATIC_ASSERT2, STRV("_Static_assert") },
    { false, ac_token_type_STRUCT, STRV("struct") },
    { false, ac_token_type_SWITCH, STRV("switch") },
    { false, ac_token_type_THREAD_LOCAL, STRV("thread_local") },
    { false, ac_token_type_THREAD_LOCAL2, STRV("_Thread_local") },
    { false, ac_token_type_TRUE, STRV("true") },
    { false, ac_token_type_TYPEDEF, STRV("typedef") },
    { false, ac_token_type_TYPEOF, STRV("typeof") },
    { false, ac_token_type_TYPEOF_UNQUAL, STRV("typeof_unqual") },
    { false, ac_token_type_UNION, STRV("union") },
    { false, ac_token_type_UNSIGNED, STRV("unsigned") },
    { false, ac_token_type_VOID, STRV("void") },
    { false, ac_token_type_VOLATILE, STRV("volatile") },
    { false, ac_token_type_WHILE, STRV("while") },

    /* Symbols */

    { false, ac_token_type_AMP, STRV("&") },
    { false, ac_token_type_AMP_EQUAL, STRV("&=") },
    { false, ac_token_type_ARROW, STRV("->") },
    { true,  ac_token_type_BACKSLASH, STRV("\\") },
    { true,  ac_token_type_BRACE_L, STRV("{") },
    { true,  ac_token_type_BRACE_R, STRV("}") },
    { false, ac_token_type_CARET, STRV("^") },
    { false, ac_token_type_CARET_EQUAL, STRV("^=") },
    { false, ac_token_type_COLON, STRV(":") },
    { true, ac_token_type_COMMA, STRV(",") },
    { false, ac_token_type_COMMENT, STRV("<comment>") },
    { false, ac_token_type_DOLLAR, STRV("$") },
    { false, ac_token_type_DOT, STRV(".") },
    { false, ac_token_type_DOUBLE_AMP, STRV("&&") },
    { false, ac_token_type_DOUBLE_DOT, STRV("..") },
    { false, ac_token_type_DOUBLE_EQUAL, STRV("==") },
    { false, ac_token_type_DOUBLE_GREATER, STRV(">>") },
    { false, ac_token_type_DOUBLE_LESS, STRV("<<") },
    { false, ac_token_type_DOUBLE_PIPE, STRV("||") },
    { false, ac_token_type_DOUBLE_QUOTE , STRV("\"") },
    { true,  ac_token_type_EOF, STRV("<end-of-line>") },
    { true,  ac_token_type_EQUAL, STRV("=") },
    { false, ac_token_type_ERROR, STRV("<error>") },
    { true,  ac_token_type_EXCLAM, STRV("!") },
    { false, ac_token_type_GREATER, STRV(">") },
    { false, ac_token_type_GREATER_EQUAL, STRV(">=") },
    { true,  ac_token_type_HASH, STRV("#") },
    { false, ac_token_type_HORIZONTAL_WHITESPACE, STRV("<horizontal_whitespace>") },
    { false, ac_token_type_IDENTIFIER, STRV("<identifier>") },
    { false, ac_token_type_LESS, STRV("<") },
    { false, ac_token_type_LESS_EQUAL, STRV("<=") },
    { false, ac_token_type_LITERAL_CHAR, STRV("<literal-char>") },
    { false, ac_token_type_LITERAL_FLOAT, STRV("<literal-float>") },
    { true,  ac_token_type_LITERAL_INTEGER, STRV("<literal-integer>") },
    { false, ac_token_type_LITERAL_STRING, STRV("<literal-string>") },
    { true,  ac_token_type_MINUS, STRV("-") },
    { false, ac_token_type_MINUS_EQUAL, STRV("-=") },
    { true, ac_token_type_NEW_LINE, STRV("<new_line>") },
    { false, ac_token_type_NOT_EQUAL, STRV("!=") },
    { true,  ac_token_type_PAREN_L, STRV("(") },
    { true,  ac_token_type_PAREN_R, STRV(")") },
    { false, ac_token_type_PERCENT, STRV("%") },
    { false, ac_token_type_PERCENT_EQUAL, STRV("%=") },
    { false, ac_token_type_PIPE, STRV("|") },
    { false, ac_token_type_PIPE_EQUAL, STRV("|=") },
    { true,  ac_token_type_PLUS, STRV("+") },
    { false, ac_token_type_PLUS_EQUAL, STRV("+=") },
    { false, ac_token_type_QUESTION, STRV("?") },
    { false, ac_token_type_QUOTE, STRV("'") },
    { true,  ac_token_type_SEMI_COLON, STRV(";") },
    { false, ac_token_type_SLASH, STRV("/") },
    { false, ac_token_type_SLASH_EQUAL, STRV("/=") },
    { false, ac_token_type_SQUARE_L, STRV("[") },
    { false, ac_token_type_SQUARE_R, STRV("]") },
    { false, ac_token_type_STAR, STRV("*") },
    { false, ac_token_type_STAR_EQUAL, STRV("*=") },
    { false, ac_token_type_TILDE, STRV("~") },
    { false, ac_token_type_TILDE_EQUAL, STRV("~=") },
    { false, ac_token_type_TRIPLE_DOT, STRV("...") }

    /* Other known identifiers like the preprocessor directives . @TODO */
};

const char* ac_token_type_to_str(enum ac_token_type type) {
    return ac_token_type_to_strv(type).data;
}

strv ac_token_type_to_strv(enum ac_token_type type)
{
    return token_infos[type].name;
}

const char* ac_token_to_str(ac_token token) {

    return ac_token_to_strv(token).data;
}

strv ac_token_to_strv(ac_token token) {

    if (token.type == ac_token_type_IDENTIFIER)
    {
        return (strv){ token.text.size, token.text.data };
    }

    return ac_token_type_to_strv(token.type);
}

bool ac_token_type_is_keyword(enum ac_token_type t) {

    return t >= ac_token_type_ALIGNAS
        && t <= ac_token_type_WHILE;
}

static size_t token_str_len(enum ac_token_type type) {
    AC_ASSERT(!token_type_is_literal(type));

    return token_infos[type].name.size;
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

static void location_increment_row(ac_location* l) {
    l->row++;
    l->col = 1;
    l->pos++;
}

static void location_increment_column(ac_location* l) {
    l->col++;
    l->pos++;
}