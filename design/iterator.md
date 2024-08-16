- current idea for compile-time iterator:
```
yield(int) iter_range(int begin, int end)  {
	for(int i = begin, i < end; i++)
	{
		yield i;
	}
}

for (i: iter_range(0, 10)) {
	printf(i);
}

yield(int) iter_is_even(yield(int) it)  {

	for (i: it)
		if (i % 2 == 0)
			yield i;
}


for (i: iter_iseven(iter_range(0, 10)) {
	printf(i);
}
```

- run-time iterator?

```
	iterator my_iterator(int begin, int end) { ... }
	for (int x: my_iterator(0, 100))   // if there is no parenthesis after for an iterator is assumed to be used.
	for (int y: my_iterator(0, 100))
	{ print(x, y); }
	// You can iterate built-in values
	int a[10];
	for (i : a) { print(i); };
	int b;
	for (i : b) { print(i); }; // even single values
```