#include "string.h"
#include "stdio.h"

#define RE_CB_IMPLEMENTATION
#include "cb.h"

void build_static_library();
void build_shared_libraries();
void build_exe_with_dependencies();
void build_with_configs();

int main()
{
	build_with_configs();
	
	return 0;
}

void build_static_library()
{
	cb_init();

	{
		cb_project("ac");
		cb_add_files("*/src/ac/*.c");

		cb_set(cbk_BINARY_TYPE, cbk_static_lib);

		cb_add(cbk_INCLUDE_DIR, "./src/ac");
		cb_add(cbk_INCLUDE_DIR, "./src/external/re.lib/c");
		cb_add(cbk_INCLUDE_DIR, "./src/external/re.lib/cpp");
	}

	cb_bake(cb_toolchain_default(), "ac");

	cb_destroy();
}

void build_shared_libraries()
{
	cb_init();

	{
		cb_project("dyn_lib_a");

		cb_add(cbk_DEFINES, "DYN_LIB_EXPORT");

		cb_add_files("*/src/dyn_lib_a/*.c");

		cb_set(cbk_BINARY_TYPE, cbk_shared_lib);
	}

	{
		cb_project("dyn_lib_b");

		cb_add(cbk_DEFINES, "DYN_LIB_EXPORT");

		cb_add_files("*/src/dyn_lib_b/*.c");

		cb_set(cbk_BINARY_TYPE, cbk_shared_lib);
	}

	cb_bake(cb_toolchain_default(), "dyn_lib_a");
	cb_bake(cb_toolchain_default(), "dyn_lib_b");

	cb_destroy();
}

void build_exe_with_dependencies()
{
	cb_init();

	/* build a static library */
	{
		cb_project("ac");
		cb_add_files("*/src/ac/*.c");

		cb_set(cbk_BINARY_TYPE, cbk_static_lib);

		cb_add(cbk_INCLUDE_DIR, "./src/ac");
		cb_add(cbk_INCLUDE_DIR, "./src/external/re.lib/c");
		cb_add(cbk_INCLUDE_DIR, "./src/external/re.lib/cpp");
	}

	{
		cb_project("dyn_lib_a");

		cb_add(cbk_DEFINES, "DYN_LIB_EXPORT");

		cb_add_files("*/src/dyn_lib_a/*.c");

		cb_set(cbk_BINARY_TYPE, cbk_shared_lib);
	}
	{
		cb_project("dyn_lib_b");

		cb_add(cbk_DEFINES, "DYN_LIB_EXPORT");

		cb_add_files("*/src/dyn_lib_b/*.c");

		cb_set(cbk_BINARY_TYPE, cbk_shared_lib);
	}

	/* build an exe that use a static library and two dynamic libraries */
	{
		cb_project("ac_tester");

		cb_add(cbk_LINK_PROJECT, "ac");
		cb_add(cbk_LINK_PROJECT, "dyn_lib_a");
		cb_add(cbk_LINK_PROJECT, "dyn_lib_b");

		cb_add_files("*/src/tester/*.c");

		cb_set(cbk_BINARY_TYPE, cbk_exe);

		cb_add(cbk_INCLUDE_DIR, "./src/dyn_lib_a");
		cb_add(cbk_INCLUDE_DIR, "./src/dyn_lib_b");
		cb_add(cbk_INCLUDE_DIR, "./src/external/re.lib/c");
	}


	cb_bake(cb_toolchain_default(), "ac");
	cb_bake(cb_toolchain_default(), "dyn_lib_a");
	cb_bake(cb_toolchain_default(), "dyn_lib_b");
	cb_bake_and_run(cb_toolchain_default(), "ac_tester");

	cb_destroy();
}

void build_ex(const char* arch, const char* config)
{
	cb_init();
	
	const char* toolchain_name = cb_toolchain_default().name;
	const char* project_name;
	/* build a static library */
	{
		project_name = "ac";
		cb_project(project_name);
		cb_set_f(cbk_OUTPUT_DIR, ".build/%s_%s_%s/%s/", toolchain_name, arch, config, project_name);
		
		cb_add_files("*/src/ac/*.c");

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

		cb_add_files("*/src/dyn_lib_a/*.c");

		cb_set(cbk_BINARY_TYPE, cbk_shared_lib);
	}
	{
		project_name = "dyn_lib_b";
		cb_project(project_name);
		cb_set_f(cbk_OUTPUT_DIR, ".build/%s_%s_%s/%s/", toolchain_name, arch, config, project_name);

		cb_add(cbk_DEFINES, "DYN_LIB_EXPORT");

		cb_add_files("*/src/dyn_lib_b/*.c");

		cb_set(cbk_BINARY_TYPE, cbk_shared_lib);
	}
	
	/* build an exe that use a static library and two dynamic libraries */
	{
		project_name = "ac_tester";
		cb_project(project_name);
		cb_set_f(cbk_OUTPUT_DIR, ".build/%s_%s_%s/%s/", toolchain_name, arch, config, project_name);

		cb_add(cbk_LINK_PROJECT, "ac");
		cb_add(cbk_LINK_PROJECT, "dyn_lib_a");
	    cb_add(cbk_LINK_PROJECT, "dyn_lib_b");
	
		cb_add_files("*/src/tester/*.c");
		cb_set(cbk_BINARY_TYPE, cbk_exe);
		
		cb_add(cbk_INCLUDE_DIR, "./src/dyn_lib_a");
		cb_add(cbk_INCLUDE_DIR, "./src/dyn_lib_b");
		cb_add(cbk_INCLUDE_DIR, "./src/external/re.lib/c");
	}
	
	cb_bake(cb_toolchain_default(), "ac");
	cb_bake(cb_toolchain_default(), "dyn_lib_a");
	cb_bake(cb_toolchain_default(), "dyn_lib_b");
	cb_bake_and_run(cb_toolchain_default(), "ac_tester");
	
	cb_destroy();
}

void build_with_configs()
{
	const char* end = 0;
	const char* arch[] = { "x86",  "x64", end };
	const char* config[] = { "Release", "Debug", end };

	for (const char** a = arch; *a != end; a++)
	{
		for (const char** c = config; *c != end; c++)
		{
			build_ex(*a, *c);
		}
	}
}