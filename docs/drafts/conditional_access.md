# Conditional access

TL;DR: it's like optional type but we don't use a type and only use a function.
I'm not aware of a use case were we need to store optional types, however I often need to query various container to know if a value exists or is valid.

Here is a function using "conditional value".
```c
int get_item(key k) ??
{
    item i = { 0 };
    if (try_find(map, k, &i))
        return i;  /* Return true and the value. */
    else
        return;    /* Return false and no value. This return is optional. */
}
```

## Example 1 - In conditions

Case were we want to validate multiple values before using them.

```c
/* In C */
int a;
int b;
int final_result = -1;
if((result_a = get_result_a(value_a)) > 0)
    && (result_b = get_result_b(value_b)) > 0)
{
    final_result = compute(a, b);
}
```

```c
/* Equivalent in AC */
int final_result = -1;
if (get_result_a(value_a) ?? a
    && get_result_b(value_b) ?? b)
{
    final_result = compute(a, b);
}
```

## Example 2 - Query an item from a container

```c
if (get_item(map, key) ?? value)
{
    /* Do something with the value. */
}
```

## Example 3 - Iteration

```c
range it = range_make(0, 10);

while(next(it) ?? i ) {
    print("value %d\n", i);
}
```