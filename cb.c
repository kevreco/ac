#include "string.h"
#include "stdio.h"

#define CB_IMPLEMENTATION
#define CB_EXTENSIONS
#include "cb/cb.h"

int build_static_library();
int build_shared_libraries();
int build_exe_with_dependencies();
int build_with_configs();
int build_with_platform_specific_flags();

const char* root_dir = "./";

int main()
{
	return build_with_platform_specific_flags();
}

int build_static_library()
{
	cb_init();

	{
		cb_project("ac");
		cb_add_files(root_dir, "*/src/ac/*.c");

		cb_set(cbk_BINARY_TYPE, cbk_static_lib);

		cb_add(cbk_INCLUDE_DIR, "./src/ac");
		cb_add(cbk_INCLUDE_DIR, "./src/external/re.lib/c");
		cb_add(cbk_INCLUDE_DIR, "./src/external/re.lib/cpp");
	}

	if (!cb_bake(cb_toolchain_default(), "ac"))
	{
		return -1;
	}

	cb_destroy();

	return 0;
}

int build_shared_libraries()
{
	cb_init();

	{
		cb_project("dyn_lib_a");

		cb_add(cbk_DEFINES, "DYN_LIB_EXPORT");

		cb_add_files(root_dir, "*/src/dyn_lib_a/*.c");

		cb_set(cbk_BINARY_TYPE, cbk_shared_lib);
	}

	{
		cb_project("dyn lib b");

		cb_add(cbk_DEFINES, "DYN_LIB_EXPORT");

		cb_add_files(root_dir, "*/src/dyn lib b/*.c");

		cb_set(cbk_BINARY_TYPE, cbk_shared_lib);
	}

	if (!cb_bake(cb_toolchain_default(), "dyn_lib_a")
		|| !cb_bake(cb_toolchain_default(), "dyn lib b"))
	{
		return -1;
	}

	cb_destroy();

	return 0;
}

int build_exe_with_dependencies()
{
	cb_init();

	/* build a static library */
	{
		cb_project("ac");
		cb_add_files(root_dir, "*/src/ac/*.c");

		cb_set(cbk_BINARY_TYPE, cbk_static_lib);

		cb_add(cbk_INCLUDE_DIR, "./src/ac");
		cb_add(cbk_INCLUDE_DIR, "./src/external/re.lib/c");
		cb_add(cbk_INCLUDE_DIR, "./src/external/re.lib/cpp");
	}

	{
		cb_project("dyn_lib_a");

		cb_add(cbk_DEFINES, "DYN_LIB_EXPORT");

		cb_add_files(root_dir, "*/src/dyn_lib_a/*.c");

		cb_set(cbk_BINARY_TYPE, cbk_shared_lib);
	}
	{
		cb_project("dyn lib b");

		cb_add(cbk_DEFINES, "DYN_LIB_EXPORT");

		cb_add_files(root_dir, "*/src/dyn lib b/*.c");

		cb_set(cbk_BINARY_TYPE, cbk_shared_lib);
	}

	/* build an exe that use a static library and two dynamic libraries */
	{
		cb_project("ac_tester");

		cb_add(cbk_LINK_PROJECT, "ac");
		cb_add(cbk_LINK_PROJECT, "dyn_lib_a");
		cb_add(cbk_LINK_PROJECT, "dyn lib b");

		cb_add_files(root_dir, "*/src/tester/*.c");

		cb_set(cbk_BINARY_TYPE, cbk_exe);

		cb_add(cbk_INCLUDE_DIR, "./src/dyn_lib_a");
		cb_add(cbk_INCLUDE_DIR, "./src/dyn lib b");
		cb_add(cbk_INCLUDE_DIR, "./src/external/re.lib/c");
	}


	if (!cb_bake(cb_toolchain_default(), "ac"))
	{
		return -1;
	}

	if (!cb_bake(cb_toolchain_default(), "dyn_lib_a"))
	{
		return -1;
	}

	if (!cb_bake(cb_toolchain_default(), "dyn lib b"))
	{
		return -1;
	}

	if (!cb_bake_and_run(cb_toolchain_default(), "ac_tester"))
	{
		return -1;
	}

	cb_destroy();

	return 0;
}

int build_ex(const char* arch, const char* config)
{
	cb_init();
	
	const char* toolchain_name = cb_toolchain_default().name;
	const char* project_name;
	/* build a static library */
	{
		project_name = "ac";
		cb_project(project_name);
		cb_set_f(cbk_OUTPUT_DIR, ".build/%s_%s_%s/%s/", toolchain_name, arch, config, project_name);
		
		cb_add_files(root_dir,  "*/src/ac/*.c");

		cb_set(cbk_BINARY_TYPE, cbk_static_lib);
		cb_add(cbk_INCLUDE_DIR, "./src/ac");
		cb_add(cbk_INCLUDE_DIR, "./src/external/re.lib/c");
		cb_add(cbk_INCLUDE_DIR, "./src/external/re.lib/cpp");
	}
	
	{
		project_name = "dyn_lib_a";
		cb_project(project_name);
		cb_set_f(cbk_OUTPUT_DIR, ".build/%s_%s_%s/%s/", toolchain_name, arch, config, project_name);

		cb_add(cbk_DEFINES, "DYN_LIB_EXPORT");

		cb_add_files(root_dir, "*/src/dyn_lib_a/*.c");

		cb_set(cbk_BINARY_TYPE, cbk_shared_lib);
	}
	{
		project_name = "dyn lib b";
		cb_project(project_name);
		cb_set_f(cbk_OUTPUT_DIR, ".build/%s_%s_%s/%s/", toolchain_name, arch, config, project_name);

		cb_add(cbk_DEFINES, "DYN_LIB_EXPORT");

		cb_add_files(root_dir, "*/src/dyn lib b/*.c");

		cb_set(cbk_BINARY_TYPE, cbk_shared_lib);
	}
	
	/* build an exe that use a static library and two dynamic libraries */
	{
		project_name = "ac_tester";
		cb_project(project_name);
		cb_set_f(cbk_OUTPUT_DIR, ".build/%s_%s_%s/%s/", toolchain_name, arch, config, project_name);

		cb_add(cbk_LINK_PROJECT, "ac");
		cb_add(cbk_LINK_PROJECT, "dyn_lib_a");
	    cb_add(cbk_LINK_PROJECT, "dyn lib b");
	
		cb_add_files(root_dir, "*/src/tester/*.c");
		cb_set(cbk_BINARY_TYPE, cbk_exe);
		
		cb_add(cbk_INCLUDE_DIR, "./src/dyn_lib_a");
		cb_add(cbk_INCLUDE_DIR, "./src/dyn lib b");
		cb_add(cbk_INCLUDE_DIR, "./src/external/re.lib/c");
	}
	
	if (!cb_bake(cb_toolchain_default(), "ac"))
	{
		return -1;
	}
	if (!cb_bake(cb_toolchain_default(), "dyn_lib_a"))
	{
		return -1;
	}
	if (!cb_bake(cb_toolchain_default(), "dyn lib b"))
	{
		return -1;
	}
	if (cb_bake_and_run(cb_toolchain_default(), "ac_tester"))
	{
		return -1;
	}
	
	cb_destroy();

	return 0;
}

int build_with_configs()
{
	const char* end = 0;
	const char* arch[] = { "x86",  "x64", end };
	const char* config[] = { "Release", "Debug", end };

	const char** a;
	for (a = arch; *a != end; a++)
	{
		const char** c;
		for (c = config; *c != end; c++)
		{
			if (build_ex(*a, *c) != 0)
			{
				return -1;
			}
		}
	}

	return 0;
}

/* shortcut to create a project with default config flags */
void my_project(const char* project_name, const char* toolchain, const char* arch, const char* config)
{
	cb_project(project_name);

	cb_set_f(cbk_WORKING_DIRECTORY, ".build/%s_%s_%s", toolchain, arch, config);
	cb_set_f(cbk_OUTPUT_DIR, ".build/%s_%s_%s/%s/", toolchain, arch, config, project_name);

	/* Defines the MESSAGE constant define which is used for the dynamic library samples */
	cb_add_f(cbk_DEFINES, "MESSAGE=%s_%s_%s", toolchain, arch, config);

	cb_bool is_debug = cb_str_equals(config, "Debug");

	if (is_debug)
	{
		cb_add(cbk_CXFLAGS, "-fsanitize=address"); /* Address sanitizer, same flag for gcc and msvc. */
	}

	if (cb_str_equals(toolchain, "msvc"))
	{
		/* Don't use full path of the .pdb, only use the filename with the .pdb extension.
		*  The reason is to make the build more deterministic.
		*/
		cb_add(cbk_LFLAGS, "/pdbaltpath:%_PDB%"); 
		cb_add(cbk_LFLAGS, "/MANIFEST:EMBED");
		cb_add(cbk_LFLAGS, "/INCREMENTAL:NO"); /* No incremental linking */
		
		if (is_debug)
		{
			cb_add(cbk_CXFLAGS, "/Zi");   /* Produce debugging information (.pdb) */
			cb_add(cbk_CXFLAGS, "-Od");   /* Disable optimization */
			cb_add(cbk_DEFINES, "DEBUG"); /* Add DEBUG constant define */
		}
		else
		{
			cb_add(cbk_CXFLAGS, "-O2");   /* Optimization level 2 */
		}
	}
	else if (cb_str_equals(toolchain, "gcc"))
	{
		if (is_debug)
		{
			cb_add(cbk_CXFLAGS, "-g");    /* Produce debugging information  */
			cb_add(cbk_CXFLAGS, "-p");    /* Profile compilation (in case of performance analysis)  */
			cb_add(cbk_CXFLAGS, "-O0");   /* Disable optimization */
			cb_add(cbk_DEFINES, "DEBUG"); /* Add DEBUG constant define */
		}
		else
		{
			cb_add(cbk_CXFLAGS, "-O2");   /* Optimization level 2 */
		}
	}
}

int build_ex_with_platform_specific_flags(const char* arch, const char* config)
{
	cb_init();

	const char* toolchain_name = cb_toolchain_default().name;
	const char* project_name;

	{
		my_project("ac", toolchain_name, arch, config);

		cb_add_files(root_dir, "*/src/ac/*.c");

		cb_set(cbk_BINARY_TYPE, cbk_static_lib);
		cb_add(cbk_INCLUDE_DIR, "./src/ac");
		cb_add(cbk_INCLUDE_DIR, "./src/external/re.lib/c");
		cb_add(cbk_INCLUDE_DIR, "./src/external/re.lib/cpp");
	}

	{
		my_project("dyn_lib_a", toolchain_name, arch, config);

		cb_add(cbk_DEFINES, "DYN_LIB_EXPORT");

		cb_add_files(root_dir, "*/src/dyn_lib_a/*.c");

		cb_set(cbk_BINARY_TYPE, cbk_shared_lib);
	}
	{
		my_project("dyn lib b", toolchain_name, arch, config);

		cb_add(cbk_DEFINES, "DYN_LIB_EXPORT");

		cb_add_files(root_dir, "*/src/dyn lib b/*.c");

		cb_set(cbk_BINARY_TYPE, cbk_shared_lib);
	}

	/* build an exe that use a static library and two dynamic libraries */
	{
		my_project("ac_tester", toolchain_name, arch, config);

		cb_add(cbk_LINK_PROJECT, "ac");
		cb_add(cbk_LINK_PROJECT, "dyn_lib_a");
		cb_add(cbk_LINK_PROJECT, "dyn lib b");

		cb_add_files(root_dir, "*/src/tester/*.c");
		cb_set(cbk_BINARY_TYPE, cbk_exe);

		cb_add(cbk_INCLUDE_DIR, "./src/dyn_lib_a");
		cb_add(cbk_INCLUDE_DIR, "./src/dyn lib b");
		cb_add(cbk_INCLUDE_DIR, "./src/external/re.lib/c");
	}

	if (!cb_bake(cb_toolchain_default(), "ac"))
	{
		return -1;
	}

	if (!cb_bake(cb_toolchain_default(), "dyn_lib_a"))
	{
		return -1;
	}

	if (!cb_bake(cb_toolchain_default(), "dyn lib b"))
	{
		return -1;
	}

	if (!cb_bake_and_run(cb_toolchain_default(), "ac_tester"))
	{
		return -1;
	}

	cb_destroy();
	return 0;

}

int build_with_platform_specific_flags()
{
	const char* end = 0;
	const char* arch[] = { "x86", "x64", end };
	const char* config[] = { "Release", "Debug", end };

	const char** a;
	for (a = arch; *a != end; a++)
	{
		const char** c;
		for (c = config; *c != end; c++)
		{
			if (build_ex_with_platform_specific_flags(*a, *c) != 0)
			{
				return -1;
			}
		}
	}

	return 0;
}