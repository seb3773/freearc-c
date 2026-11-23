#!/bin/bash
# Builds bench/work/mf_bench_c (C kernels) and bench/work/mf_bench_asm (x86-64 assembly kernels).
set -eu
D="$(cd "$(dirname "$0")" && pwd)"; S="$D/../src"; W="$D/work"; mkdir -p "$W"
CXX=${CXX:-g++}; CC=${CC:-gcc}
FLAGS="-O3 -fno-exceptions -fno-rtti -w -DFREEARC_UNIX -DFREEARC_INTEL_BYTE_ORDER -DFREEARC_64BIT -I$S -I$S/Compression"
$CXX $FLAGS -o "$W/mf_bench_c" "$D/mf_bench.cpp"
$CC -c -o "$W/fa_cpu_x86_64.o"  "$S/Compression/asm/fa_cpu_x86_64.S"
$CC -c -o "$W/LzFind_x86_64.o"  "$S/Compression/LZMA2/asm/LzFind_x86_64.S"
$CXX $FLAGS -DFA_MF_ASM -o "$W/mf_bench_asm" "$D/mf_bench.cpp" "$W/fa_cpu_x86_64.o" "$W/LzFind_x86_64.o"
echo "built $W/mf_bench_c $W/mf_bench_asm"
# Same kernels assembled with the Microsoft x64 convention (ELF object, -DFA_TEST_MSABI),
# called through ms_abi declarations: validates the Win64 entry/exit code on Linux.
$CC -c -DFA_TEST_MSABI -o "$W/fa_cpu_ms0.o" "$S/Compression/asm/fa_cpu_x86_64.S"
$CC -c -DFA_TEST_MSABI -o "$W/LzFind_ms0.o" "$S/Compression/LZMA2/asm/LzFind_x86_64.S"
objcopy --prefix-symbols=ms_ "$W/fa_cpu_ms0.o" "$W/fa_cpu_ms.o"
objcopy --prefix-symbols=ms_ "$W/LzFind_ms0.o" "$W/LzFind_ms.o"
$CXX $FLAGS -DFA_MF_ASM "-DLZF_ASM_ABI=__attribute__((ms_abi))" -Dlzf_bt_getmatches=ms_lzf_bt_getmatches \
    -Dlzf_bt_skip=ms_lzf_bt_skip -Dlzf_ht_getmatches=ms_lzf_ht_getmatches -Dfa_x86_64_cpu_init=ms_fa_x86_64_cpu_init \
    -o "$W/mf_bench_asm_ms" "$D/mf_bench.cpp" "$W/fa_cpu_ms.o" "$W/LzFind_ms.o"
rm -f "$W"/*_ms0.o
echo "built $W/mf_bench_asm_ms"
