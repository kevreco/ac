# Enum binding

Enum can be bound to some values to create tagged union or tagged struct.

```c
struct temperature_t {
    enum { Celsius, Fahrenheit, Kelvin } temp_type;
    double value;
}

struct temp
{
    /* This body is not visible outside of this type */
    temperature_t t; 
} enum (t.temp_type) /* enum values to bind to other type. */
{
    Celsius = t.value;
    Fahrenheit = t.value;
    Kelvin = t.value;
}

void print_temp(temp t) {
    switch(t) { 
        auto c ? Celsius:    printf("%f °C\n", c);
        auto f ? Fahrenheit: printf("%f °F\n", f);
        auto k ? Kelvin:     printf("%f K\n", k);
    };
}

temp t = temp.Celsius(10.0);
print_temp(t);
t.Fahrenheit = 11.0;
print_temp(t);
t.Kelvin = 12.0;
print_temp(t);
```
Output:
```
10 °C
11 °F
12 K
```

NOTE: I'm not fond of the syntax `struct { ... } enum (xxx) { ... }` as it's pretty unclear on what is going on. This will very likely change.