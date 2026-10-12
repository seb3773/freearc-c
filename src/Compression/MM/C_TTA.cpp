extern "C" {
#include "C_TTA.h"
#include "mmdet.h"
}

#define TTA_LIBRARY
#include "entropy.cpp"
#include "filters.cpp"
#include "tta.cpp"


/*-------------------------------------------------*/
/* Ðåàëèçàöèÿ êëàññà TTA_METHOD                    */
/*-------------------------------------------------*/

// Êîíñòðóêòîð, ïðèñâàèâàþùèé ïàðàìåòðàì ìåòîäà ñæàòèÿ çíà÷åíèÿ ïî óìîë÷àíèþ
TTA_METHOD::TTA_METHOD()
{
  level       = 3;
  skip_header = 0;
  is_float    = 0;
  num_chan    = 0;
  word_size   = 0;
  offset      = 0;
  raw_data    = 0;
}

// Ôóíêöèÿ ðàñïàêîâêè
int TTA_METHOD::decompress (CALLBACK_FUNC *callback, void *auxdata)
{
  // Use faster function from DLL if possible
  static FARPROC f = LoadFromDLL ("tta_decompress");
  if (!f) f = (FARPROC) tta_decompress;

  return ((int (__cdecl *)(CALLBACK_FUNC*, void*)) f) (callback, auxdata);
}

#ifndef FREEARC_DECOMPRESS_ONLY

// Ôóíêöèÿ óïàêîâêè
int TTA_METHOD::compress (CALLBACK_FUNC *callback, void *auxdata)
{
  // Use faster function from DLL if possible
  static FARPROC f = LoadFromDLL ("tta_compress");
  if (!f) f = (FARPROC) tta_compress;

  return ((int (__cdecl *)(int, int, int, int, int, int, int, CALLBACK_FUNC*, void*)) f)
                          (level, skip_header, is_float, num_chan, word_size, offset, raw_data, callback, auxdata);
}

// Çàïèñàòü â buf[MAX_METHOD_STRLEN] ñòðîêó, îïèñûâàþùóþ ìåòîä ñæàòèÿ è åãî ïàðàìåòðû (ôóíêöèÿ, îáðàòíàÿ ê parse_TTA)
void TTA_METHOD::ShowCompressionMethod (char *buf)
{
    TTA_METHOD defaults;  char eStr[100], cStr[100], rStr[100];
    if (num_chan || word_size) {
        sprintf (cStr, ":%d*%d%s", num_chan, word_size, is_float? "f":"");
        if (offset)  sprintf (str_end(cStr), ":o%d", offset);
    } else {
        sprintf (cStr, skip_header? ":s" : "");
    }
    sprintf (eStr, level      !=defaults.level?       ":m%d" : "", level);
    sprintf (rStr, raw_data   !=defaults.raw_data?    ":r%d" : "", raw_data);
    sprintf (buf, "tta%s%s%s", eStr, cStr, rStr);
}

#endif  // !defined (FREEARC_DECOMPRESS_ONLY)

// Êîíñòðóèðóåò îáúåêò òèïà TTA_METHOD ñ çàäàííûìè ïàðàìåòðàìè óïàêîâêè
// èëè âîçâðàùàåò NULL, åñëè ýòî äðóãîé ìåòîä ñæàòèÿ èëè äîïóùåíà îøèáêà â ïàðàìåòðàõ
COMPRESSION_METHOD* parse_TTA (char** parameters)
{
  if (strcmp (parameters[0], "tta") == 0) {
    // Åñëè íàçâàíèå ìåòîäà (íóëåâîé ïàðàìåòð) - "tta", òî ðàçáåð¸ì îñòàëüíûå ïàðàìåòðû

    TTA_METHOD *p = new TTA_METHOD;
    int error = 0;  // Ïðèçíàê òîãî, ÷òî ïðè ðàçáîðå ïàðàìåòðîâ ïðîèçîøëà îøèáêà

    // Ïåðåáåð¸ì âñå ïàðàìåòðû ìåòîäà (èëè âûéäåì ðàíüøå ïðè âîçíèêíîâåíèè îøèáêè ïðè ðàçáîðå î÷åðåäíîãî ïàðàìåòðà)
    while (*++parameters && !error)
    {
      char* param = *parameters;
      switch (*param) {                    // Ïàðàìåòðû, ñîäåðæàùèå çíà÷åíèÿ
        case 'm':  p->level       = parseInt (param+1, &error); continue;
        case 's':  p->skip_header = 1;                          continue;
        case 'f':  p->is_float    = 1;                          continue;
        case 'c':  p->num_chan    = parseInt (param+1, &error); continue;
        case 'w':  p->word_size   = parseInt (param+1, &error); continue;
        case 'o':  p->offset      = parseInt (param+1, &error); continue;
        case 'r':  p->raw_data    = parseInt (param+1, &error); continue;
      }
      // Ñþäà ìû ïîïàäàåì, åñëè â ïàðàìåòðå íå óêàçàíî åãî íàçâàíèå
      // Åñëè ýòîò ïàðàìåòð óäàñòñÿ ðàçîáðàòü êàê c*w,
      // òî èñïîëüçóåì ýòè çíà÷åíèÿ äëÿ ïîëåé num_chan è word_size.
      // Äîïîëíèòåëüíûé ñèìâîë 'f' îçíà÷àåò, ÷òî ýòî äàííûå â FP-ôîðìàòå
      int a, b;  char s[MAX_METHOD_STRLEN];
      if (sscanf (param, "%d*%d%s", &a, &b, s)==3  &&  strequ(s,"f"))
          p->is_float = 1, p->num_chan=a, p->word_size=b;
      else if (sscanf (param, "%d*%d", &a, &b)==2)
          p->is_float = 0, p->num_chan=a, p->word_size=b;
      else error=1;
    }
    if (error)  {delete p; return NULL;}  // Îøèáêà ïðè ïàðñèíãå ïàðàìåòðîâ ìåòîäà
    return p;
  } else
    return NULL;   // Ýòî íå ìåòîä TTA
}

static int TTA_x = AddCompressionMethod (parse_TTA);   // Çàðåãèñòðèðóåì ïàðñåð ìåòîäà TTA
