#! /bin/bash

set -e
set -u

g++ -std=c++11 -Wall -g -o v main.cc -lgmp -lgmpxx
