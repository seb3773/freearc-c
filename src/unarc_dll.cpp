/*
 * unarc_dll.cpp - Shared/static library & Win64 DLL entry point for FreeArc-c decoder
 */

#ifndef FREEARC_LIBRARY
#  define FREEARC_LIBRARY
#endif
#ifndef FREEARC_DLL
#  define FREEARC_DLL
#endif
#ifndef FREEARC_DLL_EXPORTS
#  define FREEARC_DLL_EXPORTS
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

#ifdef _WIN32
#  ifndef _WIN32_WINNT
#    define _WIN32_WINNT 0x0600
#  endif
#  include <windows.h>
#else
#  include <pthread.h>
#endif

#include "unarc.h"
#include "ArcStructure.h"
#include "ArcCommand.h"
#include "ArcProcess.h"

/* Pull all compiled codec registration translation units when linking statically */
extern "C" {
  COMPRESSION_METHOD* parse_LZMA(char**);
  COMPRESSION_METHOD* parse_BCJ_X86(char**);
  COMPRESSION_METHOD* parse_DICT(char**);
  COMPRESSION_METHOD* parse_DELTA(char**);
  COMPRESSION_METHOD* parse_REP(char**);
  COMPRESSION_METHOD* parse_LZP(char**);
  COMPRESSION_METHOD* parse_PPMD(char**);
  COMPRESSION_METHOD* parse_MM(char**);
  COMPRESSION_METHOD* parse_TTA(char**);
  COMPRESSION_METHOD* parse_TORNADO(char**);
  COMPRESSION_METHOD* parse_GRZIP(char**);
  int AddExternalCompressor(char*);
}
COMPRESSION_METHOD* parse_4x4(char**);
COMPRESSION_METHOD* parse_ENCRYPTION(char**);

static volatile int g_codec_anchors_once = 0;
static const void * const g_codec_anchors[] = {
  (const void *)&parse_LZMA,
  (const void *)&parse_BCJ_X86,
  (const void *)&parse_DICT,
  (const void *)&parse_DELTA,
  (const void *)&parse_REP,
  (const void *)&parse_LZP,
  (const void *)&parse_PPMD,
  (const void *)&parse_MM,
  (const void *)&parse_TTA,
  (const void *)&parse_TORNADO,
  (const void *)&parse_GRZIP,
  (const void *)&parse_4x4,
  (const void *)&parse_ENCRYPTION,
  (const void *)&AddExternalCompressor
};

extern "C" FILE *g_logfile;
FILE *g_logfile = NULL;

/* Error propagation across CHECK / longjmp and worker threads */
static FA_THREAD_LOCAL char tl_last_error[4096] = {0};
static FA_THREAD_LOCAL bool tl_is_owner_thread = false;
static char g_call_error[4096] = {0};
static volatile int g_call_errcode = UNARC_OK;

#ifdef _WIN32
static CRITICAL_SECTION g_unarc_cs;
static volatile LONG g_unarc_cs_init = 0;
static CRITICAL_SECTION g_err_cs;
static volatile LONG g_err_cs_init = 0;

static void ensure_cs(CRITICAL_SECTION *cs, volatile LONG *state) {
  for (;;) {
    LONG s = *state;
    if (s == 2) return;
    if (s == 0 && InterlockedCompareExchange(state, 1, 0) == 0) {
      InitializeCriticalSection(cs);
      InterlockedExchange(state, 2);
      return;
    }
    Sleep(0);
  }
}

struct UnarcCallLock {
  UnarcCallLock() { ensure_cs(&g_unarc_cs, &g_unarc_cs_init); EnterCriticalSection(&g_unarc_cs); }
  ~UnarcCallLock() { LeaveCriticalSection(&g_unarc_cs); }
};
struct ErrLock {
  ErrLock() { ensure_cs(&g_err_cs, &g_err_cs_init); EnterCriticalSection(&g_err_cs); }
  ~ErrLock() { LeaveCriticalSection(&g_err_cs); }
};
#else
static pthread_mutex_t g_unarc_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_err_mutex   = PTHREAD_MUTEX_INITIALIZER;

struct UnarcCallLock {
  UnarcCallLock() { pthread_mutex_lock(&g_unarc_mutex); }
  ~UnarcCallLock() { pthread_mutex_unlock(&g_unarc_mutex); }
};
struct ErrLock {
  ErrLock() { pthread_mutex_lock(&g_err_mutex); }
  ~ErrLock() { pthread_mutex_unlock(&g_err_mutex); }
};
#endif

static void copy_cstr(char *dst, size_t cap, const char *src) {
  if (!dst || cap == 0) return;
  if (!src) { dst[0] = '\0'; return; }
  size_t len = strlen(src);
  if (len >= cap) len = cap - 1;
  memcpy(dst, src, len);
  dst[len] = '\0';
}

extern "C" void freearc_dll_report_error(const char *msg) {
  if (!msg) return;
  copy_cstr(tl_last_error, sizeof(tl_last_error), msg);
  ErrLock lock;
  if (g_call_error[0] == '\0') {
    copy_cstr(g_call_error, sizeof(g_call_error), msg);
  }
}

extern "C" void freearc_dll_set_errcode(int code) {
  if (code < 0) {
    ErrLock lock;
    if (g_call_errcode == UNARC_OK) {
      g_call_errcode = code;
    }
  }
}

static int infer_errcode_from_message(const char *msg) {
  if (g_call_errcode < 0) return g_call_errcode;
  if (!msg || !*msg) return UNARC_ERR_GENERAL;
  if (strstr(msg, "terminated by user"))        return UNARC_ERR_TERMINATED;
  if (strstr(msg, "exceeding -ld limit"))       return UNARC_ERR_NOT_ENOUGH_MEMORY;
  if (strstr(msg, "can't alloc") || strstr(msg, "memory")) return UNARC_ERR_NOT_ENOUGH_MEMORY;
  if (strstr(msg, "unsupported compression"))   return UNARC_ERR_INVALID_COMPRESSOR;
  if (strstr(msg, "wrong password") || strstr(msg, "CRC") || strstr(msg, "corrupted") || strstr(msg, "corrupt"))
    return UNARC_ERR_BAD_COMPRESSED_DATA;
  if (strstr(msg, "file write"))                return UNARC_ERR_WRITE;
  if (strstr(msg, "can't open") || strstr(msg, "file read") || strstr(msg, "file seek"))
    return UNARC_ERR_READ;
  return UNARC_ERR_GENERAL;
}

static BASEUI *g_active_ui = NULL;
static std::string *g_active_pwd_storage = NULL;

extern "C" void freearc_dll_on_check_fail(void) {
  if (!tl_is_owner_thread) return;
  if (CurrentProcess) {
    if (CurrentProcess->outfile.isopen()) {
      CurrentProcess->outfile.close();
      CurrentProcess->outfile.remove();
    }
    CurrentProcess->arcinfo.arcfile.tryClose();
  }
  compressionLib_cleanup();
}

int freearc_dll_ask_password(int attempt) {
  if (!g_active_ui || !g_active_pwd_storage) return 'n';
  char buf[1024];
  buf[0] = '\0';
  int act = g_active_ui->AskPassword(buf, (int)sizeof(buf), attempt);
  if (act == 'y') {
    buf[sizeof(buf) - 1] = '\0';
    *g_active_pwd_storage = buf;
    SetArchivePassword((char *)g_active_pwd_storage->c_str());
    return 'y';
  }
  return (act == 'q') ? 'q' : 'n';
}

class DLLUI : public BASEUI {
public:
  bool is_legacy;
  unarc_legacy_callback legacy_cb;
  unarc_callback modern_cb;
  void *userdata;
  int last_cb_err;
  bool err_sent;
  char cur_cmd;
  char outdir[MY_FILENAME_MAX * 4];
  uint64 total_files;
  uint64 total_bytes;
  uint64 total_packed;
  std::string password_storage;

  DLLUI(bool legacy, unarc_legacy_callback lcb, unarc_callback mcb, void *ud)
    : is_legacy(legacy), legacy_cb(lcb), modern_cb(mcb), userdata(ud),
      last_cb_err(0), err_sent(false), cur_cmd(' '),
      total_files(0), total_bytes(0), total_packed(0) {
    outdir[0] = '\0';
  }

  int call_cb(const char *what, int64_t p1, int64_t p2, char *str, size_t str_cap, bool shift_int1 = false) {
    if (is_legacy) {
      if (!legacy_cb) {
        if (strequ(what, "overwrite?")) return 's';
        if (strequ(what, "password?"))  return 'n';
        return 0;
      }
      unarc_number i1 = shift_int1 ? (unarc_number)(p1 >> 20) : (unarc_number)p1;
      unarc_number i2 = shift_int1 ? (unarc_number)p1         : (unarc_number)p2;
      char empty[] = "";
      return legacy_cb((char *)what, i1, i2, str ? str : empty);
    } else {
      if (!modern_cb) {
        if (strequ(what, "overwrite?")) return 's';
        if (strequ(what, "password?"))  return 'n';
        return 0;
      }
      char empty[] = "";
      return modern_cb(what, p1, p2, str ? str : empty, str ? str_cap : 0, userdata);
    }
  }

  bool dispatch_progress(const char *what, int64_t p1, int64_t p2, char *str, size_t cap, bool shift_int1) {
    int rc = call_cb(what, p1, p2, str, cap, shift_int1);
    if (rc < 0) {
      last_cb_err = rc;
      freearc_dll_set_errcode(rc);
      return FALSE;
    }
    return TRUE;
  }

  bool AllowProcessing(char cmd, int /*silent*/, MYFILENAME /*arcname*/, char* /*comment*/, int /*cmtsize*/, FILENAME _outdir) {
    cur_cmd = cmd;
    copy_cstr(outdir, sizeof(outdir), _outdir ? _outdir : "");
    return TRUE;
  }

  FILENAME GetOutDir() { return outdir; }

  void BeginProgress(uint64 totalBytes) {
    if (!dispatch_progress("total", (int64_t)totalBytes, 0, NULL, 0, true)) {
      if (CurrentProcess) CurrentProcess->quit(last_cb_err);
    }
  }

  bool ProgressRead(uint64 readBytes) {
    return dispatch_progress("read", (int64_t)readBytes, 0, NULL, 0, true);
  }

  bool ProgressWrite(uint64 writtenBytes) {
    return dispatch_progress("write", (int64_t)writtenBytes, 0, NULL, 0, true);
  }

  bool ProgressFile(bool isdir, const char *operation, MYFILENAME filename, uint64 filesize) {
    if (operation && strequ(operation, "Skipping")) return TRUE;
    std::vector<char> buf(MY_FILENAME_MAX * 4);
    copy_cstr(&buf[0], buf.size(), filename ? filename : "");
    int64_t p2 = is_legacy ? (int64_t)filesize : (int64_t)(isdir ? 1 : 0);
    return dispatch_progress("filename", (int64_t)filesize, p2, &buf[0], buf.size(), true);
  }

  char AskOverwrite(MYFILENAME filename, uint64 size, time_t modified) {
    std::vector<char> buf(MY_FILENAME_MAX * 4);
    copy_cstr(&buf[0], buf.size(), filename ? filename : "");
    int rc = call_cb("overwrite?", (int64_t)size, (int64_t)modified, &buf[0], buf.size(), true);
    if (rc < 0) {
      last_cb_err = rc;
      freearc_dll_set_errcode(rc);
      return 'q';
    }
    switch (rc) {
      case 'y': case 'n': case 'a': case 's': case 'q': return (char)rc;
      default: return 'n';
    }
  }

  int AskPassword(char *buf, int bufsize, int attempt) {
    int rc = call_cb("password?", (int64_t)bufsize, (int64_t)attempt, buf, (size_t)bufsize, false);
    if (rc < 0) {
      last_cb_err = rc;
      freearc_dll_set_errcode(rc);
      return 'q';
    }
    if (rc == 'y' || rc == 'n' || rc == 'q') return rc;
    return 'n';
  }

  void ListHeader(COMMAND &) {
    total_files = 0;
    total_bytes = 0;
    total_packed = 0;
  }

  void ListFiles(DIRECTORY_BLOCK *dirblock, COMMAND &command) {
    int b = 0;
    uint64 packed = 0;
    std::vector<char> fullname(MY_FILENAME_MAX * 4);
    iterate_var(i, dirblock->total_files) {
      if (i >= dirblock->block_end(b)) b++;
      if (i == dirblock->block_start(b)) {
        packed = dirblock->data_block[b].compsize;
      }
      if (command.accept_file(dirblock, i)) {
        if (!is_legacy && modern_cb) {
          dirblock->fullname(i, &fullname[0]);
          int64_t p2 = ((int64_t)(dirblock->isdir[i] ? 1 : 0) << 32) | (int64_t)(uint32_t)dirblock->crc[i];
          if (!dispatch_progress("list_item", (int64_t)dirblock->size[i], p2, &fullname[0], fullname.size(), false)) {
            if (CurrentProcess) CurrentProcess->quit(last_cb_err);
            return;
          }
        }
        total_files++;
        total_bytes += dirblock->size[i];
        total_packed += packed;
        packed = 0;
      }
    }
  }

  void ListFooter(COMMAND &) {
    call_cb("total_files", (int64_t)total_files, 0, NULL, 0, false);
    call_cb("origsize",    (int64_t)total_bytes, 0, NULL, 0, true);
    call_cb("compsize",    (int64_t)total_packed, 0, NULL, 0, true);
  }

  void Abort(COMMAND *, int errcode) {
    if (last_cb_err < 0) errcode = last_cb_err;
    freearc_dll_set_errcode(errcode);
    if (g_call_error[0] == '\0') {
      const char *desc = unarc_error_string(errcode);
      freearc_dll_report_error(desc);
    }
    if (jmpready) longjmp(jumper, 1);
    abort();
  }
};

struct UnarcCallFrame {
  DLLUI ui;
  COMMAND *cmd;
  PROCESS *proc;
  UnarcCallFrame(bool legacy, unarc_legacy_callback lcb, unarc_callback mcb, void *ud)
    : ui(legacy, lcb, mcb, ud), cmd(NULL), proc(NULL) {}
};

static int unarc_run_internal(
    int argc,
    const char *const *argv,
    bool is_legacy,
    unarc_legacy_callback legacy_cb,
    unarc_callback modern_cb,
    void *userdata)
{
  if (!g_codec_anchors_once) {
    size_t acc = 0;
    for (size_t i = 0; i < sizeof(g_codec_anchors) / sizeof(g_codec_anchors[0]); i++) {
      acc ^= (size_t)g_codec_anchors[i];
    }
    g_codec_anchors_once = acc ? 1 : -1;
  }
  if (argc <= 0 || !argv) {
    const char *msg = "ERROR: invalid or empty command arguments";
    copy_cstr(tl_last_error, sizeof(tl_last_error), msg);
    if (is_legacy && legacy_cb) legacy_cb((char *)"error", UNARC_ERR_GENERAL, 0, (char *)msg);
    else if (!is_legacy && modern_cb) modern_cb("error", UNARC_ERR_GENERAL, 0, (char *)msg, strlen(msg) + 1, userdata);
    return UNARC_ERR_GENERAL;
  }

  std::vector<std::string> owned;
  owned.reserve((size_t)argc + 1);
  owned.push_back("unarc");
  for (int i = 0; i < argc; i++) {
    if (!argv[i]) break;
    owned.push_back(argv[i]);
  }
  std::vector<char *> raw_argv(owned.size() + 1, NULL);
  for (size_t i = 0; i < owned.size(); i++) {
    raw_argv[i] = &owned[i][0];
  }

  UnarcCallFrame *frame = new UnarcCallFrame(is_legacy, legacy_cb, modern_cb, userdata);
  if (!frame) return UNARC_ERR_NOT_ENOUGH_MEMORY;

  UnarcCallLock lock;
  g_call_error[0] = '\0';
  g_call_errcode = UNARC_OK;
  tl_last_error[0] = '\0';
  tl_is_owner_thread = true;
  CurrentProcess = NULL;
  g_active_ui = &frame->ui;
  g_active_pwd_storage = &frame->ui.password_storage;
  SetArchivePassword(NULL);

  int final_rc = UNARC_OK;

  if (setjmp(jumper) != 0) {
    jmpready = FALSE;
    if (frame->ui.last_cb_err < 0) {
      final_rc = frame->ui.last_cb_err;
    } else {
      final_rc = infer_errcode_from_message(g_call_error);
    }
  } else {
    jmpready = TRUE;
    frame->cmd = new COMMAND((int)(raw_argv.size() - 1), &raw_argv[0]);
    if (!frame->cmd->ok || (is_legacy && frame->cmd->cmd == 'v')) {
      final_rc = (is_legacy && frame->cmd->cmd == 'v') ? UNARC_ERR_NOT_IMPLEMENTED : UNARC_ERR_GENERAL;
      freearc_dll_report_error(final_rc == UNARC_ERR_NOT_IMPLEMENTED
                                 ? "ERROR: command 'v' is not supported by legacy FreeArcExtract()"
                                 : "ERROR: invalid command syntax or missing archive name");
    } else {
      if (frame->cmd->password && frame->cmd->password[0]) {
        frame->ui.password_storage = frame->cmd->password;
        SetArchivePassword((char *)frame->ui.password_storage.c_str());
      }
      frame->proc = new PROCESS(frame->cmd, &frame->ui);
      if (!frame->cmd->ok) {
        final_rc = (frame->ui.last_cb_err < 0) ? frame->ui.last_cb_err : UNARC_ERR_GENERAL;
      }
    }
    jmpready = FALSE;
  }

  if (final_rc != UNARC_OK && !frame->ui.err_sent) {
    if (g_call_error[0] == '\0') {
      copy_cstr(g_call_error, sizeof(g_call_error), unarc_error_string(final_rc));
    }
    copy_cstr(tl_last_error, sizeof(tl_last_error), g_call_error);
    frame->ui.err_sent = true;
    frame->ui.call_cb("error", (int64_t)final_rc, 0, g_call_error, sizeof(g_call_error), false);
  }

  if (frame->proc) {
    if (frame->proc->outfile.isopen()) {
      frame->proc->outfile.tryClose();
    }
    frame->proc->arcinfo.arcfile.tryClose();
    delete frame->proc;
    frame->proc = NULL;
  }
  if (frame->cmd) {
    delete frame->cmd;
    frame->cmd = NULL;
  }
  if (g_logfile) {
    fclose(g_logfile);
    g_logfile = NULL;
  }
  SetArchivePassword(NULL);
  compressionLib_cleanup();
  g_active_pwd_storage = NULL;
  g_active_ui = NULL;
  CurrentProcess = NULL;
  tl_is_owner_thread = false;

  delete frame;
  return final_rc;
}

extern "C" {

UNARC_API int FreeArcExtract(unarc_legacy_callback callback, ...) {
  std::vector<const char *> args;
  va_list ap;
  va_start(ap, callback);
  for (int i = 0; i < 1024; i++) {
    const char *a = va_arg(ap, const char *);
    if (!a || !*a) break;
    args.push_back(a);
  }
  va_end(ap);
  if (args.empty()) {
    return unarc_run_internal(0, NULL, true, callback, NULL, NULL);
  }
  return unarc_run_internal((int)args.size(), &args[0], true, callback, NULL, NULL);
}

UNARC_API void UNARC_CALL UnloadDLL(void) {
  unarc_cleanup();
}

UNARC_API int UNARC_CALL unarc_run(
    int argc,
    const char *const *argv,
    unarc_callback callback,
    void *userdata) {
  return unarc_run_internal(argc, argv, false, NULL, callback, userdata);
}

UNARC_API const char * UNARC_CALL unarc_version(void) {
  return "1.4.5";
}

UNARC_API const char * UNARC_CALL unarc_last_error(void) {
  return tl_last_error;
}

UNARC_API const char * UNARC_CALL unarc_error_string(int errcode) {
  switch (errcode) {
    case UNARC_OK:                        return "OK";
    case UNARC_ERR_GENERAL:               return "General error";
    case UNARC_ERR_INVALID_COMPRESSOR:    return "Unsupported or invalid compression method";
    case UNARC_ERR_ONLY_DECOMPRESS:       return "Compression disabled in decompress-only build";
    case UNARC_ERR_OUTBLOCK_TOO_SMALL:    return "Output buffer too small";
    case UNARC_ERR_NOT_ENOUGH_MEMORY:     return "Not enough memory";
    case UNARC_ERR_READ:                  return "Read error or archive not found";
    case UNARC_ERR_BAD_COMPRESSED_DATA:   return "Bad password or corrupted archive data";
    case UNARC_ERR_NOT_IMPLEMENTED:       return "Not implemented";
    case UNARC_ERR_NO_MORE_DATA:          return "No more data required";
    case UNARC_ERR_TERMINATED:            return "Operation terminated by user";
    case UNARC_ERR_WRITE:                 return "Write error";
    default:                              return "Unknown error";
  }
}

UNARC_API void UNARC_CALL unarc_cleanup(void) {
  UnarcCallLock lock;
  SetArchivePassword(NULL);
  compressionLib_cleanup();
  ClearExternalCompressorsTable();
  if (g_logfile) {
    fclose(g_logfile);
    g_logfile = NULL;
  }
}

} /* extern "C" */
