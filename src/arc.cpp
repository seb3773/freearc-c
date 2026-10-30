#include <dirent.h>
#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <stdarg.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
#include <string>
#include <set>
#include <map>
#include <algorithm>
#include <cmath>

#include "Environment.h"
#include "Compression/Compression.h"
extern "C" {
#include "Compression/MM/mmdet.h"
}
#include "WinCompat.h"
#ifndef _WIN32
#include <pthread.h>
#endif
#ifdef _WIN32
#include <process.h>
#endif
#ifdef stat
#undef stat
#endif

#include "ArcStructure.h"
#define aARCHIVE_VERSION   make4byte(0,0,5,9)
#define aNO_COMPRESSION    "storing"
#define aDEFAULT_DIR_COMPRESSION "lzma:mfbt4:d1m"

FILE *g_logfile = NULL;

static void err_msg(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  if (g_logfile) {
    va_list ap2;
    va_copy(ap2, ap);
    vfprintf(g_logfile, fmt, ap2);
    fflush(g_logfile);
    va_end(ap2);
  }
  vfprintf(stderr, fmt, ap);
  va_end(ap);
}

#undef ON_CHECK_FAIL
#define ON_CHECK_FAIL()  do { if (g_logfile) { fputs("\n", g_logfile); fflush(g_logfile); } fprintf(stderr, "\n"); exit(FREEARC_EXIT_ERROR); } while (0)

static const char *g_usage =
  "Usage: arc <command> [options] archive[.arc] [files...]\n"
  "Commands:\n"
  "  a, add, c, create  add files to archive (default)\n"
  "  u, update          update newer files and add new files\n"
  "  f, freshen         freshen existing files in archive (no new files added)\n"
  "  d, delete          delete files/patterns from archive\n"
  "  m, move            move files to archive (delete from disk after archiving)\n"
  "  l, list            display archive listing (delegates to unarc)\n"
  "  v                  display verbose archive listing with Unix attributes\n"
  "  t, test            test archive integrity\n"
  "  e                  extract files without pathnames\n"
  "  x, extract         extract files with pathnames\n"
  "  p                  extract files to standard output (pipe)\n"
  "  s                  convert archive to self-extracting executable (SFX)\n"
  "Options:\n"
  "  -dp<DIR>, --diskpath=<DIR>  base directory on disk\n"
  "  -mt<N>, --multithreaded=<N> number of compression threads\n"
  "  -m METHOD          storing, lzma, tor, ppmd, grzip, tta\n"
  "                     levels: -m0..-m5, -mx, -m4x, -m5x\n"
  "                     chains: -m exe+lzma, -m rep+lzma, -m dict+ppmd, etc.\n"
  "  -bcj               activate x86 executable filter (alias: -exe)\n"
  "  -rep               activate repetition filter (rep:64m)\n"
  "  -delta             activate delta filter\n"
  "  -dict              activate dictionary filter\n"
  "  -lzp               activate LZP filter\n"
  "  -mm                activate multimedia filter\n"
  "  -v<size>           create multi-volume archive (e.g. -v10m, -v1440k)\n"
  "  -s                 solid archive (default)\n"
  "  -s-                one data block per file (nonsolid)\n"
  "  -s<size>           solid block size (e.g. -s64m)\n"
  "  -s<N>f             solid block by file count (e.g. -s10f)\n"
  "  -r                 recurse directories (default)\n"
  "  -r-                do not recurse\n"
  "  -ep                store basenames only\n"
  "  -ep1               strip the first path component\n"
  "  -sfx[=STUB]        create self-extracting executable (.sfx/.run)\n"
  "  -ap<path>          set archive path prefix\n"
  "  -z TEXT            archive comment (or -zFILE to read a file)\n"
  "  -x PATTERN         exclude files matching pattern (@listfile supported)\n"
  "  -i PATTERN         include only files matching pattern (@listfile supported)\n"
  "  -so                write extracted files to stdout\n"
  "  -p PASS            encrypt data blocks\n"
  "  -hp PASS           encrypt headers and data\n"
  "  -ae ALG            aes (default), aes-128, aes-256, blowfish, twofish, serpent\n"
  "  -ld<mem>           limit decompression memory (e.g. -ld64m, --LimitDecompMem)\n"
  "  -lc<mem>           limit compression memory (e.g. -lc128m, --LimitCompMem)\n"
  "  --logfile=<file>   write operation log to specified file (-lo)\n"
  "  -q                 quiet\n"
  "  --noarcext         do not append .arc to the archive name\n"
  "  @LIST              read file names from LIST (one path per line)\n";

static int g_quiet = 0;

static void msg(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  if (g_logfile) {
    va_list ap2;
    va_copy(ap2, ap);
    vfprintf(g_logfile, fmt, ap2);
    fflush(g_logfile);
    va_end(ap2);
  }
  if (!g_quiet) {
    vprintf(fmt, ap);
  }
  va_end(ap);
}

struct FileRec {
  std::string path;
  std::string dir;
  std::string name;
  uint64 size;
  uint32 mtime;
  uint8  isdir;
  uint32 crc;
  uint32 mode;
  bool   is_symlink;
  std::string symlink_target;
  std::string datatype;
};

struct BlockInfo {
  int        type;
  char       compressor[MAX_METHOD_STRLEN];
  FILESIZE   pos;
  FILESIZE   origsize;
  FILESIZE   compsize;
  uint32     crc;
};

class ByteWriter {
public:
  std::vector<char> buf;
  size_t pos;

  ByteWriter() : pos(0) {}

  size_t size() const { return pos; }
  void *data() { return pos ? &buf[0] : NULL; }

  void ensure(size_t n) {
    if (pos + n > buf.size())
      buf.resize(pos + n + 64);
  }

  void write_raw(const void *p, size_t n) {
    ensure(n);
    memcpy(&buf[pos], p, n);
    pos += n;
  }

  void write_u8(uint8 v)  { write_raw(&v, 1); }
  void write_u32(uint32 v) { write_raw(&v, 4); }
  void write_u64(uint64 v) { write_raw(&v, 8); }

  void writeInteger(uint64 x) {
    if (x < 128ull) {
      ensure(4);
      uint32 v = (uint32)(x * 2u + 0u);
      memcpy(&buf[pos], &v, 4);
      pos += 1;
    } else if (x < 128ull * 128ull) {
      ensure(4);
      uint32 v = (uint32)(x * 4u + 1u);
      memcpy(&buf[pos], &v, 4);
      pos += 2;
    } else if (x < 128ull * 128ull * 128ull) {
      ensure(4);
      uint32 v = (uint32)(x * 8u + 3u);
      memcpy(&buf[pos], &v, 4);
      pos += 3;
    } else if (x < 128ull * 128ull * 128ull * 128ull) {
      ensure(4);
      uint32 v = (uint32)(x * 16u + 7u);
      memcpy(&buf[pos], &v, 4);
      pos += 4;
    } else if (x < (1ull << 35)) {
      ensure(8);
      uint64 v = (x << 5) + 15ull;
      memcpy(&buf[pos], &v, 8);
      pos += 5;
    } else if (x < (1ull << 42)) {
      ensure(8);
      uint64 v = (x << 6) + 31ull;
      memcpy(&buf[pos], &v, 8);
      pos += 6;
    } else if (x < (1ull << 49)) {
      ensure(8);
      uint64 v = (x << 7) + 63ull;
      memcpy(&buf[pos], &v, 8);
      pos += 7;
    } else if (x < (1ull << 56)) {
      ensure(8);
      uint64 v = (x << 8) + 127ull;
      memcpy(&buf[pos], &v, 8);
      pos += 8;
    } else {
      ensure(9);
      buf[pos] = (char)255;
      memcpy(&buf[pos + 1], &x, 8);
      pos += 9;
    }
  }

  void writeString(const char *s) {
    write_raw(s, strlen(s) + 1);
  }
};

static uint64 parse_vol_size(const char *s) {
  if (!s || !*s) return 0;
  if (*s == '=') s++;
  char *endp = NULL;
  unsigned long long val = strtoull(s, &endp, 10);
  if (!endp || endp == s) return 0;
  while (*endp == ' ') endp++;
  if (*endp == 'k' || *endp == 'K') {
    val *= 1024ULL;
  } else if (*endp == 'm' || *endp == 'M') {
    val *= 1024ULL * 1024ULL;
  } else if (*endp == 'g' || *endp == 'G') {
    val *= 1024ULL * 1024ULL * 1024ULL;
  } else if (*endp == 'b' || *endp == 'B' || *endp == '\0') {
    // bytes
  }
  return (uint64)val;
}

static std::string clean_option_path(const std::string &raw) {
  std::string s = raw;
  while (!s.empty() && (s[0] == ' ' || s[0] == '\t' || s[0] == '\r' || s[0] == '\n'))
    s.erase(s.begin());
  while (!s.empty() && (s[s.size() - 1] == ' ' || s[s.size() - 1] == '\t' || s[s.size() - 1] == '\r' || s[s.size() - 1] == '\n'))
    s.resize(s.size() - 1);

  bool changed = true;
  while (changed && s.size() >= 2) {
    changed = false;
    if ((s[0] == '"' && s[s.size() - 1] == '"') ||
        (s[0] == '\'' && s[s.size() - 1] == '\'')) {
      s = s.substr(1, s.size() - 2);
      changed = true;
    } else if (s[0] == '{' && s[s.size() - 1] == '}') {
      s = s.substr(1, s.size() - 2);
      changed = true;
    }
  }
  while (s.size() > 1 && (s[s.size() - 1] == '/' || s[s.size() - 1] == '\\')) {
#ifdef _WIN32
    if (s.size() == 3 && isalpha((unsigned char)s[0]) && s[1] == ':') break;
#endif
    s.resize(s.size() - 1);
  }
  return s;
}

static bool is_diskpath_opt(const char *a, std::string &out_val, bool &need_next_arg) {
  need_next_arg = false;
  out_val.clear();
  if (strcasecmp(a, "-dp") == 0 || strcasecmp(a, "--diskpath") == 0) {
    need_next_arg = true;
    return true;
  }
  if (strncasecmp(a, "-dp=", 4) == 0) {
    out_val = a + 4;
    return true;
  }
  if (strncasecmp(a, "-dp", 3) == 0 && a[3] != '\0') {
    out_val = a + 3;
    return true;
  }
  if (strncasecmp(a, "--diskpath=", 11) == 0) {
    out_val = a + 11;
    return true;
  }
  return false;
}

static void create_dir_recursive(const std::string &path) {
  if (path.empty()) return;
  char buf[PATH_MAX];
  snprintf(buf, sizeof(buf), "%s", path.c_str());
  for (char *p = buf + 1; *p; p++) {
    if (*p == '/' || *p == '\\') {
      if (p == buf + 2 && buf[1] == ':') continue; // Skip Windows drive root like "C:\"
      char save = *p;
      *p = '\0';
#ifdef _WIN32
      mkdir(buf);
#else
      mkdir(buf, 0777);
#endif
      *p = save;
    }
  }
#ifdef _WIN32
  mkdir(buf);
#else
  mkdir(buf, 0777);
#endif
}

static void ensure_parent_dirs(const std::string &filepath) {
  std::string s = clean_option_path(filepath);
  size_t pos = s.find_last_of("/\\");
  if (pos != std::string::npos && pos > 0) {
    std::string dir = s.substr(0, pos);
    if (dir.size() == 2 && dir[1] == ':') return;
    create_dir_recursive(dir);
  }
}

static inline bool is_limit_decomp_opt(const char *a, std::string &out_val, bool &need_next_arg) {
  need_next_arg = false;
  out_val.clear();
  if (strcasecmp(a, "-ld") == 0 || strcasecmp(a, "--limitdecompmem") == 0) {
    need_next_arg = true;
    return true;
  }
  if (strncasecmp(a, "-ld=", 4) == 0) {
    out_val = a + 4;
    return true;
  }
  if (strncasecmp(a, "-ld", 3) == 0 && a[3] != '\0') {
    out_val = a + 3;
    return true;
  }
  if (strncasecmp(a, "--limitdecompmem=", 17) == 0) {
    out_val = a + 17;
    return true;
  }
  return false;
}

static inline bool is_limit_comp_opt(const char *a, std::string &out_val, bool &need_next_arg) {
  need_next_arg = false;
  out_val.clear();
  if (strcasecmp(a, "-lc") == 0 || strcasecmp(a, "--limitcompmem") == 0) {
    need_next_arg = true;
    return true;
  }
  if (strncasecmp(a, "-lc=", 4) == 0) {
    out_val = a + 4;
    return true;
  }
  if (strncasecmp(a, "-lc", 3) == 0 && a[3] != '\0') {
    out_val = a + 3;
    return true;
  }
  if (strncasecmp(a, "--limitcompmem=", 15) == 0) {
    out_val = a + 15;
    return true;
  }
  return false;
}

static inline bool is_logfile_opt(const char *a, std::string &out_val, bool &need_next_arg) {
  need_next_arg = false;
  out_val.clear();
  if (strcasecmp(a, "-lo") == 0 || strcasecmp(a, "--logfile") == 0) {
    need_next_arg = true;
    return true;
  }
  if (strncasecmp(a, "-lo=", 4) == 0) {
    out_val = a + 4;
    return true;
  }
  if (strncasecmp(a, "-lo", 3) == 0 && a[3] != '\0') {
    out_val = a + 3;
    return true;
  }
  if (strncasecmp(a, "--logfile=", 10) == 0) {
    out_val = a + 10;
    return true;
  }
  return false;
}

static inline bool is_multithread_opt(const char *a, std::string &out_val, bool &need_next_arg) {
  need_next_arg = false;
  out_val.clear();
  if (strcasecmp(a, "-mt") == 0 || strcasecmp(a, "--multithreaded") == 0) {
    need_next_arg = true;
    return true;
  }
  if (strncasecmp(a, "-mt=", 4) == 0) {
    out_val = a + 4;
    return true;
  }
  if (strncasecmp(a, "-mt", 3) == 0 && a[3] >= '0' && a[3] <= '9') {
    out_val = a + 3;
    return true;
  }
  if (strncasecmp(a, "--multithreaded=", 16) == 0) {
    out_val = a + 16;
    return true;
  }
  return false;
}

static uint64 parse_mem_limit(const char *s) {
  if (!s || !*s) return 0;
  if (*s == '=') s++;
  while (*s == ' ' || *s == '\t') s++;
  char *endp = NULL;
  unsigned long long val = strtoull(s, &endp, 10);
  if (!endp || endp == s) return 0;
  while (*endp == ' ') endp++;
  if (*endp == 'k' || *endp == 'K') {
    val *= 1024ULL;
  } else if (*endp == 'm' || *endp == 'M') {
    val *= 1024ULL * 1024ULL;
  } else if (*endp == 'g' || *endp == 'G') {
    val *= 1024ULL * 1024ULL * 1024ULL;
  } else if (*endp == 'b' || *endp == 'B') {
    // bytes
  } else if (*endp == '\0') {
    if (val <= 65536ULL) {
      val *= 1024ULL * 1024ULL;
    }
  }
  return (uint64)val;
}

static int parse_threads_val(const char *s) {
  if (!s || !*s) return 0;
  if (*s == '=') s++;
  int v = atoi(s);
  return v > 0 ? v : 0;
}

static void open_logfile(const std::string &path) {
  if (path.empty()) return;
  if (g_logfile) {
    fclose(g_logfile);
    g_logfile = NULL;
  }
  std::string cleaned = clean_option_path(path);
  ensure_parent_dirs(cleaned);
  g_logfile = fopen(cleaned.c_str(), "a+");
  if (!g_logfile) {
    err_msg("WARNING: cannot open logfile '%s': %s\n", cleaned.c_str(), strerror(errno));
  }
}

static void close_logfile() {
  if (g_logfile) {
    fclose(g_logfile);
    g_logfile = NULL;
  }
}

static std::string apply_mem_limits(const std::string &method_str, uint64 limit_decomp, uint64 limit_comp) {
  if (limit_decomp == 0 && limit_comp == 0) return method_str;
  std::string result;
  const char *p = method_str.c_str();
  while (*p) {
    const char *plus = strchr(p, '+');
    std::string part = plus ? std::string(p, plus - p) : std::string(p);
    char buf[MAX_METHOD_STRLEN];
    strncopy(buf, (char*)part.c_str(), sizeof(buf));
    if (limit_decomp > 0) {
      char limited[MAX_METHOD_STRLEN];
      int rc = LimitDecompressionMem(buf, (MemSize)limit_decomp, limited);
      if (rc == FREEARC_OK) {
        strncopy(buf, limited, sizeof(buf));
      }
    }
    if (limit_comp > 0) {
      char limited[MAX_METHOD_STRLEN];
      int rc = LimitCompressionMem(buf, (MemSize)limit_comp, limited);
      if (rc == FREEARC_OK) {
        strncopy(buf, limited, sizeof(buf));
      }
    }
    if (!result.empty()) result += '+';
    result += buf;
    if (!plus) break;
    p = plus + 1;
  }
  return result;
}

static FILESIZE haskell_dict_limit(FILESIZE orig) {
  if (orig <= 0) return 0;
  uint64 padded = (uint64)orig + (uint64)orig / 100 + 512;
  if (padded > (uint64)(MemSize)-1)
    padded = (uint64)(MemSize)-1;
  MemSize mem = (MemSize)padded;
  if (mem >= 4096 * kb)
    return (FILESIZE)roundUp(mem, mb);
  return (FILESIZE)roundUp(mem, kb);
}

static std::string canonize_method_chain(const std::string &method_str, FILESIZE limit_dict) {
  std::string result;
  const char *p = method_str.c_str();
  while (*p) {
    const char *plus = strchr(p, '+');
    std::string part = plus ? std::string(p, plus - p) : std::string(p);
    char buf[MAX_METHOD_STRLEN];
    strncopy(buf, (char *)part.c_str(), sizeof(buf));
    if (limit_dict > 0) {
      char limited[MAX_METHOD_STRLEN];
      if (LimitDictionary(buf, (MemSize)limit_dict, limited) == FREEARC_OK)
        strncopy(buf, limited, sizeof(buf));
    }
    char canon[MAX_METHOD_STRLEN];
    if (CanonizeCompressionMethod(buf, canon) == FREEARC_OK)
      strncopy(buf, canon, sizeof(buf));
    if (!result.empty()) result += '+';
    result += buf;
    if (!plus) break;
    p = plus + 1;
  }
  return result.empty() ? method_str : result;
}

static std::string purify_stored_method(const std::string &method_str) {
  std::string result;
  const char *p = method_str.c_str();
  while (*p) {
    const char *plus = strchr(p, '+');
    std::string part = plus ? std::string(p, plus - p) : std::string(p);
    if (part.size() >= 4 && part.compare(0, 4, "rep:") == 0) {
      std::string cleaned;
      cleaned.reserve(part.size());
      for (size_t i = 0; i < part.size(); ) {
        if (part[i] == ':' && i + 2 < part.size() && part[i + 1] == 'c' &&
            isdigit((unsigned char)part[i + 2])) {
          i += 2;
          while (i < part.size() && isdigit((unsigned char)part[i])) i++;
          continue;
        }
        cleaned += part[i++];
      }
      part.swap(cleaned);
    }
    if (part.size() >= 6 && part.compare(0, 6, "grzip:") == 0) {
      std::string cleaned;
      cleaned.reserve(part.size());
      for (size_t i = 0; i < part.size(); ) {
        if (part[i] == ':' && i + 1 < part.size() && part[i + 1] == 'a' &&
            (i + 2 == part.size() || part[i + 2] == ':')) {
          i += 2;
          continue;
        }
        cleaned += part[i++];
      }
      part.swap(cleaned);
    }
    if (!result.empty()) result += '+';
    result += part;
    if (!plus) break;
    p = plus + 1;
  }
  return result.empty() ? method_str : result;
}

static bool is_absolute_path(const std::string &path) {
  if (path.empty()) return false;
  if (path[0] == '/' || path[0] == '\\') return true;
#ifdef _WIN32
  if (path.size() >= 2 && isalpha((unsigned char)path[0]) && path[1] == ':') return true;
#endif
  return false;
}

static std::string join_fspath(const std::string &base, const std::string &rel) {
  if (base.empty()) return rel;
  if (rel.empty()) return base;
  if (is_absolute_path(rel)) return rel;
  char last = base[base.size() - 1];
  if (last == '/' || last == '\\')
    return base + rel;
  return base + "/" + rel;
}

static char parse_arc_cmd(const char *s) {
  if (strcasecmp(s, "a") == 0 || strcasecmp(s, "add") == 0) return 'a';
  if (strcasecmp(s, "c") == 0 || strcasecmp(s, "create") == 0) return 'a';
  if (strcasecmp(s, "u") == 0 || strcasecmp(s, "update") == 0) return 'u';
  if (strcasecmp(s, "f") == 0 || strcasecmp(s, "freshen") == 0) return 'f';
  if (strcasecmp(s, "d") == 0 || strcasecmp(s, "delete") == 0) return 'd';
  if (strcasecmp(s, "m") == 0 || strcasecmp(s, "move") == 0) return 'm';
  if (strcasecmp(s, "s") == 0) return 's';
  return 0;
}

static bool is_unpacker_cmd(const char *s) {
  if (strcasecmp(s, "l") == 0 || strcasecmp(s, "list") == 0) return true;
  if (strcasecmp(s, "v") == 0 || strcasecmp(s, "view") == 0) return true;
  if (strcasecmp(s, "t") == 0 || strcasecmp(s, "test") == 0) return true;
  if (strcasecmp(s, "e") == 0) return true;
  if (strcasecmp(s, "x") == 0 || strcasecmp(s, "extract") == 0) return true;
  if (strcasecmp(s, "p") == 0) return true;
  return false;
}

struct IArchiveWriter {
  virtual bool write(const void *buf, size_t n) = 0;
  virtual FILESIZE tell() const = 0;
  virtual ~IArchiveWriter() {}
};

class VolumeWriter : public IArchiveWriter {
public:
  std::string base_name;
  uint64 vol_size;
  uint64 total_written;
  uint64 cur_vol_written;
  int cur_vol_num;
  FILE *cur_f;
  std::vector<std::string> volume_names;

  VolumeWriter(const std::string &base, uint64 v_size)
    : base_name(base), vol_size(v_size), total_written(0), cur_vol_written(0), cur_vol_num(0), cur_f(NULL)
  {}

  ~VolumeWriter() {
    close();
  }

  std::string get_vol_name(int num) {
    if (vol_size == 0) return base_name;
    char buf[MY_FILENAME_MAX * 4];
    snprintf(buf, sizeof(buf), "%s.%03d", base_name.c_str(), num);
    return std::string(buf);
  }

  bool open_next_vol() {
    if (cur_f) {
      fclose(cur_f);
      cur_f = NULL;
    }
    cur_vol_num++;
    std::string vname = get_vol_name(cur_vol_num);
    ensure_parent_dirs(vname);
    cur_f = fopen(vname.c_str(), "wb");
    if (!cur_f) {
      err_msg("ERROR: can't create volume %s (%s)\n", vname.c_str(), strerror(errno));
      return false;
    }
    volume_names.push_back(vname);
    cur_vol_written = 0;
    if (vol_size > 0) {
      msg("Creating volume %s...\n", vname.c_str());
    }
    return true;
  }

  bool open() {
    return open_next_vol();
  }

  FILESIZE tell() const {
    return (FILESIZE)total_written;
  }

  bool write(const void *buf, size_t n) {
    const char *p = (const char *)buf;
    while (n > 0) {
      if (!cur_f && !open_next_vol()) return false;

      size_t to_write = n;
      if (vol_size > 0) {
        if (cur_vol_written >= vol_size) {
          if (!open_next_vol()) return false;
        }
        uint64 space_in_vol = vol_size - cur_vol_written;
        if ((uint64)to_write > space_in_vol)
          to_write = (size_t)space_in_vol;
      }

      size_t written = fwrite(p, 1, to_write, cur_f);
      if (written != to_write) {
        err_msg("ERROR: volume write failed\n");
        return false;
      }
      cur_vol_written += written;
      total_written += written;
      p += written;
      n -= written;
    }
    return true;
  }

  void close() {
    if (cur_f) {
      fclose(cur_f);
      cur_f = NULL;
    }
  }
};

struct MemInFileOut {
  const char   *in;
  int           in_left;
  VolumeWriter *out;
  uint64        written;
};

struct SolidInFileOut {
  FileRec        *files;
  int             nfiles;
  int             idx;
  int             end;
  FILE           *cur;
  uint64          left_in_file;
  uint            file_crc;
  IArchiveWriter *out;
  uint64          written;
};

static int mem_callback(const char *what, void *data, int size, void *aux) {
  MemInFileOut *c = (MemInFileOut *)aux;
  if (strequ(what, "read")) {
    int n = c->in_left < size ? c->in_left : size;
    if (n > 0) {
      memcpy(data, c->in, n);
      c->in += n;
      c->in_left -= n;
    }
    return n;
  }
  if (strequ(what, "write")) {
    if (size > 0 && !c->out->write(data, (size_t)size))
      return FREEARC_ERRCODE_WRITE;
    c->written += (uint64)size;
    return size;
  }
  return 0;
}

static int open_next_data_file(SolidInFileOut *c) {
  while (c->idx < c->end) {
    FileRec &f = c->files[c->idx];
    if (f.isdir || f.size == 0 || f.is_symlink) {
      f.crc = 0;
      c->idx++;
      continue;
    }
    c->cur = fopen(f.path.c_str(), "rb");
    CHECK(c->cur, (s, "ERROR: can't open file %s", f.path.c_str()));
    c->left_in_file = f.size;
    c->file_crc = INIT_CRC;
    return 1;
  }
  c->cur = NULL;
  return 0;
}

static int solid_callback(const char *what, void *data, int size, void *aux) {
  SolidInFileOut *c = (SolidInFileOut *)aux;
  if (strequ(what, "read")) {
    int got = 0;
    char *dst = (char *)data;
    while (got < size) {
      if (!c->cur && !open_next_data_file(c))
        break;
      size_t want = (size_t)(c->left_in_file < (uint64)(size - got) ? c->left_in_file : (uint64)(size - got));
      size_t n = want ? fread(dst + got, 1, want, c->cur) : 0;
      CHECK(n == want, (s, "ERROR: file read failed"));
      c->file_crc = UpdateCRC(dst + got, (uint)n, c->file_crc);
      c->left_in_file -= n;
      got += (int)n;
      if (c->left_in_file == 0) {
        c->files[c->idx].crc = c->file_crc ^ INIT_CRC;
        fclose(c->cur);
        c->cur = NULL;
        c->idx++;
      }
    }
    return got;
  }
  if (strequ(what, "write")) {
    if (size > 0 && !c->out->write(data, (size_t)size))
      return FREEARC_ERRCODE_WRITE;
    c->written += (uint64)size;
    return size;
  }
  return 0;
}

/* --- Asynchronous I/O Pipeline primitives (§2.3) --- */

#ifdef _WIN32
struct ArcMutex {
  CRITICAL_SECTION cs;
  ArcMutex() { InitializeCriticalSection(&cs); }
  ~ArcMutex() { DeleteCriticalSection(&cs); }
  void lock() { EnterCriticalSection(&cs); }
  void unlock() { LeaveCriticalSection(&cs); }
};
struct ArcCond {
  CONDITION_VARIABLE cv;
  ArcCond() { InitializeConditionVariable(&cv); }
  void wait(ArcMutex &m) { SleepConditionVariableCS(&cv, &m.cs, INFINITE); }
  void signal() { WakeConditionVariable(&cv); }
  void broadcast() { WakeAllConditionVariable(&cv); }
};
typedef HANDLE ArcThread;
typedef DWORD (WINAPI *ArcThreadFunc)(void*);
#define ARC_THREAD_CALL DWORD WINAPI
#define ARC_THREAD_RET DWORD
#define ARC_THREAD_RET_VAL 0
inline ArcThread arc_spawn_thread(ArcThreadFunc fn, void *arg) {
  return CreateThread(NULL, 0, fn, arg, 0, NULL);
}
inline void arc_wait_thread(ArcThread th) {
  if (th) {
    WaitForSingleObject(th, INFINITE);
    CloseHandle(th);
  }
}
#else
struct ArcMutex {
  pthread_mutex_t m;
  ArcMutex() { pthread_mutex_init(&m, NULL); }
  ~ArcMutex() { pthread_mutex_destroy(&m); }
  void lock() { pthread_mutex_lock(&m); }
  void unlock() { pthread_mutex_unlock(&m); }
};
struct ArcCond {
  pthread_cond_t cv;
  ArcCond() { pthread_cond_init(&cv, NULL); }
  ~ArcCond() { pthread_cond_destroy(&cv); }
  void wait(ArcMutex &m) { pthread_cond_wait(&cv, &m.m); }
  void signal() { pthread_cond_signal(&cv); }
  void broadcast() { pthread_cond_broadcast(&cv); }
};
typedef pthread_t ArcThread;
typedef void* (*ArcThreadFunc)(void*);
#define ARC_THREAD_CALL void*
#define ARC_THREAD_RET void*
#define ARC_THREAD_RET_VAL NULL
inline ArcThread arc_spawn_thread(ArcThreadFunc fn, void *arg) {
  pthread_t t = 0;
  pthread_create(&t, NULL, fn, arg);
  return t;
}
inline void arc_wait_thread(ArcThread th) {
  if (th) {
    pthread_join(th, NULL);
  }
}
#endif

struct ArcChunk {
  char  *data;
  size_t capacity;
  size_t size;
  size_t pos;
  bool   is_eof;

  ArcChunk(size_t cap = 0) : data(NULL), capacity(0), size(0), pos(0), is_eof(false) {
    if (cap > 0) {
      data = (char*)malloc(cap);
      capacity = cap;
    }
  }
  ~ArcChunk() {
    if (data) free(data);
  }
  void ensure_capacity(size_t need) {
    if (need > capacity) {
      size_t new_cap = need < capacity * 2 ? capacity * 2 : need;
      data = (char*)realloc(data, new_cap);
      capacity = new_cap;
    }
  }
};

class ArcChunkQueue {
  std::vector<ArcChunk*> items;
  size_t   max_items;
  size_t   head;
  size_t   tail;
  size_t   count;
  bool     stopped;
  ArcMutex mutex;
  ArcCond  not_empty;
  ArcCond  not_full;

public:
  ArcChunkQueue(size_t max_size = 4)
    : items(max_size, (ArcChunk*)NULL), max_items(max_size), head(0), tail(0), count(0), stopped(false) {}

  void push(ArcChunk *chunk) {
    mutex.lock();
    while (count == max_items && !stopped) {
      not_full.wait(mutex);
    }
    if (stopped) {
      mutex.unlock();
      delete chunk;
      return;
    }
    items[tail] = chunk;
    tail = (tail + 1) % max_items;
    count++;
    not_empty.signal();
    mutex.unlock();
  }

  ArcChunk* pop() {
    mutex.lock();
    while (count == 0 && !stopped) {
      not_empty.wait(mutex);
    }
    if (count == 0 && stopped) {
      mutex.unlock();
      return NULL;
    }
    ArcChunk *item = items[head];
    head = (head + 1) % max_items;
    count--;
    not_full.signal();
    mutex.unlock();
    return item;
  }

  void stop() {
    mutex.lock();
    stopped = true;
    not_empty.broadcast();
    not_full.broadcast();
    mutex.unlock();
  }

  void clear_and_delete() {
    mutex.lock();
    stopped = true;
    while (count > 0) {
      delete items[head];
      head = (head + 1) % max_items;
      count--;
    }
    mutex.unlock();
  }
};

struct PipeReaderContext {
  FileRec       *files;
  int            idx;
  int            end;
  ArcChunkQueue *in_queue;
  ArcChunkQueue *free_in_chunks;
  volatile int   error_code;
  volatile bool  done;
};

static ARC_THREAD_CALL pipe_reader_func(void *arg) {
  PipeReaderContext *ctx = (PipeReaderContext*)arg;
  const size_t CHUNK_SIZE = 1024 * 1024; // 1 MB
  while (ctx->idx < ctx->end && ctx->error_code == 0) {
    FileRec &f = ctx->files[ctx->idx];
    if (f.isdir || f.size == 0 || f.is_symlink) {
      f.crc = 0;
      ctx->idx++;
      continue;
    }
    FILE *fp = fopen(f.path.c_str(), "rb");
    if (!fp) {
      ctx->error_code = FREEARC_ERRCODE_READ;
      ctx->in_queue->stop();
      break;
    }
    uint64 left = f.size;
    uint crc = INIT_CRC;
    while (left > 0 && ctx->error_code == 0) {
      ArcChunk *chunk = ctx->free_in_chunks->pop();
      if (!chunk) break;
      chunk->ensure_capacity(CHUNK_SIZE);
      size_t want = (size_t)(left < (uint64)CHUNK_SIZE ? left : (uint64)CHUNK_SIZE);
      size_t n = fread(chunk->data, 1, want, fp);
      if (n != want) {
        ctx->error_code = FREEARC_ERRCODE_READ;
        ctx->free_in_chunks->push(chunk);
        ctx->in_queue->stop();
        break;
      }
      crc = UpdateCRC(chunk->data, (uint)n, crc);
      left -= n;
      chunk->size = n;
      chunk->pos = 0;
      chunk->is_eof = false;
      ctx->in_queue->push(chunk);
    }
    fclose(fp);
    if (ctx->error_code != 0) break;
    f.crc = crc ^ INIT_CRC;
    ctx->idx++;
  }
  ArcChunk *eof_chunk = new ArcChunk();
  eof_chunk->is_eof = true;
  eof_chunk->size = 0;
  ctx->in_queue->push(eof_chunk);
  ctx->done = true;
  return ARC_THREAD_RET_VAL;
}

struct PipeWriterContext {
  IArchiveWriter *out;
  ArcChunkQueue  *out_queue;
  ArcChunkQueue  *free_out_chunks;
  uint64          written;
  volatile int    error_code;
  volatile bool   done;
};

static ARC_THREAD_CALL pipe_writer_func(void *arg) {
  PipeWriterContext *ctx = (PipeWriterContext*)arg;
  while (true) {
    ArcChunk *chunk = ctx->out_queue->pop();
    if (!chunk) break;
    if (chunk->is_eof) {
      delete chunk;
      break;
    }
    if (ctx->error_code == 0 && chunk->size > 0) {
      if (!ctx->out->write(chunk->data, chunk->size)) {
        ctx->error_code = FREEARC_ERRCODE_WRITE;
        ctx->out_queue->stop();
      } else {
        ctx->written += chunk->size;
      }
    }
    ctx->free_out_chunks->push(chunk);
  }
  ctx->done = true;
  return ARC_THREAD_RET_VAL;
}

struct PipelinedCallbackState {
  PipeReaderContext *reader_ctx;
  PipeWriterContext *writer_ctx;
  ArcChunk          *cur_in_chunk;
  bool               in_eof;
};

static int pipelined_solid_callback(const char *what, void *data, int size, void *aux) {
  PipelinedCallbackState *s = (PipelinedCallbackState*)aux;
  if (strequ(what, "read")) {
    if (s->in_eof) return 0;
    int got = 0;
    char *dst = (char*)data;
    while (got < size) {
      if (s->reader_ctx->error_code != 0)
        return s->reader_ctx->error_code;
      if (!s->cur_in_chunk) {
        s->cur_in_chunk = s->reader_ctx->in_queue->pop();
        if (!s->cur_in_chunk || s->cur_in_chunk->is_eof) {
          s->in_eof = true;
          if (s->cur_in_chunk) {
            delete s->cur_in_chunk;
            s->cur_in_chunk = NULL;
          }
          break; // EOF
        }
      }
      size_t avail = s->cur_in_chunk->size - s->cur_in_chunk->pos;
      size_t want = (size_t)(size - got);
      size_t take = avail < want ? avail : want;
      if (take > 0) {
        memcpy(dst + got, s->cur_in_chunk->data + s->cur_in_chunk->pos, take);
        s->cur_in_chunk->pos += take;
        got += (int)take;
      }
      if (s->cur_in_chunk->pos >= s->cur_in_chunk->size) {
        s->reader_ctx->free_in_chunks->push(s->cur_in_chunk);
        s->cur_in_chunk = NULL;
      }
    }
    return got;
  }
  if (strequ(what, "write")) {
    if (s->writer_ctx->error_code != 0)
      return s->writer_ctx->error_code;
    if (size <= 0) return 0;
    ArcChunk *chunk = s->writer_ctx->free_out_chunks->pop();
    if (!chunk) return FREEARC_ERRCODE_NOT_ENOUGH_MEMORY;
    chunk->ensure_capacity((size_t)size);
    memcpy(chunk->data, data, (size_t)size);
    chunk->size = (size_t)size;
    chunk->pos = 0;
    chunk->is_eof = false;
    s->writer_ctx->out_queue->push(chunk);
    return size;
  }
  return 0;
}

static void split_path(const char *path, std::string &dir, std::string &name) {
  while (*path == '/') path++;
  const char *slash = strrchr(path, '/');
  if (!slash) {
    dir.clear();
    name = path;
    return;
  }
  dir.assign(path, slash - path);
  name = slash + 1;
}

static std::string map_one_method(const char *m) {
  if (!m || !*m || strequ(m, "0") || strequ(m, "storing"))
    return "storing";
  if (strequ(m, "1"))
    return "tor:4";
  if (strequ(m, "2"))
    return "tor:6";
  if (strequ(m, "3"))
    return "lzma:96m:fast:32:mc4";
  if (strequ(m, "lzma"))
    return "lzma:8m";
  if (strequ(m, "tor") || strequ(m, "tornado"))
    return "tor:3";
  if (strequ(m, "ppmd"))
    return "ppmd:8:1m";
  if (strequ(m, "bcj") || strequ(m, "exe"))
    return "exe";
  if (strequ(m, "rep"))
    return "rep:64m";
  if (strequ(m, "delta"))
    return "delta";
  if (strequ(m, "dict"))
    return "dict";
  if (strequ(m, "lzp"))
    return "lzp";
  if (strequ(m, "mm"))
    return "mm";
  if (strequ(m, "grzip"))
    return "grzip:a";
  if (strequ(m, "tta"))
    return "tta";
  return m;
}

static std::string map_enc_alg(const char *a) {
  if (!a || !*a) return "aes-256/ctr";
  if (strequ(a, "aes") || strequ(a, "aes-256")) return "aes-256/ctr";
  if (strequ(a, "aes-128")) return "aes-128/ctr";
  if (strequ(a, "aes-192")) return "aes-192/ctr";
  if (strequ(a, "blowfish")) return "blowfish/ctr";
  if (strequ(a, "twofish")) return "twofish/ctr";
  if (strequ(a, "serpent")) return "serpent/ctr";
  return a;
}

static void wrap_encryption(const char *base, const char *password, const char *algorithm,
                            char *compress_m, char *store_m) {
  if (!password || !*password) {
    strncopy(compress_m, (char *)base, MAX_METHOD_STRLEN);
    strncopy(store_m, (char *)base, MAX_METHOD_STRLEN);
    return;
  }
  char enc[MAX_METHOD_STRLEN];
  std::string alg = map_enc_alg(algorithm);
  int rc = GenerateEncryptionMethod((char *)alg.c_str(), (char *)password, enc);
  CHECK(rc >= 0, (s, "ERROR: encryption setup failed (%d) algorithm \"%s\"", rc, algorithm ? algorithm : "default"));
  char stored_enc[MAX_METHOD_STRLEN];
  HideEncryptionKeys(enc, stored_enc);
  if (!base || !*base) {
    strncopy(compress_m, enc, MAX_METHOD_STRLEN);
    strncopy(store_m, stored_enc, MAX_METHOD_STRLEN);
  } else {
    snprintf(compress_m, MAX_METHOD_STRLEN, "%s+%s", base, enc);
    snprintf(store_m, MAX_METHOD_STRLEN, "%s+%s", base, stored_enc);
  }
}

static std::string map_method(const char *m) {
  if (!m || !*m)
    return "storing";
  std::string out;
  const char *p = m;
  while (*p) {
    const char *plus = strchr(p, '+');
    std::string part = plus ? std::string(p, plus - p) : std::string(p);
    if (!out.empty()) out += '+';
    out += map_one_method(part.c_str());
    if (!plus) break;
    p = plus + 1;
  }
  return out.empty() ? std::string("storing") : out;
}

static int auto_method_level(const char *name) {
  if (!name || !*name) return 4;
  if (name[0] >= '1' && name[0] <= '9') return name[0] - '0';
  if (strequ(name, "x") || strequ(name, "mx") || strequ(name, "max") || strequ(name, "ultra"))
    return 9;
  return 4;
}

static bool is_auto_method_name(const char *m) {
  return m && (strequ(m, "1") || strequ(m, "2") || strequ(m, "3")
               || strequ(m, "1x") || strequ(m, "2x") || strequ(m, "3x")
               || strequ(m, "4") || strequ(m, "5") || strequ(m, "4x") || strequ(m, "5x")
               || strequ(m, "x") || strequ(m, "mx") || strequ(m, "max") || strequ(m, "ultra"));
}

static bool method_is_nonsolid(const std::string &method_str) {
  if (method_str.empty()) return false;
  std::string first = method_str;
  size_t plus = first.find('+');
  if (plus != std::string::npos) first.resize(plus);
  if (first.empty()) return false;
  int r = CompressionService((char *)first.c_str(), (char *)"nosolid?");
  return r > 0;
}

static std::string apply_extra_filters(const std::string &base, const std::vector<std::string> &filters) {
  std::string r = base;
  for (size_t i = 0; i < filters.size(); i++) {
    const std::string &f = filters[i];
    if (f.empty()) continue;
    r = f + '+' + r;
  }
  return r;
}

static std::vector<std::pair<std::string, std::string> > expand_auto_groups(const char *name, int cpus) {
  std::vector<std::pair<std::string, std::string> > out;
  if (!name || !*name) return out;
  bool mt = cpus >= 2;
  bool mt3 = cpus >= 3;
  std::string lzma4 = mt3 ? "4x4:lzma:96m:normal:16:mc8" : "lzma:96m:normal:16:mc8";
  std::string lzma5 = mt3 ? "4x4:lzma:96m:normal:32:mc32" : "lzma:96m:normal:32:mc32";
  std::string etor_c3 = mt ? "4x4:tor:c3" : "tor:c3";
  std::string bmp = "mm+grzip:m1:l2048:a";
  std::string bin1 = mt ? "4x4:tor:3:8mb" : "tor:4";
  std::string bin2 = mt ? "4x4:tor:5" : "tor:6";
  std::string bin3 = mt ? "4x4:lzma:96m:fast:32:mc4" : "lzma:96m:fast:32:mc4";
  std::string bmpfastest = std::string("mm:d1+") + bin1 + ":t0";
  if (strequ(name, "1") || strequ(name, "1x")) {
    out.push_back(std::make_pair(std::string(""), std::string("rep:96m:256:c256+") + bin1));
    out.push_back(std::make_pair(std::string("$compressed"), mt ? std::string("rep:96m:256:c256") : std::string("storing")));
    if (cpus >= 4) {
      out.push_back(std::make_pair(std::string("$wav"), "tta:m1"));
      out.push_back(std::make_pair(std::string("$bmp"), bmpfastest));
    }
  } else if (strequ(name, "2")) {
    out.push_back(std::make_pair(std::string(""), std::string("rep:96m:256:c256+exe+") + bin2));
    out.push_back(std::make_pair(std::string("$obj"), std::string("rep:96m:256:c256+") + bin2));
    out.push_back(std::make_pair(std::string("$text"), mt ? "grzip:m4" : "grzip:m4:8m:32:h15"));
    out.push_back(std::make_pair(std::string("$compressed"), std::string("rep:96m:256:c256+") + bin1));
    out.push_back(std::make_pair(std::string("$wav"), "tta:m1"));
    out.push_back(std::make_pair(std::string("$bmp"), mt ? "mm+grzip:m4:l:a" : bmpfastest));
  } else if (strequ(name, "2x")) {
    out.push_back(std::make_pair(std::string(""), std::string("exe+") + bin2));
    out.push_back(std::make_pair(std::string("$obj"), bin2));
    out.push_back(std::make_pair(std::string("$text"), bin2));
    out.push_back(std::make_pair(std::string("$wav"), "tta:m1"));
    out.push_back(std::make_pair(std::string("$bmp"), bmpfastest));
    out.push_back(std::make_pair(std::string("$compressed"), std::string("rep:96m:256:c256:8m+") + bin1));
  } else if (strequ(name, "3")) {
    out.push_back(std::make_pair(std::string(""), std::string("rep:96m+exe+delta+") + bin3));
    out.push_back(std::make_pair(std::string("$obj"), std::string("rep:96m+delta+") + bin3));
    out.push_back(std::make_pair(std::string("$text"), mt ? "grzip:m2" : "dict:p:64m:85%+lzp:64m:24:h20:92%+grzip:m3:8m:l"));
    out.push_back(std::make_pair(std::string("$compressed"), std::string("rep:96m+") + bin1));
    out.push_back(std::make_pair(std::string("$wav"), "tta"));
    out.push_back(std::make_pair(std::string("$bmp"), mt ? bmp : "mm+grzip:m4:l:a"));
  } else if (strequ(name, "3x")) {
    out.push_back(std::make_pair(std::string(""), std::string("exe+delta+") + bin3));
    out.push_back(std::make_pair(std::string("$obj"), std::string("delta+") + bin3));
    out.push_back(std::make_pair(std::string("$text"), "dict:64m:80%+tor:7:96m:h64m"));
    out.push_back(std::make_pair(std::string("$wav"), "tta:m1"));
    out.push_back(std::make_pair(std::string("$bmp"), std::string("mm+") + bin3));
    out.push_back(std::make_pair(std::string("$compressed"), std::string("rep:96m:8m+") + bin1));
  } else if (strequ(name, "4")) {
    out.push_back(std::make_pair(std::string(""), std::string("rep:96m+exe+delta+") + lzma4));
    out.push_back(std::make_pair(std::string("$obj"), std::string("rep:96m+delta+") + lzma4));
    out.push_back(std::make_pair(std::string("$text"), mt ? "grzip:m1" : "dict:p:64m:80%+lzp:64m:65:d1m:s16:h20:90%+ppmd:8:96m"));
    out.push_back(std::make_pair(std::string("$compressed"), std::string("rep:96m+") + etor_c3));
    out.push_back(std::make_pair(std::string("$wav"), "tta"));
    out.push_back(std::make_pair(std::string("$bmp"), bmp));
  } else if (strequ(name, "4x")) {
    out.push_back(std::make_pair(std::string(""), std::string("exe+delta+") + lzma4));
    out.push_back(std::make_pair(std::string("$obj"), std::string("delta+") + lzma4));
    out.push_back(std::make_pair(std::string("$text"), std::string("dict:64m:75%+") + lzma4));
    out.push_back(std::make_pair(std::string("$wav"), "tta:m1"));
    out.push_back(std::make_pair(std::string("$bmp"), std::string("mm+") + lzma4));
    out.push_back(std::make_pair(std::string("$compressed"), mt ? "4x4:tor:8m:c3" : "tor:8m:c3"));
  } else if (strequ(name, "5")) {
    out.push_back(std::make_pair(std::string(""), std::string("rep:96m+exe+delta+") + lzma5));
    out.push_back(std::make_pair(std::string("$obj"), std::string("rep:96m+delta+") + lzma5));
    out.push_back(std::make_pair(std::string("$text"), "dict:p:64m:80%+lzp:80m:105:d1m:s32:h22:92%+ppmd:12:192m"));
    out.push_back(std::make_pair(std::string("$wav"), "tta"));
    out.push_back(std::make_pair(std::string("$bmp"), bmp));
  } else if (strequ(name, "5x") || strequ(name, "x") || strequ(name, "mx") || strequ(name, "max") || strequ(name, "ultra")) {
    out.push_back(std::make_pair(std::string(""), std::string("exe+delta+") + lzma5));
    out.push_back(std::make_pair(std::string("$obj"), std::string("delta+") + lzma5));
    out.push_back(std::make_pair(std::string("$text"), std::string("dict:128m:75%+") + lzma5));
    out.push_back(std::make_pair(std::string("$wav"), "tta:m1"));
    out.push_back(std::make_pair(std::string("$bmp"), std::string("mm+") + lzma5));
  }
  return out;
}

static std::string auto_default_method(const std::vector<std::pair<std::string, std::string> > &groups) {
  for (size_t i = 0; i < groups.size(); i++) {
    if (groups[i].first.empty()) return groups[i].second;
  }
  return groups.empty() ? std::string("storing") : groups[0].second;
}

static std::string auto_method_for_type(const std::vector<std::pair<std::string, std::string> > &groups,
                                       const std::string &typ) {
  const std::string *fallback = NULL;
  for (size_t i = 0; i < groups.size(); i++) {
    if (groups[i].first.empty()) fallback = &groups[i].second;
    else if (groups[i].first == typ) return groups[i].second;
  }
  return fallback ? *fallback : std::string("storing");
}

static bool auto_has_detectable_types(const std::vector<std::pair<std::string, std::string> > &groups) {
  for (size_t i = 0; i < groups.size(); i++) {
    if (!groups[i].first.empty())
      return true;
  }
  return false;
}

enum FileDataType {
  DT_DEFAULT = 0,
  DT_COMPRESSED,
  DT_TEXT
};

static const int aCHUNKS = 5;
static const uint64 aCHUNK_SIZE = 64ull * 1024ull;

static bool ext_in_list(const std::string &name, const char *const *exts) {
  const char *dot = strrchr(name.c_str(), '.');
  if (!dot || dot == name.c_str() || !dot[1]) return false;
  std::string e = dot + 1;
  for (size_t i = 0; i < e.size(); i++)
    e[i] = (char)tolower((unsigned char)e[i]);
  for (int i = 0; exts[i]; i++)
    if (e == exts[i]) return true;
  return false;
}

static bool name_in_list(const std::string &name, const char *const *names) {
  const char *slash = strrchr(name.c_str(), '/');
#ifdef _WIN32
  const char *bslash = strrchr(name.c_str(), '\\');
  if (bslash && (!slash || bslash > slash)) slash = bslash;
#endif
  std::string base = slash ? (slash + 1) : name;
  for (size_t i = 0; i < base.size(); i++)
    base[i] = (char)tolower((unsigned char)base[i]);
  for (int i = 0; names[i]; i++)
    if (base == names[i]) return true;
  return false;
}

static std::string group_type_from_name(const std::string &name) {
  static const char *obj_exts[] = {"dcu", "obj", "o", "a", "lib", NULL};
  static const char *bmp_exts[] = {"bmp", "tif", "tiff", "tga", "wbm", "wbmp", "pgm", "pnm", "ppm", NULL};
  static const char *wav_exts[] = {"wav", "wave", "pcm", "aif", "aifc", "aiff", "au", "snd", "xac", "iff", "voc", "paf", "sd2", NULL};
  static const char *text_names[] = {"file_id.diz", "descript.ion", "files.bbs", "read.me", "readme", "makefile", NULL};
  static const char *text_exts[] = {
    "txt", "asc", "lng", "css", "dtd", "dtdxml", "htm", "html", "shtml", "xml",
    "xsl", "xsd", "docbook", "fo", "vcproj", "sgml", "yaml", "asp", "cgi", "rd",
    "cue", "1", "man", "pod", "pl", "prl", "pm", "rex", "rb", "rbw", "py", "sed",
    "awk", "tcl", "lua", "mlua", "php", "m4", "el", "elc", "vim", "sh", "ksh",
    "csh", "bash", "bat", "cmd", "btm", "bas", "vbs", "fpp", "for", "f", "ada",
    "adb", "ads", "lfm", "lpi", "lrs", "dfm", "pas", "src", "s", "asi", "inc",
    "asm", "xs", "y", "h", "hpp", "ipp", "hxx", "c", "cpp", "cxx", "c++", "cc",
    "objc", "d", "cs", "java", "js", "hc", "gc", "hsc", "hs", "lhs", "mli", "ml",
    "hrl", "erl", "scm", "vi", "lst", "log", "ini", "cfg", "mak", "in", "dsp",
    "dsw", "rc", "idl", "tar", "sql", "pkt", "xbm", "xpm", "reg", "ps", "dvi",
    "tex", "txi", "texi", "texinfo", "info", "hlf", "eng", "rus", "cnt", "tbb",
    "tbi", "toc", "mbx", "csv", "rtf", "wrd", "wri", NULL
  };
  static const char *comp_exts[] = {
    "mp3", "ogg", "aac", "flv", "avi", "wmv", "mov", "vob", "3gp", "asf", "mp4",
    "m4v", "mpg", "mkv", "ts", "mts", "m2ts", "divx", "pmd", "pmm", "pms", "bz",
    "bz2", "zoo", "arc", "hyp", "pak", "lzh", "chz", "arj", "lim", "ark", "hpk",
    "cab", "bs2", "bsn", "ain", "rar", "uc2", "ha", "ufa", "bix", "7z", "j",
    "uha", "ccm", "ccmx", "chm", "djvu", NULL
  };
  static const char *exe_exts[] = {
    "com", "exe", "scr", "sfx", "dll", "so", "ocx", "bpl", "dpl", "ovr", "ovl",
    "rtl", "386", "vxd", "drv", "sys", NULL
  };
  static const char *iso_exts[] = {
    "iso", "nrg", "bin", "img", "mds", "mdf", "c2d", "ashdisc", "isz", "b6t",
    "b5t", "b5i", "bwt", "bwi", "lcd", "ccd", "dvd", "000", "cdi", "dao", "tao",
    "cif", "xmf", "xmd", "p01", "md1", "xa", "pdi", "dmg", "vaporcd", "ncd",
    "pxi", "rif", "rdf", "gcd", "gi", "ixa", "fc4", "fcd", "vcd", "vdi", "ima",
    "bif", NULL
  };
  static const char *precomp_exts[] = {
    "pdf", "swf", "zip", "jar", "png", "gif", "gz", "tgz", "svgz", "sis", "gadget",
    "fb2z", "docx", "docm", "dotx", "dotm", "xlsx", "xlsm", "xltx", "xltm", "xlam",
    "pptx", "pptm", "potx", "potm", "ppam", "ppsx", "ppsm", "odt", "ott", "odm",
    "oth", "ods", "ots", "odg", "otg", "odp", "otp", "odf", "odb", "oxt", "sxw",
    "stw", "sxg", "sxc", "stc", "sxi", "sti", "sxd", "std", "sxm", "itz", "xmz",
    "s3z", "mdz", "pk3", "pk4", "eaz", "age3scn", "scx", "xmb", "a2c", "acm",
    "ahm", "frame", "jaz", "sequence", "wd", "rfd", "rfh", "gtr", "lod", "ezd",
    "cmo", "nmo", "sue", "trc", "clz", "sga", "gsv", NULL
  };

  if (ext_in_list(name, wav_exts)) return "$wav";
  if (ext_in_list(name, bmp_exts)) return "$bmp";
  if (ext_in_list(name, obj_exts)) return "$obj";
  if (name_in_list(name, text_names) || ext_in_list(name, text_exts)) return "$text";
  if (ext_in_list(name, comp_exts)) return "$compressed";
  if (ext_in_list(name, exe_exts)) return "$exe";
  if (ext_in_list(name, iso_exts)) return "$iso";
  if (ext_in_list(name, precomp_exts)) return "$precomp";
  return "";
}

static bool read_file_chunk(FILE *fp, uint64 pos, uint64 want, std::vector<char> &buf, size_t &got) {
  if (fseeko(fp, (off_t)pos, SEEK_SET) != 0) return false;
  if (buf.size() < (size_t)want) buf.resize((size_t)want);
  got = fread(&buf[0], 1, (size_t)want, fp);
  return got > 0;
}

static bool detect_file_mm(FILE *fp, uint64 filesize, int detect_level) {
  std::vector<char> buf;
  size_t got = 0;
  uint64 hdr = filesize < 1024ull ? filesize : 1024ull;
  if (hdr > 0 && read_file_chunk(fp, 0, hdr, buf, got) && detect_mm_header(detect_level, &buf[0], (int)got))
    return true;
  int bytes = detect_mm_bytes(detect_level, (int)(filesize > 0x7fffffff ? 0x7fffffff : filesize));
  if (bytes <= 0 || filesize == 0) return false;
  uint64 pos = ((filesize - (uint64)bytes) / 2);
  if (!read_file_chunk(fp, pos, (uint64)bytes, buf, got)) return false;
  return detect_mm(detect_level, &buf[0], (int)got) != 0;
}

static std::string choose_detected_type(const std::vector<std::string> &dataTypes, const std::string &defaultType) {
  int total = (int)dataTypes.size();
  int n_nondef = 0;
  std::string x;
  bool x_same = true;
  for (int i = 0; i < total; i++) {
    if (dataTypes[i] == "default") continue;
    if (n_nondef == 0) x = dataTypes[i];
    else if (dataTypes[i] != x) x_same = false;
    n_nondef++;
  }
  std::string best = "default";
  if (n_nondef > 0 && x_same &&
      (n_nondef == total || (total >= aCHUNKS && n_nondef == total - 1) || n_nondef * 12 >= total * 11))
    best = x;
  if (best == "default") {
    if (defaultType == "$wav" || defaultType == "$bmp" || defaultType == "$obj")
      return defaultType;
    return "default";
  }
  return best;
}

static std::string detect_file_datatype(const FileRec &f, int detect_level) {
  if (f.isdir || f.is_symlink || f.size == 0 || f.path.empty())
    return "default";
  std::string defaultType = group_type_from_name(f.name);
  if (detect_level <= 1) {
    if (defaultType == "$wav" || defaultType == "$bmp" || defaultType == "$obj" ||
        defaultType == "$text" || defaultType == "$compressed" || defaultType == "$exe" ||
        defaultType == "$iso" || defaultType == "$precomp")
      return defaultType;
    return "default";
  }
  FILE *fp = fopen(f.path.c_str(), "rb");
  if (!fp) {
    if (defaultType == "$wav" || defaultType == "$bmp" || defaultType == "$obj")
      return defaultType;
    return "default";
  }
  if ((defaultType == "$wav" || defaultType == "$bmp") && detect_file_mm(fp, f.size, detect_level)) {
    fclose(fp);
    return defaultType;
  }
  uint64 filesize = f.size;
  uint64 chunks = filesize / aCHUNK_SIZE + 1;
  int n;
  if ((int)chunks < aCHUNKS)
    n = (int)chunks;
  else
    n = (int)(sqrt((double)aCHUNKS * (double)chunks) + 0.5);
  if (n < 1) n = 1;
  uint64 blocksize = aCHUNK_SIZE;
  if (filesize / (uint64)n < blocksize)
    blocksize = filesize / (uint64)n;
  if (blocksize == 0) blocksize = filesize < aCHUNK_SIZE ? filesize : aCHUNK_SIZE;
  uint64 step = 0;
  if ((uint64)n * blocksize < filesize)
    step = (filesize - (uint64)n * blocksize) / (uint64)n;
  std::vector<char> buf((size_t)blocksize);
  std::vector<std::string> dataTypes;
  for (int i = 0; i < n; i++) {
    uint64 pos = (uint64)i * (blocksize + step);
    size_t got = 0;
    if (!read_file_chunk(fp, pos, blocksize, buf, got)) break;
    char typ[64];
    detect_datatype((BYTE *)&buf[0], (int)got, typ);
    dataTypes.push_back(typ);
  }
  fclose(fp);
  if (dataTypes.empty()) {
    if (defaultType == "$wav" || defaultType == "$bmp" || defaultType == "$obj")
      return defaultType;
    return "default";
  }
  return choose_detected_type(dataTypes, defaultType);
}

static void detect_file_types(std::vector<FileRec> &files, int detect_level) {
  for (size_t i = 0; i < files.size(); i++) {
    if (files[i].datatype.empty())
      files[i].datatype = detect_file_datatype(files[i], detect_level);
  }
}

static std::string file_ext_lc(const std::string &name) {
  const char *dot = strrchr(name.c_str(), '.');
  if (!dot || dot == name.c_str()) return "";
  std::string e = dot + 1;
  for (size_t i = 0; i < e.size(); i++)
    e[i] = (char)tolower((unsigned char)e[i]);
  return e;
}

static bool file_sort_epn(const FileRec &a, const FileRec &b) {
  std::string ea = file_ext_lc(a.name);
  std::string eb = file_ext_lc(b.name);
  if (ea != eb) return ea < eb;
  if (a.dir != b.dir) return a.dir < b.dir;
  if (a.name != b.name) return a.name < b.name;
  return a.size < b.size;
}

static std::string normalize_stored(const char *path) {
  std::string cleaned = clean_option_path(path);
  const char *p = cleaned.c_str();
  while (*p == '/' || *p == '\\') p++;
  while (p[0] == '.' && (p[1] == '/' || p[1] == '\\')) p += 2;
  std::string s;
  for (; *p; p++) {
    if (*p == '\\') s += '/';
    else s += *p;
  }
  while (!s.empty() && s[s.size() - 1] == '/')
    s.resize(s.size() - 1);
  return s;
}

static std::string join_stored(const std::string &dir, const char *name) {
  if (dir.empty()) return name;
  return dir + "/" + name;
}

static bool glob_match(const char *pat, const char *s) {
  for (;;) {
    if (*pat == '*') {
      pat++;
      if (!*pat) return true;
      for (; *s; s++)
        if (glob_match(pat, s)) return true;
      return glob_match(pat, s);
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

static bool excluded(const std::string &stored, const std::vector<std::string> &ex) {
  if (ex.empty()) return false;
  const char *base = strrchr(stored.c_str(), '/');
  base = base ? base + 1 : stored.c_str();
  for (size_t i = 0; i < ex.size(); i++) {
    const std::string &pat = ex[i];
    if (glob_match(pat.c_str(), stored.c_str()) || glob_match(pat.c_str(), base))
      return true;
    std::string prefix = pat;
    while (!prefix.empty() && prefix[prefix.size() - 1] == '/')
      prefix.resize(prefix.size() - 1);
    if (!prefix.empty() && (stored == prefix || start_with((char*)stored.c_str(), (char*)(prefix + "/").c_str())))
      return true;
  }
  return false;
}

static bool included(const std::string &stored, const std::vector<std::string> &inc) {
  if (inc.empty()) return true;
  const char *base = strrchr(stored.c_str(), '/');
  base = base ? base + 1 : stored.c_str();
  for (size_t i = 0; i < inc.size(); i++) {
    const std::string &pat = inc[i];
    if (glob_match(pat.c_str(), stored.c_str()) || glob_match(pat.c_str(), base))
      return true;
    std::string prefix = pat;
    while (!prefix.empty() && prefix[prefix.size() - 1] == '/')
      prefix.resize(prefix.size() - 1);
    if (!prefix.empty() && (stored == prefix || start_with((char*)stored.c_str(), (char*)(prefix + "/").c_str())))
      return true;
  }
  return false;
}

static void read_listfile(const char *list, std::vector<std::string> &paths, const std::string &diskpath = "") {
  std::string lpath = clean_option_path(list);
  FILE *f = fopen(lpath.c_str(), "r");
  if (!f && !diskpath.empty()) {
    std::string joined = join_fspath(diskpath, lpath);
    f = fopen(joined.c_str(), "r");
  }
  CHECK(f, (s, "ERROR: can't open listfile %s", list));
  char line[MY_FILENAME_MAX * 4];
  while (fgets(line, sizeof(line), f)) {
    char *p = line;
    while (*p == ' ' || *p == '\t') p++;
    char *e = p + strlen(p);
    while (e > p && (e[-1] == '\n' || e[-1] == '\r' || e[-1] == ' ' || e[-1] == '\t'))
      *--e = 0;
    if (!*p || *p == '#') continue;
    paths.push_back(clean_option_path(p));
  }
  fclose(f);
}

static void add_pattern_arg(const char *arg, std::vector<std::string> &list, const std::string &diskpath = "") {
  if (!arg || !*arg) return;
  if (arg[0] == '@') read_listfile(arg + 1, list, diskpath);
  else list.push_back(clean_option_path(arg));
}

static std::string find_unarc_binary(const char *argv0) {
#ifdef _WIN32
  char self_exe[MAX_PATH];
  DWORD len = GetModuleFileNameA(NULL, self_exe, sizeof(self_exe) - 1);
  if (len > 0) {
    self_exe[len] = '\0';
    std::string s = self_exe;
    size_t slash = s.find_last_of("\\/");
    if (slash != std::string::npos) {
      std::string path = s.substr(0, slash + 1) + "unarc.exe";
      if (access(path.c_str(), 0) == 0) return path;
    }
  }
#endif
  if (argv0) {
    std::string s = argv0;
    size_t slash = s.find_last_of("/\\");
    if (slash != std::string::npos) {
      std::string path = s.substr(0, slash + 1) + "unarc";
#ifdef _WIN32
      if (access((path + ".exe").c_str(), 0) == 0) return path + ".exe";
#endif
      if (access(path.c_str(), X_OK) == 0) return path;
    }
  }
#ifdef _WIN32
  if (access("build/win64/unarc.exe", 0) == 0) return "build/win64/unarc.exe";
  if (access("../build/win64/unarc.exe", 0) == 0) return "../build/win64/unarc.exe";
  if (access("./unarc.exe", 0) == 0) return "./unarc.exe";
  return "unarc.exe";
#else
  if (access("build/linux/unarc", X_OK) == 0) return "build/linux/unarc";
  if (access("../build/linux/unarc", X_OK) == 0) return "../build/linux/unarc";
  if (access("./unarc", X_OK) == 0) return "./unarc";
  return "unarc";
#endif
}

static std::string find_sfx_stub(const char *argv0, const std::string &custom) {
  if (!custom.empty()) {
    if (access(custom.c_str(), R_OK) == 0) return custom;
    err_msg("WARNING: specified SFX stub '%s' not found, searching defaults...\n", custom.c_str());
  }
  char self_exe[PATH_MAX];
#ifdef _WIN32
  DWORD len = GetModuleFileNameA(NULL, self_exe, sizeof(self_exe) - 1);
  if (len > 0) {
    self_exe[len] = '\0';
    std::string s = self_exe;
    size_t slash = s.find_last_of("\\/");
    if (slash != std::string::npos) {
      std::string path = s.substr(0, slash + 1) + "arc.sfx.exe";
      if (access(path.c_str(), 0) == 0) return path;
    }
  }
#else
  ssize_t len = readlink("/proc/self/exe", self_exe, sizeof(self_exe) - 1);
  if (len > 0) {
    self_exe[len] = '\0';
    std::string s = self_exe;
    size_t slash = s.rfind('/');
    if (slash != std::string::npos) {
      std::string path = s.substr(0, slash + 1) + "arc.sfx";
      if (access(path.c_str(), R_OK) == 0) return path;
    }
  }
#endif
  if (argv0) {
    std::string s = argv0;
    size_t slash = s.rfind('/');
    if (slash != std::string::npos) {
      std::string path = s.substr(0, slash + 1) + "arc.sfx";
      if (access(path.c_str(), R_OK) == 0) return path;
    }
  }
  if (access("build/linux/arc.sfx", R_OK) == 0) return "build/linux/arc.sfx";
  if (access("./arc.sfx", R_OK) == 0) return "./arc.sfx";
  if (access("../build/linux/arc.sfx", R_OK) == 0) return "../build/linux/arc.sfx";
  if (access("/usr/local/lib/freearc/arc.sfx", R_OK) == 0) return "/usr/local/lib/freearc/arc.sfx";
  if (access("/usr/lib/freearc/arc.sfx", R_OK) == 0) return "/usr/lib/freearc/arc.sfx";
  if (access("/usr/local/lib/arc/arc.sfx", R_OK) == 0) return "/usr/local/lib/arc/arc.sfx";
  if (access("/usr/lib/arc/arc.sfx", R_OK) == 0) return "/usr/lib/arc/arc.sfx";
  return "";
}

static std::string apply_ep(const std::string &stored, int ep) {
  if (ep == 1) {
    size_t s = stored.rfind('/');
    return s == std::string::npos ? stored : stored.substr(s + 1);
  }
  if (ep == 2) {
    size_t s = stored.find('/');
    return s == std::string::npos ? std::string() : stored.substr(s + 1);
  }
  return stored;
}

static std::string load_comment(const char *z) {
  FILE *f = fopen(z, "rb");
  if (!f) return z;
  std::string s;
  char buf[4096];
  size_t n;
  while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
    s.append(buf, n);
  fclose(f);
  while (!s.empty() && (s[s.size() - 1] == '\n' || s[s.size() - 1] == '\r'))
    s.resize(s.size() - 1);
  return s;
}

static std::string ensure_arc_ext(const char *name, int noarcext, bool is_sfx = false) {
  std::string s = name;
  if (noarcext) return s;
  size_t n = s.size();
  if (n >= 4 && strcasecmp(s.c_str() + n - 4, ".arc") == 0)
    return s;
  if (n >= 4 && strcasecmp(s.c_str() + n - 4, ".sfx") == 0)
    return s;
  if (n >= 4 && strcasecmp(s.c_str() + n - 4, ".run") == 0)
    return s;
  if (is_sfx) {
    s += ".sfx";
    return s;
  }
  s += ".arc";
  return s;
}


static bool same_file(const struct stat &a, const struct stat &b) {
  if (a.st_ino != 0 && b.st_ino != 0)
    return a.st_dev == b.st_dev && a.st_ino == b.st_ino;
  return false;
}

static bool is_same_file_path(const char *path1, const char *path2) {
  if (!path1 || !path2) return false;
#ifdef _WIN32
  char f1[MAX_PATH], f2[MAX_PATH];
  if (GetFullPathNameA(path1, MAX_PATH, f1, NULL) && GetFullPathNameA(path2, MAX_PATH, f2, NULL))
    return _stricmp(f1, f2) == 0;
  return _stricmp(path1, path2) == 0;
#else
  struct stat st1, st2;
  if (stat(path1, &st1) == 0 && stat(path2, &st2) == 0)
    return same_file(st1, st2);
  return strcmp(path1, path2) == 0;
#endif
}

static void add_entry(std::vector<FileRec> &files, std::set<std::string> &seen,
                      const char *fspath, const std::string &stored,
                      const struct stat &st, uint8 isdir,
                      int ep, const std::vector<std::string> &ex,
                      const std::vector<std::string> &inc,
                      const std::string &ap) {
  std::string name = apply_ep(stored, ep);
  if (name.empty()) return;
  if (!ap.empty()) name = join_stored(ap, name.c_str());
  if (excluded(name, ex)) return;
  if (!isdir && !included(name, inc)) return;
  if (!seen.insert(name).second) return;
  FileRec f;
  f.path = fspath;
  split_path(name.c_str(), f.dir, f.name);
  if (f.name.empty()) return;
  f.size = isdir ? 0 : (uint64)st.st_size;
  f.mtime = (uint32)st.st_mtime;
  f.isdir = isdir;
  f.crc = 0;
  f.mode = (uint32)st.st_mode;
  f.is_symlink = false;
  f.symlink_target.clear();
  f.datatype.clear();
  files.push_back(f);
}

static void add_symlink_entry(std::vector<FileRec> &files, std::set<std::string> &seen,
                              const char *fspath, const std::string &stored,
                              const struct stat &st, const char *target,
                              int ep, const std::vector<std::string> &ex,
                              const std::vector<std::string> &inc,
                              const std::string &ap) {
  std::string name = apply_ep(stored, ep);
  if (name.empty()) return;
  if (!ap.empty()) name = join_stored(ap, name.c_str());
  if (excluded(name, ex)) return;
  if (!included(name, inc)) return;
  if (!seen.insert(name).second) return;
  FileRec f;
  f.path = fspath;
  split_path(name.c_str(), f.dir, f.name);
  if (f.name.empty()) return;
  f.size = 0;
  f.mtime = (uint32)st.st_mtime;
  f.isdir = 0;
  f.crc = 0;
  f.mode = (uint32)st.st_mode;
  f.is_symlink = true;
  f.symlink_target = target ? target : "";
  f.datatype.clear();
  files.push_back(f);
}

static void walk_path(std::vector<FileRec> &files, std::set<std::string> &seen,
                      const char *fspath, const std::string &stored,
                      const struct stat *skip_arc, int recurse, int ep,
                      const std::vector<std::string> &ex,
                      const std::vector<std::string> &inc,
                      const std::string &ap,
                      const char *skip_arc_path = NULL) {
  struct stat st;
  if (lstat(fspath, &st) != 0) {
    msg("WARNING: skip %s (%s)\n", fspath, strerror(errno));
    return;
  }
  if (skip_arc && S_ISREG(st.st_mode) && same_file(st, *skip_arc))
    return;
  if (skip_arc_path && S_ISREG(st.st_mode) && is_same_file_path(fspath, skip_arc_path))
    return;
#ifndef _WIN32
  if (S_ISLNK(st.st_mode)) {
    char target[PATH_MAX];
    ssize_t len = readlink(fspath, target, sizeof(target) - 1);
    if (len >= 0) {
      target[len] = '\0';
      add_symlink_entry(files, seen, fspath, stored, st, target, ep, ex, inc, ap);
    } else {
      msg("WARNING: cannot readlink %s (%s)\n", fspath, strerror(errno));
    }
    return;
  }
#endif
  if (S_ISREG(st.st_mode)) {
    add_entry(files, seen, fspath, stored, st, 0, ep, ex, inc, ap);
    return;
  }
  if (!S_ISDIR(st.st_mode)) {
    msg("WARNING: skip non-regular %s\n", fspath);
    return;
  }

  std::string ep_stored = apply_ep(stored, ep);
  if (!ap.empty() && !ep_stored.empty()) ep_stored = join_stored(ap, ep_stored.c_str());
  if (excluded(stored, ex) || excluded(ep_stored, ex))
    return;

  add_entry(files, seen, fspath, stored, st, 1, ep, ex, inc, ap);
  if (!recurse) return;
  DIR *d = opendir(fspath);
  if (!d) {
    msg("WARNING: skip directory %s (%s)\n", fspath, strerror(errno));
    return;
  }
  while (struct dirent *e = readdir(d)) {
    if (strequ(e->d_name, ".") || strequ(e->d_name, ".."))
      continue;
    std::string child_fs = std::string(fspath) + "/" + e->d_name;
    walk_path(files, seen, child_fs.c_str(), join_stored(stored, e->d_name),
              skip_arc, recurse, ep, ex, inc, ap, skip_arc_path);
  }
  closedir(d);
}

static void walk_input(std::vector<FileRec> &files, std::set<std::string> &seen,
                       const std::string &raw_input, const std::string &diskpath,
                       const struct stat *skip_arc, int recurse, int ep,
                       const std::vector<std::string> &ex,
                       const std::vector<std::string> &inc,
                       const std::string &ap,
                       const char *skip_arc_path) {
  std::string fspath = is_absolute_path(raw_input) ? raw_input : join_fspath(diskpath, raw_input);
  std::string stored = normalize_stored(raw_input.c_str());
  if (stored.empty()) {
    msg("WARNING: skip %s\n", raw_input.c_str());
    return;
  }

  // Check if raw_input contains wildcard characters
  if (raw_input.find('*') != std::string::npos || raw_input.find('?') != std::string::npos) {
    size_t slash = raw_input.find_last_of("/\\");
    std::string rel_dir = (slash != std::string::npos) ? raw_input.substr(0, slash) : "";
    std::string pattern = (slash != std::string::npos) ? raw_input.substr(slash + 1) : raw_input;

    std::string fs_dir = is_absolute_path(rel_dir) ? rel_dir : join_fspath(diskpath, rel_dir);
    if (fs_dir.empty()) fs_dir = ".";

    DIR *d = opendir(fs_dir.c_str());
    if (!d) {
      msg("WARNING: cannot open directory %s (%s)\n", fs_dir.c_str(), strerror(errno));
      return;
    }
    while (struct dirent *e = readdir(d)) {
      if (strequ(e->d_name, ".") || strequ(e->d_name, "..")) continue;
      if (glob_match(pattern.c_str(), e->d_name)) {
        std::string child_raw = rel_dir.empty() ? e->d_name : (rel_dir + "/" + e->d_name);
        std::string child_fs = fs_dir + "/" + e->d_name;
        std::string child_stored = normalize_stored(child_raw.c_str());
        walk_path(files, seen, child_fs.c_str(), child_stored, skip_arc, recurse, ep, ex, inc, ap, skip_arc_path);
      }
    }
    closedir(d);
    return;
  }

  walk_path(files, seen, fspath.c_str(), stored, skip_arc, recurse, ep, ex, inc, ap, skip_arc_path);
}

static BlockInfo write_control_block(VolumeWriter *out, int type, const char *compress_method,
                                     const char *store_method, ByteWriter &content) {
  BlockInfo b;
  memset(&b, 0, sizeof(b));
  b.type = type;
  strncopy(b.compressor, (char *)store_method, MAX_METHOD_STRLEN);
  b.pos = out->tell();
  b.origsize = (FILESIZE)content.size();
  b.crc = content.size() ? CalcCRC(content.data(), (uint)content.size()) : 0;

  MemInFileOut ctx;
  ctx.in = (const char *)content.data();
  ctx.in_left = (int)content.size();
  ctx.out = out;
  ctx.written = 0;
  int rc = MultiCompress((char *)compress_method, mem_callback, &ctx);
  CHECK(rc >= 0, (s, "ERROR: compression failed (%d) method \"%s\"", rc, compress_method));
  b.compsize = (FILESIZE)ctx.written;

  ByteWriter d;
  d.write_u32(aSIGNATURE);
  d.writeInteger((uint64)type);
  d.writeString(b.compressor);
  d.writeInteger((uint64)b.origsize);
  d.writeInteger((uint64)b.compsize);
  d.write_u32(b.crc);
  uint dcrc = CalcCRC(d.data(), (uint)d.size());
  d.write_u32(dcrc);
  CHECK(out->write(d.data(), d.size()), (s, "ERROR: archive write failed"));
  return b;
}

static BlockInfo write_control_block(VolumeWriter *out, int type, const char *method, ByteWriter &content) {
  return write_control_block(out, type, method, method, content);
}

struct DataBlockRec {
  BlockInfo info;
  int start;
  int count;
};

static void write_dir_block(ByteWriter &w, std::vector<DataBlockRec> &blocks,
                            std::vector<FileRec> &files, FILESIZE dir_pos) {
  int nfiles = (int)files.size();
  int nblocks = (int)blocks.size();
  w.writeInteger((uint64)nblocks);
  for (int i = 0; i < nblocks; i++)
    w.writeInteger((uint64)blocks[i].count);
  for (int i = 0; i < nblocks; i++)
    w.writeString(blocks[i].info.compressor);
  for (int i = 0; i < nblocks; i++)
    w.writeInteger((uint64)(dir_pos - blocks[i].info.pos));
  for (int i = 0; i < nblocks; i++)
    w.writeInteger((uint64)blocks[i].info.compsize);

  std::vector<std::string> dirs;
  std::vector<int> dir_numbers;
  dirs.reserve((size_t)nfiles);
  dir_numbers.reserve((size_t)nfiles);
  for (int i = 0; i < nfiles; i++) {
    int found = -1;
    for (size_t d = 0; d < dirs.size(); d++) {
      if (dirs[d] == files[i].dir) { found = (int)d; break; }
    }
    if (found < 0) {
      found = (int)dirs.size();
      dirs.push_back(files[i].dir);
    }
    dir_numbers.push_back(found);
  }

  w.writeInteger((uint64)dirs.size());
  for (size_t i = 0; i < dirs.size(); i++)
    w.writeString(dirs[i].c_str());

  for (int i = 0; i < nfiles; i++)
    w.writeString(files[i].name.c_str());
  for (int i = 0; i < nfiles; i++)
    w.writeInteger((uint64)dir_numbers[i]);
  for (int i = 0; i < nfiles; i++)
    w.writeInteger(files[i].size);
  for (int i = 0; i < nfiles; i++)
    w.write_u32(files[i].mtime);
  for (int i = 0; i < nfiles; i++)
    w.write_u8(files[i].isdir);
  for (int i = 0; i < nfiles; i++)
    w.write_u32(files[i].crc);

  bool need_posix = false;
  bool any_symlink = false;
  for (int i = 0; i < nfiles; i++) {
    uint32 def = files[i].isdir ? 0755u : 0644u;
    if ((files[i].mode & 07777) != def)
      need_posix = true;
    if (files[i].is_symlink)
      any_symlink = true;
  }
  if (need_posix) {
    w.writeInteger(1);
    w.writeInteger((uint64)nfiles);
    for (int i = 0; i < nfiles; i++)
      w.write_u32(files[i].mode);
  }
  if (any_symlink) {
    w.writeInteger(2);
    w.writeInteger((uint64)nfiles);
    for (int i = 0; i < nfiles; i++)
      w.writeString(files[i].is_symlink ? files[i].symlink_target.c_str() : "");
  }
  w.writeInteger(0);
}

static void write_footer(ByteWriter &w, BlockInfo *blocks, int nblocks,
                         FILESIZE footer_pos, const std::string &comment) {
  w.writeInteger((uint64)nblocks);
  for (int i = 0; i < nblocks; i++) {
    BlockInfo &b = blocks[i];
    w.writeInteger((uint64)b.type);
    w.writeString(b.compressor);
    w.writeInteger((uint64)(footer_pos - b.pos));
    w.writeInteger((uint64)b.origsize);
    w.writeInteger((uint64)b.compsize);
    w.write_u32(b.crc);
  }
  w.write_u8(0);
  w.writeInteger(0);
  w.writeString("");
  w.writeInteger((uint64)comment.size());
  if (!comment.empty())
    w.write_raw(comment.data(), comment.size());
}

class TempBlockWriter : public IArchiveWriter {
public:
  std::vector<char> mem_buf;
  FILE             *temp_file;
  std::string       temp_path;
  uint64            written;

  TempBlockWriter() : temp_file(NULL), written(0) {}
  virtual ~TempBlockWriter() {
    if (temp_file) {
      fclose(temp_file);
      temp_file = NULL;
    }
    if (!temp_path.empty()) {
      remove(temp_path.c_str());
      temp_path.clear();
    }
  }

  virtual bool write(const void *buf, size_t n) {
    if (n == 0) return true;
    const char *p = (const char*)buf;
    if (!temp_file && written + n <= 16 * 1024 * 1024) {
      size_t old_size = mem_buf.size();
      mem_buf.resize(old_size + n);
      memcpy(&mem_buf[old_size], p, n);
      written += n;
      return true;
    }
    if (!temp_file) {
#ifdef _WIN32
      char tmp_dir[MAX_PATH];
      GetTempPathA(MAX_PATH, tmp_dir);
      char tmp_file[MAX_PATH];
      GetTempFileNameA(tmp_dir, "arc", 0, tmp_file);
      temp_path = tmp_file;
      temp_file = fopen(temp_path.c_str(), "w+b");
#else
      char temp_template[MY_FILENAME_MAX];
      snprintf(temp_template, sizeof(temp_template), "/tmp/arc_block_XXXXXX");
      int fd = mkstemp(temp_template);
      if (fd >= 0) {
        temp_path = temp_template;
        temp_file = fdopen(fd, "w+b");
      }
#endif
      if (!temp_file) return false;
      if (!mem_buf.empty()) {
        if (fwrite(&mem_buf[0], 1, mem_buf.size(), temp_file) != mem_buf.size())
          return false;
        mem_buf.clear();
        std::vector<char>().swap(mem_buf);
      }
    }
    if (fwrite(p, 1, n, temp_file) != n) return false;
    written += n;
    return true;
  }

  virtual FILESIZE tell() const {
    return (FILESIZE)written;
  }

  bool copy_to(VolumeWriter *out) {
    if (!mem_buf.empty()) {
      return out->write(&mem_buf[0], mem_buf.size());
    }
    if (temp_file) {
      fflush(temp_file);
      fseek(temp_file, 0, SEEK_SET);
      char buf[65536];
      size_t nr;
      while ((nr = fread(buf, 1, sizeof(buf), temp_file)) > 0) {
        if (!out->write(buf, nr)) return false;
      }
      fclose(temp_file);
      temp_file = NULL;
      if (!temp_path.empty()) {
        remove(temp_path.c_str());
        temp_path.clear();
      }
    }
    return true;
  }
};

static BlockInfo compress_range(IArchiveWriter *out, const char *method, const char *password,
                                const char *algorithm, FileRec *files, int start, int count,
                                bool allow_io_pipeline = true) {
  uint64 orig = 0;
  for (int i = 0; i < count; i++)
    orig += files[start + i].size;
  std::string prepared = canonize_method_chain(method ? method : aNO_COMPRESSION, haskell_dict_limit((FILESIZE)orig));
  char compress_m[MAX_METHOD_STRLEN], store_m[MAX_METHOD_STRLEN];
  wrap_encryption(prepared.c_str(), password, algorithm, compress_m, store_m);
  BlockInfo b;
  memset(&b, 0, sizeof(b));
  b.type = DATA_BLOCK;
  strncopy(b.compressor, (char *)purify_stored_method(store_m).c_str(), MAX_METHOD_STRLEN);
  b.pos = out->tell();
  b.origsize = (FILESIZE)orig;

  if (orig == 0) {
    for (int i = 0; i < count; i++)
      files[start + i].crc = 0;
    b.compsize = 0;
    b.crc = 0;
    return b;
  }

  // Si mono-thread explicite (-mt1), très petit volume (<64KB), ou pipeline désactivé (worker parallèle)
  if (!allow_io_pipeline || GetCompressionThreads() <= 1 || orig < 65536) {
    SolidInFileOut sctx;
    memset(&sctx, 0, sizeof(sctx));
    sctx.files = files;
    sctx.nfiles = start + count;
    sctx.idx = start;
    sctx.end = start + count;
    sctx.cur = NULL;
    sctx.out = out;
    sctx.written = 0;
    int rc = MultiCompress(compress_m, solid_callback, &sctx);
    if (sctx.cur) fclose(sctx.cur);
    CHECK(rc >= 0, (s, "ERROR: data compression failed (%d) method \"%s\"", rc, compress_m));
    b.compsize = (FILESIZE)sctx.written;
    b.crc = 0;
    return b;
  }

  // --- Pipeline Asynchrone I/O (§2.3) ---
  const size_t QUEUE_DEPTH = 4;
  const size_t CHUNK_SIZE = 1024 * 1024; // 1 MB

  ArcChunkQueue in_queue(QUEUE_DEPTH);
  ArcChunkQueue free_in_chunks(QUEUE_DEPTH);
  for (size_t i = 0; i < QUEUE_DEPTH; i++) {
    free_in_chunks.push(new ArcChunk(CHUNK_SIZE));
  }

  ArcChunkQueue out_queue(QUEUE_DEPTH);
  ArcChunkQueue free_out_chunks(QUEUE_DEPTH);
  for (size_t i = 0; i < QUEUE_DEPTH; i++) {
    free_out_chunks.push(new ArcChunk(CHUNK_SIZE));
  }

  PipeReaderContext reader_ctx;
  reader_ctx.files = files;
  reader_ctx.idx = start;
  reader_ctx.end = start + count;
  reader_ctx.in_queue = &in_queue;
  reader_ctx.free_in_chunks = &free_in_chunks;
  reader_ctx.error_code = 0;
  reader_ctx.done = false;

  PipeWriterContext writer_ctx;
  writer_ctx.out = out;
  writer_ctx.out_queue = &out_queue;
  writer_ctx.free_out_chunks = &free_out_chunks;
  writer_ctx.written = 0;
  writer_ctx.error_code = 0;
  writer_ctx.done = false;

  PipelinedCallbackState cb_state;
  cb_state.reader_ctx = &reader_ctx;
  cb_state.writer_ctx = &writer_ctx;
  cb_state.cur_in_chunk = NULL;
  cb_state.in_eof = false;

  ArcThread reader_th = arc_spawn_thread((ArcThreadFunc)pipe_reader_func, &reader_ctx);
  ArcThread writer_th = arc_spawn_thread((ArcThreadFunc)pipe_writer_func, &writer_ctx);

  int rc = MultiCompress(compress_m, pipelined_solid_callback, &cb_state);

  // Signaler fin au Reader et Writer
  in_queue.stop();
  free_in_chunks.stop();

  ArcChunk *out_eof = new ArcChunk();
  out_eof->is_eof = true;
  out_eof->size = 0;
  out_queue.push(out_eof);

  arc_wait_thread(reader_th);
  arc_wait_thread(writer_th);

  out_queue.stop();
  free_out_chunks.stop();

  if (cb_state.cur_in_chunk) {
    delete cb_state.cur_in_chunk;
    cb_state.cur_in_chunk = NULL;
  }
  free_in_chunks.clear_and_delete();
  free_out_chunks.clear_and_delete();
  in_queue.clear_and_delete();
  out_queue.clear_and_delete();

  CHECK(reader_ctx.error_code == 0, (s, "ERROR: data read failed (%d)", reader_ctx.error_code));
  CHECK(writer_ctx.error_code == 0, (s, "ERROR: data write failed (%d)", writer_ctx.error_code));
  CHECK(rc >= 0, (s, "ERROR: data compression failed (%d) method \"%s\"", rc, compress_m));

  b.compsize = (FILESIZE)writer_ctx.written;
  b.crc = 0;
  return b;
}

struct BlockTask {
  int start;
  int count;
  std::string method;
};

struct BlockCompressJob {
  int             block_idx;
  int             start;
  int             count;
  std::string     method;
  TempBlockWriter writer;
  BlockInfo       info;
  int             errcode;
  bool            done;

  BlockCompressJob() : block_idx(0), start(0), count(0), errcode(0), done(false) {}
};

struct MultiBlockPoolContext {
  std::vector<BlockCompressJob*> *jobs;
  size_t                          next_job;
  size_t                          total_jobs;
  ArcMutex                        mutex;
  const char                     *method;
  const char                     *password;
  const char                     *algorithm;
  FileRec                        *files;
  volatile int                    global_error;
};

static ARC_THREAD_CALL block_worker_func(void *arg) {
  MultiBlockPoolContext *ctx = (MultiBlockPoolContext*)arg;
  while (true) {
    size_t idx;
    ctx->mutex.lock();
    if (ctx->global_error != 0 || ctx->next_job >= ctx->total_jobs) {
      ctx->mutex.unlock();
      break;
    }
    idx = ctx->next_job++;
    ctx->mutex.unlock();

    BlockCompressJob *job = (*ctx->jobs)[idx];
    const char *job_method = job->method.empty() ? ctx->method : job->method.c_str();
    job->info = compress_range(&job->writer, job_method, ctx->password, ctx->algorithm, ctx->files, job->start, job->count, false);
    job->done = true;
  }
  return ARC_THREAD_RET_VAL;
}

static bool write_archive(const std::string &arcname, uint64 vol_size,
                          std::vector<FileRec> &files, int solid,
                          const char *method, const char *password,
                          const char *hp_password, const char *algorithm,
                          const std::string &comment,
                          std::vector<std::string> &out_vol_names,
                          uint64 &out_total_written,
                          const std::string &sfx_stub = "",
                          uint64 solid_block_size = 0,
                          int solid_block_files = 0,
                          const std::vector<std::string> &extra_filters = std::vector<std::string>(),
                          bool sort_files = true) {
  VolumeWriter out(arcname, vol_size);
  CHECK(out.open(), (s, "ERROR: can't create archive %s", arcname.c_str()));

  if (!sfx_stub.empty()) {
    FILE *sf = fopen(sfx_stub.c_str(), "rb");
    CHECK(sf != NULL, (s, "ERROR: cannot open SFX stub %s (%s)", sfx_stub.c_str(), strerror(errno)));
    char sfx_buf[65536];
    size_t nr;
    while ((nr = fread(sfx_buf, 1, sizeof(sfx_buf), sf)) > 0) {
      CHECK(out.write(sfx_buf, nr), (s, "ERROR: failed writing SFX stub into %s", arcname.c_str()));
    }
    fclose(sf);
  }

  if (sort_files) {
    std::sort(files.begin(), files.end(), file_sort_epn);
  }

  ByteWriter header;
  header.write_u32(aSIGNATURE);
  header.write_u32(aARCHIVE_VERSION);
  BlockInfo header_block = write_control_block(&out, HEADER_BLOCK, aNO_COMPRESSION, header);

  for (size_t i = 0; i < files.size(); i++) {
    std::string disp = join_stored(files[i].dir, files[i].name.c_str());
    if (files[i].is_symlink)
      msg("Adding symlink %s -> %s\n", disp.c_str(), files[i].symlink_target.c_str());
    else if (files[i].isdir)
      msg("Adding %s/\n", disp.c_str());
    else
      msg("Compressing %s (%llu bytes)\n", disp.c_str(), (unsigned long long)files[i].size);
  }

  std::vector<std::pair<std::string, std::string> > auto_groups;
  std::string default_method = method ? method : aNO_COMPRESSION;
  bool use_auto = is_auto_method_name(method);
  if (use_auto) {
    auto_groups = expand_auto_groups(method, GetCompressionThreads());
    if (!extra_filters.empty()) {
      for (size_t gi = 0; gi < auto_groups.size(); gi++)
        auto_groups[gi].second = apply_extra_filters(auto_groups[gi].second, extra_filters);
    }
    default_method = auto_default_method(auto_groups);
    if (sort_files)
      std::sort(files.begin(), files.end(), file_sort_epn);
    if (auto_has_detectable_types(auto_groups))
      detect_file_types(files, auto_method_level(method));
    std::map<std::string, std::vector<FileRec> > by_method;
    std::vector<std::string> method_order;
    for (size_t i = 0; i < files.size(); i++) {
      std::string mth = auto_method_for_type(auto_groups, files[i].datatype);
      if (by_method.find(mth) == by_method.end())
        method_order.push_back(mth);
      by_method[mth].push_back(files[i]);
    }
    std::sort(method_order.begin(), method_order.end());
    std::vector<FileRec> reordered;
    reordered.reserve(files.size());
    for (size_t i = 0; i < method_order.size(); i++) {
      std::vector<FileRec> &grp = by_method[method_order[i]];
      reordered.insert(reordered.end(), grp.begin(), grp.end());
    }
    files.swap(reordered);
  }

  std::vector<BlockTask> tasks;
  if (use_auto && !files.empty()) {
    int i = 0;
    while (i < (int)files.size()) {
      std::string mth = auto_method_for_type(auto_groups, files[i].datatype);
      int j = i + 1;
      while (j < (int)files.size() && auto_method_for_type(auto_groups, files[j].datatype) == mth)
        j++;
      BlockTask t;
      t.start = i;
      t.count = j - i;
      t.method = mth;
      tasks.push_back(t);
      i = j;
    }
  } else if (solid == 1 && solid_block_size == 0 && solid_block_files == 0) {
    if (!files.empty()) {
      BlockTask t;
      t.start = 0;
      t.count = (int)files.size();
      t.method = default_method;
      tasks.push_back(t);
    }
  } else if (solid == 0 || solid_block_files == 1) {
    for (int i = 0; i < (int)files.size(); i++) {
      BlockTask t;
      t.start = i;
      t.count = 1;
      t.method = default_method;
      tasks.push_back(t);
    }
  } else if (solid_block_files > 1) {
    for (int i = 0; i < (int)files.size(); i += solid_block_files) {
      BlockTask t;
      t.start = i;
      t.count = std::min(solid_block_files, (int)files.size() - i);
      t.method = default_method;
      tasks.push_back(t);
    }
  } else if (solid_block_size > 0) {
    int cur_start = 0;
    uint64 cur_size = 0;
    for (int i = 0; i < (int)files.size(); i++) {
      cur_size += files[i].size;
      if (cur_size >= solid_block_size && i > cur_start) {
        BlockTask t;
        t.start = cur_start;
        t.count = i - cur_start + 1;
        t.method = default_method;
        tasks.push_back(t);
        cur_start = i + 1;
        cur_size = 0;
      }
    }
    if (cur_start < (int)files.size()) {
      BlockTask t;
      t.start = cur_start;
      t.count = (int)files.size() - cur_start;
      t.method = default_method;
      tasks.push_back(t);
    }
  } else {
    if (!files.empty()) {
      BlockTask t;
      t.start = 0;
      t.count = (int)files.size();
      t.method = default_method;
      tasks.push_back(t);
    }
  }

  {
    std::vector<BlockTask> expanded;
    expanded.reserve(tasks.size());
    for (size_t i = 0; i < tasks.size(); i++) {
      const std::string &mth = tasks[i].method.empty() ? default_method : tasks[i].method;
      if (tasks[i].count > 1 && method_is_nonsolid(mth)) {
        for (int k = 0; k < tasks[i].count; k++) {
          BlockTask t;
          t.start = tasks[i].start + k;
          t.count = 1;
          t.method = mth;
          expanded.push_back(t);
        }
      } else {
        expanded.push_back(tasks[i]);
      }
    }
    tasks.swap(expanded);
  }

  std::vector<DataBlockRec> data_blocks;
  FileRec *files_ptr = files.empty() ? NULL : &files[0];
  int threads = GetCompressionThreads();

  if (threads <= 1 || tasks.size() <= 1) {
    for (size_t i = 0; i < tasks.size(); i++) {
      DataBlockRec rec;
      rec.start = tasks[i].start;
      rec.count = tasks[i].count;
      const char *block_method = tasks[i].method.empty() ? default_method.c_str() : tasks[i].method.c_str();
      rec.info = compress_range(&out, block_method, password, algorithm, files_ptr, rec.start, rec.count, true);
      data_blocks.push_back(rec);
    }
  } else {
    std::vector<BlockCompressJob*> jobs;
    for (size_t i = 0; i < tasks.size(); i++) {
      BlockCompressJob *job = new BlockCompressJob();
      job->block_idx = (int)i;
      job->start = tasks[i].start;
      job->count = tasks[i].count;
      job->method = tasks[i].method.empty() ? default_method : tasks[i].method;
      jobs.push_back(job);
    }

    MultiBlockPoolContext ctx;
    ctx.jobs = &jobs;
    ctx.next_job = 0;
    ctx.total_jobs = jobs.size();
    ctx.method = method;
    ctx.password = password;
    ctx.algorithm = algorithm;
    ctx.files = files_ptr;
    ctx.global_error = 0;

    int num_workers = (int)std::min((size_t)threads, jobs.size());
    std::vector<ArcThread> workers;
    for (int w = 0; w < num_workers; w++) {
      workers.push_back(arc_spawn_thread((ArcThreadFunc)block_worker_func, &ctx));
    }

    for (int w = 0; w < num_workers; w++) {
      arc_wait_thread(workers[w]);
    }

    for (size_t i = 0; i < jobs.size(); i++) {
      BlockCompressJob *job = jobs[i];
      FILESIZE block_start_pos = out.tell();
      CHECK(job->writer.copy_to(&out), (s, "ERROR: writing block %d to archive failed", (int)i));
      DataBlockRec rec;
      rec.start = job->start;
      rec.count = job->count;
      rec.info = job->info;
      rec.info.pos = block_start_pos;
      data_blocks.push_back(rec);
      delete job;
    }
  }

  std::string dir_method = canonize_method_chain(aDEFAULT_DIR_COMPRESSION, 0);
  char dir_compress_m[MAX_METHOD_STRLEN], dir_store_m[MAX_METHOD_STRLEN];
  wrap_encryption(dir_method.c_str(), hp_password, algorithm, dir_compress_m, dir_store_m);

  FILESIZE dir_pos = out.tell();
  ByteWriter dir;
  write_dir_block(dir, data_blocks, files, dir_pos);
  BlockInfo dir_block = write_control_block(&out, DIR_BLOCK, dir_compress_m, dir_store_m, dir);

  std::string footer_method = canonize_method_chain(aDEFAULT_DIR_COMPRESSION, 0);
  char footer_compress_m[MAX_METHOD_STRLEN], footer_store_m[MAX_METHOD_STRLEN];
  wrap_encryption(footer_method.c_str(), hp_password, algorithm, footer_compress_m, footer_store_m);

  FILESIZE footer_pos = out.tell();
  BlockInfo control[2];
  control[0] = header_block;
  control[1] = dir_block;
  ByteWriter footer;
  write_footer(footer, control, 2, footer_pos, comment);
  write_control_block(&out, FOOTER_BLOCK, footer_compress_m, footer_store_m, footer);

  out_vol_names = out.volume_names;
  out_total_written = out.total_written;
  out.close();
  if (!sfx_stub.empty()) {
#ifndef _WIN32
    chmod(arcname.c_str(), 0755);
#endif
  }
  return true;
}


static std::string make_temp_dir() {
#ifdef _WIN32
  char tmp_path[MAX_PATH];
  GetTempPathA(MAX_PATH, tmp_path);
  char tmp_file[MAX_PATH];
  GetTempFileNameA(tmp_path, "arc", 0, tmp_file);
  DeleteFileA(tmp_file);
  CreateDirectoryA(tmp_file, NULL);
  return std::string(tmp_file);
#else
  char tmpl[PATH_MAX];
  snprintf(tmpl, sizeof(tmpl), "/tmp/arc_mod_XXXXXX");
  char *d = mkdtemp(tmpl);
  CHECK(d, (s, "ERROR: can't create temporary directory %s", tmpl));
  return std::string(d);
#endif
}

static void remove_dir_recursive(const std::string &path) {
  if (path.empty()) return;
  DIR *d = opendir(path.c_str());
  if (!d) return;
  while (struct dirent *e = readdir(d)) {
    if (strequ(e->d_name, ".") || strequ(e->d_name, "..")) continue;
    std::string child = path + "/" + e->d_name;
    struct stat st;
    if (lstat(child.c_str(), &st) == 0) {
      if (S_ISDIR(st.st_mode))
        remove_dir_recursive(child);
      else
        unlink(child.c_str());
    }
  }
  closedir(d);
  rmdir(path.c_str());
}

static void replace_archive_files(const std::string &arcname, const std::string &tmp_arcname, uint64 vol_size, bool is_sfx = false) {
  unlink(arcname.c_str());
  for (int i = 1; i <= 9999; i++) {
    char vbuf[MY_FILENAME_MAX * 4];
    snprintf(vbuf, sizeof(vbuf), "%s.%03d", arcname.c_str(), i);
    if (unlink(vbuf) != 0) break;
  }
  if (vol_size == 0) {
    rename(tmp_arcname.c_str(), arcname.c_str());
    if (is_sfx) chmod(arcname.c_str(), 0755);
  } else {
    for (int i = 1; i <= 9999; i++) {
      char tmp_vbuf[MY_FILENAME_MAX * 4];
      char new_vbuf[MY_FILENAME_MAX * 4];
      snprintf(tmp_vbuf, sizeof(tmp_vbuf), "%s.%03d", tmp_arcname.c_str(), i);
      snprintf(new_vbuf, sizeof(new_vbuf), "%s.%03d", arcname.c_str(), i);
      if (rename(tmp_vbuf, new_vbuf) != 0) break;
    }
    if (is_sfx) {
      char v1[MY_FILENAME_MAX * 4];
      snprintf(v1, sizeof(v1), "%s.001", arcname.c_str());
      chmod(v1, 0755);
    }
  }
}

struct ExistingFile {
  std::string fullname;
  uint64 size;
  uint32 mtime;
  uint8 isdir;
  uint32 crc;
  int block_idx;
  uint32 mode;
  bool is_symlink;
  std::string symlink_target;
};

struct ExistingArchiveInfo {
  bool exists;
  bool is_multivol;
  bool is_sfx;
  uint64 first_vol_size;
  std::string comment;
  std::vector<ExistingFile> files;
  int num_blocks;
  std::vector<BLOCK> data_blocks;
};

static bool check_archive_exists(const std::string &arcname) {
  if (file_exists((char*)arcname.c_str())) return true;
  char buf[MY_FILENAME_MAX * 4];
  snprintf(buf, sizeof(buf), "%s.001", arcname.c_str());
  if (file_exists(buf)) return true;
  return false;
}

static bool inspect_archive(const std::string &arcname, const char *password, ExistingArchiveInfo &info) {
  info.exists = false;
  info.is_multivol = false;
  info.is_sfx = false;
  info.first_vol_size = 0;
  info.files.clear();
  info.data_blocks.clear();
  info.comment.clear();

  if (!check_archive_exists(arcname)) return false;

  SetArchivePassword((char*)password);
  ARCHIVE arc;
  arc.arcfile.open((char*)arcname.c_str(), READ_MODE);
  if (!arc.arcfile.isopen()) return false;

  info.exists = true;
  if (arc.arcfile.num_vols > 1) {
    info.is_multivol = true;
    info.first_vol_size = arc.arcfile.vols[0].size;
  }

  arc.read_structure();
  info.is_sfx = (arc.SFXSize > 0);
  if (arc.arcComment.size > 0) {
    info.comment.assign(arc.arcComment.data, arc.arcComment.size);
  }

  int dir_idx = -1;
  for (int i = 0; i < arc.control_blocks_descriptors.size; i++) {
    if (arc.control_blocks_descriptors[i].type == DIR_BLOCK) {
      dir_idx = i;
      break;
    }
  }
  if (dir_idx < 0) {
    arc.arcfile.close();
    return false;
  }

  DIRECTORY_BLOCK dir(arc, arc.control_blocks_descriptors[dir_idx]);
  info.num_blocks = dir.num_of_blocks;
  for (int b = 0; b < dir.num_of_blocks; b++) {
    info.data_blocks.push_back(dir.data_block[b]);
  }

  char namebuf[MY_FILENAME_MAX * 4];
  for (int i = 0; i < dir.total_files; i++) {
    ExistingFile ef;
    ef.fullname = dir.fullname(i, namebuf);
    ef.size = dir.size[i];
    ef.mtime = (uint32)dir.time[i];
    ef.isdir = dir.isdir[i] ? 1 : 0;
    ef.crc = dir.crc[i];
    ef.mode = dir.mode[i];
    ef.is_symlink = dir.issymlink[i] ? true : false;
    ef.symlink_target = (dir.symlink_target[i] && dir.symlink_target[i][0]) ? dir.symlink_target[i] : "";
    ef.block_idx = 0;
    for (int b = 0; b < dir.num_of_blocks; b++) {
      if (i >= dir.block_start(b) && i < dir.block_end(b)) {
        ef.block_idx = b;
        break;
      }
    }
    info.files.push_back(ef);
  }
  arc.arcfile.close();
  return true;
}

struct DecompressContext {
  MYFILE *arcfile;
  FILESIZE bytes_left;
  FILESIZE bytes_to_write;
  FILE *cur_out;
  uint cur_crc;
  std::vector<std::string> file_paths;
  std::vector<uint64> file_sizes;
  std::vector<uint32> file_mtimes;
  std::vector<uint32> file_crcs;
  std::vector<uint32> file_modes;
  std::set<std::string> needed;
  std::string base_dir;
  int cur_idx;
  int last_needed_idx;
};

static int decompress_cb(const char *what, void *buf, int size, void *aux) {
  DecompressContext *ctx = (DecompressContext *)aux;
  if (strequ(what, "read")) {
    int to_read = (int)(ctx->bytes_left < (FILESIZE)size ? ctx->bytes_left : (FILESIZE)size);
    if (to_read <= 0) return 0;
    int got = ctx->arcfile->tryRead(buf, to_read);
    if (got > 0) ctx->bytes_left -= got;
    return got;
  }
  if (strequ(what, "write")) {
    int orig_size = size;
    char *p = (char *)buf;
    while (size > 0 && ctx->cur_idx < (int)ctx->file_paths.size()) {
      if (!ctx->cur_out && ctx->bytes_to_write > 0) {
        const std::string &fn = ctx->file_paths[ctx->cur_idx];
        if (ctx->needed.empty() || ctx->needed.count(fn)) {
          std::string full_dest = ctx->base_dir + "/" + fn;
          BuildPathTo((char*)full_dest.c_str());
          ctx->cur_out = fopen(full_dest.c_str(), "wb");
          ctx->cur_crc = INIT_CRC;
        }
      }
      int take = (int)(ctx->bytes_to_write < (FILESIZE)size ? ctx->bytes_to_write : (FILESIZE)size);
      if (ctx->cur_out && take > 0) {
        fwrite(p, 1, take, ctx->cur_out);
        ctx->cur_crc = UpdateCRC(p, take, ctx->cur_crc);
      }
      ctx->bytes_to_write -= take;
      p += take;
      size -= take;
      if (ctx->bytes_to_write == 0) {
        if (ctx->cur_out) {
          fclose(ctx->cur_out);
          ctx->cur_out = NULL;
          std::string full_dest = ctx->base_dir + "/" + ctx->file_paths[ctx->cur_idx];
          SetFileDateTime((char*)full_dest.c_str(), ctx->file_mtimes[ctx->cur_idx]);
          if (ctx->file_modes[ctx->cur_idx])
            chmod(full_dest.c_str(), ctx->file_modes[ctx->cur_idx] & 07777);
        }
        ctx->cur_idx++;
        if (ctx->last_needed_idx >= 0 && ctx->cur_idx > ctx->last_needed_idx) {
          return FREEARC_ERRCODE_NO_MORE_DATA_REQUIRED;
        }
        if (ctx->cur_idx < (int)ctx->file_paths.size()) {
          ctx->bytes_to_write = ctx->file_sizes[ctx->cur_idx];
        }
      }
    }
    return orig_size;
  }
  return 0;
}

static bool extract_preserved_files(const std::string &arcname, const std::string &dest_dir,
                                    const ExistingArchiveInfo &info,
                                    const std::set<std::string> &needed_files,
                                    const char *password) {
  if (needed_files.empty()) return true;

  SetArchivePassword((char*)password);
  ARCHIVE arc;
  arc.arcfile.open((char*)arcname.c_str(), READ_MODE);
  if (!arc.arcfile.isopen()) return false;
  arc.read_structure();

  int dir_idx = -1;
  for (int i = 0; i < arc.control_blocks_descriptors.size; i++) {
    if (arc.control_blocks_descriptors[i].type == DIR_BLOCK) {
      dir_idx = i;
      break;
    }
  }
  if (dir_idx < 0) { arc.arcfile.close(); return false; }
  DIRECTORY_BLOCK dir(arc, arc.control_blocks_descriptors[dir_idx]);

  char namebuf[MY_FILENAME_MAX * 4];
  for (int b = 0; b < dir.num_of_blocks; b++) {
    bool block_needed = false;
    int b_start = dir.block_start(b);
    int b_end = dir.block_end(b);
    for (int i = b_start; i < b_end; i++) {
      std::string fn = dir.fullname(i, namebuf);
      if (needed_files.count(fn)) {
        block_needed = true;
        break;
      }
    }

    for (int i = b_start; i < b_end; i++) {
      std::string fn = dir.fullname(i, namebuf);
      if (needed_files.count(fn)) {
        if (dir.isdir[i]) {
          std::string d = dest_dir + "/" + fn;
          create_dir_recursive(d);
#ifndef _WIN32
          if (dir.mode[i])
            chmod(d.c_str(), dir.mode[i] & 07777);
#endif
        } else if (dir.issymlink[i]) {
          std::string f = dest_dir + "/" + fn;
          BuildPathTo((char*)f.c_str());
#ifndef _WIN32
          unlink(f.c_str());
          symlink(dir.symlink_target[i], f.c_str());
#endif
        } else if (dir.size[i] == 0) {
          std::string f = dest_dir + "/" + fn;
          BuildPathTo((char*)f.c_str());
          FILE *fp = fopen(f.c_str(), "wb");
          if (fp) fclose(fp);
          SetFileDateTime((char*)f.c_str(), dir.time[i]);
#ifndef _WIN32
          if (dir.mode[i])
            chmod(f.c_str(), dir.mode[i] & 07777);
#endif
        }
      }
    }

    if (!block_needed) continue;

    DecompressContext ctx;
    ctx.arcfile = &arc.arcfile;
    ctx.bytes_left = dir.data_block[b].compsize;
    ctx.cur_out = NULL;
    ctx.cur_crc = 0;
    ctx.base_dir = dest_dir;
    ctx.cur_idx = 0;
    ctx.needed = needed_files;

    for (int i = b_start; i < b_end; i++) {
      if (!dir.isdir[i] && !dir.issymlink[i] && dir.size[i] > 0) {
        std::string fn = dir.fullname(i, namebuf);
        ctx.file_paths.push_back(fn);
        ctx.file_sizes.push_back(dir.size[i]);
        ctx.file_mtimes.push_back((uint32)dir.time[i]);
        ctx.file_crcs.push_back(dir.crc[i]);
        ctx.file_modes.push_back(dir.mode[i]);
      }
    }

    int last_idx = -1;
    for (int i = 0; i < (int)ctx.file_paths.size(); i++) {
      if (ctx.needed.count(ctx.file_paths[i])) last_idx = i;
    }
    ctx.last_needed_idx = last_idx;

    if (!ctx.file_paths.empty() && last_idx >= 0) {
      ctx.bytes_to_write = ctx.file_sizes[0];
      arc.arcfile.seek(dir.data_block[b].pos);
      char resolved[MAX_METHOD_STRLEN];
      int prc = ApplyPasswordToCompressor(dir.data_block[b].compressor, (char*)password, resolved);
      CHECK(prc >= 0, (s, "ERROR: wrong password or decryption failed for block %d", b));
      int rc = MultiDecompress(resolved, decompress_cb, &ctx);
      CHECK(rc >= 0 || rc == FREEARC_ERRCODE_NO_MORE_DATA_REQUIRED,
            (s, "ERROR: decompression failed (%d) on block %d", rc, b));
      if (ctx.cur_out) {
        fclose(ctx.cur_out);
        ctx.cur_out = NULL;
      }
    }
  }

  arc.arcfile.close();
  return true;
}

struct LogFileGuard {
  ~LogFileGuard() { close_logfile(); }
};

int main(int argc, char **argv) {
  LogFileGuard log_guard;
  char cmd = 'a';
  bool cmd_specified = false;
  int custom_threads = 0;
  uint64 limit_decomp_mem = 0;
  uint64 limit_comp_mem = 0;
  std::string logfile_path;
  std::string diskpath;
  const char *method = NULL;
  const char *password = NULL;
  const char *hp_password = NULL;
  const char *algorithm = "aes";
  int argi = 1;
  int recurse = 1;
  int solid = 1;
  uint64 solid_block_size = 0;
  int solid_block_files = 0;
  int ep = 0;
  int noarcext = 0;
  bool sort_files = true;
  bool create_sfx = false;
  std::string sfx_stub_path;
  uint64 vol_size = 0;
  std::string comment;
  std::string arcprefix;
  std::vector<std::string> excludes;
  std::vector<std::string> includes;
  std::vector<std::string> inputs;
  std::vector<std::string> filters;
  std::string arcname;

  if (argc < 2) {
    fputs(g_usage, stdout);
    return argc == 1 ? 0 : FREEARC_EXIT_ERROR;
  }

  // Check if this is an unpacker command (l, v, t, e, x, p, list, test, extract) or pipe flag (-so)
  for (int i = 1; i < argc; i++) {
    if (strequ(argv[i], "--")) break;
    if (strequ(argv[i], "-so")) {
      std::string unarc_bin = find_unarc_binary(argv[0]);
#ifdef _WIN32
      int rc = _spawnvp(_P_WAIT, unarc_bin.c_str(), (const char * const *)argv);
      if (rc < 0) {
        err_msg("ERROR: failed to execute unarc (%s): %s\n", unarc_bin.c_str(), strerror(errno));
        return FREEARC_EXIT_ERROR;
      }
      return rc;
#else
      execvp(unarc_bin.c_str(), argv);
      err_msg("ERROR: failed to execute unarc (%s): %s\n", unarc_bin.c_str(), strerror(errno));
      return FREEARC_EXIT_ERROR;
#endif
    }
    if (argv[i][0] != '-') {
      if (is_unpacker_cmd(argv[i])) {
        std::string unarc_bin = find_unarc_binary(argv[0]);
#ifdef _WIN32
        int rc = _spawnvp(_P_WAIT, unarc_bin.c_str(), (const char * const *)argv);
        if (rc < 0) {
          err_msg("ERROR: failed to execute unarc (%s): %s\n", unarc_bin.c_str(), strerror(errno));
          return FREEARC_EXIT_ERROR;
        }
        return rc;
#else
        execvp(unarc_bin.c_str(), argv);
        err_msg("ERROR: failed to execute unarc (%s): %s\n", unarc_bin.c_str(), strerror(errno));
        return FREEARC_EXIT_ERROR;
#endif
      }
      break;
    }
  }
  bool stop_opts = false;
  while (argi < argc) {
    const char *a = argv[argi];
    if (!stop_opts && strequ(a, "--")) {
      stop_opts = true;
      argi++;
      continue;
    }
    if (!stop_opts && a[0] == '-' && a[1] != '\0') {
      std::string opt_val;
      bool opt_next = false;
      if (strequ(a, "-h") || strequ(a, "--help")) {
        fputs(g_usage, stdout);
        return 0;
      }
      if (strequ(a, "-q")) g_quiet = 1;
      else if (strequ(a, "-sfx")) {
        create_sfx = true;
      }
      else if (start_with(a, "-sfx=")) {
        create_sfx = true;
        sfx_stub_path = a + 5;
      }
      else if (strequ(a, "-s")) {
        solid = 1;
        solid_block_size = 0;
        solid_block_files = 0;
      }
      else if (strequ(a, "-s-")) {
        solid = 0;
        solid_block_size = 0;
        solid_block_files = 0;
      }
      else if (start_with(a, "-s")) {
        const char *sp = a + 2;
        if (*sp == '=') sp++;
        size_t slen = strlen(sp);
        if (slen > 0 && (sp[slen - 1] == 'f' || sp[slen - 1] == 'F')) {
          solid = 2;
          solid_block_files = atoi(sp);
        } else {
          solid = 2;
          solid_block_size = parse_vol_size(sp);
        }
      }
      else if (strequ(a, "-r")) recurse = 1;
      else if (strequ(a, "-r-")) recurse = 0;
      else if (strequ(a, "-ep")) ep = 1;
      else if (strequ(a, "-ep1")) ep = 2;
      else if (strequ(a, "--noarcext")) noarcext = 1;
      else if (strequ(a, "-bcj") || strequ(a, "--bcj") || strequ(a, "-exe") || strequ(a, "--exe")) filters.push_back("exe");
      else if (strequ(a, "-rep") || strequ(a, "--rep")) filters.push_back("rep:64m");
      else if (strequ(a, "-delta") || strequ(a, "--delta")) filters.push_back("delta");
      else if (strequ(a, "-dict") || strequ(a, "--dict")) filters.push_back("dict");
      else if (strequ(a, "-lzp") || strequ(a, "--lzp")) filters.push_back("lzp");
      else if (strequ(a, "-mm") || strequ(a, "--mm")) filters.push_back("mm");
      else if (strequ(a, "-tta") || strequ(a, "--tta")) filters.push_back("tta");
      else if (is_diskpath_opt(a, opt_val, opt_next)) {
        if (opt_next && argi + 1 < argc) diskpath = clean_option_path(argv[++argi]);
        else diskpath = clean_option_path(opt_val);
      }
      else if (is_multithread_opt(a, opt_val, opt_next)) {
        if (opt_next && argi + 1 < argc && argv[argi + 1][0] != '-') custom_threads = parse_threads_val(argv[++argi]);
        else custom_threads = parse_threads_val(opt_val.c_str());
      }
      else if (is_limit_decomp_opt(a, opt_val, opt_next)) {
        if (opt_next && argi + 1 < argc && argv[argi + 1][0] != '-') limit_decomp_mem = parse_mem_limit(argv[++argi]);
        else limit_decomp_mem = parse_mem_limit(opt_val.c_str());
      }
      else if (is_limit_comp_opt(a, opt_val, opt_next)) {
        if (opt_next && argi + 1 < argc && argv[argi + 1][0] != '-') limit_comp_mem = parse_mem_limit(argv[++argi]);
        else limit_comp_mem = parse_mem_limit(opt_val.c_str());
      }
      else if (is_logfile_opt(a, opt_val, opt_next)) {
        if (opt_next && argi + 1 < argc && argv[argi + 1][0] != '-') logfile_path = clean_option_path(argv[++argi]);
        else logfile_path = clean_option_path(opt_val);
        open_logfile(logfile_path);
      }
      else if (strcasecmp(a, "-w") == 0 || strcasecmp(a, "--workdir") == 0) {
        if (argi + 1 < argc && argv[argi + 1][0] != '-') argi++;
      }
      else if (start_with(a, "-w") || strncasecmp(a, "--workdir=", 10) == 0) {}
      else if (strcasecmp(a, "-ds-") == 0 || strcasecmp(a, "--sort=-") == 0) {
        sort_files = false;
      }
      else if (start_with(a, "-ds") || start_with(a, "--sort")) {
        sort_files = true;
      }
      else if (strncasecmp(a, "-di", 3) == 0 ||
               strncasecmp(a, "-ed", 3) == 0 || strcasecmp(a, "-o+") == 0 ||
               strcasecmp(a, "-o-") == 0 || strcasecmp(a, "-y") == 0 ||
               strcasecmp(a, "-n") == 0 || strcasecmp(a, "--nodir") == 0 ||
               strcasecmp(a, "--no-pause") == 0 || strcasecmp(a, "-kb") == 0 ||
               strcasecmp(a, "-ka") == 0 || strcasecmp(a, "-kc") == 0) {}
      else if (strequ(a, "-v") && argi + 1 < argc) vol_size = parse_vol_size(argv[++argi]);
      else if (start_with(a, "-v")) vol_size = parse_vol_size(a + 2);
      else if (strequ(a, "-m") && argi + 1 < argc) method = argv[++argi];
      else if (start_with(a, "-m")) method = a + 2;
      else if (strequ(a, "-z") && argi + 1 < argc) comment = load_comment(argv[++argi]);
      else if (start_with(a, "-z")) comment = load_comment(a + 2);
      else if (strequ(a, "-x") && argi + 1 < argc) add_pattern_arg(argv[++argi], excludes, diskpath);
      else if (start_with(a, "-x")) add_pattern_arg(a + 2, excludes, diskpath);
      else if (strequ(a, "-i") && argi + 1 < argc) add_pattern_arg(argv[++argi], includes, diskpath);
      else if (start_with(a, "-i")) add_pattern_arg(a + 2, includes, diskpath);
      else if (strequ(a, "-ap") && argi + 1 < argc) arcprefix = argv[++argi];
      else if (start_with(a, "-ap")) arcprefix = a + 3;
      else if (strequ(a, "-hp") && argi + 1 < argc) hp_password = argv[++argi];
      else if (start_with(a, "-hp")) hp_password = a + 3;
      else if (strequ(a, "-p") && argi + 1 < argc) password = argv[++argi];
      else if (start_with(a, "-p")) password = a + 2;
      else if (strequ(a, "-ae") && argi + 1 < argc) algorithm = argv[++argi];
      else if (start_with(a, "-ae")) algorithm = a + 3;
      else {
        err_msg("WARNING: unrecognized legacy option '%s' ignored\n", a);
      }
    } else {
      if (!cmd_specified) {
        char parsed_cmd = parse_arc_cmd(a);
        if (parsed_cmd) {
          cmd = parsed_cmd;
          cmd_specified = true;
          argi++;
          continue;
        }
        cmd = 'a';
        cmd_specified = true;
      }
      if (arcname.empty()) {
        arcname = ensure_arc_ext(clean_option_path(a).c_str(), noarcext, create_sfx || cmd == 's');
      } else {
        if (a[0] == '@')
          read_listfile(clean_option_path(a + 1).c_str(), inputs, diskpath);
        else
          inputs.push_back(clean_option_path(a));
      }
    }
    argi++;
  }
  while (!arcprefix.empty() && arcprefix[arcprefix.size() - 1] == '/')
    arcprefix.resize(arcprefix.size() - 1);
  if (arcname.empty()) {
    fputs(g_usage, stdout);
    return FREEARC_EXIT_ERROR;
  }
  if (cmd == 'd') {
    CHECK(!inputs.empty(), (s, "ERROR: specify files or patterns to delete"));
  } else if (cmd == 's') {
    // SFX conversion: destination can be omitted or specified
  } else {
    CHECK(!inputs.empty(), (s, "ERROR: no files to archive"));
  }

  if (vol_size > 0 && vol_size < 10240) {
    err_msg("ERROR: volume size must be at least 10KB (e.g. -v10m or -v500k)\n");
    return FREEARC_EXIT_ERROR;
  }

  if (password && !*password) password = NULL;
  if (hp_password && !*hp_password) hp_password = NULL;
  if (hp_password && !password) password = hp_password;

  bool auto_m = method && is_auto_method_name(method);
  std::vector<std::string> extra_filters;
  std::string base_method;
  if (auto_m) {
    base_method = method;
    extra_filters = filters;
  } else {
    base_method = method ? map_method(method) : (!filters.empty() ? "lzma:8m" : "storing");
    extra_filters.clear();
  }
  std::string final_method = auto_m ? base_method : apply_extra_filters(base_method, filters);
  if (limit_decomp_mem > 0 || limit_comp_mem > 0) {
    final_method = apply_mem_limits(final_method, limit_decomp_mem, limit_comp_mem);
  }
  std::string method_str = final_method;
  method = method_str.c_str();

  int num_threads = (custom_threads > 0) ? custom_threads : GetProcessorsCount();
  SetCompressionThreads(num_threads);
  method = method_str.c_str();

  ExistingArchiveInfo existing_info;
  bool archive_exists = inspect_archive(arcname, password ? password : hp_password, existing_info);

  if (cmd == 's') {
    CHECK(archive_exists, (s, "ERROR: source archive %s not found", arcname.c_str()));
    if (existing_info.is_sfx) {
      msg("Archive %s is already a self-extracting executable.\nAll OK\n", arcname.c_str());
      return 0;
    }
    std::string sfx_dest;
    if (!inputs.empty()) {
      sfx_dest = inputs[0];
    } else {
      sfx_dest = arcname;
      size_t dot = sfx_dest.rfind('.');
      if (dot != std::string::npos && strcasecmp(sfx_dest.c_str() + dot, ".arc") == 0)
        sfx_dest = sfx_dest.substr(0, dot) + ".sfx";
      else
        sfx_dest += ".sfx";
    }
    std::string stub = find_sfx_stub(argv[0], sfx_stub_path);
    CHECK(!stub.empty(), (s, "ERROR: SFX stub 'arc.sfx' not found. Ensure build/linux/arc.sfx exists or specify -sfx=<stub>"));

    std::string tmp_dest = sfx_dest + ".tmp_sfx";
    ensure_parent_dirs(tmp_dest);
    FILE *fout = fopen(tmp_dest.c_str(), "wb");
    CHECK(fout, (s, "ERROR: cannot create output file %s (%s)", tmp_dest.c_str(), strerror(errno)));

    FILE *fstub = fopen(stub.c_str(), "rb");
    CHECK(fstub, (s, "ERROR: cannot open SFX stub %s (%s)", stub.c_str(), strerror(errno)));
    char buf[65536];
    size_t nr;
    uint64 total_written = 0;
    while ((nr = fread(buf, 1, sizeof(buf), fstub)) > 0) {
      fwrite(buf, 1, nr, fout);
      total_written += nr;
    }
    fclose(fstub);

    FILE *farc = fopen(arcname.c_str(), "rb");
    CHECK(farc, (s, "ERROR: cannot open source archive %s (%s)", arcname.c_str(), strerror(errno)));
    while ((nr = fread(buf, 1, sizeof(buf), farc)) > 0) {
      fwrite(buf, 1, nr, fout);
      total_written += nr;
    }
    fclose(farc);
    fclose(fout);

    unlink(sfx_dest.c_str());
    rename(tmp_dest.c_str(), sfx_dest.c_str());
    chmod(sfx_dest.c_str(), 0755);
    msg("Converted %s -> %s (%llu bytes)\nAll OK\n", arcname.c_str(), sfx_dest.c_str(), (unsigned long long)total_written);
    return 0;
  }

  std::string active_sfx_stub;
  if (create_sfx || (archive_exists && existing_info.is_sfx)) {
    active_sfx_stub = find_sfx_stub(argv[0], sfx_stub_path);
    CHECK(!active_sfx_stub.empty(),
          (s, "ERROR: SFX stub 'arc.sfx' not found. Ensure build/linux/arc.sfx exists or specify -sfx=<stub>"));
  }

  if (cmd == 'd') {
    CHECK(archive_exists, (s, "ERROR: archive %s not found", arcname.c_str()));
    std::set<std::string> to_delete;
    std::set<std::string> to_preserve;
    for (size_t i = 0; i < existing_info.files.size(); i++) {
      const ExistingFile &ef = existing_info.files[i];
      bool match = false;
      const char *base = strrchr(ef.fullname.c_str(), '/');
      base = base ? base + 1 : ef.fullname.c_str();
      for (size_t p = 0; p < inputs.size(); p++) {
        if (glob_match(inputs[p].c_str(), ef.fullname.c_str()) ||
            glob_match(inputs[p].c_str(), base)) {
          match = true;
          break;
        }
      }
      if (match && excluded(ef.fullname, excludes)) {
        match = false;
      }
      if (match) {
        to_delete.insert(ef.fullname);
        msg("Deleting %s\n", ef.fullname.c_str());
      } else {
        to_preserve.insert(ef.fullname);
      }
    }
    if (to_delete.empty()) {
      msg("No matching files found to delete.\nAll OK\n");
      return 0;
    }
    if (to_preserve.empty()) {
      unlink(arcname.c_str());
      for (int i = 1; i <= 9999; i++) {
        char vbuf[MY_FILENAME_MAX * 4];
        snprintf(vbuf, sizeof(vbuf), "%s.%03d", arcname.c_str(), i);
        if (unlink(vbuf) != 0) break;
      }
      msg("All files deleted from archive.\nAll OK\n");
      return 0;
    }
    std::string temp_dir = make_temp_dir();
    CHECK(extract_preserved_files(arcname, temp_dir, existing_info, to_preserve, password ? password : hp_password),
          (s, "ERROR: failed to extract preserved files for repacking"));

    std::vector<FileRec> new_files;
    for (size_t i = 0; i < existing_info.files.size(); i++) {
      const ExistingFile &ef = existing_info.files[i];
      if (to_preserve.count(ef.fullname)) {
        FileRec f;
        f.path = temp_dir + "/" + ef.fullname;
        split_path(ef.fullname.c_str(), f.dir, f.name);
        f.size = ef.size;
        f.mtime = ef.mtime;
        f.isdir = ef.isdir;
        f.crc = ef.crc;
        f.mode = ef.mode;
        f.is_symlink = ef.is_symlink;
        f.symlink_target = ef.symlink_target;
        f.datatype.clear();
        new_files.push_back(f);
      }
    }

    if (vol_size == 0 && existing_info.is_multivol) {
      vol_size = existing_info.first_vol_size;
    }
    std::string tmp_arc = arcname + ".tmp_update";
    std::vector<std::string> out_vols;
    uint64 total_w = 0;
    write_archive(tmp_arc, vol_size, new_files, solid, method, password, hp_password, algorithm,
                  comment.empty() ? existing_info.comment : comment, out_vols, total_w, active_sfx_stub, solid_block_size, solid_block_files, extra_filters);
    replace_archive_files(arcname, tmp_arc, vol_size, !active_sfx_stub.empty());
    remove_dir_recursive(temp_dir);
    if (vol_size > 0 && out_vols.size() > 1) {
      msg("Created %d volumes (%llu bytes total)\n", (int)out_vols.size(), (unsigned long long)total_w);
    }
    msg("Deleted %d file(s)\nAll OK\n", (int)to_delete.size());
    return 0;
  }

  if (cmd == 'f') {
    CHECK(archive_exists, (s, "ERROR: archive %s not found", arcname.c_str()));
    struct stat arc_st;
    const struct stat *skip_arc = NULL;
    std::string check_arc = (vol_size > 0) ? (arcname + ".001") : arcname;
    if (stat(check_arc.c_str(), &arc_st) == 0 && S_ISREG(arc_st.st_mode))
      skip_arc = &arc_st;

    std::vector<FileRec> disk_files;
    std::set<std::string> seen;
    for (size_t i = 0; i < inputs.size(); i++) {
      walk_input(disk_files, seen, inputs[i], diskpath, skip_arc, recurse, ep, excludes, includes, arcprefix, check_arc.c_str());
    }
    std::map<std::string, ExistingFile> arc_map;
    for (size_t i = 0; i < existing_info.files.size(); i++) {
      arc_map[existing_info.files[i].fullname] = existing_info.files[i];
    }
    std::set<std::string> to_update_from_disk;
    for (size_t i = 0; i < disk_files.size(); i++) {
      std::string st = join_stored(disk_files[i].dir, disk_files[i].name.c_str());
      if (arc_map.find(st) != arc_map.end()) {
        const ExistingFile &ef = arc_map[st];
        bool needs_freshen = false;
        if (disk_files[i].mtime > ef.mtime || disk_files[i].size != ef.size)
          needs_freshen = true;
        else if ((disk_files[i].mode & 07777) != (ef.mode & 07777))
          needs_freshen = true;
        else if (disk_files[i].is_symlink != ef.is_symlink)
          needs_freshen = true;
        else if (disk_files[i].is_symlink && disk_files[i].symlink_target != ef.symlink_target)
          needs_freshen = true;

        if (needs_freshen) {
          to_update_from_disk.insert(st);
        }
      }
    }
    if (to_update_from_disk.empty()) {
      msg("Archive is up to date, nothing to freshen.\nAll OK\n");
      return 0;
    }
    std::set<std::string> to_preserve;
    for (size_t i = 0; i < existing_info.files.size(); i++) {
      const ExistingFile &ef = existing_info.files[i];
      if (to_update_from_disk.find(ef.fullname) == to_update_from_disk.end()) {
        to_preserve.insert(ef.fullname);
      }
    }
    std::string temp_dir = make_temp_dir();
    CHECK(extract_preserved_files(arcname, temp_dir, existing_info, to_preserve, password ? password : hp_password),
          (s, "ERROR: failed to extract preserved files for repacking"));

    std::vector<FileRec> combined_files;
    for (size_t i = 0; i < existing_info.files.size(); i++) {
      const ExistingFile &ef = existing_info.files[i];
      if (to_preserve.count(ef.fullname)) {
        FileRec f;
        f.path = temp_dir + "/" + ef.fullname;
        split_path(ef.fullname.c_str(), f.dir, f.name);
        f.size = ef.size;
        f.mtime = ef.mtime;
        f.isdir = ef.isdir;
        f.crc = ef.crc;
        f.mode = ef.mode;
        f.is_symlink = ef.is_symlink;
        f.symlink_target = ef.symlink_target;
        f.datatype.clear();
        combined_files.push_back(f);
      }
    }
    for (size_t i = 0; i < disk_files.size(); i++) {
      std::string st = join_stored(disk_files[i].dir, disk_files[i].name.c_str());
      if (to_update_from_disk.count(st)) {
        combined_files.push_back(disk_files[i]);
      }
    }
    if (vol_size == 0 && existing_info.is_multivol) {
      vol_size = existing_info.first_vol_size;
    }
    std::string tmp_arc = arcname + ".tmp_update";
    std::vector<std::string> out_vols;
    uint64 total_w = 0;
    write_archive(tmp_arc, vol_size, combined_files, solid, method, password, hp_password, algorithm,
                  comment.empty() ? existing_info.comment : comment, out_vols, total_w, active_sfx_stub, solid_block_size, solid_block_files, extra_filters);
    replace_archive_files(arcname, tmp_arc, vol_size, !active_sfx_stub.empty());
    remove_dir_recursive(temp_dir);
    if (vol_size > 0 && out_vols.size() > 1) {
      msg("Created %d volumes (%llu bytes total)\n", (int)out_vols.size(), (unsigned long long)total_w);
    }
    msg("Freshened %d file(s)\nAll OK\n", (int)to_update_from_disk.size());
    return 0;
  }

  if (cmd == 'u' && archive_exists) {
    struct stat arc_st;
    const struct stat *skip_arc = NULL;
    std::string check_arc = (vol_size > 0) ? (arcname + ".001") : arcname;
    if (stat(check_arc.c_str(), &arc_st) == 0 && S_ISREG(arc_st.st_mode))
      skip_arc = &arc_st;

    std::vector<FileRec> disk_files;
    std::set<std::string> seen;
    for (size_t i = 0; i < inputs.size(); i++) {
      walk_input(disk_files, seen, inputs[i], diskpath, skip_arc, recurse, ep, excludes, includes, arcprefix, check_arc.c_str());
    }
    std::map<std::string, ExistingFile> arc_map;
    for (size_t i = 0; i < existing_info.files.size(); i++) {
      arc_map[existing_info.files[i].fullname] = existing_info.files[i];
    }
    std::set<std::string> to_update_from_disk;
    int added_count = 0, updated_count = 0;
    for (size_t i = 0; i < disk_files.size(); i++) {
      std::string st = join_stored(disk_files[i].dir, disk_files[i].name.c_str());
      if (arc_map.find(st) != arc_map.end()) {
        const ExistingFile &ef = arc_map[st];
        bool needs_update = false;
        if (disk_files[i].mtime > ef.mtime || disk_files[i].size != ef.size)
          needs_update = true;
        else if ((disk_files[i].mode & 07777) != (ef.mode & 07777))
          needs_update = true;
        else if (disk_files[i].is_symlink != ef.is_symlink)
          needs_update = true;
        else if (disk_files[i].is_symlink && disk_files[i].symlink_target != ef.symlink_target)
          needs_update = true;

        if (needs_update) {
          to_update_from_disk.insert(st);
          updated_count++;
        }
      } else {
        to_update_from_disk.insert(st);
        added_count++;
      }
    }
    if (to_update_from_disk.empty()) {
      msg("Archive is up to date, nothing to update.\nAll OK\n");
      return 0;
    }
    std::set<std::string> to_preserve;
    for (size_t i = 0; i < existing_info.files.size(); i++) {
      const ExistingFile &ef = existing_info.files[i];
      if (to_update_from_disk.find(ef.fullname) == to_update_from_disk.end()) {
        to_preserve.insert(ef.fullname);
      }
    }
    std::string temp_dir = make_temp_dir();
    CHECK(extract_preserved_files(arcname, temp_dir, existing_info, to_preserve, password ? password : hp_password),
          (s, "ERROR: failed to extract preserved files for repacking"));

    std::vector<FileRec> combined_files;
    for (size_t i = 0; i < existing_info.files.size(); i++) {
      const ExistingFile &ef = existing_info.files[i];
      if (to_preserve.count(ef.fullname)) {
        FileRec f;
        f.path = temp_dir + "/" + ef.fullname;
        split_path(ef.fullname.c_str(), f.dir, f.name);
        f.size = ef.size;
        f.mtime = ef.mtime;
        f.isdir = ef.isdir;
        f.crc = ef.crc;
        f.mode = ef.mode;
        f.is_symlink = ef.is_symlink;
        f.symlink_target = ef.symlink_target;
        f.datatype.clear();
        combined_files.push_back(f);
      }
    }
    for (size_t i = 0; i < disk_files.size(); i++) {
      std::string st = join_stored(disk_files[i].dir, disk_files[i].name.c_str());
      if (to_update_from_disk.count(st)) {
        combined_files.push_back(disk_files[i]);
      }
    }
    if (vol_size == 0 && existing_info.is_multivol) {
      vol_size = existing_info.first_vol_size;
    }
    std::string tmp_arc = arcname + ".tmp_update";
    std::vector<std::string> out_vols;
    uint64 total_w = 0;
    write_archive(tmp_arc, vol_size, combined_files, solid, method, password, hp_password, algorithm,
                  comment.empty() ? existing_info.comment : comment, out_vols, total_w, active_sfx_stub, solid_block_size, solid_block_files, extra_filters);
    replace_archive_files(arcname, tmp_arc, vol_size, !active_sfx_stub.empty());
    remove_dir_recursive(temp_dir);
    if (vol_size > 0 && out_vols.size() > 1) {
      msg("Created %d volumes (%llu bytes total)\n", (int)out_vols.size(), (unsigned long long)total_w);
    }
    msg("Updated %d file(s), added %d new file(s)\nAll OK\n", updated_count, added_count);
    return 0;
  }

  if ((cmd == 'a' || cmd == 'm') && archive_exists) {
    struct stat arc_st;
    const struct stat *skip_arc = NULL;
    std::string check_arc = (vol_size > 0) ? (arcname + ".001") : arcname;
    if (stat(check_arc.c_str(), &arc_st) == 0 && S_ISREG(arc_st.st_mode))
      skip_arc = &arc_st;

    std::vector<FileRec> disk_files;
    std::set<std::string> seen;
    for (size_t i = 0; i < inputs.size(); i++) {
      walk_input(disk_files, seen, inputs[i], diskpath, skip_arc, recurse, ep, excludes, includes, arcprefix, check_arc.c_str());
    }
    std::set<std::string> disk_set;
    for (size_t i = 0; i < disk_files.size(); i++) {
      disk_set.insert(join_stored(disk_files[i].dir, disk_files[i].name.c_str()));
    }
    std::set<std::string> to_preserve;
    for (size_t i = 0; i < existing_info.files.size(); i++) {
      if (disk_set.find(existing_info.files[i].fullname) == disk_set.end()) {
        to_preserve.insert(existing_info.files[i].fullname);
      }
    }
    std::string temp_dir = make_temp_dir();
    CHECK(extract_preserved_files(arcname, temp_dir, existing_info, to_preserve, password ? password : hp_password),
          (s, "ERROR: failed to extract preserved files for repacking"));

    std::vector<FileRec> combined_files;
    for (size_t i = 0; i < existing_info.files.size(); i++) {
      const ExistingFile &ef = existing_info.files[i];
      if (to_preserve.count(ef.fullname)) {
        FileRec f;
        f.path = temp_dir + "/" + ef.fullname;
        split_path(ef.fullname.c_str(), f.dir, f.name);
        f.size = ef.size;
        f.mtime = ef.mtime;
        f.isdir = ef.isdir;
        f.crc = ef.crc;
        f.mode = ef.mode;
        f.is_symlink = ef.is_symlink;
        f.symlink_target = ef.symlink_target;
        f.datatype.clear();
        combined_files.push_back(f);
      }
    }
    for (size_t i = 0; i < disk_files.size(); i++) {
      combined_files.push_back(disk_files[i]);
    }
    if (vol_size == 0 && existing_info.is_multivol) {
      vol_size = existing_info.first_vol_size;
    }
    std::string tmp_arc = arcname + ".tmp_update";
    std::vector<std::string> out_vols;
    uint64 total_w = 0;
    write_archive(tmp_arc, vol_size, combined_files, solid, method, password, hp_password, algorithm,
                  comment.empty() ? existing_info.comment : comment, out_vols, total_w, active_sfx_stub, solid_block_size, solid_block_files, extra_filters);
    replace_archive_files(arcname, tmp_arc, vol_size, !active_sfx_stub.empty());
    remove_dir_recursive(temp_dir);
    if (vol_size > 0 && out_vols.size() > 1) {
      msg("Created %d volumes (%llu bytes total)\n", (int)out_vols.size(), (unsigned long long)total_w);
    }
    if (cmd == 'm') {
      int moved = 0;
      for (size_t i = 0; i < disk_files.size(); i++) {
        if (!disk_files[i].isdir) {
          if (unlink(disk_files[i].path.c_str()) == 0) moved++;
        }
      }
      for (int i = (int)disk_files.size() - 1; i >= 0; i--) {
        if (disk_files[i].isdir) rmdir(disk_files[i].path.c_str());
      }
      msg("Moved %d file(s) into archive.\n", moved);
    }
    msg("All OK\n");
    return 0;
  }

  // Creating new archive
  struct stat arc_st;
  const struct stat *skip_arc = NULL;
  std::string check_arc = (vol_size > 0) ? (arcname + ".001") : arcname;
  if (stat(check_arc.c_str(), &arc_st) == 0 && S_ISREG(arc_st.st_mode))
    skip_arc = &arc_st;

  std::vector<FileRec> files;
  std::set<std::string> seen;
  for (size_t i = 0; i < inputs.size(); i++) {
    walk_input(files, seen, inputs[i], diskpath, skip_arc, recurse, ep, excludes, includes, arcprefix, check_arc.c_str());
  }
  CHECK(!files.empty(), (s, "ERROR: no files to archive"));

  std::vector<std::string> out_vols;
  uint64 total_w = 0;
  write_archive(arcname, vol_size, files, solid, method, password, hp_password, algorithm, comment, out_vols, total_w, active_sfx_stub, solid_block_size, solid_block_files, extra_filters, sort_files);
  if (!active_sfx_stub.empty()) {
    chmod(arcname.c_str(), 0755);
    msg("Created SFX archive %s (%llu bytes)\n", arcname.c_str(), (unsigned long long)total_w);
  }

  if (vol_size > 0 && out_vols.size() > 1) {
    msg("Created %d volumes (%llu bytes total)\n", (int)out_vols.size(), (unsigned long long)total_w);
  }
  if (cmd == 'm') {
    int moved = 0;
    for (size_t i = 0; i < files.size(); i++) {
      if (!files[i].isdir) {
        if (unlink(files[i].path.c_str()) == 0) moved++;
      }
    }
    for (int i = (int)files.size() - 1; i >= 0; i--) {
      if (files[i].isdir) rmdir(files[i].path.c_str());
    }
    msg("Moved %d file(s) into archive.\n", moved);
  }
  msg("All OK\n");
  return 0;
}
