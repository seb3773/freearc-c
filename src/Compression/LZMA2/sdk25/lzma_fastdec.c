/* Thin wrapper around the LZMA SDK 25.01 decoder. */
/* Rename SDK symbols so they never clash with the 2008 SDK linked in the same binary. */
#define LzmaDec_Init sdk25_LzmaDec_Init
#define LzmaDec_InitDicAndState sdk25_LzmaDec_InitDicAndState
#define LzmaDec_DecodeToDic sdk25_LzmaDec_DecodeToDic
#define LzmaDec_DecodeToBuf sdk25_LzmaDec_DecodeToBuf
#define LzmaDec_FreeProbs sdk25_LzmaDec_FreeProbs
#define LzmaDec_Free sdk25_LzmaDec_Free
#define LzmaProps_Decode sdk25_LzmaProps_Decode
#define LzmaDec_AllocateProbs sdk25_LzmaDec_AllocateProbs
#define LzmaDec_Allocate sdk25_LzmaDec_Allocate
#define LzmaDecode sdk25_LzmaDecode
#define LzmaDec_DecodeReal_3 sdk25_LzmaDec_DecodeReal_3
#include "Precomp.h"
#if defined(_WIN32) && defined(__x86_64__) && defined(Z7_LZMA_DEC_OPT)
#undef Z7_FASTCALL
#define Z7_FASTCALL __attribute__((sysv_abi))
#endif
#include <stdlib.h>
/* Unity build: the decoder is compiled into this translation unit. */
#include "LzmaDec.c"
#include "lzma_fastdec.h"

struct lzfd { CLzmaDec dec; };

static void *SzAllocF (ISzAllocPtr p, size_t size) { (void)p; return malloc (size); }
static void  SzFreeF  (ISzAllocPtr p, void *a)      { (void)p; free (a); }
static const ISzAlloc g_AllocF = { SzAllocF, SzFreeF };

lzfd *lzfd_create (unsigned lc, unsigned lp, unsigned pb, unsigned dicSize)
{
  Byte props[5];
  lzfd *d = (lzfd*) malloc (sizeof (lzfd));
  if (!d) return NULL;
  props[0] = (Byte)((pb * 5 + lp) * 9 + lc);
  props[1] = (Byte)dicSize; props[2] = (Byte)(dicSize >> 8);
  props[3] = (Byte)(dicSize >> 16); props[4] = (Byte)(dicSize >> 24);
  LzmaDec_CONSTRUCT (&d->dec);
  if (LzmaDec_Allocate (&d->dec, props, LZMA_PROPS_SIZE, &g_AllocF) != SZ_OK) { free (d); return NULL; }
  LzmaDec_Init (&d->dec);
  return d;
}

void lzfd_destroy (lzfd *d) { if (d) { LzmaDec_Free (&d->dec, &g_AllocF); free (d); } }

int lzfd_decode (lzfd *d, size_t dicLimit, const unsigned char *src, size_t *srcLen, int *finishedWithMark)
{
  ELzmaStatus status;
  SRes res = LzmaDec_DecodeToDic (&d->dec, dicLimit, src, srcLen, LZMA_FINISH_ANY, &status);
  *finishedWithMark = (status == LZMA_STATUS_FINISHED_WITH_MARK);
  return res == SZ_OK ? 0 : 1;
}

unsigned char *lzfd_dic (lzfd *d)        { return d->dec.dic; }
size_t lzfd_dicPos (lzfd *d)             { return d->dec.dicPos; }
size_t lzfd_dicBufSize (lzfd *d)         { return d->dec.dicBufSize; }
void   lzfd_resetDicPos (lzfd *d)        { d->dec.dicPos = 0; }
