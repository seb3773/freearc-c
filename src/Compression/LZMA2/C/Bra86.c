/* Bra86.c -- Converter for x86 code (BCJ)
2008-10-04 : Igor Pavlov : Public domain */

#include "Bra.h"

/* Opcode scan acceleration (freearc-c): the search for the next E8/E9 byte is done 32 bytes
   at a time with SSE2 (part of the x86-64 baseline, so no run-time dispatch is needed).
   The conversion itself is unchanged, so the output is identical to the scalar code.
   An AVX2 variant was measured and gave no additional gain, so it is not included.
   Define BRA86_NO_SIMD to build the original byte-by-byte scan only. */
#if !defined(BRA86_NO_SIMD) && (defined(__x86_64__) || defined(__SSE2__)) && (defined(__GNUC__) || defined(__clang__))
#define BRA86_SIMD 1
#include <emmintrin.h>

/* First position in [p,limit) holding E8/E9, scanning whole 16-byte blocks only (never reads
   at or beyond limit). If no block contains one, returns the first unscanned position and the
   scalar loop finishes the tail. */
static const Byte *Bra86_ScanSSE2(const Byte *p, const Byte *limit)
{
  const __m128i fe = _mm_set1_epi8((char)0xFE), e8 = _mm_set1_epi8((char)0xE8);
  for (; p + 32 <= limit; p += 32)
  {
    __m128i a = _mm_cmpeq_epi8(_mm_and_si128(_mm_loadu_si128((const __m128i *)p), fe), e8);
    __m128i b = _mm_cmpeq_epi8(_mm_and_si128(_mm_loadu_si128((const __m128i *)(p + 16)), fe), e8);
    unsigned m = (unsigned)_mm_movemask_epi8(a) | ((unsigned)_mm_movemask_epi8(b) << 16);
    if (m)
      return p + __builtin_ctz(m);
  }
  for (; p + 16 <= limit; p += 16)
  {
    unsigned m = (unsigned)_mm_movemask_epi8(_mm_cmpeq_epi8(_mm_and_si128(_mm_loadu_si128((const __m128i *)p), fe), e8));
    if (m)
      return p + __builtin_ctz(m);
  }
  return p;
}
#endif

#define Test86MSByte(b) ((b) == 0 || (b) == 0xFF)

const Byte kMaskToAllowedStatus[8] = {1, 1, 1, 0, 1, 0, 0, 0};
const Byte kMaskToBitNumber[8] = {0, 1, 2, 2, 3, 3, 3, 3};

SizeT x86_Convert(Byte *data, SizeT size, UInt32 ip, UInt32 *state, int encoding)
{
  SizeT bufferPos = 0, prevPosT;
  UInt32 prevMask = *state & 0x7;
  if (size < 5)
    return 0;
  ip += 5;
  prevPosT = (SizeT)0 - 1;

  for (;;)
  {
    Byte *p = data + bufferPos;
    Byte *limit = data + size - 4;
#ifdef BRA86_SIMD
    p = (Byte *)Bra86_ScanSSE2(p, limit);   /* stops on E8/E9 or before the last partial block */
#endif
    for (; p < limit; p++)
      if ((*p & 0xFE) == 0xE8)
        break;
    bufferPos = (SizeT)(p - data);
    if (p >= limit)
      break;
    prevPosT = bufferPos - prevPosT;
    if (prevPosT > 3)
      prevMask = 0;
    else
    {
      prevMask = (prevMask << ((int)prevPosT - 1)) & 0x7;
      if (prevMask != 0)
      {
        Byte b = p[4 - kMaskToBitNumber[prevMask]];
        if (!kMaskToAllowedStatus[prevMask] || Test86MSByte(b))
        {
          prevPosT = bufferPos;
          prevMask = ((prevMask << 1) & 0x7) | 1;
          bufferPos++;
          continue;
        }
      }
    }
    prevPosT = bufferPos;

    if (Test86MSByte(p[4]))
    {
      UInt32 src = ((UInt32)p[4] << 24) | ((UInt32)p[3] << 16) | ((UInt32)p[2] << 8) | ((UInt32)p[1]);
      UInt32 dest;
      for (;;)
      {
        Byte b;
        int index;
        if (encoding)
          dest = (ip + (UInt32)bufferPos) + src;
        else
          dest = src - (ip + (UInt32)bufferPos);
        if (prevMask == 0)
          break;
        index = kMaskToBitNumber[prevMask] * 8;
        b = (Byte)(dest >> (24 - index));
        if (!Test86MSByte(b))
          break;
        src = dest ^ ((1 << (32 - index)) - 1);
      }
      p[4] = (Byte)(~(((dest >> 24) & 1) - 1));
      p[3] = (Byte)(dest >> 16);
      p[2] = (Byte)(dest >> 8);
      p[1] = (Byte)dest;
      bufferPos += 5;
    }
    else
    {
      prevMask = ((prevMask << 1) & 0x7) | 1;
      bufferPos++;
    }
  }
  prevPosT = bufferPos - prevPosT;
  *state = ((prevPosT > 3) ? 0 : ((prevMask << ((int)prevPosT - 1)) & 0x7));
  return bufferPos;
}
