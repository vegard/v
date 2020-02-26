@gcd := lang.macro {
    # implicitly defined:
    #  - node: lang.node
    #  - new_scope
    #  - eval
    #  - define

    new_scope {
        # define 't' in the new scope so we don't pollute the
        # scope of the caller of this macro
        define(quote t, eval(node));

        # compile and return a new function
        #
        # Note that while 't' is not know at this macro's compile time, we can
        # use it in the function signature here since it _is_ known at this macro's
        # runtime.
        return compile(quote (fun t (t, t)) (a, b) {
                if (b == t 0)
                    (return a);

                while (a != t 0) {
                    if (a < b)
                        (b = b - a)
                    else
                        (a = a - b);
                };

                return b;
        });
    }
};

@u64_gcd := gcd u64;

print u64_gcd(u64 4860, u64 12960);
