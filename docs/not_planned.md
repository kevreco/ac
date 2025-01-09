# Not Planned Features

## Exceptions

- If exceptions are allowed to be caught from anywhere the you might prevent the compiler from optimizing code.

- In most case you want to deal with the error closer to where it's happening.

- The callstack generate by the exceptions Exceptions are generally expensive.

- (There are more reaons to dislike them).

## Built-in Slice Type

Slice type can be implementation in a library, there is no need to inflate the requirement of the compiler.

## Package manager

Package manager should be not closely tight to any programming language.
I'm in favor of standalone (and offline) source code management where I can build a piece of software using the source code at any given time without having to download a package from somewhere (either manually or automatically).

The existance of official or built-in package manager would encourage the use of external (remote) package.

Another (indirect) downside but package managers can add options to update you codebase using the _latest_ version of a package.

This also add some significant friction because you don't need to deal with dependency hell when you just want to run your application.

## Arbitrary compile-time execution

- Likely very difficult to secure if you do not restrict memory usage, file access or internet access.
- Likely very difficult to debug.

## Compile-time Reflection

Enabling compile-time reflection would increase the complexity of the compiler drastically.

You would need to expose a form of AST because you would need tobe able to acces every single data contained in you can.

Maybe you want to count the number of methods in your program, display a list of all used functions, serialize specific data or add arbitray function at the begining and end of any function so you could write a profiler that can be turn off and on at will.
You basically want to create new code based on parsed code.
If your compiler already have an AST it would require a meta-AST and this meta-AST needs to be standarized.

## Alternative to compile-time Reflection

Run a pre-build program `metaprog.c` analyzing your code and generating new code.
```
compiler metaprog.c -o metaprog
metaprog 
compiler generated_file.c
```

The program itself might even be the compiler itself runing with specific (static or dynamic) plugin architecture.
```
compiler program.c --plugin my_serializer_plugin
```
The advantage of this would be to only write the extra plugin parts and reusing the already existing "AST" from the compiler.

