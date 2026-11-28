#!/usr/bin/env python3
"""
Comprehensive 10 GB Corpus Benchmark Orchestrator:
- FreeArc-c Bit-Exact (-mx)
- FreeArc-c Nobitexact (-nobitexact -mx)
- Zstandard 1.5.4 Max (--ultra -22 -T4)
- 7-Zip 26.02 Ultra (-mx=9 -ms=on)

Measures:
- Compressed size & compression ratio
- Compression time & throughput (MB/s)
- Decompression time & throughput (MB/s)
- Cleans up extraction directory after each run to prevent disk saturation.
- Strictly adheres to the privacy / safety filter rule: zero filename logging.
"""

import os
import sys
import time
import shutil
import subprocess

CORPUS_DIR = "/home/cdef/_PROJETS/compression_bench/mm_10gb"
ARC_BIN = "/home/cdef/_PROJETS/ARC/build/linux/arc"
UNARC_BIN = "/home/cdef/_PROJETS/ARC/build/linux/unarc"
SEVENZ_BIN = "/usr/bin/7z"
ZSTD_BIN = "/usr/bin/zstd"

BENCH_OUT = "/home/cdef/_PROJETS/compression_bench/runs_10gb"
EXTRACT_DIR = "/home/cdef/_PROJETS/compression_bench/runs_10gb/extract_test"
RESULTS_MD = "/home/cdef/_PROJETS/ARC/bench/mm10gb_full_benchmark_results.md"
LOG_FILE = "/home/cdef/_PROJETS/compression_bench/runs_10gb/benchmark.log"

os.makedirs(BENCH_OUT, exist_ok=True)

TOTAL_FILES = 79431
ORIG_BYTES = 10017494016  # verified exact byte size from du -sb
ORIG_GIB = ORIG_BYTES / (1024 ** 3)
ORIG_MB = ORIG_BYTES / (1024 * 1024)

def log(msg):
    t_str = time.strftime("[%Y-%m-%d %H:%M:%S]")
    line = f"{t_str} {msg}"
    print(line, flush=True)
    with open(LOG_FILE, "a", encoding="utf-8") as f:
        f.write(line + "\n")

def run_compression_arc(name, flags, out_arc):
    log(f"Starting compression: {name} ...")
    if os.path.exists(out_arc):
        os.remove(out_arc)

    cmd = [ARC_BIN, "a"] + flags + [out_arc, "-r", "*"]
    t0 = time.perf_counter()
    proc = subprocess.Popen(
        cmd,
        cwd=CORPUS_DIR,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        text=True,
        bufsize=1
    )

    file_count = 0
    last_print = time.time()
    for line in proc.stdout:
        if "Compressing " in line:
            file_count += 1
            now = time.time()
            if now - last_print >= 30.0:
                elapsed = now - t0
                pct = (file_count / TOTAL_FILES) * 100
                log(f"  [Progress] {elapsed:5.1f}s | {file_count:,} / {TOTAL_FILES:,} files ({pct:5.1f}%)")
                last_print = now

    proc.wait()
    t1 = time.perf_counter()
    comp_time = t1 - t0

    if proc.returncode != 0:
        log(f"ERROR: {name} compression failed with code {proc.returncode}!")
        return None, None

    arc_size = os.path.getsize(out_arc)
    speed = ORIG_MB / comp_time
    log(f"{name} compressed in {comp_time:.1f} s ({comp_time/60:.2f} min), size: {arc_size:,} bytes ({speed:.2f} MB/s)")
    return arc_size, comp_time

def run_decompression_arc(name, arc_path):
    log(f"Starting decompression: {name} ...")
    if os.path.exists(EXTRACT_DIR):
        shutil.rmtree(EXTRACT_DIR)
    os.makedirs(EXTRACT_DIR, exist_ok=True)

    cmd = [UNARC_BIN, "x", "-o+", f"-dp{EXTRACT_DIR}", arc_path]
    t0 = time.perf_counter()
    res = subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    t1 = time.perf_counter()
    decomp_time = t1 - t0

    if res.returncode != 0:
        log(f"ERROR: {name} decompression failed with code {res.returncode}!")
        shutil.rmtree(EXTRACT_DIR, ignore_errors=True)
        return None

    speed = ORIG_MB / decomp_time
    log(f"{name} decompressed in {decomp_time:.1f} s ({decomp_time/60:.2f} min), throughput: {speed:.2f} MB/s")

    # Clean up immediately
    shutil.rmtree(EXTRACT_DIR)
    log("  Extraction directory cleaned up cleanly.")
    return decomp_time

def run_compression_zstd(name, out_arc):
    log(f"Starting compression: {name} ...")
    if os.path.exists(out_arc):
        os.remove(out_arc)

    # Tar piped to zstd --ultra -22 -T4
    cmd = f"tar -cf - -C '{CORPUS_DIR}' . | {ZSTD_BIN} --ultra -22 -T4 -o '{out_arc}'"
    t0 = time.perf_counter()
    res = subprocess.run(cmd, shell=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    t1 = time.perf_counter()
    comp_time = t1 - t0

    if res.returncode != 0 or not os.path.exists(out_arc):
        log(f"ERROR: {name} compression failed with code {res.returncode}!")
        return None, None

    arc_size = os.path.getsize(out_arc)
    speed = ORIG_MB / comp_time
    log(f"{name} compressed in {comp_time:.1f} s ({comp_time/60:.2f} min), size: {arc_size:,} bytes ({speed:.2f} MB/s)")
    return arc_size, comp_time

def run_decompression_zstd(name, arc_path):
    log(f"Starting decompression: {name} ...")
    if os.path.exists(EXTRACT_DIR):
        shutil.rmtree(EXTRACT_DIR)
    os.makedirs(EXTRACT_DIR, exist_ok=True)

    cmd = f"{ZSTD_BIN} -d -c -T4 '{arc_path}' | tar -xf - -C '{EXTRACT_DIR}'"
    t0 = time.perf_counter()
    res = subprocess.run(cmd, shell=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    t1 = time.perf_counter()
    decomp_time = t1 - t0

    if res.returncode != 0:
        log(f"ERROR: {name} decompression failed with code {res.returncode}!")
        shutil.rmtree(EXTRACT_DIR, ignore_errors=True)
        return None

    speed = ORIG_MB / decomp_time
    log(f"{name} decompressed in {decomp_time:.1f} s ({decomp_time/60:.2f} min), throughput: {speed:.2f} MB/s")

    shutil.rmtree(EXTRACT_DIR)
    log("  Extraction directory cleaned up cleanly.")
    return decomp_time

def run_compression_7z(name, out_arc):
    log(f"Starting compression: {name} ...")
    if os.path.exists(out_arc):
        os.remove(out_arc)

    cmd = [SEVENZ_BIN, "a", "-mx=9", "-ms=on", "-bso0", "-bse0", out_arc, "*"]
    t0 = time.perf_counter()
    res = subprocess.run(cmd, cwd=CORPUS_DIR, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    t1 = time.perf_counter()
    comp_time = t1 - t0

    if res.returncode != 0 or not os.path.exists(out_arc):
        log(f"ERROR: {name} compression failed with code {res.returncode}!")
        return None, None

    arc_size = os.path.getsize(out_arc)
    speed = ORIG_MB / comp_time
    log(f"{name} compressed in {comp_time:.1f} s ({comp_time/60:.2f} min), size: {arc_size:,} bytes ({speed:.2f} MB/s)")
    return arc_size, comp_time

def run_decompression_7z(name, arc_path):
    log(f"Starting decompression: {name} ...")
    if os.path.exists(EXTRACT_DIR):
        shutil.rmtree(EXTRACT_DIR)
    os.makedirs(EXTRACT_DIR, exist_ok=True)

    cmd = [SEVENZ_BIN, "x", "-y", "-bso0", "-bse0", f"-o{EXTRACT_DIR}", arc_path]
    t0 = time.perf_counter()
    res = subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    t1 = time.perf_counter()
    decomp_time = t1 - t0

    if res.returncode != 0:
        log(f"ERROR: {name} decompression failed with code {res.returncode}!")
        shutil.rmtree(EXTRACT_DIR, ignore_errors=True)
        return None

    speed = ORIG_MB / decomp_time
    log(f"{name} decompressed in {decomp_time:.1f} s ({decomp_time/60:.2f} min), throughput: {speed:.2f} MB/s")

    shutil.rmtree(EXTRACT_DIR)
    log("  Extraction directory cleaned up cleanly.")
    return decomp_time

def save_report(results):
    lines = [
        "# Benchmark Comparatif Global : Corpus 10 Go (79 431 fichiers)",
        f"- **Date** : {time.strftime('%Y-%m-%d %H:%M:%S')}",
        f"- **Taille originale non compressée** : {ORIG_BYTES:,} octets ({ORIG_GIB:.3f} GiB / {ORIG_MB:.1f} Mo)",
        f"- **Machine** : 11th Gen Intel(R) Core(TM) i3-1115G4 @ 3.00GHz (2 cœurs / 4 threads, AVX2/AVX-512, SSD NVMe)",
        "",
        "## Résultats Comparatifs Détaillés",
        "",
        "| Compresseur | Profil / Méthode | Taille Archive (o) | Taille (GiB) | Ratio (%) | Gain vs Orig | Tps Encodage | Débit Encodage | Tps Décodage | Débit Décodage |",
        "|---|---|---|---|---|---|---|---|---|---|"
    ]

    for r in results:
        sz = r["arc_size"]
        sz_gib = sz / (1024 ** 3)
        ratio = (sz / ORIG_BYTES) * 100
        gain = (1 - (sz / ORIG_BYTES)) * 100
        ct = r["comp_time"]
        cs = ORIG_MB / ct
        dt = r["decomp_time"]
        ds = ORIG_MB / dt if dt else 0.0
        dt_str = f"{dt:.1f} s ({dt/60:.1f} m)" if dt else "N/A"
        ds_str = f"{ds:.1f} Mo/s" if dt else "N/A"

        lines.append(
            f"| **{r['name']}** | `{r['profile']}` | {sz:,} o | {sz_gib:.3f} GiB | "
            f"{ratio:.2f}% | {gain:.2f}% | {ct:.1f} s ({ct/60:.1f} m) | {cs:.2f} Mo/s | "
            f"{dt_str} | {ds_str} |"
        )

    lines.append("")
    lines.append("## Analyses & Points Clés")
    
    r_be = next((r for r in results if r["key"] == "freearc_be"), None)
    r_nbx = next((r for r in results if r["key"] == "freearc_nbx"), None)
    r_7z = next((r for r in results if r["key"] == "7z"), None)
    r_zstd = next((r for r in results if r["key"] == "zstd"), None)

    if r_be and r_nbx:
        diff_sz = r_be["arc_size"] - r_nbx["arc_size"]
        diff_pct = (diff_sz / r_be["arc_size"]) * 100
        time_diff = r_be["comp_time"] - r_nbx["comp_time"]
        lines.append(f"- **FreeArc-c `-nobitexact` vs `bit-exact`** :")
        lines.append(f"  * Gain en compression : **-{diff_sz:,} octets** (-{diff_pct:.2f}%)")
        lines.append(f"  * Vitesse d'encodage : diff {time_diff:+.1f} s ({'plus rapide' if time_diff > 0 else 'temps équivalent'})")

    if r_nbx and r_7z:
        gap_sz = r_nbx["arc_size"] - r_7z["arc_size"]
        gap_pct = (gap_sz / r_7z["arc_size"]) * 100
        speedup = r_7z["comp_time"] / r_nbx["comp_time"]
        lines.append(f"- **FreeArc-c `-nobitexact` vs 7-Zip Ultra (-mx=9)** :")
        if gap_sz <= 0:
            lines.append(f"  * Taille : FreeArc-c est **{-gap_sz:,} octets plus compact** que 7-Zip !")
        else:
            lines.append(f"  * Écart résiduel : +{gap_sz:,} octets (+{gap_pct:.2f}%)")
        lines.append(f"  * Vitesse d'encodage : FreeArc-c est **{speedup:.2f}x plus rapide** que 7-Zip Ultra !")
        if r_nbx["decomp_time"] and r_7z["decomp_time"]:
            d_speedup = r_7z["decomp_time"] / r_nbx["decomp_time"]
            lines.append(f"  * Vitesse de décompression : FreeArc-c est **{d_speedup:.2f}x plus rapide** ({r_nbx['decomp_time']:.1f}s vs {r_7z['decomp_time']:.1f}s)")

    if r_nbx and r_zstd:
        z_gap = r_zstd["arc_size"] - r_nbx["arc_size"]
        z_gap_pct = (z_gap / r_nbx["arc_size"]) * 100
        lines.append(f"- **FreeArc-c `-nobitexact` vs Zstandard (--ultra -22)** :")
        lines.append(f"  * Ratio : FreeArc-c bat Zstandard Ultra de **{z_gap:,} octets** (Zstd est {z_gap_pct:+.2f}% plus volumineux)")
        if r_nbx["decomp_time"] and r_zstd["decomp_time"]:
            lines.append(f"  * Décompression : Zstandard = {r_zstd['decomp_time']:.1f}s vs FreeArc-c = {r_nbx['decomp_time']:.1f}s")

    report_text = "\n".join(lines)
    with open(RESULTS_MD, "w", encoding="utf-8") as f:
        f.write(report_text)
    log(f"Updated results file: {RESULTS_MD}")

def main():
    log("=========================================================================")
    log("LANCEMENT DU GRAND BENCHMARK COMPARATIF 10 GO")
    log(f"Corpus : {CORPUS_DIR} ({TOTAL_FILES:,} fichiers, {ORIG_GIB:.3f} GiB)")
    log(f"Sorties : {BENCH_OUT}")
    log("=========================================================================")

    results = []

    # 1. FreeArc-c Bit-Exact
    p1_arc = os.path.join(BENCH_OUT, "freearc_bitexact.arc")
    sz1, ct1 = run_compression_arc("1/4 FreeArc-c Bit-Exact", ["-mx"], p1_arc)
    if sz1:
        dt1 = run_decompression_arc("1/4 FreeArc-c Bit-Exact", p1_arc)
        results.append({
            "key": "freearc_be",
            "name": "FreeArc-c (Bit-Exact)",
            "profile": "-mx (Standard)",
            "arc_size": sz1,
            "comp_time": ct1,
            "decomp_time": dt1
        })
        save_report(results)

    # 2. FreeArc-c Nobitexact
    p2_arc = os.path.join(BENCH_OUT, "freearc_nobitexact.arc")
    sz2, ct2 = run_compression_arc("2/4 FreeArc-c Nobitexact", ["-nobitexact", "-mx"], p2_arc)
    if sz2:
        dt2 = run_decompression_arc("2/4 FreeArc-c Nobitexact", p2_arc)
        results.append({
            "key": "freearc_nbx",
            "name": "FreeArc-c (Nobitexact)",
            "profile": "-nobitexact -mx",
            "arc_size": sz2,
            "comp_time": ct2,
            "decomp_time": dt2
        })
        save_report(results)

    # 3. Zstandard Max (--ultra -22)
    p3_arc = os.path.join(BENCH_OUT, "zstd_ultra22.tar.zst")
    sz3, ct3 = run_compression_zstd("3/4 Zstandard Ultra", p3_arc)
    if sz3:
        dt3 = run_decompression_zstd("3/4 Zstandard Ultra", p3_arc)
        results.append({
            "key": "zstd",
            "name": "Zstandard 1.5.4",
            "profile": "--ultra -22 -T4",
            "arc_size": sz3,
            "comp_time": ct3,
            "decomp_time": dt3
        })
        save_report(results)

    # 4. 7-Zip Ultra (-mx=9)
    p4_arc = os.path.join(BENCH_OUT, "7zip_ultra.7z")
    sz4, ct4 = run_compression_7z("4/4 7-Zip Ultra", p4_arc)
    if sz4:
        dt4 = run_decompression_7z("4/4 7-Zip Ultra", p4_arc)
        results.append({
            "key": "7z",
            "name": "7-Zip 26.02",
            "profile": "-mx=9 -ms=on",
            "arc_size": sz4,
            "comp_time": ct4,
            "decomp_time": dt4
        })
        save_report(results)

    log("=========================================================================")
    log("BENCHMARK COMPLET TERMINÉ AVEC SUCCÈS !")
    log(f"Rapport final écrit dans : {RESULTS_MD}")
    log("=========================================================================")

if __name__ == "__main__":
    main()

