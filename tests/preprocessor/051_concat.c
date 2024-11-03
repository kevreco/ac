#define foo(x, y) 12 x ## y 56
#define bar(x, y) x ## y
foo(3, 4)
bar(12  3, 4 56)