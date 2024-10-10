#ifndef RE_HT_H
#define RE_HT_H

#include <stddef.h> /* size_t */

/* Hash Table */

#ifndef HT_API
#ifdef HT_STATIC
#define HT_API static
#else
#define HT_API extern
#endif
#endif

#ifndef HT_ASSERT
#include <assert.h>
#define HT_ASSERT assert
#endif

#ifndef HT_MALLOC
#include <stdlib.h>
#define HT_MALLOC malloc
#endif

#ifndef HT_FREE
#include <stdlib.h>
#define HT_FREE free
#endif

#ifndef HT_SIZE_T
#define HT_SIZE_T size_t
#endif

#ifdef __cplusplus
extern "C" {
#endif


typedef char            ht_byte_t;
typedef ht_byte_t*      ht_it;
typedef unsigned int    ht_bool;
typedef HT_SIZE_T       ht_size_t;
typedef ht_size_t       ht_hash_t;

typedef ht_bool (*ht_predicate_t)(void* left, void* right);
typedef void (*ht_swap_function_t)(void* left, void* right);
typedef ht_hash_t (*ht_hash_function_t)(void* item);

typedef struct ht ht;

struct ht {
    ht_byte_t* buckets;

    ht_size_t bucket_capacity; /* count of bucket in the array */
    
    ht_size_t sizeof_item;
    ht_size_t sizeof_bucket;   /* size of item + bucket header */

    ht_hash_function_t hash;
    ht_predicate_t items_are_same;
    ht_swap_function_t swap_items;

    ht_size_t filled_bucket_count;  /* number of filled entries */
    ht_size_t allocated_memory;     /* allocated memory for all the buckets and the temp entries below */

    void* tmp_entry;
    void* tmp_for_swap;
};

/* use to iterate over all items */
typedef struct ht_cursor ht_cursor;
struct ht_cursor {
    void* current_bucket; /* pointer of current bucket */
    const ht* h;
};

HT_API void ht_init(ht* h,
    ht_size_t sizeof_item, 
    ht_hash_function_t hash,
    ht_predicate_t items_are_same,
    ht_swap_function_t swap_items,
    ht_size_t initial_capacity);

HT_API void ht_destroy(ht* h);
HT_API void ht_reserve(ht* h, ht_size_t item_count);
HT_API void ht_clear(ht* h);
HT_API void ht_swap(ht* h, ht* other);

HT_API ht_bool ht_is_empty(const ht* h);
/* Number of filled entries. */
HT_API ht_size_t ht_size(const ht* h);
/* Count number of filled entries. */
HT_API ht_size_t ht_count(const ht* h);
HT_API void* ht_begin(const ht* h);
HT_API void* ht_end(const ht* h);

/* Returns true if item was inserted, false if item was replaced. */
HT_API ht_bool ht_insert(ht* h, void* item);
/* Same as ht_insert but explicitly provide the hash value. */
HT_API ht_bool ht_insert_h(ht* h, void* item, ht_hash_t hash);

/* Returns true if item was found according to items_are_same. */
HT_API ht_bool ht_contains(const ht* h, void* item);
/* Same as ht_inht_contains but explicitly provide the hash value. */
HT_API ht_bool ht_contains_h(const ht* h, void* item, ht_hash_t hash);

/* Returns item according to items_are_same implementation, returns null if item was not found. */
HT_API void* ht_get_item(const ht* h, void* item);
/* Same as ht_get_item but explicitly provide the hash value. */
HT_API void* ht_get_item_h(const ht* h, void* item, ht_hash_t hash);

/* Get a copy of the item if it has been found copy the data in result, returns true if item was found */
HT_API ht_bool ht_get(const ht* h, void* item, void* result);
/* Same as ht_get but explicitly provide the hash value. */
HT_API ht_bool ht_get_h(const ht* h, void* item, ht_hash_t hash, void* result);

/* Delete item, returns true if i8t was deleted */
HT_API ht_bool ht_erase(ht* h, void* item);
/* Same as ht_erase but explicitly provide the hash value. */
HT_API ht_bool ht_erase_h(ht* h, void* item, ht_hash_t hash);

HT_API void ht_cursor_init(ht* h, ht_cursor* cursor);
/* Go to next bucket and return point to it */
HT_API void* ht_cursor_next(ht_cursor* cursor);
HT_API void* ht_cursor_end(const ht_cursor* cursor);
HT_API void* ht_cursor_item(const ht_cursor* cursor);

HT_API ht_size_t ht_allocated_memory(const ht* h);

HT_API void ht_debug_print_info(ht* h);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* RE_HT_H */

#if defined(HT_IMPLEMENTATION)

#define ht_each_bucket(ht_ptr, bucket_ptr) \
    (bucket_ptr) = ht__bucket_begin(ht_ptr); (bucket_ptr) < ht__bucket_end(ht_ptr); (bucket_ptr) = ht__bucket_next(ht_ptr, bucket_ptr)

static const ht_size_t MIN_CAPACITY = 16;
static double MAX_LOAD = 0.75;

static const ht_hash_t RESERVED_HASH_FOR_EMPTY = (ht_hash_t)0;

/* For simplicity we just call the 'bucket header' 'bucket'. */
typedef struct bucket_t bucket_t;
struct bucket_t {
    ht_hash_t hash;
};

static struct
bucket_t* ht__bucket_begin(const ht* h)
{
    return (bucket_t*)h->buckets;
}

static struct
bucket_t* ht__bucket_end(const ht* h)
{
    return (bucket_t*)(h->buckets + (h->bucket_capacity * h->sizeof_bucket));
}

static struct
bucket_t* ht__bucket_next(const ht* h, bucket_t* bucket)
{
    return (bucket_t*) ((char*)bucket + h->sizeof_bucket);
}

static ht_hash_t
ht__do_hash(const ht* h, const void* item)
{
    ht_hash_t hash = h->hash((void*)item);

    /* Adjust hash if it's a reserved hash */
    if (hash == RESERVED_HASH_FOR_EMPTY)
    {
        hash = RESERVED_HASH_FOR_EMPTY == 0 ? hash + 1 : hash - 1;
    }
    return hash;
}

static inline bucket_t*
ht__bucket_at(const ht* h, ht_size_t index)
{
    return (bucket_t*)(h->buckets + (index * h->sizeof_bucket));
}

static inline ht_bool
ht__bucket_is_empty(const bucket_t* bucket)
{
    return bucket->hash == RESERVED_HASH_FOR_EMPTY;
}

static ht_bool
ht__bucket_is_empty_at(ht* h, ht_size_t index)
{
    bucket_t* bucket = ht__bucket_at(h, index);
    return ht__bucket_is_empty(bucket);
}

static void*
ht__get_bucket_item(bucket_t* bucket)
{
    return (char*)bucket + sizeof(bucket_t);
}

static void
ht__change_value(ht* h, bucket_t* bucket, ht_hash_t hash, void* item)
{
    bucket->hash = hash;
    memcpy((char*)bucket + sizeof(bucket_t), item, h->sizeof_item);
}

static inline ht_size_t
ht__bucket_index(const ht* h, ht_hash_t hash)
{
    /* Equivalent to hash% h->bucket_capacity but faster since we are using power of two as bucket capacity. */
    return hash & (h->bucket_capacity - 1);
}

static void
ht__bucket_copy(ht* h, bucket_t* dest, bucket_t* src)
{
    memcpy(dest, src, h->sizeof_bucket);
}

static inline
ht_size_t ht__bucket_distance(const ht* h, ht_size_t first, ht_size_t last)
{
    /* get distance and "wrap it" to fit inside the bucket indices. */
    return ht__bucket_index(h, first - last);
}

static void
ht__bucket_set_empty(bucket_t* bucket)
{
    bucket->hash = RESERVED_HASH_FOR_EMPTY;
}

static void
ht__bucket_swap(ht* h, bucket_t* a, bucket_t* b)
{
    memcpy(h->tmp_for_swap, a, h->sizeof_bucket);
    memcpy(a, b, h->sizeof_bucket);
    memcpy(b, h->tmp_for_swap, h->sizeof_bucket);
}

static ht_size_t
ht__get_next_non_empty_bucket_from(ht* h, ht_size_t index)
{
    if (!ht__bucket_is_empty_at(h, index)) return index;

    /* Until bucket is non empty, advance to the next bucket. */
    while (!ht__bucket_is_empty_at(h, index))
    {
        index += 1;
    }

    return index;
}

/* Return the bucket itself if it's non-empty. */
HT_API bucket_t*
ht__get_next_non_empty_bucket(const ht* h, bucket_t* bucket)
{
    if (!ht__bucket_is_empty(bucket)) return bucket;

    /* until bucket is non empty, advance to the next bucket. */
    while (ht__bucket_is_empty(bucket))
    {
        bucket = (bucket_t*)((char*)bucket + h->sizeof_bucket);
    }

    return bucket;
}

static void
ht__resize_up(ht* h, ht_size_t new_item_capacity)
{
    HT_ASSERT(ht_size(h) < new_item_capacity);

    ht old_ht;
    ht_init(&old_ht, h->sizeof_item, h->hash, h->items_are_same, h->swap_items, new_item_capacity);

    ht_swap(h, &old_ht);

    h->filled_bucket_count = 0;

    bucket_t* bucket;
    for (ht_each_bucket(&old_ht, bucket))
    {
        if (!ht__bucket_is_empty(bucket))
        {
            void* item = ht__get_bucket_item(bucket);
            ht_insert(h, item);
        }
    }

    ht_destroy(&old_ht);
}

HT_API void
ht_init(ht* h,
    ht_size_t sizeof_item,
    ht_hash_function_t hash,
    ht_predicate_t items_are_same,
    ht_swap_function_t swap_items,
    ht_size_t initial_capacity)
{
    HT_ASSERT(h);
    HT_ASSERT(sizeof_item);
    HT_ASSERT(items_are_same);

    memset(h, 0, sizeof(ht));

    h->sizeof_item = sizeof_item;

    ht_size_t bucket_entry_size = sizeof(bucket_t) + sizeof_item;
    // @TODO document this, this is for alignment
    while (bucket_entry_size & (sizeof(intptr_t) - 1))
    {
        ++bucket_entry_size;
    }

    h->sizeof_bucket = bucket_entry_size;

    if (initial_capacity < 0)
        initial_capacity = 0;

    if (initial_capacity)
    {
        ht_size_t bucket_capacity = initial_capacity * bucket_entry_size;
        /* mem size for all the buckets and the temporary objects */
        h->allocated_memory = bucket_capacity + bucket_entry_size + bucket_entry_size;
        char* mem = (char*)HT_MALLOC(h->allocated_memory);
        h->buckets = mem;
        h->tmp_entry = mem + bucket_capacity;
        h->tmp_for_swap = mem + bucket_capacity + bucket_entry_size;

        h->bucket_capacity = initial_capacity;
    }

    /* Mark all bucket as empty. */
    /* @TODO create an iterator */
    for (ht_size_t i = 0; i < initial_capacity; ++i)
    {
        bucket_t* bucket = ht__bucket_at(h, i);
        ht__bucket_set_empty(bucket);
    }

    h->hash = hash;
    h->items_are_same = items_are_same;
    h->swap_items = swap_items;
}

HT_API void
ht_destroy(ht* h)
{
    if (h->buckets)
        HT_FREE(h->buckets);

    memset(h, 0, sizeof(ht));
}

HT_API void
ht_reserve(ht* h, ht_size_t item_count)
{
    ht__resize_up(h, item_count);
}

HT_API void
ht_clear(ht* h)
{
    if (RESERVED_HASH_FOR_EMPTY == 0)
    {
        memset(h->buckets, 0, h->bucket_capacity * h->sizeof_bucket);
    }
    else
    {
        /* Mark all buckets with the empty flag */
        bucket_t* bucket;
        for (ht_each_bucket(h, bucket))
        {
            ht__bucket_set_empty(bucket);
        }
    }

    h->filled_bucket_count = 0;
}

HT_API void
ht_swap(ht* h, ht* other)
{
    ht tmp = *h;
    *h = *other;
    *other = tmp;
}

HT_API ht_bool
ht_is_empty(const ht* h)
{
    return h->filled_bucket_count == 0;
}

HT_API ht_size_t
ht_size(const ht* h)
{
    return h->filled_bucket_count;
}

HT_API ht_size_t
ht_count(const ht* h)
{
    bucket_t* bucket;
    ht_size_t count = 0;
    for (ht_each_bucket(h, bucket))
    {
        if (!ht__bucket_is_empty(bucket))
            ++count;
    }

    return count;
}

HT_API void*
ht_begin(const ht* h)
{
    return ht__get_next_non_empty_bucket(h, ht__bucket_begin(h));
}

HT_API void*
ht_end(const ht* h)
{
    return ht__bucket_end(h);
}

static ht_bool
ht__try_find_index(const ht* h, const void* item, ht_hash_t hash, ht_size_t* index)
{
    if (ht_is_empty(h))
        return 0;

    ht_size_t target_bucket_index = ht__bucket_index(h, hash);
    ht_size_t current_bucket_index = target_bucket_index;

    for (;;)
    {
        bucket_t* current_bucket = ht__bucket_at(h, current_bucket_index);

        /* If there is no value, we end here. */
        if (ht__bucket_is_empty(current_bucket)) return 0;

        if (current_bucket->hash == hash
            && h->items_are_same(ht__get_bucket_item(current_bucket), (void*)item))
        {
            *index = current_bucket_index;
            return 1;
        }

        /* Due to the implementation, using "current_bucket->hash" is equivalent to use 'ht__bucket_index(current_bucket->hash).'
           It's just prevent one unnecessary operation. */
        ht_size_t current_distance = ht__bucket_distance(h, current_bucket_index, current_bucket->hash);
        ht_size_t target_distance = ht__bucket_distance(h, current_bucket_index, target_bucket_index);

        if (current_distance < target_distance)
            return 0;

        current_bucket_index = ht__bucket_index(h, current_bucket_index + 1);
    }
}

HT_API ht_bool
ht_contains(const ht* h, void* item)
{
    ht_hash_t hash = ht__do_hash(h, item);
    return ht_contains_h(h, item, hash);
}

HT_API ht_bool
ht_contains_h(const ht* h, void* item, ht_hash_t hash)
{
    ht_size_t index;
    return ht__try_find_index(h, item, hash , &index);
}

HT_API void*
ht_get_item(const ht* h, void* item)
{
    ht_hash_t hash = ht__do_hash(h, item);
    return ht_get_item_h(h, item, hash);
}

HT_API void*
ht_get_item_h(const ht* h, void* item, ht_hash_t hash)
{
    ht_size_t index = 0;

    if (!ht__try_find_index(h, item, hash , &index))
    {
        return 0;
    }

    /* Bucket exists, get its value. */
    return ht__get_bucket_item(ht__bucket_at(h, index));
}

HT_API ht_bool
ht_get(const ht* h, void* item, void* result_item)
{
    ht_hash_t hash = ht__do_hash(h, item);
    return ht_get_h(h, item, hash, result_item);
}

HT_API ht_bool
ht_get_h(const ht* h, void* item, ht_hash_t hash, void* result_item)
{
    void* item_found = ht_get_item_h(h, item, hash);
    if (!item_found)
    {
        return 0;
    }

    memcpy(result_item, item_found, h->sizeof_item);

    return 1;
}

static ht_bool
ht__insert(ht* h, void* item, ht_hash_t hash, bucket_t** inserted_or_updated)
{
    if (h->bucket_capacity == 0
        || h->filled_bucket_count + 1 > (h->bucket_capacity * MAX_LOAD))
    {
        /* set initial capacity or double it (to fit power of two pattern) */
        ht_size_t next_capacity = h->bucket_capacity == 0 ? MIN_CAPACITY : h->bucket_capacity * 2;
        ht__resize_up(h, next_capacity);
    }

    bucket_t* entry = (bucket_t*)h->tmp_entry;
    ht__change_value(h, entry, hash, item);

    size_t entry_ideal_bucket_index = ht__bucket_index(h, entry->hash);
    size_t current_bucket_index = entry_ideal_bucket_index;
    bucket_t* inserted_bucket = 0;

    for (;;)
    {
        bucket_t* current_bucket = ht__bucket_at(h, current_bucket_index);

        if (ht__bucket_is_empty(current_bucket))
        {
            ht__bucket_copy(h, current_bucket, entry);
            ++h->filled_bucket_count;

            if (inserted_bucket == 0)
                inserted_bucket = current_bucket;

            *inserted_or_updated = inserted_bucket;
            return 1;
        }
        else {

            /* value already exist return iterator */
            if (current_bucket->hash == entry->hash
                && h->items_are_same(ht__get_bucket_item(current_bucket), ht__get_bucket_item(entry)))
            {
                ht__bucket_copy(h, current_bucket, entry);
                *inserted_or_updated = current_bucket;
                return 0;
            }

            /* Due to the implementation, using "current_bucket->hash" is equivalent to use 'ht__bucket_index(current_bucket->hash)'
               It's just prevent one unnecessary operation.
               index - hash will result in a value (index) that will get "wrapped" within the size of the array. */
            size_t current_distance = ht__bucket_distance(h, current_bucket_index, current_bucket->hash);
            size_t ideal_distance = ht__bucket_distance(h, current_bucket_index, entry_ideal_bucket_index);
            if (current_distance < ideal_distance)
            {
                ht__bucket_swap(h, current_bucket, entry);

                if (inserted_bucket == 0)
                    inserted_bucket = current_bucket;

                /* At this point we have to update the ideal bucket index, we used the hash earlier to calculate 'current_distance' as an optimization.
                   Note that "entry" is used because it got swapped with current_bucket earlier. */
                entry_ideal_bucket_index = ht__bucket_index(h, entry->hash);
            }
            /* get next bucket */
            current_bucket_index = ht__bucket_index(h, current_bucket_index + 1);
        }
    }
}

HT_API ht_bool
ht_insert(ht* h, void* item)
{
    ht_hash_t hash = ht__do_hash(h, item);
    return ht_insert_h(h, item, hash);
}

HT_API ht_bool
ht_insert_h(ht* h, void* item, ht_hash_t hash)
{
    bucket_t* inserted_or_updated = 0;
    return ht__insert(h, item, hash , &inserted_or_updated);
}

HT_API ht_size_t
ht_erase_at(ht* h, ht_size_t index)
{
    HT_ASSERT(index >= 0);

    ht_size_t current_bucket_index = index;
    bucket_t* current_bucket = ht__bucket_at(h, current_bucket_index);

    for (;;) {
        size_t next_bucket_index = ht__bucket_index(h, current_bucket_index + 1);
        bucket_t* next_bucket = ht__bucket_at(h, next_bucket_index);

        if (ht__bucket_is_empty(next_bucket)
            || ht__bucket_distance(h, next_bucket_index, next_bucket->hash) <= 0)
        {
            break;
        }

        ht__bucket_copy(h, current_bucket, next_bucket);

        current_bucket_index = next_bucket_index;
        current_bucket = next_bucket;
    }

    ht__bucket_set_empty(current_bucket);

    --h->filled_bucket_count;

    return ht__get_next_non_empty_bucket_from(h, index);
}

HT_API ht_bool
ht_erase(ht* h, void* item)
{
    ht_hash_t hash = ht__do_hash(h, item);
    return ht_erase_h(h, item, hash);
}

HT_API ht_bool
ht_erase_h(ht* h, void* item, ht_hash_t hash)
{
    ht_size_t index;
    if (ht__try_find_index(h, item, hash , &index))
    {
        ht_erase_at(h, index);
        return 1;
    }
    return 0;
}

HT_API void
ht_cursor_init(ht* h, ht_cursor* cursor)
{
    ht_cursor c;

    c.current_bucket = h->buckets - h->sizeof_bucket;
    c.h = h;

    *cursor = c;
}

HT_API void*
ht_cursor_end(const ht_cursor* cursor)
{
    return ht_end(cursor->h);
}

HT_API void*
ht_cursor_next(ht_cursor* cursor)
{
    if (cursor->current_bucket >= ht_end(cursor->h))
    {
        return 0;
    }

    /* current bucket */
    bucket_t* bucket = (bucket_t*)cursor->current_bucket;
    /* go to next bucket, which can be empty */
    bucket = (bucket_t*)((char*)bucket + cursor->h->sizeof_bucket);
    /* if empty bucket go to next non-empty */
    bucket = ht__get_next_non_empty_bucket(cursor->h, bucket);

    cursor->current_bucket = bucket;

    ht_bool is_valid_bucket = (void*)bucket < ht_end(cursor->h);

    return is_valid_bucket ? bucket : 0;
}

HT_API void*
ht_cursor_item(const ht_cursor* cursor)
{
    return ht__get_bucket_item((bucket_t*)cursor->current_bucket);
}

HT_API ht_size_t
ht_allocated_memory(const ht* h)
{
    return h->allocated_memory;
}

HT_API void
ht_debug_print_info(ht* h)
{
    ht_cursor cursor;
    ht_cursor_init(h, &cursor);
    while (ht_cursor_next(&cursor))
    {
        /* debug assumes int as key and value */
        int* val_ptr = (int*)ht_cursor_item(&cursor);
        printf("val %d \n", *val_ptr);
    }
}

#endif /* defined(HT_IMPLEMENTATION) */