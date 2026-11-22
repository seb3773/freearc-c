#!/usr/bin/env python3
"""
Full Matt Mahoney 10 GB Benchmark Runner for FreeArc -nobitexact -mx
"""
import subprocess
import os
import time
import sys

CORPUS_DIR = "/home/cdef/_PROJETS/compression_bench/mm_10gb"
ARC_BIN = "/home/cdef/_PROJETS/ARC/build/linux/arc"
OUT_ARC = "/tmp/mm10gb_nobitexact.arc"
UNARC_WIN = "/home/cdef/_PROJETS/ARC/research_bin/FreeArc/bin/unarc.exe"

# Historic baselines (from next_evolutions.md)
BASELINE_LEGACY_ARC = 3_961_251_304 # 3.69 GiB
BASELINE_7ZIP_MX9   = 3_652_683_873 # 3.40 GiB
BASELINE_PHASE4_NBX = 3_828_990_597 # 3.56 GiB

def main():
    if not os.path.exists(CORPUS_DIR):
        print(f"Error: Corpus dir {CORPUS_DIR} not found!")
        sys.exit(1)
    if not os.path.exists(ARC_BIN):
        print(f"Error: Binary {ARC_BIN} not found! Compile first.")
        sys.exit(1)

    if os.path.exists(OUT_ARC):
        os.remove(OUT_ARC)

    print("=" * 80)
    print("STARTING FULL MATT MAHONEY 10 GB BENCHMARK (-nobitexact -mx)")
    print(f"Corpus: {CORPUS_DIR}")
    print(f"Output: {OUT_ARC}")
    print("=" * 80)

    cmd = [
        ARC_BIN, "a", "-nobitexact", "-mx", OUT_ARC, "-r", "*"
    ]

    t0 = time.time()
    proc = subprocess.Popen(
        cmd,
        cwd=CORPUS_DIR,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1
    )

    file_count = 0
    last_print = time.time()
    for line in proc.stdout:
        if "Compressing " in line:
            file_count += 1
            now = time.time()
            if now - last_print >= 10.0 or file_count % 5000 == 0:
                elapsed = now - t0
                print(f"[{elapsed:6.1f}s] Processed {file_count:,} files... Current: {line.strip()[:60]}", flush=True)
                last_print = now
        elif "All OK" in line or "WARNING:" in line or "ERROR:" in line or "*** NOTICE" in line:
            print(f"  {line.strip()}", flush=True)

    proc.wait()
    total_time = time.time() - t0

    if proc.returncode != 0:
        print(f"ERROR: Arc exited with return code {proc.returncode}!")
        sys.exit(proc.returncode)

    if not os.path.exists(OUT_ARC):
        print("ERROR: Output archive was not created!")
        sys.exit(1)

    arc_size = os.path.getsize(OUT_ARC)

    print("\n" + "=" * 80)
    print("BENCHMARK COMPLETED SUCCESSFULLY")
    print("=" * 80)
    print(f"Archive Size        : {arc_size:,} bytes ({arc_size / (1024**3):.4f} GiB)")
    print(f"Compression Time    : {total_time:.2f} s ({total_time / 60:.2f} min)")
    print("-" * 80)

    # Comparisons
    diff_legacy = BASELINE_LEGACY_ARC - arc_size
    diff_phase4 = BASELINE_PHASE4_NBX - arc_size
    gap_7z = arc_size - BASELINE_7ZIP_MX9

    print(f"Gain vs Legacy FreeArc 0.67 : -{diff_legacy:,} B (-{(diff_legacy / BASELINE_LEGACY_ARC) * 100:.2f}%)")
    print(f"Gain vs Phase 4 nobitexact  : -{diff_phase4:,} B (-{(diff_phase4 / BASELINE_PHASE4_NBX) * 100:.2f}%)")
    if gap_7z <= 0:
        print(f"Difference vs 7-Zip 26.02   : -{-gap_7z:,} B (FreeArc BEATS 7-Zip by {(-gap_7z / (1024*1024)):.2f} MB!)")
    else:
        gap_closed = (BASELINE_LEGACY_ARC - arc_size) / (BASELINE_LEGACY_ARC - BASELINE_7ZIP_MX9) * 100
        print(f"Remaining Gap vs 7-Zip 26.02: +{gap_7z:,} B (+{(gap_7z / BASELINE_7ZIP_MX9) * 100:.2f}%) [{gap_closed:.1f}% of total gap closed]")
    print("=" * 80)

    print("\nVerifying archive with historical Wine unarc.exe 0.67 ...")
    env = dict(os.environ, WINEDEBUG="-all")
    u = subprocess.run(["wine", UNARC_WIN, "t", OUT_ARC], capture_output=True, text=True, env=env)
    print("Wine test output:", "All OK" if "All OK" in u.stdout else "FAILED")
    print("=" * 80)

if __name__ == "__main__":
    main()

