#ifndef AC_LEXER_H
#define AC_LEXER_H

#include "manager.h"
#include "location.h"

#ifdef __cplusplus
extern "C" {
#endif

enum ac_token_type {

    ac_token_type_NONE,
    ac_token_type_AMP,             // &
    ac_token_type_AMP_EQUAL,       // &=
    ac_token_type_ARROW,           // ->
    ac_token_type_BACKSLASH,       // \ //
    ac_token_type_BRACE_L,         // {
    ac_token_type_BRACE_R,         // }
    ac_token_type_CARET,           // ^
    ac_token_type_CARET_EQUAL,     // ^=
    ac_token_type_COLON,           // :
    ac_token_type_COMMA,           // ,
    ac_token_type_DOLLAR,          // $
    ac_token_type_DOT,             // .
    ac_token_type_DOUBLE_AMP,      // &&
    ac_token_type_DOUBLE_DOT,      // ..
    ac_token_type_DOUBLE_EQUAL,    // ==
    ac_token_type_DOUBLE_GREATER,  // >>
    ac_token_type_DOUBLE_LESS,     // <<
    ac_token_type_DOUBLE_PIPE,     // ||
    ac_token_type_DOUBLE_QUOTE,    // "
    ac_token_type_ELSE,
    ac_token_type_ENUM,
    ac_token_type_EOF,
    ac_token_type_EQUAL,           // =
    ac_token_type_ERROR,
    ac_token_type_EXCLAM,          // !
    ac_token_type_FOR,
    ac_token_type_GREATER,         // >
    ac_token_type_GREATER_EQUAL,   // >=
    ac_token_type_HASH,            // #
    ac_token_type_IDENTIFIER,
    ac_token_type_IF,
    ac_token_type_LESS,            // <
    ac_token_type_LESS_EQUAL,      // <=
    ac_token_type_LITERAL_BOOL,
    ac_token_type_LITERAL_CHAR,
    ac_token_type_LITERAL_FLOAT,
    ac_token_type_LITERAL_INTEGER,
    ac_token_type_LITERAL_STRING,
    ac_token_type_MINUS,           // -
    ac_token_type_MINUS_EQUAL,     // -=
    ac_token_type_NOT_EQUAL,       // !=
    ac_token_type_PAREN_L,         // (
    ac_token_type_PAREN_R,         // )
    ac_token_type_PERCENT,         // %
    ac_token_type_PERCENT_EQUAL,   // %=
    ac_token_type_PIPE,            // |
    ac_token_type_PIPE_EQUAL,      // |=
    ac_token_type_PLUS,            // +
    ac_token_type_PLUS_EQUAL,      // +=
    ac_token_type_QUESTION,        // ?
    ac_token_type_QUOTE,           // '
    ac_token_type_RETURN,
    ac_token_type_SEMI_COLON,      // ;
    ac_token_type_SIZEOF,
    ac_token_type_SLASH,           // /
    ac_token_type_SLASH_EQUAL,     // /=
    ac_token_type_SQUARE_L,        // [
    ac_token_type_SQUARE_R,        // ]
    ac_token_type_STAR,            // *
    ac_token_type_STAR_EQUAL,      // *=
    ac_token_type_STRUCT,
    ac_token_type_TILDE,           // ~
    ac_token_type_TILDE_EQUAL,     // ~=
    ac_token_type_TRIPLE_DOT,      // ...
    ac_token_type_TYPEOF,
    ac_token_type_WHILE,
    ac_token_type_COUNT
};

struct ac_token_float {
    double value;
    bool overflow : 1;
    bool is_double : 1;
};

struct ac_token_int {
    int value;
    bool overflow : 1;
};

struct ac_token_bool {
    bool value;
};

struct ac_token {
    enum ac_token_type type;
    strv text;
    union {
        struct ac_token_float f;
        struct ac_token_int i;
        struct ac_token_bool b;
    } u;
};

struct ac_lex_options {
    bool enable_hex_float;
};

struct ac_lex {
    struct ac_manager* mgr;
    struct ac_lex_options options;

    const char* filepath;
    const char* src;
    const char* end;
    const char* cur;

    int len;

    struct ac_token token; /* current token */
    struct ac_location location; /* current location */
};

void ac_lex_init(struct ac_lex* l, struct ac_manager* mgr);
void ac_lex_destroy(struct ac_lex* l);

void ac_lex_set_source(struct ac_lex* l, const char* source_content, size_t source_len, const char* filepath);
const struct ac_token* ac_lex_goto_next(struct ac_lex* l);

const char* ac_token_type_to_str(enum ac_token_type type); /* this should be used only for printf */
strv ac_token_type_to_strv(enum ac_token_type type);
const char* ac_token_to_str(const struct ac_token t);
strv ac_token_to_strv(const struct ac_token t);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* AC_LEXER_H */