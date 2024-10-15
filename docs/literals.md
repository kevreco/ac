# Constants and literals

## Integer

All cases from the standard should be handled.

### More lax single quotes digit separator

Since `C23`, it's possible to insert quotes between digits as separator.
It's not possible to insert them between before or after 'e', or, before or after '.'.
It's also not possible to have trainling quotes. 
Additionally it's possible to have a `'` symbol before or after a non-leading digit.

```
int a = 1'0;   // Already possible since C23.
int b = 1';    // Possible in AC.
int c = 1'';   // Possible in AC.
int d = 1'.'0; // Possible in AC.
```

Reason: It make the parsing way easier and I can't see any problem with this approach.

### Possible underscore as separator

Singe quotes were used as separator because underscores were creating [ambiguities with C++11](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2013/n3499.html).

> Underscores work well as a digit separator for C++03 [N0259] [N2281]. But with C++11, there exists a potential ambiguity with user-defined literals [N2747].

It's possible to use underscores in AC, mostely because we don't care about C++.

```
int a = 1_0;   // Possible in AC.
int b = 1_;    // Possible in AC.
int c = 1_._0; // Possible in AC.
```

## Float


'f', 'F', 'l' and 'L' suffixes are handled.
DF, DD and DL suffixes are not handled.

Like integer literals, floats benefit from the lax single quotes digit separator and underscore can also be used.