#undef ON_CHECK_FAIL
#define ON_CHECK_FAIL()  do { fprintf(stderr, "\n"); exit(FREEARC_EXIT_ERROR); } while (0)

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

#include "Environment.h"
#include "Compression/Compression.h"
#include "WinCompat.h"
#ifdef stat
#undef stat
#endif

#include "ArcStructure.h"
#define aARCHIVE_VERSION   make4byte(0,0,5,9)
#define aNO_COMPRESSION    "storing"

static const char *g_usage =
  "Usage: arc <command> [options] archive[.arc] [files...]\n"
  "Commands:\n"
  "  a           add files to archive (default)\n"
  "  u           update newer files and add new files\n"
  "  f           freshen existing files in archive (no new files added)\n"
  "  d           delete files/patterns from archive\n"
  "  m           move files to archive (delete from disk after archiving)\n"
  "  l           display archive listing (delegates to unarc)\n"
  "  v           display verbose archive listing with Unix attributes\n"
  "  t           test archive integrity\n"
  "  e           extract files without pathnames\n"
  "  x           extract files with pathnames\n"
  "  p           extract files to standard output (pipe)\n"
  "  s           convert archive to self-extracting executable (SFX)\n"
  "Options:\n"
  "  -m METHOD   storing, lzma, tor, ppmd, grzip, tta\n"
  "              levels: -m0..-m5, -mx, -m4x, -m5x\n"
  "              chains: -m exe+lzma, -m rep+lzma, -m dict+ppmd, etc.\n"
  "  -bcj        activate x86 executable filter (alias: -exe)\n"
  "  -rep        activate repetition filter (rep:64m)\n"
  "  -delta      activate delta filter\n"
  "  -dict       activate dictionary filter\n"
  "  -lzp        activate LZP filter\n"
  "  -mm         activate multimedia filter\n"
  "  -v<size>    create multi-volume archive (e.g. -v10m, -v1440k)\n"
  "  -s          solid archive (default)\n"
  "  -s-         one data block per file (nonsolid)\n"
  "  -r          recurse directories (default)\n"
  "  -r-         do not recurse\n"
  "  -ep         store basenames only\n"
  "  -ep1        strip the first path component\n"
  "  -sfx[=STUB] create self-extracting executable (.sfx/.run)\n"
  "  -ap<path>   set archive path prefix\n"
  "  -z TEXT     archive comment (or -zFILE to read a file)\n"
  "  -x PATTERN  exclude files matching pattern (@listfile supported)\n"
  "  -i PATTERN  include only files matching pattern (@listfile supported)\n"
  "  -so         write extracted files to stdout\n"
  "  -p PASS     encrypt data blocks\n"
  "  -hp PASS    encrypt headers and data\n"
  "  -ae ALG     aes (default), aes-128, aes-256, blowfish, twofish, serpent\n"
  "  -q          quiet\n"
  "  --noarcext  do not append .arc to the archive name\n"
  "  @LIST       read file names from LIST (one path per line)\n";

static int g_quiet = 0;

static void msg(const char *fmt, ...) {
  if (g_quiet) return;
  va_list ap;
  va_start(ap, fmt);
  vprintf(fmt, ap);
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

class VolumeWriter {
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
    cur_f = fopen(vname.c_str(), "wb");
    if (!cur_f) {
      fprintf(stderr, "ERROR: can't create volume %s (%s)\n", vname.c_str(), strerror(errno));
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
        fprintf(stderr, "ERROR: volume write failed\n");
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
  FileRec      *files;
  int           nfiles;
  int           idx;
  int           end;
  FILE         *cur;
  uint64        left_in_file;
  uint          file_crc;
  VolumeWriter *out;
  uint64        written;
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
    return "tor:1";
  if (strequ(m, "2"))
    return "tor:2";
  if (strequ(m, "3"))
    return "tor:3";
  if (strequ(m, "4"))
    return "lzma:8m";
  if (strequ(m, "5"))
    return "lzma:16m";
  if (strequ(m, "4x"))
    return "rep:64m+lzma:8m";
  if (strequ(m, "5x") || strequ(m, "x") || strequ(m, "mx") || strequ(m, "max") || strequ(m, "ultra"))
    return "rep:64m+lzma:16m";
  if (strequ(m, "lzma"))
    return "lzma:8m";
  if (strequ(m, "tor") || strequ(m, "tornado"))
    return "tor:3";
  if (strequ(m, "ppmd"))
    return "ppmd:8:16m";
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
    return "grzip";
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

static std::string normalize_stored(const char *path) {
  while (*path == '/') path++;
  while (path[0] == '.' && path[1] == '/') path += 2;
  std::string s = path;
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

static void read_listfile(const char *list, std::vector<std::string> &paths);

static void add_pattern_arg(const char *arg, std::vector<std::string> &list) {
  if (!arg || !*arg) return;
  if (arg[0] == '@') read_listfile(arg + 1, list);
  else list.push_back(arg);
}

static std::string find_unarc_binary(const char *argv0) {
  if (argv0) {
    std::string s = argv0;
    size_t slash = s.rfind('/');
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
#else
  if (access("build/linux/unarc", X_OK) == 0) return "build/linux/unarc";
  if (access("../build/linux/unarc", X_OK) == 0) return "../build/linux/unarc";
  if (access("./unarc", X_OK) == 0) return "./unarc";
#endif
  return "unarc";
}

static std::string find_sfx_stub(const char *argv0, const std::string &custom) {
  if (!custom.empty()) {
    if (access(custom.c_str(), R_OK) == 0) return custom;
    fprintf(stderr, "WARNING: specified SFX stub '%s' not found, searching defaults...\n", custom.c_str());
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

static void read_listfile(const char *list, std::vector<std::string> &paths) {
  FILE *f = fopen(list, "r");
  CHECK(f, (s, "ERROR: can't open listfile %s", list));
  char line[MY_FILENAME_MAX * 4];
  while (fgets(line, sizeof(line), f)) {
    char *p = line;
    while (*p == ' ' || *p == '\t') p++;
    char *e = p + strlen(p);
    while (e > p && (e[-1] == '\n' || e[-1] == '\r' || e[-1] == ' ' || e[-1] == '\t'))
      *--e = 0;
    if (!*p || *p == '#') continue;
    paths.push_back(p);
  }
  fclose(f);
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

  // Extended metadata:
  // Tag 1: POSIX modes
  w.writeInteger(1);
  w.writeInteger((uint64)nfiles);
  for (int i = 0; i < nfiles; i++)
    w.write_u32(files[i].mode);

  // Tag 2: Symlinks
  bool any_symlink = false;
  for (int i = 0; i < nfiles; i++) {
    if (files[i].is_symlink) { any_symlink = true; break; }
  }
  if (any_symlink) {
    w.writeInteger(2);
    w.writeInteger((uint64)nfiles);
    for (int i = 0; i < nfiles; i++) {
      w.writeString(files[i].is_symlink ? files[i].symlink_target.c_str() : "");
    }
  }

  // Tag 0: End of extension tags
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

static BlockInfo compress_range(VolumeWriter *out, const char *method, const char *password,
                                const char *algorithm, FileRec *files, int start, int count) {
  char compress_m[MAX_METHOD_STRLEN], store_m[MAX_METHOD_STRLEN];
  wrap_encryption(method, password, algorithm, compress_m, store_m);
  BlockInfo b;
  memset(&b, 0, sizeof(b));
  b.type = DATA_BLOCK;
  strncopy(b.compressor, store_m, MAX_METHOD_STRLEN);
  b.pos = out->tell();
  uint64 orig = 0;
  for (int i = 0; i < count; i++)
    orig += files[start + i].size;
  b.origsize = (FILESIZE)orig;
  SolidInFileOut sctx;
  memset(&sctx, 0, sizeof(sctx));
  sctx.files = files;
  sctx.nfiles = start + count;
  sctx.idx = start;
  sctx.end = start + count;
  sctx.cur = NULL;
  sctx.out = out;
  sctx.written = 0;
  if (orig > 0) {
    int rc = MultiCompress(compress_m, solid_callback, &sctx);
    if (sctx.cur) fclose(sctx.cur);
    CHECK(rc >= 0, (s, "ERROR: data compression failed (%d) method \"%s\"", rc, compress_m));
  }
  b.compsize = (FILESIZE)sctx.written;
  b.crc = 0;
  return b;
}

static bool write_archive(const std::string &arcname, uint64 vol_size,
                          std::vector<FileRec> &files, int solid,
                          const char *method, const char *password,
                          const char *hp_password, const char *algorithm,
                          const std::string &comment,
                          std::vector<std::string> &out_vol_names,
                          uint64 &out_total_written,
                          const std::string &sfx_stub = "") {
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

  std::vector<DataBlockRec> data_blocks;
  if (solid) {
    DataBlockRec rec;
    rec.start = 0;
    rec.count = (int)files.size();
    rec.info = compress_range(&out, method, password, algorithm, &files[0], 0, rec.count);
    data_blocks.push_back(rec);
  } else {
    for (int i = 0; i < (int)files.size(); i++) {
      DataBlockRec rec;
      rec.start = i;
      rec.count = 1;
      rec.info = compress_range(&out, method, password, algorithm, &files[0], i, 1);
      data_blocks.push_back(rec);
    }
  }

  char dir_compress_m[MAX_METHOD_STRLEN], dir_store_m[MAX_METHOD_STRLEN];
  wrap_encryption(aNO_COMPRESSION, hp_password, algorithm, dir_compress_m, dir_store_m);

  FILESIZE dir_pos = out.tell();
  ByteWriter dir;
  write_dir_block(dir, data_blocks, files, dir_pos);
  BlockInfo dir_block = write_control_block(&out, DIR_BLOCK, dir_compress_m, dir_store_m, dir);

  char footer_compress_m[MAX_METHOD_STRLEN], footer_store_m[MAX_METHOD_STRLEN];
  wrap_encryption(aNO_COMPRESSION, hp_password, algorithm, footer_compress_m, footer_store_m);

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

static void create_dir_recursive(const std::string &path) {
  char buf[PATH_MAX];
  snprintf(buf, sizeof(buf), "%s", path.c_str());
  for (char *p = buf + 1; *p; p++) {
    if (*p == '/' || *p == '\\') {
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

int main(int argc, char **argv) {
  char cmd = 'a';
  const char *method = NULL;
  const char *password = NULL;
  const char *hp_password = NULL;
  const char *algorithm = "aes";
  int argi = 1;
  int recurse = 1;
  int solid = 1;
  int ep = 0;
  int noarcext = 0;
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

  // Check if this is an unpacker command (l, v, t, e, x, p) or pipe flag (-so)
  for (int i = 1; i < argc; i++) {
    if (strequ(argv[i], "--")) break;
    if (strequ(argv[i], "-so")) {
      std::string unarc_bin = find_unarc_binary(argv[0]);
      execvp(unarc_bin.c_str(), argv);
      fprintf(stderr, "ERROR: failed to execute unarc (%s): %s\n", unarc_bin.c_str(), strerror(errno));
      return FREEARC_EXIT_ERROR;
    }
    if (argv[i][0] != '-') {
      if (strequ(argv[i], "l") || strequ(argv[i], "v") || strequ(argv[i], "t") ||
          strequ(argv[i], "e") || strequ(argv[i], "x") || strequ(argv[i], "p")) {
        std::string unarc_bin = find_unarc_binary(argv[0]);
        execvp(unarc_bin.c_str(), argv);
        fprintf(stderr, "ERROR: failed to execute unarc (%s): %s\n", unarc_bin.c_str(), strerror(errno));
        return FREEARC_EXIT_ERROR;
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
      if (strequ(a, "-h") || strequ(a, "--help")) {
        fputs(g_usage, stdout);
        return 0;
      }
      if (strequ(a, "-q")) g_quiet = 1;
      else if (strequ(a, "-s")) solid = 1;
      else if (strequ(a, "-s-")) solid = 0;
      else if (strequ(a, "-r")) recurse = 1;
      else if (strequ(a, "-r-")) recurse = 0;
      else if (strequ(a, "-ep")) ep = 1;
      else if (strequ(a, "-ep1")) ep = 2;
      else if (strequ(a, "--noarcext")) noarcext = 1;
      else if (strequ(a, "-sfx")) {
        create_sfx = true;
      }
      else if (start_with(a, "-sfx=")) {
        create_sfx = true;
        sfx_stub_path = a + 5;
      }
      else if (strequ(a, "-bcj") || strequ(a, "--bcj") || strequ(a, "-exe") || strequ(a, "--exe")) filters.push_back("exe");
      else if (strequ(a, "-rep") || strequ(a, "--rep")) filters.push_back("rep:64m");
      else if (strequ(a, "-delta") || strequ(a, "--delta")) filters.push_back("delta");
      else if (strequ(a, "-dict") || strequ(a, "--dict")) filters.push_back("dict");
      else if (strequ(a, "-lzp") || strequ(a, "--lzp")) filters.push_back("lzp");
      else if (strequ(a, "-mm") || strequ(a, "--mm")) filters.push_back("mm");
      else if (strequ(a, "-v") && argi + 1 < argc) vol_size = parse_vol_size(argv[++argi]);
      else if (start_with(a, "-v")) vol_size = parse_vol_size(a + 2);
      else if (strequ(a, "-m") && argi + 1 < argc) method = argv[++argi];
      else if (start_with(a, "-m")) method = a + 2;
      else if (strequ(a, "-z") && argi + 1 < argc) comment = load_comment(argv[++argi]);
      else if (start_with(a, "-z")) comment = load_comment(a + 2);
      else if (strequ(a, "-x") && argi + 1 < argc) add_pattern_arg(argv[++argi], excludes);
      else if (start_with(a, "-x")) add_pattern_arg(a + 2, excludes);
      else if (strequ(a, "-i") && argi + 1 < argc) add_pattern_arg(argv[++argi], includes);
      else if (start_with(a, "-i")) add_pattern_arg(a + 2, includes);
      else if (strequ(a, "-ap") && argi + 1 < argc) arcprefix = argv[++argi];
      else if (start_with(a, "-ap")) arcprefix = a + 3;
      else if (strequ(a, "-hp") && argi + 1 < argc) hp_password = argv[++argi];
      else if (start_with(a, "-hp")) hp_password = a + 3;
      else if (strequ(a, "-p") && argi + 1 < argc) password = argv[++argi];
      else if (start_with(a, "-p")) password = a + 2;
      else if (strequ(a, "-ae") && argi + 1 < argc) algorithm = argv[++argi];
      else if (start_with(a, "-ae")) algorithm = a + 3;
      else {
        printf("Unknown option: %s\n%s", a, g_usage);
        return FREEARC_EXIT_ERROR;
      }
    } else {
      if (arcname.empty()) {
        if (inputs.empty() && (strequ(a, "a") || strequ(a, "c") || strequ(a, "u") ||
                               strequ(a, "f") || strequ(a, "d") || strequ(a, "m") ||
                               strequ(a, "s"))) {
          cmd = a[0];
          if (cmd == 'c') cmd = 'a';
          argi++;
          continue;
        }
        arcname = ensure_arc_ext(a, noarcext, create_sfx || cmd == 's');
      } else {
        if (a[0] == '@')
          read_listfile(a + 1, inputs);
        else
          inputs.push_back(a);
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
    fprintf(stderr, "ERROR: volume size must be at least 10KB (e.g. -v10m or -v500k)\n");
    return FREEARC_EXIT_ERROR;
  }

  if (password && !*password) password = NULL;
  if (hp_password && !*hp_password) hp_password = NULL;
  if (hp_password && !password) password = hp_password;

  std::string base_method = method ? map_method(method) : (!filters.empty() ? "lzma:8m" : "storing");
  std::string final_method;
  for (size_t i = 0; i < filters.size(); i++) {
    const std::string &f = filters[i];
    if (base_method.find(f) == std::string::npos && final_method.find(f) == std::string::npos) {
      if (!final_method.empty()) final_method += '+';
      final_method += f;
    }
  }
  if (!final_method.empty()) {
    final_method += '+';
    final_method += base_method;
  } else {
    final_method = base_method;
  }
  std::string method_str = final_method;
  method = method_str.c_str();

  SetCompressionThreads(GetProcessorsCount());

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
                  comment.empty() ? existing_info.comment : comment, out_vols, total_w, active_sfx_stub);
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
      const char *path = inputs[i].c_str();
      std::string stored = normalize_stored(path);
      if (stored.empty()) continue;
      walk_path(disk_files, seen, path, stored, skip_arc, recurse, ep, excludes, includes, arcprefix, check_arc.c_str());
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
                  comment.empty() ? existing_info.comment : comment, out_vols, total_w, active_sfx_stub);
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
      const char *path = inputs[i].c_str();
      std::string stored = normalize_stored(path);
      if (stored.empty()) continue;
      walk_path(disk_files, seen, path, stored, skip_arc, recurse, ep, excludes, includes, arcprefix, check_arc.c_str());
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
                  comment.empty() ? existing_info.comment : comment, out_vols, total_w, active_sfx_stub);
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
      const char *path = inputs[i].c_str();
      std::string stored = normalize_stored(path);
      if (stored.empty()) continue;
      walk_path(disk_files, seen, path, stored, skip_arc, recurse, ep, excludes, includes, arcprefix, check_arc.c_str());
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
                  comment.empty() ? existing_info.comment : comment, out_vols, total_w, active_sfx_stub);
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
    const char *path = inputs[i].c_str();
    std::string stored = normalize_stored(path);
    if (stored.empty()) {
      msg("WARNING: skip %s\n", path);
      continue;
    }
    walk_path(files, seen, path, stored, skip_arc, recurse, ep, excludes, includes, arcprefix, check_arc.c_str());
  }
  CHECK(!files.empty(), (s, "ERROR: no files to archive"));

  std::vector<std::string> out_vols;
  uint64 total_w = 0;
  write_archive(arcname, vol_size, files, solid, method, password, hp_password, algorithm, comment, out_vols, total_w, active_sfx_stub);
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
