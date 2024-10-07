#include "preprocessor.h"

static strv directive_define = STRV("define");

static const ac_token* goto_next_token(ac_pp* pp);

static const ac_token* parse_directive(ac_pp* pp);

static ac_token token(ac_pp* pp); /* Current token by value. */
static const ac_token* token_ptr(ac_pp* pp); /* Current token by pointer. */
static bool expect(ac_pp* pp, enum ac_token_type type);
static bool expect_and_consume(ac_pp* pp, enum ac_token_type type);
static bool consume_if(ac_pp* pp, bool value);

void ac_pp_init(ac_pp* pp, ac_manager* mgr, strv content, const char* filepath)
{
    memset(pp, 0, sizeof(ac_pp));
    pp->mgr = mgr;

    ac_lex_init(&pp->lex, mgr, content, filepath);
}

void ac_pp_destroy(ac_pp* pp)
{
    ac_lex_destroy(&pp->lex);
}

const ac_token* ac_pp_goto_next(ac_pp* pp)
{
    /* Get raw token from the lexer until we get a none whitespace token. */
    const ac_token* token = ac_lex_goto_next(&pp->lex);

    /* Starts with '#', handle the preprocessor directive. */
    if (token->type == ac_token_type_HASH)
    {
        while (token->type == ac_token_type_HASH)
        {
            token = parse_directive(pp);
            if (!token)
                return NULL;
        }
    }

    return token;
}

static const ac_token* parse_directive(ac_pp* pp)
{
    AC_ASSERT(token(pp).type == ac_token_type_HASH);

    const ac_token* token = goto_next_token(pp); /* Skip '#' */

    if (token->type == ac_token_type_IDENTIFIER)
    {
        /* #define */
        if (consume_if(pp, strv_equals(token->text, directive_define)))
        {
            expect_and_consume(pp, ac_token_type_IDENTIFIER);

            /* @TODO register the macro here. */

            /* Macro could end with a new line or with EOF, we skip the new line if needed. */
            if (token->type == ac_token_type_NEW_LINE)
            {
                return ac_lex_goto_next(&pp->lex); /* Only skip single new line. */
            }
        }
        else
        {
            ac_report_error_loc(pp->lex.location, "Unknown directive.");
            return NULL;
        }
    }

    /* If we reached a new line of EOF the current directive is a null directive. */
    AC_ASSERT(token->type == ac_token_type_NEW_LINE || token->type == ac_token_type_EOF);

    return token_ptr(pp);

}

static const ac_token* goto_next_token(ac_pp* pp)
{
    const ac_token* token = ac_lex_goto_next(&pp->lex);
    const ac_token* previous_token = token;

    while (token->type == ac_token_type_HORIZONTAL_WHITESPACE
        || token->type == ac_token_type_COMMENT)
    {
        token = ac_lex_goto_next(&pp->lex);
    }

    return token;
}

static ac_token token(ac_pp* pp)
{
    return ac_lex_token(&pp->lex);
}

static const ac_token* token_ptr(ac_pp* pp)
{
    return ac_lex_token_ptr(&pp->lex);
}

static bool expect(ac_pp* pp, enum ac_token_type type)
{
    return ac_lex_expect(&pp->lex, type);
}

static bool expect_and_consume(ac_pp* pp, enum ac_token_type type)
{
    AC_ASSERT(type != ac_token_type_EOF);

    if (!expect(pp, type))
    {
        return false;
    }

    goto_next_token(pp);
    return true;
}

static bool consume_if(ac_pp* pp, bool value)
{
    if (value)
    {
        goto_next_token(pp);
    }
    return value;
}