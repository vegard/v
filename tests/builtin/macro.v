@gcd := lang.macro {
    return compile(quote u64 1337);
};

print gcd();
