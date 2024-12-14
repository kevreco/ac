/* Taken from the C standard documentation and slightly altered. #cpp.concat 15.6.3.4 */
#define str(s)      # s
#define xstr(s)     str(s)
#define debug(s, t) printf("x" # s "= %d, x" # t "= %s", \
               x ## s, x ## t)
#define INCFILE(n)  vers ## n
#define glue(a, b)  a ## b
#define xglue(a, b) glue(a, b)
#define HIGHLOW     "hello"
#define LOW         LOW ", world"
debug(1, 2);
fputs(str(strncmp("abc\0d", "abc", '\4')        // this goes away
    == 0) str(: @\n), s);
//#include xstr(INCFILE(2).h)  // Disabled and replace with the line below
xstr(INCFILE(2).h)
glue(HIGH, LOW);
xglue(HIGH, LOW)