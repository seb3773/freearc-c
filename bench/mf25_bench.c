/* mf25_bench.c - standalone driver for the LZMA SDK 25.01 encoder used by -nobitexact
 * (src/Compression/LZMA2/sdk25, unity build through lzma_fastenc.c).
 *
 *   enc <file> <threads 1|2> <dictMB> <fb> <mc> <reps> [maxbytes]
 *       full LZMA encode (lzfe_encode_mem, lc3 lp0 pb2, normal/optimal parser):
 *       prints the FNV-1a hash of the compressed stream, its size and the best time.
 *       threads=1: single-thread match finder (GetMatchesSpec1/SkipMatchesSpec),
 *       threads=2: multithreaded match finder (hash thread + BT thread: GetMatchesSpecN_2).
 *   mf  <file> <dictMB> <fb> <mc> <reps> [maxbytes]
 *       single-thread BT match finder alone: hash of every match/skip + final son[]/hash[].
 *   env NHB=2|3|4 selects BT2/BT3/BT4 (default 4); dictMB may be fractional (0.0625 = 64 KB).
 *
 * bench/mf25_bench_build.sh builds it with the C kernels, the x86-64 assembly kernels
 * (System V) and the same assembly in Microsoft x64 convention (ms_abi); all hashes must be
 * identical (bench/mf25_fuzz.sh runs a parameter matrix). */
#include "lzma_fastenc.c"
#include <stdio.h>
#include <time.h>

static double now (void) { struct timespec t; clock_gettime (CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
static unsigned char *load (const char *fn, size_t *n, size_t maxb)
{
  FILE *f = fopen (fn, "rb"); if (!f) { perror (fn); exit (1); }
  fseek (f, 0, SEEK_END); size_t sz = ftell (f); fseek (f, 0, SEEK_SET);
  if (maxb && sz > maxb) sz = maxb;
  unsigned char *d = malloc (sz + 1); if (fread (d, 1, sz, f) != sz) exit (1); fclose (f); *n = sz; return d;
}
#ifdef FA_MF_ASM
extern unsigned char fa_cpu_avx2;
/* FA_NO_AVX2=1 forces the SSE2 path of LZEXT (results must not change) */
static void force_sse2 (void) { if (getenv ("FA_NO_AVX2")) fa_cpu_avx2 = 0; }
#else
static void force_sse2 (void) {}
#endif
static void mix (unsigned long long *h, unsigned long long v) { *h = (*h ^ v) * 0x100000001b3ULL; }

typedef struct { ISeqInStream vt; const Byte *d; size_t n, pos; } MemIn;
static SRes MemRead (ISeqInStreamPtr pp, void *buf, size_t *size)
{
  MemIn *m = (MemIn *)(void *)pp; size_t k = *size < m->n - m->pos ? *size : m->n - m->pos;
  if (k > 65536 + 777) k = 65536 + 777;
  memcpy (buf, m->d + m->pos, k); m->pos += k; *size = k; return SZ_OK;
}
static void *BA (ISzAllocPtr p, size_t s) { (void)p; return malloc (s); }
static void  BF (ISzAllocPtr p, void *a)  { (void)p; free (a); }
static const ISzAlloc g_A = { BA, BF };

int main (int argc, char **argv)
{
  if (argc < 3) { fprintf (stderr, "see source\n"); return 2; }
  if (!strcmp (argv[1], "enc"))
  {
    size_t n; unsigned char *src = load (argv[2], &n, argc > 8 ? (size_t)atoll (argv[8]) : 0);
    lzfe_props pr; pr.dictSize = (unsigned)(atof (argv[4]) * 1048576); pr.lc = 3; pr.lp = 0; pr.pb = 2; pr.algo = 1;
    pr.fb = atoi (argv[5]); pr.btMode = 1; pr.numHashBytes = getenv("NHB") ? atoi(getenv("NHB")) : 4; pr.mc = atoi (argv[6]); pr.numThreads = atoi (argv[3]);
    int reps = atoi (argv[7]); double best = 1e30; unsigned long long h = 0; size_t dl = 0;
    size_t cap = n + n / 2 + (1 << 20); unsigned char *dst = malloc (cap);
    for (int r = 0; r < reps; r++)
    {
      lzfe *e = lzfe_create (&pr, NULL, NULL); if (!e) { fprintf (stderr, "create\n"); return 1; }
      force_sse2 ();
      dl = cap; double t0 = now ();
      int res = lzfe_encode_mem (e, dst, &dl, src, n);
      double dt = now () - t0; if (res) { fprintf (stderr, "res %d\n", res); return 1; }
      lzfe_destroy (e); if (dt < best) best = dt;
      h = 1469598103934665603ULL; for (size_t i = 0; i < dl; i++) mix (&h, dst[i]);
    }
    printf ("enc t%s %zu -> %zu  hash %016llx  best %.3f s  %.2f MB/s\n", argv[3], n, dl, h, best, n / best / 1e6);
    return 0;
  }
  if (!strcmp (argv[1], "mf"))
  {
    size_t n; unsigned char *src = load (argv[2], &n, argc > 7 ? (size_t)atoll (argv[7]) : 0);
    UInt32 dict = (UInt32)(atof (argv[3]) * 1048576), fb = atoi (argv[4]), mc = atoi (argv[5]); int reps = atoi (argv[6]);
    double best = 1e30; unsigned long long h = 0, calls = 0;
    for (int r = 0; r < reps; r++)
    {
      CMatchFinder mf; IMatchFinder2 vt; MemIn in;
      MatchFinder_Construct (&mf);
      force_sse2 ();
      mf.cutValue = mc; mf.btMode = 1; mf.numHashBytes = getenv("NHB") ? atoi(getenv("NHB")) : 4; mf.bigHash = 1; mf.expectedDataSize = n;
      in.vt.Read = MemRead; in.d = src; in.n = n; in.pos = 0;
      MatchFinder_SET_STREAM (&mf, &in.vt);
      if (!MatchFinder_Create (&mf, dict, 4096, fb, LZMA_MATCH_LEN_MAX + 1, &g_A)) { fprintf (stderr, "create\n"); return 1; }
      MatchFinder_CreateVTable (&mf, &vt);
      UInt32 dist[2 * (LZMA_MATCH_LEN_MAX + 2)];
      h = 1469598103934665603ULL; calls = 0;
      double t0 = now ();
      vt.Init (&mf);
      for (;;)
      {
        UInt32 avail = vt.GetNumAvailableBytes (&mf);
        if (avail == 0) break;
        UInt32 k = (UInt32)(vt.GetMatches (&mf, dist) - dist); calls++;
        mix (&h, k); for (UInt32 i = 0; i < k; i++) mix (&h, dist[i]);
        if (k >= 2 && dist[k - 2] >= 6 && avail > 1)
        { UInt32 s = dist[k - 2] - 1; if (s > avail - 1) s = avail - 1; if (s) { vt.Skip (&mf, s); mix (&h, s); } }
      }
      double dt = now () - t0; if (dt < best) best = dt;
      for (size_t i = 0; i < (size_t)mf.hashMask + 1 + mf.fixedHashSize; i++) mix (&h, mf.hash[i]);
      for (size_t i = 0; i < (size_t)mf.cyclicBufferSize * 2; i++) mix (&h, mf.son[i]);
      MatchFinder_Free (&mf, &g_A);
    }
    printf ("mf %zu bytes  calls %llu  hash %016llx  best %.3f s  %.2f MB/s\n", n, calls, h, best, n / best / 1e6);
    return 0;
  }
  return 2;
}
