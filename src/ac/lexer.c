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

static const ac_token* parse_ascii_char_literal(ac_lex* l);
static const ac_token* parse_number(ac_lex* l);

static const ac_token* token_from_text(ac_lex* l, enum ac_token_type type, strv text); /* set current token and got to next */
static const ac_token* token_error(ac_lex* l); /* set current token to error and returns it. */
static const ac_token* token_eof(ac_lex* l);   /* set current token to eof and returns it. */
static const ac_token* token_from_type_and_consume(ac_lex* l, enum ac_token_type type); /* set current token and got to next */
static size_t token_str_len(enum ac_token_type type);
/* Hack to use 'is_keyword' on some string pointers to static keywords. */
static bool try_parse_keyword(strv* text, enum ac_token_type* type);

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

        l->options.enable_hex_float = true;

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
}

void ac_lex_destroy(ac_lex* l)
{
    memset(l, 0, sizeof(ac_lex));
}

const ac_token* ac_lex_goto_next(ac_lex* l)
{
    if (is_eof(l)) {
        return token_eof(l);
    }

    switch (l->cur[0]) {

    case ' ':
    case '\t':
    case '\f':
    case '\v': {
        /* Parse group of whitespace. */
        const char* start = l->cur;
        consume_one(l);
        while (is_whitespace(l)) {
            consume_one(l);
        }
        return token_from_text(l, ac_token_type_HORIZONTAL_WHITESPACE, strv_make_from(start, l->cur - start));
    }

    case '\n':
    case '\r':
    {
        /* Parse single line ending. */
        const char* start = l->cur;
        consume_one(l);
        return token_from_text(l, ac_token_type_NEW_LINE, strv_make_from(start, l->cur - start));
    }
    case '#': return token_from_type_and_consume(l, ac_token_type_HASH);
    case '[': return token_from_type_and_consume(l, ac_token_type_SQUARE_L);
    case ']': return token_from_type_and_consume(l, ac_token_type_SQUARE_R);
    case '(': return token_from_type_and_consume(l, ac_token_type_PAREN_L);
    case ')': return token_from_type_and_consume(l, ac_token_type_PAREN_R);
    case '{': return token_from_type_and_consume(l, ac_token_type_BRACE_L);
    case '}': return token_from_type_and_consume(l, ac_token_type_BRACE_R);
    case ';': return token_from_type_and_consume(l, ac_token_type_SEMI_COLON);
    case ',': return token_from_type_and_consume(l, ac_token_type_COMMA);
    case '?': return token_from_type_and_consume(l, ac_token_type_QUESTION);

    case '=': {
        if (next_is(l, '=')) {
            return token_from_type_and_consume(l, ac_token_type_DOUBLE_EQUAL);
        }
        return token_from_type_and_consume(l, ac_token_type_EQUAL);
    }

    case '!': {

        if (next_is(l, '=')) {
            return token_from_type_and_consume(l, ac_token_type_NOT_EQUAL);
        }
        return token_from_type_and_consume(l, ac_token_type_EXCLAM);
    }

    case '<': {
        if (next_is(l, '<')) {
            return token_from_type_and_consume(l, ac_token_type_DOUBLE_LESS);
        }
        else if (next_is(l, '=')) {
            return token_from_type_and_consume(l,  ac_token_type_LESS_EQUAL);
        }
        return token_from_type_and_consume(l,  ac_token_type_LESS);
    }

    case '>': {

        if (next_is(l, '>')) {
            return token_from_type_and_consume(l, ac_token_type_DOUBLE_GREATER);
        }
        else if (next_is(l, '=')) {
            return token_from_type_and_consume(l, ac_token_type_GREATER_EQUAL);
        }

        return token_from_type_and_consume(l, ac_token_type_GREATER);
    }

    case '&': {
        if (next_is(l, '&')) {
            return token_from_type_and_consume(l, ac_token_type_DOUBLE_AMP);
        }
        else if (next_is(l, '=')) {
            return token_from_type_and_consume(l, ac_token_type_AMP_EQUAL);
        }
        return token_from_type_and_consume(l, ac_token_type_AMP);
    }

    case '|': {
        if (next_is(l, '|')) {
            return token_from_type_and_consume(l, ac_token_type_DOUBLE_PIPE);
        }
        else if (next_is(l, '=')) {
            return token_from_type_and_consume(l, ac_token_type_PIPE_EQUAL);
        }
        return token_from_type_and_consume(l, ac_token_type_PIPE);
    }

    case '+': {
        if (next_is(l, '=')) {
            return token_from_type_and_consume(l, ac_token_type_PLUS_EQUAL);
        }
        return token_from_type_and_consume(l, ac_token_type_PLUS);
    }
    case '-': {
        if (next_is(l, '=')) {
            return token_from_type_and_consume(l, ac_token_type_MINUS_EQUAL);
        }
        else if (next_is(l, '>')) {
            return token_from_type_and_consume(l, ac_token_type_ARROW);
        }
        return token_from_type_and_consume(l, ac_token_type_MINUS);
    }

    case '*': {
        if (next_is(l, '=')) {
            return token_from_type_and_consume(l, ac_token_type_STAR_EQUAL);
        }
        return token_from_type_and_consume(l, ac_token_type_STAR);
    }

    case '/': {
        if (next_is(l, '=')) {
            return token_from_type_and_consume(l, ac_token_type_SLASH_EQUAL);
        }
        else if (next_is(l, '/')) {  /* Parse inline comment. */
            const char* start = l->cur;
            skipn(l, 2); /* Skip '//' */
            
            while (!is_eof(l) && !is_end_line(l)) {
                consume_one(l);
            }
            return token_from_text(l, ac_token_type_COMMENT, strv_make_from(start, l->cur - start));
        }
        else if (next_is(l, '*')) {  /* Parse C comment. */
            const char* start = l->cur;
            skipn(l, 2); /* Skip opening comment tag. */
            
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

        return token_from_type_and_consume(l, ac_token_type_SLASH);
    }

    case '%': {
        if (next_is(l, '=')) {
            return token_from_type_and_consume(l, ac_token_type_PERCENT_EQUAL);
        }
        return token_from_type_and_consume(l, ac_token_type_PERCENT);
    }

    case '^': {
        if (next_is(l, '=')) {
            return token_from_type_and_consume(l, ac_token_type_CARET_EQUAL);
        }

        return token_from_type_and_consume(l, ac_token_type_CARET);
    }

    case '.': {
        if (next_is(l, '.')) {
            if (next_next_is(l, '.')) {
                return token_from_type_and_consume(l, ac_token_type_TRIPLE_DOT);
            }
            return token_from_type_and_consume(l, ac_token_type_DOUBLE_DOT);
        }
        return token_from_type_and_consume(l, ac_token_type_DOT);
    }

    case ':' : return token_from_type_and_consume(l, ac_token_type_COLON);
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

            enum ac_token_type type;
            strv text = strv_make_from(start, n);

            if (strv_equals_str(text, "true"))
            {
                l->token.type = ac_token_type_LITERAL_BOOL;
                l->token.u.b.value = true;
                return &l->token;
            }
            else if (strv_equals_str(text, "false"))
            {
                l->token.type = ac_token_type_LITERAL_BOOL;
                l->token.u.b.value = false;
                return &l->token;
            }
            else if (try_parse_keyword(&text, &type)) /* if keyword is parsed token type is also retrieved */
            {
                /* Do nothing, the type as been retrieved already. */
            }
            else
            {
                type = ac_token_type_IDENTIFIER;
            }

            return token_from_text(l, type, text);
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

static const ac_token* token_from_type_and_consume(ac_lex* l, enum ac_token_type type) {
    size_t size = token_str_len(type);
    const ac_token* t = token_from_text(l, type, strv_make_from(l->cur, size));
    skipn(l, size);
    return t;
}

/*
-------------------------------------------------------------------------------
ac_token
-------------------------------------------------------------------------------
*/

struct keyword_item {
    enum ac_token_type type;
    strv name;
};

#define KEYWORD_ITEM_COUNT (sizeof(keyword_items) / sizeof(keyword_items[0]))

static struct keyword_item keyword_items[] = {

    { ac_token_type_NONE, STRV("<none>") },
    { ac_token_type_AMP,  STRV("&") },
    { ac_token_type_AMP_EQUAL, STRV("&=") },
    { ac_token_type_ARROW, STRV("->") },
    { ac_token_type_BACKSLASH, STRV("\\") },
    { ac_token_type_BRACE_L, STRV("{") },
    { ac_token_type_BRACE_R, STRV("}") },
    { ac_token_type_CARET , STRV("^") },
    { ac_token_type_CARET_EQUAL, STRV("^=") },
    { ac_token_type_COLON, STRV(":") },
    { ac_token_type_COMMA, STRV(",") },
    { ac_token_type_COMMENT, STRV("<comment>") },
    { ac_token_type_DOLLAR, STRV("$") },
    { ac_token_type_DOT, STRV(".") },
    { ac_token_type_DOUBLE_AMP, STRV("&&") },
    { ac_token_type_DOUBLE_DOT, STRV("..") },
    { ac_token_type_DOUBLE_EQUAL, STRV("==") },
    { ac_token_type_DOUBLE_GREATER, STRV(">>") },
    { ac_token_type_DOUBLE_LESS, STRV("<<") },
    { ac_token_type_DOUBLE_PIPE, STRV("||") },
    { ac_token_type_DOUBLE_QUOTE , STRV("\"") },
    { ac_token_type_ELSE, STRV("else") },
    { ac_token_type_ENUM, STRV("enum") },
    { ac_token_type_EOF, STRV("end-of-line") },
    { ac_token_type_EQUAL, STRV("=") },
    { ac_token_type_ERROR, STRV("<error>") },
    { ac_token_type_EXCLAM, STRV("!") },
    { ac_token_type_FOR, STRV("for") },
    { ac_token_type_GREATER, STRV(">") },
    { ac_token_type_GREATER_EQUAL, STRV(">=") },
    { ac_token_type_HASH, STRV("#") },
    { ac_token_type_HORIZONTAL_WHITESPACE, STRV("<horizontal_whitespace>") },
    { ac_token_type_IDENTIFIER, STRV("<identifier>") },
    { ac_token_type_IF, STRV("if") },
    { ac_token_type_LESS, STRV("<") },
    { ac_token_type_LESS_EQUAL, STRV("<=") },
    { ac_token_type_LITERAL_BOOL, STRV("<literal-bool>") },
    { ac_token_type_LITERAL_CHAR, STRV("<literal-char>") },
    { ac_token_type_LITERAL_FLOAT, STRV("<literal-float>") },
    { ac_token_type_LITERAL_INTEGER, STRV("<literal-integer>") },
    { ac_token_type_LITERAL_STRING, STRV("<literal-string>") },
    { ac_token_type_MINUS, STRV("-") },
    { ac_token_type_MINUS_EQUAL, STRV("-=") },
    { ac_token_type_NEW_LINE, STRV("<new_line>") },
    { ac_token_type_NOT_EQUAL, STRV("!=") },
    { ac_token_type_PAREN_L, STRV("(") },
    { ac_token_type_PAREN_R, STRV(")") },
    { ac_token_type_PERCENT, STRV("%") },
    { ac_token_type_PERCENT_EQUAL, STRV("%=") },
    { ac_token_type_PIPE, STRV("|") },
    { ac_token_type_PIPE_EQUAL, STRV("|=") },
    { ac_token_type_PLUS, STRV("+") },
    { ac_token_type_PLUS_EQUAL, STRV("+=") },
    { ac_token_type_QUESTION, STRV("?") },
    { ac_token_type_QUOTE, STRV("'") },
    { ac_token_type_RETURN, STRV("return") },
    { ac_token_type_SEMI_COLON, STRV(";") },
    { ac_token_type_SIZEOF, STRV("sizeof") },
    { ac_token_type_SLASH, STRV("/") },
    { ac_token_type_SLASH_EQUAL, STRV("/=") },
    { ac_token_type_SQUARE_L, STRV("[") },
    { ac_token_type_SQUARE_R, STRV("]") },
    { ac_token_type_STAR, STRV("*") },
    { ac_token_type_STAR_EQUAL, STRV("*=") },
    { ac_token_type_STRUCT, STRV("struct") },
    { ac_token_type_TILDE, STRV("~") },
    { ac_token_type_TILDE_EQUAL, STRV("~=") },
    { ac_token_type_TRIPLE_DOT, STRV("...") },
    { ac_token_type_TYPEOF, STRV("typeof") },
    { ac_token_type_WHILE, STRV("while") }
};

const char* ac_token_type_to_str(enum ac_token_type type) {
    return keyword_items[type].name.data;
}

strv ac_token_type_to_strv(enum ac_token_type type) {

    return keyword_items[type].name;
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

bool ac_token_is_keyword(ac_token t) {
    return t.text.data >= keyword_items[0].name.data
        && t.text.data <= keyword_items[ac_token_type_COUNT-1].name.data;
}

static size_t token_str_len(enum ac_token_type type) {
    assert(!token_type_is_literal(type));

    return keyword_items[type].name.size;
}

static bool try_parse_keyword(strv* str, enum ac_token_type* type) {

    if (ac_token_type_COUNT > KEYWORD_ITEM_COUNT)
    {
        ac_report_error("There are more items in keyword_items (%d) than ac_token_type_COUNT (%d). It should be the same.", KEYWORD_ITEM_COUNT, ac_token_type_COUNT);
        exit(1);
    }

    if (ac_token_type_COUNT < KEYWORD_ITEM_COUNT)
    {
        ac_report_error("There are more items in  ac_token_type_COUNT (%d) than keyword_items (%d). It should be the same.", ac_token_type_COUNT, KEYWORD_ITEM_COUNT);
        exit(1);
    }

    const struct keyword_item* keyword_cursor = &keyword_items[0];
    const struct keyword_item* keyword_end = &keyword_items[ac_token_type_COUNT];

    while (keyword_cursor < keyword_end)
    {
        if (strv_equals(keyword_cursor->name, *str)) {
            *str = keyword_cursor->name;
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