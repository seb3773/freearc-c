// In-process codec kernel benchmark (no file I/O, no process startup, no archive layer).
// Compresses one in-memory buffer (concatenation of the given files) with a FreeArc
// method string R times and reports the min/median time of a single compression, the
// compressed size and a 64-bit FNV-1a hash of the compressed stream (for bit-exact A/B).
//
// Usage: kbench METHOD REPS FILE...
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <vector>
#include <algorithm>
#include "Compression.h"

#include <stdio.h>
FILE *g_logfile = NULL;   // referenced by the codec library (arc.cpp defines it in the real binary)
static double now (clockid_t c) { timespec t; clock_gettime (c, &t); return t.tv_sec + t.tv_nsec * 1e-9; }

int main (int argc, char **argv)
{
    if (argc < 4) { fprintf (stderr, "usage: kbench METHOD REPS FILE...\n"); return 1; }
    char *method = argv[1];
    int reps = atoi (argv[2]);
    std::vector<char> in;
    for (int i = 3; i < argc; i++) {
        FILE *f = fopen (argv[i], "rb");
        if (!f) { perror (argv[i]); return 1; }
        fseek (f, 0, SEEK_END); long n = ftell (f); fseek (f, 0, SEEK_SET);
        size_t o = in.size(); in.resize (o + n);
        if (fread (&in[o], 1, n, f) != (size_t)n) { perror ("read"); return 1; }
        fclose (f);
    }
    int outcap = (int)(in.size() + in.size() / 2 + (1 << 20));
    char *out = (char*) malloc (outcap);
    std::vector<double> wall, cpu;
    int outsize = 0;
    unsigned long long h = 0;
    for (int r = 0; r < reps; r++) {
        double w0 = now (CLOCK_MONOTONIC), c0 = now (CLOCK_PROCESS_CPUTIME_ID);
        int n = CompressMem (method, &in[0], (int)in.size(), out, outcap);
        double w1 = now (CLOCK_MONOTONIC), c1 = now (CLOCK_PROCESS_CPUTIME_ID);
        if (n < 0) { fprintf (stderr, "compression error %d\n", n); return 2; }
        unsigned long long hh = 1469598103934665603ULL;
        for (int i = 0; i < n; i++) hh = (hh ^ (unsigned char)out[i]) * 1099511628211ULL;
        if (r && (n != outsize || hh != h)) { fprintf (stderr, "non-deterministic output\n"); return 3; }
        outsize = n; h = hh;
        wall.push_back (w1 - w0); cpu.push_back (c1 - c0);
    }
    std::sort (wall.begin(), wall.end()); std::sort (cpu.begin(), cpu.end());
    printf ("%s in=%zu out=%d hash=%016llx wall_min=%.6f wall_med=%.6f cpu_min=%.6f cpu_med=%.6f\n",
            method, in.size(), outsize, h, wall[0], wall[wall.size()/2], cpu[0], cpu[cpu.size()/2]);
    return 0;
}
