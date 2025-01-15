int a = 1;
int main() {
#if 0
    a = 2;
#if 0
    a = 3;
#endif
#endif
#if 0
/*
#endif */
//#endif
// The statement below is equivalent to: a = "foo\"#endif"
// The #endif being part of the string must be ignored by the preprocessor.
    a = "foo\\
"\
#endif";
    a = 4;
#endif
#if 1
#if 2
    a = 0;
#endif
#endif
    return a;
}