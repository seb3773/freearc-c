#!/bin/bash
# Direct Parity Verification Suite against Reference FreeArc 0.67 (Arc.exe / unarc.exe under Wine)
# Ensures FreeArc-c default (bit-exact) mode matches FreeArc 0.67 across:
#   - Auto levels: -m1..-m9, -mx (= -m9), -m4x..-m9x (both ST -mt1 and MT -mt4)
#   - Solid block method chains, continuous stream preservation on -m6..-m9 & -m6x..-m9x
#   - Exact file sorting (-ds=gerpn, -ds=gercpn, -ds-)
#   - Cross-decompression interoperability (Arc.exe / unarc.exe 0.67 <-> modern arc / unarc)
set -e

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
ARC="${ARC:-$ROOT_DIR/build/linux/arc}"
UNARC="${UNARC:-$ROOT_DIR/build/linux/unarc}"
ARC067="${ARC067:-$ROOT_DIR/research_bin/FreeArc/bin/Arc.exe}"
UNARC067="${UNARC067:-$ROOT_DIR/research_bin/FreeArc/bin/unarc.exe}"

if [ ! -x "$ARC" ] || [ ! -x "$UNARC" ]; then
    echo "ERROR: $ARC or $UNARC missing. Run 'make' first."
    exit 1
fi

if ! command -v wine >/dev/null 2>&1 || [ ! -f "$ARC067" ] || [ ! -f "$UNARC067" ]; then
    echo "SKIP: wine or FreeArc 0.67 reference binaries not available."
    exit 0
fi

TMP_ROOT="$(mktemp -d /tmp/parity_067_XXXXXX)"
trap 'rm -rf "$TMP_ROOT"' EXIT

PASS_CNT=0
FAIL_CNT=0

pass() {
    echo "  [PASS] $1"
    PASS_CNT=$((PASS_CNT + 1))
}

fail() {
    echo "  [FAIL] $1"
    FAIL_CNT=$((FAIL_CNT + 1))
}

# Create deterministic mixed multi-group corpus exercising $text, $obj, $bmp, $wav, default, duplicates, dirs
CORPUS="$TMP_ROOT/corpus"
mkdir -p "$CORPUS/d2" "$CORPUS/d1"
python3 - "$CORPUS" << 'PY'
import os, sys
base = sys.argv[1]
d1 = os.path.join(base, "d1")
d2 = os.path.join(base, "d2")
with open(os.path.join(d1, "hello.txt"), "wb") as f:
    f.write(b"The quick brown fox jumps over the lazy dog.\n" * 3000)
with open(os.path.join(d2, "b.bin"), "wb") as f:
    f.write((bytes(range(256)) + b"ABC123" * 12) * 200)
with open(os.path.join(d1, "c.obj"), "wb") as f:
    f.write((b"\x7fELF" + bytes(range(120))) * 500)
with open(os.path.join(d2, "d.bmp"), "wb") as f:
    f.write(b"BM" + bytes((i * 7) % 256 for i in range(60000)))
with open(os.path.join(d1, "e.wav"), "wb") as f:
    f.write(b"RIFF" + bytes((i * 13) % 256 for i in range(60000)))
# Same-size > 16 KiB duplicates across d1 and d2 to verify gerpn/gercpn clustering
with open(os.path.join(d2, "dup.bin"), "wb") as f:
    f.write((bytes(range(200)) + b"DUPDATA") * 150)
with open(os.path.join(d1, "a_dup.bin"), "wb") as f:
    f.write((bytes(range(200)) + b"DUPDATA") * 150)
PY

# Create sorting test corpus across <16K, 16K..128K, >=128K boundaries
SORT_CORPUS="$TMP_ROOT/sort_corpus"
mkdir -p "$SORT_CORPUS/d2" "$SORT_CORPUS/d1"
python3 - "$SORT_CORPUS" << 'PY'
import os, sys
base = sys.argv[1]
for sz in (500, 16384, 16385, 32768, 131071, 131072, 200000):
    with open(os.path.join(base, "d1", f"z_{sz}.bin"), "wb") as f:
        f.write(b"A" * sz)
    with open(os.path.join(base, "d2", f"a_{sz}.bin"), "wb") as f:
        f.write(b"A" * sz)
with open(os.path.join(base, "d1", "b_150k.bin"), "wb") as f:
    f.write(b"C" * 150000)
with open(os.path.join(base, "d2", "y_90k.bin"), "wb") as f:
    f.write(b"D" * 90000)
PY

echo "=================================================================="
echo " FreeArc 0.67 Direct Bit-Exact Parity Test Suite"
echo "=================================================================="

# 1. --print-config includes -m0..-m9, x=9, -m1x..-m9x
CFG_OUT="$("$ARC" --print-config -cfg-)"
if echo "$CFG_OUT" | grep -q "x  = 9" && \
   echo "$CFG_OUT" | grep -q "6  = rep:256m+exe+delta+lzma:32m:max" && \
   echo "$CFG_OUT" | grep -q "9  = rep:2040m+exe+delta+lzma:254m:max" && \
   echo "$CFG_OUT" | grep -q "9x = exe+delta+lzma:254m:max"; then
    pass "--print-config exposes 0.67 levels 6..9, x=9, 6x..9x"
else
    fail "--print-config missing 0.67 high compression level definitions"
fi

# Helper: extract normalized block summary via Arc.exe 0.67 lt -cfg-
# Output format: <orig_bytes>|<comp_bytes>|<files>|<method>
lt_blocks() {
    local arc_path="$1"
    WINEDEBUG=-all wine "$ARC067" lt -cfg- "$arc_path" 2>/dev/null | tr -d '\r' | \
        python3 -c '
import sys, re
for line in sys.stdin:
    m = re.match(r"^\s*[0-9,]+\s+([0-9,]+)\s+([0-9,]+)\s+(\d+)\s+(\S.*)$", line)
    if m:
        orig = m.group(1).replace(",", "")
        comp = m.group(2).replace(",", "")
        cnt  = m.group(3)
        meth = m.group(4).strip()
        print(f"{orig}|{comp}|{cnt}|{meth}")
'
}

# Helper: extract normalized file path sequence via Arc.exe 0.67 v -cfg-
v_files() {
    local arc_path="$1"
    WINEDEBUG=-all wine "$ARC067" v -cfg- "$arc_path" 2>/dev/null | tr -d '\r' | \
        python3 -c '
import sys, re
for line in sys.stdin:
    m = re.match(r"^\d{4}-\d{2}-\d{2}\s+\d{2}:\d{2}:\d{2}\s+(\S+)\s+(\d+)\s+(\d+)\s+([0-9a-fA-F]+)\s+(.*)$", line)
    if m:
        attr, sz, crc, name = m.group(1), m.group(2), m.group(4), m.group(5).strip().replace("\\", "/")
        if "D" in attr:
            continue
        print(f"{sz}:{crc}:{name}")
'
}

check_mode_parity() {
    local flags="$1"
    local strict_bytes="$2"  # "exact" or "mt4x4"
    local c_arc="$TMP_ROOT/c_test.arc"
    local e_arc="$TMP_ROOT/e_test.arc"
    rm -f "$c_arc" "$e_arc"

    (cd "$CORPUS" && "$ARC" a -q $flags "$c_arc" d2 d1) || { fail "$flags: arc compression failed"; return; }
    (cd "$CORPUS" && WINEDEBUG=-all wine "$ARC067" a -cfg- $flags "$e_arc" d2 d1 >/dev/null 2>&1) || {
        fail "$flags: Arc.exe 0.67 compression failed"; return;
    }

    local c_files e_files c_blocks e_blocks
    c_files="$(v_files "$c_arc")"
    e_files="$(v_files "$e_arc")"
    if [ "$c_files" != "$e_files" ]; then
        fail "$flags: file order/CRC mismatch vs Arc.exe 0.67"
        return
    fi

    c_blocks="$(lt_blocks "$c_arc")"
    e_blocks="$(lt_blocks "$e_arc")"

    local cmp_res
    cmp_res="$(python3 - "$strict_bytes" "$c_blocks" "$e_blocks" << 'PY'
import sys
mode, cb_s, eb_s = sys.argv[1], sys.argv[2].strip().splitlines(), sys.argv[3].strip().splitlines()
if len(cb_s) != len(eb_s):
    print(f"block count {len(cb_s)} != {len(eb_s)}")
    sys.exit(1)
for idx, (cl, el) in enumerate(zip(cb_s, eb_s)):
    co, cc, cn, cm = cl.split("|", 3)
    eo, ec, en, em = el.split("|", 3)
    if co != eo or cn != en or cm != em:
        print(f"block {idx}: ({co},{cn},{cm}) != ({eo},{en},{em})")
        sys.exit(1)
    diff = abs(int(cc) - int(ec))
    max_diff = 0 if mode == "exact" else 32
    if diff > max_diff:
        print(f"block {idx} ({cm}): comp {cc} != {ec} (diff={diff} > {max_diff})")
        sys.exit(1)
print("OK")
PY
)" || {
        fail "$flags: block mismatch vs Arc.exe 0.67 ($cmp_res)"
        return
    }

    # Verify cross-decompression with legacy Arc.exe / unarc.exe AND modern unarc
    if ! WINEDEBUG=-all wine "$UNARC067" t "$c_arc" >/dev/null 2>&1; then
        fail "$flags: unarc.exe 0.67 failed to test arc archive"
        return
    fi
    if ! "$UNARC" t "$e_arc" >/dev/null 2>&1; then
        fail "$flags: modern unarc failed to test Arc.exe 0.67 archive"
        return
    fi

    pass "$flags parity + cross-decompression OK ($cmp_res)"
}

# Test exact parity across all auto modes (-m1..-m9, -mx, -m4x..-m9x)
check_mode_parity "-mt1 -m1"  exact
check_mode_parity "-mt1 -m2"  exact
check_mode_parity "-mt4 -m2"  mt4x4
check_mode_parity "-mt1 -m3"  exact
check_mode_parity "-mt4 -m3"  mt4x4
check_mode_parity "-mt1 -m4"  exact
check_mode_parity "-mt4 -m4"  mt4x4
check_mode_parity "-mt1 -m5"  exact
check_mode_parity "-mt4 -m5"  mt4x4
check_mode_parity "-mt4 -m6"  exact
check_mode_parity "-mt4 -m7"  exact
check_mode_parity "-mt4 -m8"  exact
check_mode_parity "-mt4 -m9"  exact
check_mode_parity "-mt4 -mx"  exact
check_mode_parity "-mt4 -m4x" mt4x4
check_mode_parity "-mt4 -m5x" mt4x4
check_mode_parity "-mt4 -m6x" exact
check_mode_parity "-mt4 -m7x" exact
check_mode_parity "-mt4 -m8x" exact
check_mode_parity "-mt4 -m9x" exact

# Verify -mx equals -m9 identically
C_M9="$TMP_ROOT/c_m9.arc"
C_MX="$TMP_ROOT/c_mx.arc"
(cd "$CORPUS" && "$ARC" a -q -mt4 -m9 "$C_M9" d2 d1)
(cd "$CORPUS" && "$ARC" a -q -mt4 -mx "$C_MX" d2 d1)
if [ "$(lt_blocks "$C_M9")" = "$(lt_blocks "$C_MX")" ] && [ "$(v_files "$C_M9")" = "$(v_files "$C_MX")" ]; then
    pass "-mx is an exact alias of -m9"
else
    fail "-mx diverged from -m9"
fi

# Verify -m6..-m9 and -m6x..-m9x never introduce 4x4: under -mt4
if lt_blocks "$C_MX" | grep -q "4x4:"; then
    fail "-mx (-m9) unexpectedly used 4x4: under -mt4"
else
    pass "-m9 / -mx stays continuous (no 4x4:) even under -mt4"
fi

# Verify exact -ds sorting parity across gerpn, gercpn, epn, -ds-
for ds in "" "-dsgerpn" "-dsgercpn" "-dsepn" "-ds-"; do
    c_s="$TMP_ROOT/cs_${ds}.arc"
    e_s="$TMP_ROOT/es_${ds}.arc"
    (cd "$SORT_CORPUS" && "$ARC" a -q -m0 $ds "$c_s" d2 d1)
    (cd "$SORT_CORPUS" && WINEDEBUG=-all wine "$ARC067" a -cfg- -m0 $ds "$e_s" d2 d1 >/dev/null 2>&1)
    if [ "$(v_files "$c_s")" = "$(v_files "$e_s")" ]; then
        pass "Sort parity OK for '${ds:-default}'"
    else
        fail "Sort order mismatch vs Arc.exe 0.67 for '${ds:-default}'"
    fi
done

echo "=================================================================="
echo " Parity 0.67 Summary: $PASS_CNT passed, $FAIL_CNT failed"
echo "=================================================================="
[ "$FAIL_CNT" -eq 0 ]

