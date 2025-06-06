# Graveyard

Planned ideas that were turned down.

## Declaration in conditions:

```
    if (is_even(v)
        { int a = multiplied(v) }  /* This a statement that do no return any boolean values. */
        && is_multiple_of_two(a))
    {
        /* Do something. */    
    }
```

Removed because we could also write:

```
bool b = is_even(v)
            { int a = multiplied(v) }
            && is_multiple_of_two(a);
```

Which is very clunky.

## (Complicated) Tagged Union

One issue with [tagged union](drafts/tagged_union.md) is that there is no way to control the memory layout of the tag.

The following "type binding" was meant to fix that problem.

```c
struct token {
    
    enum token_type { Integer, Float, String, Coord } type;
    union {
        int i;
        float f;
        const char* str;
        coord xy;
    };
    /* Create binding on enum. The field 'type' will generate an error if used directly from a token value. */
} bind using(type) {
    Integer = i;
    Float = f;
    String = str;
    Coord = xy;
}
```

However, there were several issues:

1) I find it way too cumbersome to be added in AC.

2) It would be even more cumbersome *and* confusing in case we want to bind a previously defined type.

```c
struct token {
    enum token_type { Integer, Float, String, Coord } type;
    union {
        int i;
        float f;
        const char* str;
        coord xy;
    };
};
    
// Theoritical syntax
bind {
    token t; // Use of token which is defined above.
} using(t.type) {
    Integer = t.i;
    Float = t.f;
    String = t.str;
    Coord = t.xy;
}
```
3) There is a case that would be even more complicated to take care of:

```c

union {
    struct {
        enum token_type type;
        float f;
    } integer;
    
    struct {
        enum token_type type;
        int i;
    } real;
    
    struct {
        enum token_type type;
        const char* str;
    } string;
    
    struct {
        enum token_type type;
        coord xy;
    } coordinate;
};

```