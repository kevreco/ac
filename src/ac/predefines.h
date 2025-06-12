/*
    Predefined macros include at compile-time.
*/

#if defined _WIN32
    #define __declspec(x) __attribute__((x))
    #define __cdecl
#endif