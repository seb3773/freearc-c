#include "../Compression.h"

#ifdef __cplusplus

class _4x4_METHOD : public COMPRESSION_METHOD
{
public:
  char    Method[MAX_METHOD_STRLEN];
  MemSize BlockSize;
  int     NumThreads;
  int     NumExtraBuffers;
  double  MinOrder0Percents;

  _4x4_METHOD();
  int GetNumThreads()       {return NumThreads>0? NumThreads : GetCompressionThreads();}
  int GetNumExtraBuffers()  {return NumExtraBuffers>=0? NumExtraBuffers : 2;}

  virtual int doit (char *what, int param, void *data, CALLBACK_FUNC *callback);
  virtual int decompress (CALLBACK_FUNC *callback, void *auxdata);
#ifndef FREEARC_DECOMPRESS_ONLY
  virtual int compress   (CALLBACK_FUNC *callback, void *auxdata);

  virtual MemSize GetCompressionMem     (void);
  virtual MemSize GetDictionary         (void);
  virtual MemSize GetBlockSize          (void)         {return BlockSize;}
  virtual void    SetCompressionMem     (MemSize mem);
  virtual void    SetDecompressionMem   (MemSize mem);
  virtual void    SetDictionary         (MemSize dict);
  virtual void    SetBlockSize          (MemSize bs)   {if (bs>0) BlockSize=bs;}
  virtual void    ShowCompressionMethod (char *buf);
#endif
  virtual MemSize GetDecompressionMem   (void);
};

COMPRESSION_METHOD* parse_4x4 (char** parameters);

#endif
