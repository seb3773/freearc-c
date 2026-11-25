#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
TARGET="${1:-linux}"
ARC_BIN="$ROOT_DIR/build/linux/arc"

if [ ! -x "$ARC_BIN" ]; then
  echo "ERROR: $ARC_BIN not found; build Linux arc first (make)."
  exit 1
fi

TMP="$(mktemp -d /tmp/freearc_lib_test_XXXXXX)"
trap 'rm -rf "$TMP"' EXIT

PASS=0
pass_case() { echo "  [PASS] $1"; PASS=$((PASS + 1)); }

# Build mixed test archives (solid + 4x4 + -nobitexact + encrypted -p / -hp + corrupt)
mkdir -p "$TMP/src_tree" "$TMP/work_dyn" "$TMP/work_stat"
cp -a "$ROOT_DIR/tests_files/corpus/." "$TMP/src_tree/"
echo "hello library test payload" > "$TMP/src_tree/hello.txt"
"$ARC_BIN" a -nobitexact -mx "$TMP/plain.arc" -dp"$TMP/src_tree" -r . >/dev/null
"$ARC_BIN" a -m3 -pLibSecret "$TMP/enc_p.arc" -dp"$TMP/src_tree" -r . >/dev/null
"$ARC_BIN" a -m3 -hpLibSecret "$TMP/enc_hp.arc" -dp"$TMP/src_tree" -r . >/dev/null
dd if=/dev/zero of="$TMP/corrupt.arc" bs=1024 count=4 status=none

echo "=== FreeArc Library Tests ($TARGET) ==="

case "$TARGET" in
  linux)
    LIB_DIR="$ROOT_DIR/build/linux"
    gcc -O2 -Wall -Wextra -I"$ROOT_DIR/src" "$ROOT_DIR/tests/test_lib_host.c" -ldl -lpthread -o "$TMP/host_dlopen"
    "$TMP/host_dlopen" "$LIB_DIR/libunarc.so" "$TMP/plain.arc" "$TMP/enc_p.arc" "$TMP/enc_hp.arc" "$TMP/corrupt.arc" "$TMP/work_dyn" "LibSecret"
    pass_case "Linux x86_64 dlopen(libunarc.so) C host test suite"

    gcc -O2 -Wall -Wextra -DUSE_STATIC_LINK -I"$ROOT_DIR/src" "$ROOT_DIR/tests/test_lib_host.c" \
      "$LIB_DIR/libunarc.a" -lstdc++ -lpthread -lm -o "$TMP/host_static"
    "$TMP/host_static" "unused" "$TMP/plain.arc" "$TMP/enc_p.arc" "$TMP/enc_hp.arc" "$TMP/corrupt.arc" "$TMP/work_stat" "LibSecret"
    pass_case "Linux x86_64 static libunarc.a C host test suite"

    diff -qr "$TMP/src_tree" "$TMP/work_dyn/out_mod"
    diff -qr "$TMP/src_tree" "$TMP/work_stat/out_leg"
    diff -qr "$TMP/src_tree" "$TMP/work_stat/out_hp"
    diff -qr "$TMP/src_tree" "$TMP/work_stat/out_t1"
    pass_case "Linux x86_64 extracted directory tree diff-verified"

    # Python ctypes FFI check
    python3 - "$LIB_DIR/libunarc.so" "$TMP/plain.arc" "$TMP/work_dyn/out_py" << 'PY'
import ctypes, sys
lib = ctypes.CDLL(sys.argv[1])
lib.unarc_version.restype = ctypes.c_char_p
assert lib.unarc_version() == b"1.4.5"
CB = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_char_p, ctypes.c_int64, ctypes.c_int64, ctypes.c_char_p, ctypes.c_size_t, ctypes.c_void_p)
seen = []
def cb(what, p1, p2, s, cap, ud):
    seen.append(what.decode())
    return 0
fn = CB(cb)
args = (ctypes.c_char_p * 5)(b"x", b"-o+", f"-dp{sys.argv[3]}".encode(), b"--", sys.argv[2].encode())
rc = lib.unarc_run(5, args, fn, None)
assert rc == 0 and "filename" in seen and "total" in seen
lib.unarc_cleanup()
PY
    diff -qr "$TMP/src_tree" "$TMP/work_dyn/out_py"
    pass_case "Linux x86_64 Python ctypes FFI test"
    ;;

  win64)
    LIB_DIR="$ROOT_DIR/build/win64"
    cp "$LIB_DIR/unarc.dll" "$TMP/unarc.dll"
    x86_64-w64-mingw32-gcc -O2 -Wall -Wextra -I"$ROOT_DIR/src" "$ROOT_DIR/tests/test_lib_host.c" -o "$TMP/host_win64.exe"
    WINEDEBUG=-all wine "$TMP/host_win64.exe" "unarc.dll" \
      "$(winepath -w "$TMP/plain.arc")" "$(winepath -w "$TMP/enc_p.arc")" \
      "$(winepath -w "$TMP/enc_hp.arc")" "$(winepath -w "$TMP/corrupt.arc")" \
      "$(winepath -w "$TMP/work_dyn")" "LibSecret"
    pass_case "Win64 LoadLibraryA(unarc.dll) C host test suite under Wine"

    x86_64-w64-mingw32-gcc -O2 -Wall -Wextra -DUSE_STATIC_LINK -I"$ROOT_DIR/src" \
      "$ROOT_DIR/tests/test_lib_host.c" "$LIB_DIR/libunarc.dll.a" -o "$TMP/host_win64_implib.exe"
    WINEDEBUG=-all wine "$TMP/host_win64_implib.exe" "unused" \
      "$(winepath -w "$TMP/plain.arc")" "$(winepath -w "$TMP/enc_p.arc")" \
      "$(winepath -w "$TMP/enc_hp.arc")" "$(winepath -w "$TMP/corrupt.arc")" \
      "$(winepath -w "$TMP/work_stat")" "LibSecret"
    pass_case "Win64 libunarc.dll.a import-library host test suite under Wine"

    diff -qr "$TMP/src_tree" "$TMP/work_dyn/out_mod"
    diff -qr "$TMP/src_tree" "$TMP/work_stat/out_leg"
    diff -qr "$TMP/src_tree" "$TMP/work_stat/out_hp"
    diff -qr "$TMP/src_tree" "$TMP/work_stat/out_t1"
    pass_case "Win64 extracted directory tree diff-verified"
    ;;

  arm64)
    LIB_DIR="$ROOT_DIR/build/linux-arm64"
    aarch64-linux-gnu-gcc -O2 -Wall -Wextra -I"$ROOT_DIR/src" "$ROOT_DIR/tests/test_lib_host.c" -ldl -lpthread -o "$TMP/host_arm64_dl"
    qemu-aarch64 -L /usr/aarch64-linux-gnu "$TMP/host_arm64_dl" "$LIB_DIR/libunarc.so" \
      "$TMP/plain.arc" "$TMP/enc_p.arc" "$TMP/enc_hp.arc" "$TMP/corrupt.arc" "$TMP/work_dyn" "LibSecret"
    pass_case "Linux ARM64 dlopen(libunarc.so) C host test suite via QEMU"

    aarch64-linux-gnu-g++ -O2 -DUSE_STATIC_LINK -I"$ROOT_DIR/src" "$ROOT_DIR/tests/test_lib_host.c" \
      "$LIB_DIR/libunarc.a" -lstdc++ -lpthread -lm -o "$TMP/host_arm64_static"
    qemu-aarch64 -L /usr/aarch64-linux-gnu "$TMP/host_arm64_static" "unused" \
      "$TMP/plain.arc" "$TMP/enc_p.arc" "$TMP/enc_hp.arc" "$TMP/corrupt.arc" "$TMP/work_stat" "LibSecret"
    pass_case "Linux ARM64 static libunarc.a host test suite via QEMU"

    diff -qr "$TMP/src_tree" "$TMP/work_dyn/out_mod"
    diff -qr "$TMP/src_tree" "$TMP/work_stat/out_hp"
    pass_case "Linux ARM64 extracted directory tree diff-verified"
    ;;

  riscv64)
    if ! command -v riscv64-linux-gnu-g++ >/dev/null 2>&1; then
      echo "ERROR: riscv64-linux-gnu-g++ not installed (sudo apt install g++-riscv64-linux-gnu)."
      exit 1
    fi
    LIB_DIR="$ROOT_DIR/build/linux-riscv64"
    riscv64-linux-gnu-gcc -O2 -Wall -Wextra -I"$ROOT_DIR/src" "$ROOT_DIR/tests/test_lib_host.c" -ldl -lpthread -o "$TMP/host_rv64_dl"
    qemu-riscv64 -L /usr/riscv64-linux-gnu "$TMP/host_rv64_dl" "$LIB_DIR/libunarc.so" \
      "$TMP/plain.arc" "$TMP/enc_p.arc" "$TMP/enc_hp.arc" "$TMP/corrupt.arc" "$TMP/work_dyn" "LibSecret"
    pass_case "Linux RISC-V 64 dlopen(libunarc.so) C host test suite via QEMU"

    riscv64-linux-gnu-g++ -O2 -DUSE_STATIC_LINK -I"$ROOT_DIR/src" "$ROOT_DIR/tests/test_lib_host.c" \
      "$LIB_DIR/libunarc.a" -lpthread -lm -o "$TMP/host_rv64_static"
    qemu-riscv64 -L /usr/riscv64-linux-gnu "$TMP/host_rv64_static" "unused" \
      "$TMP/plain.arc" "$TMP/enc_p.arc" "$TMP/enc_hp.arc" "$TMP/corrupt.arc" "$TMP/work_stat" "LibSecret"
    pass_case "Linux RISC-V 64 static libunarc.a host test suite via QEMU"

    diff -qr "$TMP/src_tree" "$TMP/work_dyn/out_mod"
    diff -qr "$TMP/src_tree" "$TMP/work_stat/out_hp"
    pass_case "Linux RISC-V 64 extracted directory tree diff-verified"
    ;;

  *)
    echo "Unknown target: $TARGET"
    exit 2
    ;;
esac

echo "=== Library Tests Summary ($TARGET): $PASS passed ==="
