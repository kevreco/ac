> [!NOTE]  
> This project is incubating and is not ready to be used for anything.

# Table of Contents

- [AC](#ac)
- [Why?](#why)
- [Features](#features)
- [How to build](#how-to-build)

# AC

"Augmented C" (AC) programming language will be a general purpose language meant to be backward compatible with C.

In this regard, I plan implement a functional C compiler that will be extended afterward.
This compiler will emit C code to make it cross-platform.

# Why?

Using C is very often frustrating so I'm just planning to create new extension/syntax/construct to get rid of this frustration.
Other languages addressing this frustration exists however I'm not satified with their direction.

# Features

## Currently missing

Since the project is in a very early stage, almost all features (C or AC) are currently missing.
You can check to [TODO](docs/TODO.md) list to see what has been implemented already.

## Planned

- Basic type alias such as:
  - `i8` `i16` `i32` `i64` `i128` for signed integer.
  - `u8` `u16` `u32` `u64` `u128` for unsigned integer.
  - `f32` `f64` for floating point numbers.
- Nested procedures and types.
- dot operator as dereference operator. `a->b` should be equivalent to `a.b`.
- `ptr(int) a = NULL;` will be an alias for `int *a = NULL`.
- `arr(int, 10) a;` will be an alias for `int[10] a;`.
- All built-in operators or directives will be prefixed with `@` such as:
    - `@sizeof`
    - `@typeof`
    - `struct @align(4) @no_padding vector3 { ... }`
    Besides standard keywords all compile-time operators will be prefixed with `@` (they can still be replaced with C macros).
- [Alternative function declaration](docs/drafts/alternative_function.md)
- [Generic functions](docs/drafts/generic_function.md)
- [Generic types](docs/drafts/generic_type.md)
- [Conditional access](docs/drafts/conditional_access.md)
- [Concise if branches](docs/drafts/concise_if.md)
- User-defined for loops. (@TODO)
- [Enum bindings](docs/drafts/binding.md)
- Declaration in conditions:
```
    if (is_even(v)
        { int a = multiplied(v) }  /* This a statement that do no return any boolean values. */
        && is_multiple_of_two(a))
    {
        /* Do something. */    
    }
```
- Underscores in number [literals](docs/literals.md#more-lax-single-quotes-digit-separator).
- Everything initialized with zero, unless explicit uninitialization.
- Compile-time evaluation via bytecode + VM.

## Unsupported features

Deprecated (and some optional) features in most recent C versions will likely never be implemented.

Examples:

- [VLA](docs/deprecated/vla.md)
- [(K&R) function style](docs/deprecated/kr.md)
- [Implicit int return](docs/deprecated/int_return.md)

## Not planned

- No package manager.
- No exceptions.
- No built-in slice type. They will likely be implemented in a standard library.
- No slice operator.

# How to build

[CB](github.com/kevreco.cb) build system which consist of a c library to build c programs.

## On Windows (MSVC)
```
cl.exe cb.c
./cb.exe
```

## On Linux (or POSIX compatible)

```
cc cb.c -o cb
./cb
```