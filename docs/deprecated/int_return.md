# Implicit int return

Function definition must return a type.
Omitting type is allowed in C (an warning or error is still issued), the following functions respectively return an `int`anda `int*`.

```
return_int()
{
   return 0;
}

*return_int_ptr()
{
   return NULL;
}
```

AC wil not support such construct.
Reason: it's unconventional, rarely used andwould complexify the compiler for no significant gain.
It has been deprecated in C99.