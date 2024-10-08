#include "preprocessor.h"

typedef struct ac_token_node ac_token_node;
struct ac_token_node {
    ac_token token;
    ac_token_node* next;
};

typedef struct ac_macro ac_macro;
struct ac_macro {
    ac_token identifier;       /* Name of the macro */
    /* '#define X(x, y) (x + y)' is a function-like macro,
    whereas '#define Y (1 + 2)' is an object-like macro. */
    bool is_function_like;     
    ac_token_node* args;       /* Argument of macro if it's function-like macro. */
    ac_token_node* body_begin; /* First token of the macro. */
    ac_token_node* body_end;   /* Last token of the macro. */
    ac_location location;
};

static strv directive_define = STRV("define");

static const ac_token* parse_directive(ac_pp* pp);
static void parse_macro_definition(ac_pp* pp, ac_token* identifier);
static void parse_macro_body(ac_pp* pp, ac_macro* macro);
/* Get and remove the first token from macro in the list. */
static ac_token_node* expanded_tokens_pop(ac_pp* pp);
/* Returns the first token of the macro. */
static ac_token* expanded_tokens_push(ac_pp* pp, ac_macro* macro);
static ac_macro* create_macro(ac_pp* pp, ac_token* macro_name);
static ac_token_node* create_token_node(ac_pp* pp, const ac_token* token_ident, ac_token_node* parent);

static const ac_token* goto_next_token(ac_pp* pp);
static ac_token token(ac_pp* pp); /* Current token by value. */
static const ac_token* token_ptr(ac_pp* pp); /* Current token by pointer. */
static bool expect(ac_pp* pp, enum ac_token_type type);
static bool expect_and_consume(ac_pp* pp, enum ac_token_type type);
static bool consume_if(ac_pp* pp, bool value);

/*------------------*/
/* macro hash table */
/*------------------*/

/* Slight reworked version of djb2 hash function. Make it take a length. */
static size_t djb2(char* str, size_t size);
static ht_hash_t macro_hash(ac_macro* m);
static ht_bool macros_are_same(ac_macro* left, ac_macro* right);
static void swap_macros(ac_macro* left, ac_macro* right);

static void add_macro(ac_pp* pp, ac_macro* macro);
static ac_macro* find_macro(ac_pp* pp, const ac_token* identifer);

void ac_pp_init(ac_pp* pp, ac_manager* mgr, strv content, const char* filepath)
{
    memset(pp, 0, sizeof(ac_pp));
    pp->mgr = mgr;

    ac_lex_init(&pp->lex, mgr, content, filepath);
    ht_init(&pp->macros, sizeof(ac_macro), (ht_hash_function_t)macro_hash, (ht_predicate_t)macros_are_same, (ht_swap_function_t)swap_macros, 0);
}

void ac_pp_destroy(ac_pp* pp)
{
    ht_destroy(&pp->macros);
    ac_lex_destroy(&pp->lex);
}

const ac_token* ac_pp_goto_next(ac_pp* pp)
{
    /* Get token from previously expanded macros if there are any left. */
    ac_token_node* token_node = expanded_tokens_pop(pp);
    if (token_node)
    {
        return &token_node->token;
    }

    /* Get raw token from the lexer until we get a none whitespace token. */
    const ac_token* token = ac_lex_goto_next(&pp->lex);

    /* Starts with '#', handle the preprocessor directive. */
    while (token->type == ac_token_type_HASH)
    {
        token = parse_directive(pp);
        if (!token)
            return NULL;
    }

    /* Expand macro for identifiers. */
    ac_macro* m;
    while (token->type == ac_token_type_IDENTIFIER
        && (m = find_macro(pp, token)) != NULL)
    {
        /* If the macro exists and have tokens we push them. */
        if (m && m->body_begin)
        {
            token = expanded_tokens_push(pp, m);
        }
    }

    return token;
}

static const ac_token* parse_directive(ac_pp* pp)
{
    AC_ASSERT(token(pp).type == ac_token_type_HASH);

    const ac_token* tok = goto_next_token(pp); /* Skip '#' */

    if (tok->type == ac_token_type_IDENTIFIER)
    {
        /* #define */
        if (consume_if(pp, strv_equals(tok->text, directive_define)))
        {
            ac_token identifier = token(pp);

            ac_lex_goto_next(&pp->lex); /* Skip identifier, but not the whitespaces.*/

            parse_macro_definition(pp, &identifier);
        }
        else
        {
            ac_report_error_loc(pp->lex.location, "Unknown directive.");
            return NULL;
        }
    }

    return token_ptr(pp);
}

static void parse_macro_definition(ac_pp* pp, ac_token* identifier)
{
    ac_macro* m = create_macro(pp, identifier);

    /* There is a '(' right next tot the macro name, it's a function-like macro. */
    if (token(pp).type == ac_token_type_PAREN_L)
    {
        m->is_function_like = true;
        ac_report_error("Internal error: function-like macro are not handled yet.");
        return;
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
    parse_macro_body(pp, m);
    add_macro(pp, m);
}

static void parse_macro_body(ac_pp* pp, ac_macro* macro)
{
    const ac_token* tok = token_ptr(pp);

    /* Return early if it's the EOF. */
    if (tok->type == ac_token_type_EOF)
    {
        return;
    }

    /* Return early if it's a new line */
    if (tok->type == ac_token_type_NEW_LINE)
    {
        /* Eat single new line*/
        ac_lex_goto_next(&pp->lex);
        return;
    }

    /* Create first token of the body with the current one and go to the next token. */
    macro->body_begin = create_token_node(pp, tok, NULL);
    tok = ac_lex_goto_next(&pp->lex);

    int token_count = 1;
    /* Get every token until the end of the body. */
    ac_token_node* node = macro->body_begin;
    ac_token_node* previous = macro->body_begin;
    while (tok->type != ac_token_type_NEW_LINE
       && tok->type != ac_token_type_EOF)
    {
        previous = node;
        node = create_token_node(pp, tok, node); /* @FIXME the last node as created but never used. */

        /* Get next raw token. */
        tok = ac_lex_goto_next(&pp->lex);
    };

    macro->body_end = previous;

    /* Eat single new line if necessary */
    if (tok->type == ac_token_type_NEW_LINE)
    {
        ac_lex_goto_next(&pp->lex);
    }
}

static ac_token_node* expanded_tokens_pop(ac_pp* pp)
{
    if (pp->expanded_tokens)
    {
        ac_token_node* node = pp->expanded_tokens;
        pp->expanded_tokens = pp->expanded_tokens->next;
        return node;
    }
    return NULL;
}

/* Add all tokens of a macro to the queue and pop the first token of the macro. */
static ac_token* expanded_tokens_push(ac_pp* pp, ac_macro* macro)
{
    ac_token_node* next = pp->expanded_tokens;
    macro->body_end = next;
    pp->expanded_tokens = macro->body_begin;

    ac_token_node* first = expanded_tokens_pop(pp);
    return &(first->token);
}

static ac_macro* create_macro(ac_pp* pp, ac_token* macro_name)
{
    AC_ASSERT(macro_name->type == ac_token_type_IDENTIFIER);
    ac_macro* m = ac_allocator_allocate(&pp->mgr->ast_arena.allocator, sizeof(ac_macro));
    memset(m, 0, sizeof(ac_macro));
    m->identifier = *macro_name;
    return m;
}

static ac_token_node* create_token_node(ac_pp* pp, const ac_token* token, ac_token_node* parent)
{
    ac_token_node* n = ac_allocator_allocate(&pp->mgr->ast_arena.allocator, sizeof(ac_token_node));
    memset(n, 0, sizeof(ac_token_node));
    n->token = *token;
    if (parent)
    {
        parent->next = n;
    }
    return n;
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

static ht_hash_t djb2(char* str, ht_size_t count)
{
    ht_hash_t hash = 5381;
    ht_size_t i = 0;
    while (i < count)
    {
        hash = ((hash << 5) + hash) + str[i]; /* hash * 33 + c */
        i++;
    }

    return hash;
}

static ht_hash_t macro_hash(ac_macro* m)
{
    return djb2((char*)m->identifier.text.data, m->identifier.text.size);
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

static ac_macro* find_macro(ac_pp* pp, const ac_token* identifer)
{
    ac_macro m;
    m.identifier = *identifer;
    return ht_get_item(&pp->macros, &m);
}
