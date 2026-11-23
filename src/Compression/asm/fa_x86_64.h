/*
 * fa_x86_64.h - shared helpers for FreeArc x86-64 GNU-as (.S) sources.
 *
 * Included (through the C preprocessor) by every hand-written x86-64 match
 * finder routine. It provides:
 *   - FA_FUNC/FA_ENDFUNC : symbol declaration for ELF (Linux) and COFF (MinGW-w64)
 *   - FA_ARG1..FA_ARG4   : integer argument registers of the native ABI
 *                          (System V AMD64: rdi rsi rdx rcx, Microsoft x64: rcx rdx r8 r9)
 *   - FA_SAVE_NONVOL / FA_RESTORE_NONVOL : push/pop of every callee-saved GPR.
 *     On Windows rdi/rsi are callee-saved too, so they are pushed as well; the
 *     routines only use xmm0..xmm5 / ymm0..ymm5, which are volatile in BOTH ABIs,
 *     so no XMM register ever needs saving.
 *   - LZEXT macro        : exact equivalent of LzExt()/find_match_len() (see below).
 *
 * All routines are leaf functions: they never call anything, so the Win64
 * 32-byte shadow space is never needed (it is owned by the caller anyway).
 * SEH unwind directives are emitted on Win64 so that debuggers/stack walkers
 * can unwind through the pushes.
 */
#ifndef FA_X86_64_H
#define FA_X86_64_H

/* ABI: Microsoft x64 on Windows targets. FA_TEST_MSABI forces the Microsoft
 * register convention inside an ELF object, so the Win64 code path can be
 * exercised on Linux (bench/abi_check: callers declared __attribute__((ms_abi))). */
#if defined(_WIN32) || defined(__CYGWIN__) || defined(FA_TEST_MSABI)
#  define FA_WIN64 1
#endif

        .intel_syntax noprefix

#if defined(__ELF__)
#  define FA_FUNC(name)  .text; .globl name; .hidden name; .type name, @function; .p2align 5; name:
#  define FA_ENDFUNC(name) .size name, .-name
#  define FA_SEH(x)
#elif defined(_WIN32) || defined(__CYGWIN__)
#  define FA_FUNC(name)  .text; .globl name; .def name; .scl 2; .type 32; .endef; .p2align 5; .seh_proc name; name:
#  define FA_ENDFUNC(name) .seh_endproc
#  define FA_SEH(x) x
#else
#  error "unsupported object format"
#endif

#ifdef FA_WIN64
#  define FA_ARG1 rcx
#  define FA_ARG2 rdx
#  define FA_ARG3 r8
#  define FA_ARG4 r9
#  define FA_ARG4_32 r9d
#else
#  define FA_ARG1 rdi
#  define FA_ARG2 rsi
#  define FA_ARG3 rdx
#  define FA_ARG4 rcx
#  define FA_ARG4_32 ecx
#endif

/* Push all callee-saved GPRs of the active ABI (6 on SysV, 8 on Win64) */
        .macro FA_SAVE_NONVOL
        push    rbx
        FA_SEH(.seh_pushreg rbx)
        push    rbp
        FA_SEH(.seh_pushreg rbp)
        push    r12
        FA_SEH(.seh_pushreg r12)
        push    r13
        FA_SEH(.seh_pushreg r13)
        push    r14
        FA_SEH(.seh_pushreg r14)
        push    r15
        FA_SEH(.seh_pushreg r15)
#ifdef FA_WIN64
        push    rdi
        FA_SEH(.seh_pushreg rdi)
        push    rsi
        FA_SEH(.seh_pushreg rsi)
#endif
        .endm

        .macro FA_RESTORE_NONVOL
#ifdef FA_WIN64
        pop     rsi
        pop     rdi
#endif
        pop     r15
        pop     r14
        pop     r13
        pop     r12
        pop     rbp
        pop     rbx
        .endm

/*
 * LZEXT a, b, i32, i64, lim32, lim64
 *
 * Exact equivalent of
 *     while (i + 8 <= lim) { x = *(u64*)(a+i) ^ *(u64*)(b+i); if (x) return i + ctz(x)/8; i += 8; }
 *     while (i < lim && a[i] == b[i]) i++;
 * i.e. i := min(first index >= i where a[] and b[] differ, lim).
 * Never reads a[k]/b[k] for k >= lim (all vector loads are bounded by lim),
 * so it is safe at the very end of a buffer, exactly like the C code.
 *
 *   a, b      : 64-bit pointer registers (not modified)
 *   i32/i64   : 32/64-bit names of the index register (in/out, upper half zero)
 *   lim32/64  : 32/64-bit names of the limit register (not modified, upper half zero)
 * Clobbers: rax, xmm0, xmm1, ymm0 (vzeroupper issued after any AVX use), flags.
 *
 * Strategy: one 8-byte probe (most matches end within 8 bytes), then 32-byte
 * AVX2 blocks (vpcmpeqb + vpmovmskb + tzcnt) when the CPU supports it, then
 * 16-byte SSE2 blocks, then 8-byte blocks, then an OVERLAPPING final 8-byte
 * load ending exactly at lim (legal because at least 8 bytes before the
 * current index are already known to be equal), so there is no byte loop on
 * the long path. tzcnt is used on non-zero inputs only, where it is
 * bit-identical to bsf (tzcnt executes as bsf on CPUs without BMI1).
 */
        .macro LZEXT a, b, i32, i64, lim32, lim64
        lea     eax, [\i64 + 8]
        cmp     eax, \lim32
        ja      .Lext_bytes\@
        mov     rax, qword ptr [\a + \i64]
        xor     rax, qword ptr [\b + \i64]
        jnz     .Lext_found8\@
        add     \i32, 8
        cmp     byte ptr [rip + fa_cpu_avx2], 0
        je      .Lext_sse\@
        lea     eax, [\i64 + 32]
        cmp     eax, \lim32
        ja      .Lext_sse\@
.Lext_avx\@:
        vmovdqu  ymm0, ymmword ptr [\a + \i64]
        vpcmpeqb ymm0, ymm0, ymmword ptr [\b + \i64]
        vpmovmskb eax, ymm0
        inc     eax                     /* all-equal mask 0xFFFFFFFF -> 0 */
        jnz     .Lext_foundavx\@
        add     \i32, 32
        lea     eax, [\i64 + 32]
        cmp     eax, \lim32
        jbe     .Lext_avx\@
        vzeroupper
.Lext_sse\@:
        lea     eax, [\i64 + 16]
        cmp     eax, \lim32
        ja      .Lext_q\@
.Lext_sse_loop\@:
        movdqu  xmm0, xmmword ptr [\a + \i64]
        movdqu  xmm1, xmmword ptr [\b + \i64]
        pcmpeqb xmm0, xmm1
        pmovmskb eax, xmm0
        xor     eax, 0xFFFF
        jnz     .Lext_foundv\@
        add     \i32, 16
        lea     eax, [\i64 + 16]
        cmp     eax, \lim32
        jbe     .Lext_sse_loop\@
.Lext_q\@:
        lea     eax, [\i64 + 8]
        cmp     eax, \lim32
        ja      .Lext_tail\@
        mov     rax, qword ptr [\a + \i64]
        xor     rax, qword ptr [\b + \i64]
        jnz     .Lext_found8\@
        add     \i32, 8
        jmp     .Lext_q\@
.Lext_tail\@:                           /* lim-8 >= start: overlap is safe */
        cmp     \i32, \lim32
        je      .Lext_done\@
        mov     rax, qword ptr [\a + \lim64 - 8]
        xor     rax, qword ptr [\b + \lim64 - 8]
        jz      .Lext_atlim\@
        tzcnt   rax, rax
        shr     eax, 3
        lea     \i32, [\lim64 + rax - 8]
        jmp     .Lext_done\@
.Lext_atlim\@:
        mov     \i32, \lim32
        jmp     .Lext_done\@
.Lext_foundavx\@:
        vzeroupper
.Lext_foundv\@:
        tzcnt   eax, eax
        add     \i32, eax
        jmp     .Lext_done\@
.Lext_found8\@:
        tzcnt   rax, rax
        shr     eax, 3
        add     \i32, eax
        jmp     .Lext_done\@
.Lext_bytes\@:                          /* fewer than 8 bytes left: plain byte loop */
        cmp     \i32, \lim32
        jae     .Lext_done\@
        movzx   eax, byte ptr [\a + \i64]
        cmp     al, byte ptr [\b + \i64]
        jne     .Lext_done\@
        inc     \i32
        jmp     .Lext_bytes\@
.Lext_done\@:
        .endm

#endif /* FA_X86_64_H */
