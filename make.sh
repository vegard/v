#! /bin/bash

set -e
set -u

g++ -std=c++14 -Wall -Wfatal-errors -Isrc -g -o v src/main.cc -lgmp -lgmpxx
