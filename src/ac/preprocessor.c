#include "preprocessor.h"

typedef struct macro_arg_node macro_arg_node;
struct macro_arg_node {
    ac_token* param_name;      /* Name of macro parameter to which this arg belongs to. */
    ac_token_node* args_node;  /* First token of the argument. */
    macro_arg_node* next;
};

typedef struct ac_macro ac_macro;
struct ac_macro {
    ac_token identifier;        /* Name of the macro */
    /* '#define X(x, y) (x + y)' is a function-like macro,
    whereas '#define Y (1 + 2)' is an object-like macro. */
    bool is_function_like;
    ac_token_node* params_node; /* Argument of macro if it's function-like macro. */
    ac_token_node* body_node;   /* First token of the macro. */
    ac_location location;
    bool cannot_expand;         /* Macro should be expanded if they already are in expansion. */
};

static strv directive_define = STRV("define");
static strv directive_undef = STRV("undef");

/* Get token from the stack or from the lexer. */
static ac_token* goto_next_raw_token(ac_pp* pp);
/* Get preprocessed token ignoring whitespaces and comments.
   @FIXME: this is what is done in the parser, and it seems they require the same processing.
   Which means we can likely create a "ac_pp_goto_next_token(pp)" and a "ac_pp_goto_next_parser_token(pp)" */
static ac_token* goto_next_token_from_parameter(ac_pp* pp);
/* Get token from goto_next_raw_token and skip comments and whitespaces. */
static ac_token* goto_next_token(ac_pp* pp);

static bool parse_directive(ac_pp* pp);
static bool parse_macro_definition(ac_pp* pp);
static bool parse_macro_parameters(ac_pp* pp, ac_macro* macro);
static bool parse_macro_body(ac_pp* pp, ac_macro* macro);

/* Get and remove the first expanded token. */
static ac_token_node* stack_pop(ac_pp* pp);
static void stack_push(ac_pp* pp, ac_token_list list);

/* Try to expand the token. Return true if it was expanded, false otherwise. */
static bool try_expand(ac_pp* pp, ac_token* token);
static macro_arg_node* find_token_in_args(ac_token* token, macro_arg_node* node);
/* Concatenate two token and return the chain of tokens created by the concatenation.
   The first node is returned. */
static ac_token_node* concat(ac_pp* pp, ac_token_node* left, ac_token_node* right);
static bool substitute_macro_body(ac_pp* pp, ac_macro* macro, macro_arg_node* arg_node);
/* Return true if something has been expanded.
   The expanded tokens are pushed into a stack used to pick the next token. */
static bool expand_function_macro(ac_pp* pp, ac_macro* macro);

static ac_macro* create_macro(ac_pp* pp, ac_token* macro_name, ac_location location);
/* Create token_node from token. */
static ac_token_node* create_token_node(ac_pp* pp, ac_token* token_ident, ac_token_node* parent);
/* Create token_node from another token_node. */
static ac_token_node* copy_token_node(ac_pp* pp, ac_token_node* node, ac_token_node* parent);
/* Copy many token_node and return the last one. */
static ac_token_node* copy_many_token_node(ac_pp* pp, ac_token_node* node, ac_token_node* parent);
static macro_arg_node* create_macro_arg_node(ac_pp* pp, macro_arg_node* parent);

static ac_location location(ac_pp* pp); /* Return location of the current token. */

static ac_token token(ac_pp* pp); /* Current token by value. */
static ac_token* token_ptr(ac_pp* pp); /* Current token by pointer. */
static bool expect(ac_pp* pp, enum ac_token_type type);
static bool expect_and_consume(ac_pp* pp, enum ac_token_type type);
static bool consume_if(ac_pp* pp, bool value);
static bool consume_if_type(ac_pp* pp, enum ac_token_type type);

/*------------------*/
/* macro hash table */
/*------------------*/

static ht_hash_t macro_hash(ht_ptr_handle* handle);
static ht_bool macros_are_same(ht_ptr_handle* hleft, ht_ptr_handle* hright);
static void add_macro(ac_pp* pp, ac_macro* macro);
static void remove_macro(ac_pp* pp, ac_macro* macro);
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
    dstr_init(&pp->concat_buffer);
}

void ac_pp_destroy(ac_pp* pp)
{
    dstr_destroy(&pp->concat_buffer);
    darrT_destroy(&pp->stack);
    ht_destroy(&pp->macros);
    ac_lex_destroy(&pp->concat_lex);
    ac_lex_destroy(&pp->lex);
}

ac_token* ac_pp_goto_next(ac_pp* pp)
{
    goto_next_raw_token(pp);

    if (token_ptr(pp)->type == ac_token_type_HASH)
    {
        while (token_ptr(pp)->type == ac_token_type_HASH)
        {
            if (!parse_directive(pp))
            {
                return ac_token_eof(pp);
            }

            if (token_ptr(pp)->type == ac_token_type_EOF)
                return ac_token_eof(pp);

            goto_next_raw_token(pp);
        }
    }
 
    if (token_ptr(pp)->type == ac_token_type_EOF)
        return token_ptr(pp);

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
    ac_token_node* token_node = stack_pop(pp);

    pp->current_token = token_node
        ? &token_node->token
        : ac_lex_goto_next(&pp->lex);

    return pp->current_token;
}

static ac_token* goto_next_token_no_expand(ac_pp* pp)
{
    pp->previous_was_space = false;
    ac_token* token = goto_next_raw_token(pp);
    ac_token* previous_token = token;

    while (token->type == ac_token_type_HORIZONTAL_WHITESPACE
        || token->type == ac_token_type_COMMENT)
    {
        pp->previous_was_space = true;
        token = goto_next_raw_token(pp);
    }

    return token;
}

static ac_token* goto_next_token_from_parameter(ac_pp* pp)
{
    pp->previous_was_space = false;
    ac_token* token = ac_pp_goto_next(pp);
    ac_token* previous_token = token;

    while (token->type == ac_token_type_HORIZONTAL_WHITESPACE
        || token->type == ac_token_type_COMMENT
        || token->type == ac_token_type_NEW_LINE)
    {
        pp->previous_was_space = true;
        token = ac_pp_goto_next(pp);
    }

    return token;
}

static ac_token* goto_next_token(ac_pp* pp)
{
    pp->previous_was_space = false;
    ac_token* token = ac_pp_goto_next(pp);
    ac_token* previous_token = token;

    while (token->type == ac_token_type_HORIZONTAL_WHITESPACE
        || token->type == ac_token_type_COMMENT)
    {
        pp->previous_was_space = true;
        token = ac_pp_goto_next(pp);
    }

    return token;
}

static bool parse_directive(ac_pp* pp)
{
    AC_ASSERT(token(pp).type == ac_token_type_HASH);

    ac_token* tok = goto_next_token_no_expand(pp); /* Skip '#' */

    if (tok->type == ac_token_type_IDENTIFIER)
    {
        /* #define */
        if (strv_equals(tok->text, directive_define))
        {
            goto_next_token_no_expand(pp); /* Skip 'define' */
            if (!parse_macro_definition(pp))
            {
                return false;
            }
        }
        /* #undef */
        else if (strv_equals(tok->text, directive_undef))
        {
            goto_next_token_no_expand(pp); /* Skip 'undef' */
            expect(pp, ac_token_type_IDENTIFIER);
            ac_token identifier = token(pp);
            goto_next_token_no_expand(pp); /* Skip 'identifier' */

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
                goto_next_token_no_expand(pp);
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
            goto_next_token(pp); /* Skip whitespaces to go to the macro value. */
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

static bool parse_macro_parameters(ac_pp* pp, ac_macro* macro)
{
    if (!expect(pp, ac_token_type_PAREN_L))
    {
        return false;
    }

    goto_next_token_no_expand(pp); /* Skip '(' */

    if (token(pp).type == ac_token_type_IDENTIFIER) /* Only allow identifiers in macro parameters. */
    {
        macro->params_node = create_token_node(pp, token_ptr(pp), NULL);
        ac_token_node* node = macro->params_node;
        goto_next_token_no_expand(pp); /* Skip identifier. */

        while (token(pp).type == ac_token_type_COMMA)
        {
            goto_next_token_no_expand(pp);  /* Skip ','. */

            if (token(pp).type != ac_token_type_IDENTIFIER) /* Only allow identifiers in macro parameters. */
            {
                break;
            }
            node->next = create_token_node(pp, token_ptr(pp), node);
            goto_next_token_no_expand(pp); /* Skip identifier. */
        }
    }

    if (!expect(pp, ac_token_type_PAREN_R))
    {
        return false;
    }

    goto_next_token_no_expand(pp); /* Skip ')' */
    return true;
}

static bool parse_macro_body(ac_pp* pp, ac_macro* macro)
{
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

    /* Create first token of the body with the current one and go to the next token. */
    macro->body_node = create_token_node(pp, tok, NULL);
    tok = goto_next_token_no_expand(pp);

    /* Get every token until the end of the body. */
    ac_token_node* node = macro->body_node;
    while (tok->type != ac_token_type_NEW_LINE
       && tok->type != ac_token_type_EOF)
    {
        node = create_token_node(pp, tok, node);
        node->previous_was_space = pp->previous_was_space;
        tok = goto_next_token_no_expand(pp);
    };

    if (macro->body_node->token.type == ac_token_type_DOUBLE_HASH)
    {
        ac_report_error_loc(macro->location, "'##' cannot appear at either end of a macro expansion.");
        return false;
    }

    if (node->token.type == ac_token_type_DOUBLE_HASH)
    {
        ac_report_error_loc(macro->location, "'##' cannot appear at either end of a macro expansion.");
        return false;
    }

    return true;
}

static ac_token_node leading_space_token_node = { { ac_token_type_HORIZONTAL_WHITESPACE , STRV(" ")} };

static ac_token_node* stack_pop(ac_pp* pp)
{
    while (pp->stack.darr.size)
    {
        size_t index_of_last = pp->stack.darr.size - 1;
        ac_token_list* list = darrT_ptr(&pp->stack, index_of_last); /* Get last list. */

        /* The last list was empty so we remove it from the stack.
           We need to this here and not when the head is shifted (in the previous pop)
           because we want the macro to be unexapandable until then. */
        if (!list->node)
        {
            if (list->macro) /* Pop macro and make is expandable again. */
            {
                list->macro->cannot_expand = false;
            }

            darrT_pop_back(&pp->stack);
            continue;
        }

        /* Weird trick to return "virtual" space token from expanded tokens.
           If the token we are about to return contains a space before then we
           return a virtual space token instead of the token itself. */
        if (list->node->previous_was_space)
        {
            list->node->previous_was_space = false;
            return &leading_space_token_node;
        }

        pp->expanded_token = *list->node;

        /* Shift the head of the list to the next token.
           New node can be empty and it will be removed in the next pop */
        list->node = list->node->next; 

        return &pp->expanded_token;
    }
    return NULL;
}

static void stack_push(ac_pp* pp, ac_token_list list)
{
    darrT_push_back(&pp->stack, list);
}

static bool try_expand(ac_pp* pp, ac_token* token)
{
    if (!ac_token_is_keyword_or_identifier(*token))
    {
        return false;
    }

    ac_macro* m = find_macro(pp, token);

    if (!m || m->cannot_expand)
    {
        return false;
    }

    if (m->is_function_like)
    {
        return expand_function_macro(pp, m);
    }
    else
    {
        if (m->body_node) /* No items to add to the stack, but the macro is considered expanded. */
        {
            ac_token_list list;
            list.node = m->body_node;
            list.macro = m;
            stack_push(pp, list);
        }
        return true;
    }

    m->cannot_expand = true;

    return true;
}

static macro_arg_node* find_token_in_args(ac_token* token, macro_arg_node* node)
{
    if (!ac_token_is_keyword_or_identifier(*token))
    {
        return NULL;
    }
    while (node) {
        /* NOTE: Identifiers/keywords are the same. They should have the same string pointer. */
        if (token->text.data == node->param_name->text.data) {
            return node;
        }
        node = node->next;
    }

    return NULL;
}

static ac_token_node* concat(ac_pp* pp, ac_token_node* left, ac_token_node* right)
{
    ac_token_sprint(&pp->concat_buffer, left->token);
    ac_token_sprint(&pp->concat_buffer, right->token);

    /* Swap lexer. */
    ac_lex_swap(&pp->lex, &pp->concat_lex);  

    strv new_content = dstr_to_strv(&pp->concat_buffer);
    /* @TODO: figure out what should be the identifier/name of the "file" being processed. */
    ac_lex_set_content(&pp->lex, new_content, "@TODO");

    ac_token_node root_node = {0};
    ac_token_node* node = &root_node;
    ac_token* tok = NULL;
    while ((tok = ac_lex_goto_next(&pp->lex))->type != ac_token_type_EOF)
    {
        node = create_token_node(pp, tok, node);
    }

    /* Restore lexer. */
    ac_lex_swap(&pp->lex, &pp->concat_lex);

    root_node.next->previous_was_space = left->previous_was_space;

    dstr_clear(&pp->concat_buffer);

    return root_node.next;
}

static bool substitute_macro_body(ac_pp* pp, ac_macro* macro, macro_arg_node* arg_node)
{
    ac_token_node results_root = { 0 };
    
    ac_token_node* results = &results_root;
    ac_token_node* body_node = macro->body_node;
   
    if (!body_node) /* No token in the body, so it expand to nothing. */
    {
        return true;
    }

    do 
    {
        /* Search for macro in case the token is an keyword or an identifier. */
        macro_arg_node* macro_arg_found = find_token_in_args(&body_node->token, arg_node);
        if (macro_arg_found)
        {
            bool empty_arg = macro_arg_found->args_node == NULL; /* Empty macro argument was found. We don't need to do anything. */
            if (!empty_arg)
            {
                /* If the token matched a macro argument we push copies.
                   NOTE: if the token from the macro body have a leading space
                   the first token to expand also need a space. */
                macro_arg_found->args_node->previous_was_space = body_node->previous_was_space;
                results = copy_many_token_node(pp, macro_arg_found->args_node, results);
            }
        }
        else if (body_node->token.type ==  ac_token_type_DOUBLE_HASH)
        {
            /* Skip all double hashes. */
            while(body_node->token.type == ac_token_type_DOUBLE_HASH && body_node->next)
                body_node = body_node->next;

            ac_token_node* c = concat(pp, results, body_node);
            if (!c) {
                ac_report_error_loc(macro->location, "Could not concatenate some tokens in macro.");
                return false;
            }
                
            *results = *c; /* Replace the previous result (left side of the concatenation) with the new created node. */

            /* Move the results to the last node of the concatenation so we can add more result. */
            while (results->next)
                results = results->next; /* in case multiple tokens were return we go to the last one. */
        }
        else
        {
            results = copy_token_node(pp, body_node, results);
        }
        body_node = body_node->next;
    } while (body_node);

    /* Commit expanded tokens. */
    ac_token_list list;
    list.node = results_root.next;
    list.macro = macro;
    stack_push(pp, list);
    return true;
}

static bool expand_function_macro(ac_pp* pp, ac_macro* macro)
{
    AC_ASSERT(macro->is_function_like);

    /* Skip the current identifier of the function-like macro. */
    if (!expect_and_consume(pp, ac_token_type_IDENTIFIER))
    {
        return false;
    }

    /* If the matching macro identifier does not have parenthesis we don't need to expand anything. */
    if (token(pp).type != ac_token_type_PAREN_L)
    {
        /* We use the macro identifier to avoid allocating here. */
        return false;
    }
   
    goto_next_token_from_parameter(pp); /* Skip parenthesis. */

    ac_token_node* params = macro->params_node;
    
    /* Root of the macro arguments which will be use to expand the macro. */
    macro_arg_node root_arg_node = {0};
    macro_arg_node* current_arg_node = &root_arg_node;
    /* Root of the token to populate the various macro_arg_node. */
    ac_token_node root_token_node = {0};
    ac_token_node* node = &root_token_node;

    /* Get all macro arguments. */
    {
        /* No argument in this macro. We do nothing else.*/
        if (token(pp).type == ac_token_type_PAREN_R)
        {
        }
        /* Process arguments. */
        else
        {
            current_arg_node = create_macro_arg_node(pp, current_arg_node);
            current_arg_node->param_name = &params->token; /* Assign parameter name and... */
            params = params->next;                         /* ... move to the next one. */

            while (token(pp).type != ac_token_type_PAREN_R
            && token(pp).type != ac_token_type_EOF
            && token(pp).type != ac_token_type_NEW_LINE)
            {
                if (token(pp).type == ac_token_type_COMMA)
                {
                    current_arg_node->args_node = root_token_node.next; /* Add node list to the current argument before creating a new one. */
                    node = &root_token_node;                            /* Reinitialize the current not list */
                    node->next = NULL;

                    current_arg_node = create_macro_arg_node(pp, current_arg_node);
                    current_arg_node->param_name = &params->token; /* Assign parameter name and... */
                    if (!params)
                    {
                        ac_report_error_loc(location(pp), "Macro call has too many arguments.");
                    }
                    params = params->next;                         /* ... move to the next one. */

                }
                else
                {
                    node = create_token_node(pp, token_ptr(pp), node);
                    node->previous_was_space = pp->previous_was_space;
                }

                goto_next_token_from_parameter(pp);
            }
            current_arg_node->args_node = root_token_node.next; /* Add node list to the current argument before creating a new one. */
        }
    }

    if (params) /* No parameter should be left. */
    {
        ac_report_error_loc(location(pp), "Macro call is missing arguments.");
        return false;
    }

    /* NOTE: We don't want to skip this right parenthis so that it will be skip on the next round
       after the consumption of all the tokens from the macro expansion(s). */
    if (!expect(pp, ac_token_type_PAREN_R)) {
        return false;
    }

    if (!substitute_macro_body(pp, macro, root_arg_node.next)) {
        return false;
    }
 
    return true;
}

static ac_macro* create_macro(ac_pp* pp, ac_token* macro_name, ac_location location)
{
    AC_ASSERT(macro_name->type == ac_token_type_IDENTIFIER);
    /* @FIXME: ast_arena should be renamed. */
    ac_macro* m = ac_allocator_allocate(&pp->mgr->ast_arena.allocator, sizeof(ac_macro));
    memset(m, 0, sizeof(ac_macro));
    m->identifier = *macro_name;
    m->location = location;
    return m;
}

static ac_token_node* create_token_node(ac_pp* pp, ac_token* token, ac_token_node* parent)
{
    /* @FIXME: ast_arena should be renamed. */
    ac_token_node* n = ac_allocator_allocate(&pp->mgr->ast_arena.allocator, sizeof(ac_token_node));
    memset(n, 0, sizeof(ac_token_node));
    n->token = *token;
    if (parent) {
        parent->next = n;
    }
    return n;
}

static ac_token_node* copy_token_node(ac_pp* pp, ac_token_node* node, ac_token_node* parent)
{
    ac_token_node* n = ac_allocator_allocate(&pp->mgr->ast_arena.allocator, sizeof(ac_token_node));
    memcpy(n, node, sizeof(ac_token_node));
    if (parent) {
        parent->next = n;
    }
    return n;
}

static ac_token_node* copy_many_token_node(ac_pp* pp, ac_token_node* node, ac_token_node* parent)
{
    while (node) {
        parent = copy_token_node(pp, node, parent);
        node = node->next;
    }
    return parent;
}

static macro_arg_node* create_macro_arg_node(ac_pp* pp, macro_arg_node* parent)
{
    /* @FIXME: ast_arena should be renamed. */
    macro_arg_node* n = ac_allocator_allocate(&pp->mgr->ast_arena.allocator, sizeof(macro_arg_node));
    memset(n, 0, sizeof(macro_arg_node));
    if (parent)
    {
        parent->next = n;
    }
    return n;
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

static bool consume_if_type(ac_pp* pp, enum ac_token_type type)
{
    return consume_if(pp, token_ptr(pp)->type == type);
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
}

static ac_macro* find_macro(ac_pp* pp, ac_token* identifer)
{
    ac_macro key;
    key.identifier = *identifer;

    return ht_ptr_get(&pp->macros, &key);
}