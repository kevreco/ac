#ifndef RE_CB_H
#define RE_CB_H

#define CB_IMPLEMENTATION

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h> /* va_start, va_end */

#if _WIN32
	#if !defined _CRT_SECURE_NO_WARNINGS
		#define _CRT_SECURE_NO_WARNINGS
	#endif
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
	#include <io.h> /* _get_oshandle */
	#define CB_THREAD  __declspec( thread )
#else
	#include <unistd.h>   /* open, close, access */
    #include <sys/stat.h> /* mkdir */
	#include <fcntl.h>    /* O_RDONLY etc. */
	#include <errno.h>
	#include <sys/sendfile.h> /* sendfile */
	#include <sys/wait.h>     /* waitpid */
	#include <dirent.h>       /* opendir */

	#define CB_THREAD __thread
#endif

/* Suppress some MSVC warnings. */
#ifdef _MSC_VER
/* byte padding */
#pragma warning(disable:4820)
/* Spectre mitigation */
#pragma warning(disable:5045)
#endif

/* in c89 va_copy does not exist */
#if defined(__GNUC__) || defined(__clang__)
#ifndef va_copy
#define va_copy(dest, src) (__builtin_va_copy(dest, src))
#endif
#endif

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)
#ifndef CB_C99_OR_LATER
#define CB_C99_OR_LATER
#endif
#endif

#ifndef CB_API
#define CB_API
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

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned int cb_id; /* hashed key, must be unsigned */
typedef unsigned int cb_bool;
typedef size_t cb_size;

/* All various typedef since c89 does not allow typedef redefinition */
typedef struct cb_toolchain cb_toolchain;
typedef struct cb_project_t cb_project_t;
typedef struct cb_strv cb_strv;
typedef struct cb_darr cb_darr;
typedef struct cb_kv cb_kv;
typedef struct cb_context cb_context;
typedef struct cb_process_handle cb_process_handle;

CB_API void cb_init(void);
CB_API void cb_destroy(void);

/* Delete projects */
CB_API void cb_clear(void);

/* Set or create current project.  */
CB_API cb_project_t* cb_project(const char* name); 

/* Wrapper around cb_project with string formatting. */
CB_API cb_project_t* cb_project_f(const char* format, ...);

/* Add value for the specific key. */
CB_API void cb_add(const char* key, const char* value);

/* Wrapper around cb_set with string formatting. */
CB_API void cb_add_f(const char* key, const char* format, ...);

/* Add multiple string values. The last value must be a null value. */
CB_API void cb_add_many_vnull(const char* key, ...);

/* Add multiple values using var args macro. */
#ifdef CB_C99_OR_LATER
#define cb_add_many_v(key, ...) \
	cb_add_many(key \
    , (const char* []) { __VA_ARGS__ } \
	, (sizeof((const char* []) { __VA_ARGS__ }) / sizeof(const char*)))
#endif

/* Remove all previous values according to the key and set the new one. */
CB_API void cb_set(const char* key, const char* value);

/* Wrapper around cb_set with string formatting. */
CB_API void cb_set_f(const char* key, const char* format, ...);

/* Remove all values associated with the key. Returns number of removed values. */
CB_API cb_size cb_remove_all(const char* key);

/* Wrapper around cb_remove_all with string formatting. */
CB_API cb_size cb_remove_all_f(const char* format, ...);

/* Remove item with the exact key and value. */
CB_API cb_bool cb_remove_one(const char* key, const char* value);

/* Wrapper around cb_remove_one with string formatting. */
CB_API cb_bool cb_remove_one_f(const char* key, const char* format, ...);

/* Check if key/value already exists in the current project. */
CB_API cb_bool cb_contains(const char* key, const char* value);

/* Returns the name of the result, which could be the path of a library or any other value depending on the toolchain. */
typedef const char* (*cb_toolchain_bake_t)(cb_toolchain* tc, const char*);

struct cb_toolchain {
	cb_toolchain_bake_t bake;
	/* Name of the toolchain, mostly for debugging purpose. */
	const char* name;
	/* Name of the default directory. */
	const char* default_directory_base;
};

/* Process the current project using the default toolchain.
   Returns the name of the result, which could be the path of a library or any other value depending on the toolchain.
   Use the default toolchain (gcc or msvc).
*/
CB_API const char* cb_bake(void);

/* Same as cb_bake. Using an explicit projectname. */
CB_API const char* cb_bake_project(const char* project_name);

/* Same as cb_bake. Take an explicit toolchain instead of using the default one. */
CB_API const char* cb_bake_project_with(cb_toolchain toolchain, const char* project_name);

/* Run executable path. Path is double quoted before being run, in case path contains some space.
   Returns exit code. Returns -1 if command could not be executed.
*/
CB_API int cb_run(const char* cmd);

/* Turn on/off debug messages */
CB_API void cb_debug(cb_bool value);

CB_API cb_toolchain cb_toolchain_default(void);

/* Run command and returns exit code. */
CB_API int cb_process(const char* cmd);

/* Same as cb_process but provide a starting directory. */
CB_API int cb_process_in_directory(const char* cmd, const char* directory);

/* Start command and wait for the process to end.
   stdout of the child process is printed into a buffer and accessible from cb_process_stdout_string(handle),
   if 'also_get_stderr' is true the stderr is printed to a buffer as well and is accessed from cb_process_stderr_string(handle).
   cb_process_end(handle) needs to be called to cleanup various resources.
   cb_process_stdout_string(handle) or cb_process_stderr_string(handle) are not accessible after cb_process_end(handle). */
CB_API cb_process_handle* cb_process_to_string(const char* cmd, const char* starting_directory, cb_bool also_get_stderr);

/* Get c string content of stdout of the child process after cb_process_to_string has been called. */
CB_API const char* cb_process_stdout_string(cb_process_handle* handle);

/* Get c string content of stderr of the child process after cb_process_to_string has been called. */
CB_API const char* cb_process_stderr_string(cb_process_handle* handle);

/* Returns exit code of the process and clean up resources. */
CB_API int cb_process_end(cb_process_handle* handle);

/* Commonly used properties (basically to make it discoverable with auto completion and avoid misspelling) */

/* keys */
extern const char* cb_BINARY_TYPE;         /* Exe, shared_lib or static_lib. */
extern const char* cb_CXFLAGS;             /* Extra flags to give to the C/C++ compiler. */
extern const char* cb_DEFINES;             /* Define preprocessing symbol. */
extern const char* cb_FILES;               /* Files to consume (could be .c, .cpp, etc.). */
extern const char* cb_INCLUDE_DIRECTORIES; /* Include directories. */
extern const char* cb_LINK_PROJECTS;       /* Other projects to link. */
extern const char* cb_LIBRARIES;           /* Libraries to link with. */
extern const char* cb_LFLAGS;              /* Extra flags to give to the linker. */
extern const char* cb_OUTPUT_DIR;          /* Ouput directory for the generated files. */
extern const char* cb_TARGET_NAME;         /* Name (basename) of the main generated file (.exe, .a, .lib, .dll, etc.). */
/* values */
extern const char* cb_EXE;                 /* cb_BINARY_TYPE value */
extern const char* cb_SHARED_LIBRARY;      /* cb_BINARY_TYPE value */
extern const char* cb_STATIC_LIBRARY;      /* cb_BINARY_TYPE value */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* RE_CB_H */

#ifdef CB_IMPLEMENTATION
#ifndef CB_IMPLEMENTATION_CPP
#define CB_IMPLEMENTATION_CPP

#ifdef _WIN32
#define CB_DEFAULT_DIR_SEPARATOR_CHAR '\\'
#define CB_DEFAULT_DIR_SEPARATOR "\\"
#else
#define CB_DEFAULT_DIR_SEPARATOR_CHAR '/'
#define CB_DEFAULT_DIR_SEPARATOR "/"
#endif

#define CB_PREFERRED_DIR_SEPARATOR_CHAR '/'
#define CB_PREFERRED_DIR_SEPARATOR "/"
/* Table of content
*  ----------------------------------------------------------------
* 
* # Structures
*   - @TODO
* # Functions of internal structures
*   - cb_log     - write some logs
*   - cb_darr    - dynamic array
*   - cb_dstr    - dynamic string
*   - cb_kv      - key value for the multimap
*   - cb_mmap    - multimap containing key/value strings
* # Functions of the cb library
*   - cb_project(...) - @TODO explanation
*   - cb_set(...)     - @TODO explanation
*   - cb_add(...)     - @TODO explanation
*   - cb_bake(...)    - @TODO explanation
* # Toolchain
*   - msvc
*   - gcc
*/

/* keys */
const char* cb_BINARY_TYPE = "binary_type";
const char* cb_CXFLAGS = "cxflags";
const char* cb_DEFINES = "defines";
const char* cb_FILES = "files";
const char* cb_INCLUDE_DIRECTORIES = "include_directories";
const char* cb_LINK_PROJECTS = "link_projects";
const char* cb_LIBRARIES = "libraries";
const char* cb_LFLAGS = "lflags";
const char* cb_OUTPUT_DIR = "output_dir";
const char* cb_TARGET_NAME = "target_name";
const char* cb_WORKING_DIRECTORY = "working_directory";
/* values */
const char* cb_EXE = "exe";
const char* cb_SHARED_LIBRARY = "shared_library";
const char* cb_STATIC_LIBRARY = "static_library";

/* string view */
struct cb_strv {
	cb_size size;
	const char* data;
};

/* dynamic array
 * NOTE: cb_darr needs to start with the same component as cb_strv
 * because we need to compare them in the same way in cb_kv 
 */
struct cb_darr {
	cb_size size;
    char* data;
	cb_size capacity;
};

/* type safe dynamic array */
#define cb_darrT(type)        \
    union {                   \
        cb_darr base;         \
        struct  {             \
            cb_size size;     \
			type* data;       \
            cb_size capacity; \
        } darr;               \
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
        cb_size last__ = (a)->darr.size; \
        cb_darr_insert_one_space(&(a)->base, last__, sizeof(*(a)->darr.data)); \
		(a)->darr.data[last__] = value; \
	} while (0)

#define cb_darrT_at(a, index) \
    ((a)->darr.data[index])

#define cb_darrT_set(a, index, value) \
    (a)->darr.data[index] = (value)

#define cb_darrT_ptr(a, index) \
    (&(a)->darr.data[index])

#define cb_darrT_size(a) \
    ((a)->darr.size)

/* dynamic string */
typedef cb_darr cb_dstr;

/* key/value data used in the map and mmap struct */
struct cb_kv {
	cb_id hash;  /* hash of the key */
	cb_strv key; /* key */
	union {
		const void* ptr;
		cb_strv strv;
	} u; /* value */
};

/* multimap */
typedef cb_darrT(cb_kv) cb_mmap;

#define cb_rangeT(type) \
struct {                \
	type* begin;        \
	type* end;          \
	cb_size count;      \
}

typedef cb_rangeT(cb_kv) cb_kv_range;

struct cb_project_t {
	cb_strv name;
	/* @FIXME: rename this "props" or "properties". */
	cb_mmap mmap; /* multi map of strings - when you want to have multiple values per key */
};

/* context, the root which hold everything */
struct cb_context {
	cb_mmap projects;
	cb_project_t* current_project;
};

static cb_context default_ctx;
static cb_context* current_ctx;

/*-----------------------------------------------------------------------*/
/* utils */
/*-----------------------------------------------------------------------*/

static cb_bool cb_debug_enabled = cb_false;

#define cb_set_and_goto(result, value, goto_label) \
	do { \
		result = (value); \
		goto goto_label; \
	} while(0)

/*-----------------------------------------------------------------------*/
/* cb_log */
/*-----------------------------------------------------------------------*/

CB_INTERNAL void
cb_log_v(FILE* file, const char* prefix, const char* fmt, va_list args)
{
	va_list args_copy;
	va_copy(args_copy, args);

	fprintf(file, "%s", prefix);
	vfprintf(file, fmt, args_copy);
	fprintf(file, "\n");
}

CB_INTERNAL void
cb_log_error(const char* fmt, ...) { va_list args; va_start(args, fmt); cb_log_v(stderr, "[CB-ERROR] ", fmt, args); va_end(args); }
CB_INTERNAL void
cb_log_warning(const char* fmt, ...) { va_list args; va_start(args, fmt); cb_log_v(stderr, "[CB-WARNING] ", fmt, args); va_end(args); }
CB_INTERNAL void
cb_log_debug(const char* fmt, ...) { va_list args; va_start(args, fmt); if (cb_debug_enabled) { cb_log_v(stdout, "[CB-DEBUG] ", fmt, args); } va_end(args); }
CB_INTERNAL void
cb_log_important(const char* fmt, ...) { va_list args; va_start(args, fmt); cb_log_v(stdout, "", fmt, args); va_end(args); }

/*-----------------------------------------------------------------------*/
/* temporary allocation */
/*-----------------------------------------------------------------------*/

#ifndef CB_TMP_CAPACITY
#define CB_TMP_CAPACITY (8 * 1024 * 1024) /* Arbitrary size. Must be big enough. Can be increased if needed. */
#endif

static CB_THREAD cb_size cb_tmp_size = 0;
static CB_THREAD char cb_tmp_buffer[CB_TMP_CAPACITY] = { 0 };

CB_INTERNAL void*
cb_tmp_alloc(cb_size size)
{
	void* data = NULL;

	CB_ASSERT((cb_tmp_size + size <= CB_TMP_CAPACITY) && "Size of the temporary allocator is too small. Increase it if needed.");

	if (cb_tmp_size + size > CB_TMP_CAPACITY)
	{
		return NULL;
	}

	data = &cb_tmp_buffer[cb_tmp_size];
	cb_tmp_size += size;
	return data;
}

CB_INTERNAL void*
cb_tmp_calloc(cb_size size)
{
	void* data = cb_tmp_alloc(size);
	memset(data, 0, size);
	return data;
}

CB_INTERNAL void
cb_tmp_reset(void)
{
	cb_tmp_size = 0;
}

CB_INTERNAL cb_strv
cb_tmp_strv_vprintf(const char* format, va_list args)
{
	cb_strv sv;
	va_list args_copy;
	int n = 0;
	va_copy(args_copy, args);

	n = vsnprintf(NULL, 0, format, args);

	sv.data = cb_tmp_alloc(n + 1);

	vsnprintf((char*)sv.data, n + 1, format, args_copy);
	sv.size = n;
	return sv;
}

CB_INTERNAL const char*
cb_tmp_vsprintf(const char* format, va_list args)
{
	cb_strv sv;
	va_list args_copy;
	va_copy(args_copy, args);
	sv = cb_tmp_strv_vprintf(format, args_copy);
	return sv.data;
}

CB_INTERNAL const char*
cb_tmp_sprintf(const char* format, ...)
{
	va_list args;
	const char* data = NULL;
	va_start(args, format);
	data = cb_tmp_vsprintf(format, args);
	va_end(args);
	return data;
}

CB_INTERNAL const char*
cb_tmp_strv_to_str(cb_strv sv)
{
	char* data = cb_tmp_alloc(sv.size + 1);
	memcpy(data, sv.data, sv.size + 1);
	data[sv.size] = '\0';
	return data;
}

CB_INTERNAL cb_strv
cb_tmp_str_to_strv(const char* str)
{
	cb_strv sv;
	sv.size = strlen(str);
	sv.data = cb_tmp_alloc(sv.size + 1);
	memcpy((char*)sv.data, str, sv.size + 1);
	((char*)sv.data)[sv.size] = '\0';
	return sv;
}

CB_INTERNAL const char*
cb_tmp_str(const char* str)
{
	cb_strv sv = cb_tmp_str_to_strv(str);
	return sv.data;
}

CB_INTERNAL cb_size
cb_tmp_save(void)
{
	return cb_tmp_size;
}

CB_INTERNAL void
cb_tmp_restore(cb_size index)
{
	cb_tmp_size = index;
}

/* Check if pointer is contained in the tmp buffer */
CB_INTERNAL cb_bool
cb_tmp_contains(const void* ptr)
{
	return ptr >= (void*)cb_tmp_buffer
		&& ptr < (void*)(cb_tmp_buffer + CB_TMP_CAPACITY);
}

/*-----------------------------------------------------------------------*/
/* cb_darr - dynamic array */
/*-----------------------------------------------------------------------*/

CB_INTERNAL void
cb_darr_init(cb_darr* arr)
{
	arr->size = 0;
	arr->capacity = 0;
	arr->data = NULL;
}

CB_INTERNAL void
cb_darr_destroy(cb_darr* arr)
{
	if (arr->data != NULL)
	{
		arr->size = 0;
		arr->capacity = 0;
		CB_FREE(arr->data);
		arr->data = NULL;
	} 
}

CB_INTERNAL void*
cb_darr_ptr(const cb_darr* arr, cb_size index, cb_size sizeof_vlaue)
{
	CB_ASSERT(
		index < arr->size /* Within accessible item range */
		|| index == arr->size /* Also allow getting the item at the end */
	);

	return arr->data + (index * sizeof_vlaue);
}

CB_INTERNAL char* cb_darr_end(const cb_darr* arr, cb_size sizeof_value) { return arr->data + (arr->size * sizeof_value); }

CB_INTERNAL cb_size
cb_darr__get_new_capacity(const cb_darr* arr, cb_size count)
{
	cb_size new_capacity = arr->capacity ? (arr->capacity + arr->capacity / 2) : 8;
	return new_capacity > count ? new_capacity : count;
}

CB_INTERNAL void
cb_darr_reserve(cb_darr* arr, cb_size new_capacity, cb_size sizeof_value)
{
	char* new_data = NULL;

	CB_ASSERT(new_capacity > arr->capacity && "You should request more capacity, not less."); /* ideally we should ensure this before this call. */
	if (new_capacity <= arr->capacity)
	{
		return;
	}

	new_data = (char*)CB_MALLOC(new_capacity * sizeof_value);
	CB_ASSERT(new_data);
	if (arr->data != NULL) {
		memcpy(new_data, arr->data, arr->size * sizeof_value);
		CB_FREE(arr->data);
	}
	arr->data = new_data;
	arr->capacity = new_capacity;
}

CB_INTERNAL void
cb_darr__grow_if_needed(cb_darr* arr, cb_size needed, cb_size sizeof_value)
{
	if (needed > arr->capacity)
		cb_darr_reserve(arr, cb_darr__get_new_capacity(arr, needed), sizeof_value);
}

CB_INTERNAL void
cb_darr_insert_many_space(cb_darr* arr, cb_size index, cb_size count, cb_size sizeof_value)
{
	cb_size count_to_move;

	CB_ASSERT(arr != NULL);
	CB_ASSERT(index <= arr->size);

	count_to_move = arr->size - index;
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

CB_INTERNAL void
cb_darr_insert_one_space(cb_darr* arr, cb_size index, cb_size sizeof_value)
{
	cb_darr_insert_many_space(arr, index, 1, sizeof_value);
}

CB_INTERNAL void
cb_darr_insert_many(cb_darr* arr, cb_size index, const void* value, cb_size count, cb_size sizeof_value)
{
	cb_darr_insert_many_space(arr, index, count, sizeof_value);

	memcpy(cb_darr_ptr(arr, index, sizeof_value), value, count * sizeof_value);
}

CB_INTERNAL void
cb_darr_insert_one(cb_darr* arr, cb_size index, const void* value, cb_size sizeof_value)
{
	cb_darr_insert_many(arr, index, value, 1, sizeof_value);
}

CB_INTERNAL void
cb_darr_push_back_many(cb_darr* arr, const void* values_ptr, cb_size count, cb_size sizeof_value)
{
	cb_darr_insert_many(arr, arr->size, values_ptr, count, sizeof_value);
}

CB_INTERNAL void
cb_darr_remove_many(cb_darr* arr, cb_size index, cb_size count, cb_size sizeof_value)
{
	CB_ASSERT(arr);
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

CB_INTERNAL void
cb_darr_remove_one(cb_darr* arr, cb_size index, cb_size sizeof_value)
{
	cb_darr_remove_many(arr, index, 1, sizeof_value);
}

typedef cb_bool(*cb_predicate_t)(const void* left, const void* right);

CB_INTERNAL cb_size
cb_lower_bound_predicate(const void* void_ptr, cb_size left, cb_size right, const void* value, cb_size sizeof_value, cb_predicate_t pred)
{
	const char* ptr = (const char*)void_ptr;
	cb_size count = right - left;
	cb_size step;
	cb_size mid; /* index of the found value */

	CB_ASSERT(left <= right);

	while (count > 0) {
		step = count >> 1; /* count divide by two using bit shift */

		mid = left + step;

		if (pred(ptr + (mid * sizeof_value), value)) {
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
/* cb_strv - string view */
/*-----------------------------------------------------------------------*/

CB_INTERNAL cb_strv
cb_strv_make(const char* data, cb_size size)
{
	cb_strv s;
	s.data = data;
	s.size = size; 
	return s;
}

CB_INTERNAL cb_strv
cb_strv_make_str(const char* str)
{
	return cb_strv_make(str, strlen(str));
}

CB_INTERNAL int
cb_lexicagraphical_cmp(const char* left, cb_size left_count, const char* right, cb_size right_count)
{
	char left_ch, right_ch;
	cb_size min_size = left_count < right_count ? left_count : right_count;
	while (min_size-- > 0)
	{
		left_ch = *left++;
		right_ch = *right++;
		if (left_ch != right_ch)
			return left_ch < right_ch ? 1 : -1;
	};

	return left_count == right_count
		? 0
		: left_count > right_count ?  1 : -1;
}

CB_INTERNAL int cb_strv_compare(cb_strv sv, const char* data, cb_size size) { return cb_lexicagraphical_cmp(sv.data, sv.size, data, size); }
CB_INTERNAL int cb_strv_compare_strv(cb_strv sv, cb_strv other) { return cb_strv_compare(sv, other.data, other.size); }
CB_INTERNAL int cb_strv_compare_str(cb_strv sv, const char* str) { return cb_strv_compare(sv, str, strlen(str)); }
CB_INTERNAL cb_bool cb_strv_equals(cb_strv sv, const char* data, cb_size size) { return cb_strv_compare(sv, data, size) == 0; }
CB_INTERNAL cb_bool cb_strv_equals_strv(cb_strv sv, cb_strv other) { return cb_strv_compare_strv(sv, other) == 0; }
CB_INTERNAL cb_bool cb_strv_equals_str(cb_strv sv, const char* other) { return cb_strv_compare_strv(sv, cb_strv_make_str(other)) == 0; }

/*-----------------------------------------------------------------------*/
/* cb_str - c string utilities */
/*-----------------------------------------------------------------------*/

CB_INTERNAL cb_bool cb_str_equals(const char* left, const char* right) { return cb_strv_equals_strv(cb_strv_make_str(left), cb_strv_make_str(right)); }

/*-----------------------------------------------------------------------*/
/* cb_dstr - dynamic string */
/*-----------------------------------------------------------------------*/

CB_INTERNAL const char* cb_empty_string(void) { return "\0EMPTY_STRING"; }
CB_INTERNAL void cb_dstr_init(cb_dstr* dstr) { cb_darr_init(dstr); dstr->data = (char*)cb_empty_string(); }
CB_INTERNAL void cb_dstr_destroy(cb_dstr* dstr) { if (dstr->data == cb_empty_string()) { dstr->data = NULL; } cb_darr_destroy(dstr); }
/* does not free anything, just reset the size to 0 */
CB_INTERNAL void cb_dstr_clear(cb_dstr* dstr) { if (dstr->data != cb_empty_string()) { dstr->size = 0; dstr->data[dstr->size] = '\0'; } }

CB_INTERNAL void
cb_dstr_reserve(cb_dstr* s, cb_size new_string_capacity)
{
	cb_size new_mem_capacity = new_string_capacity + 1;
	char* new_data = NULL;

	CB_ASSERT(new_string_capacity > s->capacity && "You should request more capacity, not less."); /* ideally we should ensure this before this call. */
	if (new_string_capacity <= s->capacity) { return; }

	new_data = (char*)CB_MALLOC(new_mem_capacity * sizeof(char));
	CB_ASSERT(new_data);
	if (!new_data) { return; }

	if (s->size)
	{
		memcpy(new_data, s->data, (s->size + 1) * sizeof(char)); /* +1 is for null-terminated string char */
		CB_FREE(s->data);
	}

	s->data = new_data;
	s->capacity = new_string_capacity;
}

CB_INTERNAL void
cb_dstr__grow_if_needed(cb_dstr* s, cb_size needed)
{
	if (needed > s->capacity) { 
		cb_dstr_reserve(s, cb_darr__get_new_capacity(s, needed));
	}
}

CB_INTERNAL void
cb_dstr_append_from(cb_dstr* s, cb_size index, const char* data, cb_size size)
{
	cb_dstr__grow_if_needed(s, index + size);

	memcpy(s->data + index, (const void*)data, ((size) * sizeof(char)));
	s->size = index + size;
	s->data[s->size] = '\0';
}

CB_INTERNAL cb_size
cb_dstr_append_from_fv(cb_dstr* s, cb_size index, const char* fmt, va_list args)
{
	va_list args_copy;
	int add_len = 0;
	va_copy(args_copy, args);

	/* Caluclate necessary len */
	add_len = vsnprintf(NULL, 0, fmt, args_copy);
	CB_ASSERT(add_len >= 0);

	cb_dstr__grow_if_needed(s, s->size + add_len);

	add_len = vsnprintf(s->data + index, add_len + 1, fmt, args);

	s->size = index + add_len;
	return add_len;
}

CB_INTERNAL void
cb_dstr_assign(cb_dstr* s, const char* data, cb_size size)
{
	cb_dstr_append_from(s, 0, data, size);
}

CB_INTERNAL void
cb_dstr_assign_str(cb_dstr* s, const char* str)
{
	cb_dstr_assign(s, str, strlen(str));
}

CB_INTERNAL void
cb_dstr_assign_f(cb_dstr* s, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	cb_dstr_append_from_fv(s, 0, fmt, args);
	va_end(args);
}

CB_INTERNAL void
cb_dstr_append_str(cb_dstr* s, const char* str)
{
	cb_dstr_append_from(s, s->size, str, strlen(str));
}

CB_INTERNAL void
cb_dstr_append_strv(cb_dstr* s, cb_strv sv)
{
	cb_dstr_append_from(s, s->size, sv.data, sv.size);
}

CB_INTERNAL cb_size
cb_dstr_append_f(cb_dstr* s, const char* fmt, ...)
{
	va_list args;
	cb_size len = 0;

	va_start(args, fmt);
	len = cb_dstr_append_from_fv(s, s->size, fmt, args);
	va_end(args);
	return len;
}

/*-----------------------------------------------------------------------*/
/* cb_hash */
/*-----------------------------------------------------------------------*/

CB_INTERNAL cb_id
djb2_strv(const char* str, cb_size count)
{
	cb_id hash = 5381;
	cb_size i = 0;
	while (i < count)
	{
		hash = ((hash << 5) + hash) + str[i]; /* hash * 33 + c */
		i++;
	}

	return hash;
}

CB_INTERNAL cb_id
cb_hash_strv(cb_strv sv)
{
	return djb2_strv(sv.data, sv.size);
}

/*-----------------------------------------------------------------------*/
/* cb_kv */
/*-----------------------------------------------------------------------*/

CB_INTERNAL void
cb_kv_init(cb_kv* kv, cb_strv sv)
{
	memset(kv, 0, sizeof(cb_kv));
	kv->hash = cb_hash_strv(sv);
	kv->key = sv;
}

CB_INTERNAL cb_kv
cb_kv_make_with_strv(cb_strv sv, cb_strv value)
{
	cb_kv kv;
	cb_kv_init(&kv, sv);
	kv.u.strv = value;
	return kv;
}

CB_INTERNAL cb_kv
cb_kv_make_with_str(cb_strv sv, const char* value)
{
	return cb_kv_make_with_strv(sv, cb_strv_make_str(value));
}

CB_INTERNAL cb_kv
cb_kv_make_with_ptr(cb_strv sv, const void* ptr)
{
	cb_kv kv;
	cb_kv_init(&kv, sv);
	kv.u.ptr = ptr;
	return kv;
}

CB_INTERNAL int
cb_kv_comp(const cb_kv* left, const cb_kv* right)
{
	return left->hash != right->hash
		? (right->hash < left->hash ? -1 : 1)
		: cb_strv_compare_strv(left->key, right->key);
}

CB_INTERNAL cb_bool
cb_kv_less(const cb_kv* left, const cb_kv* right)
{
	return cb_kv_comp(left, right) < 0;
}

/*-----------------------------------------------------------------------*/
/* cb_mmap - a multimap */
/*-----------------------------------------------------------------------*/

CB_INTERNAL void cb_mmap_init(cb_mmap* m) { cb_darrT_init(m); }
CB_INTERNAL void cb_mmap_destroy(cb_mmap* m) { cb_darrT_destroy(m); }

CB_INTERNAL cb_size
cb_mmap_lower_bound_predicate(const cb_mmap* m, const cb_kv* value)
{
	return cb_lower_bound_predicate(m->base.data, 0, m->base.size, value, sizeof(cb_kv), (cb_predicate_t)cb_kv_less);
}

CB_INTERNAL void
cb_mmap_insert(cb_mmap* m, cb_kv kv)
{
	cb_size index = cb_mmap_lower_bound_predicate(m, &kv);

	cb_darrT_insert(m, index, kv);
}

CB_INTERNAL cb_size
cb_mmap_find(const cb_mmap* m, const cb_kv* kv)
{
	cb_size index = cb_mmap_lower_bound_predicate(m, kv);

	if (index == m->base.size || cb_kv_less(kv, cb_darrT_ptr(m, index)))
	{
		index = m->base.size; /* not found */
	}

	return index;
}

CB_INTERNAL cb_bool
cb_mmap_try_get_first(const cb_mmap* m, cb_strv key, cb_kv* kv)
{
	cb_kv key_item = cb_kv_make_with_str(key, "");
	cb_size index = cb_mmap_find(m, &key_item);

	if (index != m->darr.size) /* found */
	{
		*kv = cb_darrT_at(m, index);
		return cb_true;
	}

	return cb_false;
}

CB_INTERNAL cb_kv_range
cb_mmap_get_range_all(const cb_mmap* m)
{
	cb_kv_range range;
	range.begin = m->darr.data;
	range.end = m->darr.data + m->darr.size;
	range.count = m->darr.size;
	return range;
}

CB_INTERNAL cb_kv_range
cb_mmap_get_range(const cb_mmap* m, cb_strv key)
{
	cb_size starting_index;
	cb_kv_range result = { 0, 0, 0 };

	cb_kv key_item = cb_kv_make_with_str(key, "");

	cb_size index = cb_mmap_find(m, &key_item);

	/* No item found */
	if (index == m->darr.size)
	{
		return result;
	}
	
	starting_index = index;

	/* An item has been found */

	result.begin = cb_darrT_ptr(m, index);

	/* Check for other items */
	do
	{
		index += 1;
	} while (index < m->base.size
		&& cb_kv_comp(cb_darrT_ptr(m, index), &key_item) == 0);

	result.end = cb_darrT_ptr(m, index);
	result.count = index - starting_index;
	return result;
}

CB_INTERNAL cb_kv_range
cb_mmap_get_range_str(const cb_mmap* m, const char* key)
{
	return cb_mmap_get_range(m, cb_strv_make_str(key));
}

CB_INTERNAL cb_bool
cb_mmap_range_get_next(cb_kv_range* range, cb_kv* next)
{
	memset(next, 0, sizeof(cb_kv));

	CB_ASSERT(range->begin <= range->end);

	if (range->begin < range->end)
	{
		*next = *range->begin;

		range->begin += 1;
		return cb_true;
	}

	return cb_false;
}

/* Remove all values found in keys, if the value was a dynamic string the dynamic string is destroyed */
CB_INTERNAL cb_size
cb_mmap_remove(cb_mmap* m, cb_kv kv)
{
	cb_kv_range range = cb_mmap_get_range(m, kv.key);

	cb_size count_to_remove = range.count;

	while (range.begin < range.end)
	{
		range.end -= 1;
		/* Remove item from the tail. */
		cb_darrT_remove(m, range.end - m->darr.data);
	}
	
	return count_to_remove;
}

CB_INTERNAL cb_bool
cb_mmap_get_from_kv(cb_mmap* map, const cb_kv* item, cb_kv* result)
{
	cb_size index = cb_mmap_find(map, item);
	if (index != map->base.size)
	{
		*result = cb_darrT_at(map, index);
		return cb_true;
	}

	return cb_false;
}

CB_INTERNAL void
cb_mmap_insert_ptr(cb_mmap* map, cb_strv key, const void* value_ptr)
{
	cb_mmap_insert(map, cb_kv_make_with_ptr(key, value_ptr));
}

CB_INTERNAL const void*
cb_mmap_get_ptr(cb_mmap* map, cb_strv key, const void* default_value)
{
	cb_kv key_item;
	cb_kv result;

	cb_kv_init(&key_item, key);

	return cb_mmap_get_from_kv(map, &key_item, &result) ? result.u.ptr : default_value;
}

CB_INTERNAL cb_strv
cb_mmap_get_strv(cb_mmap* map, cb_strv key, cb_strv default_value)
{
	cb_kv key_item;
	cb_kv result;

	cb_kv_init(&key_item, key);

	return cb_mmap_get_from_kv(map, &key_item, &result) ? result.u.strv : default_value;
}

CB_INTERNAL void
cb_context_init(cb_context* ctx)
{
	memset(ctx, 0, sizeof(cb_context));
	cb_mmap_init(&ctx->projects);
	ctx->current_project = NULL;
}

CB_INTERNAL void
cb_context_destroy(cb_context* ctx)
{
	cb_mmap_destroy(&ctx->projects);
	cb_context_init(ctx);
}

CB_INTERNAL void cb_project_destroy(cb_project_t* project);

CB_INTERNAL void
cb_context_clear(cb_context* ctx)
{
	cb_size i = 0;
	cb_size size = cb_darrT_size(&ctx->projects);
	cb_kv kv = { 0 };
	cb_project_t* p = NULL;

	for (; i < size; ++i)
	{
		kv = cb_darrT_at(&ctx->projects, i);
		p = (cb_project_t*)kv.u.ptr;
		cb_project_destroy(p);
	}
	
	ctx->projects.darr.size = 0;

	ctx->current_project = NULL;
}

CB_INTERNAL cb_context*
cb_current_context(void)
{
	CB_ASSERT(current_ctx);
	return current_ctx;
}

CB_INTERNAL cb_project_t*
cb_current_project(void)
{
	cb_context* ctx = cb_current_context();
	CB_ASSERT(ctx->current_project);
	if (!ctx) { return NULL; }
	return ctx->current_project;
}

CB_INTERNAL cb_bool
cb_try_find_project_by_name(cb_strv sv, cb_project_t** project)
{
	void* default_value = NULL;
	*project = (cb_project_t*)cb_mmap_get_ptr(&cb_current_context()->projects, sv, default_value);
	return (cb_bool)(*project != NULL);
}

CB_INTERNAL cb_project_t*
cb_find_project_by_name(cb_strv sv)
{
	cb_project_t* project = 0;
	if (!cb_try_find_project_by_name(sv, &project))
	{
		cb_log_error("Project not found '%.*s'", sv.size, sv.data);
		return NULL;
	}

	return project;
}

CB_INTERNAL cb_project_t* cb_find_project_by_name_str(const char* name) { return cb_find_project_by_name(cb_strv_make_str(name)); }
CB_INTERNAL cb_bool cb_try_find_project_by_name_str(const char* name, cb_project_t** project) { return cb_try_find_project_by_name(cb_strv_make_str(name), project); }

CB_INTERNAL void
cb_project_init(cb_project_t* project, cb_strv name)
{
	memset(project, 0, sizeof(cb_project_t));

	cb_mmap_init(&project->mmap);
	project->name = name;
}

CB_INTERNAL void
cb_project_destroy(cb_project_t* project)
{
	CB_ASSERT(project);
	cb_mmap_destroy(&project->mmap);
}

CB_INTERNAL cb_project_t*
cb_create_project(const char* name)
{
	cb_strv name_sv = cb_strv_make_str(name);

	cb_project_t* project = (cb_project_t*)CB_MALLOC(sizeof(cb_project_t));
	CB_ASSERT(project);
	if (!project) { return NULL; }

	cb_project_init(project, cb_tmp_str_to_strv(name));

	cb_mmap_insert_ptr(&cb_current_context()->projects, name_sv, project);
	
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

/* Convert utf-8 string to utf-16 string. String is allocated using the temp buffer */
CB_INTERNAL wchar_t*
cb_utf8_to_utf16(const char* str)
{
	/* Calculate len */
	int length = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);
	/* Allocate enough space */
	wchar_t* data = (wchar_t*)cb_tmp_alloc(length * sizeof(wchar_t));
	/* Write data */
	MultiByteToWideChar(CP_UTF8, 0, str, -1, data, length);
	/* Place null-terminated char to end the string */
	data[length] = L'\0';
	return data;
}

#endif

/* #file utils */

CB_INTERNAL cb_bool cb_is_directory_separator(char c) { return (c == '/' || c == '\\'); }

#define CB_NPOS ((cb_size)-1)

CB_INTERNAL cb_size
cb_rfind(cb_strv s, char c)
{
	const char* begin = NULL;
	const char* end = NULL;
	if (s.size == 0) return CB_NPOS;

	begin = s.data;
	end = s.data + s.size - 1;
	while (end >= begin && *end != c)
	{
		end--;
	}
	return end < begin ? CB_NPOS : (cb_size)(end - begin);
}

CB_INTERNAL cb_size
cb_rfind2(cb_strv s, char c1, char c2)
{
	const char* begin = NULL;
	const char* end = NULL;
	if (s.size == 0) return CB_NPOS;

	begin = s.data;
	end = s.data + s.size - 1;
	while (end >= begin && *end != c1 && *end != c2)
	{
		end--;
	}
	return end < begin ? CB_NPOS : (cb_size)(end - begin);
}

CB_INTERNAL cb_strv
cb_path_filename(cb_strv path)
{
	cb_size pos = cb_rfind2(path, '/', '\\');
	if (pos != CB_NPOS && pos > 0) {
		return cb_strv_make(path.data + pos + 1 /* plus one because we don't want the slash char */
			, path.size - pos);
	}
	return path;
}
CB_INTERNAL cb_strv
cb_path_filename_str(const char* str)
{
	return cb_path_filename(cb_strv_make_str(str));
}

CB_INTERNAL cb_strv
cb_path_basename(cb_strv s)
{
	cb_strv filename = cb_path_filename(s);

	if (!cb_strv_equals_str(filename, ".")
		&& !cb_strv_equals_str(filename, "..")) {
		cb_size pos = cb_rfind(filename, '.');
		if (pos != CB_NPOS && pos > 0) {
			return cb_strv_make(filename.data, pos);
		}
	}
	return filename;
}

CB_INTERNAL cb_strv
cb_path_basename_str(const char* str)
{
	return cb_path_basename(cb_strv_make_str(str));
}
/*  #file_iterator */

#define CB_MAX_PATH 1024 /* this is an arbitrary limit */

#define cb_str_append_from(dst, src, index, max) cb_str_append_from_core(dst, src, index, max, __FILE__, __LINE__)

/* Returns the number of character writter. Null-terminating char is not counted. */
CB_INTERNAL cb_size
cb_str_append_from_core(char* dst, const char* src, cb_size index, cb_size max, const char* file, int line_number)
{
	const char* begin = src;
	cb_size i = index;

	do
	{
		if (i >= (max - 1))
		{
			cb_log_error("Could not copy. String \"%s\" is too long. (file: '%s' line: '%d' max buffer length of '%d').", begin, line_number, file, max);
			CB_ASSERT(0);
			break;
		}
		dst[i++] = *src++;
	} while (*src != '\0');

	dst[i] = '\0';

	return i - index;
}

/* return 1 if separator has been added 0 otherwise */
CB_INTERNAL cb_size
cb_ensure_trailing_dir_separator(char* path, cb_size path_len)
{
	if (!path || path_len >= CB_MAX_PATH) return cb_false;

	if (path_len)
	{
		path += path_len;
		if (path[-1] != '/' && path[-1] != '\\')
		{
			*path++ = CB_PREFERRED_DIR_SEPARATOR_CHAR;
			*path = '\0';
			return 1;
		}
	}
	return 0;
}

CB_INTERNAL char*
cb_path_combine(const char* left, const char* right)
{
	char* result = cb_tmp_calloc(CB_MAX_PATH);
	cb_size n = 0;
	n += cb_str_append_from(result, left, n, CB_MAX_PATH);
	n += cb_ensure_trailing_dir_separator(result, strlen(result));
	n += cb_str_append_from(result, right, n, CB_MAX_PATH);
	return result;
}

#ifdef WIN32

CB_INTERNAL cb_bool
cb_path__exists(const wchar_t* path)
{
	DWORD attr = GetFileAttributesW(path);
	return attr != INVALID_FILE_ATTRIBUTES;
}

CB_INTERNAL cb_bool
cb_path_exists(const char* path)
{
	return cb_path__exists(cb_utf8_to_utf16(path));
}

CB_INTERNAL cb_bool
cb__create_directory(const wchar_t* path)
{
	return CreateDirectoryW(path, NULL);
}

CB_INTERNAL cb_bool
cb_create_directory(const char* path)
{
	return cb__create_directory(cb_utf8_to_utf16(path));
}
#else

CB_INTERNAL cb_bool cb_path_exists(const char* path) { return access(path, F_OK) == 0; }
CB_INTERNAL cb_bool cb_path__exists(const char* path) { return cb_path_exists(path); }

CB_INTERNAL cb_bool cb_create_directory(const char* path) { return mkdir(path, 0777) == 0; }
CB_INTERNAL cb_bool cb__create_directory(const char* path) { return cb_create_directory(path); }

#endif

CB_INTERNAL cb_bool
cb_path_is_absolute(cb_strv path)
{
	cb_size len = path.size;

	if (len == 0) return cb_false;

#ifdef _WIN32
	/* Check drive C : */
	cb_size i = 0;
	if (isalpha(path.data[0]) && path.data[1] == ':')
	{
		i = 2;
	}

	return (path.data[i] == '/' || path.data[i] == '\\');
#else

	return (path.data[0] == '/');
#endif
}

#ifdef _WIN32
#define getcwd(buf, size) GetCurrentDirectoryA((DWORD)size, buf)
#else
#include <unistd.h> /* getcwd */
#endif

CB_INTERNAL char*
cb_path_get_absolute_core(const char* path, cb_bool is_directory)
{
	cb_size n = 0;
	char* buffer = (char*)cb_tmp_calloc(FILENAME_MAX);
	char* cursor = NULL;

	if (!cb_path_is_absolute(cb_strv_make_str(path)))
	{
		/* skip ./ or .\ */
		if (path[0] == '.' && (path[1] == '\\' || path[1] == '/'))
			path += 2;

		getcwd(buffer, FILENAME_MAX);
		if (buffer == NULL)
		{
			cb_log_error("Could not get absolute path from '%s'", path);
			return NULL;
		}
		
		n = strlen(buffer);
		n += cb_ensure_trailing_dir_separator(buffer, n);
	}

	n += cb_str_append_from(buffer, path, n, CB_MAX_PATH);

	/* Add trailing slash if it's a directory path */
	if (is_directory)
	{
		cb_ensure_trailing_dir_separator(buffer, strlen(buffer));
	}

	cursor = buffer + 2; /* start at 2 to dealing with volume name. */
	/* Replace slash with the preferred one. */
	while (*cursor != '\0')
	{
		if (*cursor == CB_DEFAULT_DIR_SEPARATOR_CHAR)
		{
			*cursor = CB_PREFERRED_DIR_SEPARATOR_CHAR;
		}
		cursor += 1;
	}

	return buffer;
}

CB_INTERNAL char*
cb_path_get_absolute_file(const char* path)
{
	cb_bool is_directory = cb_false;
	return cb_path_get_absolute_core(path, is_directory);
}

CB_INTERNAL char*
cb_path_get_absolute_dir(const char* path)
{
	cb_bool is_directory = cb_true;
	return cb_path_get_absolute_core(path, is_directory);
}

/* create directories recursively */
CB_INTERNAL void
cb_create_directories_core(const char* path, cb_size size)
{
#ifdef _WIN32
	typedef wchar_t tchar;
	wchar_t* str = NULL;
#else
	typedef char tchar;
	char* str = NULL;
#endif

	if (path == NULL || size <= 0) {
		cb_log_error("Could not create directory. Path is empty.");
		return;
	}

	if (size > CB_MAX_PATH) {
		cb_log_error("Could not create directory. Path is too long '%s'.", path);
		return;
	}

#ifdef _WIN32
	str = (wchar_t*)cb_utf8_to_utf16(path);
#else
	str = (char*)path;
#endif

	if (!cb_path__exists(str))
	{
		tchar* cur = str + 2; /* + 2 to avoid root on Windows (unix would require +1 for the original slash ) */
		tchar* end = str + size;
		while (cur < end)
		{
			/* go to next directory separator */
			while (*cur && *cur != '\\' && *cur != '/')
				cur++;

			if (*cur)
			{
				*cur = '\0'; /* terminate path at separator */
				if (!cb_path__exists(str))
				{
					if (!cb__create_directory(str))
					{
						cb_log_error("Could not create directory.");
						return;
					}
				}
				*cur = CB_PREFERRED_DIR_SEPARATOR_CHAR; /* put the separator back */
			}
			cur++;
		}
	}
}

CB_INTERNAL void
cb_create_directories(const char* path, cb_size size)
{
	cb_size index = cb_tmp_save();
	cb_create_directories_core(path, size);
	cb_tmp_restore(index);
}

CB_INTERNAL cb_bool
cb_copy_file(const char* src_path, const char* dest_path)
{
#ifdef _WIN32
	/* create target directory if it does not exists */
	cb_create_directories(dest_path, strlen(dest_path));
	cb_log_debug("Copying '%s' to '%s'", src_path, dest_path);

	wchar_t* src_path_w = cb_utf8_to_utf16(src_path);
	wchar_t* dest_path_w = cb_utf8_to_utf16(dest_path);
	DWORD attr = GetFileAttributesW(src_path_w);

	if (attr == INVALID_FILE_ATTRIBUTES) {
		cb_log_error("Could not retieve file attributes of file '%s' (%d).", src_path, GetLastError());
		return cb_false;
	}

	cb_bool is_directory = attr & FILE_ATTRIBUTE_DIRECTORY;
	BOOL fail_if_exists = FALSE;
	if (!is_directory && !CopyFileW(src_path_w, dest_path_w, fail_if_exists)) {
		cb_log_error("Could not copy file '%s', %lu", src_path, GetLastError());
		return cb_false;
	}
	return cb_true;
#else

	int src_fd = -1;
	int dst_fd = -1;
	struct stat src_stat;
	off_t sendfile_off = 0;
	cb_size send_result = 0;
	cb_size total_bytes_copied = 0;
	cb_size bytes_copied = 0;
	cb_size bytes_left = 0;

	/* create target directory if it does not exists */
	cb_create_directories(dest_path, strlen(dest_path));
	cb_log_debug("Copying '%s' to '%s'", src_path, dest_path);

	src_fd = open(src_path, O_RDONLY);
	if (src_fd < 0)
	{
		cb_log_error("Could not open file '%s': %s", src_path, strerror(errno));
		return cb_false;
	}
	
    if (fstat(src_fd, &src_stat) < 0)
	{
        cb_log_error("Could not get fstat of file '%s': %s", src_path, strerror(errno));
		close(src_fd);
		return cb_false;
    }
	
	dst_fd = open(dest_path, O_CREAT | O_TRUNC | O_WRONLY, src_stat.st_mode);

	if (dst_fd < 0)
	{
        cb_log_error("Could not open file '%s': %s", dest_path, strerror(errno));
		close(src_fd);
		return cb_false;
	}
	
    total_bytes_copied = 0;
    bytes_left = src_stat.st_size;
    while (bytes_left > 0)
    {
		sendfile_off = total_bytes_copied;
		send_result = (cb_size)sendfile(dst_fd, src_fd, &sendfile_off, bytes_left);
		if(send_result <= 0)
		{
			break;
		}
		bytes_copied = send_result;
		bytes_left -= bytes_copied;
		total_bytes_copied += bytes_copied;
    }
  
	close(src_fd);
	close(dst_fd);
	return bytes_left == 0;
#endif
}

CB_INTERNAL cb_bool
cb_try_copy_file_to_dir(const char* file, const char* directory)
{
	cb_bool result = 0;
	cb_size tmp_index = cb_tmp_save();
	cb_strv filename;
	const char* destination_file = 0;

	if (cb_path_exists(file))
	{
		filename = cb_path_filename_str(file);
		destination_file = cb_tmp_sprintf("%s%.*s", directory, filename.size, filename.data);

		result = cb_copy_file(file, destination_file);
	}

	cb_tmp_restore(tmp_index);
	return result;
}

CB_INTERNAL cb_bool
cb_copy_file_to_dir(const char* file, const char* directory)
{
	if (!cb_try_copy_file_to_dir(file, directory))
	{
		cb_log_error("Could not copy file '%s' to directory %s", file, directory);
		return cb_false;
	}
	return cb_true;
}


CB_INTERNAL cb_bool
cb_delete_file(const char* src_path)
{
	cb_bool result = 0;
	cb_size tmp_index = cb_tmp_save();
	cb_log_debug("Deleting file '%s'.", src_path);
#ifdef _WIN32
	result = DeleteFileW(cb_utf8_to_utf16(src_path));
#else
	result = remove(src_path) != -1;
#endif
	if (!result) {
		cb_log_debug("Could not delete file '%s'.", src_path);
	}

	cb_tmp_restore(tmp_index);

	return result;
}

CB_INTERNAL cb_bool
cb_move_file(const char* src_path, const char* dest_path)
{
	if (cb_copy_file(src_path, dest_path))
	{
		return cb_delete_file(src_path);
	}

	return cb_false;
}

CB_INTERNAL cb_bool
cb_try_move_file_to_dir(const char* file, const char* directory)
{
	cb_bool result = cb_false;
	cb_strv filename = { 0 };
	const char* destination_file = NULL;

	cb_size index = cb_tmp_save();
	if (cb_path_exists(file))
	{
		filename = cb_path_filename_str(file);
		destination_file = cb_tmp_sprintf("%s%.*s", directory, filename.size, filename.data);

		result = cb_move_file(file, destination_file);
	}
	
	cb_tmp_restore(index);

	return result;
}

CB_INTERNAL cb_bool
cb_move_file_to_dir(const char* file, const char* directory)
{
	if (!cb_try_move_file_to_dir(file, directory))
	{
		cb_log_error("Could not move file '%s' to '%s'", file, directory);
		return cb_false;
	}
	return cb_true;
}

CB_API void
cb_init(void)
{
	cb_context_init(&default_ctx);
	current_ctx = &default_ctx;
#ifdef _WIN32
	cb_bool exit_on_exception = cb_true; /* @TODO make this configurable */
	if (exit_on_exception)
	{
		SetUnhandledExceptionFilter(exit_on_exception_handler);
	}
#endif
}

CB_API void
cb_destroy(void)
{
	cb_context_destroy(cb_current_context());
	cb_tmp_reset();
}

CB_API void
cb_clear(void)
{
	cb_context_clear(cb_current_context());
	cb_tmp_reset();
}

CB_API cb_project_t*
cb_project(const char* name)
{
	cb_project_t* project;

	if (!cb_try_find_project_by_name_str(name, &project))
	{
		project = cb_create_project(name);
	}

	cb_current_context()->current_project = project;
	return project;
}

CB_API cb_project_t*
cb_project_f(const char* format, ...)
{
	cb_project_t* p;
	va_list args;
	va_start(args, format);

	p = cb_project(cb_tmp_vsprintf(format, args));

	va_end(args);

	return p;
}

CB_API void
cb_add_many_core(cb_strv key, cb_strv values[], cb_size count)
{
	cb_size i;
	cb_strv value = { 0 };
	/* Check that the ptr is contains in the tmp buffer */
	CB_ASSERT(cb_tmp_contains(key.data));

	for (i = 0; i < count; ++i)
	{
		value = values[i];
		/* Check that the ptr is contains in the tmp buffer */
		CB_ASSERT(cb_tmp_contains(value.data));
		
		cb_mmap_insert(
			&cb_current_project()->mmap,
			cb_kv_make_with_strv(key, value));
	}
}

CB_INTERNAL void
cb_add_many(const char* key, const char* values[], cb_size count)
{
	cb_size i;
	cb_strv value = { 0 };

	for (i = 0; i < count; ++i)
	{
		value = cb_tmp_str_to_strv(values[i]);
		cb_add_many_core(cb_tmp_str_to_strv(key), &value, 1);
	}
}

CB_API void
cb_add_many_vnull(const char* key, ...)
{
	cb_strv value;
	va_list args;
	const char* current = NULL;
	va_start(args, key);

	current = va_arg(args, const char*);
	CB_ASSERT(current);
	while (current)
	{
		value = cb_tmp_str_to_strv(current);
		cb_add_many_core(cb_tmp_str_to_strv(key), &value, 1);
		current = va_arg(args, const char*);
	}
	va_end(args);
}

CB_API void
cb_add(const char* key, const char* value)
{
	cb_strv value_copy = cb_tmp_str_to_strv(value);
	cb_add_many_core(cb_tmp_str_to_strv(key), &value_copy, 1);
}

CB_API void
cb_add_f(const char* key, const char* format, ...)
{
	cb_strv value;
	va_list args;
	va_start(args, format);

	value = cb_tmp_strv_vprintf(format, args);
	cb_add_many_core(cb_tmp_str_to_strv(key), &value, 1);

	va_end(args);
}

CB_API void
cb_set(const char* key, const char* value)
{
	/* @OPT this can easily be optimized, but we don't care about that right now. */
	cb_remove_all(key);
	cb_add(key, value);
}

CB_API void
cb_set_f(const char* key, const char* format, ...)
{
	va_list args;
	va_start(args, format);

	cb_set(key, cb_tmp_vsprintf(format, args));

	va_end(args);
}

CB_API cb_size
cb_remove_all(const char* key)
{
	cb_kv kv = cb_kv_make_with_str(cb_strv_make_str(key), "");
	cb_project_t* p = cb_current_project();
	return cb_mmap_remove(&p->mmap, kv);
}

CB_API cb_size
cb_remove_all_f(const char* format, ...)
{
	va_list args;
	cb_size count = 0;
	va_start(args, format);

	count = cb_remove_all(cb_tmp_vsprintf(format, args));

	va_end(args);
	return count;
}

CB_API cb_bool
cb_contains(const char* key, const char* value)
{
	cb_project_t* p;
	cb_kv_range range;
	cb_kv kv = cb_kv_make_with_str(cb_strv_make_str(key), value);
	p = cb_current_project();
	range = cb_mmap_get_range(&p->mmap, kv.key);

	while (range.begin < range.end)
	{
		if (cb_strv_equals_strv((*range.begin).u.strv, kv.u.strv))
		{
			return cb_true;
		}
		range.begin++;
	}
	return cb_false;
}

CB_API cb_bool
cb_remove_one(const char* key, const char* value)
{
	cb_kv kv = cb_kv_make_with_str(cb_strv_make_str(key), value);
	cb_project_t* p = cb_current_project();
	cb_kv_range range = cb_mmap_get_range(&p->mmap, kv.key);

	while (range.begin < range.end)
	{
		if (cb_strv_equals_strv((*range.begin).u.strv, kv.u.strv))
		{
			cb_size index = p->mmap.darr.data - range.begin;
			cb_darrT_remove(&p->mmap, index);
			return cb_true;
		}
		range.begin++;
	}
	return cb_false;
}

CB_API cb_bool
cb_remove_one_f(const char* key, const char* format, ...)
{
	va_list args;
	cb_bool was_removed = cb_false;
	va_start(args, format);

	was_removed = cb_remove_one(key, cb_tmp_vsprintf(format, args));

	va_end(args);
	return was_removed;
}

CB_API void
cb_add_file(const char* file)
{
	cb_add(cb_FILES, file);
}

/* Properties are just (strv) values from the map of a project. */
CB_INTERNAL cb_bool
try_get_property(cb_project_t* project, const char* key, cb_kv* result)
{
	if (cb_mmap_try_get_first(&project->mmap, cb_strv_make_str(key), result))
	{
		return cb_true;
	}
	return cb_false;
}

CB_INTERNAL cb_bool
try_get_property_strv(cb_project_t* project, const char* key, cb_strv* result)
{
	cb_kv kv_result;
	if (cb_mmap_try_get_first(&project->mmap, cb_strv_make_str(key), &kv_result))
	{
		*result = kv_result.u.strv;
		return cb_true;
	}
	return cb_false;
}

CB_INTERNAL cb_bool
cb_property_equals(cb_project_t* project, const char* key, const char* comparison_value)
{
	cb_strv result;
	return try_get_property_strv(project, key, &result)
		&& cb_strv_equals_str(result, comparison_value);
}

CB_API const char*
cb_bake_project_with(cb_toolchain toolchain, const char* project_name)
{
	const char* result = toolchain.bake(&toolchain, project_name);
	cb_log_important("%s", result);
	return result;
}

CB_API const char*
cb_bake_project(const char* project_name)
{
	return cb_bake_project_with(cb_toolchain_default(), project_name);
}

CB_API const char*
cb_bake(void)
{
	cb_project_t* p = cb_current_project();
	return cb_bake_project(p->name.data);
}

CB_INTERNAL const char*
cb_get_output_directory(cb_project_t* project, const cb_toolchain* tc)
{
	cb_strv out_dir;
	if (try_get_property_strv(project, cb_OUTPUT_DIR, &out_dir))
	{
		return cb_path_get_absolute_dir(out_dir.data);
	}
	else
	{
		/* Get default output directory */
		return cb_path_get_absolute_dir(cb_path_combine(tc->default_directory_base, project->name.data));
	}
}

CB_API void
cb_debug(cb_bool value)
{
	cb_debug_enabled = value;
}

struct cb_process_handle {
	const char* cmd;
	const char* starting_directory;
	cb_bool stdout_to_string; /* If stdout needs to be copied to a string. */
	cb_bool stderr_to_string; /* If stderr needs to be copied to a string. */
	cb_dstr stdout_string;    /* If stdout_to_string has been set to true. */
	cb_dstr stderr_string;    /* If stderr_to_string has been set to true. */
	int exit_code;
};

CB_INTERNAL cb_process_handle* cb_process_core(cb_process_handle* handle);

CB_INTERNAL cb_process_handle*
cb_create_process_handle(const char* cmd, const char* starting_directory)
{
	cb_process_handle* handle = (cb_process_handle*)cb_tmp_alloc(sizeof(cb_process_handle));
	memset(handle, 0, sizeof(cb_process_handle));

	handle->cmd = cmd;
	handle->starting_directory = starting_directory;
	handle->exit_code = -1;

	cb_dstr_init(&handle->stdout_string);
	cb_dstr_init(&handle->stderr_string);

	return handle;
}

CB_API int
cb_process(const char* cmd)
{
	return cb_process_in_directory(cmd, NULL);
}

CB_API int
cb_process_in_directory(const char* cmd, const char* starting_directory)
{
	cb_process_handle* handle = cb_create_process_handle(cmd, starting_directory);
	handle = cb_process_core(handle);
	return cb_process_end(handle);
}

CB_API cb_process_handle*
cb_process_to_string(const char* cmd, const char* starting_directory, cb_bool also_get_stderr)
{
	cb_process_handle* handle = cb_create_process_handle(cmd, starting_directory);

	handle->stdout_to_string = cb_true;
	handle->stderr_to_string = also_get_stderr;

	return cb_process_core(handle);
}

CB_API int
cb_run(const char* executable_path)
{
	return cb_process_in_directory(cb_tmp_sprintf("\"%s\"", executable_path), NULL);
}

CB_API const char*
cb_process_stdout_string(cb_process_handle* handle)
{
	return handle->stdout_string.data;
}

CB_API const char*
cb_process_stderr_string(cb_process_handle* handle)
{
	return handle->stderr_string.data;
}

CB_API int
cb_process_end(cb_process_handle* handle)
{
	cb_dstr_destroy(&handle->stdout_string);
	cb_dstr_destroy(&handle->stderr_string);

	return handle->exit_code;
}

#if _WIN32

/* #process */

CB_INTERNAL cb_process_handle*
cb_process_core(cb_process_handle* handle)
{
	HANDLE process_stdout_write = NULL; /* Pipe (write) for stdout of the child process */
	HANDLE process_stdout_read = NULL;  /* Pipe (read) for stdout of the child process */
	HANDLE process_stderr_write = NULL; /* Pipe (write) for stderr of the child process */
	HANDLE process_stderr_read = NULL;  /* Pipe (read) for stderr of the child process */
	SECURITY_ATTRIBUTES saAttr;         /* To create the pipes. */

	DWORD exit_code = (DWORD) -1;
	DWORD wait_result = 0;
	DWORD byte_read_from_buffer = 0;
	char process_output_buffer[256] = { 0 };
	BOOL handles_inheritance = 0;

	wchar_t* cmd_w = cb_utf8_to_utf16(handle->cmd);
	wchar_t* starting_directory_w = NULL;

	cb_log_debug("Running process '%s'", handle->cmd);
	if (handle->starting_directory && handle->starting_directory[0])
	{
		starting_directory_w = cb_utf8_to_utf16(handle->starting_directory);
		cb_log_debug("Subprocess started in directory '%s'", handle->starting_directory);
	}

	/* Ensure that everything is written into the outputs before creating a new process that will also write in those outputs */
	fflush(stdout);
	fflush(stderr);

	STARTUPINFOW si;
	PROCESS_INFORMATION pi;

	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);

	if (handle->stdout_to_string
		|| handle->stderr_to_string)
	{
		handles_inheritance = TRUE;

		si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
		si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
		si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

		si.dwFlags |= STARTF_USESTDHANDLES;
			
		saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
		saAttr.bInheritHandle = TRUE;
		saAttr.lpSecurityDescriptor = NULL;

		if (handle->stdout_to_string)
		{
			/* Create a pipe for the child process's stdout. */
			if (!CreatePipe(&process_stdout_read, &process_stdout_write, &saAttr, 0)
				|| !SetHandleInformation(process_stdout_read, HANDLE_FLAG_INHERIT, 0))
			{
				cb_log_error("Could not create pipe for stdout.");
				exit(1);
			}

			si.hStdOutput = process_stdout_write;
		}

		if (handle->stderr_to_string)
		{
			/* Create a pipe for the child process's stderr. */
			if (!CreatePipe(&process_stderr_read, &process_stderr_write, &saAttr, 0)
				|| !SetHandleInformation(process_stderr_read, HANDLE_FLAG_INHERIT, 0))
			{
				cb_log_error("Could not create pipe for stderr.");
				exit(1);
			}

			si.hStdError = process_stderr_write;
		}
	}

	ZeroMemory(&pi, sizeof(pi));

	/* Start the child process.
	*  NOTE: we use CreateProcessW because CreateProcessA (with /utf-8) cannot create path with unicode.
	*/
	if (!CreateProcessW(
		NULL,                 /* No module name (use command line) */
		cmd_w,                /* Command line */
		NULL,                 /* Process handle not inheritable */
		NULL,                 /* Thread handle not inheritable */ 
		handles_inheritance,  /* Set handle inheritance */
		0,                    /* No creation flags */
		NULL,                 /* Use parent's environment block */
		starting_directory_w, /* Use parent's starting directory */
		&si,                  /* Pointer to STARTUPINFO structure */
		&pi)                  /* Pointer to PROCESS_INFORMATION structure */
		)
	{
		cb_log_error("CreateProcessW failed: %d", GetLastError());
		/* No need to close handles since the process creation failed */
		return handle;
	}

	/* Close the write ends of the pipes since they will not be used in the parent process. */
	
	if (handle->stdout_to_string)
		CloseHandle(process_stdout_write);
	if (handle->stderr_to_string)
		CloseHandle(process_stderr_write);

	wait_result = WaitForSingleObject(
		pi.hProcess, /* HANDLE hHandle, */
		INFINITE     /* DWORD  dwMilliseconds */
	);

	if (wait_result == WAIT_FAILED)
	{
		cb_log_error("Could not wait on child process: %lu", GetLastError());
	}
	else
	{
		if (GetExitCodeProcess(pi.hProcess, &exit_code))
		{
			if (exit_code != 0)
			{
				cb_log_error("Command exited with exit code %lu", exit_code);
			}
		}
		else
		{
			cb_log_error("Could not get process exit code: %lu", GetLastError());
		}
	}

	if (handle->stdout_to_string)
	{
		/* Read output from the child process's pipe for stdout. Stop when there is no more data. */
		while (ReadFile(process_stdout_read, process_output_buffer, sizeof(process_output_buffer), &byte_read_from_buffer, NULL)
			&& byte_read_from_buffer != 0)
		{
			cb_dstr_append_f(&handle->stdout_string, "%.*s", (int)byte_read_from_buffer, process_output_buffer);
		}
		CloseHandle(process_stdout_read);
	}

	if (handle->stderr_to_string)
	{
		while (ReadFile(process_stderr_read, process_output_buffer, sizeof(process_output_buffer), &byte_read_from_buffer, NULL)
			&& byte_read_from_buffer != 0)
		{
			cb_dstr_append_f(&handle->stderr_string, "%.*s", (int)byte_read_from_buffer, process_output_buffer);
		}

		CloseHandle(process_stderr_read);
	}

	/* Close process and thread handles. */
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	handle->exit_code = exit_code;
	return handle;
}
#else

/* space or tab */
CB_INTERNAL cb_bool cb_is_space(char c) { return c == ' ' || c == '\t'; }

CB_INTERNAL cb_bool
cb_is_end_of_quote(const char* str, char quote_type)
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
CB_INTERNAL const char*
cb_get_next_arg(const char* str, cb_strv* sv)
{
	sv->data = str;
	sv->size = 0;
	if (str == NULL || *str == '\0')
		return NULL;
		
	while (*str != '\0')
	{
		/* Skip spaces */
		while (*str != '\0' && cb_is_space(*str))
			str += 1;

		/* Return early if end of string */
		if (*str == '\0') 
			return sv->size > 0 ? str: NULL;

		/* Handle quotes */
		if (*str == '\'' || *str == '\"')
		{
			const char* quote = str;
			str += 1; /* skip quote */
			
			/* Return early if end of string */
			if (*str == '\0')
				return sv->size > 0 ? str : NULL;

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
	return NULL;
}

#define CB_INVALID_PROCESS (-1)

CB_INTERNAL pid_t
cb_fork_process(char* args[], cb_process_handle* handle, int stdout_pfd[2], int stderr_pfd[2])
{
	pid_t pid = fork();

	switch (pid)
	{
	case -1: /* Error */
		cb_log_error("Could not fork child process: %s", strerror(errno));
		return CB_INVALID_PROCESS;
	case 0:  /* Child process */
	{
		if (handle->stdout_to_string)
		{
			dup2(stdout_pfd[1], STDOUT_FILENO);
		}
		if (handle->stderr_to_string)
		{
			dup2(stderr_pfd[1], STDERR_FILENO);
		}

		/* Change directory in the fork */
		if (handle->starting_directory
			&& handle->starting_directory[0]
			&& chdir(handle->starting_directory) < 0) {
			cb_log_error("Could not change directory to '%s': %s", handle->starting_directory, strerror(errno));
			return CB_INVALID_PROCESS;
		}
		if (execvp(args[0], args) == CB_INVALID_PROCESS) {
			cb_log_error("Could not exec child process: %s", strerror(errno));
			return CB_INVALID_PROCESS;
		}
		CB_ASSERT(0 && "unreachable");
		break;
	}
	default:
		/* Do nothing for parent process */
		break;
	}

	return pid;
}

CB_INTERNAL cb_process_handle*
cb_process_core(cb_process_handle* handle)
{
	cb_darrT(const char*) args;
	cb_strv arg; /* Current argument */
	const char* cmd_cursor = handle->cmd; /* Current position in the string command */
	pid_t pid = CB_INVALID_PROCESS;
	int wstatus = 0; /* pid wait status */
	int exit_status = -1;

	ssize_t bytes_read = 0;   /* Byte read count when we retrieve the output of the child process. */
	char buffer[256] = { 0 }; /* Buffer to get the output of the child process. */
	int stdout_pfd[2] = { 0 };     /* Pipe file descriptor for stdout. */
	int stderr_pfd[2] = { 0 };     /* Pipe file descriptor for stderr. */

	cb_darrT_init(&args);

	cb_log_debug("Running process '%s'", handle->cmd);
	if (handle->starting_directory && handle->starting_directory[0])
	{
		cb_log_debug("Subprocess started in directory '%s'", handle->starting_directory);
	}

	/* Ensure that everything is written into the outputs before creating a new process that will also write in those outputs */
	fflush(stdout);
	fflush(stderr);

	/* Split args from the command line and add it to the array. */
	while ((cmd_cursor = cb_get_next_arg(cmd_cursor, &arg)) != NULL)
	{
		cb_darrT_push_back(&args, cb_tmp_strv_to_str(arg));
	}

	/* Last value of the args should be a null value, only append if necessary */
	if (args.darr.size > 0)
	{
		cb_darrT_push_back(&args, NULL);
	}

	if (handle->stdout_to_string)
	{
		if (pipe(stdout_pfd) == -1)
		{
			cb_log_error("Pipe creation failed for stdout_pfd.");
			cb_set_and_goto(exit_status, -1, cleanup);
		}
	}
	if (handle->stderr_to_string)
	{
		if (pipe(stderr_pfd) == -1)
		{
			cb_log_error("Pipe creation failed for stderr_pfd.");
			cb_set_and_goto(exit_status, -1, cleanup);
		}
	}

	pid = cb_fork_process((char**)args.darr.data, handle, stdout_pfd, stderr_pfd);

	if (pid == CB_INVALID_PROCESS)
	{
		cb_set_and_goto(exit_status, -1, cleanup);
	}

	/* wait for process to be done */
	for (;;) {
		if (waitpid(pid, &wstatus, 0) < 0) {
			cb_log_error("Could not wait on command (pid %d): '%s'", pid, strerror(errno));
			cb_set_and_goto(exit_status, -1, cleanup);
		}

		if (WIFEXITED(wstatus)) {
			exit_status = WEXITSTATUS(wstatus);
			if (exit_status != 0) {
				cb_log_error("Command exited with exit code '%d'", exit_status);
				cb_set_and_goto(exit_status, -1, cleanup);
			}

			break; /* Get out of the loop since the process exited. */
		}

		if (WIFSIGNALED(wstatus)) {
			cb_log_error("Command process was terminated by '%s'", strsignal(WTERMSIG(wstatus)));
			cb_set_and_goto(exit_status, -1, cleanup);
		}
	}

	if (handle->stdout_to_string)
	{
		/* Close the write pipe since the child program has been exited. */
		close(stdout_pfd[1]);

		/* Read output of the child process from the read file description of the pipe. */
		while ((bytes_read = read(stdout_pfd[0], buffer, sizeof(buffer))) > 0)
		{
			cb_dstr_append_f(&handle->stdout_string, "%.*s", (int)bytes_read, buffer);
		}
		/* Close the read pipe since we read all the information from it. */
		close(stdout_pfd[0]);
	}

	if (handle->stderr_to_string)
	{
		/* Close the write pipe since the child program has been exited. */
		close(stderr_pfd[1]);

		if (handle->stderr_to_string)
		{
			/* Read output of the child process from the read file description of the pipe. */
			while ((bytes_read = read(stderr_pfd[0], buffer, sizeof(buffer))) > 0)
			{
				cb_dstr_append_f(&handle->stderr_string, "%.*s", (int)bytes_read, buffer);
			}
			/* Close the read pipe since we read all the information from it. */
			close(stderr_pfd[0]);
		}
	}

cleanup:
	cb_darrT_destroy(&args);
	handle->exit_code = exit_status;
	return handle;
}

#endif

#ifdef _WIN32

/* ================================================================ */
/* toolchain MSVC */
/* ================================================================ */

/* #msvc #toolchain */

CB_API const char*
cb_toolchain_msvc_bake(cb_toolchain* tc, const char* project_name)
{
	cb_dstr str; /* cl.exe command */
	/* @FIXME use an array of string instead to make it straigthforward to follow */
	cb_dstr str_obj; /* to keep track of the .obj generated and copy them.*/
	const char* output_dir;       /* Output directory. Contains the directory path of the binary being created. */
	
	cb_kv_range range = { 0 };
	cb_kv_range lflag_range = { 0 };
	cb_kv current = { 0 };       /* Temporary kv to store results. */
	cb_kv current_lflag = { 0 }; /* Temporary kv to linker flags. */
	cb_strv basename = { 0 };
	const char* artefact = NULL; /* Resulting artefact path */
	const char* ext = "";        /* Resulting artefact extension */
	const char* _ = "  ";        /* Space to separate command arguments. */
	const char* tmp = "";
	
	cb_strv linked_project_name = { 0 };
	cb_project_t* linked_project = NULL;
	const char* linked_output_dir; /* to keep track of the .obj generated */
	const char* path_prefix = NULL;
	cb_project_t* project = NULL;
	cb_size tmp_index;
	project = cb_find_project_by_name_str(project_name);

	if (!project)
	{
		return NULL;
	}
	
	cb_dstr_init(&str);
	cb_dstr_init(&str_obj);

	/* Get and format output directory */
	output_dir = cb_get_output_directory(project, tc);

	/* Create output directory if it does not exist yet. */
	cb_create_directories(output_dir, strlen(output_dir));

	/* Use /utf-8 by default since it's retrocompatible with utf-8 */
	/* Use /nologo to avoid undesirable messages in the command line. */
	cb_dstr_append_str(&str, "cl.exe /utf-8 /nologo ");

	/* Handle binary type */

	cb_bool is_exe = cb_property_equals(project, cb_BINARY_TYPE, cb_EXE);
	cb_bool is_shared_library = cb_property_equals(project, cb_BINARY_TYPE, cb_SHARED_LIBRARY);
	cb_bool is_static_library = cb_property_equals(project, cb_BINARY_TYPE, cb_STATIC_LIBRARY);

	if (!is_exe && !is_shared_library && !is_static_library)
	{
		cb_log_error("Unknown binary type");
		cb_set_and_goto(artefact, NULL, exit);
	}

	if (is_static_library) {
		cb_dstr_append_str(&str, "/c ");
	}
	else if (is_shared_library) {
		cb_dstr_append_str(&str, "/LD ");
	}

	/* Add output directory to cl.exe command */
	/* /Fo"output/directory/" */
	cb_dstr_append_f(&str, "/Fo\"%s\" ", output_dir);

	ext = is_exe ? ".exe" : ext;
	ext = is_static_library ? ".lib" : ext;
	ext = is_shared_library ? ".dll" : ext;

	artefact = cb_tmp_sprintf("%s%s%s", output_dir, project_name, ext);

	/* Define binary output for executable and shared library. Static library is set with the link.exe command*/
	if (is_exe || is_shared_library)
	{
		/* /Fe"output/directory/bin.ext" */
		cb_dstr_append_f(&str, "/Fe\"%s\" ", artefact);
	}

	/* Append compiler flags */
	{
		range = cb_mmap_get_range_str(&project->mmap, cb_CXFLAGS);
		while (cb_mmap_range_get_next(&range, &current))
		{
			cb_dstr_append_f(&str,"%s ", current.u.strv.data);
		}
	}

	/* Append include directories */
	{
		range = cb_mmap_get_range_str(&project->mmap, cb_INCLUDE_DIRECTORIES);
		while (cb_mmap_range_get_next(&range, &current))
		{
			/* Absolute file is created using the tmp buffer allocator but we don't need it once it's inserted into the dynamic string */
			tmp_index = cb_tmp_save();
			cb_dstr_append_f(&str, "/I\"%s\" ", cb_path_get_absolute_dir(current.u.strv.data));
			cb_tmp_restore(tmp_index);
		}
	}
	
	/* Append preprocessor definition */
	{
		range = cb_mmap_get_range_str(&project->mmap, cb_DEFINES);
		while (cb_mmap_range_get_next(&range, &current))
		{
			cb_dstr_append_f(&str, "/D\"%s\" ", current.u.strv.data);
		}
	}

	/* Append files and .obj */
	{
		range = cb_mmap_get_range_str(&project->mmap, cb_FILES);
		while (cb_mmap_range_get_next(&range, &current))
		{
			/* Absolute file is created using the tmp buffer allocator but we don't need it once it's inserted into the dynamic string */
			tmp_index = cb_tmp_save();

			cb_dstr_append_f(&str, "\"%s\" ", cb_path_get_absolute_file(current.u.strv.data));

			cb_tmp_restore(tmp_index);

			basename = cb_path_basename(current.u.strv);

			cb_dstr_append_f(&str_obj, "\"%.*s.obj\" ", basename.size, basename.data);
		}
	}

	/* Append libraries */
	{
		range = cb_mmap_get_range_str(&project->mmap, cb_LIBRARIES);
		while (cb_mmap_range_get_next(&range, &current))
		{
			cb_dstr_append_f(&str, "\"%s.lib\" ", current.u.strv.data);
		}
	}

	/* For each linked project we add the link information to the cl.exe command */
	range = cb_mmap_get_range_str(&project->mmap, cb_LINK_PROJECTS);
	if (range.count > 0)
	{
		cb_dstr_append_str(&str, "/link ");
		/* Add linker flags */
		{
			lflag_range = cb_mmap_get_range_str(&project->mmap, cb_LFLAGS);
			while (cb_mmap_range_get_next(&lflag_range, &current_lflag))
			{
				cb_dstr_append_strv(&str, current_lflag.u.strv);
				cb_dstr_append_str(&str, _);
			}
		}

		/* iterate all the linked projects */
		while (cb_mmap_range_get_next(&range, &current))
		{
			linked_project_name = current.u.strv;

			linked_project = cb_find_project_by_name(linked_project_name);
			if (!project)
			{
				cb_set_and_goto(artefact, NULL, exit);
			}

			linked_output_dir = cb_get_output_directory(linked_project, tc);

			/* is shared or static library */
			if (cb_property_equals(linked_project, cb_BINARY_TYPE, cb_SHARED_LIBRARY)
				|| cb_property_equals(linked_project, cb_BINARY_TYPE, cb_STATIC_LIBRARY))
			{
				/* /LIBPATH:"output/dir/" "mlib.lib" */
				cb_dstr_append_f(&str, "/LIBPATH:\"%s\" \"%.*s.lib\" ", linked_output_dir, linked_project_name.size, linked_project_name.data);
			}

			/* is shared library */
			if (cb_property_equals(linked_project, cb_BINARY_TYPE, cb_SHARED_LIBRARY))
			{
				path_prefix = cb_tmp_sprintf("%s%.*s", linked_output_dir, linked_project_name.size, linked_project_name.data);
				/* .dll */
				tmp = cb_tmp_sprintf("%s.dll", path_prefix);
				if (!cb_copy_file_to_dir(tmp, output_dir))
				{
					cb_set_and_goto(artefact, NULL, exit);
				}

				/* .exp */
				tmp = cb_tmp_sprintf("%s.exp", path_prefix);
				if (!cb_copy_file_to_dir(tmp, output_dir))
				{
					cb_log_warning("Missing .exp. Shared libraries usually need to have some symbol exported with '__declspec(dllexport)'");
					cb_set_and_goto(artefact, NULL, exit);
				}

				/* .lib */
				tmp = cb_tmp_sprintf("%s.lib", path_prefix);
				if (!cb_copy_file_to_dir(tmp, output_dir))
				{
					cb_log_warning("Missing .lib file. Shared libraries must create a .lib file for other program to be linked with at compile time.");
					cb_set_and_goto(artefact, NULL, exit);
				}

				/* Copy .pdb if there is any. */
				tmp = cb_tmp_sprintf("%s.pdb", path_prefix);
				cb_try_copy_file_to_dir(tmp, output_dir);
			}
		}
	}

	/* execute cl.exe */
	if (cb_process_in_directory(str.data, output_dir) != 0)
	{
		cb_set_and_goto(artefact, NULL, exit);
	}
	
	if (is_static_library)
	{
		/* lib.exe /NOLOGO /OUT:"output/dir/my_lib.lib" /LIBPATH:"output/dir/" a.obj b.obj c.obj ... */
		tmp = cb_tmp_sprintf("lib.exe /NOLOGO /OUT:\"%s\"  /LIBPATH:\"%s\" %s ", artefact, output_dir, str_obj.data);
		if (cb_process_in_directory(tmp, output_dir) != 0)
		{
			cb_log_error("Could not execute command to build static library\n");
			cb_set_and_goto(artefact, NULL, exit);
		}
	}

exit:
	cb_dstr_destroy(&str);
	cb_dstr_destroy(&str_obj);

	return artefact;
}

CB_API cb_toolchain
cb_toolchain_msvc(void)
{
	cb_toolchain tc;
	tc.bake = cb_toolchain_msvc_bake;
	tc.name = "msvc";
	tc.default_directory_base = ".build/msvc";
	return tc;
}

#else

/* ================================================================ */
/* toolchain GCC */
/* ================================================================ */

/* #gcc #toolchain */

CB_INTERNAL cb_bool
cb_strv_ends_with(cb_strv sv, cb_strv rhs)
{
	cb_strv sub = { 0 };
	if (sv.size < rhs.size)
	{
		return cb_false;
	}

	sub = cb_strv_make(sv.data + (sv.size - rhs.size), rhs.size);
	return cb_strv_equals_strv(sub, rhs);
}

CB_API const char*
cb_toolchain_gcc_bake(cb_toolchain* tc, const char* project_name)
{
	cb_dstr str;
	cb_dstr str_obj; /* to keep track of the .o generated */
	const char* output_dir;        /* Output directory. Contains the directory path of the binary being created. */
	cb_darrT(const char*) objects; /* Contains the path of all .o objects */

	cb_bool is_exe = cb_false;
	cb_bool is_static_library = cb_false;
	cb_bool is_shared_library = cb_false;

	const char* ext = "";
	cb_kv_range range = { 0 };
	cb_kv current = { 0 };       /* Temporary kv to store results. */
	cb_kv_range lflag_range = { 0 };
	cb_kv current_lflag = { 0 };
	 
	cb_strv basename = { 0 };

	const char* linked_output_dir;
	cb_strv linked_project_name = { 0 };
	cb_project_t* linked_project = NULL;
	const char* tmp; /* Temp string */
	const char* current_object = NULL;
	cb_size i = 0;
	const char* _ = "  ";        /* Space to separate command arguments */
	cb_size tmp_index = 0;           /* to save temporary allocation index */
	const char* artefact = NULL; /* Resulting artifact path */

	cb_project_t* project = NULL;
	
	project = cb_find_project_by_name_str(project_name);
	
	if (!project)
	{
		return NULL;
	}

	/* gcc command */
	
	cb_dstr_init(&str);

	cb_dstr_init(&str_obj);

	cb_darrT_init(&objects);

	/* Get and format output directory */
	output_dir = cb_get_output_directory(project, tc);

	/* Create output directory if it does not exist yet. */
	cb_create_directories(output_dir, strlen(output_dir));

	/* Start command */
	cb_dstr_append_str(&str, "cc ");

	/* Handle binary type */
	is_exe = cb_property_equals(project, cb_BINARY_TYPE, cb_EXE);
	is_shared_library = cb_property_equals(project, cb_BINARY_TYPE, cb_SHARED_LIBRARY);
	is_static_library = cb_property_equals(project, cb_BINARY_TYPE, cb_STATIC_LIBRARY);

	if (!is_exe && !is_shared_library && !is_static_library)
	{
		cb_log_error("Unknown binary type");
		cb_set_and_goto(artefact, NULL, exit);
	}

	ext = is_exe ? "" : ext; /* do not provide extension to executables on linux */
	ext = is_static_library ? ".a" : ext;
	ext = is_shared_library ? ".so" : ext;

	/* Append compiler flags */
	{
		range = cb_mmap_get_range_str(&project->mmap, cb_CXFLAGS);
		while (cb_mmap_range_get_next(&range, &current))
		{
			cb_dstr_append_strv(&str, current.u.strv);
			cb_dstr_append_str(&str, _);
		}
	}

	/* Append include directories */
	{
		range = cb_mmap_get_range_str(&project->mmap, cb_INCLUDE_DIRECTORIES);
		while (cb_mmap_range_get_next(&range, &current))
		{
			tmp_index = cb_tmp_save();
			cb_dstr_append_f(&str, "-I \"%s\" ", cb_path_get_absolute_dir(current.u.strv.data));
			cb_tmp_restore(tmp_index);
		}
	}

	/* Append preprocessor definition */
	{
		range = cb_mmap_get_range_str(&project->mmap, cb_DEFINES);
		while (cb_mmap_range_get_next(&range, &current))
		{
			cb_dstr_append_f(&str, "-D%.*s ", current.u.strv);
		}
	}

	if (is_static_library)
	{
		artefact = cb_tmp_sprintf("%slib%s%s", output_dir, project_name, ext);
		cb_dstr_append_str(&str, "-c ");
	}

	if (is_shared_library)
	{
		cb_dstr_append_str(&str, "-shared ");
		artefact = cb_tmp_sprintf("%slib%s%s", output_dir, project_name, ext);
		cb_dstr_append_f(&str, "-o \"%s\" ", artefact);
	}

	if (is_exe)
	{
		artefact = cb_tmp_sprintf("%s%s", output_dir, project_name);
		cb_dstr_append_f(&str, "-o \"%s\" ", artefact);
	}

	/* Append .c files and .obj */
	{
		range = cb_mmap_get_range_str(&project->mmap, cb_FILES);
		while (cb_mmap_range_get_next(&range, &current))
		{
			/* Absolute file is created using the tmp buffer allocator but we don't need it once it's inserted into the dynamic string */
			tmp_index = cb_tmp_save();
			/* add .c files */
			cb_dstr_append_f(&str, "\"%s\" ", cb_path_get_absolute_file(current.u.strv.data));
			cb_tmp_restore(tmp_index);
			
			basename = cb_path_basename(current.u.strv);

			if (is_exe || is_static_library)
			{
				/* output/dir/my_object.o */
				cb_dstr_append_f(&str_obj, "\"%s%.*s.o\" ", output_dir, basename.size, basename.data);

				/* my_object.o */
				cb_darrT_push_back(&objects, cb_tmp_sprintf("%.*s.o", basename.size, basename.data));
			}
		}
	}

	/* Append libraries */
	{
		range = cb_mmap_get_range_str(&project->mmap, cb_LIBRARIES);
		while (cb_mmap_range_get_next(&range, &current))
		{
			cb_dstr_append_f(&str, "-l \"%s\" ", current.u.strv.data);
		}
	}

	/* For each linked project we add the link information to the gcc command */
	range = cb_mmap_get_range_str(&project->mmap, cb_LINK_PROJECTS);
	if (range.count > 0)
	{
		/* Add linker flags */
		{
			lflag_range = cb_mmap_get_range_str(&project->mmap, cb_LFLAGS);
			while (cb_mmap_range_get_next(&lflag_range, &current_lflag))
			{
				cb_dstr_append_strv(&str, current_lflag.u.strv);
				cb_dstr_append_str(&str, _);
			}
		}

		/* Give some parameters to the linker to  look for the shared library next to the binary being built */
		cb_dstr_append_str(&str, " -Wl,-rpath,$ORIGIN ");

		while (cb_mmap_range_get_next(&range, &current))
		{
			linked_project_name = current.u.strv;

			linked_project = cb_find_project_by_name(linked_project_name);
			if (!project)
			{
				cb_set_and_goto(artefact, NULL, exit);
			}

			linked_output_dir = cb_get_output_directory(linked_project, tc);

			/* Is static lib or shared lib */
			if (cb_property_equals(linked_project, cb_BINARY_TYPE, cb_STATIC_LIBRARY)
				|| cb_property_equals(linked_project, cb_BINARY_TYPE, cb_SHARED_LIBRARY))
			{
				/* -L "my/path/" -l "my_proj" */ 
				cb_dstr_append_f(&str, "-L \"%s\" -l \"%.*s\" ", linked_output_dir, linked_project_name.size, linked_project_name.data);
			}

			/* Is shared library */
			if (cb_property_equals(linked_project, cb_BINARY_TYPE, cb_SHARED_LIBRARY))
			{
				/* libmy_project.so*/
				tmp = cb_tmp_sprintf("%slib%.*s.so", linked_output_dir, linked_project_name.size, linked_project_name.data);

				if (!cb_copy_file_to_dir(tmp, output_dir))
				{
					cb_set_and_goto(artefact, NULL, exit);
				}
			}
		}
	}

	/* Example: gcc <includes> -c  <c source files> */
	if (cb_process_in_directory(str.data, output_dir) != 0)
	{
		cb_set_and_goto(artefact, NULL, exit);
	}

	if (is_static_library || is_shared_library)
	{
		if (is_static_library)
		{
			/* Create libXXX.a in the output directory */
			/* Example: ar -crs libMyLib.a MyObjectAo MyObjectB.o */
			tmp = cb_tmp_sprintf("ar -crs \"%s\" %s ", artefact, str_obj.data);
			if (cb_process_in_directory(tmp, output_dir) != 0)
			{
				cb_set_and_goto(artefact, NULL, exit);
			}
		}
	}

exit:
	cb_dstr_destroy(&str);
	cb_dstr_destroy(&str_obj);

	return artefact;
}

CB_API cb_toolchain
cb_toolchain_gcc()
{
	cb_toolchain tc;
	tc.bake = cb_toolchain_gcc_bake;
	tc.name = "gcc";
	tc.default_directory_base = ".build/gcc";
	return tc;
}

#endif /* #else of _WIN32 */

CB_API cb_toolchain
cb_toolchain_default(void)
{
#ifdef _WIN32
	return cb_toolchain_msvc();
#else
	return cb_toolchain_gcc();
#endif
}

#endif /* CB_IMPLEMENTATION_CPP  */

#endif /* CB_IMPLEMENTATION */