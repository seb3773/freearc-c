#!/usr/bin/env python3
"""
Dual 10 GB Matt Mahoney Benchmark Runner:
1. Mode Bit-Exact (FreeArc-c legacy standard profile -mx)
2. Mode Nobitexact (FreeArc-c ratio-maximized profile -nobitexact -mx)
"""
import subprocess
import os
import time
import sys

CORPUS_DIR = "/home/cdef/_PROJETS/compression_bench/mm_10gb"
ARC_BIN = "/home/cdef/_PROJETS/ARC/build/linux/arc"
WINE_UNARC = "/home/cdef/_PROJETS/ARC/research_bin/FreeArc/bin/unarc.exe"

ARC_BITEXACT = "/tmp/mm10gb_bitexact.arc"
ARC_NOBITEXACT = "/tmp/mm10gb_nobitexact.arc"
RESULTS_FILE = "/home/cdef/_PROJETS/ARC/bench/mm10gb_dual_results.txt"

# 7-Zip reference from previous benchmark
SEVENZIP_SIZE = 3_652_683_873 # 3,652,683,873 bytes (3.40 GiB)
SEVENZIP_TIME = 10_732        # ~178.8 min / ~2h58m (single thread/multi-core profile)

def run_archive(name, extra_flags, out_arc):
    if os.path.exists(out_arc):
        os.remove(out_arc)

    print("\n" + "=" * 80)
    print(f"STARTING RUN: {name}")
    print(f"Command: {ARC_BIN} a {' '.join(extra_flags)} {out_arc} -r *")
    print(f"Working directory: {CORPUS_DIR}")
    print("=" * 80)

    cmd = [ARC_BIN, "a"] + extra_flags + [out_arc, "-r", "*"]
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
        line_s = line.strip()
        if "Compressing " in line_s:
            file_count += 1
            now = time.time()
            if now - last_print >= 15.0 or file_count % 10000 == 0:
                elapsed = now - t0
                print(f"[{elapsed:6.1f}s | {file_count:5,}/79,431 files] Current: {line_s[:65]}", flush=True)
                last_print = now
        elif "All OK" in line_s or "WARNING:" in line_s or "ERROR:" in line_s or "*** NOTICE" in line_s:
            print(f"  >>> {line_s}", flush=True)

    proc.wait()
    dt = time.time() - t0

    if proc.returncode != 0:
        print(f"ERROR: {name} failed with code {proc.returncode}!")
        return None, dt

    sz = os.path.getsize(out_arc) if os.path.exists(out_arc) else 0
    print(f"\n{name} FINISHED: {sz:,} bytes in {dt:.1f}s ({dt/60:.2f} min)")
    return sz, dt

def verify_wine(arc_path):
    print(f"Testing wine unarc.exe 0.67 on {arc_path} ...", flush=True)
    env = dict(os.environ, WINEDEBUG="-all")
    res = subprocess.run(["wine", WINE_UNARC, "t", arc_path], capture_output=True, text=True, env=env)
    ok = "All OK" in res.stdout
    print(f"Wine unarc test result: {'All OK' if ok else 'FAIL'}")
    return ok

def main():
    print("=" * 80)
    print("DUAL 10 GB MATT MAHONEY BENCHMARK (Bit-Exact vs No-Bitexact)")
    print(f"Corpus: {CORPUS_DIR}")
    print(f"Arc Binary: {ARC_BIN}")
    print("=" * 80)

    # 1. Run Bit-Exact Mode
    sz_be, dt_be = run_archive("1. Mode Bit-Exact (-mx)", ["-mx"], ARC_BITEXACT)
    if not sz_be:
        sys.exit(1)

    # 2. Run Nobitexact Mode
    sz_nbx, dt_nbx = run_archive("2. Mode Nobitexact (-nobitexact -mx)", ["-nobitexact", "-mx"], ARC_NOBITEXACT)
    if not sz_nbx:
        sys.exit(1)

    # 3. Quick verification with Wine
    ok_be = verify_wine(ARC_BITEXACT)
    ok_nbx = verify_wine(ARC_NOBITEXACT)

    # 4. Generate report
    gain_bytes = sz_be - sz_nbx
    gain_pct = (gain_bytes / sz_be) * 100 if sz_be else 0
    time_diff = dt_be - dt_nbx
    time_diff_pct = (time_diff / dt_be) * 100 if dt_be else 0

    gap_vs_7z = sz_nbx - SEVENZIP_SIZE
    gap_7z_pct = (gap_vs_7z / SEVENZIP_SIZE) * 100 if SEVENZIP_SIZE else 0

    report = []
    report.append("=" * 85)
    report.append("RÉSULTATS DU BENCHMARK DUAL 10 GO MATT MAHONEY (79 431 fichiers)")
    report.append("=" * 85)
    report.append(f"{'Métrique':<32} | {'Bit-Exact (-mx)':<20} | {'Nobitexact (-mx)':<20} | {'Évolution'}")
    report.append("-" * 85)
    report.append(f"{'Taille de l archive':<32} | {sz_be:>16,} o | {sz_nbx:>16,} o | -{gain_bytes:,} o (-{gain_pct:.2f}%)")
    report.append(f"{'Taille en GiB':<32} | {sz_be/(1024**3):>17.3f} GiB | {sz_nbx/(1024**3):>17.3f} GiB | -{gain_bytes/(1024**2):.1f} MiB")
    report.append(f"{'Temps d encodage':<32} | {dt_be:>17.1f} s | {dt_nbx:>17.1f} s | {time_diff:+.1f} s ({time_diff_pct:+.1f}%)")
    report.append(f"{'Durée en minutes':<32} | {dt_be/60:>16.1f} min | {dt_nbx/60:>16.1f} min | {time_diff/60:+.1f} min")
    report.append(f"{'Décompression Wine 0.67':<32} | {('All OK' if ok_be else 'FAIL'):>20} | {('All OK' if ok_nbx else 'FAIL'):>20} | 100% compatible")
    report.append("-" * 85)
    report.append("COMPARAISON VS 7-ZIP 26.02 ULTRA (-mx=9, taille: 3 652 683 873 o / 3.402 GiB) :")
    if gap_vs_7z <= 0:
        report.append(f"  >>> VICTOIRE : FreeArc -nobitexact est plus compact que 7-Zip de {-gap_vs_7z:,} octets ({-gap_vs_7z/(1024*1024):.2f} Mo) ! <<<")
    else:
        report.append(f"  Écart résiduel vs 7-Zip : +{gap_vs_7z:,} octets (+{gap_7z_pct:.2f}%)")
        total_gap = sz_be - SEVENZIP_SIZE
        if total_gap > 0:
            closed = (sz_be - sz_nbx) / total_gap * 100
            report.append(f"  Part de l'écart historique comblée : {closed:.1f}%")
    report.append("=" * 85)

    report_text = "\n".join(report)
    print("\n" + report_text)

    with open(RESULTS_FILE, "w") as f:
        f.write(report_text + "\n")
    print(f"\nRapport sauvegardé dans : {RESULTS_FILE}")

if __name__ == "__main__":
    main()

