# ac

"Augmented C" compiler

Augmented C (AC) should be backward compatible with C and should emit C code.

In this regard, it should start to be a C compiler that transpile to C.


## Stage 0.0

The code sample should be compilable.
Every single non-failing test file should be able to produce a binary.

```
int main()
{
    return 0;
}
```

### TODO

- Create c project with premake
- Create tokenizer.
- Handle errors.
- Create lexer and AST.
- Create C generator.
- Combine everything.
