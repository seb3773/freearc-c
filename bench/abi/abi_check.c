/* abi_check.c - checks the x86-64 match finder kernels under BOTH calling conventions:
 *   1. callee-saved registers (rbx rbp r12-r15, + rdi rsi for Microsoft x64) and rsp are preserved;
 *   2. the Tornado kernels return exactly what the C reference code returns (random fuzz).
 * Build: see bench/abi/run.sh (the kernels are assembled twice: SysV and -DFA_TEST_MSABI,
 * the latter with symbols renamed by objcopy --prefix-symbols=ms_). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
typedef uint64_t u64; typedef unsigned char BYTE; typedef unsigned UINT;
u64 abi_call4 (void *fn, u64 a1, u64 a2, u64 a3, u64 a4, u64 out[10], int ms);
extern unsigned char fa_cpu_avx2, ms_fa_cpu_avx2;
void fa_x86_64_cpu_init (void);  void ms_fa_x86_64_cpu_init (void);
UINT tor_find_match_len (const BYTE*, const BYTE*, const void*, UINT);  UINT ms_tor_find_match_len (const BYTE*, const BYTE*, const void*, UINT);
UINT tor_ext4 (const BYTE*, const BYTE*, const void*, UINT);            UINT ms_tor_ext4 (const BYTE*, const BYTE*, const void*, UINT);
void *lzf_bt_getmatches (void*), *ms_lzf_bt_getmatches (void*);
void  lzf_bt_skip (void*), ms_lzf_bt_skip (void*);
void *lzf_ht_getmatches (void*), *ms_lzf_ht_getmatches (void*);

static UINT ref_fml (const BYTE *p, const BYTE *q, const void *bufend, UINT len)
{ const BYTE *end = bufend; while (p + len + 8 <= end) { u64 a, b; memcpy(&a,p+len,8); memcpy(&b,q+len,8); if (a^b) return len + (__builtin_ctzll(a^b)>>3); len += 8; }
  while (p + len < end && p[len] == q[len]) len++;  return len; }
static UINT ref_ext4 (const BYTE *p, const BYTE *q, const void *bufend, UINT len)
{ for (; (const void*)(p+len)<bufend && !memcmp(p+len, q+len, 4); len+=4);
  for (; (const void*)(p+len)<bufend && p[len] == q[len]; len++);  return len; }

static int fails = 0;
static void check_regs (const char *what, const u64 *o, int ms)
{
  static const u64 can[6] = {0x1111111111111111ULL,0x2222222222222222ULL,0x3333333333333333ULL,0x4444444444444444ULL,0x5555555555555555ULL,0x6666666666666666ULL};
  for (int i = 0; i < 6; i++) if (o[1+i] != can[i]) { printf("FAIL %s (%s): callee-saved reg #%d clobbered\n", what, ms?"ms":"sysv", i); fails++; }
  if (ms && (o[7] != 0x7777777777777777ULL || o[8] != 0x8888888888888888ULL)) { printf("FAIL %s (ms): rdi/rsi clobbered\n", what); fails++; }
  if (o[9] != 0) { printf("FAIL %s (%s): rsp changed\n", what, ms?"ms":"sysv"); fails++; }
}
typedef struct { const BYTE *cur; uint32_t *son; uint32_t *dist; uint32_t lenLimit, curMatch, pos, cbp, cbsize, cut, maxLen; } BtArgs;
typedef struct { uint32_t *row; const BYTE *cur; uint32_t *dist; uint32_t n, tag, tagMask, newEntry, cbsize, lenLimit, maxLen; } HtArgs;

int main (void)
{
  fa_x86_64_cpu_init();  ms_fa_x86_64_cpu_init();
  printf("AVX2 detected: %d\n", fa_cpu_avx2);
  enum { N = 1 << 16 };
  BYTE *buf = malloc (N + 64);
  u64 o[10];  srand (12345);
  for (int avx = 0; avx <= fa_cpu_avx2; avx++)
  {
    fa_cpu_avx2 = ms_fa_cpu_avx2 = (unsigned char)avx;     /* test both the SSE2 and the AVX2 paths */
    for (int it = 0; it < 200000; it++)
    {
      /* buffers with long common runs and a random mismatch */
      int len = rand() % 600, d = 1 + rand() % 2000, start = 2100 + rand() % 1000;
      for (int i = 0; i < 4000; i++) buf[i] = (BYTE)(rand() & 3);
      memcpy (buf + start, buf + start - d, len + (rand() % 2 ? 0 : 400));
      const BYTE *p = buf + start, *q = buf + start - d;
      const BYTE *end = p + rand() % 700;                   /* also p+len >= end cases */
      UINT l0 = rand() % 12;
      UINT r1 = ref_fml (p, q, end, l0), r2 = ref_ext4 (p, q, end, l0);
      for (int ms = 0; ms <= 1; ms++) {
        UINT a1 = (UINT) abi_call4 (ms ? (void*)ms_tor_find_match_len : (void*)tor_find_match_len, (u64)p, (u64)q, (u64)end, l0, o, ms);
        if (it < 50) check_regs ("tor_find_match_len", o, ms);
        UINT a2 = (UINT) abi_call4 (ms ? (void*)ms_tor_ext4 : (void*)tor_ext4, (u64)p, (u64)q, (u64)end, l0, o, ms);
        if (it < 50) check_regs ("tor_ext4", o, ms);
        if (a1 != r1 || a2 != r2) { if (fails++ < 10) printf("FAIL tor kernels avx=%d ms=%d it=%d: fml %u/%u ext4 %u/%u\n", avx, ms, it, a1, r1, a2, r2); }
      }
    }
  }
  /* LZMA kernels: register preservation on short real calls (functional equivalence of
     both ABI builds is covered by bench/mf_bench + fuzz) */
  {
    static uint32_t son[2*1024], row[64], dist[600];
    for (int i = 0; i < 4096; i++) buf[i] = (BYTE)(i % 7);
    for (int ms = 0; ms <= 1; ms++) {
      BtArgs b = { buf + 3000, son, dist, 32, 2990, 3000, 500, 1024, 8, 2 };
      memset (son, 0, sizeof son);
      abi_call4 (ms ? (void*)ms_lzf_bt_getmatches : (void*)lzf_bt_getmatches, (u64)&b, 0, 0, 0, o, ms);  check_regs ("lzf_bt_getmatches", o, ms);
      abi_call4 (ms ? (void*)ms_lzf_bt_skip : (void*)lzf_bt_skip, (u64)&b, 0, 0, 0, o, ms);              check_regs ("lzf_bt_skip", o, ms);
      for (int i = 0; i < 64; i++) row[i] = 2900 + i;
      HtArgs h = { row, buf + 3000, dist, 40, 0, 0xFC000000u, 3000, 4096, 32, 3 };
      abi_call4 (ms ? (void*)ms_lzf_ht_getmatches : (void*)lzf_ht_getmatches, (u64)&h, 0, 0, 0, o, ms);  check_regs ("lzf_ht_getmatches", o, ms);
    }
  }
  printf (fails ? "ABI check: %d FAILURES\n" : "ABI check: all OK (SysV + Microsoft x64 conventions, SSE2 + AVX2 paths)\n", fails);
  return fails != 0;
}
