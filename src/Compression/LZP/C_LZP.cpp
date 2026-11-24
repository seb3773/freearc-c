/* Quick&dirty LZP compresion algorithm, developed by Dmitry Shkarin.
   Original code: http://www.compression.ru/ds/lzp.rar
   In turn, this code is based on LZP preprocessor in GRZipII compression
     algorithm, developed by Ilya Grebnov, Ilya.Grebnov@magicssoft.ru.
   Original code: http://magicssoft.ru/content/download/GRZipII/GRZipIISRC.zip
*/

extern "C" {
#include "C_LZP.h"
}
#if defined(__linux__)
#include <sys/mman.h>
#endif

/* Buffers kept on the worker thread. lzp_decompress used to mmap and munmap
   the input, the output and the hash table on every block. */
static void lzp_hint_hugepage(void *p, size_t n)
{
#if defined(__linux__) && defined(MADV_HUGEPAGE)
    if (p && n >= (size_t)(2u << 20))
        (void)madvise(p, n, MADV_HUGEPAGE);
#else
    (void)p; (void)n;
#endif
}
struct LzpTlsBuf {
    BYTE *p;
    size_t cap;
    LzpTlsBuf() : p(0), cap(0) {}
    ~LzpTlsBuf() { BigFree(p); p = NULL; cap = 0; }
    BYTE *acquire(size_t need) {
        if (need == 0) return NULL;
        if (p && cap >= need) return p;
        BYTE *np = (BYTE *)BigAlloc(need);
        if (!np) return NULL;
        BigFree(p);
        p = np;
        cap = need;
        lzp_hint_hugepage(p, cap);
        return p;
    }
};
struct LzpTls {
    LzpTlsBuf in, out, ht;
};
static thread_local LzpTls lzp_tls;


/* 32-bit Rotates */
#if defined(FREEARC_WIN)

/* intrinsic rotate */
#include <stdlib.h>
#pragma intrinsic(_lrotr,_lrotl)
#define ROR(x,n) _lrotr(x,n)

#elif !defined(__STRICT_ANSI__) && defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__)) && !defined(INTEL_CC) && !defined(LTC_NO_ASM)

static inline unsigned ROR(unsigned word, int i)
{
   asm ("rorl %%cl,%0"
      :"=r" (word)
      :"0" (word),"c" (i));
   return word;
}

#else

/* rotates the hard way */
#define ROR(x, y) ( ((((unsigned long)(x)&0xFFFFFFFFUL)>>(unsigned long)((y)&31)) | ((unsigned long)(x)<<(unsigned long)(32-((y)&31)))) & 0xFFFFFFFFUL)

#endif


/*------------------------------------------------------------------------------------*/
/* ������ ��������/����������, ���������� � ������������ ������ ����� ������ � ������ */
/*------------------------------------------------------------------------------------*/

/*                          tuned for PPMd
static const BYTE MO2MML[4] = {5,11,19,44};
static inline UINT GetMinMatchLen(UINT MaxOrder) {
    return (MaxOrder < 6)?(MO2MML[MaxOrder-2]):(CLAMP(10*MaxOrder-15,51,475));
}
*/
enum { LZP_MATCH_FLAG=0xB5 };

static inline UINT& lzpC(BYTE* p) { return *(UINT*)(p-4); }
static inline UINT  lzpH(UINT c,BYTE* p,int HashMask) {
//    return (c+11*(c >> 15)+13*lzpC(p-1)) & HashMask;
    return (c+5*ROR(c,17)+3*lzpC(p-1)) & HashMask;
}
#define LZP_INIT(HashSize,Pattern)                                               \
    UINT i, k, n1=1, n=1, HashMask=HashSize-1;                                   \
    BYTE *p, *InEnd=In+Size, *OutStart=Out;                                      \
    BYTE **HTable = (BYTE**) BigAlloc (HashSize * sizeof(BYTE*));                  \
    if (HTable==NULL)  return FREEARC_ERRCODE_NOT_ENOUGH_MEMORY;                 \
    for (i=0;i < HashSize;i++)              HTable[i]=Pattern+5;                 \
    lzpC(Out+4)=lzpC(In+4);                 lzpC(Out+8)=lzpC(In+8);              \
    i=lzpC(Out += 12)=lzpC(In += 12);       k=lzpH(i,Out,HashMask);

#ifndef FREEARC_DECOMPRESS_ONLY

/*
 * Optimized LZP encoder (bit-exact with the original Shkarin/Grebnov code kept in
 * LZPEncodeLegacy below and selectable with -DFREEARC_LZP_LEGACY_ENCODER).
 *
 * - Hash table stores 32-bit offsets from the start of the input block instead of raw
 *   64-bit pointers (Size is an UINT, so every position fits). The table shrinks by half,
 *   which lowers cache/TLB misses on the per-byte random lookup.
 * - Match length is computed 8 bytes at a time with XOR + count-trailing-zeros
 *   (little-endian targets), instead of 4-byte steps followed by a byte loop.
 *   Both compute the exact common prefix length capped at the end of input.
 * - The rotate used by the hash is written in portable C so the compiler emits a
 *   plain ROR and can schedule it (the inline asm version was opaque to the optimizer).
 * - Two-stage software prefetch: the hash bucket of the position LZP_PF bytes ahead,
 *   then the input bytes referenced by the bucket of the position LZP_PF2 bytes ahead.
 *   Prefetches are hints only; table contents and output are unaffected.
 *
 * The memory estimates of LZP_METHOD (GetCompressionMem/SetCompressionMem) intentionally still
 * count 8 bytes per hash bucket: they drive the automatic HashSizeLog/BlockSize selection, which
 * must stay identical to legacy FreeArc for archive parity.
 */
// Prefetch distances (bytes ahead): LZP_PF prefetches the hash bucket, LZP_PF2 (shorter, so
// the bucket is already cached) prefetches the input bytes the bucket currently points to.
// Tuned on the bench corpus; 0 disables a stage. Results do not depend on these values.
#ifndef LZP_PF
#define LZP_PF 64
#endif
#ifndef LZP_PF2
#define LZP_PF2 16
#endif

#if defined(__GNUC__) || defined(__clang__)
#define LZP_PREFETCH(addr)  __builtin_prefetch (addr)
#else
#define LZP_PREFETCH(addr)  ((void)0)
#endif

static inline UINT lzpLoad32 (const BYTE *p)            { UINT x; memcpy (&x, p, 4); return x; }
static inline UINT lzpRor17  (UINT c)                   { return (c >> 17) | (c << 15); }
// Hash of the context ending just before position p, given c = 4 bytes p[-4..-1]
static inline UINT lzpHash   (UINT c, const BYTE *p, UINT HashMask)
{
    return (c + 5*lzpRor17(c) + 3*lzpLoad32(p-5)) & HashMask;
}

// Length of the common prefix of a[] and b[], at most avail bytes. b < a, both readable up to a+avail.
static inline UINT lzpMatchLen (const BYTE *a, const BYTE *b, size_t avail)
{
    size_t j = 0;
#if defined(FREEARC_INTEL_BYTE_ORDER) && defined(__GNUC__)
    while (j + 8 <= avail) {
        unsigned long long x, y;
        memcpy (&x, a+j, 8);  memcpy (&y, b+j, 8);
        if (x != y)  return (UINT)(j + (__builtin_ctzll (x ^ y) >> 3));
        j += 8;
    }
#endif
    while (j < avail && a[j] == b[j])  j++;
    return (UINT)j;
}

int LZPEncode(BYTE* In,UINT Size,BYTE* Out,int MinLen,int HashSize,int Barrier,int SmallestLen)
{
    BYTE* OutEnd=Out+Size;   if (Size<32)  return 0;
    UINT i, k, n1=1, n=1, HashMask=HashSize-1;
    BYTE *InEnd=In+Size, *OutStart=Out, *Base=In;
    UINT *HTable = (UINT*) BigAlloc (HashSize * sizeof(UINT));
    if (HTable==NULL)  return FREEARC_ERRCODE_NOT_ENOUGH_MEMORY;
    for (i=0;i < (UINT)HashSize;i++)        HTable[i]=5;          // == Pattern+5 of the original code
    memcpy (Out, In, 12);   Out += 12;   In += 12;
    i=lzpLoad32(In-4);                      k=lzpHash(i,In,HashMask);
    do {
        BYTE *p = Base + HTable[k];         int ml;
#if LZP_PF > 0
        if (In+LZP_PF <= InEnd)
            LZP_PREFETCH (&HTable[lzpHash(lzpLoad32(In+LZP_PF-4), In+LZP_PF, HashMask)]);
#endif
#if LZP_PF2 > 0
        if (In+LZP_PF2 <= InEnd)
            LZP_PREFETCH (Base + HTable[lzpHash(lzpLoad32(In+LZP_PF2-4), In+LZP_PF2, HashMask)] - 4);
#endif
        if ( !--n )  { HTable[k]=(UINT)(In-Base);  n=n1; }
        if (i != lzpLoad32(p-4))            *Out++ = *In++;
        else if ((ml = In-p>Barrier? SmallestLen:MinLen), (In+ml <= InEnd && lzpLoad32(p+ml-4) == lzpLoad32(In+ml-4))) {
            i = lzpMatchLen (In, p, InEnd-In);
            if (i < (UINT)ml)               goto MATCH_NOT_FOUND;
            HTable[k]=(UINT)(In-Base);      n1 += (In-p > (n1+1)*HashSize && n1 < 7);
            *Out++ = LZP_MATCH_FLAG;        In += (k=i);
            for (i -= ml;i>=254 && Out<OutEnd;i -= 254)
                    *--OutEnd = 0;
            *--OutEnd = i+1;
            while(int(k -= 2*n1+1) > 0)     HTable[lzpHash(lzpLoad32(In-k-4),In-k,HashMask)]=(UINT)(In-k-Base);
        } else {
MATCH_NOT_FOUND:
            if ((*Out++ = *In++) == LZP_MATCH_FLAG)
                    *--OutEnd = 255;
        }
        k=lzpHash(i=lzpLoad32(In-4),In,HashMask);
    } while (In<InEnd && Out<OutEnd);
    BigFree(HTable);
    if (Out >= OutEnd)       return 0;
    memmove(Out,OutEnd,OutStart+Size-OutEnd);
    return Size-(OutEnd-Out);
}

#ifdef FREEARC_LZP_LEGACY_ENCODER
#define LZPEncode LZPEncodeLegacy
#endif
// Original encoder, kept as the reference implementation (enable with -DFREEARC_LZP_LEGACY_ENCODER)
int LZPEncodeLegacy(BYTE* In,UINT Size,BYTE* Out,int MinLen,int HashSize,int Barrier,int SmallestLen)
{
    BYTE* OutEnd=Out+Size;   if (Size<32)  return 0;
    LZP_INIT(HashSize,In);
    do {
        p=HTable[k];                        int ml;
        if ( !--n )  { HTable[k]=In;        n=n1; }
        if (i != lzpC(p))                   *Out++ = *In++;
        else if ((ml = In-p>Barrier? SmallestLen:MinLen), (In+ml <= InEnd && lzpC(p+ml) == lzpC(In+ml))) {
            for (i=4;In+i <= InEnd && lzpC(p+i) == lzpC(In+i);i += 4)
                    ;
            for (i -= 4;In+i < InEnd && In[i] == p[i];i++)
                    ;
            if (i < ml)                     goto MATCH_NOT_FOUND;
            HTable[k]=In;                   n1 += (In-p > (n1+1)*HashSize && n1 < 7);
            *Out++ = LZP_MATCH_FLAG;        In += (k=i);
            for (i -= ml;i>=254 && Out<OutEnd;i -= 254)
                    *--OutEnd = 0;
            *--OutEnd = i+1;
            while(int(k -= 2*n1+1) > 0)     HTable[lzpH(lzpC(In-k),In-k,HashMask)]=In-k;
        } else {
MATCH_NOT_FOUND:
            if ((*Out++ = *In++) == LZP_MATCH_FLAG)
                    *--OutEnd = 255;
        }
        k=lzpH(i=lzpC(In),In,HashMask);
    } while (In<InEnd && Out<OutEnd);
    BigFree(HTable);
    if (Out >= OutEnd)       return 0;
    memmove(Out,OutEnd,OutStart+Size-OutEnd);
    return Size-(OutEnd-Out);
}
#endif  // !defined (FREEARC_DECOMPRESS_ONLY)

#if defined(__GNUC__) || defined(__clang__)
#define LZP_DEC_PREFETCH_R(addr) __builtin_prefetch((addr), 0, 3)
#define LZP_DEC_PREFETCH_W(addr) __builtin_prefetch((addr), 1, 3)
#else
#define LZP_DEC_PREFETCH_R(addr) ((void)0)
#define LZP_DEC_PREFETCH_W(addr) ((void)0)
#endif

/* Copy len bytes from p to Out and apply the same HTable stores as the
   original byte loop. Updates sit at 0-based offsets period, 2*period, ...
   with period = 2*n1+1 (the countdown starts at period+1 and then reloads
   period). A store reads only bytes before its position, which the copy has
   already finalized, so every hash is identical if it is applied afterwards
   in increasing order. Wide copies are used only when the match distance
   keeps each chunk from overlapping its source. */
static inline void lzpApplyHashUpdates(BYTE *base, UINT len, UINT n1, BYTE **HTable, UINT HashMask)
{
    const UINT period = 2u * n1 + 1u;
    if (len >= 5)
        LZP_DEC_PREFETCH_R(&HTable[lzpH(lzpC(base + len), base + len, HashMask)]);
    if (period >= len) return;
    enum { PIPE = 4 };
    UINT hs[PIPE] = {0, 0, 0, 0};
    BYTE *ps[PIPE] = {0, 0, 0, 0};
    int n = 0;
    UINT u = period;
    for (; n < PIPE && u < len; ++n, u += period) {
        ps[n] = base + u;
        hs[n] = lzpH(lzpC(ps[n]), ps[n], HashMask);
        LZP_DEC_PREFETCH_W(&HTable[hs[n]]);
    }
    if (n < PIPE) {
        for (int i = 0; i < n; i++) HTable[hs[i]] = ps[i];
        return;
    }
    int rd = 0;
    while (u < len) {
        HTable[hs[rd]] = ps[rd];
        BYTE *at = base + u;
        UINT h = lzpH(lzpC(at), at, HashMask);
        LZP_DEC_PREFETCH_W(&HTable[h]);
        hs[rd] = h;
        ps[rd] = at;
        rd = (rd + 1) & (PIPE - 1);
        u += period;
    }
    for (int i = 0; i < PIPE; i++) {
        HTable[hs[rd]] = ps[rd];
        rd = (rd + 1) & (PIPE - 1);
    }
}

static inline void lzpDecodeMatch(BYTE *&Out, BYTE *&p, UINT len, UINT n1, BYTE **HTable, UINT HashMask)
{
    size_t dist = (p < Out) ? (size_t)(Out - p) : 0;
    if (dist < 4 || len < 8) {
        UINT k = 2u * n1 + 2u;
        UINT i = len;
        do {
            if ( !--k ) { k = 2u * n1 + 1u; HTable[lzpH(lzpC(Out), Out, HashMask)] = Out; }
            *Out++ = *p++;
        } while ( --i );
        return;
    }
    if (dist >= len)
        LZP_DEC_PREFETCH_R(&HTable[lzpH(lzpC(p + len), p + len, HashMask)]);
    BYTE *base = Out;
    UINT done = 0;
    if (dist >= 8) {
        while (done < len) {
            size_t left = (size_t)(len - done);
            size_t ncopy = dist < left ? dist : left;
            memcpy(Out, p, ncopy);
            Out += ncopy;
            p += ncopy;
            done += (UINT)ncopy;
        }
    } else {
        /* Distance 4..7: four-byte chunks never reach back into the bytes
           just written. The tail is shorter than a word. */
        while (len - done >= 4) {
            uint32_t w;
            memcpy(&w, p, 4);
            memcpy(Out, &w, 4);
            Out += 4;
            p += 4;
            done += 4;
        }
        while (done < len) { *Out++ = *p++; done++; }
    }
    lzpApplyHashUpdates(base, len, n1, HTable, HashMask);
}

int LZPDecode(BYTE* In,UINT Size,BYTE* Out,int MinLen,int HashSize,int Barrier,int SmallestLen)
{
    UINT i, k, n1=1, n=1, HashMask=HashSize-1;
    BYTE *p, *InEnd=In+Size, *OutStart=Out;
    BYTE **HTable = (BYTE**) lzp_tls.ht.acquire((size_t)HashSize * sizeof(BYTE*));
    if (HTable==NULL)  return FREEARC_ERRCODE_NOT_ENOUGH_MEMORY;
    for (i=0;i < (UINT)HashSize;i++)        HTable[i]=Out+5;
    lzpC(Out+4)=lzpC(In+4);                 lzpC(Out+8)=lzpC(In+8);
    i=lzpC(Out += 12)=lzpC(In += 12);       k=lzpH(i,Out,HashMask);
    do {
        p=HTable[k];
        if ( !--n )  { HTable[k]=Out;       n=n1; }
        if (*In++ != LZP_MATCH_FLAG || i != lzpC(p) || *--InEnd == 255)
                *Out++ = In[-1];
        else {
            HTable[k]=Out;                  n1 += (Out-p > (n1+1)*HashSize && n1 < 7);
            for (i=(Out-p>Barrier? SmallestLen:MinLen)-1;*InEnd == 0;InEnd--)
                    i += 254;
            i += *InEnd;
            lzpDecodeMatch(Out, p, i, n1, HTable, HashMask);
        }
        k=lzpH(i=lzpC(Out),Out,HashMask);
    } while (In < InEnd);
    return (Out-OutStart);
}



/*-------------------------------------------------------------------------*/
/* ������ ��������/����������, ������������ callbacks ��� �����/������     */
/*-------------------------------------------------------------------------*/

#ifndef FREEARC_DECOMPRESS_ONLY
int lzp_compress (MemSize BlockSize, int MinCompression, int MinMatchLen, int HashSizeLog, int Barrier, int SmallestLen, CALLBACK_FUNC *callback, void *auxdata)
{
    int errcode = FREEARC_OK;   // Error code returned by last operation or FREEARC_OK
    BYTE* In = NULL;  // ��������� �� ������� ������
    BYTE* Out= NULL;  // ��������� �� �������� ������
    while (1)
    {
        int InSize, OutSize;     // ���������� ���� �� ������� � �������� ������, ��������������
        BIGALLOC (BYTE, In, BlockSize+2);
    	READ_LEN_OR_EOF (InSize, In, BlockSize);
        //In = (BYTE*) realloc(In,InSize);   -- impossible since we used BigAlloc
        BIGALLOC (BYTE, Out, InSize+2);
        OutSize = LZPEncode (In, InSize, Out, MinMatchLen, 1<<HashSizeLog, Barrier, SmallestLen);
        if (OutSize<0)  {errcode=OutSize; goto finished;}
        if (OutSize==0  ||  MinCompression>0 && OutSize>=(double(InSize)*MinCompression)/100) {
            // ��������� ������ [���������� ������] �� �������, ������� ������ ��� �������� ������
            BigFreeAndNil(Out);
            WRITE4 (-InSize);      // ������������� ����� � �������� ����� ����� - ������� Stored �����
            WRITE  (In, InSize);
            BigFreeAndNil(In);
        } else {
            // ������ ������� ���������, ����� ���������� ������� ����� ������ ��� ���������� ��
            // (����� ���������� ������ ������ ��� ���������� ��������� � ������� ���������� ������)
            BigFreeAndNil(In);
            WRITE4 (OutSize);
            WRITE  (Out, OutSize);
            BigFreeAndNil(Out);
        }
    }
finished:
    BigFreeAndNil(In); BigFreeAndNil(Out);
    return errcode;
}
#endif  // !defined (FREEARC_DECOMPRESS_ONLY)


int lzp_decompress (MemSize BlockSize, int MinCompression, int MinMatchLen, int HashSizeLog, int Barrier, int SmallestLen, CALLBACK_FUNC *callback, void *auxdata)
{
    int errcode = FREEARC_OK;   // Error code returned by last operation or FREEARC_OK
    // In/Out live in lzp_tls and are reused across blocks and 4x4 chunks.
    // They are released when this worker thread exits.
    for(;;) {
        int InSize, OutSize;
        BYTE *In = NULL;
        BYTE *Out = NULL;
        READ4_OR_EOF (InSize);
        if (InSize<0) {
            InSize = -InSize;
            In = lzp_tls.in.acquire((size_t)InSize);
            if (InSize && !In) { errcode = FREEARC_ERRCODE_NOT_ENOUGH_MEMORY; goto finished; }
            READ  (In, InSize);
            WRITE (In, InSize);
        } else {
            In  = lzp_tls.in.acquire((size_t)InSize);
            Out = lzp_tls.out.acquire((size_t)BlockSize);
            if ((InSize && !In) || (BlockSize && !Out)) {
                errcode = FREEARC_ERRCODE_NOT_ENOUGH_MEMORY;
                goto finished;
            }
            READ  (In, InSize);
            OutSize = LZPDecode (In, InSize, Out, MinMatchLen, 1<<HashSizeLog, Barrier, SmallestLen);
            if (OutSize < 0) { errcode = OutSize; goto finished; }
            WRITE (Out, OutSize);
        }
    }
finished:
    return errcode;
}


/*-------------------------------------------------*/
/* ���������� ������ LZP_METHOD                    */
/*-------------------------------------------------*/

// �����������, ������������� ���������� ������ ������ �������� �� ���������
LZP_METHOD::LZP_METHOD()
{
  BlockSize      = 8*mb;
  MinCompression = 100;
  MinMatchLen    = 64;
  HashSizeLog    = 18;
  Barrier        = INT_MAX;
  SmallestLen    = 32;
}

// ������� ����������
int LZP_METHOD::decompress (CALLBACK_FUNC *callback, void *auxdata)
{
  // Use faster function from DLL if possible
  static FARPROC f = LoadFromDLL ("lzp_decompress");
  if (!f) f = (FARPROC) lzp_decompress;

  return ((int (*)(MemSize, int, int, int, int, int, CALLBACK_FUNC*, void*)) f)
            (BlockSize, MinCompression, MinMatchLen, HashSizeLog, Barrier, SmallestLen, callback, auxdata);
}

#ifndef FREEARC_DECOMPRESS_ONLY

// ������� ��������
int LZP_METHOD::compress (CALLBACK_FUNC *callback, void *auxdata)
{
  // Use faster function from DLL if possible
  static FARPROC f = LoadFromDLL ("lzp_compress");
  if (!f) f = (FARPROC) lzp_compress;

  return ((int (*)(MemSize, int, int, int, int, int, CALLBACK_FUNC*, void*)) f)
            (BlockSize, MinCompression, MinMatchLen, HashSizeLog, Barrier, SmallestLen, callback, auxdata);
}

// ���������� ������ ����� � ��������� ������ ����, ���� �� ������� ����� ��� ������ ���������� �����
void LZP_METHOD::SetBlockSize (MemSize bs)
{
  if (bs>0) {
    BlockSize   = bs;
    HashSizeLog = mymin (HashSizeLog, 1+lb(BlockSize-1));
  }
}

// �������� � buf[MAX_METHOD_STRLEN] ������, ����������� ����� ������ � ��� ��������� (�������, �������� � parse_LZP)
void LZP_METHOD::ShowCompressionMethod (char *buf)
{
    LZP_METHOD defaults; char BlockSizeStr[100], MinCompressionStr[100], BarrierTempStr[100], BarrierStr[100], SmallestLenStr[100];
    showMem (BlockSize, BlockSizeStr);
    showMem (Barrier,   BarrierTempStr);
    sprintf (MinCompressionStr, MinCompression!=defaults.MinCompression? ":%d%%" : "", MinCompression);
    sprintf (BarrierStr, Barrier!=defaults.Barrier? ":d%s" : "", BarrierTempStr);
    sprintf (SmallestLenStr, SmallestLen!=defaults.SmallestLen? ":s%d" : "", SmallestLen);
    sprintf (buf, "lzp:%s%s:%d:h%d%s%s", BlockSizeStr, MinCompressionStr, MinMatchLen, HashSizeLog, BarrierStr, SmallestLenStr);
}

// ������������� ���������� ������, ������� ������ �������������� ��� �������� � ����������
void LZP_METHOD::SetCompressionMem (MemSize mem)
{
  MemSize hashsize = (1<<HashSizeLog) * sizeof(BYTE*);
  // ���� ��� �������� ������� ����� ����� - �������� ������� ���. ����� ����� ��������� ����������
  if (hashsize > mem/4) {
    HashSizeLog = lb(mem/16);
    if (GetCompressionMem() <= mem)  return;
    hashsize = (1<<HashSizeLog) * sizeof(BYTE*);
  }
  SetBlockSize ((mem-hashsize)/2);
}


#endif  // !defined (FREEARC_DECOMPRESS_ONLY)

// ������������ ������ ���� LZP_METHOD � ��������� ����������� ��������
// ��� ���������� NULL, ���� ��� ������ ����� ������ ��� �������� ������ � ����������
COMPRESSION_METHOD* parse_LZP (char** parameters)
{
  if (strcmp (parameters[0], "lzp") == 0) {
    // ���� �������� ������ (������� ��������) - "lzp", �� ������� ��������� ���������

    LZP_METHOD *p = new LZP_METHOD;
    int error = 0;  // ������� ����, ��� ��� ������� ���������� ��������� ������

    // �������� ��� ��������� ������ (��� ������ ������ ��� ������������� ������ ��� ������� ���������� ���������)
    while (*++parameters && !error)
    {
      char* param = *parameters;
      switch (*param) {                    // ���������, ���������� ��������
        case 'b':  p->BlockSize   = parseMem (param+1, &error); continue;
        case 'l':  p->MinMatchLen = parseInt (param+1, &error); continue;
        case 'h':  p->HashSizeLog = parseInt (param+1, &error); continue;
        case 'd':  p->Barrier     = parseMem (param+1, &error); continue;
        case 's':  p->SmallestLen = parseInt (param+1, &error); continue;
      }
      // ���� �������� ������������� ������ ��������. �� ��������� ���������� ��� ��� "N%"
      if (last_char(param) == '%') {
        char str[100]; strcpy(str,param); last_char(str) = '\0';
        int n = parseInt (str, &error);
        if (!error) { p->MinCompression = n; continue; }
        error=0;
      }
      // ���� �� ��������, ���� � ��������� �� ������� ��� ��������
      // ���� ���� �������� ������� ��������� ��� ����� ����� (�.�. � ��� - ������ �����),
      // �� �������� ��� �������� ���� MinMatchLen, ����� ��������� ��������� ��� ��� BlockSize
      int n = parseInt (param, &error);
      if (!error) p->MinMatchLen = n;
      else        error=0, p->BlockSize = parseMem (param, &error);
    }
    if (error)  {delete p; return NULL;}  // ������ ��� �������� ���������� ������
    return p;
  } else
    return NULL;   // ��� �� ����� lzp
}

static int LZP_x = AddCompressionMethod (parse_LZP);   // �������������� ������ ������ LZP
