#ifdef WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

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

void file_to_c_str(const char* variable_name, const char* src_file, const char* dst_file);
void assert_path(const char* path);
void assert_process(const char* cmd);
void assert_run(const char* exe);
void assert_file_against_content(const char* expected_file, dstr* expected_content, strv actual_content, bool new_line_insensitive);
void assert_same_file_content(const char* expected_filename, dstr* expected_content, const char* actual_filename, dstr* actual_content, bool new_line_insensitive);
void assert_same_content(strv expected, strv actual, bool new_line_insensitive);
void build_generated_exe_and_run(const char* file);
const char* build_with(const char* config);
void my_project(const char* project_name, const char* toolchain, const char* config);
void test_parse_only(const char* exe, const char* directory);
void test_preprocessor(const char* exe, const char* directory);
void test_error(const char* exe, const char* directory);
void test_generated_source(const char* exe, const char* directory);
void test_program_output(const char* exe, const char* directory);

enum test_type {
	/* Test the content of "file.g.c" against "file.g.c.expect". */
	test_type_SOURCE,
	/* Test the output of the generated program "program.exe". The exit code must be 0. */
	test_type_OUTPUT
};

void test_generated_source_or_program_output(const char* exe, const char* directory, enum test_type type);

int main()
{
	cb_init();

	/* Turn file content into another file containing a static char* with the value of the original file.
	   We do this only once per build. */
	
 	file_to_c_str("static_predefines", "./src/ac/predefines.h", "./src/ac/predefines.g.h");

	build_with("Release");

	cb_clear(); /* Clear all values of cb. */

	const char* ac_exe = build_with("Debug");

	test_parse_only(ac_exe,   "./tests/parse_declarations/");
	test_program_output(ac_exe, "./tests/generate_c/");
	test_preprocessor(ac_exe, "./tests/preprocessor_literals/");
	test_preprocessor(ac_exe, "./tests/preprocessor_splice/");
	test_preprocessor(ac_exe, "./tests/preprocessor_null/");
	test_preprocessor(ac_exe, "./tests/preprocessor_macro/");
    test_preprocessor(ac_exe, "./tests/preprocessor_conditional/");
	test_preprocessor(ac_exe, "./tests/preprocessor_include/");
	test_preprocessor(ac_exe, "./tests/preprocessor_unsupported/");
	test_preprocessor(ac_exe, "./tests/preprocessor_predefine/");
	test_preprocessor(ac_exe, "./tests/preprocessor_line/");
	test_generated_source(ac_exe, "./tests/preprocessor_embed/");

	test_error(ac_exe, "./tests/preprocessor_message/");
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
	cb_toolchain_t toolchain = cb_toolchain_default_c();
	
	/* Build Library */
	{
		my_project("aclib", toolchain.name, config);

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


	/* Build CLI */
	{
		my_project("ac", toolchain.name, config);

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
    
    /* Copy include/ content next to the binary. */

	const char* output_dir = cb_get_output_directory(cb_current_project(), &toolchain);

	char include_output[1024] = {0};
	
	sprintf(include_output, "%s/include/", output_dir);

	if (
		!cb_copy_file_to_dir("./src/ac/include/float.h", include_output)
		|| !cb_copy_file_to_dir("./src/ac/include/stdalign.h", include_output)
		|| !cb_copy_file_to_dir("./src/ac/include/stdarg.h", include_output)
		|| !cb_copy_file_to_dir("./src/ac/include/stdatomic.h", include_output)
		|| !cb_copy_file_to_dir("./src/ac/include/stdbool.h", include_output)
		|| !cb_copy_file_to_dir("./src/ac/include/stddef.h", include_output)
		|| !cb_copy_file_to_dir("./src/ac/include/stdnoreturn.h", include_output)
		|| !cb_copy_file_to_dir("./src/ac/include/varargs.h", include_output))
	{
		exit(1);
	}
	
	return ac_exe;
}

/* Turn file into another file containing a C string literal. */
void file_to_c_str(const char* variable_name, const char* src_filepath, const char* dst_filepath)
{
	dstr src_content;
	dstr_init(&src_content);
	
	dstr dst_content;
	dstr_init(&dst_content);

	if (!re_file_open_and_read(&src_content, src_filepath))
	{
		fprintf(stderr, "Cannot open file to convert: %s\n", src_filepath);
		exit(1);
	}

	const char* c = src_content.data;
	const char* end = src_content.data + src_content.size;

	while (c < end)
	{
		switch (*c)
		{
		case '\r':
		{
			c += 1;
			if (*c != '\n')
			{
				dstr_append_char(&dst_content, '\\');
				dstr_append_char(&dst_content, 'n');
				break; /* Break */
				
			}
			/* FALLTHROUGH  */
		}
		case '\n':
		{
			/* Add '\n' */
			dstr_append_char(&dst_content, '\\');
			dstr_append_char(&dst_content, 'n');
			break;
		}
		case '"':{
		
			dstr_append_char(&dst_content, '/');
			dstr_append_char(&dst_content, '"');
			/* FALLTHROUGH */
			break;
		}
		default:
			dstr_append_char(&dst_content, *c);
		}

		c += 1;
	}
	
	FILE* dst_file = re_file_open_readwrite(dst_filepath);

	fprintf(dst_file, "const char* %s = \"" STRV_FMT "\";", variable_name, STRV_ARG(dstr_to_strv(&dst_content)));

	re_file_close(dst_file);
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

void assert_file_against_content(const char* expected_file, dstr* expected_content, strv actual_content, bool new_line_insensitive)
{
	cb_assert_file_exists(expected_file);

	if (!re_file_open_and_read(expected_content, expected_file))
	{
		fprintf(stderr, "Can't open file: %s\n", expected_file);
		exit(1);
	}

	strv expected_strv = dstr_to_strv(expected_content);
	assert_same_content(expected_strv, actual_content, new_line_insensitive);
}

void assert_same_file_content(const char* expected_filename, dstr* expected_content, const char* actual_filename, dstr* actual_content, bool new_line_insensitive)
{
	cb_assert_file_exists(expected_filename);
	cb_assert_file_exists(actual_filename);

	if (!re_file_open_and_read(expected_content, expected_filename))
	{
		fprintf(stderr, "Can't open file: %s\n", expected_filename);
		exit(1);
	}

	if (!re_file_open_and_read(actual_content, actual_filename))
	{
		fprintf(stderr, "Can't open file: %s\n", actual_filename);
		exit(1);
	}

	strv expected_strv = dstr_to_strv(expected_content);
	strv actual_strv = dstr_to_strv(actual_content);
	assert_same_content(expected_strv, actual_strv, new_line_insensitive);
}

void assert_same_content(strv expected, strv actual, bool new_line_insensitive)
{
	bool is_same = new_line_insensitive
		? strv_equals_newline_insensitive(expected, actual)
		: strv_equals(expected, actual);

	if (!is_same)
	{
		/* In case of failure, display both expected and actual content. */
		fprintf(stderr, "<<<<<<<<<<<<<<<<<<<<<<<<< EXPECTED\n'" STRV_FMT "'\n", STRV_ARG(expected));
		fprintf(stderr, "==================================\n'" STRV_FMT "'\n", STRV_ARG(actual));
		fprintf(stderr, ">>>>>>>>>>>>>>>>>>>>>>>>> ACTUAL  \n'");

		fprintf(stderr, "ERROR: contents do not match.\n");

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

		dstr_assign_f(&expected_filename, "%s.expect", file);

		strv actual_strv = dstr_to_strv(&preprocessor_content);
		assert_file_against_content(expected_filename.data, &expected_content, actual_strv, new_line_insensitive);
	
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
	bool new_line_insensitive = true;
	test_output(exe, directory, output_type_STDOUT, new_line_insensitive);
}

/* Get stderr of preprocess and compare it with the .expect file.
   Comparison is new-line insensitive. */
void test_error(const char* exe, const char* directory)
{
	bool new_line_insensitive = true;
	test_output(exe, directory, output_type_STDERR, new_line_insensitive);
}

void test_generated_source(const char* exe, const char* directory)
{
	test_generated_source_or_program_output(exe, directory, test_type_SOURCE);
}

void test_program_output(const char* exe, const char* directory)
{
	test_generated_source_or_program_output(exe, directory, test_type_OUTPUT);
}

void test_generated_source_or_program_output(const char* exe, const char* directory, enum test_type type)
{
	assert_path(exe);
	assert_path(directory);

	dstr cmd;
	dstr_init(&cmd);
	dstr generated_file_name;
	dstr_init(&generated_file_name);

	dstr expected_filename;
	dstr_init(&expected_filename);

	dstr expected_content;
	dstr_init(&expected_content);

	dstr actual_content;
	dstr_init(&actual_content);

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

		if (type == test_type_SOURCE)
		{
			/* Create string like "XXX.g.c.expect". */
			dstr_assign_f(&expected_filename, "%s.expect", generated_file_name.data);

			assert_same_file_content(expected_filename.data, &expected_content, generated_file_name.data, &actual_content, true);
		}
		else if (type == test_type_OUTPUT)
		{
			/* Test executable outpudt*/
			build_generated_exe_and_run(generated_file_name.data);
		}
		
		printf("OK\n");
	}

	cb_file_it_destroy(&it);
	dstr_destroy(&generated_file_name);
	dstr_destroy(&expected_filename);
	dstr_destroy(&expected_content);
	dstr_destroy(&actual_content);
	
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