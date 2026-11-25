/*
 * unarc.h - C library interface for FreeArc-c archive extraction & inspection
 *
 * Dual C API:
 *   1. Legacy Win32/Win64 Addon compatibility: FreeArcExtract() / UnloadDLL()
 *   2. Modern explicit context API: unarc_run(), unarc_last_error(), unarc_version(), unarc_cleanup()
 */

#ifndef FREEARC_UNARC_H
#define FREEARC_UNARC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef UNARC_CALL
#  ifdef _WIN32
#    define UNARC_CALL __stdcall
#  else
#    define UNARC_CALL
#  endif
#endif

#ifndef UNARC_API
#  ifdef _WIN32
#    if defined(UNARC_STATIC) || defined(FREEARC_STATIC_LIB)
#      define UNARC_API
#    elif defined(FREEARC_DLL_EXPORTS)
#      define UNARC_API __declspec(dllexport)
#    else
#      define UNARC_API __declspec(dllimport)
#    endif
#  else
#    if defined(__GNUC__) || defined(__clang__)
#      define UNARC_API __attribute__((visibility("default")))
#    else
#      define UNARC_API
#    endif
#  endif
#endif

/* Result & error codes (matching FREEARC_ERRCODE_* in Compression.h) */
#define UNARC_OK                         0
#define UNARC_ERR_GENERAL              (-1)
#define UNARC_ERR_INVALID_COMPRESSOR   (-2)
#define UNARC_ERR_ONLY_DECOMPRESS      (-3)
#define UNARC_ERR_OUTBLOCK_TOO_SMALL   (-4)
#define UNARC_ERR_NOT_ENOUGH_MEMORY    (-5)
#define UNARC_ERR_READ                 (-6)
#define UNARC_ERR_BAD_COMPRESSED_DATA  (-7)
#define UNARC_ERR_NOT_IMPLEMENTED      (-8)
#define UNARC_ERR_NO_MORE_DATA         (-9)
#define UNARC_ERR_TERMINATED          (-10)
#define UNARC_ERR_WRITE               (-11)

/* -------------------------------------------------------------------------
 * 1. Legacy FreeArc unarc.dll API (Addons/Unarc-DLL compatible)
 *
 * Callback events ("what"):
 *   "total"       : int1 = TotalBytes>>20, int2 = (int)TotalBytes, str = ""
 *   "read"        : int1 = ReadBytes>>20,  int2 = (int)ReadBytes,  str = ""
 *   "write"       : int1 = Written>>20,    int2 = (int)Written,    str = ""
 *   "filename"    : int1 = Filesize>>20,   int2 = (int)Filesize,   str = Name
 *   "overwrite?"  : int1 = Filesize>>20,   int2 = (int)Filesize,   str = Name
 *                   return: 'y' (yes), 'n' (no), 'a' (all), 's' (skip all), 'q' (abort)
 *   "password?"   : int1 = PasswordBufCap, int2 = 0,               str = PasswordBuf (UTF-8)
 *                   return: 'y' (provided), 'n' (none/give up), 'q' (abort)
 *   "error"       : int1 = Errcode,        int2 = 0,               str = ErrorMessage
 *   "total_files" : int1 = TotalFiles,     int2 = 0,               str = ""  (during 'l')
 *   "origsize"    : int1 = OrigSize>>20,   int2 = (int)OrigSize,   str = ""  (during 'l')
 *   "compsize"    : int1 = CompSize>>20,   int2 = (int)CompSize,   str = ""  (during 'l')
 *
 * For events other than "overwrite?", "password?", and "error", returning a
 * negative value immediately aborts and becomes the return value of FreeArcExtract().
 * Terminate the variadic argument list with NULL or "".
 * ------------------------------------------------------------------------- */
typedef int unarc_number;
typedef int (UNARC_CALL *unarc_legacy_callback)(
    char *what,
    unarc_number int1,
    unarc_number int2,
    char *str);

UNARC_API int FreeArcExtract(unarc_legacy_callback callback, ...);
UNARC_API void UNARC_CALL UnloadDLL(void);

/* -------------------------------------------------------------------------
 * 2. Modern C API (full 64-bit parameters, explicit argc/argv, userdata)
 *
 * Callback events ("what"):
 *   "total"       : param1 = TotalArchiveBytes, param2 = 0
 *   "read"        : param1 = ReadArchiveBytes,  param2 = 0
 *   "write"       : param1 = WrittenBytes,      param2 = 0
 *   "filename"    : param1 = FileSize,          param2 = isdir (0 or 1), str = FilePath
 *   "overwrite?"  : param1 = FileSize,          param2 = mtime (unix),   str = FilePath
 *                   return: 'y', 'n', 'a', 's', or 'q'
 *   "password?"   : param1 = (int64_t)str_cap,  param2 = attempt (0..),  str = PasswordBuf
 *                   write NUL-terminated UTF-8 password into str (< str_cap bytes)
 *                   return: 'y', 'n', or 'q'
 *   "error"       : param1 = Errcode,           param2 = 0,              str = ErrorMessage
 *   "total_files" : param1 = TotalFiles,        param2 = 0               (during 'l'/'v')
 *   "origsize"    : param1 = OrigSize,          param2 = 0               (during 'l'/'v')
 *   "compsize"    : param1 = CompSize,          param2 = 0               (during 'l'/'v')
 *   "list_item"   : param1 = FileSize,
 *                   param2 = ((int64_t)(isdir ? 1 : 0) << 32) | (uint32_t)crc32,
 *                   str    = FullPath                                    (during 'l'/'v')
 *
 * argv[0..argc-1] start directly with the command/options (e.g. {"x", "-o+", "-dpout", "--", "a.arc"});
 * do NOT pass a dummy program name as argv[0].
 * ------------------------------------------------------------------------- */
typedef int (UNARC_CALL *unarc_callback)(
    const char *what,
    int64_t param1,
    int64_t param2,
    char *str,
    size_t str_cap,
    void *userdata);

UNARC_API int UNARC_CALL unarc_run(
    int argc,
    const char *const *argv,
    unarc_callback callback,
    void *userdata);

UNARC_API const char * UNARC_CALL unarc_version(void);
UNARC_API const char * UNARC_CALL unarc_last_error(void);
UNARC_API const char * UNARC_CALL unarc_error_string(int errcode);
UNARC_API void UNARC_CALL unarc_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif /* FREEARC_UNARC_H */

