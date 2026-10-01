// Common definitions, used by various parts of FreeArc project
#ifndef FREEARC_COMMON_H
#define FREEARC_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <time.h>
#include <setjmp.h>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef FREEARC_WIN
#include <windows.h>
#include <direct.h>
#include <float.h>
#define strcasecmp stricmp
#ifndef __GNUC__
#define logb       _logb
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************
** ������� ����������� FREEARC ************************************************
******************************************************************************/
#if !defined(FREEARC_WIN) && !defined(FREEARC_UNIX)
#error "You must define OS!"
#endif

#if defined(FREEARC_INTEL_BYTE_ORDER)
#if _BIG_ENDIAN
#error "You're compiling for Motorola byte order, but FREEARC_INTEL_BYTE_ORDER was defined."
#endif
#elif defined(FREEARC_MOTOROLA_BYTE_ORDER)
#if _M_IX86 || __i386 || __x86_64
#error "You're compiling for Intel byte order, but FREEARC_MOTOROLA_BYTE_ORDER was defined."
#endif
#else
#error "You must define byte order!"
#endif

#ifdef FREEARC_WIN
#define UNSUPPORTED_PATH_DELIMITERS   "/"
#define PATH_DELIMITER                '\\'
#define STR_PATH_DELIMITER            "\\"
#else
#define UNSUPPORTED_PATH_DELIMITERS   ":\\"
#define PATH_DELIMITER                '/'
#define STR_PATH_DELIMITER            "/"
#endif

#define  DIRECTORY_DELIMITERS         "/\\"
#define  ALL_PATH_DELIMITERS          ":/\\"


/******************************************************************************
** �������� ��� ������� �����, ������������ � ��������� ***********************
******************************************************************************/
typedef unsigned long        ulong;
typedef unsigned int         uint,   UINT;
typedef unsigned short int   ushort;
typedef unsigned char        uchar;
#ifdef __GNUC__
#include <stdint.h>
typedef          uint64_t    uint64;
typedef          uint32_t    uint32;
typedef          uint16_t    uint16;
typedef          uint8_t     uint8,  byte, BYTE;
typedef          int64_t     sint64, int64;
typedef          int32_t     sint32, int32;
typedef          int16_t     sint16, int16;
typedef          int8_t      sint8,  int8;
#else
typedef          __int64     sint64, int64;
typedef unsigned __int64     uint64;
typedef          __int32     sint32, int32;
typedef unsigned __int32     uint32;
typedef          __int16     sint16, int16;
typedef unsigned __int16     uint16;
typedef          __int8      sint8,  int8;
typedef unsigned __int8      uint8,  byte, BYTE;
#endif

typedef unsigned             MemSize;          // ����� ������
typedef char*                FILENAME;         // ��� �����
#ifdef FREEARC_WIN
typedef int64                FILESIZE;         // ������ �����
#else
typedef off_t                FILESIZE;
#endif


/******************************************************************************
** ����������� ����������� ****************************************************
******************************************************************************/
#define make4byte(a,b,c,d)       ((a)+256*((b)+256*((c)+256*(((uint32)d)))))
#define iterate(num, statement)  {for( int i=0; i<(num); i++) {statement;}}
#define iterate_var(i, num)      for( int i=0; i<(num); i++)
#define iterate_array(i, array)  for( int i=0; i<(array).size; i++)
#ifndef TRUE
#define TRUE                     1
#endif
#ifndef FALSE
#define FALSE                    0
#endif

#define PATH_CHARS               ":/\\"
#define is_path_char(c)          in_set(c, PATH_CHARS)
#define in_set(c, set)           (strchr (set, c ) != NULL)
#define in_set0(c, set)          (memchr (set, c, sizeof(set) ) != 0)
#define str_end(str)             (strchr (str,'\0'))
#define last_char(str)           (str_end(str) [-1])
#define strequ(a,b)              (strcmp((a),(b))==EQUAL)
#define namecmp                  strcasecmp
#define nameequ(s1,s2)           (namecmp(s1,s2)==EQUAL)
#define start_with(str,with)     (strncmp (str, with, strlen(with))==EQUAL)
#define end_with(str,with)       (nameequ (str_end(str)-strlen(with), with))
#define find_extension(str)      (find_extension_in_entry (drop_dirname(str)))
#define mymax(a,b)               ((a)>(b)? (a) : (b))
#define mymin(a,b)               ((a)<(b)? (a) : (b))
#define inrange(x,a,b)           ((a)<=(x) && (x)<(b))
#define elements(arr)            (sizeof(arr)/sizeof(*arr))
#define endof(arr)               ((arr)+elements(arr))
#define zeroArray(arr)           (memset (arr, 0, sizeof(arr)))
#define EQUAL                    0   /* result of strcmp/memcmp for equal strings */

// Skip directory in filename
static inline char *drop_dirname (char *filename)
{
  char *p;
  for (p = &last_char(filename); p >= filename; p--)
  {
    if (is_path_char(*p))
      return p+1;
  }
  return filename;
}


// ****************************************************************************
// FILE OPERATIONS ************************************************************
// ****************************************************************************

#define MY_FILENAME_MAX 65536                  /* maximum length of filename */

#ifdef FREEARC_WIN

#include <io.h>
#include <fcntl.h>
#include <tchar.h>
typedef TCHAR* CFILENAME;
static inline int create_dir (CFILENAME name)   {return _tmkdir(name);}
#define set_flen(stream,new_size)               (chsize( file_no(stream), new_size ))
#define get_flen(stream)                        (_filelengthi64(fileno(stream)))
#define myeof(file)                             (feof(file))
#define get_ftime(stream,tstamp)                getftime( file_no(stream), (struct ftime *) &tstamp )
#define set_ftime(stream,tstamp)                setftime( file_no(stream), (struct ftime *) &tstamp )
#define set_binary_mode(file)                   setmode(fileno(file),O_BINARY)

// Number of 100 nanosecond units from 01.01.1601 to 01.01.1970
#define EPOCH_BIAS    116444736000000000ull

static inline void WINAPI UnixTimeToFileTime( time_t time, FILETIME* ft )
{
  *(uint64*)ft = EPOCH_BIAS + time * 10000000ull;
}


#endif // FREEARC_WIN


#ifdef FREEARC_UNIX

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <utime.h>

#define __cdecl
#define SSIZE_T ssize_t

typedef char* CFILENAME;
typedef char  TCHAR;
#define _T
#define _tcscmp         strcmp
#define _tcschr         strchr
#define	_tcsrchr	strrchr
#define _tcscpy         strcpy
#define _stprintf       sprintf
#define _tcslen         strlen
#define _tstat          stat
#define _stat           stat
#define _trmdir         rmdir
#define _trename        rename
#define _tremove        remove
#define _taccess        access

typedef int (*FARPROC) (void);

static inline int create_dir (CFILENAME name)   {return mkdir(name,0777);}
#define get_flen(stream)                        (myfilelength( fileno (stream)))
#define set_binary_mode(file)
#define myeof(file)                             (get_flen(file) == ftell(file))
static inline off_t myfilelength (int h)
{
  off_t saved = lseek (h, 0, SEEK_CUR);
  off_t size  = lseek (h, 0, SEEK_END);
  lseek (h, saved, SEEK_SET);
  return size<0? -1 : size;
}

#endif // FREEARC_UNIX

#define file_read(file, buf, size)              fread  (buf, 1, size, file)
#define file_write(file, buf, size)             fwrite (buf, 1, size, file)

static inline int remove_dir  (CFILENAME name)  {return _trmdir(name);}
static inline int remove_file (CFILENAME name)  {return _tremove(name);}
static inline int file_exists (CFILENAME name)  {return _taccess(name,0) == 0;}

static inline int rename_file (CFILENAME oldname, CFILENAME newname)  {return _trename(oldname,newname);}

static inline int dir_exists (const TCHAR *name)
{
  struct _stat st;
  _tstat(name,&st);
  return (st.st_mode & S_IFDIR) != 0;
}

void BuildPathTo(CFILENAME name);  // ������� �������� �� ���� � name
void SetFileDateTime (const CFILENAME Filename, time_t t); // ���������� �����/���� ����������� �����
void RunProgram (const CFILENAME filename, const CFILENAME curdir, int wait_finish);  // Execute program `filename` in the directory `curdir` optionally waiting until it finished
int  RunCommand (const CFILENAME command,  const CFILENAME curdir, int wait_finish);  // Execute `command` in the directory `curdir` optionally waiting until it finished
void RunFile    (const CFILENAME filename, const CFILENAME curdir, int wait_finish);  // Execute file `filename` in the directory `curdir` optionally waiting until it finished
void SetTempDir (const CFILENAME dir);     // Set temporary files directory
CFILENAME GetTempDir (void);               // Return last value set or GetTempPath (%TEMP)


// ****************************************************************************
// ****************************************************************************
// ****************************************************************************

#ifdef FREEARC_INTEL_BYTE_ORDER

// Read unsigned 16/24/32-bit value at given address
#define value16(p)               (*(uint16*)(p))
#define value24(p)               (*(uint32*)(p) & 0xffffff)
#define value32(p)               (*(uint32*)(p))
#define value64(p)               (*(uint64*)(p))
// Write unsigned 16/24/32-bit value to given address
#define setvalue16(p,x)          (*(uint16*)(p) = (x))
#define setvalue24(p,x)          (*(uint32*)(p) = ((x) & 0xffffff) + (*(uint32*)(p) & 0xff000000))
#define setvalue32(p,x)          (*(uint32*)(p) = (x))
#define setvalue64(p,x)          (*(uint64*)(p) = (x))

#elif FREEARC_MOTOROLA_BYTE_ORDER
// routines for non-little-endian cpus, written by Joachim Henke
#if _ARCH_PPC
#if __GNUC__ == 4 && __GNUC_MINOR__ > 0 || __GNUC__ > 4
#define PPC_MCONSTR "Z"
#else
#define PPC_MCONSTR "Q"
#endif
#define PPC_LBRX(s,p,x)   __asm__ ("l"  s "brx %0,%y1" : "=r" (x) : PPC_MCONSTR (*p))
#define PPC_STBRX(s,p,x)  __asm__ ("st" s "brx %1,%y0" : "=" PPC_MCONSTR (*p) : "r" (x))
#endif

static inline uint16 value16 (void *p)
{
  uint16 x;
#if _ARCH_PPC
  uint16 *m = (uint16 *)p;
  PPC_LBRX("h", m, x);
#else
  uint8 *m = (uint8 *)p;
  x = m[0] + (m[1] << 8);
#endif
  return x;
}

static inline uint32 value24 (void *p)
{
  uint32 x;
#if __GNUC__ == 4 && __GNUC_MINOR__ > 2 || __GNUC__ > 4
  uint32 *m = (uint32 *)p;
  x = __builtin_bswap32(*m) & 0xffffff;
#elif _ARCH_PPC
  uint32 *m = (uint32 *)p;
  PPC_LBRX("w", m, x);
  x &= 0xffffff;
#else
  uint8 *m = (uint8 *)p;
  x = m[0] + (m[1] << 8) + (m[2] << 16);
#endif
  return x;
}

static inline uint32 value32 (void *p)
{
  uint32 x;
#if __GNUC__ == 4 && __GNUC_MINOR__ > 2 || __GNUC__ > 4
  uint32 *m = (uint32 *)p;
  x = __builtin_bswap32(*m);
#elif _ARCH_PPC
  uint32 *m = (uint32 *)p;
  PPC_LBRX("w", m, x);
#else
  uint8 *m = (uint8 *)p;
  x = m[0] + (m[1] << 8) + (m[2] << 16) + (m[3] << 24);
#endif
  return x;
}

static inline uint64 value64 (void *p)
{
  uint64 x;
#if _ARCH_PPC && __PPU__
  uint64 *m = (uint64 *)p;
  PPC_LBRX("d", m, x);
#else
  uint32 *m = (uint32 *)p;
  x = value32(m) + ((uint64)value32(m + 1) << 32);
#endif
  return x;
}

static inline void setvalue16 (void *p, uint16 x)
{
#if _ARCH_PPC
  uint16 *m = (uint16 *)p;
  PPC_STBRX("h", m, x);
#else
  uint8 *m = (uint8 *)p;
  m[0] = x;
  m[1] = x >> 8;
#endif
}

static inline void setvalue24 (void *p, uint32 x)
{
  uint8 *m = (uint8 *)p;
  m[0] = x;
  m[1] = x >> 8;
  m[2] = x >> 16;
}

static inline void setvalue32 (void *p, uint32 x)
{
#if __GNUC__ == 4 && __GNUC_MINOR__ > 2 || __GNUC__ > 4
  uint32 *m = (uint32 *)p;
  *m = __builtin_bswap32(x);
#elif _ARCH_PPC
  uint32 *m = (uint32 *)p;
  PPC_STBRX("w", m, x);
#else
  uint8 *m = (uint8 *)p;
  m[0] = x;
  m[1] = x >> 8;
  m[2] = x >> 16;
  m[3] = x >> 24;
#endif
}

static inline void setvalue64 (void *p, uint64 x)
{
#if _ARCH_PPC && __PPU__
  uint64 *m = (uint64 *)p;
  PPC_STBRX("d", m, x);
#else
  uint32 *m = (uint32 *)p;
  setvalue32(m, x);
  setvalue32(m + 1, x >> 32);
#endif
}

#endif //FREEARC_MOTOROLA_BYTE_ORDER

// Check for equality
#define val16equ(p,q)             (*(uint16*)(p) == *(uint16*)(q))
#define val24equ(p,q)             (   value24(p) ==    value24(q))
#define val32equ(p,q)             (*(uint32*)(p) == *(uint32*)(q))
#define val64equ(p,q)             (*(uint64*)(p) == *(uint64*)(q))

// Free memory block and set pointer to NULL
#ifndef FreeAndNil
#define FreeAndNil(p)            ((p) && (free(p),    (p)=NULL))
#define BigFreeAndNil(p)         ((p) && (BigFree(p), (p)=NULL))
#endif

// Exit code used to indicate serious problems in FreeArc utilities
#define FREEARC_EXIT_ERROR 2

// ����������, ������������ ��� ������������ �� ������� �� ������� �������� ��������
extern int jmpready;
extern jmp_buf jumper;

// ��������� ��������� � ����������� ��������� ���������
#ifndef CHECK
#  if defined(FREEARC_WIN) && defined(FREEARC_GUI)
#    define CHECK(a,b)           {if (!(a))  {if (jmpready) longjmp(jumper,1);  char *s=(char*)malloc(MY_FILENAME_MAX*4);  WCHAR *utf16=(WCHAR*) malloc(MY_FILENAME_MAX*4);  sprintf b;  utf8_to_utf16(s,utf16);  MessageBoxW(NULL, utf16, L"Error encountered", MB_ICONERROR);  ON_CHECK_FAIL();  exit(FREEARC_EXIT_ERROR);}}
#  elif defined(FREEARC_WIN)
#    define CHECK(a,b)           {if (!(a))  {if (jmpready) longjmp(jumper,1);  char *s=(char*)malloc(MY_FILENAME_MAX*4),  *oem=(char*)malloc(MY_FILENAME_MAX*4);  sprintf b;  utf8_to_oem(s,oem);  printf("\n%s",oem);  ON_CHECK_FAIL();  exit(FREEARC_EXIT_ERROR);}}
#  else
#    define CHECK(a,b)           {if (!(a))  {if (jmpready) longjmp(jumper,1);  char s[MY_FILENAME_MAX*4];  sprintf b;  printf("\n%s",s);  ON_CHECK_FAIL();  exit(FREEARC_EXIT_ERROR);}}
#  endif
#endif

#ifndef ON_CHECK_FAIL
#define ON_CHECK_FAIL()
#endif

// ������������� Jump Point � ����� �������� retcode
#define SET_JMP_POINT(retcode)                                                         \
{                                                                                      \
  if (!jmpready && setjmp(jumper) != 0)                                                \
    /* ���� �� ������ ��� ������������� ������ � ����� �� ���������� �������� */      \
    {jmpready = FALSE; return retcode;}                                                \
  jmpready = TRUE;                                                                     \
}

// ������� Jump Point
#define RESET_JMP_POINT()                                                              \
{                                                                                      \
  jmpready = FALSE;                                                                    \
}


// Include statements marked as debug(..)  only if we enabled debugging
#ifdef DEBUG
#define debug(stmt)  stmt
#else
#define debug(stmt)  ((void)0)
#endif

// Include statements marked as stat(..)  only if we enabled gathering stats
#ifdef STAT
#define stat(stmt)  stmt
#else
#define stat(stmt)  ((void)0)
#endif

// Define default parameter value only when compiled as C++
#ifdef __cplusplus
#define DEFAULT(x,n) x=n
#else
#define DEFAULT(x,n) x
#endif

#define then


// ****************************************************************************
// MEMORY ALLOCATION **********************************************************
// ****************************************************************************
#ifndef __cplusplus
#define throw()
#endif

#ifdef FREEARC_STANDALONE_TORNADO
#define MidAlloc(size) malloc(size)
#define MidFree(address) free(address)
#define BigAlloc(size) malloc(size)
#define BigFree(address) free(address)
#else
void *MyAlloc(size_t size) throw();
void MyFree(void *address) throw();
#ifdef FREEARC_WIN
int SetLargePageSize();
void *MidAlloc(size_t size) throw();
void MidFree(void *address) throw();
void *BigAlloc(size_t size) throw();
void BigFree(void *address) throw();
#else
#define MidAlloc(size) MyAlloc(size)
#define MidFree(address) MyFree(address)
#define BigAlloc(size) MyAlloc(size)
#define BigFree(address) MyFree(address)
#endif


// ****************************************************************************
// ������� �������� � ���������� **********************************************
// ****************************************************************************
void strncopy (char *to, char *from, int len);   // �������� ������� from � to, �� �� ����� len ��������
int  split (char *str, char splitter, char **result, int result_size);  // ������� ������ str �� ���������, ���������� �������� splitter
char*subst (char *original, char *from, char *to);  // �������� � ������ original ��� ��������� from �� to
char*trim_spaces (char *s);                      // ���������� ������� � ������ ������ � ������� �� � �����, ����������� ������
char *str_replace_n (char *orig, char *from, int how_many, char *to);   // Replace from:how_many substring and put result in new allocated area
char *str_replace   (char *orig, char *from, char *to);    // Replace substring and put result in new allocated area
#endif // !FREEARC_STANDALONE_TORNADO
MemSize parseInt (char *param, int *error);  // If the string param contains an integer, return it - otherwise set error=1
MemSize parseMem (char *param, int *error);  // Similarly, but the string param can contain a suffix b/k/m/g/^, representing units of memory, or in the case of '^' (default), the relevant power of 2
void showMem (MemSize mem, char *result);    // Returns a string with the amount of memory


// Round first number *down* to divisible by second one
static inline MemSize roundDown (MemSize a, MemSize b)
{
  return b>1? a/b*b : a;
}

// Round first number *up* to divisible by second one
static inline MemSize roundUp (MemSize a, MemSize b)
{
  return b>1? roundDown(a-1,b)+b : a;
}


// Whole part of number's binary logarithm (logb) - please ensure that n > 0
static inline MemSize lb (MemSize n)
{
    MemSize result;
#if __INTEL_COMPILER
    result = _bit_scan_reverse(n);
#elif _MSC_VER >= 1400
    _BitScanReverse((DWORD *)&result, n);
#elif __GNUC__ == 3 && __GNUC_MINOR__ > 3 || __GNUC__ > 3
    result = __builtin_clz(n) ^ (8 * sizeof(unsigned int) - 1);
#else
    result = 0;
    if (n > 0xffff) result = 16, n >>= 16;
    if (n > 0xff)   result += 8, n >>= 8;
    if (n > 0xf)    result += 4, n >>= 4;
    if (n > 0x3)    result += 2, n >>= 2;
    if (n > 0x1)    result += 1;
#endif
    return result;
}

// ��� ��������� ��������� ����� � ��������� ������ �������
// ����, �������� f(13,2)=16
static inline MemSize roundup_to_power_of (MemSize n, MemSize base)
{
    MemSize result = base;
    if (!(--n))
        return 1;
    if (base == 2)
        result <<= lb(n);
    else
        while (n /= base)
            result *= base;
    return result;
}

// ��� ��������� ��������� ����� � ��������� ����� �������
// ����, �������� f(13,2)=8
static inline MemSize rounddown_to_power_of (MemSize n, MemSize base)
{
    MemSize result = 1;
    if (!n)
        return 1;
    if (base == 2)
        result <<= lb(n);
    else
        while (n /= base)
            result *= base;
    return result;
}

// ��� ��������� ��������� ����� � �������������� ��������� �������
// ����, �������� f(9,2)=8  f(15,2)=16
static inline MemSize round_to_nearest_power_of (MemSize n, MemSize base)
{
    MemSize result;
    uint64 nn = ((uint64)n)*n/base;
    if (nn==0)  return 1;
    for (result=base; (nn/=base*base) != 0; result *= base);
    return result;
}

// ���������� ����� � ������, ���������� �������: "1.234.567"
static inline char* show3 (uint64 n, char *buf)
{
    char *p = buf + 27;
    int i = 4;

    *p = '\0';
    do {
        if (!--i) *--p = '.', i = 3;
        *--p = '0' + (n % 10);
    } while (n /= 10);

    return p;
}

// �������� ������� �� ��������� from �� ������ to
static inline char *replace (char *str, char* from, char to)
{
  char *p;
  for (p=str; *p; p++)
    if (in_set (*p, from))
      *p = to;
  return str;
}

// ��������� �������� �������� �������, ���������������� ��� ����������������� �����
static inline int char2int(char c) {return isdigit(c)? c-'0' : tolower(c)-'a'+10;}

#ifdef FREEARC_WIN
// Windows charset conversion routines
WCHAR *utf8_to_utf16 (const char  *utf8,  WCHAR *utf16);  // Converts UTF-8 string to UTF-16
char  *utf16_to_utf8 (const WCHAR *utf16, char  *utf8);   // Converts UTF-16 string to UTF-8
char  *utf8_to_oem   (const char  *utf8,  char  *oem);    // Converts UTF-8 string to OEM
char  *oem_to_utf8   (const char  *oem,   char  *utf8);   // Converts OEM string to UTF-8
#endif

#ifndef FREEARC_NO_TIMING
// ����� ��������� ����
void EnvSetConsoleTitle (CFILENAME title);  // ���������� ��������� ����������� ����
void EnvResetConsoleTitle (void);  // ������������ ���������, ������� ��� � ������ ������ ���������

// Timing execution
double GetGlobalTime     (void);   // Returns number of wall-clock seconds since some moment
double GetThreadCPUTime  (void);   // Returns number of seconds spent in this thread

// Time-based random number
unsigned time_based_random(void);
#endif

#ifdef __cplusplus

// Register/unregister temporary files that should be deleted on ^Break
class MYFILE;
void registerTemporaryFile   (MYFILE &file);
void unregisterTemporaryFile (MYFILE &file);
void removeTemporaryFiles    (void);

// Checked malloc
static inline void *malloc_msg (unsigned long size = MY_FILENAME_MAX * 4)
{
  void *ptr = malloc(size);
  CHECK (ptr, (s,"ERROR: can't alloc %lu memory bytes", size));
  return ptr;
}

// Checked strdup
static inline char *strdup_msg (char *old)
{
  char *str = (char*) malloc_msg (strlen(old)+1);
  strcpy (str, old);
  return str;
}

}       // extern "C"


/******************************************************************************
** �����, �������������� ������ � ������� *************************************
******************************************************************************/

enum MODE {READ_MODE, WRITE_MODE}; // ����� �������� �����
struct MYFILE
{
  // Mark file as temporary, removed automatically by destructor
  bool is_temp;
  void mark_as_temporary()           {registerTemporaryFile(*this); is_temp = TRUE;}

  int handle;
  TCHAR *filename;
  char *utf8name, *utf8lastname, *oemname;

  void SetBaseDir (char *utf8dir)    // Set base dir
  {
    if (utf8dir != utf8name)
      strcpy (utf8name, utf8dir);
    if (utf8name[0] != '\0' && !is_path_char (last_char(utf8name)))
      strcat (utf8name, STR_PATH_DELIMITER);
    utf8lastname = str_end(utf8name);
  }

#ifdef FREEARC_WIN
#  ifdef FREEARC_GUI                 // Win32 GUI *****************************************
  void setname (const FILENAME _filename)  {strcpy (utf8lastname, _filename);
                                      utf8_to_utf16 (utf8name, filename);}
  CFILENAME displayname (void)       {return filename;}

#  else                              // Win32 console *************************************
  void setname (const FILENAME _filename)  {strcpy (utf8lastname, _filename);
                                      utf8_to_utf16 (utf8name, filename);
                                      CharToOemW (filename, oemname);}
  FILENAME displayname (void)        {return oemname;}
#  endif

#else                                // Linux *********************************************
  void setname (const FILENAME _filename)  {strcpy (utf8lastname, _filename);  filename = utf8name;}
  FILENAME displayname (void)        {return utf8name;}

#endif                               // END ***********************************************

  // Multi-volume support
  struct VolumeEntry {
    char *name;
    FILESIZE size;
    FILESIZE start_pos;
  };
  int num_vols;
  VolumeEntry *vols;
  int cur_vol_idx;
  FILESIZE virt_pos;
  FILESIZE total_virt_size;

  static bool detect_volume_pattern(const char *fn, char *pat_buf, size_t pat_size, int *start_num)
  {
    if (!fn || !*fn) return false;
    size_t len = strlen(fn);
    // 1. Check if fn ends with .NNN (e.g. .001)
    if (len > 4 && fn[len-4] == '.' && isdigit((unsigned char)fn[len-3]) &&
        isdigit((unsigned char)fn[len-2]) && isdigit((unsigned char)fn[len-1])) {
      char prefix[MY_FILENAME_MAX * 4];
      if (len - 4 >= sizeof(prefix)) return false;
      memcpy(prefix, fn, len - 4);
      prefix[len - 4] = 0;
      snprintf(pat_buf, pat_size, "%s.%%03d", prefix);
      *start_num = atoi(fn + len - 3);
      return true;
    }
    // 2. Check if fn contains .partN.arc
    const char *part = strstr(fn, ".part");
    if (part) {
      const char *num_start = part + 5;
      if (isdigit((unsigned char)*num_start)) {
        char *endp = NULL;
        long n = strtol(num_start, &endp, 10);
        if (endp && strcasecmp(endp, ".arc") == 0) {
          int digits = (int)(endp - num_start);
          char prefix[MY_FILENAME_MAX * 4];
          size_t prefix_len = (size_t)(part - fn);
          if (prefix_len >= sizeof(prefix)) return false;
          memcpy(prefix, fn, prefix_len);
          prefix[prefix_len] = 0;
          if (digits >= 2 && num_start[0] == '0')
            snprintf(pat_buf, pat_size, "%s.part%%02d.arc", prefix);
          else
            snprintf(pat_buf, pat_size, "%s.part%%d.arc", prefix);
          *start_num = (int)n;
          return true;
        }
      }
    }
    // 3. Check if fn doesn't have .001, but fn.001 exists
    char test001[MY_FILENAME_MAX * 4];
    snprintf(test001, sizeof(test001), "%s.001", fn);
    if (file_exists(test001)) {
      snprintf(pat_buf, pat_size, "%s.%%03d", fn);
      *start_num = 1;
      return true;
    }
    // 4. Check if fn without .arc + .arc.001 exists
    snprintf(test001, sizeof(test001), "%s.arc.001", fn);
    if (file_exists(test001)) {
      snprintf(pat_buf, pat_size, "%s.arc.%%03d", fn);
      *start_num = 1;
      return true;
    }
    return false;
  }

  void init()                             {handle   = -1;
                                           is_temp  = FALSE;
                                           num_vols = 0;
                                           vols     = NULL;
                                           cur_vol_idx = -1;
                                           virt_pos = 0;
                                           total_virt_size = 0;
#ifdef FREEARC_WIN
                                           filename = (TCHAR*) malloc_msg (MY_FILENAME_MAX*4);
#endif
                                           oemname  = (char*)  malloc_msg (MY_FILENAME_MAX);
                                           utf8name = (char*)  malloc_msg (MY_FILENAME_MAX*4);
                                           utf8lastname = utf8name;
                                           setname("");}

  void setname (MYFILE &base, FILENAME filename) {SetBaseDir (base.utf8name); setname (filename);}

  MYFILE ()                               {init();}
  MYFILE (FILENAME filename)              {init(); setname (filename);}
  MYFILE (MYFILE &base, FILENAME filename){init(); setname (base, filename);}
  MYFILE (FILENAME filename, MODE mode)   {init(); open (filename, mode);}
  virtual void done()                     {tryClose();
                                           if (is_temp)  remove(), unregisterTemporaryFile(*this), is_temp = FALSE;}
  virtual ~MYFILE()                       {done();
                                           if ((char*)filename!=utf8name)  free(filename);
                                           free(oemname); free(utf8name);}
  // File operations
  virtual bool exists ()
  {
    if (file_exists(filename)) return true;
    char pat_buf[MY_FILENAME_MAX * 4];
    int start_num = 1;
    if (detect_volume_pattern(filename, pat_buf, sizeof(pat_buf), &start_num)) {
      char test_name[MY_FILENAME_MAX * 4];
      snprintf(test_name, sizeof(test_name), pat_buf, start_num);
      if (file_exists(test_name)) return true;
    }
    return false;
  }
  virtual bool rename (MYFILE &other)     {return rename_file(filename, other.filename);}
  virtual int  remove ()                  {return remove_file(filename);}

  bool tryOpen (MODE mode)
  {
    if (mode==WRITE_MODE) {
      BuildPathTo (filename);
#ifdef FREEARC_WIN
      handle = ::_wopen (filename, O_WRONLY|O_BINARY|O_CREAT|O_TRUNC, S_IREAD|S_IWRITE);
#else
      handle =   ::open (filename, O_WRONLY|O_CREAT|O_TRUNC, S_IREAD|S_IWRITE);
#endif
      return handle>=0;
    }
    // Multi-volume detection for READ_MODE
    char pat_buf[MY_FILENAME_MAX * 4];
    int start_num = 1;
    if (detect_volume_pattern(filename, pat_buf, sizeof(pat_buf), &start_num)) {
      char test_name[MY_FILENAME_MAX * 4];
      int count = 0;
      while (true) {
        snprintf(test_name, sizeof(test_name), pat_buf, start_num + count);
        if (!file_exists(test_name))
          break;
        count++;
      }
      if (count > 1 || (count >= 1 && !file_exists(filename)) || (count == 1 && start_num == 1 && strstr(filename, ".001"))) {
        num_vols = count;
        vols = (VolumeEntry*) malloc(num_vols * sizeof(VolumeEntry));
        FILESIZE cur_start = 0;
        for (int i = 0; i < num_vols; i++) {
          snprintf(test_name, sizeof(test_name), pat_buf, start_num + i);
          vols[i].name = strdup(test_name);
          int vh = ::open(vols[i].name, O_RDONLY);
          if (vh >= 0) {
            vols[i].size = (FILESIZE)myfilelength(vh);
            ::close(vh);
          } else {
            vols[i].size = 0;
          }
          vols[i].start_pos = cur_start;
          cur_start += vols[i].size;
        }
        total_virt_size = cur_start;
        virt_pos = 0;
        cur_vol_idx = 0;
        handle = ::open(vols[0].name, O_RDONLY);
        return handle >= 0;
      }
    }
#ifdef FREEARC_WIN
    handle = ::_wopen (filename, O_RDONLY|O_BINARY, S_IREAD|S_IWRITE);
#else
    handle =   ::open (filename, O_RDONLY, S_IREAD|S_IWRITE);
#endif
    return handle>=0;
  }

  MYFILE& open (MODE mode)
  {
    bool success = tryOpen(mode);
    CHECK (success, (s,"ERROR: can't open file %s", utf8name));
    return *this;
  }

  MYFILE& open (FILENAME _filename, MODE mode)
  {
    setname (_filename);
    return open (mode);
  }

  void SetFileDateTime (time_t mtime)   {::SetFileDateTime (filename, mtime);}
  void close()
  {
    if (handle >= 0) {
      CHECK (::close(handle)==0, (s,"ERROR: can't close file %s", utf8name));
      handle = -1;
    }
    if (vols) {
      for (int i = 0; i < num_vols; i++)
        free(vols[i].name);
      free(vols);
      vols = NULL;
      num_vols = 0;
      cur_vol_idx = -1;
    }
  }
  bool isopen()    {return handle>=0;}
  void tryClose()  {if (isopen()) close();}

#ifdef FREEARC_WIN
  FILESIZE size    ()                {return num_vols > 0 ? total_virt_size : _filelengthi64 (handle);}
  FILESIZE curpos  ()                {return num_vols > 0 ? virt_pos : _lseeki64 (handle, 0,   SEEK_CUR);}
  void     seek    (FILESIZE pos)
  {
    if (num_vols > 0) {
      CHECK(pos <= total_virt_size, (s,"ERROR: file seek operation failed"));
      virt_pos = pos;
      int target = num_vols - 1;
      for (int i = 0; i < num_vols - 1; i++) {
        if (pos < vols[i].start_pos + vols[i].size) {
          target = i;
          break;
        }
      }
      if (cur_vol_idx != target) {
        if (handle >= 0) ::close(handle);
        handle = ::open(vols[target].name, O_RDONLY);
        CHECK(handle >= 0, (s,"ERROR: can't open volume %s", vols[target].name));
        cur_vol_idx = target;
      }
      FILESIZE off = pos - vols[target].start_pos;
      CHECK( _lseeki64 (handle, off, SEEK_SET) == (off_t)off, (s,"ERROR: file seek operation failed"));
    } else {
      CHECK( _lseeki64 (handle, pos, SEEK_SET) == pos, (s,"ERROR: file seek operation failed"));
    }
  }
#else
  FILESIZE size    ()                {return num_vols > 0 ? total_virt_size : myfilelength (handle);}
  FILESIZE curpos  ()                {return num_vols > 0 ? virt_pos : lseek (handle, 0,   SEEK_CUR);}
  void     seek    (FILESIZE pos)
  {
    if (num_vols > 0) {
      CHECK(pos <= total_virt_size, (s,"ERROR: file seek operation failed"));
      virt_pos = pos;
      int target = num_vols - 1;
      for (int i = 0; i < num_vols - 1; i++) {
        if (pos < vols[i].start_pos + vols[i].size) {
          target = i;
          break;
        }
      }
      if (cur_vol_idx != target) {
        if (handle >= 0) ::close(handle);
        handle = ::open(vols[target].name, O_RDONLY);
        CHECK(handle >= 0, (s,"ERROR: can't open volume %s", vols[target].name));
        cur_vol_idx = target;
      }
      FILESIZE off = pos - vols[target].start_pos;
      CHECK( lseek (handle, off, SEEK_SET) == (off_t)off, (s,"ERROR: file seek operation failed"));
    } else {
      CHECK( lseek (handle, pos, SEEK_SET) == pos, (s,"ERROR: file seek operation failed"));
    }
  }
#endif

  FILESIZE tryRead (void *buf, FILESIZE size)
  {
    if (num_vols > 0) {
      if (virt_pos >= total_virt_size || size <= 0) return 0;
      char *dst = (char*)buf;
      FILESIZE total_read = 0;
      while (total_read < size && virt_pos < total_virt_size) {
        int v = cur_vol_idx;
        FILESIZE left_in_vol = (vols[v].start_pos + vols[v].size > virt_pos)
                             ? (vols[v].start_pos + vols[v].size - virt_pos) : 0;
        if (left_in_vol == 0) {
          if (v + 1 >= num_vols) break;
          cur_vol_idx = v + 1;
          if (handle >= 0) ::close(handle);
          handle = ::open(vols[cur_vol_idx].name, O_RDONLY);
          CHECK(handle >= 0, (s,"ERROR: can't open volume %s", vols[cur_vol_idx].name));
#ifdef FREEARC_WIN
          _lseeki64(handle, 0, SEEK_SET);
#else
          lseek(handle, 0, SEEK_SET);
#endif
          continue;
        }
        FILESIZE want = size - total_read;
        if (want > left_in_vol) want = left_in_vol;
        int res = ::read(handle, dst + total_read, (size_t)want);
        if (res <= 0) break;
        total_read += res;
        virt_pos += res;
      }
      return total_read;
    } else {
      int result = ::read (handle, buf, size);
      CHECK(result>=0, (s,"ERROR: file read operation failed"));
      return result;
    }
  }
  void     read    (void *buf, FILESIZE size)   {CHECK (tryRead (buf, size) == size, (s,"ERROR: can't read %lu bytes", (unsigned long)size));}
  void     write   (void *buf, FILESIZE size)   {CHECK (::write (handle, buf, size) == size, (s,"ERROR: file write operation failed"));}
};


struct MYDIR : MYFILE
{
  int create_dir() {return ::create_dir(filename);}
  int remove_dir() {return ::remove_dir(filename);}
  int dir_exists() {return ::dir_exists(filename);}
  virtual int remove ()  {return remove_dir();}

  // Make it a temporary directory, removed automatically by destructor
  bool create_tempdir()
  {
#ifdef FREEARC_WIN
    utf16_to_utf8 (GetTempDir(), utf8name);
#elif defined(FREEARC_UNIX)
    strcpy(utf8name, GetTempDir());
#else
    ???
#endif
    SetBaseDir (utf8name);
    for (unsigned i = time_based_random(), cnt=0; cnt<1000; cnt++)
    {
        i = i*54322457 + 137;
        char dirname[100];
        sprintf(dirname, "%s%u", "freearc", i);
        setname(dirname);
        BuildPathTo(filename);
        if (create_dir() == 0)   {mark_as_temporary(); return TRUE;}  // Success
    }
    return FALSE;                                                     // Fail
  }
  virtual ~MYDIR()  {done();}
};


/******************************************************************************
** Bounds-checked arrays ******************************************************
******************************************************************************/

#ifdef DEBUG
#define ARRAYD(type,name,size)  Array<type> name
template <class ELEM>
struct Array
{
    int n;
    ELEM *p;
    Array(int _n)             {n=_n; p=(ELEM*)malloc_msg(sizeof(ELEM)*n);}
    ~Array()                  {free(p);}
    ELEM& operator [](int i)  {CHECK( 0<=i && i<n, (s,"INDEXING ERROR: %d instead of [0,%d)", i, n));
                               return p[n];}
    operator void*()          {return p;}
};
#else
#define ARRAYD(type,name,size)  type name[size]
#endif

#endif  // __cplusplus

/******************************************************************************
** END. ***********************************************************************
******************************************************************************/

#endif  // FREEARC_COMMON_H
