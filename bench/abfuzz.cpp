// Differential fuzzer for the encoders: compresses generated buffers with two codec libraries
// (baseline and candidate, both built by bench/ab_build.sh) and requires byte-identical output.
// Inputs cover tiny/odd sizes (0..80 bytes), medium and multi-MB buffers, and several data
// shapes (random, word text, repetitive, x86-like with many E8/E9, 2-letter alphabet, the LZP
// flag byte 0xB5, long periodic matches). The first mismatching input is saved to fail.bin.
//
// Usage: abfuzz BASE.so CAND.so ITERATIONS METHOD...
//   e.g. abfuzz /tmp/base.so /tmp/cand.so 3000 exe lzp "lzp:80m:105:d1m:s32:h22:92%" "dict:64m:0%"
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <string>
typedef int (*CompressMemFn) (char *method, void *input, int inputSize, void *output, int outputSize);
static unsigned long long rs = 88172645463325252ULL;
static unsigned rnd() { rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return (unsigned)rs; }
static void gen (std::vector<unsigned char> &b, int kind, size_t n)
{
    b.resize (n);
    static const char *words[] = {"the ","of ","and ","function ","return ","int ","self.","import ","while ","for ","\n","    ","x86","data ","value=","(", ")", ";", "class ", "def "};
    switch (kind) {
    case 0: for (auto &c : b) c = rnd(); break;                                   // random
    case 1: { size_t i=0; while (i<n) { const char *w = words[rnd()%20]; for (; *w && i<n; w++) b[i++]=*w; } } break; // text
    case 2: for (size_t i=0;i<n;i++) b[i] = (i<64)? rnd() : (rnd()%50? b[i-1-(rnd()%3==0? 31 : 0)] : rnd()); break; // repetitive
    case 3: for (size_t i=0;i<n;i++) { unsigned r=rnd()%16; b[i] = r==0? 0xE8 : r==1? 0xE9 : r<4? 0x00 : r<6? 0xFF : rnd(); } break; // x86-ish
    case 4: for (size_t i=0;i<n;i++) b[i] = "ab"[rnd()&1]; break;                 // tiny alphabet
    case 5: for (size_t i=0;i<n;i++) b[i] = 0xB5; break;                           // LZP flag byte everywhere
    case 6: { for (size_t i=0;i<n;i++) b[i] = (rnd()%7==0)? 0xB5 : "abcdefgh"[rnd()%8]; } break;
    case 7: { size_t blk = 1 + rnd()%300; for (size_t i=0;i<n;i++) b[i] = (i<blk)? rnd() : b[i-blk]; } break; // long periodic matches
    }
}
int main (int argc, char **argv)
{
    if (argc < 5) { fprintf (stderr, "usage: abfuzz BASE.so CAND.so ITER METHOD...\n"); return 1; }
    CompressMemFn fn[2];
    for (int i = 0; i < 2; i++) { void *h = dlopen (argv[1+i], RTLD_NOW|RTLD_LOCAL); if (!h) { fprintf(stderr,"%s\n",dlerror()); return 1;} fn[i] = (CompressMemFn) dlsym (h, "CompressMem"); }
    int iters = atoi (argv[3]); long cases = 0;
    std::vector<unsigned char> in, o0, o1;
    for (int it = 0; it < iters; it++) {
        int kind = rnd() % 8;
        size_t n = (it % 4 == 0)? rnd() % 80 : (it % 4 == 1)? rnd() % 5000 : (it % 4 == 2)? rnd() % 300000 : rnd() % 3000000;
        gen (in, kind, n);
        for (int m = 4; m < argc; m++) {
            int cap = (int)(n + n/2 + (1<<16));
            o0.assign (cap, 0); o1.assign (cap, 0x55);
            char mm[256];
            strcpy (mm, argv[m]); int r0 = fn[0] (mm, in.data(), (int)n, o0.data(), cap);
            strcpy (mm, argv[m]); int r1 = fn[1] (mm, in.data(), (int)n, o1.data(), cap);
            cases++;
            if (r0 != r1 || (r0 > 0 && memcmp (o0.data(), o1.data(), r0))) {
                printf ("MISMATCH method=%s kind=%d n=%zu r0=%d r1=%d\n", argv[m], kind, n, r0, r1);
                FILE *f = fopen ("fail.bin", "wb"); fwrite (in.data(), 1, n, f); fclose (f);
                return 3;
            }
        }
    }
    printf ("OK %ld cases, all bit-identical\n", cases);
    return 0;
}
