#!/bin/bash
# Builds bench/work/mf25_bench_c (C kernels), mf25_bench_asm (x86-64 assembly, System V) and
# mf25_bench_asm_ms (same assembly built with the Microsoft x64 convention, -DFA_TEST_MSABI,
# called through ms_abi declarations: validates the Win64 entry/exit code on Linux).
set -eu
D="$(cd "$(dirname "$0")" && pwd)"; S="$D/../src/Compression"; W="$D/work"; mkdir -p "$W"
CC=${CC:-gcc}
FLAGS="-O3 -w -I$S/LZMA2/sdk25"
$CC $FLAGS -o "$W/mf25_bench_c" "$D/mf25_bench.c" -lpthread
$CC -c -o "$W/fa_cpu_x86_64.o"   "$S/asm/fa_cpu_x86_64.S"
$CC -c -o "$W/LzFind25_x86_64.o" "$S/LZMA2/asm/LzFind25_x86_64.S"
$CC $FLAGS -DFA_MF_ASM -o "$W/mf25_bench_asm" "$D/mf25_bench.c" "$W/fa_cpu_x86_64.o" "$W/LzFind25_x86_64.o" -lpthread
$CC -c -DFA_TEST_MSABI -o "$W/fa_cpu_ms0.o"   "$S/asm/fa_cpu_x86_64.S"
$CC -c -DFA_TEST_MSABI -o "$W/LzFind25_ms0.o" "$S/LZMA2/asm/LzFind25_x86_64.S"
objcopy --prefix-symbols=ms_ "$W/fa_cpu_ms0.o"   "$W/fa_cpu_ms.o"
objcopy --prefix-symbols=ms_ "$W/LzFind25_ms0.o" "$W/LzFind25_ms.o"
$CC $FLAGS -DFA_MF_ASM "-DLZF_ASM_ABI=__attribute__((ms_abi))" \
    -Dlzf25_bt_getmatches=ms_lzf25_bt_getmatches -Dlzf25_bt_skip=ms_lzf25_bt_skip \
    -Dlzf25_bt_getmatches_n=ms_lzf25_bt_getmatches_n -Dfa_x86_64_cpu_init=ms_fa_x86_64_cpu_init \
    -Dfa_cpu_avx2=ms_fa_cpu_avx2 \
    -o "$W/mf25_bench_asm_ms" "$D/mf25_bench.c" "$W/fa_cpu_ms.o" "$W/LzFind25_ms.o" -lpthread
rm -f "$W"/*_ms0.o
echo "built $W/mf25_bench_c $W/mf25_bench_asm $W/mf25_bench_asm_ms"
