#define str(x) #x
str('foo')
str('"')
str("'")
str(\n)
str('\n')
str("\n")
str("\\")
str('\\')
str("foo\nbar")
str(1'0'1'0) /* Number with quote spacing. */
str('\x09') /* Charater with escape sequence. */