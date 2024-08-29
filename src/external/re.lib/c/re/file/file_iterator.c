#include <re/file.h>
#include <re/path.h>
#include "file_iterator.h"

#define CUTE_FILES_IMPLEMENTATION
#include "cute_files.h"

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

re_file_iterator_bool_t re_file_iterator_get_next(re_file_iterator* it, strv* path)
{
	re_file_iterator_bool_t has_next = it->dir.has_next;
	if (has_next)
	{
		cf_read_file(&it->dir, &it->file);
		*path = strv_make_from_str(it->file.name);
		cf_dir_next(&it->dir);
	}

	return has_next;
}