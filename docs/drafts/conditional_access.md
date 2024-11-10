# Conditional access

TL;DR: it's like optional type but we don't use a type and only use a function.
I'm not aware of a use case were we need to store optional types, however I often need to query various container to know if a value exists or is valid.

Here is a function using "conditional value".
```c
? int get_item(key k)
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
if (int a ? get_result_a(value_a)
    && int b ? get_result_b(value_b))
{
    final_result = compute(a, b);
}
```

## Example 2 - Query an item from a container

```c
if (auto value ? get_item(map, key))
{
    /* Do something with the value. */
}
```

## Example 3 - Iteration

```c
auto it = range_make(0, 10);
while(auto i ? next(it)) {
    print("value %d\n", i);
}
```

## Example 4 - Used with "binding"

```c
struct temperature_t {
    enum { Celsius, Fahrenheit, Kelvin } temp_type;
    double value;
}

struct temp { /* This body is not visible outside of this type */
    temperature_t t; 
} enum (t.temp_type) /* Use enum to bind to values. */
{
    Celsius = t.value;
    Fahrenheit = t.value;
    Kelvin = t.value;
}

temp t = temp.Celsius(10.0);

if      (auto c ?= t.Celsius)    { printf("%f °C\n", c); }
else if (auto f ?= t.Fahrenheit) { printf("%f °F\n", f); }
else if (auto k ?= t.Kelvin)     { printf("%f K\n", k); }

/*
Output:
10 °C
*/

```

See also [binding](docs/drafts/binding.md).