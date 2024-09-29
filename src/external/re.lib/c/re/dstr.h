/* dstr.h - v0.5 - kevreco - CC0 1.0 Licence(public domain)
*  Close C89 implementation of std::string
*  https://github.com/kevreco/re_lib
*/

/*
CHANGES (DD/MM/YYYY):
====================

- 03/05/2018 (0.4):
    - Add dstr_config.h
    - Add and use macros, DSTR_MALLOC, DSTR_FREE, DSTR_MIN_ALLOC, DSTR_SIZE_T, DSTR_CHAR_T
    - use dstr_size_t and dstr_char_t

- XX/XX/2016 (0.3):
  - Change dstr_find_and_replace by dstr_substitute_xxx
  - Add dstr_find_dstr, dstr_find_str and dstr_find_ch
  - Add dstr_make_from_fmt and dstr_make_from_vfmt
  - Add dstr_append_vfmt.
  - More testsuites

- 28/11/2016 (0.2):
  - Make dstr lib C89 compliant.
  - Split dstr_make(dstr_size_t capacity) into two other constructor 'dstr_make' and 'dstr_make_reserve'.
  - Add dstr_make_ref. To use non owning str
  - Add dstr_make_with_buffer. To initialize dstr with a non owning buffer (useful for stack buffer)
  - Add dstr_append_fmt.
  - More testsuites.
  - Update notes.

- 13/11/2016 (0.1) - First implementation

*/

#ifndef RE_DSTR_H
#define RE_DSTR_H

#ifndef DSTR_API
    #ifdef DSTR_STATIC
    #define DSTR_API static
    #else
    #define DSTR_API extern
    #endif
#endif

#ifndef DSTR_INTERNAL /* use for internal functions */
#define DSTR_INTERNAL static
#endif

#ifndef DSTR_MALLOC
#define DSTR_MALLOC malloc
#endif
#ifndef DSTR_FREE
#define DSTR_FREE free
#endif

#ifndef DSTR_MEMCPY
#define DSTR_MEMCPY memcpy
#endif
#ifndef DSTR_MEMMOVE
#define DSTR_MEMMOVE memmove
#endif
#ifndef DSTR_MEMCMP
#define DSTR_MEMCMP memcmp
#endif

#ifndef DSTR_MIN_ALLOC
#define DSTR_MIN_ALLOC 8
#endif

#define DSTR_MIN(a_, b_) ((a_) < (b_) ? (a_) : (b_))
#define DSTR_MAX(a_, b_) ((a_) < (b_) ? (b_) : (a_))

#ifndef DSTR_ASSERT
#define DSTR_ASSERT assert
#include <assert.h>
#endif

#ifndef DSTR_SIZE_T
#define DSTR_SIZE_T size_t
#endif

#ifndef DSTR_CHAR_T
#define DSTR_CHAR_T char
#endif

#include "strv.h"

#include <string.h> /* strlen, memcpy, memmove, memset */
#include <stdlib.h> /* size_t malloc free */
#include <stdarg.h> /* ..., va_list  */
#include <stdio.h>
#include <ctype.h>  /* isspace */
#include <stddef.h> /* ptrdiff_t  */

#ifdef __cplusplus
extern "C" {
#endif

typedef DSTR_SIZE_T     dstr_size_t;
typedef DSTR_CHAR_T     dstr_char_t;
typedef dstr_char_t*    dstr_it;
typedef int             dstr_bool;

static const dstr_size_t DSTR_NPOS = (dstr_size_t)-1;

/*-----------------------------------------------------------------------*/
/* dstr - API */
/*-----------------------------------------------------------------------*/

typedef struct dstr dstr;
struct dstr {
	dstr_size_t  size;
	dstr_char_t* data;
	dstr_size_t  capacity; /* capacity is the number of char a string can hold, the null terminating char is not counted. */
	dstr_size_t  local_buffer_size; /* @TODO try if we can use capacity for this */
};

DSTR_API void dstr_init  (dstr* s);
DSTR_API void dstr_destroy (dstr* s);

/* Non-owning reference with buffer. */
/* Another allocated buffer will be used if the capacity is reached */
/* NOTE: dstr_destroy should always be used to free the buffer in case the capacity is reached */
DSTR_API void dstr_init_from_local_buffer(dstr* s, dstr_size_t local_buffer_capacity);

/* Default constructor. Constructs empty string with dstr_init and return it. */
DSTR_API dstr dstr_make             ();
/* Create an empty string with an initial capacity */
DSTR_API dstr dstr_make_reserve     (dstr_size_t capacity);

/* Create a dstr from the data pointer and size */
DSTR_API dstr dstr_make_from        (const dstr_char_t* data, dstr_size_t size);
/* overload of dstr_make_from */
DSTR_API dstr dstr_make_from_str    (const dstr_char_t* str);
/* overload of dstr_make_from */
DSTR_API dstr dstr_make_from_char   (dstr_char_t ch);
/* overload of dstr_make_from */
DSTR_API dstr dstr_make_from_strv   (strv sv);
/* overload of dstr_make_from */
DSTR_API dstr dstr_make_from_dstr   (const dstr* str);

/* Create with a char repeated N times */
DSTR_API dstr dstr_make_from_nchar  (dstr_size_t count, dstr_char_t ch);

DSTR_API dstr dstr_make_from_fmt    (const char* fmt, ...);
DSTR_API dstr dstr_make_from_vfmt   (const char* fmt, va_list args);

DSTR_API strv dstr_to_strv(const dstr* s);

/* Use the strv counterpart */
DSTR_API int       dstr_compare(const dstr* s, strv sv);
/* Use the strv counterpart */
DSTR_API int       dstr_compare_str(const dstr* s, const dstr_char_t* str);
/* Use the strv counterpart */
DSTR_API int       dstr_compare_dstr(const dstr* s, const dstr* str);
/* Use the strv counterpart */
DSTR_API dstr_bool dstr_equals(const dstr* s, strv sv);
/* Use the strv counterpart */
DSTR_API dstr_bool dstr_equals_str(const dstr* s, const dstr_char_t* other);
/* Use the strv counterpart */
DSTR_API dstr_bool dstr_equals_dstr(const dstr* s, const dstr* str);
/* Use the strv counterpart */
DSTR_API dstr_bool dstr_less_than(const dstr* s, strv sv);
/* Use the strv counterpart */
DSTR_API dstr_bool dstr_less_than_str(const dstr* s, const dstr_char_t* str);
/* Use the strv counterpart */
DSTR_API dstr_bool dstr_greater_than(const dstr* s, strv sv);
/* Use the strv counterpart */
DSTR_API dstr_bool dstr_greater_than_str(const dstr* s, const dstr_char_t* str);

/* Access character at index with bounds checking */
DSTR_API dstr_char_t dstr_at(const dstr* s, dstr_size_t index);
/* Returns a pointer to the first character of a string */
DSTR_API dstr_char_t* dstr_data(dstr* s);
/* Returns a c string */
DSTR_API dstr_char_t* dstr_c_str(dstr* s);

/* If new_cap is greater than the current capacity(), new storage is allocated, and capacity() is made equal or greater than new_cap. */
DSTR_API void dstr_reserve(dstr* s, dstr_size_t new_string_capacity);

/* Append data from data pointer and size */
DSTR_API void dstr_append        (dstr* s, strv sv);
/* Overload of dstr_append */
DSTR_API void dstr_append_str    (dstr* s, const dstr_char_t* str);
/* Overload of dstr_append */
DSTR_API void dstr_append_char   (dstr* s, const dstr_char_t ch);
/* Overload of dstr_append */
DSTR_API void dstr_append_dstr   (dstr* s, const dstr* dstr);
DSTR_API void dstr_append_nchar  (dstr* s, dstr_size_t count, const dstr_char_t ch);

DSTR_API int dstr_append_fv (dstr* s, const char* fmt, va_list args);
DSTR_API int dstr_append_f  (dstr* s, const char* fmt, ...);

/* append string at a certain index all content after index will be lost */
DSTR_API void dstr_append_from      (dstr* s, int index, strv sv);
/* Overload of dstr_append_from */
DSTR_API void dstr_append_str_from  (dstr* s, int index, const dstr_char_t* str);
/* Overload of dstr_append_from */
DSTR_API void dstr_append_char_from (dstr*s, int index, char ch);
/* Overload of dstr_append_from */
DSTR_API void dstr_append_dstr_from (dstr* s, int index, const dstr* str);

DSTR_API int dstr_append_from_fv   (dstr* s, int index, const char* fmt, va_list args);

/* Equivalent to dstr_append_char */
DSTR_API void dstr_push_back(dstr* s, const dstr_char_t ch);

/* Replaces content with the content from a string defined the data pointer and the size */
DSTR_API void dstr_assign           (dstr* s, strv sv);
/* Overload of dstr_assign */
DSTR_API void dstr_assign_str       (dstr* s, const dstr_char_t* str);
/* Overload of dstr_assign */
DSTR_API void dstr_assign_char      (dstr* s, dstr_char_t ch);
/* Overload of dstr_assign */
DSTR_API void dstr_assign_dstr      (dstr* s, const dstr* str);
/* Overload of dstr_assign */
DSTR_API void dstr_assign_nchar     (dstr* s, dstr_size_t count, dstr_char_t ch);

DSTR_API void dstr_assign_fv        (dstr* s, const char* fmt, va_list args);
DSTR_API void dstr_assign_f         (dstr* s, const char* fmt, ...);
DSTR_API void dstr_assign_fv_nogrow (dstr* s, const char* fmt, va_list args);
DSTR_API void dstr_assign_f_nogrow  (dstr* s, const char* fmt, ...);

/* Reduces memory usage by freeing unused memory */
DSTR_API void        dstr_shrink_to_fit(dstr* s);
DSTR_API int         dstr_empty(const dstr* s);
DSTR_API dstr_size_t dstr_size(const dstr* s);
DSTR_API dstr_size_t dstr_length(const dstr* s);
DSTR_API dstr_size_t dstr_capacity(const dstr* s);

DSTR_API dstr_it dstr_begin(const dstr* s);
DSTR_API dstr_it dstr_end(const dstr* s);

DSTR_API dstr_it dstr_insert(dstr* s, const dstr_it index, strv sv);
DSTR_API dstr_it dstr_insert_str(dstr* s, const dstr_it index, const dstr_char_t* str);
DSTR_API dstr_it dstr_insert_char(dstr* s, const dstr_it index, const dstr_char_t value);
DSTR_API dstr_it dstr_insert_dstr(dstr* s, const dstr_it index, const dstr* str);

DSTR_API dstr_it dstr_erase(dstr* s, const dstr_it data, dstr_size_t size);
DSTR_API dstr_it dstr_erase_value(dstr* s, const dstr_it index);
DSTR_API dstr_it dstr_erase_at(dstr* s, dstr_size_t index);
/* Removes the last character from the string. */
DSTR_API void dstr_pop_back(dstr* s);

/* Same effect as resize(0). This does not free the allocated buffer. */
DSTR_API void dstr_clear(dstr* s);
/* Resizes the string to contain count characters. This does not free the allocated buffer. */
DSTR_API void dstr_resize               (dstr* s, dstr_size_t size);
/* Resizes and fills extra spaces with 'ch' value */
DSTR_API void dstr_resize_fill          (dstr* s, dstr_size_t size, dstr_char_t ch);

/* Replace the content between the range [index - (index + count)] with the content of 'replacing'. */
DSTR_API void dstr_replace_range       (dstr* s, dstr_size_t index, dstr_size_t count, strv replacing);

DSTR_API void dstr_copy_to  (const dstr* s, dstr* dest);
DSTR_API void dstr_swap     (dstr* s, dstr* other);

/*-----------------------------------------------------------------------*/
/* dstr - Extended API */
/*-----------------------------------------------------------------------*/

DSTR_API strv dstr_trimmed (dstr* s);

DSTR_API void dstr_replace      (dstr* s, strv to_replace, strv with);
DSTR_API void dstr_replace_dstr (dstr* s, const dstr* to_replaced, const dstr* with);
DSTR_API void dstr_replace_str  (dstr* s, const dstr_char_t* to_replaced, const dstr_char_t* with);

#define DSTR_DEFINETYPE(TYPENAME, LOCAL_BUFFER_SIZE)          \
typedef struct TYPENAME TYPENAME;                             \
struct TYPENAME {                                             \
	dstr dstr;                                                \
    char local_buffer[LOCAL_BUFFER_SIZE];                     \
};                                                            \
static inline void TYPENAME ## _init(struct TYPENAME* s)      \
{                                                             \
	dstr_init_from_local_buffer(&s->dstr, LOCAL_BUFFER_SIZE); \
}                                                             \
static inline void TYPENAME ## _destroy(struct TYPENAME* s)   \
{                                                             \
	dstr_destroy(&s->dstr);                                   \
}                                                             \
static inline void TYPENAME ## _assign_fv(struct TYPENAME* s, const char* fmt, va_list args) \
{                                                                                   \
	dstr_assign_fv(&s->dstr, fmt, args);                                            \
}                                                                                   \
static inline void TYPENAME ## _assign_f(struct TYPENAME* s, const char* fmt, ...)  \
{                                        \
	va_list args;                        \
	va_start(args, fmt);                 \
	dstr_assign_fv(&s->dstr, fmt, args); \
	va_end(args);                        \
}

DSTR_DEFINETYPE(dstr16, 16);
DSTR_DEFINETYPE(dstr32, 32);
DSTR_DEFINETYPE(dstr64, 64);
DSTR_DEFINETYPE(dstr128, 128);
DSTR_DEFINETYPE(dstr256, 256);
DSTR_DEFINETYPE(dstr512, 512);
DSTR_DEFINETYPE(dstr1024, 1024);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* RE_DSTR_H */

/*-----------------------------------------------------------------------*/
/* dstr - Private */
/*-----------------------------------------------------------------------*/

DSTR_INTERNAL void  dstr__reserve_no_preserve_data(dstr* s, dstr_size_t new_string_capacity);
DSTR_INTERNAL void  dstr__reserve_internal(dstr* s, dstr_size_t new_string_capacity, dstr_bool preserve_data);

DSTR_INTERNAL int dstr__is_allocated      (dstr* s);

DSTR_INTERNAL dstr_bool    dstr__owns_local_buffer     (dstr* s);
DSTR_INTERNAL dstr_char_t* dstr__get_local_buffer      (dstr* s);
DSTR_INTERNAL dstr_bool    dstr__is_using_local_buffer (dstr* s);

/*-----------------------------------------------------------------------*/
/* dstr - API Implementation */
/*-----------------------------------------------------------------------*/

#ifdef DSTR_IMPLEMENTATION

DSTR_INTERNAL dstr_size_t sizeof_nchar(int count) { return count * sizeof(dstr_char_t); }

/* Shared default value to ensure that s->data is always valid with a '\0' char when a dstr is initialized */
static dstr_char_t DSTR__DEFAULT_DATA[1] = { '\0' };

/* returns 150% of the capacity or use the DSTR_MIN_ALLOC value */
static int
dstr__get_new_capacity(dstr* s, dstr_size_t needed_size)
{
	dstr_size_t minimum_size = DSTR_MAX(DSTR_MIN_ALLOC, s->capacity + (s->capacity / 2));
	return DSTR_MAX(needed_size, minimum_size);
}

static void
dstr__grow_if_needed(dstr* s, dstr_size_t needed)
{
	if (needed > s->capacity)
		dstr_reserve(s, dstr__get_new_capacity(s, needed));
}

static void
dstr__grow_if_needed_discard(dstr* s, dstr_size_t needed)
{
	if (needed > s->capacity)
		dstr__reserve_no_preserve_data(s, dstr__get_new_capacity(s, needed));
}

DSTR_API void
dstr_init(dstr* s)
{
    s->size = 0;
    s->data = DSTR__DEFAULT_DATA;
	s->capacity = 0;
	s->local_buffer_size = 0;
}

DSTR_API void
dstr_destroy(dstr* s)
{
    /* dstr is initialized */
    if (dstr__is_allocated(s))
	{
        DSTR_FREE(s->data);
    }

	if (s->local_buffer_size)
	{
		s->data = dstr__get_local_buffer(s);
		s->size = 0;
		s->data[s->size] = '\0';
		
		s->capacity = s->local_buffer_size - 1;
	}
	else
	{
		dstr_init(s);
	}
}

DSTR_API void
dstr_init_from_local_buffer(dstr* s, dstr_size_t local_buffer_size)
{
	s->data = dstr__get_local_buffer(s);

	s->size = 0;
	s->data[s->size] = '\0';
	/* capacity is the number of char a string can hold(null terminating char is not counted),
	* so the capacity is equal to buffer_size - 1 in this case 
	*/
	s->capacity = local_buffer_size - 1;
	s->local_buffer_size = local_buffer_size;
}

DSTR_API dstr
dstr_make()
{
    dstr result;
    dstr_init(&result);
    return result;
}

DSTR_API dstr
dstr_make_reserve(dstr_size_t capacity) {
    dstr result;

    dstr_init(&result);

    if (capacity) {
        dstr_reserve(&result, capacity);
    }

    return result;
}

DSTR_API dstr
dstr_make_from(const dstr_char_t* data, dstr_size_t size)
{
	return dstr_make_from_strv(strv_make_from(data, size));
}

DSTR_API dstr
dstr_make_from_str(const dstr_char_t* str)
{
	return dstr_make_from_strv(strv_make_from_str(str));
}

DSTR_API dstr
dstr_make_from_char(dstr_char_t ch)
{
	return dstr_make_from(&ch, 1);
}

DSTR_API dstr
dstr_make_from_strv(strv sv)
{
	dstr result;

	dstr_init(&result);
	dstr_assign(&result, sv);

	return result;
}

DSTR_API dstr
dstr_make_from_dstr(const dstr* s)
{
	return dstr_make_from_strv(dstr_to_strv(s));
}

DSTR_API dstr
dstr_make_from_nchar(dstr_size_t count, dstr_char_t ch)
{
    dstr result;

    dstr_init(&result);
    dstr_assign_nchar(&result, count, ch);

    return result;
}

DSTR_API dstr
dstr_make_from_fmt(const char* fmt, ...)
{
	dstr result;
	dstr_init(&result);

	va_list args;
	va_start(args, fmt);

	dstr_append_fv(&result, fmt, args);

	va_end(args);

	return result;
}

DSTR_API dstr
dstr_make_from_vfmt(const char* fmt, va_list args)
{
	dstr result;
	dstr_init(&result);

	dstr_append_fv(&result, fmt, args);

	return result;
}

DSTR_API strv
dstr_to_strv(const dstr* s)
{
	return strv_make_from(s->data, s->size);
}

DSTR_API int
dstr_compare(const dstr* s, strv sv)
{
	return strv_compare(dstr_to_strv(s), sv);
}

DSTR_API int
dstr_compare_str(const dstr* s, const dstr_char_t* str)
{
	return strv_compare_str(dstr_to_strv(s), str);
}

DSTR_API int
dstr_compare_dstr(const dstr* s, const dstr* other)
{
	return strv_compare(dstr_to_strv(s), dstr_to_strv(other));
}

DSTR_API dstr_bool
dstr_equals(const dstr* s, strv sv)
{
	return strv_equals(dstr_to_strv(s), sv);
}

DSTR_API dstr_bool
dstr_equals_str(const dstr* s, const dstr_char_t* str)
{
	return strv_equals_str(dstr_to_strv(s), str);
}

DSTR_API dstr_bool
dstr_equals_dstr(const dstr* s, const dstr* other)
{
	return strv_equals(dstr_to_strv(s), dstr_to_strv(other));
}

DSTR_API dstr_bool
dstr_less_than(const dstr* s, strv sv)
{
	return strv_less_than(dstr_to_strv(s), sv);
}

DSTR_API dstr_bool
dstr_less_than_str(const dstr* s, const dstr_char_t* str)
{
	return strv_less_than_str(dstr_to_strv(s), str);
}

DSTR_API dstr_bool
dstr_greater_than(const dstr* s, strv sv)
{
	return strv_greater_than(dstr_to_strv(s), sv);
}

DSTR_API dstr_bool
dstr_greater_than_str(const dstr* s, const dstr_char_t* str)
{
	return strv_greater_than_str(dstr_to_strv(s), str);
}

DSTR_API dstr_char_t
dstr_at(const dstr* s, dstr_size_t index)
{
	return s->data[index];
}

DSTR_API dstr_char_t*
dstr_data(dstr* s)
{
	return s->data;
}

DSTR_API dstr_char_t*
dstr_c_str(dstr* s)
{
	return s->data;
}

DSTR_API void
dstr_reserve(dstr* s, dstr_size_t new_string_capacity)
{
	dstr_bool preserve_data = (dstr_bool)1;
	dstr__reserve_internal(s, new_string_capacity, preserve_data);
}

DSTR_API void
dstr_append(dstr* s, strv sv)
{
	dstr_append_from(s, s->size, sv);
}

DSTR_API void
dstr_append_str(dstr* s, const dstr_char_t* str)
{
	dstr_append(s, strv_make_from_str(str));
}

DSTR_API void
dstr_append_char(dstr* s, const dstr_char_t ch)
{
	dstr_append_char_from(s, s->size, ch);
}

DSTR_API void
dstr_append_dstr(dstr* s, const dstr* other)
{
	dstr_append(s, dstr_to_strv(other));
}

DSTR_API void
dstr_append_nchar(dstr* s, dstr_size_t count, const dstr_char_t ch)
{
    dstr_size_t capacity_needed = s->size + count;

	dstr__grow_if_needed(s, capacity_needed);

    dstr_char_t* first = s->data + s->size;
    dstr_char_t* last = first + count;
    for (; first != last; ++first) {
        *first = ch;
    }

    s->size += count;
    s->data[s->size] = '\0';
}

DSTR_API int
dstr_append_fv(dstr* s, const char* fmt, va_list args)
{
	return dstr_append_from_fv(s, s->size, fmt, args);
}

DSTR_API int
dstr_append_f(dstr* s, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	int len = dstr_append_fv(s, fmt, args);
	va_end(args);
	return len;
}

DSTR_API void
dstr_append_from(dstr* s, int index, strv sv)
{
	dstr__grow_if_needed(s, index + sv.size);

	DSTR_MEMCPY(s->data + index, sv.data, sizeof_nchar(sv.size));
	s->size = index + sv.size;
	s->data[s->size] = '\0';
}

DSTR_API void
dstr_append_str_from(dstr* s, int index, const dstr_char_t* str)
{
	dstr_append_from(s, index, strv_make_from_str(str));
}

DSTR_API void
dstr_append_char_from(dstr*s, int index, char c)
{
	dstr_append_from(s, index, strv_make_from(&c, 1));
}

DSTR_API void
dstr_append_dstr_from(dstr* s, int index, const dstr* other)
{
	dstr_append_from(s, index, dstr_to_strv(other));
}

DSTR_API int
dstr_append_from_fv(dstr* s, int index, const char* fmt, va_list args)
{
	va_list args_copy;
	va_copy(args_copy, args);

	/* Caluclate necessary len */
	int add_len = vsnprintf(NULL, 0, fmt, args_copy);
	DSTR_ASSERT(add_len >= 0);

	dstr__grow_if_needed(s, s->size + add_len + 1);

	add_len = vsnprintf(s->data + index, add_len + 1, fmt, args);

	s->size = index + add_len;
	return add_len;
}

DSTR_API void
dstr_push_back(dstr* s, const dstr_char_t ch)
{
	dstr_append_char(s, ch);
}

DSTR_API void
dstr_pop_back(dstr* s)
{
    DSTR_ASSERT(s->size);

    --(s->size);
    s->data[s->size] = '\0';
}

DSTR_API void
dstr_assign(dstr* s, strv sv)
{
	dstr_append_from(s, 0, sv);
}

DSTR_API void
dstr_assign_char(dstr* s, dstr_char_t ch)
{
	dstr_assign(s, strv_make_from(&ch, 1));
}

DSTR_API void
dstr_assign_str(dstr* s, const dstr_char_t* str)
{
	dstr_assign(s, strv_make_from_str(str));
}

DSTR_API void
dstr_assign_dstr(dstr* s, const dstr* other)
{
    dstr_assign(s, dstr_to_strv(other));
}

DSTR_API void
dstr_assign_nchar(dstr* s, dstr_size_t count, dstr_char_t ch)
{
	dstr_size_t size = count;

	dstr_size_t capacity_needed = size;
	dstr__grow_if_needed_discard(s, capacity_needed);

    dstr_it first = s->data;
    dstr_it last = s->data + count;
    for (; first != last; ++first) {
        *first = ch;
    }

	s->data[size] = '\0';
	s->size = size;
}

DSTR_API void
dstr_assign_fv(dstr* s, const char* fmt, va_list args)
{

	dstr_append_from_fv(s, 0, fmt, args);
}

DSTR_API void
dstr_assign_f(dstr* s, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	dstr_assign_fv(s, fmt, args);
	va_end(args);
}

DSTR_API void
dstr_assign_fv_nogrow(dstr* s, const char* fmt, va_list args)
{
	int size = vsnprintf(s->data, s->capacity + 1, fmt, args);
	if (size == -1)
		size = s->capacity;

	s->size = size;
	s->data[size] = 0;
}

DSTR_API void
dstr_assign_f_nogrow(dstr* s, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	dstr_assign_fv_nogrow(s, fmt, args);
	va_end(args);
}

DSTR_API void
dstr_shrink_to_fit(dstr* s)
{
	dstr_char_t * new_data;
	dstr_size_t new_capacity;
	if (dstr__owns_local_buffer(s)
		&& s->size <= s->local_buffer_size - 1)
	{
		new_data = dstr__get_local_buffer(s);
		new_capacity = s->local_buffer_size - 1; /* - 1 for '\0' char */
	}
	else
	{
		new_capacity = s->size;
		new_data = (dstr_char_t*)DSTR_MALLOC(sizeof_nchar(s->size + 1)); /* +1 because we want to copy the '\0' */
	}

	DSTR_ASSERT(new_data);
	
	DSTR_MEMCPY(new_data, s->data, sizeof_nchar(s->size + 1)); /* +1 because we want to copy the '\0' */

	if (dstr__is_allocated(s))
		DSTR_FREE(s->data);

	s->data = new_data;
	s->capacity = new_capacity;
}

DSTR_API int
dstr_empty(const dstr* s)
{
	return !s->size;
}

DSTR_API dstr_size_t
dstr_size(const dstr* s)
{
	return s->size;
}

DSTR_API dstr_size_t
dstr_length(const dstr* s)
{
	return s->size;
}

DSTR_API dstr_size_t
dstr_capacity(const dstr* s)
{
	return s->capacity;
}

DSTR_API dstr_it
dstr_begin(const dstr* s)
{
	return strv_begin(dstr_to_strv(s));
}

DSTR_API dstr_it
dstr_end(const dstr* s)
{
	return strv_end(dstr_to_strv(s));
}

DSTR_API dstr_it
dstr_insert(dstr* s, const dstr_it index, strv sv)
{
	DSTR_ASSERT(index >= dstr_begin(s) && index <= dstr_end(s));

	const dstr_size_t count = sv.size;
	const ptrdiff_t offset = index - s->data;
	const dstr_size_t distance_from_index_to_end = (dstr_size_t)(dstr_end(s) - index);
	const dstr_size_t capacity_needed = s->size + count;

	dstr__grow_if_needed(s, capacity_needed);

	/* There is data between index and end to move */
	if (distance_from_index_to_end > 0)
	{
		DSTR_MEMMOVE(
			s->data + offset + sizeof_nchar(count),
			s->data + offset,
			distance_from_index_to_end
		);
	}

	DSTR_MEMCPY(s->data + offset, sv.data, sizeof_nchar(count));

	s->size += count;
	s->data[s->size] = '\0';

	return s->data + offset;
}

DSTR_API dstr_it
dstr_insert_str(dstr* s, const dstr_it index, const dstr_char_t* str)
{
	return dstr_insert(s, index, strv_make_from_str(str));
}

DSTR_API dstr_it
dstr_insert_char(dstr* s, const dstr_it index, const dstr_char_t ch)
{
	return dstr_insert(s, index, strv_make_from(&ch, 1));
}

DSTR_API dstr_it
dstr_insert_dstr(dstr* s, const dstr_it index, const dstr* other)
{
	return dstr_insert(s, index, dstr_to_strv(other));
}

DSTR_API dstr_it
dstr_erase_ref(dstr* s, const dstr_it first, dstr_size_t count)
{
	const dstr_it last = first + count;

	DSTR_ASSERT(first >= s->data && first <= dstr_end(s));
	DSTR_ASSERT(last >= s->data && last <= dstr_end(s));

	const dstr_size_t first_index = (dstr_size_t)(first - s->data);
	const dstr_size_t last_index = (dstr_size_t)(last - s->data);
	const dstr_size_t count_to_move = (s->size - last_index) + 1; /* +1 for '\0' */

	DSTR_MEMMOVE(s->data + first_index, s->data + last_index, sizeof_nchar(count_to_move));

	s->size -= count;

	return s->data + first_index;
}

DSTR_API dstr_it
dstr_erase(dstr* s, const dstr_it first, dstr_size_t size)
{
	if (!size) return dstr_begin(s);

	const dstr_it last = first + size;

	DSTR_ASSERT(first >= dstr_begin(s) && first < dstr_end(s));
	DSTR_ASSERT(last >= first && last <= dstr_end(s));

	const dstr_size_t first_index = (dstr_size_t)(first - s->data);
	const dstr_size_t last_index = (dstr_size_t)(last - s->data); 
	const dstr_size_t count_to_move = (dstr_size_t)(dstr_end(s) - last) + 1; /* +1 for '\0' */

	DSTR_MEMMOVE(s->data + first_index, s->data + last_index, sizeof_nchar(count_to_move));

	s->size -= size;

	return s->data + first_index;
}

DSTR_API dstr_it
dstr_erase_value(dstr* s, const dstr_it index)
{
	return dstr_erase(s, index, 1);
}

DSTR_API dstr_it
dstr_erase_at(dstr* s, dstr_size_t index)
{
	dstr_char_t* value = s->data + index;
	return dstr_erase_value(s, value);
}

DSTR_API void
dstr_clear(dstr* s)
{
	dstr_resize(s, 0);
}

DSTR_API void
dstr_resize(dstr* s, dstr_size_t size)
{
	if (s->size == size)
		return;

    dstr_size_t extra_count = 0;

    if (size > s->capacity){

		dstr__grow_if_needed(s, size);
        extra_count = s->capacity - s->size;

    } else if (size > s->size){
        extra_count = size - s->size;
    }

    if (extra_count) {
        memset(s->data + s->size, 0, sizeof_nchar(extra_count));
    }

    s->size = size;
    s->data[s->size] = '\0';
}

DSTR_API void
dstr_resize_fill(dstr* s, dstr_size_t size, dstr_char_t ch)
{
	if (!size)
	{
        dstr_destroy(s);
    } 
	else
	{
		dstr_size_t extra_count = 0;

		/* +1 for extra char */
		if (size + 1 > s->capacity)
		{
			dstr__grow_if_needed(s, size + 1);
			extra_count = s->capacity - s->size;
		}
		else if (size > s->size)
		{
			extra_count = size - s->size;
		}

		if (extra_count)
		{
			dstr_char_t* begin = s->data + s->size;
			dstr_char_t* end   = begin + extra_count;
			while (begin != end)
			{
				*begin = ch;
				++begin;
			}
		}

		s->size = size;
		s->data[s->size] = '\0';
    }
}

DSTR_API void
dstr_replace_range(dstr* s, dstr_size_t index, dstr_size_t count, strv sv)
{
	DSTR_ASSERT(index <= s->size);
	DSTR_ASSERT(count <= s->size);
	DSTR_ASSERT(index + count <= s->size);

	dstr_erase(s, s->data + index, count);
	dstr_insert(s, s->data + index, sv);
}

DSTR_API void
dstr_copy_to(const dstr* s, dstr* dest)
{
	dstr_destroy(dest);
    dstr_size_t needed_capacity = s->size;
	dstr__grow_if_needed_discard(dest, needed_capacity);
    DSTR_MEMCPY(dest->data, s->data, sizeof_nchar(needed_capacity));
}

DSTR_API void
dstr_swap(dstr* s, dstr* other)
{
    const dstr tmp = *s;
    *s = *other;
    *other = tmp;
}

/*-----------------------------------------------------------------------*/
/* dstr - Extended Implementation */
/*-----------------------------------------------------------------------*/

DSTR_API strv
dstr_trimmed(dstr* s)
{
	return strv_trimmed(dstr_to_strv(s));
}

DSTR_API void
dstr_replace(dstr* s, strv to_replaced, strv with)
{
    const void* found;

    const dstr_char_t* s_begin = s->data;
    const dstr_char_t* s_end = s->data + s->size;

    while((found = strv_memory_find(
				strv_make_from(s_begin, (dstr_size_t)s_end - (dstr_size_t)s_begin),
				strv_make_from(to_replaced.data, to_replaced.size)
		)) != 0) {

        dstr_size_t index = (dstr_char_t*)found - s->data;

        dstr_replace_range(s, index, to_replaced.size, with);

        /* reset begin and end, could be invalidated due to reallocation */
	    /* put the next position to index + found word length */
        s_begin = s->data + index + with.size;
        s_end = s->data + s->size;
    }
}

DSTR_API void
dstr_replace_dstr(dstr* s, const dstr* to_replaced, const dstr* with)
{
	dstr_replace(s, dstr_to_strv(to_replaced), dstr_to_strv(with));
}

DSTR_API void
dstr_replace_str(dstr* s, const dstr_char_t* to_replaced, const dstr_char_t* with)
{
	const strv tmp_to_replaced = strv_make_from_str(to_replaced);
	const strv tmp_with = strv_make_from_str(with);

    dstr_replace(s, tmp_to_replaced, tmp_with);
}

/*-----------------------------------------------------------------------*/
/* dstr - Private Implementation */
/*-----------------------------------------------------------------------*/

DSTR_INTERNAL void
dstr__reserve_no_preserve_data(dstr* s, dstr_size_t new_string_capacity)
{
	dstr_bool preserve_data = (dstr_bool)0;
	dstr__reserve_internal(s, new_string_capacity, preserve_data);
}

DSTR_INTERNAL void
dstr__reserve_internal(dstr* s, dstr_size_t new_string_capacity, dstr_bool preserve_data)
{
	if (new_string_capacity <= s->capacity)
		return;

	dstr_size_t memory_capacity = new_string_capacity + 1; /* capacity + 1 for '\0' */

	/* Unnnecesary condition since it's handled by 'new_capacity <= s->capacity' above */
	/* I keep it for clarity */
	if (dstr__is_using_local_buffer(s) && memory_capacity <= s->local_buffer_size)
		return;

	dstr_char_t* new_data = (dstr_char_t*)DSTR_MALLOC(sizeof_nchar(memory_capacity));

	DSTR_ASSERT(new_data);

	if (preserve_data)
	{
		/* Don't use strcpy here since it stops at the first null char */
		/* Sometime we just want to use dstr as raw buffer */
		DSTR_MEMCPY(new_data, s->data, sizeof_nchar(s->size + 1));
	}

	if (dstr__is_allocated(s))
		DSTR_FREE(s->data);

	s->data = new_data;
	s->capacity = new_string_capacity;
}

DSTR_INTERNAL int
dstr__is_allocated(dstr* s)
{
    return s->data != dstr__get_local_buffer(s) && s->data != DSTR__DEFAULT_DATA;
}

/* Returns true if the dstr has been built originally with a local buffer */
DSTR_INTERNAL dstr_bool
dstr__owns_local_buffer(dstr* s)
{
	return s->local_buffer_size != 0;
}

DSTR_INTERNAL dstr_char_t*
dstr__get_local_buffer(dstr* s)
{
	typedef char re_byte;
	return (dstr_char_t*)((re_byte*)s + sizeof(dstr));
}

DSTR_INTERNAL dstr_bool
dstr__is_using_local_buffer(dstr* s)
{
	return dstr__owns_local_buffer(s)
		&& s->data == dstr__get_local_buffer(s);
}

#endif /* DSTR_IMPLEMENTATION  */
