
# Stage 0 - C Parser and C converter

 - ☑ Support integer declaration. `int a = 0;`
 - ☑ Support multiple integer declaration. `int a = 0, b = 1;`
 - ☑ Support function definition. `int main() { return 0; }`
 - ☑ Suppprt function parameters. `int main(int a, int b) { return 0;}`
 - ☑ Support forward function declaration. `int main(int a);`
 - ☑ Support other type char parameter.
 - ☑ Support pointer parameters.
 - ☑ Support array parameters.

# Stage 1 - C Preprocessor

### 1.0

 - ☑ Object-like macro.
 - ☑ Support splice between tokens.
 - ☑ Support splice within identifiers.
 - ☑ Support splice for other multiple-char token.
 - ☑ Support splice in other literal (numbers, boolean, etc.).
 - ☑ Support function-like macro.
 - ☑ Support concatenation operator "##".
 - ☑ Macro should not be visible from the expanded tokens.
 - ☑ Support stringification operator "#".

### 1.1

 - ☑ Support special macro `__FILE__`.
 - ☑ Support special macro `__LINE__`.
 - ☑ Support special macro `__DATE__`.
 - ☑ Support special macro `__TIME__`.
 - ☑ Support special macro `__COUNTER__`.

### 1.2

We need the following feature to test `__func__`, `__FUNCTION__` and `__PRETTY_FUNCTION__`:

 - ☑ Support basic types (bool, int, char, short, long, signed, unsigned, float, double).
 - ☑ Support pointers.
 - ☑ Support dereferencing.
 - ☑ Support string literal.
 - ☑ Support char literal.

### 1.3

 - ☑ Support special macro `__func__`, `__FUNCTION__` and `__PRETTY_FUNCTION__`.

### 1.4

 - ☑ Support basic #if/#endif
 - ☑ Support #elif
 - ☑ Support evaluation in #if
 - ☑ Support defined(XXX) expression in #if/#elif
 - ☑ Support #ifdef/#ifndef/#elifdef/#elifndef
 - ☑ Support #warning and #error
 - ☑ Support #line
 - ➖ Support #embed
     Naive implementation of #embed, the directive "#embed "file.h" is translated into a string literal,
     and then converted back into a #embed in the generated C file.
     This assumes the final C compiler supports the #embed directive.
     No more complicated cases are currently handled.
 
### 1.X

 - ☐ Correctly compile the [SQLite amalgamation](https://www.sqlite.org/download.html) from file preprocessed by AC.
 - ☐ Try to make it as fast as [TCC](https://bellard.org/tcc/).
    - 16/03/2025 AC preprocessing is roughly two times slower than TCC.
 - ☐ Create benchmark page in a dedicated Github repository.

# Stage 2 - C Parser and C converter

 - Support array declaration.
 - Handle typedef.
 - Handle struct.
    
# Type check

 - ☐ Implement some type check with int8/int16/etc before introducing structure and enum?
 - ☐ Ensure that literal types have the correct size. Example: 64-bit int literal cannot be stored in 32-bit int.
 - ☐ Handle number overflows.

# Later

 - ☐ Handle raw string literals and utf8/utf16/utf32 raw literal as well.
 - ☐ Add `__STDC_NO_VLA__` as default macro. We don't want to support VLA at all.
 - ☐ Support special macro `__STDC_VERSION__`?
 - ☐ Expand the "Generic type" section.
 - ☐ Expand the "User-defined for loop" section.

