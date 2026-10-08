#ifndef FREEARC_WINCOMPAT_H
#define FREEARC_WINCOMPAT_H

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#include <io.h>
#include <sys/stat.h>

/* POSIX mode bit fallbacks for Windows */
#ifndef S_IFMT
#define S_IFMT   0170000
#endif
#ifndef S_IFLNK
#define S_IFLNK  0120000
#endif
#ifndef S_IFSOCK
#define S_IFSOCK 0140000
#endif

#ifndef S_ISLNK
#define S_ISLNK(m)  (((m) & S_IFMT) == S_IFLNK)
#endif
#ifndef S_ISSOCK
#define S_ISSOCK(m) (((m) & S_IFMT) == S_IFSOCK)
#endif

#ifndef S_ISUID
#define S_ISUID 04000
#endif
#ifndef S_ISGID
#define S_ISGID 02000
#endif
#ifndef S_ISVTX
#define S_ISVTX 01000
#endif

#ifndef S_IRUSR
#define S_IRUSR 0400
#define S_IWUSR 0200
#define S_IXUSR 0100
#endif
#ifndef S_IRGRP
#define S_IRGRP 0040
#define S_IWGRP 0020
#define S_IXGRP 0010
#endif
#ifndef S_IROTH
#define S_IROTH 0004
#define S_IWOTH 0002
#define S_IXOTH 0001
#endif

/* lstat alias */
#define lstat(path, st) stat(path, st)

#endif /* _WIN32 */

#endif /* FREEARC_WINCOMPAT_H */
