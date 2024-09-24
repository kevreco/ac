#ifndef CB_FILE_IT_H
#define CB_FILE_IT_H

#if defined(_WIN32)
#define CB_INVALID_FILE_HANDLE INVALID_HANDLE_VALUE
#else
#define CB_INVALID_FILE_HANDLE NULL
#endif

#define CB_MAX_DIR_DEPTH 256

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)
typedef HANDLE handle_t;
#else
typedef DIR* handle_t;
#endif

/* File iterator. Can be recursive. */
typedef struct cb_file_it cb_file_it;
struct cb_file_it {
	cb_bool recursive;
	cb_bool has_next;

	/* Stack used for recursion. */
	char current_file[CB_MAX_PATH];

	/* Stack used for recursion, first item has 0 len, second item contains the lenght of the initial directory */
	cb_size dir_len_stack[CB_MAX_DIR_DEPTH];
	cb_size stack_size;

	handle_t handle_stack[CB_MAX_DIR_DEPTH];
#if defined(_WIN32)
	WIN32_FIND_DATAA find_data;
#else
	struct dirent* find_data;
#endif
};

CB_API void cb_file_it_init(cb_file_it* it, const char* base_directory);

CB_API void cb_file_it_init_recursive(cb_file_it* it, const char* base_directory);

CB_API void cb_file_it_destroy(cb_file_it* it);

CB_API const char* cb_file_it_current_file(cb_file_it* it);

CB_API cb_bool cb_file_it_get_next(cb_file_it* it);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* CB_FILE_IT_H */

#ifdef CB_IMPLEMENTATION

CB_INTERNAL void
cb_file_it__push_dir(cb_file_it* it, const char* directory)
{
	cb_size new_directory_len = 0;
	cb_size n = 0;
	handle_t handle = CB_INVALID_FILE_HANDLE;

	n += it->dir_len_stack[it->stack_size];
	n += cb_str_append_from(it->current_file, directory, n, CB_MAX_PATH);
	n += cb_ensure_trailing_dir_separator(it->current_file, n);
	new_directory_len = n;

#if defined(_WIN32)

	/* add asterisk to read all files from the directory */
	cb_str_append_from(it->current_file, "*", n, CB_MAX_PATH);
	handle = FindFirstFileA(it->current_file, &it->find_data);

	if (handle == CB_INVALID_FILE_HANDLE)
	{
		cb_log_error("Could not open directory '%s'", it->current_file);

		it->has_next = cb_false;
		return;
	}

	/* remove the last asterisk '*' */
	it->current_file[new_directory_len] = '\0';

#else
	handle = opendir(it->current_file);

	if (handle == CB_INVALID_FILE_HANDLE)
	{
		cb_log_error("Could not open directory '%s': %s.", directory, strerror(errno));
		it->has_next = cb_false;
		return;
	}

#endif
	it->stack_size++;

	it->handle_stack[it->stack_size] = handle;

	it->dir_len_stack[it->stack_size] = new_directory_len;

	it->has_next = cb_true;
}

CB_INTERNAL void
cb_file_it_close_current_handle(cb_file_it* it)
{
	if (it->handle_stack[it->stack_size] != CB_INVALID_FILE_HANDLE)
	{
#if defined(_WIN32)
		FindClose(it->handle_stack[it->stack_size]);
#else
		closedir(it->handle_stack[it->stack_size]);
#endif
		it->handle_stack[it->stack_size] = CB_INVALID_FILE_HANDLE;
	}
}

CB_INTERNAL const char*
cb_file_it__get_next_entry(cb_file_it* it)
{
	/* No more entries */
	if (it->stack_size == 0) { return NULL; }
#if defined(_WIN32)

	BOOL b = FindNextFileA(it->handle_stack[it->stack_size], &it->find_data);
	if (!b)
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

	return it->find_data != NULL ? it->find_data->d_name : NULL;
#endif
}

CB_INTERNAL cb_bool
cb_file_it__current_entry_is_directory(cb_file_it* it)
{
#if defined(_WIN32)
	return !!(it->find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
#else
	/* @FIXME: in c89 DT_DIR is not defined so we explicitely define it, this is not portable though. */
#ifndef DT_DIR
#define DT_DIR 4
#endif 
	return it->find_data->d_type == DT_DIR;
#endif
}

CB_INTERNAL void
cb_file_it__pop_dir(cb_file_it* it)
{
	cb_size dir_len = 0;

	if (it->stack_size > 0)
	{
		cb_file_it_close_current_handle(it);

		it->stack_size -= 1;

		dir_len = it->dir_len_stack[it->stack_size];
		it->current_file[dir_len] = '\0'; /* set null term char */

		if (it->stack_size == 0)
		{
			it->has_next = cb_false;
		}
	}
}

CB_API void
cb_file_it_init(cb_file_it* it, const char* base_directory)
{
	memset(it, 0, sizeof(cb_file_it));
	it->stack_size = 0;
	it->current_file[it->stack_size] = '\0';
	cb_file_it__push_dir(it, base_directory);
	
	it->recursive = cb_false;
}

CB_API void
cb_file_it_init_recursive(cb_file_it* it, const char* base_directory)
{
	cb_file_it_init(it, base_directory);
	it->recursive = cb_true;
}

CB_API void
cb_file_it_destroy(cb_file_it* it)
{
	while (it->stack_size > 0)
	{
		cb_file_it_close_current_handle(it);
		it->stack_size -= 1;
	}

	it->current_file[0] = '\0';
	it->has_next = cb_false;
}

CB_API const char*
cb_file_it_current_file(cb_file_it* it)
{
	return it->current_file;
}

CB_API cb_bool
cb_file_it_get_next(cb_file_it* it)
{
	cb_bool is_directory = cb_false;
	const char* found = 0;

	CB_ASSERT(it->has_next);

	do
	{
		/* Check if there is remaining/next file, if not, just pop the stack.
		*  Do it multiple times if we are at the end of a directory and a parent directory.
		*/
		while ((found = cb_file_it__get_next_entry(it)) == NULL)
		{
			/* no parent directory so it's the end */
			if (it->stack_size == 0)
			{
				it->has_next = cb_false;
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
	cb_str_append_from(it->current_file, found, it->dir_len_stack[it->stack_size], CB_MAX_PATH);
	
	if (is_directory && it->recursive)
	{
		cb_file_it__push_dir(it, found);
	}

	return cb_true;
}

#endif /* CB_IMPLEMENTATION */