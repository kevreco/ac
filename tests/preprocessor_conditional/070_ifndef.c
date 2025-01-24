#define foo
int a = 1;
void main() {
#ifndef foo
    a = 2;
#endif
#ifndef bar
    a = 0;
#endif
    return a;
}