/*

SUMMARY:

    Map using sorted arrays.
    This library requires darr.h
    
    See end of file for license information.
    
EXAMPLE:

    #include "darr.h"
    #include "darr_map.h"
    
    struct item {
        int key;
        int value;
    };
    
    static int item_less(const struct item* left, const struct item* right)
    {
        return (left->key < right->key);
    }
    
    int main() {

        darr_map map;
        darr_map_init(&map, sizeof(item), (darr_predicate_t)map_item_less);
        
        struct item item = { 0, 1 };
        
        // Add item.
        darr_map_set(&map, &item);
        
        // Check item.
        darr_map_contains(&map, &item);
        
        // Remove item.
        darr_map_remove(&map, &item);
        
        darr_map_destroy(&map);
    }
    
    #define DARR_IMPLEMENTATION
    #include "darr.h"
    #define DARR_MAP_IMPLEMENTATION
    #include "darr_map.h"
*/

#ifndef RE_DARR_MAP_H
#define RE_DARR_MAP_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct darr_map darr_map;
struct darr_map {
    darr arr;
    darr_size_t sizeof_item;
    darr_predicate_t less;
};

/*-------------------------------------------------------------------------*/
/* darr_map - API */
/*-------------------------------------------------------------------------*/

DARR_API void darr_map_init(darr_map* m, darr_size_t sizeof_item, darr_predicate_t less);
DARR_API void darr_map_destroy(darr_map* m);

DARR_API darr_bool darr_map_get(darr_map* m, const void* item, void* result);

/* set value, returns true if the value didn't exist yet, false if the value was replaced */
DARR_API darr_bool darr_map_set(darr_map* m, void* item);
/* remove value at key, return true of a value was indeed remove, false if there was nothing to remove. */
DARR_API darr_bool darr_map_remove(darr_map* m, const void* item);
DARR_API darr_bool darr_map_contains(const darr_map* m, const void* item);
DARR_API darr_size_t darr_map_size(const darr_map* m);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif  /* RE_DARR_MAP_H */

#ifdef RE_DARR_MAP_IMPLEMENTATION

DARR_API void
darr_map_init(darr_map* m, darr_size_t sizeof_item, darr_predicate_t less)
{
	DARR_ASSERT(m);
	DARR_ASSERT(less);

	darr_init(&m->arr, sizeof_item);
	m->sizeof_item = sizeof_item;
	m->less = less;
}

DARR_API void
darr_map_destroy(darr_map* m)
{
	darr_destroy(&m->arr);
	m->sizeof_item = 0;
	m->less = 0;
}

DARR_API darr_bool
darr_map_get(darr_map* m, const void* item, void* result)
{
	darr_size_t index = darr_binary_find_predicate(&m->arr, item, m->less);
	if (index != m->arr.size)
	{
		DARR_MEMCPY(result, darr_ptr(&m->arr, index), m->sizeof_item);
		return (darr_bool)1;
	}

	return (darr_bool)0;
}

DARR_API darr_bool
darr_map_set(darr_map* m, void* item)
{
	darr_size_t index = darr_binary_find_predicate(&m->arr, item, m->less);
	if (index != m->arr.size)
	{
		DARR_MEMCPY(darr_ptr(&m->arr, index), item, m->sizeof_item);
		return (darr_bool)0;
	}
	else
	{
		darr_insert_one_sorted(&m->arr, item, m->less);
		return (darr_bool)1;
	}
}

DARR_API darr_bool
darr_map_remove(darr_map* m, const void* item)
{
	darr_size_t index = darr_binary_find_predicate(&m->arr, item, m->less);
	if (index != m->arr.size)
	{
		darr_erase_one(&m->arr, index);
		return (darr_bool)1;
	}

	return (darr_bool)0;
}

DARR_API darr_bool
darr_map_contains(const darr_map* m, const void* item)
{
	darr_size_t index = darr_binary_find_predicate(&m->arr, item, m->less);
	return index != m->arr.size;
}

DARR_API darr_size_t
darr_map_size(const darr_map* m)
{
	return darr_size(&m->arr);
}

#endif /* RE_DARR_MAP_IMPLEMENTATION */

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
