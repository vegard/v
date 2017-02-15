#! /bin/bash

set -e
set -u

for file in tests/parser/*.v
do
	diff -U100 ${file%.v}.out <(./v --dump-ast --no-compile $file)
done
