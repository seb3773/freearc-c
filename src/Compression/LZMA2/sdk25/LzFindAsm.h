/* LzFindAsm.h -- FreeArc-c: x86-64 assembly binary-tree match finder kernels for the
   LZMA SDK 25.01 encoder (-nobitexact mode), see ../asm/LzFind25_x86_64.S.

   FA_MF_ASM is defined by the Makefile only for the x86-64 encoder objects that link the
   assembly (Linux ELF and MinGW-w64). The kernels return exactly the same matches and leave
   son[] in exactly the same state as the C code of LzFind.c / LzFindOpt.c, which stays the
   reference and the portable fallback (ARM64, other CPUs, or build with MF_ASM=0).
   Arguments are passed as one argument block, so the same entry works for both ABIs; the
   blocks live on the caller's stack (no heap allocation in the hot path). */
#ifndef LZ_FIND_ASM_H
#define LZ_FIND_ASM_H

#if defined(FA_MF_ASM) && !(defined(__x86_64__) || defined(_M_X64))
#  undef FA_MF_ASM
#endif

#ifdef FA_MF_ASM
#include <stddef.h>

typedef struct
{
  const Byte *cur;              /*  0 */
  CLzRef     *son;              /*  8 */
  UInt32     *distances;        /* 16 */
  UInt32      lenLimit;         /* 24 */
  UInt32      curMatch;         /* 28 */
  UInt32      pos;              /* 32 */
  UInt32      cyclicBufferPos;  /* 36 */
  UInt32      cyclicBufferSize; /* 40 */
  UInt32      cutValue;         /* 44 */
  UInt32      maxLen;           /* 48 */
} Lzf25BtArgs;

typedef struct
{
  const Byte   *lenLimit;       /*  0  cur + lenLimit - 1, exactly as passed to GetMatchesSpecN_2() */
  const Byte   *cur;            /*  8 */
  CLzRef       *son;            /* 16 */
  UInt32       *d;              /* 24 */
  const UInt32 *hash;           /* 32 */
  const UInt32 *limit;          /* 40 */
  const UInt32 *size;           /* 48 */
  size_t        pos;            /* 56 */
  size_t        cyclicBufferPos;/* 64 */
  size_t        maxLen;         /* 72 */
  UInt32        cutValue;       /* 80 */
  UInt32        cyclicBufferSize; /* 84 */
  UInt32       *posRes;         /* 88 */
} Lzf25NArgs;

#ifndef LZF_ASM_ABI   /* the ABI test harness sets __attribute__((ms_abi)) to run the Win64 entry on Linux */
#define LZF_ASM_ABI
#endif

EXTERN_C_BEGIN
LZF_ASM_ABI UInt32 *lzf25_bt_getmatches   (const Lzf25BtArgs *a);
LZF_ASM_ABI void    lzf25_bt_skip         (const Lzf25BtArgs *a);
LZF_ASM_ABI UInt32 *lzf25_bt_getmatches_n (const Lzf25NArgs  *a);
LZF_ASM_ABI void    fa_x86_64_cpu_init    (void);
EXTERN_C_END

typedef char lzf25_check_bt[(offsetof (Lzf25BtArgs, maxLen) == 48 && offsetof (Lzf25BtArgs, cutValue) == 44) ? 1 : -1];
typedef char lzf25_check_n [(offsetof (Lzf25NArgs, cutValue) == 80 && offsetof (Lzf25NArgs, posRes) == 88) ? 1 : -1];
#endif /* FA_MF_ASM */

#endif
