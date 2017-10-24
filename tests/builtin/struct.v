@t := struct {
	foo: u64;
	bar: u64;
	baz: struct {
		x: u64;
	};
};

x := t();
x.foo = u64 10;
x.bar = u64 23;
x.baz.x = u64 999;
print x.foo;
print x.bar;
print x.baz.x;
