#ifndef RE_CB_H
#define RE_CB_H

#define RE_CB_IMPLEMENTATION

#if _WIN32
#if !defined _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#include <windows.h>
#else
	#include <unistd.h>   /* open, close, access */
    #include <sys/stat.h> /* mkdir */
	#include <fcntl.h>    /* O_RDONLY etc. */
	#include <errno.h>
	#include <sys/sendfile.h> /* sendfile */
	#include <sys/wait.h>     /* waitpid */
	#include <dirent.h>       /* opendir */
#endif

#include <stdarg.h> /* va_start, va_end */

#ifndef RE_CB_API
#define RE_CB_API
#endif
#ifndef CB_INTERNAL
#define CB_INTERNAL static
#endif

#ifndef CB_ASSERT
#include <assert.h> /* assert */
#define CB_ASSERT assert
#endif

#ifndef CB_MALLOC
#include <stdlib.h> /* malloc */
#define CB_MALLOC malloc
#endif

#ifndef CB_FREE
#include <stdlib.h> /* free */
#define CB_FREE free
#endif

#define cb_true ((cb_bool)1)
#define cb_false ((cb_bool)0)

#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned int cb_id; /* hashed key */
typedef unsigned int cb_bool;

RE_CB_API void cb_init();
/* Set or create current project. Returns true if the project was just created. */
RE_CB_API cb_bool cb_project(const char* name); 
/* Add value for the specific key. */
RE_CB_API void cb_add(const char* key, const char* value);
/* Remove all previous values according to the key and set the new one. */
RE_CB_API void cb_set(const char* key, const char* value);
/* Remove all values associated with the key. Returns number of removed values */
RE_CB_API int cb_remove_all(const char* key);
/* Remove item with the exact key and value. */
RE_CB_API cb_bool cb_remove_one(const char* key, const char* value);
     
/* @FIXME: This should be equivalent to use cb_add("file", XXX); in a loop. */
RE_CB_API void cb_add_files(const char* pattern);

struct cb_toolchain_t;
typedef cb_bool (*cb_toolchain_bake_t)(struct cb_toolchain_t* tc, const char*);

typedef struct cb_toolchain_t {
	cb_toolchain_bake_t bake;
	const char* name;
	const char* default_directory_base;
} cb_toolchain_t;

RE_CB_API cb_toolchain_t cb_toolchain_msvc();

RE_CB_API void cb_bake(cb_toolchain_t toolchain, const char* project_name);
RE_CB_API cb_bool cb_bake_and_run(cb_toolchain_t toolchain, const char* project_name);

/** wildcard matching, supporting * ** ? [] */
static cb_bool cb_wildmatch(const char* pattern, const char* str); /* forward declaration */
RE_CB_API void cb_wildmatch_test();

RE_CB_API cb_bool cb_subprocess(const char* cmd);
/* commonly used properties (basically to make it discoverable with auto completion and avoid misspelling) */

/* keys */
extern const char* cbk_BINARY_TYPE;
extern const char* cbk_INCLUDE_DIR;
extern const char* cbk_LINK_PROJECT;
extern const char* cbk_OUTPUT_DIR;
extern const char* cbk_DEFINES;
/* values */
extern const char* cbk_exe;
extern const char* cbk_shared_lib;
extern const char* cbk_static_lib;

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* RE_CB_H */

#ifdef RE_CB_IMPLEMENTATION
#ifndef RE_CB_IMPLEMENTATION_CPP
#define RE_CB_IMPLEMENTATION_CPP
/* INTERNAL */

#ifdef _WIN32
#define CB_DEFAULT_DIR_SEPARATOR_CHAR '\\'
#define CB_DEFAULT_DIR_SEPARATOR "\\"
#else
#define CB_DEFAULT_DIR_SEPARATOR_CHAR '/'
#define CB_DEFAULT_DIR_SEPARATOR "/"
#endif
/* Table of content
*  ----------------------------------------------------------------
* 
* # Structures
*   - @TODO
* # functions of internal structures
*   - cb_log     - write some logs
*   - cb_darr    - dynamic array
*   - cb_dstr    - dynamic string
*   - cb_kv      - key value for map adn multimap
*   - cb_map     - hash map containg key/value strings
*   - cb_mmap    - multi map containg key/value strings
* # functions of the cb library
*   - cb_project(...) - @TODO explanation
*   - cb_set(...)     - @TODO explanation
*   - cb_add(...)     - @TODO explanation
*   - cb_bake(...)    - @TODO explanation
* # Drivers
*   - msvc driver
* # External libraries 
*   - Wildmatch library
*/

/* keys */
const char* cbk_BINARY_TYPE = "binary_type";
const char* cbk_DEFINES = "defines";
const char* cbk_INCLUDE_DIR = "include_dir";
const char* cbk_LINK_PROJECT = "link_project";
const char* cbk_OUTPUT_DIR = "output_dir";

/* values */
const char* cbk_exe = "exe";
const char* cbk_shared_lib = "shared_library";
const char* cbk_static_lib = "static_library";

#define CB_NULL NULL

/* string view */
typedef struct cb_strv_t {
	const char* data;
	int size;
} cb_strv;

/* @TODO remove this if not used */
/* same as string view but can be modified */
typedef struct cb_str_span_t {
	char* data;
	int size;
} cb_str_span;

/* dynamic array */
typedef struct cb_darr_t {
	int size;
    int capacity;
    char* data;
} cb_darr;

/* type safe dynamic array */
#define cb_darrT(type)    \
    union {               \
        cb_darr base;     \
        struct  {         \
            int size;     \
            int capacity; \
            type* data;   \
        } darr;           \
    }

#define cb_darr_push_back(a, value) \
    do { \
        cb_darr* array_ = a; \
        void* value_ref_ = &(value); \
        cb_darr_insert_one(array_, array_->size, value_ref_, sizeof(value)); \
    } while (0)

#define cb_darrT_init(a) \
    cb_darr_init(&(a)->base)

#define cb_darrT_destroy(a) \
    cb_darr_destroy(&(a)->base)

#define cb_darrT_insert(a, index, value) \
    do {  \
        cb_darr_insert_one_space(&(a)->base, (index), sizeof(*(a)->darr.data)); \
		(a)->darr.data[(index)] = value; \
	} while (0)


#define cb_darrT_remove(a, index) \
    do {  \
        cb_darr_remove_one(&(a)->base, (index), sizeof(*(a)->darr.data)); \
	} while (0)

#define cb_darrT_push_back(a, value) \
    do {  \
        int last__ = (a)->darr.size; \
        cb_darr_insert_one_space(&(a)->base, last__, sizeof(*(a)->darr.data)); \
		(a)->darr.data[last__] = value; \
	} while (0)

#define cb_darrT_at(a, index) \
    ((a)->darr.data[index])

#define cb_darrT_set(a, index, value) \
    (a)->darr.data[index] = (value)

#define cb_darrT_ptr(a, index) \
    (&(a)->darr.data[index])

/* dynamic string */
typedef cb_darr cb_dstr;

typedef char* cb_darr_it;

/* key/value data used in the map and mmap struct */
typedef struct cb_kv_t
{
	cb_id hash; /* hash of the key */
	cb_strv key; /* key */
	union { int _int; float _float; const void* ptr;  cb_strv strv; } u; /* value */
} cb_kv;

/* map */
typedef cb_darrT(cb_kv) cb_map;

/* multimap */
typedef cb_darrT(cb_kv) cb_mmap;

#define cb_rangeT(type) \
struct {                \
	type* begin;        \
	type* end;          \
	int count;          \
}

typedef cb_rangeT(cb_kv) cb_kv_range;

typedef struct cb_file_command_t {
	cb_bool glob;
	const char* pattern; /* could be a pattern (if glob is set to true) or a regular file path. */
} cb_file_command;

typedef struct cb_context_t cb_context; /* forward declaration */

typedef struct cb_project_t {
	cb_context* context;
	cb_id id;
	cb_strv name;
	cb_darrT(cb_file_command) file_commands;

	cb_mmap mmap; /* multi map of strings - when you want to have multiple values per key */
} cb_project_t;

/* context, the root which hold everything */
struct cb_context_t {
	cb_map projects;
	cb_project_t* current_project;
	cb_darr string_pool; /* to allocate user strings, allow easy allocation of concatenated strings with cb_str */
};

static cb_context default_ctx;
static cb_context* current_ctx;

/*-----------------------------------------------------------------------*/
/* cb_log */
/*-----------------------------------------------------------------------*/

void cb_log(FILE* file, const char* prefix, const char* fmt, ...)
{
	fprintf(file, prefix);

	va_list args;
	va_start(args, fmt);
	vfprintf(file, fmt, args);
	va_end(args);
	fprintf(file, "\n");
}

/* NOTE: Those following macros are compatible with c99 only */
#define cb_log_error(fmt, ...)     cb_log(stderr, "[CB-ERROR] ", fmt, ##__VA_ARGS__)
#define cb_log_warning(fmt, ...)   cb_log(stderr, "[CB-WARNING] ", fmt, ##__VA_ARGS__)
#define cb_log_debug(fmt, ...)     cb_log(stdout, "[CB-DEBUG] ", fmt,  ##__VA_ARGS__)
#define cb_log_important(fmt, ...) cb_log(stdout, "", fmt,  ##__VA_ARGS__)

/*-----------------------------------------------------------------------*/
/* cb_darr - dynamic array */
/*-----------------------------------------------------------------------*/

static void cb_darr_init(cb_darr* arr)
{
	arr->size = 0;
	arr->capacity = 0;
	arr->data = CB_NULL;
}

static void cb_darr_destroy(cb_darr* arr)
{
	if (arr->data != CB_NULL)
	{
		arr->size = 0;
		arr->capacity = 0;
		CB_FREE(arr->data);
		arr->data = CB_NULL;
	} 
}

CB_INTERNAL void*
cb_darr_ptr(const cb_darr* arr, int index, int sizeof_vlaue)
{
	CB_ASSERT(index >= 0);
	CB_ASSERT(
		index < arr->size /* Within accessible item range */
		|| index == arr->size /* Also allow getting the item at the end */
	);

	return arr->data + (index * sizeof_vlaue);
}

static char* cb_darr_end(const cb_darr* arr, int sizeof_value) { return arr->data + (arr->size * sizeof_value); }

static int cb_darr__get_new_capacity(const cb_darr* arr, int sz) { int new_capacity = arr->capacity ? (arr->capacity + arr->capacity / 2) : 8; return new_capacity > sz ? new_capacity : sz; }

static void cb_darr_reserve(cb_darr* arr, int new_capacity, int sizeof_value)
{
	if (new_capacity <= arr->capacity)
	{
		return;
	}

	char* new_data = (char*)CB_MALLOC((size_t)new_capacity * sizeof_value);
	CB_ASSERT(new_data);
	if (arr->data != CB_NULL) {
		memcpy(new_data, arr->data, (size_t)arr->size * sizeof_value);
		CB_FREE(arr->data);
	}
	arr->data = new_data;
	arr->capacity = new_capacity;
}

CB_INTERNAL void
cb_darr__grow_if_needed(cb_darr* arr, int needed, int sizeof_value)
{
	if (needed > arr->capacity)
		cb_darr_reserve(arr, cb_darr__get_new_capacity(arr, needed), sizeof_value);
}

void
cb_darr_insert_many_space(cb_darr* arr, int index, int count, int sizeof_value)
{
	int count_to_move = arr->size - index;

	CB_ASSERT(arr != NULL);
	CB_ASSERT(count > 0);
	CB_ASSERT(index >= 0);
	CB_ASSERT(index <= arr->size);

	cb_darr__grow_if_needed(arr, arr->size + count, sizeof_value);

	if (count_to_move > 0)
	{
		memmove(
			cb_darr_ptr(arr, index + count, sizeof_value),
			cb_darr_ptr(arr, index, sizeof_value),
			count_to_move * sizeof_value);
	}

	arr->size += count;
}

void
cb_darr_insert_one_space(cb_darr* arr, int index, int sizeof_value)
{
	cb_darr_insert_many_space(arr, index, 1, sizeof_value);
}

void
cb_darr_insert_many(cb_darr* arr, int index, const void* value, int count, int sizeof_value)
{
	cb_darr_insert_many_space(arr, index, count, sizeof_value);

	memcpy(cb_darr_ptr(arr, index, sizeof_value), value, count * sizeof_value);
}

void
cb_darr_insert_one(cb_darr* arr, int index, const void* value, int sizeof_value)
{
	cb_darr_insert_many(arr, index, value, 1, sizeof_value);
}

static void
cb_darr_push_back_many(cb_darr* arr, const void* values_ptr, int count, int sizeof_value)
{
	cb_darr_insert_many(arr, arr->size, values_ptr, count, sizeof_value);
}

static void
cb_darr_remove_many(cb_darr* arr, int index, int count, int sizeof_value)
{
	CB_ASSERT(arr);
	CB_ASSERT(index >= 0);
	CB_ASSERT(count >= 0);
	CB_ASSERT(index < arr->size);
	CB_ASSERT(count <= arr->size);
	CB_ASSERT(index + count <= arr->size);

	if (count <= 0)
		return;

	memmove(
		cb_darr_ptr(arr, index, sizeof_value),
		cb_darr_ptr(arr, index + count, sizeof_value),
		(arr->size - (index + count)) * sizeof_value
	);

	arr->size -= count;
}

static void
cb_darr_remove_one(cb_darr* arr, int index, int sizeof_value)
{
	cb_darr_remove_many(arr, index, 1, sizeof_value);
}

typedef cb_bool(*cb_predicate_t)(const void* left, const void* right);
typedef int (*cb_comp_t)(const void* left, const void* right);

int
cb_lower_bound_comp(const void* void_ptr, int left, int right, const void* value, int sizeof_value, cb_comp_t comp)
{
	const char* ptr = (const char*)void_ptr;
	int count = right - left;
	int step;
	int mid; /* index of the found value */

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

int
cb_lower_bound_predicate(const void* void_ptr, int left, int right, const void* value, int sizeof_value, cb_predicate_t pred)
{
	const char* ptr = (const char*)void_ptr;
	int count = right - left;
	int step;
	int mid; /* index of the found value */

	while (count > 0) {
		step = count >> 1; /* count divide by two using bit shift */

		mid = left + step;

		if (pred(ptr + (mid * sizeof_value), value) != cb_false) {
			left = mid + 1;
			count -= step + 1;
		}
		else {
			count = step;
		}
	}
	return left;
}
/* @FIXME maybe we can directly create an overload for cb_map and cb_mmap, not sure we want to use raw array with lower_bound */
static int cb_darr_lower_bound_predicate(const cb_darr* arr, const void* value, int sizeof_value, cb_predicate_t less)
{
	return cb_lower_bound_predicate(arr->data, 0, arr->size, value, sizeof_value, less);
}

static int cb_darr_lower_bound_comp(const cb_darr* arr, const void* value, int sizeof_value, cb_comp_t comp)
{
	return cb_lower_bound_comp(arr->data, 0, arr->size, value, sizeof_value, comp);
}

/*-----------------------------------------------------------------------*/
/* cb_strv - string view */
/*-----------------------------------------------------------------------*/

static cb_strv cb_strv_make(const char* data, int size) { cb_strv s; s.data = data; s.size = size; return s; }
static cb_strv cb_strv_make_str(const char* str) { return cb_strv_make(str, strlen(str)); }

static int cb_lexicagraphical_cmp(const char* left, size_t left_count, const char* right, size_t right_count)
{
	char c1, c2;
	size_t min_size = left_count < right_count ? left_count : right_count;
	while (min_size-- > 0)
	{
		c1 = (unsigned char)*left++;
		c2 = (unsigned char)*right++;
		if (c1 != c2)
			return c1 < c2 ? 1 : -1;
	};

	return left_count - right_count;
}

int cb_strv_compare(cb_strv sv, const char* data, int size) { return cb_lexicagraphical_cmp(sv.data, sv.size, data, size); }
int cb_strv_compare_strv(cb_strv sv, cb_strv other) { return cb_strv_compare(sv, other.data, other.size); }
int cb_strv_compare_str(cb_strv sv, const char* str) { return cb_strv_compare(sv, str, strlen(str)); }
cb_bool cb_strv_equals(cb_strv sv, const char* data, int size) { return cb_strv_compare(sv, data, size) == 0; }
cb_bool cb_strv_equals_strv(cb_strv sv, cb_strv other) { return cb_strv_compare_strv(sv, other) == 0; }
cb_bool cb_strv_equals_str(cb_strv sv, const char* other) { return cb_strv_compare_strv(sv, cb_strv_make_str(other)) == 0; }

/*-----------------------------------------------------------------------*/
/* cb_dstr - dynamic string */
/*-----------------------------------------------------------------------*/

#define cb_dstr_append_v(dstr, ...) \
	cb_dstr_append_many(dstr \
    , (const char* []) { __VA_ARGS__ } \
	, (sizeof((const char* []) { __VA_ARGS__ }) / sizeof(const char*)))

static const char* cb_empty_string() { return "\0EMPTY_STRING"; }
static void cb_dstr_init(cb_dstr* dstr) { cb_darr_init(dstr); dstr->data = (char*)cb_empty_string(); }
static void cb_dstr_destroy(cb_dstr* dstr) { if (dstr->data == cb_empty_string()) { dstr->data = CB_NULL; } cb_darr_destroy(dstr); }
/* does not free anything, just reset the size to 0 */
static void cb_dstr_clear(cb_dstr* dstr) { if (dstr->data != cb_empty_string()) { dstr->size = 0; dstr->data[dstr->size] = '\0'; } }

static void cb_dstr_reserve(cb_dstr* s, int new_string_capacity)
{
	assert(new_string_capacity > s->capacity && "You should request more capacity, not less."); /* ideally we should ensure this before this call. */
	
	if (new_string_capacity <= s->capacity)
		return; 

	int new_mem_capacity = new_string_capacity + 1;
	char* new_data = (char*)CB_MALLOC((size_t)new_mem_capacity * sizeof(char));
	if (s->size)
	{
		int size_plus_null_term = s->size + 1;
		memcpy(new_data, s->data, (size_t)size_plus_null_term * sizeof(char));
		CB_FREE(s->data);
	}
	s->data = new_data;
	s->capacity = new_string_capacity;
}

static void cb_dstr__grow_if_needed(cb_dstr* s, int needed)
{
	if (needed > s->capacity) { 
		cb_dstr_reserve(s, cb_darr__get_new_capacity(s, needed));
	}
}

static void cb_dstr_append_from(cb_dstr* s, int index, const char* data, int size)
{
	cb_dstr__grow_if_needed(s, index + size);

	memcpy(s->data + index, (const void*)data, ((size) * sizeof(char)));
	s->size = index + size;
	s->data[s->size] = '\0';
}

static int cb_dstr_append_from_fv(cb_dstr* s, int index, const char* fmt, va_list args)
{
	va_list args_copy;
	va_copy(args_copy, args);

	/* Caluclate necessary len */
	int add_len = vsnprintf(NULL, 0, fmt, args_copy);
	CB_ASSERT(add_len >= 0);

	cb_dstr__grow_if_needed(s, s->size + add_len);

	add_len = vsnprintf(s->data + index, add_len + 1, fmt, args);

	s->size = index + add_len;
	return add_len;
}
static void cb_dstr_assign(cb_dstr* s, const char* data, int size) { cb_dstr_append_from(s, 0, data, size); }
static void cb_dstr_assign_str(cb_dstr* s, const char* str) { cb_dstr_assign(s, str, strlen(str)); }

static void cb_dstr_assign_f(cb_dstr* s, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	cb_dstr_append_from_fv(s, 0, fmt, args);
	va_end(args);
}

static void cb_dstr_append_many(cb_dstr* s, const char* strings[], int count)
{
	for (int i = 0; i < count; ++i)
	{
		cb_dstr_append_from( s, s->size, strings[i], strlen(strings[i]));
	}
}

static void cb_dstr_append_str(cb_dstr* s, const char* str) { cb_dstr_append_from(s, s->size, str, strlen(str)); }
static void cb_dstr_append_strv(cb_dstr* s, cb_strv sv) { cb_dstr_append_from(s, s->size, sv.data, sv.size); }
static int  cb_dstr_append_f(cb_dstr* s, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	int len = cb_dstr_append_from_fv(s, s->size, fmt, args);
	va_end(args);
	return len;
}

/*-----------------------------------------------------------------------*/
/* cb_hash */
/*-----------------------------------------------------------------------*/

static unsigned long djb2(const char* s)
{
	unsigned long hash = 5381;
	int c;

	while ((c = *s++))
		hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

	return hash;
}

static unsigned long djb2_strv(const char* str, int count)
{
	unsigned long hash = 5381;
	int i = 0;
	while (i < count)
	{
		hash = ((hash << 5) + hash) + str[i]; /* hash * 33 + c */
		i++;
	}

	return hash;
}

static cb_id cb_hash(const char* str) { return djb2(str); }
static cb_id cb_hash_strv(cb_strv sv) { return djb2_strv(sv.data, sv.size); }

/*-----------------------------------------------------------------------*/
/* cb_kv */
/*-----------------------------------------------------------------------*/

static void
cb_kv__set_key(cb_kv* p, cb_strv sv)
{
	p->hash = cb_hash_strv(sv);
	p->key = sv;
}

static cb_kv
cb_kv_make_with_str(cb_strv sv, const char* value)
{
	cb_kv pair;
	cb_kv__set_key(&pair, sv);
	pair.u.strv = cb_strv_make_str(value);
	return pair;
}

static cb_kv
cb_kv_make_with_ptr(cb_strv sv, const void* ptr)
{
	cb_kv pair;
	cb_kv__set_key(&pair, sv);
	pair.u.ptr = ptr;
	return pair;
}

static int
cb_kv_comp(const cb_kv* left, const cb_kv* right)
{
	return left->hash != right->hash
		? right->hash - left->hash
		: cb_strv_compare_strv(left->key, right->key);
}

static cb_bool
cb_kv_less(const cb_kv* left, const cb_kv* right)
{
	return cb_kv_comp(left, right) < 0;
}

/*-----------------------------------------------------------------------*/
/* cb_map */
/*-----------------------------------------------------------------------*/

static void cb_map_init(cb_map* map) { cb_darrT_init(map); }
static void cb_map_destroy(cb_map* map) { cb_darrT_destroy(map); }

static void cb_map_set(cb_map* map, cb_kv kv)
{
	int index = cb_darr_lower_bound_predicate(&map->base, &kv, sizeof(cb_kv), (cb_predicate_t)cb_kv_less);

    cb_darrT_insert(map, index, kv);
}

static int
cb_map_find(const cb_map* map, const cb_kv* kv)
{
    int index = cb_darr_lower_bound_predicate(&map->base, kv, sizeof(cb_kv), (cb_predicate_t)cb_kv_less);

    if (index == map->base.size || cb_kv_less(kv, cb_darrT_ptr(map, index)))
    {
		index = map->base.size; /* not found */
    }

    return index;
}

static cb_bool cb_map_get_from_kv(cb_map* map, const cb_kv* item, cb_kv* result)
{
	int index = cb_map_find(map, item);
	if (index != map->base.size)
	{
		*result = cb_darrT_at(map, index);
		return cb_true;
	}

	return cb_false;
}

static void cb_map_set_ptr(cb_map* map, cb_strv key, const void* value_ptr)
{
	cb_map_set(map, cb_kv_make_with_ptr(key, value_ptr));
}

static const void* cb_map_get_ptr(cb_map* map, cb_strv key, const void* default_value)
{
	cb_kv key_item;
	cb_kv__set_key(&key_item, key);
	cb_kv result;

	return cb_map_get_from_kv(map, &key_item, &result) ? result.u.ptr : default_value;
}

static cb_strv cb_map_get_strv(cb_map* map, cb_strv key, cb_strv default_value)
{
	cb_kv key_item;
	cb_kv__set_key(&key_item, key);
	cb_kv result;

	return cb_map_get_from_kv(map, &key_item, &result) ? result.u.strv : default_value;
}

static void
cb_map_insert(cb_map* map, cb_kv kv)
{
	/* we don't need to check if something was found or not, if value is not found the new value will be added at the end of the array */
	int index = cb_darr_lower_bound_predicate(&map->base, &kv, sizeof(cb_kv), (cb_predicate_t)cb_kv_less);

	cb_darrT_insert(map, index, kv);
}

static cb_bool
cb_map_remove(cb_map* m, cb_kv kv)
{
	int index = cb_map_find(m, &kv);
	if (index != m->darr.size)
	{
		cb_darrT_remove(m, index);
		return cb_true;
	}

	return cb_false;
}

/*-----------------------------------------------------------------------*/
/* cb_mmap - a multimap */
/*-----------------------------------------------------------------------*/

static void cb_mmap_init(cb_mmap* m) { cb_darrT_init(m); }
static void cb_mmap_destroy(cb_mmap* m) { cb_darrT_destroy(m); }

static void cb_mmap_insert(cb_mmap* m, cb_kv kv)
{
	int index = cb_darr_lower_bound_comp(&m->base, &kv, sizeof(cb_kv), (cb_comp_t)cb_kv_comp);

	cb_darrT_insert(m, index, kv);
}

/* cb_mmap_find does the same as cb_map_find, we should probably remove cb_map implementation if it's not used anymore */
static int
cb_mmap_find(const cb_mmap* map, const cb_kv* kv)
{
	int index = cb_darr_lower_bound_predicate(&map->base, kv, sizeof(cb_kv), (cb_predicate_t)cb_kv_less);

	if (index == map->base.size || cb_kv_less(kv, cb_darrT_ptr(map, index)))
	{
		index = map->base.size; /* not found */
	}

	return index;
}

CB_INTERNAL cb_bool
cb_mmap_try_get_first(const cb_mmap* m, cb_strv key, cb_kv* kv)
{
	cb_kv key_item = cb_kv_make_with_str(key, "");
	int index = cb_mmap_find(m, &key_item);

	if (index != m->darr.size) /* found */
	{
		*kv = cb_darrT_at(m, index);
		return cb_true;
	}

	return cb_false;
}

CB_INTERNAL cb_kv_range
cb_mmap_get_range(const cb_mmap* m, cb_strv key)
{
	cb_comp_t comp = (cb_comp_t)cb_kv_comp;

	cb_kv_range result = { 0, 0, 0 };

	cb_kv key_item = cb_kv_make_with_str(key, "");

	int index = cb_mmap_find(m, &key_item);

	if (index == m->darr.size)
	{
		return result;
	}

	/* An item has been found */

	result.begin = cb_darrT_ptr(m, index);

	result.count++;

	/* Check for other items */
	while (index != m->base.size && comp(cb_darrT_ptr(m, index), &key_item) == 0)
	{
		index += 1;

		result.count++;
	}

	result.end = cb_darrT_ptr(m, index);
	return result;
}

CB_INTERNAL cb_kv_range
cb_mmap_get_range_str(const cb_mmap* m, const char* key)
{
	return cb_mmap_get_range(m, cb_strv_make_str(key));
}

cb_bool cb_mmap_range_get_next(cb_kv_range* range, cb_kv* next)
{
	CB_ASSERT(range->begin <= range->end);

	if (range->begin < range->end)
	{
		*next = *range->begin;

		range->begin += 1;
		return cb_true;
	}

	return cb_false;
}

/* Remove all values found in keys*/
static int cb_mmap_remove(cb_mmap* m, cb_kv kv)
{
	cb_kv_range range = cb_mmap_get_range(m, kv.key);

	int count_to_remove = range.count;
	int current_index = range.begin - m->darr.data;

	if (range.count)
	{
		while (current_index < count_to_remove)
		{
			cb_darrT_remove(m, current_index);
			current_index += 1;
		}
	}
	return count_to_remove;
}

static void cb_context_init(cb_context* ctx)
{
	memset(ctx, 0, sizeof(cb_context));
	cb_map_init(&ctx->projects);
	ctx->current_project = 0;
}

static cb_project_t* cb_find_project_by_name(cb_strv sv)
{
	void* default_value = CB_NULL;
	return (cb_project_t*)cb_map_get_ptr(&current_ctx->projects, sv, default_value);
}

static cb_project_t* cb_find_project_by_name_str(const char* name) { return cb_find_project_by_name(cb_strv_make_str(name)); }

static void cb_project_init(cb_project_t* project)
{
	memset(project, 0, sizeof(cb_project_t));

	cb_darrT_init(&project->file_commands);
	cb_mmap_init(&project->mmap);
}

static void cb_project_destroy(cb_project_t* project)
{
	cb_darrT_destroy(&project->file_commands);
	cb_mmap_destroy(&project->mmap);
}

static cb_project_t* cb_create_project(const char* name)
{
    cb_id id = cb_hash(name);
	
	cb_project_t* project = (cb_project_t*)CB_MALLOC(sizeof(cb_project_t));
	cb_project_init(project);
	project->context = current_ctx;
	project->id = id;
	project->name.data = name;
	project->name.size = strlen(name);
	
	cb_map_set_ptr(&current_ctx->projects, cb_strv_make_str(name), project);
	
    return project;
}

/* API */

#ifdef _WIN32
/* Any error would silently crash any application, this handler is just there to display a message and exit the application with a specific value */
 __declspec(noinline) static LONG WINAPI exit_on_exception_handler(EXCEPTION_POINTERS* ex_ptr)
 {
    (void)ex_ptr;
	int exit_code=1;
	printf("[CB] Error: unexpected error. exited with code %d\n", exit_code);
	exit(exit_code);
}
#endif

RE_CB_API void cb_init()
{
	cb_context_init(&default_ctx);
	current_ctx = &default_ctx;
#ifdef _WIN32
	cb_bool exit_on_exception = cb_true; /* @TODO make this configurable */
    if (exit_on_exception)
	{
		SetUnhandledExceptionFilter(exit_on_exception_handler);
	}
#else
	CB_ASSERT("Not Yet Implemented");
#endif
}

RE_CB_API cb_bool cb_project(const char* name)
{
	cb_project_t* project = cb_find_project_by_name_str(name);
	cb_bool is_new_project = project == CB_NULL;
	if (is_new_project)
	{
		project = cb_create_project(name);
	}
	
	current_ctx->current_project = project;
	return is_new_project;
}

RE_CB_API void cb_add_files(const char* pattern)
{
	CB_ASSERT(current_ctx->current_project);
	
	cb_project_t* p = current_ctx->current_project;
	
	cb_file_command cmd;
	cmd.glob = 1; 
	cmd.pattern = pattern;

	cb_darrT_push_back(&p->file_commands, cmd);
}

RE_CB_API void cb_add_file(const char* file)
{
	CB_ASSERT(current_ctx->current_project);
	
	cb_project_t* p = current_ctx->current_project;
	
	cb_file_command cmd;
	cmd.glob = 0; 
	cmd.pattern = file;

	cb_darrT_push_back(&p->file_commands, cmd);
}


RE_CB_API void
cb_add(const char* key, const char* value)
{
	CB_ASSERT(current_ctx->current_project);

	cb_project_t* p = current_ctx->current_project;

	cb_kv kv = cb_kv_make_with_str(cb_strv_make_str(key), value);

	cb_mmap_insert(&p->mmap, kv);
}

RE_CB_API void
cb_set(const char* key, const char* value)
{
	/* @FIXME this can easily be optimized, but we don't care about that right now. */
	cb_remove_all(key);
	cb_add(key, value);
}

RE_CB_API int
cb_remove_all(const char* key)
{
	cb_project_t* p = current_ctx->current_project;

	cb_kv kv = cb_kv_make_with_str(cb_strv_make_str(key), "");
	return cb_mmap_remove(&p->mmap, kv);
}

RE_CB_API cb_bool
cb_remove_one(const char* key, const char* value)
{
	cb_project_t* p = current_ctx->current_project;

	cb_kv kv = cb_kv_make_with_str(cb_strv_make_str(key), value);
	cb_kv_range range = cb_mmap_get_range(&p->mmap, kv.key);

	while (range.begin < range.begin)
	{
		if (cb_strv_equals_strv((*range.begin).u.strv, kv.u.strv))
		{
			int index = p->mmap.darr.data - range.begin;
			cb_darrT_remove(&p->mmap, index);
			return cb_true;
		}
		range.begin++;
	}
	return cb_false;
}

/* #file utils */

static inline cb_bool cb_is_directory_separator(char c) { return (c == '/' || c == '\\'); }

#define CB_NPOS (-1)
static int cb_rfind(cb_strv s, char c)
{
	if (s.size == 0) return CB_NPOS;

	const char* begin = s.data;
	const char* end = s.data + s.size - 1;
	while (end >= begin && *end != c)
	{
		end--;
	}
	return end < begin ? CB_NPOS : (end - begin);
}
static int cb_rfind2(cb_strv s, char c1, char c2)
{
	if (s.size == 0) return CB_NPOS;

	const char* begin = s.data;
	const char* end = s.data + s.size - 1;
	while (end >= begin && *end != c1 && *end != c2)
	{
		end--;
	}
	return end < begin ? CB_NPOS : (end - begin);
}
static cb_strv cb_path_filename(cb_strv path)
{
	int pos = cb_rfind2(path, '/', '\\');
	if (pos != CB_NPOS && pos > 0) {
		return cb_strv_make(path.data + pos + 1 /* plus one because we don't want the slash char */
			, path.size - pos);
	}
	return path;
}

static cb_strv cb_path_basename(cb_strv s)
{
	cb_strv filename = cb_path_filename(s);

	if (!cb_strv_equals_str(filename, ".")
		&& !cb_strv_equals_str(filename, "..")) {
		int pos = cb_rfind(filename, '.');
		if (pos != CB_NPOS && pos > 0) {
			return cb_strv_make(filename.data, pos);
		}
	}
	return filename;
}
/*  #file_iterator */

#define CB_MAX_PATH 1024 /* this is an arbitrary limit */

/* file iterator (can be recursive) */
typedef struct cb_file_it_t {
	cb_bool has_next;

    /* stack used for recursion */
	char current_file[CB_MAX_PATH];

#define CB_MAX_DIR_DEPTH 256
	/* stack used for recursion */
	int dir_len_stack[CB_MAX_DIR_DEPTH];
	int stack_size;

#if defined(_WIN32)
#define CB_INVALID_FILE_HANDLE INVALID_HANDLE_VALUE
	WIN32_FIND_DATAA find_data;
	
	HANDLE handle_stack[CB_MAX_DIR_DEPTH];
#else
#define CB_INVALID_FILE_HANDLE NULL
	DIR* handle_stack[CB_MAX_DIR_DEPTH];
	struct dirent* find_data;
#endif

} cb_file_it;

#define cb_safe_strcpy(dst, src, index, max) cb_safe_strcpy_internal(dst, src, index, max, __FILE__, __LINE__)
static int cb_safe_strcpy_internal(char* dst, const char* src, int index, int max, const char* file, int line)
{
	char c;
	const char* original = src;

	do {
		if (index >= max) {
			cb_log_error("[String \"%s\" too long to copy on line %d in file %s (max length of %d).", original, line, file, max);
			CB_ASSERT(0);
			break;
		}

		c = *src++;
		dst[index] = c;
		++index;
	} while (c);

	return index;
}

static void cb_file_it_destroy(cb_file_it* it);

static int cb_safe_combine_path(char* dst, const char* path, int index)
{
	return cb_safe_strcpy(dst, path, index, CB_MAX_PATH);
}

static void cb_file_it__push_dir(cb_file_it* it, const char* directory)
{
	int current_dir_len = it->stack_size >= 0 ? it->dir_len_stack[it->stack_size] : 0;
	int n = cb_safe_combine_path(it->current_file, directory, current_dir_len);

	/* add slash if it's missing , n-1 to get the last char (null terminating char), n-2 to get the last valid char */
	if (!cb_is_directory_separator(it->current_file[n - 2]))
	{
		n = cb_safe_combine_path(it->current_file, CB_DEFAULT_DIR_SEPARATOR, n - 1);
	}
	
	int new_directory_len = n - 1;
	
#if defined(_WIN32)
	
    /* add asterisk to read all files from the directory */
	n = cb_safe_combine_path(it->current_file, "*", n - 1);
	HANDLE handle = FindFirstFileA(it->current_file, &it->find_data);
	
	if (handle == INVALID_HANDLE_VALUE)
	{
		cb_log_error("Could not open directory '%s'", it->current_file);

		it->has_next = 0;
		return;
	}

	/* remove the last asterisk '*' */
	it->current_file[new_directory_len] = '\0';
	
#else
	DIR* handle = opendir(it->current_file);

	if (handle == NULL)
	{
		cb_log_error("Could not open directory '%s': %s.", directory, strerror(errno));
		it->has_next = 0;
		return;
	}

#endif
	it->stack_size++;

	it->handle_stack[it->stack_size] = handle;

	it->dir_len_stack[it->stack_size] =  new_directory_len;
	
	it->has_next = 1;
}

void cb_file_it_close_current_handle(cb_file_it* it)
{
	if (it->handle_stack[it->stack_size] != CB_INVALID_FILE_HANDLE)
	{
#if defined(_WIN32)
		FindClose(it->handle_stack[it->stack_size]);
#else
		closedir(it->handle_stack[it->stack_size]);
#endif
		it->handle_stack[it->stack_size] = 0;
	}
}

static const char*
cb_file_it__get_next_entry(cb_file_it* it)
{
#if defined(_WIN32)
	BOOL b = FindNextFileA(it->handle_stack[it->stack_size], &it->find_data);
	if(!b)
	{
		DWORD err = GetLastError();
		if (err != ERROR_SUCCESS && err != ERROR_NO_MORE_FILES)
		{
			cb_log_error("Could not go to the next entry.");
		}
		return NULL;
	}
	return it->find_data.cFileName;
#else
	it->find_data = readdir(it->handle_stack[it->stack_size]);

	return it->find_data != NULL ? it->find_data->d_name: NULL;
#endif
}

static cb_bool
cb_file_it__current_entry_is_directory(cb_file_it* it)
{
#if defined(_WIN32)
	return !!(it->find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
#else
	return it->find_data->d_type == DT_DIR;
#endif
}

void cb_file_it__pop_dir(cb_file_it* it)
{
	if (it->stack_size > 0)
	{
		cb_file_it_close_current_handle(it);

		it->stack_size -= 1;
		
		int dir_len = it->dir_len_stack[it->stack_size];
		it->current_file[dir_len] = 0; /* set null term char at the end of the*/

		if (it->stack_size < 0)
		{
			it->has_next = 0;
		}
	}
}
	
static void cb_file_it_init(cb_file_it* it, const char* base_directory)
{
	memset(it, 0, sizeof(cb_file_it));
	it->stack_size = -1;
	
	cb_file_it__push_dir(it, base_directory);
}

static void cb_file_it_destroy(cb_file_it* it)
{
	while(it->stack_size > 0)
	{
		cb_file_it_close_current_handle(it);
		it->stack_size -= 1;
	}
	
	it->current_file[0] = 0;
	it->has_next = 0;
}

static const char* cb_file_it_current_file(cb_file_it* it) { return it->current_file; }

static cb_bool cb_file_it_get_next(cb_file_it* it)
{
	CB_ASSERT(it->has_next);

	cb_bool is_directory = false;
	const char* found = 0;

	do
	{
		/* Check if there is remaining/next file, if not just pop the stack.
		*  Do it multiple times if we are at the end of a directory and a parent directory.
		*/
		while ((found = cb_file_it__get_next_entry(it)) == NULL)
		{
			/* no parent directory so it's the end */
			if (it->stack_size == 0)
			{
				it->has_next = false;
				return cb_false;
			}

			/* since there is no more file in the directory
			   we pop the directory and loop again to get the next file
			*/
			cb_file_it__pop_dir(it);
		}
		
		is_directory = cb_file_it__current_entry_is_directory(it);
	/* skip parent directory or current directory ..' or '.'*/
	} while (it->has_next
		&& is_directory
		&& found[0] == '.');

	/* build path with current file found */
	cb_safe_combine_path(it->current_file, found, it->dir_len_stack[it->stack_size]);

	if (is_directory)
	{
		cb_file_it__push_dir(it, found);
	}
	return cb_true;
}

static cb_bool cb_file_it_get_next_glob(cb_file_it* it, const char* pattern)
{
	while (cb_file_it_get_next(it))
	{
		if (cb_wildmatch(pattern, cb_file_it_current_file(it)))
		{
			return cb_true;
		}
	}
	return cb_false;
}

/* return cb_true if separator has been added */
static cb_bool cb_add_trailing_dir_separator(char* path, int path_len)
{
	if (!path || path_len >= CB_MAX_PATH) return cb_false;

	if (path_len)
	{
		path += path_len;
		if (path[-1] != '/' && path[-1] != '\\')
		{
			*path++ = CB_DEFAULT_DIR_SEPARATOR_CHAR;
			*path = '\0';
			return cb_true;
		}
	}
	return cb_false;
}
#ifdef WIN32
/* @TODO rename everything related to file into cp_path_XXX */
static cb_bool cb_path_exists(const char* path) {

	DWORD attr = GetFileAttributesA(path);
	return attr != INVALID_FILE_ATTRIBUTES;
}
static cb_bool cb_create_directory(const char* path) { return CreateDirectoryA(path, NULL); }
#else

static cb_bool cb_path_exists(const char* path) { return access(path, F_OK) == 0; }

static cb_bool cb_create_directory(const char* path) { return mkdir(path, 0777) == 0; }

#endif

static void cb_create_directory_recursively(const char* path, int size)
{
	if (path == CB_NULL || size <= 0) {
		cb_log_error("Could not create directory. Path is empty.");
		return;
	}
	
	if (size > CB_MAX_PATH) {
		cb_log_error("Could not create directory. Path is too long '%s'.", path);
		return;
	}

	if (!cb_path_exists(path))
	{
		char tmp_buffer[CB_MAX_PATH + 1];  /* extra for the possible missing separator */

		cb_safe_strcpy(tmp_buffer, path, 0, CB_MAX_PATH);

		char* cur = tmp_buffer;
		char* end = tmp_buffer + size;
		while (cur < end)
		{
			/* go to next directory separator */
			while (*cur && *cur != '\\' && *cur != '/')
				cur++;

			if (*cur)
			{
				
				*cur = '\0'; /* terminate path at separator */
				if(!cb_path_exists(tmp_buffer))
				{
					if(!cb_create_directory(tmp_buffer))
					{
						cb_log_error("Could not create directory '%s'.", tmp_buffer);
						return;
					}
				}
				*cur = CB_DEFAULT_DIR_SEPARATOR_CHAR; /* put the separator back */
			}
			cur++;
		}
	}
}

static cb_bool cb_copy_file(const char* src_path, const char* dest_path)
{
	/* create target directory if it does not exists */
	cb_create_directory_recursively(dest_path, strlen(dest_path));
	cb_log_debug("Copying '%s' to '%s'", src_path, dest_path);
#ifdef _WIN32

	DWORD attr = GetFileAttributesA(src_path);

	if (attr == INVALID_FILE_ATTRIBUTES) {
		cb_log_error("Could not rertieve file attributes of file '%s' (%d).", src_path, GetLastError());
		return cb_false;
	}

	cb_bool is_directory = attr & FILE_ATTRIBUTE_DIRECTORY;
	BOOL fail_if_exists = FALSE;
	if (!is_directory && !CopyFileA(src_path, dest_path, fail_if_exists)) {
		cb_log_error("Could not copy file '%s', %lu", src_path, GetLastError());
		return cb_false;
	}
	return cb_true;
#else

	int src_fd = -1;
	int dst_fd = -1;
	src_fd = open(src_path, O_RDONLY);
	if (src_fd < 0) {
		cb_log_error("Could not open file '%s': %s", src_path, strerror(errno));
		return cb_false;
	}
	
    struct stat src_stat;
    if (fstat(src_fd, &src_stat) < 0) {
        cb_log_error("Could not get fstat of file '%s': %s", src_path, strerror(errno));
		close(src_fd);
		return cb_false;
    }
	
	dst_fd = open(dest_path, O_CREAT | O_TRUNC | O_WRONLY, src_stat.st_mode);

	if (dst_fd < 0) {
        cb_log_error("Could not open file '%s': %s", dest_path, strerror(errno));
		close(src_fd);
		return cb_false;
	}
	
    int64_t total_bytes_copied = 0;
    int64_t bytes_left = src_stat.st_size;
    while (bytes_left > 0)
    {
		off_t sendfile_off = total_bytes_copied;
		int64_t send_result = sendfile(dst_fd, src_fd, &sendfile_off, bytes_left);
		if(send_result <= 0)
		{
			break;
		}
		int64_t bytes_copied = (int64_t)send_result;
		bytes_left -= bytes_copied;
		total_bytes_copied += bytes_copied;
    }
  
	close(src_fd);
	close(dst_fd);
	return bytes_left == 0;
#endif
}

/* recursively copy the content of the directory in another one, empty directory will be omitted */
static cb_bool cb_copy_directory(const char* source_dir, const char* target_dir)
{
	/* only use to copy a directory */
	char dest_buffer[CB_MAX_PATH];
	memset(dest_buffer, 0, sizeof(dest_buffer));

	cb_file_it it;
	cb_file_it_init(&it, source_dir);

	while (cb_file_it_get_next(&it))
	{
		/* copy current directory*/
		const char* source_relative_path = it.current_file + it.dir_len_stack[0];

		int n = snprintf(dest_buffer, CB_MAX_PATH, "%s", target_dir);
		n += cb_add_trailing_dir_separator(dest_buffer, n) ? 1 : 0;
		n += snprintf(dest_buffer + n, CB_MAX_PATH, "%s", source_relative_path);

		cb_copy_file(it.current_file, dest_buffer);
	}
	return cb_true;
}


static cb_bool
cb_delete_file(const char* src_path)
{
	cb_bool result = 0;
	cb_log_debug("Deleting file '%s'.", src_path);
#ifdef _WIN32
	result = DeleteFileA(src_path);
#else
	result = remove(src_path) != -1;
#endif
	if (!result)
		cb_log_debug("Could not delete file '%s'.", src_path);

	return result;
}

static cb_bool
cb_move_file(const char* src_path, const char* dest_path)
{
	if (cb_copy_file(src_path, dest_path))
	{
		return cb_delete_file(src_path);
	}

	return cb_false;
}

/* recursively copy the content of the directory in another one, empty directory will be omitted */
static cb_bool
cb_move_files(const char* source_dir, const char* target_dir, cb_bool(*can_move)(cb_strv path))
{
	/* only use to copy a directory */
	char dest_buffer[CB_MAX_PATH];
	memset(dest_buffer, 0, sizeof(dest_buffer));

	cb_file_it it;
	cb_file_it_init(&it, source_dir);

	while (cb_file_it_get_next(&it))
	{
		/* copy current directory*/
		const char* source_relative_path = it.current_file + it.dir_len_stack[0];

		int n = snprintf(dest_buffer, CB_MAX_PATH, "%s", target_dir);
		n += cb_add_trailing_dir_separator(dest_buffer, n) ? 1 : 0;
		n += snprintf(dest_buffer + n, CB_MAX_PATH, "%s", source_relative_path);

		cb_strv p = cb_strv_make(dest_buffer, n);

		cb_bool should_move = !can_move || (can_move && can_move(p));
		if (should_move)
		{
			cb_move_file(it.current_file, dest_buffer);
		}
	}
	return cb_true;
}

/* Properties are just (strv) values from the map of a project. */
static cb_bool try_get_property(cb_project_t* project, const char* key, cb_strv* result)
{
	cb_kv kv_result;
	if (cb_mmap_try_get_first(&project->mmap, cb_strv_make_str(key), &kv_result))
	{
		*result = kv_result.u.strv;
		return cb_true;
	}
	return cb_false;
}

static cb_bool
cb_property_equals(cb_project_t* project, const char* key, const char* comparison_value)
{
	cb_strv result;
	return try_get_property(project, key, &result)
		&& cb_strv_equals_str(result, comparison_value);
}

RE_CB_API void
cb_bake(cb_toolchain_t toolchain, const char* project_name)
{
	toolchain.bake(&toolchain, project_name);
}

static void
cb_dstr_add_output_path(cb_dstr* s, cb_project_t* project, const char* default_output_directory)
{
	cb_strv out_dir;
	if (try_get_property(project, cbk_OUTPUT_DIR, &out_dir))
	{
		cb_dstr_append_v(s, out_dir.data);
		/* Add trailing slash if necessary */
		cb_dstr_append_v(s, cb_is_directory_separator(out_dir.data[out_dir.size - 1]) ? "" : CB_DEFAULT_DIR_SEPARATOR);
	}
	else /* Get default output directory */
	{
		cb_dstr_append_v(s, default_output_directory, CB_DEFAULT_DIR_SEPARATOR);
		cb_dstr_append_strv(s, project->name);
		cb_dstr_append_str(s, CB_DEFAULT_DIR_SEPARATOR);
	}
}

RE_CB_API cb_bool
cb_bake_and_run(cb_toolchain_t toolchain, const char* project_name)
{
	cb_bake(toolchain, project_name);
	
	cb_project_t* project = cb_find_project_by_name_str(project_name);

	if (!project)
	{
		cb_log_error("Project not found '%s'", project_name);
		return cb_false;
	}

	cb_dstr str;
	cb_dstr_init(&str);
	cb_dstr_add_output_path(&str, project, toolchain.default_directory_base);

	if (!cb_property_equals(project, cbk_BINARY_TYPE, cbk_exe))
	{
		cb_log_error("Cannot use 'cb_bake_and_run' in non-executable project");
		return cb_false;
	}

	/* executable */
	cb_dstr_append_v(&str, project_name);
	if (!cb_subprocess(str.data))
	{
		return cb_false;
	}

	return cb_true;
}

/* EXTERNAL DEPENDENCY (still internal API) */

/* ================================================================ */
/* WILDMATCH_H */
/* Taken from https://github.com/ujr/wildmatch - UNLICENSED
*  - 'cb_decode_utf8' was named 'decode'
*  - 'cb_wildmatch' was created from 'match1'
/* ================================================================ */

static cb_bool cb_wildcad_debug = 0;

/** return nbytes, 0 on end, -1 on error */
static int
cb_decode_utf8(const void* p, int* pc)
{
	const int replacement = 0xFFFD;
	const unsigned char* s = (const unsigned char*)p;
	if (s[0] < 0x80) {
		*pc = s[0];
		return *pc ? 1 : 0;
	}
	if ((s[0] & 0xE0) == 0xC0) {
		*pc = (int)(s[0] & 0x1F) << 6
			| (int)(s[1] & 0x3F);
		return 2;
	}
	if ((s[0] & 0xF0) == 0xE0) {
		*pc = (int)(s[0] & 0x0F) << 12
			| (int)(s[1] & 0x3F) << 6
			| (int)(s[2] & 0x3F);
		/* surrogate pairs not allowed in UTF8 */
		if (0xD800 <= *pc && *pc <= 0xDFFF)
			*pc = replacement;
		return 3;
	}
	if ((s[0] & 0xF8) == 0xF0 && (s[0] <= 0xF4)) {
		/* 2nd cond: not greater than 0x10FFFF */
		*pc = (int)(s[0] & 0x07) << 18
			| (int)(s[1] & 0x3F) << 12
			| (int)(s[2] & 0x3F) << 6
			| (int)(s[3] & 0x3F);
		return 4;
	}
	*pc = replacement;
	/*errno = EILSEQ;*/
	return -1;
}

/* backslash and slash are assumed to be the same */
static cb_bool
cb_path_char_is_different(int left, int right) { return cb_is_directory_separator((char)left) ? !cb_is_directory_separator((char)right) : left != right; }

static cb_bool
cb_wildmatch(const char* pat, const char* str)
{
	const char* p, * s;
	int pc, sc;
	int len = 0;
	p = s = 0;           /* anchor initially not set */

	if (!pat || !str) return cb_false;

	for (;;) {
		if (cb_wildcad_debug)
			fprintf(stderr, "s=%s\tp=%s\n", str, pat);
		len = cb_decode_utf8(pat, &pc);
		if (len < 0)
			return cb_false;
		pat += len;
		if (pc == '*') {
			while (*pat == '*') pat++; /* multiple wildcards have not special effect compared to a single wildcard */
			p = pat;         /* set anchor just after wild star */
			s = str;
			continue;
		}
		len = cb_decode_utf8(str, &sc);
		if (len < 0)
			return cb_false;
		str += len;
		if (sc == '\0')
			return pc == '\0';
		if (pc != '?' && cb_path_char_is_different(pc,sc)) {
			if (!p)
				return cb_false;
			pat = p;         /* resume at anchor in pattern */     
			str = s += cb_decode_utf8(s, &pc); /* but one later in string */
			continue;
		}
	}
}

RE_CB_API void
cb_wildmatch_test()
{
	CB_ASSERT(cb_wildmatch("a", "a"));
	CB_ASSERT(!cb_wildmatch("a", "B"));
	CB_ASSERT(cb_wildmatch("*", "a"));

	CB_ASSERT(cb_wildmatch("*.c", "a.c"));
	CB_ASSERT(cb_wildmatch("\\*.c", "\\a.c"));
	CB_ASSERT(cb_wildmatch("\\**.c", "\\a.c"));
	CB_ASSERT(cb_wildmatch("/**.c", "\\a.c"));
	CB_ASSERT(cb_wildmatch("a/**.c", "a/a.c"));
	CB_ASSERT(cb_wildmatch("a\\**.c", "a/a.c"));
	CB_ASSERT(cb_wildmatch("*.c", ".\\src\\tester\\a.c"));
	CB_ASSERT(cb_wildmatch("*src*.c", ".\\src\\tester\\a.c"));
	CB_ASSERT(cb_wildmatch("*\\src\\*.c", ".\\src\\tester\\a.c"));
}

#if _WIN32

/* #process #subprocess */

/* the char* cmd should be writtable */
RE_CB_API cb_bool
cb_subprocess(const char* str)
{
	cb_dstr cmd;
	cb_dstr_init(&cmd);
	cb_dstr_append_from(&cmd, 0, str, strlen(str));
	
	cb_log_debug("Running subprocess '%s'", cmd.data);
	STARTUPINFOA si;
	PROCESS_INFORMATION pi;

	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));

	// Start the child process. 
	if (!CreateProcessA(NULL,   // No module name (use command line)
		cmd.data,       // Command line
		NULL,           // Process handle not inheritable
		NULL,           // Thread handle not inheritable
		FALSE,          // Set handle inheritance to FALSE
		0,              // No creation flags
		NULL,           // Use parent's environment block
		NULL,           // Use parent's starting directory 
		&si,            // Pointer to STARTUPINFO structure
		&pi)            // Pointer to PROCESS_INFORMATION structure
		)
	{
		cb_log_error("CreateProcessA failed (%d).", GetLastError());
		return cb_false;
	}

	WaitForSingleObject(pi.hProcess, INFINITE);

	// Close process and thread handles. 
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	return cb_true;
}
#else
	

/* space or tab */
static cb_bool cb_is_space(char c) { return c == ' ' || c == '\t'; }
static cb_bool cb_is_end_of_quote(const char* str, char quote_type)
{
	return *str != '\0'
		&& *str == quote_type
		&& (str[1] == '\0' || cb_is_space(str[1]));

}
/* Parse arguments, space is a separator and everything between <space or begining of string><quote> and <quote><space or end of string>
* Quotes can be double quotes or simple quotes.
a b c   => 3 arguments a b c
abc     => 1 argument "abc"
a "b" c => 3 arguments a b c;
a"b"c   => 1 argument a"b"c
"a"b"c" => 1 argument a"b"c
*/
static const char* cb_get_next_arg(const char* str, cb_strv* sv)
{
	sv->data = str;
	sv->size = 0;
	if (str == NULL || *str == '\0')
		return CB_NULL;
		
	while (*str != '\0')
	{
		/* Skip spaces */
		while (*str != '\0' && cb_is_space(*str))
			str += 1;

		/* Return early if end of string */
		if (*str == '\0') 
			return sv->size > 0 ? str: CB_NULL;

		/* Handle quotes */
		if (*str == '\'' || *str == '\"')
		{
			const char* quote = str;
			str += 1; /* skip quote */
			
			/* Return early if end of string */
			if (*str == '\0')
				return sv->size > 0 ? str : CB_NULL;

			/* Quote next the previous one so it's an empty content, we look for another item */
			if (cb_is_end_of_quote(str, *quote))
			{
				str += 1; /* Skip quote */
				continue;
			}

			sv->data = str; /* The next argument will begin right after the quote */
			/* Skip everything until the next unescaped quote */
			while (*str != '\0' && !cb_is_end_of_quote(str, *quote))
				str += 1;

			/* Either it's the end of the quoted string, either we reached the null terminating string */
			sv->size = (str - quote) - 1;

			/* Eat trailing quote so that next argument can start with space */
			while (*str != '\0' && *str == *quote)
				str += 1;

			return str;
		}
		else /* is char */
		{
			const char* ch = str;
			while (*str != '\0' && !cb_is_space(*str))
				str += 1;

			sv->data = ch; /* remove quote */
			sv->size = str - ch;
			return str;
		}
	}
	return CB_NULL;
}

#define CB_INVALID_PROCESS (-1)

pid_t cb_fork_process(char* args[])
{
	pid_t pid = fork();
	if (pid < 0) {
		cb_log_error("Could not fork child process: %s", strerror(errno));
		return CB_INVALID_PROCESS;
	}

	if (pid == 0) {
		if (execvp(args[0], args) == CB_INVALID_PROCESS) {
			cb_log_error("Could not exec child process: %s", strerror(errno));
			exit(1);
		}
		CB_ASSERT(0 && "unreachable");
	}
}

RE_CB_API cb_bool
cb_subprocess(const char* str)
{
	cb_log_debug("Running subprocess '%s'", str);

	cb_dstr cmd;
	cb_dstr_init(&cmd);
	/* cmd will be populated later, but we don't want to reallocate here
	* to avoid getting invalid pointers.
	*/
	/* @FIXME maybe there is better way to create an array of args that does not involve*/
	cb_dstr_reserve(&cmd, strlen(str) + 1); /* +1 for the last space */
	cb_darrT(const char*) args;
	cb_darrT_init(&args);

	cb_strv arg;
	const char* cursor = str;
	while ((cursor = cb_get_next_arg(cursor, &arg)) != NULL)
	{
		const char* arg_ptr = cmd.data + cmd.size;
		cb_darrT_push_back(&args, arg_ptr);

		/* Add string with trailing space */
		cb_dstr_append_f(&cmd, "%.*s ", arg.size, arg.data);

		/* Replace previously added space with \0 */
		cmd.data[cmd.size - 1] = '\0';
	}

	/* last value of the args should be a null value, only append if necessary */
	if (args.darr.size > 0)
	{
		cb_darrT_push_back(&args, NULL);
	}

	pid_t pid  = cb_fork_process((char**)args.darr.data);
	if (pid == CB_INVALID_PROCESS)
	{
		cb_darrT_destroy(&args);
		cb_dstr_destroy(&cmd);
		return cb_false;
	}

	/* wait for process to be done */
	for (;;) {
		int wstatus = 0;
		if (waitpid(pid, &wstatus, 0) < 0) {
			cb_log_error("Could not wait on command (pid %d): '%s'", pid, strerror(errno));
			cb_darrT_destroy(&args);
			cb_dstr_destroy(&cmd);
			return cb_false;
		}

		if (WIFEXITED(wstatus)) {
			int exit_status = WEXITSTATUS(wstatus);
			if (exit_status != 0) {
				cb_log_error("Command exited with exit code '%d'", exit_status);
				cb_darrT_destroy(&args);
				cb_dstr_destroy(&cmd);
				return cb_false;
			}

			break;
		}

		if (WIFSIGNALED(wstatus)) {
			cb_log_error("Command process was terminated by '%s'", strsignal(WTERMSIG(wstatus)));
			cb_darrT_destroy(&args);
			cb_dstr_destroy(&cmd);
			return cb_false;
		}
	}
	
	cb_darrT_destroy(&args);
	cb_dstr_destroy(&cmd);
	return cb_true;
}

#endif

#ifdef _WIN32

/* ================================================================ */
/* toolchain MSVC */
/* ================================================================ */

/* #msvc #toolchain */

RE_CB_API cb_bool
cb_toolchain_msvc_bake(cb_toolchain_t* tc, const char* project_name)
{
	cb_project_t* project = cb_find_project_by_name_str(project_name);

	if (!project)
	{
		cb_log_error("Project not found '%s'\n", project_name);
		return cb_false;
	}
	
	const char* _ = "  "; /* Space to separate command arguments. */
	/* cl.exe command */
	cb_dstr str;
	cb_dstr_init(&str);
	
	cb_dstr str_obj; /* to keep track of the .obj generated and copy them.*/
	cb_dstr_init(&str_obj);

	/* Output path - contains the directory path or binary file path (it just depends on how deep we are) */
	cb_dstr str_ouput_path;
	cb_dstr_init(&str_ouput_path);

	/* Format output directory */
	cb_dstr_add_output_path(&str_ouput_path, project, tc->default_directory_base);

	cb_create_directory_recursively(str_ouput_path.data, str_ouput_path.size);
	cb_dstr_append_v(&str, "cl.exe", _);

	/* Handle binary type */

	cb_bool is_exe = cb_property_equals(project, cbk_BINARY_TYPE, cbk_exe);
	cb_bool is_shared_library = cb_property_equals(project, cbk_BINARY_TYPE, cbk_shared_lib);
	cb_bool is_static_library = cb_property_equals(project, cbk_BINARY_TYPE, cbk_static_lib);

	CB_ASSERT(is_exe || is_shared_library || is_static_library && "Unknown library type");

	if (is_static_library) {
		cb_dstr_append_v(&str, "/c", _);
	}
	else if (is_shared_library) {
		cb_dstr_append_v(&str, "/LD", _);
	}

	/* Add output directory to cl.exe command */
	cb_dstr_append_v(&str, "/Fo", str_ouput_path.data, _);

	const char* ext = "";
	ext = is_exe ? ".exe" : ext;
	ext = is_static_library ? ".lib" : ext;
	ext = is_shared_library ? ".dll" : ext;

	/* Define binary output for executable and shared library. Static library is set with the link.exe command*/
	if (is_exe || is_shared_library)
	{
		cb_dstr_append_v(&str, "/Fe", str_ouput_path.data, "\\", project_name, ext, _);
	}

	/* Append include directories */
	{
		cb_kv_range range = cb_mmap_get_range_str(&project->mmap, cbk_INCLUDE_DIR);
		cb_kv current;
		while (cb_mmap_range_get_next(&range, &current))
		{
			cb_dstr_append_v(&str, "/I", "\"");
			cb_dstr_append_strv(&str, current.u.strv);

			cb_dstr_append_v(&str, "\"", _);
		}
	}
	
	/* Append  preprocessor definition */
	{
		cb_kv_range range = cb_mmap_get_range_str(&project->mmap, cbk_DEFINES);
		cb_kv current;
		while (cb_mmap_range_get_next(&range, &current))
		{
			cb_dstr_append_v(&str, "/D", "\"");
			cb_dstr_append_strv(&str, current.u.strv);

			cb_dstr_append_v(&str, "\"", _);
		}
	}

	cb_file_it it;
	for (int i = 0; i < project->file_commands.darr.size; ++i)
	{
		cb_file_command cmd = cb_darrT_at(&project->file_commands, i);

		if (cmd.glob)
		{
			cb_file_it_init(&it, ".");

			while(cb_file_it_get_next_glob(&it, cmd.pattern))
			{
				cb_dstr_append_v(&str, cb_file_it_current_file(&it), _);

				cb_strv current_strv = cb_strv_make_str(cb_file_it_current_file(&it));
				cb_strv basename = cb_path_basename(current_strv);

				cb_dstr_append_strv(&str_obj, basename);
				cb_dstr_append_str(&str_obj, ".obj");
				cb_dstr_append_str(&str_obj, _);
			}

			cb_file_it_destroy(&it);
		}
		else
		{
			CB_ASSERT("Not Implemented Yet");
		}
	}

	/* for each linked project we add the link information to the cl.exe command */
	cb_kv_range range = cb_mmap_get_range_str(&project->mmap, cbk_LINK_PROJECT);
	if (range.count > 0)
	{
		cb_dstr linked_output_dir; /* to keep track of the .obj generated */
		cb_dstr_init(&linked_output_dir);

		cb_dstr_append_v(&str, "/link", _);
		cb_kv current;
		while (cb_mmap_range_get_next(&range, &current))
		{
			cb_strv linked_project_name = current.u.strv;
			cb_dstr_clear(&linked_output_dir);

			cb_project_t* linked_project = cb_find_project_by_name(linked_project_name);
			if (!project)
			{
				cb_log_warning("linked_project '%.*s' not found \n", linked_project_name.size, linked_project_name.data);
				continue;
			}

			CB_ASSERT(linked_project);

			cb_bool is_shared_libary = cb_property_equals(linked_project, cbk_BINARY_TYPE, cbk_shared_lib);
			cb_bool is_static_libary = cb_property_equals(linked_project, cbk_BINARY_TYPE, cbk_static_lib);

			if (is_static_libary || is_shared_libary)
			{
				cb_dstr_add_output_path(&linked_output_dir, linked_project, tc->default_directory_base);

				cb_dstr_append_v(&str, "/LIBPATH:", linked_output_dir.data, _);
				cb_dstr_append_strv(&str, linked_project_name);
				cb_dstr_append_v(&str, ".lib", _);
			}

			if (is_shared_libary)
			{
				cb_copy_directory(linked_output_dir.data, str_ouput_path.data);
			}
		}

		cb_dstr_destroy(&linked_output_dir);
	}

	/* execute cl.exe */
	if (!cb_subprocess(str.data))
	{
		return cb_false;
	}
	
	if (is_static_library)
	{
		cb_dstr_clear(&str);
		cb_dstr_append_v(&str,
			"lib.exe", _,
			"/OUT:", str_ouput_path.data, project_name, ext, _,
			"/LIBPATH:", str_ouput_path.data, _, str_obj.data); /* @TODO add space here? */
	
		if (!cb_subprocess(str.data))
		{
			cb_log_error("Could not execute command to build static library\n");
			return cb_false;
		}
	}

	cb_dstr_destroy(&str);
	cb_dstr_destroy(&str_obj);
	cb_dstr_destroy(&str_ouput_path);

	return cb_true;
}

RE_CB_API cb_toolchain_t
cb_toolchain_msvc()
{
	cb_toolchain_t tc;
	tc.bake = cb_toolchain_msvc_bake;
	tc.name = "msvc";
	tc.default_directory_base = ".cb\\msvc";
	return tc;
}

#else

/* ================================================================ */
/* toolchain GCC */
/* ================================================================ */

/* #gcc #toolchain */

static cb_bool
cb_strv_ends_with(cb_strv sv, cb_strv rhs)
{
	if (sv.size < rhs.size)
	{
		return (cb_bool)(0);
	}

	cb_strv sub = cb_strv_make(sv.data + (sv.size - rhs.size), rhs.size);
	return cb_strv_equals_strv(sub, rhs);
}

static cb_bool
is_created_by_gcc(cb_strv file)
{
	static cb_strv o_ext = { ".o" , 2 };
	static cb_strv so_ext = { ".so" , 3 };
	static cb_strv a_ext = { ".a" , 3 };
	return cb_strv_ends_with(file, o_ext)
		|| cb_strv_ends_with(file, so_ext)
		|| cb_strv_ends_with(file, a_ext);
}

RE_CB_API cb_bool
cb_toolchain_gcc_bake(cb_toolchain_t* tc, const char* project_name)
{
	cb_project_t* project = cb_find_project_by_name_str(project_name);

	if (!project)
	{
		cb_log_error("Project not found '%s'\n", project_name);
		return cb_false;
	}

	const char* _ = "  "; /* Space to separate command arguments */
	/* gcc command */
	cb_dstr str;
	cb_dstr_init(&str);

	cb_dstr str_obj; /* to keep track of the .o generated */
	cb_dstr_init(&str_obj);

	/* Output path - contains the directory path or binary file path (it just depends on how deep we are) */
	cb_dstr str_ouput_path;
	cb_dstr_init(&str_ouput_path);

	/* Format output directory */
	cb_dstr_add_output_path(&str_ouput_path, project, tc->default_directory_base);

	cb_create_directory_recursively(str_ouput_path.data, str_ouput_path.size);
	cb_dstr_append_v(&str, "cc ", _);

	/* Handle binary type */
	cb_bool is_exe = cb_property_equals(project, cbk_BINARY_TYPE, cbk_exe);
	cb_bool is_shared_library = cb_property_equals(project, cbk_BINARY_TYPE, cbk_shared_lib);
	cb_bool is_static_library = cb_property_equals(project, cbk_BINARY_TYPE, cbk_static_lib);

	CB_ASSERT(is_exe || is_shared_library || is_static_library && "Unknown library type");

	const char* ext = "";
	ext = is_exe ? "" : ext;
	ext = is_static_library ? ".a" : ext;
	ext = is_shared_library ? ".so" : ext;

	/* Append include directories */
	{
		cb_kv_range range = cb_mmap_get_range_str(&project->mmap, cbk_INCLUDE_DIR);
		cb_kv current;
		while (cb_mmap_range_get_next(&range, &current))
		{
			cb_dstr_append_v(&str, "-I", _, "\"");
			cb_dstr_append_strv(&str, current.u.strv);

			cb_dstr_append_v(&str, "\"", _);
		}
	}

	/* Append preprocessor definition */
	{
		cb_kv_range range = cb_mmap_get_range_str(&project->mmap, cbk_DEFINES);
		cb_kv current;
		while (cb_mmap_range_get_next(&range, &current))
		{
			cb_dstr_append_v(&str, "-D");
			cb_dstr_append_strv(&str, current.u.strv);

			cb_dstr_append_v(&str, _);
		}
	}

	if (is_static_library)
	{
		cb_dstr_append_str(&str, "-c ");
	}

	if (is_shared_library)
	{
		cb_dstr_append_str(&str, "-shared ");
		cb_dstr_append_f(&str, "-o lib%s%s ", project_name, ext);
	}

	if (is_exe)
	{
		cb_dstr_append_f(&str, "-o %s ", project_name);
	}

	cb_file_it it;
	for (int i = 0; i < project->file_commands.darr.size; ++i)
	{
		cb_file_command cmd = cb_darrT_at(&project->file_commands, i);

		if (cmd.glob)
		{
			cb_file_it_init(&it, ".");

			while (cb_file_it_get_next_glob(&it, cmd.pattern))
			{
				/* add .c files */
				cb_dstr_append_v(&str, cb_file_it_current_file(&it), _);

				cb_strv current_strv = cb_strv_make_str(cb_file_it_current_file(&it));
				cb_strv basename = cb_path_basename(current_strv);

				cb_dstr_append_str(&str_obj, str_ouput_path.data);
				cb_dstr_append_strv(&str_obj, basename);
				cb_dstr_append_str(&str_obj, ".o");
				cb_dstr_append_str(&str_obj, _);
			}

			cb_file_it_destroy(&it);
		}
		else
		{
			CB_ASSERT("Not Implemented Yet");
		}
	}

	/* for each linked project we add the link information to the gcc command */
	cb_kv_range range = cb_mmap_get_range_str(&project->mmap, cbk_LINK_PROJECT);
	if (range.count > 0)
	{
		/* Give some parameters to the linker to  look for the shared library next to the binary being built */
		cb_dstr_append_str(&str, " -Wl,-rpath,$ORIGIN ");

		cb_dstr linked_output_dir; /* to keep track of the .obj generated and copy them*/
		cb_dstr_init(&linked_output_dir);

		cb_kv current;
		while (cb_mmap_range_get_next(&range, &current))
		{
			cb_strv linked_project_name = current.u.strv;
			cb_dstr_clear(&linked_output_dir);

			cb_project_t* linked_project = cb_find_project_by_name(linked_project_name);
			if (!project)
			{
				cb_log_warning("linked_project '%.*s' not found \n", linked_project_name.size, linked_project_name.data);
				continue;
			}

			CB_ASSERT(linked_project);

			cb_bool linked_project_is_shared_libary = cb_property_equals(linked_project, cbk_BINARY_TYPE, cbk_shared_lib);
			cb_bool linked_project_is_static_libary = cb_property_equals(linked_project, cbk_BINARY_TYPE, cbk_static_lib);

			if (linked_project_is_static_libary || linked_project_is_shared_libary)
			{
				cb_dstr_add_output_path(&linked_output_dir, linked_project, tc->default_directory_base);

				cb_dstr_append_v(&str, "-L", linked_output_dir.data, _);
				cb_dstr_append_f(&str, "-l%.*s ", linked_project_name.size, linked_project_name.data);
			}

			if (linked_project_is_shared_libary)
			{
				cb_copy_directory(linked_output_dir.data, str_ouput_path.data);
			}
		}

		cb_dstr_destroy(&linked_output_dir);
	}

	/* Example: gcc <includes> -c  <c source files> */
	if (!cb_subprocess(str.data))
	{
		return cb_false;
	}

	if (is_exe && cb_path_exists(project_name))
	{
		/* set executable permission - only the ownder can read/write/execute the binary */
		char mode_str[] = "0777";
		int mode = strtol(mode_str, 0, 8);

		if (chmod(project_name, mode) < 0)
		{
			cb_log_error("Could not give executable permission to '%s'.", project_name);
			return cb_false;
		}
		/* move executable to the output directory */
		cb_dstr_assign_f(&str, "%s%s", str_ouput_path.data, project_name);
		cb_move_file(project_name, str.data);
	}

	if (is_static_library || is_shared_library)
	{
		/* Move all generated file in the output directory */
		if (!cb_move_files("./", str_ouput_path.data, is_created_by_gcc))
		{
			cb_log_error("Could not move files from './' to '%s'.", str_ouput_path.data);
			return cb_false;
		}

		if (is_static_library)
		{
			/* Create libXXX.a in the output directory */
			/* Example: ar -crs libMyLib.a MyObjectAo MyObjectB.o */
			cb_dstr_assign_f(&str, "ar -crs %slib%s%s %s", str_ouput_path.data, project_name, ext, str_obj.data);
			if (!cb_subprocess(str.data))
			{
				return cb_false;
			}
			cb_log_important("Created static library: %slib%s%s", str_ouput_path.data, project_name, ext);
		}
		else
		{
			cb_log_important("Created shared library: %slib%s%s", str_ouput_path.data, project_name, ext);
		}
	}

	cb_dstr_destroy(&str);

	cb_dstr_destroy(&str_obj);

	cb_dstr_destroy(&str_ouput_path);

	return cb_true;
}

RE_CB_API cb_toolchain_t
cb_toolchain_gcc()
{
	cb_toolchain_t tc;
	tc.bake = cb_toolchain_gcc_bake;
	tc.name = "gcc";
	tc.default_directory_base = ".cb/gcc";
	return tc;
}

#endif /* #else of _WIN32 */

RE_CB_API cb_toolchain_t
cb_toolchain_default()
{
#ifdef _WIN32
	return cb_toolchain_msvc();
#else
	return cb_toolchain_gcc();
#endif
}

#endif /* RE_CB_IMPLEMENTATION_CPP  */
#endif /* RE_CB_IMPLEMENTATION */