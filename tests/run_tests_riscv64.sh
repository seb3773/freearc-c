#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
ARC_RV="$ROOT_DIR/build/linux-riscv64/arc"
UNARC_RV="$ROOT_DIR/build/linux-riscv64/unarc"
SFX_RV="$ROOT_DIR/build/linux-riscv64/arc.sfx"
ARC_X64="$ROOT_DIR/build/linux/arc"
UNARC_X64="$ROOT_DIR/build/linux/unarc"
QEMU_SYSROOT="/usr/riscv64-linux-gnu"

if ! command -v qemu-riscv64 >/dev/null 2>&1; then
  echo "ERROR: qemu-riscv64 not found."
  exit 1
fi
if [ ! -x "$ARC_RV" ] || [ ! -x "$UNARC_RV" ]; then
  echo "ERROR: RISC-V binaries missing in $ROOT_DIR/build/linux-riscv64 (run: make riscv64)."
  exit 1
fi

run_rv() {
  qemu-riscv64 -L "$QEMU_SYSROOT" "$@"
}

TMP="$(mktemp -d /tmp/arc_riscv64_test_XXXXXX)"
trap 'rm -rf "$TMP"' EXIT

PASS=0
pass_case() { echo "  [PASS] $1"; PASS=$((PASS + 1)); }

mkdir -p "$TMP/src" "$TMP/ext" "$TMP/cross1" "$TMP/cross2"
cp -a "$ROOT_DIR/tests_files/corpus/." "$TMP/src/"

echo "=== FreeArc RISC-V 64 Conformance Suite ==="
run_rv "$ARC_RV" a -m5 -dp"$TMP/src" "$TMP/rv_m5.arc" -r . >/dev/null
run_rv "$UNARC_RV" x -o+ -dp"$TMP/ext" "$TMP/rv_m5.arc" >/dev/null
diff -qr "$TMP/src" "$TMP/ext"
pass_case "RISC-V 64 roundtrip (-m5)"

run_rv "$ARC_RV" a -nobitexact -mx -dp"$TMP/src" "$TMP/rv_nbx.arc" -r . >/dev/null
"$UNARC_X64" x -o+ -dp"$TMP/cross1" "$TMP/rv_nbx.arc" >/dev/null
diff -qr "$TMP/src" "$TMP/cross1"
pass_case "Cross: RISC-V 64 -nobitexact -> x86_64 unarc"

"$ARC_X64" a -m4 -dp"$TMP/src" "$TMP/x64_m4.arc" -r . >/dev/null
run_rv "$UNARC_RV" x -o+ -dp"$TMP/cross2" "$TMP/x64_m4.arc" >/dev/null
diff -qr "$TMP/src" "$TMP/cross2"
pass_case "Cross: x86_64 arc -> RISC-V 64 unarc"

if [ -x "$SFX_RV" ]; then
  run_rv "$ARC_RV" s -sfx="$SFX_RV" "$TMP/rv_m5.arc" "$TMP/rv.run" >/dev/null
  chmod +x "$TMP/rv.run"
  mkdir -p "$TMP/sfx_out"
  run_rv "$TMP/rv.run" -o+ -dp"$TMP/sfx_out" >/dev/null
  diff -qr "$TMP/src" "$TMP/sfx_out"
  pass_case "RISC-V 64 SFX creation and execution"
fi

echo "=== RISC-V 64 Results: $PASS Passed, 0 Failed ==="

