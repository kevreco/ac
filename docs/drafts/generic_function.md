# Generic functions

`@(my_type)` means the type must be known at compile time`

The following example show a compile-time method that take a compile-time integer.

```c
void assert_integer(@(int) value) {
    static_assert(value != 0, "value is non-zero");
}
void main(void) {
    assert_integer(0);
    assert_integer(1);
}
```
Possible output:
```
error: value is non-zero
```

`@type` is can be any type and is always known at compile-time.
This allow generic function to be written like this:

```c
fn min(@type T, T left, T right) -> T {
    return left < right ? left : right;
}
```

`@type` can be used to host any kind of compile-time values.
`@type` in function parameters also allow duck typing:

```c
/* @type is a compile-time type containing other compile-time types and/or values. */
@type base8_encoder {
    int init(char* data, size_t size)     { /* ... */ }
    int encode(char* data, size_t size)   { /* ... */ }
}

@type base64_encoder {
    int init(char* data, size_t size)     { /* ... */ }
    int encode(char* data, size_t size)   { /* ... */ }
}

void encode(char* data, size_t size, @type encoder) {
    encoder.init(data, size);
    encoder.encode(data, size);
}

int main(void) {
    const char* text = "Hello World";
    size_t len = strlen(text);
    
    encode(text, len, base8_encoder);
    encode(text, len, base64_encoder);
}
```