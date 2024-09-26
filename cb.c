#define CB_IMPLEMENTATION
#include <cb/cb.h>
#include <cb/cb_add_files.h>
#include <cb/cb_assert.h>

const char* root_dir = "./";

/* Forward declarations */

void assert_path(const char* path);
void assert_subprocess(const char* cmd);
void assert_run(const char* exe);
void build_generated_exe_and_run(const char* file);
const char* build_with(const char* config);
void my_project(const char* project_name, const char* toolchain, const char* config);
void test_parse_only(const char* exe, const char* directory);
void test_c_generation(const char* exe, const char* directory);

int main()
{
	cb_init();

	build_with("Release");

	const char* ac_exe = build_with("Debug");

	test_parse_only(ac_exe, "./tests/01_parse_only/");

	test_c_generation(ac_exe, "./tests/02_generate_c/");

	cb_destroy();

	return 0;
}


/* Shortcut to create a project with default config flags. */
void my_project(const char* project_name, const char* toolchain, const char* config)
{
	cb_project(project_name);

	cb_set_f(cb_OUTPUT_DIR, ".build/%s_%s/%s/", toolchain, config, project_name);

	cb_bool is_debug = cb_str_equals(config, "Debug");

	if (is_debug
		/* @FIXME sanitize=address require clang with msvc*/
		&& cb_str_equals(toolchain, "gcc")
		)
	{
		cb_add(cb_CXFLAGS, "-fsanitize=address"); /* Address sanitizer, same flag for gcc and msvc. */
	}

	if (cb_str_equals(toolchain, "msvc"))
	{
		/* Don't use full path of the .pdb, only use the filename with the .pdb extension.
		*  The reason is to make the build more deterministic.
		*/
		if (is_debug)
		{
			cb_add(cb_LFLAGS, "/pdbaltpath:%_PDB%");
			cb_add(cb_LFLAGS, "/MANIFEST:EMBED");
			cb_add(cb_LFLAGS, "/INCREMENTAL:NO"); /* No incremental linking */


			cb_add(cb_CXFLAGS, "/Zi");   /* Produce debugging information (.pdb) */
			cb_add(cb_CXFLAGS, "-Od");   /* Disable optimization */
			cb_add(cb_DEFINES, "DEBUG"); /* Add DEBUG constant define */
		}
		else
		{
			cb_add(cb_CXFLAGS, "-O2");   /* Optimization level 2 */
		}
	}
	else if (cb_str_equals(toolchain, "gcc"))
	{
		if (is_debug)
		{
			cb_add(cb_CXFLAGS, "-g");    /* Produce debugging information  */
			cb_add(cb_CXFLAGS, "-p");    /* Profile compilation (in case of performance analysis)  */
			cb_add(cb_CXFLAGS, "-O0");   /* Disable optimization */
			cb_add(cb_DEFINES, "DEBUG"); /* Add DEBUG constant define */
		}
		else
		{
			cb_add(cb_CXFLAGS, "-O2");   /* Optimization level 2 */
		}
	}
}

const char* build_with(const char* config)
{
	cb_clear();

	const char* toolchain_name = cb_toolchain_default().name;

	/* Library */
	{
		my_project("aclib", toolchain_name, config);

		cb_add_files_recursive("./src/ac", "*.c");

		cb_set(cb_BINARY_TYPE, cb_STATIC_LIBRARY);
		cb_add(cb_INCLUDE_DIRECTORIES, "./src/ac");
		cb_add(cb_INCLUDE_DIRECTORIES, "./src/external/re.lib/c");
		cb_add(cb_INCLUDE_DIRECTORIES, "./src/external/re.lib/cpp");
	}

	if (!cb_bake())
	{
		exit(1);
	}


	/* CLI */
	{
		my_project("ac", toolchain_name, config);

		cb_add(cb_LINK_PROJECTS, "aclib");

		cb_add_files_recursive("./src/cli", "*.c");
		cb_set(cb_BINARY_TYPE, cb_EXE);

		cb_add(cb_INCLUDE_DIRECTORIES, "./src/external/re.lib/c");
		cb_add(cb_INCLUDE_DIRECTORIES, "./src/");
	}

	const char* ac_exe = cb_bake();
	if (!ac_exe)
	{
		exit(1);
	}

	return ac_exe;
}

void assert_path(const char* path)
{
	if (!cb_path_exists(path))
	{
		cb_log_error("Path does not exists: %s", path);
		exit(1);
	}
}

void assert_subprocess(const char* cmd)
{
	if (cb_subprocess(cmd) != 0)
	{
		cb_log_error("Subprocess did not exit with 0: %s", cmd);
		exit(1);
	}
}

void assert_run(const char* exe)
{
	if (cb_run(exe) != 0)
	{
		cb_log_error("Exe did not exit with 0: %s", exe);
		exit(1);
	}
}

void test_parse_only(const char* exe, const char* directory)
{
	assert_path(exe);
	assert_path(directory);

	cb_dstr buffer;
	cb_dstr_init(&buffer);

	cb_file_it it;
	cb_file_it_init(&it, directory);

	while (cb_file_it_get_next_glob(&it, "*.c"))
	{
		const char* file = cb_file_it_current_file(&it);
		
		cb_dstr_assign_f(&buffer, "%s --option-file %soptions.txt %s", exe, directory, file);

		printf("Testing: %s \n", file);

		assert_subprocess(buffer.data);

		printf("OK\n");
	}
	
	cb_file_it_destroy(&it);
	cb_dstr_destroy(&buffer);
}


int str_ends_with(const char* str, const char* suffix)
{
	if (!str || !suffix)
		return 0;
	size_t lenstr = strlen(str);
	size_t lensuffix = strlen(suffix);
	if (lensuffix > lenstr)
		return 0;
	return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}

cb_bool next_non_generated_c_file(cb_file_it* it)
{
	while (cb_file_it_get_next_glob(it, "*.c"))
	{
		const char* file = cb_file_it_current_file(it);
		if (!str_ends_with(file, ".g.c"))
		{
			return cb_true;
		}
	}
	return cb_false;
}


void test_c_generation(const char* exe, const char* directory)
{
	assert_path(exe);
	assert_path(directory);

	cb_dstr buffer;
	cb_dstr_init(&buffer);

	cb_file_it it;
	cb_file_it_init(&it, directory);

	cb_strv c_ext = cb_strv_make_str(".c");
	cb_strv gc_ext = cb_strv_make_str(".g.c");

	while (next_non_generated_c_file(&it))
	{
		const char* file = cb_file_it_current_file(&it);

		cb_dstr_assign_f(&buffer, "%s --option-file %soptions.txt %s", exe, directory, file);

		printf("Testing: %s \n", file);

		assert_subprocess(buffer.data);

		cb_dstr_assign_f(&buffer, "%s", file);
		/* Replace .c extension with .g.c extension */
		cb_dstr_append_from(&buffer, buffer.size - c_ext.size, gc_ext.data, gc_ext.size);

		cb_assert_file_exists(buffer.data);

		build_generated_exe_and_run(buffer.data);

		printf("OK\n");
	}

	cb_file_it_destroy(&it);
	cb_dstr_destroy(&buffer);
}

/* Count generated project to have a unique id. */
static int generated_project_count = 0;

void build_generated_exe_and_run(const char* file)
{
	cb_clear();

	generated_project_count += 1;

	cb_project_f("generated_%d", generated_project_count);

	cb_set(cb_BINARY_TYPE, cb_EXE);
	cb_add(cb_FILES, file);

	const char* generated_exe = cb_bake();

	cb_assert_file_exists(generated_exe);

	assert_run(generated_exe);
}