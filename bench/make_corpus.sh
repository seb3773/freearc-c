#!/bin/bash
# Build a deterministic, mixed benchmark corpus (~60 MB) under bench/work/corpus.
set -eu
D="$(cd "$(dirname "$0")" && pwd)"; R="$D/.."; W="$D/work"; C="$W/corpus"
rm -rf "$C"; mkdir -p "$C"/{text,bin,game,media}
# Real legacy data (game scripts/dlls) from the legacy sample archive.
T=$(mktemp -d); (cd "$T" && "$R/build/linux/unarc" x -o+ "$R/tests_files/samples_legacy/data2.arc" >/dev/null)
find "$T" -type f -name '*.dll' -exec cp {} "$C/game/" \;
f=$(find "$T" -type f -name '*.sch' | sort | head -2); for x in $f; do cp "$x" "$C/game/"; done
rm -rf "$T"
# Text: all source code of the project concatenated a few times (C/C++ text).
find "$R/src" -type f \( -name '*.c*' -o -name '*.h' \) | sort | xargs cat > "$C/text/sources.txt"
PYLIB=$(python3 -c "import os;print(os.path.dirname(os.__file__))")
find "$PYLIB" -type f -name '*.py' 2>/dev/null | sort | xargs cat 2>/dev/null | head -c 10000000 > "$C/text/python.txt"
# Executables.
ls /usr/bin/* | sort | head -400 | xargs cat 2>/dev/null | head -c 12000000 > "$C/bin/exes.bin"
python3 - "$C" <<'PY'
import sys, random, math, struct
c=sys.argv[1]; r=random.Random(1234)
open(f"{c}/bin/random.bin","wb").write(r.randbytes(4_000_000))
# 16-bit stereo wav-like signal
n=1_500_000; s=bytearray()
for i in range(n):
    v=int(8000*math.sin(i*0.01)+3000*math.sin(i*0.137)+r.randint(-200,200))
    s+=struct.pack('<hh',v,-v//2)
hdr=b'RIFF'+struct.pack('<I',36+len(s))+b'WAVEfmt '+struct.pack('<IHHIIHH',16,1,2,44100,176400,4,16)+b'data'+struct.pack('<I',len(s))
open(f"{c}/media/sound.wav","wb").write(hdr+s)
# 24-bit image-like smooth gradients + noise
w,h=1024,1024; px=bytearray()
for y in range(h):
    for x in range(w):
        px+=bytes(((x+y+r.randint(0,7))&255,(x*2^y)&255,(y+r.randint(0,3))&255))
open(f"{c}/media/image.bmp","wb").write(b'BM'+struct.pack('<IHHI',54+len(px),0,0,54)+struct.pack('<IiiHHIIiiII',40,w,h,1,24,0,len(px),2835,2835,0,0)+px)
PY
find "$C" -exec touch -d "2026-01-01 12:00:00" {} +
du -sh "$C"; (cd "$C" && find . -type f | sort | xargs sha256sum) > "$W/corpus.sha256"
