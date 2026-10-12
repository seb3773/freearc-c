extern "C" {
#include "C_MM.h"
}
#define MM_LIBRARY
#include "mm.cpp"


/*-------------------------------------------------*/
/* Ðåàëèçàöèÿ êëàññà MM_METHOD                     */
/*-------------------------------------------------*/

// Êîíñòðóêòîð, ïðèñâàèâàþùèé ïàðàìåòðàì ìåòîäà ñæàòèÿ çíà÷åíèÿ ïî óìîë÷àíèþ
MM_METHOD::MM_METHOD()
{
    mode        = 9;
    skip_header = 0;
    is_float    = 0;
    num_chan    = 0;
    word_size   = 0;
    offset      = 0;
    reorder     = 0;
}

// Ôóíêöèÿ ðàñïàêîâêè
int MM_METHOD::decompress (CALLBACK_FUNC *callback, void *auxdata)
{
    return mm_decompress (callback, auxdata);
}

#ifndef FREEARC_DECOMPRESS_ONLY

// Ôóíêöèÿ óïàêîâêè
int MM_METHOD::compress (CALLBACK_FUNC *callback, void *auxdata)
{
    return mm_compress (mode, skip_header, is_float, num_chan, word_size, offset, reorder, callback, auxdata);
}

// Çàïèñàòü â buf[MAX_METHOD_STRLEN] ñòðîêó, îïèñûâàþùóþ ìåòîä ñæàòèÿ è åãî ïàðàìåòðû (ôóíêöèÿ, îáðàòíàÿ ê parse_MM)
void MM_METHOD::ShowCompressionMethod (char *buf)
{
    MM_METHOD defaults;
    char dStr[100], cStr[100], rStr[100];
    if (num_chan || word_size) {
        sprintf (cStr, ":%d*%d%s", num_chan, word_size, is_float? "f":"");
        if (offset)  sprintf (str_end(cStr), ":o%d", offset);
    } else {
        sprintf (cStr, skip_header? ":s" : "");
    }
    sprintf (rStr, reorder? ":r%d" : "", reorder);
    sprintf (dStr, mode!=defaults.mode? ":d%d" : "", mode);
    sprintf (buf, "mm%s%s%s", dStr, cStr, rStr);
}

#endif  // !defined (FREEARC_DECOMPRESS_ONLY)

// Êîíñòðóèðóåò îáúåêò òèïà MM_METHOD ñ çàäàííûìè ïàðàìåòðàìè óïàêîâêè
// èëè âîçâðàùàåò NULL, åñëè ýòî äðóãîé ìåòîä ñæàòèÿ èëè äîïóùåíà îøèáêà â ïàðàìåòðàõ
COMPRESSION_METHOD* parse_MM (char** parameters)
{
  if (strcmp (parameters[0], "mm") == 0) {
    // Åñëè íàçâàíèå ìåòîäà (íóëåâîé ïàðàìåòð) - "mm", òî ðàçáåð¸ì îñòàëüíûå ïàðàìåòðû

    MM_METHOD *p = new MM_METHOD;
    int error = 0;  // Ïðèçíàê òîãî, ÷òî ïðè ðàçáîðå ïàðàìåòðîâ ïðîèçîøëà îøèáêà

    // Ïåðåáåð¸ì âñå ïàðàìåòðû ìåòîäà (èëè âûéäåì ðàíüøå ïðè âîçíèêíîâåíèè îøèáêè ïðè ðàçáîðå î÷åðåäíîãî ïàðàìåòðà)
    while (*++parameters && !error)
    {
      char* param = *parameters;
      switch (*param) {                    // Ïàðàìåòðû, ñîäåðæàùèå çíà÷åíèÿ
        case 's':  p->skip_header = 1;                          continue;
        case 'f':  p->is_float    = 1;                          continue;
        case 'd':  p->mode        = parseInt (param+1, &error); continue;
        case 'c':  p->num_chan    = parseInt (param+1, &error); continue;
        case 'w':  p->word_size   = parseInt (param+1, &error); continue;
        case 'o':  p->offset      = parseInt (param+1, &error); continue;
        case 'r':  p->reorder     = parseInt (param+1, &error); continue;
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
    return NULL;   // Ýòî íå ìåòîä MM
}

static int MM_x = AddCompressionMethod (parse_MM);   // Çàðåãèñòðèðóåì ïàðñåð ìåòîäà MM

