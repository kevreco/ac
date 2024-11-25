#ifndef AC_PREPROCESSOR_H
#define AC_PREPROCESSOR_H

#include "lexer.h"
#include "re_lib.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ac_macro ac_macro;

typedef darrT(ac_token) darr_token;

typedef struct ac_token_list ac_token_list;
struct ac_token_list {
	darr_token* tokens;
	size_t i;        /* Next token to pick up. */
	ac_macro* macro; /* Not null if it's coming from a macro. */
	int macro_depth; /* Macro depth of the list of token. To avoid counting argument delimiter if the tokens are coming from expanded macros. */
};

typedef struct ac_pp ac_pp;
struct ac_pp {
	ac_manager* mgr;
	ac_lex lex;        /* Main lexer. */
	ac_lex concat_lex; /* Lexer for string concatenation. */
	ht macros;         /* Hash table containing macros. */

	/* Stack of list of tokens. It's mostly used for macro but we should be able to add tokens if we peek some next ones. */
	darrT(ac_token_list) stack;
	ac_token expanded_token;    /* Last token retrieved from the stack. */
	ac_token* current_token;
	
	/* previous_was_space: to know if the previous token was a space. This is only relevant in case of macro expansion. */
	bool previous_was_space;    
	dstr concat_buffer;         /* Concatenation of token is done via tokenizing a string. */
	int macro_depth;            /* Macro depth is not currently needed, it's mostly for inspectiong purpose. */
	darr_token buffer_for_peek; /* Sometimes we need to peek some tokens and send them on the stack. */
};

void ac_pp_init(ac_pp* pp, ac_manager* mgr, strv content, const char* filepath);
void ac_pp_destroy(ac_pp* pp);

ac_token* ac_pp_goto_next(ac_pp* pp);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* AC_PREPROCESSOR_H */