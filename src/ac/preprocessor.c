#include "preprocessor.h"

typedef struct range range;
struct range {
    size_t start;
    size_t end;
};

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
    bool cannot_expand;         /* Prevent macro expansion of already expanding macro to avoid recursion. */
    bool is_undef;
    /* expanded_args contains the expanded tokens of the current macro,
       since macro cannot be called recursively we can use a single struct
       to host the definition and the instantiation. */
    darr_token expanded_args;
};

static void ac_macro_init(ac_macro* m)
{
    memset(m, 0, sizeof(ac_macro));

    darrT_init(&m->definition);
    darrT_init(&m->expanded_args);
}

static void ac_macro_destroy(ac_macro* m)
{
    darrT_destroy(&m->definition);
    darrT_destroy(&m->expanded_args);
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

static bool parse_directive(ac_pp* pp);
static bool parse_macro_definition(ac_pp* pp);
static bool parse_macro_parameters(ac_pp* pp, ac_macro* m);
static bool parse_macro_body(ac_pp* pp, ac_macro* m);

static void macro_push(ac_pp* pp, ac_macro* m);
static void macro_pop(ac_pp* pp, ac_macro* m);

/* Get and remove the first expanded token. */
static ac_token* stack_pop(ac_pp* pp);
static void stack_push(ac_pp* pp, ac_token_list list);

/* Try to expand the token. Return true if it was expanded, false otherwise. */
static bool try_expand(ac_pp* pp, ac_token* token);
static size_t find_parameter_index(ac_token* token, ac_macro* m);

/* /* Concatenate two tokens and add them to the expanded_token array of the macro. */
static void concat(ac_pp* pp, ac_macro* m, ac_token left, ac_token right);
/* Add token to the expanded_token array of the macro.
   This also handle the concat operator '##'. */
static void push_back_expanded_token(ac_pp* pp, ac_macro* m, ac_token token);

/* Return true if something has been expanded.
   The expanded tokens are pushed into a stack used to pick the next token. */
static bool expand_function_macro(ac_pp* pp, ac_token* ident, ac_macro* m);
static void expand_object_macro(ac_pp* pp, ac_token* ident, ac_macro* m);

static ac_macro* create_macro(ac_pp* pp, ac_token* macro_name, ac_location location);

static ac_location location(ac_pp* pp); /* Return location of the current token. */

static ac_token token(ac_pp* pp); /* Current token by value. */
static ac_token* token_ptr(ac_pp* pp); /* Current token by pointer. */
static bool expect(ac_pp* pp, enum ac_token_type type);

/*------------------*/
/* macro hash table */
/*------------------*/

static ht_hash_t macro_hash(ht_ptr_handle* handle);
static ht_bool macros_are_same(ht_ptr_handle* hleft, ht_ptr_handle* hright);
static void add_macro(ac_pp* pp, ac_macro* m);
static void remove_macro(ac_pp* pp, ac_macro* m);
static ac_macro* find_macro(ac_pp* pp, ac_token* identifer);

void ac_pp_init(ac_pp* pp, ac_manager* mgr, strv content, const char* filepath)
{
    memset(pp, 0, sizeof(ac_pp));
    pp->mgr = mgr;

    ac_lex_init(&pp->lex, mgr);
    ac_lex_set_content(&pp->lex, content, filepath);
    ac_lex_init(&pp->concat_lex, mgr);

    ht_ptr_init(&pp->macros, (ht_hash_function_t)macro_hash, (ht_predicate_t)macros_are_same);

    darrT_init(&pp->stack);
    darrT_init(&pp->buffer_for_peek);
    dstr_init(&pp->concat_buffer);
}

void ac_pp_destroy(ac_pp* pp)
{
    dstr_destroy(&pp->concat_buffer);
    darrT_destroy(&pp->buffer_for_peek);
    darrT_destroy(&pp->stack);

    ac_lex_destroy(&pp->concat_lex);
    ac_lex_destroy(&pp->lex);

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

    if (token_ptr(pp)->type == ac_token_type_HASH)
    {
        while (token_ptr(pp)->type == ac_token_type_HASH)
        {
            if (!parse_directive(pp)
                || token_ptr(pp)->type == ac_token_type_EOF)
            {
                return ac_token_eof(pp);
            }

            goto_next_raw_token(pp);
        }
    }

    if (token_ptr(pp)->type == ac_token_type_EOF)
    {
        return token_ptr(pp);
    }

    return token_ptr(pp);
}

static ac_token* goto_next_token_from_directive(ac_pp* pp)
{
    ac_token* token = goto_next_normal_token(pp);
    ac_token* previous_token = token;

    while (token->type == ac_token_type_HORIZONTAL_WHITESPACE
        || token->type == ac_token_type_COMMENT)
    {
        token = goto_next_normal_token(pp);
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

static bool parse_directive(ac_pp* pp)
{
    AC_ASSERT(token(pp).type == ac_token_type_HASH);

    ac_token* tok = goto_next_token_from_directive(pp); /* Skip '#' */

    if (tok->type == ac_token_type_IDENTIFIER)
    {
        /* #define */
        if (strv_equals(tok->text, directive_define))
        {
            goto_next_token_from_directive(pp); /* Skip 'define' */
            if (!parse_macro_definition(pp))
            {
                return false;
            }
        }
        /* #undef */
        else if (strv_equals(tok->text, directive_undef))
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
                remove_macro(pp, m);
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

            if (!ac_token_is_keyword_or_identifier(token(pp))) /* Only allow identifiers or keywords in macro parameters. */
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

    goto_next_token_from_directive(pp); /* Skip ')' */

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
    
    /* Create first token of the body with the current one and go to the next token. */
    darrT_push_back(&m->definition, *tok);

    tok = goto_next_token_from_directive(pp);

    /* Get every token until the end of the body. */
    while (tok->type != ac_token_type_NEW_LINE
       && tok->type != ac_token_type_EOF)
    {
        darrT_push_back(&m->definition, *tok);

        tok = goto_next_token_from_directive(pp);
    };

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
    pp->macro_depth += 1;
}

static void macro_pop(ac_pp* pp, ac_macro* m)
{
    pp->macro_depth -= 1;
    m->cannot_expand = false;
    if (m->is_undef)
    {
        ac_macro_destroy(m);
    }
}

static ac_token* stack_pop(ac_pp* pp)
{
    while (darrT_size(&pp->stack))
    {
        size_t index_of_last = darrT_size(&pp->stack) - 1;
        ac_token_list* list = darrT_ptr(&pp->stack, index_of_last); /* Get last list. */

        /* End of the list has been reached so we remove the list from the stack.
           This is done now and after incrementing i because we want the macro
           to be unexapandable until then. */
        if (list->i == darrT_size(list->tokens))
        {
            /* If the list of token was coming from a macro
               we adjust the macro depth and allow it to be expandable again.*/
            if (list->macro)
            {
                macro_pop(pp, list->macro);
            }
            darrT_pop_back(&pp->stack);
            continue;
        }

        /* Update current token. */
        pp->expanded_token = darrT_at(list->tokens, list->i);

        list->i += 1; /* Go to next token index. */

        return &pp->expanded_token;
    }
    return NULL;
}

static void stack_push(ac_pp* pp, ac_token_list list)
{
    AC_ASSERT(list.tokens);

    if (list.macro)
    {
        macro_push(pp, list.macro);
    }

    darrT_push_back(&pp->stack, list);
}

static bool try_expand(ac_pp* pp, ac_token* tok)
{
    if (!ac_token_is_keyword_or_identifier(*tok))
    {
        return false;
    }

    ac_macro* m = find_macro(pp, tok);

    if (!m || m->cannot_expand)
    {
        return false;
    }

    ac_token identifier = *tok;

    bool expanded = false;
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

            ac_token_list list = { 0 };
            list.tokens = &pp->buffer_for_peek;
            list.macro = NULL;
            stack_push(pp, list);

            return false;
        }

        expanded = expand_function_macro(pp, &identifier, m);
    }
    else
    {
        expand_object_macro(pp, &identifier, m);
        expanded = true;
    }

    m->cannot_expand = expanded;

    return expanded;
}

static size_t find_parameter_index(ac_token* token, ac_macro* m)
{
    if (!ac_token_is_keyword_or_identifier(*token))
    {
        return (size_t)(-1);
    }

    size_t param_index = m->params.start;

    while (param_index < m->params.end) {
        ac_token* current_param = darrT_ptr(&m->definition, param_index);
        /* NOTE: Identifiers/keywords are the same. They should have the same string pointer. */
        if (token->text.data == current_param->text.data) {
            return param_index;
        }
        param_index += 1;
    }

    return (size_t)(-1);
}

static void concat(ac_pp* pp, ac_macro* m, ac_token left, ac_token right)
{
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

    darrT_push_back(&m->expanded_args, *tok);

    while ((tok = ac_lex_goto_next(&pp->lex))->type != ac_token_type_EOF)
    {
        darrT_push_back(&m->expanded_args, *tok);
    }

    /* Restore lexer. */
    ac_lex_swap(&pp->lex, &pp->concat_lex);
}

static void push_back_expanded_token(ac_pp* pp, ac_macro* m, ac_token token)
{
    size_t size = darrT_size(&m->expanded_args);

    /* The previous token was a ##, concatenation needed. */
    if (size
        && darrT_last(&m->expanded_args).type == ac_token_type_DOUBLE_HASH)
    {
        size_t left_index = size - 2;
        ac_token left = darrT_at(&m->expanded_args, left_index);
        ac_token right = token;

        /* Change current size because we want to override the left token and the '##' token. */
        m->expanded_args.arr.size = left_index;

        concat(pp, m, left, right);
    }
    else
    {
        darrT_push_back(&m->expanded_args, token);
    }
}

static bool expand_function_macro(ac_pp* pp, ac_token* identifier, ac_macro* m)
{
    bool result = false;
    AC_ASSERT(m->is_function_like);
    AC_ASSERT(token(pp).type == ac_token_type_PAREN_L);

    goto_next_token_from_macro_agrument(pp); /* Skip '('. */

    darrT(ac_token) args;
    darrT(range) ranges;

    darrT_init(&args);
    darrT_init(&ranges);

    size_t param_count = m->params.end - m->params.start;
    size_t current_param_index = m->params.start;

    /* Collect all the arguments. */
    if (token(pp).type != ac_token_type_PAREN_R)
    {
        range r = { 0 };

        while (current_param_index < param_count)
        {
            ac_token param = darrT_at(&m->definition, current_param_index);
            while (token(pp).type != ac_token_type_EOF
                && token(pp).type != ac_token_type_COMMA
                && token(pp).type != ac_token_type_PAREN_R)
            {
                ac_token t = token(pp);

                if (!try_expand(pp, &t))
                {
                    darrT_push_back(&args, t);
                }

                goto_next_token_from_macro_agrument(pp);
            }

            if (token(pp).type == ac_token_type_EOF)
            {
                goto cleanup;
            }

            if (token(pp).type == ac_token_type_COMMA)
            {
                goto_next_token_from_macro_agrument(pp); /* Skip ',' */
            }

            r.end = darrT_size(&args);

            darrT_push_back(&ranges, r);

            r.start = darrT_size(&args);

            current_param_index += 1;
        }
    }

    if (current_param_index < param_count) /* No parameter should be left. */
    {
        ac_report_error_loc(location(pp), "Macro call is missing arguments.");
        goto cleanup;
    }

    if (token(pp).type != ac_token_type_PAREN_R) {
        ac_report_error_loc(location(pp), "Macro does not end with ')'.");
        goto cleanup;
    }

    size_t body_count = m->body.end - m->body.start;
    if (body_count <= 0) /* Expand to nothing. */
    {
        result = true;
        goto cleanup;
    }

    /* Expand tokens. */
    darrT_clear(&m->expanded_args);

    for (int i = m->body.start; i < m->body.end; i += 1)
    {
        ac_token body_token = darrT_at(&m->definition, i);

        size_t parameter_index = find_parameter_index(&body_token, m);
        if (parameter_index != (size_t)(-1)) /* Parameter found. */
        {
            range r = darrT_at(&ranges, parameter_index);
            for (int j = r.start; j < r.end; ++j)
            {
                ac_token token_from_argument = darrT_at(&args, j);
                if (j == r.start)
                {
                    token_from_argument.previous_was_space = body_token.previous_was_space;
                }

                push_back_expanded_token(pp, m, token_from_argument);
            }
        }
        else
        {
            if (i == 0)
            {
                body_token.previous_was_space = identifier->previous_was_space;
            }
            push_back_expanded_token(pp, m, body_token);
        }
    }

    ac_token_list list = { 0 };
    list.tokens = &m->expanded_args;
    list.macro = m;

    stack_push(pp, list);

    result = true;
cleanup:
    darrT_destroy(&args);
    darrT_destroy(&ranges);
    return result;
}

static void expand_object_macro(ac_pp* pp, ac_token* identifier, ac_macro* m)
{
    AC_ASSERT(!m->is_function_like);

    darrT_clear(&m->expanded_args);

    size_t body_size = m->body.end - m->body.start;
    if (body_size) /* No items to add to the stack, but the macro is considered expanded. */
    {
        for (int i = m->body.start; i < m->body.end; i += 1)
        {
            ac_token body_token = darrT_at(&m->definition, i);
            if (i == 0)
            {
                body_token.previous_was_space = identifier->previous_was_space;
            }
            push_back_expanded_token(pp, m, body_token);
        }

        ac_token_list list = { 0 };
        list.tokens = &m->expanded_args;
        list.macro = m;
        stack_push(pp, list);
    }
}

static ac_macro* create_macro(ac_pp* pp, ac_token* macro_name, ac_location location)
{
    AC_ASSERT(macro_name->type == ac_token_type_IDENTIFIER);
    /* @FIXME: ast_arena should be renamed. */
    ac_macro* m = ac_allocator_allocate(&pp->mgr->ast_arena.allocator, sizeof(ac_macro));
    ac_macro_init(m);
    m->identifier = *macro_name;
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

static ht_hash_t macro_hash(ht_ptr_handle* handle)
{
    ac_macro* m = (ac_macro*)handle->ptr;
    return ac_djb2_hash((char*)m->identifier.text.data, m->identifier.text.size);
}

static ht_bool macros_are_same(ht_ptr_handle* hleft, ht_ptr_handle* hright)
{
    ac_macro* left = (ac_macro*)hleft->ptr;
    ac_macro* right = (ac_macro*)hright->ptr;
    return strv_equals(left->identifier.text, right->identifier.text);
}

static void add_macro(ac_pp* pp, ac_macro* m)
{
    ht_ptr_insert(&pp->macros, m);
}

static void remove_macro(ac_pp* pp, ac_macro* m)
{
    ht_ptr_remove(&pp->macros, m);

    m->is_undef = true;
}

static ac_macro* find_macro(ac_pp* pp, ac_token* identifer)
{
    ac_macro key;
    key.identifier = *identifer;

    return ht_ptr_get(&pp->macros, &key);
}