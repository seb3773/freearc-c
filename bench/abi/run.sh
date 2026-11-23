#!/bin/bash
# Assemble the kernels for System V and (in ELF, -DFA_TEST_MSABI) Microsoft x64, then run abi_check.
set -eu
D="$(cd "$(dirname "$0")" && pwd)"; S="$D/../../src/Compression"; W=$(mktemp -d)
for f in asm/fa_cpu_x86_64.S LZMA2/asm/LzFind_x86_64.S LZMA2/asm/LzFind25_x86_64.S Tornado/asm/tor_match_x86_64.S; do
  b=$(basename $f .S)
  gcc -c -o $W/$b.o $S/$f
  gcc -c -DFA_TEST_MSABI -o $W/$b.ms0.o $S/$f
  objcopy --prefix-symbols=ms_ $W/$b.ms0.o $W/$b.ms.o
done
rm -f $W/*.ms0.o
gcc -O2 -o $W/abi_check $D/abi_check.c $D/abi_call.S $W/*.o
$W/abi_check
