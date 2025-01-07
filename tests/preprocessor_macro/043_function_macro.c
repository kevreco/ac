#define foo() 12
#define bar() foo()
#define baz() foo ()
bar()
baz()