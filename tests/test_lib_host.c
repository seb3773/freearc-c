/*
 * test_lib_host.c - Native C host test harness for unarc.dll / libunarc.so / libunarc.a
 * Tests legacy FreeArcExtract()/UnloadDLL() and modern unarc_run() APIs.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define UNARC_STATIC
#include "unarc.h"

#ifdef _WIN32
#  include <windows.h>
#else
#  include <dlfcn.h>
#  include <pthread.h>
#  include <unistd.h>
#endif

typedef int (*pfn_FreeArcExtract)(unarc_legacy_callback callback, ...);
typedef void (UNARC_CALL *pfn_UnloadDLL)(void);
typedef int (UNARC_CALL *pfn_unarc_run)(int argc, const char *const *argv, unarc_callback cb, void *ud);
typedef const char * (UNARC_CALL *pfn_unarc_last_error)(void);
typedef const char * (UNARC_CALL *pfn_unarc_version)(void);

static pfn_FreeArcExtract   fn_FreeArcExtract = NULL;
static pfn_UnloadDLL        fn_UnloadDLL      = NULL;
static pfn_unarc_run        fn_unarc_run      = NULL;
static pfn_unarc_last_error fn_unarc_last_error = NULL;
static pfn_unarc_version    fn_unarc_version  = NULL;

#ifdef USE_STATIC_LINK
static int load_api(const char *lib_path) {
  (void)lib_path;
  fn_FreeArcExtract    = FreeArcExtract;
  fn_UnloadDLL         = UnloadDLL;
  fn_unarc_run         = unarc_run;
  fn_unarc_last_error  = unarc_last_error;
  fn_unarc_version     = unarc_version;
  return 1;
}
#else
static int load_api(const char *lib_path) {
#ifdef _WIN32
  HMODULE h = LoadLibraryA(lib_path);
  if (!h) { fprintf(stderr, "LoadLibraryA(%s) failed: %lu\n", lib_path, GetLastError()); return 0; }
  fn_FreeArcExtract    = (pfn_FreeArcExtract)(void *)GetProcAddress(h, "FreeArcExtract");
  fn_UnloadDLL         = (pfn_UnloadDLL)(void *)GetProcAddress(h, "UnloadDLL");
  fn_unarc_run         = (pfn_unarc_run)(void *)GetProcAddress(h, "unarc_run");
  fn_unarc_last_error  = (pfn_unarc_last_error)(void *)GetProcAddress(h, "unarc_last_error");
  fn_unarc_version     = (pfn_unarc_version)(void *)GetProcAddress(h, "unarc_version");
#else
  void *h = dlopen(lib_path, RTLD_NOW);
  if (!h) { fprintf(stderr, "dlopen(%s) failed: %s\n", lib_path, dlerror()); return 0; }
  fn_FreeArcExtract    = (pfn_FreeArcExtract)dlsym(h, "FreeArcExtract");
  fn_UnloadDLL         = (pfn_UnloadDLL)dlsym(h, "UnloadDLL");
  fn_unarc_run         = (pfn_unarc_run)dlsym(h, "unarc_run");
  fn_unarc_last_error  = (pfn_unarc_last_error)dlsym(h, "unarc_last_error");
  fn_unarc_version     = (pfn_unarc_version)dlsym(h, "unarc_version");
#endif
  return fn_FreeArcExtract && fn_UnloadDLL && fn_unarc_run && fn_unarc_last_error && fn_unarc_version;
}
#endif

typedef struct {
  int seen_total;
  int seen_filename;
  int seen_write;
  int seen_error;
  int seen_list_item;
  int seen_total_files;
  int overwrite_reply;
  int abort_after_read;
  const char *pwd0;
  const char *pwd1;
  int pwd_attempts;
  int64_t total_files;
  int64_t orig_size;
  int64_t comp_size;
  int check_caller_thread;
  int wrong_thread_hits;
  int concurrent_hits;
  int active_cb;
#ifdef _WIN32
  DWORD caller_tid;
#else
  pthread_t caller_tid;
#endif
} HostState;

static HostState g_legacy_state;

static int UNARC_CALL legacy_cb(char *what, unarc_number int1, unarc_number int2, char *str) {
  (void)int2;
  if (g_legacy_state.check_caller_thread) {
    if (g_legacy_state.active_cb != 0) g_legacy_state.concurrent_hits++;
    g_legacy_state.active_cb++;
#ifdef _WIN32
    if (GetCurrentThreadId() != g_legacy_state.caller_tid) g_legacy_state.wrong_thread_hits++;
#else
    if (!pthread_equal(pthread_self(), g_legacy_state.caller_tid)) g_legacy_state.wrong_thread_hits++;
#endif
  }
  int rc = 1;
  if (strcmp(what, "total") == 0) g_legacy_state.seen_total++;
  else if (strcmp(what, "filename") == 0) g_legacy_state.seen_filename++;
  else if (strcmp(what, "write") == 0) {
    g_legacy_state.seen_write++;
    if (g_legacy_state.abort_after_read) rc = -43;
  }
  else if (strcmp(what, "error") == 0) g_legacy_state.seen_error++;
  else if (strcmp(what, "total_files") == 0) { g_legacy_state.seen_total_files++; g_legacy_state.total_files = int1; }
  else if (strcmp(what, "origsize") == 0) g_legacy_state.orig_size = int2;
  else if (strcmp(what, "compsize") == 0) g_legacy_state.comp_size = int2;
  else if (strcmp(what, "overwrite?") == 0) rc = g_legacy_state.overwrite_reply;
  else if (strcmp(what, "password?") == 0) {
    int att = g_legacy_state.pwd_attempts++;
    const char *pw = (att == 0) ? g_legacy_state.pwd0 : g_legacy_state.pwd1;
    if (!pw) rc = 'n';
    else {
      snprintf(str, (size_t)int1, "%s", pw);
      rc = 'y';
    }
  }
  if (g_legacy_state.check_caller_thread) g_legacy_state.active_cb--;
  return rc;
}

static int UNARC_CALL modern_cb(
    const char *what, int64_t p1, int64_t p2, char *str, size_t cap, void *ud) {
  HostState *st = (HostState *)ud;
  (void)p2;
  if (strcmp(what, "total") == 0) st->seen_total++;
  else if (strcmp(what, "filename") == 0) st->seen_filename++;
  else if (strcmp(what, "write") == 0) {
    st->seen_write++;
    if (st->abort_after_read) return -42;
  }
  else if (strcmp(what, "error") == 0) st->seen_error++;
  else if (strcmp(what, "list_item") == 0) st->seen_list_item++;
  else if (strcmp(what, "total_files") == 0) { st->seen_total_files++; st->total_files = p1; }
  else if (strcmp(what, "origsize") == 0) st->orig_size = p1;
  else if (strcmp(what, "compsize") == 0) st->comp_size = p1;
  else if (strcmp(what, "overwrite?") == 0) return st->overwrite_reply;
  else if (strcmp(what, "password?") == 0) {
    int att = st->pwd_attempts++;
    const char *pw = (att == 0) ? st->pwd0 : st->pwd1;
    if (!pw) return 'n';
    snprintf(str, cap, "%s", pw);
    return 'y';
  }
  return 0;
}

#define CHECK_TEST(cond, msg) do { if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); return 1; } } while (0)

typedef struct {
  const char *arc;
  const char *outdir;
  int rc;
} ThreadJob;

#ifdef _WIN32
static DWORD WINAPI worker_fn(LPVOID arg)
#else
static void *worker_fn(void *arg)
#endif
{
  ThreadJob *j = (ThreadJob *)arg;
  HostState st;
  memset(&st, 0, sizeof(st));
  st.overwrite_reply = 'y';
  char dp_arg[512];
  snprintf(dp_arg, sizeof(dp_arg), "-dp%s", j->outdir);
  const char *argv[] = {"x", "-o+", dp_arg, "--", j->arc};
  j->rc = fn_unarc_run(5, argv, modern_cb, &st);
  return 0;
}

int main(int argc, char **argv) {
  if (argc < 8) {
    fprintf(stderr, "Usage: %s <libpath> <plain.arc> <enc_p.arc> <enc_hp.arc> <corrupt.arc> <workdir> <pass>\n", argv[0]);
    return 2;
  }
  const char *lib_path    = argv[1];
  const char *plain_arc   = argv[2];
  const char *enc_p_arc   = argv[3];
  const char *enc_hp_arc  = argv[4];
  const char *corrupt_arc = argv[5];
  const char *workdir     = argv[6];
  const char *secret      = argv[7];

  CHECK_TEST(load_api(lib_path), "load library exports");
  CHECK_TEST(fn_unarc_version() && strcmp(fn_unarc_version(), "1.4.6") == 0, "unarc_version == 1.4.6");

  char out_leg[512], out_mod[512], out_pw[512], out_hp[512], out_abort[512], out_t1[512], out_t2[512];
  char dp_leg[540], dp_mod[540], dp_pw[540], dp_hp[540], dp_abort[540];
  snprintf(out_leg, sizeof(out_leg), "%s/out_leg", workdir);
  snprintf(out_mod, sizeof(out_mod), "%s/out_mod", workdir);
  snprintf(out_pw,  sizeof(out_pw),  "%s/out_pw",  workdir);
  snprintf(out_hp,  sizeof(out_hp),  "%s/out_hp",  workdir);
  snprintf(out_abort, sizeof(out_abort), "%s/out_abort", workdir);
  snprintf(out_t1, sizeof(out_t1), "%s/out_t1", workdir);
  snprintf(out_t2, sizeof(out_t2), "%s/out_t2", workdir);

  snprintf(dp_leg, sizeof(dp_leg), "-dp%s", out_leg);
  snprintf(dp_mod, sizeof(dp_mod), "-dp%s", out_mod);
  snprintf(dp_pw,  sizeof(dp_pw),  "-dp%s", out_pw);
  snprintf(dp_hp,  sizeof(dp_hp),  "-dp%s", out_hp);
  snprintf(dp_abort, sizeof(dp_abort), "-dp%s", out_abort);

  /* 1. Legacy FreeArcExtract: list */
  memset(&g_legacy_state, 0, sizeof(g_legacy_state));
  int rc = fn_FreeArcExtract(legacy_cb, "l", "--", plain_arc, NULL);
  CHECK_TEST(rc == 0, "legacy list rc == 0");
  CHECK_TEST(g_legacy_state.seen_total_files > 0 && g_legacy_state.total_files > 0, "legacy list total_files");
  CHECK_TEST(g_legacy_state.orig_size > 0 && g_legacy_state.comp_size > 0, "legacy list orig/comp size");

  /* 2. Legacy FreeArcExtract: extract with C#-style empty string terminator and verify caller-thread marshaling */
  memset(&g_legacy_state, 0, sizeof(g_legacy_state));
  g_legacy_state.overwrite_reply = 'y';
  g_legacy_state.check_caller_thread = 1;
#ifdef _WIN32
  g_legacy_state.caller_tid = GetCurrentThreadId();
#else
  g_legacy_state.caller_tid = pthread_self();
#endif
  rc = fn_FreeArcExtract(legacy_cb, "x", dp_leg, "-o+", "--", plain_arc, "", NULL);
  CHECK_TEST(rc == 0, "legacy extract empty-term rc == 0");
  CHECK_TEST(g_legacy_state.seen_total > 0 && g_legacy_state.seen_filename > 0 && g_legacy_state.seen_write > 0,
             "legacy progress callbacks fired");
  CHECK_TEST(g_legacy_state.wrong_thread_hits == 0 && g_legacy_state.concurrent_hits == 0,
             "legacy callbacks marshaled strictly sequentially on caller thread");

  /* 2b. Legacy FreeArcExtract: user abort from callback */
  memset(&g_legacy_state, 0, sizeof(g_legacy_state));
  g_legacy_state.overwrite_reply = 'y';
  g_legacy_state.abort_after_read = 1;
  g_legacy_state.check_caller_thread = 1;
#ifdef _WIN32
  g_legacy_state.caller_tid = GetCurrentThreadId();
#else
  g_legacy_state.caller_tid = pthread_self();
#endif
  rc = fn_FreeArcExtract(legacy_cb, "x", dp_abort, "-o+", "--", plain_arc, NULL);
  CHECK_TEST(rc == -43 && g_legacy_state.wrong_thread_hits == 0 && g_legacy_state.concurrent_hits == 0,
             "legacy callback abort returned -43 cleanly on caller thread");

  /* 3. Legacy overwrite 'n' preserves marker vs 'y' overwrites */
  {
    char hello_p[600];
    snprintf(hello_p, sizeof(hello_p), "%s/hello.txt", out_leg);
    FILE *f = fopen(hello_p, "wb");
    CHECK_TEST(f != NULL, "open extracted hello.txt");
    fputs("KEEP_ME", f);
    fclose(f);

    memset(&g_legacy_state, 0, sizeof(g_legacy_state));
    g_legacy_state.overwrite_reply = 'n';
    rc = fn_FreeArcExtract(legacy_cb, "x", dp_leg, "--", plain_arc, NULL);
    CHECK_TEST(rc == 0, "legacy skip overwrite rc == 0");

    f = fopen(hello_p, "rb");
    char buf[32] = {0};
    fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    CHECK_TEST(strcmp(buf, "KEEP_ME") == 0, "overwrite 'n' kept existing file");

    memset(&g_legacy_state, 0, sizeof(g_legacy_state));
    g_legacy_state.overwrite_reply = 'y';
    rc = fn_FreeArcExtract(legacy_cb, "x", dp_leg, "--", plain_arc, NULL);
    CHECK_TEST(rc == 0, "legacy allow overwrite rc == 0");

    f = fopen(hello_p, "rb");
    memset(buf, 0, sizeof(buf));
    fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    CHECK_TEST(strcmp(buf, "KEEP_ME") != 0, "overwrite 'y' replaced existing file");
  }

  /* 4. Modern unarc_run: list with list_item + extract */
  {
    HostState st;
    memset(&st, 0, sizeof(st));
    const char *l_argv[] = {"l", "--", plain_arc};
    rc = fn_unarc_run(3, l_argv, modern_cb, &st);
    CHECK_TEST(rc == 0, "modern list rc == 0");
    CHECK_TEST(st.seen_list_item > 0 && st.seen_total_files > 0 && st.orig_size > 0, "modern list callbacks");

    memset(&st, 0, sizeof(st));
    const char *x_argv[] = {"x", "-o+", dp_mod, "--", plain_arc};
    rc = fn_unarc_run(5, x_argv, modern_cb, &st);
    CHECK_TEST(rc == 0, "modern extract rc == 0");
    CHECK_TEST(st.seen_filename > 0 && st.seen_write > 0, "modern extract callbacks");
  }

  /* 5. Password retry via callback on -p and -hp archives, plus wrong-password failure */
  {
    HostState st;
    memset(&st, 0, sizeof(st));
    st.pwd0 = "wrong_first";
    st.pwd1 = secret;
    const char *argv_p[] = {"x", "-o+", dp_pw, "--", enc_p_arc};
    rc = fn_unarc_run(5, argv_p, modern_cb, &st);
    CHECK_TEST(rc == 0 && st.pwd_attempts == 2, "password retry on -p archive succeeded");

    memset(&st, 0, sizeof(st));
    st.pwd0 = "wrong_hp";
    st.pwd1 = secret;
    const char *argv_hp[] = {"x", "-o+", dp_hp, "--", enc_hp_arc};
    rc = fn_unarc_run(5, argv_hp, modern_cb, &st);
    CHECK_TEST(rc == 0 && st.pwd_attempts == 2, "password retry on -hp archive succeeded");

    memset(&st, 0, sizeof(st));
    st.pwd0 = "bad_only";
    st.pwd1 = NULL;
    const char *argv_bad[] = {"t", "--", enc_hp_arc};
    rc = fn_unarc_run(3, argv_bad, modern_cb, &st);
    CHECK_TEST(rc < 0 && st.seen_error > 0 && strlen(fn_unarc_last_error()) > 0,
               "wrong password returned negative error cleanly without exit");
  }

  /* 6. Corrupted archive, -ld limit, missing archive, and callback user-abort (-42) */
  {
    HostState st;
    memset(&st, 0, sizeof(st));
    const char *argv_cor[] = {"t", "--", corrupt_arc};
    rc = fn_unarc_run(3, argv_cor, modern_cb, &st);
    CHECK_TEST(rc < 0 && st.seen_error > 0, "corrupt archive fails safely without exit");

    memset(&st, 0, sizeof(st));
    const char *argv_ld[] = {"t", "-ld1k", "--", plain_arc};
    rc = fn_unarc_run(4, argv_ld, modern_cb, &st);
    CHECK_TEST(rc == UNARC_ERR_NOT_ENOUGH_MEMORY, "-ld1k returns UNARC_ERR_NOT_ENOUGH_MEMORY");

    memset(&st, 0, sizeof(st));
    st.abort_after_read = 1;
    const char *argv_ab[] = {"x", "-o+", dp_abort, "--", plain_arc};
    rc = fn_unarc_run(5, argv_ab, modern_cb, &st);
    CHECK_TEST(rc == -42, "callback abort returns exact negative code -42");

    memset(&st, 0, sizeof(st));
    const char *argv_miss[] = {"t", "--", "does_not_exist_12345.arc"};
    rc = fn_unarc_run(3, argv_miss, modern_cb, &st);
    CHECK_TEST(rc < 0, "missing archive returns negative error cleanly");
  }

  /* 7. Concurrent host threads + recovery extraction after all previous errors */
  {
    ThreadJob j1 = {plain_arc, out_t1, -1};
    ThreadJob j2 = {plain_arc, out_t2, -1};
#ifdef _WIN32
    HANDLE th[2];
    th[0] = CreateThread(NULL, 0, worker_fn, &j1, 0, NULL);
    th[1] = CreateThread(NULL, 0, worker_fn, &j2, 0, NULL);
    WaitForMultipleObjects(2, th, TRUE, INFINITE);
    CloseHandle(th[0]);
    CloseHandle(th[1]);
#else
    pthread_t th[2];
    pthread_create(&th[0], NULL, worker_fn, &j1);
    pthread_create(&th[1], NULL, worker_fn, &j2);
    pthread_join(th[0], NULL);
    pthread_join(th[1], NULL);
#endif
    CHECK_TEST(j1.rc == 0 && j2.rc == 0, "concurrent host threads succeeded");
  }

  fn_UnloadDLL();
  printf("ALL_LIB_HOST_TESTS_OK\n");
  return 0;
}

