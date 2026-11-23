/*
 * LzFind_x86_64.S - hand-written x86-64 match finder kernels for the
 * bit-exact (2009-era) LZMA encoder in Compression/LZMA2/C/LzFind.c.
 *
 * GNU as, Intel syntax, C preprocessor. Builds unchanged for
 *   - Linux x86-64   (ELF, System V AMD64 ABI)
 *   - Windows x64    (COFF, Microsoft x64 ABI, MinGW-w64) - with SEH unwind info
 * Every routine takes ONE pointer to an argument block (see LzFind.c), so the
 * only ABI difference is which register holds that pointer (FA_ARG1) and the
 * set of callee-saved registers (FA_SAVE_NONVOL). Only xmm0-5/ymm0-5 are used.
 *
 * Routines (all results are bit-identical to the C reference code):
 *
 *   UInt32 *lzf_bt_getmatches(const LzfBtArgs *a)   == GetMatchesSpec1()
 *   void    lzf_bt_skip      (const LzfBtArgs *a)   == SkipMatchesSpec()
 *   UInt32 *lzf_ht_getmatches(const LzfHtArgs *a)   == Ht_GetMatchesSpec()
 *
 * Binary tree walk (BT2/BT3/BT4): every hot variable lives in a register for
 * the whole walk (cur, son, ptr0, ptr1, len0, len1, pos, cyclicBufferPos,
 * cyclicBufferSize, lenLimit, pb, len, curMatch, pair). The only memory
 * scalars are the cut counter and the rarely-touched output state
 * (distances/maxLen), kept in a 16-byte frame. The left/right descent - a
 * ~50% unpredictable branch in C - is done with cmov (store target, next
 * pointer, len0/len1 and next curMatch are all selected without branching).
 * The son[] pair of the candidate is prefetched as soon as its index is known.
 *
 * Hash table rows (HT4): tags of up to 32 entries are classified at once (SSE2);
 * curMatch/delta are derived without the tag mask (delta = newEntry - entry,
 * exact modulo 2^32 because entry = match|tag and newEntry = pos|tag), and the
 * row shift (memmove in C) is an inlined backward 16-byte copy.
 *
 * Match extension uses the LZEXT macro (fa_x86_64.h): 8-byte probe, AVX2
 * 32-byte blocks (runtime-detected), SSE2 16-byte blocks, overlapped tail.
 */
#include "../../asm/fa_x86_64.h"

/* LzfBtArgs layout (must match LzFind.c) */
#define BT_CUR      0
#define BT_SON      8
#define BT_DIST     16
#define BT_LENLIM   24
#define BT_CURMATCH 28
#define BT_POS      32
#define BT_CBP      36
#define BT_CBSIZE   40
#define BT_CUT      44
#define BT_MAXLEN   48

/* stack frame of the BT routines */
#define F_DIST      0
#define F_MAXLEN    8
#define F_CUT       12
#define F_SIZE      24

/*
 * Register map of the BT walk
 *   rdi cur        rsi son         r8  ptr0       r9  ptr1
 *   r10d len0      r11d len1       r12d pos       r13d cyclicBufferPos
 *   r14d cyclicBufferSize          r15 pair (&son[2*index])
 *   ebp lenLimit   rbx pb          edx len        ecx curMatch      eax scratch
 */
        .macro BT_PROLOGUE
        FA_SAVE_NONVOL
        sub     rsp, F_SIZE
        FA_SEH(.seh_stackalloc F_SIZE)
        FA_SEH(.seh_endprologue)
        mov     rax, FA_ARG1
        mov     rsi, qword ptr [rax + BT_SON]
        mov     rdx, qword ptr [rax + BT_DIST]
        mov     qword ptr [rsp + F_DIST], rdx
        mov     ebp, dword ptr [rax + BT_LENLIM]
        mov     ecx, dword ptr [rax + BT_CURMATCH]
        mov     r12d, dword ptr [rax + BT_POS]
        mov     r13d, dword ptr [rax + BT_CBP]
        mov     r14d, dword ptr [rax + BT_CBSIZE]
        mov     edx, dword ptr [rax + BT_CUT]
        mov     dword ptr [rsp + F_CUT], edx
        mov     edx, dword ptr [rax + BT_MAXLEN]
        mov     dword ptr [rsp + F_MAXLEN], edx
        mov     rdi, qword ptr [rax + BT_CUR]
        lea     r9, [rsi + r13*8]               /* ptr1 = son + 2*cbp     */
        lea     r8, [r9 + 4]                    /* ptr0 = son + 2*cbp + 1 */
        xor     r10d, r10d
        xor     r11d, r11d
        .endm

        .macro BT_EPILOGUE
        mov     rax, qword ptr [rsp + F_DIST]
        add     rsp, F_SIZE
        FA_RESTORE_NONVOL
        ret
        .endm

/* Loop head shared by both BT routines: cut/delta checks, pair, pb, len */
        .macro BT_STEP empty
        sub     dword ptr [rsp + F_CUT], 1      /* if (cutValue-- == 0) */
        jb      \empty
        mov     eax, r12d
        sub     eax, ecx                        /* delta = pos - curMatch */
        cmp     eax, r14d
        jae     \empty                          /* delta >= cyclicBufferSize */
        mov     r15d, r13d
        sub     r15d, eax                       /* cbp - delta, CF = delta > cbp */
        lea     edx, [r15 + r14]
        cmovb   r15d, edx                       /* + cyclicBufferSize if wrapped */
        lea     r15, [rsi + r15*8]              /* pair = son + 2*index */
        prefetcht0 byte ptr [r15]
        mov     rbx, rdi
        sub     rbx, rax                        /* pb = cur - delta */
        mov     edx, r10d
        cmp     edx, r11d
        cmova   edx, r11d                       /* len = min(len0, len1) */
        .endm

/* Branchless descent: if (pb[len] < cur[len]) go right (ptr1) else left (ptr0) */
        .macro BT_DESCEND loop
        movzx   eax, byte ptr [rbx + rdx]
        cmp     al, byte ptr [rdi + rdx]        /* CF = pb[len] < cur[len] */
        mov     rax, r8
        cmovb   rax, r9
        mov     dword ptr [rax], ecx            /* *ptr1 or *ptr0 = curMatch */
        lea     rax, [r15 + 4]
        cmovb   r9, rax                         /* ptr1 = pair + 1 */
        cmovae  r8, r15                         /* ptr0 = pair     */
        cmovb   r11d, edx                       /* len1 = len      */
        cmovae  r10d, edx                       /* len0 = len      */
        mov     ecx, dword ptr [r15]            /* curMatch = *ptr0 ... */
        cmovb   ecx, dword ptr [r15 + 4]        /*        ... or *ptr1  */
        jmp     \loop
        .endm

/* ------------------------------------------------------------------------- */
/* UInt32 *lzf_bt_getmatches(const LzfBtArgs *a)                             */
/* ------------------------------------------------------------------------- */
FA_FUNC(lzf_bt_getmatches)
        BT_PROLOGUE
        .p2align 4
.Lbt_loop:
        BT_STEP .Lbt_empty
        movzx   eax, byte ptr [rbx + rdx]
        cmp     al, byte ptr [rdi + rdx]
        jne     .Lbt_descend                    /* pb[len] != cur[len] */
        inc     edx
        cmp     edx, ebp
        je      .Lbt_max                        /* ++len == lenLimit */
        movzx   eax, byte ptr [rbx + rdx]
        cmp     al, byte ptr [rdi + rdx]
        jne     .Lbt_max
        inc     edx
        LZEXT   rbx, rdi, edx, rdx, ebp, rbp    /* len = LzExt(pb, cur, len+1, lenLimit) */
.Lbt_max:
        cmp     edx, dword ptr [rsp + F_MAXLEN]
        jbe     .Lbt_descend
        mov     dword ptr [rsp + F_MAXLEN], edx /* maxLen = len */
        mov     rax, qword ptr [rsp + F_DIST]
        mov     dword ptr [rax], edx            /* *distances++ = len */
        not     ecx
        add     ecx, r12d                       /* ecx = pos - curMatch - 1 = delta - 1 */
        mov     dword ptr [rax + 4], ecx        /* *distances++ = delta - 1 */
        sub     ecx, r12d
        not     ecx                             /* restore curMatch */
        add     rax, 8
        mov     qword ptr [rsp + F_DIST], rax
        cmp     edx, ebp
        je      .Lbt_full
.Lbt_descend:
        BT_DESCEND .Lbt_loop
.Lbt_full:                                      /* len == lenLimit */
        mov     eax, dword ptr [r15]
        mov     dword ptr [r9], eax             /* *ptr1 = pair[0] */
        mov     eax, dword ptr [r15 + 4]
        mov     dword ptr [r8], eax             /* *ptr0 = pair[1] */
        BT_EPILOGUE
.Lbt_empty:
        mov     dword ptr [r8], 0               /* *ptr0 = *ptr1 = kEmptyHashValue */
        mov     dword ptr [r9], 0
        BT_EPILOGUE
FA_ENDFUNC(lzf_bt_getmatches)

/* ------------------------------------------------------------------------- */
/* void lzf_bt_skip(const LzfBtArgs *a)                                      */
/* ------------------------------------------------------------------------- */
FA_FUNC(lzf_bt_skip)
        BT_PROLOGUE
        .p2align 4
.Lsk_loop:
        BT_STEP .Lsk_empty
        movzx   eax, byte ptr [rbx + rdx]
        cmp     al, byte ptr [rdi + rdx]
        jne     .Lsk_descend
        inc     edx
        LZEXT   rbx, rdi, edx, rdx, ebp, rbp    /* len = LzExt(pb, cur, len+1, lenLimit) */
        cmp     edx, ebp
        je      .Lsk_full
.Lsk_descend:
        BT_DESCEND .Lsk_loop
.Lsk_full:
        mov     eax, dword ptr [r15]
        mov     dword ptr [r9], eax
        mov     eax, dword ptr [r15 + 4]
        mov     dword ptr [r8], eax
        BT_EPILOGUE
.Lsk_empty:
        mov     dword ptr [r8], 0
        mov     dword ptr [r9], 0
        BT_EPILOGUE
FA_ENDFUNC(lzf_bt_skip)

/* ------------------------------------------------------------------------- */
/* UInt32 *lzf_ht_getmatches(const LzfHtArgs *a)                             */
/* ------------------------------------------------------------------------- */
/* LzfHtArgs layout (must match LzFind.c) */
#define HT_ROW      0
#define HT_CUR      8
#define HT_DIST     16
#define HT_N        24
#define HT_TAG      28
#define HT_TAGMASK  32
#define HT_NEWENTRY 36
#define HT_CBSIZE   40
#define HT_LENLIM   44
#define HT_MAXLEN   48
/*
 * Register map
 *   rsi row        rdi cur         r8 distances   r9d n (cutValue)
 *   r10d newEntry  r11d cyclicBufferSize          r12d lenLimit   r13d maxLen
 *   r14d base (first entry of the current 32-entry chunk)  r15d *(UInt32*)cur
 *   ebx tag-match bits of the chunk   ecx i   rdx pb   ebp len   eax scratch
 *   xmm4 = tagMask x4, xmm5 = tag x4
 */
FA_FUNC(lzf_ht_getmatches)
        FA_SAVE_NONVOL
        FA_SEH(.seh_endprologue)
        mov     rax, FA_ARG1
        mov     rsi, qword ptr [rax + HT_ROW]
        mov     r8,  qword ptr [rax + HT_DIST]
        mov     r9d, dword ptr [rax + HT_N]
        movd    xmm5, dword ptr [rax + HT_TAG]
        pshufd  xmm5, xmm5, 0
        movd    xmm4, dword ptr [rax + HT_TAGMASK]
        pshufd  xmm4, xmm4, 0
        mov     r10d, dword ptr [rax + HT_NEWENTRY]
        mov     r11d, dword ptr [rax + HT_CBSIZE]
        mov     r12d, dword ptr [rax + HT_LENLIM]
        mov     r13d, dword ptr [rax + HT_MAXLEN]
        mov     rdi, qword ptr [rax + HT_CUR]
        mov     r15d, dword ptr [rdi]
        xor     r14d, r14d

/* --- tag-match mask of the next chunk of up to 32 entries -> ebx (bit k = entry base+k).
   Like the C code, the whole chunk is classified first (one data-dependent loop over
   the set bits instead of one branch per small block). Entries are never read past
   the end of the row. */
        .macro TAGCMP x
        pand    \x, xmm4
        pcmpeqd \x, xmm5                       /* (entry & tagMask) == tag */
        .endm
        .macro GROUP16 p                        /* eax = 16 bits for entries p[0..15] */
        movdqu  xmm0, xmmword ptr [\p]
        movdqu  xmm1, xmmword ptr [\p + 16]
        movdqu  xmm2, xmmword ptr [\p + 32]
        movdqu  xmm3, xmmword ptr [\p + 48]
        TAGCMP  xmm0
        TAGCMP  xmm1
        TAGCMP  xmm2
        TAGCMP  xmm3
        packssdw xmm0, xmm1
        packssdw xmm2, xmm3
        packsswb xmm0, xmm2
        pmovmskb eax, xmm0
        .endm
.Lht_chunk:
        mov     eax, r9d
        sub     eax, r14d                       /* entries left */
        jbe     .Lht_nobreak
        lea     rdx, [rsi + r14*4]
        cmp     eax, 32
        jb      .Lht_part
        GROUP16 rdx
        mov     ebx, eax
        GROUP16 rdx + 64
        shl     eax, 16
        or      ebx, eax
        jz      .Lht_next
        jmp     .Lht_bitloop
.Lht_part:                                      /* 1..31 entries: 16 / 8 / 4 / 1..3 groups */
        cmp     eax, 8
        je      .Lht_exact8                     /* the common mc8 row (-m4): one step */
        mov     ebp, eax
        xor     ebx, ebx
        xor     ecx, ecx                        /* bit position of the next group */
        cmp     ebp, 16
        jb      1f
        GROUP16 rdx
        mov     ebx, eax
        add     rdx, 64
        mov     ecx, 16
        sub     ebp, 16
1:      cmp     ebp, 8
        jb      2f
        movdqu  xmm0, xmmword ptr [rdx]
        movdqu  xmm1, xmmword ptr [rdx + 16]
        TAGCMP  xmm0
        TAGCMP  xmm1
        packssdw xmm0, xmm1
        packsswb xmm0, xmm0
        pmovmskb eax, xmm0
        and     eax, 0xFF
        shl     eax, cl
        or      ebx, eax
        add     rdx, 32
        add     ecx, 8
        sub     ebp, 8
2:      cmp     ebp, 4
        jb      3f
        movdqu  xmm0, xmmword ptr [rdx]
        TAGCMP  xmm0
        movmskps eax, xmm0
        shl     eax, cl
        or      ebx, eax
        add     rdx, 16
        add     ecx, 4
        sub     ebp, 4
3:      test    ebp, ebp
        jz      .Lht_have
        movd    xmm0, dword ptr [rdx]           /* 1..3 last entries */
        cmp     ebp, 2
        jb      4f
        movd    xmm1, dword ptr [rdx + 4]
        punpckldq xmm0, xmm1
        cmp     ebp, 3
        jb      4f
        movd    xmm1, dword ptr [rdx + 8]
        punpcklqdq xmm0, xmm1
4:      TAGCMP  xmm0
        movmskps eax, xmm0
        lea     rdx, [rip + .Lht_tailmask]
        movzx   edx, byte ptr [rdx + rbp]
        and     eax, edx                        /* drop the zero-filled lanes */
        shl     eax, cl
        or      ebx, eax
        jmp     .Lht_have
.Lht_exact8:
        movdqu  xmm0, xmmword ptr [rdx]
        movdqu  xmm1, xmmword ptr [rdx + 16]
        TAGCMP  xmm0
        TAGCMP  xmm1
        movmskps ebx, xmm0
        movmskps eax, xmm1
        shl     eax, 4
        or      ebx, eax
.Lht_have:
        test    ebx, ebx
        jz      .Lht_next

/* --- candidates of the chunk, in increasing row order (same order as the C code) --- */
        .p2align 4
.Lht_bitloop:
        tzcnt   ecx, ebx
        lea     eax, [rbx - 1]
        and     ebx, eax                        /* bits &= bits - 1 */
        add     ecx, r14d                       /* i */
        mov     eax, r10d
        sub     eax, dword ptr [rsi + rcx*4]    /* delta = pos - getMatch(row[i]) */
        cmp     eax, r11d
        jae     .Lht_shift                      /* delta >= cyclicBufferSize: last = i */
        mov     rdx, rdi
        sub     rdx, rax                        /* pb = cur - delta */
        movzx   ebp, byte ptr [rdx + r13]
        cmp     bpl, byte ptr [rdi + r13]       /* pb[maxLen] == cur[maxLen] */
        jne     .Lht_nextbit
        cmp     r15d, dword ptr [rdx]           /* *(UInt32*)pb == *(UInt32*)cur */
        jne     .Lht_nextbit
        mov     ebp, 4
        LZEXT   rdx, rdi, ebp, rbp, r12d, r12   /* len = LzExt(pb, cur, 4, lenLimit) */
        cmp     ebp, r12d
        je      .Lht_full
        cmp     ebp, r13d
        jbe     .Lht_nextbit
        mov     r13d, ebp                       /* maxLen = len */
        mov     dword ptr [r8], ebp
        mov     rax, rdi
        sub     rax, rdx
        dec     eax
        mov     dword ptr [r8 + 4], eax
        add     r8, 8
.Lht_nextbit:
        test    ebx, ebx
        jnz     .Lht_bitloop
.Lht_next:
        add     r14d, 32
        jmp     .Lht_chunk
.Lht_full:                                      /* len == lenLimit: emit and stop, last = i */
        mov     dword ptr [r8], ebp
        mov     rax, rdi
        sub     rax, rdx
        dec     eax
        mov     dword ptr [r8 + 4], eax
        add     r8, 8
        jmp     .Lht_shift
.Lht_nobreak:
        lea     ecx, [r9 - 1]                   /* last = n - 1 */

/* --- memmove(row+1, row, last*4); row[0] = newEntry --------------------------------
   All loads of a size class are issued before the stores (overlapping head/tail
   vectors), or - for long rows - chunks are copied top-down with the head block
   preloaded, so the overlapping 4-byte shift is always correct. */
.Lht_shift:
        shl     ecx, 2                          /* L = last*4 bytes */
        cmp     ecx, 16
        jb      .Lsh_small
        cmp     ecx, 32
        ja      .Lsh_gt32
        movdqu  xmm0, xmmword ptr [rsi]
        movdqu  xmm1, xmmword ptr [rsi + rcx - 16]
        movdqu  xmmword ptr [rsi + 4], xmm0
        movdqu  xmmword ptr [rsi + rcx - 12], xmm1
        jmp     .Lsh_done
.Lsh_gt32:
        cmp     ecx, 64
        ja      .Lsh_gt64
        movdqu  xmm0, xmmword ptr [rsi]
        movdqu  xmm1, xmmword ptr [rsi + 16]
        movdqu  xmm2, xmmword ptr [rsi + rcx - 32]
        movdqu  xmm3, xmmword ptr [rsi + rcx - 16]
        movdqu  xmmword ptr [rsi + 4], xmm0
        movdqu  xmmword ptr [rsi + 20], xmm1
        movdqu  xmmword ptr [rsi + rcx - 28], xmm2
        movdqu  xmmword ptr [rsi + rcx - 12], xmm3
        jmp     .Lsh_done
.Lsh_gt64:
        cmp     byte ptr [rip + fa_cpu_avx2], 0
        jne     .Lsh_avx
        movdqu  xmm4, xmmword ptr [rsi]         /* head [0,32) preloaded */
        movdqu  xmm5, xmmword ptr [rsi + 16]
        lea     eax, [rcx - 16]
1:      movdqu  xmm0, xmmword ptr [rsi + rax]
        movdqu  xmmword ptr [rsi + rax + 4], xmm0
        cmp     eax, 32
        jbe     2f
        sub     eax, 16
        jmp     1b
2:      movdqu  xmmword ptr [rsi + 4], xmm4
        movdqu  xmmword ptr [rsi + 20], xmm5
        jmp     .Lsh_done
.Lsh_avx:
        cmp     ecx, 128
        ja      .Lsh_avxloop
        vmovdqu ymm0, ymmword ptr [rsi]
        vmovdqu ymm1, ymmword ptr [rsi + 32]
        vmovdqu ymm2, ymmword ptr [rsi + rcx - 64]
        vmovdqu ymm3, ymmword ptr [rsi + rcx - 32]
        vmovdqu ymmword ptr [rsi + 4], ymm0
        vmovdqu ymmword ptr [rsi + 36], ymm1
        vmovdqu ymmword ptr [rsi + rcx - 60], ymm2
        vmovdqu ymmword ptr [rsi + rcx - 28], ymm3
        vzeroupper
        jmp     .Lsh_done
.Lsh_avxloop:
        vmovdqu ymm4, ymmword ptr [rsi]         /* head [0,64) preloaded */
        vmovdqu ymm5, ymmword ptr [rsi + 32]
        lea     eax, [rcx - 32]
1:      vmovdqu ymm0, ymmword ptr [rsi + rax]
        vmovdqu ymmword ptr [rsi + rax + 4], ymm0
        cmp     eax, 64
        jbe     2f
        sub     eax, 32
        jmp     1b
2:      vmovdqu ymmword ptr [rsi + 4], ymm4
        vmovdqu ymmword ptr [rsi + 36], ymm5
        vzeroupper
        jmp     .Lsh_done
.Lsh_small:                                     /* 0..3 entries */
        shr     ecx, 2
        jz      .Lsh_done
1:      dec     ecx
        mov     eax, dword ptr [rsi + rcx*4]
        mov     dword ptr [rsi + rcx*4 + 4], eax
        jnz     1b
.Lsh_done:
        mov     dword ptr [rsi], r10d           /* row[0] = newEntry */
        mov     rax, r8
        FA_RESTORE_NONVOL
        ret
FA_ENDFUNC(lzf_ht_getmatches)

#if defined(__ELF__)
        .section .rodata.lzf,"a",@progbits
#else
        .section .rdata,"dr"
#endif
.Lht_tailmask:
        .byte   0, 1, 3, 7

#if defined(__ELF__)
        .section .note.GNU-stack,"",@progbits
#endif
