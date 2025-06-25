#include "alloc.h"

static void* ac_default_malloc(ac_handle handle, void* old, size_t size);
static void  ac_default_free(ac_handle handle, void* ptr);
static void* ac_arena_malloc(ac_handle handle, void* old, size_t size);
static void  ac_arena_free(ac_handle handle, void* ptr);

void
ac_allocator_init(ac_allocator* a, ac_malloc malloc, ac_free free, ac_handle handle)
{
    memset(a, 0, sizeof(ac_allocator));
    a->malloc = malloc;
    a->free = free;
    a->user_data = handle;
}

void
ac_allocator_destroy(ac_allocator* a)
{
    memset(a, 0, sizeof(ac_allocator));
}

void
ac_allocator_init_with_default(ac_allocator* a)
{
    ac_handle h = { 0 };
    ac_allocator_init(a, ac_default_malloc, ac_default_free, h);
}

void
ac_allocator_arena_init(ac_allocator_arena* a, size_t check_min_capacity)
{
    re_arena_init(&a->arena, check_min_capacity);
    ac_handle h;
    h.ptr = a;
    ac_allocator_init(&a->allocator, ac_arena_malloc, ac_arena_free, h);
}

void
ac_allocator_arena_destroy(ac_allocator_arena* a)
{
    re_arena_destroy(&a->arena);
    ac_allocator_destroy(&a->allocator);
}

void*
ac_allocator_allocate(ac_allocator* a, size_t byte_size)
{
    AC_ASSERT(a);
    void* ptr = a->malloc(a->user_data, NULL, byte_size);
    AC_ASSERT(ptr && "Could not allocate memory.");
    memset(ptr, 0, byte_size);
    return ptr;
}

void
ac_allocator_free(ac_allocator* a, void* ptr)
{
    a->free(a->user_data, ptr);
}

static void*
ac_default_malloc(ac_handle handle, void* old, size_t size)
{
    AC_UNUSED(handle);
    AC_UNUSED(old);
    return malloc(size);
}

static void
ac_default_free(ac_handle handle, void* ptr)
{
    AC_UNUSED(handle);
    free(ptr);
}

static void*
ac_arena_malloc(ac_handle handle, void* old, size_t size)
{
    AC_UNUSED(old);

    ac_allocator_arena* arena = (ac_allocator_arena*)handle.ptr;
    return re_arena_alloc(&arena->arena, size);
}

static void
ac_arena_free(ac_handle handle, void* ptr)
{
    AC_UNUSED(handle);
    AC_UNUSED(ptr);

    /* Do nothing. All memory created by the arena will be release when the arena is destroyed. */
}

