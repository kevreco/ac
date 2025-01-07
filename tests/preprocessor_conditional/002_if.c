int a = 2;
int main() {
#if 2-1
    a = 0;
#endif
#if 1-1
    a = 1;
#endif
    return a;
}