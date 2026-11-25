/****************************************************************************
 *  This file is part of PPMd project                                       *
 *  Written and distributed to public domain by Dmitry Shkarin 1997,        *
 *  1999-2001                                                               *
 *  Contents: PPMII model description and encoding/decoding routines        *
 ****************************************************************************/
#include <string.h>
#include "PPMd.h"
#pragma hdrstop
#if defined(__GNUC__)
#pragma GCC optimize ("no-strict-aliasing")
#endif
#include "Coder.hpp"
#include "SubAlloc.hpp"

enum { UP_FREQ=5, INT_BITS=7, PERIOD_BITS=7, TOT_BITS=INT_BITS+PERIOD_BITS,
    INTERVAL=1 << INT_BITS, BIN_SCALE=1 << TOT_BITS, MAX_FREQ=124, O_BOUND=9 };

#pragma pack(1)
struct SEE2_CONTEXT { // SEE-contexts for PPM-contexts with masked symbols
    WORD Summ;
    BYTE Shift, Count;
    void init(UINT InitVal) { Summ=InitVal << (Shift=PERIOD_BITS-4); Count=7; }
    UINT getMean() {
        UINT RetVal=(Summ >> Shift);        Summ -= RetVal;
        return RetVal+(RetVal == 0);
    }
    void update() {
        if (Shift < PERIOD_BITS && --Count == 0) {
            Summ += Summ;                   Count=3 << Shift++;
        }
    }
} _PACK_ATTR;
static PPMD_TLS SEE2_CONTEXT SEE2Cont[24][32], DummySEE2Cont;
struct PPM_CONTEXT {                 // Notes:
    BYTE NumStats, Flags;                   // 1. NumStats & NumMasked contain
    WORD SummFreq;                          //  number of symbols minus 1
    struct STATE {                          // 2. sizeof(WORD) > sizeof(BYTE)
        BYTE Symbol, Freq;                  // 3. contexts example:
        P32<PPM_CONTEXT> Successor;         // MaxOrder:
    } _PACK_ATTR;                           //  ABCD    context
    P32<STATE> Stats;                       //   BCD    suffix
    P32<PPM_CONTEXT> Suffix;
    inline void encodeBinSymbol(int symbol);//   BCDE   successor
    inline void   encodeSymbol1(int symbol);// other orders:
    inline void   encodeSymbol2(int symbol);//   BCD    context
    inline void           decodeBinSymbol(const BYTE *hs, DWORD &l_low, DWORD &l_code, DWORD &l_range, SUBRANGE &sr);//    CD    suffix
    inline void             decodeSymbol1(const BYTE *hs, DWORD &l_low, DWORD &l_code, DWORD &l_range, SUBRANGE &sr);//   BCDE   successor
    inline uint64_t         decodeSymbol2(const BYTE *hs, DWORD l_diff, DWORD l_range);//   BCDE   successor
    inline void           update1(STATE* p);
    inline void           update2(STATE* p);
    inline SEE2_CONTEXT*     makeEscFreq2(const BYTE *hs, SUBRANGE &sr);
    void                          rescale();
    void      refresh(int OldNU,BOOL Scale);
    PPM_CONTEXT*          cutOff(int Order);
    PPM_CONTEXT*  removeBinConts(int Order);
    STATE& oneState() const { return (STATE&) SummFreq; }
} _PACK_ATTR;
static PPMD_TLS PPM_CONTEXT* MaxContext;
#pragma pack()
typedef char ppmd_state_size_check[sizeof(PPM_CONTEXT::STATE)==6 ? 1 : -1];
typedef char ppmd_ctx_size_check[sizeof(PPM_CONTEXT)==12 ? 1 : -1];

static BYTE NS2BSIndx[256], QTable[260];    // constants
static PPMD_TLS PPM_CONTEXT::STATE* FoundState;      // found next state transition
static PPMD_TLS int  InitEsc, OrderFall, RunLength, InitRL, MaxOrder;
static PPMD_TLS BYTE CharMask[256], NumMasked, PrevSuccess, EscCount, PrintCount;
static PPMD_TLS WORD BinSumm[25][64];                // binary SEE-contexts
static PPMD_TLS MR_METHOD MRMethod;

inline void SWAP(PPM_CONTEXT::STATE& s1,PPM_CONTEXT::STATE& s2)
{
    WORD t1=(WORD&) s1;                     P32<PPM_CONTEXT> t2=s1.Successor;
    (WORD&) s1 = (WORD&) s2;                s1.Successor=s2.Successor;
    (WORD&) s2 = t1;                        s2.Successor=t2;
}
inline void StateCpy(PPM_CONTEXT::STATE& s1,const PPM_CONTEXT::STATE& s2)
{
    (WORD&) s1=(WORD&) s2;                  s1.Successor=s2.Successor;
}
struct PPMD_STARTUP { inline PPMD_STARTUP(); } PPMd_StartUp;
inline PPMD_STARTUP::PPMD_STARTUP()         // constants initialization
{
    UINT i, k, m, Step;
    for (i=0,k=1;i < N1     ;i++,k += 1)    Indx2Units[i]=k;
    for (k++;i < N1+N2      ;i++,k += 2)    Indx2Units[i]=k;
    for (k++;i < N1+N2+N3   ;i++,k += 3)    Indx2Units[i]=k;
    for (k++;i < N1+N2+N3+N4;i++,k += 4)    Indx2Units[i]=k;
    for (k=i=0;k < 128;k++) {
        i += (Indx2Units[i] < k+1);         Units2Indx[k]=i;
    }
    NS2BSIndx[0]=2*0;                       NS2BSIndx[1]=2*1;
    memset(NS2BSIndx+2,2*2,9);              memset(NS2BSIndx+11,2*3,256-11);
    for (i=0;i < UP_FREQ;i++)               QTable[i]=i;
    for (m=i=UP_FREQ, k=Step=1;i < 260;i++) {
        QTable[i]=m;
        if ( !--k ) { k = ++Step;           m++; }
    }
    (DWORD&) DummySEE2Cont=PPMdSignature;
}
static void _STDCALL StartModelRare(int _MaxOrder,MR_METHOD _MRMethod)
{
    UINT i, k, m;
    // DummySEE2Cont may live in thread-local storage: (re)initialize it in the model's own thread
    (DWORD&) DummySEE2Cont=PPMdSignature;
    memset(CharMask,0,sizeof(CharMask));    EscCount=PrintCount=1;
    if (_MaxOrder < 2) {                    // we are in solid mode
        OrderFall=MaxOrder;
        for (PPM_CONTEXT* pc=MaxContext;pc->Suffix != NULL;pc=pc->Suffix)
                OrderFall--;
        return;
    }
    OrderFall=MaxOrder=_MaxOrder;           MRMethod=_MRMethod;
    InitSubAllocator();
    RunLength=InitRL=-((MaxOrder < 12)?MaxOrder:12)-1;
    MaxContext = (PPM_CONTEXT*) AllocContext();
    MaxContext->Suffix=NULL;
    MaxContext->SummFreq=(MaxContext->NumStats=255)+2;
    MaxContext->Stats = (PPM_CONTEXT::STATE*) AllocUnits(256/2);
    for (PrevSuccess=i=0;i < 256;i++) {
        MaxContext->Stats[i].Symbol=i;      MaxContext->Stats[i].Freq=1;
        MaxContext->Stats[i].Successor=NULL;
    }
static const WORD InitBinEsc[]={0x3CDD,0x1F3F,0x59BF,0x48F3,0x64A1,0x5ABC,0x6632,0x6051};
    for (i=m=0;m < 25;m++) {
        while (QTable[i] == m)              i++;
        for (k=0;k < 8;k++)
                BinSumm[m][k]=BIN_SCALE-InitBinEsc[k]/(i+1);
        for (k=8;k < 64;k += 8)
                memcpy(BinSumm[m]+k,BinSumm[m],8*sizeof(WORD));
    }
    for (i=m=0;m < 24;m++) {
        while (QTable[i+3] == m+3)          i++;
        SEE2Cont[m][0].init(2*i+5);
        for (k=1;k < 32;k++)                SEE2Cont[m][k]=SEE2Cont[m][0];
    }
}
void PPM_CONTEXT::refresh(int OldNU,BOOL Scale)
{
    int i=NumStats, EscFreq;
    STATE* p = Stats = (STATE*) ShrinkUnits(Stats,OldNU,(i+2) >> 1);
    Flags=(Flags & (0x10+0x04*Scale))+0x08*(p->Symbol >= 0x40);
    EscFreq=SummFreq-p->Freq;
    SummFreq = (p->Freq=(p->Freq+Scale) >> Scale);
    do {
        EscFreq -= (++p)->Freq;
        SummFreq += (p->Freq=(p->Freq+Scale) >> Scale);
        Flags |= 0x08*(p->Symbol >= 0x40);
    } while ( --i );
    SummFreq += (EscFreq=(EscFreq+Scale) >> Scale);
}
#define P_CALL(F) ( PrefetchData(p->Successor), \
                    p->Successor=p->Successor->F(Order+1))
PPM_CONTEXT* PPM_CONTEXT::cutOff(int Order)
{
    int i, tmp;
    STATE* p;
    if ( !NumStats ) {
        if ((BYTE*) (p=&oneState())->Successor.get() >= UnitsStart) {
            if (Order < MaxOrder)           P_CALL(cutOff);
            else                            p->Successor=NULL;
            if (!p->Successor && Order > O_BOUND)
                    goto REMOVE;
            return this;
        } else {
REMOVE:     SpecialFreeUnit(this);          return NULL;
        }
    }
    PrefetchData(Stats);
    Stats = (STATE*) MoveUnitsUp(Stats,tmp=(NumStats+2) >> 1);
    for (p=Stats+(i=NumStats);p >= Stats;p--)
            if ((BYTE*) p->Successor.get() < UnitsStart) {
                p->Successor=NULL;          SWAP(*p,Stats[i--]);
            } else if (Order < MaxOrder)    P_CALL(cutOff);
            else                            p->Successor=NULL;
    if (i != NumStats && Order) {
        NumStats=i;                         p=Stats;
        if (i < 0) { FreeUnits(p,tmp);      goto REMOVE; }
        else if (i == 0) {
            Flags=(Flags & 0x10)+0x08*(p->Symbol >= 0x40);
            StateCpy(oneState(),*p);        FreeUnits(p,tmp);
            oneState().Freq=(oneState().Freq+11) >> 3;
        } else                              refresh(tmp,SummFreq > 16*i);
    }
    return this;
}
PPM_CONTEXT* PPM_CONTEXT::removeBinConts(int Order)
{
    STATE* p;
    if ( !NumStats ) {
        p=&oneState();
        if ((BYTE*) p->Successor.get() >= UnitsStart && Order < MaxOrder)
                P_CALL(removeBinConts);
        else                                p->Successor=NULL;
        if (!p->Successor && (!Suffix->NumStats || Suffix->Flags == 0xFF)) {
            FreeUnits(this,1);              return NULL;
        } else                              return this;
    }
    PrefetchData(Stats);
    for (p=Stats+NumStats;p >= Stats;p--)
            if ((BYTE*) p->Successor.get() >= UnitsStart && Order < MaxOrder)
                    P_CALL(removeBinConts);
            else                            p->Successor=NULL;
    return this;
}
static void RestoreModelRare(PPM_CONTEXT* pc1,PPM_CONTEXT* MinContext,
        PPM_CONTEXT* FSuccessor)
{
    PPM_CONTEXT* pc;
    PPM_CONTEXT::STATE* p;
    ppmd_note_ptext();
    pText=HeapStart;
    for (pc=MaxContext;pc != pc1;pc=pc->Suffix)
            if (--(pc->NumStats) == 0) {
                pc->Flags=(pc->Flags & 0x10)+0x08*(pc->Stats->Symbol >= 0x40);
                p=pc->Stats;                StateCpy(pc->oneState(),*p);
                SpecialFreeUnit(p);
                pc->oneState().Freq=(pc->oneState().Freq+11) >> 3;
            } else
                    pc->refresh((pc->NumStats+3) >> 1,FALSE);
    for ( ;pc != MinContext;pc=pc->Suffix)
            if ( !pc->NumStats )
                    pc->oneState().Freq -= pc->oneState().Freq >> 1;
            else if ((pc->SummFreq += 4) > 128+4*pc->NumStats)
                    pc->refresh((pc->NumStats+2) >> 1,TRUE);
    if (MRMethod > MRM_FREEZE) {
        MaxContext=FSuccessor;              GlueCount += !(BList[1].Stamp & 1);
    } else if (MRMethod == MRM_FREEZE) {
        while ( MaxContext->Suffix )        MaxContext=MaxContext->Suffix;
        MaxContext->removeBinConts(0);      MRMethod=MR_METHOD(MRMethod+1);
        GlueCount=0;                        OrderFall=MaxOrder;
    } else if (MRMethod == MRM_RESTART || GetUsedMemory() < (SubAllocatorSize >> 1)) {
        StartModelRare(MaxOrder,MRMethod);
        EscCount=0;                         PrintCount=0xFF;
    } else {
        while ( MaxContext->Suffix )        MaxContext=MaxContext->Suffix;
        do {
            MaxContext->cutOff(0);          ExpandTextArea();
        } while (GetUsedMemory() > 3*(SubAllocatorSize >> 2));
        GlueCount=0;                        OrderFall=MaxOrder;
    }
}
static PPM_CONTEXT* _FASTCALL CreateSuccessors(BOOL Skip,PPM_CONTEXT::STATE* p,
        PPM_CONTEXT* pc,const BYTE *hs);
static PPM_CONTEXT* _FASTCALL ReduceOrder(PPM_CONTEXT::STATE* p,PPM_CONTEXT* pc,const BYTE *hs)
{
    PPM_CONTEXT::STATE* p1,  * ps[MAX_O], ** pps=ps;
    PPM_CONTEXT* pc1=pc, * UpBranch = (PPM_CONTEXT*) pText;
    BYTE tmp, sym=FoundState->Symbol;
    *pps++ = FoundState;                    FoundState->Successor=UpBranch;
    OrderFall++;
    if ( p ) { pc=pc->Suffix.get_at(hs);    goto LOOP_ENTRY; }
    for ( ; ; ) {
        if ( !pc->Suffix.o ) {
            if (MRMethod > MRM_FREEZE) {
FROZEN:         do { (*--pps)->Successor = pc; } while (pps != ps);
                ppmd_note_ptext();
                pText=HeapStart+1;          OrderFall=1;
            }
            return pc;
        }
        pc=pc->Suffix.get_nn(hs);
        if (pc->Suffix.o) PrefetchData(pc->Suffix.get_nn(hs));
        if ( pc->NumStats ) {
            if ((p=pc->Stats.get_nn(hs))->Symbol != sym)
                    do { tmp=p[1].Symbol;   p++; } while (tmp != sym);
            tmp=2*(p->Freq < MAX_FREQ-9);
            p->Freq += tmp;                 pc->SummFreq += tmp;
        } else { p=&(pc->oneState());       p->Freq += (p->Freq < 32); }
LOOP_ENTRY:
        if ( p->Successor.o )               break;
        *pps++ = p;                         p->Successor=UpBranch;
        OrderFall++;
    }
    if (MRMethod > MRM_FREEZE) {
        pc = p->Successor.get_at(hs);       goto FROZEN;
    } else if (p->Successor.get_at(hs) <= UpBranch) {
        p1=FoundState;                      FoundState=p;
        p->Successor=CreateSuccessors(FALSE,NULL,pc,hs);
        FoundState=p1;
    }
    if (OrderFall == 1 && pc1 == MaxContext) {
        FoundState->Successor=p->Successor; ppmd_note_ptext(); pText--;
    }
    return p->Successor.get_at(hs);
}
void PPM_CONTEXT::rescale()
{
    UINT OldNU, Adder, EscFreq, i=NumStats;
    STATE tmp, * p1, * p;
    for (p=FoundState;p != Stats;p--)       SWAP(p[0],p[-1]);
    p->Freq += 4;                           SummFreq += 4;
    EscFreq=SummFreq-p->Freq;
    Adder=(OrderFall != 0 || MRMethod > MRM_FREEZE);
    SummFreq = (p->Freq=(p->Freq+Adder) >> 1);
    do {
        EscFreq -= (++p)->Freq;
        SummFreq += (p->Freq=(p->Freq+Adder) >> 1);
        if (p[0].Freq > p[-1].Freq) {
            StateCpy(tmp,*(p1=p));
            do StateCpy(p1[0],p1[-1]); while (tmp.Freq > (--p1)[-1].Freq);
            StateCpy(*p1,tmp);
        }
    } while ( --i );
    if (p->Freq == 0) {
        do { i++; } while ((--p)->Freq == 0);
        EscFreq += i;                       OldNU=(NumStats+2) >> 1;
        if ((NumStats -= i) == 0) {
            StateCpy(tmp,*Stats);
            tmp.Freq=(2*tmp.Freq+EscFreq-1)/EscFreq;
            if (tmp.Freq > MAX_FREQ/3)      tmp.Freq=MAX_FREQ/3;
            FreeUnits(Stats,OldNU);         StateCpy(oneState(),tmp);
            Flags=(Flags & 0x10)+0x08*(tmp.Symbol >= 0x40);
            FoundState=&oneState();         return;
        }
        Stats = (STATE*) ShrinkUnits(Stats,OldNU,(NumStats+2) >> 1);
        Flags &= ~0x08;                     i=NumStats;
        Flags |= 0x08*((p=Stats)->Symbol >= 0x40);
        do { Flags |= 0x08*((++p)->Symbol >= 0x40); } while ( --i );
    }
    SummFreq += (EscFreq -= (EscFreq >> 1));
    Flags |= 0x04;                          FoundState=Stats;
}
static PPM_CONTEXT* _FASTCALL CreateSuccessors(BOOL Skip,PPM_CONTEXT::STATE* p,
        PPM_CONTEXT* pc,const BYTE *hs)
{
    PPM_CONTEXT ct, * UpBranch=FoundState->Successor.get_at(hs);
    PrefetchData(UpBranch);
    PPM_CONTEXT::STATE* ps[MAX_O], ** pps=ps;
    UINT cf, s0;
    BYTE tmp, sym=FoundState->Symbol;
    if ( !Skip ) {
        *pps++ = FoundState;
        if ( !pc->Suffix.o )                goto NO_LOOP;
    }
    if ( p ) { pc=pc->Suffix.get_at(hs);    goto LOOP_ENTRY; }
    do {
        pc=pc->Suffix.get_nn(hs);
        if (pc->Suffix.o) PrefetchData(pc->Suffix.get_nn(hs));
        if ( pc->NumStats ) {
            if ((p=pc->Stats.get_nn(hs))->Symbol != sym)
                    do { tmp=p[1].Symbol;   p++; } while (tmp != sym);
            tmp=(p->Freq < MAX_FREQ-9);
            p->Freq += tmp;                 pc->SummFreq += tmp;
        } else {
            p=&(pc->oneState());
            p->Freq += (!pc->Suffix.get_nn(hs)->NumStats & (p->Freq < 24));
        }
LOOP_ENTRY:
        if (p->Successor.get_at(hs) != UpBranch) {
            pc=p->Successor.get_at(hs);
            PrefetchData(pc);
            break;
        }
        *pps++ = p;
    } while ( pc->Suffix.o );
NO_LOOP:
    if (pps == ps)                          return pc;
    if (pc->NumStats) PrefetchData(pc->Stats.get_nn(hs));
    ct.NumStats=0;                          ct.Flags=0x10*(sym >= 0x40);
    ct.oneState().Symbol=sym=*(BYTE*) UpBranch;
    ct.oneState().Successor=(PPM_CONTEXT*) (((BYTE*) UpBranch)+1);
    ct.Flags |= 0x08*(sym >= 0x40);
    if ( pc->NumStats ) {
        if ((p=pc->Stats.get_nn(hs))->Symbol != sym)
                do { tmp=p[1].Symbol;       p++; } while (tmp != sym);
        s0=pc->SummFreq-pc->NumStats-(cf=p->Freq-1);
        ct.oneState().Freq=1+((2*cf <= s0)?(5*cf > s0):((cf+2*s0-3)/s0));
    } else
            ct.oneState().Freq=pc->oneState().Freq;
    do {
        PPM_CONTEXT* pc1 = (PPM_CONTEXT*) AllocContext();
        if ( !pc1 )                         return NULL;
        ((DWORD*) pc1)[0] = ((DWORD*) &ct)[0];
        ((DWORD*) pc1)[1] = ((DWORD*) &ct)[1];
        pc1->Suffix=pc;                     (*--pps)->Successor=pc=pc1;
    } while (pps != ps);
    return pc;
}
static inline void UpdateModel(PPM_CONTEXT* MinContext, const BYTE *hs)
{
    PPM_CONTEXT::STATE* p=NULL;
    PPM_CONTEXT* Successor, * FSuccessor, * pc, * pc1=MaxContext;
    UINT ns1, ns, cf, sf, s0, FFreq=FoundState->Freq;
    BYTE Flag, sym, FSymbol=FoundState->Symbol;
    FSuccessor=FoundState->Successor.get_at(hs); pc=MinContext->Suffix.get_at(hs);
    if (FFreq < MAX_FREQ/4 && pc) {
        if ( pc->NumStats ) {
            if ((p=pc->Stats.get_nn(hs))->Symbol != FSymbol) {
                do { sym=p[1].Symbol;       p++; } while (sym != FSymbol);
                if (p[0].Freq >= p[-1].Freq) {
                    SWAP(p[0],p[-1]);       p--;
                }
            }
            cf=2*(p->Freq < MAX_FREQ-9);
            p->Freq += cf;                  pc->SummFreq += cf;
        } else { p=&(pc->oneState());       p->Freq += (p->Freq < 32); }
    }
    if (!OrderFall && FSuccessor) {
        FoundState->Successor=CreateSuccessors(TRUE,p,MinContext,hs);
        if ( !FoundState->Successor.o )     goto RESTART_MODEL;
        MaxContext=FoundState->Successor.get_nn(hs); return;
    }
    *pText++ = FSymbol;                     Successor = (PPM_CONTEXT*) pText;
    ppmd_note_ptext();
    if (pText >= UnitsStart)                goto RESTART_MODEL;
    if ( FSuccessor ) {
        if ((BYTE*) FSuccessor < UnitsStart)
                FSuccessor=CreateSuccessors(FALSE,p,MinContext,hs);
    } else
                FSuccessor=ReduceOrder(p,MinContext,hs);
    if ( !FSuccessor )                      goto RESTART_MODEL;
    if ( !--OrderFall ) {
        Successor=FSuccessor;               ppmd_note_ptext(); pText -= (MaxContext != MinContext);
    } else if (MRMethod > MRM_FREEZE) {
        Successor=FSuccessor;               ppmd_note_ptext(); pText=HeapStart;
        OrderFall=0;
    }
    s0=MinContext->SummFreq-(ns=MinContext->NumStats)-FFreq;
    for (Flag=0x08*(FSymbol >= 0x40);pc1 != MinContext;pc1=pc1->Suffix.get_at(hs)) {
        if ((ns1=pc1->NumStats) != 0) {
            if ((ns1 & 1) != 0) {
                p=(PPM_CONTEXT::STATE*) ExpandUnits(pc1->Stats.get_nn(hs),(ns1+1) >> 1);
                if ( !p )                   goto RESTART_MODEL;
                pc1->Stats=p;
            }
            pc1->SummFreq += (3*ns1+1 < ns);
        } else {
            p=(PPM_CONTEXT::STATE*) AllocUnits(1);
            if ( !p )                       goto RESTART_MODEL;
            StateCpy(*p,pc1->oneState());   pc1->Stats=p;
            if (p->Freq < MAX_FREQ/4-1)     p->Freq += p->Freq;
            else                            p->Freq  = MAX_FREQ-4;
            pc1->SummFreq=p->Freq+InitEsc+(ns > 2);
        }
        cf=2*FFreq*(pc1->SummFreq+6);       sf=s0+pc1->SummFreq;
        if (cf < 6*sf) {
            cf=1+(cf > sf)+(cf >= 4*sf);
            pc1->SummFreq += 4;
        } else {
            cf=4+(cf > 9*sf)+(cf > 12*sf)+(cf > 15*sf);
            pc1->SummFreq += cf;
        }
        p=pc1->Stats.get_nn(hs)+(++pc1->NumStats); p->Successor=Successor;
        p->Symbol = FSymbol;                p->Freq = cf;
        pc1->Flags |= Flag;
    }
    MaxContext=FSuccessor;
    if (FSuccessor->NumStats) PrefetchData(FSuccessor->Stats.get_nn(hs));
    return;
RESTART_MODEL:
    RestoreModelRare(pc1,MinContext,FSuccessor);
}
// Tabulated escapes for exponential symbol distribution
static const BYTE ExpEscape[16]={ 25,14, 9, 7, 5, 5, 4, 4, 4, 3, 3, 3, 2, 2, 2, 2 };
#define GET_MEAN(SUMM,SHIFT,ROUND) ((SUMM+(1 << (SHIFT-ROUND))) >> (SHIFT))
inline void PPM_CONTEXT::encodeBinSymbol(int symbol)
{
    BYTE indx=NS2BSIndx[Suffix->NumStats]+PrevSuccess+Flags;
    STATE& rs=oneState();
    WORD& bs=BinSumm[QTable[rs.Freq-1]][indx+((RunLength >> 26) & 0x20)];
    if (rs.Symbol == symbol) {
        FoundState=&rs;                     rs.Freq += (rs.Freq < 196);
        SubRange.LowCount=0;                SubRange.HighCount=bs;
        bs += INTERVAL-GET_MEAN(bs,PERIOD_BITS,2);
        PrevSuccess=1;                      RunLength++;
    } else {
        SubRange.LowCount=bs;               bs -= GET_MEAN(bs,PERIOD_BITS,2);
        SubRange.HighCount=BIN_SCALE;       InitEsc=ExpEscape[bs >> 10];
        CharMask[rs.Symbol]=EscCount;
        NumMasked=PrevSuccess=0;            FoundState=NULL;
    }
}
inline void PPM_CONTEXT::decodeBinSymbol(const BYTE *hs, DWORD &l_low, DWORD &l_code, DWORD &l_range, SUBRANGE &sr)
{
    BYTE indx=NS2BSIndx[Suffix.get_nn(hs)->NumStats]+PrevSuccess+Flags;
    STATE& rs=oneState();
    WORD& bs=BinSumm[QTable[rs.Freq-1]][indx+((RunLength >> 26) & 0x20)];
    l_range >>= TOT_BITS;
    if (__builtin_expect((uint64_t)(l_code - l_low) < (uint64_t)bs * (uint64_t)l_range, 1)) {
        PrefetchData(rs.Successor.get_at(hs));
        FoundState=&rs;                     rs.Freq += (rs.Freq < 196);
        sr.LowCount=0;                      sr.HighCount=bs;
        bs += INTERVAL-GET_MEAN(bs,PERIOD_BITS,2);
        PrevSuccess=1;                      RunLength++;
    } else {
        sr.LowCount=bs;                     bs -= GET_MEAN(bs,PERIOD_BITS,2);
        sr.HighCount=BIN_SCALE;             InitEsc=ExpEscape[bs >> 10];
        CharMask[rs.Symbol]=EscCount;
        NumMasked=PrevSuccess=0;            FoundState=NULL;
    }
}
inline void PPM_CONTEXT::update1(STATE* p)
{
    (FoundState=p)->Freq += 4;              SummFreq += 4;
    if (p[0].Freq > p[-1].Freq) {
        SWAP(p[0],p[-1]);                   FoundState=--p;
        if (p->Freq > MAX_FREQ)             rescale();
    }
}
inline void PPM_CONTEXT::encodeSymbol1(int symbol)
{
    UINT LoCnt, i=Stats->Symbol;
    STATE* p=Stats;                         SubRange.scale=SummFreq;
    if (i == symbol) {
        PrevSuccess=(2*(SubRange.HighCount=p->Freq) >= SubRange.scale);
        (FoundState=p)->Freq += 4;          SummFreq += 4;
        RunLength += PrevSuccess;
        if (p->Freq > MAX_FREQ)             rescale();
        SubRange.LowCount=0;                return;
    }
    LoCnt=p->Freq;
    i=NumStats;                             PrevSuccess=0;
    while ((++p)->Symbol != symbol) {
        LoCnt += p->Freq;
        if (--i == 0) {
            if ( Suffix )                   PrefetchData(Suffix);
            SubRange.LowCount=LoCnt;        CharMask[p->Symbol]=EscCount;
            i=NumMasked=NumStats;           FoundState=NULL;
            do { CharMask[(--p)->Symbol]=EscCount; } while ( --i );
            SubRange.HighCount=SubRange.scale;
            return;
        }
    }
    SubRange.HighCount=(SubRange.LowCount=LoCnt)+p->Freq;
    update1(p);
}
inline void PPM_CONTEXT::decodeSymbol1(const BYTE *hs, DWORD &l_low, DWORD &l_code, DWORD &l_range, SUBRANGE &sr)
{
    STATE* p=Stats.get_nn(hs);
    PrefetchData(p);
    if (Suffix.o) PrefetchData(Suffix.get_nn(hs));
    sr.scale=SummFreq;
    ariScaleRangeDownLocal(l_range, sr.scale);
    UINT i, count, HiCnt=p->Freq;
    UINT n = l_code - l_low;
    if (__builtin_expect((uint64_t)n < (uint64_t)HiCnt * (uint64_t)l_range, 1)) {
        PrefetchData(p->Successor.get_at(hs));
        PrevSuccess=(2*(sr.HighCount=HiCnt) >= sr.scale);
        (FoundState=p)->Freq=(HiCnt += 4);  SummFreq += 4;
        RunLength += PrevSuccess;
        if (__builtin_expect(HiCnt > MAX_FREQ, 0)) rescale();
        sr.LowCount=0;                      return;
    }
    count = n / l_range;
    i=NumStats;                             PrevSuccess=0;
    while ((HiCnt += (++p)->Freq) <= count)
        if (__builtin_expect(--i == 0, 0)) {
            sr.LowCount=HiCnt;              CharMask[p->Symbol]=EscCount;
            i=NumMasked=NumStats;           FoundState=NULL;
            do { CharMask[(--p)->Symbol]=EscCount; } while ( --i );
            sr.HighCount=sr.scale;
            return;
        }
    PrefetchData(p->Successor.get_at(hs));
    sr.LowCount=(sr.HighCount=HiCnt)-p->Freq;
    update1(p);
}
inline void PPM_CONTEXT::update2(STATE* p)
{
    (FoundState=p)->Freq += 4;              SummFreq += 4;
    if (p->Freq > MAX_FREQ)                 rescale();
    EscCount++;                             RunLength=InitRL;
}
inline SEE2_CONTEXT* PPM_CONTEXT::makeEscFreq2(const BYTE *hs, SUBRANGE &sr)
{
    BYTE* pb=(BYTE*) Stats.get_nn(hs);      UINT t=2*NumStats;
    PrefetchData(pb);                       PrefetchData(pb+t);
    PrefetchData(pb += 2*t);                PrefetchData(pb+t);
    SEE2_CONTEXT* psee2c;
    if (NumStats != 0xFF) {
        t=Suffix.get_nn(hs)->NumStats;
        psee2c=SEE2Cont[QTable[NumStats+2]-3]+(SummFreq > 11*(NumStats+1));
        psee2c += 2*(2*NumStats < t+NumMasked)+Flags;
        sr.scale=psee2c->getMean();
    } else {
        psee2c=&DummySEE2Cont;              sr.scale=1;
    }
    return psee2c;
}
inline void PPM_CONTEXT::encodeSymbol2(int symbol)
{
    // PERF (bit-exact): single branchless pass over all states with no unpredictable branches.
    //  - Sum of frequencies of NON-masked symbols (mask = CharMask[s] == EscCount),
    //    cumulative position of encoded symbol (never masked: not present in any higher context);
    //  - CharMask[s]=EscCount written for all states: identical to original in the "escape"
    //    case (masks all unmasked symbols; already-masked symbols already equal EscCount),
    //    and harmless in the "found" case because update2() increments EscCount (EscCount only
    //    grows until ClearMask; a stale value can never match a future EscCount).
    SEE2_CONTEXT* psee2c=makeEscFreq2(HeapStart, SubRange);
    STATE* p=Stats, * pe=p+NumStats, * found=NULL;
    UINT LoCnt=0, Tot=0;
    const BYTE Esc=EscCount;
    do {
        UINT Sym=p->Symbol;
        UINT f=p->Freq & (UINT)-(UINT)(CharMask[Sym] != Esc);
        if (Sym == (UINT)symbol) { found=p; LoCnt=Tot; }
        Tot += f;
        CharMask[Sym]=Esc;
    } while (++p <= pe);
    if ( !found ) {
        SubRange.HighCount=(SubRange.scale += (SubRange.LowCount=Tot));
        psee2c->Summ += SubRange.scale;         NumMasked = NumStats;
        return;
    }
    SubRange.LowCount=LoCnt;                SubRange.HighCount=LoCnt+found->Freq;
    SubRange.scale += Tot;
    psee2c->update();                       update2(found);
}
inline uint64_t PPM_CONTEXT::decodeSymbol2(const BYTE *hs, DWORD l_diff, DWORD l_range)
{
    SUBRANGE sr;
    if (Suffix.o) PrefetchData(Suffix.get_nn(hs));
    SEE2_CONTEXT* psee2c=makeEscFreq2(hs, sr);
    STATE* p=Stats.get_nn(hs), * pe=p+NumStats;
    const BYTE Esc=EscCount;
    UINT Tot=0, Tot1=0, Tot2=0, Tot3=0, extra=0;
    #define PPMD_MASKED_FREQ(q) ((q)->Freq & (UINT)-(UINT)(CharMask[(q)->Symbol] != Esc))
    if (__builtin_expect(pe - p >= 63, 0)) {
        UINT Tot4=0, Tot5=0, Tot6=0, Tot7=0;
        for (; p+7 <= pe; p += 8) {
            Tot  += PPMD_MASKED_FREQ(p);    Tot1 += PPMD_MASKED_FREQ(p+1);
            Tot2 += PPMD_MASKED_FREQ(p+2);  Tot3 += PPMD_MASKED_FREQ(p+3);
            Tot4 += PPMD_MASKED_FREQ(p+4);  Tot5 += PPMD_MASKED_FREQ(p+5);
            Tot6 += PPMD_MASKED_FREQ(p+6);  Tot7 += PPMD_MASKED_FREQ(p+7);
        }
        extra = (Tot4 + Tot5) + (Tot6 + Tot7);
    }
    for (; p+3 <= pe; p += 4) {
        Tot  += PPMD_MASKED_FREQ(p);        Tot1 += PPMD_MASKED_FREQ(p+1);
        Tot2 += PPMD_MASKED_FREQ(p+2);      Tot3 += PPMD_MASKED_FREQ(p+3);
    }
    for (; p <= pe; p++)  Tot += PPMD_MASKED_FREQ(p);
    Tot += Tot1 + Tot2 + Tot3 + extra;
    sr.scale += Tot;
    ariScaleRangeDownLocal(l_range, sr.scale);
    DWORD low_add, new_range;
    UINT count = l_diff / l_range;
    if (count < Tot) {
        UINT HiCnt=0;
        p=Stats.get_nn(hs);
        for (;;) {
            HiCnt += PPMD_MASKED_FREQ(p);
            if (HiCnt > count) break;
            p++;
        }
        PrefetchData(p->Successor.get_at(hs));
        DWORD freq = p->Freq;
        low_add = l_range * (HiCnt - freq);
        new_range = l_range * freq;
        psee2c->update();                   update2(p);
    } else {
        low_add = l_range * Tot;
        new_range = l_range * (sr.scale - Tot);
        NumMasked = NumStats;
        p=Stats.get_nn(hs);
        do { CharMask[p->Symbol]=Esc; } while (++p <= pe);
        psee2c->Summ += sr.scale;
    }
    #undef PPMD_MASKED_FREQ
    return ((uint64_t)new_range << 32) | (uint64_t)low_add;
}
inline void ClearMask(_PPMD_FILE* EncodedFile,_PPMD_FILE* DecodedFile)
{
    EscCount=1;                             memset(CharMask,0,sizeof(CharMask));
    if (++PrintCount == 0)                  PrintInfo(DecodedFile,EncodedFile);
}
void _STDCALL EncodeFile(_PPMD_FILE* EncodedFile,_PPMD_FILE* DecodedFile,
                            int MaxOrder,MR_METHOD MRMethod)
{
    ariInitEncoder();                       StartModelRare(MaxOrder,MRMethod);
    const BYTE * const hs = HeapStart;
    for (PPM_CONTEXT* MinContext; ; ) {
        BYTE ns=(MinContext=MaxContext)->NumStats;
        int c = _PPMD_E_GETC(DecodedFile);
        if ( ns ) {
            MinContext->encodeSymbol1(c);   ariEncodeSymbol();
        } else {
            MinContext->encodeBinSymbol(c); ariShiftEncodeSymbol(TOT_BITS);
        }
        while ( !FoundState ) {
            ARI_ENC_NORMALIZE(EncodedFile);
            do {
                OrderFall++;                MinContext=MinContext->Suffix.get_at(hs);
                if ( !MinContext )          goto STOP_ENCODING;
            } while (MinContext->NumStats == NumMasked);
            MinContext->encodeSymbol2(c);   ariEncodeSymbol();
        }
        if (!OrderFall && (BYTE*) FoundState->Successor.get_at(hs) >= UnitsStart)
                PrefetchData(MaxContext=FoundState->Successor.get_nn(hs));
        else {
            UpdateModel(MinContext, hs);    PrefetchData(MaxContext);
            if (EscCount == 0)              ClearMask(EncodedFile,DecodedFile);
        }
        ARI_ENC_NORMALIZE(EncodedFile);
        if( _PPMD_ERROR_CODE(DecodedFile)<0 || _PPMD_ERROR_CODE(EncodedFile)<0 ) return;
    }
STOP_ENCODING:
    ARI_FLUSH_ENCODER(EncodedFile);         PrintInfo(DecodedFile,EncodedFile);
}
void _STDCALL DecodeFile(_PPMD_FILE* DecodedFile,_PPMD_FILE* EncodedFile,
                            int MaxOrder,MR_METHOD MRMethod)
{
    DWORD l_low = 0, l_code = 0, l_range = (DWORD)-1;
    for (UINT i = 0; i < 4; i++)
        l_code = (l_code << 8) | _PPMD_D_GETC(EncodedFile);
    SUBRANGE sr;
    StartModelRare(MaxOrder,MRMethod);
    const BYTE * const hs = HeapStart;
    PPM_CONTEXT* MinContext=MaxContext;
    for (BYTE ns=MinContext->NumStats; ; ) {
        if ( ns )
            MinContext->decodeSymbol1(hs, l_low, l_code, l_range, sr);
        else
            MinContext->decodeBinSymbol(hs, l_low, l_code, l_range, sr);
        ariRemoveSubrangeLocal(l_low, l_range, sr);
        while ( !FoundState ) {
            ariDecNormalizeLocal(l_low, l_code, l_range, EncodedFile);
            do {
                OrderFall++;
                if (MinContext->Suffix.o)   PrefetchData(MinContext->Suffix.get_nn(hs));
                MinContext=MinContext->Suffix.get_at(hs);
                if ( !MinContext )          goto STOP_DECODING;
            } while (MinContext->NumStats == NumMasked);
            uint64_t r2 = MinContext->decodeSymbol2(hs, l_code - l_low, l_range);
            l_low += (DWORD)r2;
            l_range = (DWORD)(r2 >> 32);
        }
        BYTE sym_out = FoundState->Symbol;
        PPM_CONTEXT *succ = FoundState->Successor.get_at(hs);
        if (!OrderFall && (BYTE*) succ >= UnitsStart) {
            PrefetchData(succ);
            _PPMD_D_PUTC(sym_out, DecodedFile);
            ariDecNormalizeLocal(l_low, l_code, l_range, EncodedFile);
            MaxContext = succ;
            if (MaxContext->NumStats) PrefetchData(MaxContext->Stats.get_nn(hs));
        } else {
            if (MinContext->Suffix.o) PrefetchData(MinContext->Suffix.get_nn(hs));
            PrefetchData(succ);
            _PPMD_D_PUTC(sym_out, DecodedFile);
            ariDecNormalizeLocal(l_low, l_code, l_range, EncodedFile);
            UpdateModel(MinContext, hs);
            PrefetchData(MaxContext);
            if (EscCount == 0) ClearMask(EncodedFile, DecodedFile);
        }
        ns=(MinContext=MaxContext)->NumStats;
        if( _PPMD_ERROR_CODE(DecodedFile)<0 || _PPMD_ERROR_CODE(EncodedFile)<0 ) return;
    }
STOP_DECODING:
    PrintInfo(DecodedFile,EncodedFile);
}
