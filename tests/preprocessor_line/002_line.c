int main()
{
    const char* foo;
    foo = __LINE__;
    foo = __FILE__;
#line 100
    foo = __LINE__;
    foo = __FILE__;
#line 200 "foo.c" bar
    foo = __LINE__;
    foo = __FILE__;
}