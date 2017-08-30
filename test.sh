#! /bin/bash

set -e
set -u

for file in tests/parser/*.v
do
	echo $file
	diff -U100 ${file%.v}.out <(./v --dump-ast --no-compile $file) || true
done

for file in tests/builtin/*.v
do
	echo $file
	diff -U100 ${file%.v}.out <(./v $file) || true
done
