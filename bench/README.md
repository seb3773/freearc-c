# Decoder benchmark & bit-exact harness

Every optimization of the decoder must (1) keep extraction bit-exact and
(2) show a real, reproducible speed-up. This directory provides both checks.

## Setup (once)

```sh
make                       # build ./build/linux/{arc,unarc}
bench/make_corpus.sh       # ~52 MB deterministic mixed corpus -> bench/work/corpus
bench/make_archives.sh     # one archive per method/profile   -> bench/work/archives
```

The corpus mixes real legacy data (game scripts/DLLs extracted from
`tests_files/samples_legacy/data2.arc`), C/C++ and Python source text, x86
executables, random data, a 16-bit stereo WAV and a 24-bit BMP.
Archives cover `-m1..-m5`, `-mx`, `-m4x`, `-m5x`, lzma (several lc/lp/pb and
match-finder variants), tor (levels 1/3/5/7), ppmd, grzip, tta, the
`-bcj/-delta/-dict/-lzp/-rep/-mm` filters, `-m0`, AES and `4x4:` multithreaded
variants. Archives are created by this repo's encoder, which is bit-exact
with legacy FreeArc 0.67 (`tests/test_bitexact.sh`).

## Bit-exact verification

```sh
python3 bench/bench.py verify build/linux/unarc
```

Extracts every archive in `bench/work/archives`, every `tests/reference/*.arc`
and the legacy `tests_files/samples_legacy/*.arc` (golden SHA-256 in
`golden_legacy.sha256`), and compares SHA-256 of every extracted file with the
originals. Exit code != 0 on any mismatch or extraction error.

## A/B timing

```sh
cp build/linux/unarc bench/bin/unarc.base     # keep a reference binary
# ... change code, rebuild ...
python3 bench/bench.py run bench/bin/unarc.base build/linux/unarc -n 5 [-f ppmd,lzma]
```

Runs are interleaved (ABBA order) and extraction goes to tmpfs (`/dev/shm`) to
minimize disk noise. Each candidate run is also verified bit-exact. Verdict per
archive: `GAIN` only when the gain on the minimum exceeds both 1% and the
baseline's run-to-run spread (median - min), and the median also improves;
anything else is reported as `noise` (or `LOSS`).

## PGO (optional)

`bench/pgo_build.sh` produces `build/linux/unarc.pgo` (profile-guided build).

## Encoder benchmark & A/B harness

For testing and benchmarking encoder changes:

```sh
# 1. Whole-archive bit-exact verification & benchmarks
bench/enc_bench.py refs build/linux/arc       # store reference hashes
bench/enc_bench.py verify build/linux/arc     # verify candidate bit-exactness

# 2. In-process A/B benchmark (eliminates CPU frequency & machine noise)
bench/ab_build.sh src /tmp/base.so            # build baseline shared library
bench/ab_build.sh src /tmp/cand.so            # build candidate shared library
g++ -O3 bench/abbench.cpp -o bench/abbench -ldl
bench/abbench /tmp/base.so /tmp/cand.so "tor:6" 8 bench/work/corpus/text/sources.txt
```

## Encoder differential fuzzing and cold-start A/B

```sh
g++ -O2 bench/abfuzz.cpp  -o bench/abfuzz  -ldl
g++ -O2 bench/abfirst.cpp -o bench/abfirst -ldl
# byte-identical output on thousands of generated inputs (tiny/odd sizes, many data shapes)
bench/abfuzz  /tmp/base.so /tmp/cand.so 3000 exe lzp "lzp:80m:105:d1m:s32:h22:92%" "dict:64m:0%"
# timing of the first call in a fresh process (cold globals, as in a real arc run)
bench/abfirst /tmp/base.so /tmp/cand.so "dict:p:64m:80%" 30 bench/work/corpus/text/*.txt
```

Note: shared-library builds (`ab_build.sh`, `-fPIC`) can distort codecs that use many global
variables in hot loops (DICT). Confirm such results with the production objects:
`bench/kbench_build.sh` on two built trees, run alternately in fresh processes.
The `f_*` cases of `enc_bench.py` time the preprocessors alone (no back-end compressor).
