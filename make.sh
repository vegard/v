#! /bin/bash

set -e
set -u

g++ -std=c++14 -Wall -g -o v main.cc -lgmp -lgmpxx
