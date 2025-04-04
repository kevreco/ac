
Abstract syntax tree of the C language supported in this compiler:

```
top_level:
     declaration−list

declaration−list:
    declaration
    declaration−list declaration

declaration:
    simple−declarations
    type−declaration
    function−declaration
    type−definition

simple−declarations:
    type−specifier declarator-list ;

declarator-list:
    declarator
    declarator-list , declarator
 
declarator:
    pointer? identifier array−specifier?
    pointer? identifier array−specifier? = initializer−list
	pointer? identifier array−specifier? ( parameter-list )
    ( pointer? identifier array−specifier? ) ( parameter-list )

pointer:
    *
    pointer *

array−specifier:
    [ constant−expression ]
    [ constant−expression ] array−specifier

type−specifier:
    scope? type

type:
    signature? char
    signature? int
    signature? short
    signature? short int
    signature? long
    signature? long int
    signature? long long
    signature? long long int
    void
    float
    double
    typename

signature:
    signed
    unsigned

scope:
    static
    extern
    inline

function−declaration:
    type−specifier identifier ( parameter-list ) block

parameter-list:
    parameter
    parameter-list , parameter

parameter:
    type
    type pointer
	type pointer array−specifier
    type ( pointer ) ( arglist )
    type declarator
    ...
```