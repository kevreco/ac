#define foo
int a = 1;
void main() {
#ifdef foo
    a = 0;
#endif
#ifdef bar
    a = 2;
#endif
    return a;
}