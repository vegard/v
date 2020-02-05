@gcd := lang.macro {
    return state.compile(quote u64 1337);
};

print gcd();
