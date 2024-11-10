
# Stage 0 - C Parser and C converter
  
    - [./] Support integer declaration. "int a = 0;"
    - [./] Support multiple integer declaration. "int a = 0, b = 1;"
    - [./] Support function definition. "int main() { return 0; }
    - [./] Suppprt function parameters. "int main(int a, int b) { return 0;}"
    - [./] Support forward function declaration. "int main(int a);
    - [./] Support other type char parameter.
    - [./] Support pointer parameters.
    - [./] Support array parameters.
    
# Stage 1 - C Preprocessor

    - [WIP] Handle macro.
        - [./] Object-like macro.
        - [./] Strays '\' between tokens.
        - [./] Strays '\' for identifiers.
        - [./] Strays '\' for other multiple-char token.
        - [./] Strays '\' in other literal (numbers, boolean, etc.).
        - [./] Strays '\' in string and character literals.
        - [./] Function-like macro.
        - [./] Token concatenation.
        - [./] Macro should not be visible from the expanded tokens.
        - [] Support Stringification operator #
        - [] Implement special macro like __COUNT__, __LINE__, __FILE__ etc.
        
    - [] Support basic #if/#endif
    - [] Support #elif
    - [] Support #if defined(XXX)
    - [] Support evaluation if #if
    
# Stage 2 - C Parser and C converter

    - [] Support pointer declaration.
    - [] Support array declaration.
    - [] Handle typedef.
    - [] Handle struct.
    
# Type check

    - Implement some type check with int8/int16/etc before introducing structure and enum?
    - Ensure that literal types have the correct size. Example: 64-bit int literal cannot be stored in 32-bit int.
    - Handle number overflows.
    
# Later
    - Handle raw string literals and utf8/utf16/utf32 raw literal as well.
    - Add __STDC_NO_VLA__ as default macro. We don't want to support VLA at all.
    
# Documentation
    - Expand the "Generic type" section.
    - Expand the "User-defined for loop" section.