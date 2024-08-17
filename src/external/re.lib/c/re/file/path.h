#ifndef RE_PATH_H
#define RE_PATH_H

#include "../dstr_util.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int re_path_bool_t;

static re_path_bool_t re_path_is_absolute_str(const dstr_char_t* str);
// get current absolute path
re_path_bool_t re_path_get_current(dstr* str);
// get absolute path from relative one (according to the current path)
re_path_bool_t re_path_get_absolute(dstr* path);
// remove file or directory at
re_path_bool_t re_path_remove(const dstr_char_t* str);

// remove last segment of the path, returned value does not contains the trailing slash
static dstr_view re_path_remove_last_segment(const dstr_view path);
static dstr_view re_path_parent_directory(const dstr_view path);
static dstr_view re_path_basename(const dstr_view path);
static dstr_view re_path_filename(const dstr_view path);
static re_path_bool_t re_path_is_root(const dstr_view path);
static dstr_char_t re_path_system_slash();

// details
static re_path_bool_t details__is_colon(dstr_char_t ch);
static re_path_bool_t details__is_slash(dstr_char_t ch);
static void* details__memrchr(const void * buffer, int ch, size_t _count);
static re_path_bool_t details__find_last_slash(const dstr_view* path, size_t* index);
static re_path_bool_t details__find_last_pathname_separator(const dstr_view* path, size_t* index);
static re_path_bool_t details__is_valid_drive_char(dstr_char_t ch);

static inline void re_path_combine(dstr* str, const dstr_view path)
{
	// We initialize the slash with the system slash
	// but maybe we don't want the system slash, so save the one we found in the strings
	dstr_char_t slash = re_path_system_slash();
	dstr_view mutable_path = path;
	dstr_view str_view = dstr_to_view(str);

	if (str->size > 0
		&& details__is_slash(dstr_view_back(str_view)))
	{
		slash = dstr_view_back(str_view); // save slash for later use
		str->size--; // remove trailing slash
	}

	if (mutable_path.size > 0
		&& details__is_slash(dstr_view_front(mutable_path)))
	{
		slash = dstr_view_front(mutable_path); // save slash for later use
		mutable_path.data = mutable_path.data + 1; // remove leading slash
		mutable_path.size = mutable_path.size - 1;
	}

	if (str->size > 0) // only add separator if str is already a path (tested as non empty string here)
		dstr_append_char(str, slash); // add needed separator

	dstr_append_view(str, mutable_path);
}

static inline void re_path_combine_str(dstr* str, const char* path)
{
	dstr_view view = dstr_view_make_from_str(path);
	re_path_combine(str, view);
}

static inline re_path_bool_t re_path_has_parent_path(dstr_view path)
{
	size_t last_separator_start_pos;

	if (!path.size)
		return (re_path_bool_t)(0);

	if (!details__find_last_slash(&path, &last_separator_start_pos))
		return (re_path_bool_t)(0);

	return (re_path_bool_t)(1);
}

static inline dstr_view re_path_without_extension(dstr_view path)
{
	if (re_path_is_root(path)) return path;
	if (dstr_view_equals_str(path, ".")) return path;
	if (dstr_view_equals_str(path, "..")) return path;

	dstr_view result = path;

	int count = path.size;
	while (count)
	{
		dstr_char_t ch = path.data[count]; // TODO: optimize this

		// retrieve string until the "."
		if (ch == '.')
		{
			result = dstr_view_substr_from(path, 0, count);
		}

		// stop if we get the parent directory
		if (details__is_slash(ch))
		{
			break;
		}

		--count;
	}

	return result;
}

static inline void re_path_replace_extension(dstr* path, dstr_view ext)
{
	if (path->size <= 0)
		return;

	if (ext.size <= 0)
		return;
	
	dstr_view without_ext = re_path_without_extension(dstr_to_view(path));

	dstr_assign_view(path, without_ext);
	
	if (ext.data[0] != '.') // if ext does not start with a dot, appends one.
		dstr_append_char(path, '.');

	dstr_append_view(path, ext);
}

static inline void re_path_replace_extension_str(dstr* str, const char* ext)
{
	dstr_view view = dstr_view_make_from_str(ext);
	re_path_replace_extension(str, view);
}

static inline dstr_view re_path_basename(const dstr_view path)
{
	dstr_view filename = re_path_filename(path);
	return re_path_without_extension(filename);
}

static inline dstr_view re_path_get_last_segment(const dstr_view path)
{
	if (path.size <= 0)
		return dstr_view_make();

	if (re_path_is_root(path))
		return path;

	dstr_view result = path;
	if (details__is_slash(dstr_view_back(result)))
	{
		result.size--; // remove last slash, so that it does not get counted in the next find_last_pathname_separator
	}

	size_t last_separator_start_pos;
	if (!details__find_last_slash(&result, &last_separator_start_pos))
	{
		return result; // no separator found, return the original path
	}

	size_t new_size = last_separator_start_pos + 1; // new_size = index + 1
	result.data = result.data + new_size;
	result.size = result.size - new_size;

	return result;
}

static inline dstr_view re_path_filename(const dstr_view path)
{
	return re_path_get_last_segment(path);
}
static inline re_path_bool_t re_path_is_root(const dstr_view path)
{
#ifdef _WIN32
	if (path.size == 2)
	{
		return details__is_valid_drive_char(path.data[0]) && path.data[1] == ':';
	}
	else if (path.size == 3)
	{
		return details__is_valid_drive_char(path.data[0]) && path.data[1] == ':' && details__is_slash(path.data[2]);
	}	
	return (re_path_bool_t)(0);
#else
	return path.size == 1 && path.data[0] == '/';
#endif
}


static inline dstr_view re_path_remove_last_segment(const dstr_view path)
{
	if (!path.size)
		return dstr_view_make();

	dstr_view result = path;
	if (details__is_slash(dstr_view_back(result)))
	{
		result.size--; // remove last slash, so that it does not get counted in the next find_last_pathname_separator
	}

	size_t last_separator_start_pos;

	if (!details__find_last_slash(&result, &last_separator_start_pos))
		return dstr_view_make();

	size_t new_size = last_separator_start_pos + 1; // new_size = index + 1
	result.size = new_size;

	// we also remove the slash from the result if there is any
	if (details__is_slash(dstr_view_back(result)))
	{
		result.size--; // remove last slash
	}

	return result;
}

static inline dstr_view re_path_parent_directory(const dstr_view path)
{
	return re_path_remove_last_segment(path);
}

static inline re_path_bool_t re_path_is_absolute_str(const char* path)
{
	if (path == 0 || *path == 0)
	{
		return (re_path_bool_t)(0);
	}

#ifdef _WIN32
	// Check drive C:
	int i = 0;
	if (isalpha(path[0]) && path[1] == ':')
	{
		i = 2;
	}

	return (path[i] == '/' || path[i] == '\\');
#else

	return (path->data[0] == '/');
#endif
}

static inline re_path_bool_t re_path_is_absolute(const dstr_view path)
{
	int len = path.size;

	if (len == 0)
	{
		return (re_path_bool_t)(0);
	}

#ifdef _WIN32
	// Check drive C:
	int i = 0;
	if (isalpha(path.data[0]) && path.data[1] == ':')
	{
		i = 2;
	}

	return (path.data[i] == '/' || path.data[i] == '\\');
#else

	return (path->data[0] == '/');
#endif
}
	static inline char re_path_system_slash()
	{
#ifdef _WIN32
		return '\\';
#else
		return '/';
#endif
	}
	static inline re_path_bool_t details__is_colon(dstr_char_t _c)
	{
		return _c == ':';
	}

	static inline re_path_bool_t details__is_slash(dstr_char_t _c) {
		return	((_c) == '/' || (_c) == '\\');
	}

	static inline void* details__memrchr(const void * _buffer, int _char, size_t _count)
	{
		unsigned char *ptr = (unsigned char *)_buffer;

		for (;;) {
			if (_count-- == 0) {
				return NULL;
			}

			if (*ptr-- == (unsigned char)_char) {
				break;
			}
		}

		return (void *)(ptr + 1);
	}

	static inline re_path_bool_t details__find_last_slash(const dstr_view* path, size_t* index)
	{
		const char* cursor;
		const char* begin;

		char c;

		cursor = path->data + path->size - 1; // set the cursor to the last char
		begin = path->data;

		while (cursor > begin) {
			c = *cursor;
			if (c == '\\' || c == '/')
			{
				*index = (cursor - begin);
				return (re_path_bool_t)(1);
			}
			--cursor;
		}

		return (re_path_bool_t)(0);
	}

	static inline re_path_bool_t details__find_last_pathname_separator(const dstr_view* path, size_t* index)
	{
		const char* cursor;
		const char* begin;

#ifdef _WIN32
		char c;

		cursor = path->data + path->size - 1; // set the cursor to the last char
		begin = path->data;

		--cursor;  // if the last char is a separator, it will be skipped

		while (cursor > begin) {
			c = *cursor;
			if (c == '\\' || c == '/')
			{
				*index = (cursor - begin);
				return (re_path_bool_t)(1);
			}
			--cursor;
		}

		// no directory found, then search for a driver letter
		cursor = (char*)memchr(path->data, ':', path->size);
		if (cursor)
		{
			*index = (cursor - begin) + 1; // + 1 to retrieve the ':' char
			return (re_path_bool_t)(1);
		}
#else
		cursor = (char*)details__memrchr(path->data, ':', path->size);
		if (cursor)
		{
			*index = (cursor - begin);
			return (re_path_bool_t)(1);
		}
#endif
		return (re_path_bool_t)(0);
	}

	static re_path_bool_t details__is_valid_drive_char(dstr_char_t ch)
	{
		return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
	}
//#endif

#ifdef __cplusplus
} // extern "C"
#endif

#endif // RE_PATH_H
