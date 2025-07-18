#ifndef AC_ALLOC_H
#define AC_ALLOC_H

#include "re_lib.h" /* re_arena */
#include "ac.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef union { void* ptr; int id; } ac_handle;

typedef void* (*ac_malloc)(ac_handle user_data, void* old, size_t byte_size);
typedef void (*ac_free)(ac_handle user_data, void* old);

typedef struct ac_allocator ac_allocator;
struct ac_allocator {
    ac_handle user_data;
    ac_malloc malloc;
    ac_free free;
};

typedef struct ac_allocator_arena ac_allocator_arena;
struct ac_allocator_arena {
    ac_allocator allocator;
    re_arena arena;
};

void ac_allocator_init(ac_allocator* a, ac_malloc malloc, ac_free free, ac_handle handle);
void ac_allocator_init_with_default(ac_allocator* a);
void ac_allocator_destroy(ac_allocator* a);

void ac_allocator_arena_init(ac_allocator_arena* a, size_t check_min_capacity);
void ac_allocator_arena_destroy(ac_allocator_arena* a);

void* ac_allocator_allocate(ac_allocator* a, size_t byte_size);
void ac_allocator_free(ac_allocator* a, void *ptr);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* AC_ALLOC_H */