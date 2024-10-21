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

typedef struct ac_token_list ac_token_list;
struct ac_token_list {
	ac_token_node* node;
	/* @TODO use this macro field to hide/unhide a macro when the list is pushed/poped from the stack. */
	ac_macro* macro; /* Not null if it's coming from a macro.  */
};

typedef struct ac_pp ac_pp;
struct ac_pp {
	ac_manager* mgr;
	ac_lex lex;        /* Main lexer. */
	ac_lex concat_lex; /* Lexer for string concatenation. */
	ht macros;  /* Hash table containing macros. */

	/* Stack of list of tokens. It's mostly used for macro but we should be able to add tokens if we peek some next ones. */
	darrT(ac_token_list) stack;
	ac_token_node expanded_token; /* last token retrieved from the stack. */
	ac_token* current_token;
	/* previous_was_space: to know if the previous token was a space. This is only relevant in case of macro expansion.
	   @TODO check if it's only for function-like macro or even object-like macro.
		 #define foo(x) x
		 foo(1+2)         // Will be displayed as "1+2"
		 foo(1 + 2)       // Will be displayed as "1 + 2"
		 foo(  1  +  2 )  // Will be displayed as "1 + 2"
	*/
	bool previous_was_space;
	dstr concat_buffer;
};

void ac_pp_init(ac_pp* pp, ac_manager* mgr, strv content, const char* filepath);
void ac_pp_destroy(ac_pp* pp);

ac_token* ac_pp_goto_next(ac_pp* pp);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* AC_PREPROCESSOR_H */