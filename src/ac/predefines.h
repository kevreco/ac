/*
    Predefined macros include at compile-time.
*/

#if defined _WIN32
    #define __declspec(x) __attribute__((x))
    #define __cdecl
#endif

#define __STDC_NO_ATOMICS__
#define __STDC_NO_COMPLEX__
#define __STDC_NO_THREADS__
#define __STDC_NO_VLA__