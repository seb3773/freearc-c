extern "C" {
#include "C_REP.h"
}


#define REP_LIBRARY
#include "rep.cpp"

/*-------------------------------------------------*/
/* ���������� ������ REP_METHOD                    */
/*-------------------------------------------------*/

// �����������, ������������� ���������� ������ ������ �������� �� ���������
REP_METHOD::REP_METHOD()
{
  BlockSize      = 64*mb;
  MinCompression = 100;
  ChunkSize      = 0;
  MinMatchLen    = 512;
  HashSizeLog    = 0;
  Barrier        = INT_MAX;
  SmallestLen    = 512;
  Amplifier      = 1;
}

// ������� ����������
int REP_METHOD::decompress (CALLBACK_FUNC *callback, void *auxdata)
{
  // Use faster function from DLL if possible
  static FARPROC f = LoadFromDLL ("rep_decompress");
  if (!f) f = (FARPROC) rep_decompress;

  return ((int (__cdecl *)(MemSize, int, int, int, int, int, int, int, CALLBACK_FUNC*, void*)) f)
                          (BlockSize, MinCompression, ChunkSize, MinMatchLen, Barrier, SmallestLen, HashSizeLog, Amplifier, callback, auxdata);
}

#ifndef FREEARC_DECOMPRESS_ONLY

// ������� ��������
int REP_METHOD::compress (CALLBACK_FUNC *callback, void *auxdata)
{
  // Use faster function from DLL if possible
  static FARPROC f = LoadFromDLL ("rep_compress");
  if (!f) f = (FARPROC) rep_compress;

  return ((int (__cdecl *)(MemSize, int, int, int, int, int, int, int, CALLBACK_FUNC*, void*)) f)
                          (BlockSize, MinCompression, ChunkSize, MinMatchLen, Barrier, SmallestLen, HashSizeLog, Amplifier, callback, auxdata);
}

// �������� � buf[MAX_METHOD_STRLEN] ������, ����������� ����� ������ � ��� ��������� (�������, �������� � parse_REP)
void REP_METHOD::ShowCompressionMethod (char *buf)
{
  REP_METHOD defaults; char BlockSizeStr[100], MinCompressionStr[100], BarrierTempStr[100], BarrierStr[100], SmallestLenStr[100], HashSizeLogStr[100], AmplifierStr[100], ChunkSizeStr[100], MinMatchLenStr[100];
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

// ���������, ������� ������ ��������� ��� �������� �������� �������
MemSize REP_METHOD::GetCompressionMem (void)
{
  // ����������� �� rep_compress
  int L;
  int HashSize = CalcHashSize (HashSizeLog, BlockSize, SmallestLen, MinMatchLen, ChunkSize, Amplifier, &L);
  return BlockSize + HashSize*sizeof(int);
}

// ���������, ������� ������ ��������� ��� �������� �������� �������
void REP_METHOD::SetCompressionMem (MemSize mem)
{
  if (mem>0)
  {
    // ����������� �� rep_compress
    int L;
    int HashSize = CalcHashSize (HashSizeLog, mem/5*4, SmallestLen, MinMatchLen, ChunkSize, Amplifier, &L);
    BlockSize = mem - HashSize*sizeof(int);
  }
}

#endif  // !defined (FREEARC_DECOMPRESS_ONLY)

// ������������ ������ ���� REP_METHOD � ��������� ����������� ��������
// ��� ���������� NULL, ���� ��� ������ ����� ������ ��� �������� ������ � ����������
COMPRESSION_METHOD* parse_REP (char** parameters)
{
  if (strcmp (parameters[0], "rep") == 0) {
    // ���� �������� ������ (������� ��������) - "rep", �� ������� ��������� ���������

    REP_METHOD *p = new REP_METHOD;
    int error = 0;  // ������� ����, ��� ��� ������� ���������� ��������� ������

    // �������� ��� ��������� ������ (��� ������ ������ ��� ������������� ������ ��� ������� ���������� ���������)
    while (*++parameters && !error)
    {
      char* param = *parameters;
      switch (*param) {                    // ���������, ���������� ��������
        case 'b':  p->BlockSize   = parseMem (param+1, &error); continue;
        case 'l':  p->MinMatchLen = parseInt (param+1, &error); continue;
        case 'c':  p->ChunkSize   = parseInt (param+1, &error); continue;
        case 'd':  p->Barrier     = parseMem (param+1, &error); continue;
        case 's':  p->SmallestLen = parseInt (param+1, &error); continue;
        case 'h':  p->HashSizeLog = parseInt (param+1, &error); continue;
        case 'a':  p->Amplifier   = parseInt (param+1, &error); continue;
      }
      // ���� �������� ������������� ������ ��������. �� ��������� ���������� ��� ��� "N%"
      if (last_char(param) == '%') {
        char str[100]; strcpy(str,param); last_char(str) = '\0';
        int n = parseInt (str, &error);
        if (!error) { p->MinCompression = n; continue; }
        error=0;
      }
      // ���� �� ��������, ���� � ��������� �� ������� ��� ��������
      // ���� ���� �������� ������� ��������� ��� ����� ����� (�.�. � ��� - ������ �����),
      // �� �������� ��� �������� ���� MinMatchLen, ����� ��������� ��������� ��� ��� BlockSize
      int n = parseInt (param, &error);
      if (!error) p->MinMatchLen = n;
      else        error=0, p->BlockSize = parseMem (param, &error);
    }
    if (error)  {delete p; return NULL;}  // ������ ��� �������� ���������� ������
    return p;
  } else
    return NULL;   // ��� �� ����� REP
}

static int REP_x = AddCompressionMethod (parse_REP);   // �������������� ������ ������ REP
