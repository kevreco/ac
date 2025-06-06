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
Other languages addressing this frustration exist however I'm not satified with their direction.

New language rely on FFI or LLVM to import C projects, however I want AC compiler to compiler C files without external dependencies and without need to create thin a wrapper from AC code.

In the long run, I want to be able to use AC as scripting language and the compiler should be several times faster than a standard C compiler.
Some design choice as already been made around this (the preprocessor is done during parsing time, no need to run it beforehand) and future design choice will follow,

# Features

## Currently missing

Since the project is in a very early stage, almost all features (C or AC) are currently missing.

You can check to [TODO](docs/TODO.md) list to see what has been implemented already.

## Planned

- Comprehensive preprocessor. GCC, MSVC and clang generate the most correct preprocessor output, but all other compilers I tried were generating incorrect output for some tests. I will create a pages to compare all the well known compilers.
- Basic type alias such as:
  - `i8` `i16` `i32` `i64` `i128` for signed integer.
  - `u8` `u16` `u32` `u64` `u128` for unsigned integer.
  - `f32` `f64` for floating point numbers.
- Nested procedures and types.
- dot operator as dereference operator. `a->b` should be equivalent to `a.b`.
- `ptr(int) a = NULL;` will be an alias for `int *a = NULL`.
- `arr(int, 10) a;` will be an alias for `int a[10];`.
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
- Underscores in number [literals](docs/literals.md#more-lax-single-quotes-digit-separator).
- [Identifier literal](docs/literals.md#identifier).
    - `int @"Hello World" = 1;`
- [Constants declaration](docs/constants.md#constants).
    - `PI @= 3.14f;`
- User-defined for loops. (@TODO)
- Everything initialized with zero, unless explicit uninitialization.
- Compile-time evaluation via bytecode + VM.

## Unsupported features

Deprecated (and some optional) features in most recent C versions will likely never be implemented.

Examples:

- [VLA](docs/deprecated/vla.md)
- [(K&R) function style](docs/deprecated/kr.md)
- [Implicit int return](docs/deprecated/int_return.md)

## Not planned

- No [exceptions](docs/not_planned.md#exceptions).
- No [package manager](docs/not_planned.md#package-manager).
- No [built-in slice type](docs/not_planned.md#built-in-slice-type). They will likely be implemented in a standard library.
- No slice operator.
- No [compilte-time reflection](docs/not_planned.md#compile-time-reflection).
- No run-time reflection.

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