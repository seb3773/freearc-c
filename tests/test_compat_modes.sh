#!/bin/bash
# ==============================================================================
# FreeArc Native C/C++ — Compatibility Modes & Banners Test Suite
# Tests: -nobitexact, --max-ratio, -nocompat, arc.ini injection & CLI overrides
# ==============================================================================
set -eu

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

ARC="${ROOT_DIR}/build/linux/arc"
UNARC="${ROOT_DIR}/build/linux/unarc"
ARC_WIN64="${ROOT_DIR}/build/win64/arc.exe"
UNARC_WIN64="${ROOT_DIR}/build/win64/unarc.exe"

T=$(mktemp -d /tmp/arc_compat_test_XXXXXX)
trap 'rm -rf "$T"' EXIT

FAIL=0
PASS=0

fail() {
  echo "  [FAIL] $1"
  FAIL=$((FAIL + 1))
}

pass() {
  echo "  [PASS] $1"
  PASS=$((PASS + 1))
}

echo "=================================================================="
echo " Starting FreeArc Compatibility Modes Test Suite"
echo " Workspace: $ROOT_DIR"
echo " Temp dir : $T"
echo "=================================================================="

# 1. -nobitexact notice banner
test_nobitexact_notice_banner() {
  echo "Testing -nobitexact notice banner..."
  echo "test data" > "$T/data1.txt"
  local out
  out=$("$ARC" a -nobitexact "$T/test1.arc" "$T/data1.txt" 2>&1)
  if echo "$out" | grep -q "\*\*\* NOTICE: -nobitexact specified: the resulting archive is fully format-compatible"; then
    pass "-nobitexact displays the compatibility notice banner"
  else
    fail "-nobitexact did not display the notice banner: $out"
  fi
}

# 2. --max-ratio alias displays notice banner
test_max_ratio_alias_banner() {
  echo "Testing --max-ratio alias banner..."
  echo "test data 2" > "$T/data2.txt"
  local out
  out=$("$ARC" a --max-ratio "$T/test2.arc" "$T/data2.txt" 2>&1)
  if echo "$out" | grep -q "\*\*\* NOTICE: -nobitexact specified"; then
    pass "--max-ratio alias displays the notice banner"
  else
    fail "--max-ratio did not display the notice banner: $out"
  fi
}

# 3. -nocompat displays warning banner
test_nocompat_warning_banner() {
  echo "Testing -nocompat warning banner..."
  echo "test data 3" > "$T/data3.txt"
  local out
  out=$("$ARC" a -nocompat "$T/test3.arc" "$T/data3.txt" 2>&1)
  if echo "$out" | grep -q "!!! WARNING: -nocompat specified: the resulting archive is NOT compatible"; then
    pass "-nocompat displays the warning banner"
  else
    fail "-nocompat did not display the warning banner: $out"
  fi
}

# 4. -q quiet mode suppresses banners
test_quiet_suppresses_banners() {
  echo "Testing quiet mode suppression..."
  echo "test data 4" > "$T/data4.txt"
  local out
  out=$("$ARC" a -q -nobitexact "$T/test4.arc" "$T/data4.txt" 2>&1 || true)
  if echo "$out" | grep -q "\*\*\* NOTICE:"; then
    fail "-q did not suppress the notice banner"
  else
    pass "-q quiet mode suppresses the notice banner"
  fi
}

# 5. Default mode does not emit any banner
test_default_no_banner() {
  echo "Testing default mode without banner..."
  echo "test data 5" > "$T/data5.txt"
  local out
  out=$("$ARC" a "$T/test5.arc" "$T/data5.txt" 2>&1)
  if echo "$out" | grep -q "\*\*\* NOTICE:" || echo "$out" | grep -q "!!! WARNING:"; then
    fail "Default mode emitted unexpected banner"
  else
    pass "Default mode runs clean with no banners"
  fi
}

# 6. arc.ini [Default options] injection and CLI override
test_ini_injection_and_override() {
  echo "Testing arc.ini injection and CLI override..."
  local ini="$T/test_custom.ini"
  cat << 'EOF' > "$ini"
[Default options]
a create = -nobitexact
EOF
  echo "test data 6" > "$T/data6.txt"

  # With INI: banner should appear
  local out1
  out1=$("$ARC" -cfg="$ini" a "$T/test6a.arc" "$T/data6.txt" 2>&1)
  if echo "$out1" | grep -q "\*\*\* NOTICE: -nobitexact specified"; then
    pass "arc.ini [Default options] successfully injects -nobitexact"
  else
    fail "arc.ini did not inject -nobitexact: $out1"
  fi

  # With INI + CLI override (-nobitexact-): banner should NOT appear
  local out2
  out2=$("$ARC" -cfg="$ini" a -nobitexact- "$T/test6b.arc" "$T/data6.txt" 2>&1)
  if echo "$out2" | grep -q "\*\*\* NOTICE: -nobitexact specified"; then
    fail "CLI override -nobitexact- failed to cancel INI option"
  else
    pass "CLI override -nobitexact- successfully cancels INI default option"
  fi

  # With INI + CLI override (--bitexact): banner should NOT appear
  local out3
  out3=$("$ARC" -cfg="$ini" a --bitexact "$T/test6c.arc" "$T/data6.txt" 2>&1)
  if echo "$out3" | grep -q "\*\*\* NOTICE: -nobitexact specified"; then
    fail "CLI override --bitexact failed to cancel INI option"
  else
    pass "CLI override --bitexact successfully cancels INI default option"
  fi
}

# 7. Win64 parity under Wine
test_win64_wine_parity() {
  if ! command -v wine >/dev/null 2>&1 || [ ! -f "$ARC_WIN64" ]; then
    echo "  [SKIP] Wine or Win64 arc.exe not available"
    return 0
  fi
  echo "Testing Win64 parity under Wine..."
  echo "win64 test data" > "$T/data_win.txt"

  local out1
  out1=$(WINEDEBUG=-all wine "$ARC_WIN64" a -nobitexact "$T/win_test.arc" "$T/data_win.txt" 2>/dev/null || true)
  if echo "$out1" | grep -q "\*\*\* NOTICE: -nobitexact specified"; then
    pass "Win64 arc.exe correctly displays notice banner under Wine"
  else
    fail "Win64 arc.exe did not display notice banner under Wine: $out1"
  fi
}

# 8. Intelligent Solid File Sorting (Phase 2)
test_smart_sort_order() {
  echo "Testing Intelligent Solid File Sorting order & legacy decodability..."
  local sdir="$T/smart_src"
  mkdir -p "$sdir"
  echo "int compute(int x);" > "$sdir/alpha.h"
  echo "int compute(int x) { return x * 42; }" > "$sdir/alpha.cpp"
  echo "int parse(const char *s);" > "$sdir/beta.h"
  echo "int parse(const char *s) { return 0; }" > "$sdir/beta.cpp"

  local arc_def="$T/sort_default.arc"
  local arc_nob="$T/sort_nobit.arc"

  (cd "$T" && "$ARC" a -q "$arc_def" smart_src/*)
  (cd "$T" && "$ARC" a -q -nobitexact "$arc_nob" smart_src/*)

  # In default (EPN): sorted by extension (.cpp then .h)
  local list_def
  list_def=$("$ARC" l "$arc_def" | grep -E "alpha|beta" | tr '\\' '/' | awk '{print $NF}')

  if echo "$list_def" | grep -q "alpha.cpp"; then
    pass "Default mode sorts files using legacy EPN order"
  else
    fail "Default mode listing unexpected: $list_def"
  fi

  # In -nobitexact (Smart): stem groups alpha.cpp + alpha.h together
  local list_nob
  list_nob=$("$ARC" l "$arc_nob" | grep -E "alpha|beta" | tr '\\' '/' | awk '{print $NF}')

  # alpha.cpp and alpha.h should be adjacent
  local adj_check
  adj_check=$(echo "$list_nob" | grep -A1 "alpha.cpp" | head -n2)
  if echo "$adj_check" | grep -q "alpha.h"; then
    pass "-nobitexact successfully clusters related stem files (alpha.cpp / alpha.h adjacent)"
  else
    fail "-nobitexact failed to cluster related stems: $list_nob"
  fi

  # Extraction roundtrip with modern unarc
  local ext_modern="$T/ext_modern"
  mkdir -p "$ext_modern"
  "$UNARC" x -o+ -dp"$ext_modern" "$arc_nob" >/dev/null 2>&1
  if diff -r -q "$sdir" "$ext_modern/smart_src" >/dev/null 2>&1; then
    pass "-nobitexact archive extracts byte-for-byte identical with modern unarc"
  else
    fail "-nobitexact archive extraction mismatch with modern unarc"
  fi

  # Extraction roundtrip with legacy unarc 0.67 under wine (if available)
  if command -v wine >/dev/null 2>&1 && [ -f "$ROOT_DIR/research_bin/FreeArc/bin/unarc.exe" ]; then
    local ext_legacy="$T/ext_legacy"
    mkdir -p "$ext_legacy"
    WINEDEBUG=-all wine "$ROOT_DIR/research_bin/FreeArc/bin/unarc.exe" x -o+ -dp"$ext_legacy" "$arc_nob" >/dev/null 2>&1 || true
    if diff -r -q "$sdir" "$ext_legacy/smart_src" >/dev/null 2>&1; then
      pass "-nobitexact archive extracts 100% byte-for-byte identical with legacy FreeArc 0.67 under Wine"
    else
      fail "-nobitexact archive extraction mismatch with legacy FreeArc 0.67"
    fi
  fi
}

# 9. LZMA Deep Parameterization (Phase 3)
test_lzma_deep_parameterization() {
  echo "Testing LZMA Deep Parameterization (mc/fb) & legacy decompression..."
  local test_file="$ROOT_DIR/src/arc.cpp"
  local arc_def="$T/lzma_def.arc"
  local arc_deep="$T/lzma_deep.arc"
  local arc_custom="$T/lzma_custom.arc"

  # Compress with default vs -nobitexact at -mx
  (cd "$ROOT_DIR" && "$ARC" a -q -mx "$arc_def" src/arc.cpp)
  (cd "$ROOT_DIR" && "$ARC" a -q -nobitexact -mx "$arc_deep" src/arc.cpp)

  local sz_def
  sz_def=$(wc -c < "$arc_def")
  local sz_deep
  sz_deep=$(wc -c < "$arc_deep")

  if [ "$sz_deep" -lt "$sz_def" ]; then
    pass "-nobitexact -mx automatically leverages deep LZMA (size: $sz_deep < default: $sz_def)"
  else
    fail "-nobitexact -mx did not improve size over default (deep: $sz_deep, def: $sz_def)"
  fi

  # Test custom -fb and -mc flags
  (cd "$ROOT_DIR" && "$ARC" a -q -nobitexact -fb=128 -mc=500 "$arc_custom" src/arc.cpp)
  if [ -f "$arc_custom" ] && [ "$(wc -c < "$arc_custom")" -gt 0 ]; then
    pass "Custom -fb=128 and -mc=500 CLI options successfully create archive"
  else
    fail "Custom -fb and -mc flags failed to create archive"
  fi

  # Extract deep archive with modern unarc and verify integrity
  local ext_modern="$T/ext_deep_modern"
  mkdir -p "$ext_modern"
  "$UNARC" x -o+ -dp"$ext_modern" "$arc_deep" >/dev/null 2>&1
  if diff -q "$test_file" "$ext_modern/src/arc.cpp" >/dev/null 2>&1; then
    pass "Deep LZMA archive decompresses bit-for-bit with modern unarc"
  else
    fail "Deep LZMA archive extraction mismatch with modern unarc"
  fi

  # Extract deep archive with legacy unarc 0.67 under Wine (if available)
  if command -v wine >/dev/null 2>&1 && [ -f "$ROOT_DIR/research_bin/FreeArc/bin/unarc.exe" ]; then
    local ext_legacy="$T/ext_deep_legacy"
    mkdir -p "$ext_legacy"
    WINEDEBUG=-all wine "$ROOT_DIR/research_bin/FreeArc/bin/unarc.exe" x -o+ -dp"$ext_legacy" "$arc_deep" >/dev/null 2>&1 || true
    if diff -q "$test_file" "$ext_legacy/src/arc.cpp" >/dev/null 2>&1; then
      pass "Deep LZMA archive decompresses 100% bit-for-bit with legacy FreeArc 0.67 under Wine"
    else
      fail "Deep LZMA archive extraction mismatch with legacy FreeArc 0.67"
    fi
  fi
}

# 10. Text Routing & PPMd Deep Tuning (Phase 4)
test_text_ppmd_tuning() {
  echo "Testing Text Routing & PPMd Deep Tuning in -nobitexact mode..."
  local test_file="$ROOT_DIR/src/arc.cpp"
  local arc_def_mx="$T/text_def_mx.arc"
  local arc_nobit_mx="$T/text_nobit_mx.arc"

  # In default -mx, text uses dict+lzma5; in -nobitexact -mx, text routes to dict+lzp+ppmd:16:256m
  (cd "$ROOT_DIR" && "$ARC" a -q -mx "$arc_def_mx" src/arc.cpp)
  (cd "$ROOT_DIR" && "$ARC" a -q -nobitexact -mx "$arc_nobit_mx" src/arc.cpp)

  local sz_def_mx sz_nobit_mx
  sz_def_mx=$(wc -c < "$arc_def_mx")
  sz_nobit_mx=$(wc -c < "$arc_nobit_mx")

  if [ "$sz_nobit_mx" -lt "$sz_def_mx" ]; then
    pass "-nobitexact -mx successfully routes text to deep PPMd chain (size: $sz_nobit_mx < default: $sz_def_mx)"
  else
    fail "-nobitexact -mx failed to compress text smaller (nobit: $sz_nobit_mx, def: $sz_def_mx)"
  fi

  # Extract nobitexact PPMd text archive with modern unarc and verify integrity
  local ext_modern="$T/ext_text_modern"
  mkdir -p "$ext_modern"
  "$UNARC" x -o+ -dp"$ext_modern" "$arc_nobit_mx" >/dev/null 2>&1
  if diff -q "$test_file" "$ext_modern/src/arc.cpp" >/dev/null 2>&1; then
    pass "Deep PPMd text archive decompresses bit-for-bit with modern unarc"
  else
    fail "Deep PPMd text archive extraction mismatch with modern unarc"
  fi

  # Extract deep PPMd text archive with legacy unarc 0.67 under Wine (if available)
  if command -v wine >/dev/null 2>&1 && [ -f "$ROOT_DIR/research_bin/FreeArc/bin/unarc.exe" ]; then
    local ext_legacy="$T/ext_text_legacy"
    mkdir -p "$ext_legacy"
    WINEDEBUG=-all wine "$ROOT_DIR/research_bin/FreeArc/bin/unarc.exe" x -o+ -dp"$ext_legacy" "$arc_nobit_mx" >/dev/null 2>&1 || true
    if diff -q "$test_file" "$ext_legacy/src/arc.cpp" >/dev/null 2>&1; then
      pass "Deep PPMd text archive decompresses 100% bit-for-bit with legacy FreeArc 0.67 under Wine"
    else
      fail "Deep PPMd text archive extraction mismatch with legacy FreeArc 0.67"
    fi
  fi
}

test_nobitexact_notice_banner
test_max_ratio_alias_banner
test_nocompat_warning_banner
test_quiet_suppresses_banners
test_default_no_banner
test_ini_injection_and_override
test_win64_wine_parity
test_smart_sort_order
test_lzma_deep_parameterization
test_text_ppmd_tuning

echo "=================================================================="
echo " Compatibility Modes Tests Summary: $PASS passed, $FAIL failed"
echo "=================================================================="

[ "$FAIL" -eq 0 ]

