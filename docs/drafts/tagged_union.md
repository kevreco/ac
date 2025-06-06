# Union with implicit tag

```c
union @tagged node {
    int integer;
    float real;
    const char* name;
};

void print_node(node n) {
    switch(n) { 
        integer ?? i : printf("%d", i);
        real ?? f : printf("%f", f);
        name ?? str : printf("%s", str);
    };
}
```

An integer tag field is implicit added to the union to indicate which type is in use.
The downside is that the memory layout of the union is unclear.
This `union @variant` will be added only if it requires minimal adjustment on the compiler side.