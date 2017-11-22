# define a type 't'
@t := fun u64 ();

# declare a variable 'f' of type 't' (without necessarily assigning a value to it)
f: t;

# assign a value to 'f'
f = t () {
    return u64 7;
};

print f();
