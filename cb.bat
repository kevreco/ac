 @echo OFF
::SETLOCAL EnableDelayedExpansion
cd /D "%~dp0"

@REM This script name. Get name without extension (%~n0) and append ".bat"
set "cb_script=%~n0.bat" 

set "cb_help="
@REM use msvc
set "cb_msvc=1"
set "cb_clang="
@REM also run the executable
set "cb_run=1"
@REM input file
set "cb_file=cb.c"
@REM temp directory - NOTE: strings in .bat files requires backslashes
set "cb_tmp_dir=.build\cb"
@REM output path with exe name - NOTE: strings in .bat files requires backslashes
set "cb_output=.\cb.exe"

@REM --------------------------------------------------------------------------------
@REM Parse Arguments
@REM --------------------------------------------------------------------------------
echo [%cb_script%] input args: %*
:loop
IF NOT "%1"=="" (
    IF "%1"=="help"      set "cb_help=1"
	@REM toolchain       
    IF "%1"=="msvc"      set "cb_msvc=1"  && set "cb_clang="
	IF "%1"=="clang"     set "cb_clang=1" && set "cb_msvc="
	@REM options         
	IF "%1"=="run"       set "cb_run=1"
    IF "%1"=="--file"    set "cb_file=%2" && SHIFT
    IF "%1"=="--tmp-dir" set "cb_tmp_dir=%2"  && SHIFT
	IF "%1"=="--output"  set "cb_output=%2" && SHIFT

    SHIFT
    GOTO :loop
)

@REM --------------------------------------------------------------------------------
@REM Display help if needed
@REM --------------------------------------------------------------------------------
if "%cb_help%"=="1" (
echo usage:
echo.
echo Compile the builder
echo %0 ac [help] [msvc] [run] [--file [filename]] [--output [path]] [--tmp [directory]]
echo.   
echo    help                  Display help
echo    msvc                  Using MSVC, the only option supported so far. [default]
echo    run                   Run the builder once it's compiled [default]
echo    --file   [filename]   Input c file to compile.
echo    --output [path]       The output full path of the generated executable
echo    --tmp    [directory]  Temporary directory for intermediate objects.
echo.

Exit /B 5
)

@REM use a loop just to use the %%XXX function
for %%a in (%cb_file%) do (
    set "cb_basename=%%~na"
) 

@REM --------------------------------------------------------------------------------
@REM Prepare and clean output directory
@REM --------------------------------------------------------------------------------
echo [%cb_script%] Clean up output directory "%cb_tmp_dir%"
if exist %cb_output% del %cb_output%
if exist %cb_tmp_dir% rmdir /S /Q %cb_tmp_dir%
if not exist %cb_tmp_dir% mkdir %cb_tmp_dir%

@REM --------------------------------------------------------------------------------
@REM Compile the builder
@REM --------------------------------------------------------------------------------

if "%cb_msvc%"=="1" (
    echo [%cb_script%] Compile builder "%cb_file%"
	
    cl.exe /EHsc /nologo /Zi /utf-8 /D UNICODE /D _UNICODE /Fo%cb_tmp_dir%/ /Fd%cb_tmp_dir%/  %cb_file% /link /OUT:"%cb_output%" /PDB:"%cb_tmp_dir%/" /ILK:"%cb_tmp_dir%/%cb_basename%.ilk" || goto clean_up
)

if "%cb_clang%"=="1" (
    echo [%cb_script%] ERROR: clang is not suported yet.
    goto clean_up
)

if ERRORLEVEL 1 goto clean_up
 
if "%cb_run%"=="1" (
    echo [%cb_script%] Running "%cb_output%"
	"%cb_output%" || goto clean_up
)

@REM --------------------------------------------------------------------------------
@REM Clean up this script variable
@REM --------------------------------------------------------------------------------
:clean_up
echo [%cb_script%] Clean up .bat variables.

Set "cb_basename="
set "cb_output="
set "cb_tmp_dir="
set "cb_file="
set "cb_run="
set "cb_clang="
set "cb_msvc"
set "cb_help="
set "cb_script="

Exit /B 0