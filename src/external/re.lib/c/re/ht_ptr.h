#ifndef RE_HT_PTR_H
#define RE_HT_PTR_H

/* Hash table made to hold pointers. */

#include "ht.h"

typedef struct ht_ptr_handle ht_ptr_handle;
struct ht_ptr_handle {
    void* ptr;
};

HT_API void ht_ptr_init(ht* h, ht_hash_function_t hash, ht_predicate_t items_are_same);

HT_API void ht_ptr_destroy(ht* h);

/* Insert new pointer. */
HT_API void ht_ptr_insert(ht* h, void* ptr);

/* Return item from a key. NULL is returned if nothing was found. */
HT_API void* ht_ptr_get(ht* h, void* key_ptr);

/* Remove pointer. Return true if it was removed. */
HT_API bool ht_ptr_remove(ht* h, void* ptr);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* RE_HT_PTR_H */

#if defined(HT_IMPLEMENTATION)

static void swap_ptrs(ht_ptr_handle* left, ht_ptr_handle* right)
{
    ht_ptr_handle tmp = *left;
    *left = *right;
    *right = tmp;
}

HT_API void
ht_ptr_init(ht* h, ht_hash_function_t hash, ht_predicate_t items_are_same)
{
    ht_init(h,
        sizeof(ht_ptr_handle),
        hash,
        items_are_same,
        (ht_swap_function_t)swap_ptrs,
        0);
}

HT_API void
ht_ptr_destroy(ht* h)
{
    ht_destroy(h);
}

HT_API void
ht_ptr_insert(ht* h, void* ptr)
{
    ht_ptr_handle handle = { ptr };
    ht_insert(h, &handle);
}

HT_API void*
ht_ptr_get(ht* h, void* key_ptr)
{
    ht_ptr_handle handle = { key_ptr };
    ht_ptr_handle* result = ht_get_item(h, &handle);

    return result
        ? result->ptr
        : NULL;
}

HT_API bool
ht_ptr_remove(ht* h, void* ptr)
{
    ht_ptr_handle handle = { ptr };
    return ht_erase(h, &handle);
}

#endif /* defined(HT_IMPLEMENTATION) */