#include "file.h"
#include "file_iterator.h"
#include "path.h"
#define CUTE_FILES_IMPLEMENTATION
#include "cute_files.h"

//-------------------------------------------------------------------------
// File
//-------------------------------------------------------------------------
re_file_bool_t re_file_exists(const char* path)
{
	return cf_file_exists(path);
}

//-------------------------------------------------------------------------
// File - Iterator
//-------------------------------------------------------------------------

re_file_iterator re_file_iterator_make(const dstr* str)
{
	return re_file_iterator_make_str(str->data);
}

re_file_iterator re_file_iterator_make_str(const dstr_char_t* str)
{
	re_file_iterator it;
	re_file_iterator_init_str(&it, str);
	return it;
}

void re_file_iterator_init(re_file_iterator* it, const dstr* str)
{
	re_file_iterator_init_str(it, str->data);
}

void re_file_iterator_init_str(re_file_iterator* it, const dstr_char_t* str)
{
	cf_dir_open(&it->dir, str);
}

void re_file_iterator_free(re_file_iterator* it)
{
	cf_dir_close(&it->dir);
}

re_file_iterator_bool_t re_file_iterator_get_next(re_file_iterator* it, dstr_view* path)
{
	re_file_iterator_bool_t has_next = it->dir.has_next;
	if (has_next)
	{
		cf_read_file(&it->dir, &it->file);
		*path = dstr_view_make_from_str(it->file.name);
		cf_dir_next(&it->dir);
	}

	return has_next;
}

#ifdef _WIN32
#include <direct.h> // _getcwd
#define GetCurrentDir _getcwd
#else
//#include <unistd.h>
#define GetCurrentDir getcwd
#endif

//-------------------------------------------------------------------------
// File - Path
//-------------------------------------------------------------------------

re_path_bool_t re_path_get_current(dstr* str)
{
	dstr_char_t buffer[FILENAME_MAX];
	dstr_char_t* path = (dstr_char_t*)GetCurrentDir(buffer, FILENAME_MAX);

	if (path == 0)
		return (re_path_bool_t)(0);

	dstr_assign_str(str, path);
	return (re_path_bool_t)(1);
}

re_path_bool_t re_path_get_absolute(dstr* path)
{
	if (re_path_is_absolute(dstr_to_view(path)))
		return (re_path_bool_t)(0);

	dstr current;
	dstr_init(&current);
	re_path_get_current(&current);

	re_path_combine(&current, dstr_to_view(path));
	dstr_assign_view(path, dstr_to_view(&current));

	dstr_destroy(&current);

	return (re_path_bool_t)(1);
}

#ifdef _WIN32
//#include <direct.h>
#define remove_file _unlink
#else
//#include <unistd.h>
#define remove_file unlink
#endif

re_path_bool_t re_path_remove(const dstr_char_t* str)
{
	return remove_file(str) == 0;
}