#define CB_IMPLEMENTATION
#include <cb/cb.h>
#include <cb/cb_add_files.h>

const char* root_dir = "./";

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
	cb_init();

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

	/* CLI */
	{
		my_project("ac", toolchain_name, config);

		cb_add(cb_LINK_PROJECTS, "aclib");

		cb_add_files_recursive("./src/cli", "*.c");
		cb_set(cb_BINARY_TYPE, cb_EXE);

		cb_add(cb_INCLUDE_DIRECTORIES, "./src/external/re.lib/c");
		cb_add(cb_INCLUDE_DIRECTORIES, "./src/");
	}

	if (!cb_bake("aclib"))
	{
		exit(1);
	}

	const char* ac_exe = cb_bake("ac");
	if (!ac_exe)
	{
		exit(1);
	}

	cb_destroy();

	return ac_exe;
}

void tests(const char* exe);

int main()
{
	build_with("Release");

	const char* exe = build_with("Debug");
	
	tests(exe);
	
	return 0;
}

void assert_path(const char* path)
{
	if (!cb_path_exists(path))
	{
		cb_log_error("Path does not exists: %s", path);
		exit(1);
	}
}
void assert_subprocess(const char* cmd, const char* starting_directory)
{
	if (cb_subprocess(cmd) != 0)
	{
		cb_log_error("Subprocess did not exit with 0: %s", cmd);
		exit(1);
	}
}

void test_directory(const char* exe, const char* directory)
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

		assert_subprocess(buffer.data, directory);

		printf("OK\n");
	}
	
	cb_file_it_destroy(&it);
	cb_dstr_destroy(&buffer);
}

void tests(const char* exe)
{
	test_directory(exe, "./testsuits/");
}