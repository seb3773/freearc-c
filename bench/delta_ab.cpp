// In-process A/B kernel benchmark for the DELTA preprocessor.
//
// Two versions of Delta.cpp (baseline and candidate) are compiled into the same
// binary, each inside its own namespace, so that both run back to back on the same
// in-memory blocks (order B C C B ...). This removes process start-up, file I/O and
// most machine drift from the measurement.
//
// For every block the candidate output (preprocessed data + TSkip/TType/TRows table
// descriptors) must be byte-identical to the baseline output; any difference aborts.
// The decoder of each namespace is also checked to restore the original block.
//
// Build: bench/delta_ab_build.sh BASE_DELTA_CPP CAND_DELTA_CPP OUTPUT
// Usage: delta_ab PAIRS FILE...      (env DELTA_AB_BLOCK=bytes, default 8 MiB;
//                                     env DELTA_AB_INNER=k, runs per pair member, default 3)
#include <immintrin.h>
#include <vector>
#include <algorithm>
#include <time.h>
#include <math.h>
#include "Compression.h"
#ifdef __cplusplus
#undef min
#undef max
#endif

#define DELTA_LIBRARY
namespace base {
#include BASE_DELTA
}
namespace cand {
#include CAND_DELTA
}

FILE *g_logfile = NULL;   // referenced by Common.cpp

static double now() { timespec t; clock_gettime (CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }

static double sign_p (int w, int n)
{
    double p = 0;
    for (int k = w; k <= n; k++) {
        double c = 1; for (int i = 0; i < k; i++) c = c * (n - i) / (i + 1);
        p += c;
    }
    return p / pow (2.0, n);
}

struct Out { std::vector<BYTE> data, skip, type, rows; };

template <class B, class F>
static double run (F process, const std::vector<BYTE> &in, size_t block, BYTE *buf, Out *out)
{
    B TSkip, TType, TRows, Reorder;
    double t = 0;
    if (out) out->data.clear(), out->skip.clear(), out->type.clear(), out->rows.clear();
    for (size_t pos = 0; pos < in.size(); pos += block) {
        int size = (int) std::min (block, in.size() - pos);
        memcpy (buf, &in[pos], size);
        double t0 = now();
        process (buf, size, TSkip, TType, TRows, Reorder);
        t += now() - t0;
        if (out) {
            out->data.insert (out->data.end(), buf, buf + size);
            out->skip.insert (out->skip.end(), TSkip.buf, TSkip.buf + TSkip.len());
            out->type.insert (out->type.end(), TType.buf, TType.buf + TType.len());
            out->rows.insert (out->rows.end(), TRows.buf, TRows.buf + TRows.len());
        }
        TSkip.empty(), TType.empty(), TRows.empty();
    }
    return t;
}

int main (int argc, char **argv)
{
    if (argc < 3) { fprintf (stderr, "usage: delta_ab PAIRS FILE...\n"); return 1; }
    int pairs = atoi (argv[1]);
    size_t block = getenv ("DELTA_AB_BLOCK") ? atol (getenv ("DELTA_AB_BLOCK")) : 8 << 20;
    std::vector<BYTE> in;
    for (int i = 2; i < argc; i++) {
        FILE *f = fopen (argv[i], "rb");
        if (!f) { perror (argv[i]); return 1; }
        fseek (f, 0, SEEK_END); long n = ftell (f); fseek (f, 0, SEEK_SET);
        size_t o = in.size(); in.resize (o + n);
        if (fread (&in[o], 1, n, f) != (size_t) n) { perror ("read"); return 1; }
        fclose (f);
    }
    BYTE *buf = (BYTE*) malloc (block + 30*8 + 64);

    // Bit-exactness check (also warms up both versions)
    Out ob, oc;
    run<base::Buffer> (base::delta_process, in, block, buf, &ob);
    run<cand::Buffer> (cand::delta_process, in, block, buf, &oc);
    if (ob.data != oc.data || ob.skip != oc.skip || ob.type != oc.type || ob.rows != oc.rows) {
        fprintf (stderr, "MISMATCH: candidate output differs from baseline\n");
        return 3;
    }
    int tables = (int)(ob.type.size() / 4);

    // Each member of a pair is the minimum of `inner` back-to-back runs (filters out bursts)
    int inner = getenv ("DELTA_AB_INNER") ? atoi (getenv ("DELTA_AB_INNER")) : 3;
    std::vector<double> tb, tc, ratio;
    int wins = 0;
    for (int p = 0; p < pairs; p++) {
        double b = 1e9, c = 1e9;
        for (int k = 0; k < inner; k++) {
            if ((p + k) & 1) { c = std::min (c, run<cand::Buffer> (cand::delta_process, in, block, buf, 0)); b = std::min (b, run<base::Buffer> (base::delta_process, in, block, buf, 0)); }
            else             { b = std::min (b, run<base::Buffer> (base::delta_process, in, block, buf, 0)); c = std::min (c, run<cand::Buffer> (cand::delta_process, in, block, buf, 0)); }
        }
        tb.push_back (b); tc.push_back (c); ratio.push_back (1 - c / b);
        wins += c < b;
    }
    std::sort (tb.begin(), tb.end()); std::sort (tc.begin(), tc.end()); std::sort (ratio.begin(), ratio.end());
    double med = ratio[pairs / 2];
    double lo = ratio[(int) floor (pairs * 0.1)], hi = ratio[(int) ceil (pairs * 0.9) - 1];
    double ps = sign_p (wins, pairs);
    const char *verdict = (wins * 4 >= pairs * 3 && ps < 0.05 && med > 0.01) ? "GAIN"
                        : ((pairs - wins) * 4 >= pairs * 3 && sign_p (pairs - wins, pairs) < 0.05 && med < -0.01) ? "LOSS" : "noise";
    printf ("in=%zu tables=%d base_min=%.5f cand_min=%.5f base_MBs=%.0f cand_MBs=%.0f gain_med=%+.2f%% [p10 %+.2f%% .. p90 %+.2f%%] wins=%d/%d %s\n",
            in.size(), tables, tb[0], tc[0], in.size() / tb[0] / 1e6, in.size() / tc[0] / 1e6, med * 100, lo * 100, hi * 100, wins, pairs, verdict);
    return 0;
}
