#define COPYRIGHT "Delta: binary tables preprocessor v1.51  (c) Bulat.Ziganshin@gmail.com  2013-09-18"
/* All rights reserved. You can for free use decompression part of the algorithm for decompression of FreeArc archives.
   For any other usage ask me for the license.

This algorithm preprocess data improving their further compression. It detects tables
of binary records and 1) substracts sucessive values in columns, 2) reorder columns
trying to maximize results of further compression.

Algorithm includes 3 phases:

1) Preliminary table detection. It finds 6+ repetitions of the same byte at the same distance,
   i.e. anything like a...a...a...a...a...a where '.' denotes any byte except for 'a'.
   This is done in delta_compress

2) Candidates detected at first phase are then checked by FAST_CHECK_FOR_DATA_TABLE looking for
   monotonic sequence of bytes with fixed distance. Most candidates found at first stage are
   filtered out here

3) Remaining candidates are tested by slow_check_for_data_table() that finds exact table boundaries
   and detects columns that should and that shouldn't be substracted. Only if table is large enough
   it will be finally processed

The algorithm processes 200 MB/sec on i7-2600 (1GB/sec in multithreaded mode)



New to-do list (2012-12-05):
0. === 5..30-�������� ������� ���� ������� ����� 0.1% � �������, � �������� ������� � ��� ��� ��� ������ �����������
1. ��������� ����� ������� ������, �������� ������� � � �������� �/��� �����/int16, ������ � ���������� �������
2. ������ analyze ������ ��� ��� ������� ����������! ������������ ��� ����������� ��������������� ������� ������,
     ������� � ������������� � search_for_table_boundary
3. ��������� ������� �� ������������� ������� ����� analyze, ��������� ��������� �� ����������
     (����� ���-�� ���������� ������, � ��� ����� - ������� ���������� ��������)
4. ��� ��������� �������� ������ ����������� ��� ����������� �������� (�����. ����������)
5. *p/8? � ������ 24/12 - ������� �������� (��� �������� ����� ������ � ������� N)
6. ������ check/search/analyze ��� ��������� N (1-4,8)? ��� ��������� N ������������� int16 � search ����� ���� �������������
7. ���. �������� ��� N=1 �� p=ptr+LINE/2
8. ������������� ��� N-wide ������� �� �������� �� ����� ���� N*k-wide � ��������?
9. n=p-hash[], n1=p-hash1[], hash1[]=hash[], hash[]=p, count[n]++, count[n1]++ : �������� ��������� ������ ������
10. � ������ ����� � ����� �������� ������� ��������� hash[] (������������� - ������ ptr+=LINE � ��������� ��� ptr>=table_end ����� FAST_CHECK)
11. �������� ���������� ����� ���������� �� N-1 ����� ����� ������� �������� ������ ������, ��� FAST_CHECK ������� � ���������� offset.
    �������, ����� ��������� ��� offset ��� ������� N � ����� ������� �������� �������/������ �������,
    � ��� ����� ��������� ��� ��������� N/offset � �������� �������, ������ ���������� ��������� ��������
12. ��� ������ �� search �� table_start/end ������������� ��������� range, ����� �� ������ ��� ��� �������� ������������� ��������
13. � FAST_CHECK �������� �������� �� ���������� (���������� search), ����� �� ��������� ��� ������ ��������� ������ ��������� �����
    ��� ������ p[0]-p[N]<64 && p[N]-p[2N]<64 && ...  ||  p[N]-p[0]<64 && p[2N]-p[N]<64 && ...

=== �����������
1. n&255 -> BYTE hash[]
2. *p -> *(uint32*)p; �������� ��� ������� ������������� hash1
3. analyze/reorder - �������� ����� ��� N==2/4/8, diff1/2/4/8
4. search: ��������� ����� ��� diff>0 � diff<0, ��� �������� ���������� �� ��������� � abs
     "difflb<itemlb" �������� �� "difff<item/2..4"; ��������� *(uint16*)t ��� ����. �����
5. ���� ��� ��������� ������� ����� ����� � ����� ����, �� ����� ������ fast_check ��� ����?
   ��������, ��������� 12/5 ����� (�� 15/10?), LINE ��������, ���� ��������� �� ������� p, p+LINE/4... p+3*LINE/4
+6. for(...)  count1[i]=count[i],count[i]=0;...  if (count[i]+count1[i] > 12/5) ...
   ��� �������� �������� 12/5 �� ����� (?) ������� ��������, ��������� ������ ��� ��������� ��� ������� �������� ������� ����� ����� �������
   (����� ������� loop unrollling: count[]=0, count[]++, if(count[]+count1[]), count1[]=0, count1[]++, if(count[]+count1[]) )
   (�������������� ������: count[]/=2 ����� ������� �����)
7. ������ ����������� ����� count[]++ � FAST_CHECK, ��� �������� ������ �������� � ������ ����� ��-�� ������������ �� ������ � count
   (�������� ������ ������� � count2, � ��� FAST_CHECK ������������ count+count1)



To-do list:
+1. �������� ����������� ������� � �������� �� �������� � ������ ������� (��� ���������)
      ��������� ���������� ����� � ������� 4 ������!
2. �������� �������� ���������� ������������� � ��������������� ��������
3. ���������� ��������� �������� �������:
     +��������� ������� � �������� ��������� � ��������� ������ ����� (sqrt(N)*rows >= X)
     +��������� ���������� �� ���������� ������� ��� ����������� ��������� ������ ������
     �������� ������ ��� �������� � ������������� �������� ����� ���������� (���� 8,16,24,32...)
4. �������� ������� �������, �������� ���������� ����������� �����/������ � �������� ������� �������,
     ��� ���� ����� ������ ����� ���� SLOW �������� ��� ������� ���������� ����� ��������
     (������ ���������� ����� �������� ����� ����� ��������)
5. ��������� LINE �� 64, �������� �������� count[i]>5, ������� hash1 � �������� ��� 1-2 ������� ������
     LINE=64; hash1 only; ������ �������� ��� N=1,2 ��� �������� ���������� �����
+6. ������������� 4 5 6 1 2 3 ��� ���� "����� �����������" ������ ���� (��������� ���� ��������� ��������)
7. ���������� last_checked?
8. ��������� DELTA=4
9. MAXELEM=64+hash1 �������� �� 35�� ������ mdb, �������� �� 12-14�� ������ skype/wwlib/ruby
   MAXELEM=32+hash1 �������� �� 20�� ������ ruby

office.dll i7-2600 4.6GHz
FULL  4.948 seconds, speed 163.773 mb/sec
-ENCD 4.625 seconds, speed 175.242 mb/sec
-SLOW 2.820 seconds, speed 287.344 mb/sec
-FAST 1.802 seconds, speed 449.755 mb/sec
Tables 89334 * 665 = 59440712 (50039022) bytes (5.837.170 (215.745.931) /36.578.538 probes) 6.2 skipbits

after FAST_CYCLES optimization:
FULL  3.880 seconds, speed 208.848 mb/sec
-ENCD 3.522 seconds, speed 230.114 mb/sec
-SLOW 2.263 seconds, speed 358.149 mb/sec
-FAST 1.767 seconds, speed 458.766 mb/sec
Tables 89,374 * 669 (556) = 59,804,728 (49,705,953) bytes.  6.231 skipbits
Fast 2.066*15,551,956=32,124,055.  Slow 37.498*5,366,922=201,247,900

After count[n&255]++ optimization:
FULL  3.326 seconds, speed 243.642 mb/sec
-ENCD 2.961 seconds, speed 273.687 mb/sec
-SLOW 1.592 seconds, speed 508.947 mb/sec
-FAST 1.265 seconds, speed 640.537 mb/sec
Fast 2.066*15,555,409=32,140,479.  Slow 37.493*5,368,836=201,294,213.  6.237 skipbits
Tables 89,192 * 670 (557) = 59,822,474 (49,716,487) bytes.

p=ptr-FAST_CYCLES*LINE ��� N=5..30:
Fast 2.067*15,588,033=32,225,132.  Slow 37.387*5,406,747=202,142,831.  6.149 skipbits
Tables 90,339 * 664 (551) = 60,027,076 (49,825,741) bytes

�������� ����� ���������� ������������������� ����� ��� ���� N=1..30:
Fast 2.067*15,541,712=32,122,349.  Slow 35.298*5,271,232=186,065,728.  6.418 skipbits
Tables 89,189 * 668 (555) = 59,657,566 (49,555,106) bytes

Delta9
Fast 15,576,061*2.068=32,204,777.  Slow 5,379,507*36.93=198,648,121  incl. back 5,379,507*16.38=88,138,588
Tables 93,551 * 651 (540) = 60,911,339 (50,570,857) bytes.  5.999 skipbits
  fast  7,717,216*1  3,876,251*2  1,707,232*3  1,657,787*4  105,244*8  512,331*8.081=4,140,263
  slow  693,999*35.90=24,912,309  2,438,601*23.66=57,690,612  409,050*66.49=27,197,731  1,386,627*53.64=74,383,470  114,826*32.85=3,772,331  336,404*31.78=10,691,668
  tables  1,095*293=320,497  27,545*461=12,700,330  5,913*340=2,012,448  44,280*488=21,590,132  4,163*774=3,221,824  10,555*1996=21,066,108

Delta10: count[]+count1[] >= 24/12
Fast 14,072,083*1.990=28,000,094.  Slow 5,187,047*38.17=197,978,444  incl. back 5,187,047*16.96=87,983,926
Tables 91,713 * 656 (544) = 60,184,771 (49,927,197) bytes.  5.955 skipbits
  fast  7,323,338*1  3,576,324*2  1,271,677*3  1,401,217*4  91,396*8  408,131*8.265=3,373,041
  slow  689,510*36.12=24,904,369  2,447,211*23.66=57,889,971  374,772*69.94=26,210,985  1,264,064*59.65=75,401,244  107,070*31.50=3,372,632  304,420*33.50=10,199,243
  tables  1,084*293=318,071  27,593*462=12,741,550  5,891*337=1,983,198  42,545*494=21,020,580  4,076*779=3,176,136  10,524*1990=20,945,236
FULL  3.257 seconds, speed 248.855 mb/sec  "-DPROFILING=4"
-ENCD 2.866 seconds, speed 282.724 mb/sec  "-DPROFILING=3"
-SLOW 1.780 seconds, speed 455.403 mb/sec  "-DPROFILING=2"
-FAST 1.375 seconds, speed 589.201 mb/sec  "-DPROFILING=1"

18.09.2013: g++ -O2 -fomit-frame-pointer
Delta D:\Testing\office.dll (810 mb)
FULL  2.878 seconds, speed 281.566 mb/sec  "-DPROFILING=4"
-ENCD 2.734 seconds, speed 296.472 mb/sec  "-DPROFILING=3"
-SLOW 1.549 seconds, speed 523.235 mb/sec  "-DPROFILING=2"
-FAST 1.208 seconds, speed 670.610 mb/sec  "-DPROFILING=1"


skype - ����������� �������:
58602 17
587e0 14
cdc0ce 2

table5: table20070921181100.rar
FULL  Compression: 0.911 seconds, speed 21.394 mb/sec
-SLOW Compression: 0.480 seconds, speed 40.605 mb/sec
-FAST Compression: 0.320 seconds, speed 60.907 mb/sec

fast+slow checks by N:
total    727111
1 283578 682737
2 132068
4 162505
3 104586


Old list:
+1. ��������� ������� �� ��������� (������ C � GX)
+2. ����� ���������� ����� ����� ������������� �������� ���������� �������� ������� �� � ����� ���������
3. �� ���� ��������� ��������� ������� � ���, ���������� �� ��� ������� ������
4. ����������������? �������� ������� � ������ ��������� (���������� ����������� ������)
-5. hash *p/16 -> /8 ��� /32?
+6. �������� �� 64�� �� ������ ������� � �������������� ����������� ��� �������������
+7. ��� �������� ��������� ������� ���������� ��� �������, ��� ������ ��������� ��� ptr=table_end-32 � ������
-8. ������� ���������� ���������� ����� (� �������� ������� �������) �� � ����� ���������/xor
9. ����������, �������� ������� �������, �������� ���������� ����������� ����� � �������� ������� �������
+10. else carry = 0;
+11. if (*(int32*)ptr != *(int32*)(ptr+3))   //  a little speed optimization

�������� �������:
+1. 0C,04,FC... - ��� �������� ����� 0 �������� ������ ����� ��������,
      ������� ���� ������ difflb<itemlb/1.1? len++:omit++
+2. ���� ����� �� last_table_end ��� ��������� LINE �� 64 (�������� �������� count[i]>5)
+3. ������������ ��� ���������� ������� ����� ����� �� ��������, ��� � ��� ���������� �����?
+4. ��������� ������� � �������� ��������� � ��������� ������ ����� (sqrt(N)*rows >= X ?)
+5. CHECK_FOR_DATA_TABLE: ����������� p[-1]+p[N-1] != p[2*N-1]+p[3*N-1] ����� �� ��������
      ��-�� ���������� ���������� � ���� �������� ��������� �������
+6. DELTA=8 (����. ������� ����� ��������� ���������� � ����� �������, ������� ��������������� ��� ������� �� �������)
-7. 29-������� ������� ����������, ������ ��-�� *p/16 (�������� �� /8?)
8. �������� ������ ��� �������� � ������������� �������� ����� ���������� (���� 8,16,24,32...)
-9. �������� �������� ������� ����� ������� �� ����� ������ ����� ������� (500->400?)

���������� ���������:
+1. ���������� �����, ������� ��������
+2. ������ last_checked?
3. ������ � 64 ����� � ��� �������� (+0, +32) ��� 1-�������� ������
+4. �������� ��������� ����������� ���� ��������� lines (count[i]/=2 ?)
5. �������� ���������� 1-2-������� ���������; �������� ��� ������ �� 32-� �����, unicode-�������
+6. ������������ 4 ���� �� 32 �����, �������� ����� ������ ������ ������ �� 1-4-������� �������
7. ������� ������� CHECK_FOR_DATA_TABLE ��� 1, 2 � 3+ ������� ������


340ms   592 ns/line  18.5 ns/b = ������ ���������� count
470ms   634 ns/line  24.3 ns/b + ���� �� count
610ms                          + �������� ������ 1-4 �������� ������
750ms 1.230 ns/line  38.4 ns/b + �������� ���������� ������
*/

// ALGORITHM PARAMETERS ************************************************************************

// Maximum size of one table element (31 max. with current `type` encoding scheme)
#define MAX_ELEMENT_SIZE 30

// Elements with size up to 4 are checked every cycle...
#define SMALL_ELEMENT_SIZE 4

// ... and the rest - every 4th cycle
#define FAST_CYCLES 4

// ������ ������ �����, � ������� ������ ���������� ���������
#define LINE 32

// ������������ ���������� �� ����������� �������� � ��� �� �������,
// ������� �� ��������� ��� ����� ������� �� �������� �������
// � FAST_CHECK_FOR_DATA_TABLE
#define DELTA 8


// C HEADERS **************************************************************************************
#include "../Compression.h"

struct Buffer
{
    byte*  buf;
    byte*  p;
    byte*  end;
    byte*  bufend;

    Buffer (uint size=64*kb) { buf=p=end= (byte*) malloc(size);  bufend=buf+size; }
    ~Buffer()                { free(); }
    void   free()            { ::free(buf);  buf=p=end=NULL; }
    void   empty()           { p=end=buf; }
    int    len()             { return mymax(p,end)-buf; }

    void   put8 (uint x)     { reserve(sizeof(uint8 ));  *(uint8 *)p=x;    p+=sizeof(uint8 ); }
    void   put16(uint x)     { reserve(sizeof(uint16));  *(uint16*)p=x;    p+=sizeof(uint16); }
    void   put32(uint x)     { reserve(sizeof(uint32));  setvalue32(p,x);  p+=sizeof(uint32); }

    void   put(void *b, int n)  { reserve(n);  memcpy(p,b,n);  p+=n; }

    int    remainingSpace()  { return bufend-p; }
    void   reserve(uint n)   {
                               if (remainingSpace() < (int)n)
                               {
                                 uint newsize = mymax(p+n-buf, (bufend-buf)*2);
                                 byte* newbuf = (byte*) realloc (buf, newsize);
                                 bufend = newbuf + newsize;
                                 p   += newbuf-buf;
                                 end += newbuf-buf;
                                 buf  = newbuf;
                               }
                             }

    void   rewind()          { end=mymax(p,end);  p=buf; }
    uint   get8 ()           { uint x = *(uint8 *)p;  p+=sizeof(uint8 );  return x; }
    uint   get16()           { uint x = *(uint16*)p;  p+=sizeof(uint16);  return x; }
    uint   get32()           { uint x = value32(p);   p+=sizeof(uint32);  return x; }
    bool   eof()             { return p>=end; }
};


// OPTIONS FOR STANDALONE EXECUTABLE **************************************************************
#ifndef DELTA_LIBRARY
// ����� ����������, ���������� �� stdout
//   0   ������ ������
//   1   ����� ����������
//   2   ��������� ���������� � ��������
static int verbose = 0;

// Print contents of found tables
static int print_tab = 0;

// �������� ����� ���������� ������� ���� ���������
static int print_timings = 1;

// Current offset of buf[] contents relative to file (increased after each input block processed)
static uint64 offset = 0;
// Total stats for sucessfully processed tables
static uint64 table_count=0, table_sumlen=0, table_diffed=0, table_count_by[MAX_ELEMENT_SIZE], table_sumlen_by[MAX_ELEMENT_SIZE];
static double table_skipBits=0;
// Count of FAST_CHECK_FOR_DATA_TABLE and slow_check_for_data_table calls and iterations inside calls, total and by N
static uint64 fast_checks_by[MAX_ELEMENT_SIZE], fast_checks=0, fast_iterations=0,
              backward_slow_iterations_by[MAX_ELEMENT_SIZE], slow_iterations_by[MAX_ELEMENT_SIZE], slow_checks_by[MAX_ELEMENT_SIZE],
              slow_checks=0, backward_slow_iterations=0, slow_iterations=0;
// Buffers for printing numbers in comma-delimited format
static char table_count_str[100], table_sumlen_str[100], table_diffed_str[100], fast_checks_str[100], fast_iterations_str[100], slow_checks_str[100], slow_iterations_str[100];
#endif

// Used for early exit for the profiling purposes
#ifdef PROFILING
#define profiling(level,statement)  if(level>=PROFILING) statement;
#else
#define profiling(level,statement)
#endif


// UTILITY FUNCTIONS ******************************************************************************

// type �������� ������ �������. ����� �������� �������������� ���� - � ������,
// ��������� ����=1 ���� ��������������� ������� �� ����� ��������

// Encode `type` word from N, doDiff[] and immutable[] values.
inline static uint32 encode_type (int N, bool doDiff[], bool immutable[])
{
    uint32 type = 1<<N;
    for (int i=0; i<N; i++) {
        type += immutable[i] << i;
    }
    return type;
}

// Decode `type` word into N, doDiff[] and immutable[] values
static void decode_type (uint32 type, int &N, bool doDiff[], bool immutable[])
{
    int i;
    for (i=0; type>1; i++, type>>=1) {
        immutable[i] = type&1;
        doDiff[i]    = !immutable[i];
    }
    N=i;
}

// Process data table subtracting from each N-byte element contents of previous one
// (bytewise with carries starting from lower address, i.e. in LSB aka Intel byte order).
// bool doDiff[0..N-1] marks columns what should be diffed,
// other columns are left untouched. Carry saved only over adjancent diffed columns
inline static void diff_table (int N, BYTE *table_start, int table_len, bool doDiff[])
{
    for (BYTE *r = table_start + N*table_len; (r-=N) > table_start; )
        for (int i=0,carry=0; i<N; i++)
            if (doDiff[i]) {
                int newcarry = r[i] < r[i-N]+carry;
                r[i]        -= r[i-N]+carry;
                carry        = newcarry; }
            else carry = 0;
}

// Process data table adding to each element contents of previous one
static void undiff_table (int N, BYTE *table_start, int table_len, bool doDiff[])
{
    for (BYTE *r = table_start + N; r < table_start + N*table_len; r+=N)
        for (int i=0,carry=0; i<N; i++)
            if (doDiff[i]) {
                int sum = r[i]+r[i-N]+carry;
                r[i]    = sum;
                carry   = sum/256; }
            else carry = 0;

}

// Reorder table so that all immutable columns are placed before all mutable ones.
// bool immutable[0..N-1] marks immutable columns
static inline void reorder_table (int N, BYTE *table_start, int table_len, bool immutable[], Buffer &tempbuf)
{
    // First, copy all the data into temporary area
    tempbuf.reserve (N*table_len);
    memcpy (tempbuf.buf, table_start, N*table_len);

    // Then, copy contents of immutable columns into the table beginning
    BYTE *p=table_start, *q=tempbuf.buf;
    for (int i=0; i<table_len; i++)
        for (int k=0; k<N; k++, q++)
            if (immutable[k])
                *p++ = *q;

    // And last, copy rest of data to the table end
    q=tempbuf.buf;
    for (int i=0; i<table_len; i++)
        for (int k=0; k<N; k++, q++)
            if (!immutable[k])
                *p++ = *q;
}

// Undo effect of reorder_table()
static void unreorder_table (int N, BYTE *table_start, int table_len, bool immutable[], Buffer &tempbuf)
{
    // Count number of immutable columns. Exit if reordering isn't required
    int imm_columns=0; iterate_var(i,N) imm_columns+=immutable[i];
    if (imm_columns==0 || imm_columns==N)  return;

    // First, copy all data into temporary area
    tempbuf.reserve (N*table_len);
    memcpy (tempbuf.buf, table_start, N*table_len);

    // Gather immutable and mutable columns together
    BYTE *p=table_start, *q=tempbuf.buf, *q1 = tempbuf.buf+imm_columns*table_len;
    for (int i=0; i<table_len; i++)
        for (int k=0; k<N; k++)
            *p++ = immutable[k]? *q++ : *q1++;
}


#ifndef FREEARC_DECOMPRESS_ONLY
// TABLE COLUMNS ANALYSIS *********************************************************************

// Analyze which table colums need to be diffed and which ones are (almost) immutable
static void analyze_table (int N, BYTE *table_start, int table_len, bool doDiff[], bool immutable[])
{
    // �������� ������ ������� ��������
    for (int k=0; k<N; k++) {
        // ��� ������� �� ����� �� 4 ���������:
        //   (�����) �����������
        //   (�����) ����������� ����� ���������
        //   ���������� (�������� ����������� ����� ���������)
        //   ���������� "���������" ������
        // (����� ��� �������� ��� xor-���������)
        // ���� ��� �������� �� ���������� ������ ����������� �������,
        //   � ��� ��������� ������� �����������

        BYTE *p = table_start+k; int neq=0;
        for (int i=1; i<table_len; i++, p+=N) {
            neq  +=  p[N]!=p[0];
        }

        // �������� ������������ �������: ����������(p[i]!=p[i+1]) < 1/4 �� ����� ���������
        immutable[k] = neq*4 < table_len  && N!=2 && N!=4 && N!=8;
        if (immutable[k]) {
            stat_only (verbose>0 && printf (" %d", k));
        }
        // � ��������� ������ ���� ������� ������ �������� �� ���������
        doDiff[k] = !immutable[k];
        if (doDiff[k]) {
            stat_only (table_diffed += table_len);
        }
    }
}


// DISCOVERING TABLE BOUNDARIES *********************************************************************
// Check the following data for presence of table which will be better compressed
// after subtraction of subsequent elements and find it's exact boundaries

// ��������� ������� �������, �������������� � ��� ������, ����� ������� ������������ ���������� ������� ���� (�����<4)
static BYTE* search_for_table_boundary (int N, BYTE *t, byte *bufstart, byte *bufend, int &_useless)
{
    int dir = (*(int16*)(t+N) - *(int16*)t < 0)? -1:1,  len=0, omit=0, useless=_useless=0, bad=0;
    BYTE* lastpoint=t;  bool first_time=TRUE;
    for (t+=N; bufstart<=t+N && t+N+sizeof(int16)<=bufend; t+=N) {
        stat_only ((slow_iterations_by[abs(N)]++,  N<0 && backward_slow_iterations_by[-N]++));
        int diff = *(int16*)t - *(int16*)(t-N);
        uint abs_diff = abs(diff);
        uint abs_item = abs(*(int16*)t);
        uint cmp_item = (abs_item>4095? abs_item/6 : abs_item/3);
        stat_only ((verbose>2 && printf("%08x omit %d  useless %d  abs_diff %d  cmp_item %d\n", int(t-bufstart+offset), omit, useless, abs_diff, cmp_item)));
             if (diff==0)      useless++;
        else if (dir*diff>0)   abs_diff<cmp_item?  (len++,omit=0)  :  (useless++,omit++);
        else {
            stat_only ((verbose>2 && printf("===\n")));
            if (len>=4 || first_time)  bad=0, lastpoint=t-N*omit, _useless=useless, first_time=FALSE;
            else if (++bad>=2) break;  // �����, ���� ��� ������ ������ ������� ������������ � ������ <4
            dir = (*(int16*)(t+N) - *(int16*)t < 0)? -1:1;  len=omit=0;
            if (dir*diff>0)  t-=N;   // ������ ������� ������������ ����� � �������� ��������, ���� ������� ����� ��� V (�����-������), � �� ����������, ���� �� ����� ��� N (������-������-������)
        }
    }
    return lastpoint;
}

// ���������, ����� �� ������� �������� ����� � ������� p � ������� ��������� N
static bool slow_check_for_data_table (int N, BYTE *p, uint32 &type, BYTE *&table_start, BYTE *&table_end, BYTE *bufstart, BYTE *bufend, BYTE *buf, Buffer &ReorderingBuffer)
{
    profiling(2,return FALSE);

    // ������� ��������� �����, ������� � p, � ������� ������ �������
    int useless;
    table_start = search_for_table_boundary (-N, p,           bufstart, bufend, useless);
    // ����� ��������� �����, ������� � table_start, � ������� ����� �������
    table_end   = search_for_table_boundary (N,  table_start, bufstart, bufend, useless);

    // +��������� ������� � �������� ��������� � ��������� ������ ����� (sqrt(N)*rows >= X)
    // +��������� ���������� �� ���������� ������� ��� ����������� ��������� ������ ������
    // �������� ������ ��� �������� � ������������� �������� ����� ���������� (���� 8,16,24,32...)
    // ������� ���������� ������, �������� ������� ����������� �� ��������� [��� ������� �� ��� ����]

    // ������ ��������, ���������� �� ������ ��� ������� ��� ����, ����� � ������ ������������
    int rows   = (table_end-table_start)/N;
    int useful = rows - useless;  // ���������� �������� ����� �������
    double skipBits = logb(mymax(table_start-bufstart,1));  // ������� ��� ������� ��������� �� ����������� ���� skip
    stat_only ((slow_checks_by[N]++, verbose>1 && printf ("Slow check  %08x-%08x (%d*%d+%d)", int(table_start-buf+offset), int(table_end-buf+offset), N, useful, useless)));
    if (useful*sqrt((double)N) > 30+4*skipBits) {
        stat_only ((table_count_by[N]++,  table_sumlen_by[N]+=rows, table_skipBits+=skipBits));
        stat_only (verbose==1 && printf("%08x-%08x (%d*%d+%d)", int(table_start-buf+offset), int(table_end-buf+offset), N, useful, useless));
        stat_only (verbose >1 && printf(" ++++++ "));
        profiling(3,return TRUE);

        // ���������� ����� ������� ����� �������, � ����� �������� ��������������.
        // ������� ���������� � ������� ������������� ������� � ������ ������� (��� �������� ������ lz77)
        bool doDiff[MAX_ELEMENT_SIZE], immutable[MAX_ELEMENT_SIZE];
        analyze_table (N, table_start, rows, doDiff, immutable);    stat_only (if(print_tab) {iterate_var(i,rows) {if(i%20==0) {printf("\n"); iterate_var(j,N) printf(immutable[j]? "== ":"   ");}  iterate_var(j,N) printf("%s%02x", j==0?"\n": " ", table_start[i*N+j]);} printf("\n");});
        diff_table    (N, table_start, rows, doDiff);
        reorder_table (N, table_start, rows, immutable, ReorderingBuffer);
        type = encode_type (N, doDiff, immutable);
        stat_only (verbose>0 && printf("\n"));
        return TRUE;
    }

    stat_only (verbose>1 && printf("\n"));
    return FALSE;
}


// MAIN ALGORITHM *********************************************************************************

#define FAST_CHECK(N,p)                                                                         \
       (uint(p[    1] - p[  N+1] + DELTA) <= 2*DELTA                                            \
    &&  uint(p[  N+1] - p[2*N+1] + DELTA) <= 2*DELTA                                            \
    &&  uint(p[2*N+1] - p[3*N+1] + DELTA) <= 2*DELTA                                            \
    &&  *(int16*)(p) + *(int16*)(p+N)  !=  *(int16*)(p+2*N) + *(int16*)(p+3*N))                 \

// Check for data table at p with N-byte elements
#define FAST_CHECK_FOR_DATA_TABLE(N,p)                                                          \
{                                                                                               \
    /* Make a quick-and-dirty check and if it's successful - call the slow check */             \
    if (FAST_CHECK(N,p))                                                                        \
    {                                                                                           \
        BYTE *table_start, *table_end;  uint32 type;                                            \
        if (slow_check_for_data_table (N, p, type, table_start, table_end, last_table_end, bufend, buf, ReorderingBuffer)) {  \
            encode_table (table_start-last_table_end, type, (table_end-table_start)/N);         \
            last_table_end = table_end;                                                         \
            ptr = last_table_end - LINE;                                                        \
            cycle = 0;                                                                          \
            goto found;                                                                         \
        }                                                                                       \
    }                                                                                           \
}

#define encode_table(skip, type, rows)   \
{                                        \
    TSkip.put32 (skip);                  \
    TType.put32 (type);                  \
    TRows.put32 (rows);                  \
}


// Process data in the buf[] and save descriptors of found tables in Buffers
void delta_process (BYTE *buf, int Size, Buffer &TSkip, Buffer &TType, Buffer &TRows, Buffer &ReorderingBuffer)
{
    BYTE *bufend = buf + Size;     // End of data in buf[]
    BYTE *last_table_end = buf;    // End of last table found so far
    BYTE *hash[256], *hash1[256];
    iterate_var(i,256)  hash[i] = hash1[i] = buf-1;
    BYTE count[256], count1[256];  zeroArray(count);  zeroArray(count1);
    unsigned cycle=0;              // on every 4th cycle we are checking all MAX_ELEMENT_SIZE counts, on other cycles - only counts[1..4]

    for (BYTE *ptr=buf+MAX_ELEMENT_SIZE;  ptr+LINE < bufend;  ptr+=LINE)
    {
        if (*(int32*)ptr != *(int32*)(ptr+3))   //  a little speed optimization, mainly to skip blocks of all zeroes
        {
            // ��������� ���������� ���������� ���������� ��� ������� ���� �� ������ ����������
            iterate_var(i, (cycle==0?MAX_ELEMENT_SIZE:SMALL_ELEMENT_SIZE))  count1[i+1]=count[i+1], count[i+1]=0;
            for (BYTE *p=ptr; p<ptr+LINE; p++)
            {
                int n = p - hash[*p/16];   // detecting repeated data by 4 higher bits
                hash[*p/16] = p;
                count[n&255]++;            // since MAX_ELEMENT_SIZE << 256, errors should be rare
#if 0
                // Detecting repeated data by all 8 bits - useful for tables with longer rows
                int n1 = p - hash1[*p];
                hash1[*p] = p;
                if (n!=n1)  count[n1&255]++;
#endif
            }
            cycle = (cycle+1) % FAST_CYCLES;  profiling(1,continue);

            // ������ ������ �� ���������, �� ������� ���� ��� ������� 24/12 ���������� - ��� ��������� �� ������ ������ �������
            iterate_var(i, (cycle==0?MAX_ELEMENT_SIZE:SMALL_ELEMENT_SIZE))
            {
                if (count[i+1]+count1[i+1]  >=  (i>=SMALL_ELEMENT_SIZE?24:12))
                {
                    int N = i+1;
                    BYTE *p = ptr - (i>=SMALL_ELEMENT_SIZE? (FAST_CYCLES-1)*LINE : 0);
                    stat_only ((fast_checks_by[N]++, verbose>1 && printf ("Fast check  %08x (%d*%d)\n", int(p-buf+offset), N, count[N])));
                    for (int j=0; j<N; j++, p++)  FAST_CHECK_FOR_DATA_TABLE(N,p);
                }
            }
        }
found:  ;
    }
}


int delta_compress (MemSize BlockSize, int ExtendedTables, CALLBACK_FUNC *callback, void *auxdata)
{
    int errcode = FREEARC_OK;
    BYTE *buf = (BYTE*) BigAlloc(BlockSize+MAX_ELEMENT_SIZE*8+1);   // Buffer for one block of input data (typically, 8mb long)
    if (buf==NULL)  return FREEARC_ERRCODE_NOT_ENOUGH_MEMORY;       // Error: not enough memory
    Buffer TSkip, TType, TRows;                                     // Buffers for storing info about each table filtered
    Buffer ReorderingBuffer;                                        // Buffer used in reorder_table
    FOPEN();
#ifdef STAT
    for(int N=1; N<=MAX_ELEMENT_SIZE; N++)  fast_checks_by[N] = slow_checks_by[N] = slow_iterations_by[N] = backward_slow_iterations_by[N] = table_count_by[N] = table_sumlen_by[N] = 0;
#endif

    // Each iteration of this cycle reads, process and encodes one block of data
    for (;;)
    {
        // Read input block
        int Size;  READ_LEN_OR_EOF (Size, buf, BlockSize);

        // Process data block
        delta_process (buf, Size, TSkip, TType, TRows, ReorderingBuffer);

        // Write output block
        QUASIWRITE (sizeof(int32)*2 + TType.len()*3 + Size);
        FWRITE4 (Size);                      // output the input block size
        FWRITE4 (TType.len());               // output the buffer size
        FWRITE (TSkip.buf, TSkip.len());     // output the TSkip buffer contents
        FWRITE (TType.buf, TType.len());     // ..
        FWRITE (TRows.buf, TRows.len());     // ..
        TSkip.empty(), TType.empty(), TRows.empty();
        FWRITE (buf, Size);                  // output the preprocessed data
        stat_only (offset += Size);
        FFLUSH();
    }

 finished:
#ifdef STAT
    for(int N=1; N<=MAX_ELEMENT_SIZE; N++)  fast_checks += fast_checks_by[N],  fast_iterations += fast_checks_by[N]*N,
                                            slow_checks += slow_checks_by[N],  slow_iterations += slow_iterations_by[N],  backward_slow_iterations += backward_slow_iterations_by[N],
                                            table_count += table_count_by[N],  table_sumlen += table_sumlen_by[N]*N;

    printf("\rFast %s*%.3lf=%s.", show3(fast_checks,fast_checks_str), double(fast_iterations)/double(fast_checks), show3(fast_iterations,fast_iterations_str));
    printf("  Slow %s*%.2lf=%s", show3(slow_checks,slow_checks_str), double(slow_iterations)/double(slow_checks), show3(slow_iterations,slow_iterations_str));
    printf("  incl. back %s*%.2lf=%s", show3(slow_checks,slow_checks_str), double(backward_slow_iterations)/double(slow_checks), show3(backward_slow_iterations,slow_iterations_str));
    printf("\nTables %s * %.0lf (%.0lf) = %s (%s) bytes.  %.3lf skipbits\n  fast", show3(table_count,table_count_str), double(table_sumlen/mymax(table_count,1)), double(table_diffed/mymax(table_count,1)), show3(table_sumlen,table_sumlen_str), show3(table_diffed,table_diffed_str), double(table_skipBits/mymax(table_count,1)));

    for(int N=1; N<=MAX_ELEMENT_SIZE; N++)
        if (N<=SMALL_ELEMENT_SIZE || N==8)
            printf("  %s*%d", show3(fast_checks_by[N],fast_checks_str), N),  fast_checks -= fast_checks_by[N],  fast_iterations -= fast_checks_by[N]*N;
            printf("  %s*%.3lf=%s\n  slow", show3(fast_checks,fast_checks_str), double(fast_iterations)/double(mymax(fast_checks,1)), show3(fast_iterations,fast_iterations_str));

    for(int N=1; N<=MAX_ELEMENT_SIZE; N++)
        if (N<=SMALL_ELEMENT_SIZE || N==8)
            printf("  %s*%.2lf=%s", show3(slow_checks_by[N],slow_checks_str), double(slow_iterations_by[N])/double(mymax(slow_checks_by[N],1)), show3(slow_iterations_by[N],slow_iterations_str)),  slow_checks -= slow_checks_by[N],  slow_iterations -= slow_iterations_by[N];
            printf("  %s*%.2lf=%s\n  tables", show3(slow_checks,slow_checks_str), double(slow_iterations)/double(mymax(slow_checks,1)), show3(slow_iterations,slow_iterations_str));

    for(int N=1; N<=MAX_ELEMENT_SIZE; N++)
        if (N<=SMALL_ELEMENT_SIZE || N==8)
            printf("  %s*%.0lf=%s", show3(table_count_by[N],table_count_str), double(table_sumlen_by[N]*N)/mymax(table_count_by[N],1), show3(table_sumlen_by[N]*N,table_sumlen_str)),  table_count -= table_count_by[N],  table_sumlen -= table_sumlen_by[N]*N;
            printf("  %s*%.0lf=%s\n", show3(table_count,table_count_str), double(table_sumlen)/mymax(table_count,1), show3(table_sumlen,table_sumlen_str));
#endif
    FCLOSE(); BigFree(buf); return errcode;
}
#endif


// Decompression which undiffs all data tables which was diffed by table_compress()
int delta_decompress (MemSize BlockSize, int ExtendedTables, CALLBACK_FUNC *callback, void *auxdata)
{
    int errcode = FREEARC_OK;          // Error code returned by last operation or FREEARC_OK
    uint64 offset = 0;                 // Current offset of buf[] contents relative to file (increased after each input block processd)
    Buffer Data, TSkip, TType, TRows,
           ReorderingBuffer;           // Buffer used in reorder_table

    // ����, ������ �������� �������� ������������ ���� ���� ������ ������
    for (;;)
    {
        // ��������� ���� ���� ������ � �������� ����������� � ��� ������
        int DataSize;              READ4_OR_EOF(DataSize);       // Size of data block
        int TableSize;             READ4(TableSize);             // Size of each table describing data tables
        TSkip.reserve(TableSize);  READ (TSkip.buf, TableSize);  // Read table descriptions (see below)
        TType.reserve(TableSize);  READ (TType.buf, TableSize);
        TRows.reserve(TableSize);  READ (TRows.buf, TableSize);
        Data .reserve(DataSize);   READ (Data.buf,  DataSize);   // Finally, read block contents itself

        // Undiff all data tables in this block
        BYTE *p = Data.buf;
        for (int i=TableSize/sizeof(int32); i; i--)
        {
            int skip = TSkip.get32();   // How many bytes to skip after previous data table
            int type = TType.get32();   // Type of data table (actually, just number of bytes in each element)
            int rows = TRows.get32();   // Number of rows in table

            int N; bool doDiff[MAX_ELEMENT_SIZE], immutable[MAX_ELEMENT_SIZE];
            decode_type (type, N, doDiff, immutable);
            p += skip;
            stat_only (verbose>0 && printf("%08x-%08x %d*%d\n", int(p-Data.buf+offset), int(p-Data.buf+N*rows+offset), N, rows));
            unreorder_table (N, p, rows, immutable, ReorderingBuffer);
            undiff_table    (N, p, rows, doDiff);
            p += N*rows;
        }
        TSkip.empty(), TType.empty(), TRows.empty();

        // And finally write undiffed data
        WRITE (Data.buf, DataSize);  Data.empty();
        offset += DataSize;
    }
finished:
    return errcode;
}


// FUNCTIONS FOR STANDALONE EXECUTABLE ************************************************************

#ifndef DELTA_LIBRARY
#include "../Common.cpp"

// Structure for recording compression statistics and zero record of this type
struct Results {
  char *msg;                 // Mode: compression/decompression
  FILE *fin, *fout;          // Input and output files
  uint64 filesize;           // Size of input file
  uint64 insize, outsize;    // How many bytes was already read/written
  double time;               // How many time was spent in (de)compression routines
} r0;

int ReadWriteCallback (const char *what, void *buf, int size, void *r_)
{
  Results &r = *(Results*)r_;        // Accumulator for compression statistics

  if (strequ(what,"init")) {
    r.filesize = get_flen(r.fin);
    r.time -= GetGlobalTime();
    return FREEARC_OK;

  } else if (strequ(what,"read")) {
    r.time += GetGlobalTime();
    int n = file_read (r.fin, buf, size);
    r.insize += n;
    r.time -= GetGlobalTime();
    return n;

  } else if (strequ(what,"write")) {
    r.time += GetGlobalTime();
    if (r.fout)  file_write (r.fout, buf, size);
    r.outsize += size;
    if (!verbose)
    {
      char percents[10] = "";
      if (r.filesize)    sprintf (percents, "%2d%%: ", int(double(r.insize)*100/r.filesize));
      double insizeMB = double(r.insize)/1000/1000;
      if (r.time > 0.01)  printf( "\r%sprocessed %.0lf mb, %.3lf seconds, speed %.3lf mb/sec",
                                    percents, insizeMB, r.time, insizeMB/r.time);
      //    4096.00 KiB ->     1230.92 KiB (ratio  30.05%, speed  299 KiB/s)
    }
    r.time -= GetGlobalTime();
    return size;

  } else if (strequ(what,"done")) {
    r.time += GetGlobalTime();
    if (!verbose)
    {
      double insizeMB = double(r.insize)/1000/1000;
      if (r.time > 0.01)  printf( "\r%s: %.0lf mb, %.3lf seconds, speed %.3lf mb/sec     ",
                                    r.msg, insizeMB, r.time, insizeMB/r.time);
    }
    return FREEARC_OK;

  } else {
    return FREEARC_ERRCODE_NOT_IMPLEMENTED;
  }
}

// ������ ��������� ������ � ����� delta_compress/delta_decompress � ���������������� �����������
int main (int argc, char **argv)
{
    // ���������� ������ ��������?
    int unpack = 0;

    int BlockSize=8*mb, ExtendedTables=0;

    while (argv[1] && argv[1][0] == '-') {
        switch( tolower(argv[1][1]) ) {
            case 'v':   verbose++;                           break;
            case 'p':   print_tab++;                         break;
            case 't':   print_timings++;                     break;
            case 'd':   unpack++;                            break;
            case 'x':   ExtendedTables++;                    break;
            case 'b':   BlockSize = atoi(argv[1]+2)*(1<<20); break;
            default :   printf( "\n Unknown option '%s'\n", argv[1]);
                        exit(1);
        }
        argv++, argc--;
    }

    // ����� �����, � ��������� ������ ������ ���� ����� 1 ��� 2 ���������
    // (������� � ����������� �������� �����)
    if (argc != 2  &&  argc != 3) {
        printf( COPYRIGHT);
        printf( "\n" );
        printf( "\n Usage: delta [options] original-file [packed-file]");
        printf( "\n   -bN --  process data in N mb blocks");
        //printf( "\n   -x  --  enable extended tables (with 32..64-byte elements)");
        printf( "\n   -v  --  increment verbosity level (0 - default, 3 - maximum, requires compilation with -DSTAT)");
        printf( "\n   -p  --  print contents of found tables (requires compilation with -DSTAT)");
        printf( "\n" );
        printf( "\n For decompress: delta -d [-v] packed-file [unpacked-file]");
        printf( "\n" );
        exit(2);
    }

    Results r = r0;

    // ������� ������� ����
    r.fin = fopen (argv[1], "rb");
    if (r.fin == NULL) {
        printf( "Can't open %s for read\n", argv[1]);
        exit(3);
    }

    // ������� �������� ����, ���� �� ����� � ��������� ������
    if (argc == 3) {
        r.fout = fopen (argv[2], "wb");
        if (r.fout == NULL) {
            printf( "Can't open %s for write\n", argv[2]);
            exit(4);
        }
    } else {
        r.fout = NULL;
    }

    // (De)compress
    ReadWriteCallback ("init", NULL, 0, (void*)&r);
    if (!unpack) {
        r.msg = "Compression";
        delta_compress   (BlockSize, ExtendedTables, ReadWriteCallback, &r);
    } else {
        r.msg = "Decompression";
        delta_decompress (BlockSize, ExtendedTables, ReadWriteCallback, &r);
    }
    ReadWriteCallback ("done", NULL, 0, (void*)&r);

    fclose(r.fin);  if (r.fout)  fclose(r.fout);
    return 0;
}

#endif
