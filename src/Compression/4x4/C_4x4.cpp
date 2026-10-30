#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "C_4x4.h"
#include "../MultiThreading.h"

#define _4x4_VERSION     0
#define _4x4_HEADER_SIZE 8

static MemSize _4x4_max_out (MemSize insize)
{
  return insize + (insize/4) + 16*kb;
}

static MemSize _4x4_effective_block (_4x4_METHOD *method)
{
  if (method->BlockSize) return method->BlockSize;
#ifndef FREEARC_DECOMPRESS_ONLY
  MemSize dict = GetDictionary (method->Method);
  if (dict) return dict;
#endif
  return 8*mb;
}

struct LocalMemBuf
{
  BYTE *readPtr;
  int   readLeft;
  BYTE *writePtr;
  int   writeLeft;
};

static int local_mem_callback (const char *what, void *buf, int size, void *aux)
{
  LocalMemBuf *m = (LocalMemBuf *) aux;
  if (strequ (what, "read")) {
    int n = m->readLeft < size ? m->readLeft : size;
    if (n > 0) {
      memcpy (buf, m->readPtr, n);
      m->readPtr  += n;
      m->readLeft -= n;
    }
    return n;
  }
  if (strequ (what, "write")) {
    if (size > m->writeLeft) return FREEARC_ERRCODE_OUTBLOCK_TOO_SMALL;
    if (size > 0) {
      memcpy (m->writePtr, buf, size);
      m->writePtr  += size;
      m->writeLeft -= size;
    }
    return size;
  }
  return 0;
}

static int compress_mem_local (char *method, void *input, int inputSize, void *output, int outputSize)
{
#ifndef FREEARC_DECOMPRESS_ONLY
  LocalMemBuf m;
  m.readPtr = (BYTE *) input;  m.readLeft = inputSize;
  m.writePtr = (BYTE *) output; m.writeLeft = outputSize;
  int rc = Compress (method, local_mem_callback, &m);
  return rc < 0 ? rc : outputSize - m.writeLeft;
#else
  return FREEARC_ERRCODE_ONLY_DECOMPRESS;
#endif
}

static int decompress_mem_local (char *method, void *input, int inputSize, void *output, int outputSize)
{
  LocalMemBuf m;
  m.readPtr = (BYTE *) input;  m.readLeft = inputSize;
  m.writePtr = (BYTE *) output; m.writeLeft = outputSize;
  int rc = Decompress (method, local_mem_callback, &m);
  return rc < 0 ? rc : outputSize - m.writeLeft;
}

static double order0_compression_ratio (void *buf, size_t bufsize)
{
  if (bufsize == 0) return 1.0;
  size_t count1[256];
  size_t count2[256];
  size_t count3[256];
  size_t count4[256];
  memset (count1, 0, sizeof (count1));
  memset (count2, 0, sizeof (count2));
  memset (count3, 0, sizeof (count3));
  memset (count4, 0, sizeof (count4));

  byte *p = (byte *) buf;
  size_t i;
  for (i = 0; i + 3 < bufsize; i += 4) {
    count1[p[i]]++;
    count2[p[i+1]]++;
    count3[p[i+2]]++;
    count4[p[i+3]]++;
  }
  for (; i < bufsize; i++)
    count1[p[i]]++;

  double compressed = 0;
  double log2 = log (2.0);
  for (int c = 0; c < 256; c++) {
    size_t count = count1[c] + count2[c] + count3[c] + count4[c];
    if (count)
      compressed += count * log ((double) bufsize / (double) count) / log2 / 8.0;
  }
  return compressed / (double) bufsize;
}

static double parseDouble (char *param, int *error)
{
  if (*param == '=') param++;
  if (*param == '\0') { *error = 1; return 0; }
  char *end = NULL;
  double v = strtod (param, &end);
  if (end == param || (end && *end != '\0')) { *error = 1; return 0; }
  return v;
}

static void join_params (char **parameters, char *out, int outsize)
{
  out[0] = 0;
  while (*parameters) {
    int used = (int) strlen (out);
    if (used && used + 1 < outsize) {
      out[used++] = COMPRESSION_METHOD_PARAMETERS_DELIMITER;
      out[used] = 0;
    }
    strncopy (out + used, *parameters, outsize - used);
    parameters++;
  }
}

struct _4x4MTCompressor;
struct _4x4Job : WorkerThread
{
  _4x4MTCompressor *compressor;
  uint32 orig_size;
  void *CodecState;
  int init();
  int process();
  int done();
};

struct _4x4MTCompressor : MTCompressor<_4x4Job>
{
  int     direction;
  char    Method[MAX_METHOD_STRLEN];
  COMPRESSION_METHOD *cmethod;
  int     BlockSize;
  int     in_buf_size;
  int     out_buf_size;
  double  MinOrder0Ratio;
  CALLBACK_FUNC *cb;
  void    *cbdata;

  _4x4MTCompressor (int _direction, _4x4_METHOD *method, CALLBACK_FUNC *_callback, void *_auxdata)
  {
    direction = _direction;
    cb = _callback;
    cbdata = _auxdata;
    callback = _callback;
    auxdata = _auxdata;
    strncopy (Method, method->Method, MAX_METHOD_STRLEN);
    cmethod = ParseCompressionMethod (method->Method);
    BlockSize = (int) _4x4_effective_block (method);
    in_buf_size = BlockSize;
    out_buf_size = (int) (_4x4_HEADER_SIZE + _4x4_max_out (BlockSize));
    if (out_buf_size < BlockSize + _4x4_HEADER_SIZE)
      out_buf_size = BlockSize + _4x4_HEADER_SIZE;
    MinOrder0Ratio = method->MinOrder0Percents / 100.0;
  }

  ~_4x4MTCompressor()
  {
    delete cmethod;
    cmethod = NULL;
  }

  int main_cycle();
};

int _4x4Job::init()
{
  compressor = (_4x4MTCompressor *) task;
  InBuf = NULL;
  OutBuf = NULL;
  orig_size = 0;
  CodecState = NULL;
  if (compressor->direction == 0) {
    InBuf  = (char *) BigAlloc (compressor->in_buf_size);
    OutBuf = (char *) BigAlloc (compressor->out_buf_size);
    return (InBuf && OutBuf) ? 0 : FREEARC_ERRCODE_NOT_ENOUGH_MEMORY;
  }
  return 0;
}

int _4x4Job::done()
{
  if (CodecState && compressor && compressor->cmethod)
    compressor->cmethod->DeCompressMem ((COMPRESSION) compressor->direction, NULL, 0, NULL, NULL, NULL, NULL, &CodecState);
  CodecState = NULL;
  BigFree (OutBuf); OutBuf = NULL;
  BigFree (InBuf);  InBuf  = NULL;
  return 0;
}

int _4x4Job::process()
{
  if (compressor->direction == 0) {
#ifndef FREEARC_DECOMPRESS_ONLY
    int rc = FREEARC_ERRCODE_OUTBLOCK_TOO_SMALL;
    int out_cap = InSize;
    if (out_cap > compressor->out_buf_size - _4x4_HEADER_SIZE)
      out_cap = compressor->out_buf_size - _4x4_HEADER_SIZE;
    if (compressor->cmethod &&
        (compressor->MinOrder0Ratio == 0 || InSize < (int)(32*kb) ||
         order0_compression_ratio (InBuf, InSize) < compressor->MinOrder0Ratio)) {
      int OutputSize = out_cap;
      rc = compressor->cmethod->DeCompressMem (COMPRESS, InBuf, InSize, OutBuf + _4x4_HEADER_SIZE, &OutputSize, NULL, NULL, &CodecState);
      if (rc >= 0) rc = OutputSize;
    }
    if (rc == FREEARC_ERRCODE_OUTBLOCK_TOO_SMALL || (rc >= 0 && rc >= InSize)) {
      setvalue32 (OutBuf, (uint32) -1);
      setvalue32 (OutBuf + 4, (uint32) InSize);
      memcpy (OutBuf + _4x4_HEADER_SIZE, InBuf, InSize);
      return _4x4_HEADER_SIZE + InSize;
    }
    if (rc < 0) return rc;
    setvalue32 (OutBuf, (uint32) InSize);
    setvalue32 (OutBuf + 4, (uint32) rc);
    return _4x4_HEADER_SIZE + rc;
#else
    return FREEARC_ERRCODE_ONLY_DECOMPRESS;
#endif
  }

  if ((int32) orig_size == -1) {
    memcpy (OutBuf, InBuf, InSize);
    return InSize;
  }
  int rc = decompress_mem_local (compressor->Method, InBuf, InSize, OutBuf, (int) orig_size);
  if (rc < 0) return rc;
  if (rc != (int) orig_size) return FREEARC_ERRCODE_BAD_COMPRESSED_DATA;
  return rc;
}

int _4x4MTCompressor::main_cycle()
{
  if (direction == 0) {
#ifndef FREEARC_DECOMPRESS_ONLY
    unsigned char ver[_4x4_HEADER_SIZE > 4 ? 4 : 4];
    setvalue32 (ver, _4x4_VERSION);
    int w = cb ("write", ver, 4, cbdata);
    if (w < 0) return w;

    for (;;) {
      _4x4Job *job = FreeJobs.Get();
      if (job == NULL || errcode < 0) return errcode < 0 ? errcode : 0;
      int n = cb ("read", job->InBuf, BlockSize, cbdata);
      if (n < 0) return n;
      if (n == 0) return FREEARC_OK;
      job->InSize = n;
      WriterJobs.Put (job);
      job->StartOperation.Signal();
    }
#else
    return FREEARC_ERRCODE_ONLY_DECOMPRESS;
#endif
  }

  unsigned char ver[4];
  int n = cb ("read", ver, 4, cbdata);
  if (n == 0) return FREEARC_OK;
  if (n != 4) return n < 0 ? n : FREEARC_ERRCODE_READ;
  if (value32 (ver) != _4x4_VERSION) return FREEARC_ERRCODE_BAD_COMPRESSED_DATA;

  for (;;) {
    unsigned char hdr[_4x4_HEADER_SIZE];
    n = cb ("read", hdr, _4x4_HEADER_SIZE, cbdata);
    if (n == 0) return FREEARC_OK;
    if (n != _4x4_HEADER_SIZE) return n < 0 ? n : FREEARC_ERRCODE_READ;

    uint32 out_size = value32 (hdr);
    uint32 in_size  = value32 (hdr + 4);
    int need_in  = (int) in_size;
    int need_out = ((int32) out_size == -1) ? (int) in_size : (int) out_size;
    if (need_in < 0 || need_out < 0) return FREEARC_ERRCODE_BAD_COMPRESSED_DATA;

    _4x4Job *job = FreeJobs.Get();
    if (job == NULL || errcode < 0) return errcode < 0 ? errcode : 0;

    if (job->InBuf == NULL) {
      int alloc_in  = need_in;
      int alloc_out = need_out;
      if (alloc_in < BlockSize) alloc_in = BlockSize;
      if (alloc_out < BlockSize) alloc_out = BlockSize;
      job->InBuf  = (char *) BigAlloc (alloc_in);
      job->OutBuf = (char *) BigAlloc (alloc_out);
      if (!job->InBuf || !job->OutBuf) return FREEARC_ERRCODE_NOT_ENOUGH_MEMORY;
      in_buf_size = alloc_in;
      out_buf_size = alloc_out;
      BlockSize = alloc_out > alloc_in ? alloc_out : alloc_in;
    }
    if (need_in > in_buf_size || need_out > out_buf_size)
      return FREEARC_ERRCODE_BAD_COMPRESSED_DATA;

    if (need_in > 0) {
      n = cb ("read", job->InBuf, need_in, cbdata);
      if (n != need_in) return n < 0 ? n : FREEARC_ERRCODE_READ;
    }
    job->InSize = need_in;
    job->orig_size = out_size;
    WriterJobs.Put (job);
    job->StartOperation.Signal();
  }
}

_4x4_METHOD::_4x4_METHOD()
{
  strcpy (Method, "tor:3:1mb");
  NumThreads = 0;
  NumExtraBuffers = -1;
  BlockSize = 0;
  MinOrder0Percents = 99;
}

int _4x4_METHOD::doit (char *what, int param, void *data, CALLBACK_FUNC *callback)
{
  if (strequ (what, "has_progress?")) return 1;
  return COMPRESSION_METHOD::doit (what, param, data, callback);
}

int _4x4_METHOD::decompress (CALLBACK_FUNC *callback, void *auxdata)
{
  int save_once = compress_all_at_once;
  compress_all_at_once = 1;
  _4x4MTCompressor d (1, this, callback, auxdata);
  int rc = d.run();
  compress_all_at_once = save_once;
  return rc;
}

#ifndef FREEARC_DECOMPRESS_ONLY

int _4x4_METHOD::compress (CALLBACK_FUNC *callback, void *auxdata)
{
  int save_once = compress_all_at_once;
  compress_all_at_once = 1;
  _4x4MTCompressor c (0, this, callback, auxdata);
  int rc = c.run();
  compress_all_at_once = save_once;
  return rc;
}

MemSize _4x4_METHOD::GetCompressionMem (void)
{
  int t = GetNumThreads();
  int i = GetNumExtraBuffers();
  MemSize bs = _4x4_effective_block (this);
  MemSize tmem = ::GetCompressionMem (Method);
  MemSize imem = 2 * (_4x4_HEADER_SIZE + _4x4_max_out (bs));
  tmem += imem;
  return t * tmem + i * imem;
}

MemSize _4x4_METHOD::GetDictionary (void)
{
  return ::GetDictionary (Method);
}

void _4x4_METHOD::SetCompressionMem (MemSize mem)
{
  if (mem == 0) return;
  MemSize need = GetCompressionMem();
  if (mem >= need) return;
  int t = GetNumThreads();
  MemSize per = t > 0 ? GetCompressionMem() / t : GetCompressionMem();
  if (per == 0) per = 1;
  int nt = (int) (mem / per);
  if (nt < 1) nt = 1;
  NumThreads = nt;
  NumExtraBuffers = 0;
}

void _4x4_METHOD::SetDecompressionMem (MemSize mem)
{
  SetCompressionMem (mem);
}

void _4x4_METHOD::SetDictionary (MemSize dict)
{
  if (dict > 0)
    ::SetDictionary (Method, dict, Method);
}

void _4x4_METHOD::ShowCompressionMethod (char *buf)
{
  _4x4_METHOD defaults;
  char ThreadsStr[100], ExtraBuffersStr[100], BlockSizeStr[100], PercentsStr[100];
  sprintf (ThreadsStr, ":t%d", NumThreads);
  sprintf (ExtraBuffersStr, ":i%d", NumExtraBuffers);
  sprintf (PercentsStr, ":r%lg", MinOrder0Percents);
  showMem (BlockSize, BlockSizeStr);
  sprintf (buf, "4x4%s%s%s%s%s:%s",
           NumThreads != defaults.NumThreads ? ThreadsStr : "",
           NumExtraBuffers != defaults.NumExtraBuffers ? ExtraBuffersStr : "",
           BlockSize ? ":b" : "",
           BlockSize ? BlockSizeStr : "",
           MinOrder0Percents != defaults.MinOrder0Percents ? PercentsStr : "",
           Method);
}

#endif

MemSize _4x4_METHOD::GetDecompressionMem (void)
{
  int t = GetNumThreads();
  int i = GetNumExtraBuffers();
  MemSize bs = BlockSize ? BlockSize : 8*mb;
  MemSize tmem = ::GetDecompressionMem (Method);
  MemSize imem = 2 * (_4x4_HEADER_SIZE + _4x4_max_out (bs));
  tmem += imem;
  return t * tmem + i * imem;
}

COMPRESSION_METHOD* parse_4x4 (char** parameters)
{
  if (strcmp (parameters[0], "4x4") == 0) {
    _4x4_METHOD *p = new _4x4_METHOD;
    int error = 0;

    while (!error && *++parameters) {
      char *param = *parameters;
      if (!(isdigit ((unsigned char) param[0]) || isdigit ((unsigned char) param[1]))) {
        join_params (parameters, p->Method, sizeof (p->Method));
        COMPRESSION_METHOD *cmethod = ParseCompressionMethod (p->Method);
        error = (cmethod == NULL);
        delete cmethod;
        break;
      }

      if (strlen (param) == 1) {
      } else switch (*param) {
        case 'b': p->BlockSize         = parseMem (param+1, &error); continue;
        case 't': p->NumThreads        = (int) parseInt (param+1, &error); continue;
        case 'i': p->NumExtraBuffers   = (int) parseInt (param+1, &error); continue;
        case 'r': p->MinOrder0Percents = parseDouble (param+1, &error); continue;
      }

      int n = (int) parseInt (param, &error);
      if (!error) p->NumThreads = n;
      else        error = 0, p->BlockSize = parseMem (param, &error);
    }
    if (error) { delete p; return NULL; }
    return p;
  }
  return NULL;
}

static int _4x4_x = AddCompressionMethod (parse_4x4);
