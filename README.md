# ac

"Augmented C" compiler

Augmented C (AC) should be backward (mostly) compatible with C and should emit C code.

In this regard, it should start to be a C compiler that transpile to C.


## Stage 0.0

The code sample should be compilable.
Every single non-failing test file should be able to produce a binary.

```
int main()
{
    return 0;
}
```

### Limitations

(K&R) function style (allow declarations between function parameter and body) is not handled.
```
int g(a,b,c,d)
int a,b,c,d;
{
	return 0;
} 
```
Reason: it's unconventional, not often used, would complexify the compiler for no significant gain.
It has been deprecated in C23.

Function definition should return a type. Omitting type was working fine, the following function definition returned an `int*`.
```
*func()
{
   return NULL;
}
```
Reason: it's unconventional, not often used, would complexify the compiler for no significant gain.
It has been deprecated in C99.
