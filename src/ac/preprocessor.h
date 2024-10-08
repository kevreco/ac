#ifndef AC_PREPROCESSOR_H
#define AC_PREPROCESSOR_H

#include "lexer.h"
#include "re_lib.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ac_macro ac_macro;
typedef struct ac_token_node ac_token_node;

typedef struct ac_pp ac_pp;
struct ac_pp {
	ac_manager* mgr;
	ac_lex lex;
	ht macros;  /* Hash table containing macros. */

	ac_token_node* expanded_tokens; /* Tokens expanded from macro. This is the head of a linked list. */
};

void ac_pp_init(ac_pp* pp, ac_manager* mgr, strv content, const char* filepath);
void ac_pp_destroy(ac_pp* pp);

const ac_token* ac_pp_goto_next(ac_pp* pp);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* AC_PREPROCESSOR_H */