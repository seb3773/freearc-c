#!/bin/bash
# Optional profile-guided (PGO) build of unarc. Measured gain: ~2-3% on most methods.
# Training uses the benchmark archives (run make_corpus.sh and
# make_archives.sh first) plus the legacy sample archives.
set -eu
D="$(cd "$(dirname "$0")" && pwd)"; R="$D/.."; P=$(mktemp -d /tmp/freearc_pgo_XXXXXX)
cd "$R/src"
rm -rf dec && make -j"$(nproc)" unarc PGO_FLAGS="-fprofile-generate -fprofile-dir=$P -fprofile-update=atomic"
T=$(mktemp -d)
for a in "$D"/work/archives/*.arc "$R"/tests_files/samples_legacy/*.arc; do
  [ -f "$a" ] || continue
  rm -rf "$T"/* && (cd "$T" && "$R/src/unarc" x -o+ -pSecret123 "$a" >/dev/null 2>&1 || true)
done
rm -rf "$T" dec
make -j"$(nproc)" unarc PGO_FLAGS="-fprofile-use -fprofile-dir=$P -fprofile-partial-training -Wno-missing-profile"
mkdir -p "$R/build/linux" && cp unarc "$R/build/linux/unarc.pgo"
rm -rf "$P" dec
echo "PGO binary: build/linux/unarc.pgo  (verify with: bench/bench.py verify build/linux/unarc.pgo)"
