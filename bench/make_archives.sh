#!/bin/bash
# Compress the benchmark corpus with every method/profile (single-thread, deterministic).
# Archives are produced by the bit-exact (vs legacy Arc.exe) encoder of this repo.
set -eu
D="$(cd "$(dirname "$0")" && pwd)"; R="$D/.."; W="$D/work"; A="$W/archives"
ARC="${ARC:-$R/build/linux/arc}"
mkdir -p "$A"; rm -f "$A"/*.arc
CASES=(
 "m1:-m1" "m2:-m2" "m3:-m3" "m4:-m4" "m5:-m5" "mx:-mx" "m4x:-m4x" "m5x:-m5x"
 "lzma:-mlzma" "lzmalc8:-mlzma:lc8" "lzmalp:-mlzma:lc0:lp2:pb0" "lzmafast:-mlzma:1m:fast:hc4" "tor:-mtor" "tor1:-mtor:1" "tor3:-mtor:3" "tor7:-mtor:7"
 "ppmd:-mppmd" "ppmdtxt:-mppmd" "grziptxt:-mgrzip" "grzip:-mgrzip" "tta:-mtta"
 "bcj:-m5 -bcj" "delta:-m5 -delta" "dict:-m5 -dict" "lzp:-m5 -lzp"
 "rep:-m5 -rep" "mm:-m5 -mm" "store:-m0" "aes:-m5 -pSecret123 -ae aes"
 "x4lzma:-m4x4:lzma" "x4tor:-m4x4:tor"
)
# Optional per-case input subset (default: whole corpus).
declare -A INPUT=( [ppmdtxt]="text" [grziptxt]="text" [lzmalc8]="text game" [lzmalp]="text game" [lzmafast]="text game" )
for c in "${CASES[@]}"; do
  n="${c%%:*}"; f="${c#*:}"; in="${INPUT[$n]:-.}"
  (cd "$W/corpus" && "$ARC" a -mt1 $f "$A/$n.arc" $in >/dev/null 2>&1) || echo "FAILED: $n ($f)"
  (cd "$W/corpus" && find $in -type f | sort | xargs sha256sum) > "$A/$n.sha256"
  [ -f "$A/$n.arc" ] && printf "%-8s %10d  %s\n" "$n" "$(stat -c%s "$A/$n.arc")" "$f"
done
