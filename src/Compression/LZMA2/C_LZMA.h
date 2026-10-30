#include "../Compression.h"

int lzma_compress   (int dictionarySize,
                     int hashSize,
                     int algorithm,
                     int numFastBytes,
                     int matchFinder,
                     int matchFinderCycles,
                     int posStateBits,
                     int litContextBits,
                     int litPosBits,
                     CALLBACK_FUNC *callback,
                     void *auxdata);

int lzma_decompress (int dictionarySize,
                     int hashSize,
                     int algorithm,
                     int numFastBytes,
                     int matchFinder,
                     int matchFinderCycles,
                     int posStateBits,
                     int litContextBits,
                     int litPosBits,
                     CALLBACK_FUNC *callback,
                     void *auxdata);


#ifdef __cplusplus

// ���������� ������������ ���������� ������� ������ COMPRESSION_METHOD
class LZMA_METHOD : public COMPRESSION_METHOD
{
public:
  // ��������� ����� ������ ������
  MemSize dictionarySize;
  MemSize hashSize;
  int     algorithm;
  int     numFastBytes;
  int     matchFinder;
  int     matchFinderCycles;
  int     posStateBits;
  int     litContextBits;
  int     litPosBits;

  // �����������, ������������� ���������� ������ ������ �������� �� ���������
  LZMA_METHOD();

  virtual int DeCompressMem (COMPRESSION direction, void *input, int inputSize, void *output, int *outputSize, CALLBACK_FUNC *callback=0, void *auxdata=0, void **CodecState=0);
  virtual int decompress (CALLBACK_FUNC *callback, void *auxdata);
#ifndef FREEARC_DECOMPRESS_ONLY
  virtual int compress (CALLBACK_FUNC *callback, void *auxdata);

  void CalcCompressionMemories (MemSize *pmem, MemSize *phashSize);

  // �������� � buf[MAX_METHOD_STRLEN] ������, ����������� ����� ������ � ��� ��������� (�������, �������� � parse_LZMA)
  virtual void ShowCompressionMethod (char *buf);

  // ��������/���������� ����� ������, ������������ ��� ��������/����������, ������ ������� ��� ������ �����
  virtual MemSize GetCompressionMem     (void);
  virtual MemSize GetDictionary         (void)         {return dictionarySize;}
  virtual MemSize GetBlockSize          (void)         {return 0;}
  virtual void    SetCompressionMem     (MemSize mem);
  virtual void    SetDecompressionMem   (MemSize mem);
  virtual void    SetDictionary         (MemSize dict);
  virtual void    SetBlockSize          (MemSize)      {}
#endif
  virtual MemSize GetDecompressionMem   (void);
};

// ��������� ������ ������ ������ LZMA
COMPRESSION_METHOD* parse_LZMA (char** parameters);

#endif  // __cplusplus
