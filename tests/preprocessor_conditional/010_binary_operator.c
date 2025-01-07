int a = 2;
int main() {
#if 1 * 2 - 3 + 1 * 1 - 0
    a = 1;
#endif
#if 1 || 2 && 3 && !4
    a = 0;
#endif
    return a;
}