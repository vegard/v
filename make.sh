#! /bin/bash

set -e
set -u

make -C udis86 install
g++ -std=c++14 -Wall -Iudis86-install/include -Isrc -g -o v src/main.cc -lgmp -lgmpxx -Ludis86-install/lib/ -ludis86
