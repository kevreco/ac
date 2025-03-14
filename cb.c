#define CB_IMPLEMENTATION
#include "cb/cb.h"
#include "cb/cb_add_files.h"
#include "cb/cb_assert.h"

/* STRV_IMPLEMENTATION is defined at the bottom of this file. */
#include "src/external/re.lib/c/re/strv.h"
#include "src/external/re.lib/c/re/strv_extensions.h"

/* DSTR_IMPLEMENTATION is defined at the bottom of this file. */
#include "src/external/re.lib/c/re/dstr.h"

/* RE_FILE_IMPLEMENTATION is defined at the bottom of this file. */
#include "src/external/re.lib/c/re/file.h"

const char* root_dir = "./";

/* Forward declarations */

void assert_path(const char* path);
void assert_process(const char* cmd);
void assert_run(const char* exe);
void build_generated_exe_and_run(const char* file);
const char* build_with(const char* config);
void my_project(const char* project_name, const char* toolchain, const char* config);
void test_parse_only(const char* exe, const char* directory);
void test_preprocessor(const char* exe, const char* directory);
void test_error(const char* exe, const char* directory);
void test_c_generation(const char* exe, const char* directory);

int main()
{
	cb_init();

	build_with("Release");

	cb_clear(); /* Clear all values of cb. */

	const char* ac_exe = build_with("Debug");

	test_parse_only(ac_exe,   "./tests/parse_declarations/");
	test_c_generation(ac_exe, "./tests/generate_c/");
	test_preprocessor(ac_exe, "./tests/preprocessor_literals/");
	test_preprocessor(ac_exe, "./tests/preprocessor_splice/");
	test_preprocessor(ac_exe, "./tests/preprocessor_null/");
	test_preprocessor(ac_exe, "./tests/preprocessor_macro/");
    test_preprocessor(ac_exe, "./tests/preprocessor_conditional/");
	test_preprocessor(ac_exe, "./tests/preprocessor_include/");
	test_preprocessor(ac_exe, "./tests/preprocessor_unsupported/");

	test_error(ac_exe, "./tests/errors/preprocessor/");
	test_error(ac_exe, "./tests/errors/parsing/");
	
	/* Test CLI options. */
	
	test_preprocessor(ac_exe, "./tests/options/preprocess/");
	test_preprocessor(ac_exe, "./tests/options/preprocess_preserve_comment/");
	test_preprocessor(ac_exe, "./tests/options/gcc_e/");
	test_preprocessor(ac_exe, "./tests/options/gcc_multiple_short/");

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
		cb_add(cb_CXFLAGS, "/Zi");   /* Produce debugging information (.pdb) */

		/* Use alternate location for the .pdb.
		   In this case it will be next to the .exe */
		cb_add(cb_LFLAGS, "/pdbaltpath:%_PDB%"); 

		if (is_debug)
		{
			cb_add(cb_LFLAGS, "/MANIFEST:EMBED");
			cb_add(cb_LFLAGS, "/INCREMENTAL:NO"); /* No incremental linking */

			cb_add(cb_CXFLAGS, "-Od");   /* Disable optimization */
			cb_add(cb_DEFINES, "DEBUG"); /* Add DEBUG constant define */
		}
		else
		{
			cb_add(cb_CXFLAGS, "-O1");   /* Optimization level 1 */
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
			cb_add(cb_CXFLAGS, "-O1");   /* Optimization level 1 */
		}
	}
}

const char* build_with(const char* config)
{
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
		fprintf(stderr, "Path does not exists: %s\n", path);
		exit(1);
	}
}

void assert_process(const char* cmd)
{
	cb_bool also_get_stderr = cb_false;
	cb_process_handle* process = cb_process_to_string(cmd, NULL, also_get_stderr);

	if (cb_process_end(process) != 0)
	{
		fprintf(stderr, "Process did not exit with 0: %s\n", cmd);
		exit(1);
	}
}

void assert_run(const char* exe)
{
	if (cb_run(exe) != 0)
	{
		fprintf(stderr, "Exe did not exit with 0: %s\n", exe);
		exit(1);
	}
}

void test_parse_only(const char* exe, const char* directory)
{
	assert_path(exe);
	assert_path(directory);

	dstr cmd;
	dstr_init(&cmd);

	cb_file_it it;
	cb_file_it_init(&it, directory);

	while (cb_file_it_get_next_glob(&it, "*.c"))
	{
		const char* file = cb_file_it_current_file(&it);
		
		dstr_assign_f(&cmd, "%s --option-file %soptions.txt %s", exe, directory, file);

		printf("Testing: %s \n", file);

		assert_process(cmd.data);

		printf("OK\n");
	}
	
	cb_file_it_destroy(&it);
	dstr_destroy(&cmd);
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

enum output_type {
	output_type_STDOUT,
	output_type_STDERR
};

void test_output(const char* exe, const char* directory, enum output_type type, bool new_line_insensitive)
{
	assert_path(exe);
	assert_path(directory);

	dstr cmd;
	dstr_init(&cmd);

	dstr expected_filename;
	dstr_init(&expected_filename);

	dstr expected_content;
	dstr_init(&expected_content);

	dstr preprocessor_content;
	dstr_init(&preprocessor_content);

	cb_file_it it;
	cb_file_it_init(&it, directory);

	while (cb_file_it_get_next_glob(&it, "*.c"))
	{
		const char* file = cb_file_it_current_file(&it);

		dstr_assign_f(&cmd, "%s --option-file %soptions.txt %s", exe, directory, file);

		printf("Testing: %s \n", file);

		/* Run compiler and get the preprocessor output. */
		{
			cb_bool also_get_stderr = type == output_type_STDERR;
			cb_process_handle* process = cb_process_to_string(cmd.data, NULL, also_get_stderr);

			const char* o = type == output_type_STDOUT
				? cb_process_stdout_string(process)
				: cb_process_stderr_string(process);

			dstr_assign_f(&preprocessor_content, "%s", o);

			/* Only exit if we are expecting result from stdout */
			if (cb_process_end(process) != 0 
				&& type == output_type_STDOUT)
			{
				exit(1);
			}
		}

		/* Compare output with the expected content. */
		{
			dstr_assign_f(&expected_filename, "%s.expect", file);

			cb_assert_file_exists(expected_filename.data);

			if (!re_file_open_and_read(&expected_content, expected_filename.data))
			{
				fprintf(stderr, "Can't open file: %s\n", expected_filename.data);
				exit(1);
			}

			bool is_same = new_line_insensitive
				? strv_equals_newline_insensitive(dstr_to_strv(&expected_content), dstr_to_strv(&preprocessor_content))
				: strv_equals(dstr_to_strv(&expected_content), dstr_to_strv(&preprocessor_content));

			if (!is_same)
			{
				/* In case of failure, display both expected and actual content. */
				fprintf(stderr,"<<<<<<<<<<<<<<<<<<<<<<<<< EXPECTED\n'" STRV_FMT "'\n", STRV_ARG(expected_content));
				fprintf(stderr,"==================================\n'" STRV_FMT "'\n", STRV_ARG(preprocessor_content));
				fprintf(stderr,">>>>>>>>>>>>>>>>>>>>>>>>> ACTUAL  \n'");

				fprintf(stderr, "ERROR: Preprocessor contents do not match.\n");

				exit(1);
			}
		}

		printf("OK\n");
	}

	cb_file_it_destroy(&it);
	dstr_destroy(&cmd);
	dstr_destroy(&expected_filename);
	dstr_destroy(&expected_content);
	dstr_destroy(&preprocessor_content);
}

/* Get stdout of preprocess and compare it with the .expect file. */
void test_preprocessor(const char* exe, const char* directory)
{
	bool new_line_insensitive = false;
	test_output(exe, directory, output_type_STDOUT, new_line_insensitive);
}

/* Get stderr of preprocess and compare it with the .expect file.
   Comparison is new-line insensitive. */
void test_error(const char* exe, const char* directory)
{
	bool new_line_insensitive = true;
	test_output(exe, directory, output_type_STDERR, new_line_insensitive);
}

void test_c_generation(const char* exe, const char* directory)
{
	assert_path(exe);
	assert_path(directory);

	dstr cmd;
	dstr_init(&cmd);
	dstr generated_file_name;
	dstr_init(&generated_file_name);

	cb_file_it it;
	cb_file_it_init(&it, directory);

	strv c_ext = strv_make_from_str(".c");
	strv gc_ext = strv_make_from_str(".g.c");

	while (next_non_generated_c_file(&it))
	{
		const char* file = cb_file_it_current_file(&it);

		dstr_assign_f(&cmd, "%s --option-file %soptions.txt %s", exe, directory, file);

		printf("Testing: %s \n", file);

		assert_process(cmd.data);

		dstr_assign_f(&generated_file_name, "%s", file);
		/* Replace .c extension with .g.c extension */
		dstr_append_from(&generated_file_name, generated_file_name.size - c_ext.size, gc_ext);

		cb_assert_file_exists(generated_file_name.data);

		build_generated_exe_and_run(generated_file_name.data);

		printf("OK\n");
	}

	cb_file_it_destroy(&it);
	dstr_destroy(&generated_file_name);
	dstr_destroy(&cmd);
}

/* Count generated project to have a unique id. */
static int generated_project_count = 0;

void build_generated_exe_and_run(const char* file)
{
	generated_project_count += 1;

	cb_project_f("generated_%d", generated_project_count);

	cb_set(cb_BINARY_TYPE, cb_EXE);
	cb_add(cb_FILES, file);

	const char* generated_exe = cb_bake();

	cb_assert_file_exists(generated_exe);

	assert_run(generated_exe);
}

#define STRV_IMPLEMENTATION
#include "src/external/re.lib/c/re/strv.h"
#include "src/external/re.lib/c/re/strv_extensions.h"

#define DSTR_IMPLEMENTATION
#include "src/external/re.lib/c/re/dstr.h"

#define RE_FILE_IMPLEMENTATION
#include "src/external/re.lib/c/re/file.h"