#ifndef AC_PREPROCESSOR_H
#define AC_PREPROCESSOR_H

#include "lexer.h"
#include "re_lib.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ac_macro ac_macro;

typedef darrT(ac_token) darr_token;

enum ac_token_cmd_type {
	ac_token_cmd_type_TOKEN_LIST,   /* Expanded tokens coming from macro. */
	ac_token_cmd_type_MACRO_POP,    /* To make a macro expandable again. */
};

enum {
	ac_pp_branch_MAX_DEPTH = 32,  /* Max depth of #if#else branches. */
	ac_pp_MAX_INCLUDE_DEPTH = 32, /* Max depth of #include. */
	ac_pp_MAX_FILEPATH = 1024     /* Max size of path. */
};

typedef struct ac_token_cmd ac_token_cmd;
struct ac_token_cmd {
	enum ac_token_cmd_type type;
	
	union {
		struct {
			ac_token* data;
			size_t count;
			size_t i;
		} token_list;

		struct {
			ac_macro* macro;
			darr_token tokens;
		} macro_pop;
	};
};

typedef struct ac_pp ac_pp;
struct ac_pp {
	ac_manager* mgr;
	ac_lex lex;        /* Main lexer. */
	ac_lex concat_lex; /* Lexer for string concatenation. */

	/* Stack of list of tokens. It's mostly used for macro but we should be able to add tokens if we peek some next ones. */
	darrT(ac_token_cmd) cmd_stack;
	/* Keep a reference of all created macros.
	   NOTE: It's possible to undefine and redefine a macro while it's being expanded.
	         This mean we can't destroy the undefine macro right away, hence we keep them forever like defined macros.
	   @OPT: It's wasteful to keep undefined macro. But at the same time it's not used that much.
	         It shouldn't have significant impact overall. */
	darrT(ac_macro*) macros;

	ac_token* current_token;

	dstr concat_buffer;         /* Concatenation of token is done via tokenizing a string. */
	int macro_depth;            /* Macro depth is not currently needed, it's mostly for inspectiong purpose. */
	darr_token buffer_for_peek; /* Sometimes we need to peek some tokens and send them on the stack. */
	int counter_value;

	/* @OPT: We could use an octect for this branch_flags struct instead of two int. */
	struct branch_state {
		enum ac_token_type type;  /* none/if/else/elif/ifndef/elifdef/elifndef */
		ac_location loc;
		bool was_enabled;         /* Once this value is non-zero this mean we can skip all else/elif/elifdef/elifndef. */
	};
	/* Only allow MAX_DEPTH of nested #if/#else */
	struct branch_state if_else_stack[ac_pp_branch_MAX_DEPTH];
	int if_else_level;
	
	/* Buffer to combine include paths from #include directives. */
	char path_buffer[ac_pp_MAX_FILEPATH];

	/* Stack of lexer state to handle #include directives. */
	struct include_stack {
		struct ac_lex_state lex_state;
		int starting_if_else_level;
	} include_stack[ac_pp_MAX_INCLUDE_DEPTH];

	int include_stack_depth;
};

void ac_pp_init(ac_pp* pp, ac_manager* mgr, strv content, strv filepath);
void ac_pp_destroy(ac_pp* pp);

ac_token* ac_pp_goto_next(ac_pp* pp);

void ac_pp_preprocess(ac_pp* pp, FILE* file);
void ac_pp_preprocess_benchmark(ac_pp* pp, FILE* file);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* AC_PREPROCESSOR_H */