#include "parser_c.h"

#include "ast.h"
#include "global.h"
#include "lexer.h"

#define CAST(type_, object_) (type_)(object_)

#define AST_NEW(p, type_, ident_, location_, ast_type_) \
        type_* ident_ = (type_*)allocate(p, sizeof(type_)); \
        do { \
            ident_->type = ast_type_; \
            ident_->loc = location_; \
        } while (0);

#define AST_NEW_CTOR(p, type_, ident_, location_, constructor_) \
        type_* ident_ = (type_*)allocate(p, sizeof(type_)); \
        do { \
            constructor_(ident_); \
            ident_->loc = location_; \
        } while (0);
        
#define if_printf(condition_, ...) do { if(condition_) printf(__VA_ARGS__); } while(0);

static int LOWEST_PRIORITY_PRECEDENCE = 999;

typedef bool (*ensure_expr_t)(ac_ast_expr* expr);

static void* allocate(ac_parser_c * p, size_t byte_size);
static ac_options* options(ac_parser_c* p);

static ac_ast_top_level* parse_top_level(ac_parser_c* p);
static int get_precedence_if_binary_op(enum ac_token_type type);
static ac_ast_expr* parse_expr(ac_parser_c* p, int previous_precedence);
static ac_ast_expr* parse_primary(ac_parser_c* p);
static ac_ast_block* parse_block(ac_parser_c* p);
static ac_ast_block* parse_block_or_inline_block(ac_parser_c* p);
static bool parse_statements(ac_parser_c* p, ensure_expr_t post_check, const char* message);
static ac_ast_expr* parse_statement(ac_parser_c* p);
static ac_ast_identifier* parse_identifier(ac_parser_c* p);

static ac_ast_type_specifier* parse_type_specifier(ac_parser_c * p);
static ac_ast_type_specifier* multiple_type_specifier_error(ac_parser_c * p, enum ac_token_type left, enum ac_token_type right);
static ac_ast_type_specifier* duplicate_type_specifier_warning(ac_parser_c * p, enum ac_token_type type);
static ac_ast_type_specifier* cannot_combine_error(ac_parser_c * p, enum ac_token_type left, enum ac_token_type type);

static ac_ast_declaration* parse_declaration_list(ac_parser_c* p, ac_ast_type_specifier* type_specifier);
static ac_ast_declaration* make_simple_declaration(ac_parser_c* p, ac_ast_type_specifier* type_specifier, ac_ast_declarator* declarator);
static ac_ast_declaration* make_function_declaration(ac_parser_c* p, ac_ast_type_specifier* type_specifier, ac_ast_declarator* declarator, ac_ast_block* block);
static ac_ast_declarator* parse_declarator_core(ac_parser_c* p, bool identifier_required);
static ac_ast_declarator* parse_declarator_for_parameter(ac_parser_c* p);
static ac_ast_declarator* parse_declarator(ac_parser_c* p);
static ac_ast_parameters* parse_parameter_list(ac_parser_c* p, enum ac_token_type expected_opening_token);
static ac_ast_parameter* parse_parameter(ac_parser_c* p);
static int count_and_consume_pointers(ac_parser_c* p);
static ac_ast_array_specifier* parse_array_specifier(ac_parser_c* p);
static ac_ast_expr* parse_unary(ac_parser_c* p);

static bool is_basic_type_or_identifier(enum ac_token_type type);
static bool is_leading_declaration(enum ac_token_type type);

static ac_token token(const ac_parser_c* p); /* Current token by value. */
static const ac_token* token_ptr(const ac_parser_c * p);  /* Current token by pointer. */
static ac_location location(const ac_parser_c* p);
static void goto_next_token(ac_parser_c* p);
static enum ac_token_type token_type(const ac_parser_c* p);              /* current token type */
static bool token_is(const ac_parser_c* p, enum ac_token_type type);     /* current token is */
static bool token_is_not(const ac_parser_c* p, enum ac_token_type type); /* current token is not */
static bool token_is_unary_operator(const ac_parser_c* p);               /* current token is an unary operator */
static bool token_equal_type(ac_token, enum ac_token_type type);
static bool expect(ac_parser_c* p, enum ac_token_type type);
static bool expect_and_consume(ac_parser_c* p, enum ac_token_type type);
static bool consume_if(ac_parser_c * p, bool value);

static bool expr_is(ac_ast_expr* expr, enum ac_ast_type type);

static bool expr_is_statement(ac_ast_expr* expr);
static ac_ast_expr* to_expr(void* any);
static void add_to_current_block(ac_parser_c* p, ac_ast_expr* expr);

static bool only_declaration(ac_ast_expr* expr) { return ac_ast_is_declaration(expr); }
static bool any_expr(ac_ast_expr* expr) { (void)expr; return true; }

void ac_parser_c_init(ac_parser_c* p, ac_manager* mgr, strv content, strv filepath)
{
    memset(p, 0, sizeof(ac_parser_c));

    ac_pp_init(&p->pp, mgr, content, filepath);
    p->mgr = mgr;
}

void ac_parser_c_destroy(ac_parser_c* p)
{
    ac_pp_destroy(&p->pp);
}

bool ac_parser_c_parse(ac_parser_c* p)
{
    /* At the point the lexer is not initialized yet. Go to the first token. */
    goto_next_token(p);

    ac_ast_top_level* top_level = parse_top_level(p);
    p->mgr->top_level = top_level;

    return top_level != 0;
}

static void* allocate(ac_parser_c* p, size_t byte_size)
{
    return ac_allocator_allocate(&p->mgr->ast_arena.allocator, byte_size);
}

static ac_options* options(ac_parser_c* p)
{
    return &p->mgr->options;
}

static ac_ast_top_level* parse_top_level(ac_parser_c* p)
{
    AST_NEW_CTOR(p, ac_ast_top_level, top_level, location(p), ac_ast_top_level_init);

    ac_ast_block* previous = p->current_block;

    p->current_block = &top_level->block;

    if (!parse_statements(p, only_declaration, "top level expressions can only be declarations.\n"))
    {
        return 0;
    }

    p->current_block = previous;

    return top_level;
}

static int get_precedence_if_binary_op(enum ac_token_type type)
{
    switch (type) {
    case ac_token_type_PERCENT: /* Remainder. */
    case ac_token_type_SLASH:   /* Division. */
    case ac_token_type_STAR:    /* Multiplication. */
        return 50;
    case ac_token_type_MINUS:
    case ac_token_type_PLUS:
        return 60;
    case ac_token_type_DOUBLE_LESS:     /* Bitwise left shift. */
    case ac_token_type_DOUBLE_GREATER:  /* Bitwise right shift. */
        return 70;
    case ac_token_type_GREATER:
    case ac_token_type_GREATER_EQUAL:
    case ac_token_type_LESS:
    case ac_token_type_LESS_EQUAL:
        return 90;
    case ac_token_type_DOUBLE_EQUAL:
    case ac_token_type_NOT_EQUAL:
        return 100;
    case ac_token_type_AMP:   /* Bitwise AND */
        return 110;
    case ac_token_type_CARET: /* Bitwise XOR */
        return 120;
    case ac_token_type_PIPE:  /* Bitwise OR */
        return 130;
    case ac_token_type_DOUBLE_AMP:  /* Logicel AND */
        return 140;
    case ac_token_type_DOUBLE_PIPE: /* Logicel OR */
        return 150;
    case ac_token_type_EQUAL:
    case ac_token_type_CARET_EQUAL:
    case ac_token_type_MINUS_EQUAL:
    case ac_token_type_PERCENT_EQUAL:
    case ac_token_type_PLUS_EQUAL:
    case ac_token_type_SLASH_EQUAL:
    case ac_token_type_STAR_EQUAL:
        return 160;
    default:
        return LOWEST_PRIORITY_PRECEDENCE;
    }
}

/*
    The following algorithm to parse an expression is a reimplementation of what Jonathan Blow and Casey Muratori
    have been discussing in this video: https://www.youtube.com/watch?v=fIPO4G42wYE

    Here is an interpretation considering those examples:
    
    A: 1 + 2 + 3 + 4
    B: 1 + 2 * 3 + 4
    C: 1 * 2 + 3 + 4

    We take the first primary expression "1" as left expression, check if there is a binary operator right after.
    If so we need to combine this "1" with the result a following expression since that the purpose a binary operator.
    The result of following expression can be known depending on the precendences of the previous operator and next operator.

    A: Since the second "+" is equal precedence to the first "+" we can can combine "2 + 3" we would become the expression next to "1 +"
    B: Is the same case as A. Since "*" is higher priority we an also combine "2 * 3" and can also use it as right node for "1 +".
    C: The second operator is a "+" and is lower priority over "*", hence we return only "2" as right node for "1 *".

    In all cases we continue combine those new nodes until there is no remaining operators.
*/
static ac_ast_expr* parse_expr(ac_parser_c* p, int previous_precedence)
{
    ac_ast_expr* left = parse_primary(p);

    if (left == NULL)
    {
        return NULL;
    }

    while (true) /* Left can be NULL if */
    {
        enum ac_token_type new_token_type = token(p).type;
        int new_precedence = get_precedence_if_binary_op(new_token_type);

        /* While the next token is a binary operator and if the new precedence is higher priority,
           We need to combine the left expression with a following expression one. */
        if (new_precedence < previous_precedence)
        {
            goto_next_token(p); /* Skip binary op. */

            AST_NEW_CTOR(p, ac_ast_binary, binary, location(p), ac_ast_binary_init);
            binary->op = new_token_type;
            binary->left = left;

            /* Get the right expression. */
            ac_ast_expr* right = parse_expr(p, new_precedence);
            if (right == NULL)
            {
                return NULL;
            }
            binary->right = right;

            /* Left node has been merged with next expression so it became the left node itself and we continue to try to combine.
               The combination above could have been ended if the prioritt was lower than the previous one. */
            left = to_expr(binary);
        }
        else
        {
            return left;
        }
    }
}

/* A primary expression can be a constant/literal, an identifier or _Generic. */
static ac_ast_expr* parse_primary(ac_parser_c* p)
{
    if_printf(options(p)->debug_parser, "parse_primary\n");

    ac_ast_expr* result = 0;

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
        case ac_token_type_FALSE: {
            AST_NEW(p, ac_ast_literal, literal, location(p), ac_ast_type_LITERAL_BOOL);
            literal->token = token(p);
            result = to_expr(literal);
            goto_next_token(p);
            break;
        }
        case ac_token_type_IDENTIFIER: { /* <identifier> */
            ac_ast_identifier* ident = parse_identifier(p);
            /* @TODO create parse_postfix_expression() to handle function call or array access */
            result = to_expr(ident);
            break;
        }
        case ac_token_type_LITERAL_CHAR: {
            AST_NEW(p, ac_ast_literal, literal, location(p), ac_ast_type_LITERAL_CHAR);
            literal->token = token(p);
            result = to_expr(literal);
            goto_next_token(p);
            break;
        }
        case ac_token_type_LITERAL_FLOAT: {
            AST_NEW(p, ac_ast_literal, literal, location(p), ac_ast_type_LITERAL_FLOAT);
            literal->token = token(p);
            result = to_expr(literal);
            goto_next_token(p);
            break;
        }
        case ac_token_type_LITERAL_INTEGER: { 
            AST_NEW(p, ac_ast_literal, literal, location(p), ac_ast_type_LITERAL_INTEGER);
            literal->token = token(p);
            result = to_expr(literal);
            goto_next_token(p);
            break;
        }
        case ac_token_type_LITERAL_STRING: {
            AST_NEW(p, ac_ast_literal, literal, location(p), ac_ast_type_LITERAL_STRING);
            literal->token = token(p);
            result = to_expr(literal);
            goto_next_token(p);
            break;
        }
        case ac_token_type_TRUE: {
            AST_NEW(p, ac_ast_literal, literal, location(p), ac_ast_type_LITERAL_BOOL);
            literal->token = token(p);
            result = to_expr(literal);
            goto_next_token(p);
            break;
        }
        case ac_token_type_GENERIC: {
            ac_report_internal_error_loc(location(p), "_Generic has not been implemented yet");
            return NULL;
            break;
        }
        /* Special macro*/
        case ac_token_type__FUNC__:
        case ac_token_type__FUNCTION__:
        case ac_token_type__PRETTY_FUNCTION__:
        {
            /* Similar code as for ac_token_type_LITERAL_STRING but we use the current function name. */
            AST_NEW(p, ac_ast_literal, literal, location(p), ac_ast_type_LITERAL_STRING);
            literal->token.type = ac_token_type_LITERAL_STRING;
            literal->token.text = ac_create_or_reuse_literal(p->mgr, p->current_function_name);
            result = to_expr(literal);
            goto_next_token(p);
            break;
        }
        default: {
            ac_report_internal_error_loc(location(p), "parse_primary, case not handled %d\n", token_type(p));
            return 0;
        }
        } /* switch (token_type(p)) */
    }

    return result;
}

static ac_ast_block* parse_block(ac_parser_c* p)
{
    if_printf(options(p)->debug_parser, "parse_block\n");

    AST_NEW_CTOR(p, ac_ast_block, block, location(p), ac_ast_block_init);

    if (!expect_and_consume(p, ac_token_type_BRACE_L))
    {
        return 0;
    }

    ac_ast_block* previous = p->current_block;

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

static ac_ast_block* parse_block_or_inline_block(ac_parser_c* p)
{
    if (token_is_not(p, ac_token_type_BRACE_L))
    {
        AST_NEW_CTOR(p, ac_ast_block, block, location(p), ac_ast_block_init);
        ac_ast_block* previous = p->current_block;

        p->current_block = block;

        ac_ast_expr* statement = parse_statement(p);
        if (!statement)
        {
            return 0;
        }

        p->current_block = previous;
        return block;
    }

    return parse_block(p);
}

static bool parse_statements(ac_parser_c* p, ensure_expr_t post_check, const char* message)
{
    if_printf(options(p)->debug_parser, "parse_statements\n");

    /* no statement if there is already a closing brace */
    if (token_is(p, ac_token_type_BRACE_R))
    {
        return true;
    }

    /* if last expression is not null this means everything was alright */
    ac_ast_expr* expr = 0;
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

static ac_ast_expr* parse_statement(ac_parser_c* p)
{
    if_printf(options(p)->debug_parser, "parse_statement\n");

    ac_ast_expr* result = 0;

    ac_token_type type = token_type(p);
    switch (type) {
    case ac_token_type_BRACE_L: { /* parse nested block statement */

        ac_report_internal_error_loc(location(p), "nested block not handled yet, case not handled %d", type);
        return 0;
    }
    case ac_token_type_RETURN: {
        goto_next_token(p); /* Skip 'return' */
        AST_NEW_CTOR(p, ac_ast_return, ret, location(p), ac_ast_return_init);

        ret->expr = parse_expr(p, LOWEST_PRIORITY_PRECEDENCE);
        result = to_expr(ret);

        if (!expect_and_consume(p, ac_token_type_SEMI_COLON))
        {
            return 0;
        }
        break;
    }
    case ac_token_type_SEMI_COLON: { /* empty expression */
        AST_NEW(p, ac_ast_expr, empty_statement, location(p), ac_ast_type_EMPTY_STATEMENT);

        goto_next_token(p); /* Skip ';' */

        result = empty_statement;
        break;
    }

    default: {
      
        /* Built-in type, struct or typedef */
        ac_ast_type_specifier* type_specifier = parse_type_specifier(p);

        if (!type_specifier) { return 0; }

        ac_ast_expr* declaration = to_expr(parse_declaration_list(p, type_specifier));

        if (!declaration) { return 0; }

        AC_ASSERT(ac_ast_is_declaration(declaration));

        /* All declarations (exception function definition) requires a trailing semi-colon. */
        if (declaration->type != ac_ast_type_DECLARATION_FUNCTION_DEFINITION)
        {
            if (!expect_and_consume(p, ac_token_type_SEMI_COLON))
            {
                return 0;
            }
        }

        return declaration;
    }
    }

    add_to_current_block(p, to_expr(result));

    return result;
}

static ac_ast_expr* parse_unary(ac_parser_c* p) {
    if_printf(options(p)->debug_parser, "parse_unary\n");

    assert(token_is_unary_operator(p));

    AST_NEW_CTOR(p,ac_ast_unary, unary, location(p), ac_ast_unary_init);
    unary->op = token_type(p);

    goto_next_token(p); /* skip current unary token */

    unary->operand = parse_primary(p);

    return to_expr(unary);
}

static ac_ast_identifier* parse_identifier(ac_parser_c* p) {

    AC_ASSERT(ac_token_is_keyword_or_identifier(token(p).type));

    AST_NEW(p, ac_ast_identifier, result, location(p), ac_ast_type_IDENTIFIER);
    result->name = token(p).ident->text;

    goto_next_token(p); /* skip identifier */

    return result;
}

static ac_ast_type_specifier* parse_type_specifier(ac_parser_c* p)
{
    ac_token_type type = token_type(p);

    if (!is_leading_declaration(type))
    {
        ac_report_internal_error_loc(location(p), "Invalid start of type specifier, this must be handled earlier.");
        return NULL;
    }

    AST_NEW_CTOR(p, ac_ast_type_specifier, ts, location(p), ac_ast_type_specifier_init);
    
    bool another_one = true;
    while(another_one)
    {
        ac_token_type type = token_type(p);
        switch (type)
        {
       
        case ac_token_type_BOOL:
        case ac_token_type_CHAR:
        case ac_token_type_DOUBLE:
        case ac_token_type_FLOAT:
        case ac_token_type_INT:
        case ac_token_type_VOID:
        {
            if (ts->type_specifier != ac_token_type_NONE)
            {
                return multiple_type_specifier_error(p, ts->type_specifier, type);
            }

            ts->type_specifier = type;
            break;
        }

        case ac_token_type_AUTO:
        case ac_token_type_EXTERN:
        case ac_token_type_REGISTER:
        case ac_token_type_STATIC:
        case ac_token_type_ATOMIC:
        case ac_token_type_THREAD_LOCAL:
        case ac_token_type_THREAD_LOCAL2:
        case ac_token_type_INLINE:
        case ac_token_type_CONST:
        case ac_token_type_VOLATILE: {

            enum ac_specifier spec = ac_specifier_NONE;
            switch (type)
            {
         
            case ac_token_type_AUTO: spec = ac_specifier_AUTO; break;
            case ac_token_type_EXTERN: spec = ac_specifier_EXTERN; break;
            case ac_token_type_REGISTER: spec = ac_specifier_REGISTER; break;
            case ac_token_type_STATIC: spec = ac_specifier_STATIC; break;

            case ac_token_type_ATOMIC: {
                ac_report_error_loc(location(p), "'atomic' is not supported");
                return NULL;
            }
            case ac_token_type_THREAD_LOCAL:
            case ac_token_type_THREAD_LOCAL2: {
                ac_report_error_loc(location(p), "'thread_local' is not supported");
                return NULL;
            }
            case ac_token_type_INLINE: spec = ac_specifier_INLINE; break;
            case ac_token_type_CONST: spec = ac_specifier_CONST; break;
            case ac_token_type_VOLATILE: spec = ac_specifier_VOLATILE; break;

            }

            if (ts->specifiers & spec)
            {
                return duplicate_type_specifier_warning(p, type);
            }

            ts->specifiers |= spec;

            break;
        }

        /* Special case for signed and unsigned. */
        case ac_token_type_SIGNED:
        case ac_token_type_UNSIGNED: {

            if (type == ac_token_type_SIGNED && (ts->specifiers & ac_specifier_UNSIGNED))
                return cannot_combine_error(p, ac_token_type_SIGNED, ac_token_type_UNSIGNED);
            else if (type == ac_token_type_UNSIGNED && (ts->specifiers & ac_specifier_SIGNED))
                return cannot_combine_error(p, ac_token_type_UNSIGNED, ac_token_type_SIGNED);

            enum ac_specifier spec = ac_specifier_NONE;
            switch (type)
            {
            case ac_token_type_SIGNED: spec = ac_specifier_SIGNED; break;
            case ac_token_type_UNSIGNED: spec = ac_specifier_UNSIGNED; break;
            }
            ts->specifiers |= spec;
            break;
        }
        case ac_token_type_SHORT:
        {
            if (ts->specifiers & ac_specifier_SHORT)
            {
                return duplicate_type_specifier_warning(p, type);
            }

            ts->specifiers |= ac_specifier_SHORT;
            break;
        }
        /* Special case for long since it can be stacked twice. */
        case ac_token_type_LONG:
        {
            if (ts->specifiers & ac_specifier_LONG)
            {
                ts->specifiers &= ~ac_specifier_LONG;
                ts->specifiers |= ac_specifier_LONG_LONG;

            }
            else if (ts->specifiers & ac_specifier_LONG_LONG)
            {
                ac_report_error_loc(location(p), "too many 'long' specifier");
                return NULL;
            }
            else
            {
                ts->specifiers |= ac_specifier_LONG;
            }
          
            break;
        }
      
        case ac_token_type_ENUM:
        case ac_token_type_STRUCT:
        case ac_token_type_TYPEDEF:
        case ac_token_type_UNION:
        {
            if (ts->type_specifier != ac_token_type_NONE)
            {
                return multiple_type_specifier_error(p, ts->type_specifier, type);
            }

            ac_report_internal_error("parse_type_specifier: '%s' not handled yet", ac_token_type_to_str(type));
            ts->type_specifier = type;
            break;
        }
        case ac_token_type_IDENTIFIER:
        default:

            if (ts->type_specifier == ac_specifier_NONE)
            {
                if (ts->specifiers & ac_specifier_LONG_LONG
                    || ts->specifiers & ac_specifier_LONG
                    || ts->specifiers & ac_specifier_SHORT
                    || ts->specifiers & ac_specifier_UNSIGNED
                    || ts->specifiers & ac_specifier_SIGNED)
                {
                    ts->type_specifier = ac_token_type_INT;
                }
            }

            if (ts->type_specifier == ac_token_type_NONE && !(ts->specifiers & ac_specifier_AUTO))
            {
                ac_report_error_loc(location(p), "missing type specifier");
                return NULL;
            }
            return ts;
        }

        goto_next_token(p);
    }

    AC_ASSERT(0 && "Unreachable");
    return NULL;
}

static ac_ast_type_specifier* multiple_type_specifier_error(ac_parser_c* p, enum ac_token_type left, enum ac_token_type right)
{
    ac_report_error_loc(location(p), "invalid declaration with multiple type specifiers: '%s' and '%s'", ac_token_type_to_str(left), ac_token_type_to_str(right));

    return NULL;
}

static ac_ast_type_specifier* duplicate_type_specifier_warning(ac_parser_c* p, enum ac_token_type type)
{
    ac_report_warning_loc(location(p), "duplicate specifiers used: '%s'", ac_token_type_to_str(type));

    return NULL;
}
static ac_ast_type_specifier* cannot_combine_error(ac_parser_c* p, enum ac_token_type left, enum ac_token_type type)
{
    ac_report_error_loc(location(p), "cannot combine '%s' and '%s'", ac_token_type_to_str(left), ac_token_type_to_str(type));

    return NULL;
}


static ac_ast_declaration* parse_declaration_list(ac_parser_c* p, ac_ast_type_specifier* type_specifier)
{
    ac_ast_declaration* last_declaration = 0;

    ac_ast_declarator* declarator = parse_declarator(p);

    if (!declarator) { return 0; }

    /* if the declarator match the function prototype "ident(a,b,c)" we check if there is a function body, if that's the case we handle  */
    if (declarator->pointer_depth == 0
        && declarator->parameters
        && declarator->array_specifier == 0
        && token_is(p, ac_token_type_BRACE_L))
    {
        p->current_function_name = declarator->ident->name;
        
        ac_ast_block* block = parse_block(p);
        if (!block)
        {
            return 0;
        }
        ac_ast_declaration* declaration = make_function_declaration(p, type_specifier, declarator, block);
        
        p->current_function_name = (strv)STRV("");

        return declaration;
    }

    last_declaration = make_simple_declaration(p, type_specifier, declarator);

    while (token_is(p, ac_token_type_COMMA))  /* , */
    {
        goto_next_token(p); /* skip , */

        ac_ast_declarator* next_declarator = parse_declarator(p);

        /* for each new declarator we materialize a separated delcaration, this way "int a, b;" becomes "int a; int b;" */
        last_declaration = make_simple_declaration(p, type_specifier, next_declarator);

        if (!last_declaration) { return 0; }
    }   

    /* we only return the last declaration to mean that everything want alright. */
    return last_declaration;
}

static ac_ast_declaration* make_simple_declaration(ac_parser_c* p, ac_ast_type_specifier* type_specifier, ac_ast_declarator* declarator)
{
    AST_NEW_CTOR(p, ac_ast_declaration, declaration, location(p), ac_ast_declaration_init);
    declaration->type = ac_ast_type_DECLARATION_SIMPLE;
    declaration->type_specifier = type_specifier;
    declaration->declarator = declarator;

    add_to_current_block(p, to_expr(declaration));

    return declaration;
}

static ac_ast_declaration* make_function_declaration(ac_parser_c* p, ac_ast_type_specifier* type_specifier, ac_ast_declarator* declarator, ac_ast_block* block)
{
    ac_ast_declaration* decl = make_simple_declaration(p, type_specifier, declarator);
    decl->type = ac_ast_type_DECLARATION_FUNCTION_DEFINITION;
    decl->function_block = block;
    return decl;
}

static ac_ast_declarator* parse_declarator_core(ac_parser_c* p, bool from_declaration)
{
    AST_NEW_CTOR(p, ac_ast_declarator, declarator, location(p), ac_ast_declarator_init);

    if (token_is(p, ac_token_type_STAR))
    {
        declarator->pointer_depth = count_and_consume_pointers(p);
    }

    if (token_is(p, ac_token_type_RESTRICT))
    {
        goto_next_token(p); /* skip 'restrict' */
        declarator->is_restrict = true;
    }

    /* Declarator from declaration must contain an identifier */
    if (from_declaration && !token_is(p, ac_token_type_IDENTIFIER))
    {
        ac_report_error_loc(location(p), "declaration needs an identifier");
        return NULL;
    }

    if (token_is(p, ac_token_type_IDENTIFIER))
    {
        declarator->ident = parse_identifier(p);
    }

    /* check for optional array specifier */
    while (token_is(p, ac_token_type_SQUARE_L)) /* [ */
    {
        ac_ast_array_specifier* array_specifier = parse_array_specifier(p);
        if (!array_specifier) { return 0; }

        declarator->array_specifier = array_specifier;
    }

    if (token_is(p, ac_token_type_EQUAL)) /* = */
    {
        goto_next_token(p); /* skip = */
        declarator->initializer = parse_expr(p, LOWEST_PRIORITY_PRECEDENCE);
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

static ac_ast_declarator* parse_declarator(ac_parser_c* p)
{
    bool from_declaration = true;
    ac_ast_declarator* declarator = parse_declarator_core(p, from_declaration);

    if (!declarator) { return 0; }

    assert(declarator->ident);

    return declarator;
}

static ac_ast_declarator* parse_declarator_for_parameter(ac_parser_c* p)
{
    bool from_declaration = false;
    ac_ast_declarator* declarator = parse_declarator_core(p, from_declaration);

    if (!declarator) { return 0; }

    /* There must be at least one of an identifier, or a pointer, or an array specifier */
    assert(declarator->ident || declarator->pointer_depth || declarator->array_specifier);

    return declarator;
}

static ac_ast_parameters* parse_parameter_list(ac_parser_c* p, enum ac_token_type expected_opening_token)
{
    if_printf(options(p)->debug_parser, "parse_parameter_list\n");

    AST_NEW_CTOR(p, ac_ast_parameters, parameters, location(p), ac_ast_parameters_init);

    /* only accept [ or ( */
    assert(expected_opening_token == ac_token_type_PAREN_L
        || expected_opening_token == ac_token_type_SQUARE_L);

    if (!expect_and_consume(p, expected_opening_token))
    {
        ac_report_error_loc(location(p), "parameters should start with '%.*s'", ac_token_type_to_strv(expected_opening_token));
        return 0;
    }

    enum ac_token_type expected_closing_token = expected_opening_token == ac_token_type_PAREN_L ? ac_token_type_PAREN_R : ac_token_type_SQUARE_R;

    if (consume_if(p, token_is(p, expected_closing_token)))
    {
        return parameters;
    }

    do
    {
        ac_ast_parameter* param = parse_parameter(p);

        if (param == 0)
        {
            return 0;
        }

        ac_expr_list_add(&parameters->list, to_expr(param));
        /* @TODO use consume_if here. */
    } while (token_is(p, ac_token_type_COMMA)
        && expect_and_consume(p, ac_token_type_COMMA));

    if (!expect_and_consume(p, expected_closing_token))
    {
        ac_report_error_loc(location(p), "parameters should end with parenthesis or square brackets");
        return 0;
    }

    return parameters;
}

static ac_ast_parameter* parse_parameter(ac_parser_c* p)
{
    if_printf(options(p)->debug_parser, "parse_parameter\n");

    AST_NEW_CTOR(p, ac_ast_parameter, param, location(p), ac_ast_parameter_init);

    if (token_is(p, ac_token_type_TRIPLE_DOT))
    {
        ac_report_internal_error_loc(location(p), "var args are not supported yet");
        return param;
    }

    ac_ast_type_specifier* type_specifier = parse_type_specifier(p);

    if (!type_specifier)
    {
        return NULL;
    }

    param->type_specifier = type_specifier;

    /* End of the parameter expression we return early. */
    if (token_is(p, ac_token_type_COMMA)
        || token_is(p, ac_token_type_PAREN_R))
    {
        return param;
    }

    ac_ast_declarator* declarator = parse_declarator_for_parameter(p);

    if (!declarator)
    {
        return NULL;
    }

    param->declarator = declarator;

    return param;
}

static int count_and_consume_pointers(ac_parser_c* p)
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

static ac_ast_array_specifier* parse_array_specifier(ac_parser_c* p)
{
    assert(token_is(p, ac_token_type_SQUARE_L));

    ac_ast_array_specifier* first = 0;
    ac_ast_array_specifier* previous = 0;

    while (token_is(p, ac_token_type_SQUARE_L)) /* [ */
    {
        goto_next_token(p); /* Skip [ */
        AST_NEW_CTOR(p, ac_ast_array_specifier, array_specifier, location(p), ac_ast_array_specifier_init);
       
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
            AST_NEW_CTOR(p, ac_ast_array_empty_size, array_empty_size, location(p), ac_ast_array_empty_size_init);
            array_specifier->size_expression = to_expr(array_empty_size);

            goto_next_token(p); /* Skip ] */
            /* continue to the next part (array or not) */
            continue;
        }

        ac_ast_expr* array_size_expr = parse_expr(p, LOWEST_PRIORITY_PRECEDENCE);

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

static bool is_basic_type_or_identifier(enum ac_token_type type)
{
    return type == ac_token_type_IDENTIFIER
        || type == ac_token_type_BOOL
        || type == ac_token_type_CHAR
        || type == ac_token_type_DOUBLE
        || type == ac_token_type_FLOAT
        || type == ac_token_type_INT
        || type == ac_token_type_LONG
        || type == ac_token_type_SHORT
        || type == ac_token_type_SIGNED
        || type == ac_token_type_UNSIGNED
        || type == ac_token_type_VOID;
}

static bool is_leading_declaration(enum ac_token_type type)
{
    return is_basic_type_or_identifier(type)

        || type == ac_token_type_AUTO
        || type == ac_token_type_EXTERN
        || type == ac_token_type_REGISTER
        || type == ac_token_type_STATIC

        || type == ac_token_type_ATOMIC
        || type == ac_token_type_THREAD_LOCAL
        || type == ac_token_type_THREAD_LOCAL2

        || type == ac_token_type_INLINE
        || type == ac_token_type_CONST
        || type == ac_token_type_VOLATILE

        || type == ac_token_type_ENUM
        || type == ac_token_type_STRUCT
        || type == ac_token_type_TYPEDEF
        ;
}

static ac_token token(const ac_parser_c* p)
{
    return p->pp.lex.token;
}
static const ac_token* token_ptr(const ac_parser_c* p)
{
    return &p->pp.lex.token;
}
static ac_location location(const ac_parser_c* p)
{
    return p->pp.lex.location;
}

static void goto_next_token(ac_parser_c* p)
{
    ac_pp_goto_next(&p->pp);

    const ac_token* token = token_ptr(p);

    while (token->type == ac_token_type_COMMENT
         || token->type == ac_token_type_NEW_LINE
         || token->type == ac_token_type_HORIZONTAL_WHITESPACE)
    {
        ac_pp_goto_next(&p->pp);
        token = token_ptr(p);
    }
}

static enum ac_token_type token_type(const ac_parser_c* p)
{
    return p->pp.lex.token.type;
}

static bool token_is(const ac_parser_c* p, enum ac_token_type type) {
    return token_equal_type(p->pp.lex.token, type);
}

static bool token_is_not(const ac_parser_c* p, enum ac_token_type type)
{
    return !token_is(p, type);
}

static bool token_is_unary_operator(const ac_parser_c* p)
{
    switch (p->pp.lex.token.type) {
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

static bool token_equal_type(ac_token token, enum ac_token_type type)
{
    return token.type == type;
}

static bool expect(ac_parser_c* p, enum ac_token_type type) {
    return ac_lex_expect(&p->pp.lex, type);
}

static bool expect_and_consume(ac_parser_c* p, enum ac_token_type type)
{
    AC_ASSERT(type != ac_token_type_EOF);

    if (expect(p, type))
    {
        ac_location loc = location(p);
        ac_token previous = token(p);
        goto_next_token(p);
        /* Special case where the last token should either be a ';' or a '}'. */
        {
            if (token_ptr(p)->type == ac_token_type_EOF
                && previous.type != ac_token_type_SEMI_COLON
                && previous.type != ac_token_type_BRACE_R)
            {
                strv current_string = ac_token_to_strv(previous);

                ac_report_error_loc(loc, "syntax error: unexpected end-of-file after: '%.*s'"
                    , current_string.size, current_string.data
                );

                return false;
            }
        }
        return true;
    }

    return false;
}

static bool consume_if(ac_parser_c* p, bool value)
{
    if (value)
    {
        goto_next_token(p);
    }
    return value;
}

static bool expr_is(ac_ast_expr* expr, enum ac_ast_type type)
{
    return expr->type == type;
}

static bool expr_is_statement(ac_ast_expr* expr)
{
    /* @TODO handle 'if' here */
    return expr->type == ac_ast_type_EMPTY_STATEMENT
        || expr->type == ac_ast_type_RETURN
        || ac_ast_is_declaration(expr);
}

static ac_ast_expr* to_expr(void* any)
{
    return (ac_ast_expr*)any;
}

static void add_to_current_block(ac_parser_c* p, ac_ast_expr* expr)
{
    assert(p && expr);

    ac_expr_list_add(&p->current_block->statements, expr);
}