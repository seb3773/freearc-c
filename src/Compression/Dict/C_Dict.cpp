extern "C" {
#include "C_Dict.h"
}
#if defined(__linux__)
#include <sys/mman.h>
#endif

/* In/Out of dict_decompress, kept on the worker thread instead of mmap/munmap
   of a 64 MB pair on every block. Freed when the thread exits. */
static void dict_hint_hugepage(void *p, size_t n)
{
#if defined(__linux__) && defined(MADV_HUGEPAGE)
    if (p && n >= (size_t)(2u << 20))
        (void)madvise(p, n, MADV_HUGEPAGE);
#else
    (void)p; (void)n;
#endif
}
struct DictTlsBuf {
    BYTE *p;
    size_t cap;
    DictTlsBuf() : p(0), cap(0) {}
    ~DictTlsBuf() { BigFree(p); p = NULL; cap = 0; }
    BYTE *acquire(size_t need) {
        if (need == 0) return NULL;
        if (p && cap >= need) return p;
        BYTE *np = (BYTE *)BigAlloc(need);
        if (!np) return NULL;
        BigFree(p);
        p = np;
        cap = need;
        dict_hint_hugepage(p, cap);
        return p;
    }
};
struct DictTls {
    DictTlsBuf in, out;
};
static thread_local DictTls dict_tls;

#define DICT_LIBRARY
#include "dict.cpp"

#ifndef FREEARC_DECOMPRESS_ONLY
int dict_compress (MemSize BlockSize, int MinCompression, int MinWeakChars, int MinLargeCnt, int MinMediumCnt, int MinSmallCnt, int MinRatio, CALLBACK_FUNC *callback, void *auxdata)
{
    BYTE* In = NULL;  // ��������� �� ������� ������
    BYTE* Out= NULL;  // ��������� �� �������� ������
    int x;            // ��� ������������ ������
    while ( (x = callback ("read", (In = (BYTE*) BigAlloc(BlockSize)), BlockSize, auxdata)) > 0 )
    {
        unsigned InSize, OutSize;     // ���������� ���� �� ������� � �������� ������, ��������������
        InSize=x;                     // impossible: In = (BYTE*) realloc(In,InSize=x);
        x = DictEncode(In,InSize,&Out,&OutSize,MinWeakChars,MinLargeCnt,MinMediumCnt,MinSmallCnt,MinRatio);
        if (x  ||  MinCompression>0 && OutSize>=(double(InSize)*MinCompression)/100) {
            // ��������� ������ [���������� ������] �� �������, ������� ������ ��� �������� ������
            int WrSize=-InSize;
            BigFreeAndNil(Out);
            // �������� �������� ���� � �����, ���� ��� ������ ��������� ������/������ ������ �� �����
            checked_write (&WrSize, sizeof(WrSize));
            checked_write (In, InSize);
            BigFreeAndNil(In);
        } else {
            // ������ ������� ���������, ����� ���������� ������� ����� ������ ��� ���������� ��
            // (����� ���������� ������ ������ ��� ���������� ��������� � ������� ���������� ������)
            BigFreeAndNil(In);
            // �������� ������ ���� � �����, ���� ��� ������ ��������� ������/������ ������ �� �����
            checked_write (&OutSize, sizeof(OutSize));
            checked_write (Out, OutSize);
            BigFreeAndNil(Out);
        }
    }
finished:
    BigFreeAndNil(In); BigFreeAndNil(Out); return x;  // 0, ���� �� � �������, � ��� ������ �����
}
#endif  // !defined (FREEARC_DECOMPRESS_ONLY)


int dict_decompress (MemSize BlockSize, int MinCompression, int MinWeakChars, int MinLargeCnt, int MinMediumCnt, int MinSmallCnt, int MinRatio, CALLBACK_FUNC *callback, void *auxdata)
{
  int x;
  for(;;) {
    int InSize; unsigned OutSize;
    checked_eof_read (&InSize, sizeof(InSize));
    if (InSize<0) {
        BYTE *In = dict_tls.in.acquire((size_t)(-InSize));
        if ((-InSize) && !In) { x = FREEARC_ERRCODE_NOT_ENOUGH_MEMORY; goto finished; }
        checked_read  (In, -InSize);
        checked_write (In, -InSize);
    } else {
        BYTE *In  = dict_tls.in.acquire((size_t)InSize);
        BYTE *Out = dict_tls.out.acquire((size_t)BlockSize);
        if ((InSize && !In) || (BlockSize && !Out)) {
            x = FREEARC_ERRCODE_NOT_ENOUGH_MEMORY;
            goto finished;
        }
        checked_read  (In, InSize);
        x = DictDecode (In, InSize, Out, &OutSize);
        if (x) break;
        checked_write (Out, OutSize);
    }
  }
finished:
  return x<=0? x : FREEARC_ERRCODE_GENERAL;
}


/*-------------------------------------------------*/
/* ���������� ������ DICT_METHOD                    */
/*-------------------------------------------------*/

// �����������, ������������� ���������� ������ ������ �������� �� ���������
DICT_METHOD::DICT_METHOD()
{
  BlockSize      = 64*mb;
  MinCompression = 100;
  MinWeakChars   = 20;
  MinLargeCnt    = 2048;
  MinMediumCnt   = 100;
  MinSmallCnt    = 50;
  MinRatio       = 4;
}

// ������� ����������
int DICT_METHOD::decompress (CALLBACK_FUNC *callback, void *auxdata)
{
  // Use faster function from DLL if possible
  static FARPROC f = LoadFromDLL ("dict_decompress");
  if (!f) f = (FARPROC) dict_decompress;

  return ((int (*)(MemSize, int, int, int, int, int, int, CALLBACK_FUNC*, void*)) f)
                  (BlockSize, MinCompression, MinWeakChars, MinLargeCnt, MinMediumCnt, MinSmallCnt, MinRatio, callback, auxdata);
}

#ifndef FREEARC_DECOMPRESS_ONLY

// ������� ��������
int DICT_METHOD::compress (CALLBACK_FUNC *callback, void *auxdata)
{
  // Use faster function from DLL if possible
  static FARPROC f = LoadFromDLL ("dict_compress");
  if (!f) f = (FARPROC) dict_compress;

  return ((int (*)(MemSize, int, int, int, int, int, int, CALLBACK_FUNC*, void*)) f)
                  (BlockSize, MinCompression, MinWeakChars, MinLargeCnt, MinMediumCnt, MinSmallCnt, MinRatio, callback, auxdata);
}

// �������� � buf[MAX_METHOD_STRLEN] ������, ����������� ����� ������ � ��� ��������� (�������, �������� � parse_DICT)
void DICT_METHOD::ShowCompressionMethod (char *buf)
{
    DICT_METHOD defaults; char BlockSizeStr[100], MinCompressionStr[100], MinWeakCharsStr[100];
    char MinLargeCntStr[100], MinMediumCntStr[100], MinSmallCntStr[100], MinRatioStr[100];
    showMem (BlockSize, BlockSizeStr);
    sprintf (MinCompressionStr, MinCompression!=defaults.MinCompression? ":%d%%" : "", MinCompression);
    sprintf (MinWeakCharsStr,   MinWeakChars  !=defaults.MinWeakChars  ? ":c%d"  : "", MinWeakChars);
    sprintf (MinLargeCntStr,    MinLargeCnt   !=defaults.MinLargeCnt   ? ":l%d"  : "", MinLargeCnt );
    sprintf (MinMediumCntStr,   MinMediumCnt  !=defaults.MinMediumCnt  ? ":m%d"  : "", MinMediumCnt);
    sprintf (MinSmallCntStr,    MinSmallCnt   !=defaults.MinSmallCnt   ? ":s%d"  : "", MinSmallCnt );
    sprintf (MinRatioStr,       MinRatio      !=defaults.MinRatio      ? ":r%d"  : "", MinRatio    );
    sprintf (buf, "dict:%s%s%s%s%s%s%s", BlockSizeStr, MinCompressionStr, MinWeakCharsStr,
                                         MinLargeCntStr, MinMediumCntStr, MinSmallCntStr, MinRatioStr);
}

#endif  // !defined (FREEARC_DECOMPRESS_ONLY)

// ������������ ������ ���� DICT_METHOD � ��������� ����������� ��������
// ��� ���������� NULL, ���� ��� ������ ����� ������ ��� �������� ������ � ����������
COMPRESSION_METHOD* parse_DICT (char** parameters)
{
  if (strcmp (parameters[0], "dict") == 0) {
    // ���� �������� ������ (������� ��������) - "dict", �� ������� ��������� ���������

    DICT_METHOD *p = new DICT_METHOD;
    int error = 0;  // ������� ����, ��� ��� ������� ���������� ��������� ������

    // �������� ��� ��������� ������ (��� ������ ������ ��� ������������� ������ ��� ������� ���������� ���������)
    while (*++parameters && !error)
    {
      char* param = *parameters;
      if (strlen(param)==1) switch (*param) {    // ������������� ���������
        case 'p':  p->MinLargeCnt=8192; p->MinMediumCnt=400; p->MinSmallCnt=100; p->MinRatio=4; continue;
        case 'f':  p->MinLargeCnt=2048; p->MinMediumCnt=100; p->MinSmallCnt= 50; p->MinRatio=0; continue;
      }
      else switch (*param) {                    // ���������, ���������� ��������
        case 'b':  p->BlockSize    = parseMem (param+1, &error); continue;
        case 'c':  p->MinWeakChars = parseInt (param+1, &error); continue;
        case 'l':  p->MinLargeCnt  = parseInt (param+1, &error); continue;
        case 'm':  p->MinMediumCnt = parseInt (param+1, &error); continue;
        case 's':  p->MinSmallCnt  = parseInt (param+1, &error); continue;
        case 'r':  p->MinRatio     = parseInt (param+1, &error); continue;
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
      if (!error) p->MinWeakChars = n;
      else        error=0, p->BlockSize = parseMem (param, &error);
    }
    if (error)  {delete p; return NULL;}  // ������ ��� �������� ���������� ������
    return p;
  } else
    return NULL;   // ��� �� ����� DICT
}

static int DICT_x = AddCompressionMethod (parse_DICT);   // �������������� ������ ������ DICT
