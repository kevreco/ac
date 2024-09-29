#ifndef RE_PATH_H
#define RE_PATH_H

#include <re/dstr.h>

#ifndef RE_PATH_API
#ifdef RE_PATH_STATIC
#define RE_PATH_API static
#else
#define RE_PATH_API extern
#endif
#endif

#ifdef _WIN32
#include <direct.h> /* _unlink, _getcwd */
#define remove_file _unlink
#define GetCurrentDir _getcwd
#else
#include <unistd.h>
#define remove_file unlink
#define GetCurrentDir getcwd
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef int re_path_bool;

RE_PATH_API void         re_path_combine(dstr* str, strv path);
RE_PATH_API void         re_path_combine_str(dstr* str, const dstr_char_t* path);
RE_PATH_API re_path_bool re_path_has_parent_path(strv path);
RE_PATH_API strv  re_path_without_extension(strv path);
RE_PATH_API void         re_path_replace_extension(dstr* path, strv ext);
RE_PATH_API void         re_path_replace_extension_str(dstr* str, const dstr_char_t* ext);
RE_PATH_API strv  re_path_basename(strv path);
/* Remove last segment of the path, returned value does not contains the trailing slash */
RE_PATH_API strv  re_path_get_last_segment(strv path);
RE_PATH_API strv  re_path_filename(strv path);
RE_PATH_API re_path_bool re_path_is_root(strv path);
RE_PATH_API strv  re_path_remove_last_segment(strv path);
RE_PATH_API strv  re_path_parent_directory(strv path);
RE_PATH_API re_path_bool re_path_is_absolute_str(const dstr_char_t* path);
RE_PATH_API re_path_bool re_path_is_absolute(strv path);
RE_PATH_API dstr_char_t  re_path_system_slash();

/* Get current absolute path */
RE_PATH_API re_path_bool re_path_get_current(dstr* str);
/* Get absolute path from relative one (according to the current path) */
RE_PATH_API re_path_bool re_path_get_absolute(dstr* path);
/* Remove file or directory at */
RE_PATH_API re_path_bool re_path_remove(const dstr_char_t* str);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* RE_PATH_H */

#ifdef RE_PATH_IMPLEMENTATION

/* details @TODO rename details__XXX to re_path__XXX */
static re_path_bool details__is_colon(dstr_char_t ch);
static re_path_bool details__is_slash(dstr_char_t ch);
static void* details__memrchr(const void * buffer, int ch, size_t _count);
static re_path_bool details__find_last_slash(strv path, size_t* index);
static re_path_bool details__find_last_pathname_separator(strv path, size_t* index);
static re_path_bool details__is_valid_drive_char(dstr_char_t ch);

RE_PATH_API void
re_path_combine(dstr* str, strv path)
{
	// We initialize the slash with the system slash
	// but maybe we don't want the system slash, so save the one we found in the strings
	dstr_char_t slash = re_path_system_slash();
	strv mutable_path = path;
	strv str_view = dstr_to_strv(str);

	if (str->size > 0
		&& details__is_slash(strv_back(str_view)))
	{
		slash = strv_back(str_view); /* Save slash for later use */
		str->size--; /* Remove trailing slash */
	}

	if (mutable_path.size > 0
		&& details__is_slash(strv_front(mutable_path)))
	{
		slash = strv_front(mutable_path);  /* Save slash for later use */
		mutable_path.data = mutable_path.data + 1; /* Remove leading slash */
		mutable_path.size = mutable_path.size - 1;
	}

	if (str->size > 0) /* Only add separator if str is already a path (tested as non empty string here) */
		dstr_append_char(str, slash); /* add needed separator */

	dstr_append(str, mutable_path);
}

RE_PATH_API void
re_path_combine_str(dstr* str, const dstr_char_t* path)
{
	strv sv = strv_make_from_str(path);
	re_path_combine(str, sv);
}

RE_PATH_API re_path_bool
re_path_has_parent_path(strv path)
{
	size_t last_separator_start_pos;

	if (!path.size)
		return (re_path_bool)(0);

	if (!details__find_last_slash(path, &last_separator_start_pos))
		return (re_path_bool)(0);

	return (re_path_bool)(1);
}

RE_PATH_API strv
re_path_without_extension(strv path)
{
	if (re_path_is_root(path)) return path;
	if (strv_equals_str(path, ".")) return path;
	if (strv_equals_str(path, "..")) return path;

	strv result = path;

	int count = path.size;
	while (count)
	{
		dstr_char_t ch = path.data[count]; /* @TODO: optimize this */

		/* retrieve string until the "." */
		if (ch == '.')
		{
			result = strv_substr_from(path, 0, count);
		}

		/* stop if we get the parent directory */
		if (details__is_slash(ch))
		{
			break;
		}

		--count;
	}

	return result;
}

RE_PATH_API void
re_path_replace_extension(dstr* path, strv ext)
{
	if (path->size <= 0)
		return;
	if (ext.size <= 0)
		return;
	if (strv_equals_str(dstr_to_strv(path), "."))
		return;
	if (strv_equals_str(dstr_to_strv(path), ".."))
		return;
	
	strv without_ext = re_path_without_extension(dstr_to_strv(path));

	dstr_assign(path, without_ext);
	
	if (ext.data[0] != '.') /* if ext does not start with a dot, appends one. */
		dstr_append_char(path, '.');

	dstr_append(path, ext);
}

RE_PATH_API void
re_path_replace_extension_str(dstr* str, const dstr_char_t* ext)
{
	strv sv = strv_make_from_str(ext);
	re_path_replace_extension(str, sv);
}

RE_PATH_API strv
re_path_basename(strv path)
{
	strv filename = re_path_filename(path);
	return re_path_without_extension(filename);
}

RE_PATH_API strv
re_path_get_last_segment(strv path)
{
	if (path.size <= 0)
		return strv_make();

	if (re_path_is_root(path))
		return path;

	strv result = path;
	if (details__is_slash(strv_back(result)))
	{
		result.size--; /* Remove last slash, so that it does not get counted in the next find_last_pathname_separator */
	}

	dstr_size_t last_separator_start_pos;
	if (!details__find_last_slash(result, &last_separator_start_pos))
	{
		return result; /* No separator found, return the original path */
	}

	dstr_size_t new_size = last_separator_start_pos + 1; /* new_size = index + 1 */
	result.data = result.data + new_size;
	result.size = result.size - new_size;

	return result;
}

RE_PATH_API strv
re_path_filename(strv path)
{
	return re_path_get_last_segment(path);
}

RE_PATH_API re_path_bool
re_path_is_root(strv path)
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
	return (re_path_bool)(0);
#else
	return path.size == 1 && path.data[0] == '/';
#endif
}


RE_PATH_API strv
re_path_remove_last_segment(strv path)
{
	if (!path.size)
		return strv_make();

	strv result = path;
	if (details__is_slash(strv_back(result)))
	{
		result.size--; /* Remove last slash, so that it does not get counted in the next find_last_pathname_separator */
	}

	size_t last_separator_start_pos;

	if (!details__find_last_slash(result, &last_separator_start_pos))
		return strv_make();

	size_t new_size = last_separator_start_pos + 1; /* new_size = index + 1 */
	result.size = new_size;

	/* we also remove the slash from the result if there is any */
	if (details__is_slash(strv_back(result)))
	{
		result.size--; /* remove last slash */
	}

	return result;
}

RE_PATH_API strv
re_path_parent_directory(strv path)
{
	return re_path_remove_last_segment(path);
}

RE_PATH_API re_path_bool
re_path_is_absolute_str(const dstr_char_t* path)
{
	if (path == 0 || *path == 0)
	{
		return (re_path_bool)(0);
	}

#ifdef _WIN32
	/* Check drive C:*/
	int i = 0;
	if (isalpha(path[0]) && path[1] == ':')
	{
		i = 2;
	}

	return (path[i] == '/' || path[i] == '\\');
#else

	return (path[0] == '/');
#endif
}

RE_PATH_API re_path_bool
re_path_is_absolute(strv path)
{
	int len = path.size;

	if (len == 0)
	{
		return (re_path_bool)(0);
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

	return (path.data[0] == '/');
#endif
}

RE_PATH_API dstr_char_t
re_path_system_slash()
{
#ifdef _WIN32
		return '\\';
#else
		return '/';
#endif
}

RE_PATH_API re_path_bool
re_path_get_current(dstr* str)
{
	dstr_char_t buffer[FILENAME_MAX];
	dstr_char_t* path = (dstr_char_t*)GetCurrentDir(buffer, FILENAME_MAX);

	if (path == NULL)
		return (re_path_bool)(0);

	dstr_assign_str(str, path);
	return (re_path_bool)(1);
}

RE_PATH_API re_path_bool
re_path_get_absolute(dstr* path)
{
	if (re_path_is_absolute(dstr_to_strv(path)))
		return (re_path_bool)(0);

	dstr current;
	dstr_init(&current);
	re_path_get_current(&current);

	re_path_combine(&current, dstr_to_strv(path));
	dstr_assign(path, dstr_to_strv(&current));

	dstr_destroy(&current);

	return (re_path_bool)(1);
}

RE_PATH_API re_path_bool
re_path_remove(const dstr_char_t* str)
{
	return remove_file(str) == 0;
}

static inline re_path_bool
details__is_colon(dstr_char_t c)
{
	return c == ':';
}

static inline re_path_bool
details__is_slash(dstr_char_t c)
{
	return	(c == '/' || c == '\\');
}

static inline void*
details__memrchr(const void * buffer, int ch, size_t count)
{
	unsigned char *ptr = (unsigned char *)buffer;

	for (;;) {
		if (count-- == 0) {
			return NULL;
		}

		if (*ptr-- == (unsigned char)ch) {
			break;
		}
	}

	return (void *)(ptr + 1);
}

static inline re_path_bool
details__find_last_slash(strv path, size_t* index)
{
	const char* cursor;
	const char* begin;

	char c;

	cursor = path.data + path.size - 1; // set the cursor to the last char
	begin = path.data;

	while (cursor > begin) {
		c = *cursor;
		if (c == '\\' || c == '/')
		{
			*index = (cursor - begin);
			return (re_path_bool)(1);
		}
		--cursor;
	}

	return (re_path_bool)(0);
}

static inline re_path_bool
details__find_last_pathname_separator(strv path, size_t* index)
{
	const char* cursor;
	const char* begin;

#ifdef _WIN32
	char c;

	cursor = path.data + path.size - 1; /* Set the cursor to the last char */
	begin = path.data;

	--cursor;  /* If the last char is a separator, it will be skipped */

	while (cursor > begin) {
		c = *cursor;
		if (c == '\\' || c == '/')
		{
			*index = (cursor - begin);
			return (re_path_bool)(1);
		}
		--cursor;
	}

	/* no directory found, then search for a driver letter */
	cursor = (char*)memchr(path.data, ':', path.size);
	if (cursor)
	{
		*index = (cursor - begin) + 1; /* + 1 to retrieve the ':' char */
		return (re_path_bool)(1);
	}
#else
	cursor = (char*)details__memrchr(path.data, ':', path.size);
	if (cursor)
	{
		*index = (cursor - begin);
		return (re_path_bool)(1);
	}
#endif
	return (re_path_bool)(0);
}

static inline re_path_bool
details__is_valid_drive_char(dstr_char_t ch)
{
	return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
}

#endif /* RE_PATH_IMPLEMENTATION */