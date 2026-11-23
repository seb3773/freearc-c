/*
 * LzFind25_x86_64.S - hand-written x86-64 binary-tree match finder kernels for the
 * LZMA SDK 25.01 encoder used in -nobitexact mode (Compression/LZMA2/sdk25/).
 *
 * GNU as, Intel syntax, C preprocessor. Builds unchanged for
 *   - Linux x86-64   (ELF, System V AMD64 ABI)
 *   - Windows x64    (COFF, Microsoft x64 ABI, MinGW-w64) - with SEH unwind info
 * Every routine takes ONE pointer to an argument block (see sdk25/LzFindAsm.h), so
 * the only ABI difference is the register holding that pointer (FA_ARG1) and the
 * set of callee-saved registers (FA_SAVE_NONVOL). Only xmm0-1/ymm0 are used
 * (volatile in both ABIs). All routines are leaf functions.
 *
 * Routines (results, distances[] and son[] contents are identical to the C code):
 *
 *   UInt32 *lzf25_bt_getmatches  (const Lzf25BtArgs *a) == GetMatchesSpec1()   sdk25/LzFind.c
 *   void    lzf25_bt_skip        (const Lzf25BtArgs *a) == SkipMatchesSpec()   sdk25/LzFind.c
 *   UInt32 *lzf25_bt_getmatches_n(const Lzf25NArgs  *a) == GetMatchesSpecN_2() sdk25/LzFindOpt.c
 *
 * Note the SDK 25.01 loop rules differ from the 2009 encoder (LzFind_x86_64.S):
 *   - single thread: the walk continues while (--cutValue && cmCheck < curMatch),
 *     cmCheck = pos - cyclicBufferSize (0 while pos < cyclicBufferSize);
 *   - batched (BT thread of LzFindMt): hash[] holds deltas, a per-position
 *     cbs = min(pos, cyclicBufferSize), son[] links are validated (return NULL on
 *     corrupted data) and the "long match" shortcut copies son[] pairs while the
 *     same delta keeps matching at full length.
 *
 * Speed-ups over the C code:
 *   - every hot variable of the tree walk lives in a register;
 *   - the ~50% unpredictable left/right descent is done with cmov (store target,
 *     next pointer, len0/len1 and the next node are selected without a branch);
 *   - the son[] pair of a node is prefetched as soon as its index is known;
 *   - batched mode: the root node and the first data byte of the tree of position
 *     pos+LZF25_PF_DIST are prefetched from the (already known) hash[] deltas, so the
 *     first, always-cold, miss of each walk overlaps the walk of the previous position;
 *   - match extension with the shared LZEXT macro (8-byte probe, AVX2 32-byte /
 *     SSE2 16-byte blocks with pmovmskb+tzcnt, overlapped 8-byte tail; never reads
 *     past lenLimit).
 */
#include "../../asm/fa_x86_64.h"

#ifndef LZF25_PF_DIST
#define LZF25_PF_DIST 2
#endif

/* Lzf25BtArgs layout (must match sdk25/LzFindAsm.h) */
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

/* stack frame of the single-thread routines */
#define F_DIST      0
#define F_MAXLEN    8
#define F_CUT       12
#define F_CMCHECK   16
#define F_SIZE      24

/*
 * Register map of the single-thread walk
 *   rdi cur        rsi son         r8  ptr0       r9  ptr1
 *   r10d len0      r11d len1       r12d pos       r13d cyclicBufferPos
 *   r14d cyclicBufferSize          r15 pair (&son[2*index])
 *   ebp lenLimit   rbx pb          edx len        ecx curMatch      eax scratch
 */
        .macro B25_PROLOGUE
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
        mov     edx, r12d
        xor     eax, eax
        sub     edx, r14d                       /* cmCheck = pos - cyclicBufferSize */
        cmovb   edx, eax                        /*         = 0 if pos < cyclicBufferSize */
        mov     dword ptr [rsp + F_CMCHECK], edx
        lea     r9, [rsi + r13*8]               /* ptr1 = son + 2*cbp     */
        lea     r8, [r9 + 4]                    /* ptr0 = son + 2*cbp + 1 */
        xor     r10d, r10d
        xor     r11d, r11d
        .endm

        .macro B25_EPILOGUE
        mov     rax, qword ptr [rsp + F_DIST]
        add     rsp, F_SIZE
        FA_RESTORE_NONVOL
        ret
        .endm

/* Node address, pb and len. In: ecx = curMatch. Out: r15 = pair, rbx = pb, edx = len */
        .macro B25_NODE
        mov     eax, r12d
        sub     eax, ecx                        /* delta = pos - curMatch */
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

/* Branchless descent: if (pb[len] < cur[len]) go right (ptr1) else left (ptr0).
   In: ecx = curMatch. Out: ecx = next curMatch */
        .macro B25_DESCEND
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
        mov     ecx, dword ptr [r15]            /* curMatch = pair[0] ... */
        cmovb   ecx, dword ptr [r15 + 4]        /*         ... or pair[1] */
        .endm

/* ------------------------------------------------------------------------- */
/* UInt32 *lzf25_bt_getmatches(const Lzf25BtArgs *a)    == GetMatchesSpec1() */
/* ------------------------------------------------------------------------- */
FA_FUNC(lzf25_bt_getmatches)
        B25_PROLOGUE
        cmp     ecx, dword ptr [rsp + F_CMCHECK]
        jbe     .Lg_empty                       /* if (!(cmCheck < curMatch)) */
        .p2align 4
.Lg_loop:
        B25_NODE
        movzx   eax, byte ptr [rbx + rdx]
        cmp     al, byte ptr [rdi + rdx]
        jne     .Lg_descend                     /* pb[len] != cur[len] */
        inc     edx
        cmp     edx, ebp
        je      .Lg_max                         /* ++len == lenLimit */
        movzx   eax, byte ptr [rbx + rdx]
        cmp     al, byte ptr [rdi + rdx]
        jne     .Lg_max
        inc     edx
        LZEXT   rbx, rdi, edx, rdx, ebp, rbp    /* len = LzExt(pb, cur, len+1, lenLimit) */
.Lg_max:
        cmp     edx, dword ptr [rsp + F_MAXLEN]
        jbe     .Lg_descend
        mov     dword ptr [rsp + F_MAXLEN], edx /* maxLen = len */
        mov     rax, qword ptr [rsp + F_DIST]
        mov     dword ptr [rax], edx            /* *d++ = len */
        not     ecx
        add     ecx, r12d                       /* ecx = pos - curMatch - 1 = delta - 1 */
        mov     dword ptr [rax + 4], ecx        /* *d++ = delta - 1 */
        sub     ecx, r12d
        not     ecx                             /* restore curMatch */
        add     rax, 8
        mov     qword ptr [rsp + F_DIST], rax
        cmp     edx, ebp
        je      .Lg_full
.Lg_descend:
        B25_DESCEND
        sub     dword ptr [rsp + F_CUT], 1      /* while (--cutValue ... */
        jz      .Lg_empty
        cmp     ecx, dword ptr [rsp + F_CMCHECK]
        ja      .Lg_loop                        /*        ... && cmCheck < curMatch) */
.Lg_empty:
        mov     dword ptr [r8], 0               /* *ptr0 = *ptr1 = kEmptyHashValue */
        mov     dword ptr [r9], 0
        B25_EPILOGUE
.Lg_full:                                       /* len == lenLimit */
        mov     eax, dword ptr [r15]
        mov     dword ptr [r9], eax             /* *ptr1 = pair[0] */
        mov     eax, dword ptr [r15 + 4]
        mov     dword ptr [r8], eax             /* *ptr0 = pair[1] */
        B25_EPILOGUE
FA_ENDFUNC(lzf25_bt_getmatches)

/* ------------------------------------------------------------------------- */
/* void lzf25_bt_skip(const Lzf25BtArgs *a)             == SkipMatchesSpec() */
/* ------------------------------------------------------------------------- */
FA_FUNC(lzf25_bt_skip)
        B25_PROLOGUE
        cmp     ecx, dword ptr [rsp + F_CMCHECK]
        jbe     .Ls_empty
        .p2align 4
.Ls_loop:
        B25_NODE
        movzx   eax, byte ptr [rbx + rdx]
        cmp     al, byte ptr [rdi + rdx]
        jne     .Ls_descend
        inc     edx
        LZEXT   rbx, rdi, edx, rdx, ebp, rbp    /* len = LzExt(pb, cur, len+1, lenLimit) */
        cmp     edx, ebp
        je      .Ls_full
.Ls_descend:
        B25_DESCEND
        sub     dword ptr [rsp + F_CUT], 1
        jz      .Ls_empty
        cmp     ecx, dword ptr [rsp + F_CMCHECK]
        ja      .Ls_loop
.Ls_empty:
        mov     dword ptr [r8], 0
        mov     dword ptr [r9], 0
        B25_EPILOGUE
.Ls_full:
        mov     eax, dword ptr [r15]
        mov     dword ptr [r9], eax
        mov     eax, dword ptr [r15 + 4]
        mov     dword ptr [r8], eax
        B25_EPILOGUE
FA_ENDFUNC(lzf25_bt_skip)

/* ------------------------------------------------------------------------- */
/* UInt32 *lzf25_bt_getmatches_n(const Lzf25NArgs *a)  == GetMatchesSpecN_2() */
/* ------------------------------------------------------------------------- */
/* Lzf25NArgs layout (must match sdk25/LzFindAsm.h) */
#define N_LENLIM    0       /* const Byte *: cur + lenLimit - 1 (as passed by LzFindMt.c) */
#define N_CUR       8
#define N_SON       16
#define N_D         24
#define N_HASH      32
#define N_LIMIT     40
#define N_SIZE      48
#define N_POS       56
#define N_CBP       64
#define N_MAXLEN    72
#define N_CUT       80
#define N_CBSIZE    84
#define N_POSRES    88

/* stack frame */
#define G_D         0       /* d (output cursor)           */
#define G_HASH      8       /* hash (input cursor)         */
#define G_END       16      /* size (end of hash[])        */
#define G_LIMIT     24      /* limit (end of d[])          */
#define G_DIST0     32      /* _distances of the position  */
#define G_POSRES    40
#define G_MAXLEN    48      /* maxLen of the position      */
#define G_CUT       52      /* cutValue counter            */
#define G_CUT0      56      /* _cutValue                   */
#define G_MAXLEN0   60      /* _maxLen                     */
#define G_CBS0      64      /* _cyclicBufferSize           */
#define G_SIZE      72

/*
 * Register map
 *   rdi cur        rsi son         r8  ptr0       r9  ptr1
 *   r10d len0      r11d len1       r12 pos        r13 cyclicBufferPos
 *   r14d cbs (per position: min(pos, cyclicBufferSize))
 *   r15 pair       ebp lenLimit (length, constant for the batch)
 *   rbx pb         edx len         ecx curMatch    eax scratch / delta
 */
FA_FUNC(lzf25_bt_getmatches_n)
        FA_SAVE_NONVOL
        sub     rsp, G_SIZE
        FA_SEH(.seh_stackalloc G_SIZE)
        FA_SEH(.seh_endprologue)
        mov     rax, FA_ARG1
        mov     rdx, qword ptr [rax + N_D]
        mov     qword ptr [rsp + G_D], rdx
        mov     rdx, qword ptr [rax + N_HASH]
        mov     qword ptr [rsp + G_HASH], rdx
        mov     rdx, qword ptr [rax + N_SIZE]
        mov     qword ptr [rsp + G_END], rdx
        mov     rdx, qword ptr [rax + N_LIMIT]
        mov     qword ptr [rsp + G_LIMIT], rdx
        mov     rdx, qword ptr [rax + N_POSRES]
        mov     qword ptr [rsp + G_POSRES], rdx
        mov     edx, dword ptr [rax + N_CUT]
        mov     dword ptr [rsp + G_CUT0], edx
        mov     edx, dword ptr [rax + N_MAXLEN]
        mov     dword ptr [rsp + G_MAXLEN0], edx
        mov     edx, dword ptr [rax + N_CBSIZE]
        mov     dword ptr [rsp + G_CBS0], edx
        mov     rsi, qword ptr [rax + N_SON]
        mov     r12, qword ptr [rax + N_POS]
        mov     r13, qword ptr [rax + N_CBP]
        mov     rdi, qword ptr [rax + N_CUR]
        mov     rbp, qword ptr [rax + N_LENLIM]
        sub     rbp, rdi
        inc     ebp                             /* lenLimit = (lenLimit + 1) - cur */

        .p2align 4
.Ln_pos:                                        /* ---- next position ---- */
        mov     rax, qword ptr [rsp + G_HASH]
        cmp     rax, qword ptr [rsp + G_END]
        je      .Ln_done                        /* if (hash == size) break */
        mov     ecx, dword ptr [rax]            /* delta = *hash++ */
        add     rax, 4
        mov     qword ptr [rsp + G_HASH], rax
        test    ecx, ecx
        jz      .Ln_fail                        /* if (delta == 0) return NULL */
        mov     r14d, dword ptr [rsp + G_CBS0]  /* cbs = _cyclicBufferSize */
        cmp     r12d, r14d
        jae     1f
        cmp     ecx, r12d
        ja      .Ln_fail                        /* if (delta > pos) return NULL */
        mov     r14d, r12d                      /* cbs = pos */
1:
#if LZF25_PF_DIST > 0
        /* lookahead: root node + first data byte of position pos + LZF25_PF_DIST */
        lea     rdx, [rax + 4*(LZF25_PF_DIST-1)]
        cmp     rdx, qword ptr [rsp + G_END]
        jae     2f
        mov     edx, dword ptr [rdx]
        lea     r15d, [r13 + LZF25_PF_DIST]
        sub     r15d, edx
        lea     ebx, [r15 + r14]
        cmovb   r15d, ebx
        prefetcht0 byte ptr [rsi + r15*8]
        mov     rbx, rdi
        sub     rbx, rdx
        prefetcht0 byte ptr [rbx + LZF25_PF_DIST]
2:
#endif
        lea     r9, [rsi + r13*8]               /* ptr1 = son + 2*cbp */
        cmp     ecx, r14d
        jae     .Ln_nomatch                     /* if (delta >= cbs) */
        lea     r8, [r9 + 4]                    /* ptr0 = son + 2*cbp + 1 */
        mov     rax, qword ptr [rsp + G_D]
        add     rax, 4
        mov     qword ptr [rsp + G_D], rax      /* _distances = ++d */
        mov     qword ptr [rsp + G_DIST0], rax
        mov     eax, dword ptr [rsp + G_CUT0]
        mov     dword ptr [rsp + G_CUT], eax
        mov     eax, dword ptr [rsp + G_MAXLEN0]
        mov     dword ptr [rsp + G_MAXLEN], eax
        xor     r10d, r10d
        xor     r11d, r11d
        mov     eax, ecx                        /* eax = delta */
        mov     ecx, r12d
        sub     ecx, eax                        /* curMatch = pos - delta */

        .p2align 4
.Ln_tree:                                       /* ---- tree node, eax = delta ---- */
        mov     r15d, r13d
        sub     r15d, eax                       /* cbp - delta, CF = delta > cbp */
        lea     edx, [r15 + r14]
        cmovb   r15d, edx                       /* + cbs if wrapped */
        lea     r15, [rsi + r15*8]              /* pair */
        prefetcht0 byte ptr [r15]
        mov     rbx, rdi
        sub     rbx, rax                        /* pb = cur - delta */
        mov     edx, r10d
        cmp     edx, r11d
        cmova   edx, r11d                       /* len = min(len0, len1) */
        movzx   eax, byte ptr [rbx + rdx]
        cmp     al, byte ptr [rdi + rdx]
        jne     .Ln_descend
        inc     edx
        cmp     edx, ebp
        je      .Ln_max                         /* ++len == lenLimit */
        movzx   eax, byte ptr [rbx + rdx]
        cmp     al, byte ptr [rdi + rdx]
        jne     .Ln_max
        inc     edx
        LZEXT   rbx, rdi, edx, rdx, ebp, rbp
.Ln_max:
        cmp     edx, dword ptr [rsp + G_MAXLEN]
        jbe     .Ln_descend                     /* if (maxLen < len) */
        mov     dword ptr [rsp + G_MAXLEN], edx
        mov     rax, qword ptr [rsp + G_D]
        mov     dword ptr [rax], edx            /* *d++ = len */
        not     ecx
        add     ecx, r12d                       /* delta - 1 */
        mov     dword ptr [rax + 4], ecx        /* *d++ = delta - 1 */
        sub     ecx, r12d
        not     ecx
        add     rax, 8
        mov     qword ptr [rsp + G_D], rax
        cmp     edx, ebp
        je      .Ln_full
.Ln_descend:
        movzx   eax, byte ptr [rbx + rdx]
        cmp     al, byte ptr [rdi + rdx]        /* CF = len[diff] < len[0] */
        mov     rax, r8
        cmovb   rax, r9
        mov     dword ptr [rax], ecx            /* *ptr1 / *ptr0 = curMatch */
        lea     rax, [r15 + 4]
        cmovb   r9, rax                         /* ptr1 = pair + 1 */
        cmovae  r8, r15                         /* ptr0 = pair */
        cmovb   r11d, edx                       /* len1 = len */
        cmovae  r10d, edx                       /* len0 = len */
        mov     ebx, dword ptr [r15]            /* next = pair[0] ... */
        cmovb   ebx, dword ptr [r15 + 4]        /*      ... or pair[1] */
        cmp     ebx, ecx
        jae     .Ln_fail                        /* if (delta >= curMatch) return NULL */
        mov     ecx, ebx
        mov     eax, r12d
        sub     eax, ebx                        /* delta = pos - next */
        sub     dword ptr [rsp + G_CUT], 1
        jz      .Ln_endtree                     /* --cutValue == 0 */
        cmp     eax, r14d
        jb      .Ln_tree                        /* delta < cbs */
.Ln_endtree:
        mov     dword ptr [r8], 0               /* *ptr0 = *ptr1 = kEmptyHashValue */
        mov     dword ptr [r9], 0
        mov     rax, qword ptr [rsp + G_D]
        mov     rdx, qword ptr [rsp + G_DIST0]
        sub     rax, rdx
        shr     rax, 2
        mov     dword ptr [rdx - 4], eax        /* _distances[-1] = d - _distances */
.Ln_next:
        inc     r12                             /* pos++ */
        inc     r13                             /* _cyclicBufferPos++ */
        inc     rdi                             /* cur++ */
        mov     rax, qword ptr [rsp + G_D]
        cmp     rax, qword ptr [rsp + G_LIMIT]
        jb      .Ln_pos                         /* while (d < limit) */
.Ln_done:
        mov     rax, qword ptr [rsp + G_POSRES]
        mov     dword ptr [rax], r12d           /* *posRes = (UInt32)pos */
        mov     rax, qword ptr [rsp + G_D]
        jmp     .Ln_ret
.Ln_fail:
        xor     eax, eax
.Ln_ret:
        add     rsp, G_SIZE
        FA_RESTORE_NONVOL
        ret

.Ln_nomatch:                                    /* delta >= cbs: empty node */
        mov     rax, qword ptr [rsp + G_D]
        mov     dword ptr [rax], 0              /* *d++ = 0 */
        add     rax, 4
        mov     qword ptr [rsp + G_D], rax
        mov     qword ptr [r9], 0               /* ptr1[0] = ptr1[1] = kEmptyHashValue */
        jmp     .Ln_next

.Ln_full:                                       /* len == lenLimit */
        mov     eax, dword ptr [r15]
        mov     edx, dword ptr [r15 + 4]
        mov     dword ptr [r9], eax             /* *ptr1 = pair0 */
        mov     dword ptr [r8], edx             /* *ptr0 = pair1 */
        mov     rax, qword ptr [rsp + G_D]
        mov     rdx, qword ptr [rsp + G_DIST0]
        sub     rax, rdx
        shr     rax, 2
        mov     dword ptr [rdx - 4], eax        /* _distances[-1] = d - _distances */
        mov     ebx, r12d
        sub     ebx, ecx                        /* ebx = delta (constant below) */
.Ln_long:                                       /* USE_LONG_MATCH_OPT */
        mov     rax, qword ptr [rsp + G_HASH]
        cmp     rax, qword ptr [rsp + G_END]
        je      .Ln_next                        /* hash == size */
        cmp     ebx, dword ptr [rax]
        jne     .Ln_next                        /* *hash != delta */
        lea     rdx, [rdi + rbp]                /* lenLimit (pointer) */
        mov     r15, rdx
        sub     r15, rbx
        movzx   eax, byte ptr [r15]
        cmp     al, byte ptr [rdx]
        jne     .Ln_next                        /* lenLimit[diff] != lenLimit[0] */
        mov     rax, qword ptr [rsp + G_D]
        cmp     rax, qword ptr [rsp + G_LIMIT]
        jae     .Ln_next                        /* d >= limit */
        mov     dword ptr [rax], 2              /* *d++ = 2 */
        mov     dword ptr [rax + 4], ebp        /* *d++ = lenLimit - cur */
        lea     edx, [rbx - 1]
        mov     dword ptr [rax + 8], edx        /* *d++ = delta - 1 */
        add     rax, 12
        mov     qword ptr [rsp + G_D], rax
        inc     rdi                             /* cur++ (lenLimit++) */
        inc     r13                             /* _cyclicBufferPos++ */
        mov     edx, r13d
        sub     edx, ebx
        lea     eax, [rdx + r14]
        cmovb   edx, eax                        /* src index = cbp - delta (+ cbs) */
        mov     rax, qword ptr [rsi + rdx*8]
        mov     qword ptr [rsi + r13*8], rax    /* dest[0..1] = src[0..1] */
        inc     r12                             /* pos++ */
        add     qword ptr [rsp + G_HASH], 4     /* hash++ */
        jmp     .Ln_long
FA_ENDFUNC(lzf25_bt_getmatches_n)

#if defined(__ELF__)
        .section .note.GNU-stack,"",@progbits
#endif
