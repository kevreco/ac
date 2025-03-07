#ifndef AC_LEXER_H
#define AC_LEXER_H

#include "manager.h"
#include "location.h"

#ifdef __cplusplus
extern "C" {
#endif
typedef enum ac_token_type ac_token_type;
enum ac_token_type {
    ac_token_type_NONE,
    ac_token_type_EMPTY, /* Special token which mean "no token". */

    /* Keywords */

    ac_token_type_ALIGNAS,
    ac_token_type_ALIGNAS2,
    ac_token_type_ALIGNOF,
    ac_token_type_ALIGNOF2,
    ac_token_type_AT,
    ac_token_type_ATOMIC,
    ac_token_type_AUTO,
    ac_token_type_BITINT,
    ac_token_type_BOOL,
    ac_token_type_BOOL2,
    ac_token_type_BREAK,
    ac_token_type_CASE,
    ac_token_type_CHAR,
    ac_token_type_CONST,
    ac_token_type_CONSTEXPR,
    ac_token_type_CONTINUE,
    ac_token_type_COMPLEX,
    ac_token_type_DECIMAL128,
    ac_token_type_DECIMAL32,
    ac_token_type_DECIMAL64,
    ac_token_type_DEFAULT,
    ac_token_type_DO,
    ac_token_type_DOUBLE,
    ac_token_type_ELSE,
    ac_token_type_ENUM,
    ac_token_type_EXTERN,
    ac_token_type_FALSE,
    ac_token_type_FLOAT,
    ac_token_type_FOR,
    ac_token_type_GENERIC,
    ac_token_type_GOTO,
    ac_token_type_IF,
    ac_token_type_INLINE,
    ac_token_type_INT,
    ac_token_type_IMAGINARY,
    ac_token_type_LONG,
    ac_token_type_NORETURN,
    ac_token_type_NULLPTR,
    ac_token_type_REGISTER,
    ac_token_type_RESTRICT,
    ac_token_type_RETURN,
    ac_token_type_SHORT,
    ac_token_type_SIGNED,
    ac_token_type_SIZEOF,
    ac_token_type_STATIC,
    ac_token_type_STATIC_ASSERT,
    ac_token_type_STATIC_ASSERT2,
    ac_token_type_STRUCT,
    ac_token_type_SWITCH,
    ac_token_type_THREAD_LOCAL,
    ac_token_type_THREAD_LOCAL2,
    ac_token_type_TRUE,
    ac_token_type_TYPEDEF,
    ac_token_type_TYPEOF,
    ac_token_type_TYPEOF_UNQUAL,
    ac_token_type_UNION,
    ac_token_type_UNSIGNED,
    ac_token_type_VOID,
    ac_token_type_VOLATILE,
    ac_token_type_WHILE,

    /* Preprocessor keywords */
   
    ac_token_type_DEFINE,
    ac_token_type_DEFINED,
    ac_token_type_ELIF,
    ac_token_type_ELIFDEF,  /* C23 */
    ac_token_type_ELIFNDEF, /* C23 */
    ac_token_type_ENDIF,
    ac_token_type_ERROR,
    ac_token_type_EMBED,
    ac_token_type_IFDEF,
    ac_token_type_IFNDEF,
    ac_token_type_INCLUDE,
    ac_token_type_PRAGMA,
    ac_token_type_LINE,
    ac_token_type_UNDEF,
    ac_token_type_WARNING,  /* C23 */

    /* Special macros */

    ac_token_type__FILE__,            /* __FILE__  */
    ac_token_type__LINE__,            /* __LINE__  */
    ac_token_type__DATE__,            /* __DATE__  */
    ac_token_type__TIME__,            /* __TIME__  */
    ac_token_type__COUNTER__,         /* __COUNTER__ */
    ac_token_type__FUNC__,            /* __func__  */
    ac_token_type__FUNCTION__,        /* __FUNCTION__ */
    ac_token_type__PRETTY_FUNCTION__, /* __PRETTY_FUNCTION__ */

    /* Symbols */

    ac_token_type_AMP,             /* &  */ 
    ac_token_type_AMP_EQUAL,       /* &= */
    ac_token_type_ARROW,           /* -> */
    ac_token_type_BACKSLASH,       /* \  */
    ac_token_type_BRACE_L,         /* {  */
    ac_token_type_BRACE_R,         /* }  */
    ac_token_type_CARET,           /* ^  */
    ac_token_type_CARET_EQUAL,     /* ^= */
    ac_token_type_COLON,           /* :  */
    ac_token_type_COMMA,           /* ,  */
    ac_token_type_COMMENT,
    ac_token_type_DOLLAR,          /* $  */
    ac_token_type_DOT,             /* .  */
    ac_token_type_DOUBLE_AMP,      /* && */
    ac_token_type_DOUBLE_DOT,      /* .. */
    ac_token_type_DOUBLE_EQUAL,    /* == */
    ac_token_type_DOUBLE_GREATER,  /* >> */
    ac_token_type_DOUBLE_HASH,     /* ## */
    ac_token_type_DOUBLE_LESS,     /* << */
    ac_token_type_DOUBLE_MINUS,    /* -- */
    ac_token_type_DOUBLE_PIPE,     /* || */
    ac_token_type_DOUBLE_PLUS,     /* ++ */
    ac_token_type_DOUBLE_QUOTE,    /* "  */
    ac_token_type_EOF,             /* End of file. */
    ac_token_type_EQUAL,           /* =  */
    ac_token_type_EXCLAM,          /* !  */
    ac_token_type_GREATER,         /* >  */
    ac_token_type_GREATER_EQUAL,   /* >= */
    ac_token_type_HASH,            /* #  */
    ac_token_type_HORIZONTAL_WHITESPACE, /* All comon whitespaces but the new line and carriage return */
    ac_token_type_IDENTIFIER,
    ac_token_type_LESS,            /* <  */
    ac_token_type_LESS_EQUAL,      /* <= */
    ac_token_type_LITERAL_CHAR,
    ac_token_type_LITERAL_FLOAT,
    ac_token_type_LITERAL_INTEGER,
    ac_token_type_LITERAL_STRING,
    ac_token_type_MINUS,           /* -  */
    ac_token_type_MINUS_EQUAL,     /* -= */
    ac_token_type_NEW_LINE,        /* \n or \r or \r\n */
    ac_token_type_NOT_EQUAL,       /* != */
    ac_token_type_PAREN_L,         /* (  */
    ac_token_type_PAREN_R,         /* )  */
    ac_token_type_PERCENT,         /* %  */
    ac_token_type_PERCENT_EQUAL,   /* %= */
    ac_token_type_PIPE,            /* |  */
    ac_token_type_PIPE_EQUAL,      /* |= */
    ac_token_type_PLUS,            /* +  */
    ac_token_type_PLUS_EQUAL,      /* += */
    ac_token_type_QUESTION,        /* ?  */
    ac_token_type_QUOTE,           /* '  */
    ac_token_type_SEMI_COLON,      /* ;  */
    ac_token_type_SLASH,           /* /  */
    ac_token_type_SLASH_EQUAL,     /* /= */
    ac_token_type_SQUARE_L,        /* [  */
    ac_token_type_SQUARE_R,        /* ]  */
    ac_token_type_STAR,            /* *  */
    ac_token_type_STAR_EQUAL,      /* *= */
    ac_token_type_TILDE,           /* ~   */
    ac_token_type_TILDE_EQUAL,     /* ~=  */
    ac_token_type_TRIPLE_DOT,      /* ... */

    ac_token_type_COUNT
};

typedef struct ac_token_number ac_token_number;
struct ac_token_number {
    bool overflow : 1;
    bool is_float : 1;
    bool is_double : 1;
    bool is_unsigned : 1;
    int long_depth : 2;
    union {
        int64_t int_value;
        double float_value;
    } u;
};

typedef struct ac_ident ac_ident;
struct ac_ident {
    strv text;
    bool cannot_expand;
};

typedef struct ac_token ac_token;
struct ac_token {
    enum ac_token_type type;
    union {
        /* Keywords or identifiers. */
        ac_ident* ident; 
        /* Literals, comments, or tokens known at compile time like "<<".
           It contains the verbatim content.
           It's necessary to keep a reference of this for the preprocessor part.
           Example: "\n" count for two characters. */
        strv text;       
    };
    union {
        ac_token_number number;
        struct {
            bool is_utf8;    /* @TODO @OPT place this in a flag. */
            bool is_utf16;   /* @TODO @OPT place this in a flag. */
            bool is_utf32;   /* @TODO @OPT place this in a flag. */
            bool is_wide;    /* @TODO @OPT place this in a flag. */
        } str;
        struct {
            int64_t value;   /* @TODO @OPT place this in a flag. */
            bool is_utf8;    /* @TODO @OPT place this in a flag. */
            bool is_utf16;   /* @TODO @OPT place this in a flag. */
            bool is_utf32;   /* @TODO @OPT place this in a flag. */
            bool is_wide;    /* @TODO @OPT place this in a flag. */
        } ch;
    } u;

    bool previous_was_space; /* @TODO @OPT place this in a flag. */
    /* Only matters for directives beginning with '#'. */
    bool beginning_of_line;  /* @TODO @OPT place this in a flag. */
    /* Macro identifiers must be marked as "non expandable" to avoid recursive expansion. */
    bool cannot_expand;      /* @TODO @OPT place this in a flag. */
    /* EOF token is also used on error, premature_eof is true when there was indeed an error. */
    bool is_premature_eof;   /* @TODO @OPT place this in a flag. */
};

typedef struct ac_token_info ac_token_info;
struct ac_token_info {
    bool is_supported;
    enum ac_token_type type;
    ac_ident ident;
};

/* @TODO move this to the compiler options. */
typedef struct ac_lex_options ac_lex_options;
struct ac_lex_options {
    bool reject_hex_float;
};

typedef struct ac_lex ac_lex;
struct ac_lex {
    ac_manager* mgr;
    ac_lex_options options;

    strv filepath;
    const char* src;
    const char* end;
    const char* cur;

    int len;

    ac_token token;       /* Current token */
    ac_location leading_location; /* Location when at the very begining of ac_lex_goto_next. */
    ac_location location; /* Current location */
    dstr tok_buf;         /* Token buffer in case we can't just use a string view to the memory. */
    dstr str_buf;         /* Buffer for string conversion. */
    /* If the current character is an end of line,
       it's assumes to be part of the current line
       hence we need to adjust the current line number on
       the following character. */
    int end_of_line_adjustment_counter;
    bool beginning_of_line;
};

void ac_lex_init(ac_lex* l, ac_manager* mgr);
void ac_lex_destroy(ac_lex* l);

void ac_lex_set_content(ac_lex* l, strv content, strv filepath);
/* Got to signficant next token. */
ac_token* ac_lex_goto_next(ac_lex* l);

/* Returns current token. */
ac_token ac_lex_token(ac_lex* l);

/* Returns current token pointer. */
ac_token* ac_lex_token_ptr(ac_lex* l);

/* Report error if the current token is not of the type specified. */
bool ac_lex_expect(ac_lex* l, enum ac_token_type type);

void ac_lex_swap(ac_lex* left, ac_lex* right);

/* We need to save and restore state (ac_lex_state) of the lexer to handle stack of include files.
   The reason we don't use stack of ac_lex directly is because we don't need to allocated new temporary buffers.
   @TODO maybe buffers can be part of the ac_manager, it would actually make sense. */
typedef struct ac_lex_state ac_lex_state;
struct ac_lex_state {
    strv filepath;
    const char* src;
    const char* end;
    const char* cur;

    int len;

    ac_token token;
    ac_location leading_location;
    ac_location location;
    bool beginning_of_line;
};

ac_lex_state ac_lex_save(ac_lex* l);
void ac_lex_restore(ac_lex* l, ac_lex_state* s);

/* Skip block of text between #if and #endif. */
bool ac_skip_preprocessor_block(ac_lex* l, bool was_end_of_line);

/* Parse every characters until the first '>' */
ac_token* ac_parse_include_path(ac_lex* l);

ac_token* ac_token_eof();

const char* ac_token_type_to_str(enum ac_token_type type); /* this should be used only for printf */
strv ac_token_type_to_strv(enum ac_token_type type);
const char* ac_token_to_str(ac_token t);
strv ac_token_to_strv(ac_token t);
void ac_token_fprint(FILE* file, ac_token t); /* Print to file. */
void ac_token_sprint(dstr* str, ac_token t);  /* Print to dynamic string. */

ac_token_info* ac_token_infos();

bool ac_token_is_keyword_or_identifier(enum ac_token_type type);
strv ac_token_prefix(ac_token t);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* AC_LEXER_H */