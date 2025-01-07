int a = 0;
int main() {
#define foo 0
#define bar() 0
#if foo
    a = 1;
#endif
#if bar()
    a = 2;
#endif
#if baz
    a = 3;
#endif
    return a;
}