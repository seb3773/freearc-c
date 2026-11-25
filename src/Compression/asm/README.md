# x86-64 assembly match finders & decoder kernels

Hand-written x86-64 kernels for the hot encoder and LZMA decoder loops. Each match-finder
kernel matches the C code semantics it accelerates (C stays in place as reference and portable
fallback).

| File | Replaces (C) | Used by |
|---|---|---|
| `LZMA2/asm/LzFind_x86_64.S` | `GetMatchesSpec1`, `SkipMatchesSpec`, `Ht_GetMatchesSpec` (`LZMA2/C/LzFind.c`) | bit-exact LZMA: BT2/BT3/BT4 (`lzma:mfbt*`, archive directory), HT4 rows of 64+ entries (`lzma:...:mc64` and above; default profiles use mc4–mc32 inlined C) |
| `LZMA2/asm/LzFind25_x86_64.S` | `GetMatchesSpec1`, `SkipMatchesSpec` (`LZMA2/sdk25/LzFind.c`), `GetMatchesSpecN_2` (`LZMA2/sdk25/LzFindOpt.c`) | `-nobitexact` LZMA SDK 25.01 encoder (single-threaded & batched MT BT4 walks) |
| `Tornado/asm/tor_match_x86_64.S` | `find_match_len`, the 4-byte extension loops (`Tornado/MatchFinder.cpp`) | Tornado `-m1`, `-m2`, `-mtor`, `tor:N` |
| `LZMA2/sdk25/asm/LzmaDecOpt.S` | `LzmaDec_DecodeReal_3` (`LZMA2/sdk25/LzmaDec.c`) | `unarc`, SFX, and encoder self-decode (`LZMA_ASM=1` on x86-64) |
| `asm/fa_cpu_x86_64.S` | – | runtime AVX2 detection (CPUID + XGETBV) |
| `asm/fa_x86_64.h` | – | ABI macros, `LZEXT` match-extension macro |

## Design

* **One source for both ABIs.** The `.S` files go through the C preprocessor. `FA_ARG1..4`
  and `FA_SAVE_NONVOL` pick System V (Linux) or Microsoft x64 (MinGW-w64) registers. On
  Win64 the code also pushes `rdi`/`rsi` and emits SEH unwind directives. The kernels only
  use `xmm0-5`/`ymm0-5`, which are volatile in both ABIs, so no XMM register is saved.
  The LZMA kernels take a single pointer to an argument block, so the entry code is the
  same on both ABIs.
* **BT walk:** all hot state stays in registers. The only stack values are the cut counter
  and the rarely used output state. The left/right descent, which C compiles to a branch
  that is mispredicted about half the time, is done with `cmov`. The son[] pair is
  prefetched as soon as its index is known. `LzFind.c` also prefetches ahead: the hash
  head of pos+2, plus the root son[] pair and data of pos+1.
* **HT4 rows:** up to 32 tags are checked with SSE2 before one loop over the matching bits.
  This runs in the same order as the C code. `delta` is taken without masking
  (`newEntry - entry`, exact modulo 2^32). The row shift (`memmove` in C) is inlined with
  overlapping head/tail vectors.
* **Match extension (`LZEXT`):** an 8-byte probe first, then 32-byte AVX2 blocks
  (`vpcmpeqb`/`vpmovmskb`/`tzcnt`) when the CPU has AVX2, then 16-byte SSE2 blocks, then an
  overlapped 8-byte tail. Loads never go past the limit. Tornado's legacy 4-byte loop can
  step past `bufend`, and that changes the output, so `tor_ext4` reproduces it exactly.

## Build switches

* `MF_ASM=1` is the default on x86-64 Linux hosts and `MF_ASM_WIN64=1` is the default for
  the MinGW build. `MF_ASM=0` / `MF_ASM_WIN64=0` build the pure C code.
* Only the encoder objects `C_LZMA.o` and `C_Tornado.o` get `-DFA_MF_ASM`. The decoder
  (`unarc`, SFX) and ARM64 builds never see it. The sources also turn it off on
  non-x86-64 targets.

## Verification

* `tests/test_bitexact.sh --verify`, `make test`, `make test-arm64`.
* `bench/mf_bench_build.sh` builds `mf_bench_c`, `mf_bench_asm` and `mf_bench_asm_ms`
  (Microsoft x64 convention called through `ms_abi`). They drive the LZMA match finder
  over a file and print a checksum of every match, every skip and the final son[]/hash[]
  state. The checksums must match.
* `bench/abi/run.sh` checks that callee-saved registers and `rsp` are preserved under both
  ABIs, and fuzzes the Tornado kernels against the C code on the SSE2 and AVX2 paths.
