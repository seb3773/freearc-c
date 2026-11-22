#!/usr/bin/env python3
"""Whole-archive wall-time A/B of two `arc` binaries on a directory corpus.

Unlike enc_bench.py (single-thread, codec level), this measures the complete `arc a` command
with the default thread count: file-type detection, group scheduling, filters and codecs.

  bench/mt_bench.py BASE_ARC CAND_ARC CORPUS_DIR PAIRS PROFILES [extra arc options...]
  e.g. bench/mt_bench.py base/arc cand/arc bench/work/corpus 16 2,3
       bench/mt_bench.py base/arc cand/arc bench/work/corpus 6 3 -mt1

Runs are interleaved ABBA, so slow machine drifts hit both binaries alike. Both archives of
every pair are hashed: any byte difference aborts (exit 2). Verdict: GAIN/LOSS only if the
median paired gain exceeds 1% AND the one-sided sign test gives p < 0.05; otherwise noise.
"""
import sys, subprocess, time, statistics, math, os, hashlib

def p_sign(w, n):
    return sum(math.comb(n, k) for k in range(w, n + 1)) / 2 ** n

def main():
    if len(sys.argv) < 6:
        print(__doc__); sys.exit(1)
    base, cand = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2])
    corpus, n = sys.argv[3], int(sys.argv[4])
    modes, extra = sys.argv[5].split(","), sys.argv[6:]
    tmp = "/dev/shm" if os.path.isdir("/dev/shm") else "/tmp"
    out = os.path.join(tmp, f"mt_bench_{os.getpid()}.arc")

    def run(exe, m):
        if os.path.exists(out): os.remove(out)
        t0 = time.perf_counter()
        subprocess.run([exe, "a", f"-m{m}", *extra, out, "."], cwd=corpus,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True)
        t = time.perf_counter() - t0
        with open(out, "rb") as f: h = hashlib.sha256(f.read()).hexdigest()
        os.remove(out)
        return t, h

    for m in modes:
        tb, tc = [], []
        for i in range(n):
            order = [(0, base), (1, cand)] if i % 2 == 0 else [(1, cand), (0, base)]
            hs = {}
            for who, exe in order:
                t, h = run(exe, m); hs[who] = h
                (tb if who == 0 else tc).append(t)
            if hs[0] != hs[1]:
                print(f"-m{m} BIT-EXACT FAILURE: archives differ"); sys.exit(2)
        r = [c / b for b, c in zip(tb, tc)]
        w = sum(x < 1 for x in r)
        g = (1 - statistics.median(r)) * 100
        v = "GAIN" if g > 1 and p_sign(w, n) < 0.05 else ("LOSS" if g < -1 and p_sign(n - w, n) < 0.05 else "noise")
        print(f"-m{m:<4} {' '.join(extra):<5} base med {statistics.median(tb):7.3f}s min {min(tb):7.3f}  "
              f"cand med {statistics.median(tc):7.3f}s min {min(tc):7.3f}  median paired gain {g:6.2f}%  "
              f"wins {w}/{n} p={p_sign(w, n):.3f} {v}  (bit-exact)", flush=True)

if __name__ == "__main__":
    main()
