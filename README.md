# hexdiff

## Overview

A command-line tool for comparing binary files

![hexdiff screenshot](screenshots/screenshot.png?raw=true "Hexdiff screenshot")

## Compiling

`hexdiff` uses only the standard C library, so it should be fairly easy to build
for any platform that has a compiler with support for C11

Download the source code. With `git` that can be done by running:

	git clone https://github.com/pgeorgiev98/hexdiff

And to build `hexdiff` with `cmake`:

	cd hexdiff
	mkdir build
	cd build
	cmake ..
	make
