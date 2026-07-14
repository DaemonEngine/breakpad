#ifndef SRC_COMPAT_SYSCALL_H_
#define SRC_COMPAT_SYSCALL_H_

#if defined(__linux__)
#include "third_party/lss/linux_syscall_support.h"
#endif

#if defined(__APPLE__) || defined(__FreeBSD__)
#include <unistd.h>
#include <sys/stat.h>

#define sys_mmap mmap
#define sys_munmap munmap

#define sys_open open
#define sys_close close
#define sys_readlink readlink
#define sys_fstat fstat

#define kernel_stat stat

#elif defined(_WIN32)
#define sys_open _open
#define sys_close _close

#define kernel_stat stat

static inline ssize_t sys_readlink(const char* /*path*/,
                                   char* /*buffer*/,
                                   size_t /*buffer_size*/) {
  // Windows doesn't have a LINUX readlink().
  return -1;
}

static inline int sys_fstat(int fd,
                            struct kernel_stat* st) {
  struct _stat64 win_st;

  if (_fstat64(fd, &win_st) != 0) {
    return -1;
  }

  st->st_size = win_st.st_size;
  return 0;
}
#endif

#endif // SRC_COMPAT_SYSCALL_H_
