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

test_nobitexact_notice_banner
test_max_ratio_alias_banner
test_nocompat_warning_banner
test_quiet_suppresses_banners
test_default_no_banner
test_ini_injection_and_override
test_win64_wine_parity

echo "=================================================================="
echo " Compatibility Modes Tests Summary: $PASS passed, $FAIL failed"
echo "=================================================================="

[ "$FAIL" -eq 0 ]

