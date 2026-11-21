/* lzma_fastenc.c -- thin wrapper around the LZMA SDK 25.01 encoder.
   Unity build: the SDK encoder sources are compiled into this translation unit with every
   external symbol renamed (see lzma_fastenc_rename.h). */
#if !defined(_WIN32) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE   /* Threads.c needs it before the first system header (CPU affinity API) */
#endif
#include "lzma_fastenc_rename.h"
#include "Precomp.h"
#include <stdlib.h>
#include <string.h>

#include "CpuArch.c"
#include "Threads.c"
#include "LzFind.c"
#include "LzFindOpt.c"
#include "LzFindMt.c"
#include "LzmaEnc.c"

#include "lzma_fastenc.h"

typedef struct
{
  ISzAlloc      vt;          /* must be the first member */
  lzfe_alloc_cb alloc;
  lzfe_free_cb  free;
} CBigAlloc;

struct lzfe
{
  CLzmaEncHandle enc;
  CBigAlloc      big;
};

static void *SmallAlloc (ISzAllocPtr p, size_t size) { (void)p; return malloc (size); }
static void  SmallFree  (ISzAllocPtr p, void *a)      { (void)p; free (a); }
static const ISzAlloc g_SmallAlloc = { SmallAlloc, SmallFree };

static void *BigAllocF (ISzAllocPtr p, size_t size) { return ((const CBigAlloc *)(const void *)p)->alloc (size); }
static void  BigFreeF  (ISzAllocPtr p, void *a)      { ((const CBigAlloc *)(const void *)p)->free (a); }

static void *DefBigAlloc (size_t size) { return malloc (size); }
static void  DefBigFree  (void *a)     { free (a); }

lzfe *lzfe_create (const lzfe_props *pr, lzfe_alloc_cb big_alloc, lzfe_free_cb big_free)
{
  CLzmaEncProps props;
  lzfe *e = (lzfe *) malloc (sizeof (lzfe));
  if (!e) return NULL;
  e->big.vt.Alloc = BigAllocF;
  e->big.vt.Free  = BigFreeF;
  e->big.alloc    = big_alloc ? big_alloc : DefBigAlloc;
  e->big.free     = big_free  ? big_free  : DefBigFree;
  e->enc = LzmaEnc_Create (&g_SmallAlloc);
  if (!e->enc) { free (e); return NULL; }

  LzmaEncProps_Init (&props);
  props.level        = 9;
  props.dictSize     = pr->dictSize;
  props.lc           = pr->lc;
  props.lp           = pr->lp;
  props.pb           = pr->pb;
  props.algo         = pr->algo;
  props.fb           = pr->fb;
  props.btMode       = pr->btMode;
  props.numHashBytes = pr->numHashBytes;
  props.mc           = pr->mc;
  props.numThreads   = pr->numThreads;
  props.writeEndMark = 1;
  if (LzmaEnc_SetProps (e->enc, &props) != SZ_OK) {
    LzmaEnc_Destroy (e->enc, &g_SmallAlloc, &e->big.vt);
    free (e);
    return NULL;
  }
  return e;
}

void lzfe_destroy (lzfe *e)
{
  if (!e) return;
  LzmaEnc_Destroy (e->enc, &g_SmallAlloc, &e->big.vt);
  free (e);
}

int lzfe_encode_mem (lzfe *e, unsigned char *dest, size_t *destLen, const unsigned char *src, size_t srcLen)
{
  SizeT dl = *destLen;
  SRes res = LzmaEnc_MemEncode (e->enc, dest, &dl, src, srcLen, 1, NULL, &g_SmallAlloc, &e->big.vt);
  *destLen = dl;
  return res;
}

typedef struct { ISeqInStream  vt; lzfe_read_cb  cb; void *aux; } CInWrap;
typedef struct { ISeqOutStream vt; lzfe_write_cb cb; void *aux; } COutWrap;

static SRes InWrap_Read (ISeqInStreamPtr pp, void *buf, size_t *size)
{
  const CInWrap *p = (const CInWrap *)(const void *)pp;
  return p->cb (p->aux, buf, size) ? SZ_ERROR_READ : SZ_OK;
}

static size_t OutWrap_Write (ISeqOutStreamPtr pp, const void *buf, size_t size)
{
  const COutWrap *p = (const COutWrap *)(const void *)pp;
  return p->cb (p->aux, buf, size);
}

int lzfe_encode_stream (lzfe *e, lzfe_read_cb rd, void *rd_aux, lzfe_write_cb wr, void *wr_aux)
{
  CInWrap  in;
  COutWrap out;
  in.vt.Read   = InWrap_Read;    in.cb  = rd;  in.aux  = rd_aux;
  out.vt.Write = OutWrap_Write;  out.cb = wr;  out.aux = wr_aux;
  return LzmaEnc_Encode (e->enc, &out.vt, &in.vt, NULL, &g_SmallAlloc, &e->big.vt);
}

size_t lzfe_hash_bytes (unsigned dictSize, int btMode, int numHashBytes)
{
  /* Mirrors the hash sizing of MatchFinder_Create() in LzFind.c */
  UInt32 hs;
  size_t fixed = 0;
  if (numHashBytes <= 2)
    hs = (1 << 16) - 1;
  else {
    hs = dictSize - 1;
    hs |= (hs >> 1);  hs |= (hs >> 2);  hs |= (hs >> 4);  hs |= (hs >> 8);
    hs >>= 1;
    hs |= 0xFFFF;
    if (hs > (1 << 24)) {
      if (numHashBytes == 3) hs = (1 << 24) - 1;
      else hs >>= 1;
    }
    fixed = (numHashBytes >= 3 ? kHash2Size : 0) + (numHashBytes >= 4 ? kHash3Size : 0);
  }
  (void)btMode;
  return ((size_t)hs + 1 + fixed) * sizeof (CLzRef);
}
