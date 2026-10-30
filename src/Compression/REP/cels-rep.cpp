#include "../CELS.h"

namespace CELS
{

#define REP_LIBRARY
#include "rep.cpp"

// ���������� ������������ ���������� ������� ������ COMPRESSION_METHOD
struct REP_METHOD : COMPRESSION_METHOD
{
  // ��������� ����� ������ ������
  MemSize BlockSize;        // ������ ������. ���������� ������ ������ � �������� ���� ���������. ������ ������ - BlockSize+BlockSize/4
  int     MinCompression;   // ����������� ������� ������. ���� �������� ������ ������, �� ������ ��� ����� �������� ������������ (��������) ������
  int     ChunkSize;        // ������ ������, �� ������� ��������� � ���
  int     MinMatchLen;      // ����������� ����� ������, ��� ������� ��� ����� ���������� ������� �� ���������� ���������
  int     Barrier;          // �������, ����� ������� ����������� ������������ ���������� �������� ������� (��������� lzma/ppmd �� ����� ��������� ��)
  int     SmallestLen;      // ���� ������� ������
  int     HashSizeLog;      // �������� ������� ���� (� 4-�������� ������). ������� �������� ����������� ������, �� ��������� ���. ��� ������� �������� ����������� ������ ����������� �������������
  int     Amplifier;        // ����������� "��������" ������

  // �����������, ������������� ���������� ������ �������� �� ���������
  REP_METHOD (TABI_ELEMENT* params) : COMPRESSION_METHOD(params)
  {
    BlockSize      = 64*mb;
    MinCompression = 100;
    ChunkSize      = 0;
    MinMatchLen    = 512;
    Barrier        = INT_MAX;
    SmallestLen    = 512;
    HashSizeLog    = 0;
    Amplifier      = 1;
  }

  // ������� ����������
  virtual int decompress (CALLBACK_FUNC *callback, void *auxdata)
  {
    return rep_decompress (BlockSize, MinCompression, ChunkSize, MinMatchLen, Barrier, SmallestLen, HashSizeLog, Amplifier, callback, auxdata);
  }

#ifndef FREEARC_DECOMPRESS_ONLY
  // ������� ��������
  virtual int compress (CALLBACK_FUNC *callback, void *auxdata)
  {
    return rep_compress (BlockSize, MinCompression, ChunkSize, MinMatchLen, Barrier, SmallestLen, HashSizeLog, Amplifier, callback, auxdata);
  }

  // ��������� ������ � ����������� ������
  virtual void parse_method()
  {
    // ��������� ������ ������ ������ � ������ ����� `parameters`, �������� ��� �������� � ���������
    char* method = p._str("method");
    char* params [MAX_PARAMETERS], **parameters = params;
    char  local_method [MAX_METHOD_STRLEN];
    strncopy (local_method, method, sizeof (local_method));
    split (local_method, COMPRESSION_METHOD_PARAMETERS_DELIMITER, parameters, MAX_PARAMETERS);
    if (!strequ (parameters[0], "rep"))  throw "REP_METHOD:parse_method";

    // ���� �������� ������ (������� ��������) - "rep", �� ������� ��������� ���������
    int error = 0;  // ������� ����, ��� ��� ������� ���������� ��������� ������

    // �������� ��� ��������� ������ (��� ������ ������ ��� ������������� ������ ��� ������� ���������� ���������)
    while (*++parameters && !error)
    {
      char* param = *parameters;
      switch (*param) {                    // ���������, ���������� ��������
        case 'b':  BlockSize   = parseMem (param+1, &error); continue;
        case 'l':  MinMatchLen = parseInt (param+1, &error); continue;
        case 'c':  ChunkSize   = parseInt (param+1, &error); continue;
        case 'd':  Barrier     = parseMem (param+1, &error); continue;
        case 's':  SmallestLen = parseInt (param+1, &error); continue;
        case 'h':  HashSizeLog = parseInt (param+1, &error); continue;
        case 'a':  Amplifier   = parseInt (param+1, &error); continue;
      }
      // ���� �������� ������������� ������ ��������. �� ��������� ���������� ��� ��� "N%"
      if (last_char(param) == '%') {
        char str[100]; strcpy(str,param); last_char(str) = '\0';
        int n = parseInt (str, &error);
        if (!error) { MinCompression = n; continue; }
        error=0;
      }
      // ���� �� ��������, ���� � ��������� �� ������� ��� ��������
      // ���� ���� �������� ������� ��������� ��� ����� ����� (�.�. � ��� - ������ �����),
      // �� �������� ��� �������� ���� MinMatchLen, ����� ��������� ��������� ��� ��� BlockSize
      int n = parseInt (param, &error);
      if (!error) MinMatchLen = n;
      else        error=0, BlockSize = parseMem (param, &error);
    }
    if (error)  throw "rep:parse_method";  // ������ ��� �������� ���������� ������
  }

  // �������� � buf[MAX_METHOD_STRLEN] ������, ����������� ����� ������ � ��� ��������� (�������, �������� � parse_method)
  virtual void ShowCompressionMethod (char *buf)
  {
    REP_METHOD defaults(NULL); char BlockSizeStr[100], MinCompressionStr[100], BarrierTempStr[100], BarrierStr[100], SmallestLenStr[100], HashSizeLogStr[100], AmplifierStr[100], ChunkSizeStr[100], MinMatchLenStr[100];
    showMem (BlockSize, BlockSizeStr);
    showMem (Barrier,   BarrierTempStr);
    sprintf (MinCompressionStr, MinCompression!=defaults.MinCompression? ":%d%%" : "", MinCompression);
    sprintf (MinMatchLenStr, MinMatchLen!=defaults.MinMatchLen? ":%d"  : "", MinMatchLen);
    sprintf (ChunkSizeStr,   ChunkSize  !=defaults.ChunkSize  ? ":c%d" : "", ChunkSize);
    sprintf (BarrierStr,     Barrier    !=defaults.Barrier    ? ":d%s" : "", BarrierTempStr);
    sprintf (SmallestLenStr, SmallestLen!=defaults.SmallestLen? ":s%d" : "", SmallestLen);
    sprintf (AmplifierStr,   Amplifier  !=defaults.Amplifier  ? ":a%d" : "", Amplifier);
    sprintf (HashSizeLogStr, HashSizeLog!=defaults.HashSizeLog? ":h%d" : "", HashSizeLog);
    sprintf (buf, "rep:%s%s%s%s%s%s%s%s", BlockSizeStr, MinCompressionStr, MinMatchLenStr, ChunkSizeStr, BarrierStr, SmallestLenStr, HashSizeLogStr, AmplifierStr);
  }

  // ��������� ����� ������ �� ������������� ��������� ������ ������
  virtual void SetCompressionMem (MemSize mem)
  {
    // ����������� �� rep_compress
    int L;
    int HashSize = CalcHashSize (HashSizeLog, mem/5*4, SmallestLen, MinMatchLen, ChunkSize, Amplifier, &L);
    BlockSize = mem - HashSize*sizeof(int);
  }

  // ���������, ������� ������ ��������� ��� �������� �������� �������
  virtual MemSize GetCompressionMem()
  {
    // ����������� �� rep_compress
    int L;
    int HashSize = CalcHashSize (HashSizeLog, BlockSize, SmallestLen, MinMatchLen, ChunkSize, Amplifier, &L);
    return BlockSize + HashSize*sizeof(int);
  }

  // ��������/���������� ����� ������, ������������ ��� ��������/����������, ������ ������� ��� ������ �����
  virtual void    SetDictionary       (MemSize dict) {BlockSize = dict;}
  virtual MemSize GetDictionary       (void)         {return BlockSize;}
  virtual void    SetDecompressionMem (MemSize mem)  {BlockSize = mem;}
#endif
  virtual MemSize GetDecompressionMem (void)         {return BlockSize;}
};

// Function that represents REP compression method
int rep_server (TABI_ELEMENT* params)
{
  return REP_METHOD(params).server();
}

// Register REP method in CELS
int rep_register = CELS_Register(rep_server);

}
