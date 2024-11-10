# K&R

(K&R) function style (allow declarations between function parameter and body) is not supported.
```
int g(a,b,c,d)
int a,b,c,d;
{
	return 0;
} 
```
Reason: it's unconventional, not often used, would complexify the compiler for no significant gain.
It has been deprecated in C23.