# Alternative if syntax

If the `if` keyword is followed by a `{` then we enter the "alternative if branches" case where all statements should start with some conditions between `(...)` followed by a statement.
The last statement can be the `else` branch.

```c
if {
    (value == 0) { printf("equal zero\n"); }
    (value < -10 && value > 10) { printf("between -10 and 10\n"); }
    else { printf("else\n") }
} /* This if statement cannot be followed by 'else' here */
```
The AC code above equivalent to this C code:
```c
if (value == 0) { printf("equal zero\n"); }
else if (value < -10 && value > 10) { printf("between -10 and 10\n"); }
else { printf("else\n") }
```
This concise if statement can also be used like an expression.
```c
const char* text = if {
    (value == 0) "equal zero\n";
    (value > 10 && value < -10) "between -10 and 10\n";
    else "else\n";
}
printf(text);
```
The type of all branches should be the same or a "type mismatch" error will be raised.