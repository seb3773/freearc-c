/*
 * tor_match_x86_64.S - x86-64 match length kernels for the Tornado LZ77 codec
 * (Compression/Tornado/MatchFinder.cpp). GNU as, Intel syntax, C preprocessor;
 * System V (Linux) and Microsoft x64 (MinGW-w64) ABIs. Leaf routines using only
 * volatile registers (rax rcx rdx r8-r11, xmm0/xmm1/ymm0), so no register is
 * saved on either ABI.
 *
 *   UINT tor_find_match_len(const BYTE *p, const BYTE *q, const void *bufend, UINT len)
 *       == find_match_len(): length of the common prefix of p[] and q[], starting
 *          at len, never extending past bufend (returns len unchanged if
 *          p+len >= bufend).
 *
 *   UINT tor_ext4(const BYTE *p, const BYTE *q, const void *bufend, UINT len)
 *       == the legacy pair of loops used by the caching match finders:
 *              for (; p+len<bufend && val32equ(p+len, q+len); len+=4);
 *              for (; p+len<bufend && p[len] == q[len];       len++);
 *          including its quirk: the 4-byte loop may step up to 3 bytes PAST
 *          bufend (bufend is followed by MAX_HASHED_BYTES of slack), which changes
 *          the encoded stream, so it is reproduced exactly.
 *
 * Both use AVX2 32-byte compares (vpcmpeqb + vpmovmskb + tzcnt; runtime
 * detected through fa_cpu_avx2) and SSE2 16-byte compares, with every vector
 * load bounded by bufend.
 */
#include "../../asm/fa_x86_64.h"

/* Normalise both ABIs to: r11 = p, r10 = q, rdx = bufend, ecx = len (zero-extended) */
        .macro TOR_ARGS
#ifdef FA_WIN64
        mov     r11, rcx
        mov     r10, rdx
        mov     rdx, r8
        mov     ecx, r9d
#else
        mov     r11, rdi
        mov     r10, rsi
        mov     ecx, ecx
#endif
        .endm

/* ------------------------------------------------------------------------- */
FA_FUNC(tor_find_match_len)
        FA_SEH(.seh_endprologue)
        TOR_ARGS
        mov     eax, ecx
        sub     rdx, r11                        /* lim = bufend - p (signed) */
        cmp     rdx, rcx
        jle     .Lfm_ret                        /* p + len >= bufend: return len */
        mov     r9d, 0xFFFFFFFF
        cmp     rdx, r9
        cmova   rdx, r9                         /* (buffers are < 4 GB anyway) */
        LZEXT   r10, r11, ecx, rcx, edx, rdx
        mov     eax, ecx
.Lfm_ret:
        ret
FA_ENDFUNC(tor_find_match_len)

/* ------------------------------------------------------------------------- */
FA_FUNC(tor_ext4)
        FA_SEH(.seh_endprologue)
        TOR_ARGS
        sub     rdx, r11                        /* E = bufend - p (signed) */
        cmp     rdx, rcx
        jle     .Le4_ret                        /* p + len >= bufend */
        mov     r9d, 0xFFFFFFFF
        cmp     rdx, r9
        cmova   rdx, r9
        /* Vector phase: while len+W <= E, a mismatch m found here is < E, and
           the legacy loops would return exactly m. Steps keep len = len0 + 4k. */
        cmp     byte ptr [rip + fa_cpu_avx2], 0
        je      .Le4_sse
        lea     eax, [rcx + 32]
        cmp     eax, edx
        ja      .Le4_sse
.Le4_avx:
        vmovdqu  ymm0, ymmword ptr [r11 + rcx]
        vpcmpeqb ymm0, ymm0, ymmword ptr [r10 + rcx]
        vpmovmskb eax, ymm0
        inc     eax
        jnz     .Le4_foundavx
        add     ecx, 32
        lea     eax, [rcx + 32]
        cmp     eax, edx
        jbe     .Le4_avx
        vzeroupper
.Le4_sse:
        lea     eax, [rcx + 16]
        cmp     eax, edx
        ja      .Le4_scalar
.Le4_sse_loop:
        movdqu  xmm0, xmmword ptr [r11 + rcx]
        movdqu  xmm1, xmmword ptr [r10 + rcx]
        pcmpeqb xmm0, xmm1
        pmovmskb eax, xmm0
        xor     eax, 0xFFFF
        jnz     .Le4_foundv
        add     ecx, 16
        lea     eax, [rcx + 16]
        cmp     eax, edx
        jbe     .Le4_sse_loop
.Le4_scalar:                                    /* exact legacy loops for the last < 16 bytes */
        cmp     ecx, edx
        jae     .Le4_done
        mov     eax, dword ptr [r11 + rcx]
        cmp     eax, dword ptr [r10 + rcx]
        jne     .Le4_bytes
        add     ecx, 4
        jmp     .Le4_scalar
.Le4_bytes:
        cmp     ecx, edx
        jae     .Le4_done
        movzx   eax, byte ptr [r11 + rcx]
        cmp     al, byte ptr [r10 + rcx]
        jne     .Le4_done
        inc     ecx
        jmp     .Le4_bytes
.Le4_foundavx:
        vzeroupper
.Le4_foundv:
        tzcnt   eax, eax
        add     ecx, eax
.Le4_done:
        mov     eax, ecx
        ret
.Le4_ret:
        mov     eax, ecx
        ret
FA_ENDFUNC(tor_ext4)

#if defined(__ELF__)
        .section .note.GNU-stack,"",@progbits
#endif
