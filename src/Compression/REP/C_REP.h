#include "../Compression.h"

int rep_compress   (MemSize BlockSize, int MinCompression, int ChunkSize, int MinMatchLen, int Barrier, int SmallestLen, int HashSizeLog, int Amplifier, CALLBACK_FUNC *callback, void *auxdata);
int rep_decompress (MemSize BlockSize, int MinCompression, int ChunkSize, int MinMatchLen, int Barrier, int SmallestLen, int HashSizeLog, int Amplifier, CALLBACK_FUNC *callback, void *auxdata);


#ifdef __cplusplus

// ���������� ������������ ���������� ������� ������ COMPRESSION_METHOD
class REP_METHOD : public COMPRESSION_METHOD
{
public:
  // ��������� ����� ������ ������
  MemSize BlockSize;        // ������ ������. ���������� ������ ������ � �������� ���� ���������. ������ ������ - BlockSize+BlockSize/4
  int     MinCompression;   // ����������� ������� ������. ���� �������� ������ ������, �� ������ ��� ����� �������� ������������ (��������) ������
  int     ChunkSize;        // ������ ������, �� ������� ��������� � ���
  int     MinMatchLen;      // ����������� ����� ������, ��� ������� ��� ����� ���������� ������� �� ���������� ���������
  int     Barrier;          // �������, ����� ������� ����������� ������������ ���������� �������� ������� (��������� lzma/ppmd �� ����� ��������� ��)
  int     SmallestLen;      // ���� ������� ������
  int     HashSizeLog;      // �������� ������� ���� (� 4-�������� ������). ������� �������� ����������� ������, �� ��������� ���. ��� ������� �������� ����������� ������ ����������� �������������
  int     Amplifier;        // ����������� "��������" ������

  // �����������, ������������� ���������� ������ ������ �������� �� ���������
  REP_METHOD();

  // ������� ���������� � ��������
  virtual int decompress (CALLBACK_FUNC *callback, void *auxdata);
#ifndef FREEARC_DECOMPRESS_ONLY
  virtual int compress   (CALLBACK_FUNC *callback, void *auxdata);

  // �������� � buf[MAX_METHOD_STRLEN] ������, ����������� ����� ������ � ��� ��������� (�������, �������� � parse_REP)
  virtual void ShowCompressionMethod (char *buf);

  // ��������/���������� ����� ������, ������������ ��� ��������/����������, ������ ������� ��� ������ �����
  virtual MemSize GetCompressionMem     (void);
  virtual MemSize GetDictionary         (void)         {return BlockSize;}
  virtual MemSize GetBlockSize          (void)         {return 0;}
  virtual void    SetCompressionMem     (MemSize mem);
  virtual void    SetDecompressionMem   (MemSize mem)  {if (mem>0)   BlockSize = mem;}
  virtual void    SetDictionary         (MemSize dict) {if (dict>0)  BlockSize = dict;}
  virtual void    SetBlockSize          (MemSize bs)   {}
#endif
  virtual MemSize GetDecompressionMem   (void)         {return BlockSize;}
};

// ��������� ������ ������ ������ REP
COMPRESSION_METHOD* parse_REP (char** parameters);

#endif  // __cplusplus
