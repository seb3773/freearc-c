/****************************************************************************
 *  This file is part of PPMd project                                       *
 *  Contents: 'Carryless rangecoder' by Dmitry Subbotin                     *
 *  Comments: this implementation is claimed to be a public domain          *
 ****************************************************************************/
/**********************  Original text  *************************************
////////   Carryless rangecoder (c) 1999 by Dmitry Subbotin   ////////

typedef unsigned int  uint;
typedef unsigned char uc;

#define  DO(n)     for (int _=0; _<n; _++)
#define  TOP       (1<<24)
#define  BOT       (1<<16)


class RangeCoder
{
 uint  low, code, range, passed;
 FILE  *f;

 void OutByte (uc c)           { passed++; fputc(c,f); }
 uc   InByte ()                { passed++; return fgetc(f); }

public:

 uint GetPassed ()             { return passed; }
 void StartEncode (FILE *F)    { f=F; passed=low=0;  range= (uint) -1; }
 void FinishEncode ()          { DO(4)  OutByte(low>>24), low<<=8; }
 void StartDecode (FILE *F)    { passed=low=code=0;  range= (uint) -1;
                                 f=F; DO(4) code= code<<8 | InByte();
                               }

 void Encode (uint cumFreq, uint freq, uint totFreq) {
    assert(cumFreq+freq<totFreq && freq && totFreq<=BOT);
    low  += cumFreq * (range/= totFreq);
    range*= freq;
    while ((low ^ low+range)<TOP || range<BOT && ((range= -low & BOT-1),1))
       OutByte(low>>24), range<<=8, low<<=8;
 }

 uint GetFreq (uint totFreq) {
   uint tmp= (code-low) / (range/= totFreq);
   if (tmp >= totFreq)  throw ("Input data corrupt"); // or force it to return
   return tmp;                                         // a valid value :)
 }

 void Decode (uint cumFreq, uint freq, uint totFreq) {
    assert(cumFreq+freq<totFreq && freq && totFreq<=BOT);
    low  += cumFreq*range;
    range*= freq;
    while ((low ^ low+range)<TOP || range<BOT && ((range= -low & BOT-1),1))
       code= code<<8 | InByte(), range<<=8, low<<=8;
 }
};
*****************************************************************************/

struct SUBRANGE {
    DWORD LowCount, HighCount, scale;
};
static PPMD_TLS SUBRANGE SubRange;
enum { TOP=1 << 24, BOT=1 << 15 };
static PPMD_TLS DWORD low, code, range;

inline void ariInitEncoder()
{
    low=0;                                  range=DWORD(-1);
}
#define ARI_ENC_NORMALIZE(stream) {                                         \
    while ((low ^ (low+range)) < TOP || range < BOT &&                      \
            ((range= -low & (BOT-1)),1)) {                                  \
        _PPMD_E_PUTC(low >> 24,stream);                                     \
        range <<= 8;                        low <<= 8;                      \
    }                                                                       \
}
inline void ariEncodeSymbol()
{
    low += SubRange.LowCount*(range /= SubRange.scale);
    range *= SubRange.HighCount-SubRange.LowCount;
}
inline void ariShiftEncodeSymbol(UINT SHIFT)
{
    low += SubRange.LowCount*(range >>= SHIFT);
    range *= SubRange.HighCount-SubRange.LowCount;
}
#define ARI_FLUSH_ENCODER(stream) {                                         \
    for (UINT i=0;i < 4;i++) {                                              \
        _PPMD_E_PUTC(low >> 24,stream);                 low <<= 8;          \
    }                                                                       \
}
#define ARI_INIT_DECODER(stream) {                                          \
    low=code=0;                             range=DWORD(-1);                \
    for (UINT i=0;i < 4;i++)                                                \
            code=(code << 8) | _PPMD_D_GETC(stream);                        \
}
/* Same bytes as the original while-loop. The common path (range already above
   BOT and the high byte stable) returns immediately. When a shift is needed and
   range cannot overflow 32 bits, the shift count comes from the leading zeros
   of (low XOR low+range) instead of re-testing the condition each byte. */
inline void ariDecNormalizeLocal(DWORD &l_low, DWORD &l_code, DWORD &l_range, _PPMD_FILE *stream)
{
    if (l_range >= (DWORD)BOT && l_range < (DWORD)TOP) {
        DWORD diff = l_low ^ (l_low + l_range);
        if (diff >= (DWORD)TOP)
            return;
        if (diff != 0) {
            unsigned k = (unsigned)__builtin_clz(diff) >> 3;
            if (k > 0 && l_range < (1u << (32 - 8*k))) {
                if (__builtin_expect(k == 1, 1)) {
                    DWORD b;
                    if (__builtin_expect(stream->readByte(b), 1)) {
                        l_code = (l_code << 8) | b;
                        l_range <<= 8;        l_low <<= 8;
                        return;
                    }
                } else {
                    DWORD packed;
                    if (stream->readPacked(k, packed)) {
                        unsigned sh = k << 3;
                        l_code = (l_code << sh) | packed;
                        l_range <<= sh;      l_low <<= sh;
                        return;
                    }
                }
                do {
                    l_code=(l_code << 8) | _PPMD_D_GETC(stream);
                    l_range <<= 8;            l_low <<= 8;
                } while (--k);
                return;
            }
        }
    }
    while ((l_low ^ (l_low+l_range)) < TOP || l_range < BOT &&
            ((l_range= -l_low & (BOT-1)),1)) {
        l_code=(l_code << 8) | _PPMD_D_GETC(stream);
        l_range <<= 8;                        l_low <<= 8;
    }
}
inline void ariScaleRangeDownLocal(DWORD &l_range, DWORD scale)
{
    if (__builtin_expect(scale - 1u < 65535u, 1))
        l_range = ::ppmd_div_u32_small(l_range, scale);
    else
        l_range /= scale;
}
inline void ariRemoveSubrangeLocal(DWORD &l_low, DWORD &l_range, const SUBRANGE &sr)
{
    l_low += l_range*sr.LowCount;
    l_range *= sr.HighCount-sr.LowCount;
}
inline void ariDecNormalize(_PPMD_FILE *stream) { ariDecNormalizeLocal(low, code, range, stream); }
#define ARI_DEC_NORMALIZE(stream) ariDecNormalize(stream)
/* range := floor(range / SubRange.scale), bit-identical to the hardware
   division. scale is a frequency total; values above 65535 fall back to it.
   ppmd_div_u32_small() is the reciprocal multiply defined in C_PPMD.cpp. */
inline void ariScaleRangeDown() { ariScaleRangeDownLocal(range, SubRange.scale); }
inline UINT ariGetCurrentCount() {
    ariScaleRangeDown();
    /* (code-low)/range stays a hardware division: after the scale step the
       divisor is an arbitrary 32-bit range, and the quotient is needed in full.
       decodeSymbol1 avoids it when the first symbol matches. */
    return (code-low)/range;
}
inline UINT ariGetCurrentShiftCount(UINT SHIFT) {
    return (code-low)/(range >>= SHIFT);
}
inline void ariRemoveSubrange() { ariRemoveSubrangeLocal(low, range, SubRange); }
