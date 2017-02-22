#! /bin/bash

set -e
set -u

udis86_prefix=$PWD/udis86-install

(
	cd udis86
	./configure --enable-static=yes --enable-shared=no --prefix="${udis86_prefix}"
)
