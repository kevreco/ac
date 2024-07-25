#pragma once

#ifdef __cplusplus
#define DYN_LIB_EXTERN_C extern "C"
#else
#define DYN_LIB_EXTERN_C
#endif

#ifdef _WIN32

#ifdef DYN_LIB_EXPORT
	#define DYN_LIB_API DYN_LIB_EXTERN_C __declspec(dllexport)
	#else
	#define DYN_LIB_API DYN_LIB_EXTERN_C __declspec(dllimport)
#endif

#else
	#define DYN_LIB_API DYN_LIB_EXTERN_C
#endif

DYN_LIB_API const char* dyn_lib_b_get_string();

DYN_LIB_API unsigned dyn_lib_b_get_int();
