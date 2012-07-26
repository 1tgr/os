#!/bin/bash
args="--prefix=$(pwd)/newlib --target=i386-elf"
export CC_FOR_TARGET=i386-elf-gcc-4.3.2
(cd ../newlib-1.20.0 && ./configure $args)
