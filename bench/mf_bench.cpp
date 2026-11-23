// mf_bench.cpp - standalone driver for the bit-exact LZMA match finder (C/LzFind.c).
//
// Build twice - once with the C kernels, once with the x86-64 assembly kernels
// (see bench/mf_bench_build.sh) - and compare:
//   * correctness: a 64-bit checksum of EVERY (len,dist) pair returned by GetMatches,
//     of every Skip, and of the final son[]/hash[] contents must be identical;
//   * speed: rdtsc-free wall time of the match finder alone (no parser/range coder),
//     min of N repetitions (robust against noise on a shared machine).
//
// usage: mf_bench <file> <mode bt2|bt3|bt4|hc4|ht4> <cut> <fb> <dictKB> [reps] [maxbytes]
#include "Compression/Compression.h"
#include "Compression/LZMA2/C/Types.h"
#define MF_HashChain  0
#define MF_BinaryTree 1
#define MF_HashTable  2
namespace LzmaEncoder {
#include "Compression/LZMA2/C/LzFind.c"
}
using namespace LzmaEncoder;
#include <chrono>
#include <vector>

static void *A (void *, size_t n) { return malloc(n); }
static void  F (void *, void *a)  { free(a); }
static ISzAlloc g_al = { A, F };

struct MemIn { ISeqInStream s; const Byte *d; size_t n, pos; };
static SRes rd (void *p, void *buf, size_t *size)
{
  MemIn *m = (MemIn*)p;  size_t k = *size < m->n - m->pos ? *size : m->n - m->pos;
  if (k > 65536+777) k = 65536+777;          // irregular chunks: exercise MoveBlock/ReadBlock
  memcpy (buf, m->d + m->pos, k);  m->pos += k;  *size = k;  return SZ_OK;
}
static inline void mix (unsigned long long &h, unsigned long long v) { h = (h ^ v) * 0x100000001b3ULL; }

int main (int argc, char **argv)
{
  if (argc < 6) { fprintf(stderr, "usage: %s file bt2|bt3|bt4|hc4|ht4 cut fb dictKB [reps] [maxbytes]\n", argv[0]); return 2; }
  FILE *f = fopen (argv[1], "rb");  if (!f) return 1;
  std::vector<Byte> data;  { Byte b[1<<16]; size_t k; while ((k = fread(b,1,sizeof b,f)) > 0) data.insert(data.end(), b, b+k); }  fclose(f);
  if (argc > 7 && (size_t)atoll(argv[7]) < data.size())  data.resize (atoll(argv[7]));
  const char *mode = argv[2];  UInt32 cut = atoi(argv[3]), fb = atoi(argv[4]), dict = atoi(argv[5]) * 1024u;
  int reps = argc > 6 ? atoi(argv[6]) : 1;
  double best = 1e30;  unsigned long long H = 0;  unsigned long long calls = 0;
  for (int r = 0; r < reps; r++)
  {
    CMatchFinder mf;  MatchFinder_Construct (&mf);
    mf.cutValue = cut;  mf.bigHash = 0;
    if      (!strcmp(mode,"bt2")) { mf.btMode = MF_BinaryTree; mf.numHashBytes = 2; }
    else if (!strcmp(mode,"bt3")) { mf.btMode = MF_BinaryTree; mf.numHashBytes = 3; }
    else if (!strcmp(mode,"bt4")) { mf.btMode = MF_BinaryTree; mf.numHashBytes = 4; }
    else if (!strcmp(mode,"hc4")) { mf.btMode = MF_HashChain;  mf.numHashBytes = 4; }
    else                          { mf.btMode = MF_HashTable;  mf.numHashBytes = 4; }
    UInt32 hashBytes = mf.btMode == MF_HashTable ? 4u*1024*1024*4 : 4u*1024*1024;   // HT: 4M entries, BT/HC: 1M entries
    if (mf.numHashBytes == 2) hashBytes = 65536*4;
    if (!MatchFinder_Create (&mf, dict, hashBytes, 0, fb, 273, &g_al)) { fprintf(stderr,"create failed\n"); return 1; }
    MemIn in;  in.s.Read = rd;  in.d = data.data();  in.n = data.size();  in.pos = 0;
    mf.stream = &in.s;
    IMatchFinder vt;  MatchFinder_CreateVTable (&mf, &vt);
    UInt32 dist[2*(273+2)];
    unsigned long long h = 1469598103934665603ULL;  calls = 0;
    auto t0 = std::chrono::steady_clock::now();
    vt.Init (&mf);
    for (;;)
    {
      UInt32 avail = vt.GetNumAvailableBytes (&mf);
      if (avail == 0) break;
      UInt32 k = vt.GetMatches (&mf, dist);  calls++;
      mix (h, k);
      for (UInt32 i = 0; i < k; i++)  mix (h, dist[i]);
      if (k >= 2 && dist[k-2] >= 6 && avail > 1)
      {
        UInt32 s = dist[k-2] - 1;  if (s > avail - 1) s = avail - 1;
        if (s) { vt.Skip (&mf, s);  mix (h, s); }
      }
    }
    double dt = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    if (dt < best) best = dt;
    for (size_t i = 0; i < mf.hashSizeSum; i++)  mix (h, mf.hash[i]);
    for (size_t i = 0; i < mf.numSons; i++)      mix (h, mf.son[i]);
    MatchFinder_Free (&mf, &g_al);
    if (r && h != H) { fprintf(stderr, "non-deterministic!\n"); return 1; }
    H = h;
  }
  printf ("%-4s cut=%-4u fb=%-4u dict=%-7uK  calls=%-9llu  hash=%016llx  best=%.4f s\n", mode, cut, fb, dict/1024, calls, H, best);
  return 0;
}
