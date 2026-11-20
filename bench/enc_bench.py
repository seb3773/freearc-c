#!/usr/bin/env python3
"""Encoder benchmark + bit-exact verification harness.

The golden rule: the archive produced by a candidate `arc` must be byte-identical
to the archive produced by the reference (legacy-exact) `arc` for the same input
and the same command line.

Usage:
  enc_bench.py refs   BASE_ARC [-f FILTER]          # store reference SHA-256 of every case
  enc_bench.py verify CAND_ARC [-f FILTER]          # compare candidate archives to the stored refs
  enc_bench.py run    BASE_ARC CAND_ARC [-n RUNS] [-f FILTER] [--cpu N]
      Interleaved A/B timing (ABBA order). Every candidate archive is checked
      byte-for-byte against the baseline archive of the same run.
  enc_bench.py list                                 # print the case table

Timing metric: wall time for single-threaded (-mt1) cases, user+sys CPU time
(os.wait4 rusage) for multithreaded cases. Base and candidate runs are paired
back to back in alternating order; the verdict uses the median of per-pair
ratios plus a one-sided sign test (see verdict()), which is robust against the
slow drift and bursts typical of shared/virtualized machines. Optional --cpu N
pins single-threaded runs to one CPU.
"""
import argparse, hashlib, os, statistics, subprocess, sys, time, json, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
WORK = os.path.join(HERE, "work")
CORPUS = os.path.join(WORK, "corpus")
REFS = os.path.join(WORK, "enc_refs.json")
TMP = "/dev/shm" if os.path.isdir("/dev/shm") else tempfile.gettempdir()

# name, arc options, input subset (relative to corpus), multithreaded?
CASES = [
    # --- FreeArc profiles, single thread (deterministic method mapping) ---
    ("m1",      "-mt1 -m1",  ".", False),
    ("m1x",     "-mt1 -m1x", ".", False),
    ("m2",      "-mt1 -m2",  ".", False),
    ("m2x",     "-mt1 -m2x", ".", False),
    ("m3",      "-mt1 -m3",  ".", False),
    ("m3x",     "-mt1 -m3x", ".", False),
    ("m4",      "-mt1 -m4",  ".", False),
    ("m4x",     "-mt1 -m4x", ".", False),
    ("m5",      "-mt1 -m5",  ".", False),
    ("m5x",     "-mt1 -m5x", ".", False),
    # --- Tornado presets ---
    ("tor1",    "-mt1 -mtor:1", ".", False),
    ("tor2",    "-mt1 -mtor:2", ".", False),
    ("tor3",    "-mt1 -mtor:3", ".", False),
    ("tor4",    "-mt1 -mtor:4", ".", False),
    ("tor5",    "-mt1 -mtor:5", ".", False),
    ("tor6",    "-mt1 -mtor:6", ".", False),
    ("tor7",    "-mt1 -mtor:7", ".", False),
    ("tor8",    "-mt1 -mtor:8", ".", False),
    ("tor9",    "-mt1 -mtor:9", ".", False),
    ("torc3",   "-mt1 -mtor:8m:c3", ".", False),
    ("tor5c3",  "-mt1 -mtor:5:c3", ".", False),
    # --- LZMA variants (match finders, modes, literal contexts) ---
    ("lzma",       "-mt1 -mlzma", ".", False),
    ("lzbt4n32",   "-mt1 -mlzma:96m:normal:32:mc32", ".", False),
    ("lzbt4n16",   "-mt1 -mlzma:96m:normal:16:mc8", ".", False),
    ("lzbt4n273",  "-mt1 -mlzma:96m:normal:273:mc128", "text game", False),
    ("lzfast",     "-mt1 -mlzma:96m:fast:32:mc4", ".", False),
    ("lzbt2",      "-mt1 -mlzma:16m:normal:mfbt2", "text game", False),
    ("lzbt3",      "-mt1 -mlzma:16m:normal:mfbt3", "text game", False),
    ("lzhc4",      "-mt1 -mlzma:16m:fast:mfhc4", ".", False),
    ("lzlc8",      "-mt1 -mlzma:lc8", "text game", False),
    ("lzlp2",      "-mt1 -mlzma:lc0:lp2:pb0", "text game", False),
    # --- Other codecs / filters (safety net for bit-exactness) ---
    ("ppmd",    "-mt1 -mppmd", ".", False),
    ("ppmdtxt", "-mt1 -mppmd:12:192m", "text", False),
    ("grzip",   "-mt1 -mgrzip", ".", False),
    ("grzm4",   "-mt1 -mgrzip:m4:8m:32:h15", "text", False),
    ("tta",     "-mt1 -mtta", "media", False),
    ("bcj",     "-mt1 -m5 -bcj", "bin game", False),
    ("delta",   "-mt1 -m5 -delta", "media game", False),
    ("dict",    "-mt1 -m5 -dict", "text", False),
    ("lzp",     "-mt1 -m5 -lzp", "text game", False),
    ("rep",     "-mt1 -m5 -rep", ".", False),
    ("mm",      "-mt1 -m5 -mm", "media", False),
    ("store",   "-mt1 -m0", ".", False),
    # --- Filter-only chains: isolate the cost of the preprocessors themselves ---
    ("f_dict",    "-mt1 -mdict:p:64m:80%", "text", False),
    ("f_lzp",     "-mt1 -mlzp:80m:105:d1m:s32:h22:92%", "text", False),
    ("f_dictlzp", "-mt1 -mdict:p:64m:80%+lzp:80m:105:d1m:s32:h22:92%", "text", False),
    ("f_lzpbin",  "-mt1 -mlzp:64m:24:h20:92%", "bin game", False),
    ("f_exe",     "-mt1 -mexe", "bin game", False),
    # --- Multithreaded variants (4x4 / default thread count) ---
    ("mt_m1",   "-m1", ".", True),
    ("mt_m2",   "-m2", ".", True),
    ("mt_m3",   "-m3", ".", True),
    ("mt_m5",   "-m5", ".", True),
    ("x4tor",   "-m4x4:tor", ".", True),
    ("x4lzma",  "-m4x4:lzma", ".", True),
]

def sha(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()

def select(flt):
    if not flt:
        return CASES
    keys = flt.split(",")
    return [c for c in CASES if any(c[0] == k or (k.endswith("*") and c[0].startswith(k[:-1])) for k in keys)]

def compress(arc, case, cpu):
    name, opts, inp, mt = case
    out = os.path.join(TMP, f"encb_{os.getpid()}_{name}.arc")
    if os.path.exists(out):
        os.remove(out)
    cmd = [arc, "a", *opts.split(), out, *inp.split()]
    if cpu is not None and not mt:
        cmd = ["taskset", "-c", str(cpu)] + cmd
    t0 = time.perf_counter()
    p = subprocess.Popen(cmd, cwd=CORPUS, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    _, status, ru = os.wait4(p.pid, 0)
    wall = time.perf_counter() - t0
    p.stderr.close()
    rc = os.waitstatus_to_exitcode(status)
    if rc != 0 or not os.path.exists(out):
        raise SystemExit(f"compression failed: {' '.join(cmd)} (rc={rc})")
    h = sha(out)
    os.remove(out)
    return h, ru.ru_utime + ru.ru_stime, wall

def start(arc, case, cpus, tag):
    name, opts, inp, mt = case
    out = os.path.join(TMP, f"encb_{os.getpid()}_{tag}_{name}.arc")
    if os.path.exists(out):
        os.remove(out)
    cmd = [arc, "a", *opts.split(), out, *inp.split()]
    if cpus is not None:
        cmd = ["taskset", "-c", cpus] + cmd
    return out, cmd, time.perf_counter(), subprocess.Popen(cmd, cwd=CORPUS, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

def finish(job):
    out, cmd, t0, p = job
    _, status, ru = os.wait4(p.pid, 0)
    wall = time.perf_counter() - t0
    if os.waitstatus_to_exitcode(status) != 0 or not os.path.exists(out):
        raise SystemExit(f"compression failed: {' '.join(cmd)}")
    h = sha(out); os.remove(out)
    return h, ru.ru_utime + ru.ru_stime, wall

def compress_pair(base, cand, case, i):
    """Run base and candidate at the same time on two different CPUs (CPU assignment
    and start order swapped every pair), so that machine-wide noise affects both
    runs of a pair equally. End times are taken from whichever child exits first."""
    cpus = ("2", "9") if i % 2 == 0 else ("9", "2")
    if i % 4 < 2:
        jb = start(base, case, cpus[0], "b"); jc = start(cand, case, cpus[1], "c")
    else:
        jc = start(cand, case, cpus[1], "c"); jb = start(base, case, cpus[0], "b")
    jobs = {jb[3].pid: jb, jc[3].pid: jc}
    res = {}
    while jobs:
        pid, status, ru = os.wait4(-1, 0)
        if pid not in jobs:
            continue
        out, cmd, t0, p = jobs.pop(pid)
        wall = time.perf_counter() - t0
        p.returncode = os.waitstatus_to_exitcode(status)
        if p.returncode != 0 or not os.path.exists(out):
            raise SystemExit(f"compression failed: {' '.join(cmd)}")
        h = sha(out); os.remove(out)
        res[pid] = (h, ru.ru_utime + ru.ru_stime, wall)
    return res[jb[3].pid], res[jc[3].pid]

def check_corpus():
    if not os.path.isdir(CORPUS):
        raise SystemExit("missing corpus: run bench/make_corpus.sh first")

def cmd_refs(base, flt):
    check_corpus()
    refs = json.load(open(REFS)) if os.path.exists(REFS) else {}
    for c in select(flt):
        h, cpu, wall = compress(base, c, None)
        refs[c[0]] = {"opts": c[1], "input": c[2], "sha256": h}
        print(f"{c[0]:<11}{cpu:8.2f}s cpu {wall:8.2f}s wall  {h[:16]}")
        json.dump(refs, open(REFS, "w"), indent=1)
    return 0

def cmd_verify(cand, flt):
    check_corpus()
    refs = json.load(open(REFS))
    bad = 0
    for c in select(flt):
        r = refs.get(c[0])
        if not r or r["opts"] != c[1] or r["input"] != c[2]:
            print(f"SKIP {c[0]} (no reference, run 'refs' with the baseline binary)"); bad += 1; continue
        h, cpu, wall = compress(cand, c, None)
        ok = h == r["sha256"]
        print(f"{'OK  ' if ok else 'FAIL'} {c[0]:<11}{cpu:8.2f}s cpu  {wall:7.2f}s wall")
        bad += not ok
    print("ALL BIT-EXACT" if not bad else f"{bad} FAILURE(S)")
    return 1 if bad else 0

def sign_test_p(wins, n):
    """One-sided binomial sign test: P(X >= wins) for X ~ Bin(n, 1/2)."""
    from math import comb
    return sum(comb(n, k) for k in range(wins, n + 1)) / 2 ** n

def verdict(tb, tc):
    """Paired comparison. tb[i]/tc[i] were measured back to back (alternating order),
    so the per-pair ratio cancels slow machine drift. GAIN requires the median paired
    gain to exceed 1 % AND the candidate to win significantly more pairs than chance
    (sign test, p < 0.05). LOSS symmetrically. Otherwise: noise."""
    n = len(tb)
    ratios = [c / b for b, c in zip(tb, tc)]
    med_gain = (1 - statistics.median(ratios)) * 100
    min_gain = (min(tb) - min(tc)) / min(tb) * 100
    wins = sum(r < 1 for r in ratios)
    losses = sum(r > 1 for r in ratios)
    if med_gain > 1.0 and sign_test_p(wins, n) < 0.05:
        v = "GAIN"
    elif med_gain < -1.0 and sign_test_p(losses, n) < 0.05:
        v = "LOSS"
    else:
        v = "noise"
    return med_gain, min_gain, wins, v

def cmd_run(base, cand, n, flt, cpu, inner, concurrent):
    check_corpus()
    refs = json.load(open(REFS)) if os.path.exists(REFS) else {}
    print(f"{'case':<11}{'base':>8}{'cand':>8}{'med gain':>10}{'min gain':>10}{'wins':>7}{'cpu gain':>10}  verdict")
    tb_all = tc_all = 0.0
    for c in select(flt):
        mt = c[3]
        tb, tc, cb, cc = [], [], [], []
        hb = None
        for i in range(n):
            if concurrent and not mt:
                rb, rc = compress_pair(base, cand, c, i)
                hb = hb or rb[0]
                if rb[0] != hb:
                    raise SystemExit(f"baseline is not deterministic on {c[0]}")
                hc = rc[0]
                tb.append(rb[2]); cb.append(rb[1]); tc.append(rc[2]); cc.append(rc[1])
                if hc != hb:
                    print(f"BIT-EXACT FAILURE on {c[0]}: candidate archive differs from baseline"); sys.exit(2)
                continue
            order = ((0, base, tb, cb), (1, cand, tc, cc)) if i % 2 == 0 else ((1, cand, tc, cc), (0, base, tb, cb))
            for who, exe, wl, cl in order:
                # One sample = best of `inner` consecutive runs (filters short bursts of noise).
                runs = [compress(exe, c, cpu) for _ in range(inner)]
                h = runs[0][0]
                if any(r[0] != h for r in runs):
                    raise SystemExit(f"non-deterministic output on {c[0]}")
                t = min(r[1] for r in runs); w = min(r[2] for r in runs)
                if who == 0:
                    hb = hb or h
                    if h != hb:
                        raise SystemExit(f"baseline is not deterministic on {c[0]}")
                else:
                    hc = h
                # Single-threaded: wall time (CPU accounting granularity is too coarse).
                # Multithreaded: CPU time (total work), wall is reported in cpu column.
                wl.append(t if mt else w); cl.append(w if mt else t)
            if hc != hb:
                print(f"BIT-EXACT FAILURE on {c[0]}: candidate archive differs from baseline"); sys.exit(2)
        if c[0] in refs and refs[c[0]]["opts"] == c[1] and refs[c[0]]["sha256"] != hb:
            print(f"WARNING: baseline differs from stored reference on {c[0]}")
        med_gain, min_gain, wins, v = verdict(tb, tc)
        cgain = (min(cb) - min(cc)) / min(cb) * 100
        tb_all += statistics.median(tb); tc_all += statistics.median(tc)
        print(f"{c[0]:<11}{statistics.median(tb):8.3f}{statistics.median(tc):8.3f}{med_gain:9.1f}%{min_gain:9.1f}%{wins:4d}/{n:<2d}{cgain:9.1f}%  {v}", flush=True)
    print(f"{'TOTAL':<11}{tb_all:8.3f}{tc_all:8.3f}{(tb_all - tc_all) / tb_all * 100:9.1f}%")

if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("cmd")
    ap.add_argument("bins", nargs="*")
    ap.add_argument("-n", type=int, default=10)
    ap.add_argument("-f", default="")
    ap.add_argument("--inner", type=int, default=3)
    ap.add_argument("--concurrent", action="store_true")
    ap.add_argument("--cpu", type=int, default=-1)
    a = ap.parse_args()
    bins = [os.path.abspath(b) for b in a.bins]
    if a.cmd == "list":
        for c in CASES: print(f"{c[0]:<11}{c[1]:<40}{c[2]}")
    elif a.cmd == "refs": sys.exit(cmd_refs(bins[0], a.f))
    elif a.cmd == "verify": sys.exit(cmd_verify(bins[0], a.f))
    elif a.cmd == "run": cmd_run(bins[0], bins[1], a.n, a.f, None if a.cpu < 0 else a.cpu, a.inner, a.concurrent)
    else: ap.error("unknown command")
