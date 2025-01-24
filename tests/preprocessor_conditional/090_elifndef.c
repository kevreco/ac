int a = 1;
void main() {
#if 0
    a = 2;
#elifndef foo
    a = 0;
#else
    a = 3;
#endif
    return a;
}