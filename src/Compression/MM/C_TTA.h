// Wave sound comression algorithm
#include "../Compression.h"
#include "ttaenc.h"


#ifdef __cplusplus

// Ðåàëèçàöèÿ ñòàíäàðòíîãî èíòåðôåéñà ìåòîäîâ ñæàòèÿ COMPRESSION_METHOD
class TTA_METHOD : public COMPRESSION_METHOD
{
public:
  // Ïàðàìåòðû ýòîãî ìåòîäà ñæàòèÿ
  int level;        // Compression level (1..3, higher means tighter and slower compression)
  int skip_header;  // Skip WAV header detection
  int is_float;     // Floating-point data format
  int num_chan;     // Channels count
  int word_size;    // Size of each encoded value, in bits
  int offset;       // File offset where MM data start (header is copied intact)
  int raw_data ;    // Write raw predictor's output without using entropy encoder

  // Êîíñòðóêòîð, ïðèñâàèâàþùèé ïàðàìåòðàì ìåòîäà ñæàòèÿ çíà÷åíèÿ ïî óìîë÷àíèþ
  TTA_METHOD();
  // Óíèâåðñàëüíûé ìåòîä: âîçâðàùàåì ðàçëè÷íûå ïðîñòûå õàðàêòåðèñòèêè ìåòîäà ñæàòèÿ
  virtual int doit (char *what, int param, void *data, CALLBACK_FUNC *callback)
  {
      // Çàïðåòèòü ñîëèä-ñæàòèå, åñëè êîäèðóþòñÿ 2+ áàéòîâûå çíà÷åíèÿ èëè ïðîïóñêàþòñÿ áàéòû â íà÷àëå êàæäîãî ôàéëà
      if (strequ (what,"nosolid?"))   return word_size!=8 || offset!=0;
      else return COMPRESSION_METHOD::doit (what, param, data, callback);
  }

  // Ôóíêöèè ðàñïàêîâêè è óïàêîâêè
  virtual int decompress (CALLBACK_FUNC *callback, void *auxdata);
#ifndef FREEARC_DECOMPRESS_ONLY
  virtual int compress   (CALLBACK_FUNC *callback, void *auxdata);

  // Çàïèñàòü â buf[MAX_METHOD_STRLEN] ñòðîêó, îïèñûâàþùóþ ìåòîä ñæàòèÿ è åãî ïàðàìåòðû (ôóíêöèÿ, îáðàòíàÿ ê parse_TTA)
  virtual void ShowCompressionMethod (char *buf);

  // Ïîëó÷èòü/óñòàíîâèòü îáú¸ì ïàìÿòè, èñïîëüçóåìîé ïðè óïàêîâêå/ðàñïàêîâêå, ðàçìåð ñëîâàðÿ èëè ðàçìåð áëîêà
  virtual MemSize GetCompressionMem     (void)         {return 2*mb;}
  virtual MemSize GetDictionary         (void)         {return 0;}
  virtual MemSize GetBlockSize          (void)         {return 0;}
  virtual void    SetCompressionMem     (MemSize mem)  {}
  virtual void    SetDecompressionMem   (MemSize mem)  {}
  virtual void    SetDictionary         (MemSize dict) {}
  virtual void    SetBlockSize          (MemSize bs)   {}
#endif
  virtual MemSize GetDecompressionMem   (void)         {return 1*mb;}
};

// Ðàçáîðùèê ñòðîêè ìåòîäà ñæàòèÿ TTA
COMPRESSION_METHOD* parse_TTA (char** parameters);

#endif  // __cplusplus
