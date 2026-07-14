#ifndef SRC_COMPAT_WINDOWS_LINUX_H_
#define SRC_COMPAT_WINDOWS_LINUX_H_

#if !defined(_WIN32)
#error This file is a Windows wrapper!
#endif

#include <windows.h>
#include <stdio.h>
#include <sys/stat.h>

#ifndef N_UNDF
#define N_UNDF 0
#endif

#ifndef _PATH_DEVNULL
#define _PATH_DEVNULL "NUL"
#endif

#ifndef NAME_MAX
#define NAME_MAX 255
#endif

typedef int64_t breakpad_off_t;

static inline int getpagesize(void) {
  SYSTEM_INFO system_info;
  GetSystemInfo(&system_info);
  return (int)system_info.dwPageSize;
}

static inline ssize_t pread(int fd,
                            void* buf,
                            size_t count,
                            breakpad_off_t offset) {
  __int64 current = _lseeki64(fd, 0, SEEK_CUR);
  if (current == -1) {
    return -1;
  }

  if (_lseeki64(fd, offset, SEEK_SET) == -1) {
    return -1;
  }

  int result = _read(fd, buf, (unsigned int)count);

  _lseeki64(fd, current, SEEK_SET);

  return result;
}

static inline char* breakpad_realpath(const char* path,
                                      char* resolved)
{
  DWORD len = GetFullPathNameA(path, MAX_PATH, resolved, nullptr);

  if (len == 0 || len >= MAX_PATH) {
    return nullptr;
  }

  return resolved;
}

#ifndef realpath
#define realpath breakpad_realpath
#endif

#endif // SRC_COMPAT_WINDOWS_LINUX_H_
