#ifndef AC_PREPROCESSOR_H
#define AC_PREPROCESSOR_H

#include "lexer.h"
#include "re_lib.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ac_macro ac_macro;

typedef struct ac_token_node ac_token_node;
struct ac_token_node {
	ac_token token;
	bool previous_was_space; /* To know if the previous token was a space. */
	ac_token_node* next;
};

typedef struct ac_pp ac_pp;
struct ac_pp {
	ac_manager* mgr;
	ac_lex lex;
	ht macros;  /* Hash table containing macros. */

	darrT(ac_token_node) expanded_tokens_queue;
	ac_token_node expanded_token;

	/* previous_was_space: to know if the previous token was a space. This is only relevant in case of macro expansion.
	   @TODO check if it 's only for function-like macro or even object-like macro.
		 #define foo(x) x
		 foo(1+2)         // Will be displayed as "1+2"
		 foo(1 + 2)       // Will be displayed as "1 + 2"
		 foo(  1  +  2 )  // Will be displayed as "1 + 2"
	*/
	bool previous_was_space; 
};

void ac_pp_init(ac_pp* pp, ac_manager* mgr, strv content, const char* filepath);
void ac_pp_destroy(ac_pp* pp);

ac_token* ac_pp_goto_next(ac_pp* pp);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* AC_PREPROCESSOR_H */