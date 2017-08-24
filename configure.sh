#! /bin/bash

set -e
set -u

git submodule init
git submodule update

udis86_prefix=$PWD/udis86-install

(
	cd udis86
	./autogen.sh
	./configure --enable-static=yes --enable-shared=no --prefix="${udis86_prefix}"
)
