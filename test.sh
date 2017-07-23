#! /bin/bash

set -e
set -u

for file in tests/parser/*.v
do
	diff -U100 ${file%.v}.out <(./v --dump-ast --no-compile $file)
done

for file in tests/builtin/*.v
do
	diff -U100 ${file%.v}.out <(./v $file)
done
