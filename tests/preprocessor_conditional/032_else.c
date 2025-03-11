int main() {
#if 0
#define A() 1
#else
#define A() 0
#endif
#if 1
#define B() 0
#else
#define B() 2
#endif
    return A() + B();
}