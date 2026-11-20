#!/bin/bash
# Build the in-process codec benchmark against the encoder objects of a source tree.
# usage: bench/kbench_build.sh SRC_DIR OUTPUT_BINARY     (SRC_DIR must contain a built enc/ directory: make -C SRC_DIR arc)
set -eu
SRC="$(cd "$1" && pwd)"; OUT="$2"; HERE="$(cd "$(dirname "$0")" && pwd)"
FLAGS="-O3 -flto=auto -fno-exceptions -fno-rtti -w -DFREEARC_UNIX -DFREEARC_INTEL_BYTE_ORDER -DFREEARC_64BIT"
T=$(mktemp -d)
g++ -c $FLAGS -I"$SRC/Compression" "$HERE/kbench.cpp" -o "$T/kbench.o"
g++ -O3 -flto=auto -w -o "$OUT" "$T/kbench.o" $(ls "$SRC"/enc/*.o | grep -v '/arc.o$') -lstdc++ -lrt -lpthread
rm -rf "$T"
