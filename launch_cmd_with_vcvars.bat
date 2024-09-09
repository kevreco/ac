@echo off
@REM :: Support !! syntax for delayed variable expansion.
setlocal enabledelayedexpansion

set "MY_VCVARS_PATH="

:: Search for the default VS installation path.
for %%x in ("%PROGRAMFILES(X86)%" "%PROGRAMFILES%") do (
  for %%y in (2022 2019 2017) do (
    for %%z in (Professional Enterprise Community BuildTools) do (

	  if exist "%%~x\Microsoft Visual Studio\%%y\%%z\VC\Auxiliary\Build\%vcvarsbat%" (
	     set "MY_VCVARS_PATH=%%~x\Microsoft Visual Studio\%%y\%%z\VC\Auxiliary\Build\%vcvarsbat%"
		 
		goto :end_of_loop
	  )
    )
  )
)

echo ERROR: vcvars path not found

:end_of_loop

echo VC VARS path found : %MY_VCVARS_PATH%

cmd /k ""%MY_VCVARS_PATH%\vcvarsall.bat"" x64