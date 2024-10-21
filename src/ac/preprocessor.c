#include "preprocessor.h"

typedef struct macro_arg_node macro_arg_node;
struct macro_arg_node {
    ac_token* param_name;      /* Name of macro parameter to which this arg belongs to. */
    ac_token_node* args_node;  /* First token of the argument. */
    macro_arg_node* next;
};

typedef struct ac_macro ac_macro;
struct ac_macro {
    ac_token identifier;       /* Name of the macro */
    /* '#define X(x, y) (x + y)' is a function-like macro,
    whereas '#define Y (1 + 2)' is an object-like macro. */
    bool is_function_like;
    ac_token_node* params_node; /* Argument of macro if it's function-like macro. */
    ac_token_node* body_node;   /* First token of the macro. */
    ac_location location;
};

static strv directive_define = STRV("define");

/* Get token from the stack or from the lexer. */
static ac_token* goto_next_raw_token(ac_pp* pp);
/* Get token from goto_next_raw_token and skip comments and whitespaces. */
static ac_token* goto_next_token(ac_pp* pp);

static ac_token* parse_directive(ac_pp* pp);
static bool parse_macro_definition(ac_pp* pp);
static void parse_macro_parameters(ac_pp* pp, ac_macro* macro);
static bool parse_macro_body(ac_pp* pp, ac_macro* macro);

/* Get and remove the first token from macro in the list. */
static ac_token_node* stack_pop(ac_pp* pp);
static void stack_push(ac_pp* pp, ac_token_list list);

/* Expand the macro and return the first token of the macro. */
static ac_token* expand_macro(ac_pp* pp, ac_macro* macro);
static macro_arg_node* find_token_in_args(ac_token* token, macro_arg_node* node);
/* Concatenate two token and return the chain of tokens created by the concatenation.
   The first node is returned. */
static ac_token_node* concat(ac_pp* pp, ac_token_node* left, ac_token_node* right);
static bool substitute_macro_body(ac_pp* pp, ac_macro* macro, macro_arg_node* arg_node);
static ac_token* expand_function_macro(ac_pp* pp, ac_macro* macro);

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

static ht_hash_t macro_hash(ac_macro* m);
static ht_bool macros_are_same(ac_macro* left, ac_macro* right);
static void swap_macros(ac_macro* left, ac_macro* right);

static void add_macro(ac_pp* pp, ac_macro* macro);
static ac_macro* find_macro(ac_pp* pp, ac_token* identifer);

void ac_pp_init(ac_pp* pp, ac_manager* mgr, strv content, const char* filepath)
{
    memset(pp, 0, sizeof(ac_pp));
    pp->mgr = mgr;

    ac_lex_init(&pp->lex, mgr);
    ac_lex_set_content(&pp->lex, content, filepath);
    ac_lex_init(&pp->concat_lex, mgr);

    ht_init(&pp->macros, sizeof(ac_macro), (ht_hash_function_t)macro_hash, (ht_predicate_t)macros_are_same, (ht_swap_function_t)swap_macros, 0);

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
next_token_again:
    ac_token* token = goto_next_raw_token(pp);

    /* Starts with '#', handle the preprocessor directive. */
    while (token->type == ac_token_type_HASH)
    {
        token = parse_directive(pp);
        if (token->type == ac_token_type_EOF)
            return token;
    }

    /* Expand macro for identifiers. */
    ac_macro* m;
    while (ac_token_is_keyword_or_identifier(*token)
        && (m = find_macro(pp, token)) != NULL)
    {
        /* If the macro exists and have tokens we push them. */
        if (m)
        {
            if (m->body_node) {
                token = expand_macro(pp, m);
            }
            else {
                /* Macro does not have body so it expands to nothing, hence just jump straight to the next token. */
                goto next_token_again;
            }
        }

    }

    return token;
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

static ac_token* goto_next_token(ac_pp* pp)
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

static ac_token* parse_directive(ac_pp* pp)
{
    AC_ASSERT(token(pp).type == ac_token_type_HASH);

    ac_token* tok = goto_next_token(pp); /* Skip '#' */

    if (tok->type == ac_token_type_IDENTIFIER)
    {
        /* #define */
        if (consume_if(pp, strv_equals(tok->text, directive_define)))
        {
            if (!parse_macro_definition(pp))
            {
                return ac_token_eof();
            }
        }
        else
        {
            ac_report_error_loc(location(pp), "Unknown directive.");
            return ac_token_eof();
        }
    }

    return token_ptr(pp);
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
        
        parse_macro_parameters(pp, m);
    }
    else
    {
        /* There are spaces or new lines right after the macro name. */

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

static void parse_macro_parameters(ac_pp* pp, ac_macro* macro)
{
    if (!expect_and_consume(pp, ac_token_type_PAREN_L))
    {
        return;
    }

    if (token(pp).type == ac_token_type_IDENTIFIER)
    {
        macro->params_node = create_token_node(pp, token_ptr(pp), NULL);
        ac_token_node* node = macro->params_node;
        goto_next_token(pp); /* Skip identifier. */
        while (consume_if_type(pp, ac_token_type_COMMA)
            && token(pp).type == ac_token_type_IDENTIFIER)
        {
            node->next = create_token_node(pp, token_ptr(pp), node);
            goto_next_token(pp); /* Skip identifier. */
        }
    }

    expect_and_consume(pp, ac_token_type_PAREN_R);
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
        /* Eat single new line. */
        goto_next_raw_token(pp);
        return true;
    }

    /* Create first token of the body with the current one and go to the next token. */
    macro->body_node = create_token_node(pp, tok, NULL);
    tok = goto_next_token(pp);

    /* Get every token until the end of the body. */
    ac_token_node* node = macro->body_node;
    while (tok->type != ac_token_type_NEW_LINE
       && tok->type != ac_token_type_EOF)
    {
        node = create_token_node(pp, tok, node);
        node->previous_was_space = pp->previous_was_space;
        tok = goto_next_token(pp);
    };

    /* Eat single new line if necessary. */
    if (tok->type == ac_token_type_NEW_LINE)
    {
        goto_next_raw_token(pp);
    }

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
    size_t size = pp->stack.darr.size;
    if (size)
    {
        ac_token_list* list = darrT_ptr(&pp->stack, size - 1 ); /* Get last list. */

        /* Weird trick to return "virtual" space token from expanded tokens.
           If the token we are about to return contains a space before then we
           return a virtual space token instead of the token itself. */
        if (list->node->previous_was_space)
        {
            list->node->previous_was_space = false;
            return &leading_space_token_node;
        }
       
        pp->expanded_token = *list->node;
        /* If it was the last token from the list we go to the next list. */
        if (list->node->next)
            list->node = list->node->next;
        else
            darrT_pop_back(&pp->stack);

        return &pp->expanded_token;
    }
    return NULL;
}

static void stack_push(ac_pp* pp, ac_token_list list)
{
    darrT_push_back(&pp->stack, list);
}

/* Add all tokens of a macro to the queue and pop the first token of the macro. */
static ac_token* expand_macro(ac_pp* pp, ac_macro* macro)
{
    if (macro->is_function_like)
    {
        return expand_function_macro(pp, macro);
    }
    else
    {
        ac_token_list list;
        list.node = macro->body_node;
        list.macro = macro;
        stack_push(pp, list);
        ac_token_node* tok_node = stack_pop(pp);
        return &tok_node->token;
    }
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

static ac_token* expand_function_macro(ac_pp* pp, ac_macro* macro)
{
    AC_ASSERT(macro->is_function_like);

    /* Skip the current identifier of the function-like macro. */
    if (!expect_and_consume(pp, ac_token_type_IDENTIFIER))
    {
        return ac_token_eof();
    }

    /* If the matching macro identifier does not have parenthesis we need to return the identifier itself. */
    if (token(pp).type != ac_token_type_PAREN_L)
    {
        /* We use the macro identifier to avoid allocating here. */
        return &macro->identifier;
    }
   
    goto_next_token(pp); /* Skip parenthesis. */

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

                goto_next_token(pp);
            }
            current_arg_node->args_node = root_token_node.next; /* Add node list to the current argument before creating a new one. */
        }
    }

    if (params) /* No parameter should be left. */
    {
        ac_report_error_loc(location(pp), "Macro call is missing arguments.");
        return ac_token_eof(pp);
    }

    /* NOTE: We don't want to skip this right parenthis so that it will be skip on the next round
       after the consumption of all the tokens from the macro expansion(s). */
    if (!expect(pp, ac_token_type_PAREN_R)) {
        return ac_token_eof(pp);
    }

    if (!substitute_macro_body(pp, macro, root_arg_node.next)) {
        return ac_token_eof(pp);
    }
 
    ac_token_node* tok_node = stack_pop(pp);
    return &tok_node->token;
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

static ht_hash_t macro_hash(ac_macro* m)
{
    return ac_djb2_hash((char*)m->identifier.text.data, m->identifier.text.size);
}

static ht_bool macros_are_same(ac_macro* left, ac_macro* right)
{
    return strv_equals(left->identifier.text, right->identifier.text);
}

static void swap_macros(ac_macro* left, ac_macro* right)
{
    ac_macro tmp;
    tmp = *left;
    *left = *right;
    *right = tmp;
}

static void add_macro(ac_pp* pp, ac_macro* m)
{
    AC_ASSERT(m);

    ht_insert(&pp->macros, m);
}

static ac_macro* find_macro(ac_pp* pp, ac_token* identifer)
{
    ac_macro m;
    m.identifier = *identifer;
    return ht_get_item(&pp->macros, &m);
}
