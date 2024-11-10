# Alternative function syntax

In AC, function can be written like this:
```c
fn main(void) -> int {
    return 0;
}
```

The reason for that is to be able to return generic result from generic parameters.

```c
fn min(@type T, T left, T right) -> T {
    return left < right ? left : right;
}
```
The C syntax would not allow this:
```c
T min(@type T, T left, T right) {
    return left < right ? left : right;
}
```
because the return type `T` would not be known before the first parameter `@type T`.

`@type` can be any type.
All types are known at compile time which is why the '@' prefix is used as '@' always refer to something happening at compile-time.