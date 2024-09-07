 @echo OFF
::SETLOCAL EnableDelayedExpansion
cd /D "%~dp0"

@REM This script name. Get name without extension (%~n0) the append ".bat"
set "SCRIPT=%~n0.bat" 


set "help="
@REM use msvc
set "msvc=1"
set "clang="
@REM also run the executable
set "run=1"
@REM input file
set "file=cb.c"
@REM temp directory - NOTE: strings in .bat files requires backslashes
set "tmp=.cb\.tmp"
@REM output directory with exe name - NOTE: strings in .bat files requires backslashes
set "output=.\cb.exe"

:: --------------------------------------------------------------------------------
:: Unpack Arguments
:: --------------------------------------------------------------------------------
echo [%SCRIPT%] input args: %*
:loop
IF NOT "%1"=="" (
    IF "%1"=="help"   set "help=1"
	@REM toolchain
    IF "%1"=="msvc"   set "msvc=1"  && set "clang="
	IF "%1"=="clang"  set "clang=1" && set "msvc="
	@REM options
	IF "%1"=="run"    set "run=1"
    IF "%1"=="--file" set "file=%2" && SHIFT
    IF "%1"=="--tmp"  set "tmp=%2"  && SHIFT

    SHIFT
    GOTO :loop
)

:: --------------------------------------------------------------------------------
:: Display help if needed
:: --------------------------------------------------------------------------------
if "%help%"=="1" (
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

set "C_SOURCE=%file%"
set "C_OBJ_DIR=%tmp%"
set "C_EXE_PATH=%output%"
@REM use a loop just to use the %%XXX function
for %%a in (%file%) do (
    set "basename=%%~na"
) 

:: --------------------------------------------------------------------------------
:: Prepare and clean output directory
:: --------------------------------------------------------------------------------
echo [%SCRIPT%] Clean up output directory "%C_OBJ_DIR%"
if exist %C_EXE_PATH% del %C_EXE_PATH%
if exist %C_OBJ_DIR% rmdir /S /Q %C_OBJ_DIR%
if not exist %C_OBJ_DIR% mkdir %C_OBJ_DIR%

:: --------------------------------------------------------------------------------
:: Compile the builder
:: --------------------------------------------------------------------------------
echo [%SCRIPT%] Compile builder "%C_SOURCE%"

if "%msvc%"=="1" (
    cl.exe  /EHsc /nologo /Zi /utf-8 %INCLUDES% /D UNICODE /D _UNICODE /Fo%C_OBJ_DIR%/ /Fd%C_OBJ_DIR%/  %C_SOURCE% /link /OUT:"%C_EXE_PATH%" /PDB:"%C_OBJ_DIR%/" /ILK:"%C_OBJ_DIR%/%basename%.ilk" 
)

if "%clang%"=="1" (
    echo [%SCRIPT%] ERROR: clang is not suported yet.
    goto clean_up
)

if ERRORLEVEL 1 goto clean_up
 
if "%run%"=="1" (
    echo [%SCRIPT%] Running "%C_EXE_PATH%"
    call "%C_EXE_PATH%"
)

:: --------------------------------------------------------------------------------
:: Clean up this script variable
:: --------------------------------------------------------------------------------
:clean_up
echo [%SCRIPT%] Clean up .bat variables.

set "C_EXE_PATH="
set "C_OBJ_DIR="
set "C_SOURCE="

set "help="
set "msvc"
set "clang="
set "run="
set "file="
set "tmp="
set "output="
set "basename="
set "SCRIPT="

Exit /B 0