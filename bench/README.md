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
