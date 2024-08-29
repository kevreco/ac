#include "string.h"
#include "stdio.h"

#define RE_CB_IMPLEMENTATION
#include "cb.h"


int main(int argc, const char* args[])
{
	cb_init(argc, args);
	
	/* build a static library */
	{
		cb_project("ac");

		/* @TODO this should be "src/ac/*.c" but we need to create function to compare paths that does not discriminate slashes */
		cb_add_files("*/src/ac/*.c");

		cb_set(cbk_BINARY_TYPE, cbk_static_lib);
		/* @TODO create cb_add_many() function and cb_add_v() macro ?*/
		cb_add(cbk_INCLUDE_DIR, "./src/ac");
		cb_add(cbk_INCLUDE_DIR, "./src/external/re.lib/c");
		cb_add(cbk_INCLUDE_DIR, "./src/external/re.lib/cpp");
	}
	
	{
		cb_project("dyn_lib_a");

		cb_add(cbk_DEFINES, "DYN_LIB_EXPORT");
		/* @TODO this should be "src/ac/*.c" but we need to create function to compare paths that does not discriminate slashes */
		cb_add_files("*/src/dyn_lib_a/*.c");

		cb_set(cbk_BINARY_TYPE, cbk_shared_lib);
	}
	{
		cb_project("dyn_lib_b");

		cb_add(cbk_DEFINES, "DYN_LIB_EXPORT");
		/* @TODO this should be "src/ac/*.c" but we need to create function to compare paths that does not discriminate slashes */
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
	
	return 0;
}