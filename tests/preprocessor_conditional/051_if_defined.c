#define foo
int a = 1;
void main() {
#if defined foo
    a = 0;
#endif
#if defined bar
    a = 2;
#endif
    return a;
}