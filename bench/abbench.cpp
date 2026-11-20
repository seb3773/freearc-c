// Same-process A/B benchmark for the FreeArc encoders.
//
// Loads two builds of the codec library (bench/ab_build.sh: baseline and candidate)
// side by side and compresses the same in-memory buffer alternately with each of
// them (B C C B B C ...), so that both members of a pair run within a second of each
// other in the same process. This cancels most of the slow drift and bursts of a
// shared/virtualized machine, which dominate the noise of separate-process timings.
// Every compressed stream must be byte-identical between the two libraries.
//
// Usage: abbench BASE.so CAND.so METHOD PAIRS FILE...
// Output: one line with the median paired gain, sign-test wins and verdict.
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <vector>
#include <algorithm>

typedef int (*CompressMemFn) (char *method, void *input, int inputSize, void *output, int outputSize);

static double now() { timespec t; clock_gettime (CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }

static unsigned long long fnv (const char *p, int n)
{
    unsigned long long h = 1469598103934665603ULL;
    for (int i = 0; i < n; i++) h = (h ^ (unsigned char)p[i]) * 1099511628211ULL;
    return h;
}

// One-sided sign test: P(X >= w), X ~ Bin(n, 1/2)
static double sign_p (int w, int n)
{
    double p = 0;
    for (int k = w; k <= n; k++) {
        double c = 1; for (int i = 0; i < k; i++) c = c * (n - i) / (i + 1);
        p += c;
    }
    return p / pow (2.0, n);
}

int main (int argc, char **argv)
{
    if (argc < 6) { fprintf (stderr, "usage: abbench BASE.so CAND.so METHOD PAIRS FILE...\n"); return 1; }
    CompressMemFn fn[2];
    for (int i = 0; i < 2; i++) {
        void *h = dlopen (argv[1+i], RTLD_NOW | RTLD_LOCAL);
        if (!h) { fprintf (stderr, "%s\n", dlerror()); return 1; }
        fn[i] = (CompressMemFn) dlsym (h, "CompressMem");
        if (!fn[i]) { fprintf (stderr, "no CompressMem in %s\n", argv[1+i]); return 1; }
    }
    char *method = argv[3];
    int pairs = atoi (argv[4]);
    std::vector<char> in;
    for (int i = 5; i < argc; i++) {
        FILE *f = fopen (argv[i], "rb");
        if (!f) { perror (argv[i]); return 1; }
        fseek (f, 0, SEEK_END); long n = ftell (f); fseek (f, 0, SEEK_SET);
        size_t o = in.size(); in.resize (o + n);
        if (n && fread (&in[o], 1, n, f) != (size_t)n) { perror ("read"); return 1; }
        fclose (f);
    }
    int cap = (int)(in.size() + in.size() / 2 + (1 << 20));
    char *out = (char*) malloc (cap);
    char m[256];
    std::vector<double> t[2], ratio;
    unsigned long long ref = 0; int refsize = -1;
    // warm-up (page faults, allocator) for both libraries, also checks bit-exactness
    for (int r = -1; r < pairs; r++) {
        double tt[2];
        for (int k = 0; k < 2; k++) {
            int who = (r & 1) ? 1 - k : k;
            strcpy (m, method);
            double t0 = now();
            int n = fn[who] (m, &in[0], (int)in.size(), out, cap);
            tt[who] = now() - t0;
            if (n < 0) { fprintf (stderr, "compression error %d\n", n); return 2; }
            unsigned long long h = fnv (out, n);
            if (refsize < 0) { ref = h; refsize = n; }
            else if (h != ref || n != refsize) { printf ("%-30s BIT-EXACT FAILURE (%s)\n", method, who ? "candidate" : "baseline"); return 3; }
        }
        if (r < 0) continue;
        t[0].push_back (tt[0]); t[1].push_back (tt[1]); ratio.push_back (tt[1] / tt[0]);
    }
    std::vector<double> rs = ratio;  std::sort (rs.begin(), rs.end());
    double med = rs.size() % 2 ? rs[rs.size()/2] : (rs[rs.size()/2-1] + rs[rs.size()/2]) / 2;
    int wins = 0, losses = 0;
    for (double x : ratio) { wins += x < 1; losses += x > 1; }
    // Mean of per-pair log ratios with a 95% confidence interval (the speed of the machine
    // fluctuates randomly from sample to sample; the pair ratio removes the common part and
    // averaging many pairs shrinks the rest).
    double sum = 0, sum2 = 0;
    for (double x : ratio) { double l = log (x); sum += l; sum2 += l * l; }
    double mean = sum / pairs, sd = sqrt (fmax (0.0, (sum2 - sum * sum / pairs) / (pairs - 1)));
    double ci = 1.96 * sd / sqrt ((double)pairs);
    double gain = (1 - exp (mean)) * 100, glo = (1 - exp (mean + ci)) * 100, ghi = (1 - exp (mean - ci)) * 100;
    double medgain = (1 - med) * 100;
    // GAIN: the whole 95% interval is above +0.5% and the point estimate above 1%; LOSS symmetrically.
    const char *v = gain > 1 && glo > 0.5 ? "GAIN" : gain < -1 && ghi < -0.5 ? "LOSS" : "noise";
    std::sort (t[0].begin(), t[0].end()); std::sort (t[1].begin(), t[1].end());
    printf ("%-26s base %7.4f cand %7.4f  gain %6.2f%% [%6.2f%% ..%6.2f%%] med %6.2f%% wins %3d/%-3d %-5s size %d\n",
            method, t[0][0], t[1][0], gain, glo, ghi, medgain, wins, pairs, v, refsize);
    return 0;
}
