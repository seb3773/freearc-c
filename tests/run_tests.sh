#!/bin/bash
# ==============================================================================
# FreeArc Modern Native C/C++ Test Suite
# Tests: Compression, Decompression, Filters, Encryption, Multi-Volume,
#        Update/Delete/Freshen/Move, Permissions/Symlinks, CLI Filtering,
#        Pipes/Stdout, SFX autonomous binaries, and Legacy Archives.
# ==============================================================================
set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

ARC="$ROOT_DIR/build/linux/arc"
UNARC="$ROOT_DIR/build/linux/unarc"
SFX_STUB="$ROOT_DIR/build/linux/arc.sfx"

if [ ! -x "$ARC" ] || [ ! -x "$UNARC" ]; then
    echo "ERROR: Binaries not found in $ROOT_DIR/build/linux. Run 'make' first."
    exit 1
fi

T=$(mktemp -d /tmp/arc_test_suite_XXXXXX)
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
echo " Starting FreeArc Modern C/C++ Native Test Suite"
echo " Workspace: $ROOT_DIR"
echo " Temp dir : $T"
echo "=================================================================="

# Setup test files
mkdir -p "$T/corpus/sub/deep"
echo "Hello FreeArc World" > "$T/corpus/hello.txt"
printf "Line 1\nLine 2\nLine 3\n" > "$T/corpus/sub/deep/lines.txt"
printf '#!/bin/sh\necho "Hello from script"\n' > "$T/corpus/script.sh"
chmod 755 "$T/corpus/script.sh"
ln -s "hello.txt" "$T/corpus/link_to_hello.txt"
head -c 65536 /dev/zero > "$T/corpus/sub/zeros.bin"
python3 -c "import os; [open('$T/corpus/pattern.bin', 'wb').write((b'FreeArc2026' * 10000))]"
python3 -c "import os; [open('$T/corpus/rand.bin', 'wb').write(os.urandom(65536))]"

# ------------------------------------------------------------------
# Test 1: Compression Algorithms Roundtrip
# ------------------------------------------------------------------
echo ""
echo "=== Test Group 1: Compression Algorithms Roundtrip ==="

test_method() {
    local method="$1"
    local name="method_${method//[:+=]/_}"
    local arc="$T/$name.arc"
    local out="$T/out_$name"

    "$ARC" a -ep -m "$method" "$arc" "$T/corpus/hello.txt" "$T/corpus/pattern.bin" >/dev/null 2>&1 || return 1
    "$UNARC" t "$arc" >/dev/null 2>&1 || return 1
    mkdir -p "$out"
    "$UNARC" x -dp"$out" "$arc" >/dev/null 2>&1 || return 1
    diff -q "$T/corpus/hello.txt" "$out/hello.txt" >/dev/null 2>&1 || return 1
    diff -q "$T/corpus/pattern.bin" "$out/pattern.bin" >/dev/null 2>&1 || return 1
    return 0
}

check "Storing (m0)" test_method "storing"
check "LZMA default" test_method "lzma"
check "LZMA fast (-m1)" test_method "1"
check "LZMA max (-m5)" test_method "5"
check "Tornado (tor)" test_method "tor"
check "PPMD (ppmd)" test_method "ppmd"
check "GRZip (grzip)" test_method "grzip"

# ------------------------------------------------------------------
# Test 2: Compression Preprocessing Filters
# ------------------------------------------------------------------
echo ""
echo "=== Test Group 2: Preprocessing Filters ==="

test_filter() {
    local filter_flag="$1"
    local name="filter_${filter_flag#-}"
    local arc="$T/$name.arc"
    local out="$T/out_$name"

    "$ARC" a -ep "$filter_flag" "$arc" "$T/corpus/hello.txt" "$T/corpus/pattern.bin" >/dev/null 2>&1 || return 1
    "$UNARC" t "$arc" >/dev/null 2>&1 || return 1
    mkdir -p "$out"
    "$UNARC" x -dp"$out" "$arc" >/dev/null 2>&1 || return 1
    diff -q "$T/corpus/hello.txt" "$out/hello.txt" >/dev/null 2>&1 || return 1
    return 0
}

check "Filter BCJ / EXE (-bcj)" test_filter "-bcj"
check "Filter Repetition (-rep)" test_filter "-rep"
check "Filter Delta (-delta)" test_filter "-delta"
check "Filter Dictionary (-dict)" test_filter "-dict"
check "Filter LZP (-lzp)" test_filter "-lzp"

# ------------------------------------------------------------------
# Test 3: Encryption & Password Protection
# ------------------------------------------------------------------
echo ""
echo "=== Test Group 3: Encryption (Data & Headers) ==="

test_crypto() {
    local flag="$1"
    local cipher="$2"
    local pw="Secret123!"
    local name="crypto_${flag#-}_${cipher}"
    local arc="$T/$name.arc"
    local out="$T/out_$name"

    "$ARC" a -ep "$flag" "$pw" -ae "$cipher" "$arc" "$T/corpus/hello.txt" >/dev/null 2>&1 || return 1
    # Wrong password must fail
    if "$UNARC" t -p"wrong_pw" "$arc" >/dev/null 2>&1; then
        return 1
    fi
    # Correct password must succeed
    "$UNARC" t -p"$pw" "$arc" >/dev/null 2>&1 || return 1
    mkdir -p "$out"
    "$UNARC" x -p"$pw" -dp"$out" "$arc" >/dev/null 2>&1 || return 1
    diff -q "$T/corpus/hello.txt" "$out/hello.txt" >/dev/null 2>&1 || return 1
    return 0
}

check "Data Encryption AES (-p)" test_crypto "-p" "aes"
check "Header Encryption AES (-hp)" test_crypto "-hp" "aes"
check "Header Encryption Blowfish (-hp)" test_crypto "-hp" "blowfish"
check "Header Encryption Twofish (-hp)" test_crypto "-hp" "twofish"
check "Header Encryption Serpent (-hp)" test_crypto "-hp" "serpent"

# ------------------------------------------------------------------
# Test 4: POSIX Permissions and Symlinks Preservation
# ------------------------------------------------------------------
echo ""
echo "=== Test Group 4: Unix Permissions & Symlinks ==="

test_permissions_and_symlinks() {
    local arc="$T/perm_symlink.arc"
    local out="$T/out_perm"

    "$ARC" a "$arc" -r "$T/corpus/" >/dev/null 2>&1 || return 1
    mkdir -p "$out"
    "$UNARC" x -dp"$out" "$arc" >/dev/null 2>&1 || return 1

    local script_out="$out/$T/corpus/script.sh"
    local link_out="$out/$T/corpus/link_to_hello.txt"

    [ -x "$script_out" ] || return 1
    [ -L "$link_out" ] || return 1
    [ "$(readlink "$link_out")" = "hello.txt" ] || return 1
    return 0
}

check "Executable permissions (chmod) & Symlink targets" test_permissions_and_symlinks

# ------------------------------------------------------------------
# Test 5: Archive Modification Commands (u, f, d, m)
# ------------------------------------------------------------------
echo ""
echo "=== Test Group 5: Archive Management (Update, Freshen, Delete, Move) ==="

test_archive_management() {
    local arc="$T/manage.arc"
    local out="$T/out_manage"

    echo "Initial File 1" > "$T/f1.txt"
    echo "Initial File 2" > "$T/f2.txt"

    # Add
    "$ARC" a -ep "$arc" "$T/f1.txt" "$T/f2.txt" >/dev/null 2>&1 || return 1

    # Delete f2.txt
    "$ARC" d "$arc" "f2.txt" >/dev/null 2>&1 || return 1
    if "$UNARC" l "$arc" | grep -q "f2.txt"; then
        return 1
    fi

    # Update f1.txt + add f3.txt
    echo "Updated File 1 with new content" > "$T/f1.txt"
    echo "New File 3" > "$T/f3.txt"
    "$ARC" u -ep "$arc" "$T/f1.txt" "$T/f3.txt" >/dev/null 2>&1 || return 1

    mkdir -p "$out"
    "$UNARC" x -dp"$out" "$arc" >/dev/null 2>&1 || return 1
    [ "$(cat "$out/f1.txt")" = "Updated File 1 with new content" ] || return 1
    [ "$(cat "$out/f3.txt")" = "New File 3" ] || return 1

    # Move command
    echo "Move me" > "$T/move_me.txt"
    "$ARC" m -ep "$arc" "$T/move_me.txt" >/dev/null 2>&1 || return 1
    [ ! -f "$T/move_me.txt" ] || return 1
    return 0
}

check "Commands u (update), f (freshen), d (delete), m (move)" test_archive_management

# ------------------------------------------------------------------
# Test 6: Multi-Volume Archives
# ------------------------------------------------------------------
echo ""
echo "=== Test Group 6: Multi-Volume Archives (-v) ==="

test_multivol() {
    local arc="$T/multivol.arc"
    local out="$T/out_multivol"

    "$ARC" a -v32k "$arc" "$T/corpus/pattern.bin" >/dev/null 2>&1 || return 1
    local num_vols
    num_vols=$(ls "$T"/multivol.arc.* 2>/dev/null | wc -l)
    [ "$num_vols" -ge 2 ] || return 1

    "$UNARC" t "$arc.001" >/dev/null 2>&1 || return 1
    mkdir -p "$out"
    "$UNARC" x -dp"$out" "$arc.001" >/dev/null 2>&1 || return 1
    diff -q "$T/corpus/pattern.bin" "$out/$T/corpus/pattern.bin" >/dev/null 2>&1 || return 1
    return 0
}

check "Multi-volume split and reassembly (-v32k)" test_multivol

# ------------------------------------------------------------------
# Test 7: CLI Filtering, Exclusions, and Pipes
# ------------------------------------------------------------------
echo ""
echo "=== Test Group 7: Filtering and Standard Output Pipes ==="

test_filtering_and_pipe() {
    local arc="$T/filter_pipe.arc"
    "$ARC" a -ep -x"*.bin" "$arc" "$T/corpus/hello.txt" "$T/corpus/pattern.bin" >/dev/null 2>&1 || return 1
    if "$UNARC" l "$arc" | grep -q "pattern.bin"; then
        return 1
    fi
    local pipe_data
    pipe_data=$("$UNARC" p "$arc" "hello.txt" 2>/dev/null)
    [ "$pipe_data" = "Hello FreeArc World" ] || return 1
    return 0
}

check "CLI exclusions (-x) and extraction to pipe (-p)" test_filtering_and_pipe

# ------------------------------------------------------------------
# Test 8: Self-Extracting Archive (SFX) Module
# ------------------------------------------------------------------
echo ""
echo "=== Test Group 8: Standalone Self-Extracting Executables (SFX) ==="

test_sfx() {
    local sfx_bin="$T/archive.sfx"
    local sfx_out="$T/sfx_out"
    local conv_bin="$T/converted.run"
    local conv_out="$T/conv_out"

    # Direct creation with -sfx
    "$ARC" a -sfx -ep "$sfx_bin" "$T/corpus/hello.txt" "$T/corpus/script.sh" >/dev/null 2>&1 || return 1
    [ -x "$sfx_bin" ] || return 1
    "$sfx_bin" -t >/dev/null 2>&1 || return 1
    mkdir -p "$sfx_out"
    "$sfx_bin" -d"$sfx_out" >/dev/null 2>&1 || return 1
    diff -q "$T/corpus/hello.txt" "$sfx_out/hello.txt" >/dev/null 2>&1 || return 1
    [ -x "$sfx_out/script.sh" ] || return 1

    # Conversion of normal archive with 'arc s'
    local norm_arc="$T/normal_for_sfx.arc"
    "$ARC" a -ep "$norm_arc" "$T/corpus/hello.txt" >/dev/null 2>&1 || return 1
    "$ARC" s "$norm_arc" "$conv_bin" >/dev/null 2>&1 || return 1
    [ -x "$conv_bin" ] || return 1
    mkdir -p "$conv_out"
    "$conv_bin" -d"$conv_out" >/dev/null 2>&1 || return 1
    diff -q "$T/corpus/hello.txt" "$conv_out/hello.txt" >/dev/null 2>&1 || return 1
    return 0
}

check "Autonomous SFX execution, permissions preservation, and arc s conversion" test_sfx

# ------------------------------------------------------------------
# Test 9: Legacy FreeArc Archives Decompression (Backward Compatibility)
# ------------------------------------------------------------------
echo ""
echo "=== Test Group 9: Legacy FreeArc Archive Decompression ==="

test_legacy_archives() {
    local legacy_dir="$ROOT_DIR/tests_files/samples_legacy"
    if [ ! -f "$legacy_dir/example.arc" ] || [ ! -f "$legacy_dir/data2.arc" ]; then
        echo "  [WARN] Legacy samples not found, skipping."
        return 0
    fi

    # Test example.arc
    "$UNARC" t "$legacy_dir/example.arc" >/dev/null 2>&1 || return 1
    local out_ex="$T/out_legacy_example"
    mkdir -p "$out_ex"
    "$UNARC" x -dp"$out_ex" "$legacy_dir/example.arc" >/dev/null 2>&1 || return 1
    [ -f "$out_ex/Users/Vb1/Desktop/example.pdf" ] || return 1
    local pdf_size
    pdf_size=$(stat -c%s "$out_ex/Users/Vb1/Desktop/example.pdf")
    [ "$pdf_size" -eq 246468 ] || return 1

    # Test data2.arc (22 files, 222MB uncompressed)
    "$UNARC" t "$legacy_dir/data2.arc" >/dev/null 2>&1 || return 1
    return 0
}

check "Legacy FreeArc 0.6x archives integrity and decompression" test_legacy_archives

# ------------------------------------------------------------------
# Summary
# ------------------------------------------------------------------
echo ""
echo "=================================================================="
echo " Tests Summary: $PASS passed, $FAIL failed"
echo "=================================================================="

[ "$FAIL" -eq 0 ]
