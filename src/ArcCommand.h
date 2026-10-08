#pragma once
#include <vector>
#include <string>
#include <limits.h>
#include <unistd.h>

// SFX module is just unarc.cpp compiled with FREEARC_SFX defined
#ifdef FREEARC_SFX
#define NAME           "SFX"
#else
#define NAME           "unpacker"
#endif

#define HEADER1        "UnArc 0.60RC "
#define HEADER2        "  http://freearc.org  2009-10-05 (link seems dead)\n"


/******************************************************************************
** Callbacks ��� ����������� ������� ******************************************
******************************************************************************/
class COMMAND;

#if defined(FREEARC_WIN) && defined(FREEARC_GUI)
typedef CFILENAME MYFILENAME;
#else
typedef  FILENAME MYFILENAME;
#endif

class BASEUI
{
public:
  virtual ~BASEUI() {}
  virtual void DisplayHeader (char* header) {}
  virtual bool AllowProcessing (char cmd, int silent, MYFILENAME arcname, char* comment, int cmtsize, FILENAME outdir)  {return TRUE;}
  virtual FILENAME GetOutDir() {return "";}
  virtual void BeginProgress (uint64 totalBytes)    {}
  virtual bool ProgressRead  (uint64 readBytes)     {return TRUE;}
  virtual bool ProgressWrite (uint64 writtenBytes)  {return TRUE;}
  virtual bool ProgressFile  (bool isdir, const char *operation, MYFILENAME filename, uint64 filesize)  {return TRUE;}
  virtual void EndProgress(COMMAND*) {}
  virtual char AskOverwrite (MYFILENAME filename, uint64 size, time_t modified) {return 'n';}
  virtual void ListHeader (COMMAND &) {}
  virtual void ListFooter (COMMAND &) {}
  virtual void ListFiles (DIRECTORY_BLOCK*, COMMAND &) {}
  virtual void Abort (COMMAND*, int errcode)  {exit (FREEARC_EXIT_ERROR);}
};


/******************************************************************************
** ���������� � ����������� ������������� ������� *****************************
******************************************************************************/
class COMMAND
{
public:
  char cmd;             // Executed command
  FILENAME arcname;     // Archive name
  FILENAME *filenames;  // Pointer to argv filenames
  std::vector<std::string> excludes;      // -x patterns
  std::vector<std::string> includes;      // -i patterns
  std::vector<std::string> file_patterns; // Positional patterns (and @listfiles)
  std::string arcprefix;                  // -ap prefix
  BOOL to_stdout;                         // Extract to stdout (-so or cmd 'p')
  MYDIR    outpath;     // Output path (-dp)
  MYDIR    workdir;     // Temp directory (-w)
  MYFILE   runme;       // SFX executable
  BOOL tempdir;
  BOOL wipeoutdir;
  BOOL ok;
  int  silent;          // Silent mode -s
  BOOL yes;             // -o+
  BOOL no;              // -o-
  BOOL noarcext;        // --noarcext
  BOOL nooptions;       // --
  char *password;       // -p

  COMMAND (int argc, char *argv[]);
  void Prepare();
  bool list_cmd()  {return cmd=='l' || cmd=='v';}
  BOOL accept_file (DIRECTORY_BLOCK *dirblock, int i);
  void add_exclude (const char *pat);
  void add_include (const char *pat);
};


/******************************************************************************
** External compressors support ***********************************************
******************************************************************************/
extern "C" {
#include "Compression/External/C_External.h"
}

// Register external compressors declared in arc.ini
void RegisterExternalCompressors (char *progname)
{
#ifndef FREEARC_TINY
  // Open config file arc.ini found in the same dir as sfx/unarc
  char *cfgfile = "arc.ini";
  char *name = (char*) malloc (strlen(progname) + strlen(cfgfile));
                                                 if (!name)  return;
  strcpy(name, progname);
  strcpy(drop_dirname(name), cfgfile);
  MYFILE f(name);
  if (!f.tryOpen(READ_MODE))                     return;

  // Read config file into memory
  FILESIZE size = f.size();                      if (!size)  return;
  char *contents = (char*) malloc(size+2);       if (!contents)  return;
  *contents = '\n';
  size = f.tryRead(contents+1, size);            if (size<0)  return;
  contents[size] = '\0';

  // Register each external compressor found in config file
  char *ANY_HEADING = "\n[", *EXT_HEADING = "[External compressor:";
  ClearExternalCompressorsTable();
  for (char *p, *section = strstr(contents, ANY_HEADING);  section != NULL;  section = p)
  {
    section++;
    p = strstr(section, ANY_HEADING);
    if (p)  *p = '\0';
    if (start_with(section,EXT_HEADING)  &&  AddExternalCompressor(section) != 1)
    {
      //printf("Error in config file %s section:\n%s\n", cfgfile, section);
    }
  }

  free(contents);
  f.close();
#endif
}


/******************************************************************************
** ������ ��������� ������ ****************************************************
******************************************************************************/
static void read_patterns_file(const char *filename, std::vector<std::string> &list) {
  FILE *f = fopen(filename, "r");
  if (!f) return;
  char line[MY_FILENAME_MAX * 4];
  while (fgets(line, sizeof(line), f)) {
    char *p = line;
    while (*p == ' ' || *p == '\t') p++;
    char *e = p + strlen(p);
    while (e > p && (e[-1] == '\n' || e[-1] == '\r' || e[-1] == ' ' || e[-1] == '\t'))
      *--e = '\0';
    if (!*p || *p == '#') continue;
    list.push_back(p);
  }
  fclose(f);
}

void COMMAND::add_exclude(const char *pat) {
  if (!pat || !*pat) return;
  if (pat[0] == '@') read_patterns_file(pat + 1, excludes);
  else excludes.push_back(pat);
}

void COMMAND::add_include(const char *pat) {
  if (!pat || !*pat) return;
  if (pat[0] == '@') read_patterns_file(pat + 1, includes);
  else includes.push_back(pat);
}

COMMAND::COMMAND (int argc, char *argv[])
{
#if defined(FREEARC_WIN) && !defined(FREEARC_LIBRARY)
  // Instead of those ANSI-codepage encoded argv[] strings provide true UTF-8 data!
  WCHAR **argv_w = CommandLineToArgvW (GetCommandLineW(), &argc);
  argv = (char**) malloc ((argc+1) * sizeof(*argv));
  for (int i=0; i<argc; i++)
  {
    argv[i] = (char*) malloc (wcslen (argv_w[i]) * 4 + 1);
    utf16_to_utf8 (argv_w[i], argv[i]);
    argv[i] = (char*) realloc (argv[i], strlen(argv[i]) + 1);
  }
  argv[argc] = NULL;
#endif
// Register external compressors using arc.ini in the same dir as argv[0]
  RegisterExternalCompressors(argv[0]);

  // Default options
  noarcext  = FALSE;
  nooptions = FALSE;
  outpath.setname("");
  workdir.setname("");
  runme.setname("");
  wipeoutdir = FALSE;
  tempdir = FALSE;
  yes = FALSE;
  no  = FALSE;
  silent = 0;
  password = NULL;
#ifdef FREEARC_SFX
  arcname = argv[0];
#ifndef FREEARC_WIN
  static char sfx_exe_path[PATH_MAX];
  ssize_t sfx_len = readlink("/proc/self/exe", sfx_exe_path, sizeof(sfx_exe_path) - 1);
  if (sfx_len > 0) {
    sfx_exe_path[sfx_len] = '\0';
    arcname = sfx_exe_path;
  }
#endif
  cmd     = 'x';

#ifdef FREEARC_INSTALLER
  // Installer by default extracts itself into some temp directory, runs setup.exe and then remove directory's contents
  if (argv[1] == NULL)
  {
      silent = 2;

      // Create unique tempdir
      if (!outpath.create_tempdir()) {
#ifdef FREEARC_GUI
        MessageBoxW (NULL, _T("Error creating temporary directory"), _T("Extraction impossible"), MB_OK | MB_ICONERROR);
#else
        printf("Error creating temporary directory");
#endif
        ok = false;
        return;
      }
      tempdir = TRUE;

      // Run setup.exe from this dir
      runme.setname (outpath, "setup.exe");

      // Delete extracted files afterwards
      wipeoutdir = TRUE;
  }
#endif

  // Parse options
  for (ok=TRUE; ok && *++argv; )
  {
    if (argv[0][0]=='-' || strequ(argv[0],"/?") || strequ(argv[0],"/help"))
    {
           if (strequ(argv[0],"-l"))       cmd = 'l', silent = silent || 2;
      else if (strequ(argv[0],"-v"))       cmd = 'v', silent = silent || 2;
      else if (strequ(argv[0],"-e"))       cmd = 'e', silent = silent || 2;
      else if (strequ(argv[0],"-x"))       cmd = 'x', silent = silent || 2;
      else if (strequ(argv[0],"-t"))       cmd = 't', silent = silent || 2;
      else if (strequ(argv[0],"-p"))       cmd = 'p', silent = silent || 2, to_stdout = TRUE;
      else if (strequ(argv[0],"-so"))      to_stdout = TRUE;
      else if (strequ(argv[0],"-y") || strequ(argv[0],"-o+")) yes = TRUE;
      else if (strequ(argv[0],"-n") || strequ(argv[0],"-o-")) no  = TRUE;
      else if (strequ(argv[0],"-p") && argv[1]) password = *++argv;
      else if (start_with(argv[0],"-p") && argv[0][2]) password = argv[0]+2;
      else if (strequ(argv[0],"-d") && argv[1]) outpath.setname(*++argv);
      else if (start_with(argv[0],"-d"))   outpath.setname(argv[0]+2);
      else if (strequ(argv[0],"-dp") && argv[1]) outpath.setname(*++argv);
      else if (start_with(argv[0],"-dp"))  outpath.setname(argv[0]+3);
      else if (start_with(argv[0],"-w"))   workdir.setname(argv[0]+2);
      else if (start_with(argv[0],"-ap"))  arcprefix = argv[0]+3;
      else if (strequ(argv[0],"-x") && argv[1]) add_exclude(*++argv);
      else if (start_with(argv[0],"-x") && argv[0][2]) add_exclude(argv[0]+2);
      else if (strequ(argv[0],"-i") && argv[1]) add_include(*++argv);
      else if (start_with(argv[0],"-i") && argv[0][2]) add_include(argv[0]+2);
      else if (strequ(argv[0],"-s") || strequ(argv[0],"-s1") || strequ(argv[0],"-q")) silent = 1;
      else if (strequ(argv[0],"-s0"))      silent = 0;
      else if (strequ(argv[0],"-s2"))      silent = 2;
      else if (strequ(argv[0],"--"))       nooptions=TRUE;
      else ok=FALSE;
    }
    else break;
  }

  filenames = argv;            // the rest of arguments are filenames
  if (cmd == 'p') to_stdout = TRUE;
  if (ok) {
    while (*argv) {
      if (argv[0][0] == '@') {
        read_patterns_file(argv[0] + 1, file_patterns);
      } else {
        file_patterns.push_back(argv[0]);
      }
      argv++;
    }
    return;
  }

  // Display help
  char *helpMsg = (char*) malloc_msg(1000+strlen(arcname));
  sprintf (helpMsg,
#ifdef FREEARC_GUI
         HEADER1 NAME HEADER2
#else
         HEADER2
#endif
         "Usage: %s [options] [filenames...]\n"
         "Available options:\n"
#ifndef FREEARC_GUI
         "  -l       - display archive listing\n"
         "  -v       - display verbose archive listing\n"
#endif
         "  -x       - extract files with pathnames (default)\n"
         "  -e       - extract files without pathnames\n"
         "  -t       - test archive integrity\n"
         "  -p       - extract files to standard output (pipe)\n"
         "  -d{Path} - set destination path\n"
         "  -p{Pass} - archive password\n"
         "  -w{Path} - set temporary files directory\n"
         "  -y       - answer Yes on all overwrite queries\n"
         "  -n       - answer No  on all overwrite queries\n"
         "  -s       - silent mode\n"
         "  -x{Pat}  - exclude pattern (@listfile supported)\n"
         "  -i{Pat}  - include pattern (@listfile supported)\n"
         "  --       - no more options\n"
         , drop_dirname(arcname));
#ifdef FREEARC_GUI
  MessageBoxW (NULL, MYFILE(helpMsg).displayname(), _T("Command-line help"), MB_OK | MB_ICONERROR);
#else
  printf("%s", MYFILE(helpMsg).displayname());
#endif

#else
  cmd       = ' ';
  arcname   = NULL;
  to_stdout = FALSE;
  for (ok=TRUE; ok && *++argv; )
  {
    if (argv[0][0]=='-')
    {
      if (strequ(argv[0],"--noarcext"))    noarcext =TRUE;
      else if (strequ(argv[0],"-o+"))      yes      =TRUE;
      else if (strequ(argv[0],"-o-"))      no       =TRUE;
      else if (strequ(argv[0],"-so"))      to_stdout=TRUE;
      else if (strequ(argv[0],"-p") && argv[1]) password = *++argv;
      else if (start_with(argv[0],"-p") && argv[0][2]) password = argv[0]+2;
      else if (start_with(argv[0],"-dp"))  outpath.setname(argv[0]+3);
      else if (start_with(argv[0],"-ap"))  arcprefix = argv[0]+3;
      else if (start_with(argv[0],"-w"))   workdir.setname(argv[0]+2);
      else if (strequ(argv[0],"-s") || strequ(argv[0],"-s1")) silent = 1;
      else if (strequ(argv[0],"-s2"))      silent = 2;
      else if (strequ(argv[0],"-s0"))      silent = 0;
      else if (strequ(argv[0],"-x") && argv[1]) add_exclude(*++argv);
      else if (start_with(argv[0],"-x") && argv[0][2]) add_exclude(argv[0]+2);
      else if (strequ(argv[0],"-i") && argv[1]) add_include(*++argv);
      else if (start_with(argv[0],"-i") && argv[0][2]) add_include(argv[0]+2);
      else if (strequ(argv[0],"--"))       nooptions=TRUE;
      else ok=FALSE;
    }
    else if (cmd==' ')   cmd = argv[0][0], ok = ok && strlen(argv[0])==1;
    else if (!arcname)   arcname = argv[0];
    else break;
  }

  filenames = argv;            // the rest of arguments are filenames
  if (cmd == 'p') to_stdout = TRUE;
  ok = ok && strchr("lvtexp",cmd) && arcname;
  if (ok) {
    while (*argv) {
      if (argv[0][0] == '@') {
        read_patterns_file(argv[0] + 1, file_patterns);
      } else {
        file_patterns.push_back(argv[0]);
      }
      argv++;
    }
    return;
  }
  printf(HEADER2
         "Usage: unarc command [options] archive[.arc] [filenames...]\n"
         "Available commands:\n"
         "  l - display archive listing\n"
         "  v - display verbose archive listing\n"
         "  e - extract files into current directory\n"
         "  x - extract files with pathnames\n"
         "  t - test archive integrity\n"
         "  p - extract files to standard output (pipe)\n"
         "Available options:\n"
         "  -dp{Path}   - set destination path\n"
         "  -ap{Path}   - set path prefix\n"
         "  -w{Path}    - set temporary files directory\n"
         "  -o+         - overwrite existing files\n"
         "  -o-         - don't overwrite existing files\n"
         "  -so         - write extracted files to stdout\n"
         "  -x{Pattern} - exclude files matching pattern (@listfile supported)\n"
         "  -i{Pattern} - include only files matching pattern (@listfile supported)\n"
         "  -s[0..2]    - silent mode\n"
         "  --noarcext  - don't add default extension to archive name\n"
         "  -p{Password}- password for encrypted archive\n"
         "  --          - no more options\n");
#endif
}


// Prepare
void COMMAND::Prepare()
{
  SetTempDir (workdir.filename);
  SetCompressionThreads (GetProcessorsCount());
  SetArchivePassword (password);
}


static bool glob_match_unarc(const char *pat, const char *s) {
  for (;;) {
    if (*pat == '*') {
      pat++;
      if (!*pat) return true;
      for (; *s; s++)
        if (glob_match_unarc(pat, s)) return true;
      return glob_match_unarc(pat, s);
    }
    if (*pat == '?') {
      if (!*s) return false;
      pat++; s++;
      continue;
    }
    if (*pat != *s) return false;
    if (!*pat) return true;
    pat++; s++;
  }
}

static bool match_one_pattern(const std::string &pat, const char *full, const char *base) {
  if (pat.empty()) return false;

  std::string p = pat;
  while (!p.empty() && (p[p.size() - 1] == '/' || p[p.size() - 1] == '\\'))
    p.resize(p.size() - 1);

  if (strequ(full, p.c_str()) || strequ(base, p.c_str()))
    return true;

  std::string prefix = p + "/";
  if (strncmp(full, prefix.c_str(), prefix.size()) == 0)
    return true;

  if (glob_match_unarc(pat.c_str(), full))
    return true;

  if (pat.find('/') == std::string::npos && pat.find('\\') == std::string::npos) {
    if (glob_match_unarc(pat.c_str(), base))
      return true;
  }

  return false;
}

// Accept file matching
BOOL COMMAND::accept_file (DIRECTORY_BLOCK *dirblock, int i)
{
  char fullname_buf[MY_FILENAME_MAX * 4];
  const char *full = dirblock->fullname(i, fullname_buf);
  const char *base = dirblock->name[i];

  // 1. Exclude patterns (-x)
  for (size_t x = 0; x < excludes.size(); x++) {
    if (match_one_pattern(excludes[x], full, base))
      return FALSE;
  }

  // 2. Include patterns (-i)
  if (!includes.empty()) {
    bool inc_matched = false;
    for (size_t inc = 0; inc < includes.size(); inc++) {
      if (match_one_pattern(includes[inc], full, base)) {
        inc_matched = true;
        break;
      }
    }
    if (!inc_matched) return FALSE;
  }

  // 3. Positional patterns
  if (!file_patterns.empty()) {
    bool pos_matched = false;
    for (size_t f = 0; f < file_patterns.size(); f++) {
      if (match_one_pattern(file_patterns[f], full, base)) {
        pos_matched = true;
        break;
      }
    }
    if (!pos_matched) return FALSE;
  }

  return TRUE;
}
