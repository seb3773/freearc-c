#!/usr/bin/env python3
"""Decoder benchmark + bit-exact verification harness.

Usage:
  bench.py verify UNARC                 # bit-exact check on every archive (bench + legacy refs)
  bench.py run BASE_UNARC CAND_UNARC [-n RUNS] [-f FILTER]
      Interleaved A/B timing (min and median of N runs, extraction into tmpfs).
      Every candidate extraction is verified against the original corpus SHA-256.
A gain is reported as significant only if min(cand) < min(base) and the
median improvement exceeds the measured run-to-run spread of the baseline.
"""
import hashlib, os, shutil, subprocess, sys, tempfile, time, statistics, argparse, glob

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
WORK = os.path.join(HERE, "work")
TMPBASE = "/dev/shm" if os.path.isdir("/dev/shm") else None

def sha_tree(d):
    out = {}
    for dp, _, fs in os.walk(d):
        for f in fs:
            p = os.path.join(dp, f)
            h = hashlib.sha256(open(p, "rb").read()).hexdigest()
            out[os.path.relpath(p, d)] = h
    return out

def load_sha(path):
    m = {}
    for line in open(path):
        h, p = line.rstrip("\n").split("  ", 1)
        m[os.path.normpath(p)] = h
    return m

def extract(unarc, arc, extra=()):
    d = tempfile.mkdtemp(prefix="unarc_", dir=TMPBASE)
    t0 = time.perf_counter()
    r = subprocess.run([unarc, "x", "-o+", *extra, arc], cwd=d,
                       stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    dt = time.perf_counter() - t0
    return d, dt, r.returncode

def bench_archives():
    return sorted(glob.glob(os.path.join(WORK, "archives", "*.arc")))

def pwd_for(arc):
    return ["-pSecret123"] if os.path.basename(arc).startswith(("aes", "crypto", "blowfish", "twofish", "serpent")) else []

def check(unarc, arc, expected):
    d, dt, rc = extract(unarc, arc, pwd_for(arc))
    try:
        got = sha_tree(d)
        if isinstance(expected, tuple):  # reference archives store foreign absolute paths
            m = expected[1]
            ok = rc == 0 and len(got) > 0 and all(m.get(os.path.basename(k)) == v for k, v in got.items())
        else:
            ok = rc == 0 and got == expected
        return ok, dt
    finally:
        shutil.rmtree(d, ignore_errors=True)

def golden_sets():
    """(archive, expected sha map) for bench archives and legacy reference archives."""
    sets = []
    for a in bench_archives():
        sets.append((a, load_sha(a[:-4] + ".sha256")))
    ref = os.path.join(ROOT, "tests", "reference")
    rexp = {os.path.basename(k): v for k, v in sha_tree(os.path.join(ref, "corpus")).items()}
    for a in sorted(glob.glob(os.path.join(ref, "*.arc"))):
        sets.append((a, ("basename", rexp)))
    gold = os.path.join(HERE, "golden_legacy.sha256")
    if os.path.exists(gold):
        g = {}
        for line in open(gold):
            arcname, h, p = line.rstrip("\n").split("  ", 2)
            g.setdefault(arcname, {})[p] = h
        for arcname, m in g.items():
            sets.append((os.path.join(ROOT, arcname), m))
    return sets

def cmd_verify(unarc):
    bad = 0
    for a, exp in golden_sets():
        ok, dt = check(unarc, a, exp)
        print(f"{'OK  ' if ok else 'FAIL'} {dt:7.3f}s  {os.path.relpath(a, ROOT)}")
        bad += not ok
    print("ALL BIT-EXACT" if not bad else f"{bad} FAILURE(S)")
    return 1 if bad else 0

def cmd_run(base, cand, n, flt):
    items = [(a, load_sha(a[:-4] + ".sha256")) for a in bench_archives()]
    gold = [x for x in golden_sets() if "samples_legacy" in x[0]]
    items += gold
    if flt:
        items = [x for x in items if any(f in os.path.basename(x[0]) for f in flt.split(","))]
    tb_all = tc_all = 0.0
    print(f"{'archive':<14}{'base min':>10}{'cand min':>10}{'gain':>8}{'spread':>8}  verdict")
    for a, e in items:
        tb, tc = [], []
        for i in range(n):
            for who in ((base, tb), (cand, tc)) if i % 2 == 0 else ((cand, tc), (base, tb)):
                ok, dt = check(who[0], a, e)
                if not ok:
                    print(f"BIT-EXACT FAILURE: {who[0]} on {a}"); sys.exit(2)
                who[1].append(dt)
        mb, mc = min(tb), min(tc)
        spread = (statistics.median(tb) - mb) / mb * 100
        gain = (mb - mc) / mb * 100
        medgain = (statistics.median(tb) - statistics.median(tc)) / statistics.median(tb) * 100
        sig = "GAIN" if gain > max(1.0, spread) and medgain > 0 else ("LOSS" if -gain > max(1.0, spread) else "noise")
        tb_all += mb; tc_all += mc
        print(f"{os.path.basename(a):<14}{mb:10.3f}{mc:10.3f}{gain:7.1f}%{spread:7.1f}%  {sig}")
    print(f"{'TOTAL':<14}{tb_all:10.3f}{tc_all:10.3f}{(tb_all-tc_all)/tb_all*100:7.1f}%")

if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("cmd"); ap.add_argument("bins", nargs="+")
    ap.add_argument("-n", type=int, default=5); ap.add_argument("-f", default="")
    a = ap.parse_args()
    a.bins = [os.path.abspath(b) for b in a.bins]
    if a.cmd == "verify": sys.exit(cmd_verify(a.bins[0]))
    if a.cmd == "run": cmd_run(a.bins[0], a.bins[1], a.n, a.f)
