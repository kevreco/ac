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
	ht macros;         /* Hash table containing macros. */

	/* Stack of list of tokens. It's mostly used for macro but we should be able to add tokens if we peek some next ones. */
	darrT(ac_token_cmd) cmd_stack;
	/* Array of undefined macro.
	   It's possible to undefine and redefine a macro while it's being expanded.
	   We store the undefined macro here and destroy them when the preprocessor is destroyed.
	   @OPT: find a better way to destroy them. It's a bit wastefull to keep instances of macro that we won't use. */
	darrT(ac_macro*) undef_macros;

	ac_token* current_token;

	dstr concat_buffer;         /* Concatenation of token is done via tokenizing a string. */
	int macro_depth;            /* Macro depth is not currently needed, it's mostly for inspectiong purpose. */
	darr_token buffer_for_peek; /* Sometimes we need to peek some tokens and send them on the stack. */
	int counter_value;
};

void ac_pp_init(ac_pp* pp, ac_manager* mgr, strv content, const char* filepath);
void ac_pp_destroy(ac_pp* pp);

ac_token* ac_pp_goto_next(ac_pp* pp);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* AC_PREPROCESSOR_H */