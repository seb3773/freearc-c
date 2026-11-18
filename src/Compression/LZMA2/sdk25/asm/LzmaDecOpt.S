# LzmaDecOpt.S - x86-64 (System V ABI) LZMA decoder inner loop, GNU as syntax.
#
# Generated from LzmaDecOpt.asm (LZMA SDK 25.01, Igor Pavlov, public domain),
# so that it builds with plain gcc/clang without a MASM-compatible assembler:
#   jwasm -nologo -elf64 -DABI_LINUX -Fo LzmaDecOpt.o LzmaDecOpt.asm
#   objconv -fgasm LzmaDecOpt.o LzmaDecOpt.S
#   then: "byte|word|dword|qword [" -> "... ptr [", explicit section flags,
#   and a .note.GNU-stack section (non-executable stack).
# The resulting instruction sequence is identical to the MASM build
# (verified with objdump; only jump encodings/padding differ).
# Implements: int LzmaDec_DecodeReal_3(CLzmaDec *p, SizeT limit, const Byte *bufLimit)
# The C side must be compiled with -DZ7_LZMA_DEC_OPT (see lzma_fastdec.c).

# Disassembly of file: LzmaDecOpt.o
# Wed Nov 18 02:47:43 2026
# Type: ELF64
# Syntax: GAS(Intel)
# Instruction set: Pentium Pro, x64

# Note: Uses Intel syntax with destination operand first. Remember to
# put syntax directives in the beginning and end of inline assembly:
.intel_syntax noprefix 

#ifdef __ELF__
.global sdk25_LzmaDec_DecodeReal_3
.hidden sdk25_LzmaDec_DecodeReal_3
.type sdk25_LzmaDec_DecodeReal_3, @function
.section .text,"ax",@progbits
        .ALIGN  16
.section .data,"aw",@progbits
        .ALIGN  16
.section .text.lzmadecopt,"ax",@progbits
#else
.global sdk25_LzmaDec_DecodeReal_3
.def sdk25_LzmaDec_DecodeReal_3; .scl 2; .type 32; .endef
.text
        .ALIGN  16
.data
        .ALIGN  16
.section .text
#endif
        .ALIGN  64

sdk25_LzmaDec_DecodeReal_3:                                   # Function
#ifdef __ELF__
        .type   sdk25_LzmaDec_DecodeReal_3, @function
#endif
        push    rbx                                     # 0000 _ 53
        push    rbp                                     # 0001 _ 55
        push    r12                                     # 0002 _ 41: 54
        push    r13                                     # 0004 _ 41: 55
        push    r14                                     # 0006 _ 41: 56
        push    r15                                     # 0008 _ 41: 57
        lea     rax, [rsp-0x80]                         # 000A _ 48: 8D. 44 24, 80
        and     rax, 0xFFFFFFFFFFFFFF80                 # 000F _ 48: 83. E0, 80
        mov     rbp, rsp                                # 0013 _ 48: 8B. EC
        mov     rsp, rax                                # 0016 _ 48: 8B. E0
        mov     qword ptr [rax], rbp                        # 0019 _ 48: 89. 28
        mov     qword ptr [rax+0x8], rdi                    # 001C _ 48: 89. 78, 08
        mov     dword ptr [rax+0x64], 0                     # 0020 _ C7. 40, 64, 00000000
        mov     qword ptr [rax+0x48], rdx                   # 0027 _ 48: 89. 50, 48
        mov     rbx, rdi                                # 002B _ 48: 8B. DF
        mov     r12, qword ptr [rbx+0x18]                   # 002E _ 4C: 8B. 63, 18
        add     rsi, r12                                # 0032 _ 49: 03. F4
        mov     qword ptr [rax+0x40], rsi                   # 0035 _ 48: 89. 70, 40
        mov     edi, dword ptr [rbx+0x48]                   # 0039 _ 8B. 7B, 48
        mov     dword ptr [rax+0x70], edi                   # 003C _ 89. 78, 70
        mov     edi, dword ptr [rbx+0x4C]                   # 003F _ 8B. 7B, 4C
        mov     dword ptr [rax+0x74], edi                   # 0042 _ 89. 78, 74
        mov     edi, dword ptr [rbx+0x50]                   # 0045 _ 8B. 7B, 50
        mov     dword ptr [rax+0x78], edi                   # 0048 _ 89. 78, 78
        mov     edi, dword ptr [rbx+0x54]                   # 004B _ 8B. 7B, 54
        mov     dword ptr [rax+0x7C], edi                   # 004E _ 89. 78, 7C
        mov     r14, qword ptr [rbx+0x28]                   # 0051 _ 4C: 8B. 73, 28
        add     r14, r12                                # 0055 _ 4D: 03. F4
        mov     qword ptr [rax+0x68], r14                   # 0058 _ 4C: 89. 70, 68
        mov     qword ptr [rax+0x38], r12                   # 005C _ 4C: 89. 60, 38
        mov     cl, byte ptr [rbx+0x2]                      # 0060 _ 8A. 4B, 02
        mov     edi, 1                                  # 0063 _ BF, 00000001
        shl     edi, cl                                 # 0068 _ D3. E7
        dec     edi                                     # 006A _ FF. CF
        mov     dword ptr [rax+0x58], edi                   # 006C _ 89. 78, 58
        mov     cl, byte ptr [rbx]                          # 006F _ 8A. 0B
        mov     edx, 256                                # 0071 _ BA, 00000100
        mov     edi, edx                                # 0076 _ 8B. FA
        shr     edx, cl                                 # 0078 _ D3. EA
        add     cl, 1                                   # 007A _ 80. C1, 01
        mov     dword ptr [rax+0x50], ecx                   # 007D _ 89. 48, 50
        mov     cl, byte ptr [rbx+0x1]                      # 0080 _ 8A. 4B, 01
        shl     edi, cl                                 # 0083 _ D3. E7
        sub     edi, edx                                # 0085 _ 2B. FA
        mov     dword ptr [rax+0x54], edi                   # 0087 _ 89. 78, 54
        mov     r9d, edi                                # 008A _ 44: 8B. CF
        mov     r11, qword ptr [rbx+0x10]                   # 008D _ 4C: 8B. 5B, 10
        mov     qword ptr [rax+0x30], r11                   # 0091 _ 4C: 89. 58, 30
        mov     rdi, qword ptr [rbx+0x20]                   # 0095 _ 48: 8B. 7B, 20
        mov     qword ptr [rax+0x28], rdi                   # 0099 _ 48: 89. 78, 28
        mov     ecx, dword ptr [rbx+0x44]                   # 009D _ 8B. 4B, 44
        mov     dword ptr [rax+0x5C], ecx                   # 00A0 _ 89. 48, 5C
        mov     r13d, dword ptr [rbx+0x40]                  # 00A3 _ 44: 8B. 6B, 40
        mov     r8d, dword ptr [rbx+0x58]                   # 00A7 _ 44: 8B. 43, 58
        shl     r8d, 1                                  # 00AB _ 41: D1. E0
        mov     r15, qword ptr [rbx+0x30]                   # 00AE _ 4C: 8B. 7B, 30
        mov     eax, dword ptr [rbx+0x38]                   # 00B2 _ 8B. 43, 38
        mov     ebp, dword ptr [rbx+0x3C]                   # 00B5 _ 8B. 6B, 3C
        mov     r10d, 2048                              # 00B8 _ 41: BA, 00000800
        xor     ebx, ebx                                # 00BE _ 33. DB
        or      ecx, r13d                               # 00C0 _ 41: 0B. CD
        jz      $_001                                   # 00C3 _ 74, 0E
        add     rdi, r12                                # 00C5 _ 49: 03. FC
        cmp     r14, r12                                # 00C8 _ 4D: 3B. F4
        cmovne  rdi, r14                                # 00CB _ 49: 0F 45. FE
        movzx   ebx, byte ptr [rdi-0x1]                     # 00CF _ 0F B6. 5F, FF
$_001:  mov     ecx, dword ptr [rsp+0x58]                   # 00D3 _ 8B. 4C 24, 58
        and     ecx, r13d                               # 00D7 _ 41: 23. CD
        shl     ecx, 5                                  # 00DA _ C1. E1, 05
        lea     rsi, [r11+r8]                           # 00DD _ 4B: 8D. 34 03
        cmp     r8d, 8                                  # 00E1 _ 41: 83. F8, 08
        jc      $_012                                   # 00E5 _ 0F 82, 000002FE
        cmp     r8d, 14                                 # 00EB _ 41: 83. F8, 0E
        jc      $_056                                   # 00EF _ 0F 82, 00000DC5
        jmp     $_045                                   # 00F5 _ E9, 00000A0F

# Filling space: 0x6
# Filler type: Multi-byte NOP
#       .byte 0x66, 0x0F, 0x1F, 0x44, 0x00, 0x00

.ALIGN  8
$_002:  xor     r8d, r8d                                # 0100 _ 45: 33. C0
$_003:  mov     edi, r13d                               # 0103 _ 41: 8B. FD
        shl     edi, 8                                  # 0106 _ C1. E7, 08
        add     ebx, edi                                # 0109 _ 03. DF
        and     ebx, r9d                                # 010B _ 41: 23. D9
        add     rsi, rcx                                # 010E _ 48: 03. F1
        mov     ecx, dword ptr [rsp+0x50]                   # 0111 _ 8B. 4C 24, 50
        lea     ebx, [rbx+rbx*2]                        # 0115 _ 8D. 1C 5B
        add     r11, 640                                # 0118 _ 49: 81. C3, 00000280
        shl     ebx, cl                                 # 011F _ D3. E3
        add     r11, rbx                                # 0121 _ 4C: 03. DB
        mov     edi, r10d                               # 0124 _ 41: 8B. FA
        sub     edi, edx                                # 0127 _ 2B. FA
        shr     edi, 5                                  # 0129 _ C1. EF, 05
        add     edx, edi                                # 012C _ 03. D7
        mov     word ptr [rsi-0x200], dx                    # 012E _ 66: 89. 96, FFFFFE00
        inc     r13d                                    # 0135 _ 41: FF. C5
        movzx   ecx, word ptr [r11+0x2]                     # 0138 _ 41: 0F B7. 4B, 02
        movzx   edx, word ptr [r11+0x4]                     # 013D _ 41: 0F B7. 53, 04
        cmp     eax, 16777216                           # 0142 _ 3D, 01000000
        jnc     $_004                                   # 0147 _ 73, 0C
        shl     ebp, 8                                  # 0149 _ C1. E5, 08
        mov     bpl, byte ptr [r15]                         # 014C _ 41: 8A. 2F
        shl     eax, 8                                  # 014F _ C1. E0, 08
        inc     r15                                     # 0152 _ 49: FF. C7
$_004:  mov     edi, eax                                # 0155 _ 8B. F8
        shr     eax, 11                                 # 0157 _ C1. E8, 0B
        imul    eax, ecx                                # 015A _ 0F AF. C1
        sub     edi, eax                                # 015D _ 2B. F8
        mov     esi, ebp                                # 015F _ 8B. F5
        sub     ebp, eax                                # 0161 _ 2B. E8
        cmovnc  eax, edi                                # 0163 _ 0F 43. C7
        movzx   edi, word ptr [r11+0x6]                     # 0166 _ 41: 0F B7. 7B, 06
        cmovnc  edx, edi                                # 016B _ 0F 43. D7
        mov     edi, 31                                 # 016E _ BF, 0000001F
        cmovc   ebp, esi                                # 0173 _ 0F 42. EE
        cmovc   edi, r10d                               # 0176 _ 41: 0F 42. FA
        mov     ebx, 2                                  # 017A _ BB, 00000002
        sbb     ebx, -1                                 # 017F _ 83. DB, FF
        sub     edi, ecx                                # 0182 _ 2B. F9
        sar     edi, 5                                  # 0184 _ C1. FF, 05
        add     edi, ecx                                # 0187 _ 03. F9
        mov     word ptr [r11+0x2], di                      # 0189 _ 66 41: 89. 7B, 02
        movzx   ecx, word ptr [r11+rbx*4]                   # 018E _ 41: 0F B7. 0C 9B
        add     ebx, ebx                                # 0193 _ 03. DB
        cmp     eax, 16777216                           # 0195 _ 3D, 01000000
        jnc     $_005                                   # 019A _ 73, 0C
        shl     ebp, 8                                  # 019C _ C1. E5, 08
        mov     bpl, byte ptr [r15]                         # 019F _ 41: 8A. 2F
        shl     eax, 8                                  # 01A2 _ C1. E0, 08
        inc     r15                                     # 01A5 _ 49: FF. C7
$_005:  mov     edi, eax                                # 01A8 _ 8B. F8
        shr     eax, 11                                 # 01AA _ C1. E8, 0B
        imul    eax, edx                                # 01AD _ 0F AF. C2
        sub     edi, eax                                # 01B0 _ 2B. F8
        mov     esi, ebp                                # 01B2 _ 8B. F5
        sub     ebp, eax                                # 01B4 _ 2B. E8
        cmovnc  eax, edi                                # 01B6 _ 0F 43. C7
        movzx   edi, word ptr [r11+rbx*2+0x2]               # 01B9 _ 41: 0F B7. 7C 5B, 02
        cmovnc  ecx, edi                                # 01BF _ 0F 43. CF
        mov     edi, 31                                 # 01C2 _ BF, 0000001F
        cmovc   ebp, esi                                # 01C7 _ 0F 42. EE
        mov     esi, ebx                                # 01CA _ 8B. F3
        cmovc   edi, r10d                               # 01CC _ 41: 0F 42. FA
        sbb     ebx, -1                                 # 01D0 _ 83. DB, FF
        sub     edi, edx                                # 01D3 _ 2B. FA
        sar     edi, 5                                  # 01D5 _ C1. FF, 05
        add     edi, edx                                # 01D8 _ 03. FA
        mov     word ptr [r11+rsi], di                      # 01DA _ 66 41: 89. 3C 33
        movzx   edx, word ptr [r11+rbx*4]                   # 01DF _ 41: 0F B7. 14 9B
        add     ebx, ebx                                # 01E4 _ 03. DB
        cmp     eax, 16777216                           # 01E6 _ 3D, 01000000
        jnc     $_006                                   # 01EB _ 73, 0C
        shl     ebp, 8                                  # 01ED _ C1. E5, 08
        mov     bpl, byte ptr [r15]                         # 01F0 _ 41: 8A. 2F
        shl     eax, 8                                  # 01F3 _ C1. E0, 08
        inc     r15                                     # 01F6 _ 49: FF. C7
$_006:  mov     edi, eax                                # 01F9 _ 8B. F8
        shr     eax, 11                                 # 01FB _ C1. E8, 0B
        imul    eax, ecx                                # 01FE _ 0F AF. C1
        sub     edi, eax                                # 0201 _ 2B. F8
        mov     esi, ebp                                # 0203 _ 8B. F5
        sub     ebp, eax                                # 0205 _ 2B. E8
        cmovnc  eax, edi                                # 0207 _ 0F 43. C7
        movzx   edi, word ptr [r11+rbx*2+0x2]               # 020A _ 41: 0F B7. 7C 5B, 02
        cmovnc  edx, edi                                # 0210 _ 0F 43. D7
        mov     edi, 31                                 # 0213 _ BF, 0000001F
        cmovc   ebp, esi                                # 0218 _ 0F 42. EE
        mov     esi, ebx                                # 021B _ 8B. F3
        cmovc   edi, r10d                               # 021D _ 41: 0F 42. FA
        sbb     ebx, -1                                 # 0221 _ 83. DB, FF
        sub     edi, ecx                                # 0224 _ 2B. F9
        sar     edi, 5                                  # 0226 _ C1. FF, 05
        add     edi, ecx                                # 0229 _ 03. F9
        mov     word ptr [r11+rsi], di                      # 022B _ 66 41: 89. 3C 33
        movzx   ecx, word ptr [r11+rbx*4]                   # 0230 _ 41: 0F B7. 0C 9B
        add     ebx, ebx                                # 0235 _ 03. DB
        cmp     eax, 16777216                           # 0237 _ 3D, 01000000
        jnc     $_007                                   # 023C _ 73, 0C
        shl     ebp, 8                                  # 023E _ C1. E5, 08
        mov     bpl, byte ptr [r15]                         # 0241 _ 41: 8A. 2F
        shl     eax, 8                                  # 0244 _ C1. E0, 08
        inc     r15                                     # 0247 _ 49: FF. C7
$_007:  mov     edi, eax                                # 024A _ 8B. F8
        shr     eax, 11                                 # 024C _ C1. E8, 0B
        imul    eax, edx                                # 024F _ 0F AF. C2
        sub     edi, eax                                # 0252 _ 2B. F8
        mov     esi, ebp                                # 0254 _ 8B. F5
        sub     ebp, eax                                # 0256 _ 2B. E8
        cmovnc  eax, edi                                # 0258 _ 0F 43. C7
        movzx   edi, word ptr [r11+rbx*2+0x2]               # 025B _ 41: 0F B7. 7C 5B, 02
        cmovnc  ecx, edi                                # 0261 _ 0F 43. CF
        mov     edi, 31                                 # 0264 _ BF, 0000001F
        cmovc   ebp, esi                                # 0269 _ 0F 42. EE
        mov     esi, ebx                                # 026C _ 8B. F3
        cmovc   edi, r10d                               # 026E _ 41: 0F 42. FA
        sbb     ebx, -1                                 # 0272 _ 83. DB, FF
        sub     edi, edx                                # 0275 _ 2B. FA
        sar     edi, 5                                  # 0277 _ C1. FF, 05
        add     edi, edx                                # 027A _ 03. FA
        mov     word ptr [r11+rsi], di                      # 027C _ 66 41: 89. 3C 33
        movzx   edx, word ptr [r11+rbx*4]                   # 0281 _ 41: 0F B7. 14 9B
        add     ebx, ebx                                # 0286 _ 03. DB
        cmp     eax, 16777216                           # 0288 _ 3D, 01000000
        jnc     $_008                                   # 028D _ 73, 0C
        shl     ebp, 8                                  # 028F _ C1. E5, 08
        mov     bpl, byte ptr [r15]                         # 0292 _ 41: 8A. 2F
        shl     eax, 8                                  # 0295 _ C1. E0, 08
        inc     r15                                     # 0298 _ 49: FF. C7
$_008:  mov     edi, eax                                # 029B _ 8B. F8
        shr     eax, 11                                 # 029D _ C1. E8, 0B
        imul    eax, ecx                                # 02A0 _ 0F AF. C1
        sub     edi, eax                                # 02A3 _ 2B. F8
        mov     esi, ebp                                # 02A5 _ 8B. F5
        sub     ebp, eax                                # 02A7 _ 2B. E8
        cmovnc  eax, edi                                # 02A9 _ 0F 43. C7
        movzx   edi, word ptr [r11+rbx*2+0x2]               # 02AC _ 41: 0F B7. 7C 5B, 02
        cmovnc  edx, edi                                # 02B2 _ 0F 43. D7
        mov     edi, 31                                 # 02B5 _ BF, 0000001F
        cmovc   ebp, esi                                # 02BA _ 0F 42. EE
        mov     esi, ebx                                # 02BD _ 8B. F3
        cmovc   edi, r10d                               # 02BF _ 41: 0F 42. FA
        sbb     ebx, -1                                 # 02C3 _ 83. DB, FF
        sub     edi, ecx                                # 02C6 _ 2B. F9
        sar     edi, 5                                  # 02C8 _ C1. FF, 05
        add     edi, ecx                                # 02CB _ 03. F9
        mov     word ptr [r11+rsi], di                      # 02CD _ 66 41: 89. 3C 33
        movzx   ecx, word ptr [r11+rbx*4]                   # 02D2 _ 41: 0F B7. 0C 9B
        add     ebx, ebx                                # 02D7 _ 03. DB
        cmp     eax, 16777216                           # 02D9 _ 3D, 01000000
        jnc     $_009                                   # 02DE _ 73, 0C
        shl     ebp, 8                                  # 02E0 _ C1. E5, 08
        mov     bpl, byte ptr [r15]                         # 02E3 _ 41: 8A. 2F
        shl     eax, 8                                  # 02E6 _ C1. E0, 08
        inc     r15                                     # 02E9 _ 49: FF. C7
$_009:  mov     edi, eax                                # 02EC _ 8B. F8
        shr     eax, 11                                 # 02EE _ C1. E8, 0B
        imul    eax, edx                                # 02F1 _ 0F AF. C2
        sub     edi, eax                                # 02F4 _ 2B. F8
        mov     esi, ebp                                # 02F6 _ 8B. F5
        sub     ebp, eax                                # 02F8 _ 2B. E8
        cmovnc  eax, edi                                # 02FA _ 0F 43. C7
        movzx   edi, word ptr [r11+rbx*2+0x2]               # 02FD _ 41: 0F B7. 7C 5B, 02
        cmovnc  ecx, edi                                # 0303 _ 0F 43. CF
        mov     edi, 31                                 # 0306 _ BF, 0000001F
        cmovc   ebp, esi                                # 030B _ 0F 42. EE
        mov     esi, ebx                                # 030E _ 8B. F3
        cmovc   edi, r10d                               # 0310 _ 41: 0F 42. FA
        sbb     ebx, -1                                 # 0314 _ 83. DB, FF
        sub     edi, edx                                # 0317 _ 2B. FA
        sar     edi, 5                                  # 0319 _ C1. FF, 05
        add     edi, edx                                # 031C _ 03. FA
        mov     word ptr [r11+rsi], di                      # 031E _ 66 41: 89. 3C 33
        movzx   edx, word ptr [r11+rbx*4]                   # 0323 _ 41: 0F B7. 14 9B
        add     ebx, ebx                                # 0328 _ 03. DB
        cmp     eax, 16777216                           # 032A _ 3D, 01000000
        jnc     $_010                                   # 032F _ 73, 0C
        shl     ebp, 8                                  # 0331 _ C1. E5, 08
        mov     bpl, byte ptr [r15]                         # 0334 _ 41: 8A. 2F
        shl     eax, 8                                  # 0337 _ C1. E0, 08
        inc     r15                                     # 033A _ 49: FF. C7
$_010:  mov     edi, eax                                # 033D _ 8B. F8
        shr     eax, 11                                 # 033F _ C1. E8, 0B
        imul    eax, ecx                                # 0342 _ 0F AF. C1
        sub     edi, eax                                # 0345 _ 2B. F8
        mov     esi, ebp                                # 0347 _ 8B. F5
        sub     ebp, eax                                # 0349 _ 2B. E8
        cmovnc  eax, edi                                # 034B _ 0F 43. C7
        movzx   edi, word ptr [r11+rbx*2+0x2]               # 034E _ 41: 0F B7. 7C 5B, 02
        cmovnc  edx, edi                                # 0354 _ 0F 43. D7
        mov     edi, 31                                 # 0357 _ BF, 0000001F
        cmovc   ebp, esi                                # 035C _ 0F 42. EE
        mov     esi, ebx                                # 035F _ 8B. F3
        cmovc   edi, r10d                               # 0361 _ 41: 0F 42. FA
        sbb     ebx, -1                                 # 0365 _ 83. DB, FF
        sub     edi, ecx                                # 0368 _ 2B. F9
        sar     edi, 5                                  # 036A _ C1. FF, 05
        add     edi, ecx                                # 036D _ 03. F9
        mov     word ptr [r11+rsi], di                      # 036F _ 66 41: 89. 3C 33
        add     ebx, ebx                                # 0374 _ 03. DB
        cmp     eax, 16777216                           # 0376 _ 3D, 01000000
        jnc     $_011                                   # 037B _ 73, 0C
        shl     ebp, 8                                  # 037D _ C1. E5, 08
        mov     bpl, byte ptr [r15]                         # 0380 _ 41: 8A. 2F
        shl     eax, 8                                  # 0383 _ C1. E0, 08
        inc     r15                                     # 0386 _ 49: FF. C7
$_011:  mov     edi, eax                                # 0389 _ 8B. F8
        shr     eax, 11                                 # 038B _ C1. E8, 0B
        imul    eax, edx                                # 038E _ 0F AF. C2
        sub     edi, eax                                # 0391 _ 2B. F8
        mov     esi, ebp                                # 0393 _ 8B. F5
        sub     ebp, eax                                # 0395 _ 2B. E8
        cmovnc  eax, edi                                # 0397 _ 0F 43. C7
        mov     edi, 31                                 # 039A _ BF, 0000001F
        cmovc   ebp, esi                                # 039F _ 0F 42. EE
        mov     esi, ebx                                # 03A2 _ 8B. F3
        cmovc   edi, r10d                               # 03A4 _ 41: 0F 42. FA
        sbb     ebx, 255                                # 03A8 _ 81. DB, 000000FF
        sub     edi, edx                                # 03AE _ 2B. FA
        sar     edi, 5                                  # 03B0 _ C1. FF, 05
        add     edi, edx                                # 03B3 _ 03. FA
        mov     word ptr [r11+rsi], di                      # 03B5 _ 66 41: 89. 3C 33
        mov     r11, qword ptr [rsp+0x30]                   # 03BA _ 4C: 8B. 5C 24, 30
        mov     ecx, dword ptr [rsp+0x58]                   # 03BF _ 8B. 4C 24, 58
        and     ecx, r13d                               # 03C3 _ 41: 23. CD
        shl     ecx, 5                                  # 03C6 _ C1. E1, 05
        lea     rsi, [r11+r8]                           # 03C9 _ 4B: 8D. 34 03
        mov     byte ptr [r14], bl                          # 03CD _ 41: 88. 1E
        inc     r14                                     # 03D0 _ 49: FF. C6
        cmp     r15, qword ptr [rsp+0x48]                   # 03D3 _ 4C: 3B. 7C 24, 48
        jnc     $_076                                   # 03D8 _ 0F 83, 00000E25
        cmp     r14, qword ptr [rsp+0x40]                   # 03DE _ 4C: 3B. 74 24, 40
        jnc     $_076                                   # 03E3 _ 0F 83, 00000E1A
$_012:  movzx   edx, word ptr [rsi+rcx-0x200]               # 03E9 _ 0F B7. 94 0E, FFFFFE00
        cmp     eax, 16777216                           # 03F1 _ 3D, 01000000
        jnc     $_013                                   # 03F6 _ 73, 0C
        shl     ebp, 8                                  # 03F8 _ C1. E5, 08
        mov     bpl, byte ptr [r15]                         # 03FB _ 41: 8A. 2F
        shl     eax, 8                                  # 03FE _ C1. E0, 08
        inc     r15                                     # 0401 _ 49: FF. C7
$_013:  mov     edi, eax                                # 0404 _ 8B. F8
        shr     eax, 11                                 # 0406 _ C1. E8, 0B
        imul    eax, edx                                # 0409 _ 0F AF. C2
        cmp     ebp, eax                                # 040C _ 3B. E8
        jc      $_002                                   # 040E _ 0F 82, FFFFFCEC
$_014:  sub     edi, eax                                # 0414 _ 2B. F8
        sub     ebp, eax                                # 0416 _ 2B. E8
        mov     eax, edi                                # 0418 _ 8B. C7
        mov     edi, edx                                # 041A _ 8B. FA
        shr     edx, 5                                  # 041C _ C1. EA, 05
        sub     edi, edx                                # 041F _ 2B. FA
        mov     word ptr [rsi+rcx-0x200], di                # 0421 _ 66: 89. BC 0E, FFFFFE00
        movzx   edx, word ptr [rsi+0x20]                    # 0429 _ 0F B7. 56, 20
        cmp     eax, 16777216                           # 042D _ 3D, 01000000
        jnc     $_015                                   # 0432 _ 73, 0C
        shl     ebp, 8                                  # 0434 _ C1. E5, 08
        mov     bpl, byte ptr [r15]                         # 0437 _ 41: 8A. 2F
        shl     eax, 8                                  # 043A _ C1. E0, 08
        inc     r15                                     # 043D _ 49: FF. C7
$_015:  mov     edi, eax                                # 0440 _ 8B. F8
        shr     eax, 11                                 # 0442 _ C1. E8, 0B
        imul    eax, edx                                # 0445 _ 0F AF. C2
        cmp     ebp, eax                                # 0448 _ 3B. E8
        jnc     $_060                                   # 044A _ 0F 83, 00000B10
        mov     edi, r10d                               # 0450 _ 41: 8B. FA
        sub     edi, edx                                # 0453 _ 2B. FA
        shr     edi, 5                                  # 0455 _ C1. EF, 05
        add     edx, edi                                # 0458 _ 03. D7
        mov     word ptr [rsi+0x20], dx                     # 045A _ 66: 89. 56, 20
        add     r11, -1536                              # 045E _ 49: 81. C3, FFFFFA00
        add     r8d, 24                                 # 0465 _ 41: 83. C0, 18
$_016:  mov     r12d, 5                                 # 0469 _ 41: BC, 00000005
        movzx   edx, word ptr [r11]                         # 046F _ 41: 0F B7. 13
        cmp     eax, 16777216                           # 0473 _ 3D, 01000000
        jnc     $_017                                   # 0478 _ 73, 0C
        shl     ebp, 8                                  # 047A _ C1. E5, 08
        mov     bpl, byte ptr [r15]                         # 047D _ 41: 8A. 2F
        shl     eax, 8                                  # 0480 _ C1. E0, 08
        inc     r15                                     # 0483 _ 49: FF. C7
$_017:  mov     edi, eax                                # 0486 _ 8B. F8
        shr     eax, 11                                 # 0488 _ C1. E8, 0B
        imul    eax, edx                                # 048B _ 0F AF. C2
        cmp     ebp, eax                                # 048E _ 3B. E8
        jc      $_021                                   # 0490 _ 0F 82, 000000CA
        sub     edi, eax                                # 0496 _ 2B. F8
        sub     ebp, eax                                # 0498 _ 2B. E8
        mov     eax, edi                                # 049A _ 8B. C7
        mov     edi, edx                                # 049C _ 8B. FA
        shr     edx, 5                                  # 049E _ C1. EA, 05
        sub     edi, edx                                # 04A1 _ 2B. FA
        mov     word ptr [r11], di                          # 04A3 _ 66 41: 89. 3B
        add     r11, 16                                 # 04A7 _ 49: 83. C3, 10
        mov     r12d, 4294967293                        # 04AB _ 41: BC, FFFFFFFD
        movzx   edx, word ptr [r11]                         # 04B1 _ 41: 0F B7. 13
        cmp     eax, 16777216                           # 04B5 _ 3D, 01000000
        jnc     $_018                                   # 04BA _ 73, 0C
        shl     ebp, 8                                  # 04BC _ C1. E5, 08
        mov     bpl, byte ptr [r15]                         # 04BF _ 41: 8A. 2F
        shl     eax, 8                                  # 04C2 _ C1. E0, 08
        inc     r15                                     # 04C5 _ 49: FF. C7
$_018:  mov     edi, eax                                # 04C8 _ 8B. F8
        shr     eax, 11                                 # 04CA _ C1. E8, 0B
        imul    eax, edx                                # 04CD _ 0F AF. C2
        cmp     ebp, eax                                # 04D0 _ 3B. E8
        jc      $_021                                   # 04D2 _ 0F 82, 00000088
        sub     edi, eax                                # 04D8 _ 2B. F8
        sub     ebp, eax                                # 04DA _ 2B. E8
        mov     eax, edi                                # 04DC _ 8B. C7
        mov     edi, edx                                # 04DE _ 8B. FA
        shr     edx, 5                                  # 04E0 _ C1. EA, 05
        sub     edi, edx                                # 04E3 _ 2B. FA
        mov     word ptr [r11], di                          # 04E5 _ 66 41: 89. 3B
        add     r11, 496                                # 04E9 _ 49: 81. C3, 000001F0
        mov     ebx, 1                                  # 04F0 _ BB, 00000001
        movzx   ecx, word ptr [r11+0x2]                     # 04F5 _ 41: 0F B7. 4B, 02
# Filling space: 0x6
# Filler type: Multi-byte NOP
#       .byte 0x66, 0x0F, 0x1F, 0x44, 0x00, 0x00

.ALIGN  8
$_019:  movzx   edx, word ptr [r11+rbx*4]                   # 0500 _ 41: 0F B7. 14 9B
        add     ebx, ebx                                # 0505 _ 03. DB
        cmp     eax, 16777216                           # 0507 _ 3D, 01000000
        jnc     $_020                                   # 050C _ 73, 0C
        shl     ebp, 8                                  # 050E _ C1. E5, 08
        mov     bpl, byte ptr [r15]                         # 0511 _ 41: 8A. 2F
        shl     eax, 8                                  # 0514 _ C1. E0, 08
        inc     r15                                     # 0517 _ 49: FF. C7
$_020:  mov     edi, eax                                # 051A _ 8B. F8
        shr     eax, 11                                 # 051C _ C1. E8, 0B
        imul    eax, ecx                                # 051F _ 0F AF. C1
        sub     edi, eax                                # 0522 _ 2B. F8
        mov     esi, ebp                                # 0524 _ 8B. F5
        sub     ebp, eax                                # 0526 _ 2B. E8
        cmovnc  eax, edi                                # 0528 _ 0F 43. C7
        movzx   edi, word ptr [r11+rbx*2+0x2]               # 052B _ 41: 0F B7. 7C 5B, 02
        cmovnc  edx, edi                                # 0531 _ 0F 43. D7
        mov     edi, 31                                 # 0534 _ BF, 0000001F
        cmovc   ebp, esi                                # 0539 _ 0F 42. EE
        mov     esi, ebx                                # 053C _ 8B. F3
        cmovc   edi, r10d                               # 053E _ 41: 0F 42. FA
        sbb     ebx, -1                                 # 0542 _ 83. DB, FF
        sub     edi, ecx                                # 0545 _ 2B. F9
        sar     edi, 5                                  # 0547 _ C1. FF, 05
        add     edi, ecx                                # 054A _ 03. F9
        mov     word ptr [r11+rsi], di                      # 054C _ 66 41: 89. 3C 33
        mov     ecx, edx                                # 0551 _ 8B. CA
        cmp     ebx, 64                                 # 0553 _ 83. FB, 40
        jc      $_019                                   # 0556 _ 72, A8
        mov     r12d, 237                               # 0558 _ 41: BC, 000000ED
        jmp     $_023                                   # 055E _ EB, 67

$_021:  mov     edi, r10d                               # 0560 _ 41: 8B. FA
        sub     edi, edx                                # 0563 _ 2B. FA
        shr     edi, 5                                  # 0565 _ C1. EF, 05
        add     edx, edi                                # 0568 _ 03. D7
        mov     word ptr [r11], dx                          # 056A _ 66 41: 89. 13
        add     r11, rcx                                # 056E _ 4C: 03. D9
        movzx   edx, word ptr [r11+0x2]                     # 0571 _ 41: 0F B7. 53, 02
        movzx   ecx, word ptr [r11+0x4]                     # 0576 _ 41: 0F B7. 4B, 04
        cmp     eax, 16777216                           # 057B _ 3D, 01000000
        jnc     $_022                                   # 0580 _ 73, 0C
        shl     ebp, 8                                  # 0582 _ C1. E5, 08
        mov     bpl, byte ptr [r15]                         # 0585 _ 41: 8A. 2F
        shl     eax, 8                                  # 0588 _ C1. E0, 08
        inc     r15                                     # 058B _ 49: FF. C7
$_022:  mov     edi, eax                                # 058E _ 8B. F8
        shr     eax, 11                                 # 0590 _ C1. E8, 0B
        imul    eax, edx                                # 0593 _ 0F AF. C2
        sub     edi, eax                                # 0596 _ 2B. F8
        mov     esi, ebp                                # 0598 _ 8B. F5
        sub     ebp, eax                                # 059A _ 2B. E8
        cmovnc  eax, edi                                # 059C _ 0F 43. C7
        movzx   edi, word ptr [r11+0x6]                     # 059F _ 41: 0F B7. 7B, 06
        cmovnc  ecx, edi                                # 05A4 _ 0F 43. CF
        mov     edi, 31                                 # 05A7 _ BF, 0000001F
        cmovc   ebp, esi                                # 05AC _ 0F 42. EE
        cmovc   edi, r10d                               # 05AF _ 41: 0F 42. FA
        mov     ebx, 2                                  # 05B3 _ BB, 00000002
        sbb     ebx, -1                                 # 05B8 _ 83. DB, FF
        sub     edi, edx                                # 05BB _ 2B. FA
        sar     edi, 5                                  # 05BD _ C1. FF, 05
        add     edi, edx                                # 05C0 _ 03. FA
        mov     word ptr [r11+0x2], di                      # 05C2 _ 66 41: 89. 7B, 02
$_023:  movzx   edx, word ptr [r11+rbx*4]                   # 05C7 _ 41: 0F B7. 14 9B
        add     ebx, ebx                                # 05CC _ 03. DB
        cmp     eax, 16777216                           # 05CE _ 3D, 01000000
        jnc     $_024                                   # 05D3 _ 73, 0C
        shl     ebp, 8                                  # 05D5 _ C1. E5, 08
        mov     bpl, byte ptr [r15]                         # 05D8 _ 41: 8A. 2F
        shl     eax, 8                                  # 05DB _ C1. E0, 08
        inc     r15                                     # 05DE _ 49: FF. C7
$_024:  mov     edi, eax                                # 05E1 _ 8B. F8
        shr     eax, 11                                 # 05E3 _ C1. E8, 0B
        imul    eax, ecx                                # 05E6 _ 0F AF. C1
        sub     edi, eax                                # 05E9 _ 2B. F8
        mov     esi, ebp                                # 05EB _ 8B. F5
        sub     ebp, eax                                # 05ED _ 2B. E8
        cmovnc  eax, edi                                # 05EF _ 0F 43. C7
        movzx   edi, word ptr [r11+rbx*2+0x2]               # 05F2 _ 41: 0F B7. 7C 5B, 02
        cmovnc  edx, edi                                # 05F8 _ 0F 43. D7
        mov     edi, 31                                 # 05FB _ BF, 0000001F
        cmovc   ebp, esi                                # 0600 _ 0F 42. EE
        mov     esi, ebx                                # 0603 _ 8B. F3
        cmovc   edi, r10d                               # 0605 _ 41: 0F 42. FA
        sbb     ebx, -1                                 # 0609 _ 83. DB, FF
        sub     edi, ecx                                # 060C _ 2B. F9
        sar     edi, 5                                  # 060E _ C1. FF, 05
        add     edi, ecx                                # 0611 _ 03. F9
        mov     word ptr [r11+rsi], di                      # 0613 _ 66 41: 89. 3C 33
        add     ebx, ebx                                # 0618 _ 03. DB
        cmp     eax, 16777216                           # 061A _ 3D, 01000000
        jnc     $_025                                   # 061F _ 73, 0C
        shl     ebp, 8                                  # 0621 _ C1. E5, 08
        mov     bpl, byte ptr [r15]                         # 0624 _ 41: 8A. 2F
        shl     eax, 8                                  # 0627 _ C1. E0, 08
        inc     r15                                     # 062A _ 49: FF. C7
$_025:  mov     edi, eax                                # 062D _ 8B. F8
        shr     eax, 11                                 # 062F _ C1. E8, 0B
        imul    eax, edx                                # 0632 _ 0F AF. C2
        sub     edi, eax                                # 0635 _ 2B. F8
        mov     esi, ebp                                # 0637 _ 8B. F5
        sub     ebp, eax                                # 0639 _ 2B. E8
        cmovnc  eax, edi                                # 063B _ 0F 43. C7
        mov     edi, 31                                 # 063E _ BF, 0000001F
        cmovc   ebp, esi                                # 0643 _ 0F 42. EE
        mov     esi, ebx                                # 0646 _ 8B. F3
        cmovc   edi, r10d                               # 0648 _ 41: 0F 42. FA
        sbb     ebx, r12d                               # 064C _ 41: 1B. DC
        sub     edi, edx                                # 064F _ 2B. FA
        sar     edi, 5                                  # 0651 _ C1. FF, 05
        add     edi, edx                                # 0654 _ 03. FA
        mov     word ptr [r11+rsi], di                      # 0656 _ 66 41: 89. 3C 33
        mov     r11, qword ptr [rsp+0x30]                   # 065B _ 4C: 8B. 5C 24, 30
        cmp     r8d, 24                                 # 0660 _ 41: 83. F8, 18
        jc      $_040                                   # 0664 _ 0F 82, 000003FE
        mov     edi, 5                                  # 066A _ BF, 00000005
        cmp     ebx, 5                                  # 066F _ 83. FB, 05
        cmovc   edi, ebx                                # 0672 _ 0F 42. FB
        add     r11, -128                               # 0675 _ 49: 83. C3, 80
        shl     edi, 7                                  # 0679 _ C1. E7, 07
        add     r11, rdi                                # 067C _ 4C: 03. DF
        mov     r12d, ebx                               # 067F _ 44: 8B. E3
        movzx   ecx, word ptr [r11+0x2]                     # 0682 _ 41: 0F B7. 4B, 02
        movzx   edx, word ptr [r11+0x4]                     # 0687 _ 41: 0F B7. 53, 04
        cmp     eax, 16777216                           # 068C _ 3D, 01000000
        jnc     $_026                                   # 0691 _ 73, 0C
        shl     ebp, 8                                  # 0693 _ C1. E5, 08
        mov     bpl, byte ptr [r15]                         # 0696 _ 41: 8A. 2F
        shl     eax, 8                                  # 0699 _ C1. E0, 08
        inc     r15                                     # 069C _ 49: FF. C7
$_026:  mov     edi, eax                                # 069F _ 8B. F8
        shr     eax, 11                                 # 06A1 _ C1. E8, 0B
        imul    eax, ecx                                # 06A4 _ 0F AF. C1
        sub     edi, eax                                # 06A7 _ 2B. F8
        mov     esi, ebp                                # 06A9 _ 8B. F5
        sub     ebp, eax                                # 06AB _ 2B. E8
        cmovnc  eax, edi                                # 06AD _ 0F 43. C7
        movzx   edi, word ptr [r11+0x6]                     # 06B0 _ 41: 0F B7. 7B, 06
        cmovnc  edx, edi                                # 06B5 _ 0F 43. D7
        mov     edi, 31                                 # 06B8 _ BF, 0000001F
        cmovc   ebp, esi                                # 06BD _ 0F 42. EE
        cmovc   edi, r10d                               # 06C0 _ 41: 0F 42. FA
        mov     ebx, 2                                  # 06C4 _ BB, 00000002
        sbb     ebx, -1                                 # 06C9 _ 83. DB, FF
        sub     edi, ecx                                # 06CC _ 2B. F9
        sar     edi, 5                                  # 06CE _ C1. FF, 05
        add     edi, ecx                                # 06D1 _ 03. F9
        mov     word ptr [r11+0x2], di                      # 06D3 _ 66 41: 89. 7B, 02
        movzx   ecx, word ptr [r11+rbx*4]                   # 06D8 _ 41: 0F B7. 0C 9B
        add     ebx, ebx                                # 06DD _ 03. DB
        cmp     eax, 16777216                           # 06DF _ 3D, 01000000
        jnc     $_027                                   # 06E4 _ 73, 0C
        shl     ebp, 8                                  # 06E6 _ C1. E5, 08
        mov     bpl, byte ptr [r15]                         # 06E9 _ 41: 8A. 2F
        shl     eax, 8                                  # 06EC _ C1. E0, 08
        inc     r15                                     # 06EF _ 49: FF. C7
$_027:  mov     edi, eax                                # 06F2 _ 8B. F8
        shr     eax, 11                                 # 06F4 _ C1. E8, 0B
        imul    eax, edx                                # 06F7 _ 0F AF. C2
        sub     edi, eax                                # 06FA _ 2B. F8
        mov     esi, ebp                                # 06FC _ 8B. F5
        sub     ebp, eax                                # 06FE _ 2B. E8
        cmovnc  eax, edi                                # 0700 _ 0F 43. C7
        movzx   edi, word ptr [r11+rbx*2+0x2]               # 0703 _ 41: 0F B7. 7C 5B, 02
        cmovnc  ecx, edi                                # 0709 _ 0F 43. CF
        mov     edi, 31                                 # 070C _ BF, 0000001F
        cmovc   ebp, esi                                # 0711 _ 0F 42. EE
        mov     esi, ebx                                # 0714 _ 8B. F3
        cmovc   edi, r10d                               # 0716 _ 41: 0F 42. FA
        sbb     ebx, -1                                 # 071A _ 83. DB, FF
        sub     edi, edx                                # 071D _ 2B. FA
        sar     edi, 5                                  # 071F _ C1. FF, 05
        add     edi, edx                                # 0722 _ 03. FA
        mov     word ptr [r11+rsi], di                      # 0724 _ 66 41: 89. 3C 33
        movzx   edx, word ptr [r11+rbx*4]                   # 0729 _ 41: 0F B7. 14 9B
        add     ebx, ebx                                # 072E _ 03. DB
        cmp     eax, 16777216                           # 0730 _ 3D, 01000000
        jnc     $_028                                   # 0735 _ 73, 0C
        shl     ebp, 8                                  # 0737 _ C1. E5, 08
        mov     bpl, byte ptr [r15]                         # 073A _ 41: 8A. 2F
        shl     eax, 8                                  # 073D _ C1. E0, 08
        inc     r15                                     # 0740 _ 49: FF. C7
$_028:  mov     edi, eax                                # 0743 _ 8B. F8
        shr     eax, 11                                 # 0745 _ C1. E8, 0B
        imul    eax, ecx                                # 0748 _ 0F AF. C1
        sub     edi, eax                                # 074B _ 2B. F8
        mov     esi, ebp                                # 074D _ 8B. F5
        sub     ebp, eax                                # 074F _ 2B. E8
        cmovnc  eax, edi                                # 0751 _ 0F 43. C7
        movzx   edi, word ptr [r11+rbx*2+0x2]               # 0754 _ 41: 0F B7. 7C 5B, 02
        cmovnc  edx, edi                                # 075A _ 0F 43. D7
        mov     edi, 31                                 # 075D _ BF, 0000001F
        cmovc   ebp, esi                                # 0762 _ 0F 42. EE
        mov     esi, ebx                                # 0765 _ 8B. F3
        cmovc   edi, r10d                               # 0767 _ 41: 0F 42. FA
        sbb     ebx, -1                                 # 076B _ 83. DB, FF
        sub     edi, ecx                                # 076E _ 2B. F9
        sar     edi, 5                                  # 0770 _ C1. FF, 05
        add     edi, ecx                                # 0773 _ 03. F9
        mov     word ptr [r11+rsi], di                      # 0775 _ 66 41: 89. 3C 33
        movzx   ecx, word ptr [r11+rbx*4]                   # 077A _ 41: 0F B7. 0C 9B
        add     ebx, ebx                                # 077F _ 03. DB
        cmp     eax, 16777216                           # 0781 _ 3D, 01000000
        jnc     $_029                                   # 0786 _ 73, 0C
        shl     ebp, 8                                  # 0788 _ C1. E5, 08
        mov     bpl, byte ptr [r15]                         # 078B _ 41: 8A. 2F
        shl     eax, 8                                  # 078E _ C1. E0, 08
        inc     r15                                     # 0791 _ 49: FF. C7
$_029:  mov     edi, eax                                # 0794 _ 8B. F8
        shr     eax, 11                                 # 0796 _ C1. E8, 0B
        imul    eax, edx                                # 0799 _ 0F AF. C2
        sub     edi, eax                                # 079C _ 2B. F8
        mov     esi, ebp                                # 079E _ 8B. F5
        sub     ebp, eax                                # 07A0 _ 2B. E8
        cmovnc  eax, edi                                # 07A2 _ 0F 43. C7
        movzx   edi, word ptr [r11+rbx*2+0x2]               # 07A5 _ 41: 0F B7. 7C 5B, 02
        cmovnc  ecx, edi                                # 07AB _ 0F 43. CF
        mov     edi, 31                                 # 07AE _ BF, 0000001F
        cmovc   ebp, esi                                # 07B3 _ 0F 42. EE
        mov     esi, ebx                                # 07B6 _ 8B. F3
        cmovc   edi, r10d                               # 07B8 _ 41: 0F 42. FA
        sbb     ebx, -1                                 # 07BC _ 83. DB, FF
        sub     edi, edx                                # 07BF _ 2B. FA
        sar     edi, 5                                  # 07C1 _ C1. FF, 05
        add     edi, edx                                # 07C4 _ 03. FA
        mov     word ptr [r11+rsi], di                      # 07C6 _ 66 41: 89. 3C 33
        movzx   edx, word ptr [r11+rbx*4]                   # 07CB _ 41: 0F B7. 14 9B
        add     ebx, ebx                                # 07D0 _ 03. DB
        cmp     eax, 16777216                           # 07D2 _ 3D, 01000000
        jnc     $_030                                   # 07D7 _ 73, 0C
        shl     ebp, 8                                  # 07D9 _ C1. E5, 08
        mov     bpl, byte ptr [r15]                         # 07DC _ 41: 8A. 2F
        shl     eax, 8                                  # 07DF _ C1. E0, 08
        inc     r15                                     # 07E2 _ 49: FF. C7
$_030:  mov     edi, eax                                # 07E5 _ 8B. F8
        shr     eax, 11                                 # 07E7 _ C1. E8, 0B
        imul    eax, ecx                                # 07EA _ 0F AF. C1
        sub     edi, eax                                # 07ED _ 2B. F8
        mov     esi, ebp                                # 07EF _ 8B. F5
        sub     ebp, eax                                # 07F1 _ 2B. E8
        cmovnc  eax, edi                                # 07F3 _ 0F 43. C7
        movzx   edi, word ptr [r11+rbx*2+0x2]               # 07F6 _ 41: 0F B7. 7C 5B, 02
        cmovnc  edx, edi                                # 07FC _ 0F 43. D7
        mov     edi, 31                                 # 07FF _ BF, 0000001F
        cmovc   ebp, esi                                # 0804 _ 0F 42. EE
        mov     esi, ebx                                # 0807 _ 8B. F3
        cmovc   edi, r10d                               # 0809 _ 41: 0F 42. FA
        sbb     ebx, -1                                 # 080D _ 83. DB, FF
        sub     edi, ecx                                # 0810 _ 2B. F9
        sar     edi, 5                                  # 0812 _ C1. FF, 05
        add     edi, ecx                                # 0815 _ 03. F9
        mov     word ptr [r11+rsi], di                      # 0817 _ 66 41: 89. 3C 33
        mov     ecx, ebx                                # 081C _ 8B. CB
        add     ebx, ebx                                # 081E _ 03. DB
        cmp     eax, 16777216                           # 0820 _ 3D, 01000000
        jnc     $_031                                   # 0825 _ 73, 0C
        shl     ebp, 8                                  # 0827 _ C1. E5, 08
        mov     bpl, byte ptr [r15]                         # 082A _ 41: 8A. 2F
        shl     eax, 8                                  # 082D _ C1. E0, 08
        inc     r15                                     # 0830 _ 49: FF. C7
$_031:  mov     edi, eax                                # 0833 _ 8B. F8
        shr     eax, 11                                 # 0835 _ C1. E8, 0B
        imul    eax, edx                                # 0838 _ 0F AF. C2
        sub     edi, eax                                # 083B _ 2B. F8
        mov     esi, ebp                                # 083D _ 8B. F5
        sub     ebp, eax                                # 083F _ 2B. E8
        cmovnc  eax, edi                                # 0841 _ 0F 43. C7
        mov     edi, 31                                 # 0844 _ BF, 0000001F
        cmovc   ebp, esi                                # 0849 _ 0F 42. EE
        mov     esi, ebx                                # 084C _ 8B. F3
        cmovc   edi, r10d                               # 084E _ 41: 0F 42. FA
        sbb     ebx, 63                                 # 0852 _ 83. DB, 3F
        sub     edi, edx                                # 0855 _ 2B. FA
        sar     edi, 5                                  # 0857 _ C1. FF, 05
        add     edi, edx                                # 085A _ 03. FA
        mov     word ptr [r11+rsi], di                      # 085C _ 66 41: 89. 3C 33
        and     ebx, 0x03                               # 0861 _ 83. E3, 03
        mov     r11, qword ptr [rsp+0x30]                   # 0864 _ 4C: 8B. 5C 24, 30
        cmp     ecx, 39                                 # 0869 _ 83. F9, 27
        jc      $_068                                   # 086C _ 0F 82, 0000088E
        sub     ecx, 37                                 # 0872 _ 83. E9, 25
        or      ebx, 0x02                               # 0875 _ 83. CB, 02
        movzx   edx, word ptr [r11+0x2]                     # 0878 _ 41: 0F B7. 53, 02
        shl     ebx, 5                                  # 087D _ C1. E3, 05
        lea     r9, [r11+0x4]                           # 0880 _ 4D: 8D. 4B, 04
        jmp     $_033                                   # 0884 _ EB, 2C

# Filling space: 0x1A
# Filler type: Multi-byte NOP
#       .byte 0x0F, 0x1F, 0x80, 0x00, 0x00, 0x00, 0x00, 0x0F
#       .byte 0x1F, 0x80, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x1F
#       .byte 0x80, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x1F, 0x44
#       .byte 0x00, 0x00

.ALIGN  16
$_032:  shr     eax, 1                                  # 08A0 _ D1. E8
        mov     edi, ebp                                # 08A2 _ 8B. FD
        sub     ebp, eax                                # 08A4 _ 2B. E8
        cmovs   ebp, edi                                # 08A6 _ 0F 48. EF
        cmovns  ebx, esi                                # 08A9 _ 0F 49. DE
        dec     ecx                                     # 08AC _ FF. C9
        jz      $_034                                   # 08AE _ 74, 30
        add     ebx, ebx                                # 08B0 _ 03. DB
$_033:  lea     esi, [rbx+0x10]                         # 08B2 _ 8D. 73, 10
        cmp     eax, 16777216                           # 08B5 _ 3D, 01000000
# Note: Immediate operand could be made smaller by sign extension
        jnc     $_032                                   # 08BA _ 0F 83, FFFFFFE0
        shl     ebp, 8                                  # 08C0 _ C1. E5, 08
        mov     bpl, byte ptr [r15]                         # 08C3 _ 41: 8A. 2F
        shl     eax, 8                                  # 08C6 _ C1. E0, 08
        inc     r15                                     # 08C9 _ 49: FF. C7
        jmp     $_032                                   # 08CC _ EB, D2

# Filling space: 0x12
# Filler type: Multi-byte NOP
#       .byte 0x0F, 0x1F, 0x80, 0x00, 0x00, 0x00, 0x00, 0x0F
#       .byte 0x1F, 0x80, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x1F
#       .byte 0x40, 0x00

.ALIGN  16
$_034:  movzx   ecx, word ptr [r9]                          # 08E0 _ 41: 0F B7. 09
        cmp     eax, 16777216                           # 08E4 _ 3D, 01000000
        jnc     $_035                                   # 08E9 _ 73, 0C
        shl     ebp, 8                                  # 08EB _ C1. E5, 08
        mov     bpl, byte ptr [r15]                         # 08EE _ 41: 8A. 2F
        shl     eax, 8                                  # 08F1 _ C1. E0, 08
        inc     r15                                     # 08F4 _ 49: FF. C7
$_035:  mov     edi, eax                                # 08F7 _ 8B. F8
        shr     eax, 11                                 # 08F9 _ C1. E8, 0B
        imul    eax, edx                                # 08FC _ 0F AF. C2
        sub     edi, eax                                # 08FF _ 2B. F8
        mov     esi, ebp                                # 0901 _ 8B. F5
        sub     ebp, eax                                # 0903 _ 2B. E8
        cmovnc  eax, edi                                # 0905 _ 0F 43. C7
        movzx   edi, word ptr [r11+0x6]                     # 0908 _ 41: 0F B7. 7B, 06
        cmovnc  ecx, edi                                # 090D _ 0F 43. CF
        cmovc   ebp, esi                                # 0910 _ 0F 42. EE
        mov     edi, 31                                 # 0913 _ BF, 0000001F
        cmovc   edi, r10d                               # 0918 _ 41: 0F 42. FA
        lea     rsi, [r11+0x6]                          # 091C _ 49: 8D. 73, 06
        cmovnc  r9, rsi                                 # 0920 _ 4C: 0F 43. CE
        sub     edi, edx                                # 0924 _ 2B. FA
        sar     edi, 5                                  # 0926 _ C1. FF, 05
        add     edi, edx                                # 0929 _ 03. FA
        mov     word ptr [r11+0x2], di                      # 092B _ 66 41: 89. 7B, 02
        add     r9, 4                                   # 0930 _ 49: 83. C1, 04
        movzx   edx, word ptr [r9]                          # 0934 _ 41: 0F B7. 11
        cmp     eax, 16777216                           # 0938 _ 3D, 01000000
        jnc     $_036                                   # 093D _ 73, 0C
        shl     ebp, 8                                  # 093F _ C1. E5, 08
        mov     bpl, byte ptr [r15]                         # 0942 _ 41: 8A. 2F
        shl     eax, 8                                  # 0945 _ C1. E0, 08
        inc     r15                                     # 0948 _ 49: FF. C7
$_036:  mov     edi, eax                                # 094B _ 8B. F8
        shr     eax, 11                                 # 094D _ C1. E8, 0B
        imul    eax, ecx                                # 0950 _ 0F AF. C1
        sub     edi, eax                                # 0953 _ 2B. F8
        mov     esi, ebp                                # 0955 _ 8B. F5
        sub     ebp, eax                                # 0957 _ 2B. E8
        cmovnc  eax, edi                                # 0959 _ 0F 43. C7
        movzx   edi, word ptr [r9+0x4]                      # 095C _ 41: 0F B7. 79, 04
        cmovnc  edx, edi                                # 0961 _ 0F 43. D7
        cmovc   ebp, esi                                # 0964 _ 0F 42. EE
        mov     edi, 31                                 # 0967 _ BF, 0000001F
        cmovc   edi, r10d                               # 096C _ 41: 0F 42. FA
        lea     rsi, [r9+0x4]                           # 0970 _ 49: 8D. 71, 04
        cmovnc  r9, rsi                                 # 0974 _ 4C: 0F 43. CE
        sub     edi, ecx                                # 0978 _ 2B. F9
        sar     edi, 5                                  # 097A _ C1. FF, 05
        add     edi, ecx                                # 097D _ 03. F9
        mov     word ptr [rsi-0x8], di                      # 097F _ 66: 89. 7E, F8
        add     r9, 8                                   # 0983 _ 49: 83. C1, 08
        movzx   ecx, word ptr [r9]                          # 0987 _ 41: 0F B7. 09
        cmp     eax, 16777216                           # 098B _ 3D, 01000000
        jnc     $_037                                   # 0990 _ 73, 0C
        shl     ebp, 8                                  # 0992 _ C1. E5, 08
        mov     bpl, byte ptr [r15]                         # 0995 _ 41: 8A. 2F
        shl     eax, 8                                  # 0998 _ C1. E0, 08
        inc     r15                                     # 099B _ 49: FF. C7
$_037:  mov     edi, eax                                # 099E _ 8B. F8
        shr     eax, 11                                 # 09A0 _ C1. E8, 0B
        imul    eax, edx                                # 09A3 _ 0F AF. C2
        sub     edi, eax                                # 09A6 _ 2B. F8
        mov     esi, ebp                                # 09A8 _ 8B. F5
        sub     ebp, eax                                # 09AA _ 2B. E8
        cmovnc  eax, edi                                # 09AC _ 0F 43. C7
        movzx   edi, word ptr [r9+0x8]                      # 09AF _ 41: 0F B7. 79, 08
        cmovnc  ecx, edi                                # 09B4 _ 0F 43. CF
        cmovc   ebp, esi                                # 09B7 _ 0F 42. EE
        mov     edi, 31                                 # 09BA _ BF, 0000001F
        cmovc   edi, r10d                               # 09BF _ 41: 0F 42. FA
        lea     rsi, [r9+0x8]                           # 09C3 _ 49: 8D. 71, 08
        cmovnc  r9, rsi                                 # 09C7 _ 4C: 0F 43. CE
        sub     edi, edx                                # 09CB _ 2B. FA
        sar     edi, 5                                  # 09CD _ C1. FF, 05
        add     edi, edx                                # 09D0 _ 03. FA
        mov     word ptr [rsi-0x10], di                     # 09D2 _ 66: 89. 7E, F0
        sub     r9, r11                                 # 09D6 _ 4D: 2B. CB
        shr     r9d, 1                                  # 09D9 _ 41: D1. E9
        or      ebx, r9d                                # 09DC _ 41: 0B. D9
        cmp     eax, 16777216                           # 09DF _ 3D, 01000000
        jnc     $_038                                   # 09E4 _ 73, 0C
        shl     ebp, 8                                  # 09E6 _ C1. E5, 08
        mov     bpl, byte ptr [r15]                         # 09E9 _ 41: 8A. 2F
        shl     eax, 8                                  # 09EC _ C1. E0, 08
        inc     r15                                     # 09EF _ 49: FF. C7
$_038:  mov     edi, eax                                # 09F2 _ 8B. F8
        shr     eax, 11                                 # 09F4 _ C1. E8, 0B
        imul    eax, ecx                                # 09F7 _ 0F AF. C1
        sub     edi, eax                                # 09FA _ 2B. F8
        mov     esi, ebp                                # 09FC _ 8B. F5
        sub     ebp, eax                                # 09FE _ 2B. E8
        cmovnc  eax, edi                                # 0A00 _ 0F 43. C7
# Note: Address size prefix should be avoided
        lea     edi, [ebx-0x8]                          # 0A03 _ 67: 8D. 7B, F8
        cmovc   ebx, edi                                # 0A07 _ 0F 42. DF
        cmovc   ebp, esi                                # 0A0A _ 0F 42. EE
        mov     edi, 31                                 # 0A0D _ BF, 0000001F
        cmovc   edi, r10d                               # 0A12 _ 41: 0F 42. FA
        sub     edi, ecx                                # 0A16 _ 2B. F9
        sar     edi, 5                                  # 0A18 _ C1. FF, 05
        add     edi, ecx                                # 0A1B _ 03. F9
        mov     word ptr [r11+r9*2], di                     # 0A1D _ 66 43: 89. 3C 4B
$_039:  mov     esi, dword ptr [rsp+0x70]                   # 0A22 _ 8B. 74 24, 70
        mov     ecx, dword ptr [rsp+0x74]                   # 0A26 _ 8B. 4C 24, 74
        mov     edx, dword ptr [rsp+0x78]                   # 0A2A _ 8B. 54 24, 78
        mov     edi, dword ptr [rsp+0x5C]                   # 0A2E _ 8B. 7C 24, 5C
        test    edi, edi                                # 0A32 _ 85. FF
        cmove   edi, r13d                               # 0A34 _ 41: 0F 44. FD
        cmp     ebx, edi                                # 0A38 _ 3B. DF
        jnc     $_075                                   # 0A3A _ 0F 83, 000007B3
        inc     ebx                                     # 0A40 _ FF. C3
        mov     dword ptr [rsp+0x70], ebx                   # 0A42 _ 89. 5C 24, 70
        mov     ebx, r12d                               # 0A46 _ 41: 8B. DC
        mov     dword ptr [rsp+0x74], esi                   # 0A49 _ 89. 74 24, 74
        mov     dword ptr [rsp+0x78], ecx                   # 0A4D _ 89. 4C 24, 78
        mov     dword ptr [rsp+0x7C], edx                   # 0A51 _ 89. 54 24, 7C
        cmp     r8d, 38                                 # 0A55 _ 41: 83. F8, 26
        mov     r8d, 14                                 # 0A59 _ 41: B8, 0000000E
        mov     edi, 20                                 # 0A5F _ BF, 00000014
        cmovnc  r8d, edi                                # 0A64 _ 44: 0F 43. C7
$_040:  mov     rdx, qword ptr [rsp+0x40]                   # 0A68 _ 48: 8B. 54 24, 40
        sub     rdx, r14                                # 0A6D _ 49: 2B. D6
        je      $_073                                   # 0A70 _ 0F 84, 00000741
        cmp     rdx, rbx                                # 0A76 _ 48: 3B. D3
        cmovnc  edx, ebx                                # 0A79 _ 0F 43. D3
        mov     r12, qword ptr [rsp+0x38]                   # 0A7C _ 4C: 8B. 64 24, 38
        mov     ecx, dword ptr [rsp+0x70]                   # 0A81 _ 8B. 4C 24, 70
        mov     rdi, r14                                # 0A85 _ 49: 8B. FE
        add     r14, rdx                                # 0A88 _ 4C: 03. F2
        add     r13d, edx                               # 0A8B _ 44: 03. EA
        sub     ebx, edx                                # 0A8E _ 2B. DA
        mov     dword ptr [rsp+0x64], ebx                   # 0A90 _ 89. 5C 24, 64
        sub     rdi, r12                                # 0A94 _ 49: 2B. FC
        sub     rdi, rcx                                # 0A97 _ 48: 2B. F9
        jnc     $_041                                   # 0A9A _ 73, 14
        mov     rcx, qword ptr [rsp+0x28]                   # 0A9C _ 48: 8B. 4C 24, 28
        add     rdi, rcx                                # 0AA1 _ 48: 03. F9
        sub     rcx, rdi                                # 0AA4 _ 48: 2B. CF
        cmp     rdx, rcx                                # 0AA7 _ 48: 3B. D1
        ja      $_071                                   # 0AAA _ 0F 87, 000006DB
$_041:  add     rdi, r12                                # 0AB0 _ 49: 03. FC
        movzx   ebx, byte ptr [rdi]                         # 0AB3 _ 0F B6. 1F
        add     rdi, rdx                                # 0AB6 _ 48: 03. FA
        neg     rdx                                     # 0AB9 _ 48: F7. DA
$_042:  dec     r14                                     # 0ABC _ 49: FF. CE
        mov     ecx, dword ptr [rsp+0x58]                   # 0ABF _ 8B. 4C 24, 58
        and     ecx, r13d                               # 0AC3 _ 41: 23. CD
        shl     ecx, 5                                  # 0AC6 _ C1. E1, 05
        lea     rsi, [r11+r8]                           # 0AC9 _ 4B: 8D. 34 03
        inc     rdx                                     # 0ACD _ 48: FF. C2
        jz      $_044                                   # 0AD0 _ 74, 1B
# Filling space: 0xE
# Filler type: Multi-byte NOP
#       .byte 0x0F, 0x1F, 0x80, 0x00, 0x00, 0x00, 0x00, 0x0F
#       .byte 0x1F, 0x80, 0x00, 0x00, 0x00, 0x00

.ALIGN  16
$_043:  mov     byte ptr [r14+rdx], bl                      # 0AE0 _ 41: 88. 1C 16
        movzx   ebx, byte ptr [rdi+rdx]                     # 0AE4 _ 0F B6. 1C 17
        inc     rdx                                     # 0AE8 _ 48: FF. C2
        jnz     $_043                                   # 0AEB _ 75, F3
$_044:  mov     byte ptr [r14], bl                          # 0AED _ 41: 88. 1E
        inc     r14                                     # 0AF0 _ 49: FF. C6
        cmp     r15, qword ptr [rsp+0x48]                   # 0AF3 _ 4C: 3B. 7C 24, 48
        jnc     $_076                                   # 0AF8 _ 0F 83, 00000705
        cmp     r14, qword ptr [rsp+0x40]                   # 0AFE _ 4C: 3B. 74 24, 40
        jnc     $_076                                   # 0B03 _ 0F 83, 000006FA
$_045:  movzx   edx, word ptr [rsi+rcx-0x200]               # 0B09 _ 0F B7. 94 0E, FFFFFE00
        cmp     eax, 16777216                           # 0B11 _ 3D, 01000000
        jnc     $_046                                   # 0B16 _ 73, 0C
        shl     ebp, 8                                  # 0B18 _ C1. E5, 08
        mov     bpl, byte ptr [r15]                         # 0B1B _ 41: 8A. 2F
        shl     eax, 8                                  # 0B1E _ C1. E0, 08
        inc     r15                                     # 0B21 _ 49: FF. C7
$_046:  mov     edi, eax                                # 0B24 _ 8B. F8
        shr     eax, 11                                 # 0B26 _ C1. E8, 0B
        imul    eax, edx                                # 0B29 _ 0F AF. C2
        cmp     ebp, eax                                # 0B2C _ 3B. E8
        jnc     $_014                                   # 0B2E _ 0F 83, FFFFF8E0
        mov     edi, r13d                               # 0B34 _ 41: 8B. FD
        shl     edi, 8                                  # 0B37 _ C1. E7, 08
        add     ebx, edi                                # 0B3A _ 03. DF
        and     ebx, dword ptr [rsp+0x54]                   # 0B3C _ 23. 5C 24, 54
        add     rsi, rcx                                # 0B40 _ 48: 03. F1
        mov     ecx, dword ptr [rsp+0x50]                   # 0B43 _ 8B. 4C 24, 50
        lea     ebx, [rbx+rbx*2]                        # 0B47 _ 8D. 1C 5B
        add     r11, 640                                # 0B4A _ 49: 81. C3, 00000280
        shl     ebx, cl                                 # 0B51 _ D3. E3
        add     r11, rbx                                # 0B53 _ 4C: 03. DB
        mov     edi, r10d                               # 0B56 _ 41: 8B. FA
        sub     edi, edx                                # 0B59 _ 2B. FA
        shr     edi, 5                                  # 0B5B _ C1. EF, 05
        add     edx, edi                                # 0B5E _ 03. D7
        mov     word ptr [rsi-0x200], dx                    # 0B60 _ 66: 89. 96, FFFFFE00
        inc     r13d                                    # 0B67 _ 41: FF. C5
        mov     ecx, dword ptr [rsp+0x70]                   # 0B6A _ 8B. 4C 24, 70
        mov     qword ptr [rsp+0x68], r14                   # 0B6E _ 4C: 89. 74 24, 68
        lea     edi, [r8-0xC]                           # 0B73 _ 41: 8D. 78, F4
        sub     r8d, 6                                  # 0B77 _ 41: 83. E8, 06
        cmp     r8d, 14                                 # 0B7B _ 41: 83. F8, 0E
        cmovnc  r8d, edi                                # 0B7F _ 44: 0F 43. C7
        sub     r14, r12                                # 0B83 _ 4D: 2B. F4
        sub     r14, rcx                                # 0B86 _ 4C: 2B. F1
        jnc     $_047                                   # 0B89 _ 73, 05
        add     r14, qword ptr [rsp+0x28]                   # 0B8B _ 4C: 03. 74 24, 28
$_047:  movzx   r9d, byte ptr [r12+r14]                     # 0B90 _ 47: 0F B6. 0C 34
        mov     r12d, 512                               # 0B95 _ 41: BC, 00000200
        shl     r9d, 2                                  # 0B9B _ 41: C1. E1, 02
        mov     r14d, r12d                              # 0B9F _ 45: 8B. F4
        and     r14d, r9d                               # 0BA2 _ 45: 23. F1
        movzx   ecx, word ptr [r11+r14+0x202]               # 0BA5 _ 43: 0F B7. 8C 33, 00000202
        lea     rdx, [r11+r14+0x202]                    # 0BAE _ 4B: 8D. 94 33, 00000202
        xor     r12d, r14d                              # 0BB6 _ 45: 33. E6
        add     r9d, r9d                                # 0BB9 _ 45: 03. C9
        cmp     eax, 16777216                           # 0BBC _ 3D, 01000000
        jnc     $_048                                   # 0BC1 _ 73, 0C
        shl     ebp, 8                                  # 0BC3 _ C1. E5, 08
        mov     bpl, byte ptr [r15]                         # 0BC6 _ 41: 8A. 2F
        shl     eax, 8                                  # 0BC9 _ C1. E0, 08
        inc     r15                                     # 0BCC _ 49: FF. C7
$_048:  mov     edi, eax                                # 0BCF _ 8B. F8
        shr     eax, 11                                 # 0BD1 _ C1. E8, 0B
        imul    eax, ecx                                # 0BD4 _ 0F AF. C1
        sub     edi, eax                                # 0BD7 _ 2B. F8
        mov     esi, ebp                                # 0BD9 _ 8B. F5
        sub     ebp, eax                                # 0BDB _ 2B. E8
        cmovnc  r12d, r14d                              # 0BDD _ 45: 0F 43. E6
        mov     r14d, r9d                               # 0BE1 _ 45: 8B. F1
        cmovnc  eax, edi                                # 0BE4 _ 0F 43. C7
        mov     edi, 31                                 # 0BE7 _ BF, 0000001F
        cmovc   ebp, esi                                # 0BEC _ 0F 42. EE
        cmovc   edi, r10d                               # 0BEF _ 41: 0F 42. FA
        mov     ebx, 0                                  # 0BF3 _ BB, 00000000
        sbb     ebx, -3                                 # 0BF8 _ 83. DB, FD
        sub     edi, ecx                                # 0BFB _ 2B. F9
        sar     edi, 5                                  # 0BFD _ C1. FF, 05
        add     edi, ecx                                # 0C00 _ 03. F9
        mov     word ptr [rdx], di                          # 0C02 _ 66: 89. 3A
        and     r14d, r12d                              # 0C05 _ 45: 23. F4
        lea     rdx, [r11+r12]                          # 0C08 _ 4B: 8D. 14 23
        add     rdx, r14                                # 0C0C _ 49: 03. D6
        movzx   ecx, word ptr [rdx+rbx*2]                   # 0C0F _ 0F B7. 0C 5A
        xor     r12d, r14d                              # 0C13 _ 45: 33. E6
        add     ebx, ebx                                # 0C16 _ 03. DB
        add     r9d, r9d                                # 0C18 _ 45: 03. C9
        cmp     eax, 16777216                           # 0C1B _ 3D, 01000000
        jnc     $_049                                   # 0C20 _ 73, 0C
        shl     ebp, 8                                  # 0C22 _ C1. E5, 08
        mov     bpl, byte ptr [r15]                         # 0C25 _ 41: 8A. 2F
        shl     eax, 8                                  # 0C28 _ C1. E0, 08
        inc     r15                                     # 0C2B _ 49: FF. C7
$_049:  mov     edi, eax                                # 0C2E _ 8B. F8
        shr     eax, 11                                 # 0C30 _ C1. E8, 0B
        imul    eax, ecx                                # 0C33 _ 0F AF. C1
        sub     edi, eax                                # 0C36 _ 2B. F8
        mov     esi, ebp                                # 0C38 _ 8B. F5
        sub     ebp, eax                                # 0C3A _ 2B. E8
        cmovnc  r12d, r14d                              # 0C3C _ 45: 0F 43. E6
        mov     r14d, r9d                               # 0C40 _ 45: 8B. F1
        cmovnc  eax, edi                                # 0C43 _ 0F 43. C7
        mov     edi, 31                                 # 0C46 _ BF, 0000001F
        cmovc   ebp, esi                                # 0C4B _ 0F 42. EE
        mov     esi, ebx                                # 0C4E _ 8B. F3
        cmovc   edi, r10d                               # 0C50 _ 41: 0F 42. FA
        sbb     ebx, -1                                 # 0C54 _ 83. DB, FF
        sub     edi, ecx                                # 0C57 _ 2B. F9
        sar     edi, 5                                  # 0C59 _ C1. FF, 05
        add     edi, ecx                                # 0C5C _ 03. F9
        mov     word ptr [rdx+rsi], di                      # 0C5E _ 66: 89. 3C 32
        and     r14d, r12d                              # 0C62 _ 45: 23. F4
        lea     rdx, [r11+r12]                          # 0C65 _ 4B: 8D. 14 23
        add     rdx, r14                                # 0C69 _ 49: 03. D6
        movzx   ecx, word ptr [rdx+rbx*2]                   # 0C6C _ 0F B7. 0C 5A
        xor     r12d, r14d                              # 0C70 _ 45: 33. E6
        add     ebx, ebx                                # 0C73 _ 03. DB
        add     r9d, r9d                                # 0C75 _ 45: 03. C9
        cmp     eax, 16777216                           # 0C78 _ 3D, 01000000
        jnc     $_050                                   # 0C7D _ 73, 0C
        shl     ebp, 8                                  # 0C7F _ C1. E5, 08
        mov     bpl, byte ptr [r15]                         # 0C82 _ 41: 8A. 2F
        shl     eax, 8                                  # 0C85 _ C1. E0, 08
        inc     r15                                     # 0C88 _ 49: FF. C7
$_050:  mov     edi, eax                                # 0C8B _ 8B. F8
        shr     eax, 11                                 # 0C8D _ C1. E8, 0B
        imul    eax, ecx                                # 0C90 _ 0F AF. C1
        sub     edi, eax                                # 0C93 _ 2B. F8
        mov     esi, ebp                                # 0C95 _ 8B. F5
        sub     ebp, eax                                # 0C97 _ 2B. E8
        cmovnc  r12d, r14d                              # 0C99 _ 45: 0F 43. E6
        mov     r14d, r9d                               # 0C9D _ 45: 8B. F1
        cmovnc  eax, edi                                # 0CA0 _ 0F 43. C7
        mov     edi, 31                                 # 0CA3 _ BF, 0000001F
        cmovc   ebp, esi                                # 0CA8 _ 0F 42. EE
        mov     esi, ebx                                # 0CAB _ 8B. F3
        cmovc   edi, r10d                               # 0CAD _ 41: 0F 42. FA
        sbb     ebx, -1                                 # 0CB1 _ 83. DB, FF
        sub     edi, ecx                                # 0CB4 _ 2B. F9
        sar     edi, 5                                  # 0CB6 _ C1. FF, 05
        add     edi, ecx                                # 0CB9 _ 03. F9
        mov     word ptr [rdx+rsi], di                      # 0CBB _ 66: 89. 3C 32
        and     r14d, r12d                              # 0CBF _ 45: 23. F4
        lea     rdx, [r11+r12]                          # 0CC2 _ 4B: 8D. 14 23
        add     rdx, r14                                # 0CC6 _ 49: 03. D6
        movzx   ecx, word ptr [rdx+rbx*2]                   # 0CC9 _ 0F B7. 0C 5A
        xor     r12d, r14d                              # 0CCD _ 45: 33. E6
        add     ebx, ebx                                # 0CD0 _ 03. DB
        add     r9d, r9d                                # 0CD2 _ 45: 03. C9
        cmp     eax, 16777216                           # 0CD5 _ 3D, 01000000
        jnc     $_051                                   # 0CDA _ 73, 0C
        shl     ebp, 8                                  # 0CDC _ C1. E5, 08
        mov     bpl, byte ptr [r15]                         # 0CDF _ 41: 8A. 2F
        shl     eax, 8                                  # 0CE2 _ C1. E0, 08
        inc     r15                                     # 0CE5 _ 49: FF. C7
$_051:  mov     edi, eax                                # 0CE8 _ 8B. F8
        shr     eax, 11                                 # 0CEA _ C1. E8, 0B
        imul    eax, ecx                                # 0CED _ 0F AF. C1
        sub     edi, eax                                # 0CF0 _ 2B. F8
        mov     esi, ebp                                # 0CF2 _ 8B. F5
        sub     ebp, eax                                # 0CF4 _ 2B. E8
        cmovnc  r12d, r14d                              # 0CF6 _ 45: 0F 43. E6
        mov     r14d, r9d                               # 0CFA _ 45: 8B. F1
        cmovnc  eax, edi                                # 0CFD _ 0F 43. C7
        mov     edi, 31                                 # 0D00 _ BF, 0000001F
        cmovc   ebp, esi                                # 0D05 _ 0F 42. EE
        mov     esi, ebx                                # 0D08 _ 8B. F3
        cmovc   edi, r10d                               # 0D0A _ 41: 0F 42. FA
        sbb     ebx, -1                                 # 0D0E _ 83. DB, FF
        sub     edi, ecx                                # 0D11 _ 2B. F9
        sar     edi, 5                                  # 0D13 _ C1. FF, 05
        add     edi, ecx                                # 0D16 _ 03. F9
        mov     word ptr [rdx+rsi], di                      # 0D18 _ 66: 89. 3C 32
        and     r14d, r12d                              # 0D1C _ 45: 23. F4
        lea     rdx, [r11+r12]                          # 0D1F _ 4B: 8D. 14 23
        add     rdx, r14                                # 0D23 _ 49: 03. D6
        movzx   ecx, word ptr [rdx+rbx*2]                   # 0D26 _ 0F B7. 0C 5A
        xor     r12d, r14d                              # 0D2A _ 45: 33. E6
        add     ebx, ebx                                # 0D2D _ 03. DB
        add     r9d, r9d                                # 0D2F _ 45: 03. C9
        cmp     eax, 16777216                           # 0D32 _ 3D, 01000000
        jnc     $_052                                   # 0D37 _ 73, 0C
        shl     ebp, 8                                  # 0D39 _ C1. E5, 08
        mov     bpl, byte ptr [r15]                         # 0D3C _ 41: 8A. 2F
        shl     eax, 8                                  # 0D3F _ C1. E0, 08
        inc     r15                                     # 0D42 _ 49: FF. C7
$_052:  mov     edi, eax                                # 0D45 _ 8B. F8
        shr     eax, 11                                 # 0D47 _ C1. E8, 0B
        imul    eax, ecx                                # 0D4A _ 0F AF. C1
        sub     edi, eax                                # 0D4D _ 2B. F8
        mov     esi, ebp                                # 0D4F _ 8B. F5
        sub     ebp, eax                                # 0D51 _ 2B. E8
        cmovnc  r12d, r14d                              # 0D53 _ 45: 0F 43. E6
        mov     r14d, r9d                               # 0D57 _ 45: 8B. F1
        cmovnc  eax, edi                                # 0D5A _ 0F 43. C7
        mov     edi, 31                                 # 0D5D _ BF, 0000001F
        cmovc   ebp, esi                                # 0D62 _ 0F 42. EE
        mov     esi, ebx                                # 0D65 _ 8B. F3
        cmovc   edi, r10d                               # 0D67 _ 41: 0F 42. FA
        sbb     ebx, -1                                 # 0D6B _ 83. DB, FF
        sub     edi, ecx                                # 0D6E _ 2B. F9
        sar     edi, 5                                  # 0D70 _ C1. FF, 05
        add     edi, ecx                                # 0D73 _ 03. F9
        mov     word ptr [rdx+rsi], di                      # 0D75 _ 66: 89. 3C 32
        and     r14d, r12d                              # 0D79 _ 45: 23. F4
        lea     rdx, [r11+r12]                          # 0D7C _ 4B: 8D. 14 23
        add     rdx, r14                                # 0D80 _ 49: 03. D6
        movzx   ecx, word ptr [rdx+rbx*2]                   # 0D83 _ 0F B7. 0C 5A
        xor     r12d, r14d                              # 0D87 _ 45: 33. E6
        add     ebx, ebx                                # 0D8A _ 03. DB
        add     r9d, r9d                                # 0D8C _ 45: 03. C9
        cmp     eax, 16777216                           # 0D8F _ 3D, 01000000
        jnc     $_053                                   # 0D94 _ 73, 0C
        shl     ebp, 8                                  # 0D96 _ C1. E5, 08
        mov     bpl, byte ptr [r15]                         # 0D99 _ 41: 8A. 2F
        shl     eax, 8                                  # 0D9C _ C1. E0, 08
        inc     r15                                     # 0D9F _ 49: FF. C7
$_053:  mov     edi, eax                                # 0DA2 _ 8B. F8
        shr     eax, 11                                 # 0DA4 _ C1. E8, 0B
        imul    eax, ecx                                # 0DA7 _ 0F AF. C1
        sub     edi, eax                                # 0DAA _ 2B. F8
        mov     esi, ebp                                # 0DAC _ 8B. F5
        sub     ebp, eax                                # 0DAE _ 2B. E8
        cmovnc  r12d, r14d                              # 0DB0 _ 45: 0F 43. E6
        mov     r14d, r9d                               # 0DB4 _ 45: 8B. F1
        cmovnc  eax, edi                                # 0DB7 _ 0F 43. C7
        mov     edi, 31                                 # 0DBA _ BF, 0000001F
        cmovc   ebp, esi                                # 0DBF _ 0F 42. EE
        mov     esi, ebx                                # 0DC2 _ 8B. F3
        cmovc   edi, r10d                               # 0DC4 _ 41: 0F 42. FA
        sbb     ebx, -1                                 # 0DC8 _ 83. DB, FF
        sub     edi, ecx                                # 0DCB _ 2B. F9
        sar     edi, 5                                  # 0DCD _ C1. FF, 05
        add     edi, ecx                                # 0DD0 _ 03. F9
        mov     word ptr [rdx+rsi], di                      # 0DD2 _ 66: 89. 3C 32
        and     r14d, r12d                              # 0DD6 _ 45: 23. F4
        lea     rdx, [r11+r12]                          # 0DD9 _ 4B: 8D. 14 23
        add     rdx, r14                                # 0DDD _ 49: 03. D6
        movzx   ecx, word ptr [rdx+rbx*2]                   # 0DE0 _ 0F B7. 0C 5A
        xor     r12d, r14d                              # 0DE4 _ 45: 33. E6
        add     ebx, ebx                                # 0DE7 _ 03. DB
        add     r9d, r9d                                # 0DE9 _ 45: 03. C9
        cmp     eax, 16777216                           # 0DEC _ 3D, 01000000
        jnc     $_054                                   # 0DF1 _ 73, 0C
        shl     ebp, 8                                  # 0DF3 _ C1. E5, 08
        mov     bpl, byte ptr [r15]                         # 0DF6 _ 41: 8A. 2F
        shl     eax, 8                                  # 0DF9 _ C1. E0, 08
        inc     r15                                     # 0DFC _ 49: FF. C7
$_054:  mov     edi, eax                                # 0DFF _ 8B. F8
        shr     eax, 11                                 # 0E01 _ C1. E8, 0B
        imul    eax, ecx                                # 0E04 _ 0F AF. C1
        sub     edi, eax                                # 0E07 _ 2B. F8
        mov     esi, ebp                                # 0E09 _ 8B. F5
        sub     ebp, eax                                # 0E0B _ 2B. E8
        cmovnc  r12d, r14d                              # 0E0D _ 45: 0F 43. E6
        mov     r14d, r9d                               # 0E11 _ 45: 8B. F1
        cmovnc  eax, edi                                # 0E14 _ 0F 43. C7
        mov     edi, 31                                 # 0E17 _ BF, 0000001F
        cmovc   ebp, esi                                # 0E1C _ 0F 42. EE
        mov     esi, ebx                                # 0E1F _ 8B. F3
        cmovc   edi, r10d                               # 0E21 _ 41: 0F 42. FA
        sbb     ebx, -1                                 # 0E25 _ 83. DB, FF
        sub     edi, ecx                                # 0E28 _ 2B. F9
        sar     edi, 5                                  # 0E2A _ C1. FF, 05
        add     edi, ecx                                # 0E2D _ 03. F9
        mov     word ptr [rdx+rsi], di                      # 0E2F _ 66: 89. 3C 32
        and     r14d, r12d                              # 0E33 _ 45: 23. F4
        lea     rdx, [r11+r12]                          # 0E36 _ 4B: 8D. 14 23
        add     rdx, r14                                # 0E3A _ 49: 03. D6
        movzx   ecx, word ptr [rdx+rbx*2]                   # 0E3D _ 0F B7. 0C 5A
        add     ebx, ebx                                # 0E41 _ 03. DB
        cmp     eax, 16777216                           # 0E43 _ 3D, 01000000
        jnc     $_055                                   # 0E48 _ 73, 0C
        shl     ebp, 8                                  # 0E4A _ C1. E5, 08
        mov     bpl, byte ptr [r15]                         # 0E4D _ 41: 8A. 2F
        shl     eax, 8                                  # 0E50 _ C1. E0, 08
        inc     r15                                     # 0E53 _ 49: FF. C7
$_055:  mov     edi, eax                                # 0E56 _ 8B. F8
        shr     eax, 11                                 # 0E58 _ C1. E8, 0B
        imul    eax, ecx                                # 0E5B _ 0F AF. C1
        sub     edi, eax                                # 0E5E _ 2B. F8
        mov     esi, ebp                                # 0E60 _ 8B. F5
        sub     ebp, eax                                # 0E62 _ 2B. E8
        cmovnc  eax, edi                                # 0E64 _ 0F 43. C7
        mov     edi, 31                                 # 0E67 _ BF, 0000001F
        cmovc   ebp, esi                                # 0E6C _ 0F 42. EE
        mov     esi, ebx                                # 0E6F _ 8B. F3
        cmovc   edi, r10d                               # 0E71 _ 41: 0F 42. FA
        sbb     ebx, 255                                # 0E75 _ 81. DB, 000000FF
        sub     edi, ecx                                # 0E7B _ 2B. F9
        sar     edi, 5                                  # 0E7D _ C1. FF, 05
        add     edi, ecx                                # 0E80 _ 03. F9
        mov     word ptr [rdx+rsi], di                      # 0E82 _ 66: 89. 3C 32
        mov     r11, qword ptr [rsp+0x30]                   # 0E86 _ 4C: 8B. 5C 24, 30
        mov     ecx, dword ptr [rsp+0x58]                   # 0E8B _ 8B. 4C 24, 58
        and     ecx, r13d                               # 0E8F _ 41: 23. CD
        shl     ecx, 5                                  # 0E92 _ C1. E1, 05
        lea     rsi, [r11+r8]                           # 0E95 _ 4B: 8D. 34 03
        mov     r14, qword ptr [rsp+0x68]                   # 0E99 _ 4C: 8B. 74 24, 68
        mov     byte ptr [r14], bl                          # 0E9E _ 41: 88. 1E
        inc     r14                                     # 0EA1 _ 49: FF. C6
        cmp     r15, qword ptr [rsp+0x48]                   # 0EA4 _ 4C: 3B. 7C 24, 48
        jnc     $_076                                   # 0EA9 _ 0F 83, 00000354
        cmp     r14, qword ptr [rsp+0x40]                   # 0EAF _ 4C: 3B. 74 24, 40
        jnc     $_076                                   # 0EB4 _ 0F 83, 00000349
$_056:  movzx   edx, word ptr [rsi+rcx-0x200]               # 0EBA _ 0F B7. 94 0E, FFFFFE00
        cmp     eax, 16777216                           # 0EC2 _ 3D, 01000000
        jnc     $_057                                   # 0EC7 _ 73, 0C
        shl     ebp, 8                                  # 0EC9 _ C1. E5, 08
        mov     bpl, byte ptr [r15]                         # 0ECC _ 41: 8A. 2F
        shl     eax, 8                                  # 0ECF _ C1. E0, 08
        inc     r15                                     # 0ED2 _ 49: FF. C7
$_057:  mov     edi, eax                                # 0ED5 _ 8B. F8
        shr     eax, 11                                 # 0ED7 _ C1. E8, 0B
        imul    eax, edx                                # 0EDA _ 0F AF. C2
        cmp     ebp, eax                                # 0EDD _ 3B. E8
        jnc     $_014                                   # 0EDF _ 0F 83, FFFFF52F
        mov     r9d, dword ptr [rsp+0x54]                   # 0EE5 _ 44: 8B. 4C 24, 54
        sub     r8d, 6                                  # 0EEA _ 41: 83. E8, 06
        jmp     $_003                                   # 0EEE _ E9, FFFFF210

# Filling space: 0xD
# Filler type: Multi-byte NOP
#       .byte 0x0F, 0x1F, 0x80, 0x00, 0x00, 0x00, 0x00, 0x66
#       .byte 0x0F, 0x1F, 0x44, 0x00, 0x00

.ALIGN  16
$_058:  mov     edi, r10d                               # 0F00 _ 41: 8B. FA
        sub     edi, edx                                # 0F03 _ 2B. FA
        shr     edi, 5                                  # 0F05 _ C1. EF, 05
        add     edx, edi                                # 0F08 _ 03. D7
        mov     word ptr [rsi+rcx-0xC00], dx                # 0F0A _ 66: 89. 94 0E, FFFFF400
        mov     r12, qword ptr [rsp+0x38]                   # 0F12 _ 4C: 8B. 64 24, 38
        mov     rdi, r14                                # 0F17 _ 49: 8B. FE
        mov     edx, dword ptr [rsp+0x70]                   # 0F1A _ 8B. 54 24, 70
        sub     rdi, r12                                # 0F1E _ 49: 2B. FC
        sub     r11, -2560                              # 0F21 _ 49: 81. EB, FFFFF600
        or      r8d, 0x02                               # 0F28 _ 41: 83. C8, 02
        inc     r13d                                    # 0F2C _ 41: FF. C5
        mov     ecx, dword ptr [rsp+0x58]                   # 0F2F _ 8B. 4C 24, 58
        and     ecx, r13d                               # 0F33 _ 41: 23. CD
        shl     ecx, 5                                  # 0F36 _ C1. E1, 05
        lea     rsi, [r11+r8]                           # 0F39 _ 4B: 8D. 34 03
        sub     rdi, rdx                                # 0F3D _ 48: 2B. FA
        jnc     $_059                                   # 0F40 _ 73, 05
        add     rdi, qword ptr [rsp+0x28]                   # 0F42 _ 48: 03. 7C 24, 28
$_059:  movzx   ebx, byte ptr [r12+rdi]                     # 0F47 _ 41: 0F B6. 1C 3C
        jmp     $_044                                   # 0F4C _ E9, FFFFFB9C

# Filling space: 0xF
# Filler type: Multi-byte NOP
#       .byte 0x0F, 0x1F, 0x80, 0x00, 0x00, 0x00, 0x00, 0x0F
#       .byte 0x1F, 0x80, 0x00, 0x00, 0x00, 0x00, 0x90

.ALIGN  16
$_060:  sub     edi, eax                                # 0F60 _ 2B. F8
        sub     ebp, eax                                # 0F62 _ 2B. E8
        mov     eax, edi                                # 0F64 _ 8B. C7
        mov     edi, edx                                # 0F66 _ 8B. FA
        shr     edx, 5                                  # 0F68 _ C1. EA, 05
        sub     edi, edx                                # 0F6B _ 2B. FA
        mov     word ptr [rsi+0x20], di                     # 0F6D _ 66: 89. 7E, 20
        cmp     r8d, 14                                 # 0F71 _ 41: 83. F8, 0E
        mov     r8d, 16                                 # 0F75 _ 41: B8, 00000010
        mov     edx, 22                                 # 0F7B _ BA, 00000016
        cmovnc  r8d, edx                                # 0F80 _ 44: 0F 43. C2
        add     r11, -2560                              # 0F84 _ 49: 81. C3, FFFFF600
        movzx   edx, word ptr [rsi+0x38]                    # 0F8B _ 0F B7. 56, 38
        cmp     eax, 16777216                           # 0F8F _ 3D, 01000000
        jnc     $_061                                   # 0F94 _ 73, 0C
        shl     ebp, 8                                  # 0F96 _ C1. E5, 08
        mov     bpl, byte ptr [r15]                         # 0F99 _ 41: 8A. 2F
        shl     eax, 8                                  # 0F9C _ C1. E0, 08
        inc     r15                                     # 0F9F _ 49: FF. C7
$_061:  mov     edi, eax                                # 0FA2 _ 8B. F8
        shr     eax, 11                                 # 0FA4 _ C1. E8, 0B
        imul    eax, edx                                # 0FA7 _ 0F AF. C2
        cmp     ebp, eax                                # 0FAA _ 3B. E8
        jnc     $_063                                   # 0FAC _ 73, 72
        mov     edi, r10d                               # 0FAE _ 41: 8B. FA
        sub     edi, edx                                # 0FB1 _ 2B. FA
        shr     edi, 5                                  # 0FB3 _ C1. EF, 05
        add     edx, edi                                # 0FB6 _ 03. D7
        mov     word ptr [rsi+0x38], dx                     # 0FB8 _ 66: 89. 56, 38
        movzx   edx, word ptr [rsi+rcx-0xC00]               # 0FBC _ 0F B7. 94 0E, FFFFF400
        cmp     eax, 16777216                           # 0FC4 _ 3D, 01000000
        jnc     $_062                                   # 0FC9 _ 73, 0C
        shl     ebp, 8                                  # 0FCB _ C1. E5, 08
        mov     bpl, byte ptr [r15]                         # 0FCE _ 41: 8A. 2F
        shl     eax, 8                                  # 0FD1 _ C1. E0, 08
        inc     r15                                     # 0FD4 _ 49: FF. C7
$_062:  mov     edi, eax                                # 0FD7 _ 8B. F8
        shr     eax, 11                                 # 0FD9 _ C1. E8, 0B
        imul    eax, edx                                # 0FDC _ 0F AF. C2
        cmp     ebp, eax                                # 0FDF _ 3B. E8
        jc      $_058                                   # 0FE1 _ 0F 82, FFFFFF19
        sub     edi, eax                                # 0FE7 _ 2B. F8
        sub     ebp, eax                                # 0FE9 _ 2B. E8
        mov     eax, edi                                # 0FEB _ 8B. C7
        mov     edi, edx                                # 0FED _ 8B. FA
        shr     edx, 5                                  # 0FEF _ C1. EA, 05
        sub     edi, edx                                # 0FF2 _ 2B. FA
        mov     word ptr [rsi+rcx-0xC00], di                # 0FF4 _ 66: 89. BC 0E, FFFFF400
        jmp     $_016                                   # 0FFC _ E9, FFFFF468

# Filling space: 0x1F
# Filler type: Multi-byte NOP
#       .byte 0x0F, 0x1F, 0x80, 0x00, 0x00, 0x00, 0x00, 0x0F
#       .byte 0x1F, 0x80, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x1F
#       .byte 0x80, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x1F, 0x80
#       .byte 0x00, 0x00, 0x00, 0x00, 0x0F, 0x1F, 0x00

.ALIGN  16
$_063:  sub     edi, eax                                # 1020 _ 2B. F8
        sub     ebp, eax                                # 1022 _ 2B. E8
        mov     eax, edi                                # 1024 _ 8B. C7
        mov     edi, edx                                # 1026 _ 8B. FA
        shr     edx, 5                                  # 1028 _ C1. EA, 05
        sub     edi, edx                                # 102B _ 2B. FA
        mov     word ptr [rsi+0x38], di                     # 102D _ 66: 89. 7E, 38
        mov     r9d, dword ptr [rsp+0x70]                   # 1031 _ 44: 8B. 4C 24, 70
        mov     ebx, dword ptr [rsp+0x74]                   # 1036 _ 8B. 5C 24, 74
        mov     dword ptr [rsp+0x74], r9d                   # 103A _ 44: 89. 4C 24, 74
        movzx   edx, word ptr [rsi+0x50]                    # 103F _ 0F B7. 56, 50
        cmp     eax, 16777216                           # 1043 _ 3D, 01000000
        jnc     $_064                                   # 1048 _ 73, 0C
        shl     ebp, 8                                  # 104A _ C1. E5, 08
        mov     bpl, byte ptr [r15]                         # 104D _ 41: 8A. 2F
        shl     eax, 8                                  # 1050 _ C1. E0, 08
        inc     r15                                     # 1053 _ 49: FF. C7
$_064:  mov     edi, eax                                # 1056 _ 8B. F8
        shr     eax, 11                                 # 1058 _ C1. E8, 0B
        imul    eax, edx                                # 105B _ 0F AF. C2
        cmp     ebp, eax                                # 105E _ 3B. E8
        jnc     $_065                                   # 1060 _ 73, 17
        mov     edi, r10d                               # 1062 _ 41: 8B. FA
        sub     edi, edx                                # 1065 _ 2B. FA
        shr     edi, 5                                  # 1067 _ C1. EF, 05
        add     edx, edi                                # 106A _ 03. D7
        mov     word ptr [rsi+0x50], dx                     # 106C _ 66: 89. 56, 50
        mov     dword ptr [rsp+0x70], ebx                   # 1070 _ 89. 5C 24, 70
        jmp     $_016                                   # 1074 _ E9, FFFFF3F0

$_065:  sub     edi, eax                                # 1079 _ 2B. F8
        sub     ebp, eax                                # 107B _ 2B. E8
        mov     eax, edi                                # 107D _ 8B. C7
        mov     edi, edx                                # 107F _ 8B. FA
        shr     edx, 5                                  # 1081 _ C1. EA, 05
        sub     edi, edx                                # 1084 _ 2B. FA
        mov     word ptr [rsi+0x50], di                     # 1086 _ 66: 89. 7E, 50
        mov     r9d, dword ptr [rsp+0x78]                   # 108A _ 44: 8B. 4C 24, 78
        mov     dword ptr [rsp+0x78], ebx                   # 108F _ 89. 5C 24, 78
        movzx   edx, word ptr [rsi+0x68]                    # 1093 _ 0F B7. 56, 68
        cmp     eax, 16777216                           # 1097 _ 3D, 01000000
        jnc     $_066                                   # 109C _ 73, 0C
        shl     ebp, 8                                  # 109E _ C1. E5, 08
        mov     bpl, byte ptr [r15]                         # 10A1 _ 41: 8A. 2F
        shl     eax, 8                                  # 10A4 _ C1. E0, 08
        inc     r15                                     # 10A7 _ 49: FF. C7
$_066:  mov     edi, eax                                # 10AA _ 8B. F8
        shr     eax, 11                                 # 10AC _ C1. E8, 0B
        imul    eax, edx                                # 10AF _ 0F AF. C2
        cmp     ebp, eax                                # 10B2 _ 3B. E8
        jnc     $_067                                   # 10B4 _ 73, 18
        mov     edi, r10d                               # 10B6 _ 41: 8B. FA
        sub     edi, edx                                # 10B9 _ 2B. FA
        shr     edi, 5                                  # 10BB _ C1. EF, 05
        add     edx, edi                                # 10BE _ 03. D7
        mov     word ptr [rsi+0x68], dx                     # 10C0 _ 66: 89. 56, 68
        mov     dword ptr [rsp+0x70], r9d                   # 10C4 _ 44: 89. 4C 24, 70
        jmp     $_016                                   # 10C9 _ E9, FFFFF39B

$_067:  sub     edi, eax                                # 10CE _ 2B. F8
        sub     ebp, eax                                # 10D0 _ 2B. E8
        mov     eax, edi                                # 10D2 _ 8B. C7
        mov     edi, edx                                # 10D4 _ 8B. FA
        shr     edx, 5                                  # 10D6 _ C1. EA, 05
        sub     edi, edx                                # 10D9 _ 2B. FA
        mov     word ptr [rsi+0x68], di                     # 10DB _ 66: 89. 7E, 68
        mov     ebx, dword ptr [rsp+0x7C]                   # 10DF _ 8B. 5C 24, 7C
        mov     dword ptr [rsp+0x7C], r9d                   # 10E3 _ 44: 89. 4C 24, 7C
        mov     dword ptr [rsp+0x70], ebx                   # 10E8 _ 89. 5C 24, 70
        jmp     $_016                                   # 10EC _ E9, FFFFF378

# Filling space: 0xF
# Filler type: Multi-byte NOP
#       .byte 0x0F, 0x1F, 0x80, 0x00, 0x00, 0x00, 0x00, 0x0F
#       .byte 0x1F, 0x80, 0x00, 0x00, 0x00, 0x00, 0x90

.ALIGN  16
$_068:  sub     ecx, 33                                 # 1100 _ 83. E9, 21
        jbe     $_039                                   # 1103 _ 0F 86, FFFFF919
        or      ebx, 0x02                               # 1109 _ 83. CB, 02
        shl     ebx, cl                                 # 110C _ D3. E3
        lea     rbx, [r11+rbx*2-0xCFE]                  # 110E _ 49: 8D. 9C 5B, FFFFF302
        mov     r9d, 2                                  # 1116 _ 41: B9, 00000002
# Filling space: 0x4
# Filler type: Multi-byte NOP
#       .byte 0x0F, 0x1F, 0x40, 0x00

.ALIGN  8
$_069:  movzx   edx, word ptr [rbx]                         # 1120 _ 0F B7. 13
        mov     r11, rbx                                # 1123 _ 4C: 8B. DB
        add     rbx, r9                                 # 1126 _ 49: 03. D9
        cmp     eax, 16777216                           # 1129 _ 3D, 01000000
        jnc     $_070                                   # 112E _ 73, 0C
        shl     ebp, 8                                  # 1130 _ C1. E5, 08
        mov     bpl, byte ptr [r15]                         # 1133 _ 41: 8A. 2F
        shl     eax, 8                                  # 1136 _ C1. E0, 08
        inc     r15                                     # 1139 _ 49: FF. C7
$_070:  mov     edi, eax                                # 113C _ 8B. F8
        shr     eax, 11                                 # 113E _ C1. E8, 0B
        imul    eax, edx                                # 1141 _ 0F AF. C2
        sub     edi, eax                                # 1144 _ 2B. F8
        mov     esi, ebp                                # 1146 _ 8B. F5
        sub     ebp, eax                                # 1148 _ 2B. E8
        cmovnc  eax, edi                                # 114A _ 0F 43. C7
        lea     rdi, [rbx+r9]                           # 114D _ 4A: 8D. 3C 0B
        cmovnc  rbx, rdi                                # 1151 _ 48: 0F 43. DF
        mov     edi, 31                                 # 1155 _ BF, 0000001F
        cmovc   ebp, esi                                # 115A _ 0F 42. EE
        cmovc   edi, r10d                               # 115D _ 41: 0F 42. FA
        add     r9d, r9d                                # 1161 _ 45: 03. C9
        sub     edi, edx                                # 1164 _ 2B. FA
        sar     edi, 5                                  # 1166 _ C1. FF, 05
        add     edi, edx                                # 1169 _ 03. FA
        mov     word ptr [r11], di                          # 116B _ 66 41: 89. 3B
        dec     ecx                                     # 116F _ FF. C9
        jnz     $_069                                   # 1171 _ 75, AD
        mov     r11, qword ptr [rsp+0x30]                   # 1173 _ 4C: 8B. 5C 24, 30
        sub     ebx, r9d                                # 1178 _ 41: 2B. D9
        sub     ebx, -3328                              # 117B _ 81. EB, FFFFF300
        sub     rbx, r11                                # 1181 _ 49: 2B. DB
        shr     ebx, 1                                  # 1184 _ D1. EB
        jmp     $_039                                   # 1186 _ E9, FFFFF897

$_071:  mov     rsi, rdi                                # 118B _ 48: 8B. F7
        mov     rdi, r12                                # 118E _ 49: 8B. FC
        mov     rcx, qword ptr [rsp+0x28]                   # 1191 _ 48: 8B. 4C 24, 28
        neg     rdx                                     # 1196 _ 48: F7. DA
$_072:  movzx   ebx, byte ptr [rdi+rsi]                     # 1199 _ 0F B6. 1C 37
        inc     rsi                                     # 119D _ 48: FF. C6
        mov     byte ptr [r14+rdx], bl                      # 11A0 _ 41: 88. 1C 16
        inc     rdx                                     # 11A4 _ 48: FF. C2
        cmp     rsi, rcx                                # 11A7 _ 48: 3B. F1
        jnz     $_072                                   # 11AA _ 75, ED
        movzx   ebx, byte ptr [rdi]                         # 11AC _ 0F B6. 1F
        sub     rdi, rdx                                # 11AF _ 48: 2B. FA
        jmp     $_042                                   # 11B2 _ E9, FFFFF905

$_073:  mov     dword ptr [rsp+0x64], ebx                   # 11B7 _ 89. 5C 24, 64
        jmp     $_076                                   # 11BB _ EB, 46

$_074:  add     r12d, 512                               # 11BD _ 41: 81. C4, 00000200
        mov     dword ptr [rsp+0x64], r12d                  # 11C4 _ 44: 89. 64 24, 64
        mov     dword ptr [rsp+0x70], ebx                   # 11C9 _ 89. 5C 24, 70
        mov     dword ptr [rsp+0x74], esi                   # 11CD _ 89. 74 24, 74
        mov     dword ptr [rsp+0x78], ecx                   # 11D1 _ 89. 4C 24, 78
        mov     dword ptr [rsp+0x7C], edx                   # 11D5 _ 89. 54 24, 7C
        cmp     r8d, 38                                 # 11D9 _ 41: 83. F8, 26
        mov     r8d, 14                                 # 11DD _ 41: B8, 0000000E
        mov     edi, 20                                 # 11E3 _ BF, 00000014
        cmovnc  r8d, edi                                # 11E8 _ 44: 0F 43. C7
        mov     ebx, 1                                  # 11EC _ BB, 00000001
        jmp     $_077                                   # 11F1 _ EB, 12

$_075:  inc     ebx                                     # 11F3 _ FF. C3
        jnz     $_074                                   # 11F5 _ 75, C6
        mov     dword ptr [rsp+0x64], 274                   # 11F7 _ C7. 44 24, 64, 00000112
        sub     r8d, 24                                 # 11FF _ 41: 83. E8, 18
$_076:  xor     ebx, ebx                                # 1203 _ 33. DB
$_077:  cmp     eax, 16777216                           # 1205 _ 3D, 01000000
        jnc     $_078                                   # 120A _ 73, 0C
        shl     ebp, 8                                  # 120C _ C1. E5, 08
        mov     bpl, byte ptr [r15]                         # 120F _ 41: 8A. 2F
        shl     eax, 8                                  # 1212 _ C1. E0, 08
        inc     r15                                     # 1215 _ 49: FF. C7
$_078:  mov     rcx, qword ptr [rsp+0x8]                    # 1218 _ 48: 8B. 4C 24, 08
        sub     r14, qword ptr [rsp+0x38]                   # 121D _ 4C: 2B. 74 24, 38
        mov     qword ptr [rcx+0x28], r14                   # 1222 _ 4C: 89. 71, 28
        mov     qword ptr [rcx+0x30], r15                   # 1226 _ 4C: 89. 79, 30
        mov     dword ptr [rcx+0x38], eax                   # 122A _ 89. 41, 38
        mov     dword ptr [rcx+0x3C], ebp                   # 122D _ 89. 69, 3C
        shr     r8d, 1                                  # 1230 _ 41: D1. E8
        mov     dword ptr [rcx+0x58], r8d                   # 1233 _ 44: 89. 41, 58
        mov     dword ptr [rcx+0x40], r13d                  # 1237 _ 44: 89. 69, 40
        mov     edi, dword ptr [rsp+0x64]                   # 123B _ 8B. 7C 24, 64
        mov     dword ptr [rcx+0x5C], edi                   # 123F _ 89. 79, 5C
        mov     edi, dword ptr [rsp+0x70]                   # 1242 _ 8B. 7C 24, 70
        mov     dword ptr [rcx+0x48], edi                   # 1246 _ 89. 79, 48
        mov     edi, dword ptr [rsp+0x74]                   # 1249 _ 8B. 7C 24, 74
        mov     dword ptr [rcx+0x4C], edi                   # 124D _ 89. 79, 4C
        mov     edi, dword ptr [rsp+0x78]                   # 1250 _ 8B. 7C 24, 78
        mov     dword ptr [rcx+0x50], edi                   # 1254 _ 89. 79, 50
        mov     edi, dword ptr [rsp+0x7C]                   # 1257 _ 8B. 7C 24, 7C
        mov     dword ptr [rcx+0x54], edi                   # 125B _ 89. 79, 54
        mov     eax, ebx                                # 125E _ 8B. C3
        mov     rsp, qword ptr [rsp]                        # 1260 _ 48: 8B. 24 24
        pop     r15                                     # 1264 _ 41: 5F
        pop     r14                                     # 1266 _ 41: 5E
        pop     r13                                     # 1268 _ 41: 5D
        pop     r12                                     # 126A _ 41: 5C
        pop     rbp                                     # 126C _ 5D
        pop     rbx                                     # 126D _ 5B
        ret                                             # 126E _ C3
        #ifdef __ELF__
        .size   sdk25_LzmaDec_DecodeReal_3, . - sdk25_LzmaDec_DecodeReal_3
#endif

# Return to AT&T syntax with destination operand last:
.att_syntax prefix 

#ifdef __ELF__
.section .note.GNU-stack,"",@progbits
#endif
