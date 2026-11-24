/****************************************************************************
 *  This file is part of PPMd project                                       *
 *  Written and distributed to public domain by Dmitry Shkarin 1997,        *
 *  1999-2001                                                               *
 *  Contents: memory allocation routines                                    *
 ****************************************************************************/

#include <stdint.h>

enum { UNIT_SIZE=12, N1=4, N2=4, N3=4, N4=(128+3-1*N1-2*N2-3*N3)/4,
        N_INDEXES=N1+N2+N3+N4 };

static PPMD_TLS BYTE* HeapStart;

#if defined(__linux__) && defined(__x86_64__)
#include <sys/mman.h>
#ifndef MAP_32BIT
#define MAP_32BIT 0x40
#endif
#define PPMD_HEAP_MMAP 1
#endif

#pragma pack(1)
template<class T>
struct P32 {
    DWORD o;
    T* get() const { return o ? (T*)(HeapStart + (o - 1)) : (T*)0; }
    inline T* get_at(const BYTE *hs) const { return o ? (T*)(hs + (o - 1)) : (T*)0; }
    inline T* get_nn(const BYTE *hs) const { return (T*)(hs + (o - 1)); }
    T* operator->() const { return get(); }
    operator T*() const { return get(); }
    T& operator[](int i) const { return get()[i]; }
    P32& operator=(T* p) {
        o = p ? (DWORD)((BYTE*)p - HeapStart) + 1 : 0;
        return *this;
    }
} _PACK_ATTR;
#pragma pack()

#pragma pack(1)
struct BLK_NODE {
    DWORD Stamp;
    P32<BLK_NODE> next;
    BOOL   avail()      const { return (next.o != 0); }
    void    link(BLK_NODE* p) { p->next=next; next=p; }
    void  unlink()            { next=next->next; }
    void* remove()            {
        BLK_NODE* p=next;                   unlink();
        Stamp--;                            return p;
    }
    inline void insert(void* pv,int NU);
};
static PPMD_TLS BLK_NODE BList[N_INDEXES];
struct MEM_BLK: public BLK_NODE {
    DWORD NU;
} _PACK_ATTR;
#pragma pack()

typedef char ppmd_p32_size_check[sizeof(P32<BLK_NODE>)==4 ? 1 : -1];
typedef char ppmd_mem_blk_size_check[sizeof(MEM_BLK)==UNIT_SIZE ? 1 : -1];
typedef char ppmd_blk_node_size_check[sizeof(BLK_NODE)==8 ? 1 : -1];

static BYTE Indx2Units[N_INDEXES], Units2Indx[128];
static PPMD_TLS DWORD GlueCount, SubAllocatorSize=0;
/* Bytes actually reserved for HeapStart. May be larger than SubAllocatorSize when a
   thread reuses its arena for a smaller model. SubAllocatorSize stays the logical
   model size (it changes the restart point and must match the encoder). */
static PPMD_TLS UINT HeapCapacity=0;
#ifdef PPMD_HEAP_MMAP
static PPMD_TLS int HeapViaMmap=0;
#endif
static PPMD_TLS BYTE * pText, * UnitsStart, * LoUnit, * HiUnit;
/* Bytes actually written by the current arena, across model restarts.
   Low* is the text area and the units grown from UnitsStart.
   High* is the contexts grown down from the top.
   The virgin gap between them stays zero and is not rewritten. */
static PPMD_TLS BYTE *LowLo, *LowHi, *HighLo, *HighHi;

inline void PrefetchData(const void* Addr)
{
#if defined(__GNUC__) || defined(__clang__)
    __builtin_prefetch(Addr, 0, 3);
#elif defined(_MSC_VER)
    _mm_prefetch((const char*)Addr, _MM_HINT_T0);
#endif
}
static inline void ppmd_note_low(const void *p, size_t n)
{
    if (!p || !n) return;
    BYTE *b = (BYTE*)p, *e = b + n;
    if (!LowLo || b < LowLo) LowLo = b;
    if (e > LowHi)           LowHi = e;
}
static inline void ppmd_note_high(const void *p, size_t n)
{
    if (!p || !n) return;
    BYTE *b = (BYTE*)p, *e = b + n;
    if (!HighLo || b < HighLo) HighLo = b;
    if (e > HighHi)            HighHi = e;
}
static inline void ppmd_note_ptext()
{
    if (HeapStart && pText > HeapStart)
        ppmd_note_low(HeapStart, (size_t)(pText - HeapStart));
}
inline void BLK_NODE::insert(void* pv,int NU) {
    MEM_BLK* p=(MEM_BLK*) pv;               link(p);
    p->Stamp=~(DWORD)0;                     p->NU=NU;
    Stamp++;
}
inline UINT U2B(UINT NU) { return 8*NU+4*NU; }
inline void SplitBlock(void* pv,UINT OldIndx,UINT NewIndx)
{
    UINT i, k, UDiff=Indx2Units[OldIndx]-Indx2Units[NewIndx];
    BYTE* p=((BYTE*) pv)+U2B(Indx2Units[NewIndx]);
    if (Indx2Units[i=Units2Indx[UDiff-1]] != UDiff) {
        k=Indx2Units[--i];                  BList[i].insert(p,k);
        p += U2B(k);                        UDiff -= k;
    }
    BList[Units2Indx[UDiff-1]].insert(p,UDiff);
}
DWORD _STDCALL GetUsedMemory()
{
    DWORD i, RetVal=SubAllocatorSize-(HiUnit-LoUnit)-(UnitsStart-pText);
    for (i=0;i < N_INDEXES;i++)
            RetVal -= UNIT_SIZE*Indx2Units[i]*BList[i].Stamp;
    return RetVal;
}
void _STDCALL StopSubAllocator() {
    if ( HeapCapacity ) {
#ifdef PPMD_HEAP_MMAP
        if (HeapViaMmap)
            ::munmap(HeapStart, HeapCapacity);
        else
#endif
            BigFree(HeapStart);
        HeapCapacity=0;                     SubAllocatorSize=0;
        HeapStart=NULL;
        LowLo=LowHi=HighLo=HighHi=NULL;
#ifdef PPMD_HEAP_MMAP
        HeapViaMmap=0;
#endif
    }
}

/* Freed when the worker thread exits, including threads that never reach an
   explicit ppmd_release_thread_arenas() call. */
struct PPMDThreadArenaGuard {
    int alive;
    PPMDThreadArenaGuard() : alive(1) {}
    ~PPMDThreadArenaGuard() { StopSubAllocator(); }
};
static thread_local PPMDThreadArenaGuard ppmd_thread_arena_guard;

/* Zero only the ranges the previous block actually wrote. A fresh mapping is
   already zero, and a mid-stream MRM_RESTART must keep the leftovers of the
   current block, so this runs once per block and not from InitSubAllocator.
   The watermarks are cleared after the wipe. The next block records only what
   it writes, so one block that filled the arena cannot force every later block
   on this thread to memset the whole heap. */
static void ZeroSpan(BYTE *b, BYTE *e)
{
    if (!HeapStart || !b || !e || e <= b) return;
    BYTE *lim = HeapStart + HeapCapacity;
    if (b < HeapStart) b = HeapStart;
    if (e > lim)       e = lim;
    if (e > b)         memset(b, 0, (size_t)(e - b));
}
static void WipeReusedArena()
{
    ppmd_note_ptext();
    if (LowHi && HighLo && HighLo <= LowHi) {
        BYTE *b = (LowLo && LowLo < HighLo) ? LowLo : HighLo;
        BYTE *e = (HighHi && HighHi > LowHi) ? HighHi : LowHi;
        ZeroSpan(b, e);
    } else {
        ZeroSpan(LowLo, LowHi);
        ZeroSpan(HighLo, HighHi);
    }
    LowLo = LowHi = HighLo = HighHi = NULL;
}

BOOL _STDCALL StartSubAllocator(UINT t)
{
    (void) ppmd_thread_arena_guard.alive;
    if (HeapStart != NULL && t != 0 && HeapCapacity >= t) {
        WipeReusedArena();
        SubAllocatorSize = t;
        return TRUE;
    }
    StopSubAllocator();
    int os_zeroed = 0;
    size_t cap_t = t;
#ifdef PPMD_HEAP_MMAP
    if (t) {
        const size_t page_sz = 4096u;
        const size_t align = (t >= (2u << 20)) ? (2u << 20) : page_sz;
        size_t map_t = ((size_t)t + page_sz - 1) & ~(page_sz - 1);
        size_t req = map_t + (align > page_sz ? align : 0);
        void *raw = ::mmap(NULL, req, PROT_READ|PROT_WRITE,
                           MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
        if (raw == MAP_FAILED)        HeapStart = NULL;
        else {
            uintptr_t addr = (uintptr_t)raw;
            uintptr_t aligned = (addr + align - 1) & ~(uintptr_t)(align - 1);
            size_t prefix = (size_t)(aligned - addr);
            size_t suffix = req - prefix - map_t;
            if (prefix) ::munmap((void *)addr, prefix);
            if (suffix) ::munmap((void *)(aligned + map_t), suffix);
            HeapStart = (BYTE *)aligned;
            cap_t = map_t;
            HeapViaMmap = 1;
            os_zeroed = 1;
        }
    }
#endif
    if (HeapStart == NULL) {
        HeapStart = (BYTE*) BigAlloc(t);
#ifdef FREEARC_WIN
        if (HeapStart)                      os_zeroed = 1;   /* VirtualAlloc */
#endif
    }
    if (HeapStart == NULL)                  return FALSE;
    if (!os_zeroed && t)                    memset(HeapStart, 0, t);
    HeapCapacity=(UINT)cap_t;               SubAllocatorSize=t;
    LowLo=LowHi=HighLo=HighHi=NULL;
#if defined(PPMD_HEAP_MMAP) && defined(MADV_HUGEPAGE)
    /* 256 MB in 4 KB pages overflows the L2 TLB. 2 MB pages bring it down to 128 entries. */
    if (HeapViaMmap)
        ::madvise(HeapStart, HeapCapacity, MADV_HUGEPAGE);
#endif
    return TRUE;
}
static inline void InitSubAllocator()
{
    ppmd_note_ptext();
    memset(BList,0,sizeof(BList));
    HiUnit=(pText=HeapStart)+SubAllocatorSize;
    UINT Diff=UNIT_SIZE*(SubAllocatorSize/8/UNIT_SIZE*7);
    LoUnit=UnitsStart=HiUnit-Diff;          GlueCount=0;
}
static void GlueFreeBlocks()
{
    UINT i, k, sz;
    MEM_BLK s0, * p, * p0, * p1;
    if (LoUnit != HiUnit) {
        ppmd_note_low(LoUnit, 1);
        *LoUnit=0;
    }
    for (i=0, (p0=&s0)->next=(BLK_NODE*)NULL;i < N_INDEXES;i++)
            while ( BList[i].avail() ) {
                p=(MEM_BLK*) BList[i].remove();
                if ( !p->NU )               continue;
                while ((p1=p+p->NU)->Stamp == ~(DWORD)0) {
                    p->NU += p1->NU;        p1->NU=0;
                }
                p0->link(p);                p0=p;
            }
    while ( s0.avail() ) {
        p=(MEM_BLK*) s0.remove();           sz=p->NU;
        if ( !sz )                          continue;
        for ( ;sz > 128;sz -= 128, p += 128)
                BList[N_INDEXES-1].insert(p,128);
        if (Indx2Units[i=Units2Indx[sz-1]] != sz) {
            k=sz-Indx2Units[--i];           BList[k-1].insert(p+(sz-k),k);
        }
        BList[i].insert(p,Indx2Units[i]);
    }
    GlueCount=1 << 13;
}
static void* _STDCALL AllocUnitsRare(UINT indx)
{
    UINT i=indx;
    if ( !GlueCount ) {
        GlueFreeBlocks();
        if ( BList[i].avail() )             return BList[i].remove();
    }
    do {
        if (++i == N_INDEXES) {
            GlueCount--;                    i=U2B(Indx2Units[indx]);
            if (UnitsStart-pText > i) {
                UnitsStart -= i;
                ppmd_note_low(UnitsStart, i);
                return UnitsStart;
            }
            return NULL;
        }
    } while ( !BList[i].avail() );
    void* RetVal=BList[i].remove();         SplitBlock(RetVal,i,indx);
    return RetVal;
}
inline void* AllocUnits(UINT NU)
{
    UINT indx=Units2Indx[NU-1];
    if ( BList[indx].avail() )              return BList[indx].remove();
    void* RetVal=LoUnit;                    LoUnit += U2B(Indx2Units[indx]);
    if (LoUnit <= HiUnit) {
        ppmd_note_low(RetVal, (size_t)(LoUnit - (BYTE*)RetVal));
        return RetVal;
    }
    LoUnit -= U2B(Indx2Units[indx]);        return AllocUnitsRare(indx);
}
inline void* AllocContext()
{
    if (HiUnit != LoUnit) {
        HiUnit -= UNIT_SIZE;
        ppmd_note_high(HiUnit, UNIT_SIZE);
        return HiUnit;
    }
    else if ( BList->avail() )              return BList->remove();
    else                                    return AllocUnitsRare(0);
}
inline void UnitsCpy(void* Dest,void* Src,UINT NU)
{
    DWORD* p1=(DWORD*) Dest, * p2=(DWORD*) Src;
    do {
        p1[0]=p2[0];                        p1[1]=p2[1];
        p1[2]=p2[2];
        p1 += 3;                            p2 += 3;
    } while ( --NU );
}
inline void* ExpandUnits(void* OldPtr,UINT OldNU)
{
    UINT i0=Units2Indx[OldNU-1], i1=Units2Indx[OldNU-1+1];
    if (i0 == i1)                           return OldPtr;
    void* ptr=AllocUnits(OldNU+1);
    if ( ptr ) {
        UnitsCpy(ptr,OldPtr,OldNU);         BList[i0].insert(OldPtr,OldNU);
    }
    return ptr;
}
inline void* ShrinkUnits(void* OldPtr,UINT OldNU,UINT NewNU)
{
    UINT i0=Units2Indx[OldNU-1], i1=Units2Indx[NewNU-1];
    if (i0 == i1)                           return OldPtr;
    if ( BList[i1].avail() ) {
        void* ptr=BList[i1].remove();       UnitsCpy(ptr,OldPtr,NewNU);
        BList[i0].insert(OldPtr,Indx2Units[i0]);
        return ptr;
    } else {
        SplitBlock(OldPtr,i0,i1);           return OldPtr;
    }
}
inline void FreeUnits(void* ptr,UINT NU) {
    UINT indx=Units2Indx[NU-1];
    BList[indx].insert(ptr,Indx2Units[indx]);
}
inline void SpecialFreeUnit(void* ptr)
{
    if ((BYTE*) ptr != UnitsStart)          BList->insert(ptr,1);
    else { *(DWORD*) ptr=~(DWORD)0;         UnitsStart += UNIT_SIZE; }
}
inline void* MoveUnitsUp(void* OldPtr,UINT NU)
{
    UINT indx=Units2Indx[NU-1];
    if ((BYTE*) OldPtr > UnitsStart+16*1024 || (BLK_NODE*) OldPtr > (BLK_NODE*)BList[indx].next)
            return OldPtr;
    void* ptr=BList[indx].remove();
    UnitsCpy(ptr,OldPtr,NU);                NU=Indx2Units[indx];
    if ((BYTE*) OldPtr != UnitsStart)       BList[indx].insert(OldPtr,NU);
    else                                    UnitsStart += U2B(NU);
    return ptr;
}
static inline void ExpandTextArea()
{
    BLK_NODE* p;
    UINT Count[N_INDEXES];                  memset(Count,0,sizeof(Count));
    while ((p=(BLK_NODE*) UnitsStart)->Stamp == ~(DWORD)0) {
        MEM_BLK* pm=(MEM_BLK*) p;           UnitsStart=(BYTE*) (pm+pm->NU);
        Count[Units2Indx[pm->NU-1]]++;      pm->Stamp=0;
    }
    for (UINT i=0;i < N_INDEXES;i++)
        for (p=BList+i;Count[i] != 0;p=p->next)
            while ( !p->next->Stamp ) {
                p->unlink();                BList[i].Stamp--;
                if ( !--Count[i] )          break;
            }
}
