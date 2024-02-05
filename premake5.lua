----------------------------------------------------------------
-- Options
----------------------------------------------------------------

newoption {
    trigger = "tests_directory",
	category  = "[AC] for 'run_tests' action",
	default = "./testsuits",
    description = "Directory with (.c) files to be tested.",
}

newoption {
    --  TODO rename this to "compiler_path"
    trigger = "tests_exe",
	category  = "[AC] for 'run_tests' action",
	default = "",
    description = "Executable performing the tests",
}

----------------------------------------------------------------
-- Helpers
----------------------------------------------------------------

function dir_files(dir)
    files { dir .. "/**.hpp", dir .. "/**.cpp", dir .. "/**.h", dir .. "/**.c"}
	filter "system:windows"
		files { dir .. "/**.hpp.win", dir .. "/**.cpp.win", dir .. "/**.h.win", dir .. "/**.c.win"}
	filter "system:unix"
		files { dir .. "/**.hpp.unix", dir .. "/**.cpp.unix", dir .. "/**.h.unix", dir .. "/**.c.unix"}
	filter {} -- clear filter
end

function setup_configuration()
     filter { "configurations:Debug" }
        defines { "DEBUG" }
        symbols "On"
		targetsuffix "debug" -- Add "-debug" to debug versions of files @TODO this does not work for some reason...

    filter { "configurations:Release" }
        defines { "NDEBUG" }
        optimize "On"
		
	filter {} -- clear filter
end

-- =============================================================
-- Build project files
-- =============================================================
-- Example: premake vs2022
-- =============================================================

_location = ".build"; -- location of the generated files (vs, make, etc.)

workspace "ac"
    configurations { "Debug", "Release" }
	location(_location)
	startproject "ac"

project "ac"
    kind "ConsoleApp"
    language "C++"
	location(path.join(_location, "ac")) 
	
	dir_files("./src/cli")
	dir_files("./src/external")
	dir_files("./src/ac")
	
	includedirs { "./src/" }
	includedirs { "./src/external/re.lib/c/" }
	
	targetdir ( path.join(_location, "ac", "bin", "%{cfg.buildcfg}") ) -- location of binaries depending of configuration
	objdir ( path.join(_location, "ac", "obj", "%{cfg.buildcfg}") )    -- location of intermediate files depending of configuration
	
	flags { "FatalWarnings"}
	warnings "Extra"
	
	filter "system:windows"
		defines { "WIN32"}
	filter {}
	
	debugargs { "compile " .. path.getabsolute("./testsuits/empty_main.c") }
	
	-- every time we build we want to check if the compiler actually run the tests
	postbuildcommands  {
		-- premake5 --file="<my_file> run_tests --tests_directory=<dir> --tests_exe=<exe>"
		string.format(
			' "%s" --file="%s" run_tests --tests_directory="%s" --tests_exe="%s"' ,
			_PREMAKE_COMMAND,
			_MAIN_SCRIPT,
			path.getabsolute(_OPTIONS["tests_directory"]),
			"%{cfg.buildtarget.abspath}"
		)
	}
	
-- =============================================================
-- Clean
-- =============================================================
-- Example: premake clean
-- =============================================================

function rmdir(dir)
	if os.isdir(dir) then
		if not os.rmdir(dir) then
			print("cannot remove directory: '" .. dir .. "'")
		end
	end
end

newaction {
	trigger = "clean",
	description = "Clean generated files and directory",
	execute = function ()
		rmdir(_location)
	end
}
-- =============================================================
-- Run Tests
-- =============================================================
-- Example: premake run_tests
-- =============================================================
	
newaction {
	trigger = "run_tests",
	description = "Run tests",
	execute = function ()
	
	    local directory = _OPTIONS["tests_directory"]
		local exe =  _OPTIONS["tests_exe"]
		
		local files = os.matchfiles(directory .. "/*.c")

		if (not os.isdir(directory)) then
		    print("Directory does not exist: '" .. directory .. "'")
		end
		if (not os.isfile(exe)) then
		    print("File does not exist: '" .. exe .. "'")
		end
		if (files == nil) then
		    print("Warning: no file to test in directory '" .. directory .. "'.")
		end
		
		for i, file in ipairs(files) do
		    test_file(exe, path.getabsolute(file))
		end
		
		os.exit(0)
	end
}

function file_to_str(filepath)
    local f = assert(io.open(filepath, "rb"))
    local content = f:read("*all")
    f:close()
    return content
end

function file_str_ignorespace(filepath)
   local str = file_to_str(filepath)
   
   -- remove all spaces
   str = string.gsub(str, "%s+", "")
   
   return str
end

function files_are_different(fileA, fileB)
    -- @FIXME We read the whole file and compare the string, maybe there is a more efficient way to do this.
    local strA = file_str_ignorespace(fileA)
	local strB = file_str_ignorespace(fileB)
	if (strA == strB) then
	    return false;
	end
	return true;
end

function test_file(exe, file)
	print("test_file: " .. file)
	
	local generated = file .. ".generated"
	
	if (os.isfile(generated)) then
		if (files_are_different(file, generated)) then
			print("ERROR:files are different");
			os.exit(-1)
		end
	end
end

function test_files(files)
	for file in files do
        test_file(file)
    end
end