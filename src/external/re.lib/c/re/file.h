#ifndef RE_FILE_H
#define RE_FILE_H

#include "dstr.h"

#if _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/stat.h>     /* mkdir, fstat */
#include <fcntl.h>        /* open, close */
#include <sys/sendfile.h> /* sendfile */
#endif

#include <stdio.h> /* fopen, fclose, tmpfile_s */
#include <errno.h> /* errno_t */

#ifndef RE_FILE_API
#ifdef RE_FILE_STATIC
#define RE_FILE_API static
#else
#define RE_FILE_API extern
#endif
#endif

#ifndef RE_FILE_ASSERT
#define RE_FILE_ASSERT assert
#include <assert.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef int re_file_bool;

typedef struct re_file_predicate
{
	void* data;
	// char 'ch' is an int to be able to check with 'EOF'
	re_file_bool(*check)(void* data, strv sv, int ch);
} re_file_predicate_t;

RE_FILE_API FILE* re_file_open(const char* path, const char* mode);
RE_FILE_API FILE* re_file_open_readonly(const char* path);
RE_FILE_API FILE* re_file_open_readwrite(const char* path);

RE_FILE_API re_file_bool re_file_close(FILE* file);
RE_FILE_API re_file_bool re_file_read_all(dstr* str, FILE* file);
RE_FILE_API re_file_bool re_file_open_and_read(dstr* str, const char* path);

RE_FILE_API re_file_bool re_file_get_until(dstr* str, FILE* fp, re_file_predicate_t predicate);

RE_FILE_API re_file_predicate_t re_file_make_get_until_string(strv* sv);
RE_FILE_API re_file_predicate_t re_file_make_get_until_eol();
RE_FILE_API re_file_predicate_t re_file_make_get_until_eof();

RE_FILE_API FILE* re_file__create_temp_file(const char* text);
RE_FILE_API re_file_bool re_file_exists(const dstr* path);
RE_FILE_API re_file_bool re_file_exists_str(const char* path);

RE_FILE_API re_file_bool re_file_create_directory_recursively(const char* path, int size);
RE_FILE_API re_file_bool re_file_copy(const char* src_path, const char* dest_path);
RE_FILE_API re_file_bool re_file_copy_ex(const char* src_path, const char* dest_path);
RE_FILE_API re_file_bool re_file_delete(const char* src_path);
RE_FILE_API re_file_bool re_file_move(const char* src_path, const char* dest_path);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* RE_FILE_H */

#ifdef RE_FILE_IMPLEMENTATION

static char
re_file_system_slash()
{
#ifdef _WIN32
	return '\\';
#else
	return '/';
#endif
}

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

RE_FILE_API FILE*
re_file_open(const char* path, const char* mode)
{
#ifdef WIN32
	FILE* file = NULL;
	if (fopen_s(&file, path, mode) != 0)
#else
	FILE* file = fopen(path, mode);
	if (!file)
#endif
	{
		fprintf(stderr, "Could not open file '%s'", path);
		return NULL;
	}

	return file;
}

RE_FILE_API FILE*
re_file_open_readonly(const char* path)
{
#ifdef WIN32
	const char* mode = "rb";
#else
	const char* mode = "r";
#endif
	return re_file_open(path, mode);
}

RE_FILE_API FILE*
re_file_open_readwrite(const char* path)
{
#ifdef WIN32
	const char* mode = "wb+";
#else
	const char* mode = "w+";
#endif
	return re_file_open(path, mode);
}

RE_FILE_API re_file_bool
re_file_close(FILE* file)
{
	if (fclose(file) != 0)
	{
		fprintf(stderr, "Could not close file '%p'", file);
		return 0;
	}
	return 1;
}

RE_FILE_API re_file_bool
re_file_read_all(dstr* str, FILE* file)
{
	dstr_clear(str);

	fseek(file, 0, SEEK_END);

	if (feof(file)) return (re_file_bool)0;

	long file_size = ftell(file);

	if (feof(file)) return (re_file_bool)0;

	fseek(file, 0, SEEK_SET);  /* rewind(file) can also be used. */

	dstr_resize(str, file_size);

	if (feof(file)) return (re_file_bool)0;

	size_t read_count = fread(str->data, file_size, 1 , file);

	if (read_count != 1 && file_size != 0) return (re_file_bool)0;

	if (feof(file)) return (re_file_bool)0;

	return (re_file_bool)1;
}

RE_FILE_API re_file_bool
re_file_open_and_read(dstr* str, const char* path)
{
	FILE* f = re_file_open_readonly(path);

	return f != NULL
		&& re_file_read_all(str, f)
		&& re_file_close(f);
}

RE_FILE_API re_file_bool
re_file_get_until(dstr* str, FILE* fp, re_file_predicate_t predicate)
{
	char ch;

	dstr_clear(str);

	if (feof(fp))
		return 0;

	file_lock(fp);
	while ((ch = file_nolock_getc(fp)) != EOF) {
		dstr_append_char(str, ch);
		if (predicate.check(predicate.data, dstr_to_strv(str), ch))
			break;
	}
	file_unlock(fp);

	if (ch == EOF && dstr_empty(str))
		return 0;

	return 1;
}

static inline re_file_bool
re_file__predicate_get_until_string_func(void* data, strv sv, int ch)
{
	strv* delimiter = (strv*)data;
	return strv_ends_with(sv, *delimiter) && ch != EOF;
}

/* until string 'sv' defined by user */
RE_FILE_API re_file_predicate_t
re_file_make_get_until_string(strv* sv)
{
	re_file_predicate_t get_until;
	get_until.data = sv;
	get_until.check = re_file__predicate_get_until_string_func;
	return get_until;
}

static inline re_file_bool
re_file__predicate_get_until_eol_func(void* data, strv sv, int ch)
{
	(void)data;
	strv delimiter;
	delimiter.data = "\n";
	delimiter.size = 1;
	return strv_ends_with(sv, delimiter) && ch != EOF;
}

/* until end of line */
RE_FILE_API re_file_predicate_t
re_file_make_get_until_eol()
{
	re_file_predicate_t get_until;
	get_until.data = 0;
	get_until.check = re_file__predicate_get_until_eol_func;
	return get_until;
}

static inline re_file_bool
re_file__predicate_get_until_eof_func(void* data, strv sv, int ch)
{
	(void)data;
	(void)sv;

	return ch == EOF;
}

/* until end of file */
RE_FILE_API re_file_predicate_t
re_file_make_get_until_eof()
{
	re_file_predicate_t get_until;
	get_until.data = 0;
	get_until.check = re_file__predicate_get_until_eof_func;
	return get_until;
}


RE_FILE_API FILE*
re_file__create_temp_file(const char* text)
{
#ifdef WIN32
	FILE* file = NULL;
	if (tmpfile_s(&file) != 0)
	{
		fprintf(stderr, "Could not create temp file\n");
		return NULL;
	}
#else
	FILE* file = tmpfile();
#endif
	if (file == NULL)
	{
		fprintf(stderr, "Could not create temp file\n");
		return NULL;
	}

	int begin = ftell(file);
	if (ferror(file)) {
		fprintf(stderr, "Could not use 'ftell'\n");
		return NULL;
	}

	fprintf(file, "%s", text);

	fseek(file, begin, SEEK_SET); // reset cursor to the beginning
	if (ferror(file)) {
		fprintf(stderr, "Could not use 'fseek'\n");
		return NULL;
	}
	return file;
}

RE_FILE_API re_file_bool
re_file_exists(const dstr* path)
{
	return re_file_exists_str(path->data);
}

RE_FILE_API re_file_bool
re_file_exists_str(const char* path)
{
#if _WIN32
	WIN32_FILE_ATTRIBUTE_DATA unused;
	return GetFileAttributesExA(path, GetFileExInfoStandard, &unused);
#else
	return access(path, F_OK) != -1;
#endif
}
#if _WIN32
static re_file_bool re_file_create_directory(const char* path) { return CreateDirectoryA(path, NULL); }
#else
static re_file_bool re_file_create_directory(const char* path) { return mkdir(path, 0777) == 0; }
#endif

#define RE_FILE_MAX_FULLPATH 1024
#define RE_FILE_MAX_ENTRY 256

#define re_file__safe_strcpy(dst, src, index, max) re_file__safe_strcpy_internal(dst, src, index, max, __FILE__, __LINE__)
static int re_file__safe_strcpy_internal(char* dst, const char* src, int index, int max, const char* file, int line)
{
	char c;
	const char* original = src;

	do {
		if (index >= max) {
			fprintf(stderr, "String \"%s\" too long to copy on line %d in file %s (max length of %d).\n", original, line, file, max);
			RE_FILE_ASSERT(0);
			break;
		}

		c = *src++;
		dst[index] = c;
		++index;
	} while (c);

	return index;
}

RE_FILE_API re_file_bool
re_file_create_directory_recursively(const char* path, int size)
{
	if (path == NULL || size <= 0) {
		fprintf(stderr, "Could not create directory. Path is empty.\n");
		return (re_file_bool)0;
	}

	if (size > RE_FILE_MAX_FULLPATH) {
		fprintf(stderr, "Could not create directory. Path is too long '%s'.\n", path);
		return (re_file_bool)0;
	}

	if (!re_file_exists_str(path))
	{
		char tmp_buffer[RE_FILE_MAX_FULLPATH + 1];  /* extra for the possible missing separator */

		re_file__safe_strcpy(tmp_buffer, path, 0, RE_FILE_MAX_FULLPATH);

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
				if (!re_file_exists_str(tmp_buffer))
				{
					if (!re_file_create_directory(tmp_buffer))
					{
						fprintf(stderr, "Could not create directory '%s'.\n", tmp_buffer);
						return (re_file_bool)0;
					}
				}
				*cur = re_file_system_slash(); /* put the separator back */
			}
			cur++;
		}
	}
	return (re_file_bool)1;
}

RE_FILE_API re_file_bool
re_file_copy(const char* src_path, const char* dest_path)
{
#ifdef _WIN32
	DWORD attr = GetFileAttributesA(src_path);

	if (attr == INVALID_FILE_ATTRIBUTES) {
		fprintf(stderr, "Could not rertieve file attributes of file '%s' (%d).\n", src_path, GetLastError());
		return (re_file_bool)0;
	}

	re_file_bool is_directory = attr & FILE_ATTRIBUTE_DIRECTORY;
	BOOL fail_if_exists = FALSE;
	if (!is_directory && !CopyFileA(src_path, dest_path, fail_if_exists)) {
		fprintf(stderr, "Could not copy file '%s', %lu\n", src_path, GetLastError());
		return (re_file_bool)0;
	}
	return (re_file_bool)1;
#else

	int src_fd = -1;
	int dst_fd = -1;
	src_fd = open(src_path, O_RDONLY, 0755);
	if (src_fd < 0) {
		fprintf(stderr, "Could not open file '%s': '%s'", src_path, strerror(errno));
		return (re_file_bool)0;
	}

	struct stat src_stat;
	if (fstat(src_fd, &src_stat) < 0) {
		fprintf(stderr, "Could not get fstat of file '%s': '%s'", src_path, strerror(errno));
		close(src_fd);
		return (re_file_bool)0;
	}

	dst_fd = open(src_path, O_CREAT | O_TRUNC | O_WRONLY, 0755);

	if (src_fd < 0) {
		fprintf(stderr, "Could not open file %s: %s", src_path, strerror(errno));
		close(src_fd);
		return (re_file_bool)0;
	}

	int64_t total_bytes_copied = 0;
	int64_t bytes_left = src_stat.st_size;
	while (bytes_left > 0)
	{
		off_t sendfile_off = total_bytes_copied;
		int64_t send_result = sendfile(dst_fd, src_fd, &sendfile_off, bytes_left);
		if (send_result <= 0)
		{
			break;
		}
		int64_t bytes_copied = (int64_t)send_result;
		bytes_left -= bytes_copied;
		total_bytes_copied += bytes_copied;
	}

	close(src_fd);
	close(dst_fd);
	return (re_file_bool)(bytes_left == 0);

#endif
}

RE_FILE_API re_file_bool
re_file_copy_ex(const char* src_path, const char* dest_path)
{
	/* create target directory if it does not exists */
	re_file_create_directory_recursively(dest_path, strlen(dest_path));
	return re_file_copy(src_path, dest_path);
}

RE_FILE_API re_file_bool
re_file_delete(const char* src_path)
{
#ifdef _WIN32
	return  DeleteFileA(src_path);
#else
	return remove(src_path) != -1;
#endif

}

RE_FILE_API re_file_bool
re_file_move(const char* src_path, const char* dest_path)
{
	if (re_file_copy(src_path, dest_path))
	{
		return re_file_delete(src_path);
	}

	return (re_file_bool)0;
}

#endif /* RE_FILE_IMPLEMENTATION */
