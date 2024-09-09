#include "dyn_lib_b.h"

/* Print value of constant defines */
#define XSTR(x) STR(x)
#define STR(x) #x


#ifndef MESSAGE
#define MESSAGE "World"
#endif

const char* dyn_lib_b_get_string()
{
	return "Hello " XSTR(MESSAGE) " from dyn_lib B\n";
}

unsigned dyn_lib_b_get_int()
{
		return 43;
}