#!/bin/bash

cb_root=$(dirname $(realpath -- "$0"))
cb_starting_dir=$(dirname $PWD)
cb_gcc=1
cb_compiler="${CC:-gcc}"
cb_run=1
cb_file="$(realpath -- cb.c)"  # Source file name to compile
cb_output="./cb.bin"           # Executable name
cb_include_dir="$cb_root/"     # Include directory to locate the cb.h file

while (( "$#" )); do
    if [ "$1" == "clang" ];  then cb_clang=1; cb_compiler="${CC:-clang}"; unset cb_gcc; fi
    if [ "$1" == "gcc" ];    then cb_gcc=1;   cb_compiler="${CC:-gcc}";   unset cb_clang; fi
    if [ "$1" == "help" ];   then cb_help=1; fi
    if [ "$1" == "run" ];    then cb_run=1; fi
    if [ "$1" == "--pedantic" ]; then cb_pedantic=1; fi
    if [ "$1" == "--file" ]; then cb_file=$2; shift; fi
    if [ "$1" == "--output" ]; then cb_output=$2; shift; fi 
    if [ "$1" == "--include-dir" ]; then cb_include_dir=$2; shift; fi
    shift
done

# Extract parent directory of the cb.c file path
cb_dir=${cb_file%/*}
# Extract filename of the file path
cb_filename=$(basename "$cb_file")

cd "$cb_dir"

cleanup() {
  cd "$cb_starting_dir"
}

# Restore initial directory on exit
trap cleanup EXIT

# Remove previous executable if it exists.
if [ -f "$cb_output" ]; then
   rm "$cb_output"
fi

if [ -v cb_gcc ] && [ -v cb_pedantic ]; then cb_cxflags="-std=c89 -pedantic -Werror -Wextra" ; fi

# Check if there is a value in cb_gcc.
if [ -v cb_gcc ]; then
   $cb_compiler $cb_cxflags -g -I $cb_include_dir -o "$cb_output" -O0 $cb_filename || { echo "'$cb_compiler' exited with $?"; exit 1; }
fi

# Check if there is a value in cb_run.
if [ -v cb_run ]; then
   "$cb_output" || { echo "'$cb_output' exited with $?"; exit 1; }
fi
