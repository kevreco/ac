#define concat(a, b) a ## b
#define foo_0 concat(x, y)
#define foo(x) concat(foo_, x)
#define bar_0() concat(x, y)
#define bar(x) concat(bar_, x)()
foo(0)
bar(0)