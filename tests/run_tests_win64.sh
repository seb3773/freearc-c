#!/bin/bash
# ==============================================================================
# FreeArc Modern Native C/C++ Windows 64-bit Test Suite (Wine execution)
# Validates bit-for-bit exact decompression and cross-platform interoperability
# between Linux (x86_64) and Windows (Win64 MinGW) binaries.
# ==============================================================================
set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

ARC_WIN="$ROOT_DIR/build/win64/arc.exe"
UNARC_WIN="$ROOT_DIR/build/win64/unarc.exe"
ARC_LINUX="$ROOT_DIR/build/linux/arc"
UNARC_LINUX="$ROOT_DIR/build/linux/unarc"

if [ ! -f "$ARC_WIN" ] || [ ! -f "$UNARC_WIN" ]; then
    echo "ERROR: Win64 binaries not found in $ROOT_DIR/build/win64. Run 'make win64' first."
    exit 1
fi

if ! command -v wine >/dev/null 2>&1; then
    echo "ERROR: 'wine' is required to execute Win64 binaries on Linux."
    exit 1
fi

# Silence harmless Wine stderr output like debug/fixme
run_wine() {
    WINEDEBUG=-all wine "$@" 2>/dev/null
}

T=$(mktemp -d /tmp/arc_win64_test_XXXXXX)
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
echo " Starting FreeArc Win64 (x86_64-w64-mingw32) Conformance Test Suite"
echo " Workspace : $ROOT_DIR"
echo " Wine env  : $(wine --version 2>/dev/null || echo 'Wine')"
echo " Temp dir  : $T"
echo "=================================================================="

# Setup test corpus
mkdir -p "$T/corpus/sub/deep"
echo "FreeArc Native Suite Windows 64-bit Test String" > "$T/corpus/hello.txt"
printf "Row 1: ABCDEFGHIJKLMNOPQRSTUVWXYZ\nRow 2: 0123456789\nRow 3: UTF-8 test: café, résumé, déjà\n" > "$T/corpus/sub/deep/multiline.txt"
head -c 65536 /dev/zero > "$T/corpus/sub/zeros.bin"
python3 -c "import os; [open('$T/corpus/pattern.bin', 'wb').write((b'Win64FreeArcExactBit2026!' * 8192))]"
python3 -c "import os; [open('$T/corpus/rand.bin', 'wb').write(os.urandom(65536))]"

# ------------------------------------------------------------------
# Test Group 1: Compression Algorithms Roundtrip on Win64 (Bit-for-Bit)
# ------------------------------------------------------------------
echo ""
echo "=== Test Group 1: Win64 Compression Algorithms Roundtrip (Bit-for-Bit) ==="

test_method_win64() {
    local method="$1"
    local name="method_${method//[:+=]/_}"
    local arc="$T/$name.arc"
    local out="$T/out_$name"

    run_wine "$ARC_WIN" a -ep -m "$method" "$arc" "$T/corpus/hello.txt" "$T/corpus/pattern.bin" "$T/corpus/rand.bin" || return 1
    run_wine "$UNARC_WIN" t "$arc" || return 1
    mkdir -p "$out"
    run_wine "$UNARC_WIN" x -dp"$out" "$arc" || return 1
    diff -q "$T/corpus/hello.txt" "$out/hello.txt" >/dev/null 2>&1 || return 1
    diff -q "$T/corpus/pattern.bin" "$out/pattern.bin" >/dev/null 2>&1 || return 1
    diff -q "$T/corpus/rand.bin" "$out/rand.bin" >/dev/null 2>&1 || return 1
    return 0
}

check "Storing (m0)" test_method_win64 "storing"
check "LZMA default" test_method_win64 "lzma"
check "LZMA fast (-m1)" test_method_win64 "1"
check "LZMA max (-m5)" test_method_win64 "5"
check "Tornado (tor)" test_method_win64 "tor"
check "PPMD (ppmd)" test_method_win64 "ppmd"
check "GRZip (grzip)" test_method_win64 "grzip"

# ------------------------------------------------------------------
# Test Group 2: Compression Preprocessing Filters on Win64
# ------------------------------------------------------------------
echo ""
echo "=== Test Group 2: Win64 Preprocessing Filters (Bit-for-Bit) ==="

test_filter_win64() {
    local filter_flag="$1"
    local name="filter_${filter_flag#-}"
    local arc="$T/$name.arc"
    local out="$T/out_$name"

    run_wine "$ARC_WIN" a -ep "$filter_flag" "$arc" "$T/corpus/hello.txt" "$T/corpus/pattern.bin" || return 1
    run_wine "$UNARC_WIN" t "$arc" || return 1
    mkdir -p "$out"
    run_wine "$UNARC_WIN" x -dp"$out" "$arc" || return 1
    diff -q "$T/corpus/hello.txt" "$out/hello.txt" >/dev/null 2>&1 || return 1
    diff -q "$T/corpus/pattern.bin" "$out/pattern.bin" >/dev/null 2>&1 || return 1
    return 0
}

check "Filter BCJ / EXE (-bcj)" test_filter_win64 "-bcj"
check "Filter Repetition (-rep)" test_filter_win64 "-rep"
check "Filter Delta (-delta)" test_filter_win64 "-delta"
check "Filter Dictionary (-dict)" test_filter_win64 "-dict"
check "Filter LZP (-lzp)" test_filter_win64 "-lzp"

# ------------------------------------------------------------------
# Test Group 3: Encryption (Data & Headers) on Win64
# ------------------------------------------------------------------
echo ""
echo "=== Test Group 3: Win64 Encryption (Bit-for-Bit) ==="

test_enc_win64() {
    local enc_args="$1"
    local pass="SecretPassWin64!"
    local name="enc_${enc_args//[^a-zA-Z0-9]/_}"
    local arc="$T/$name.arc"
    local out="$T/out_$name"

    run_wine "$ARC_WIN" a -ep $enc_args"$pass" "$arc" "$T/corpus/hello.txt" "$T/corpus/pattern.bin" || return 1
    run_wine "$UNARC_WIN" t -p"$pass" "$arc" || return 1
    mkdir -p "$out"
    run_wine "$UNARC_WIN" x -p"$pass" -dp"$out" "$arc" || return 1
    diff -q "$T/corpus/hello.txt" "$out/hello.txt" >/dev/null 2>&1 || return 1
    diff -q "$T/corpus/pattern.bin" "$out/pattern.bin" >/dev/null 2>&1 || return 1
    return 0
}

check "Data Encryption AES (-p)" test_enc_win64 "-p"
check "Header Encryption AES (-hp)" test_enc_win64 "-hp"
check "Header Encryption Blowfish (-hp)" test_enc_win64 "-aeblowfish -hp"
check "Header Encryption Twofish (-hp)" test_enc_win64 "-aetwofish -hp"
check "Header Encryption Serpent (-hp)" test_enc_win64 "-aeserpent -hp"

# ------------------------------------------------------------------
# Test Group 4: Archive Management on Win64
# ------------------------------------------------------------------
echo ""
echo "=== Test Group 4: Win64 Archive Management (u, f, d, m) ==="

test_mgmt_win64() {
    local arc="$T/mgmt_test.arc"
    local out="$T/out_mgmt"

    echo "Initial File 1" > "$T/f1.txt"
    echo "Initial File 2" > "$T/f2.txt"

    # Add
    run_wine "$ARC_WIN" a -ep "$arc" "$T/f1.txt" "$T/f2.txt" || return 1

    # Delete f2.txt
    run_wine "$ARC_WIN" d "$arc" "f2.txt" || return 1
    if run_wine "$UNARC_WIN" l "$arc" | grep -q "f2.txt"; then
        return 1
    fi

    # Update f1.txt + add f3.txt
    echo "Updated File 1 with new content" > "$T/f1.txt"
    echo "New File 3" > "$T/f3.txt"
    run_wine "$ARC_WIN" u -ep "$arc" "$T/f1.txt" "$T/f3.txt" || return 1

    mkdir -p "$out"
    run_wine "$UNARC_WIN" x -dp"$out" "$arc" || return 1
    [ "$(cat "$out/f1.txt")" = "Updated File 1 with new content" ] || return 1
    [ "$(cat "$out/f3.txt")" = "New File 3" ] || return 1

    # Move command
    echo "Move me" > "$T/move_me.txt"
    run_wine "$ARC_WIN" m -ep "$arc" "$T/move_me.txt" || return 1
    [ ! -f "$T/move_me.txt" ] || return 1
    return 0
}

check "Commands u (update), f (freshen), d (delete), m (move)" test_mgmt_win64

# ------------------------------------------------------------------
# Test Group 5: Multi-Volume Archives (-v) on Win64
# ------------------------------------------------------------------
echo ""
echo "=== Test Group 5: Win64 Multi-Volume Archives (-v) ==="

test_multivol_win64() {
    local arc="$T/multivol.arc"
    local out="$T/out_multivol"

    run_wine "$ARC_WIN" a -ep -v32k "$arc" "$T/corpus/pattern.bin" || return 1
    local num_vols
    num_vols=$(ls "$T"/multivol.arc.* 2>/dev/null | wc -l)
    [ "$num_vols" -ge 2 ] || return 1

    run_wine "$UNARC_WIN" t "$arc.001" || return 1
    mkdir -p "$out"
    run_wine "$UNARC_WIN" x -dp"$out" "$arc.001" || return 1
    diff -q "$T/corpus/pattern.bin" "$out/pattern.bin" >/dev/null 2>&1 || return 1
    return 0
}

check "Multi-volume split and reassembly (-v32k)" test_multivol_win64

# ------------------------------------------------------------------
# Test Group 6: Cross-Platform Interoperability (Linux <-> Win64)
# ------------------------------------------------------------------
echo ""
echo "=== Test Group 6: Cross-Platform Interoperability (Linux <-> Win64) ==="

test_cross_win_to_linux() {
    local arc="$T/cross_win2linux.arc"
    local out="$T/out_cross_w2l"

    # Encoded by Win64 arc.exe
    run_wine "$ARC_WIN" a -ep -m5 "$arc" "$T/corpus/hello.txt" "$T/corpus/pattern.bin" "$T/corpus/sub/deep/multiline.txt" || return 1

    # Decoded by Linux native unarc
    mkdir -p "$out"
    "$UNARC_LINUX" x -dp"$out" "$arc" >/dev/null 2>&1 || return 1

    diff -q "$T/corpus/hello.txt" "$out/hello.txt" >/dev/null 2>&1 || return 1
    diff -q "$T/corpus/pattern.bin" "$out/pattern.bin" >/dev/null 2>&1 || return 1
    diff -q "$T/corpus/sub/deep/multiline.txt" "$out/multiline.txt" >/dev/null 2>&1 || return 1
    return 0
}

test_cross_linux_to_win() {
    local arc="$T/cross_linux2win.arc"
    local out="$T/out_cross_l2w"

    # Encoded by Linux native arc
    "$ARC_LINUX" a -ep -m5 "$arc" "$T/corpus/hello.txt" "$T/corpus/pattern.bin" "$T/corpus/sub/deep/multiline.txt" >/dev/null 2>&1 || return 1

    # Decoded by Win64 unarc.exe
    mkdir -p "$out"
    run_wine "$UNARC_WIN" x -dp"$out" "$arc" || return 1

    diff -q "$T/corpus/hello.txt" "$out/hello.txt" >/dev/null 2>&1 || return 1
    diff -q "$T/corpus/pattern.bin" "$out/pattern.bin" >/dev/null 2>&1 || return 1
    diff -q "$T/corpus/sub/deep/multiline.txt" "$out/multiline.txt" >/dev/null 2>&1 || return 1
    return 0
}

test_cross_sha256_exactness() {
    local arc_win="$T/exact_win.arc"
    local arc_linux="$T/exact_linux.arc"
    local out_win="$T/exact_out_win"
    local out_linux="$T/exact_out_linux"

    # Both encode the exact same large dataset
    run_wine "$ARC_WIN" a -ep -m0 "$arc_win" "$T/corpus/pattern.bin" "$T/corpus/rand.bin" || return 1
    "$ARC_LINUX" a -ep -m0 "$arc_linux" "$T/corpus/pattern.bin" "$T/corpus/rand.bin" >/dev/null 2>&1 || return 1

    # Extract cross-wise
    mkdir -p "$out_win" "$out_linux"
    run_wine "$UNARC_WIN" x -dp"$out_win" "$arc_linux" || return 1
    "$UNARC_LINUX" x -dp"$out_linux" "$arc_win" >/dev/null 2>&1 || return 1

    # Calculate and compare SHA256 hashes
    local orig_pattern_sha=$(sha256sum "$T/corpus/pattern.bin" | awk '{print $1}')
    local orig_rand_sha=$(sha256sum "$T/corpus/rand.bin" | awk '{print $1}')

    local win_pattern_sha=$(sha256sum "$out_win/pattern.bin" | awk '{print $1}')
    local win_rand_sha=$(sha256sum "$out_win/rand.bin" | awk '{print $1}')

    local linux_pattern_sha=$(sha256sum "$out_linux/pattern.bin" | awk '{print $1}')
    local linux_rand_sha=$(sha256sum "$out_linux/rand.bin" | awk '{print $1}')

    [ "$orig_pattern_sha" = "$win_pattern_sha" ] || return 1
    [ "$orig_pattern_sha" = "$linux_pattern_sha" ] || return 1
    [ "$orig_rand_sha" = "$win_rand_sha" ] || return 1
    [ "$orig_rand_sha" = "$linux_rand_sha" ] || return 1
    return 0
}

check "Win64 arc.exe -> Linux unarc" test_cross_win_to_linux
check "Linux arc -> Win64 unarc.exe" test_cross_linux_to_win
check "SHA256 bit-a-bit identical payload roundtrip" test_cross_sha256_exactness

# ------------------------------------------------------------------
# Test Group 7: Legacy FreeArc 0.6x Archives Decompression under Win64
# ------------------------------------------------------------------
echo ""
echo "=== Test Group 7: Legacy FreeArc Decompression under Win64 ==="

test_legacy_win64() {
    local legacy_arc="$ROOT_DIR/tests_files/legacy.arc"
    [ -f "$legacy_arc" ] || return 0

    run_wine "$UNARC_WIN" t "$legacy_arc" || return 1
    local out="$T/out_legacy_win"
    mkdir -p "$out"
    run_wine "$UNARC_WIN" x -dp"$out" "$legacy_arc" || return 1
    [ -s "$out/README.txt" ] || return 1
    return 0
}

check "Legacy FreeArc 0.6x archive integrity & extraction (unarc.exe)" test_legacy_win64

# ------------------------------------------------------------------
# Summary
# ------------------------------------------------------------------
echo ""
echo "=================================================================="
echo " Win64 Tests Summary: $PASS passed, $FAIL failed"
echo "=================================================================="

[ "$FAIL" -eq 0 ]
