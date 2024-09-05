#!/bin/bash
cd "$(dirname "$0")"

gcc=1
compiler="${CC:-gcc}"
run=1
file=cb.c
tmp=".cb/.tmp"
output="./cb"

while (( "$#" )); do

    echo "$1"
    if [ "$1" == "clang" ];  then clang=1; compiler="${CC:-clang}"; unset gcc; fi
    if [ "$1" == "gcc" ];    then gcc=1;   compiler="${CC:-gcc}";   unset clang; fi
    if [ "$1" == "help" ];   then help=1; fi
    if [ "$1" == "run" ];    then run=1; fi    
    if [ "$1" == "--file" ]; then file=$2; shift; fi
    if [ "$1" == "--output" ]; then output=$2; shift; fi    
    if [ "$1" == "--tmp" ];  then tmp=$2; shift; fi    
    
    shift

done


if [ -v gcc ]; then
   $compiler -g -o $output -O0 ./cb.c 
fi

# exit code not equal to 0
if [ $? -ne 0 ]; then echo "ERROR: Could not run the compiler command properly."; exit 1; fi

if [ -v run ]; then
   exec $output
fi

if [ $? -ne 0 ]; then echo "ERROR: Could not run the builder properly."; exit 1; fi
