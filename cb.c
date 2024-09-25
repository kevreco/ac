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

int build_with(const char* config)
{
	cb_init();

	const char* toolchain_name = cb_toolchain_default().name;

	{
		my_project("ac", toolchain_name, config);

		cb_add_files_recursive("./src/ac", "*.c");

		cb_set(cb_BINARY_TYPE, cb_STATIC_LIBRARY);
		cb_add(cb_INCLUDE_DIRECTORIES, "./src/ac");
		cb_add(cb_INCLUDE_DIRECTORIES, "./src/external/re.lib/c");
		cb_add(cb_INCLUDE_DIRECTORIES, "./src/external/re.lib/cpp");
	}

	/* build an exe that use a static library and two dynamic libraries */
	{
		my_project("ac_tester", toolchain_name, config);

		cb_add(cb_LINK_PROJECTS, "ac");

		cb_add_files_recursive("./src/tester", "*.c");
		cb_set(cb_BINARY_TYPE, cb_EXE);

		cb_add(cb_INCLUDE_DIRECTORIES, "./src/external/re.lib/c");
	}

	if (!cb_bake("ac"))
	{
		return -1;
	}

	const char* s = cb_bake("ac_tester");
	if (cb_run(s) != 0)
	{
		return -1;
	}

	cb_destroy();
	return 0;

}

int main()
{
	char* config[] = { "Release", "Debug", NULL };

	char** c = config;
	while (*c != 0)
	{
		if (build_with(*c) != 0)
		{
			return -1;
		}
		++c;
	}
	
	return 0;
}