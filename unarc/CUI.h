#pragma once

#include <stdio.h>

class CUI : public BASEUI
{
private:
  char outdir[MY_FILENAME_MAX*4];  //unicode: utf-8 encoding
  uint64 total_files, total_bytes, total_packed;
  char cur_cmd;
  int silent_mode;
public:
  void DisplayHeader (char* header);
  bool ProgressFile  (bool isdir, const char *operation, FILENAME filename, uint64 filesize);
  void EndProgress   (COMMAND *cmd);
  void EndProgress   ();
  bool AllowProcessing (char cmd, int silent, FILENAME arcname, char* comment, int cmtsize, FILENAME outdir);
  FILENAME GetOutDir();
  char AskOverwrite (FILENAME filename, uint64 size, time_t modified);

  void ListHeader (COMMAND &);
  void ListFooter (COMMAND &);
  void ListFiles (DIRECTORY_BLOCK *, COMMAND &);
};

void CUI::DisplayHeader (char* header)
{
  printf ("%s", header);
}

bool CUI::ProgressFile (bool isdir, const char *operation, FILENAME filename, uint64 filesize)
{
  if (cur_cmd == 'p' || silent_mode) return TRUE;
  printf (isdir?  "%s %s" STR_PATH_DELIMITER "\n"  :  "%s %s (%llu bytes)\n",
          operation, filename, filesize);
  return TRUE;
}

void CUI::EndProgress (COMMAND *cmd)
{
  if (cur_cmd == 'p' || silent_mode || (cmd && (cmd->to_stdout || cmd->silent))) return;
  printf ("All OK");
}

void CUI::EndProgress ()
{
  if (cur_cmd == 'p' || silent_mode) return;
  printf ("All OK");
}

FILENAME CUI::GetOutDir()
{
  return outdir;
}

bool CUI::AllowProcessing (char cmd, int silent, FILENAME arcname, char* comment, int cmtsize, FILENAME _outdir)
{
  cur_cmd = cmd;
  silent_mode = silent;
  strcpy (outdir, _outdir);
  if (cmd != 'p' && silent == 0) {
  printf (". %s archive: %s\n",                       // Выведем имя обрабатываемого архива
    cmd=='l'||cmd=='v'? "Listing" : cmd=='t' ? "Testing" : "Extracting", drop_dirname(arcname));
  if (cmtsize>0)                                      // Выведем архивный комментарий
#ifdef FREEARC_WIN
{
    // Convert comment from UTF-8 to OEM encoding before printing
    char *oemname = (char*) malloc(cmtsize+1);
    strncpy (oemname, comment, cmtsize);
    oemname[cmtsize] = 0;
    utf8_to_oem (oemname, oemname);
    printf ("%s\n", oemname);
    free (oemname);
}
#else
    printf("%*.*s\n", cmtsize, cmtsize, comment);
#endif
  }

  return TRUE;
}

char CUI::AskOverwrite (FILENAME filename, uint64 size, time_t modified)
{
  char help[] = "Valid answers: Y - yes, N - no, A - overwrite all, S - skip all, Q - quit\n";
  again: printf ("Overwrite %s (y/n/a/s/q) ? ", filename);
  char answer[256];  fgets(answer, 256, stdin);  *answer = tolower(*answer);
  if (strlen(answer)!=1 || !strchr("ynasq", *answer))  {printf (help);  goto again;}
  if (*answer=='q') {printf ("Extraction aborted\n");  exit(1);}
  return *answer;
}


/******************************************************************************
** Реализация команды получения листинга архива *******************************
******************************************************************************/
static inline void format_mode(uint32 mode, char *buf) {
  if (S_ISLNK(mode))      buf[0] = 'l';
  else if (S_ISDIR(mode)) buf[0] = 'd';
  else if (S_ISCHR(mode)) buf[0] = 'c';
  else if (S_ISBLK(mode)) buf[0] = 'b';
  else if (S_ISFIFO(mode))buf[0] = 'p';
  else if (S_ISSOCK(mode))buf[0] = 's';
  else                    buf[0] = '-';

  buf[1] = (mode & S_IRUSR) ? 'r' : '-';
  buf[2] = (mode & S_IWUSR) ? 'w' : '-';
  buf[3] = (mode & S_ISUID) ? ((mode & S_IXUSR) ? 's' : 'S') : ((mode & S_IXUSR) ? 'x' : '-');

  buf[4] = (mode & S_IRGRP) ? 'r' : '-';
  buf[5] = (mode & S_IWGRP) ? 'w' : '-';
  buf[6] = (mode & S_ISGID) ? ((mode & S_IXGRP) ? 's' : 'S') : ((mode & S_IXGRP) ? 'x' : '-');

  buf[7] = (mode & S_IROTH) ? 'r' : '-';
  buf[8] = (mode & S_IWOTH) ? 'w' : '-';
  buf[9] = (mode & S_ISVTX) ? ((mode & S_IXOTH) ? 't' : 'T') : ((mode & S_IXOTH) ? 'x' : '-');

  buf[10] = '\0';
}

void CUI::ListHeader (COMMAND &command)
{
  if (command.cmd=='l')
      printf ("Date/time                  Size Filename\n"
              "----------------------------------------\n");
  else
      printf ("Date/time            Attr               Size          Packed      CRC Filename\n"
              "---------------------------------------------------------------------------------\n");
  total_files=total_bytes=total_packed=0;
}

void CUI::ListFooter (COMMAND &command)
{
  if (command.cmd=='l')
      printf ("----------------------------------------\n");
  else
      printf ("---------------------------------------------------------------------------------\n");
  printf ("%.0lf files, %.0lf bytes, %.0lf compressed", double(total_files), double(total_bytes), double(total_packed));
}

void CUI::ListFiles (DIRECTORY_BLOCK *dirblock, COMMAND &command)
{
  int  b=0;                // current_data_block
  bool Encrypted = FALSE;  // текущий солид-блок зашифрован?
  uint64 packed=0;
  iterate_var (i, dirblock->total_files) {
    // Увеличим номер солид-блока если мы вышли за последний принадлежащий ему файл
    if (i >= dirblock->block_end(b))
      b++;
    // Если это первый файл в солид-блоке - соберём block-related информацию
    if (i == dirblock->block_start(b))
    { // Запишем на первый файл в блоке весь его упакованный размер
      packed = dirblock->data_block[b].compsize;
      // Запомним информацию о солид-блоке для использования её со всеми файлами из этого солид-блока
      char *c = dirblock->data_block[b].compressor;
      Encrypted = strstr (c, "+aes-")!=NULL || strstr (c, "+serpent-")!=NULL || strstr (c, "+blowfish-")!=NULL || strstr (c, "+twofish-")!=NULL;
    }


    if (command.accept_file (dirblock, i)) { //   Если этот файл требуется обработать
      unsigned long long filesize = dirblock->size[i];
      char timestr[100];  FormatDateTime (timestr, 100, dirblock->time[i]);

      char modestr[16];
      format_mode (dirblock->mode[i], modestr);

      if (command.cmd=='l') {
        if (dirblock->issymlink[i])
          printf ("%s       -lnk-", timestr);
        else if (dirblock->isdir[i])
          printf ("%s       -dir-", timestr);
        else
          printf ("%s %11.0lf", timestr, double(filesize));
      } else {
        printf ("%s %s %15.0lf %15.0lf %08x", timestr, modestr, double(filesize), double(packed), dirblock->crc[i]);
      }
      printf ("%c", Encrypted? '*':' ');

      // Print filename using console encoding
      static char filename[MY_FILENAME_MAX*4];
      dirblock->fullname (i, filename);
      static MYFILE file;  file.setname (filename);
      if (dirblock->issymlink[i] && dirblock->symlink_target[i] && dirblock->symlink_target[i][0])
        printf ("%s -> %s\n", file.displayname(), dirblock->symlink_target[i]);
      else
        printf ("%s\n", file.displayname());

      total_files++;
      total_bytes  += filesize;
      total_packed += packed;    packed = 0;
    }
  }
}

