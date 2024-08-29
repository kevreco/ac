#include "parser_c.h"

#include "ast.h"
#include "global.h"
#include "lexer.h"

#define CAST(type_, object_) (type_)(object_)

#define AST_NEW(type_, ident_, location_, ast_type_) \
        type_* ident_ = (type_*)malloc(sizeof(type_)); \
        do { \
            if (!ident_) { \
                printf("Could not malloc"); \
                exit(-1);  \
            } \
            ident_->type = ast_type_; \
            ident_->loc = location_; \
        } while (0);

#define AST_NEW_CTOR(type_, ident_, location_, constructor_) \
        type_* ident_ = (type_*)malloc(sizeof(type_)); \
        do { \
            if (!ident_) { \
                printf("Could not malloc"); \
                exit(-1);  \
            } \
            constructor_(ident_); \
            ident_->loc = location_; \
        } while (0);

#define TYPE_NEW_LIST(list_type_, ident_) \
        list_type_* ident_ = (list_type_*)malloc(sizeof(list_type_)); \
        do { \
            if (!ident_) { \
                printf("Could not malloc"); \
                exit(-1);  \
            } \
            ident_->value = 0; \
            ident_->next = 0; \
        } while (0);
        
#define if_printf(condition_, ...) do { if(condition_) printf(__VA_ARGS__); } while(0);


typedef bool (*ensure_expr_t)(struct ac_ast_expr* expr);

static struct ac_ast_top_level* parse_top_level(struct ac_parser_c* p);
static struct ac_ast_expr* parse_expr(struct ac_parser_c* p, struct ac_ast_expr* lhs);
static struct ac_ast_expr* parse_primary(struct ac_parser_c* p);
static struct ac_ast_block* parse_block(struct ac_parser_c* p);
static struct ac_ast_block* parse_block_or_inline_block(struct ac_parser_c* p);
static bool parse_statements(struct ac_parser_c* p, ensure_expr_t post_check, const char* message);
static struct ac_ast_expr* parse_statement(struct ac_parser_c* p);
static struct ac_ast_expr* parse_rhs(struct ac_parser_c* p, struct ac_ast_expr* lhs, int lhs_precedence);
static struct ac_ast_identifier* parse_identifier(struct ac_parser_c* p);
static struct ac_ast_declaration* parse_declaration_list(struct ac_parser_c* p, struct ac_ast_type_specifier* type_specifier);
static struct ac_ast_declaration* make_simple_declaration(struct ac_parser_c* p, struct ac_ast_type_specifier* type_specifier, struct ac_ast_declarator* declarator);
static struct ac_ast_declaration* make_function_declaration(struct ac_parser_c* p, struct ac_ast_type_specifier* type_specifier, struct ac_ast_declarator* declarator, struct ac_ast_block* block);
static struct ac_ast_declarator* parse_declarator_core(struct ac_parser_c* p, bool identifier_required);
static struct ac_ast_declarator* parse_declarator_for_parameter(struct ac_parser_c* p);
static struct ac_ast_declarator* parse_declarator(struct ac_parser_c* p);
static struct ac_ast_parameters* parse_parameter_list(struct ac_parser_c* p, enum ac_token_type expected_opening_token);
static struct ac_ast_parameter* parse_parameter(struct ac_parser_c* p);
static int count_and_consume_pointers(struct ac_parser_c* p);
static struct ac_ast_array_specifier* parse_array_specifier(struct ac_parser_c* p);
static struct ac_ast_expr* parse_statement_from_identifier(struct ac_parser_c* p, struct ac_ast_identifier* identifier);
static struct ac_ast_expr* parse_unary(struct ac_parser_c* p);
static struct ac_ast_type_specifier* try_parse_type(struct ac_parser_c* p, struct ac_ast_identifier* identifier);

static const struct ac_token* token(const struct ac_parser_c* p); /* current token type */
static struct ac_location location(const struct ac_parser_c* p);
static void goto_next_token(struct ac_parser_c* p);
static enum ac_token_type token_type(const struct ac_parser_c* p);              /* current token type */
static bool token_is(const struct ac_parser_c* p, enum ac_token_type type);     /* current token is */
static bool token_is_not(const struct ac_parser_c* p, enum ac_token_type type); /* current token is not */
static bool token_is_unary_operator(const struct ac_parser_c* p);               /* current token is an unary operator */
static bool token_equal_type(struct ac_token, enum ac_token_type type);
static bool expect(const struct ac_parser_c* p, enum ac_token_type type);
static bool expect_and_consume(struct ac_parser_c* p, enum ac_token_type type);

static bool expr_is(struct ac_ast_expr* expr, enum ac_ast_type type);

static bool expr_is_statement(struct ac_ast_expr* expr);
static struct ac_ast_expr* to_expr(void* any);
static void add_to_current_block(struct ac_parser_c* p, struct ac_ast_expr* expr);

static bool only_declaration(struct ac_ast_expr* expr) { return ac_ast_is_declaration(expr); }
static bool any_expr(struct ac_ast_expr* expr) { (void)expr; return true; }

void ac_parser_c_init(struct ac_parser_c* p, struct ac_manager* mgr)
{
    memset(p, 0, sizeof(struct ac_parser_c));

    ac_lex_init(&p->lex, mgr);

    p->mgr = mgr;

    p->options.debug_verbose = true;
}

void ac_parser_c_destroy(struct ac_parser_c* p)
{
    (void)p;

    ac_lex_destroy(&p->lex);
}

bool ac_parser_c_parse(struct ac_parser_c* p, const char* content, size_t content_size, const char* filepath)
{
    ac_lex_set_source(&p->lex, content, content_size, filepath);

    struct ac_ast_top_level* top_level = parse_top_level(p);
    p->mgr->top_level = top_level;

    return top_level != 0;
}

static struct ac_ast_top_level* parse_top_level(struct ac_parser_c* p)
{
    AST_NEW_CTOR(struct ac_ast_top_level, top_level, location(p), ac_ast_top_level_init);

    struct ac_ast_block* previous = p->current_block;

    p->current_block = &top_level->block;

    if (!parse_statements(p, only_declaration, "top level expressions can only be declarations.\n"))
    {
        return 0;
    }

    p->current_block = previous;

    return top_level;
}

static struct ac_ast_expr* parse_expr(struct ac_parser_c* p, struct ac_ast_expr* lhs)
{
    if_printf(p->options.debug_verbose, "parse_expression\n");

    if (token_is(p, ac_token_type_EOF))
    {
        return 0;
    }

    lhs = lhs ? lhs : parse_primary(p);

    if (!lhs)
    {
        return 0;
    }

    int lhs_precedence = 0;
    struct ac_ast_expr* rhs = parse_rhs(p, lhs, lhs_precedence);

    if (!rhs)
    {
        return lhs;
    }

    return rhs;
}

static struct ac_ast_expr* parse_primary(struct ac_parser_c* p)
{
    if_printf(p->options.debug_verbose, "parse_primary\n");

    struct ac_ast_expr* result = 0;

    if (token_is_unary_operator(p))
    {
        result = parse_unary(p);
    }
    else
    {
        switch (token_type(p)) {
        case ac_token_type_EOF: { /* Error must have been reported by "goto_next_token" or "expect_and_consume" */
            result = 0;
            return result;
        }
        case ac_token_type_IDENTIFIER: { /* <identifier> */
            struct ac_ast_identifier* ident = parse_identifier(p);
            /* @TODO create parse_postfix_expression() to handle function call or array access */
            result = to_expr(ident);
            break;
        }
        case ac_token_type_LITERAL_BOOL: {
            AST_NEW(struct ac_ast_literal, literal, location(p), ac_ast_type_LITERAL_BOOL);
            literal->u.boolean = p->lex.token.u.b.value;
            result = to_expr(literal);
            goto_next_token(p);
            break;
        }
        case ac_token_type_LITERAL_INTEGER: { 
            AST_NEW(struct ac_ast_literal, literal, location(p), ac_ast_type_LITERAL_INTEGER);
            literal->u.integer = p->lex.token.u.i.value;
            result = to_expr(literal);
            goto_next_token(p);
            break;
        }
        case ac_token_type_LITERAL_FLOAT: {
            AST_NEW(struct ac_ast_literal, literal, location(p), ac_ast_type_LITERAL_FLOAT);
            literal->u._float = p->lex.token.u.f.value;
            result = to_expr(literal);
            goto_next_token(p);
            break;
        }
        case ac_token_type_LITERAL_STRING: {
            AST_NEW(struct ac_ast_literal, literal, location(p), ac_ast_type_LITERAL_STRING);
            literal->u.str = p->lex.token.text;
            result = to_expr(literal);
            goto_next_token(p);
            break;
        }
        
        default: {
            ac_report_error_loc(location(p), "parse_primary, case not handled %d\n", token_type(p));
            return 0;
        }
        } /* switch (token_type(p)) */
    }

    return result;
}

static struct ac_ast_block* parse_block(struct ac_parser_c* p)
{
    if_printf(p->options.debug_verbose, "parse_block\n");

    AST_NEW_CTOR(struct ac_ast_block, block, location(p), ac_ast_block_init);

    if (!expect_and_consume(p, ac_token_type_BRACE_L))
    {
        return 0;
    }

    struct ac_ast_block* previous = p->current_block;

    p->current_block = block;

    if (!parse_statements(p, any_expr, ""))
    {
        return 0;
    }

    p->current_block = previous;

    if (!expect_and_consume(p, ac_token_type_BRACE_R))
    {
        return 0;
    }

    return block;
}

static struct ac_ast_block* parse_block_or_inline_block(struct ac_parser_c* p)
{
    if (token_is_not(p, ac_token_type_BRACE_L))
    {
        AST_NEW_CTOR(struct ac_ast_block, block, location(p), ac_ast_block_init);
        struct ac_ast_block* previous = p->current_block;

        p->current_block = block;

        struct ac_ast_expr* statement = parse_statement(p);
        if (!statement)
        {
            return 0;
        }

        p->current_block = previous;
        return block;
    }

    return parse_block(p);
}

static bool parse_statements(struct ac_parser_c* p, ensure_expr_t post_check, const char* message)
{
    if_printf(p->options.debug_verbose, "parse_statements\n");

    /* no statement if there is already a closing brace */
    if (token_is(p, ac_token_type_BRACE_R))
    {
        return true;
    }

    /* if last expression is not null this means everything was alright */
    struct ac_ast_expr* expr = 0;
    while (token_is_not(p, ac_token_type_BRACE_R)
        && token_is_not(p, ac_token_type_EOF))
    {
        expr = parse_statement(p);

        if (expr == 0)
        {
            return false;
        }

        if (!post_check(expr))
        {
            ac_report_error_expr(expr, message);
            return false;
        }
    }

    return expr != 0;
}

static struct ac_ast_expr* parse_statement(struct ac_parser_c* p)
{
    if_printf(p->options.debug_verbose, "parse_statement\n");

    struct ac_ast_expr* result = 0;

    switch (token_type(p)) {
    case ac_token_type_BRACE_L: { /* parse nested block statement */

        ac_report_error_loc(location(p), "internal error: nested block not handled yet, case not handled %d\n", token_type(p));
        return 0;
    }
    case ac_token_type_IDENTIFIER: { /* <identifier> */
        struct ac_ast_identifier* ident = parse_identifier(p);

        /* any kind of declarations etc. */
        struct ac_ast_expr* declaration = parse_statement_from_identifier(p, ident);

        if (!declaration) { return 0; }

        assert(ac_ast_is_declaration(declaration));

        /* all declarations (exception function definition) requires a trailing semi-colon */
        if (declaration->type != ac_ast_type_DECLARATION_FUNCTION_DEFINITION)
        {
            if (!expect_and_consume(p, ac_token_type_SEMI_COLON))
            {
                return 0;
            }
        }

        return declaration;
    }
    case ac_token_type_RETURN: {
        goto_next_token(p); /* Skip 'return' */
        AST_NEW_CTOR(struct ac_ast_return, ret, location(p), ac_ast_return_init);

        struct ac_ast_expr* lhs = 0;
        ret->expr = parse_expr(p, lhs);
        result = to_expr(ret);

        if (!expect_and_consume(p, ac_token_type_SEMI_COLON))
        {
            return 0;
        }
        break;
    }
    case ac_token_type_SEMI_COLON: { /* empty expression */
        AST_NEW(struct ac_ast_expr, empty_statement, location(p), ac_ast_type_EMPTY_STATEMENT);

        goto_next_token(p); /* Skip ';' */

        result = empty_statement;
        break;
    }

    default: {
        ac_report_error_loc(location(p), "parse_statement, case not handled %d\n", token_type(p));
    }
    }

    add_to_current_block(p, to_expr(result));

    return result;
}

static struct ac_ast_expr* parse_unary(struct ac_parser_c* p) {
    if_printf(p->options.debug_verbose, "parse_unary\n");

    assert(token_is_unary_operator(p));

    AST_NEW_CTOR(struct ac_ast_unary, unary, location(p), ac_ast_unary_init);
    unary->op = p->lex.token.type;

    goto_next_token(p); /* skip current unary token */

    unary->operand = parse_primary(p);

    return to_expr(unary);
}

static struct ac_ast_type_specifier* try_parse_type(struct ac_parser_c* p, struct ac_ast_identifier* identifier)
{
    if (!strv_equals_str(identifier->name, "int"))
    {
        ac_report_error_loc(location(p), "This parser can only handle 'int' as type.");
    }

    AST_NEW_CTOR(struct ac_ast_type_specifier, type_specifier, location(p), ac_ast_type_specifier_init);
    type_specifier->identifier = identifier;

    return type_specifier;
}

static struct ac_ast_expr* parse_rhs(struct ac_parser_c* p, struct ac_ast_expr* lhs, int lhs_precedence) {
    if_printf(p->options.debug_verbose, "parse_rhs\n");

    /* @TODO: handle precedence, binary operators etc.*/
    (void)lhs_precedence;
    (void)lhs;
    
    return lhs;
}

static struct ac_ast_identifier* parse_identifier(struct ac_parser_c* p) {

    assert(token_is(p, ac_token_type_IDENTIFIER));

    AST_NEW(struct ac_ast_identifier, result, location(p), ac_ast_type_IDENTIFIER);
    result->name = token(p)->text;

    goto_next_token(p); /* skip identifier */

    return result;
}

static struct ac_ast_expr* parse_statement_from_identifier(struct ac_parser_c* p, struct ac_ast_identifier* identifier)
{
    struct ac_ast_type_specifier* parsed_type = try_parse_type(p, identifier);

    return to_expr(parse_declaration_list(p, parsed_type));
}

static struct ac_ast_declaration* parse_declaration_list(struct ac_parser_c* p, struct ac_ast_type_specifier* type_specifier)
{
    struct ac_ast_declaration* last_declaration = 0;

    struct ac_ast_declarator* declarator = parse_declarator(p);

    if (!declarator) { return 0; }

    /* if the declarator match the function prototype "ident(a,b,c)" we check if there is a function body, if that's the case we handle  */
    if (declarator->pointer_depth == 0
        && declarator->parameters
        && declarator->array_specifier == 0
        && token_is(p, ac_token_type_BRACE_L))
    {
        struct ac_ast_block* block = parse_block(p);
        if (!block)
        {
            return 0;
        }

        return make_function_declaration(p, type_specifier, declarator, block);
    }

    last_declaration = make_simple_declaration(p, type_specifier, declarator);

    while (token_is(p, ac_token_type_COMMA))  /* , */
    {
        goto_next_token(p); /* skip , */

        struct ac_ast_declarator* next_declarator = parse_declarator(p);

        /* for each new declarator we materialize a separated delcaration, this way "int a, b;" becomes "int a; int b;" */
        last_declaration = make_simple_declaration(p, type_specifier, next_declarator);

        if (!last_declaration) { return 0; }
    }   

    /* we only return the last declaration to mean that everything want alright. */
    return last_declaration;
}

static struct ac_ast_declaration* make_simple_declaration(struct ac_parser_c* p, struct ac_ast_type_specifier* type_specifier, struct ac_ast_declarator* declarator)
{
    AST_NEW_CTOR(struct ac_ast_declaration, declaration, location(p), ac_ast_declaration_init);
    declaration->type = ac_ast_type_DECLARATION_SIMPLE;
    declaration->type_specifier = type_specifier;
    declaration->declarator = declarator;

    add_to_current_block(p, to_expr(declaration));

    return declaration;
}

static struct ac_ast_declaration* make_function_declaration(struct ac_parser_c* p, struct ac_ast_type_specifier* type_specifier, struct ac_ast_declarator* declarator, struct ac_ast_block* block)
{
    struct ac_ast_declaration* decl = make_simple_declaration(p, type_specifier, declarator);
    decl->type = ac_ast_type_DECLARATION_FUNCTION_DEFINITION;
    decl->function_block = block;
    return decl;
}

static struct ac_ast_declarator* parse_declarator_core(struct ac_parser_c* p, bool identifier_required)
{
    AST_NEW_CTOR(struct ac_ast_declarator, declarator, location(p), ac_ast_declarator_init);

    if (token_is(p, ac_token_type_STAR))
    {
        declarator->pointer_depth = count_and_consume_pointers(p);
    }

    if ((identifier_required && expect(p, ac_token_type_IDENTIFIER))
        || token_is(p, ac_token_type_IDENTIFIER))
    {
        declarator->ident = parse_identifier(p);
    }

    /* check for optional array specifier */
    while (token_is(p, ac_token_type_SQUARE_L)) /* [ */
    {
        struct ac_ast_array_specifier* array_specifier = parse_array_specifier(p);
        if (!array_specifier) { return 0; }

        declarator->array_specifier = array_specifier;
    }

    if (token_is(p, ac_token_type_EQUAL)) /* = */
    {
        goto_next_token(p); /* skip = */
        struct ac_ast_expr* lhs = 0;
        declarator->initializer = parse_expr(p, lhs);
        return declarator;
    }

    if (token_is(p, ac_token_type_PAREN_L))  /* ( */
    {
        declarator->parameters = parse_parameter_list(p, ac_token_type_PAREN_L);
        if (!declarator->parameters) { return 0; }

        return declarator;
    }

    return declarator;
}

static struct ac_ast_declarator* parse_declarator(struct ac_parser_c* p)
{
    bool identifier_required = true;
    struct ac_ast_declarator* declarator = parse_declarator_core(p, identifier_required);

    if (!declarator) { return 0; }

    assert(declarator->ident);

    return declarator;
}

static struct ac_ast_declarator* parse_declarator_for_parameter(struct ac_parser_c* p)
{
    bool identifier_required = false;

    struct ac_ast_declarator* declarator = parse_declarator_core(p, identifier_required);

    if (!declarator) { return 0; }

    /* There must be at least one of an identifier, or a pointer, or an array specifier */
    assert(declarator->ident || declarator->pointer_depth || declarator->array_specifier);

    return declarator;
}

static struct ac_ast_parameters* parse_parameter_list(struct ac_parser_c* p, enum ac_token_type expected_opening_token)
{
    if_printf(p->options.debug_verbose, "parse_parameter_list\n");

    AST_NEW_CTOR(struct ac_ast_parameters, parameters, location(p), ac_ast_parameters_init);

    /* only accept [ or ( */
    assert(expected_opening_token == ac_token_type_PAREN_L
        || expected_opening_token == ac_token_type_SQUARE_L);

    if (!expect_and_consume(p, expected_opening_token))
    {
        ac_report_error_loc(location(p), "parameters should start with '%.*s'.\n", ac_token_type_to_strv(expected_opening_token));
        return 0;
    }

    enum ac_token_type expected_closing_token = expected_opening_token == ac_token_type_PAREN_L ? ac_token_type_PAREN_R : ac_token_type_SQUARE_R;

    if (token_is(p, ac_token_type_PAREN_R))
    {
        return parameters;
    }

    do
    {
        struct ac_ast_parameter* param = parse_parameter(p);

        if (param == 0)
        {
            return 0;
        }

        ac_expr_list_add(&parameters->list, to_expr(param));

    } while (token_is(p, ac_token_type_COMMA)
        && expect_and_consume(p, ac_token_type_COMMA));

    if (!expect_and_consume(p, expected_closing_token))
    {
        ac_report_error_loc(location(p), "parameters should end with parenthesis or square brackets.\n");
        return 0;
    }

    return parameters;
}

static struct ac_ast_parameter* parse_parameter(struct ac_parser_c* p)
{
    if_printf(p->options.debug_verbose, "parse_parameter\n");

    AST_NEW_CTOR(struct ac_ast_parameter, param, location(p), ac_ast_parameter_init);

    if (token_is(p, ac_token_type_TRIPLE_DOT))
    {
        ac_report_error_loc(location(p), "internal error: var args are not supported yet.");
        return param;
    }

    if (!expect(p, ac_token_type_IDENTIFIER))
    {
        return 0;
    }
    
    struct ac_ast_identifier* type_name = parse_identifier(p);
    param->type_name = type_name;


    if (strv_equals_str(param->type_name->name, "void"))
    {
        goto_next_token(p);
        /* we return early because void parameter function cannot contains other parameters */
        if (!expect(p, ac_token_type_PAREN_R))
        {
            return 0;
        }
        return param;
    }

    /* End of the parameter expression we return early. */
    if (token_is(p, ac_token_type_COMMA)
        || token_is(p, ac_token_type_PAREN_R))
    {
        return param;
    }

    struct ac_ast_declarator* declarator = parse_declarator_for_parameter(p);

    if (!declarator) { return 0; }

    /* at this point parsing declarator is the only option */
    //struct ac_ast_declarator* declarator = parse_declarator(p);
    param->declarator = declarator;

    return param;
}

static int count_and_consume_pointers(struct ac_parser_c* p)
{
    assert(token_is(p, ac_token_type_STAR));

    int i = 0;
    while (token_is(p, ac_token_type_STAR))
    {
        goto_next_token(p); /* skip pointer symbol */
        
        i += 1;
    }

    return i;
}

static struct ac_ast_array_specifier* parse_array_specifier(struct ac_parser_c* p)
{
    assert(token_is(p, ac_token_type_SQUARE_L));

    struct ac_ast_array_specifier* first = 0;
    struct ac_ast_array_specifier* previous = 0;

    while (token_is(p, ac_token_type_SQUARE_L)) /* [ */
    {
        goto_next_token(p); /* Skip [ */
        AST_NEW_CTOR(struct ac_ast_array_specifier, array_specifier, location(p), ac_ast_array_specifier_init);
       
        /* Save the first array specifier, which would need to be returned. */
        if (!first) { first = array_specifier; }

        /* Attached new "dimensions" to the previous array. */
        if (previous)
        {
            previous->next_array = array_specifier;
        }

        /* Reach the ], so there is no no size. Could be valid in some case. Like "void func(int[]);" */
        if (token_is(p, ac_token_type_SQUARE_R))
        {
            AST_NEW_CTOR(struct ac_ast_array_empty_size, array_empty_size, location(p), ac_ast_array_empty_size_init);
            array_specifier->size_expression = to_expr(array_empty_size);

            goto_next_token(p); /* Skip ] */
            /* continue to the next part (array or not) */
            continue;
        }

        struct ac_ast_expr* lhs = 0;
        struct ac_ast_expr* array_size_expr = parse_expr(p, lhs);

        if (!array_size_expr)
        {
            return 0;
        }

        array_specifier->size_expression = array_size_expr;

        if (!expect_and_consume(p, ac_token_type_SQUARE_R))
        {
            return 0;
        }
    }

    return first;
}

static const struct ac_token* token(const struct ac_parser_c* p)
{
    return &p->lex.token;
}

static struct ac_location location(const struct ac_parser_c* p)
{
    return p->lex.location;
}

static void goto_next_token(struct ac_parser_c* p)
{
    ac_lex_goto_next(&p->lex);
}

static enum ac_token_type token_type(const struct ac_parser_c* p)
{
    return p->lex.token.type;
}

static bool token_is(const struct ac_parser_c* p, enum ac_token_type type) {
    return token_equal_type(p->lex.token, type);
}

static bool token_is_not(const struct ac_parser_c* p, enum ac_token_type type)
{
    return !token_is(p, type);
}

static bool token_is_unary_operator(const struct ac_parser_c* p)
{
    switch (p->lex.token.type) {
    case ac_token_type_AMP:
    case ac_token_type_DOT:
    case ac_token_type_EXCLAM:
    case ac_token_type_MINUS:
    case ac_token_type_PLUS:
    case ac_token_type_STAR:
    case ac_token_type_TILDE:
        return true;
    }
    return false;
}

static bool token_equal_type(struct ac_token token, enum ac_token_type type)
{
    return token.type == type;
}

static bool expect(const struct ac_parser_c* p, enum ac_token_type type) {
    struct ac_location current_location = p->lex.location;
    struct ac_token current = p->lex.token;
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

static bool expect_and_consume(struct ac_parser_c* p, enum ac_token_type type)
{
    struct ac_location current_location = p->lex.location;
    struct ac_token current = p->lex.token;
    if (expect(p, type)) {
        goto_next_token(p);

        if (token_is(p, ac_token_type_EOF)
            && (!token_equal_type(current, ac_token_type_SEMI_COLON) && !token_equal_type(current, ac_token_type_BRACE_R)))
        {
            strv current_string = ac_token_to_strv(current);

            ac_report_error_loc(current_location, "Syntax error: unexpected end-of-file after: '%.*s'\n"
                , current_string.size, current_string.data
            );

            return false;
        }
        return true;
    }

    return false;
}

static bool expr_is(struct ac_ast_expr* expr, enum ac_ast_type type)
{
    return expr->type == type;
}

static bool expr_is_statement(struct ac_ast_expr* expr)
{
    /* @TODO handle 'if' here */
    return expr->type == ac_ast_type_EMPTY_STATEMENT
        || expr->type == ac_ast_type_RETURN
        || ac_ast_is_declaration(expr);
}

static struct ac_ast_expr* to_expr(void* any)
{
    return (struct ac_ast_expr*)any;
}

static void add_to_current_block(struct ac_parser_c* p, struct ac_ast_expr* expr)
{
    assert(p && expr);

    ac_expr_list_add(&p->current_block->statements, expr);
}