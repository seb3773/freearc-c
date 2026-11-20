#!/bin/bash
# Build a shared library containing the whole FreeArc codec library of a source tree,
# for the same-process A/B benchmark (abbench.cpp). Only CompressMem is exported, so two
# such libraries (baseline and candidate) can be loaded side by side with dlopen().
# usage: bench/ab_build.sh SRC_DIR OUTPUT.so [EXTRA_CFLAGS]
set -eu
SRC="$(cd "$1" && pwd)"; OUT="$2"; EXTRA="${3:-}"; HERE="$(cd "$(dirname "$0")" && pwd)"
T=$(mktemp -d)
FLAGS="-O3 -flto=auto -fPIC -fno-exceptions -fno-rtti -w -fstrict-aliasing -fomit-frame-pointer -fno-stack-protector -fno-math-errno -fno-threadsafe-statics -DNDEBUG -DFREEARC_UNIX -DFREEARC_INTEL_BYTE_ORDER -DFREEARC_64BIT -I$SRC/Compression -I$SRC/Compression/_Encryption -I$SRC/Compression/_Encryption/headers $EXTRA"
C=$SRC/Compression
CXX_SRCS="$C/Common.cpp $C/CompressionLibrary.cpp $C/LZMA2/C_LZMA.cpp $C/LZMA2/C_BCJ.cpp $C/Dict/C_Dict.cpp $C/Delta/C_Delta.cpp $C/REP/C_REP.cpp $C/LZP/C_LZP.cpp $C/PPMD/C_PPMD.cpp $C/External/C_External.cpp $C/MM/C_MM.cpp $C/MM/C_TTA.cpp $C/Tornado/C_Tornado.cpp $C/GRZip/C_GRZip.cpp $C/4x4/C_4x4.cpp $C/_Encryption/C_Encryption.cpp"
objs=""
i=0
for f in $CXX_SRCS; do
  [ -f "$f" ] || { echo "missing $f" >&2; exit 1; }
  i=$((i+1)); o="$T/o$i.o"
  case "$f" in
    *C_PPMD.cpp) g++ -c $FLAGS -fno-strict-aliasing -o "$o" "$f" & ;;
    *C_TTA.cpp)  g++ -c $FLAGS -fno-strict-aliasing -fwrapv -fno-lto -o "$o" "$f" & ;;
    *) g++ -c $FLAGS -o "$o" "$f" & ;;
  esac
  objs="$objs $o"
done
# LZMA SDK 25 decoder TU (needed by C_LZMA.cpp)
gcc -c ${FLAGS/-fno-rtti/} -x c -o "$T/fastdec.o" "$C/LZMA2/sdk25/lzma_fastdec.c" &
objs="$objs $T/fastdec.o"
printf '#include <stdio.h>\nFILE *g_logfile = NULL;\n' > "$T/stub.cpp"
g++ -c $FLAGS -o "$T/stub.o" "$T/stub.cpp" &
objs="$objs $T/stub.o"
wait
printf '{ global: CompressMem; local: *; };\n' > "$T/v.map"
g++ -shared -O3 -flto=auto -fPIC -w -Wl,-Bsymbolic -Wl,--version-script=$T/v.map -o "$OUT" $objs -lstdc++ -lrt -lpthread
rm -rf "$T"
