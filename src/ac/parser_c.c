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
static void parse_statements(struct ac_parser_c* p, ensure_expr_t post_check, const char* message);
static struct ac_ast_expr* parse_expr(struct ac_parser_c* p, struct ac_ast_expr* lhs);
static struct ac_ast_expr* parse_primary(struct ac_parser_c* p);
static struct ac_ast_expr* parse_rhs(struct ac_parser_c* p, struct ac_ast_expr* lhs, int lhs_precedence);
static struct ac_ast_identifier* parse_identifier(struct ac_parser_c* p);
static struct ac_ast_declaration* parse_declaration_list(struct ac_parser_c* p, struct ac_ast_type_specifier* type_specifier, struct ac_ast_identifier* identifier);
static struct ac_ast_declaration* parse_declaration(struct ac_parser_c* p, struct ac_ast_type_specifier* type_specifier, struct ac_ast_identifier* identifier);
static struct ac_ast_expr* parse_block(struct ac_parser_c* p, bool expect_braces);
static void parse_parameters(struct ac_parser_c* p, struct ac_expr_list* head, enum ac_token_type expected_opening_token);
static struct ac_ast_expr* parse_postfix_expression(struct ac_parser_c* p, struct ac_ast_identifier* identifier);
static struct ac_ast_expr* parse_unary(struct ac_parser_c* p);
static struct ac_ast_type_specifier* try_parse_type(struct ac_parser_c* p, struct ac_ast_identifier* identifier);

/* parse function prototype and body if it contains one */
static struct ac_ast_block* parse_function_info(struct ac_parser_c* p);

static const struct ac_token* token(const struct ac_parser_c* p); /* current token type */
static struct ac_location location(const struct ac_parser_c* p);
static void goto_next_token(struct ac_parser_c* p);
static enum ac_token_type token_type(const struct ac_parser_c* p);              /* current token type */
static bool token_is(const struct ac_parser_c* p, enum ac_token_type type);     /* current token is */
static bool token_is_not(const struct ac_parser_c* p, enum ac_token_type type); /* current token is not */
static bool token_is_unary_operator(const struct ac_parser_c* p);               /* current token is an unary operator */
static bool token_equal_type(struct ac_token, enum ac_token_type type);
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

    parse_statements(p, only_declaration, "Top level expressions can only be declarations.\n");

    p->current_block = previous;

    return top_level;
}

static void parse_statements(struct ac_parser_c* p, ensure_expr_t post_check, const char* message)
{
    if_printf(p->options.debug_verbose, "parse_statements\n");

    while (token_is_not(p, ac_token_type_BRACE_R)
        && token_is_not(p, ac_token_type_EOF))
    {
        struct ac_ast_expr* lhs = 0;
        struct ac_ast_expr* expr = parse_expr(p, lhs);

        if (expr == 0)
        {
            return;
        }

        if (!post_check(expr))
        {
            ac_report_error_expr(expr, message);
            return;
        }
    }
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
        case ac_token_type_BRACE_L: { /* parse nested block */
            bool expect_braces = true;
            result = parse_block(p, expect_braces);
            break;
        }
        case ac_token_type_EOF: { /* Error must have been reported by "goto_next_token" or "expect_and_consume" */
            result = 0;
            return result;
        }
        case ac_token_type_IDENTIFIER: { /* <identifier> */
            struct ac_ast_identifier* ident = parse_identifier(p);
            /* can be function call, array access, any kind of declarations etc. */
            /* if not then the identifier itself is returned */
            result = parse_postfix_expression(p, ident);
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
        case ac_token_type_LITERAL_NULL: {
            AST_NEW(struct ac_ast_literal, literal, location(p), ac_ast_type_LITERAL_NULL);
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
        case ac_token_type_RETURN: {
            goto_next_token(p); /* Skip 'return' */
            AST_NEW_CTOR(struct ac_ast_return, ret, location(p), ac_ast_return_init);
           
            struct ac_ast_expr* lhs = 0;
            ret->expr = parse_expr(p, lhs);
            result = to_expr(ret);

            expect_and_consume(p, ac_token_type_SEMI_COLON);
            break;
        }
        case ac_token_type_SEMI_COLON: { /* empty expression */
            AST_NEW(struct ac_ast_expr, empty_statement, location(p), ac_ast_type_EMPTY_STATEMENT);

            goto_next_token(p); /* Skip ';' */

            result = empty_statement;
            break;
        }
        
        default: {
            ac_report_error_loc(location(p), "parse_primary, case note handled %d\n", token_type(p));
            return 0;
        }
        } /* switch (token_type(p)) */
    }

    /* declarations are already handled when they are created, we only need to add the other statements into there parent block */
    if (!ac_ast_is_declaration(result) && expr_is_statement(result))
    {
        add_to_current_block(p, result);
    }

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
    if (!dstr_view_equals_str(identifier->name, "int"))
    {
        ac_report_error_loc(location(p), "This parser can only handle 'int' as type.");
    }

    AST_NEW_CTOR(struct ac_ast_type_specifier, type_specifier, location(p), ac_ast_type_specifier_init);
    type_specifier->identifier = identifier;

    return type_specifier;
}

static struct ac_ast_block* parse_function_info(struct ac_parser_c* p)
{
    AST_NEW_CTOR(struct ac_ast_block, block, location(p), ac_ast_block_init);

    parse_parameters(p, &block->parameters, ac_token_type_PAREN_L);

    /* we expect semi colon ...1 */
    if (token_is(p, ac_token_type_SEMI_COLON))
    {
        expect_and_consume(p, ac_token_type_SEMI_COLON);
        return block;
    }

    
    expect_and_consume(p, ac_token_type_BRACE_L);

    /* ...1 or a body */

    struct ac_ast_block* previous = p->current_block;

    p->current_block = block;

    parse_statements(p, any_expr, "");

    p->current_block = previous;

    expect_and_consume(p, ac_token_type_BRACE_R);

    return block;
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

    expect_and_consume(p, ac_token_type_IDENTIFIER); /* Skip identifier */

    return result;
}

static struct ac_ast_expr* parse_postfix_expression(struct ac_parser_c* p, struct ac_ast_identifier* identifier)
{
    struct ac_ast_expr* previous = CAST(struct ac_ast_expr* , identifier);
    struct ac_ast_expr* parsed;
    
    /* Maybe the identifier is a "long" then we should look for "long long" or similar.
       Maybe the identifier is a previously defined type (typedef, enum or struct)
    */
    struct ac_ast_type_specifier* parsed_type = try_parse_type(p, identifier);

    do {
        parsed = 0;
        switch (token_type(p)) {
        case ac_token_type_PAREN_L: { /* parse function call */
            /* @TODO */
            /* struct ac_ast_function_call* function_call = parse_function_call(p, previous); */
            /* parsed = function_call; */
            break;
        }
        /* Parse array like <previous-expression>[<expression-for-index>] */
        case ac_token_type_SQUARE_L: {  /* parse array access */
            /* @TODO */
            /*goto_next_token(p);*/ /* skip [ */
            /*AST_NEW(struct ac_ast_array_access, array_access, location(p), ac_ast_type_ARRAY_ACCESS); */
            /*array_access->left = previous; */
            /*array_access->index_expression = parse_expression(p); */
            
            /*if (!array_access->index_expression) return false; */
            /*if (!expect_and_consume(p, token_type_SquareR)) return false; */

            /*parsed = array_access; */
            break;
        }
        case ac_token_type_DOT: { /* parse member access */
            /* goto_next_token(p); */ /* skip [ */
            /* AST_NEW(struct ac_ast_member_access, array_access, location(p), ac_ast_type_MEMBER_ACCESS); */
            /* member_access->left = previous; */
            /* member_access->identifier = parse_identifier(p);*/

            /* if (!member_access->identifier) return false; */

            /*parsed = member_access; */
            break;
        }
      
       
        case ac_token_type_IDENTIFIER: { /* parse declaration */

            /* If previous expression was a type, and this one is a identifier, then we should parse a declaration.
            */
            if (parsed_type)
            {
                struct ac_ast_identifier* current_identifier = parse_identifier(p);
        
                struct ac_ast_declaration* expr = parse_declaration_list(p, parsed_type, current_identifier);
                if (!expr) return 0;
                
                return to_expr(expr);
            }
            else
            {
                assert("@FIXME not sure what to do here");
            }
           
        }
        default: /* do nothing */
        {}
        } /* end of switch (token_type(p)) */

        /* only update previous if new has been parsed. */
        previous = parsed ? parsed : previous;
    } while (parsed);

    return previous;
}

/* Example:
   case 1a: <'type'> value;
   case 1b: <'type'> value = 0;
   case 2a: <'type'> value, value2;
   case 2b: <'type'> value = 0, value2 = 0;
   case 3a: <'type'> forward_function_declaration();
   case 3b: <'type'> function_declaration() { ... }
*/
static struct ac_ast_declaration* parse_declaration_list(struct ac_parser_c* p, struct ac_ast_type_specifier* type_specifier, struct ac_ast_identifier* identifier)
{
    /* case 1 */
    struct ac_ast_declaration* declaration = parse_declaration(p, type_specifier, identifier);
    add_to_current_block(p, to_expr(declaration));

    /* case 2 */
    while (token_is(p, ac_token_type_COMMA))  /* , */
    {
        expect_and_consume(p, ac_token_type_COMMA);

        // @TODO handle pointer and array, we actually need to expect something like "identifier" or "identifier[0]" or "idenitifer()" or "* identifier"
        // so maybe we should just accept any expression and deal with the syntax at the semantic part.
        if (token_is_not(p, ac_token_type_IDENTIFIER))
        {
            ac_report_error_loc(location(p), "expect identifier after the comma in a declaration list.");
        }

        struct ac_ast_identifier* next_identifier = parse_identifier(p);

        struct ac_ast_expr* next_identifier_expr = to_expr(next_identifier);
        struct ac_ast_declaration* inner_declaration = parse_declaration(p, type_specifier, next_identifier);
        
        assert(inner_declaration);

        if (!inner_declaration)
        {
            ac_report_error_expr(next_identifier_expr, "internal error: could not parse declaration after identifier.");
        }

        add_to_current_block(p, to_expr(inner_declaration));
    }

    /* case 3 */
    if (token_is(p, ac_token_type_PAREN_L)) /* ( */
    {
        declaration->function_block = parse_function_info(p);

        assert(declaration->function_block);

        if (declaration->function_block)
        {
            declaration->type = ac_ast_type_DECLARATION_FUNCTION_DEFINITION;
            /* we return early here because function definition do not require any semi colon */
            return declaration;
        }
    }

    /* once we reach this that mean all other cases have been handled */
    if (expect_and_consume(p, ac_token_type_SEMI_COLON)) /* ; */
    {
        return declaration;
    }

    return 0;
}

/* Example:
   case 1a: <'type'> value;
   case 1b: <'type'> value = 0;
*/
static struct ac_ast_declaration* parse_declaration(struct ac_parser_c* p, struct ac_ast_type_specifier* type_specifier, struct ac_ast_identifier* identifier)
{
    AST_NEW_CTOR(struct ac_ast_declaration, declaration, location(p), ac_ast_declaration_init);
    declaration->type_specifier = type_specifier;
    declaration->ident = identifier;

    if (token_is(p, ac_token_type_EQUAL))  /* = */
    {
        expect_and_consume(p, ac_token_type_EQUAL);
        declaration->initializer = parse_expr(p, 0);
    }

    return declaration;
}

struct ac_ast_expr* parse_block(struct ac_parser_c* p, bool expect_braces) {

    (void)p;
    (void)expect_braces;
    return 0;

    //if_printf(p->options.debug_verbose, "parse_block_body\n");
    //
    //AST_NEW(struct ac_ast_block, block, location(p), ac_ast_type_BLOCK);
    //
    //if (expect_braces && !expect_and_consume(p, ac_token_type_BRACE_L)) return false;
    //
    ///* empty block no need to do anything else */
    //if (expect_braces && token_is(p, ac_token_type_BRACE_R))
    //{
    //    goto_next_token(p);
    //    return true;
    //}
    //
    //// Loop parsing one top level item
    //while (!token_is(p, ac_token_type_BRACE_R)
    //    && !token_is(p, ac_token_type_EOF))
    //{
    //    struct ac_ast_expr* expr = parse_statement(p);
    //
    //    if (!expr) return false;
    //
    //    block->statements.push_back(expression);
    //}
    //
    //if (expect_braces && !expect_and_consume(p, ac_token_type_BRACE_R)) return false;
    //
    //return true;

} 

static void parse_parameters(struct ac_parser_c* p, struct ac_expr_list* head, enum ac_token_type expected_opening_token)
{
    /* only accept [ or ( */
    assert(expected_opening_token == ac_token_type_PAREN_L || expected_opening_token || ac_token_type_SQUARE_L);

    (void)head;

    if_printf(p->options.debug_verbose, "parse_parameters\n");

   
    if (!expect_and_consume(p, expected_opening_token))
    {
        ac_report_error_loc(location(p), "parameters should start with '%.*s'.\n", ac_token_type_to_strv(expected_opening_token));
        return;
    }

    enum ac_token_type expected_closing_token = expected_opening_token == ac_token_type_PAREN_L ? ac_token_type_PAREN_R : ac_token_type_SQUARE_R;

    // @TODO handle parameters

    if (!expect_and_consume(p, expected_closing_token))
    {
        ac_report_error_loc(location(p), "parameters should end with parenthesis or square brackets.\n");
        return;
    }
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
        dstr_view expected = ac_token_type_to_strv(type);
        dstr_view actual = ac_token_to_strv(current);

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
            dstr_view current_string = ac_token_to_strv(current);

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