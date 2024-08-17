#ifndef RE_FILE_H
#define RE_FILE_H

#include "../dstr_util.h"

#include <stdio.h> /* tmpfile_s */
#include <errno.h> /* errno_t */

#ifdef __cplusplus
extern "C" {
#endif

typedef int re_file_bool_t;

typedef struct re_file_predicate_t
{
	void* data;
	// char 'ch' is an int to be able to check with 'EOF'
	re_file_bool_t(*check)(void* data, dstr_view sv, int ch);
} re_file_predicate_t;

static re_file_bool_t re_file_read_all(dstr* str, FILE *fp);

re_file_bool_t re_file_exists(const char* path);

static re_file_bool_t re_file_get_until(dstr* str, FILE *fp, re_file_predicate_t predicate);

// details

static inline re_file_bool_t re_file__predicate_get_until_string_func(void* data, dstr_view sv, int ch)
{
	dstr_view* delimiter = (dstr_view*)data;
	return dstr_view_ends_with(sv, *delimiter) && ch != EOF;
}

// until string 'sv' defined by user
inline re_file_predicate_t re_file_make_get_until_string(dstr_view* sv)
{
	re_file_predicate_t get_until;
	get_until.data = sv;
	get_until.check = re_file__predicate_get_until_string_func;
	return get_until;
}

static inline re_file_bool_t re_file__predicate_get_until_eol_func(void* data, dstr_view sv, int ch)
{
	(void)data;
	dstr_view delimiter;
	delimiter.data = "\n";
	delimiter.size = 1;
	return dstr_view_ends_with(sv, delimiter) && ch != EOF;
}

// until end of line
inline re_file_predicate_t re_file_make_get_until_eol()
{
	re_file_predicate_t get_until;
	get_until.data = 0;
	get_until.check = re_file__predicate_get_until_eol_func;
	return get_until;
}

static inline re_file_bool_t re_file__predicate_get_until_eof_func(void* data, dstr_view sv, int ch)
{
	(void)data;
	(void)sv;

	return ch == EOF;
}

// until end of file
inline re_file_predicate_t re_file_make_get_until_eof()
{
	re_file_predicate_t get_until;
	get_until.data = 0;
	get_until.check = re_file__predicate_get_until_eof_func;
	return get_until;
}

// details

// FILE

static inline char file_nolock_getc(FILE *file)
{
#ifdef WIN32
	return (char)_fgetc_nolock(file);
#else
	return getc_unlocked(file);
#endif
}

static inline void file_lock(FILE *file)
{
#ifdef WIN32
	_lock_file(file);
#else
	flockfile(file);
#endif
}

static inline void file_unlock(FILE *file)
{
#ifdef WIN32
	_unlock_file(file);
#else
	funlockfile(file);
#endif
}

//#ifdef RE_FILE_IMPLEMENTATION

static inline re_file_bool_t re_file_read_all(dstr* str, FILE* file)
{
	dstr_clear(str);

	fseek(file, 0, SEEK_END);

	if (feof(file)) return 0;

	long file_size = ftell(file);

	if (feof(file)) return 0;

	fseek(file, 0, SEEK_SET);  // rewind(file) can also be used.

	dstr_resize(str, file_size);

	if (feof(file)) return 0;

	fread(str->data, 1, file_size, file);

	if (feof(file)) return 0;

	return 1;
}

inline re_file_bool_t re_file_get_until(dstr* str, FILE *fp, re_file_predicate_t predicate)
{
	char ch;

	dstr_clear(str);

	if (feof(fp))
		return 0;

	file_lock(fp);
	while ((ch = file_nolock_getc(fp)) != EOF) {
		dstr_append_char(str, ch);
		if (predicate.check(predicate.data, dstr_to_view(str), ch))
			break;
	}
	file_unlock(fp);

	if (ch == EOF && dstr_empty(str))
		return 0;

	return 1;
}

//#endif /* RE_FILE_IMPLEMENTATION * /

static inline FILE* re_file__create_temp_file(const char* text)
{
	FILE* file = tmpfile();
	if (file == NULL)
	{
		fprintf(stderr, "could not create temp file\n");
		return NULL;
	}

	int begin = ftell(file);
	if (ferror(file)) {
		fprintf(stderr, "could not use 'ftell'\n");
		return NULL;
	}

	fprintf(file, "%s", text);

	fseek(file, begin, SEEK_SET); // reset cursor to the beginning
	if (ferror(file)) {
		fprintf(stderr, "could not use 'fseek'\n");
		return NULL;
	}
	return file;
}

extern void re_file_test();

#ifdef __cplusplus
} // extern "C"
#endif

#endif // RE_FILE_H
