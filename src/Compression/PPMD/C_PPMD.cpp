#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

extern "C" {
#include "C_PPMD.h"
}
#include "PPMdType.h"

/*-------------------------------------------------*/
/* Ðåàëèçàöèÿ ppmd_compress                        */
/*-------------------------------------------------*/
#ifndef FREEARC_DECOMPRESS_ONLY

namespace PPMD_compression {

#include "Model.cpp"

extern "C" {
int ppmd_compress (int order, MemSize mem, int MRMethod, CALLBACK_FUNC *callback, void *auxdata)
{
  if ( !StartSubAllocator(mem) ) {
    return FREEARC_ERRCODE_NOT_ENOUGH_MEMORY;
  }
  _PPMD_FILE* fpIn  = new _PPMD_FILE (callback, auxdata);
  _PPMD_FILE* fpOut = new _PPMD_FILE (callback, auxdata);
  EncodeFile (fpOut, fpIn, order, MR_METHOD(MRMethod));
  fpOut->flush();
  int ErrCode = FREEARC_OK;
  if (_PPMD_ERROR_CODE(fpIn) <0)  ErrCode = _PPMD_ERROR_CODE (fpIn);
  if (_PPMD_ERROR_CODE(fpOut)<0)  ErrCode = _PPMD_ERROR_CODE (fpOut);
  delete fpOut;
  delete fpIn;
  StopSubAllocator();
  return ErrCode;
}
} // extern "C"

void _STDCALL PrintInfo (_PPMD_FILE* DecodedFile, _PPMD_FILE* EncodedFile)
{
}

} // namespace PPMD_compression
#undef _PPMD_H_

#endif // FREEARC_DECOMPRESS_ONLY


/*-------------------------------------------------*/
/* Ðåàëèçàöèÿ ppmd_decompress                      */
/*-------------------------------------------------*/

namespace PPMD_decompression {

#include "Model.cpp"

extern "C" {
int ppmd_decompress (int order, MemSize mem, int MRMethod, CALLBACK_FUNC *callback, void *auxdata)
{
  if ( !StartSubAllocator(mem) ) {
    return FREEARC_ERRCODE_NOT_ENOUGH_MEMORY;
  }
  _PPMD_FILE* fpIn  = new _PPMD_FILE (callback, auxdata);
  _PPMD_FILE* fpOut = new _PPMD_FILE (callback, auxdata);
  DecodeFile (fpOut, fpIn, order, MR_METHOD(MRMethod));
  fpOut->flush();
  int ErrCode = FREEARC_OK;
  if (_PPMD_ERROR_CODE(fpIn) <0)  ErrCode = _PPMD_ERROR_CODE (fpIn);
  if (_PPMD_ERROR_CODE(fpOut)<0)  ErrCode = _PPMD_ERROR_CODE (fpOut);
  delete fpOut;
  delete fpIn;
  StopSubAllocator();
  return ErrCode;
}
} // extern "C"

void _STDCALL PrintInfo(_PPMD_FILE* DecodedFile,_PPMD_FILE* EncodedFile)
{
}

} // namespace PPMD_decompression


/*-------------------------------------------------*/
/* Ðåàëèçàöèÿ êëàññà PPMD_METHOD                  */
/*-------------------------------------------------*/

// Êîíñòðóêòîð, ïðèñâàèâàþùèé ïàðàìåòðàì ìåòîäà ñæàòèÿ çíà÷åíèÿ ïî óìîë÷àíèþ
PPMD_METHOD::PPMD_METHOD()
{
  order    = 10;
  mem      = 48*mb;
  MRMethod = 0;
}

// Ôóíêöèÿ ðàñïàêîâêè
int PPMD_METHOD::decompress (CALLBACK_FUNC *callback, void *auxdata)
{
  // Use faster function from DLL if possible
  static FARPROC f = LoadFromDLL ("ppmd_decompress");
  if (!f) f = (FARPROC) ppmd_decompress;

  return ((int (*)(int, MemSize, int, CALLBACK_FUNC*, void*)) f) (order, mem, MRMethod, callback, auxdata);
}

#ifndef FREEARC_DECOMPRESS_ONLY

// Ôóíêöèÿ óïàêîâêè
int PPMD_METHOD::compress (CALLBACK_FUNC *callback, void *auxdata)
{
  // Use faster function from DLL if possible
  static FARPROC f = LoadFromDLL ("ppmd_compress");
  if (!f) f = (FARPROC) ppmd_compress;

  return ((int (*)(int, MemSize, int, CALLBACK_FUNC*, void*)) f) (order, mem, MRMethod, callback, auxdata);
}

// Çàïèñàòü â buf[MAX_METHOD_STRLEN] ñòðîêó, îïèñûâàþùóþ ìåòîä ñæàòèÿ è åãî ïàðàìåòðû (ôóíêöèÿ, îáðàòíàÿ ê parse_PPMD)
void PPMD_METHOD::ShowCompressionMethod (char *buf)
{
  char MemStr[100];
  showMem (mem, MemStr);
  sprintf (buf, "ppmd:%d:%s%s", order, MemStr, MRMethod==2? ":r2": (MRMethod==1? ":r":""));
}

// Èçìåíèòü ïîòðåáíîñòü â ïàìÿòè, çàîäíî îòòþíèíãîâàâ order
void PPMD_METHOD::SetCompressionMem (MemSize _mem)
{
  if (_mem==0)  return;
  order  +=  int (log(double(_mem)/mem) / log(double(2)) * 4);
  mem = _mem;
}


#endif  // !defined (FREEARC_DECOMPRESS_ONLY)

// Êîíñòðóèðóåò îáúåêò òèïà PPMD_METHOD ñ çàäàííûìè ïàðàìåòðàìè óïàêîâêè
// èëè âîçâðàùàåò NULL, åñëè ýòî äðóãîé ìåòîä ñæàòèÿ èëè äîïóùåíà îøèáêà â ïàðàìåòðàõ
COMPRESSION_METHOD* parse_PPMD (char** parameters)
{
  if (strcmp (parameters[0], "ppmd") == 0) {
    // Åñëè íàçâàíèå ìåòîäà (íóëåâîé ïàðàìåòð) - "ppmd", òî ðàçáåð¸ì îñòàëüíûå ïàðàìåòðû

    PPMD_METHOD *p = new PPMD_METHOD;
    int error = 0;  // Ïðèçíàê òîãî, ÷òî ïðè ðàçáîðå ïàðàìåòðîâ ïðîèçîøëà îøèáêà

    // Ïåðåáåð¸ì âñå ïàðàìåòðû ìåòîäà (èëè âûéäåì ðàíüøå ïðè âîçíèêíîâåíèè îøèáêè ïðè ðàçáîðå î÷åðåäíîãî ïàðàìåòðà)
    while (*++parameters && !error)
    {
      char *param = *parameters;
      if (start_with (param, "mem")) {
        param+=2;  // Îáðàáîòàòü "mem..." êàê "m..."
      }
      if (strlen(param)==1) switch (*param) {    // Îäíîáóêâåííûå ïàðàìåòðû
        case 'r':  p->MRMethod = 1; continue;
      }
      else switch (*param) {                    // Ïàðàìåòðû, ñîäåðæàùèå çíà÷åíèÿ
        case 'm':  p->mem      = parseMem (param+1, &error); continue;
        case 'o':  p->order    = parseInt (param+1, &error); continue;
        case 'r':  p->MRMethod = parseInt (param+1, &error); continue;
      }
      // Ñþäà ìû ïîïàäàåì, åñëè â ïàðàìåòðå íå óêàçàíî åãî íàçâàíèå
      // Åñëè ýòîò ïàðàìåòð óäàñòñÿ ðàçîáðàòü êàê öåëîå ÷èñëî (ò.å. â í¸ì - òîëüêî öèôðû),
      // òî ïðèñâîèì åãî çíà÷åíèå ïîëþ order, èíà÷å ïîïðîáóåì ðàçîáðàòü åãî êàê mem
      int n = parseInt (param, &error);
      if (!error) p->order = n;
      else        error=0, p->mem = parseMem (param, &error);
    }
    if (error)  {delete p; return NULL;}  // Îøèáêà ïðè ïàðñèíãå ïàðàìåòðîâ ìåòîäà
    return p;
  } else
    return NULL;   // Ýòî íå ìåòîä ppmd
}

static int PPMD_x = AddCompressionMethod (parse_PPMD);   // Çàðåãèñòðèðóåì ïàðñåð ìåòîäà PPMD
