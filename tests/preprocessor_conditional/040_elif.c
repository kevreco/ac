int a = 1;
int b = 2;
int c = 3;
int d = 4;
int main() {
#if 0
    a = 5;
#elif 0
    a = 6;
#else
    a = 0;
#endif
#if 0
    b = 7;
#elif 1
    b = 0;
#else
    b = 8;
#endif
#if 1
    c = 0;
#elif 0
    c = 9;
#else
    c = 10;
#endif
#if 1
    d = 0;
#elif 1
    d = 11;
#else
    d = 12;
#endif
    return a + b + c + d;
}