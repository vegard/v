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

for file in tests/integration/*.v
do
	echo $file
	diff -U100 ${file%.v}.out <(./v $file) || true
done

for file in tests/elf/*.v
do
	echo $file
	rm -rf ${file}.exe
	./v $file >/dev/null
	diff -U100 ${file%.v}.out <(${file}.exe; echo $?) || true
done
