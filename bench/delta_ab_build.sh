#!/bin/bash
# Build the in-process DELTA A/B benchmark from two versions of Delta.cpp.
# usage: bench/delta_ab_build.sh BASE_DELTA_CPP CAND_DELTA_CPP OUTPUT [EXTRA_CFLAGS]
# The flags match the production encoder build (-O3, no -march=native).
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"; SRC="$HERE/../src"
B="$(realpath "$1")"; C="$(realpath "$2")"; OUT="$3"; EXTRA="${4:-}"
MAIN=delta_ab.cpp; case "$EXTRA" in *DELTA_FUZZ*) MAIN=delta_fuzz.cpp;; esac
g++ -O3 -fno-exceptions -fno-rtti -w -fstrict-aliasing -fomit-frame-pointer -fno-stack-protector \
    -DNDEBUG -DFREEARC_UNIX -DFREEARC_INTEL_BYTE_ORDER -DFREEARC_64BIT \
    -I"$SRC/Compression" -DBASE_DELTA="\"$B\"" -DCAND_DELTA="\"$C\"" $EXTRA \
    "$HERE/$MAIN" "$SRC/Compression/Common.cpp" -o "$OUT" -lpthread -lrt
