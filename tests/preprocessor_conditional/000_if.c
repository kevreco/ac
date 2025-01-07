int a = 2;
int main() {
#if 1
    a = 0;
#endif
#if 0
    a = 1;
#endif
    return a;
}