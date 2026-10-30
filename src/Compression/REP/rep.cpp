/*
    REP is an LZ77-family algorithm, i.e. it founds matches and outputs them as
    (len,offset) pairs. It is oriented toward very fast compression and small
    memory overhead (1/4 of buffer size), but limited to rather large values of
    mimimal match length (say, 32), and don't search for optimum match. It's
    intended to preprocess data before using full-fledged compressors. and in
    this area it beats RZIP and, to some degree, LZP preprocessors. Small
    memory overhead means that RZIP/LZP/REP are capable to find matches at very
    long distances and this algorithm does it much better than RZIP and LZP.
    The algorithm implemented in functions REPEncode() and REPDecode().

    Main differences comparing to RZIP:
    1) Sliding window which slides at 1/16 of buffer size each time
    2) Almost ideal hash function (see update_hash)
    3) Direct hashing without hash chains which 1.5x cuts memory requirements
    4) Tags are not saved in hashtable, which again halves memory requirements.
         Instead, a few lower bits of hash table entry are used to save a few
         bits of tag (see chksum)
    5) Hash size is proportional to buffer size (which is equal to the maximum
         search distance) and by default limited to 1/4 of buffer size
    6) In order to find strings of length >=MinLen, blocks of length L=MinLen/2
         are indexed via hash. Of all those possible blocks, only 1/sqrt(L) are
         indexed and only 1/sqrt(L) are searched. It is alternative to solution
         described in RZIP paper where 1/L of blocks are indexed and each block
         searched. This means that logb(sqrt(L)) lower bits of hash entry are
         zeroes which allows to use trick 4.


References for RZIP algorithm explanation and implementations:
http://samba.org/~tridge/phd_thesis.pdf
http://rzip.samba.org/ftp/rzip/rzip-2.1.tar.gz
http://ck.kolivas.org/apps/lrzip/lrzip-0.18.tar.bz2
http://www.edcassa-ict.nl/lrzip.zip
http://www.edcassa-ict.nl/rzip21.zip

TAYLOR, R., JANA, R., AND GRIGG, M. 1997. Checksum testing of remote
synchronisation tool. Technical Report 0627 (November), Defence Science and
Technology Organisation, Canberra, Australia. (p.72)


References for LZP algorithm implementations:
http://magicssoft.ru/content/download/GRZipII/GRZipIISRC.zip
http://www.compression.ru/ds/lzp.rar


** Detailed algorithm description in Russian **********************************************

    ���� �������� �������� �������������� LZ77, �.�. �� ������� �������������
    ������ �� ������� ������, � �������� �� ��� (len,offset). ��� ������������
    �������� ���������� �� ����� ���������� ���������� ������� ����� �� �������
    ����������. ������� �� ������ ���������� ���������� ������ - ��� �������,
    ��� �������� ������ ��������� �� ����� 25% �� ������� ���� ������. ��� ����
    �� ������� ����������� ��� ���������� ���� ����������� ����� (MinLen)
    ������� ����� - 512 ����, � ������� 98% - � ����� ��� ������������ ��
    ����� ���������� � ������ �� 32 ����. �� �������� ���� �������� ������� ��
    ������������� � �������� �������������, ������������ ������������ �����
    �/��� ���������� ��������� �� ����� ����������, ������� ����������
    ��������� ��������� ��������, � � ���� �������� �� ����������� � ������
    �����������, ��� LZP by Ilya Grebnev � RZIP. ��� ����, ��� ����������
    ������������, ��� ������������� ����������� �������� ����������� �������
    ������ ��������� ������ � ���� �������� - 32-512 ����. ���� ��������
    ������� ���� ������ ����������, ��� LZP/RZIP, � ����� ����, ���
    �������� ������ ������������� ��� ���������� MinLen.

    �������� ����������� ��������� REPEncode() � REPDecode(), � ����������
    ��������� ���� �� LZP, RZIP � ���� �����������. ����� ���������� ������ �
    ���������� ���� - ������� ������ ����������� ������� �� 1/16 �� �������
    ������, � ��� �������� ��� � ����� ������ ������� ��� ������� 15/16 ������
    �������� ���������� ������, ������� ����������� � ������� ����������. ���
    ��������� ��������� �� ������� �����, �� ���������� �� ����� ����������
    ������� ������.

    ��� ������, ��� ������ ����� � ������ �� MinLen � ������� ����� �����
    ������ MinLen ����������� ����� ����������� ����� (��), ������� ��������� �
    ���-�������. ��������� �������� ������������ �� ������� �������� MinLen,
    ������� ���������� �� �� ������ ����� ����� �������� ���������. ���
    �������� �������� �������������� "���������� ��", �� ���� �����, �������
    ����� ������ ����������� ��� ���������� ������ ����� � ����� ����� �
    �������� ������ ����� � ������ (��. update_hash).

    ������ ��������� ������� ��� ����������� ����������� ��� ���������
    ������������. � ����� ������ ������� ������� hash = p[-1] + PRIME*p[-2] +
    PRIME*PRIME*p[-3] + ..., ��� PRIME - ������� �����, ��������� ����� �������
    � ������ ������ ����������� �������������. ����������, ��� ���������� ����
    �� ������ 1<<32, ������� ���������������� ��� ����������� :)

    �����, ���� ������������ �������������� ���� ��� ���������� ���������� �
    ������ � ���������� ��������. ���������� � ������� ������ ��������� ���
    MinLen=512. ��������� ����� 512-������� ���� �������� � ���� 256-�������
    ����, ������������ � �������, ������� 256, �� ��� ���������� ��������� �
    ���-������� ������ ������ �� ��� ����� � ������ ���������� ������ � ����.
    ����������, ��� �������� ���������� �� �� �������������� � �������� 256
    �������, � �������� ���������� ��� ��� ����� ������ � ��� �������. ������
    ��� � ��������� ����������� ��������� ������ ������ ��� ���������������
    ���������� ����� ���� ���������� - �� ������ ������, ����� MinLen
    ���������� ������.

    ������ ����� ����� ��� ������ - ������ ����, ����� ��������� � ���-�������
    ������ 256-� ����, �� ������ ������-������, �� ����� �������� ���������
    ������ 32-�, � ������ ������ 8-�, ��� ��������� ������ 2-�, � ������ ������
    128-�. ����������, ��������� ����� ��������� � ������ ������ 16-� ����.
    ������ ������, ����� ��������� ���� ���� ����� ������ 16 ����, � ������
    ������ 16 ������ �� ������ 256, �� ���� ��������� �����, ������������ �
    ������� 0, 16, 32..., � ���� �����, ������������ � ������� 0, 1, 2..., 15,
    256. 257... ����� �������, ��� MinLen=512 ����������� 8-������� ���������
    ������ (�� ���� 8-�������� ���������� ���������� ��������� � ������) ��
    ��������� � ������������� ����������� - ������, �� ���� ����������
    ���������� � ������ (� 1/64 ������� ������ �� 1/4, ��� �� ��� ������ ������
    ���������).

    �������, ��������� ������ �������� ������������� ������� ����� ������ �
    ���-������� ��� �������� ���������� ��� �� �������� ���-������� (chksum) -
    ����������, ���, ������� �� �������� ������ ������� � ���-�������. ���
    ��������� ������� ������� ����� ������ ����������, �� ��������� ����������
    ������, � ��� ����� ��������� ���������� ��������� � ������ � ��� ������
    �������� ������ ���������.

    � ��������� ������������ ����������� � ������ ����������, ��� ����������
    �����������, ��� ������ ���������� ����� �������. �������� ���-�������
    �� 256-�������� ����� (� ����� ������ ������ ����� ����� - L=MinLen/2)
    ������������ ��� ������ � ���-������� (hasharr[hash&HashMask]), ���
    ��������� ����� ���� ������ �������� ����� ������. �� �������� ���
    (�����������) �� ���� � ���������� ����������. ��� ��� ���������, ���
    ���� ��������, � ������� �� ����������� LZ77 ����������, ���� ��
    ����������� (����� �������) ����������, � ��������� ���� ���� ������ - ��
    ��������� ����, ������� ����� ���� ���-����, � ��� ��, �������������,
    ���������������� ��������� � �� �������� �����.

    ������ ���� (HashSize): ��� ���������� ��������� � �����������, ��� ��
    ������ ���� � 2-4 ���� ������ ���������� ���������, ������� � ���� �������
    ���������. ������ �� �������� ���������, ��� ������ ���������� ����� �� ��
    ����� ���-�� ������, � ��� MinLen=32 - ���� �������� (!) �������. �� ����,
    ��������, ��� 32 �� ����� ��� MinLen=512 � ��� ����������� ������ 16-�
    256-������� ���� � ����� ���������� ����������� ��������� - 32���/16=2���,
    �.�. 8 ��, � ��� �������� ������ ������ �������. ��� MinLen=32 �����
    ���������� ��������� 32���/4=8���, �� �� ������ ���-������� ��������
    ������, �� ���� ���������� �� �� ����� 8 ��. ����� �������, �����������
    ���������� ������������� ������ ���-������� ������� �� ����������� 1/4
    ������� �������� ������. ���� �� ������ ���������� ������ �������� - ��
    ����������� �������� HashBits (����� -h). ���������� HashSize ��� ���������
    MinLen �������� ������� ��������� ������� ������.

    Amplifier: ��� ���� ������� ����, ��� ������ ����������� ������ �����
    ������, ������� �� � ��������� ������� ��� ���������� ���� ����� � ������
    >=MinLen - ���� � ��� ��������� �����������. ������ ���� �����������
    ����������, � ����� ������������� ����� ��-�� ����� ��������. ��������
    Amplifier (����� -a) ��������� ����������� ������������ �������� �����
    ������ (� ��� ����� Amplifier ���). ����� �������, ��� �����������
    ����������� ������ ����� ������ ���������� Amplifier � ���������� �������
    ��������, ������ 99. ����������, ��� ��������� �������� � ���� ���������
    ����������� ������.

    Barrier � SmallestLen: ��������� ���������, � ��������� ppmd, ����������,
    ���� ������������ ���������� ������� �������� MinLen ��� �������
    ���������. ��� ��� ��������� ��������� ���������� ��������������� �������
    ������ ����������, �������� "� ������ ��������� - MinLen=128, �����
    MinLen=32" ������� ����� MinLen=128, Barrier=1<<20, SmallestLen=32
    (����� -l128 -d1048576 -s32). ��� ���� ����� ����� �������������, ��-��,
    �� ���������� ����� � ������ �� SmallestLen ������ MinLen.


** Benchmarks using 1GHz processor ****************************************************************

Test results for 26mb:
        Compression time   Compressed size
-l8192  0.5 seconds
 -l512  1.1
 -l128  1.4
  -l32  2.5                12.7 mb
lrzip   2.6                14.1
lzp:h20 6.5                13.1
lzp:h13 3.0                20.6

Compression speed on incompressible data:
-l8192  52 mb/sec
 -l512  25 mb/sec
 -l128  17 mb/sec
  -l32   8 mb/sec
lrzip    8 mb/sec


** REP �� ������� ������ ������� ���**************************************************************

������, ��� �� �������� �������� ������ rep � �������� ���������. �� ���������
� ��� ������������ ������ 1�� ��� ����� ������ � �������� ������ - ��� �������.
������� ������� ������ ���������� ������ ������, ��� ����������� 4-������� ���,
������� ����������� �� ���� 512 ���� - �������� � ������ ��������. ������
����������� ����, ��� �� ������ ������ ���� 16-������� cryptographically strong
hash - ���� md5. ����� ������� ����� ���������� �����. ����� ����, �� ���
���-������� �������� �������� �� ������ ��������������� ������ - ���
�������������. ���� ������� ����� ��� ��� ������� 256-�������� ����� ������, ��
��� ����������� ��� ���������� ���� ������ ����� �� 511 (��������� ����� �����
���� �������� ��� ������� ���� ������ 256-������� ����, ������������ �
256-������� �������). �.�. ��� ������ ����� ����� 511+ c N-�� ��������
���������� ������ � N/16 ��

�������� ��������� ������ ��� ���������� :D  ���� ��� �������� ������ ������
��� � �� ����� - ���������� ����� 100% ����������� � �� ����������, �� ���
���������� ��� ���-����� ���� �� ���������� �� ������� ����� :D  ���� �������,
��� �� ��� ��� ������ ����� ������� �� ����� ������ ���, �� ����������� ������
������ ��������� �������� ������ � �����, ��������� ������� �� �������
����������� ����� disk seek time - �.�. 10 �� ��� ����� � 1 �� ��� �����
������� ������

���������� ����, ��� �� ����� ���������� �������� ���������� ������ 1 ��/�. ���
��������, ��� ������ 10 �� �� ������ ������������� ��� ������� 10 ��, ��� �
���� ������� ������������� ������ ���� � ��� rep �������� ������ ����������
����� 10��+

������, ���� ������������ �� ����������� ����� ����� 4��+, �� ��� ������
����������� N/128 �� ������ (�.�. ���� 18 ��� ����� �����������, ���������
����� 160 ��� ���) � �������� ���������� ����� ���������� 400 ��/�. ��� ������
���� ����� :D

Ghost, �������� ��� �������� - ��� �������� ������ ����� ������ ��� �������� ��
rep:512 (�� ���������) � rep:4096? � ���������� ���������� lzma � ��� ����.
�������, ��� ���� ��������, ��������� ������� rep ������ ��������� �������
��������� �� ����� :(  �����, ��� ��� ���� �������� �� ������� ����, ���
����������� ����������...

*/


// �������� ************************************************************************
#include "../Compression.h"


#ifdef REP_LIBRARY
#define stat1(nextmsg,size)
#else
void stat1 (char *nextmsg, int Size);
#endif


// ����� ��������� ������ **********************************************************************
#ifndef REP_LIBRARY
// ����� ����������, ���������� �� stdout
//   0   ������ ������
//   1   ����� ����������
//   2   ��������� ���������� � ��������
static int verbose = 0;

#endif


// ��������������� ������� *********************************************************************

template <class T>
T power (T base, unsigned n)
{
  uint32 result = 1;
  uint32 b = (uint32)base;
  while (n) {
    if (n % 2)  result*=b, n--;
    n /= 2;  b*=b;
  }
  return (T)result;
}

// ���������� ������� base, �� ������������� sqrt(n),
// �������� sqrtb(36,2) = 4
inline static unsigned sqrtb (unsigned n, unsigned base = 2)
{
    int result;
    for (result=1; (n/=base*base) != 0; result *= base);
    return result;
}

// ������� ����� ������ ����������, ��� ����� �� *p � *q
static inline byte* find_match_start (byte* p, byte* q, byte* start)
{
    while (q>start)   if (*--p != *--q)  return q+1;
    return q;
}

// ������� ����� ������� �������������� �����, ��� ����� �� *p � *q
static inline byte* find_match_end (byte* p, byte* q, byte* end)
{
    while (q<end && *p==*q) p++,q++;
    return q;
}

// �������� ������ �� ������ � �����, ��� � ������� ����������� �������
// (��� �����, ��������� ������ ����� ������������ � � ���� ������ �����
// ���������� ������������ ������)
static inline void memcpy_lz_match (byte* p, byte* q, unsigned len)
{
    if (len)
    do *p++ = *q++;
    while (--len);
}


// �����, ������������ ��� ����������� ���������� ����������� ������� ������
// � ���������. ����� ����� ���������� � ���� 32-��������� �����. �������
// ���������� ������ ������������ � �������� �����.
// ������������� ����� ������������ ������ ����� ���������� � ���� ������.
// ����� ���������� ����� ������ - ��� max(p,end), ��� p - ������� ���������,
// � end - ������������ ������� ����� ���������� ������.
// �������� ������������ �� ������������, ��������� �������� �����������,
// ��� ������������ �� ���������.
namespace {
struct Buffer
{
    byte*  buf;                 // ����� ����������� ������
    byte*  p;                   // ������� ��������� ������/������ ������ ����� ������
    byte*  end;                 // ����� ����� ����� �����������/���������� ������
    byte*  bufend;              // ����� ����������� ������
    byte   smallbuf[16];        // ��������� �����, ������������ ��� ������ ��������� ��������
    int    len()                { return mymax(p,end)-buf; }
    Buffer (int size)           { buf=p=end= (byte*) malloc(size);  bufend=buf+size;}
    ~Buffer()                   { free(); }
    void   free ()              { if (buf) { ::free(buf); }  buf=p=end=NULL; }
    int    remainingSpace()     { return bufend-p; }
    void   reserve(int n)       {
                                  if (remainingSpace() < n)
                                  {
                                    int used    = p-buf;
                                    int endoff  = end-buf;
                                    int newsize = mymax(used+n, (bufend-buf)*2);
                                    byte* newbuf = (byte*) realloc(buf, newsize);
                                    buf = newbuf;
                                    bufend = buf+newsize;
                                    p = buf+used;
                                    end = buf+endoff;
                                  }
                                }
    void   put32(int x)         { reserve(sizeof(int32)); *(int32*)p = x; p+= sizeof(int32); }
    void   put(void *b, int n)  { reserve(n); memcpy(p,b,n); p+= n; }
    void   rewind()             { end=mymax(p,end); p=buf; }
    int    get32()              { int x = *(int32*)p; p+= sizeof(int32); return x; }
    bool   eof()                { return p>=end; }
    void   empty()              { p=end=buf; }
};
}

// �������� 32-������ ����� � �������� �����
#define Put32(x)                                           \
{                                                          \
    Buffer header(sizeof(int32));                          \
    header.put32 (x);                                      \
    FWRITE (header.buf, header.len());                     \
    header.free();                                         \
}


// �������� �������� *********************************************************************

/*
    ��� ���������� ���������� ������ �� MinLen ���� ����� �������� � ��� ��������
    ����������� ������� �� ������ ������ L = MinLen/2 ���� � �������� k = sqrt(L) ����.
    ������ � ���� ���-������� ���������� ��� ������, ������������ � ������ test=k ������
    �� ������� ����� ������ L ����.
*/

#define update_hash(sub,add)                        \
{                                                   \
    hash = (int32)((uint32)hash * (uint32)PRIME     \
                 + (uint32)(uint8)(add)             \
                 - (uint32)(uint8)(sub) * (uint32)PRIME_power_L); \
}

const int PRIME = 153191;

const int MAX_BLOCK = 8*mb;
const int MAX_READ  = 8*mb;


// ��������� ���������� ��������� ����
MemSize CalcHashSize (MemSize HashBits, MemSize BlockSize, int SmallestLen, int MinMatchLen, int ChunkSize, int Amplifier, int *L)
{
    int Len = mymin(SmallestLen,MinMatchLen);
    *L = ChunkSize? ChunkSize : rounddown_to_power_of(Len-1,2)/2;
    return HashBits>0? (1<<HashBits) : roundup_to_power_of(BlockSize/3*2,2) / mymax(*L/4,16);
}

#ifndef FREEARC_DECOMPRESS_ONLY

#define MULTI_THREADING_BASICS
#include "../MultiThreading.h"

struct Job
{
    int*   hashtable;
    byte*  buf;
    int    bytes;
    int    L;

    void process()
    {
        int   PRIME_power_L = power(PRIME,L);
        int  *hashptr       = hashtable;
        byte *ptr           = buf;
        int   num_blocks    = bytes/L;
        if (num_blocks > 0)
        {
            int hash=0;  for (int i=0; i < L; i++)  update_hash (0, buf[i]);

            for (int block=0; block<num_blocks; block++) {
                int maxhash = hash, maxi = 0;
                for (int i=0; i<L; i++, ptr++) {
                    if unlikely(hash > maxhash)
                        maxhash = hash, maxi = i;
                    update_hash (ptr[0], ptr[L]);
                }
                *hashptr++ = maxhash;
                *hashptr++ = maxi;
            }
        }
    }
};


int rep_compress (unsigned BlockSize, int MinCompression, int ChunkSize, int MinMatchLen, int Barrier, int SmallestLen, int HashBits, int Amplifier, CALLBACK_FUNC *callback, void *auxdata)
{
    if (SmallestLen>MinMatchLen)  SmallestLen=MinMatchLen;
    int L;
    int HashSize = CalcHashSize (HashBits, BlockSize, SmallestLen, MinMatchLen, ChunkSize, Amplifier, &L);
    int HashMask = HashSize-1;
    int *hasharr=NULL;  int errcode=FREEARC_OK;
    int DataEnd=0, last_i=0, last_match=0;

    byte *buf = (byte*) BigAlloc(BlockSize);
    if (buf==NULL)  return FREEARC_ERRCODE_NOT_ENOUGH_MEMORY;

    hasharr  = (int *) BigAlloc (HashSize * sizeof(int));
    if (HashSize && hasharr==NULL)  {BigFree(buf); return FREEARC_ERRCODE_NOT_ENOUGH_MEMORY;}
    memset (hasharr, 0, HashSize * sizeof(int));
    debug (verbose>0 && MinMatchLen==SmallestLen && printf(" Buf %d mb, MinLen %d, Hash %d mb, Amplifier %d\n", ((BlockSize-1)/mb)+1, MinMatchLen, (HashSize*sizeof(int))>>20, Amplifier));
    debug (verbose>0 && MinMatchLen!=SmallestLen && printf(" Buf %d mb, MinLen %d, Barrier %d, Smallest Len %d, Hash %d mb, Amplifier %d\n", ((BlockSize-1)/mb)+1, MinMatchLen, Barrier, SmallestLen, (HashSize*sizeof(int))>>20, Amplifier));

    FOPEN();

    int bsize = (mymin(BlockSize,MAX_BLOCK)/SmallestLen+1) * sizeof(int32);
    Buffer lens(bsize), offsets(bsize), datalens(bsize), dataOffsets(bsize);

    const int HTJOBS  = GetCompressionThreads(),
              HTBUFS  = mymax(16,HTJOBS*2),
              HTBLOCK = mymax(256*kb,L*4)/L*L,
              HTCOUNT = HTBLOCK/L*2;
    int free_jobs     = HTBUFS;
    int ht_index      = 0;
    int next_job_i    = 0;
    int next_flush    = 0;
    int *ht = (int*) BigAlloc(HTBUFS*HTCOUNT*sizeof(*ht));
    MultipleProcessingThreads<Job> HashingThreads;  HashingThreads.MaxJobs = HTBUFS;  HashingThreads.NumThreads = HTJOBS;
    if (ht==NULL || HashingThreads.start()!=0)  {errcode=FREEARC_ERRCODE_NOT_ENOUGH_MEMORY; goto finished;}

   {bool DoItOnce       = true;
    bool MoreInputData  = true;
    bool MoreHashedData = true;

    while (MoreHashedData) {
        int literals=0; lens.empty(), offsets.empty(), datalens.empty(), dataOffsets.empty();
#ifdef DEBUG
        int match_cnt=0, matches=0;
#endif

        next_flush += mymin (BlockSize-next_flush, MAX_BLOCK);
        while (last_i+L < next_flush) {

            while (free_jobs>0 && DataEnd<BlockSize && MoreInputData)
            {
                int Size;  READ_LEN(Size, buf+DataEnd, mymin (BlockSize-DataEnd, HTBLOCK));
                if (Size < 0)  {errcode=Size; goto finished;}
                if (DoItOnce)  {FWRITE4 (BlockSize); DoItOnce=false;}
                if (Size == 0)  MoreInputData = false;
                DataEnd += Size;

                int bytes =  DataEnd-(next_job_i+L);    if (bytes<=0)  {if (Size>0)  continue;  else bytes=0;}
                Job job   =  {ht+ht_index*HTCOUNT, buf+next_job_i, bytes, L};
                HashingThreads.Put(job);
                debug (verbose>0 && printf(" Read (%x,%x),  Job[%d] (%x,%x),  free_jobs %d\n", DataEnd-Size, Size, ht_index, next_job_i, bytes, free_jobs));
                next_job_i += bytes;
                ht_index = (ht_index+1)%HTBUFS;
                free_jobs--;
            }
            Job job = HashingThreads.Get();
            int *hashptr  = job.hashtable;
            int next_fill = job.buf+job.bytes-buf;
            free_jobs++;
            debug (verbose>0 && printf(" Hashed (%x,%x),  free_jobs %d\n", job.buf-buf, job.bytes, free_jobs));
            if (job.bytes==0)  {MoreHashedData=false; goto encode_data;}

            for ( ; last_i<next_fill; last_i+=L) {
                int hash = *hashptr++;
                int i    = *hashptr++ + last_i;
                if (i >= last_match) {
                    int match = hasharr[hash&HashMask];
                    if (match) {
                        if (match>=i && match<DataEnd)  goto no_match;
                        int LowBound  = match<i? i-match : match-DataEnd>i? 0 : i - (match-DataEnd);
                        int HighBound = BlockSize - match + i;
                        int start = find_match_start (buf+match, buf+i, buf+mymax(last_match,LowBound)) - buf;
                        int end   = find_match_end   (buf+match, buf+i, buf+mymin(DataEnd,HighBound)) - buf;
                        if (end-start >= (i-match<Barrier? MinMatchLen : SmallestLen) ) {
                            int offset = i-match;  if (offset<0)  offset+=BlockSize;
                            dataOffsets.put32 (last_match);
                               datalens.put32 (start-last_match);
                                offsets.put32 (offset);
                                   lens.put32 (end-start);
                            debug ((match_cnt++, matches+=end-start));
                            debug (verbose>1 && printf ("Match %d %d %d  (lit %d)\n", -offset, start, end-start, start-last_match));
                            literals += start-last_match;  last_match=end;
                        }
                    }
                }
    no_match:
                hasharr[hash&HashMask] = i;
            }
        }

    encode_data:
        if (next_flush==BlockSize || !MoreHashedData)  last_i=DataEnd;
        if (last_match > last_i) {
          datalens.put32 (0);
        } else {
          dataOffsets.put32 (last_match);
             datalens.put32 (last_i-last_match);
          literals  += last_i-last_match;
          last_match = last_i;
        }
        if (next_flush==BlockSize) {
          DataEnd=last_match=last_i=next_job_i=next_flush=0;
        }
        int outsize = sizeof(int32)*2+lens.len()+offsets.len()+datalens.len()+literals;
        QUASIWRITE (outsize);
        FWRITE4 (outsize-sizeof(int32));
        FWRITE4 (lens.len()/sizeof(int32));
        FWRITE (    lens.buf,     lens.len());
        FWRITE ( offsets.buf,  offsets.len());
        FWRITE (datalens.buf, datalens.len());
        dataOffsets.rewind(); datalens.rewind();
        while (!dataOffsets.eof()) {
            FWRITE (buf + dataOffsets.get32(), datalens.get32());
        }
        FFLUSH();
        debug (verbose>0 && printf(" Compressed %d -> %d (%d + %d), %d bytes in %d matches, free_jobs %d\n", literals+matches, outsize, outsize-literals, literals, matches, match_cnt, free_jobs));
    }}

   {int datalen = DataEnd-last_match;
    if (datalen) {
        FWRITE4 (sizeof(int32)*2 + datalen);
        FWRITE4 (0);
        FWRITE4 (datalen);
        FWRITE  (buf+last_match, datalen);
    }}
    FWRITE4 (0);
finished:
    FCLOSE();
    BigFree(hasharr);
    if (errcode>=0) {
        HashingThreads.finish();
        BigFree(ht);
    }
    BigFree(buf);
    return errcode>=0? 0 : errcode;
}
#endif // FREEARC_DECOMPRESS_ONLY


// Classical LZ77 decoder with sliding window
int rep_decompress (unsigned BlockSize, int MinCompression, int ChunkSize, int MinMatchLen, int Barrier, int SmallestLen, int HashBits, int Amplifier, CALLBACK_FUNC *callback, void *auxdata)
{
    int errcode;
    byte *buf0 = NULL;
    MemSize bufsize, ComprSize;
    // ������ ��� ������� ����� ���������� ����������� ������� ����� �� �������� �� ������������ ��������� ������������
    const int MAX_BLOCKS = 100;                      // ����. ���������� ���������� �������
    int       total_blocks = 0;                      // �������� ���������� ���������� �������
    byte     *datap[MAX_BLOCKS];                     // ������ ����� �������, ���������� ��� �������
    byte     *endp[MAX_BLOCKS];                      // ������ ������ �������

    // ����������� ������ ������� �������� �� ������� ������
    READ4(BlockSize);

    // ����, ���������� ������ ��� ������� ���������� ������� ��� ����� �������� �������
    {
    MemSize   cumulative_size[MAX_BLOCKS+1] = {0};   // ��������� ������ �������, �������������� i-��
    MemSize   remaining_size = BlockSize;            // ������� ��� ������ ����� �������� ��� �������
    while (remaining_size>0)
    {
        if (total_blocks >= MAX_BLOCKS)          ReturnErrorCode (FREEARC_ERRCODE_NOT_ENOUGH_MEMORY);
        MemSize data_size = remaining_size;
        for(;;)
        {
            if (NULL  !=  (datap[total_blocks] = (byte*) BigAlloc (data_size)))  break;
            if (data_size <= 1*mb)   ReturnErrorCode (FREEARC_ERRCODE_NOT_ENOUGH_MEMORY);
            data_size     -= 1*mb;
        }
        endp[total_blocks] = datap[total_blocks] + data_size;
        remaining_size -= data_size;
        total_blocks++;
        cumulative_size[total_blocks] = cumulative_size[total_blocks-1] + data_size;
    }

    // �����, ���� ����� ���������� ������� ������
    bufsize = mymin(BlockSize,MAX_READ)+1024;
    buf0 = (byte*) BigAlloc (bufsize);
    if (buf0==NULL)  ReturnErrorCode (FREEARC_ERRCODE_NOT_ENOUGH_MEMORY);

    // ����, ������ �������� �������� ������������ ���� ���� ������ ������
    int current_block = 0;            // ����� �������� ������
    byte *start     = datap[0];       // ������ �������� ������
    byte *last_data = datap[0];       // ������ ������������ ����� ������
    byte *data      = datap[0];       // ������� ��������� ������
    byte *end       = endp[0];        // ����� �������� ������     (start <= last_data <= data <= end)
    for(;;)
    {
        // ��������� ���� ���� ������ ������
        READ4(ComprSize);
        if (ComprSize == 0)  break;    // EOF flag (see above)

        if (ComprSize > bufsize)
        {
            BigFree(buf0); bufsize=ComprSize; buf0 = (byte*) BigAlloc(bufsize);
            if (buf0==NULL)  ReturnErrorCode (FREEARC_ERRCODE_NOT_ENOUGH_MEMORY);
        }
        byte *buf = buf0;

        READ(buf, ComprSize);

        // ��������� ����� �������� ������ ������ lens/offsets/datalens; ����� ���� ���� ��� ������� � ������� ����������� ������
        int         num = *(int32*)buf;  buf += sizeof(int32);           // ���������� ���������� (= ���������� ������� � �������� lens/offsets/datalens)
        int32*     lens =  (int32*)buf;  buf += num*sizeof(int32);
        int32*  offsets =  (int32*)buf;  buf += num*sizeof(int32);
        int32* datalens =  (int32*)buf;  buf += (num+1)*sizeof(int32);   // ������, datalens �������� num+1 �������

        // ������ �������� ����� ����� �������� ���� ������ �������� ������ � ���� match, ������� interleaved � ����� ���������� �������� ��������
        for (int i=0; ; i++) {
            int len = datalens[i];
            // ���� ���������� ������ ���������� ������� ������
            while (end-data < len)
            {   //printf("  str: %d %d %d %d\n", BlockSize, data-datap[0], end-data, len);

                // �������� ��������� ������� � ���������� ����������� �����
                int bytes = end-data;
                memcpy (data, buf, bytes);  buf += bytes;  data += bytes;  len -= bytes;

                WRITE(last_data, data-last_data);

                // ����� � ���� ������ ���������, ��������� �� ���������
                if (++current_block >= total_blocks)   current_block = 0;
                last_data = start = data = datap[current_block];
                end = endp[current_block];
            }
            // ����������� ��� ����������� ������� ������
            memcpy (data, buf, len);  buf += len;  data += len;


            if (i==num)  break;   // � ����� ����� � ��� ��� ���� ���� ����������� ������ (��������, ������� �����) ��� ������� ��� lz-�����


            int offset = offsets[i];  len = lens[i];
            debug (verbose>1 && printf ("Match %d %d %d\n", -offset, data-start+cumulative_size[current_block], len));
            // ���� ���� �� ���������� ����� ���������� ������� ������
            while (offset > data-start && len  ||  end-data < len)
            {
                MemSize dataPos = data-start+cumulative_size[current_block];   // Absolute position of LZ dest
                MemSize fromPos = dataPos-offset;  int k;                      // Absolute position of LZ src
                if (offset<=dataPos) {
                    // ���������� ������ ����� ������� ��������, ��� ������� �������
                    for (k=current_block;  fromPos < cumulative_size[k]; k--);  // ���� ����, �������� ����������� ���������� ������
                } else {
                    // ���������� ������ ����� ������� ��������, ��� ������� �������
                    fromPos += BlockSize;
                    for (k=current_block;  fromPos >= cumulative_size[k+1];  k++);  // ���� ����, �������� ����������� ���������� ������
                }
                byte *from    = fromPos - cumulative_size[k] + datap[k];       // Memory address of LZ src
                byte *fromEnd = endp[k];                                       // End of membuf containing LZ src

                int bytes = mymin(len, mymin(end-data, fromEnd-from));  // How much bytes we can copy without overrunning src or dest buffers
                //printf("? %d-%d=%d %d(%d %d)\n", dataPos, offset, fromPos, bytes, end-data, fromEnd-from);

                // �������� ��������� �������
                memcpy_lz_match (data, from, bytes);  data += bytes;  len -= bytes;

                // ���� dest ����� �������� - ���������� ����������� �����, � ������������� �� ������
                if (data==end)
                {
                    WRITE(last_data, data-last_data);

                    if (++current_block >= total_blocks)   current_block = 0;
                    last_data = start = data = datap[current_block];
                    end = endp[current_block];
                }
            }
            // ����������� ��� ����������� ������� ������
            memcpy_lz_match (data, data-offset, len);  data += len;
        }

        // ����� ������������� ������, ������ ���������� ���������� � ���������� � ��������� �������� �����
        WRITE(last_data, data-last_data);
        debug (verbose>0 && printf( " Decompressed: %u => %u bytes\n", ComprSize+sizeof(int32), data-last_data) );
        last_data = data;
        // NB! check that buf==buf0+Size, data==data0+UncomprSize, and add buffer overflowing checks inside cycle
    }
    errcode = FREEARC_OK;}
finished:
    BigFree(buf0);
    for(int i=total_blocks-1; i>=0; i--)
        BigFree(datap[i]);
    return errcode;
}


/* to do:
+1. sliding window, In() function to read data
+2. ������������ ������, ���. ��� �������
+3. save pointers to unmatched blocks instead of copying data
4. ���������, ��� ���� ����� ����������, � �������� ��� ����� ���������.
     ������, ���������� ������� �������� last_match � �������� ��� �������� ������
5. last_small_match - ���� ��������� match ������ �� ��������� ���������� (<Barrier),
     �� ������������ ��������� ����� �� ������� ���������� (>Barrier) ���� ���� �� ��������.
     ��� �������� ��� ��������� �������� ���� � ������� ����� :)
6. -l8192 -s512
7. buffer data for Out() in 256k blocks

Fixed bugs:
1. �������� ������ �� ������� ������: offset<data-data0 ������ <=
2. last_match �� ��������� ��� ������ �� ����� ��� Base=0 � Size=0
*/
