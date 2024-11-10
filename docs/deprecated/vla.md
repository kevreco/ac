#Variable length arrays (VLA).

See this example taken from [cppreference](https://en.cppreference.com/w/c/language/array#Variable-length%20arrays):

```
int main(void)
{
   int n = 1;
label:;
   int a[n]; /* re-allocated 10 times, each with a different size */
   printf("The array has %zu elements\n", sizeof a / sizeof *a);
   if (n++ < 10)
       goto label; /* leaving the scope of a VLA ends its lifetime */
}
```

VLA will not be supported in AC.
Reason: it's easy to create a stack overflow and it raises some security concern.