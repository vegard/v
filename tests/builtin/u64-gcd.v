@gcd := (fun u64(u64, u64)) (a, b) {
	if (b == u64 0)
		(return a);

	while (a != u64 0) {
		if (a < b)
			(b = b - a)
		else
			(a = a - b);
	};

	return b;
};

print gcd(u64 4860, u64 12960);
