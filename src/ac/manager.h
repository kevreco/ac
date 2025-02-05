#ifndef AC_MANAGER_H
#define AC_MANAGER_H

#include "stdbool.h"

#include "alloc.h"
#include "global.h"
#include "re_lib.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ac_ident ac_ident;
typedef struct ac_ast_top_level ac_ast_top_level;

typedef struct ac_source_file ac_source_file;
struct ac_source_file {
    const char* filepath; /* The filename is used only for informational purpose, mostly to report errors. */
    strv content;
};

/* options */

enum ac_compilation_step {

    ac_compilation_step_NONE = 1 << 0,
    ac_compilation_step_PARSE = 1 << 1,
    ac_compilation_step_SEMANTIC = 1 << 2,
    ac_compilation_step_GENERATE = 1 << 3,
    ac_compilation_step_ALL = ~0,
};

typedef struct ac_options ac_options;
struct ac_options {

    enum ac_compilation_step step;

    darrT(const char*) files;            /* Files to compile. */
    strv output_extension;               /* Extension of generated c file. */
    dstr config_file_memory;             /* Config file content with line endings replaced with \0 */
    darrT(const char*) config_file_args; /* Args parsed from the config file. */

    global_options_t global;             /* Some non critical global options. They will be set to a static global_options_t later on. */

    /* @FIXME find a better way to debug the parser.
        I don't remember when I last use it so maybe it should be removed.
    */
    bool debug_parser;                   /* Will print some debugging values in the output. */ 
    bool preprocess;                     /* Print preprocess result in the standard output. */
    bool preserve_comment;               /* Also print comments while preprocessing. */
    bool reject_hex_float;               /* Prevent hex float parsing. */
};

void ac_options_init_default(ac_options* o);
void ac_options_destroy(ac_options* o);

/* manager */

typedef struct ac_manager ac_manager;
struct ac_manager {
    ac_options options;

    /*
       Arena allocator to create new ast-related objects.
       We dont't free them until this manager is destroyed.
    */
    ac_allocator_arena ast_arena;
    ac_allocator_arena identifiers_arena;
    ht identifiers; /* Hash table with all identifiers to compare them faster with a hash. */
    ht literals;    /* Hash table with all literals to compare them faster with a hash. */
    /* keep reference to destroy it. */
    ac_source_file source_file;
    ac_ast_top_level* top_level;

    /* Map (lookup) of all opened (mmapped) files. */
    darr_map opened_files;

#ifdef _WIN32
    darrT(wchar_t) wchars;
#endif
};

void ac_manager_init(ac_manager* m, ac_options* o);
void ac_manager_destroy(ac_manager* m);

bool ac_manager_load_content(ac_manager* m, char* filepath, ac_source_file* src_file);

typedef struct ac_ident_holder ac_ident_holder;
struct ac_ident_holder
{
    ac_ident* ident;
    /* enum ac_token_type */ size_t token_type;
};
/* NOTE: An ac_token is returned as result simply because we want a string view and a token type. */
ac_ident_holder ac_create_or_reuse_identifier(ac_manager* m, strv ident_text);
ac_ident_holder ac_create_or_reuse_identifier_h(ac_manager* m, strv ident_text, size_t hash);
/* Register keywords or known identifier. It helps to retrieve the type of a token from it's text value. */
void ac_register_known_identifier(ac_manager* m, ac_ident* id, /* enum ac_token_type */ size_t type);

strv ac_create_or_reuse_literal(ac_manager* m, strv literal_text);
strv ac_create_or_reuse_literal_h(ac_manager* m, strv literal_text, size_t hash);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* AC_MANAGER_H */