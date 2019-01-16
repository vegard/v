Introduction
============

This is a toy language/compiler that I'm designing/writing for fun.
Some explicit goals are:

 - giving the programmer access to compiler internals
 - metaprogramming using the same syntax/semantics as regular (compiled)
   code
 - simplicity; "keywords" are just built-in macros


Getting started
===============

For now, building the compiler is only supported on Linux x86-64 hosts.

Building:

    ./configure.sh
    ./make.sh

Running a program:

    ./v main.v
