#pragma once

// ��������� �������, ��������� FreeArc:
//   ������ � ������������� Footer ����� � ������ ����������
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

#include "Environment.h"
#include "Compression/Compression.h"
#include "WinCompat.h"

#define aSIGNATURE make4byte(65,114,67,1)  /* ��������� ������� FreeArc: ArC */
#define MAX_FOOTER_DESCRIPTOR_SIZE 4096    /* ������������ ������ ����������� ����� ������ */

static char *g_archive_password = NULL;
static inline void SetArchivePassword (char *p) { g_archive_password = p; }
static inline char *GetArchivePassword (void)   { return g_archive_password; }

/******************************************************************************
** �����, ����������� �������, ������� ���� ������ :) *************************
******************************************************************************/
#ifdef __cplusplus
template <typename T> class ARRAY
{
public:
  int size;                         // ���-�� ��������� � �������
  T  *data;                         // ������, ���������� � �������
  bool autodelete;                  // ������������� ������� data ��� �������� ������ �������

  void setsize (int _size)          {size = _size; data = size? new T[size] : NULL; autodelete=TRUE;}
  void resize (int _size)           {if(autodelete) delete data; setsize(_size);}   // �������� ����� ��� ������������� �������
  void set (int _size, void* ptr)   {resize(0); size=_size, data=(T*)ptr, autodelete=FALSE;}  // ������������ � �������� ����������� ������� ��������� ����� � ������
  ARRAY (int _size=0)               {setsize (_size);}       // ������� ������ � ������ _size
  ~ARRAY()                          {resize(0);}
  T& operator[] (int i)             {return data[i];}
  T& operator() (int i)             {return data[i];}
};

#endif  // __cplusplus


/******************************************************************************
** �������� ��� ������� �����, ������������ � ��������� ***********************
******************************************************************************/
typedef time_t   XFILETIME;        // ����/����� �����
typedef int      BOOL;             // ��������� ���
typedef uint32   CRC;              // CRC �����
typedef char*    COMPRESSOR;       // ����� ������
typedef int      BLOCKTYPE;        // ��� ��������� �����:
enum {DESCR_BLOCK=0, HEADER_BLOCK, DATA_BLOCK, DIR_BLOCK, FOOTER_BLOCK, RECOVERY_BLOCK};

struct BLOCK                       // ���������� � ����� ������
{
  BLOCKTYPE  type;
  COMPRESSOR compressor;
  FILESIZE   pos;
  FILESIZE   origsize;
  FILESIZE   compsize;
  CRC        crc;
};

struct BLOCK_DESCRIPTOR : BLOCK {};// ���������� ����� ������


/******************************************************************************
** ������ ������ ������ *******************************************************
******************************************************************************/
class MEMORY_BUFFER
{
public:
    char *buf;         // ����� ������ ������, ��������� ��� ������������ ������
    char *bufend;      // ����� ����� ������, ������������ ��� �������� ������ �� ��� �������
    char *p;           // ������� ��������� ������

    MEMORY_BUFFER () {buf = NULL;}
    ~MEMORY_BUFFER() {free (buf);}

    // ������������ ����� ��� ������ ������ �� ����� `file` � ������� `pos` ����� `len`
    MEMORY_BUFFER& open (MYFILE &file, FILESIZE pos, FILESIZE size)
    {
      free (buf);                      // ��������� ���������� �������������� �����
      buf = (char*) malloc (size+8);   // �� �������� 8 ������ ����, ����� ����� ���� ������ ������������ ����� �����, �� �������� ����� �� ������� ������
      CHECK (buf, (s,"ERROR: can't alloc %lu memory bytes", (unsigned long)(size+8)));
      file.seek (pos);
      file.read (buf, size);
      p=buf, bufend=p+size;
      return *this;
    }

    // ��������� ������ �� ����� � �����, ����������� �� � ��������� �� CRC
    MEMORY_BUFFER& openCompressedCheckCRC (COMPRESSOR compressor, FILESIZE origsize, MYFILE &file, FILESIZE pos, FILESIZE compsize, CRC right_crc)
    {
      open (file, pos, compsize);
      char *origbuf = (char*) malloc (origsize+8);  // ������ 8 ���� ��� ������ ��� ���������� readInteger
      char resolved[MAX_METHOD_STRLEN];
      int prc = ApplyPasswordToCompressor (compressor, GetArchivePassword(), resolved);
      CHECK (prc!=FREEARC_ERRCODE_GENERAL, (s,"ERROR: archive is encrypted, use -pPassword"));
      CHECK (prc!=FREEARC_ERRCODE_BAD_COMPRESSED_DATA, (s,"ERROR: wrong password"));
      CHECK (prc>=0, (s,"ERROR: unsupported compression method \"%s\"", compressor));
      int result = DecompressMem (resolved, buf, compsize, origbuf, origsize);
      CHECK (result!=FREEARC_ERRCODE_INVALID_COMPRESSOR, (s,"ERROR: unsupported compression method \"%s\"", resolved));
      CHECK (result==origsize, (s,"ERROR: archive structure corrupted (decompression of control block failed)"));
      free(buf), p=buf=origbuf, bufend=buf+origsize;
      CRC crc = CalcCRC (buf, origsize);
      CHECK (crc==right_crc, (s,"ERROR: archive structure corrupted (control block failed CRC check)"))
      return *this;
    }

    // ��������� ������ �� ����� � ����� � ��������� ������������ �� CRC ��������, ����������� � ��������� ������ ���� ������
    MEMORY_BUFFER& openWithCRCAtEnd (MYFILE &file, FILESIZE pos, FILESIZE size)
    {
      open (file, pos, size);
      bufend -= sizeof(CRC);
      CRC right_crc = *(CRC*)bufend;
      CRC crc = CalcCRC (buf, size-sizeof(CRC));
      CHECK (crc==right_crc, (s,"ERROR: archive structure corrupted (descriptor failed CRC check)"))
      return *this;
    }


    // ��������� ����� ������?
    bool eof ()         {return p>=bufend;}
    // ���������� ��������� ������ �� n ������ ����� � ���������, ��� �� �� ����� �� ����� ������ :)
    void skip (int n)   {p+=n; CHECK(p<=bufend, (s,"ERROR: archive structure corrupted (bad data)"));}

    // ��������� ����� ����� � ������� � ���������� ������
    uint64 readInteger()
    {
      uint32 x = *(uint32*)p;
           if ((x&  1)==  0)  {skip(1); return (x & ((1u<< 8)-1))>>1;}
      else if ((x&  3)==  1)  {skip(2); return (x & ((1u<<16)-1))>>2;}
      else if ((x&  7)==  3)  {skip(3); return (x & ((1u<<24)-1))>>3;}
      else if ((x& 15)==  7)  {skip(4); return (x               )>>4;}
      uint64 y = *(uint64*)p;
           if ((x& 31)== 15)  {skip(5); return (y & ((uint64(1)<<40)-1))>>5;}
      else if ((x& 63)== 31)  {skip(6); return (y & ((uint64(1)<<48)-1))>>6;}
      else if ((x&127)== 63)  {skip(7); return (y & ((uint64(1)<<56)-1))>>7;}
      else if ((x&255)==127)  {skip(8); return (y                      )>>8;}
      else                    {skip(1); uint64 y = *(uint64*)p; skip(8); return y;}
    }

    template <typename T> MEMORY_BUFFER &read (T *x)   {*x = readInteger();                       return *this;}
    template <typename T> MEMORY_BUFFER &read1(T *x)   {*x = *(uint8 *)p & ((1u<< 8)-1); skip(1); return *this;}
    template <typename T> MEMORY_BUFFER &read2(T *x)   {*x = *(uint16*)p & ((1u<<16)-1); skip(2); return *this;}
    template <typename T> MEMORY_BUFFER &read4(T *x)   {*x = *(uint32*)p               ; skip(4); return *this;}

    // ��������� `n` �������� � ������� �� ��� ����������������� ������
    template <typename T> MEMORY_BUFFER &read (int n, ARRAY<T> *array)
    {
      array->resize(n);
      iterate (n, read( &((*array)[i]) ));
      return *this;
    }
    // ���������� �����������, �� �������� ������������ ��������
    template <typename T> MEMORY_BUFFER &read1 (int n, ARRAY<T> *array)
    {
      array->resize(n);
      iterate (n, read1( &((*array)[i]) ));
      return *this;
    }
    // ���������� �����������, �� �������� �������������� ��������
    template <typename T> MEMORY_BUFFER &read4 (int n, ARRAY<T> *array)
    {
      array->resize(n);
      iterate (n, read4( &((*array)[i]) ));
      return *this;
    }

    // ��������� �� ������ ���-�� ��������� � ������� � ����� ��� ����������
    template <typename T> MEMORY_BUFFER &read( ARRAY<T> *array)
    {
      int n; read (&n);        // ��������� ���������� ��������� � �������
      return read (n, array);  // ������� � ������ ��������� �������
    }

    MEMORY_BUFFER &read (char *x)     // ��������� ������
    {
      *x = *(char*)p;
      skip(1);
      return *this;
    }

    MEMORY_BUFFER &read (char* *x)    // ��������� ������
    {
      char *end = (char*) memchr( p, '\0', (uint8*)bufend - (uint8*)p);
      CHECK(end, (s,"ERROR: archive structure corrupted (bad string)"));
      *x = (char*)p;         // ����������� ������ ����� ��������� ��������������� � �����
      p = end+1;
      return *this;
    }

    MEMORY_BUFFER &read (BLOCK_DESCRIPTOR *x)    // ��������� ���������� ����� ������
    {
      read (&x->type);
      read (&x->compressor);
      read (&x->pos);
      read (&x->origsize);
      read (&x->compsize);
      read4(&x->crc);
      return *this;
    }
};


/*****************************************************************************************************
** ��������� ���������� ����� ������, �.�. ����������� � ������ ��������������� ����� ������ ����� ***
*****************************************************************************************************/

struct LOCAL_BLOCK_DESCRIPTOR : BLOCK
{
  MEMORY_BUFFER buffer;  // �����, ������������ ��� ������ �����������. ����������� �������� compressor ����� ��������� �� ������ � ���� ������

  // ��������� �� ������ ��������� ���������� �����
  LOCAL_BLOCK_DESCRIPTOR (MYFILE &arcfile, FILESIZE descr_pos)
  {
    FILESIZE descr_size  =  mymin (arcfile.size()-descr_pos, MAX_FOOTER_DESCRIPTOR_SIZE);
    buffer.openWithCRCAtEnd (arcfile, descr_pos, descr_size);
    uint32 sign;
    buffer.read4 (&sign );
    buffer.read  (&type );
    buffer.read  (&compressor );
    buffer.read  (&origsize );
    buffer.read  (&compsize );
    buffer.read4 (&crc );
    CHECK (sign==aSIGNATURE && origsize>0 && compsize>0 && compsize<=descr_pos, (s,"ERROR: archive structure corrupted (strange descriptor)"));
    pos = descr_pos-compsize;
    //printf("%4.4s %d %s %u %u %08x\n", &sign, type, compressor, origsize, compsize, crc);
  }
};

// ��������� ���������� FOOTER BLOCK
struct FOOTER_BLOCK_LOCAL_DESCRIPTOR : LOCAL_BLOCK_DESCRIPTOR
{
  // ��������� ��������� ���������� ����� � ��������� �������������� ��������, ������� ����� ������ ��� FOOTER BLOCK
  FOOTER_BLOCK_LOCAL_DESCRIPTOR (MYFILE &arcfile, FILESIZE descr_pos)  :  LOCAL_BLOCK_DESCRIPTOR (arcfile, descr_pos)
  {
    CHECK (type==FOOTER_BLOCK, (s,"ERROR: archive structure corrupted (footer block not found)"));
  }
};

// ����� � �������� ����� ���������� FOOTER BLOCK � ���������� ��� �������
FILESIZE FindFooterDescriptor (MYFILE &arcfile)
{
  char buf[MAX_FOOTER_DESCRIPTOR_SIZE];
  FILESIZE arcsize = arcfile.size();
  FILESIZE size = mymin (arcsize, MAX_FOOTER_DESCRIPTOR_SIZE);  // �� ����� ������ ��������� � ��������� size ������ ������
  arcfile.seek (arcsize-size);
  arcfile.read (buf, size);
  for (char *ptr=buf+size-sizeof(uint32); ; ptr--) {
    if (*(uint32*)ptr == aSIGNATURE)    return (arcsize-size)+(ptr-buf);   // ������� � ����� ���������, � ������� ���������� ���������� FOOTER BLOCK
    CHECK (ptr>buf, (s,"ERROR: this is not FreeArc archive or this archive is corrupt"));   // ��������� �� ������� � ��������� MAX_FOOTER_DESCRIPTOR_SIZE ������ ������
  }
}


/******************************************************************************
** ���������� � ��������� ������ (�.�. ���� ��������� ������) *****************
******************************************************************************/
class ARCHIVE
{
private:
  MEMORY_BUFFER buffer;  // �����, �������� ���������� FOOTER BLOCK. ������������ ������ ��� �������� ������, ��������� �� ���������� ������ �� ���������� � ��� ������
public:
  MYFILE arcfile;        // ���� ������. ����������� ��� �������� ARCHIVE � ����������� ��� ��� �����������
  ARRAY <BLOCK_DESCRIPTOR> control_blocks_descriptors;   // ����������� ��������� ������ ������, �������� �� FOOTER BLOCK
  int                      arcLocked;  // ������� ����, ��� ����� ������ �� ���������
  ARRAY <char>             arcComment; // ����������� � ������. ����� ��������� ������� �������
  FILESIZE                 SFXSize;    // ������ SFX-������ ����� �������

  ARCHIVE () {}
  ARCHIVE (FILENAME arcname) : arcfile (arcname, READ_MODE) {}   // ��������� ���� ������
  void read_structure();               // ��������� �������� ��������� ������
};

// ��������� �� FOOTER BLOCK �������� ��������� ������
void ARCHIVE::read_structure()
{
  FILESIZE pos = FindFooterDescriptor (arcfile);            // ����� � ������ ���������� FOOTER BLOCK
  FOOTER_BLOCK_LOCAL_DESCRIPTOR arcFooter (arcfile, pos);   // ��������� ���� ���������� � ������������ ���
  buffer.openCompressedCheckCRC (arcFooter.compressor, arcFooter.origsize, arcfile, arcFooter.pos, arcFooter.compsize, arcFooter.crc); // ��������� � ����� ���������� FOOTER BLOCK
  buffer.read (&control_blocks_descriptors);                // ������������ �� ������ ����������� ��������� ������ ������
  iterate_array (i, control_blocks_descriptors) {
    control_blocks_descriptors[i].pos  =  arcFooter.pos - control_blocks_descriptors[i].pos; // ������� ������������� ������ ������ (���������� ��� �������� ������������ ������ ����� �����) �� ����������
    //printf("%d %d\n", control_blocks_descriptors[i].pos, control_blocks_descriptors[i].compsize);
  }
  SFXSize = control_blocks_descriptors[0].pos;   // ��, ��� ��������� ����� ������ ������ ������, ����� ����� ������� SFX-������� :)
  buffer.read1 (&arcLocked);                     // 1 ����: 1 - ����� ������������ �� ���������� ���������, 0 - ���
  int cmtlen;  buffer.read (&cmtlen);            // ����������� ������� ������� - � UCS4
  arcComment.set (cmtlen, buffer.p);
  for (int i=0; i<cmtlen; i++)  arcComment[i] = buffer.p[i*4];
  buffer.skip (cmtlen*4);
  char *rr_settings; if (!buffer.eof())  buffer.read (&rr_settings);
  if (!buffer.eof()) {
    buffer.read (&cmtlen);                       // ����������� ���������� ��� ������ �������� � ���� �������� ������
    if (cmtlen>0)  arcComment.set (cmtlen, buffer.p);
  }
  //printf("%d %d %*.*s\n", arcLocked, arcComment.size, arcComment.size, arcComment.size, &arcComment[0]);
}


/******************************************************************************
** ���� �������� **************************************************************
******************************************************************************/
class DIRECTORY_BLOCK
{
public:
  MYFILE &arcfile;                     // ���� ������, �������� ����������� ��� ������� ���� ��������
private:
  MEMORY_BUFFER buffer;                // �����, �������� ���� ������� � �������� ����. ��������������� ����� ������ ��������� �� ���� �����, ������� �� �� ��������� �� ���������� ������ � ���������

  int               dirs_in_block;     // ���������� ���������, ���������� � ���� DIRECTORY BLOCK
  ARRAY <FILENAME>  dirs;              // ����� ���������
  ARRAY <int>       dir_numbers;       // ����� �������� ��� ������� �� ������
public:
  FILENAME  dirname (int i)  {return dirs[dir_numbers[i]];}  // ��� �������� ��� i-�� �����
  FILENAME  fullname(int i, char buffer[]);                  // ������ ��� i-�� �����
  int               total_files;       // ���������� ������, ��������� � ���� ����� ��������
  ARRAY <FILENAME>  name;              // ����� ������ (��� ����� ��������)
  ARRAY <FILESIZE>  size;              // ������� ������
  ARRAY <XFILETIME> time;              // ����� ����������� ������
  ARRAY <BOOL>      isdir;             // ��������� ����� "��� �������?"
  ARRAY <CRC>       crc;               // CRC ������

  int                       num_of_blocks;  // ���-�� ������ ������
  ARRAY <int>               num_of_files;   // ���-�� ������ � ������ ����� ������, ������� ����� ������ ��������� ���������� �� ����� ������� ����� � ��������� ����� ��� block_start()/block_end()
  ARRAY <BLOCK_DESCRIPTOR>  data_block;

  ARRAY <uint32>            mode;           // Unix permissions / file mode
  ARRAY <BOOL>              issymlink;      // Is entry a symlink?
  ARRAY <char*>             symlink_target; // Symlink target path     // �������� ������ ������ (����������, ������� � ������, �����)

  int block_start (int block_num)  {return block_num>0? num_of_files[block_num-1] : 0;}  // ����� ������� ����� � ����� ������ block_num
  int block_end   (int block_num)  {return num_of_files[block_num];}                     // ����� ������� ����� � ��������� ����� ������ (�.�. ���������� � ���� + 1)

  DIRECTORY_BLOCK (ARCHIVE &arc, BLOCK &block_info);   // ������ �� ������ ���������� ����� �������� � ���������� ��� ���, ����� ���������� ������� ������ � �������� ������ ����� � ������ ����� ������
};

DIRECTORY_BLOCK::DIRECTORY_BLOCK (ARCHIVE &arc, BLOCK &block_info) : arcfile (arc.arcfile)
{
  // �������� � ����� ���������� ��������, ��������� ��� � �������� CRC
  CHECK (block_info.type == DIR_BLOCK, (s,"INTERNAL ERROR: must be dir block"));
  buffer.openCompressedCheckCRC (block_info.compressor, block_info.origsize, arcfile, block_info.pos, block_info.compsize, block_info.crc);

  // ��������� ����� ���-�� solid-������ � ���������� � ������ �� ��� - ���-�� ������, ����������,
  // �������� ������ solid-����� ������������ ����� ��������, � ����������� ������
  buffer.read  (&num_of_blocks);    buffer.read  (num_of_blocks, &num_of_files);
  ARRAY <COMPRESSOR> compressors;   buffer.read  (num_of_blocks, &compressors);
  ARRAY <FILESIZE>   offsets;       buffer.read  (num_of_blocks, &offsets);
  ARRAY <FILESIZE>   compsizes;     buffer.read  (num_of_blocks, &compsizes);

  // �������������� data_block[] �� ����������� ������
  data_block.setsize (num_of_blocks);
  iterate_array (i, data_block)
  {
    data_block[i].type       = DATA_BLOCK;
    data_block[i].compressor = compressors[i];
    data_block[i].pos        = block_info.pos - offsets[i];    // �������� ���������� ����� ����� � ������ ������ �� ��� �������� ������������ ����� ��������
    data_block[i].origsize   = 0;               // � ��� ���� ����?
    data_block[i].compsize   = compsizes[i];
    data_block[i].crc        = 0;               // CRC ������ ������ �� �������� - ��� �� � ����
    //printf("datablock %s %d %d\n", data_block[i].compressor, data_block[i].pos, data_block[i].compsize);
  }

  // ��������� ����� ���-�� ������ � ���� �������� � ������� num_of_files[block_num] ���, ����� ���� ������ ����� ���� ������������ ��� ����������� ������, ������������� ����� ������ block_num
  total_files=0;  iterate (num_of_blocks, (total_files += num_of_files[i], num_of_files[i] = total_files));

  // ��������� ����� ��������� � ������� �������-����������� ��������� � �������� �� ������ ���������
  buffer.read  (&dirs);
  iterate_array (i, dirs)    replace (dirs[i], UNSUPPORTED_PATH_DELIMITERS, PATH_DELIMITER);

  // ��������� ���������� �� ��������� ������
  buffer.read  (total_files, &name);
  buffer.read  (total_files, &dir_numbers);
  buffer.read  (total_files, &size);
  buffer.read4 (total_files, &time);
  buffer.read1 (total_files, &isdir);
  buffer.read4 (total_files, &crc);

  // Initialize defaults for POSIX mode and symlinks
  mode.resize (total_files);
  issymlink.resize (total_files);
  symlink_target.resize (total_files);
  for (int i = 0; i < total_files; i++) {
    mode[i] = isdir[i] ? (S_IFDIR | 0755) : (S_IFREG | 0644);
    issymlink[i] = FALSE;
    symlink_target[i] = (char*)"";
  }

  // Parse extended metadata tags if present
  while (!buffer.eof()) {
    uint64 tag = buffer.readInteger();
    if (tag == 0) break;
    if (tag == 1) { // TAG_POSIX_MODE = 1
      uint64 count = buffer.readInteger();
      for (int i = 0; i < (int)count; i++) {
        uint32 m = 0;
        buffer.read4(&m);
        if (i < total_files) mode[i] = m;
      }
    } else if (tag == 2) { // TAG_SYMLINK = 2
      uint64 count = buffer.readInteger();
      for (int i = 0; i < (int)count; i++) {
        char *target = NULL;
        buffer.read(&target);
        if (i < total_files) {
          symlink_target[i] = target ? target : (char*)"";
          if (target && *target) {
            issymlink[i] = TRUE;
            mode[i] = (mode[i] & ~S_IFMT) | S_IFLNK;
          }
        }
      }
    } else {
      break; // Unknown tag
    }
  }

  //iterate( total_files, printf("%s %s %d %d\n", dirname(i), name[i], size[i], isdir[i]));
  //printf("%d files\n", total_files);
}

// ������ ��� i-�� �����
FILENAME DIRECTORY_BLOCK::fullname (int i, char buffer[])
{
  strcpy (buffer, dirname(i));
  if (buffer[0] != '\0')  strcat (buffer, STR_PATH_DELIMITER);
  strcat (buffer, name[i]);
  return buffer;
}

