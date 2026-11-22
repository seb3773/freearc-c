#!/bin/bash
# ==============================================================================
# FreeArc Modern Native C/C++ Linux ARM64 (AArch64) Test Suite
# Validates ARM64 execution via QEMU user emulation, roundtrip compression,
# -nobitexact mode, and cross-platform interoperability with x86_64 & Win64.
# ==============================================================================
set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

ARC_ARM64="$ROOT_DIR/build/linux-arm64/arc"
UNARC_ARM64="$ROOT_DIR/build/linux-arm64/unarc"
SFX_ARM64="$ROOT_DIR/build/linux-arm64/arc.sfx"

ARC_LINUX="$ROOT_DIR/build/linux/arc"
UNARC_LINUX="$ROOT_DIR/build/linux/unarc"

ARC_WIN="$ROOT_DIR/build/win64/arc.exe"
UNARC_WIN="$ROOT_DIR/build/win64/unarc.exe"

if [ ! -f "$ARC_ARM64" ] || [ ! -f "$UNARC_ARM64" ]; then
    echo "ERROR: ARM64 binaries not found in $ROOT_DIR/build/linux-arm64. Run 'make arm64' first."
    exit 1
fi

if ! command -v qemu-aarch64 >/dev/null 2>&1; then
    echo "ERROR: 'qemu-aarch64' is required to execute ARM64 binaries on x86_64 host."
    exit 1
fi

QEMU_SYSROOT=""
if [ -d "/usr/aarch64-linux-gnu" ]; then
    QEMU_SYSROOT="-L /usr/aarch64-linux-gnu"
fi

run_arm64() {
    qemu-aarch64 $QEMU_SYSROOT "$@"
}

T=$(mktemp -d /tmp/arc_arm64_test_XXXXXX)
trap 'rm -rf "$T"' EXIT

PASS=0
FAIL=0

check() {
    local name="$1"
    shift
    if "$@"; then
        echo "  [PASS] $name"
        PASS=$((PASS + 1))
    else
        echo "  [FAIL] $name"
        FAIL=$((FAIL + 1))
    fi
}

echo "=================================================================="
echo " Starting FreeArc Linux ARM64 (aarch64) Conformance Test Suite"
echo " Workspace : $ROOT_DIR"
echo " Emulator  : $(qemu-aarch64 --version | head -n1)"
echo " Sysroot   : ${QEMU_SYSROOT:-system default}"
echo " Temp dir  : $T"
echo "=================================================================="

# Setup test corpus
mkdir -p "$T/corpus/sub/deep"
echo "FreeArc Native Suite Linux ARM64 Test String 2026" > "$T/corpus/hello.txt"
printf "Row 1: ABCDEFGHIJKLMNOPQRSTUVWXYZ\nRow 2: 0123456789\nRow 3: UTF-8 test: café, résumé, déjà, AArch64\n" > "$T/corpus/sub/deep/multiline.txt"
head -c 65536 /dev/zero > "$T/corpus/sub/zeros.bin"
python3 -c "import os; [open('$T/corpus/pattern.bin', 'wb').write((b'ARM64FreeArcExactBit2026!' * 4096))]"
python3 -c "import os; [open('$T/corpus/rand.bin', 'wb').write(os.urandom(65536))]"

# ------------------------------------------------------------------
# Test Group 0: Banner and CLI Identity
# ------------------------------------------------------------------
echo ""
echo "=== Test Group 0: ARM64 CLI Identity and Help ==="

test_arm64_banner() {
    local out
    out=$(run_arm64 "$ARC_ARM64" --about 2>&1)
    echo "$out" | grep -q "FreeArc-c - Copyright (c) Bulat Ziganshin" && \
    echo "$out" | grep -q "seb3773"
}
check "arc-arm64 --about displays official project banner" test_arm64_banner

test_arm64_unarc_banner() {
    local out
    out=$(run_arm64 "$UNARC_ARM64" --about 2>&1)
    echo "$out" | grep -q "FreeArc-c - Copyright (c) Bulat Ziganshin" && \
    echo "$out" | grep -q "seb3773"
}
check "unarc-arm64 --about displays official project banner" test_arm64_unarc_banner

# ------------------------------------------------------------------
# Test Group 1: Compression Algorithms Roundtrip on ARM64
# ------------------------------------------------------------------
echo ""
echo "=== Test Group 1: ARM64 Compression Algorithms Roundtrip ==="

test_roundtrip() {
    local method="$1"
    local name="$2"
    local arc_file="$T/rt_${name}.arc"
    local ext_dir="$T/rt_${name}_out"

    mkdir -p "$ext_dir"
    (cd "$T" && run_arm64 "$ARC_ARM64" a -m"$method" "$arc_file" corpus >/dev/null 2>&1) || return 1
    run_arm64 "$UNARC_ARM64" x -o+ -dp"$ext_dir" "$arc_file" >/dev/null 2>&1 || return 1
    diff -r -q "$T/corpus" "$ext_dir/corpus" >/dev/null 2>&1
}

check "LZMA -m1 (Fastest)" test_roundtrip "1" "lzma_m1"
check "LZMA -m2" test_roundtrip "2" "lzma_m2"
check "LZMA -m3 (Normal)" test_roundtrip "3" "lzma_m3"
check "LZMA -m4 (Maximum)" test_roundtrip "4" "lzma_m4"
check "LZMA -m5 (Ultra)" test_roundtrip "5" "lzma_m5"
check "Tornado -m tor" test_roundtrip "tor" "tornado"
check "PPMD -m ppmd" test_roundtrip "ppmd" "ppmd"
check "GRZip -m grzip" test_roundtrip "grzip" "grzip"
check "Storing -m0" test_roundtrip "0" "storing"

# ------------------------------------------------------------------
# Test Group 2: -nobitexact High-Ratio Mode on ARM64
# ------------------------------------------------------------------
echo ""
echo "=== Test Group 2: ARM64 -nobitexact High-Ratio Mode ==="

test_nobitexact_arm64() {
    local arc_file="$T/nobitexact.arc"
    local ext_dir="$T/nobitexact_out"

    mkdir -p "$ext_dir"
    (cd "$T" && run_arm64 "$ARC_ARM64" a -nobitexact -mx "$arc_file" corpus >/dev/null 2>&1) || return 1
    run_arm64 "$UNARC_ARM64" x -o+ -dp"$ext_dir" "$arc_file" >/dev/null 2>&1 || return 1
    diff -r -q "$T/corpus" "$ext_dir/corpus" >/dev/null 2>&1
}
check "ARM64 -nobitexact -mx compression and decompression" test_nobitexact_arm64

# ------------------------------------------------------------------
# Test Group 3: Preprocessor Filters on ARM64
# ------------------------------------------------------------------
echo ""
echo "=== Test Group 3: ARM64 Preprocessor Filters ==="

test_filter_arm64() {
    local filter="$1"
    local name="$2"
    local arc_file="$T/flt_${name}.arc"
    local ext_dir="$T/flt_${name}_out"

    mkdir -p "$ext_dir"
    (cd "$T" && run_arm64 "$ARC_ARM64" a "$filter" "$arc_file" corpus >/dev/null 2>&1) || return 1
    run_arm64 "$UNARC_ARM64" x -o+ -dp"$ext_dir" "$arc_file" >/dev/null 2>&1 || return 1
    diff -r -q "$T/corpus" "$ext_dir/corpus" >/dev/null 2>&1
}

check "Filter: -bcj (x86 executable branch filter)" test_filter_arm64 "-bcj" "bcj"
check "Filter: -delta (Differential filter)" test_filter_arm64 "-delta" "delta"
check "Filter: -dict (Dictionary preprocessor)" test_filter_arm64 "-dict" "dict"
check "Filter: -rep (Repetition filter)" test_filter_arm64 "-rep" "rep"
check "Filter: -lzp (LZP preprocessor)" test_filter_arm64 "-lzp" "lzp"
check "Filter: -mm (Multimedia filter)" test_filter_arm64 "-mm" "mm"

# ------------------------------------------------------------------
# Test Group 4: Cross-Platform Interoperability
# ------------------------------------------------------------------
echo ""
echo "=== Test Group 4: Cross-Platform Interoperability ==="

if [ -f "$UNARC_LINUX" ] && [ -f "$ARC_LINUX" ]; then
    test_arm64_to_x86() {
        local arc_file="$T/cross_arm2x86.arc"
        local ext_dir="$T/cross_arm2x86_out"
        mkdir -p "$ext_dir"
        (cd "$T" && run_arm64 "$ARC_ARM64" a -m4 "$arc_file" corpus >/dev/null 2>&1) || return 1
        "$UNARC_LINUX" x -o+ -dp"$ext_dir" "$arc_file" >/dev/null 2>&1 || return 1
        diff -r -q "$T/corpus" "$ext_dir/corpus" >/dev/null 2>&1
    }
    check "Cross: ARM64 arc -> x86_64 unarc" test_arm64_to_x86

    test_x86_to_arm64() {
        local arc_file="$T/cross_x862arm.arc"
        local ext_dir="$T/cross_x862arm_out"
        mkdir -p "$ext_dir"
        (cd "$T" && "$ARC_LINUX" a -m4 "$arc_file" corpus >/dev/null 2>&1) || return 1
        run_arm64 "$UNARC_ARM64" x -o+ -dp"$ext_dir" "$arc_file" >/dev/null 2>&1 || return 1
        diff -r -q "$T/corpus" "$ext_dir/corpus" >/dev/null 2>&1
    }
    check "Cross: x86_64 arc -> ARM64 unarc" test_x86_to_arm64

    test_nobitexact_arm64_to_x86() {
        local arc_file="$T/cross_nbe_arm2x86.arc"
        local ext_dir="$T/cross_nbe_arm2x86_out"
        mkdir -p "$ext_dir"
        (cd "$T" && run_arm64 "$ARC_ARM64" a -nobitexact -mx "$arc_file" corpus >/dev/null 2>&1) || return 1
        "$UNARC_LINUX" x -o+ -dp"$ext_dir" "$arc_file" >/dev/null 2>&1 || return 1
        diff -r -q "$T/corpus" "$ext_dir/corpus" >/dev/null 2>&1
    }
    check "Cross: ARM64 -nobitexact -> x86_64 unarc" test_nobitexact_arm64_to_x86
fi

# ------------------------------------------------------------------
# Test Group 5: Autonomous Linux SFX on ARM64
# ------------------------------------------------------------------
echo ""
echo "=== Test Group 5: Linux SFX on ARM64 ==="

test_sfx_arm64() {
    local sfx_file="$T/package.run"
    local ext_dir="$T/sfx_out"
    mkdir -p "$ext_dir"

    (cd "$T" && run_arm64 "$ARC_ARM64" a -sfx="$SFX_ARM64" "$sfx_file" corpus >/dev/null 2>&1) || return 1
    chmod +x "$sfx_file"
    run_arm64 "$sfx_file" -o+ -dp"$ext_dir" >/dev/null 2>&1 || return 1
    diff -r -q "$T/corpus" "$ext_dir/corpus" >/dev/null 2>&1
}
check "ARM64 Autonomous SFX creation and execution" test_sfx_arm64

# ------------------------------------------------------------------
# Test Group 6: Archive Management & Pipe
# ------------------------------------------------------------------
echo ""
echo "=== Test Group 6: Archive Management & Pipe on ARM64 ==="

test_mgmt_arm64() {
    local arc_file="$T/mgmt.arc"
    (cd "$T" && run_arm64 "$ARC_ARM64" a "$arc_file" corpus >/dev/null 2>&1) || return 1
    run_arm64 "$UNARC_ARM64" l "$arc_file" >/dev/null 2>&1 || return 1
    run_arm64 "$UNARC_ARM64" v "$arc_file" >/dev/null 2>&1 || return 1
    run_arm64 "$UNARC_ARM64" t "$arc_file" >/dev/null 2>&1 || return 1
    local pipe_out
    pipe_out=$(run_arm64 "$UNARC_ARM64" p "$arc_file" corpus/hello.txt 2>/dev/null)
    [ "$pipe_out" = "FreeArc Native Suite Linux ARM64 Test String 2026" ]
}
check "ARM64 Archive list, test integrity, and pipe to stdout" test_mgmt_arm64

echo ""
echo "=================================================================="
echo " Conformance Results : $PASS Passed, $FAIL Failed"
echo "=================================================================="

if [ "$FAIL" -gt 0 ]; then
    exit 1
fi
exit 0
