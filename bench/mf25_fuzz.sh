#!/bin/bash
# mf25_fuzz.sh - equivalence matrix for the LZMA SDK 25.01 match finder kernels (-nobitexact):
# C reference vs x86-64 assembly (System V) vs assembly in Microsoft x64 convention.
# Every case compares the hash of the compressed stream (enc, single-thread and MT match
# finder) and of every match + final son[]/hash[] (mf). Any difference is a failure.
# Covers BT2/BT3/BT4, 64 KB dictionaries (cyclic buffer wrap-around), mc 1..1000, fb 5..273,
# random / zero / periodic / long-repeat / text / binary / tiny inputs.
#   usage: bench/mf25_fuzz.sh [jobs]     (run bench/mf25_bench_build.sh first)
set -eu
D="$(cd "$(dirname "$0")" && pwd)"; W="$D/work"; F="$W/fuzz25"; J=${1:-$(nproc)}
mkdir -p "$F"
python3 - "$F" "$D/../tests_files/corpus" <<'PY'
import sys, random, os, shutil
f, corpus = sys.argv[1], sys.argv[2]; r = random.Random(7)
def w(n, b): open(os.path.join(f, n), 'wb').write(b)
w('rand1.bin', r.randbytes(1000000))
w('zeros2.bin', b'\0' * 2000000)
w('pat.bin', b'abcabcabd' * 50000 + b'xyzw' * 100000 + bytes(r.choice(b'ab') for _ in range(300000)))
base = r.randbytes(20000); out = bytearray()
for i in range(120):
    b = bytearray(base)
    for k in range(r.randint(0, 5)): b[r.randrange(len(b))] = r.randrange(256)
    out += b
w('rep.bin', bytes(out))
w('tiny.bin', b'hello hello hello world')
words = [bytes(r.choice(b'etaoinshrdlu') for _ in range(r.randint(2, 9))) for _ in range(3000)]
w('text.txt', b' '.join(r.choice(words) for _ in range(400000))[:2500000])
mix = bytearray()
while len(mix) < 2000000:
    k = r.randrange(4)
    if k == 0: mix += r.randbytes(r.randint(1, 5000))
    elif k == 1 and len(mix) > 10: s = r.randrange(len(mix)); mix += mix[s:s + r.randint(1, 3000)]
    elif k == 2: mix += bytes([r.randrange(256)]) * r.randint(1, 3000)
    else: mix += bytes(x & 0x0f for x in r.randbytes(r.randint(1, 4000)))
w('mix.bin', bytes(mix))
for n in ('records.bin', 'image24.raw', 'mixed.bin', 'biased.bin', 'eng.txt', 'code.c'):
    p = os.path.join(corpus, n)
    if os.path.exists(p): shutil.copy(p, f)
PY
jobs=$(for f in "$F"/*; do
  for nhb in 2 3 4; do
    for cfg in "0.0625 5 1" "0.0625 32 16" "0.0625 273 256" "1 16 2" "1 64 48" "1 273 1000" "96 128 256" "96 32 3"; do
      set -- $cfg
      echo "$nhb enc $f 1 $1 $2 $3 1"
      echo "$nhb enc $f 2 $1 $2 $3 1"
      echo "$nhb mf $f $1 $2 $3 1"
    done
  done
done)
res=$(echo "$jobs" | xargs -P "$J" -L 1 bash -c '
  nhb=$0; shift 0
  r=""; for b in c asm asm_ms; do r="$r|$(NHB=$nhb "'"$W"'/mf25_bench_$b" "$@" 2>&1 | sed -E "s/best.*//")"; done
  IFS="|" read -r _ a b c <<< "$r"
  if [ "$a" = "$b" ] && [ "$a" = "$c" ]; then echo OK; else echo "FAIL nhb=$nhb $*: [$a] [$b] [$c]"; fi')
ok=$(echo "$res" | grep -c '^OK' || true); bad=$(echo "$res" | grep -v '^OK' || true)
[ -n "$bad" ] && echo "$bad"
echo "mf25 fuzz: $ok cases identical (C / asm SysV / asm ms_abi), $(echo -n "$bad" | grep -c FAIL || true) failures"
[ -z "$bad" ]
