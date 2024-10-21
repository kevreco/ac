#define foo() 23
#define fofoo() 42
#define bar(x, y) x fo##o() y
#define baz(x, y) x fo## ## ##o() y
#define bag(x, y) x fo ## ## ## foo() y
#define bam(x, y) y )##( x
bar(1, 4)
baz(1, 4)
bag(1, 4)
bam(1, 4)