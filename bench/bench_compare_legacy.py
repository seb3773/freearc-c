#!/usr/bin/env python3
"""
Benchmark comparison: Legacy FreeArc 0.67 (Win32) vs Modern FreeArc (Win64 Wine) vs Modern FreeArc (Linux Native).
Tests both encoding (compression) and decoding (decompression) on a ~2.3 GB mixed corpus.
"""

import os
import sys
import time
import subprocess
import shutil
import hashlib

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CORPUS_DIR = "/tmp/bench_corpus_2_5gb"
WORK_DIR = "/tmp/bench_run"

CONFIGS = [
    {
        "id": "legacy_wine",
        "name": "Legacy FreeArc 0.67 (Win32 / Wine)",
        "arc": ["wine", os.path.join(ROOT_DIR, "research_bin/FreeArc/bin/Arc.exe")],
        "unarc": ["wine", os.path.join(ROOT_DIR, "research_bin/FreeArc/bin/unarc.exe")],
    },
    {
        "id": "win64_wine",
        "name": "Modern Port v1.3.4 (Win64 / Wine)",
        "arc": ["wine", os.path.join(ROOT_DIR, "build/win64/arc.exe")],
        "unarc": ["wine", os.path.join(ROOT_DIR, "build/win64/unarc.exe")],
    },
    {
        "id": "linux_native",
        "name": "Modern Port v1.3.4 (Linux x86_64 Native)",
        "arc": [os.path.join(ROOT_DIR, "build/linux/arc")],
        "unarc": [os.path.join(ROOT_DIR, "build/linux/unarc")],
    },
]

def get_corpus_files():
    files = []
    for root, dirs, fnames in os.walk(CORPUS_DIR):
        for f in fnames:
            files.append(os.path.join(root, f))
    files.sort()
    return files

def get_total_size(files):
    return sum(os.path.getsize(f) for f in files)

def run_bench_mode(mode, files, total_bytes):
    print(f"\n{'='*75}")
    print(f"  RUNNING BENCHMARK ON PROFILE: -{mode}  (Corpus: {total_bytes / (1024*1024):.2f} MB)")
    print(f"{'='*75}")

    env = os.environ.copy()
    env["WINEDEBUG"] = "-all"
    
    results = {}

    for cfg in CONFIGS:
        cid = cfg["id"]
        cname = cfg["name"]
        arc_file = os.path.join(WORK_DIR, f"{cid}_{mode}.arc")
        extract_dir = os.path.join(WORK_DIR, f"ext_{cid}_{mode}")
        if os.path.exists(arc_file):
            os.remove(arc_file)
        if os.path.exists(extract_dir):
            shutil.rmtree(extract_dir)

        print(f"\n>>> [{cname}]")

        # 1. ENCODING (Compression)
        print(f"  [*] Compressing with -{mode} ...", end="", flush=True)
        t0 = time.perf_counter()
        cmd = cfg["arc"] + ["a", f"-{mode}", "-y", arc_file] + files
        p_c = subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, env=env)
        comp_time = time.perf_counter() - t0
        
        if p_c.returncode != 0 or not os.path.exists(arc_file):
            print(f" FAILED (code {p_c.returncode})")
            continue
        
        arc_size = os.path.getsize(arc_file)
        comp_mb = total_bytes / (1024 * 1024)
        comp_speed = comp_mb / comp_time if comp_time > 0 else 0
        ratio = (arc_size / total_bytes) * 100
        print(f" Done in {comp_time:.2f}s ({comp_speed:.1f} MB/s) | Archive: {arc_size / (1024*1024):.2f} MB ({ratio:.1f}%)")

        # 2. DECODING (Integrity Test: unarc t)
        print(f"  [*] Testing archive (unarc t) ...", end="", flush=True)
        t0 = time.perf_counter()
        cmd_t = cfg["unarc"] + ["t", arc_file]
        p_t = subprocess.run(cmd_t, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, env=env)
        test_time = time.perf_counter() - t0
        test_speed = comp_mb / test_time if test_time > 0 else 0
        test_ok = (p_t.returncode == 0)
        print(f" Done in {test_time:.2f}s ({test_speed:.1f} MB/s) | Status: {'OK' if test_ok else 'FAIL'}")

        # 3. DECODING (Extraction to disk: unarc x)
        print(f"  [*] Extracting archive (unarc x) ...", end="", flush=True)
        os.makedirs(extract_dir, exist_ok=True)
        t0 = time.perf_counter()
        cmd_x = cfg["unarc"] + ["x", "-o+", f"-dp{extract_dir}", arc_file]
        p_x = subprocess.run(cmd_x, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, env=env)
        extract_time = time.perf_counter() - t0
        extract_speed = comp_mb / extract_time if extract_time > 0 else 0
        extract_ok = (p_x.returncode == 0)
        print(f" Done in {extract_time:.2f}s ({extract_speed:.1f} MB/s) | Status: {'OK' if extract_ok else 'FAIL'}")

        # Clean extracted files immediately to save RAM disk space
        shutil.rmtree(extract_dir)

        results[cid] = {
            "name": cname,
            "comp_time": comp_time,
            "comp_speed": comp_speed,
            "arc_size": arc_size,
            "ratio": ratio,
            "test_time": test_time,
            "test_speed": test_speed,
            "extract_time": extract_time,
            "extract_speed": extract_speed,
        }

    return results

def print_summary(all_results, total_bytes):
    print("\n" + "="*95)
    print("                     BENCHMARK SUMMARY RESULTS (~2.3 GB CORPUS)")
    print("="*95)

    for mode, res in all_results.items():
        print(f"\n### Compression Profile: -{mode}\n")
        print(f"Corpus uncompressed size: {total_bytes / (1024*1024):.2f} MB ({total_bytes / (1024*1024*1024):.2f} GB)\n")

        print(f"| Engine / Configuration | Compress Time | Comp. Speed | Archive Size | Ratio | Decomp. Test | Decomp. Speed | Speedup (vs Legacy) |")
        print(f"|------------------------|--------------:|------------:|-------------:|------:|-------------:|--------------:|:-------------------:|")

        legacy = res.get("legacy_wine")
        for cid, data in res.items():
            comp_t = f"{data['comp_time']:.2f} s"
            comp_s = f"{data['comp_speed']:.1f} MB/s"
            arc_sz = f"{data['arc_size'] / (1024*1024):.1f} MB"
            ratio_s = f"{data['ratio']:.1f} %"
            dec_t = f"{data['test_time']:.2f} s"
            dec_s = f"{data['test_speed']:.1f} MB/s"

            if legacy and legacy["comp_time"] > 0 and legacy["test_time"] > 0:
                c_gain = legacy["comp_time"] / data["comp_time"]
                d_gain = legacy["test_time"] / data["test_time"]
                gain_str = f"**{c_gain:.2f}x** enc / **{d_gain:.2f}x** dec"
            else:
                gain_str = "baseline"

            print(f"| {data['name']} | {comp_t} | {comp_s} | {arc_sz} | {ratio_s} | {dec_t} | {dec_s} | {gain_str} |")
    print("\n" + "="*95)

def main():
    if not os.path.exists(CORPUS_DIR):
        print(f"Corpus directory {CORPUS_DIR} not found. Please create it first.")
        sys.exit(1)

    os.makedirs(WORK_DIR, exist_ok=True)
    files = get_corpus_files()
    total_bytes = get_total_size(files)
    print(f"Found {len(files)} files totaling {total_bytes / (1024*1024):.2f} MB ({total_bytes / (1024*1024*1024):.2f} GB)")

    modes = sys.argv[1:] if len(sys.argv) > 1 else ["m1", "m2"]
    
    all_results = {}
    for mode in modes:
        clean_mode = mode.lstrip("-")
        all_results[clean_mode] = run_bench_mode(clean_mode, files, total_bytes)

    print_summary(all_results, total_bytes)

    # Cleanup temp archives
    shutil.rmtree(WORK_DIR)
    print(f"\nTemporary benchmark archives cleaned up from {WORK_DIR}.")

if __name__ == "__main__":
    main()

