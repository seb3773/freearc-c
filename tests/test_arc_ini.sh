#!/bin/bash
# ==============================================================================
# FreeArc arc.ini Configuration System Test Suite
# Tests:
#   1. --print-config (arc & unarc)
#   2. Custom config via -cfg<file>, -cfg <file>, --config=<file>
#   3. Disabling config via -cfg-, --config-, --no-config
#   4. Default options injection ([Default options] & global options)
#   5. User CLI options override over defaults
#   6. Custom compression method aliases ([Compression methods] with # wildcards)
#   7. Multi-group slash compression methods from arc.ini
#   8. Auto-discovery of arc.ini in executable dir and current working dir
#   9. Win64 (MinGW) parity under Wine
# ==============================================================================
set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

ARC="$ROOT_DIR/build/linux/arc"
UNARC="$ROOT_DIR/build/linux/unarc"
ARC_WIN="$ROOT_DIR/build/win64/arc.exe"
UNARC_WIN="$ROOT_DIR/build/win64/unarc.exe"

if [ ! -x "$ARC" ] || [ ! -x "$UNARC" ]; then
    echo "ERROR: Linux binaries not found in $ROOT_DIR/build/linux. Run 'make' first."
    exit 1
fi

T=$(mktemp -d /tmp/arc_ini_test_XXXXXX)
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

run_wine() {
    WINEDEBUG=-all wine "$@" 2>/dev/null
}

echo "=================================================================="
echo " Starting FreeArc arc.ini Configuration Test Suite"
echo " Workspace: $ROOT_DIR"
echo " Temp dir : $T"
echo "=================================================================="

# Setup corpus
mkdir -p "$T/corpus"
echo "Hello FreeArc INI World!" > "$T/corpus/hello.txt"
printf "Some text lines for testing\nAnother line of data\n" > "$T/corpus/text.txt"
dd if=/dev/urandom of="$T/corpus/data.bin" bs=1024 count=32 status=none
python3 -c 'print("FreeArc repetitive data line for compression test\n" * 2000)' > "$T/corpus/pattern.bin"

# ------------------------------------------------------------------
# Test 1: --print-config displays method definitions
# ------------------------------------------------------------------
test_print_config() {
    local out_arc="$T/print_cfg_arc.txt"
    local out_unarc="$T/print_cfg_unarc.txt"

    "$ARC" --print-config > "$out_arc" 2>&1 || return 1
    grep -q "\[Compression methods\]" "$out_arc" || return 1
    grep -q "tor:4" "$out_arc" || return 1
    grep -q "All OK" "$out_arc" || return 1

    "$UNARC" --print-config > "$out_unarc" 2>&1 || return 1
    grep -q "\[Compression methods\]" "$out_unarc" || return 1
    grep -q "All OK" "$out_unarc" || return 1
    return 0
}
check "--print-config displays built-in compression methods (arc & unarc)" test_print_config

# ------------------------------------------------------------------
# Test 2: Custom config via -cfg and [Default options] injection
# ------------------------------------------------------------------
test_custom_config_default_options() {
    local cfg="$T/custom1.ini"
    cat << EOF > "$cfg"
; Global default options
--logfile=$T/test_arc.log

[Default options]
a create = -m0 -s-
x extract = -o+
EOF

    local arc_file="$T/custom_test.arc"
    local log_file="$T/test_arc.log"
    rm -f "$arc_file" "$log_file"

    # Running "arc a -cfg<file>" should use default -m0 and write logfile
    (cd "$T" && "$ARC" a -ep -cfg"$cfg" "$arc_file" corpus/hello.txt >/dev/null 2>&1) || return 1
    [ -f "$arc_file" ] || return 1
    [ -f "$log_file" ] || return 1
    grep -q "All OK" "$log_file" || return 1

    # Verify that -m0 (storing) was used: compressed size >= original size
    local arc_size=$(stat -c%s "$arc_file")
    local orig_size=$(stat -c%s "$T/corpus/hello.txt")
    [ "$arc_size" -ge "$orig_size" ] || return 1

    # Extract with unarc passing the same config file
    local out_dir="$T/out_custom1"
    mkdir -p "$out_dir"
    "$UNARC" x -cfg"$cfg" -dp"$out_dir" "$arc_file" >/dev/null 2>&1 || return 1
    diff -q "$T/corpus/hello.txt" "$out_dir/hello.txt" >/dev/null 2>&1 || return 1

    return 0
}
check "Custom config via -cfg and [Default options] injection" test_custom_config_default_options

# ------------------------------------------------------------------
# Test 3: User CLI options override [Default options]
# ------------------------------------------------------------------
test_user_options_override_defaults() {
    local cfg="$T/custom_override.ini"
    cat << 'EOF' > "$cfg"
[Default options]
a = -m0
x = -o-
EOF

    local arc_file="$T/override_test.arc"
    # Even though default is -m0, user passes -m2. The user option should override default -m0!
    "$ARC" a -ep -cfg"$cfg" -m2 "$arc_file" "$T/corpus/pattern.bin" >/dev/null 2>&1 || return 1
    [ -f "$arc_file" ] || return 1

    # With -m2, pattern.bin (100KB) compresses to ~2KB vs ~100KB with m0
    local arc_size=$(stat -c%s "$arc_file")

    local arc_m0="$T/override_m0.arc"
    "$ARC" a -ep -cfg"$cfg" "$arc_m0" "$T/corpus/pattern.bin" >/dev/null 2>&1 || return 1
    local m0_size=$(stat -c%s "$arc_m0")

    # Compressed with -m2 should be vastly smaller than storing (-m0)
    [ "$arc_size" -lt "$((m0_size / 2))" ] || return 1

    # Test extraction with user override -o+ overriding default -o-
    local out_dir="$T/out_override"
    mkdir -p "$out_dir"
    echo "old file" > "$out_dir/pattern.bin"
    "$UNARC" x -cfg"$cfg" -o+ -dp"$out_dir" "$arc_file" >/dev/null 2>&1 || return 1
    diff -q "$T/corpus/pattern.bin" "$out_dir/pattern.bin" >/dev/null 2>&1 || return 1

    return 0
}
check "User CLI options properly override [Default options]" test_user_options_override_defaults

# ------------------------------------------------------------------
# Test 4: Disabling config file via -cfg- and --no-config
# ------------------------------------------------------------------
test_disable_config() {
    # In a directory with an arc.ini setting default storing (-m0)
    local test_dir="$T/disable_test_dir"
    mkdir -p "$test_dir"
    cat << 'EOF' > "$test_dir/arc.ini"
[Default options]
a = -m0
EOF

    local arc_default="$test_dir/default.arc"
    local arc_disabled="$test_dir/disabled.arc"
    local arc_noconfig="$test_dir/noconfig.arc"

    # Default run discovers ./arc.ini -> uses -m0 (~100KB)
    (cd "$test_dir" && "$ARC" a -ep "$arc_default" "$T/corpus/pattern.bin" >/dev/null 2>&1) || return 1

    # -cfg- disables config -> user specifies -m1 -> compressed (~2KB)
    (cd "$test_dir" && "$ARC" a -ep -cfg- -m1 "$arc_disabled" "$T/corpus/pattern.bin" >/dev/null 2>&1) || return 1

    # --no-config disables config
    (cd "$test_dir" && "$ARC" a -ep --no-config -m1 "$arc_noconfig" "$T/corpus/pattern.bin" >/dev/null 2>&1) || return 1

    local size_default=$(stat -c%s "$arc_default")
    local size_disabled=$(stat -c%s "$arc_disabled")
    local size_noconfig=$(stat -c%s "$arc_noconfig")

    # m1 compression should be vastly smaller than m0
    [ "$size_disabled" -lt "$((size_default / 2))" ] || return 1
    [ "$size_disabled" -eq "$size_noconfig" ] || return 1

    return 0
}
check "Disabling config file via -cfg- and --no-config" test_disable_config

# ------------------------------------------------------------------
# Test 5: Custom compression method aliases and # wildcards
# ------------------------------------------------------------------
test_compression_methods_aliases() {
    local cfg="$T/methods.ini"
    cat << 'EOF' > "$cfg"
[Compression methods]
fastalias = tor:4
#my = tor:#
chainalias = rep:64m + tor:4
EOF

    local arc_fast="$T/fast.arc"
    local arc_wildcard="$T/wildcard.arc"
    local arc_chain="$T/chain.arc"

    # Test direct alias: -mfastalias
    "$ARC" a -ep --config="$cfg" -mfastalias "$arc_fast" "$T/corpus/text.txt" >/dev/null 2>&1 || return 1
    [ -f "$arc_fast" ] || return 1

    # Test # wildcard: -m3my -> expands to tor:3
    "$ARC" a -ep -cfg "$cfg" -m3my "$arc_wildcard" "$T/corpus/text.txt" >/dev/null 2>&1 || return 1
    [ -f "$arc_wildcard" ] || return 1

    # Test compound chain alias: -mchainalias
    "$ARC" a -ep -cfg="$cfg" -mchainalias "$arc_chain" "$T/corpus/text.txt" >/dev/null 2>&1 || return 1
    [ -f "$arc_chain" ] || return 1

    # Verify all 3 decompress bit-for-bit identical
    local out1="$T/out_fast"
    local out2="$T/out_wildcard"
    local out3="$T/out_chain"
    mkdir -p "$out1" "$out2" "$out3"

    "$UNARC" x -dp"$out1" "$arc_fast" >/dev/null 2>&1 || return 1
    "$UNARC" x -dp"$out2" "$arc_wildcard" >/dev/null 2>&1 || return 1
    "$UNARC" x -dp"$out3" "$arc_chain" >/dev/null 2>&1 || return 1

    diff -q "$T/corpus/text.txt" "$out1/text.txt" >/dev/null 2>&1 || return 1
    diff -q "$T/corpus/text.txt" "$out2/text.txt" >/dev/null 2>&1 || return 1
    diff -q "$T/corpus/text.txt" "$out3/text.txt" >/dev/null 2>&1 || return 1

    return 0
}
check "Custom compression method aliases and # wildcards" test_compression_methods_aliases

# ------------------------------------------------------------------
# Test 6: Multi-group slash compression methods from ini
# ------------------------------------------------------------------
test_slash_auto_methods() {
    local cfg="$T/slash_methods.ini"
    cat << 'EOF' > "$cfg"
[Compression methods]
splitmode = tor:4 / $text=tor:6
EOF

    local arc_file="$T/split.arc"
    "$ARC" a -ep -cfg="$cfg" -msplitmode "$arc_file" "$T/corpus/text.txt" "$T/corpus/data.bin" >/dev/null 2>&1 || return 1
    [ -f "$arc_file" ] || return 1

    local out="$T/out_split"
    mkdir -p "$out"
    "$UNARC" x -dp"$out" "$arc_file" >/dev/null 2>&1 || return 1

    diff -q "$T/corpus/text.txt" "$out/text.txt" >/dev/null 2>&1 || return 1
    diff -q "$T/corpus/data.bin" "$out/data.bin" >/dev/null 2>&1 || return 1

    return 0
}
check "Multi-group slash compression methods from arc.ini" test_slash_auto_methods

# ------------------------------------------------------------------
# Test 7: Auto-discovery in binary directory
# ------------------------------------------------------------------
test_autodiscovery_exe_dir() {
    local bin_dir="$T/bindir"
    mkdir -p "$bin_dir"
    cp "$ARC" "$bin_dir/arc"
    cp "$UNARC" "$bin_dir/unarc"

    # Place arc.ini in the same directory as the binaries
    cat << 'EOF' > "$bin_dir/arc.ini"
[Default options]
a = -m0
EOF

    local arc_file="$T/autodisc.arc"
    # Execute from a different directory without -cfg
    (cd "$T" && "$bin_dir/arc" a -ep "$arc_file" corpus/hello.txt >/dev/null 2>&1) || return 1
    [ -f "$arc_file" ] || return 1

    # Verify that -m0 was picked up from bin_dir/arc.ini
    local arc_size=$(stat -c%s "$arc_file")
    local orig_size=$(stat -c%s "$T/corpus/hello.txt")
    [ "$arc_size" -ge "$orig_size" ] || return 1

    local out="$T/out_autodisc"
    mkdir -p "$out"
    (cd "$T" && "$bin_dir/unarc" x -dp"$out" "$arc_file" >/dev/null 2>&1) || return 1
    diff -q "$T/corpus/hello.txt" "$out/hello.txt" >/dev/null 2>&1 || return 1

    return 0
}
check "Auto-discovery of arc.ini in executable directory" test_autodiscovery_exe_dir

# ------------------------------------------------------------------
# Test 8: Win64 Parity under Wine
# ------------------------------------------------------------------
test_win64_parity() {
    if ! command -v wine >/dev/null 2>&1 || [ ! -f "$ARC_WIN" ] || [ ! -f "$UNARC_WIN" ]; then
        echo "  [SKIP] Win64 testing (wine or win64 binaries not available)"
        return 0
    fi

    local cfg="$T/win64_test.ini"
    cat << 'EOF' > "$cfg"
[Default options]
a = -m0
x = -o+

[Compression methods]
winfast = tor:4
EOF

    local arc_file="$T/win64_test.arc"
    run_wine "$ARC_WIN" a -ep -cfg="$cfg" "$arc_file" "$T/corpus/hello.txt" || return 1
    [ -f "$arc_file" ] || return 1

    # Test print-config under Win64
    local print_out="$T/win64_print.txt"
    run_wine "$ARC_WIN" --print-config > "$print_out" 2>&1 || return 1
    grep -q "\[Compression methods\]" "$print_out" || return 1
    grep -q "All OK" "$print_out" || return 1

    # Test extraction with unarc.exe
    local out="$T/out_win64"
    mkdir -p "$out"
    run_wine "$UNARC_WIN" x -cfg="$cfg" -dp"$out" "$arc_file" || return 1
    diff -q "$T/corpus/hello.txt" "$out/hello.txt" >/dev/null 2>&1 || return 1

    return 0
}
check "Win64 arc.exe and unarc.exe parity under Wine" test_win64_parity

# ------------------------------------------------------------------
# Test 9: -nobitexact configuration via arc.ini & CLI override
# ------------------------------------------------------------------
test_nobitexact_ini_support() {
    # 1. arc = -nobitexact in [Default options]
    local cfg1="$T/nbx_cfg1.ini"
    printf "[Default options]\narc = -nobitexact\n" > "$cfg1"
    local out1="$T/out_nbx1.txt"
    "$ARC" a -ep -cfg="$cfg1" "$T/nbx1.arc" "$T/corpus/hello.txt" > "$out1" 2>&1 || return 1
    grep -q "NOTICE: -nobitexact specified" "$out1" || return 1

    # 2. CLI override with -nobitexact- should enforce bit-exact parity
    local out2="$T/out_nbx2.txt"
    "$ARC" a -ep -cfg="$cfg1" -nobitexact- "$T/nbx2.arc" "$T/corpus/hello.txt" > "$out2" 2>&1 || return 1
    if grep -q "NOTICE: -nobitexact specified" "$out2"; then return 1; fi

    # 3. Key-value style: nobitexact = true in [Options]
    local cfg3="$T/nbx_cfg3.ini"
    printf "[Options]\nnobitexact = true\n" > "$cfg3"
    local out3="$T/out_nbx3.txt"
    "$ARC" a -ep -cfg="$cfg3" "$T/nbx3.arc" "$T/corpus/hello.txt" > "$out3" 2>&1 || return 1
    grep -q "NOTICE: -nobitexact specified" "$out3" || return 1

    # 4. Bare option line under [Default options]
    local cfg4="$T/nbx_cfg4.ini"
    printf "[Default options]\n-nobitexact\n" > "$cfg4"
    local out4="$T/out_nbx4.txt"
    "$ARC" a -ep -cfg="$cfg4" "$T/nbx4.arc" "$T/corpus/hello.txt" > "$out4" 2>&1 || return 1
    grep -q "NOTICE: -nobitexact specified" "$out4" || return 1

    # 5. Top-level option before any section
    local cfg5="$T/nbx_cfg5.ini"
    printf '%s\n\n[Default options]\na = -m1\n' "-nobitexact" > "$cfg5"
    local out5="$T/out_nbx5.txt"
    "$ARC" a -ep -cfg="$cfg5" "$T/nbx5.arc" "$T/corpus/hello.txt" > "$out5" 2>&1 || return 1
    grep -q "NOTICE: -nobitexact specified" "$out5" || return 1

    # 6. Win64 under Wine parity with INI -nobitexact
    if command -v wine >/dev/null 2>&1 && [ -f "$ARC_WIN" ]; then
        local out_w64="$T/out_nbx_w64.txt"
        run_wine "$ARC_WIN" a -ep -cfg="$cfg1" "$T/nbx_w64.arc" "$T/corpus/hello.txt" > "$out_w64" 2>&1 || return 1
        grep -q "NOTICE: -nobitexact specified" "$out_w64" || return 1
    fi

    return 0
}
check "-nobitexact configuration via arc.ini and CLI override" test_nobitexact_ini_support

# ------------------------------------------------------------------
# Summary
# ------------------------------------------------------------------
echo ""
echo "=================================================================="
echo " arc.ini Tests Summary: $PASS passed, $FAIL failed"
echo "=================================================================="

[ "$FAIL" -eq 0 ]
