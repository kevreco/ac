#define foo(x, y) x y
#define bar(x) x x
foo(foo(1, 2),foo(3, 4))
bar(bar(1))