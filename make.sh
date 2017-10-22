#! /bin/bash

set -e
set -u

g++ -std=c++14 -Wall -Wfatal-errors -Iudis86-install/include -Isrc -g -o v src/main.cc -lgmp -lgmpxx -Ludis86-install/lib/ -ludis86
