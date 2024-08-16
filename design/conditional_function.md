
## easy iteration


?? int next(range it)
{
	it.value += 1;
	if (it.value < it.max)
		return it;
	else
		return void;
}
auto it = range_make(0, 10);
while(auto i ?? next(it)) { /* get next would be an operator? */

}

## finally clause

?? int with_file(string file)
{
	int handle = open(file);
} finally {
	close(handle);
}

## lock

if (var lock ?? with_lock("note.txt"))
{
	/* do something */
}

/* if you think the "if" a little bit weid, no problem you can also write this. */

var lock ?? with_lock() {

}

 
## tagged union

enum Type { integer, float}
struct maybe_t {

	enum Type type;
	union
	{
	 int i;
	 float f;
	}
}

?? auto maybe_is(maybe_t m, enum type) {

	#switch type
	{
		Type.integer: return m.i;
		Type.float: return m.f;
		default: return void;
	}
}

maybe_t maybe(maybe_t m, T val) {
	#switch T
	{
		int: m.i = val;
		float: m.i = val;
	}
	return m;
}


maybe_t m = maybe(100);

if(!get_maybe(m, Type.integer)) {
  // Do nothing
}
else if(var i ?? get_maybe(m, Type.integer)) {
  print(i);
}