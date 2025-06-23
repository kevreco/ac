int main()
{
    const char* foo;
    foo = __LINE__;
    foo = __FILE__;
#line 100
    foo = __LINE__;
    foo = __FILE__;
#line 200 "fo\
o.c"
    foo = __LINE__;
    foo = __FILE__;
}