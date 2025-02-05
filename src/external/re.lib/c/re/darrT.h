/*
    darrT.h - See end of file for license information.
*/

/*

SUMMARY:

    A more type-safe equivalent of the darr.h API, darr.h is required.
*/

#ifndef RE_DARRT_H
#define RE_DARRT_H

#include "darr.h"

#ifdef __cplusplus
extern "C" {
#endif

#define darrT(type)                   \
    union {                           \
        darr base;                    \
        struct {                      \
            darr_size_t size;         \
            darr_size_t capacity;     \
            type* data;               \
            darr_size_t sizeof_value; \
        } arr;                        \
    }

#define darrT_init(a) \
    darr_init(&(a)->base, sizeof(*(a)->arr.data))

#define darrT_destroy(a) \
    darr_destroy(&(a)->base)

#define darrT_clear(a) \
    darr_clear(&(a)->base)

#define darrT_insert(a, index, value) \
    do {  \
        darr_insert_one_space(&(a)->base, (index)); \
		(a)->arr.data[(index)] = value; \
	} while (0);

#define darrT_first(a) \
    ((a)->arr.data[0])

#define darrT_last(a) \
    ((a)->arr.data[(a)->arr.size - 1])

#define darrT_pop_back(a) \
    (darrT_last(a), (a)->arr.size -= 1)

#define darrT_pop_front(a) \
     (darrT_first(a), darr_pop_front((&(a)->base))

#define darrT_push_back(a, value) \
    do {  \
        int last__ = (a)->arr.size; \
        darr_insert_one_space(&(a)->base, last__); \
		(a)->arr.data[last__] = value; \
	} while (0);

#define darrT_at(a, index) \
    ((a)->arr.data[index])

#define darrT_set(a, index, value) \
    ((a)->arr.data[index] = (value))

#define darrT_ptr(a, index) \
    (&(a)->arr.data[index])

#define darrT_size(a) \
    darr_size(&((a)->base))

#define darrT_resize(a, size) \
    darr_resize(&((a)->base), size)

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* RE_DARRT_H */

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