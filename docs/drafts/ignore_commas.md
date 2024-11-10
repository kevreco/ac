# Ignore consecutive commas

If empty statements are allowed, then empty arguments should be allowed as well.
A simple warning should be issued. Treating warnings as errors should be also possible.

Example of empty statements:

```c
int value = 1;;    // The second semi-colon is an empty statement.
;                  // This is an empty statement.
```

Example of empty arguments:

```c
function(a, , b);    // The second comma should just be skipped (warning should be raised).
function(a,  b,);    // The last comma should also be ignored (warning should be raised).
```