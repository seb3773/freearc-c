/*
 * fa_cpu_x86_64.S - runtime CPU feature detection shared by the x86-64
 * match-finder routines (LZMA LzFind_x86_64.S, Tornado tor_match_x86_64.S).
 *
 *   extern "C" unsigned char fa_cpu_avx2;     0 = use SSE2 only, 1 = AVX2 usable
 *   extern "C" void fa_x86_64_cpu_init(void); idempotent, cheap, thread-safe
 *                                             (writes the same value every time)
 *
 * AVX2 is reported only when CPUID says AVX+AVX2+OSXSAVE AND the OS saves the
 * YMM state (XCR0 bits 1-2), so the AVX2 paths are never taken on systems
 * where they would fault. Results never depend on this flag: the SSE2 and
 * AVX2 paths compute exactly the same match lengths.
 */
#include "fa_x86_64.h"

#if defined(__ELF__)
        .section .bss.fa_cpu,"aw",@nobits
        .globl  fa_cpu_avx2
        .hidden fa_cpu_avx2
        .type   fa_cpu_avx2, @object
        .size   fa_cpu_avx2, 1
#else
        .bss
        .globl  fa_cpu_avx2
#endif
fa_cpu_avx2:
        .zero   1

FA_FUNC(fa_x86_64_cpu_init)
        push    rbx                     /* cpuid clobbers rbx (callee-saved) */
        FA_SEH(.seh_pushreg rbx)
        FA_SEH(.seh_endprologue)
        xor     r8d, r8d                /* r8 = result */
        xor     eax, eax
        cpuid
        cmp     eax, 7
        jb      1f
        mov     eax, 1
        cpuid
        mov     r9d, ecx
        and     r9d, 0x18000000         /* OSXSAVE (27) | AVX (28) */
        cmp     r9d, 0x18000000
        jne     1f
        xor     ecx, ecx
        xgetbv                          /* edx:eax = XCR0 */
        and     eax, 6
        cmp     eax, 6                  /* XMM + YMM state enabled by the OS */
        jne     1f
        mov     eax, 7
        xor     ecx, ecx
        cpuid
        shr     ebx, 5                  /* AVX2 = CPUID.7.0:EBX bit 5 */
        and     ebx, 1
        mov     r8d, ebx
1:
        mov     byte ptr [rip + fa_cpu_avx2], r8b
        pop     rbx
        ret
FA_ENDFUNC(fa_x86_64_cpu_init)

#if defined(__ELF__)
        .section .note.GNU-stack,"",@progbits
#endif
