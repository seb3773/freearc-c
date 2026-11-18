/* Thin wrapper around the LZMA SDK 25.01 decoder (opaque handle API). */
#ifndef LZMA_FASTDEC_H
#define LZMA_FASTDEC_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct lzfd lzfd;
/* Returns NULL on allocation failure. */
lzfd *lzfd_create (unsigned lc, unsigned lp, unsigned pb, unsigned dicSize);
void  lzfd_destroy (lzfd *d);
/* Decode until dicLimit (absolute position in the dictionary buffer).
   *srcLen: in = available, out = consumed. *finishedWithMark set to 1 when the end
   marker was decoded. Returns 0 on success, 1 on data error. */
int   lzfd_decode (lzfd *d, size_t dicLimit, const unsigned char *src, size_t *srcLen, int *finishedWithMark);
unsigned char *lzfd_dic (lzfd *d);
size_t lzfd_dicPos (lzfd *d);
size_t lzfd_dicBufSize (lzfd *d);
void   lzfd_resetDicPos (lzfd *d);
#ifdef __cplusplus
}
#endif
#endif
