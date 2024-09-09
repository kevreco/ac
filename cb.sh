#!/bin/bash
cd "$(dirname "$0")"

cb_gcc=1
cb_compiler="${CC:-gcc}"
cb_run=1
cb_file="./cb.c"   # Source file name to compile, , relative to tmp_dir
cb_output="./cb"   # Executable name, relative to tmp_dir

while (( "$#" )); do

    echo "$1"
    if [ "$1" == "clang" ];  then clang=1; cb_compiler="${CC:-clang}"; unset cb_gcc; fi
    if [ "$1" == "gcc" ];    then cb_gcc=1;   cb_compiler="${CC:-gcc}";   unset clang; fi
    if [ "$1" == "help" ];   then cb_help=1; fi
    if [ "$1" == "run" ];    then cb_run=1; fi    
    if [ "$1" == "--file" ]; then cb_file=$2; shift; fi
    if [ "$1" == "--output" ]; then cb_output=$2; shift; fi    
    shift

done

# Check if there is a value in cb_gcc
if [ -v cb_gcc ]; then
   $cb_compiler -std=c89 -g -o $cb_output -O0 $cb_file
fi

# exit code not equal to 0
if [ $? -ne 0 ]; then echo "ERROR: Could not run the compiler command properly."; exit 1; fi

# Check if there is a value in cb_run
if [ -v cb_run ]; then
   exec $cb_output
fi

if [ $? -ne 0 ]; then echo "ERROR: Could not run the builder properly."; exit 1; fi
