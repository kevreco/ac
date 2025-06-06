# Constants

Constant expression can be declared with the following syntax: `@ <identifier> = <value>`.

```
// float constant
PI @= 3.1415;

// integer constant
MAX_ENTRY @= 3;

// string constant
MAGIC @= "abcd";

struct point {
    float x;
    float y;
};

// Alias of type
vec2 @= typeof(point);

// alias of anynmous struct
coord @= struct {
    float x;
    float y;
}
```

@TODO: investigate if we can use `<identifier> @= struct { ... }` as a new canonical way to define new types and even functions.