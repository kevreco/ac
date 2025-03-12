#include "preprocessor.h"

#include <time.h>

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
static ac_token* goto_next_macro_expanded(ac_pp* pp);
/* @FIXME: all those "goto_next" are becoming messy. Make it clearer. */
static ac_token* goto_next_macro_expanded_no_space(ac_pp* pp);

static void skip_all_until_new_line(ac_pp* pp);

static bool parse_directive(ac_pp* pp);
static bool parse_macro_definition(ac_pp* pp);
static bool parse_macro_parameters(ac_pp* pp, ac_macro* m);
static bool parse_macro_body(ac_pp* pp, ac_macro* m);
static bool parse_include_directive(ac_pp* pp);
static bool parse_include_path(ac_pp* pp, strv* path, bool* is_system_path);
/* Look if a specific file exists in any directories from the array.
   If there is a match pp->path_buffer contains the existing path. */
static bool look_for_filepath(ac_pp* pp, path_array* directories, strv filepath);
/* Combine directory path and a filepath. The result is stored in pp->path_buffer. */
static bool combine_filepath(ac_pp* pp, strv folder, strv filepath);

static void macro_push(ac_pp* pp, ac_macro* m);

static ac_token* process_cmd(ac_pp* pp, ac_token_cmd* cmd);
/* Get and remove the first expanded token. */
static ac_token* stack_pop(ac_pp* pp);
static void push_cmd(ac_pp* pp, ac_token_cmd list);

static void handle_some_special_macros(ac_pp* pp, ac_token* tok);
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

/*-----------------------------------------------------------------------*/
/* macro hash table */
/*-----------------------------------------------------------------------*/

static ht_hash_t macro_hash(ht_ptr_handle* handle);
static ht_bool macros_are_same(ht_ptr_handle* hleft, ht_ptr_handle* hright);
static void add_macro(ac_pp* pp, ac_macro* m);
static void undefine_macro(ac_pp* pp, ac_macro* m);
static ac_macro* find_macro(ac_pp* pp, ac_token* identifer);

/*-----------------------------------------------------------------------*/
/* Preprocessor evaluation */
/*-----------------------------------------------------------------------*/

static int LOWEST_PRIORITY_PRECEDENCE = 999;

typedef struct eval_t eval_t;
struct eval_t {
    int64_t value;
    bool succes;
};

static eval_t eval_false = { 0, false};

static ac_token* goto_next_for_eval(ac_pp* pp);

static int get_precedence_if_binary_op(enum ac_token_type type);

static eval_t eval_primary(ac_pp* pp);
static int64_t eval_binary(ac_pp* pp, enum ac_token_type binary_op, int64_t left, int64_t right);
static eval_t eval_expr2(ac_pp* pp, int previous_precedence);
/* For "#if X" X must be a preprocessor expression while "#ifdef Y" Y must be an identifier. */
static eval_t eval_expr(ac_pp* pp, bool expect_identifier_expression);

static void pop_branch(ac_pp* pp);
static void push_branch(ac_pp* pp, enum ac_token_type type, ac_location loc);
static void set_branch_type(ac_pp* pp, enum ac_token_type type);
static void set_branch_value(ac_pp* pp, bool value);
static bool branch_is(ac_pp* pp, enum ac_token_type type);
static bool branch_is_empty(ac_pp* pp);
static bool branch_was_enabled(ac_pp* pp);

/*-----------------------------------------------------------------------*/
/* #include related code */
/*-----------------------------------------------------------------------*/

static void push_include_stack(ac_pp* pp, strv content, strv filepath);
static void pop_include_stack(ac_pp* pp);

/*-----------------------------------------------------------------------*/
/* API */
/*-----------------------------------------------------------------------*/

void ac_pp_init(ac_pp* pp, ac_manager* mgr, strv content, strv filepath)
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

    /* Unroll the remaining item in the stack. */
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
    /* Get next token. */
    ac_token* t = goto_next_macro_expanded(pp);

    /* Once the end of file is reached the include stack needs to be popped if we are not already in the top level file.
       'while' loop is used instead of 'if' because we might need to pop from multiple files
       in case the '#include' directive is at the end of the file consecutively. */
    while (t->type == ac_token_type_EOF)
    {
        if (pp->if_else_level > pp->include_stack[pp->include_stack_depth].starting_if_else_level)
        {
            struct branch_state b = pp->if_else_stack[pp->if_else_level];
            ac_report_error_loc(b.loc, "unterminated #%s", ac_token_type_to_str(b.type));
            return ac_set_token_error(&pp->lex);
        }

        if (pp->include_stack_depth > 0)
        {
            pop_include_stack(pp);

            /* The previous lexer must have left on a NEW_LINE or EOF token, right after the #include "file.h" */
            AC_ASSERT(token(pp).type == ac_token_type_NEW_LINE || token(pp).type == ac_token_type_EOF);
        }
        else
        {
            break;
        }
    }

    return t;
}

void ac_pp_preprocess(ac_pp* pp, FILE* file)
{
    /* Print preprocessed tokens in the standard output. */
    const ac_token* token = NULL;

    ac_token previous_token = { 0 };
    previous_token.type = ac_token_type_NEW_LINE;

    while ((token = ac_pp_goto_next(pp)) != NULL
        && token->type != ac_token_type_EOF)
    {
        /* Avoid printing multiple new lines in a row. */
        if (previous_token.type == ac_token_type_NEW_LINE && token->type == ac_token_type_NEW_LINE)
        {
            continue;
        }

        ac_token_fprint(stdout, *token);

        previous_token = *token;
    }
}

void ac_pp_preprocess_benchmark(ac_pp* pp, FILE* file)
{
    const ac_token* token = NULL;

    while ((token = ac_pp_goto_next(pp)) != NULL
        && token->type != ac_token_type_EOF)
    {
    }

    /* @TODO display line count, byte count, and number of identifiers.
       @TODO display time, line per seconds and bytes per seconds. */
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

    /* Directives must begin with a '#' and the '#' should be at the beginning of the line.
       macro_depth is used because we don't want to parse directives for tokens '#' coming from macros. */
    while (token_ptr(pp)->beginning_of_line
        && token_ptr(pp)->type == ac_token_type_HASH
        && pp->macro_depth == 0)
    {
        if (!parse_directive(pp)
            || token_ptr(pp)->type == ac_token_type_EOF)
        {
            return ac_token_eof(pp);
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
    ac_token* token = goto_next_raw_token(pp);

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

    while (token->type == ac_token_type_HORIZONTAL_WHITESPACE
        || token->type == ac_token_type_COMMENT
        || token->type == ac_token_type_NEW_LINE)
    {
        token = goto_next_raw_token(pp);
        token->previous_was_space = true;
    }

    return token;
}

static ac_token* goto_next_macro_expanded(ac_pp* pp)
{
    goto_next_normal_token(pp);

    /* Every time the token is expanded we need to try to expand again. */
    while (try_expand(pp, token_ptr(pp))) {
        goto_next_raw_token(pp);
    }

    return token_ptr(pp);
}

static ac_token* goto_next_macro_expanded_no_space(ac_pp* pp)
{
    ac_token* token = goto_next_macro_expanded(pp);

    while (token->type == ac_token_type_HORIZONTAL_WHITESPACE
        || token->type == ac_token_type_COMMENT
        || token->type == ac_token_type_NEW_LINE)
    {
        token = goto_next_macro_expanded(pp);
    }

    return token;
}

static void skip_all_until_new_line(ac_pp* pp)
{
    /* Skip all tokens until end of line */
    while (token_ptr(pp)->type != ac_token_type_NEW_LINE
        && token_ptr(pp)->type != ac_token_type_EOF)
    {
        goto_next_raw_token(pp);
    }
}

static bool parse_directive(ac_pp* pp)
{
    AC_ASSERT(token(pp).type == ac_token_type_HASH);

    ac_token* tok = goto_next_token_from_directive(pp); /* Skip '#' */

    switch (tok->type)
    {
    case ac_token_type_ENDIF:
    {
        if (branch_is_empty(pp))
        {
            ac_report_error_loc(location(pp), "#endif without #if");
            goto_next_token_from_directive(pp); /* Skip 'endif'. */
            return false;
        }

        pop_branch(pp);

        goto_next_token_from_directive(pp); /* Skip 'endif' */
        break;
    }
    case ac_token_type_DEFINE:
    {
        goto_next_token_from_directive(pp); /* Skip 'define' */
        if (!parse_macro_definition(pp))
        {
            return false;
        }
        break;
    }

    case ac_token_type_ELIF:
    {
        if (branch_is_empty(pp))
        {
            ac_report_error_loc(location(pp), "#elif without #if");
            goto_next_token_from_directive(pp); /* Skip 'elif'. */
            return false;
        }

        goto branch_case;
    }
    case ac_token_type_ELSE:
    {
        if (branch_is_empty(pp))
        {
            ac_report_error_loc(location(pp), "#else without #if");
            goto_next_token_from_directive(pp); /* Skip 'else'. */
            return false;
        }
        
        goto branch_case;
    }
    case ac_token_type_IF:
    case ac_token_type_IFDEF:
    case ac_token_type_IFNDEF:
    case ac_token_type_ELIFDEF:
    case ac_token_type_ELIFNDEF:
    {
        /* @TODO: Once ifdef/elifdef and elifndef are implemented remove the branch_case label and create a handle_branch_error(pp) function. */
branch_case:
        for(;;)
        {
            enum ac_token_type t = token_ptr(pp)->type;
            ac_location loc = location(pp);
            bool need_to_skip_block;
            if (t == ac_token_type_ELSE) {
                goto_next_token_from_directive(pp);
                /* If one of the previous branch was enabled we need to skip this one. */
                need_to_skip_block = branch_was_enabled(pp);
            } else {
                if (t == ac_token_type_IF
                    || t == ac_token_type_ELIF
                    || t == ac_token_type_ELSE)
                {
                    goto_next_for_eval(pp); /* Skip if/elif/else and get the next expanded token. */
                }
                else 
                {
                    goto_next_token_from_directive(pp); /* Skip ifdef/ifndef/elifdef/elifndef and get next non-expanded_token. */
                }

                /* Push new if/else chain if when necessary. */
                if (t == ac_token_type_IF
                    || t == ac_token_type_IFDEF
                    || t == ac_token_type_IFNDEF)
                {
                    push_branch(pp, t, loc);
                }
                /* If one of the previous branch was enabled we need to skip the current one. */
                if (branch_was_enabled(pp))
                {
                    need_to_skip_block = true;
                }
                /* Otherwise we evaluate the expression and check if we need to skip the block or not.*/
                else
                {
                    bool require_identifier_expression = t == ac_token_type_IFDEF
                        || t == ac_token_type_IFNDEF
                        || t == ac_token_type_ELIFDEF
                        || t == ac_token_type_ELIFNDEF;
                    eval_t eval = eval_expr(pp, require_identifier_expression);
                    if (!eval.succes)
                    {
                        return false;
                    }
                    bool branch_value = eval.value;
                    /* Flip value for negative if */
                    if (t == ac_token_type_IFNDEF || t == ac_token_type_ELIFNDEF)
                    {
                        branch_value = !branch_value;
                    }

                    set_branch_value(pp, branch_value);
                    need_to_skip_block = !branch_value;
                }
            }

            /* If expression is non-zero we skip the preprocessor block. */
            if (need_to_skip_block) /* Eval expression */
            {
                bool was_end_of_line = token(pp).type == ac_token_type_NEW_LINE;

                pp->current_token = ac_skip_preprocessor_block(&pp->lex, was_end_of_line);

                if (pp->current_token->type == ac_token_type_EOF)
                {
                    /* NOTE: "unterminated <branch> error will be displayed later */
                    return ac_set_token_error(&pp->lex);
                }

                switch (token_ptr(pp)->type)
                {
                case ac_token_type_ENDIF:
                {
                    goto_next_token_from_directive(pp); /* Skip 'endif' */
                    pop_branch(pp);
                    break;
                }
                
                case ac_token_type_ELIF:
                case ac_token_type_ELIFDEF:
                case ac_token_type_ELIFNDEF:
                case ac_token_type_ELSE:
                    /* Loop again. */
                    continue;
                default:
                    AC_ASSERT(0 && "UNREACHABLE");
                }
            }
            break; /* end of the for(;;) */
        }
        break;
    }
    case ac_token_type_INCLUDE: {
        if (!parse_include_directive(pp)) {
            return false;
        }
        break;
    }
    case ac_token_type_UNDEF:
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

        skip_all_until_new_line(pp);

        /* Remove macro it's previously defined. */
        ac_macro* m = find_macro(pp, &identifier);
        if (m)
        {
            undefine_macro(pp, m);
        }
        break;
    }

    case ac_token_type_ERROR:
    case ac_token_type_EMBED:
    case ac_token_type_PRAGMA:
    case ac_token_type_LINE:
    case ac_token_type_WARNING:
        ac_report_warning_loc(location(pp), "ignoring unsupported directive");
        goto_next_raw_token(pp); /* Skip directive name. */
        skip_all_until_new_line(pp);
        return true;
    case ac_token_type_IDENTIFIER:
        ac_report_warning_loc(location(pp), "ignoring unknown directive '" STRV_FMT "'", STRV_ARG(tok->ident->text));
        goto_next_raw_token(pp); /* Skip directive name. */
        skip_all_until_new_line(pp);
        return true;
    case ac_token_type_NEW_LINE:
    case ac_token_type_EOF:
        /* Null directives, nothing needs to be done. */
        break;
    }

    /* All directives must end with a new line or a EOF. */
    if (token_ptr(pp)->type != ac_token_type_NEW_LINE
        && token_ptr(pp)->type != ac_token_type_EOF)
    {
        ac_report_internal_error_loc(location(pp), "directive did not end with a new line");
        return false;
    }
    goto_next_raw_token(pp); /* Skip new line or EOF. */


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
   
    /* Skip spaces or comments right after the macro name or the function-like "macro prototype"
      to go to the first token from the macro body */
    if (token(pp).type == ac_token_type_HORIZONTAL_WHITESPACE
        || token(pp).type == ac_token_type_COMMENT)
    {
        goto_next_token_from_directive(pp);
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
        ac_report_error_loc(m->location, "'##' cannot appear at either end of a macro expansion");
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
        ac_report_error_loc(m->location, "'##' cannot appear at either end of a macro expansion");
        return false;
    }

    range r = { body_start_index, darrT_size(&m->definition) };
    m->body = r;

    return true;
}

static bool parse_include_directive(ac_pp* pp)
{
    ac_location loc = location(pp);
    goto_next_token_from_directive(pp); /* Skip 'include' */

    if (pp->include_stack_depth == ac_pp_MAX_INCLUDE_DEPTH)
    {
        ac_report_error_loc(loc, "maximum number of #include file reached (%d)", ac_pp_MAX_INCLUDE_DEPTH);
        return false;
    }

    strv path;
    bool is_system_path;
    if (!parse_include_path(pp, &path, &is_system_path))
    {
        return false;
    }

    char* dst = pp->path_buffer;
    bool file_found = false;
    /* Search the file to include and build path into the relevant buffer. */
    {
        strv dir = re_path_is_absolute(path)
            ? (strv)STRV("")
            : re_path_remove_last_segment(pp->lex.filepath);

        if (!combine_filepath(pp, dir, path))
        {
            return false;
        }

        file_found = re_file_exists_str(pp->path_buffer);

        /* Try to look into the user include directory. */
        if (!file_found)
        {
            file_found = look_for_filepath(pp, &pp->mgr->options.user_includes, path);
        }

        /* Try to look into the system include directory. */
        if (!file_found)
        {
            file_found = look_for_filepath(pp, &pp->mgr->options.system_includes, path);
        }

        if (!file_found)
        {
            ac_report_error_loc(loc, "include file not found: '" STRV_FMT "'", STRV_ARG(path));
            ac_set_token_error(&pp->lex);
            return false;
        }
    }

    ac_source_file src_file;
    if (!ac_manager_load_content(pp->mgr, pp->path_buffer, &src_file))
    {
        ac_set_token_error(&pp->lex);
        return false;
    }

    if (src_file.content.size)
    {
        push_include_stack(pp, src_file.content, src_file.filepath);
    }

    return true;
}

static bool parse_include_path(ac_pp* pp, strv* path, bool* is_system_path)
{
    *is_system_path = false;
    /*
       (1) #include "path.h"
       (2) #include <abc>
       (3) #include a b c

       If (1), the next token would be a string literal.
       If (2), the next token would be a < and we would need to parse all token until the next >
       if (3), a b c must represent the equivalent of (1) or (2)
    */
    switch (token(pp).type) {
    literal_string_case:
    case ac_token_type_LITERAL_STRING: { /* "text" */
        *path = token(pp).text;
        goto_next_token_from_directive(pp); /* Skip string literal. */
        break;
    }
    case ac_token_type_LESS: { /* '<' */
        ac_token* t = ac_parse_include_path(&pp->lex);

        *is_system_path = true;
        *path = t->text;

        /* Problem during ac_parse_include_path, and error message was already displayed. */
        if (t->type != ac_token_type_LITERAL_STRING)
        {
            return false;
        }
        goto_next_token_from_directive(pp); /* Skip path (string literal). */

        break;
    }

    case ac_token_type_IDENTIFIER: {

        /* Expand token until we can't. */
        while (try_expand(pp, token_ptr(pp))) {
            goto_next_raw_token(pp);
        }

        if (token_ptr(pp)->type == ac_token_type_LITERAL_STRING)
        {
            goto literal_string_case;
        }

        if (token_ptr(pp)->type != ac_token_type_LESS)
        {
            ac_report_error_loc(location(pp), "#include directive expects \"filepath\" or <filepath>");
            return false;
        }

        *is_system_path = true;

        /* Get next token expended above.*/
        ac_token* t = goto_next_macro_expanded_no_space(pp); /* Skip '<' */

        /* Concatenate all tokens between the '<' '>' into a string. */
        dstr_clear(&pp->concat_buffer);
        do {

            dstr_append_f(&pp->concat_buffer, STRV_FMT, STRV_ARG(ac_token_to_strv(*t)));
            t = goto_next_macro_expanded_no_space(pp);
        } while (t->type != ac_token_type_GREATER
            && t->type != ac_token_type_EOF
            && t->type != ac_token_type_NEW_LINE);

        if (t->type != ac_token_type_GREATER)
        {
            ac_report_error_loc(location(pp), "expect a closing '>'");
            return false;
        }

        goto_next_token_from_directive(pp); /* Skip '>', no need to expand macro anymore. */

        *path = ac_create_or_reuse_literal(pp->mgr, dstr_to_strv(&pp->concat_buffer));
        break;
    }
    default:
        ac_report_error_loc(location(pp), "#include directive expects \"filepath\" or <filepath>");
        return false;
    }

    if (token(pp).type != ac_token_type_EOF
        && token(pp).type != ac_token_type_NEW_LINE)
    {
        ac_report_warning_loc(location(pp), "extra tokens found in #include directives");

        skip_all_until_new_line(pp);
    }

    return true;
}

static bool look_for_filepath(ac_pp* pp, path_array* arr, strv filepath)
{
    char* dst = 0;
   
    for (int i = 0; i < darrT_size(arr); i += 1)
    {
        strv dir = darrT_at(arr, i);

        if (!combine_filepath(pp, dir, filepath)) {
            return false;
        }

        if (re_file_exists_str(pp->path_buffer)) {
            return true;
        }
    }
    return false;
}

static bool combine_filepath(ac_pp* pp, strv folder, strv filepath)
{
    if ((folder.size + filepath.size) > ac_pp_MAX_FILEPATH)
    {
        ac_report_error_loc(location(pp), "path longer than %d characters are not yet supported.", ac_pp_MAX_FILEPATH);
        return false;
    }

    char* dst = pp->path_buffer;

    if (folder.size)
    {
        /* Add folder. */
        memcpy(dst, folder.data, folder.size);
        dst += folder.size;

        /* Add directory separator if needed. */
        if (dst[0] != '\\' && dst[0] != '/')
        {
            if ((folder.size + filepath.size + 1) > ac_pp_MAX_FILEPATH)
            {
                ac_report_error_loc(location(pp), "path longer than %d characters are not yet supported.", ac_pp_MAX_FILEPATH);
                return false;
            }

            dst[0] = '/';
            dst += 1;
        }
    }

    /* Add leaf. */
    memcpy(dst, filepath.data, filepath.size);
    dst += filepath.size;

    dst[0] = '\0';

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

static void handle_some_special_macros(ac_pp* pp, ac_token* tok)
{
    /* Buffer used for
        __DATE__:    "MMM DD YYYY\0"
        __TIME__:    "HH:mm:ss\0"
        __LINE__:    "XXXXXXXXXXXXXXXXXXXXX\0" - max int64 digits + '\0'
        __COUNTER__: "XXXXXXXXXXXXXXXXXXXXX\0" - max int64 digits + '\0'
   */

    char buffer[22];

    if (tok->type == ac_token_type__FILE__)
    {
        tok->type = ac_token_type_LITERAL_STRING;
        tok->text = pp->lex.filepath;
    }
    else if (tok->type == ac_token_type__LINE__
        || tok->type == ac_token_type__COUNTER__)
    {
        int number = tok->type == ac_token_type__COUNTER__
            ? pp->counter_value
            : pp->lex.location.row;

        pp->counter_value += tok->type == ac_token_type__COUNTER__;
        snprintf(buffer, sizeof(buffer), "%d", number);
    
        tok->text = ac_create_or_reuse_literal(pp->mgr, strv_make_from_str(buffer));
        tok->type = ac_token_type_LITERAL_INTEGER;
        tok->u.number.is_unsigned = true;
        tok->u.number.u.int_value = number;
    }
    else if (tok->type == ac_token_type__DATE__
        || tok->type == ac_token_type__TIME__)
    {
        time_t t;
        struct tm* tm;
        time(&t);
        tm = localtime(&t);

        if (tok->type == ac_token_type__DATE__)
        {
            static char const months[12][4] = {
                "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
            };
            snprintf(buffer, sizeof(buffer),
                "%s %2d %d",
                months[tm->tm_mon], tm->tm_mday, tm->tm_year + 1900);
        }
        else
        {
            snprintf(buffer, sizeof(buffer),
                "%02d:%02d:%02d",
                tm->tm_hour, tm->tm_min, tm->tm_sec);
        }

        tok->type = ac_token_type_LITERAL_STRING;
        tok->text = ac_create_or_reuse_literal(pp->mgr, strv_make_from_str(buffer));
    }
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
        /* @OPT: create an inline function or a macro like 'ac_token_is_special_macro' to avoid unnecessary function call (if it ever matters). */
        handle_some_special_macros(pp, tok);
        return false;
    }

    if (m->identifier.ident->cannot_expand)
    {
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

static const strv todo = STRV("@TODO");

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
    ac_lex_set_content(&pp->lex, new_content, todo);

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
                    ac_report_error_loc(loc, "unexpected end of file in macro expansion '"STRV_FMT"'", STRV_ARG(identifier->ident->text));
                    goto cleanup;
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
            /* @TODO try to display this error. */
            ac_report_error_loc(loc, "function-like macro invocation '"STRV_FMT"' does not end with ')'", STRV_ARG(m->identifier.ident->text));
            goto cleanup;
        }

        if (current_param_index > param_count) /* No parameter should be left. */
        {
            ac_report_warning_loc(loc, "too many argument in function-like macro invocation '"STRV_FMT"'", STRV_ARG(m->identifier.ident->text));
        }

        if (current_param_index < param_count) /* No parameter should be left. */
        {
            ac_report_warning_loc(loc, "missing arguments in function-like macro invocation '"STRV_FMT"'", STRV_ARG(m->identifier.ident->text));

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

static ac_token* goto_next_for_eval(ac_pp* pp)
{
    ac_token* token = goto_next_macro_expanded(pp);
    while (token->type == ac_token_type_HORIZONTAL_WHITESPACE
       || token->type == ac_token_type_COMMENT)
    {
        token = goto_next_macro_expanded(pp);
    }

    return token;
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
    default:
        return LOWEST_PRIORITY_PRECEDENCE;
    }
}

static eval_t eval_primary(ac_pp* pp)
{
    eval_t result = {0, true};
    enum ac_token_type type = token(pp).type;
    switch (type) {
    case ac_token_type_DEFINED: {
        ac_location loc = location(pp); /* Save location for error. */
        
        goto_next_token_from_directive(pp); /* Skip 'defined' but do not expand macro, and skip horizontal whitespace. */
        
        bool expect_closing_parenthesis = false;
        if (token(pp).type == ac_token_type_PAREN_L)
        {
            expect_closing_parenthesis = true;
            goto_next_token_from_directive(pp); /* Skip '(' */
        }
        
        if (!ac_token_is_keyword_or_identifier(token(pp).type))
        {
            result = eval_false;
            ac_report_error_loc(loc, "operator 'defined' requires an identifier");
            break;
        }

        result.value = find_macro(pp, token_ptr(pp)) != NULL;
        goto_next_for_eval(pp); /* Skip identifier. */

        if (expect_closing_parenthesis && expect(pp, ac_token_type_PAREN_R))
        {
            goto_next_token_from_directive(pp); /* Skip ')' */
        }
        break;
    }
    case ac_token_type_EOF: { /* Error must have been reported by "goto_next_token" or "expect_and_consume" */
        result = eval_false;
        break;
    }
    case ac_token_type_PAREN_L: {
        goto_next_for_eval(pp); /* Skip '(' */
        result = eval_expr2(pp, LOWEST_PRIORITY_PRECEDENCE);

        if (!expect(pp, ac_token_type_PAREN_R))
        {
            result = eval_false;
            return result;
        }

        goto_next_for_eval(pp); /* Skip ')' */
        break;
    }
    case ac_token_type_FALSE: {
        result.value = 0;
        goto_next_for_eval(pp);
        break;
    }
    /* Macros are already expanded, hence an identifier here means an non-existing macro.
       They are replaced with 0. */
    case ac_token_type_IDENTIFIER: {
        result.value = 0;
        goto_next_for_eval(pp);
        break;
    }
    case ac_token_type_LITERAL_CHAR: {
        result.value = ac_token_type_to_strv(type).data[0];
        goto_next_for_eval(pp); /* Skip literal. */
        break;
    }
    case ac_token_type_LITERAL_INTEGER: {
        result.value = token(pp).u.number.u.int_value;
        goto_next_for_eval(pp); /* Skip literal. */
        break;
    }
    case ac_token_type_TRUE: {
        result.value = 1;
        goto_next_for_eval(pp); /* Skip 'true'. */
        break;
    }
    case ac_token_type_EXCLAM: {
        goto_next_for_eval(pp); /* Skip '!' */

        eval_t right = eval_primary(pp);
        if (!right.succes)
        {
            result = eval_false;
            break;
        }
        result.value = !right.value;
        break;
    }

    case ac_token_type_MINUS:
    case ac_token_type_PLUS:
    case ac_token_type_TILDE: /* '~' Bitwise NOT */
    {
        goto_next_for_eval(pp); /* Skip operator */
        eval_t right = eval_expr2(pp, LOWEST_PRIORITY_PRECEDENCE);
        if (!right.succes)
        {
            result = eval_false;
            break;
        }
        /* Evaluation unary operation as binary operation with zero value on the left.*/
        result.value = eval_binary(pp, type, 0, right.value);
        break;
    }
    /* Binary operator used as unary operator */
    default: {
        result = eval_false;
        break;
    }
    }
    return result;
}

static int64_t eval_binary(ac_pp* pp, enum ac_token_type binary_op, int64_t left, int64_t right)
{
    switch (binary_op)
    {
    case ac_token_type_PERCENT:        return left % right;
    case ac_token_type_SLASH:          return left / right;
    case ac_token_type_STAR:           return left * right;
    case ac_token_type_MINUS:          return left - right;
    case ac_token_type_PLUS:           return left + right;
    case ac_token_type_DOUBLE_LESS:    return left << right;
    case ac_token_type_DOUBLE_GREATER: return left >> right;
    case ac_token_type_GREATER:        return (int64_t)(left > right);
    case ac_token_type_GREATER_EQUAL:  return (int64_t)(left >= right);
    case ac_token_type_LESS:           return (int64_t)(left < right);
    case ac_token_type_LESS_EQUAL:     return (int64_t)(left <= right);
    case ac_token_type_DOUBLE_EQUAL:   return (int64_t)(left == right);
    case ac_token_type_NOT_EQUAL:      return (int64_t)(left != right);
    case ac_token_type_AMP:            return left & right;
    case ac_token_type_CARET:          return left ^ right;
    case ac_token_type_PIPE:           return left | right;
    case ac_token_type_DOUBLE_AMP:     return (int64_t)(left && right);
    case ac_token_type_DOUBLE_PIPE:    return (int64_t)(left || right);
    default:
        AC_ASSERT(0 && "Unexpected binary operator.");
    }
    AC_ASSERT(0 && "Unreachable.");
    return 0;
}

static eval_t eval_expr2(ac_pp* pp, int previous_precedence)
{
    eval_t left = eval_primary(pp);
    if (!left.succes)
    {
        return eval_false;
    }

    while (true) /* Left can be NULL if */
    {
        enum ac_token_type new_token_type = token(pp).type;
        int new_precedence = get_precedence_if_binary_op(new_token_type);

        /* While the next token is a binary operator and if the new precedence is higher priority,
           We need to combine the left expression with a following one. */
        if (new_precedence < previous_precedence)
        {
            ac_location loc = location(pp); /* Save binary operator location for error. */
            goto_next_for_eval(pp); /* Skip binary op. */

            eval_t right = eval_expr2(pp, new_precedence);
            if (!right.succes)
            {
                ac_report_error_loc(loc, "operator '"STRV_FMT"' has no right operand", STRV_ARG(ac_token_type_to_strv(new_token_type)));
                return eval_false;
            }
            left.value = eval_binary(pp, new_token_type, left.value, right.value);
        }
        else
        {
            return left;
        }
    }
}

static eval_t eval_expr(ac_pp* pp, bool expect_identifier_expression)
{
    eval_t eval = { 0, true };
    ac_location expression_location = location(pp);

    if (token(pp).type == ac_token_type_EOF)
    {
        ac_report_error_loc(expression_location, "unexpected end-of-file in preprocessor expression");
        return eval_false;
    }

    if (expect_identifier_expression)
    {
        if (!ac_token_is_keyword_or_identifier(token(pp).type))
        {
            ac_report_error_loc(expression_location, "identifier expected after #ifdef, #ifndef, #elifdef or #elifndef.");
            eval = eval_false;
        }
        else
        {
            eval.value = find_macro(pp, token_ptr(pp)) != NULL;
            goto_next_for_eval(pp); /* Skip identifier. */
        }
    }
    else
    {
        eval = eval_expr2(pp, LOWEST_PRIORITY_PRECEDENCE);
    }

    if ((token(pp).type != ac_token_type_NEW_LINE && token(pp).type != ac_token_type_EOF) || !eval.succes) {
        ac_report_error_loc(expression_location, "invalid preprocessor expression");
    }

    /* All directives must end with a new line or a EOF. */
    if ((token(pp).type != ac_token_type_NEW_LINE && token(pp).type != ac_token_type_EOF)
        || !eval.succes)
    {
        skip_all_until_new_line(pp);

        return eval_false;
    }

    return eval;
}

static void pop_branch(ac_pp* pp)
{
    pp->if_else_stack[pp->if_else_level].type = ac_token_type_NONE;
    pp->if_else_stack[pp->if_else_level].was_enabled = false;
    pp->if_else_level -= 1;
}

static void push_branch(ac_pp* pp, enum ac_token_type type, ac_location loc)
{
    pp->if_else_level += 1;
    if (pp->if_else_level >= ac_pp_branch_MAX_DEPTH)
    {
        ac_report_error("too many nested #if/#else (more than %d)", ac_pp_branch_MAX_DEPTH);
        return;
    }
    pp->if_else_stack[pp->if_else_level].type = type;
    pp->if_else_stack[pp->if_else_level].loc = loc;
}

static void set_branch_type(ac_pp* pp, enum ac_token_type type)
{
    pp->if_else_stack[pp->if_else_level].type = type;
}

static void set_branch_value(ac_pp* pp, bool value)
{
    pp->if_else_stack[pp->if_else_level].was_enabled = value;
}

static bool branch_is(ac_pp* pp, enum ac_token_type type)
{
    return pp->if_else_level > pp->include_stack[pp->include_stack_depth].starting_if_else_level
        && pp->if_else_stack[pp->if_else_level].type == type;
}

static bool branch_is_empty(ac_pp* pp)
{
    return pp->if_else_level <= pp->include_stack[pp->include_stack_depth].starting_if_else_level;
}

static bool branch_was_enabled(ac_pp* pp)
{
    return pp->if_else_stack[pp->if_else_level].was_enabled;
}

static void push_include_stack(ac_pp* pp, strv content, strv filepath)
{
    ac_lex_state state = ac_lex_save(&pp->lex);
    
    pp->include_stack_depth += 1;

    pp->include_stack[pp->include_stack_depth].starting_if_else_level = pp->if_else_level;
    pp->include_stack[pp->include_stack_depth].lex_state = state;

    ac_lex_set_content(&pp->lex, content, filepath);
}

static void pop_include_stack(ac_pp* pp)
{
    ac_lex_restore(&pp->lex, &pp->include_stack[pp->include_stack_depth].lex_state);

    pp->include_stack_depth -= 1;
}