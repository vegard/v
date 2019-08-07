@fib_type := fun u64(u64);
@fib : fib_type;
@fib = fib_type(n) {
    if (n < u64 2)
        (return n);
    return (fib(n - u64 1) + fib(n - u64 2));
};

print fib(u64 18);
