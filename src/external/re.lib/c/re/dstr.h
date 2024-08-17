// dstr.h - v0.5 - kevreco - CC0 1.0 Licence (public domain)
// Close C89 implementation of std::string
// https://github.com/kevreco/re_lib

/*
CHANGES (DD/MM/YYYY):
====================

- 13/10/2019 (0.5):
    - Fix string comparison, does not stop at null terminator string
    - Add dstr_view
	- dstr takes now 16 bytes instead of 24 (on 32-bit machine), remove reference to local buffer, remove ownership flag since there is dstr_view
	- Renamed DSTR_FREE to DSTR_MFREE
	- Renamed dstr_clear to dstr_free
	- more tests

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

API:
===

dstr_init
dstr_free

dstr_at
dstr_get
dstr_data
dstr_c_str

dstr_reserve

dstr_append
dstr_append_xxx

dstr_append_from
dstr_append_xxx_from

dstr_push_back
dstr_pop_back

dstr_assign
dstr_assign_xxx

dstr_compare
dstr_compare_xxx

dstr_shrink_to_fit
dstr_empty
dstr_size
dstr_length
dstr_capacity

dstr_it dstr_begin
dstr_it dstr_end

dstr_insert
dstr_insert_xxx

dstr_erase

dstr_resize
dstr_resize_fill

dstr_replace_with
dstr_replace_with_xxx

dstr_find
dstr_find_xxx

dstr_substr

dstr_copy
dstr_swap

dstr_trim
dstr_ltrim
dstr_rtrim

dstr_substitute_dstr
dstr_substitute_str

TODO:
====

- Implement STD function:
  - dstr_find_first_of, dstr_find_first_not_of
  - dstr_find_last_of, dstr_find_last_not_of
  - dstr_getline (?)

- Find a name for non growing fmt functions (don't realloc if capacity is reached)
  - dstr_append_fmtb ? format_bounded
  - dstr_assign_fmtb ? format_bounded

- Reimplement dstr_substitute_xxx more efficiently.
  - Count the necessary space first, grow the new dstr if needed.

- testsuite:
  - dstr_find_dstr
  - dstr_copy
  - dstr_swap
  - dstr_with_buffer
  - dstr_append_fmt

- Add "DSTR_FOR_EACH" macro

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
#define DSTR_CLAMP(min_, value_, max_) (DSTR_MAX(DSTR_MIN(value_, max_), min_))

#ifndef DSTR_GROW_POLICY
// returns 150% of the capacity or use the DSTR_MIN_ALLOC value
#define DSTR_GROW_POLICY(s_, needed_size_)      \
	DSTR_MAX(                                   \
        needed_size_,                           \
        DSTR_MAX(                               \
			DSTR_MIN_ALLOC,                     \
			(s_->capacity + (s_->capacity / 2)) \
		)                                       \
	) 
#endif

#ifndef DSTR_ASSERT
#define DSTR_ASSERT   assert
#include <assert.h>
#endif

#ifndef DSTR_SIZE_T
#define DSTR_SIZE_T size_t  // default value
#endif

#ifndef DSTR_CHAR_T
#define DSTR_CHAR_T char    // default value
#endif

#include <string.h> // strlen, memcpy, memmove, memset
#include <stdlib.h> // size_t malloc free
#include <stdarg.h> // ..., va_list
#include <stdio.h>
#include <ctype.h>  // isspace
#include <stddef.h> // ptrdiff_t // TODO: maybe I should get rid of this since I use size_t almost everywhere...

#ifdef __cplusplus
extern "C" {
#endif

typedef DSTR_SIZE_T     dstr_size_t;
typedef DSTR_CHAR_T     dstr_char_t;
typedef dstr_char_t*    dstr_it;
typedef int             dstr_bool_t;

static const dstr_size_t DSTR_NPOS = (dstr_size_t)-1;

//-------------------------------------------------------------------------
// dstr_view - API - BEGIN
//-------------------------------------------------------------------------

typedef struct dstr_view_t
{
	dstr_size_t        size;
	const dstr_char_t* data;

} dstr_view;

DSTR_API dstr_view    dstr_view_make();
// create a dstr_view from the content between 'data' and 'data + size'
DSTR_API dstr_view    dstr_view_make_from(const dstr_char_t* data, dstr_size_t size);
DSTR_API dstr_view    dstr_view_make_from_str(const char* str);
DSTR_API dstr_view    dstr_view_make_from_view(dstr_view view);

DSTR_API void         dstr_view_reset(dstr_view* view);

DSTR_API void         dstr_view_assign      (dstr_view* view, const dstr_char_t* data, dstr_size_t size);
DSTR_API void         dstr_view_assign_str  (dstr_view* view, const dstr_char_t* str);

DSTR_API int          dstr_view_compare            (dstr_view sv, const dstr_char_t* data, dstr_size_t size);
DSTR_API int          dstr_view_compare_view       (dstr_view sv, dstr_view other);
DSTR_API int          dstr_view_compare_str        (dstr_view sv, const dstr_char_t* str);
DSTR_API dstr_bool_t  dstr_view_equals             (dstr_view sv, dstr_view other);
DSTR_API dstr_bool_t  dstr_view_equals_str         (dstr_view sv, const dstr_char_t* str);
DSTR_API dstr_bool_t  dstr_view_less_than          (dstr_view sv, dstr_view other);
DSTR_API dstr_bool_t  dstr_view_less_than_str      (dstr_view sv, const dstr_char_t* str);
DSTR_API dstr_bool_t  dstr_view_greater_than       (dstr_view sv, dstr_view other);
DSTR_API dstr_bool_t  dstr_view_greater_than_str   (dstr_view sv, const dstr_char_t* str);

// to mimic iterator behavior
DSTR_API inline dstr_it dstr_view_begin(dstr_view sv) { return (dstr_it)(sv.data); }
DSTR_API inline dstr_it dstr_view_end(dstr_view sv) { return (dstr_it)(sv.data + sv.size); }

// get first char
DSTR_API dstr_char_t dstr_view_front(dstr_view view);
// get last char
DSTR_API dstr_char_t dstr_view_back(dstr_view view);

DSTR_API dstr_size_t dstr_view_find(dstr_view sv, dstr_size_t pos, const dstr_char_t* sub_data, dstr_size_t sub_size);
DSTR_API dstr_size_t dstr_view_find_str(dstr_view sv, dstr_size_t pos, const dstr_char_t* sub);
DSTR_API dstr_size_t dstr_view_find_char(dstr_view sv, dstr_size_t pos, dstr_char_t ch);
DSTR_API dstr_size_t dstr_view_find_view(dstr_view sv, dstr_size_t pos, dstr_view sub);

// Returns empty string if pos == s->size
// Returns empty string if pos > s->size.
DSTR_API dstr_view dstr_view_substr(dstr_view sv, const dstr_it pos, dstr_size_t count);
DSTR_API dstr_view dstr_view_substr_from(dstr_view sv, dstr_size_t index, dstr_size_t count);

DSTR_API inline void dstr_view_swap(dstr_view* s, dstr_view* other) {
	const dstr_view tmp = *s;
	*s = *other;
	*other = tmp;
} // dstr_view_swap

//-------------------------------------------------------------------------
// dstr_view - API - END
//-------------------------------------------------------------------------

//-------------------------------------------------------------------------
// dstr - API - BEGIN
//-------------------------------------------------------------------------

typedef struct dstr {
	dstr_size_t  size;
	dstr_char_t* data;
	dstr_size_t  capacity;
	dstr_size_t  local_buffer_size; // @TODO try if we can use capacity for this
} dstr;

DSTR_API void dstr_init  (dstr* s);
DSTR_API void dstr_destroy (dstr* s);

// Non-owning reference with buffer.
// Another allocated buffer will be used if needed
// dstr_destroy must be used if more data then 'capacity' could be written
//@TODO testsuite
DSTR_API void dstr_init_from_local_buffer(dstr* s, dstr_size_t local_buffer_capacity);

// --- Constructors

// Default constructor. Constructs empty string (zero size and unspecified capacity).
DSTR_API dstr dstr_make             ();
// Empty string with 'capacity' capacity
DSTR_API dstr dstr_make_reserve     (dstr_size_t capacity);

// create a dstr from the content between 'data' and 'data + size'
DSTR_API dstr dstr_make_from        (const dstr_char_t* data, dstr_size_t size);
DSTR_API dstr dstr_make_from_str    (const dstr_char_t* str);
DSTR_API dstr dstr_make_from_char   (dstr_char_t ch);
DSTR_API dstr dstr_make_from_view   (dstr_view view);
DSTR_API dstr dstr_make_from_dstr   (const dstr* str);
DSTR_API dstr dstr_make_from_nchar  (dstr_size_t count, dstr_char_t ch);
DSTR_API dstr dstr_make_from_fmt    (const char* fmt, ...);
DSTR_API dstr dstr_make_from_vfmt   (const char* fmt, va_list args);

DSTR_API inline dstr_view dstr_to_view(const dstr* s) { return dstr_view_make_from(s->data, s->size); }

DSTR_API int          dstr_compare(const dstr* s, const dstr_char_t* data, dstr_size_t size);
DSTR_API int          dstr_compare_str(const dstr* s, const dstr_char_t* str);
DSTR_API int          dstr_compare_dstr(const dstr* s, const dstr* str);
DSTR_API dstr_bool_t  dstr_equals(const dstr* s, const dstr_char_t* data, dstr_size_t size);
DSTR_API dstr_bool_t  dstr_equals_str(const dstr* s, const dstr_char_t* other);
DSTR_API dstr_bool_t  dstr_equals_dstr(const dstr* s, const dstr* str);
DSTR_API dstr_bool_t  dstr_less_than(const dstr* s, const dstr_char_t* data, dstr_size_t size);
DSTR_API dstr_bool_t  dstr_less_than_str(const dstr* s, const dstr_char_t* str);
DSTR_API dstr_bool_t  dstr_greater_than(const dstr* s, const dstr_char_t* data, dstr_size_t size);
DSTR_API dstr_bool_t  dstr_greater_than_str(const dstr* s, const dstr_char_t* str);
// --- Element access

// Access specified character with bounds checking
DSTR_API inline dstr_char_t dstr_at         (const dstr* s, dstr_size_t index) { return s->data[index]; }
// Returns a pointer to the first character of a string
DSTR_API inline dstr_char_t* dstr_data      (dstr* s) { return s->data; }
// Returns a non-modifiable standard C character array version of the string
DSTR_API inline dstr_char_t* dstr_c_str     (dstr* s) { return s->data; }

//If new_cap is greater than the current capacity(), new storage is allocated, and capacity() is made equal or greater than new_cap.
//If new_cap is less than the current capacity(), this is a non-binding shrink request.
//If new_cap is less than the current size(), this is a non-binding shrink-to-fit request equivalent to shrink_to_fit() (since C++11). // @TODO Implement this
// @TODO testsuite
// 'new_string_capacity' is the string capacity, the effective memory allocated will be 'new_string_capacity + 1' for the null termination char '\0'
DSTR_API inline void dstr_reserve(dstr* s, dstr_size_t new_string_capacity);

// Append data from 'data' to 'data + size'
DSTR_API void dstr_append        (dstr* s, const dstr_char_t* data, dstr_size_t size);
DSTR_API void dstr_append_str    (dstr* s, const dstr_char_t* str);
DSTR_API void dstr_append_char   (dstr* s, const dstr_char_t ch);
DSTR_API void dstr_append_view   (dstr* s, dstr_view view);
DSTR_API void dstr_append_dstr   (dstr* s, const dstr* dstr);
DSTR_API void dstr_append_nchar  (dstr* s, dstr_size_t count, const dstr_char_t ch);

DSTR_API int dstr_append_fv (dstr* s, const char* fmt, va_list args);
DSTR_API int dstr_append_f  (dstr* s, const char* fmt, ...);

// append string from a certain point, appends a null terminator char at the end.
DSTR_API void dstr_append_from      (dstr* s, int index, const dstr_char_t* data, dstr_size_t size);
DSTR_API void dstr_append_str_from  (dstr* s, int index, const dstr_char_t* str);
DSTR_API void dstr_append_char_from (dstr*s, int index, char ch);
DSTR_API void dstr_append_view_from (dstr* s, int index, dstr_view view);
DSTR_API void dstr_append_dstr_from (dstr* s, int index, const dstr* str);

// FIXME: merge assign_fv and appen_from_f ?
DSTR_API int dstr_append_from_fv   (dstr* s, int index, const char* fmt, va_list args);

// Equivalent to dstr_append_char
DSTR_API inline void dstr_push_back     (dstr* s, const dstr_char_t ch) { dstr_append_char(s, ch); }

// Replaces content with the content from 'data' to 'data + (size * sizeof_value)'
DSTR_API void dstr_assign           (dstr* s, const dstr_char_t* data, dstr_size_t size);
DSTR_API void dstr_assign_str       (dstr* s, const dstr_char_t* str);
DSTR_API void dstr_assign_char      (dstr* s, dstr_char_t ch);
DSTR_API void dstr_assign_view      (dstr* s, dstr_view view);
DSTR_API void dstr_assign_dstr      (dstr* s, const dstr* str);
DSTR_API void dstr_assign_nchar     (dstr* s, dstr_size_t count, dstr_char_t ch);

DSTR_API void dstr_assign_fv        (dstr* s, const char* fmt, va_list args);
DSTR_API void dstr_assign_f         (dstr* s, const char* fmt, ...);
DSTR_API void dstr_assign_fv_nogrow (dstr* s, const char* fmt, va_list args);
DSTR_API void dstr_assign_f_nogrow  (dstr* s, const char* fmt, ...);

// Reduces memory usage by freeing unused memory
DSTR_API void               dstr_shrink_to_fit  (dstr* s);
DSTR_API inline int         dstr_empty          (const dstr* s) { return !s->size; }
DSTR_API inline dstr_size_t dstr_size           (const dstr* s) { return s->size; }
DSTR_API inline dstr_size_t dstr_length         (const dstr* s) { return s->size; }
DSTR_API inline dstr_size_t dstr_capacity       (const dstr* s) { return s->capacity; }

DSTR_API inline dstr_it dstr_begin(const dstr* s) { return dstr_view_begin(dstr_to_view(s)); }
DSTR_API inline dstr_it dstr_end(const dstr* s) { return dstr_view_end(dstr_to_view(s)); }

DSTR_API dstr_it dstr_insert(dstr* s, const dstr_it index, const dstr_char_t* data, dstr_size_t size);
DSTR_API dstr_it dstr_insert_str(dstr* s, const dstr_it index, const dstr_char_t* str);
DSTR_API dstr_it dstr_insert_char(dstr* s, const dstr_it index, const dstr_char_t value);
DSTR_API dstr_it dstr_insert_view(dstr* s, const dstr_it index, const dstr_view* view);
DSTR_API dstr_it dstr_insert_dstr(dstr* s, const dstr_it index, const dstr* str);
DSTR_API dstr_it dstr_insert_nchar(dstr* s, const dstr_it index, dstr_size_t count, const dstr_char_t ch);

DSTR_API dstr_it dstr_erase(dstr* s, const dstr_it data, dstr_size_t size);
DSTR_API dstr_it dstr_erase_value(dstr* s, const dstr_it index);
DSTR_API dstr_it dstr_erase_at(dstr* s, dstr_size_t index);
// Removes the last character from the string.
DSTR_API void dstr_pop_back(dstr* s);

// Same effect as resize(0), this does not free allocated buffer.
DSTR_API void dstr_clear(dstr* s);
// Resizes the string to contain count characters.
DSTR_API void dstr_resize               (dstr* s, dstr_size_t size);
// Resizes and fills extra spaces with 'ch' value
DSTR_API void dstr_resize_fill          (dstr* s, dstr_size_t size, dstr_char_t ch);

// repace the content between the interval [index - (index+count)] with the content of [r_data - (r_data + r_size)]
DSTR_API void dstr_replace_with       (dstr* s, dstr_size_t index, dstr_size_t count, const dstr_char_t* r_data, dstr_size_t r_size);
DSTR_API void dstr_replace_with_str   (dstr* s, dstr_size_t index, dstr_size_t count, const dstr_char_t* replacing);
DSTR_API void dstr_replace_with_char  (dstr* s, dstr_size_t index, dstr_size_t count, const dstr_char_t ch);
DSTR_API void dstr_replace_with_view  (dstr* s, dstr_size_t index, dstr_size_t count, dstr_view replacing);
DSTR_API void dstr_replace_with_dstr  (dstr* s, dstr_size_t index, dstr_size_t count, const dstr* replacing);
DSTR_API void dstr_replace_with_nchar (dstr* s, dstr_size_t index, dstr_size_t count, dstr_char_t ch, dstr_size_t ch_count);
// @TODO move this in dstr_util
// @TODO suitetest
// Returns position of the first character of the found substring or npos if no such substring is found.
DSTR_API dstr_size_t dstr_find(const dstr* s, dstr_size_t pos, const dstr_char_t* sub_data, dstr_size_t sub_size);
DSTR_API dstr_size_t dstr_find_str     (const dstr* s, dstr_size_t pos, const dstr_char_t* sub);
DSTR_API dstr_size_t dstr_find_char    (const dstr* s, dstr_size_t pos, dstr_char_t ch);
DSTR_API dstr_size_t dstr_find_view    (const dstr* s, dstr_size_t pos, dstr_view sub);
DSTR_API dstr_size_t dstr_find_dstr    (const dstr* s, dstr_size_t pos, const dstr* sub);

DSTR_API void dstr_copy_to  (const dstr* s, dstr* dest);
DSTR_API void dstr_swap     (dstr* s, dstr* other);

//-------------------------------------------------------------------------
// dstr - STD - END
//-------------------------------------------------------------------------
//-------------------------------------------------------------------------
// dstr - Extended API - BEGIN
//-------------------------------------------------------------------------

DSTR_API void dstr_trim     (dstr *s);
DSTR_API void dstr_ltrim    (dstr* s);
DSTR_API void dstr_rtrim    (dstr* s);

// @TODO more testsuite
DSTR_API void dstr_substitute_view      (dstr* s, dstr_view to_replaced, dstr_view with);
DSTR_API void dstr_substitute_dstr      (dstr* s, const dstr* to_replaced, const dstr* with);
DSTR_API void dstr_substitute_str       (dstr* s, const dstr_char_t* to_replaced, const dstr_char_t* with);


#define DSTR_DEFINETYPE(TYPENAME, LOCAL_BUFFER_SIZE)          \
typedef struct TYPENAME {                                     \
	dstr dstr;                                                \
    char local_buffer[LOCAL_BUFFER_SIZE];                     \
} TYPENAME;                                                   \
inline void TYPENAME ## _init(TYPENAME* s)                    \
{                                                             \
	dstr_init_from_local_buffer(&s->dstr, LOCAL_BUFFER_SIZE); \
}                                                             \
inline void TYPENAME ## _destroy(TYPENAME* s)                 \
{                                                             \
	dstr_destroy(&s->dstr);                                   \
}                                                             \
inline void TYPENAME ## _assign_fv(TYPENAME* s, const char* fmt, va_list args) \
{                                                                              \
	dstr_assign_fv(&s->dstr, fmt, args);                                       \
}

DSTR_DEFINETYPE(dstr16, 16);
DSTR_DEFINETYPE(dstr32, 32);
DSTR_DEFINETYPE(dstr64, 64);
DSTR_DEFINETYPE(dstr128, 128);
DSTR_DEFINETYPE(dstr256, 256);
DSTR_DEFINETYPE(dstr512, 512);
DSTR_DEFINETYPE(dstr1024, 1024);

//-------------------------------------------------------------------------
// dstr - Extended API - END
//-------------------------------------------------------------------------

#ifdef __cplusplus
} // extern "C"
#endif

//-------------------------------------------------------------------------
// dstr - Private - BEGIN
//-------------------------------------------------------------------------

DSTR_INTERNAL void  dstr__reserve_no_preserve_data(dstr* s, dstr_size_t new_string_capacity);
DSTR_INTERNAL void  dstr__reserve_internal(dstr* s, dstr_size_t new_string_capacity, dstr_bool_t _preserve_data);
// This should be an internal function but some other of my files are using ths.
DSTR_API void* dstr__memory_find(const void* memory_ptr, dstr_size_t mem_len, const void* pattern_ptr, dstr_size_t pattern_len);

// Equivalent of strncmp, this implementation does not stop on null termination char
DSTR_INTERNAL int   dstr__lexicagraphical_cmp(const char* _str1, size_t _count, const char* _str2, size_t _count2);

DSTR_INTERNAL int   dstr__is_allocated      (dstr* s);

// code related to dstr with local buffer

DSTR_INTERNAL dstr_bool_t  dstr__owns_local_buffer     (dstr* s);
DSTR_INTERNAL dstr_char_t* dstr__get_local_buffer      (dstr* s);
DSTR_INTERNAL dstr_bool_t  dstr__is_using_local_buffer (dstr* s);

//-------------------------------------------------------------------------
// dstr - Private - END
//-------------------------------------------------------------------------
//-------------------------------------------------------------------------
// dstr - API Implementation - BEGIN
//-------------------------------------------------------------------------

#ifdef DSTR_IMPLEMENTATION

// On some platform vsnprintf() takes va_list by reference and modifies it.
// va_copy is the 'correct' way to copy a va_list but Visual Studio prior to 2013 doesn't have it.
#ifndef va_copy
#define va_copy(dest, src) (dest = src)
#endif

DSTR_API dstr_view dstr_view_make()
{
	dstr_view sv;
	sv.data = 0;
	sv.size = 0;
	return sv;
} // dstr_view_make

DSTR_API dstr_view dstr_view_make_from(const dstr_char_t* data, dstr_size_t size)
{
	dstr_view sv;
	sv.data = data;
	sv.size = size;
	return sv;
} // dstr_view_make_from

DSTR_API dstr_view dstr_view_make_from_str(const char* str)
{
	return dstr_view_make_from(str, strlen(str));
} // dstr_view_make_from_str

DSTR_API dstr_view dstr_view_make_from_view(dstr_view view)
{
	return dstr_view_make_from(view.data, view.size);
} // dstr_view_make_from_view


DSTR_API void dstr_view_reset(dstr_view* view)
{
	view->size = 0;
	view->data = 0;
} // dstr_view_reset

DSTR_API void dstr_view_assign(dstr_view* view, const dstr_char_t* data, dstr_size_t size)
{
	view->size = size;
	view->data = data;
} // dstr_view_assign

DSTR_API void dstr_view_assign_str(dstr_view* view, const dstr_char_t* str)
{
	if (str == 0) // maybe this case shouldn't be possible...
	{
		view->size = 0;
		view->data = 0;
	}
	else
	{
		view->size = strlen(str);
		view->data = str;
	}
} // dstr_view_assign_str

DSTR_API int dstr_view_compare(dstr_view sv, const dstr_char_t* data, dstr_size_t size)
{
	return dstr__lexicagraphical_cmp(sv.data, sv.size, data, size);
} // dstr_view_compare

DSTR_API int dstr_view_compare_view(dstr_view sv, dstr_view other)
{
	return dstr_view_compare(sv, other.data, other.size);
} // dstr_view_compare

DSTR_API int dstr_view_compare_str(dstr_view sv, const dstr_char_t* str)
{
	return dstr_view_compare(sv, str, strlen(str));
}

DSTR_API dstr_bool_t dstr_view_equals(dstr_view sv, dstr_view other)
{
	return dstr_view_compare_view(sv, other) == 0;
}

DSTR_API dstr_bool_t dstr_view_equals_str(dstr_view sv, const dstr_char_t* str)
{
	return dstr_view_compare_str(sv, str) == 0;
}

DSTR_API dstr_bool_t dstr_view_less_than(dstr_view sv, dstr_view other)
{
	return dstr_view_compare_view(sv, other) < 0;
}

DSTR_API dstr_bool_t dstr_view_less_than_str(dstr_view sv, const dstr_char_t* str)
{
	return dstr_view_compare_str(sv, str) < 0;
}

DSTR_API dstr_bool_t dstr_view_greater_than(dstr_view sv, dstr_view other)
{
	return dstr_view_compare_view(sv, other) > 0;
}

DSTR_API dstr_bool_t dstr_view_greater_than_str(dstr_view sv, const dstr_char_t* str)
{
	return dstr_view_compare_str(sv, str) > 0;
}

DSTR_API dstr_char_t dstr_view_front(dstr_view view)
{
	DSTR_ASSERT(view.size > 0);
	return view.data[0];
}

DSTR_API dstr_char_t dstr_view_back(dstr_view view)
{
	DSTR_ASSERT(view.size > 0);
	return view.data[view.size - 1];
}

DSTR_API dstr_size_t dstr_view_find(dstr_view sv, dstr_size_t pos, const dstr_char_t* sub_data, dstr_size_t sub_size)
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
} // dstr_view_find

DSTR_API dstr_size_t dstr_view_find_str(dstr_view sv, dstr_size_t pos, const dstr_char_t* sub) {

	return dstr_view_find(sv, pos, sub, strlen(sub));
} // dstr_view_find_str

DSTR_API dstr_size_t dstr_view_find_char(dstr_view sv, dstr_size_t pos, dstr_char_t ch) {

	return dstr_view_find(sv, pos, &ch, 1);
} // dstr_view_find_char

DSTR_API dstr_size_t dstr_view_find_view(dstr_view sv, dstr_size_t pos, dstr_view sub) {

	return dstr_view_find(sv, pos, sub.data, sub.size);
} // dstr_view_find_view

DSTR_API dstr_view dstr_view_substr(dstr_view sv, const dstr_it pos, dstr_size_t count)
{
	const dstr_it last = pos + (count * sizeof(dstr_char_t));

	DSTR_ASSERT(pos >= dstr_view_begin(sv) && pos < dstr_view_end(sv));
	DSTR_ASSERT(last >= pos && last <= dstr_view_end(sv));

	dstr_view result;

	result.data = pos;
	result.size = count;

	return result;
} // dstr_view_substr

DSTR_API dstr_view dstr_view_substr_from(dstr_view sv, dstr_size_t index, dstr_size_t count)
{
	const dstr_it it = (const dstr_it)(sv.data + (index * sizeof(dstr_char_t)));
	return dstr_view_substr(sv, it, count);
} // dstr_view_substr_from

// Shared default value to ensure that s->data is always valid with a '\0' char when a dstr is initialized
static dstr_char_t DSTR__DEFAULT_DATA[1] = { '\0' };

#define DSTR__GROW(s, needed) \
    dstr_reserve(s,  DSTR_GROW_POLICY(s, needed));

#define DSTR__GROW_DISCARD(s, needed) \
    dstr__reserve_no_preserve_data(s,  DSTR_GROW_POLICY(s, needed));

#define DSTR__GROW_IF_NEEDED(s, needed) \
    if (needed > s->capacity) {         \
    DSTR__GROW(s, needed)               \
}

#define DSTR__GROW_IF_NEEDED_DISCARD(s, needed) \
    if (needed > s->capacity) {         \
    DSTR__GROW_DISCARD(s, needed)               \
}

DSTR_API void dstr_init(dstr* s) {

    s->size = 0;
    s->data = DSTR__DEFAULT_DATA;
	s->capacity = 0;
	s->local_buffer_size = 0;

} // dstr_init

DSTR_API void dstr_destroy(dstr* s) {

    // dstr is initialized
    if (dstr__is_allocated(s)) {
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

} // dstr_destroy

DSTR_API void dstr_init_from_local_buffer(dstr* s, dstr_size_t local_buffer_size)
{
	s->data = dstr__get_local_buffer(s);

	s->size = 0;
	s->data[s->size] = '\0';
	// capacity is the number of char a string can hold (null terminating char is not counted)
	// therefore capacity is equal it's equal to
	s->capacity = local_buffer_size - 1;
	s->local_buffer_size = local_buffer_size;
} // dstr_init_from_local_buffer

DSTR_API dstr dstr_make() {
    dstr result;

    dstr_init(&result);

    return result;
} // dstr_make

DSTR_API dstr dstr_make_reserve(dstr_size_t capacity) {
    dstr result;

    dstr_init(&result);

    if (capacity) {
        dstr_reserve(&result, capacity);
    }

    return result;
} // dstr_make_reserve

DSTR_API dstr dstr_make_from(const dstr_char_t* data, dstr_size_t size) {
	dstr result;

	dstr_init(&result);
	dstr_assign(&result, data, size);

	return result;
} // dstr_make_from

DSTR_API dstr dstr_make_from_str(const dstr_char_t* str) {

	return dstr_make_from(str, strlen(str));
} // dstr_make_from_str

DSTR_API dstr dstr_make_from_char(dstr_char_t ch)
{
	return dstr_make_from(&ch, 1);
}

DSTR_API dstr dstr_make_from_view(dstr_view view)
{
	return dstr_make_from(view.data, view.size);
} // dstr_make_from_view

DSTR_API dstr dstr_make_from_dstr(const dstr* str) {
    
	return dstr_make_from(str->data, str->size);
} // dstr_make_from_dstr

DSTR_API dstr dstr_make_from_nchar(dstr_size_t count, dstr_char_t ch) {
    dstr result;

    dstr_init(&result);
    dstr_assign_nchar(&result, count, ch);

    return result;
} // dstr_make_from_nchar

DSTR_API dstr dstr_make_from_fmt(const char* fmt, ...)
{
	dstr result;
	dstr_init(&result);

	va_list args;
	va_start(args, fmt);

	dstr_append_fv(&result, fmt, args);

	va_end(args);

	return result;
} // dstr_make_from_fmt

DSTR_API dstr dstr_make_from_vfmt(const char* fmt, va_list args)
{
	dstr result;
	dstr_init(&result);

	dstr_append_fv(&result, fmt, args);

	return result;
} // dstr_make_from_vfmt

DSTR_API int dstr_compare(const dstr* s, const dstr_char_t* data, dstr_size_t size)
{
	return dstr_view_compare(dstr_to_view(s), data, size);
} // dstr_compare

DSTR_API int dstr_compare_str(const dstr* s, const dstr_char_t* str)
{
	return dstr_compare(s, str, strlen(str));
} // dstr_compare_str

DSTR_API int dstr_compare_dstr(const dstr* s, const dstr* str)
{
	return dstr_compare(s, str->data, str->size);
}

DSTR_API dstr_bool_t dstr_equals(const dstr* s, const dstr_char_t* data, dstr_size_t size)
{
	return dstr_compare(s, data, size) == 0;
} // dstr_equals

DSTR_API dstr_bool_t dstr_equals_str(const dstr* s, const dstr_char_t* str)
{
	return dstr_compare(s, str, strlen(str)) == 0;
} // dstr_equals_str

DSTR_API dstr_bool_t dstr_equals_dstr(const dstr* s, const dstr* str)
{
	return dstr_compare(s, str->data, str->size) == 0;
} // dstr_equals_dstr

DSTR_API dstr_bool_t dstr_less_than(const dstr* s, const dstr_char_t* data, dstr_size_t size)
{
	return dstr_compare(s, data, size) < 0;
} // dstr_less_than

DSTR_API dstr_bool_t dstr_less_than_str(const dstr* s, const dstr_char_t* str)
{
	return dstr_compare(s, str, strlen(str)) < 0;
} // dstr_less_than_str

DSTR_API dstr_bool_t dstr_greater_than(const dstr* s, const dstr_char_t* data, dstr_size_t size)
{
	return dstr_compare(s, data, size) > 0;
} // dstr_greater_than

DSTR_API dstr_bool_t dstr_greater_than_str(const dstr* s, const dstr_char_t* str)
{
	return dstr_compare(s, str, strlen(str)) > 0;
} // dstr_greater_than_str

void dstr_reserve(dstr* s, dstr_size_t new_string_capacity)
{
	dstr_bool_t preserve_data = 1;
	dstr__reserve_internal(s, new_string_capacity, preserve_data);
} // dstr_reserve

DSTR_API void dstr_append(dstr* s, const dstr_char_t* data, dstr_size_t size) {

	dstr_append_from(s, s->size, data, size);

} // dstr_append

DSTR_API void dstr_append_str(dstr* s, const dstr_char_t* str) {

	dstr_append(s, str, strlen(str));
} // dstr_append_str

void dstr_append_char(dstr* s, const dstr_char_t ch) {

	dstr_append_char_from(s, s->size, ch);
} // dstr_append_char

DSTR_API void dstr_append_view(dstr* s, dstr_view view) {

	dstr_append(s, view.data, view.size);
} // dstr_append_view

DSTR_API void dstr_append_dstr(dstr* s, const dstr* other) {

	dstr_append(s, other->data, other->size);
} // dstr_append_dstr

void dstr_append_nchar(dstr* s, dstr_size_t count, const dstr_char_t ch) {

    dstr_size_t capacity_needed = s->size + count;

	DSTR__GROW_IF_NEEDED(s, capacity_needed);

    dstr_char_t* first = s->data + s->size;
    dstr_char_t* last = first + count;
    for (; first != last; ++first) {
        *first = ch;
    }

    s->size += count;
    s->data[s->size] = '\0';
} // dstr_append_nchar

DSTR_API int dstr_append_fv(dstr* s, const char* fmt, va_list args)
{
	return dstr_append_from_fv(s, s->size, fmt, args);
} // dstr_append_fv

DSTR_API int dstr_append_f(dstr* s, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	int len = dstr_append_fv(s, fmt, args);
	va_end(args);
	return len;
} // dstr_append_f

DSTR_API void dstr_append_from(dstr* s, int index, const dstr_char_t* data, dstr_size_t size)
{
	DSTR__GROW_IF_NEEDED(s, index + size); 

	DSTR_MEMCPY(s->data + index, (const void*)data, ((size) * sizeof(dstr_char_t)));
	s->size = index + size;
	s->data[s->size] = '\0';
} // dstr_append_from

DSTR_API void dstr_append_str_from(dstr* s, int index, const dstr_char_t* str)
{
	dstr_append_from(s, index, str, strlen(str));
} // dstr_append_str_from

DSTR_API void dstr_append_char_from(dstr*s, int index, char c)
{
	dstr_append_from(s, index, &c, 1);
} // dstr_append_char_from

DSTR_API void dstr_append_view_from(dstr* s, int index, dstr_view view)
{
	dstr_append_from(s, index, view.data, view.size);
}

DSTR_API void dstr_append_dstr_from(dstr* s, int index, const dstr* str)
{
	dstr_append_from(s, index, str->data, str->size);
} // dstr_append_dstr_from

DSTR_API int dstr_append_from_fv(dstr* s, int index, const char* fmt, va_list args)
{
	// Needed for portability on platforms where va_list are passed by reference and modified by functions
	va_list args2;
	va_copy(args2, args);

	// MSVC returns -1 on overflow when writing, which forces us to do two passes
	// FIXME-OPT: Find a way around that.
#ifdef _MSC_VER
	int add_len = vsnprintf(NULL, 0, fmt, args);
	DSTR_ASSERT(add_len >= 0);

	DSTR__GROW_IF_NEEDED(s, (size_t)(index + add_len));

	add_len = vsnprintf(s->data + index, add_len + 1, fmt, args2);
#else
	// First try
	int add_len = vsnprintf(s->data + index, 0, fmt, args);
	DSTR_ASSERT(add_len >= 0);

	if (s->capacity < s->size + add_len)
	{
		DSTR__GROW(s, index + add_len);
		add_len = vsnprintf(s->data + index, add_len + 1, fmt, args2);
	}
#endif
	s->size += add_len;
	return add_len;
} // dstr_append_from_fv

void dstr_pop_back(dstr* s) {
    DSTR_ASSERT(s->size);

    --(s->size);
    s->data[s->size] = '\0';
} // dstr_pop_back

void dstr_assign(dstr* s, const dstr_char_t* data, dstr_size_t size) {

	dstr_size_t capacity_needed = size;
	
	DSTR__GROW_IF_NEEDED_DISCARD(s, capacity_needed);

	DSTR_MEMCPY((char*)s->data, data, size * sizeof(dstr_char_t));
	s->data[size] = '\0';
	s->size = size;
} // dstr_assign

void dstr_assign_char(dstr* s, dstr_char_t ch) {

	dstr_assign(s, &ch, 1);
} // dstr_assign_char

void dstr_assign_str(dstr* s, const dstr_char_t* str) {

	dstr_assign(s, str, strlen(str));
} // dstr_assign_str

void dstr_assign_view(dstr* s, dstr_view view) {

	dstr_assign(s, view.data, view.size);
} // dstr_assign_view

void dstr_assign_dstr(dstr* s, const dstr* str) {

    dstr_assign(s, str->data, str->size);
} // dstr_assign_dstr

void dstr_assign_nchar(dstr* s, dstr_size_t count, dstr_char_t ch) {

	dstr_size_t size = count;

	dstr_size_t capacity_needed = size;
	DSTR__GROW_IF_NEEDED_DISCARD(s, capacity_needed);

    dstr_it first = s->data;
    dstr_it last = s->data + count;
    for (; first != last; ++first) {
        *first = ch;
    }

	s->data[size] = '\0';
	s->size = size;
} // dstr_assign_nchar

DSTR_API void dstr_assign_fv(dstr* s, const char* fmt, va_list args)
{
	// Needed for portability on platforms where va_list are passed by reference and modified by functions
	va_list args2;
	va_copy(args2, args);

	// MSVC returns -1 on overflow when writing, which forces us to do two passes
	// FIXME-OPT: Find a way around that.
#ifdef _MSC_VER
	int len = vsnprintf(NULL, 0, fmt, args);
	DSTR_ASSERT(len >= 0);

	DSTR__GROW_IF_NEEDED_DISCARD(s, (dstr_size_t)len);

	len = vsnprintf(s->data, len + 1, fmt, args2); // +1 for '\0'
#else
	// First try
	int len = vsnprintf(s->data, s->capacity, fmt, args);
	DSTR_ASSERT(len >= 0);

	if (s->capacity < (dstr_size_t)(len))
	{
		DSTR__GROW_DISCARD(s, len);
		len = vsnprintf(s->data, len + 1, fmt, args2);
	}
#endif

	s->size = len;
	s->data[s->size] = '\0';
}

void dstr_assign_f(dstr* s, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	dstr_assign_fv(s, fmt, args);
	va_end(args);
}

void dstr_assign_fv_nogrow(dstr* s, const char* fmt, va_list args)
{
	int size = vsnprintf(s->data, s->capacity + 1, fmt, args);
	if (size == -1)
		size = s->capacity;

	s->size = size;
	s->data[size] = 0;
}

void dstr_assign_f_nogrow(dstr* s, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	dstr_assign_fv_nogrow(s, fmt, args);
	va_end(args);
}

void dstr_shrink_to_fit(dstr* s)
{
	dstr_char_t * new_data;
	dstr_size_t new_capacity;
	if (dstr__owns_local_buffer(s)
		&& s->size <= s->local_buffer_size - 1)
	{
		new_data = dstr__get_local_buffer(s);
		new_capacity = s->local_buffer_size - 1; // - 1 for '\0' char
	}
	else
	{
		new_capacity = s->size;
		new_data = (dstr_char_t*)DSTR_MALLOC((s->size + 1) * sizeof(dstr_char_t));
	}

	DSTR_ASSERT(new_data);
	
	memcpy(new_data, s->data, s->size + 1); // +1 because we want to copy the '\0'

	if (dstr__is_allocated(s))
		DSTR_FREE(s->data);

	s->data = new_data;
	s->capacity = new_capacity;

} // dstr_shrink_to_fit

dstr_it dstr_insert(dstr* s, const dstr_it index, const dstr_char_t* data, dstr_size_t size)
{
	DSTR_ASSERT(index >= dstr_begin(s) && index <= dstr_end(s));

	const dstr_size_t count = size;
	const ptrdiff_t offset = index - s->data;
	const dstr_size_t distance_from_index_to_end = (dstr_size_t)(dstr_end(s) - index);
	const dstr_size_t capacity_needed = s->size + count;

	DSTR__GROW_IF_NEEDED(s, capacity_needed);

	// There is data between index and end to move
	if (distance_from_index_to_end > 0)
	{
		DSTR_MEMMOVE(
			s->data + offset + (count * sizeof(dstr_char_t)),
			s->data + offset,
			distance_from_index_to_end
		);
	}

	DSTR_MEMCPY(s->data + offset, data, (count * sizeof(dstr_char_t)));

	s->size += count;
	s->data[s->size] = '\0';

	return s->data + offset;
} // dstr_insert

DSTR_API dstr_it dstr_insert_str(dstr* s, const dstr_it index, const dstr_char_t* str)
{
	return dstr_insert(s, index, str, strlen(str));
} // dstr_insert_str

DSTR_API dstr_it dstr_insert_char(dstr* s, const dstr_it index, const dstr_char_t ch) {

	return dstr_insert(s, index, &ch, 1);
} // dstr_insert_char

DSTR_API dstr_it dstr_insert_view(dstr* s, const dstr_it index, const dstr_view* view)
{
	return dstr_insert(s, index, view->data, view->size);
} //dstr_insert_view

DSTR_API dstr_it dstr_insert_dstr(dstr* s, const dstr_it index, const dstr* str)
{
	return dstr_insert(s, index, str->data, str->size);
} // dstr_insert_dstr

DSTR_API dstr_it dstr_insert_nchar(dstr* s, const dstr_it index, dstr_size_t count, const dstr_char_t ch)
{
	DSTR_ASSERT(index >= s->data && index <= s->data + s->size);

	const ptrdiff_t offset = index - s->data;

	dstr_size_t capacity_needed = s->size + count;

	DSTR__GROW_IF_NEEDED(s, capacity_needed);

	if (offset < (ptrdiff_t)s->size) {
		memmove(s->data + offset + count, s->data + offset, ((dstr_size_t)s->size - (dstr_size_t)offset) * sizeof(dstr_char_t));
	}

	memset(s->data + offset, ch, count * sizeof(dstr_char_t));

	s->size += count;

	s->data[s->size] = '\0';

	return s->data + offset;
} // dstr_insert_nchar

DSTR_API dstr_it dstr_erase_ref(dstr* s, const dstr_it first, dstr_size_t count)
{
	const dstr_it last = first + (count * sizeof(dstr_char_t));

	DSTR_ASSERT(first >= s->data && first <= (s->data + s->size));
	DSTR_ASSERT(last >= s->data && last <= (s->data + s->size));

	const dstr_size_t first_index = (dstr_size_t)(first - s->data);
	const dstr_size_t last_index = (dstr_size_t)(last - s->data);
	const dstr_size_t count_removed = last_index - first_index;
	const dstr_size_t count_to_move = (s->size - last_index) + 1; // +1 for '\0'

	DSTR_MEMMOVE(s->data + first_index, s->data + last_index, count_to_move * sizeof(dstr_char_t));

	s->size -= count_removed;

	return s->data + first_index;
} // dstr_erase_ref

DSTR_API dstr_it dstr_erase(dstr* s, const dstr_it first, dstr_size_t size)
{
	if (!size) return dstr_begin(s);

	const dstr_it last = first + (size * sizeof(dstr_char_t));

	DSTR_ASSERT(first >= dstr_begin(s) && first < dstr_end(s));
	DSTR_ASSERT(last >= first && last <= dstr_end(s));

	const dstr_size_t first_index = (dstr_size_t)(first - s->data);
	const dstr_size_t last_index = (dstr_size_t)(last - s->data);
	const dstr_size_t byte_to_move = (dstr_size_t)(dstr_end(s) - last) + 1;  // +1 for '\0'

	DSTR_MEMMOVE(s->data + first_index, s->data + last_index, byte_to_move);

	s->size -= size;

	return s->data + first_index;
} // dstr_erase

DSTR_API dstr_it dstr_erase_value(dstr* s, const dstr_it index)
{
	return dstr_erase(s, index, 1);
} // dstr_erase_value

DSTR_API dstr_it dstr_erase_at(dstr* s, dstr_size_t index)
{
	dstr_char_t* value = s->data + (index * sizeof(dstr_char_t));
	return dstr_erase_value(s, value);
} // dstr_erase_at

DSTR_API void dstr_clear(dstr* s) {
	dstr_resize(s, 0);
}

DSTR_API void dstr_resize(dstr* s, dstr_size_t size) {

	if (s->size == size)
		return;

   // if (!size) {
    //    dstr_free(s);
   // }  else {

        dstr_size_t extra_count = 0;

        // +1 for extra char
        if (size > s->capacity){

            DSTR__GROW(s, size);
            extra_count = s->capacity - s->size;

        } else if (size > s->size){
            extra_count = size - s->size;
        }

        if (extra_count) {
            memset(s->data + s->size, 0, extra_count * sizeof(dstr_char_t));
        }

        s->size = size;
        s->data[s->size] = '\0';
    //}
} // dstr_resize

DSTR_API void dstr_resize_fill(dstr* s, dstr_size_t size, dstr_char_t ch) {

    if (!size) {
        dstr_destroy(s);
    }  else {

        dstr_size_t extra_count = 0;

        // +1 for extra char
        if (size + 1 > s->capacity){

            DSTR__GROW(s, size + 1);
            extra_count = s->capacity - s->size;

        } else if (size > s->size){
            extra_count = size - s->size;
        }

        if (extra_count) {

            dstr_char_t* begin = s->data + s->size;
            dstr_char_t* end   = begin + extra_count;
            while (begin != end) {
                *begin = ch;
                ++begin;
            }
        }

        s->size = size;
        s->data[s->size] = '\0';
    }
} // dstr_resize_fill

DSTR_API void dstr_replace_with(dstr* s, dstr_size_t index, dstr_size_t count, const dstr_char_t* r_data, dstr_size_t r_size) {

	DSTR_ASSERT(index <= s->size);
	DSTR_ASSERT(count <= s->size);
	DSTR_ASSERT(index + count <= s->size);

	if (r_size < count) { // mem replacing <  mem to replace

		char* first = s->data + index;
		char* last = (s->data + index + count);

		dstr_size_t count_to_move = s->size - (index + count);
		dstr_size_t count_removed = count - r_size;

		if (count_to_move) {
			DSTR_MEMMOVE(last - count_removed, last, count_to_move * sizeof(dstr_char_t));
		}
		if (s->size) {
			DSTR_MEMCPY(first, r_data, r_size * sizeof(dstr_char_t));
		}

		s->size -= count_removed;
		s->data[s->size] = '\0';

	}
	else if (r_size > count) { // mem replacing >  mem to replace

		dstr_size_t extra_count = r_size - count;
		dstr_size_t needed_capacity = s->size + extra_count;
		dstr_size_t count_to_move = s->size - index - count;

		DSTR__GROW_IF_NEEDED(s, needed_capacity);

		// Need to set this after "grow" because of potential allocation
		char* first = s->data + index;
		char* last = s->data + index + count;

		if (count_to_move) {
			DSTR_MEMMOVE(last + extra_count, last, count_to_move * sizeof(dstr_char_t));
		}

		DSTR_MEMCPY(first, r_data, r_size * sizeof(dstr_char_t));

		s->size += extra_count;
		s->data[s->size] = '\0';

	}
	else { // mem replacing == mem to replace
		char* first = s->data + index;
		DSTR_MEMCPY(first, r_data, r_size * sizeof(dstr_char_t));
	}

} // dstr_replace_with

DSTR_API void dstr_replace_with_str(dstr* s, dstr_size_t index, dstr_size_t count, const dstr_char_t* replacing) {

	dstr_replace_with(s, index, count, replacing, strlen(replacing));
} // dstr_replace_with_str

DSTR_API void dstr_replace_with_char(dstr* s, dstr_size_t index, dstr_size_t count, const dstr_char_t ch) {

	dstr_replace_with(s, index, count, &ch, 1);
} // dstr_replace_with_char

DSTR_API void dstr_replace_with_view(dstr* s, dstr_size_t index, dstr_size_t count, dstr_view replacing) {
	dstr_replace_with(s, index, count, replacing.data, replacing.size);
} // dstr_replace_with_view

DSTR_API void dstr_replace_with_dstr(dstr* s, dstr_size_t index, dstr_size_t count, const dstr* replacing) {
	dstr_replace_with(s, index, count, replacing->data, replacing->size);
} // dstr_replace_with_dstr

DSTR_API inline void dstr_replace_with_nchar(dstr* s, dstr_size_t index, dstr_size_t count, dstr_char_t ch, dstr_size_t ch_count) {
    // There is an allocation to construct 'tmp':
    // @TODO: implement this without any alloc.
    dstr tmp = dstr_make_from_nchar(ch_count, ch);

    dstr_replace_with_dstr(s, index, count, &tmp);
} // dstr_replace_with_nchar

DSTR_API dstr_size_t dstr_find(const dstr* s, dstr_size_t pos, const dstr_char_t* sub_data, dstr_size_t sub_size)
{
	return dstr_view_find(dstr_to_view(s), pos, sub_data, sub_size);
} // dstr_find

DSTR_API dstr_size_t dstr_find_str(const dstr* s, dstr_size_t pos, const dstr_char_t* sub)
{
	return dstr_view_find_str(dstr_to_view(s), pos, sub);
} // dstr_find_str

DSTR_API dstr_size_t dstr_find_char(const dstr* s, dstr_size_t pos, dstr_char_t ch)
{
	return dstr_view_find_char(dstr_to_view(s), pos, ch);
} // dstr_find_char

DSTR_API dstr_size_t dstr_find_view(const dstr* s, dstr_size_t pos, dstr_view sub)
{
	return dstr_view_find_view(dstr_to_view(s), pos, sub);
} // dstr_find_view

DSTR_API dstr_size_t dstr_find_dstr(const dstr* s, dstr_size_t pos, const dstr* sub)
{
	return dstr_view_find_view(dstr_to_view(s), pos, dstr_to_view(sub));
} // dstr_find_dstr

DSTR_API inline void dstr_copy_to(const dstr* s, dstr* dest)
{
	dstr_destroy(dest);
    dstr_size_t needed_capacity = s->size;
    DSTR__GROW_IF_NEEDED_DISCARD(dest, needed_capacity);
    DSTR_MEMCPY(dest->data, s->data, needed_capacity * sizeof(dstr_char_t) );
} // dstr_copy

DSTR_API inline void dstr_swap(dstr* s, dstr* other)
{
    const dstr tmp = *s;
    *s = *other;
    *other = tmp;
} // dstr_swap

//-------------------------------------------------------------------------
// dstr - API Implementation - END
//-------------------------------------------------------------------------

//-------------------------------------------------------------------------
// dstr - Extended Implementation - BEGIN
//-------------------------------------------------------------------------

DSTR_API void dstr_trim(dstr* s)
{
    dstr_char_t* cursor_left  = s->data;
    dstr_char_t* cursor_right = s->data + (s->size - 1);

    // Trim right
    while (cursor_right >= cursor_left && isspace(*cursor_right)) {
        --cursor_right;
    }

    // Trim left
    while (cursor_right > cursor_left && isspace(*cursor_left)) {
        ++cursor_left;
    }

    s->size = (cursor_right - cursor_left) + 1;
    memmove(s->data, cursor_left, s->size * sizeof(dstr_char_t));

    s->data[s->size] = '\0';
} // dstr_trim

DSTR_API void dstr_ltrim(dstr* s) {
    char *cursor = s->data;

    while (s->size > 0 && isspace(*cursor)) {
        ++cursor;
        --s->size;
    }

    memmove(s->data, cursor, s->size * sizeof(dstr_char_t));
    s->data[s->size] = '\0';
} // dstr_ltrim

DSTR_API void dstr_rtrim(dstr* s)
{
    while (s->size > 0 && isspace(s->data[s->size - 1])) {
        --s->size;
    }
    s->data[s->size] = '\0';
} // dstr_rtrim

DSTR_API void dstr_substitute_view(dstr* s, dstr_view to_replaced, dstr_view with) {

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

        // reset begin and end, could be invalidated (realloc)
        // put the next position to index + found word length
        s_begin = s->data + index + with.size;
        s_end = s->data + s->size;
    }
} // dstr_substitute_view

DSTR_API void dstr_substitute_dstr(dstr* s, const dstr* to_replaced, const dstr* with) {

	dstr_substitute_view(s, dstr_to_view(to_replaced), dstr_to_view(with));
} // dstr_substitute_dstr

DSTR_API void dstr_substitute_str(dstr* s, const dstr_char_t* to_replaced, const dstr_char_t* with) {

    const dstr_view tmp_to_replaced = {
        strlen(to_replaced),
		(dstr_char_t*)to_replaced,
    };

    const dstr_view tmp_with = {
        strlen(with),
		(dstr_char_t*)with,
    };

    dstr_substitute_view(s, tmp_to_replaced, tmp_with);

} // dstr_substitute_str


//-------------------------------------------------------------------------
// dstr - Extended Implementation - END
//-------------------------------------------------------------------------

//-------------------------------------------------------------------------
// dstr - Private Implementation - BEGIN
//-------------------------------------------------------------------------

DSTR_INTERNAL void dstr__reserve_no_preserve_data(dstr* s, dstr_size_t new_string_capacity)
{
	dstr_bool_t preserve_data = 0;
	dstr__reserve_internal(s, new_string_capacity, preserve_data);
} // dstr__reserve_no_preserve_data

DSTR_INTERNAL void dstr__reserve_internal(dstr* s, dstr_size_t new_string_capacity, dstr_bool_t _preserve_data)
{
	if (new_string_capacity <= s->capacity)
		return;

	dstr_size_t  memory_capacity = new_string_capacity + 1; // capacity + 1 for '\0';

	// Unneeded condition since it's handled by 'new_capacity <= s->capacity' above
	// Keep it for clarity
	if (dstr__is_using_local_buffer(s) && memory_capacity <= s->local_buffer_size)
		return;

	dstr_char_t* new_data = (dstr_char_t*)DSTR_MALLOC(memory_capacity * sizeof(dstr_char_t));

	DSTR_ASSERT(new_data);

	if (_preserve_data)
	{
		// This should have make more sense to have strcpy here instead of memcpy
		// but strcpy prevent dstr to be usable as raw buffer.
		// strcpy would just stop the copy on the first null character value.

		DSTR_MEMCPY(new_data, s->data, ( s->size + 1) * sizeof(dstr_char_t));
	}

	if (dstr__is_allocated(s))
		DSTR_FREE(s->data);

	s->data = new_data;
	s->capacity = new_string_capacity;
} // dstr__reserve_internal

DSTR_INTERNAL int dstr__lexicagraphical_cmp(const char* _str1, size_t _count, const char* _str2, size_t _count2)
{
	char c1, c2;
	size_t min_size = _count < _count2 ? _count : _count2;
	while (min_size-- > 0)
	{
		c1 = (unsigned char)*_str1++;
		c2 = (unsigned char)*_str2++;
		if (c1 != c2)
			return c1 < c2 ? 1 : -1;
	};

	return _count - _count2;
} // dstr__lexicagraphical_cmp

// The correctness and speed of this alternative need to be tested.
DSTR_INTERNAL int dstr__lexicagraphical_cmp_alt(const char* _str1, size_t _count, const char* _str2, size_t _count2)
{
	int result;

	size_t min = _count < _count2 ? _count : _count2;

	// memcmp is used because strncmp terminates on '\0'
	// a dstr can be "aaa\0bbb" with a size of 7
	int cmp = DSTR_MEMCMP(_str1, _str2, min);

	if (cmp) {
		result = cmp;
	}
	else { // strings are equal until 'min' chars count
		result = _count2 - _count; //_count < rhs.size ? -1 : _count != rhs.size;
	}

	return result;
}

DSTR_INTERNAL int dstr__is_allocated(dstr* s) {

    return s->data != dstr__get_local_buffer(s) && s->data != DSTR__DEFAULT_DATA;
} // dstr__is_allocated

DSTR_API void* dstr__memory_find(const void* memory_ptr, dstr_size_t mem_len, const void* pattern_ptr, dstr_size_t pattern_len)
{
    const char *mem_ptr = (const char *)memory_ptr;
    const char *patt_ptr = (const char *)pattern_ptr;

    // pattern_len can't be greater than mem_len
    if ((mem_len == 0) || (pattern_len == 0) || pattern_len > mem_len) {
        return 0;
    }

    // pattern is a char
    if (pattern_len == 1) {
        return memchr((void*)mem_ptr, *patt_ptr, mem_len);
    }

    // Last possible position
    const char* cur = mem_ptr;
    const char* last = mem_ptr + mem_len - pattern_len;

    while(cur <= last) {
        // Test the first char before calling a function
        if (*cur == *patt_ptr && DSTR_MEMCMP(cur, pattern_ptr, pattern_len) == 0) {
            return (void*)cur;
        }
        ++cur;
    }

    return 0;
} // dstr__memory_find


// If the dstr has been built originally with a local buffer
DSTR_INTERNAL dstr_bool_t dstr__owns_local_buffer(dstr* s)
{
	return s->local_buffer_size != 0;
} // dstr__owns_local_buffer

DSTR_INTERNAL dstr_char_t* dstr__get_local_buffer(dstr* s)
{
	return (char*)s + sizeof(dstr);
} // dstr__get_local_buffer

DSTR_INTERNAL dstr_bool_t dstr__is_using_local_buffer(dstr* s)
{
	return dstr__owns_local_buffer(s) && s->data == dstr__get_local_buffer(s);
} // dstr__is_using_local_buffer

//-------------------------------------------------------------------------
// dstr - Private Implementation - END
//-------------------------------------------------------------------------

#endif // DSTR_IMPLEMENTATION

#endif // RE_DSTR_H
