/*
	strv - v0.0 - See end of file for license information.
*/

/*

String view C library.

Do this
	#define STRV_IMPLEMENTATION
before you include this file in *one* C or C++ file to create the implementation.

Assert can be redefined with:
	#define STRV_ASSERT(x) my_assert(x)

Example of use:

	int main() {

		strv sv = strv_from_str("    HELLO    ");
		strv trimmed = strv_trimmed(sv);

		return strv_equals_str(trimmed, "HELLO");
	}
*/

#ifndef RE_STRV_H
#define RE_STRV_H

#include <stdbool.h>
#include <stddef.h>

#ifndef STRV_API
#define STRV_API
#endif

#ifndef STRV_INTERNAL /* use for internal functions */
#define STRV_INTERNAL static
#endif

#ifndef STRV_ASSERT
#include <assert.h>
#define STRV_ASSERT(x) assert(x)
#endif

/* Literal contructor */
#define STRV(x) \
	{ sizeof(x)-1 , (const char*)(x)  }

#ifdef __cplusplus
extern "C" {
#endif

static const size_t STRV_NPOS = (size_t)-1;

#ifndef STRV_FMT
#define STRV_FMT "%.*s"
#endif

#ifndef STRV_ARG
#define STRV_ARG(strv) (int) (strv).size, (strv).data
#endif

typedef struct strv strv;
struct strv {
	size_t size;
	const char* data;
};

/* Create zero-initialized strv */
STRV_API strv strv_make(void);

/* Create a strv taking a pointer of data and a size. */
STRV_API strv strv_make_from(const char* data, size_t size);

/* Wrapper around strv_make_from taking a C string. */
STRV_API strv strv_make_from_str(const char* str);

/* Reset string view to zero. */
STRV_API void strv_clear(strv* sv);

/* Set new value for the string view. */
STRV_API void strv_assign(strv* sv, strv other);

/* Wrapper around strv_assign taking a C string. */
STRV_API void strv_assign_str(strv* sv, const char* str);

/* Return true if string is empty */
STRV_API bool strv_empty(strv sv);

/* Lexicagraphical comparison */
STRV_API int  strv_lexicagraphical_compare(strv left, strv right);

/* Wrapper around strv_lexicagraphical_compare. */
STRV_API int  strv_compare(strv sv, strv other);

/* Wrapper around strv_compare taking a C string. */
STRV_API int  strv_compare_str(strv sv, const char* str);

/* Equivalent of strv_compare == 0 */
STRV_API bool strv_equals(strv sv, strv other);

/* Wrapper around strv_equals taking a C string. */
STRV_API bool strv_equals_str(strv sv, const char* str);

/* Equivalent of strv_compare < 0 */
STRV_API bool strv_less_than(strv sv, strv other);

/* Wrapper around strv_less_than taking a C string. */
STRV_API bool strv_less_than_str(strv sv, const char* str);

/* Equivalent of strv_compare > 0 */
STRV_API bool strv_greater_than(strv sv, strv other);

/* Wrapper around strv_greater_than taking a C string. */
STRV_API bool strv_greater_than_str(strv sv, const char* str);

/* Get pointer to the first character */
STRV_API char* strv_begin(strv sv);

/* Get pointer past the last character. */
STRV_API char* strv_end(strv sv);

/* Get first char - cpp-style */
STRV_API char strv_front(strv view);

/* Get last char - cpp-style */
STRV_API char strv_back(strv view);

/* Find first string in string. */
STRV_API size_t strv_find(strv sv, strv sub);

/* Wrapper around strv_find taking a C string. */
STRV_API size_t strv_find_str(strv sv, const char* sub);

/* Wrapper around strv_find taking a char. */
STRV_API size_t strv_find_char(strv sv, char ch);

/* Find first character among multple ones in the string view. */
STRV_API size_t strv_find_first_of_chars(strv str, strv chars);

/* Find last character in the string view. */
STRV_API size_t strv_find_last_of_char(strv sv, char c);

/* Find last character among multple ones in the string view. */
STRV_API size_t strv_find_last_of_chars(strv str, strv chars);

/* Returns true if the specified character is contained in the string view. */
STRV_API bool strv_contains_char(strv sv, char c);

/* Returns true if the specified characters are contained in the string view. */
STRV_API bool strv_contains_chars(strv sv, strv chars);

/* Returns empty string if pos == s->size */
/* Returns empty string if pos > s->size. */
STRV_API strv strv_substr(strv sv, const char* pos, size_t count);

/* Get substring from index  + count. */
STRV_API strv strv_substr_from(strv sv, size_t index, size_t count);

/* Swap two strv. */
STRV_API void strv_swap(strv* s, strv* other);

/* Start with a specific value. */
STRV_API bool strv_starts_with(strv sv, strv other);

/* Wrapper around strv_starts_with taking a C string. */
STRV_API bool strv_starts_with_str(strv sv, const char* str);

/* Ends with a specific value. */
STRV_API bool strv_ends_with(strv sv, strv other);

/* Wrapper around strv_ends_with taking a C string. */
STRV_API bool strv_ends_with_str(strv sv, const char* str);

/* Remove N characters from the left. */
STRV_API strv strv_remove_left(strv sv, size_t count);

/* Remove N characters from the right. */
STRV_API strv strv_remove_right(strv sv, size_t count);

/* Trim any characters from data/size buffer, from the left. */
STRV_API strv strv_trimmed_left_by(strv sv, strv chars);

/* Trim any characters from data/size buffer, from the right. */
STRV_API strv strv_trimmed_right_by(strv sv, strv chars);

/* Trim any characters from data/size, from left and right. */
STRV_API strv strv_trimmed_by(strv sv, strv chars);

/* Wrapper around strv_trimmed_by. taking a C string. */
STRV_API strv strv_trimmed_by_str(strv sv, const char* str);

/* Trim whitespaces from left and right. */
STRV_API strv strv_trimmed(strv sv);

/* Make public because other library might use it. */
STRV_API void* strv_memory_find(strv mem, strv pattern);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* RE_STRV_H */

#ifdef STRV_IMPLEMENTATION

STRV_API strv
strv_make(void)
{
	strv sv;
	sv.data = 0;
	sv.size = 0;
	return sv;
}

STRV_API strv
strv_make_from(const char* data, size_t size)
{
	strv sv;
	sv.data = data;
	sv.size = size;
	return sv;
}

STRV_API strv
strv_make_from_str(const char* str)
{
	return strv_make_from(str, strlen(str));
}

STRV_API void
strv_clear(strv* sv)
{
	sv->size = 0;
	sv->data = NULL;
}

STRV_API void
strv_assign(strv* sv, strv other)
{
	sv->size = other.size;
	sv->data = other.data;
}

STRV_API void
strv_assign_str(strv* sv, const char* str)
{
	strv_assign(sv, strv_make_from_str(str));
}

STRV_API bool
strv_empty(strv sv)
{
	return sv.size == 0;
}

STRV_API int
strv_lexicagraphical_compare(strv left, strv right)
{
	char left_char, right_char;
	size_t min_size = left.size < right.size ? left.size : right.size;
	while (min_size-- > 0)
	{
		left_char = *left.data++;
		right_char = *right.data++;
		if (left_char != right_char)
			return left_char < right_char ? 1 : -1;
	};

	return left.size - right.size;
}

STRV_API int
strv_compare(strv sv, strv other)
{
	return strv_lexicagraphical_compare(sv, other);
}

STRV_API int
strv_compare_str(strv sv, const char* str)
{
	return strv_compare(sv, strv_make_from_str(str));
}

STRV_API bool
strv_equals(strv sv, strv other)
{
	return sv.size == other.size
		&& strv_compare(sv, other) == 0;
}

STRV_API bool
strv_equals_str(strv sv, const char* str)
{
	return strv_equals(sv, strv_make_from_str(str));
}

STRV_API bool
strv_less_than(strv sv, strv other)
{
	return strv_compare(sv, other) < 0;
}

STRV_API bool
strv_less_than_str(strv sv, const char* str)
{
	return strv_less_than(sv, strv_make_from_str(str));
}

STRV_API bool
strv_greater_than(strv sv, strv other)
{
	return strv_compare(sv, other) > 0;
}

STRV_API bool
strv_greater_than_str(strv sv, const char* str)
{
	return strv_greater_than(sv, strv_make_from_str(str));
}

STRV_API char*
strv_begin(strv sv)
{
	return (char*)(sv.data);
}

STRV_API char*
strv_end(strv sv)
{
	return (char*)(sv.data + sv.size);
}

STRV_API char
strv_front(strv sv)
{
	STRV_ASSERT(sv.size > 0);
	return sv.data[0];
}

STRV_API char
strv_back(strv sv)
{
	STRV_ASSERT(sv.size > 0);
	return sv.data[sv.size - 1];
}

STRV_API size_t
strv_find(strv sv, strv sub)
{
	size_t result = STRV_NPOS;

	if (sv.size >= sub.size) {

		void* found = strv_memory_find(sv, sub);

		if (found) {
			result = (char*)found - sv.data;
		}
	}

	return result;
}

STRV_API size_t
strv_find_str(strv sv, const char* sub)
{
	return strv_find(sv, strv_make_from_str(sub));
}

STRV_API size_t
strv_find_char(strv sv, char ch)
{
	return strv_find(sv, strv_make_from(&ch, 1));
}

STRV_API size_t
strv_find_first_of_chars(strv str, strv chars)
{
	size_t index = STRV_NPOS;

	if (str.size <= 0)
	{
		return index;
	}

	const char* begin = str.data;
	const char* data = str.data;
	const char* end = str.data + str.size;

	while (data < end)
	{
		if (strv_contains_char(chars, *data))
		{
			index = data - begin;
			break;
		}
		data++;
	}

	return index;
}

STRV_API size_t
strv_find_last_of_char(strv sv, char c)
{
	size_t index = STRV_NPOS;

	if (!sv.size)
	{
		return index;
	}

	const char* begin = strv_begin(sv);
	const char* cursor = sv.data + sv.size;

	size_t i = cursor - begin;
	for (; cursor >= begin; --cursor, --i) {
		if (*cursor == c) {
			index = i;
			break;
		}
	}

	return index;
}

STRV_API size_t
strv_find_last_of_chars(strv sv, strv chars)
{
	size_t index = STRV_NPOS;

	if (!sv.size)
	{
		return index;
	}

	const char* begin = strv_begin(sv);
	const char* cursor = sv.data + sv.size;

	size_t i = cursor - begin;
	for (; cursor >= begin; cursor -= 1, i -= 1) {
		if (strv_contains_char(chars, *cursor)) {
			index = i;
			break;
		}
	}

	return index;
}

STRV_API bool
strv_contains_char(strv sv, char ch)
{
	return strv_find_char(sv, ch) != STRV_NPOS;
}

STRV_API bool
strv_contains_chars(strv sv, strv chars)
{
	const char* end = chars.data + chars.size;
	const char* cursor = chars.data;
	while (cursor < end)
	{
		if (strv_contains_char(sv, *cursor))
		{
			return true;
		}

		++cursor;
	}

	return false;
}

STRV_API strv
strv_substr(strv sv, const char* pos, size_t count)
{
	const char* last = pos + count;
	STRV_ASSERT(pos >= strv_begin(sv));
	STRV_ASSERT(pos <= strv_end(sv));
	STRV_ASSERT(last >= pos);
	STRV_ASSERT(last <= strv_end(sv));

	strv result;

	result.data = pos;
	result.size = count;

	return result;
}

STRV_API strv
strv_substr_from(strv sv, size_t index, size_t count)
{
	const char* it = (sv.data + index);
	return strv_substr(sv, it, count);
}

STRV_API void
strv_swap(strv* s, strv* other)
{
	const strv tmp = *s;
	*s = *other;
	*other = tmp;
}

STRV_API bool
strv_starts_with(strv sv, strv other)
{
	if (sv.size < other.size)
	{
		return false;
	}

	strv sub = strv_make_from(sv.data, other.size);
	return strv_equals(sub, other);
}

STRV_API bool
strv_starts_with_str(strv sv, const char* str)
{
	return strv_starts_with(sv, strv_make_from_str(str));
}

STRV_API bool
strv_ends_with(strv sv, strv other)
{
	if (sv.size < other.size)
	{
		return false;
	}

	strv sub = strv_make_from(sv.data + (sv.size - other.size), other.size);
	return strv_equals(sub, other);
}

STRV_API bool
strv_ends_with_str(strv sv, const char* str)
{
	return strv_ends_with(sv, strv_make_from_str(str));
}

STRV_API strv
strv_remove_left(strv sv, size_t count)
{
	strv result = sv;
	size_t i = 0;
	while (i < sv.size && i < count)
	{
		result.size -= 1;
		result.data += 1;
		i += 1;
	}
	return result;
}

STRV_API strv
strv_remove_right(strv sv, size_t count)
{
	strv result = sv;
	size_t i = count;
	while (result.size > 0 && i > 0)
	{
		result.size -= 1;
		i -= 1;
	}
	return result;
}

STRV_API strv
strv_trimmed_left_by(strv sv, strv chars)
{
	size_t i = 0;

	while (i < sv.size
		&& strv_contains_char(chars, sv.data[i]))
	{
		i += 1;
	}

	return strv_make_from(sv.data + i, sv.size - i);
}

STRV_API strv
strv_trimmed_right_by(strv sv, strv chars)
{
	size_t i = 0;

	while (sv.size > 0
		&& strv_contains_char(chars, sv.data[sv.size - 1]))
	{
		sv.size -= 1;
	}

	return strv_make_from(sv.data, sv.size - i);
}

STRV_API strv
strv_trimmed_by(strv sv, strv chars)
{
	strv left = strv_trimmed_left_by(sv, chars);
	return strv_trimmed_right_by(left, chars);
}

STRV_API strv
strv_trimmed_by_str(strv sv, const char* str)
{
	return strv_trimmed_by(sv, strv_make_from_str(str));
}

STRV_API strv
strv_trimmed(strv sv)
{
	static strv chars = { 6, "\n\r\t \f\v" };
	return strv_trimmed_by(sv, chars);
}

STRV_API void*
strv_memory_find(strv mem, strv pattern)
{
	/* pattern.size can't be greater than mem.size */
	if ((mem.size == 0) || (pattern.size == 0) || pattern.size > mem.size) {
		return NULL;
	}

	/* Pattern is a single char */
	if (pattern.size == 1) {
		return memchr((void*)mem.data, *pattern.data, mem.size);
	}

	/* Last possible position */
	const char* cur = mem.data;
	const char* last = mem.data + mem.size - pattern.size;

	while (cur <= last)
	{
		/* Test the first char before calling a function */
		if (*cur == *pattern.data && memcmp(cur, pattern.data, pattern.size) == 0)
		{
			return (void*)cur;
		}
		++cur;
	}

	return NULL;
}

#endif /* STRV_IMPLEMENTATION */

/*
------------------------------------------------------------------------------
This software is available under 2 licenses -- choose whichever you prefer.
------------------------------------------------------------------------------
ALTERNATIVE 1 - The MIT License (MIT)

Copyright (c) 2024 kevreco

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
------------------------------------------------------------------------------
ALTERNATIVE 2 - Public Domain (www.unlicense.org)

This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <https://unlicense.org>
------------------------------------------------------------------------------
*/