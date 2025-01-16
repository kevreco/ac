int a = 1;
int b = 2;
int main() {
#if 0
    a = 3;
#else
    a = 0;
#endif
#if 1
    b = 0;
#else
    b = 4;
#endif
    return a + b;
}