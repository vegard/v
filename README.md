Introduction
============

This is a toy language/compiler that I'm designing/writing just for fun
in my spare time. Some explicit goals for the language/compiler:

 - giving the programmer full access to the compiler internals
 - metaprogramming using the same syntax/semantics as regular (compiled)
   code
 - simplicity; "keywords" are just built-in macros


Getting started
===============

Building:

    git submodule update --init
    ./configure.sh
    ./make.sh

Running a program:

    ./v main.v


Thanks
======

- [Jonathan Blow](https://twitter.com/Jonathan_Blow)
    - compiler/language demos and ideas
- [Eli Bendersky](https://twitter.com/elibendersky)
    - lots of interesting parsing/compiling blog posts
- Blaise Boscaccy
    - language design discussions/LLVM help
