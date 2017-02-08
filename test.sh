#! /bin/bash

set -e
set -u

for file in tests/parser/*.v
do
	cmp ${file%.v}.out <(./v $file)
done
