#ifndef AC_PREPROCESSOR_H
#define AC_PREPROCESSOR_H

#include "lexer.h"
#include "re_lib.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ac_pp ac_pp;
struct ac_pp {
	ac_manager* mgr;
	ac_lex lex;
};

void ac_pp_init(ac_pp* pp, ac_manager* mgr, strv content, const char* filepath);
void ac_pp_destroy(ac_pp* pp);

const ac_token* ac_pp_goto_next(ac_pp* pp);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* AC_PREPROCESSOR_H */