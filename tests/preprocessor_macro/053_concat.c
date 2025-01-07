#define foo(x, y) bar(x, y) 
#define bar(x, y) x ## y
#define a 1 2
#define b 3 4
foo(a + a, b + b)
bar(a + a, b + b)