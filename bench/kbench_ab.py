#!/usr/bin/env python3
"""A/B timing of two kbench binaries (see kbench.cpp) on the benchmark corpus.

usage: kbench_ab.py BASE_KBENCH CAND_KBENCH METHOD[,METHOD...] [-k ROUNDS] [-r REPS] [-i INPUT_SUBDIRS]

Each round runs base and candidate (alternating order); each run compresses the whole
in-memory corpus REPS times and reports its fastest compression. The compressed stream
hash must be identical (bit-exact). Verdict as in enc_bench.py: median paired gain > 1 %
and one-sided sign test p < 0.05.
"""
import argparse, os, subprocess, statistics, sys
from math import comb

HERE = os.path.dirname(os.path.abspath(__file__))
CORPUS = os.path.join(HERE, "work", "corpus")

def files(subdirs):
    out = []
    for s in subdirs.split():
        for dp, _, fs in os.walk(os.path.join(CORPUS, s)):
            out += [os.path.join(dp, f) for f in fs]
    return sorted(out)

def run(exe, method, reps, fl):
    r = subprocess.run([exe, method, str(reps)] + fl, capture_output=True, text=True)
    if r.returncode != 0:
        raise SystemExit(f"{exe} {method} failed: {r.stderr}")
    kv = dict(x.split("=", 1) for x in r.stdout.split()[1:])
    return kv["hash"], int(kv["out"]), float(kv["wall_min"])

def sign_p(w, n):
    return sum(comb(n, k) for k in range(w, n + 1)) / 2 ** n

ap = argparse.ArgumentParser()
ap.add_argument("base"); ap.add_argument("cand"); ap.add_argument("methods")
ap.add_argument("-k", type=int, default=8); ap.add_argument("-r", type=int, default=3)
ap.add_argument("-i", default=".")
a = ap.parse_args()
fl = files(a.i)
print(f"{'method':<28}{'base':>9}{'cand':>9}{'med gain':>10}{'wins':>7}  verdict   size")
for m in a.methods.split(","):
    tb, tc = [], []
    hb = None
    for i in range(a.k):
        order = (0, 1) if i % 2 == 0 else (1, 0)
        for who in order:
            h, n, t = run(a.base if who == 0 else a.cand, m, a.r, fl)
            if hb is None: hb, nb = h, n
            if h != hb:
                print(f"BIT-EXACT FAILURE: {m}"); sys.exit(2)
            (tb if who == 0 else tc).append(t)
    ratios = [c / b for b, c in zip(tb, tc)]
    g = (1 - statistics.median(ratios)) * 100
    w = sum(r < 1 for r in ratios); l = sum(r > 1 for r in ratios)
    v = "GAIN" if g > 1 and sign_p(w, a.k) < 0.05 else "LOSS" if g < -1 and sign_p(l, a.k) < 0.05 else "noise"
    print(f"{m:<28}{min(tb):9.4f}{min(tc):9.4f}{g:9.1f}%{w:4d}/{a.k:<2d}  {v:<8}{nb}", flush=True)
