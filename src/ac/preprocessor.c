#include "preprocessor.h"

typedef struct range range;
struct range {
    size_t start;
    size_t end;
};

typedef darrT(range) darr_range;

size_t range_size(range r) { return r.end - r.start; }

typedef struct ac_macro ac_macro;
struct ac_macro {
    ac_token identifier;        /* Name of the macro */
    /* Example of function-like macro:
         #define X(x, y) (x + y)'
       Example of object-like macro:
         #define Y (1 + 2)*/
    bool is_function_like;

    darr_token definition; /* Contains tokens from parameters and body. */
    range params;               /* If function-like macro, range of tokens from definition representing the parameters. Parsed at directive-time. */
    range body;                 /* Range of token from definition representing the body. Parsed at directive-time.*/

    ac_location location;
};

static void ac_macro_init(ac_macro* m)
{
    memset(m, 0, sizeof(ac_macro));

    darrT_init(&m->definition);
}

static void ac_macro_destroy(ac_macro* m)
{
    darrT_destroy(&m->definition);
}

static strv directive_define = STRV("define");
static strv directive_undef = STRV("undef");

/* Get token from the stack or from the lexer. */
static ac_token* goto_next_raw_token(ac_pp* pp);
/* Get next raw token and resolve directives. */
static ac_token* goto_next_normal_token(ac_pp* pp);
/* Get next normal token ignoring whitespaces (but not new lines) */
static ac_token* goto_next_token_from_directive(ac_pp* pp);
/* Get preprocessed token ignoring all whitespaces. */
static ac_token* goto_next_token_from_macro_agrument(ac_pp* pp);
/* Get next raw token ignoring whitespaces. */
static ac_token* goto_next_token_from_macro_body(ac_pp* pp);

static bool parse_directive(ac_pp* pp);
static bool parse_macro_definition(ac_pp* pp);
static bool parse_macro_parameters(ac_pp* pp, ac_macro* m);
static bool parse_macro_body(ac_pp* pp, ac_macro* m);

static void macro_push(ac_pp* pp, ac_macro* m);

static ac_token* process_cmd(ac_pp* pp, ac_token_cmd* cmd);
/* Get and remove the first expanded token. */
static ac_token* stack_pop(ac_pp* pp);
static void push_cmd(ac_pp* pp, ac_token_cmd list);

/* Try to expand the token. Return true if it was expanded, false otherwise. */
static bool try_expand(ac_pp* pp, ac_token* token);
static size_t find_parameter_index(ac_token* token, ac_macro* m);

/* Concatenate two tokens and add them to the expanded_token array of the macro. */
static void concat(ac_pp* pp, darr_token* arr, ac_macro* m, ac_token left, ac_token right);
/* Stringize the token (used by operator '#') */
static ac_token stringize(ac_pp* pp, ac_token* tokens, size_t count);

/* Add token to the expanded_token array of the macro.
   This also handle the concat operator '##'. */
static void push_back_expanded_token(ac_pp* pp, darr_token* arr, ac_macro* m, ac_token token);

/* Add empty argument to sequence of tokens. */
static void add_empty_arg(darr_token* args, darr_range* ranges);

/* Return true if something has been expanded.
   The expanded tokens are pushed into a stack used to pick the next token. */
static bool expand_macro(ac_pp* pp, ac_token* ident, ac_macro* m);

static ac_macro* create_macro(ac_pp* pp, ac_token* macro_name, ac_location location);

static ac_location location(ac_pp* pp); /* Return location of the current token. */

static ac_token token(ac_pp* pp);        /* Current token by value. */
static ac_token* token_ptr(ac_pp* pp);   /* Current token by pointer. */
static bool expect(ac_pp* pp, enum ac_token_type type);

static ac_token_cmd make_cmd_token_list(ac_token* ptr, size_t count);
static ac_token_cmd make_cmd_macro_pop(ac_macro* m, darr_token tokens);
static ac_token_cmd to_cmd_token_list(darr_token* arr);

/*------------------*/
/* macro hash table */
/*------------------*/

static ht_hash_t macro_hash(ht_ptr_handle* handle);
static ht_bool macros_are_same(ht_ptr_handle* hleft, ht_ptr_handle* hright);
static void add_macro(ac_pp* pp, ac_macro* m);
static void undefine_macro(ac_pp* pp, ac_macro* m);
static ac_macro* find_macro(ac_pp* pp, ac_token* identifer);

void ac_pp_init(ac_pp* pp, ac_manager* mgr, strv content, const char* filepath)
{
    memset(pp, 0, sizeof(ac_pp));
    pp->mgr = mgr;

    ac_lex_init(&pp->lex, mgr);
    ac_lex_set_content(&pp->lex, content, filepath);
    ac_lex_init(&pp->concat_lex, mgr);

    ht_ptr_init(&pp->macros, (ht_hash_function_t)macro_hash, (ht_predicate_t)macros_are_same);

    darrT_init(&pp->cmd_stack);
    darrT_init(&pp->undef_macros);
    darrT_init(&pp->buffer_for_peek);
    dstr_init(&pp->concat_buffer);
}

void ac_pp_destroy(ac_pp* pp)
{
    dstr_destroy(&pp->concat_buffer);
    darrT_destroy(&pp->buffer_for_peek);
    
    ac_lex_destroy(&pp->concat_lex);
    ac_lex_destroy(&pp->lex);

    /* Wnroll the remaining item in the stack. */
    while(stack_pop(pp) != NULL)
    {
        /* Do nothing. */
    }

    darrT_destroy(&pp->cmd_stack);

    /* Destroy all macros before destroying the macro hash table.
       @FIXME: macros should be using an allocator and we should be able to destroy them all at once.
    */
    ht_cursor c;
    ht_cursor_init(&pp->macros, &c);
    while (ht_cursor_next(&c))
    {
        ht_ptr_handle* h = ht_cursor_item(&c);
        ac_macro* m = h->ptr;
        ac_macro_destroy(m);
    }
    ht_ptr_destroy(&pp->macros);
   
    /* Destroy all undef macros. */
    for (int i = 0; i < darrT_size(&pp->undef_macros); i += 1)
    {
        ac_macro* m = darrT_at(&pp->undef_macros, i);
        ac_macro_destroy(m);
    }

    darrT_destroy(&pp->undef_macros);

}

ac_token* ac_pp_goto_next(ac_pp* pp)
{
    goto_next_normal_token(pp);

    /* Every time the token is expanded we need to try to expand again. */
    while (try_expand(pp, token_ptr(pp))) {
        goto_next_raw_token(pp);
    }

    return token_ptr(pp);
}

static ac_token* goto_next_raw_token(ac_pp* pp)
{
    ac_token* token = NULL;
    /* Get token from previously expanded macros if there are any left. */
    ac_token* token_node = stack_pop(pp);

    pp->current_token = token_node
        ? token_node
        : ac_lex_goto_next(&pp->lex);

    return pp->current_token;
}

static ac_token* goto_next_normal_token(ac_pp* pp)
{
    goto_next_raw_token(pp);

    /* macro_depth is used because we don't want to parse directives for tokens '#' coming from macros. */
    while (token_ptr(pp)->type == ac_token_type_HASH && pp->macro_depth == 0)
    {
        if (!parse_directive(pp)
            || token_ptr(pp)->type == ac_token_type_EOF)
        {
            return ac_token_eof(pp);
        }

        goto_next_raw_token(pp);
    }

    if (token_ptr(pp)->type == ac_token_type_EOF)
    {
        return token_ptr(pp);
    }

    return token_ptr(pp);
}

static ac_token* goto_next_token_from_directive(ac_pp* pp)
{
    ac_token* token = goto_next_raw_token(pp);
    ac_token* previous_token = token;

    while (token->type == ac_token_type_HORIZONTAL_WHITESPACE
        || token->type == ac_token_type_COMMENT)
    {
        token = goto_next_raw_token(pp);
        token->previous_was_space = true;
    }

    return token;
}

static ac_token* goto_next_token_from_macro_agrument(ac_pp* pp)
{
    ac_token* token = goto_next_normal_token(pp);
    ac_token* previous_token = token;

    while (token->type == ac_token_type_HORIZONTAL_WHITESPACE
        || token->type == ac_token_type_COMMENT
        || token->type == ac_token_type_NEW_LINE)
    {
        token = goto_next_normal_token(pp);
        token->previous_was_space = true;
    }

    return token;
}

static ac_token* goto_next_token_from_macro_body(ac_pp* pp)
{
    ac_token* token = goto_next_raw_token(pp);
    ac_token* previous_token = token;

    while (token->type == ac_token_type_HORIZONTAL_WHITESPACE
        || token->type == ac_token_type_COMMENT
        || token->type == ac_token_type_NEW_LINE)
    {
        token = goto_next_raw_token(pp);
        token->previous_was_space = true;
    }

    return token;
}

static bool parse_directive(ac_pp* pp)
{
    AC_ASSERT(token(pp).type == ac_token_type_HASH);

    ac_token* tok = goto_next_token_from_directive(pp); /* Skip '#' */

    if (tok->type == ac_token_type_IDENTIFIER)
    {
        /* #define */
        if (strv_equals(tok->ident->text, directive_define))
        {
            goto_next_token_from_directive(pp); /* Skip 'define' */
            if (!parse_macro_definition(pp))
            {
                return false;
            }
        }
        /* #undef */
        else if (strv_equals(tok->ident->text, directive_undef))
        {
            goto_next_token_from_directive(pp); /* Skip 'undef' */
            expect(pp, ac_token_type_IDENTIFIER);
            ac_token identifier = token(pp);
            goto_next_token_from_directive(pp); /* Skip 'identifier' */

            if (token_ptr(pp)->type != ac_token_type_COMMENT
                && token_ptr(pp)->type != ac_token_type_HORIZONTAL_WHITESPACE
                && token_ptr(pp)->type != ac_token_type_NEW_LINE
                && token_ptr(pp)->type != ac_token_type_EOF)
            {
                ac_report_warning("Extra tokens at end of '#undef' directive");
            }

            /* Skip all tokens until end of line */
            while (token_ptr(pp)->type != ac_token_type_NEW_LINE
                && token_ptr(pp)->type != ac_token_type_EOF)
            {
                goto_next_token_from_directive(pp);
            }

            /* Remove macro it's previously defined. */
            ac_macro* m = find_macro(pp, &identifier);
            if (m)
            {
                undefine_macro(pp, m);
            }
        }
        else
        {
            ac_report_error_loc(location(pp), "Unknown directive.");
            return false;
        }
    }

    /* All directives must end with a new line of a EOF. */
    if (token_ptr(pp)->type != ac_token_type_NEW_LINE
        && token_ptr(pp)->type != ac_token_type_EOF)
    {
        ac_report_error("Internal error: directive did not end with a new line.");
        return false;
    }

    return true;
}

static bool parse_macro_definition(ac_pp* pp)
{
    expect(pp, ac_token_type_IDENTIFIER);

    ac_token* identifier = token_ptr(pp);
    ac_location loc = location(pp);
    ac_macro* m = create_macro(pp, identifier, loc);

    goto_next_raw_token(pp); /* Skip identifier, but not the whitespaces or comment. */

    /* There is a '(' right next tot the macro name, it's a function-like macro. */
    if (token(pp).type == ac_token_type_PAREN_L)
    {
        m->is_function_like = true;

        if (!parse_macro_parameters(pp, m))
        {
            return false;
        }
    }
    else
    {
        /* There are spaces or comment right after the macro name. */

        if (token(pp).type == ac_token_type_HORIZONTAL_WHITESPACE
            || token(pp).type == ac_token_type_COMMENT)
        {
            goto_next_token_from_directive(pp); /* Skip whitespaces to go to the macro value. */
        }
    }

    /* Parse body of macro. Which are all tokens until the next EOL or EOF. */
    if (!parse_macro_body(pp, m))
    {
        return false;
    }

    add_macro(pp, m);

    return true;
}

static bool parse_macro_parameters(ac_pp* pp, ac_macro* m)
{
    if (!expect(pp, ac_token_type_PAREN_L))
    {
        return false;
    }

    goto_next_token_from_directive(pp); /* Skip '(' */

    if (token(pp).type == ac_token_type_IDENTIFIER) /* Only allow identifiers in macro parameters. */
    {
        darrT_push_back(&m->definition, token(pp));

        goto_next_token_from_directive(pp); /* Skip identifier. */

        while (token(pp).type == ac_token_type_COMMA)
        {
            goto_next_token_from_directive(pp);  /* Skip ','. */

            if (!ac_token_is_keyword_or_identifier(token(pp).type)) /* Only allow identifiers or keywords in macro parameters. */
            {
                break;
            }
            darrT_push_back(&m->definition, token(pp));

            goto_next_token_from_directive(pp); /* Skip identifier. */
        }
    }

    if (!expect(pp, ac_token_type_PAREN_R))
    {
        return false;
    }

    goto_next_token_from_macro_body(pp); /* Skip ')' */

    range r = { 0u, darrT_size(&m->definition) };
    m->params = r;
    return true;
}

static bool parse_macro_body(ac_pp* pp, ac_macro* m)
{
    size_t body_start_index = darrT_size(&m->definition);

    ac_token* tok = token_ptr(pp);

    /* Return early if it's the EOF. */
    if (tok->type == ac_token_type_EOF)
    {
        return true;
    }

    /* Return early if it's a new line */
    if (tok->type == ac_token_type_NEW_LINE)
    {
        return true;
    }

    if (tok->type == ac_token_type_DOUBLE_HASH)
    {
        ac_report_error_loc(m->location, "'##' cannot appear at either end of a macro expansion.");
        return false;
    }

    tok->previous_was_space = false; /* Don't take into account the space before the first token of the macro body. */
    
    /* Get every token until the end of the body. */
    do
    {
        darrT_push_back(&m->definition, *tok);

        tok = goto_next_token_from_directive(pp);
    } while (tok->type != ac_token_type_NEW_LINE
           && tok->type != ac_token_type_EOF);

    if (tok->type == ac_token_type_DOUBLE_HASH)
    {
        ac_report_error_loc(m->location, "'##' cannot appear at either end of a macro expansion.");
        return false;
    }

    range r = { body_start_index, darrT_size(&m->definition) };
    m->body = r;

    return true;
}

static void macro_push(ac_pp* pp, ac_macro* m)
{
    m->identifier.ident->cannot_expand = true;
    pp->macro_depth += 1;
}

static ac_token* process_cmd(ac_pp* pp, ac_token_cmd* cmd)
{
    ac_token* result = NULL;
    switch (cmd->type) {
    case ac_token_cmd_type_TOKEN_LIST:
    {
        /* Update current token. */
        result = (cmd->token_list.data + cmd->token_list.i);

        cmd->token_list.i += 1; /* Go to next token index. */

        /* Remove list from the stack if the last token was just removed. */
        if (cmd->token_list.i == cmd->token_list.count)
        {
            darrT_pop_back(&pp->cmd_stack);
        }
        return result;
    }
    case ac_token_cmd_type_MACRO_POP:
    {
        pp->macro_depth -= 1;
        ac_macro* m = cmd->macro_pop.macro;

        m->identifier.ident->cannot_expand = false;

        darrT_destroy(&cmd->macro_pop.tokens);

        darrT_pop_back(&pp->cmd_stack);
        break;
    }
    default: 
        AC_ASSERT("Unreachable");
    }

    return NULL;
}
static ac_token* stack_pop(ac_pp* pp)
{
    if (!darrT_size(&pp->cmd_stack))
    {
        return NULL;
    }

    ac_token* result = NULL;
    do
    {
        result = process_cmd(pp, &darrT_last(&pp->cmd_stack));
    }
    while (darrT_size(&pp->cmd_stack) && result == NULL);

    return result;
}

static void push_cmd(ac_pp* pp, ac_token_cmd cmd)
{
    darrT_push_back(&pp->cmd_stack, cmd);
}

static bool try_expand(ac_pp* pp, ac_token* tok)
{
    if (!ac_token_is_keyword_or_identifier(tok->type))
    {
        return false;
    }

    ac_macro* m = find_macro(pp, tok);

    if (!m)
    {
        return false;
    }

    if (m->identifier.ident->cannot_expand)
    {
        /* @FIXME this might be totally uncessary because token.cannot_expand is setup when it's added to the command stack. */
        tok->cannot_expand = true;
    }

    if (tok->cannot_expand)
    {
        return false;
    }

    ac_token identifier = *tok;

    if (m->is_function_like)
    {
        goto_next_token_from_macro_agrument(pp); /* Skip identifier. */

        darrT_clear(&pp->buffer_for_peek);

        while (token(pp).type == ac_token_type_HORIZONTAL_WHITESPACE
            || token(pp).type == ac_token_type_COMMENT)
        {
            darrT_push_back(&pp->buffer_for_peek, token(pp));
        }

        /* In function-like macro, if there is no '(' next to the  identifier there is no need to expand anything.
           In this case, we also need to put back all the characters we just eaten when looking got the '('. */
        if (token(pp).type != ac_token_type_PAREN_L)
        {
            ac_token token_after_ident_and_whitespaces = token(pp);
            *pp->current_token = identifier; /* Restore  identifier. */
            darrT_push_back(&pp->buffer_for_peek, token_after_ident_and_whitespaces);

            push_cmd(pp, to_cmd_token_list(&pp->buffer_for_peek));

            return false;
        }
    }

    return expand_macro(pp, &identifier, m);
}

static size_t find_parameter_index(ac_token* token, ac_macro* m)
{
    if (!ac_token_is_keyword_or_identifier(token->type))
    {
        return (size_t)(-1);
    }

    size_t param_index = m->params.start;

    while (param_index < m->params.end) {
        ac_token* current_param = darrT_ptr(&m->definition, param_index);
        /* NOTE: Identifiers/keywords are the same. They should have the same string pointer. */
        if (token->ident == current_param->ident) {
            return param_index;
        }
        param_index += 1;
    }

    return (size_t)(-1);
}

static void concat(ac_pp* pp, darr_token* arr, ac_macro* m, ac_token left, ac_token right)
{
    /* Special case: when two empty tokens are concatenated a single empty token is created. */
    if (left.type == ac_token_type_EMPTY && right.type == ac_token_type_EMPTY)
    {
        darrT_push_back(arr, left);
        return;
    }

    /* Special case: when two hash are concatenated it should not result in the '##' operator.
       Hance, two different '#' are added. */
    if (left.type == ac_token_type_HASH && right.type == ac_token_type_HASH)
    {
        darrT_push_back(arr, left);
        right.previous_was_space = false;
        darrT_push_back(arr, right);
        return;
    }

    bool previous_was_space = left.previous_was_space;
    left.previous_was_space = false;  /* Avoid space in future concatenation. */
    right.previous_was_space = false; /* Avoid space in future concatenation. */
    dstr_clear(&pp->concat_buffer);
    ac_token_sprint(&pp->concat_buffer, left);
    ac_token_sprint(&pp->concat_buffer, right);

    /* Swap lexer. */
    ac_lex_swap(&pp->lex, &pp->concat_lex);

    strv new_content = dstr_to_strv(&pp->concat_buffer);
    /* @TODO: figure out what should be the identifier/name of the "file" being processed. */
    ac_lex_set_content(&pp->lex, new_content, "@TODO");

    ac_token* tok = ac_lex_goto_next(&pp->lex);

    tok->previous_was_space = previous_was_space;

    AC_ASSERT(tok);
    AC_ASSERT(tok->type != ac_token_type_EOF);

    do {
        darrT_push_back(arr, *tok);
    } while ((tok = ac_lex_goto_next(&pp->lex))->type != ac_token_type_EOF);

    /* Restore lexer. */
    ac_lex_swap(&pp->lex, &pp->concat_lex);
}

static ac_token stringize(ac_pp* pp, ac_token* tokens, size_t count)
{
    dstr_clear(&pp->concat_buffer);

    for (int i = 0; i < count; i += 1)
    {
        ac_token token = tokens[i];
    
        if (i > 0 && token.previous_was_space) /* add space if necessary. */
        {
            dstr_append_str(&pp->concat_buffer, " ");
        }

        if (token.type == ac_token_type_LITERAL_STRING
            || token.type == ac_token_type_LITERAL_CHAR)
        {
            const char* quote = token.type == ac_token_type_LITERAL_STRING ? "\\\"" : "'";
            dstr_append(&pp->concat_buffer, ac_token_prefix(token));
          
            dstr_append_str(&pp->concat_buffer, quote); /* Add escaped quote: \" */
            strv sv = ac_token_to_strv(token);

            for (size_t i = 0; i < sv.size; i += 1) {
                if (sv.data[i] == '\\' || sv.data[i] == '"')
                {
                    dstr_append_char(&pp->concat_buffer, '\\');
                }
                dstr_append_char(&pp->concat_buffer, sv.data[i]);
            }
            dstr_append_str(&pp->concat_buffer, quote); /* Add escaped quote: \" */
        }
        else
        {
            strv sv = ac_token_to_strv(token);
            dstr_append(&pp->concat_buffer, sv);
        }
    }
    
    ac_token t = { 0 };
    strv sv = ac_create_or_reuse_literal(pp->mgr, dstr_to_strv(& pp->concat_buffer));
    t.type = ac_token_type_LITERAL_STRING;
    t.text = sv;
    return t;
}

static void push_back_expanded_token(ac_pp* pp, darr_token* arr, ac_macro* m, ac_token token)
{
    size_t size = darrT_size(arr);

    if (size)
    {
        ac_token last = darrT_last(arr);
        
        /* The previous token was a ##, concatenation needed. */
        if (last.type == ac_token_type_DOUBLE_HASH)
        {
            size_t left_index = size - 2;
            ac_token left = darrT_at(arr, left_index);
            ac_token right = token;

            /* Change current size because we want to override the left token and the '##' token. */
            arr->arr.size = left_index;

            concat(pp, arr, m, left, right);
            return;
        }
    }

    /* If the token is the macro identifier we mark it as non expandable. */
    if (ac_token_is_keyword_or_identifier(token.type)
        && token.ident == m->identifier.ident)
    {
        token.cannot_expand = true;
    }
    darrT_push_back(arr, token);
}

static void add_empty_arg(darr_token* args, darr_range* ranges)
{
    range r = { 0 };
    r.start = darrT_size(args);
    
    {
        /* Add empty token */
        ac_token t = { 0 };
        t.type = ac_token_type_EMPTY;
        darrT_push_back(args, t);

        /* Add EOF token */
        ac_token eof = *ac_token_eof();
        darrT_push_back(args, eof);
    }

    r.end = darrT_size(args);
    /* Add the range. */
    darrT_push_back(ranges, r);
}

static bool expand_macro(ac_pp* pp, ac_token* identifier, ac_macro* m)
{
    bool result = false;

    ac_location loc = location(pp);

    darr_token args; /* @OPT: Use a pool allocator to avoid unncessary malloc/free. */
    darr_range ranges;  /* @OPT: Use a pool allocator to avoid unncessary malloc/free. */

    darrT_init(&args);
    darrT_init(&ranges);

    if (m->is_function_like)
    {
        AC_ASSERT(token(pp).type == ac_token_type_PAREN_L);

        int nesting_level = 0;
        goto_next_token_from_macro_agrument(pp); /* Skip '('. */
        nesting_level += 1;

        size_t param_count = m->params.end - m->params.start;
        size_t current_param_index = m->params.start;

        /*** Collect all the arguments. ***/

        /* Next token is a right parenthesis we only adjust the nesting level. */
        if (token(pp).type == ac_token_type_PAREN_R)
        {
            --nesting_level;
        }
        else
        {
            range r = { 0 };
            /* Continue until we can the correct right parenthesis. */
            while (nesting_level != 0)
            {
                while (token(pp).type != ac_token_type_EOF)
                {
                    if (token(pp).type == ac_token_type_PAREN_L)
                    {
                        nesting_level += 1;
                    }
                    else if (token(pp).type == ac_token_type_PAREN_R)
                    {
                        nesting_level -= 1;
                        /* @TODO check if we can just get rid of this one since there is another one on top*/
                        if (nesting_level == 0) /* Stop if the right parenthesis as been reached on the level 0. */
                        {
                            break;
                        }
                    }
                    /* Stop if a comma has been reached on the same depth. */
                    else if (token(pp).type == ac_token_type_COMMA && nesting_level == 1)
                    {
                        break;
                    }

                    ac_token t = token(pp);

                    darrT_push_back(&args, t);

                    goto_next_token_from_macro_agrument(pp);
                }

                if (token(pp).type == ac_token_type_EOF
                    && nesting_level != 0)
                {
                    ac_report_error_loc(loc, "Unexpected end of file in macro expansion "STRV_FMT".", STRV_ARG(identifier->ident->text));
                    break;
                }

                if (token(pp).type == ac_token_type_COMMA)
                {
                    goto_next_token_from_macro_agrument(pp); /* Skip ',' */
                }

                bool no_token_added = r.start == darrT_size(&args);
                if (no_token_added) /* Add empty token if there is no token in the arguments. */
                {
                    add_empty_arg(&args, &ranges);
                }
                else
                {
                    /* Add EOF token as sentinel value to be able to know where this sequence of tokens is ending. */
                    /* @FIXME: create a special token to avoid confusion. */
                    ac_token eof = *ac_token_eof();
                    darrT_push_back(&args, eof);

                    r.end = darrT_size(&args);

                    /* Add range. */
                    darrT_push_back(&ranges, r);

                }
                r.start = darrT_size(&args);
                current_param_index += 1;
            }
        }

        if (nesting_level != 0) {
            ac_report_error_loc(loc, "Function-like macro invocation '"STRV_FMT"' does not end with ')'.", STRV_ARG(m->identifier.ident->text));
            goto cleanup;
        }

        if (current_param_index > param_count) /* No parameter should be left. */
        {
            ac_report_warning_loc(loc, "Too many argument in function-like macro invocation '"STRV_FMT"'.", STRV_ARG(m->identifier.ident->text));
        }

        if (current_param_index < param_count) /* No parameter should be left. */
        {
            ac_report_warning_loc(loc, "Missing arguments in function-like macro invocation '"STRV_FMT"'.", STRV_ARG(m->identifier.ident->text));

            /* Where macro is missing argument we replace them with empty arguments. */
            for (int i = current_param_index; i < param_count; i += 1)
            {
                add_empty_arg(&args, &ranges);
            }
        }
    }

    size_t body_count = m->body.end - m->body.start;
    if (body_count <= 0) /* Expand to nothing. */
    {
        result = true;
        goto cleanup;
    }

    darr_token exp; /* @OPT: Use a pool allocator to avoid unncessary malloc/free. */
    darrT_init(&exp);

    /* Substitute body and expand arguments. */

    for (int i = m->body.start; i < m->body.end; i += 1)
    {
        ac_token body_token = darrT_at(&m->definition, i);

        size_t parameter_index = m->is_function_like ? find_parameter_index(&body_token, m) : -1;
        if (parameter_index != (size_t)(-1)) /* Parameter found. */
        {
            range original_range = darrT_at(&ranges, parameter_index);
            AC_ASSERT(darrT_at(&args, original_range.end - 1).type == ac_token_type_EOF);
            range adjusted_range = { original_range.start, original_range.end - 1 }; /* Adjust range to remove the last EOF. */

            /* If the previous tokan was '#' handle stringification. */
            if (darrT_size(&exp) && darrT_last(&exp).type == ac_token_type_HASH)
            {
                ac_token last = darrT_last(&exp);
                ac_token* tokens = darrT_ptr(&args, adjusted_range.start);
                size_t token_count = range_size(adjusted_range);

                size_t hash_index = darrT_size(&exp) - 1;
                exp.arr.size -= 1;
                
                ac_token t = stringize(pp, tokens, token_count);
                
                t.previous_was_space = last.previous_was_space;
                /* Replace '#'  token with the new stringified token*/
                darrT_push_back(&exp, t);
            }
            else
            {
                bool next_is_double_hash = i + 1 < m->body.end ? darrT_at(&m->definition, i + 1).type == ac_token_type_DOUBLE_HASH : false;
                bool previous_is_double_hash = i - 1 >= m->body.start ? darrT_at(&m->definition, i - 1).type == ac_token_type_DOUBLE_HASH : false;

                ac_token_cmd list = { 0 };
                ac_token* tokens = darrT_ptr(&args, original_range.start);
                size_t token_count = range_size(original_range);
                push_cmd(pp, make_cmd_token_list(tokens, token_count));

                int count = 0;
                while (goto_next_token_from_macro_body(pp)
                    && token_ptr(pp)->type != ac_token_type_EOF)
                {
                    size_t adjusted_token_count = range_size(adjusted_range);
                    ac_token* last_token = tokens + adjusted_token_count - 1;
                    ac_token* first_token = tokens;
                    bool do_not_expand_if_next_is_concat = next_is_double_hash && token_ptr(pp) == last_token;
                    bool do_not_expand_if_previous_is_concat = previous_is_double_hash && token_ptr(pp) == first_token;

                    bool expanded = false;
                    if (!do_not_expand_if_next_is_concat
                        && !do_not_expand_if_previous_is_concat)
                    {
                        expanded = try_expand(pp, token_ptr(pp));
                    }

                    /* If tokens were expanded we continue to the next tokens until it's unexapendable. */
                    if (!expanded)
                    {
                        ac_token token_from_argument = token(pp);
                        if (count == 0) /* First token must have a space if the body token had a space as well. */
                        {
                            token_from_argument.previous_was_space = body_token.previous_was_space;
                        }

                        push_back_expanded_token(pp, &exp, m, token_from_argument);
                        count++;
                    }
                }
            }
        }
        else
        {
            if (i == 0) /* First token must have a space if the body token had a space as well. */
            {
                body_token.previous_was_space = identifier->previous_was_space;
            }

            push_back_expanded_token(pp, &exp, m, body_token);
        }
    }

    macro_push(pp, m);
    push_cmd(pp, make_cmd_macro_pop(m, exp));

    /* Only push tokens if there are some. */
    if (exp.arr.size)
    {
        push_cmd(pp, to_cmd_token_list(&exp));
    }

    result = true;
cleanup:
    darrT_destroy(&args);
    darrT_destroy(&ranges);
    return result;
}

static ac_macro* create_macro(ac_pp* pp, ac_token* macro_name, ac_location location)
{
    AC_ASSERT(macro_name);
    AC_ASSERT(macro_name->type == ac_token_type_IDENTIFIER);
    /* @FIXME: ast_arena should be renamed. */
    ac_macro* m = ac_allocator_allocate(&pp->mgr->ast_arena.allocator, sizeof(ac_macro));
    AC_ASSERT(m);
    ac_macro_init(m);
    if (m && macro_name) {
        m->identifier = *macro_name;
    }
    m->location = location;
    return m;
}

static ac_location location(ac_pp* pp)
{
    return pp->lex.location;
}

static ac_token token(ac_pp* pp)
{
    return *pp->current_token;
}

static ac_token* token_ptr(ac_pp* pp)
{
    return pp->current_token;
}

static bool expect(ac_pp* pp, enum ac_token_type type)
{
    return pp->current_token->type == type;
}

static ac_token_cmd make_cmd_token_list(ac_token* data, size_t count)
{
    ac_token_cmd cmd = {0};
    cmd.type = ac_token_cmd_type_TOKEN_LIST;
    cmd.token_list.data = data;
    cmd.token_list.count = count;
    return cmd;
}

static ac_token_cmd make_cmd_macro_pop(ac_macro* m, darr_token tokens)
{
    ac_token_cmd cmd = { 0 };
    cmd.type = ac_token_cmd_type_MACRO_POP;
    cmd.macro_pop.macro = m;
    cmd.macro_pop.tokens = tokens;
    return cmd;
}

static ac_token_cmd to_cmd_token_list(darr_token* arr)
{
    ac_token_cmd cmd = { 0 };
    cmd.token_list.data = arr->arr.data;
    cmd.token_list.count = arr->arr.size;
    return cmd;
}

static ht_hash_t macro_hash(ht_ptr_handle* handle)
{
    ac_macro* m = (ac_macro*)handle->ptr;
    return ac_djb2_hash((char*)m->identifier.ident->text.data, m->identifier.ident->text.size);
}

static ht_bool macros_are_same(ht_ptr_handle* hleft, ht_ptr_handle* hright)
{
    ac_macro* left = (ac_macro*)hleft->ptr;
    ac_macro* right = (ac_macro*)hright->ptr;
    return left->identifier.ident == right->identifier.ident;
}

static void add_macro(ac_pp* pp, ac_macro* m)
{
    ht_ptr_insert(&pp->macros, m);
}

static void undefine_macro(ac_pp* pp, ac_macro* m)
{
    ht_ptr_remove(&pp->macros, m);

    /* Add reference to undefined macro to garbage collect them. */
    darrT_push_back(&pp->undef_macros, m);
}

static ac_macro* find_macro(ac_pp* pp, ac_token* identifer)
{
    ac_macro key;
    key.identifier = *identifer;

    return ht_ptr_get(&pp->macros, &key);
}