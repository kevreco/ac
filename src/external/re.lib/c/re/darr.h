/* darr.h - v0.3 - kevreco - CC0 1.0 Licence(public domain) */

/*

 Dynamic (Strechy) Array - a C partially equivalent of std::vector


NOTES:
=====

- Allocators are not implemented.

CHANGES (DD/MM/YYYY):
====================

-

TODO:
====

- check all tag "@TODO" in this file

*/

#ifndef RE_DARR_H
#define RE_DARR_H

#ifdef DARR_USE_CONFIG
#include "darr_config.h"
#endif

#ifndef DARR_API
    #ifdef DARR_STATIC
    #define DARR_API static
    #else
    #define DARR_API extern
    #endif
#endif

#ifndef DARR_ASSERT
#include <assert.h>
#define DARR_ASSERT   assert
#endif

#ifndef DARR_INTERNAL /* use for internal functions */
#define DARR_INTERNAL static 
#endif

#ifndef DARR_MALLOC
#define DARR_MALLOC malloc
#endif

#ifndef DARR_FREE
#define DARR_FREE free
#endif

#ifndef DARR_MEMCPY
#define DARR_MEMCPY memcpy
#endif
#ifndef DARR_MEMMOVE
#define DARR_MEMMOVE memmove
#endif
#ifndef DARR_MEMCMP
#define DARR_MEMCMP memcmp
#endif
#ifndef DARR_MEMSET
#define DARR_MEMSET memset
#endif

#ifndef DARR_SIZE_T
#define DARR_SIZE_T size_t
#endif

#ifndef DARR_MIN_ALLOC
#define DARR_MIN_ALLOC 8
#endif

#define DARR_MIN(a_, b_) ((a_) < (b_) ? (a_) : (b_))
#define DARR_MAX(a_, b_) ((a_) < (b_) ? (b_) : (a_))

#include "stdlib.h" /* size_t malloc free */
#include "string.h" /* strlen, memcpy, memmove, memset */
#include <stddef.h> /* ptrdiff_t */

/*  
*    each_arr_range:  it_ should be T*, arr_range is passed by value
*    Use case:
*
*    T* item;
*    for (each_arr_range(item, arr_range)
*    {
*    }
*/

#ifdef __cplusplus
#define each_arr_range(it_, arr_range_) \
    (it_) = (decltype(it_))arr_range_.begin; (void*)(it_) < (decltype(it_))arr_range_.end; ++(it_)
#else
#define each_arr_range(it_, arr_range_) \
    (it_) = (void*)((arr_range_).begin); (void*)(it_) < (void*)((arr_range_).end); ++(it_)
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef char darr_byte_t;
typedef darr_byte_t* darr_it;
typedef unsigned int darr_bool;
typedef DARR_SIZE_T  darr_size_t;

static const darr_size_t DARR_NPOS = (darr_size_t)-1;

/*-----------------------------------------------------------------------*/
/* arr_range - API */
/*-----------------------------------------------------------------------*/

typedef struct arr_range arr_range;
struct arr_range {
    darr_byte_t* begin;
    darr_byte_t* end;
};

DARR_API darr_bool arr_range_not_empty(arr_range range);
DARR_API void* arr_range_first(arr_range range);

/*-----------------------------------------------------------------------*/
/* arr_view - API */
/*-----------------------------------------------------------------------*/

typedef struct arr_view arr_view;
struct arr_view {
    darr_size_t  size;
    darr_byte_t* data;
};

#define arr_viewT(type_) union { arr_view base; struct { darr_size_t size; type_ *data; } arrv; }

#define EACH_ARR_VIEW(i, arr) \
	(i) = &((arr).data[0]); (i) < &((arr).data[(arr).size]); ++(i)

/* Create an arr_view and initialize all values to zero */
DARR_API arr_view  arr_view_make();
/* Create an arr_view from content */
DARR_API arr_view  arr_view_make_from(const void* data, darr_size_t count);
/* Wrapper aournd arr_view_make_from */
DARR_API arr_view  arr_view_make_from_view(arr_view v);

DARR_API darr_bool arr_view_empty(arr_view v);

DARR_API void      arr_view_reset(arr_view* v);

DARR_API void      arr_view_assign(arr_view* v, const void* data, darr_size_t size);

DARR_API int       arr_view_compare(arr_view v, const darr_byte_t* data, darr_size_t size, darr_size_t sizeof_value);
DARR_API int       arr_view_compare_view(arr_view v, arr_view other, darr_size_t sizeof_value);
DARR_API darr_bool arr_view_equals(arr_view v, arr_view other, darr_size_t sizeof_value);
DARR_API darr_bool arr_view_less_than(arr_view v, arr_view other, darr_size_t sizeof_value);
DARR_API darr_bool arr_view_greater_than(arr_view v, arr_view other, darr_size_t sizeof_value);

DARR_API void* arr_view_begin(arr_view v);
DARR_API void* arr_view_end(arr_view v, darr_size_t sizeof_value);

/* Get first value */
DARR_API void* arr_view_front(arr_view v);
/* Get last value */
DARR_API void* arr_view_back(arr_view v, darr_size_t sizeof_value);

DARR_API darr_size_t arr_view_find_value(arr_view v, const darr_byte_t* value, darr_size_t sizeof_value);

DARR_API void arr_view_swap(arr_view* v, arr_view* other);

/*-----------------------------------------------------------------------*/
/* darr - struct */
/*-----------------------------------------------------------------------*/

/* Example: int less(const void* a, const void* b) { return a < b; } */
typedef int (*darr_comp_t)(const void* left_ptr, const void* right_ptr);
typedef darr_bool(*darr_predicate_t)(const void* left_ptr, const void* right_ptr);

typedef struct darr darr;
struct darr {
    darr_size_t size;         /* Item count */
    darr_size_t capacity;     /* Capacity of buffer */
    darr_byte_t* data;        /* Buffer pointer */
    darr_size_t sizeof_value; /* Byte size of each item */
};

#define darrT(type)                   \
    union {                           \
        darr base;                    \
        struct  {                     \
            darr_size_t size;         \
            darr_size_t capacity;     \
            type* data;               \
            darr_size_t sizeof_value; \
        } darr;                       \
    }

/*-----------------------------------------------------------------------*/
/* darr - API */
/*-----------------------------------------------------------------------*/

DARR_API void darr_init(darr* arr, darr_size_t _sizeof_value);
DARR_API void darr_destroy(darr* arr);
DARR_API void darr_clear(darr* arr);

DARR_API void darr_swap(darr* arr, darr* other);

DARR_API arr_view darr_to_view(darr* arr);

DARR_API void* darr_front(darr* view);
DARR_API void* darr_back(darr* view);

DARR_API void darr_set(darr* array, darr_size_t index, const void* value);

DARR_API void* darr_ptr(const darr* array, darr_size_t index);
DARR_API void darr_get_value(const darr* array, darr_size_t index, void* data);

DARR_API void darr_reserve(darr* s, darr_size_t new_value_count);

DARR_API void darr_append(darr* arr, const void* value, darr_size_t count);
DARR_API void darr_append_value(darr* arr, const void* value);
DARR_API void darr_append_view(darr* arr, arr_view view);
DARR_API void darr_append_darr(darr* arr, const darr* other);

/* Append from a specific index, only the excessive data will make the array to grow. */
/* It will overwrite any data from the index position */
DARR_API void darr_append_from(darr* arr, darr_size_t index, const void* data, darr_size_t count);
DARR_API void darr_append_view_from(darr* arr, darr_size_t index, arr_view view);
DARR_API void darr_append_darr_from(darr* arr, darr_size_t index, const darr* other);

/* Removes the last element from the array */
DARR_API void darr_pop_back(darr* arr);
/* Removes the first element from the array */
DARR_API void darr_pop_front(darr* arr);

/* Assign other values to the array */
DARR_API void darr_assign(darr* arr, const void* value, const darr_size_t size);
/* warpper around darr_assign */
DARR_API void darr_assign_view(darr* arr, arr_view v);
/* warpper around darr_assign */
DARR_API void darr_assign_darr(darr* arr, const darr* other);
DARR_API void darr_assign_nvalue(darr* arr, darr_size_t count, const void* value);

/* @TODO Implement darr_shrink_to_fit */
/* Reduces memory usage by freeing unused memory */
/*void          darr_shrink_to_fit(darr* array);  */
DARR_API int         darr_empty(const darr* array);
DARR_API darr_size_t darr_size(const darr* array);
DARR_API darr_size_t darr_length(const darr* array);
DARR_API darr_size_t darr_capacity(const darr* array);

DARR_API darr_it darr_begin(const darr* array);
DARR_API darr_it darr_end(const darr* array);

/* Special operation that add uninitialized space for new elements */
DARR_API void darr_insert_many_space(darr* arr, darr_size_t index, darr_size_t count);
DARR_API void darr_insert_one_space(darr* arr, darr_size_t index);
DARR_API void darr_append_one_space(darr* arr);

DARR_API void darr_insert_many(darr* arr, darr_size_t index, const void* values, darr_size_t count);
DARR_API void darr_insert_one(darr* arr, darr_size_t index, const void* value);
DARR_API void darr_insert_view(darr* arr, darr_size_t index, arr_view view);
DARR_API void darr_insert_darr(darr* arr, darr_size_t index, const darr* other);

/* Erase multiple element('size' count) from pos */
DARR_API void darr_erase_many(darr* arr, darr_size_t index, darr_size_t count);
/* Erase element at the index */
DARR_API void darr_erase_one(darr* arr, darr_size_t index);
/* This is a special erase that does not move the memory, it just pick the last items 
*  And erase the values we want to.
*/
DARR_API void darr_erase_many_unsorted(darr* arr, darr_size_t index, darr_size_t size);

/* Same effect as resize(0). This does not free allocated buffer. */
DARR_API void darr_clear(darr* arr);
/* Resizes the array to contain count values. This does not free allocated buffer. */
DARR_API void darr_resize(darr* arr, darr_size_t size);

DARR_API void darr_insert_one_sorted(darr* arr, const void* value, darr_predicate_t comp);
DARR_API darr_size_t darr_binary_find_predicate(const darr* arr, const void* value, darr_predicate_t comp);
DARR_API darr_size_t darr_binary_find_comp(const darr* arr, const void* value, darr_comp_t comp);

/* Returns index of lower*/
DARR_API darr_size_t darr_lower_bound_comp(const void* void_ptr, darr_size_t left, darr_size_t right, const void* value, darr_size_t value_size, darr_comp_t comp);
DARR_API darr_size_t darr_lower_bound_predicate(const void* void_ptr, darr_size_t left, darr_size_t right, const void* value, darr_size_t value_size, darr_predicate_t comp);

#define darr_push_back(a, value) \
    do { \
        darr* array_ = a; \
        void* value_ref_ = &(value); \
        darr_insert_one(array_, array_->size, value_ref_); \
    } while (0);

#define darrT_init(a) \
    darr_init(&(a)->base, sizeof(*(a)->darr.data))

#define darrT_destroy(a) \
    darr_destroy(&(a)->base)

#define darrT_insert(a, index, value) \
    do {  \
        darr_insert_one_space(&(a)->base, (index)); \
		(a)->darr.data[(index)] = value; \
	} while (0);

#define darrT_pop_back(a) \
    ((a)->darr.data[(a)->darr.size - 1], (a)->darr.size -= 1)

#define darrT_pop_front(a) \
     ((a)->darr.data[0]), darr_pop_front((&(a)->base))

#define darrT_push_back(a, value) \
    do {  \
        int last__ = (a)->darr.size; \
        darr_insert_one_space(&(a)->base, last__); \
		(a)->darr.data[last__] = value; \
	} while (0);

#define darrT_at(a, index) \
    ((a)->darr.data[index])

#define darrT_set(a, index, value) \
    (a)->darr.data[index] = (value)

#define darrT_ptr(a, index) \
    (&(a)->darr.data[index])

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* RE_DARR_H */

#if defined(DARR_IMPLEMENTATION)

/*-----------------------------------------------------------------------*/
/* arr_range */
/*-----------------------------------------------------------------------*/

DARR_API darr_bool
arr_range_not_empty(arr_range range)
{
    return range.begin != range.end;
}

DARR_API void*
arr_range_first(arr_range range)
{
    return range.begin;
}

/*-----------------------------------------------------------------------*/
/* darr - Private */
/*-----------------------------------------------------------------------*/

DARR_INTERNAL void darr__reserve_no_preserve_data(darr* s, darr_size_t new_value_count);
DARR_INTERNAL void darr__reserve_internal(darr* s, darr_size_t new_value_count, darr_bool _preserve_data);

DARR_INTERNAL darr_size_t darr__growing_policy(darr* array, darr_size_t needed_size);
DARR_INTERNAL int   darr__is_allocated(darr* s);

DARR_INTERNAL const darr_it darr__begin(const darr* arr);
DARR_INTERNAL const darr_it darr__end(const darr* arr);
 
/*-----------------------------------------------------------------------*/
/* arr_view - API Implementation */
/*-----------------------------------------------------------------------*/

DARR_API arr_view
arr_view_make()
{
    arr_view sv;
    sv.data = 0;
    sv.size = 0;
    return sv;
}

DARR_API arr_view
arr_view_make_from(const void* data, darr_size_t count)
{
    arr_view sv;
    sv.data = (darr_byte_t*)data;
    sv.size = count;
    return sv;
}

DARR_API arr_view
arr_view_make_from_view(arr_view v)
{
    arr_view sv;
    sv.data = v.data;
    sv.size = v.size;
    return sv;
}

DARR_API darr_bool
arr_view_empty(arr_view v)
{
    return v.size == 0;
}

DARR_API void
arr_view_reset(arr_view* v)
{
    v->size = 0;
    v->data = 0;
}

DARR_API void
arr_view_assign(arr_view* v, const void* data, darr_size_t size)
{
    v->size = size;
    v->data = (darr_byte_t *)data;
}

DARR_API int
arr_view_compare(arr_view v, const darr_byte_t* data, darr_size_t size, darr_size_t sizeof_element)
{
    return DARR_MEMCMP(v.data,  data, size * sizeof_element);
}

DARR_API int
arr_view_compare_view(arr_view v, arr_view other, darr_size_t sizeof_element)
{
    return arr_view_compare(v, other.data, other.size, sizeof_element);
}

DARR_API darr_bool
arr_view_equals(arr_view v, arr_view other, darr_size_t sizeof_element)
{
    return  v.size != other.size
        ? (darr_bool)0
        : arr_view_compare_view(v, other, sizeof_element) == 0;
}

DARR_API darr_bool
arr_view_less_than(arr_view v, arr_view other, darr_size_t sizeof_element)
{
    return arr_view_compare_view(v, other, sizeof_element) < 0;
}

DARR_API darr_bool
arr_view_greater_than(arr_view v, arr_view other, darr_size_t sizeof_element)
{
    return arr_view_compare_view(v, other, sizeof_element) > 0;
}

DARR_API void*
arr_view_begin(arr_view v)
{
    return (darr_it)(v.data);
}

DARR_API void*
arr_view_end(arr_view v, darr_size_t sizeof_value)
{
    return (darr_it)(v.data + (v.size * sizeof_value));
}

DARR_API void*
arr_view_front(arr_view v)
{
    DARR_ASSERT(v.size > 0);
    return v.data;
}

DARR_API void*
arr_view_back(arr_view v, darr_size_t sizeof_element)
{
    DARR_ASSERT(v.size > 0);
    return v.data + (sizeof_element * (v.size - 1));
}

DARR_API darr_size_t
arr_view_find_value(arr_view v,const darr_byte_t* value, darr_size_t sizeof_element)
{
    darr_size_t result = DARR_NPOS;

    darr_byte_t* cursor = v.data;
    for (darr_size_t i = 0; i < v.size; ++i)
    {
        int res = DARR_MEMCMP(cursor, value, sizeof_element);
        int found = res == 0;
        if (found) {
            return result;
        }
        cursor += sizeof_element;
    }

    return result;
}

DARR_API void
arr_view_swap(arr_view* v, arr_view* other)
{
    const arr_view tmp = *v;
    *v = *other;
    *other = tmp;
}

/*-----------------------------------------------------------------------*/
/* darr - API Implementation */
/*-----------------------------------------------------------------------*/

/* returns 150% of the capacity or use the DARR_MIN_ALLOC value */
static int
darr__get_new_capacity(darr* arr, darr_size_t needed_size)
{
    darr_size_t minimum_size = DARR_MAX(DARR_MIN_ALLOC, arr->capacity + (arr->capacity / 2));
    return DARR_MAX(needed_size, minimum_size);
}

static void
darr__grow_if_needed(darr* arr, darr_size_t needed)
{
    if (needed > arr->capacity)
        darr_reserve(arr, darr__get_new_capacity(arr, needed));
}

static void
darr__grow_if_needed_discard(darr* arr, darr_size_t needed)
{
    if (needed > arr->capacity)
        darr__reserve_no_preserve_data(arr, darr__get_new_capacity(arr, needed));
}

DARR_API void
darr_init(darr* arr, darr_size_t sizeof_value)
{
    DARR_ASSERT(sizeof_value > 0);

    arr->size = 0;
    arr->data = 0;
    arr->capacity = 0;
    arr->sizeof_value = sizeof_value;
}

DARR_API void
darr_destroy(darr* arr)
{
    if (darr__is_allocated(arr))
        DARR_FREE(arr->data);

    darr_init(arr, arr->sizeof_value);
}

DARR_API void
darr_clear(darr* arr)
{
    darr_resize(arr, 0);
}

DARR_API void
darr_swap(darr* arr, darr* other)
{
    const darr tmp = *arr;
    *arr = *other;
    *other = tmp;
}

DARR_API arr_view
darr_to_view(darr* arr)
{
    return arr_view_make_from(arr->data, arr->size);
}

DARR_API void
darr_resize(darr* arr, darr_size_t size)
{
    if (arr->size == size)
        return;

    darr_size_t extra_count = 0;

    if (size > arr->capacity)
    {
        darr__grow_if_needed(arr, size);
        extra_count = arr->capacity - arr->size;
    }
    else if (size > arr->size)
    {
        extra_count = size - arr->size;
    }

    if (extra_count)
    {
        DARR_MEMSET(arr->data + (arr->size * arr->sizeof_value), 0, extra_count * arr->sizeof_value);
    }

    arr->size = size;
}

DARR_API void*
darr_front(darr* arr)
{
    return arr_view_front(darr_to_view(arr));
}

DARR_API void*
darr_back(darr* arr)
{
   return arr_view_back(darr_to_view(arr), arr->sizeof_value);
}

DARR_API void
darr_set(darr* arr, darr_size_t index, const void* value)
{
    DARR_ASSERT(index >= 0);
    DARR_ASSERT(index < arr->size);
    
    DARR_MEMCPY(darr_ptr(arr, index), value, arr->sizeof_value);
}

DARR_API void*
darr_ptr(const darr* arr, darr_size_t index)
{
    DARR_ASSERT(index >= 0);
    DARR_ASSERT(
        index < arr->size /* Within accessible item range */
        || index == arr->size /* Also allow getting the item at the end */
    );

    return arr->data + (index * arr->sizeof_value);
}

DARR_API void
darr_get_value(const darr* arr, darr_size_t index, void* value)
{
    DARR_ASSERT(index >= 0);
    DARR_ASSERT(index < arr->size);

    DARR_MEMCPY(value, arr->data + (index * arr->sizeof_value), arr->sizeof_value);
}

DARR_API void
darr_reserve(darr* s, darr_size_t new_value_count)
{
    darr_bool preserve_data = (darr_bool)1;
    darr__reserve_internal(s, new_value_count, preserve_data);
}

DARR_API void
darr_append(darr* arr, const void* value, darr_size_t size)
{
    darr_append_from(arr, arr->size, value, size);
}

DARR_API void
darr_append_value(darr* arr, const void* value)
{
    darr_append(arr, value, 1);
}

DARR_API void
darr_append_view(darr* arr, arr_view view)
{
    darr_append(arr, view.data, view.size);
}

DARR_API void
darr_append_darr(darr* arr, const darr* other)
{
    darr_append(arr, other->data, other->size);
}

DARR_API void
darr_append_from(darr* arr, darr_size_t index, const void* data, darr_size_t count)
{
    darr__grow_if_needed(arr, index + count);

    DARR_MEMCPY(arr->data + (index * arr->sizeof_value), data, (count * arr->sizeof_value));
    arr->size = index + count;
}

DARR_API void
darr_append_view_from(darr* arr, darr_size_t index, arr_view view)
{
    darr_append_from(arr, index, view.data, view.size);
}

DARR_API void
darr_append_darr_from(darr* arr, darr_size_t index, const darr* other)
{
    darr_append_from(arr, index, other->data, other->size);
}

DARR_API void
darr_insert_many_space(darr* arr, darr_size_t index, darr_size_t count)
{
    darr_size_t count_to_move = arr->size - index;

    DARR_ASSERT(arr != NULL);
    DARR_ASSERT(count != 0);
    DARR_ASSERT(index >= 0);
    DARR_ASSERT(index <= arr->size);

    darr__grow_if_needed(arr, arr->size + count);

    if (count_to_move > 0)
    {
        DARR_MEMMOVE(darr_ptr(arr, index + count), darr_ptr(arr, index), count_to_move * arr->sizeof_value);
    }

    arr->size += count;
}

DARR_API void
darr_insert_one_space(darr* arr, darr_size_t index)
{
    darr_insert_many_space(arr, index, 1);
}

DARR_API void
darr_append_one_space(darr* arr)
{
    darr_insert_many_space(arr, arr->size, 1);
}

DARR_API void
darr_insert_many(darr* arr, darr_size_t index, const void* value, darr_size_t count)
{
    darr_insert_many_space(arr, index, count);

    DARR_MEMCPY(darr_ptr(arr, index), value, count * arr->sizeof_value);
}

DARR_API void
darr_insert_one(darr* arr, darr_size_t index, const void* value)
{
    darr_insert_many(arr, index, value, 1);
}

DARR_API void
darr_insert_view(darr* arr, darr_size_t index, arr_view view)
{
    darr_insert_many(arr, index, view.data, view.size);
}

DARR_API void
darr_insert_darr(darr* arr, darr_size_t index, const darr* other)
{
    darr_insert_many(arr, index, other->data, other->size);
}

DARR_API void
darr_erase_many(darr* arr, darr_size_t index, darr_size_t count)
{
    DARR_ASSERT(arr);
    DARR_ASSERT(index >= 0);
    DARR_ASSERT(count >= 0);
    DARR_ASSERT(index < arr->size);
    DARR_ASSERT(count <= arr->size);
    DARR_ASSERT(index + count <= arr->size);

    if (!count)
        return;

    DARR_MEMMOVE(
        darr_ptr(arr, index), 
        darr_ptr(arr, index + count),
        (arr->size - (index + count)) * arr->sizeof_value
    );

    arr->size -= count;
}

DARR_API void
darr_erase_one(darr* arr, darr_size_t index)
{
    darr_erase_many(arr, index, 1);
}

DARR_API void
darr_erase_many_unsorted(darr* arr, darr_size_t index, darr_size_t count)
{
    DARR_ASSERT(arr);
    DARR_ASSERT(index >= 0);
    DARR_ASSERT(index <= arr->size);
    DARR_ASSERT(count >= 0);
    DARR_ASSERT(count < arr->size);
    DARR_ASSERT(index + count < arr->size);

    /* only copy items if they are not the last ones, otherwise only decrementing the counter is enough. */
    if (index + count < arr->size)
    {
        DARR_MEMCPY(
            darr_ptr(arr, index),
            darr_ptr(arr, arr->size - count),
            count * arr->sizeof_value);
    }

    arr->size -= count;
}

DARR_API void
darr_pop_back(darr* arr)
{
    DARR_ASSERT(arr->size > 0);
    darr_erase_one(arr, arr->size - 1);
}

DARR_API void
darr_pop_front(darr* arr)
{
    DARR_ASSERT(arr->size > 0);
    darr_erase_one(arr, 0);
}

DARR_API void
darr_assign(darr* arr, const void* value, darr_size_t count)
{
    darr_clear(arr);

    darr__grow_if_needed(arr, count);

    arr->size = count;

    DARR_MEMCPY(arr->data, value, count * arr->sizeof_value);
}

DARR_API void
darr_assign_view(darr* arr, arr_view v)
{
    darr_assign(arr, v.data, v.size);
}

DARR_API void
darr_assign_darr(darr* arr, const darr* other)
{
    darr_assign(arr, other->data, other->size);
}

DARR_API void
darr_assign_nvalue(darr* arr, darr_size_t count, const void* value)
{
    darr_clear(arr);

    darr_size_t capacity_needed = count;

    darr__grow_if_needed(arr, capacity_needed);

    darr_byte_t* cursor = arr->data;
    darr_byte_t* last = arr->data + (count * arr->sizeof_value);

    for (; cursor != last; cursor += arr->sizeof_value)
    {
        DARR_MEMCPY(cursor, value, arr->sizeof_value);
    }

    arr->size = count;
}

DARR_API int
darr_empty(const darr* array)
{
    return !array->size;
}

DARR_API darr_size_t
darr_size(const darr* array)
{
    return array->size;
}

DARR_API darr_size_t
darr_length(const darr* array)
{
    return array->size;
}

DARR_API darr_size_t
darr_capacity(const darr* array)
{
    return array->capacity;
}

DARR_API darr_it
darr_begin(const darr* array)
{
    return array->data;
}
DARR_API darr_it
darr_end(const darr* array)
{
    return array->data + (array->size * array->sizeof_value);
}

/*-----------------------------------------------------------------------*/
/* darr - Extended Implementation */
/*-----------------------------------------------------------------------*/

DARR_API void
darr_insert_one_sorted(darr* arr, const void* value, darr_predicate_t comp)
{
    darr_size_t index = darr_lower_bound_predicate(arr->data, 0, arr->size, value, arr->sizeof_value, comp);

    darr_insert_one(arr, index, value);

}

DARR_API darr_size_t
darr_binary_find_predicate(const darr* arr, const void* value, darr_predicate_t comp)
{
    darr_size_t index = darr_lower_bound_predicate(arr->data, 0, arr->size, value, arr->sizeof_value, comp);

    if (index == arr->size || comp(value, darr_ptr(arr, index)))
    {
        index = arr->size;
    }

    return index;
}

DARR_API darr_size_t
darr_binary_find_comp(const darr* arr, const void* value, darr_comp_t comp)
{
    darr_size_t index = darr_lower_bound_comp(arr->data, 0, arr->size, value, arr->sizeof_value, comp);

    if (index == arr->size || comp(value, darr_ptr(arr, index)) < 0)
    {
        index = arr->size;
    }

    return index;
}

DARR_API darr_size_t
darr_lower_bound_comp(const void* void_ptr, darr_size_t left, darr_size_t right, const void* value, darr_size_t sizeof_value, darr_comp_t comp)
{
    const char* ptr = (const char*)void_ptr;
    darr_size_t count = right - left;
    darr_size_t step;
    darr_size_t mid; /* index of the found value */

    while (count > 0) {
        step = count >> 1; /* count divide by two using bit shift */

        mid = left + step;
       
        if (comp(ptr + (mid * sizeof_value), value) < 0) {
            left = mid + 1;
            count -= step + 1;
        }
        else {
            count = step;
        }
    }
    return left;
}

DARR_API darr_size_t
darr_lower_bound_predicate(const void* void_ptr, darr_size_t left, darr_size_t right, const void* value, darr_size_t sizeof_value, darr_predicate_t comp)
{
    const char* ptr = (const char*)void_ptr;
    darr_size_t count = right - left;
    darr_size_t step;
    darr_size_t mid; /* index of the found value */

    while (count > 0) {
        step = count >> 1; /* count divide by two using bit shift */

        mid = left + step;

        if (comp(ptr + (mid * sizeof_value), value) != 0) {
            left = mid + 1;
            count -= step + 1;
        }
        else {
            count = step;
        }
    }
    return left;
}

/*-----------------------------------------------------------------------*/
/* darr - Private Implementation */
/*-----------------------------------------------------------------------*/

DARR_INTERNAL const darr_it
darr__begin(const darr* arr)
{
    return (const darr_it)darr_begin(arr);
}

DARR_INTERNAL const darr_it
darr__end(const darr* arr)
{
    return (const darr_it)darr_end(arr);
}

DARR_INTERNAL void
darr__reserve_no_preserve_data(darr* arr, darr_size_t new_value_count)
{
    darr_bool preserve_data = (darr_bool)0;
    darr__reserve_internal(arr, new_value_count, preserve_data);
}

DARR_INTERNAL void
darr__reserve_internal(darr* arr, darr_size_t new_value_count, darr_bool preserve_data)
{
    if (new_value_count <= arr->capacity)
        return;

    darr_size_t  memory_capacity = new_value_count * arr->sizeof_value;

    darr_byte_t* new_data = (darr_byte_t*)DARR_MALLOC(memory_capacity);

    DARR_ASSERT(new_data);

    if (preserve_data && arr-> size)
    {
        DARR_MEMCPY(new_data, arr->data, arr->size * arr->sizeof_value);
    }

    if (darr__is_allocated(arr))
        DARR_FREE(arr->data);

    arr->data = new_data;
    arr->capacity = new_value_count;
} 

DARR_INTERNAL size_t
darr__growing_policy(darr* arr, darr_size_t needed_size)
{
    /* If the array is not initialize, set the minimal size.
    *  Otherwise increase the size by 50%
    */
    darr_size_t new_capacity = !arr->capacity ? DARR_MIN_ALLOC : (arr->capacity + (arr->capacity / 2));

    /* Use the greatest of both needed_size and new_capacity */
    return new_capacity > needed_size ? new_capacity : needed_size;

}

DARR_INTERNAL int
darr__is_allocated(darr* arr)
{
    return arr->data != NULL;
}

#endif /* DARR_IMPLEMENTATION */

