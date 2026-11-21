/* lzma_fastenc.h -- thin wrapper around the LZMA SDK 25.01 encoder (opaque handle API).
   Used only in -nobitexact mode: the produced stream is a standard LZMA stream (with end
   marker, no header), decodable by any LZMA decoder with the same lc/lp/pb and dictionary. */
#ifndef LZMA_FASTENC_H
#define LZMA_FASTENC_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct lzfe lzfe;

typedef struct
{
  unsigned dictSize;       /* dictionary (history) size in bytes */
  int      lc, lp, pb;     /* literal context / literal position / position state bits */
  int      algo;           /* 0 = fast, 1 = normal (optimal parsing) */
  int      fb;             /* number of fast bytes (nice match length), 5..273 */
  int      btMode;         /* 0 = hash chain, 1 = binary tree */
  int      numHashBytes;   /* 2..5 */
  unsigned mc;             /* match finder cycles (cut value) */
  int      numThreads;     /* 1 or 2 (2 = multithreaded match finder, binary tree mode only) */
} lzfe_props;

/* Stream callbacks. read: on input *size = buffer size, on output *size = bytes read
   (0 = end of stream); returns 0 on success. write: returns the number of bytes written. */
typedef int    (*lzfe_read_cb)  (void *aux, void *buf, size_t *size);
typedef size_t (*lzfe_write_cb) (void *aux, const void *buf, size_t size);
/* Allocator used for the big buffers (match finder tables, window). */
typedef void  *(*lzfe_alloc_cb) (size_t size);
typedef void   (*lzfe_free_cb)  (void *address);

/* Returns NULL on allocation failure or unsupported parameters. */
lzfe *lzfe_create (const lzfe_props *props, lzfe_alloc_cb big_alloc, lzfe_free_cb big_free);
void  lzfe_destroy (lzfe *e);

/* Both functions return an LZMA SDK SRes code (0 = SZ_OK). The handle may be reused for
   several consecutive encodings (each one produces an independent stream). */
int lzfe_encode_mem    (lzfe *e, unsigned char *dest, size_t *destLen, const unsigned char *src, size_t srcLen);
int lzfe_encode_stream (lzfe *e, lzfe_read_cb rd, void *rd_aux, lzfe_write_cb wr, void *wr_aux);

/* Bytes of match finder hash tables allocated by the SDK encoder for these parameters
   (used for memory accounting only). */
size_t lzfe_hash_bytes (unsigned dictSize, int btMode, int numHashBytes);

#ifdef __cplusplus
}
#endif
#endif
