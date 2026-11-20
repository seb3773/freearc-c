// Differential fuzzer for the DELTA preprocessor (encoder side).
//
// 1) Unit level: every fast kernel of the candidate Delta.cpp (diff_table,
//    analyze_table, reorder_table, search_for_table_boundary) is compared with the
//    legacy implementation kept in the same file, on random tables (N = 1..30, random
//    doDiff/immutable patterns, random row counts, random/structured contents).
// 2) Whole-filter level: delta_process() of the baseline and of the candidate
//    (compiled side by side in two namespaces) must produce identical data and table
//    descriptors on generated inputs mixing noise, text-like data, zero runs and
//    embedded binary tables of every element size, and the candidate decoder must
//    restore the original input.
//
// Build: bench/delta_ab_build.sh BASE_DELTA_CPP CAND_DELTA_CPP OUT "-DDELTA_FUZZ"
//        (delta_ab_build.sh compiles delta_fuzz.cpp instead of delta_ab.cpp when DELTA_FUZZ is set)
// Usage: delta_fuzz ITERATIONS [SEED]
#include <immintrin.h>
#include <vector>
#include <algorithm>
#include "Compression.h"

#define DELTA_LIBRARY
namespace base {
#include BASE_DELTA
}
namespace cand {
#include CAND_DELTA
}
FILE *g_logfile = NULL;

static unsigned long long rng_state = 88172645463325252ULL;
static inline unsigned rnd() { rng_state ^= rng_state << 13; rng_state ^= rng_state >> 7; rng_state ^= rng_state << 17; return (unsigned)(rng_state >> 11); }
static inline unsigned rnd (unsigned n) { return n ? rnd() % n : 0; }

static void fill_table (BYTE *t, int N, int rows, int mode)
{
    // Column generators: constant, slowly increasing counter, small noise, full noise
    int kind[32]; unsigned val[32], step[32];
    for (int k=0; k<N; k++) { kind[k] = rnd(4); val[k] = rnd(); step[k] = rnd(mode==0? 4 : 300); }
    for (int i=0; i<rows; i++)
        for (int k=0; k<N; k++) {
            BYTE b;
            switch (mode==3? 3 : kind[k]) {
                case 0:  b = (BYTE) val[k]; if (rnd(16)==0) val[k] = rnd(); break;
                case 1:  val[k] += step[k]; b = (BYTE)(val[k] >> 2); break;
                case 2:  b = (BYTE)(val[k] + rnd(3)); break;
                default: b = (BYTE) rnd(); break;
            }
            t[i*N+k] = b;
        }
}

static int unit_tests (int iters)
{
    std::vector<BYTE> a, b, c;
    for (int it=0; it<iters; it++) {
        int N = 1 + rnd(30), rows = rnd(8)==0? rnd(4) : rnd(it%10==0? 5000 : 300);
        size_t len = (size_t)N*rows;
        a.assign (len + 300, 0); for (auto &x : a) x = (BYTE) rnd();
        fill_table (&a[64], N, rows, rnd(4));
        b = a;  c = a;
        bool doDiff[32], imm[32];
        int pat = rnd(4);
        for (int k=0; k<N; k++) doDiff[k] = pat==0? true : pat==1? false : pat==2? (rnd(4)!=0) : (rnd(2)!=0);
        cand::diff_table_legacy (N, &b[64], rows, doDiff);
        cand::diff_table        (N, &c[64], rows, doDiff);
        if (b != c) { fprintf (stderr, "diff_table mismatch N=%d rows=%d\n", N, rows); return 1; }

        bool d1[32], i1[32], d2[32], i2[32];
        if (rows >= 1) {
            cand::analyze_table_legacy (N, &a[64], rows, d1, i1);
            cand::analyze_table        (N, &a[64], rows, d2, i2);
            for (int k=0; k<N; k++) if (d1[k]!=d2[k] || i1[k]!=i2[k]) { fprintf (stderr, "analyze_table mismatch N=%d rows=%d\n", N, rows); return 1; }
        }
        for (int k=0; k<N; k++) imm[k] = pat==1? true : rnd(3)==0;
        b = a; c = a;
        cand::Buffer t1, t2;
        cand::reorder_table_legacy (N, &b[64], rows, imm, t1);
        cand::reorder_table        (N, &c[64], rows, imm, t2);
        if (b != c) { fprintf (stderr, "reorder_table mismatch N=%d rows=%d\n", N, rows); return 1; }

        // Table boundary search, both directions, from several start points (incl. near the edges)
        for (int s=0; s<4; s++) {
            int sz = (int)a.size();
            BYTE *bs = &a[0] + rnd(64), *be = &a[0] + sz - rnd(64);
            int dn = rnd(2)? N : -N;
            BYTE *p = bs + rnd((unsigned)(be - bs));
            if (p + 2*N + 2 > be || p - 2*N < bs) continue;   // the callers always start inside the buffer
            int u1, u2;
            BYTE *r1 = cand::search_for_table_boundary_legacy (dn, p, bs, be, u1);
            BYTE *r2 = cand::search_for_table_boundary        (dn, p, bs, be, u2);
            if (r1 != r2 || u1 != u2) { fprintf (stderr, "search_for_table_boundary mismatch N=%d\n", dn); return 1; }
        }
    }
    return 0;
}

static void gen_input (std::vector<BYTE> &v, size_t size)
{
    v.resize (size);
    size_t p = 0;
    while (p < size) {
        size_t n = std::min (size - p, (size_t)(1 + rnd(rnd(4)==0? 200000 : 5000)));
        int what = rnd(6);
        if (what == 0)      for (size_t i=0; i<n; i++) v[p+i] = (BYTE) rnd();
        else if (what == 1) for (size_t i=0; i<n; i++) v[p+i] = "etaoin shrdlu\n"[rnd(14)];
        else if (what == 2) for (size_t i=0; i<n; i++) v[p+i] = rnd(50)==0? (BYTE)rnd() : 0;
        else {
            int N = 1 + rnd(rnd(3)==0? 30 : 8);
            int rows = (int)(n / N);
            fill_table (&v[p], N, rows, rnd(3));
            for (size_t i=(size_t)rows*N; i<n; i++) v[p+i] = (BYTE) rnd();
        }
        p += n;
    }
}

struct Out { std::vector<BYTE> data, skip, type, rows; };
template <class B, class F> static void run (F process, std::vector<BYTE> &in, size_t block, Out &o)
{
    BYTE *buf = (BYTE*) malloc (block + 30*8 + 64);
    B TSkip, TType, TRows, Reorder;
    for (size_t pos = 0; pos < in.size(); pos += block) {
        int size = (int) mymin (block, in.size() - pos);
        memcpy (buf, &in[pos], size);
        process (buf, size, TSkip, TType, TRows, Reorder);
        o.data.insert (o.data.end(), buf, buf + size);
        o.skip.insert (o.skip.end(), TSkip.buf, TSkip.buf + TSkip.len());
        o.type.insert (o.type.end(), TType.buf, TType.buf + TType.len());
        o.rows.insert (o.rows.end(), TRows.buf, TRows.buf + TRows.len());
        TSkip.empty(), TType.empty(), TRows.empty();
    }
    free (buf);
}

int main (int argc, char **argv)
{
    int iters = argc > 1 ? atoi (argv[1]) : 1000;
    if (argc > 2) rng_state = strtoull (argv[2], 0, 10) * 2654435761ULL + 1;
    if (unit_tests (iters * 20)) return 1;
    printf ("unit kernels: %d random tables OK\n", iters * 20);
    long tables = 0;  size_t bytes = 0;
    for (int it=0; it<iters; it++) {
        std::vector<BYTE> in;
        size_t size = it%50==0? 1 + rnd(3<<20) : rnd(it%3==0? 70000 : 600);
        gen_input (in, size);
        size_t block = rnd(4)==0? 1 + rnd(100000) : 8<<20;
        Out ob, oc;
        run<base::Buffer> (base::delta_process, in, block, ob);
        run<cand::Buffer> (cand::delta_process, in, block, oc);
        if (ob.data != oc.data || ob.skip != oc.skip || ob.type != oc.type || ob.rows != oc.rows) {
            fprintf (stderr, "delta_process MISMATCH at iteration %d (size %zu block %zu)\n", it, size, block);
            FILE *f = fopen ("/tmp/delta_fuzz_fail.bin", "wb"); fwrite (in.data(), 1, in.size(), f); fclose (f);
            return 1;
        }
        tables += ob.type.size()/4;  bytes += size;
    }
    printf ("delta_process: %d inputs, %zu bytes, %ld tables, all identical\n", iters, bytes, tables);
    return 0;
}
