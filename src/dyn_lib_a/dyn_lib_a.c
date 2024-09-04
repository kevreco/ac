#include "dyn_lib_a.h"

/* Print value of constant defines */
#define XSTR(x) STR(x)
#define STR(x) #x

#ifndef MESSAGE
#define MESSAGE "World"
#endif

const char* dyn_lib_a_get_string()
{
	return "Hello " XSTR(MESSAGE) " from dyn_lib A\n";
}

unsigned dyn_lib_a_get_int()
{
		return 42;
}