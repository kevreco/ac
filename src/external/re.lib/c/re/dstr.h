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

#ifdef DSTR_USE_CONFIG
#include "dstr_config.h"
#endif

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
/* strv - API */
/*-----------------------------------------------------------------------*/
typedef struct strv strv;
struct strv {
	dstr_size_t        size;
	const dstr_char_t* data;
};

DSTR_API strv strv_make();
/* create a strv from the content between 'data' and 'data + size' */
DSTR_API strv strv_make_from(const dstr_char_t* data, dstr_size_t size);
DSTR_API strv strv_make_from_str(const char* str);
DSTR_API strv strv_make_from_view(strv sv);

DSTR_API void      strv_reset(strv* sv);

/* set new value for the string view */
DSTR_API void      strv_assign      (strv* sv, const dstr_char_t* data, dstr_size_t size);
/* overload of strv_assign */
DSTR_API void      strv_assign_str  (strv* sv, const dstr_char_t* str);

/* strv is empty */
DSTR_API dstr_bool strv_empty(strv sv);

/* lexicagraphical comparison */
DSTR_API int       strv_compare          (strv sv, const dstr_char_t* data, dstr_size_t size);
/* overload of strv_compare */
DSTR_API int       strv_compare_view     (strv sv, strv other);
/* overload of strv_compare */
DSTR_API int       strv_compare_str      (strv sv, const dstr_char_t* str);
/* equivalent of strv_compare == 0 */
DSTR_API dstr_bool strv_equals           (strv sv, strv other);
/* overload of strv_equals_str */
DSTR_API dstr_bool strv_equals_str       (strv sv, const dstr_char_t* str);
/* equivalent of strv_compare < 0 */
DSTR_API dstr_bool strv_less_than        (strv sv, strv other);
/* overload of strv_less_than */
DSTR_API dstr_bool strv_less_than_str    (strv sv, const dstr_char_t* str);
/* equivalent of strv_compare > 0 */
DSTR_API dstr_bool strv_greater_than     (strv sv, strv other);
/* overload of strv_greater_than_str */
DSTR_API dstr_bool strv_greater_than_str (strv sv, const dstr_char_t* str);

/* to mimic iterator behavior - cpp-style */
DSTR_API dstr_it strv_begin(strv sv);
DSTR_API dstr_it strv_end(strv sv);

/* get first char - cpp-style */
DSTR_API dstr_char_t strv_front(strv view);
/* get last char - cpp-style */
DSTR_API dstr_char_t strv_back(strv view);

/* find sub string in string */
DSTR_API dstr_size_t strv_find(strv sv, dstr_size_t pos, const dstr_char_t* sub_data, dstr_size_t sub_size);
/* overload of strv_find */
DSTR_API dstr_size_t strv_find_str(strv sv, dstr_size_t pos, const dstr_char_t* sub);
/* overload of strv_find */
DSTR_API dstr_size_t strv_find_char(strv sv, dstr_size_t pos, dstr_char_t ch);
/* overload of strv_find */
DSTR_API dstr_size_t strv_find_view(strv sv, dstr_size_t pos, strv sub);

/* Returns empty string if pos == s->size */
/* Returns empty string if pos > s->size. */
DSTR_API strv strv_substr(strv sv, const dstr_it pos, dstr_size_t count);
/* get substring from from index  + count */
DSTR_API strv strv_substr_from(strv sv, dstr_size_t index, dstr_size_t count);

DSTR_API void strv_swap(strv* s, strv* other);

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
DSTR_API dstr dstr_make_from_view   (strv view);
/* overload of dstr_make_from */
DSTR_API dstr dstr_make_from_dstr   (const dstr* str);

/* Create with a char repeated N times */
DSTR_API dstr dstr_make_from_nchar  (dstr_size_t count, dstr_char_t ch);

DSTR_API dstr dstr_make_from_fmt    (const char* fmt, ...);
DSTR_API dstr dstr_make_from_vfmt   (const char* fmt, va_list args);

DSTR_API strv dstr_to_view(const dstr* s);

/* Use the strv counterpart */
DSTR_API int       dstr_compare(const dstr* s, const dstr_char_t* data, dstr_size_t size);
/* Use the strv counterpart */
DSTR_API int       dstr_compare_str(const dstr* s, const dstr_char_t* str);
/* Use the strv counterpart */
DSTR_API int       dstr_compare_dstr(const dstr* s, const dstr* str);
/* Use the strv counterpart */
DSTR_API dstr_bool dstr_equals(const dstr* s, const dstr_char_t* data, dstr_size_t size);
/* Use the strv counterpart */
DSTR_API dstr_bool dstr_equals_str(const dstr* s, const dstr_char_t* other);
/* Use the strv counterpart */
DSTR_API dstr_bool dstr_equals_dstr(const dstr* s, const dstr* str);
/* Use the strv counterpart */
DSTR_API dstr_bool dstr_less_than(const dstr* s, const dstr_char_t* data, dstr_size_t size);
/* Use the strv counterpart */
DSTR_API dstr_bool dstr_less_than_str(const dstr* s, const dstr_char_t* str);
/* Use the strv counterpart */
DSTR_API dstr_bool dstr_greater_than(const dstr* s, const dstr_char_t* data, dstr_size_t size);
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
DSTR_API void dstr_append        (dstr* s, const dstr_char_t* data, dstr_size_t size);
/* Overload of dstr_append */
DSTR_API void dstr_append_str    (dstr* s, const dstr_char_t* str);
/* Overload of dstr_append */
DSTR_API void dstr_append_char   (dstr* s, const dstr_char_t ch);
/* Overload of dstr_append */
DSTR_API void dstr_append_view   (dstr* s, strv view);
/* Overload of dstr_append */
DSTR_API void dstr_append_dstr   (dstr* s, const dstr* dstr);
DSTR_API void dstr_append_nchar  (dstr* s, dstr_size_t count, const dstr_char_t ch);

DSTR_API int dstr_append_fv (dstr* s, const char* fmt, va_list args);
DSTR_API int dstr_append_f  (dstr* s, const char* fmt, ...);

/* append string at a certain index all content after index will be lost */
DSTR_API void dstr_append_from      (dstr* s, int index, const dstr_char_t* data, dstr_size_t size);
/* Overload of dstr_append_from */
DSTR_API void dstr_append_str_from  (dstr* s, int index, const dstr_char_t* str);
/* Overload of dstr_append_from */
DSTR_API void dstr_append_char_from (dstr*s, int index, char ch);
/* Overload of dstr_append_from */
DSTR_API void dstr_append_view_from (dstr* s, int index, strv view);
/* Overload of dstr_append_from */
DSTR_API void dstr_append_dstr_from (dstr* s, int index, const dstr* str);

DSTR_API int dstr_append_from_fv   (dstr* s, int index, const char* fmt, va_list args);

/* Equivalent to dstr_append_char */
DSTR_API void dstr_push_back(dstr* s, const dstr_char_t ch);

/* Replaces content with the content from a string defined the data pointer and the size */
DSTR_API void dstr_assign           (dstr* s, const dstr_char_t* data, dstr_size_t size);
/* Overload of dstr_assign */
DSTR_API void dstr_assign_str       (dstr* s, const dstr_char_t* str);
/* Overload of dstr_assign */
DSTR_API void dstr_assign_char      (dstr* s, dstr_char_t ch);
/* Overload of dstr_assign */
DSTR_API void dstr_assign_view      (dstr* s, strv view);
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

DSTR_API dstr_it dstr_insert(dstr* s, const dstr_it index, const dstr_char_t* data, dstr_size_t size);
DSTR_API dstr_it dstr_insert_str(dstr* s, const dstr_it index, const dstr_char_t* str);
DSTR_API dstr_it dstr_insert_char(dstr* s, const dstr_it index, const dstr_char_t value);
DSTR_API dstr_it dstr_insert_view(dstr* s, const dstr_it index, strv view);
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

/* Replace the content between the interval [index - (index + count)] with the content of[r_data - (r_data + r_size)] */
DSTR_API void dstr_replace_with       (dstr* s, dstr_size_t index, dstr_size_t count, const dstr_char_t* r_data, dstr_size_t r_size);
/* Overload of dstr_replace_with */
DSTR_API void dstr_replace_with_str   (dstr* s, dstr_size_t index, dstr_size_t count, const dstr_char_t* replacing);
/* Overload of dstr_replace_with */
DSTR_API void dstr_replace_with_char  (dstr* s, dstr_size_t index, dstr_size_t count, const dstr_char_t ch);
/* Overload of dstr_replace_with */
DSTR_API void dstr_replace_with_view  (dstr* s, dstr_size_t index, dstr_size_t count, strv replacing);
/* Overload of dstr_replace_with */
DSTR_API void dstr_replace_with_dstr  (dstr* s, dstr_size_t index, dstr_size_t count, const dstr* replacing);

/* Wrapper around strv_find */
DSTR_API dstr_size_t dstr_find(const dstr* s, dstr_size_t pos, const dstr_char_t* sub_data, dstr_size_t sub_size);
/* Wrapper around strv_find */
DSTR_API dstr_size_t dstr_find_str     (const dstr* s, dstr_size_t pos, const dstr_char_t* sub);
/* Wrapper around strv_find */
DSTR_API dstr_size_t dstr_find_char    (const dstr* s, dstr_size_t pos, dstr_char_t ch);
/* Wrapper around strv_find */
DSTR_API dstr_size_t dstr_find_view    (const dstr* s, dstr_size_t pos, strv sub);
/* Wrapper around strv_find */
DSTR_API dstr_size_t dstr_find_dstr    (const dstr* s, dstr_size_t pos, const dstr* sub);

DSTR_API void dstr_copy_to  (const dstr* s, dstr* dest);
DSTR_API void dstr_swap     (dstr* s, dstr* other);


/*-----------------------------------------------------------------------*/
/* dstr - Extended API */
/*-----------------------------------------------------------------------*/

DSTR_API void dstr_trim     (dstr* s);
DSTR_API void dstr_ltrim    (dstr* s);
DSTR_API void dstr_rtrim    (dstr* s);

/* @TODO create tests for dstr_substitute_view */
DSTR_API void dstr_substitute_view      (dstr* s, strv to_replaced, strv with);
DSTR_API void dstr_substitute_dstr      (dstr* s, const dstr* to_replaced, const dstr* with);
DSTR_API void dstr_substitute_str       (dstr* s, const dstr_char_t* to_replaced, const dstr_char_t* with);

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
/* DSTR_API is used instead of DSTR_INTERNAL because other libaries also use it. */
DSTR_API void* dstr__memory_find(const void* memory_ptr, dstr_size_t mem_len, const void* pattern_ptr, dstr_size_t pattern_len);

/* Equivalent of strncmp, this implementation does not stop on null termination char */
DSTR_INTERNAL int dstr__lexicagraphical_cmp(const dstr_char_t* left, size_t count_left, const dstr_char_t* right, size_t count_right);

DSTR_INTERNAL int dstr__is_allocated      (dstr* s);

DSTR_INTERNAL dstr_bool    dstr__owns_local_buffer     (dstr* s);
DSTR_INTERNAL dstr_char_t* dstr__get_local_buffer      (dstr* s);
DSTR_INTERNAL dstr_bool    dstr__is_using_local_buffer (dstr* s);

/*-----------------------------------------------------------------------*/
/* dstr - API Implementation */
/*-----------------------------------------------------------------------*/

#ifdef DSTR_IMPLEMENTATION

DSTR_INTERNAL DSTR_SIZE_T sizeof_nchar(int count) { return count * sizeof(dstr_char_t); }

DSTR_API strv
strv_make()
{
	strv sv;
	sv.data = 0;
	sv.size = 0;
	return sv;
}

DSTR_API strv
strv_make_from(const dstr_char_t* data, dstr_size_t size)
{
	strv sv;
	sv.data = data;
	sv.size = size;
	return sv;
}

DSTR_API strv
strv_make_from_str(const char* str)
{
	return strv_make_from(str, strlen(str));
}

DSTR_API strv
strv_make_from_view(strv view)
{
	return strv_make_from(view.data, view.size);
}

DSTR_API void
strv_reset(strv* sv)
{
	sv->size = 0;
	sv->data = 0;
}

DSTR_API void
strv_assign(strv* sv, const dstr_char_t* data, dstr_size_t size)
{
	sv->size = size;
	sv->data = data;
}

DSTR_API void
strv_assign_str(strv* sv, const dstr_char_t* str)
{
	if (str == 0) /* Maybe this case shouldn't be possible? */
	{
		sv->size = 0;
		sv->data = 0;
	}
	else
	{
		sv->size = strlen(str);
		sv->data = str;
	}
}

DSTR_API dstr_bool
strv_empty(strv sv)
{
	return sv.size == 0;
}

DSTR_API int
strv_compare(strv sv, const dstr_char_t* data, dstr_size_t size)
{
	return dstr__lexicagraphical_cmp(sv.data, sv.size, data, size);
}

DSTR_API int
strv_compare_view(strv sv, strv other)
{
	return strv_compare(sv, other.data, other.size);
}

DSTR_API int
strv_compare_str(strv sv, const dstr_char_t* str)
{
	return strv_compare(sv, str, strlen(str));
}

DSTR_API dstr_bool
strv_equals(strv sv, strv other)
{
	return strv_compare_view(sv, other) == 0;
}

DSTR_API dstr_bool
strv_equals_str(strv sv, const dstr_char_t* str)
{
	return strv_compare_str(sv, str) == 0;
}

DSTR_API dstr_bool
strv_less_than(strv sv, strv other)
{
	return strv_compare_view(sv, other) < 0;
}

DSTR_API dstr_bool
strv_less_than_str(strv sv, const dstr_char_t* str)
{
	return strv_compare_str(sv, str) < 0;
}

DSTR_API dstr_bool
strv_greater_than(strv sv, strv other)
{
	return strv_compare_view(sv, other) > 0;
}

DSTR_API dstr_bool
strv_greater_than_str(strv sv, const dstr_char_t* str)
{
	return strv_compare_str(sv, str) > 0;
}

DSTR_API dstr_it
strv_begin(strv sv)
{
	return (dstr_it)(sv.data);
}

DSTR_API dstr_it
strv_end(strv sv)
{
	return (dstr_it)(sv.data + sv.size);
}

DSTR_API dstr_char_t
strv_front(strv view)
{
	DSTR_ASSERT(view.size > 0);
	return view.data[0];
}

DSTR_API dstr_char_t
strv_back(strv view)
{
	DSTR_ASSERT(view.size > 0);
	return view.data[view.size - 1];
}

DSTR_API dstr_size_t
strv_find(strv sv, dstr_size_t pos, const dstr_char_t* sub_data, dstr_size_t sub_size)
{
	dstr_size_t result = DSTR_NPOS;

	int worth_a_try = sub_size
		&& (pos <= sv.size)
		&& (pos <= (sv.size - sub_size));

	if (worth_a_try) {

		void* found = dstr__memory_find(sv.data + pos, sv.size, sub_data, sub_size);

		if (found) {
			result = (dstr_char_t*)found - sv.data;
		}
	}

	return result;
}

DSTR_API dstr_size_t
strv_find_str(strv sv, dstr_size_t pos, const dstr_char_t* sub)
{
	return strv_find(sv, pos, sub, strlen(sub));
}

DSTR_API dstr_size_t
strv_find_char(strv sv, dstr_size_t pos, dstr_char_t ch)
{
	return strv_find(sv, pos, &ch, 1);
}

DSTR_API dstr_size_t
strv_find_view(strv sv, dstr_size_t pos, strv sub)
{
	return strv_find(sv, pos, sub.data, sub.size);
}

DSTR_API strv
strv_substr(strv sv, const dstr_it pos, dstr_size_t count)
{
	const dstr_it last = pos + count;

	DSTR_ASSERT(pos >= strv_begin(sv) && pos < strv_end(sv));
	DSTR_ASSERT(last >= pos && last <= strv_end(sv));

	strv result;

	result.data = pos;
	result.size = count;

	return result;
}

DSTR_API strv
strv_substr_from(strv sv, dstr_size_t index, dstr_size_t count)
{
	const dstr_it it = (const dstr_it)(sv.data + index);
	return strv_substr(sv, it, count);
}

DSTR_API void
strv_swap(strv* s, strv* other)
{
	const strv tmp = *s;
	*s = *other;
	*other = tmp;
}

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
	dstr result;

	dstr_init(&result);
	dstr_assign(&result, data, size);

	return result;
}

DSTR_API dstr
dstr_make_from_str(const dstr_char_t* str)
{
	return dstr_make_from(str, strlen(str));
}

DSTR_API dstr
dstr_make_from_char(dstr_char_t ch)
{
	return dstr_make_from(&ch, 1);
}

DSTR_API dstr
dstr_make_from_view(strv view)
{
	return dstr_make_from(view.data, view.size);
}

DSTR_API dstr
dstr_make_from_dstr(const dstr* str)
{
	return dstr_make_from(str->data, str->size);
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
dstr_to_view(const dstr* s)
{
	return strv_make_from(s->data, s->size);
}

DSTR_API int
dstr_compare(const dstr* s, const dstr_char_t* data, dstr_size_t size)
{
	return strv_compare(dstr_to_view(s), data, size);
}

DSTR_API int
dstr_compare_str(const dstr* s, const dstr_char_t* str)
{
	return dstr_compare(s, str, strlen(str));
}

DSTR_API int
dstr_compare_dstr(const dstr* s, const dstr* str)
{
	return dstr_compare(s, str->data, str->size);
}

DSTR_API dstr_bool
dstr_equals(const dstr* s, const dstr_char_t* data, dstr_size_t size)
{
	return dstr_compare(s, data, size) == 0;
}

DSTR_API dstr_bool
dstr_equals_str(const dstr* s, const dstr_char_t* str)
{
	return dstr_compare(s, str, strlen(str)) == 0;
}

DSTR_API dstr_bool
dstr_equals_dstr(const dstr* s, const dstr* str)
{
	return dstr_compare(s, str->data, str->size) == 0;
}

DSTR_API dstr_bool
dstr_less_than(const dstr* s, const dstr_char_t* data, dstr_size_t size)
{
	return dstr_compare(s, data, size) < 0;
}

DSTR_API dstr_bool
dstr_less_than_str(const dstr* s, const dstr_char_t* str)
{
	return dstr_compare(s, str, strlen(str)) < 0;
}

DSTR_API dstr_bool
dstr_greater_than(const dstr* s, const dstr_char_t* data, dstr_size_t size)
{
	return dstr_compare(s, data, size) > 0;
}

DSTR_API dstr_bool
dstr_greater_than_str(const dstr* s, const dstr_char_t* str)
{
	return dstr_compare(s, str, strlen(str)) > 0;
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
dstr_append(dstr* s, const dstr_char_t* data, dstr_size_t size)
{
	dstr_append_from(s, s->size, data, size);
}

DSTR_API void
dstr_append_str(dstr* s, const dstr_char_t* str)
{
	dstr_append(s, str, strlen(str));
}

DSTR_API void dstr_append_char(dstr* s, const dstr_char_t ch)
{
	dstr_append_char_from(s, s->size, ch);
}

DSTR_API void
dstr_append_view(dstr* s, strv view)
{
	dstr_append(s, view.data, view.size);
}

DSTR_API void
dstr_append_dstr(dstr* s, const dstr* other)
{
	dstr_append(s, other->data, other->size);
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
dstr_append_from(dstr* s, int index, const dstr_char_t* data, dstr_size_t size)
{
	dstr__grow_if_needed(s, index + size);

	DSTR_MEMCPY(s->data + index, (const void*)data, sizeof_nchar(size));
	s->size = index + size;
	s->data[s->size] = '\0';
}

DSTR_API void
dstr_append_str_from(dstr* s, int index, const dstr_char_t* str)
{
	dstr_append_from(s, index, str, strlen(str));
}

DSTR_API void
dstr_append_char_from(dstr*s, int index, char c)
{
	dstr_append_from(s, index, &c, 1);
}

DSTR_API void
dstr_append_view_from(dstr* s, int index, strv view)
{
	dstr_append_from(s, index, view.data, view.size);
}

DSTR_API void
dstr_append_dstr_from(dstr* s, int index, const dstr* str)
{
	dstr_append_from(s, index, str->data, str->size);
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
dstr_assign(dstr* s, const dstr_char_t* data, dstr_size_t size)
{
	dstr_append_from(s, 0, data, size);
}

DSTR_API void
dstr_assign_char(dstr* s, dstr_char_t ch)
{
	dstr_assign(s, &ch, 1);
}

DSTR_API void
dstr_assign_str(dstr* s, const dstr_char_t* str)
{
	dstr_assign(s, str, strlen(str));
}

DSTR_API void
dstr_assign_view(dstr* s, strv view)
{
	dstr_assign(s, view.data, view.size);
}

DSTR_API void
dstr_assign_dstr(dstr* s, const dstr* str)
{
    dstr_assign(s, str->data, str->size);
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
	return strv_begin(dstr_to_view(s));
}

DSTR_API dstr_it
dstr_end(const dstr* s)
{
	return strv_end(dstr_to_view(s));
}

DSTR_API dstr_it
dstr_insert(dstr* s, const dstr_it index, const dstr_char_t* data, dstr_size_t size)
{
	DSTR_ASSERT(index >= dstr_begin(s) && index <= dstr_end(s));

	const dstr_size_t count = size;
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

	DSTR_MEMCPY(s->data + offset, data, sizeof_nchar(count));

	s->size += count;
	s->data[s->size] = '\0';

	return s->data + offset;
}

DSTR_API dstr_it
dstr_insert_str(dstr* s, const dstr_it index, const dstr_char_t* str)
{
	return dstr_insert(s, index, str, strlen(str));
}

DSTR_API dstr_it
dstr_insert_char(dstr* s, const dstr_it index, const dstr_char_t ch)
{
	return dstr_insert(s, index, &ch, 1);
}

DSTR_API dstr_it
dstr_insert_view(dstr* s, const dstr_it index, strv view)
{
	return dstr_insert(s, index, view.data, view.size);
}

DSTR_API dstr_it
dstr_insert_dstr(dstr* s, const dstr_it index, const dstr* str)
{
	return dstr_insert(s, index, str->data, str->size);
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
dstr_replace_with(dstr* s, dstr_size_t index, dstr_size_t count, const dstr_char_t* r_data, dstr_size_t r_size)
{
	DSTR_ASSERT(index <= s->size);
	DSTR_ASSERT(count <= s->size);
	DSTR_ASSERT(index + count <= s->size);

	if (r_size < count) /* mem replacing <  mem to replace */
	{ 
		char* first = s->data + index;
		char* last = (s->data + index + count);

		dstr_size_t count_to_move = s->size - (index + count);
		dstr_size_t count_removed = count - r_size;

		if (count_to_move) {
			DSTR_MEMMOVE(last - count_removed, last, sizeof_nchar(count_to_move));
		}
		if (s->size) {
			DSTR_MEMCPY(first, r_data, sizeof_nchar(r_size));
		}

		s->size -= count_removed;
		s->data[s->size] = '\0';

	}
	else if (r_size > count) /* mem replacing >  mem to replace */
	{ 
		dstr_size_t extra_count = r_size - count;
		dstr_size_t needed_capacity = s->size + extra_count;
		dstr_size_t count_to_move = s->size - index - count;

		dstr__grow_if_needed(s, needed_capacity);

		/* Need to set this after "grow" because of potential allocation */
		char* first = s->data + index;
		char* last = s->data + index + count;

		if (count_to_move) {
			DSTR_MEMMOVE(last + extra_count, last, sizeof_nchar(count_to_move));
		}

		DSTR_MEMCPY(first, r_data, sizeof_nchar(r_size));

		s->size += extra_count;
		s->data[s->size] = '\0';

	}
	else /* mem replacing == mem to replace */
	{ 
		char* first = s->data + index;
		DSTR_MEMCPY(first, r_data, sizeof_nchar(r_size));
	}
}

DSTR_API void
dstr_replace_with_str(dstr* s, dstr_size_t index, dstr_size_t count, const dstr_char_t* replacing)
{
	dstr_replace_with(s, index, count, replacing, strlen(replacing));
}

DSTR_API void
dstr_replace_with_char(dstr* s, dstr_size_t index, dstr_size_t count, const dstr_char_t ch)
{
	dstr_replace_with(s, index, count, &ch, 1);
}

DSTR_API void
dstr_replace_with_view(dstr* s, dstr_size_t index, dstr_size_t count, strv replacing)
{
	dstr_replace_with(s, index, count, replacing.data, replacing.size);
}

DSTR_API void
dstr_replace_with_dstr(dstr* s, dstr_size_t index, dstr_size_t count, const dstr* replacing)
{
	dstr_replace_with(s, index, count, replacing->data, replacing->size);
}

DSTR_API dstr_size_t
dstr_find(const dstr* s, dstr_size_t pos, const dstr_char_t* sub_data, dstr_size_t sub_size)
{
	return strv_find(dstr_to_view(s), pos, sub_data, sub_size);
}

DSTR_API dstr_size_t
dstr_find_str(const dstr* s, dstr_size_t pos, const dstr_char_t* sub)
{
	return strv_find_str(dstr_to_view(s), pos, sub);
}

DSTR_API dstr_size_t
dstr_find_char(const dstr* s, dstr_size_t pos, dstr_char_t ch)
{
	return strv_find_char(dstr_to_view(s), pos, ch);
}

DSTR_API dstr_size_t
dstr_find_view(const dstr* s, dstr_size_t pos, strv sub)
{
	return strv_find_view(dstr_to_view(s), pos, sub);
}

DSTR_API dstr_size_t
dstr_find_dstr(const dstr* s, dstr_size_t pos, const dstr* sub)
{
	return strv_find_view(dstr_to_view(s), pos, dstr_to_view(sub));
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

DSTR_API void
dstr_trim(dstr* s)
{
    dstr_char_t* cursor_left  = s->data;
    dstr_char_t* cursor_right = s->data + (s->size - 1);

    /* Trim right */
    while (cursor_right >= cursor_left && isspace(*cursor_right)) {
        --cursor_right;
    }

	/* Trim left */
    while (cursor_right > cursor_left && isspace(*cursor_left)) {
        ++cursor_left;
    }

    s->size = (cursor_right - cursor_left) + 1;
    DSTR_MEMMOVE(s->data, cursor_left, sizeof_nchar(s->size));

    s->data[s->size] = '\0';
}

DSTR_API void
dstr_ltrim(dstr* s)
{
    char *cursor = s->data;

    while (s->size > 0 && isspace(*cursor)) {
        ++cursor;
        --s->size;
    }

    DSTR_MEMMOVE(s->data, cursor, sizeof_nchar(s->size));
    s->data[s->size] = '\0';
}

DSTR_API void
dstr_rtrim(dstr* s)
{
    while (s->size > 0 && isspace(s->data[s->size - 1])) {
        --s->size;
    }
    s->data[s->size] = '\0';
}

DSTR_API void
dstr_substitute_view(dstr* s, strv to_replaced, strv with)
{
    const void* found;

    const dstr_char_t* s_begin = s->data;
    const dstr_char_t* s_end = s->data + s->size;

    while((found = dstr__memory_find(
               (const void*)s_begin,
               (dstr_size_t)s_end - (dstr_size_t)s_begin,
               (void*)to_replaced.data,
               to_replaced.size)) != 0) {

        dstr_size_t index = (dstr_char_t*)found - s->data;

        dstr_replace_with_view(s, index, to_replaced.size, with);

        /* reset begin and end, could be invalidated due to reallocation */
	    /* put the next position to index + found word length */
        s_begin = s->data + index + with.size;
        s_end = s->data + s->size;
    }
}

DSTR_API void
dstr_substitute_dstr(dstr* s, const dstr* to_replaced, const dstr* with)
{
	dstr_substitute_view(s, dstr_to_view(to_replaced), dstr_to_view(with));
}

DSTR_API void
dstr_substitute_str(dstr* s, const dstr_char_t* to_replaced, const dstr_char_t* with)
{
    const strv tmp_to_replaced = {
        strlen(to_replaced),
		(dstr_char_t*)to_replaced,
    };

    const strv tmp_with = {
        strlen(with),
		(dstr_char_t*)with,
    };

    dstr_substitute_view(s, tmp_to_replaced, tmp_with);
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
dstr__lexicagraphical_cmp(const dstr_char_t* str_left, size_t count_left, const dstr_char_t* str_right, size_t count_right)
{
	dstr_char_t c1, c2;
	dstr_size_t min_size = count_left < count_right ? count_left : count_right;
	while (min_size-- > 0)
	{
		c1 = *str_left++;
		c2 = *str_right++;
		if (c1 != c2)
			return c1 < c2 ? 1 : -1;
	};

	return count_left - count_right;
}

DSTR_INTERNAL int
dstr__is_allocated(dstr* s)
{
    return s->data != dstr__get_local_buffer(s) && s->data != DSTR__DEFAULT_DATA;
}

DSTR_API void*
dstr__memory_find(const void* memory_ptr, dstr_size_t mem_len, const void* pattern_ptr, dstr_size_t pattern_len)
{
    const char *mem_ptr = (const char *)memory_ptr;
    const char *patt_ptr = (const char *)pattern_ptr;

	/* pattern_len can't be greater than mem_len */
    if ((mem_len == 0) || (pattern_len == 0) || pattern_len > mem_len) {
        return 0;
    }

	/* pattern is a char */
    if (pattern_len == 1) {
        return memchr((void*)mem_ptr, *patt_ptr, mem_len);
    }

    /* Last possible position */
    const char* cur = mem_ptr;
    const char* last = mem_ptr + mem_len - pattern_len;

    while(cur <= last)
	{
		/* Test the first char before calling a function */
        if (*cur == *patt_ptr && DSTR_MEMCMP(cur, pattern_ptr, pattern_len) == 0)
		{
            return (void*)cur;
        }
        ++cur;
    }

    return 0;
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
