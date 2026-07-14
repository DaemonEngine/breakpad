#ifndef SRC_COMPAT_LINUX_H_
#define SRC_COMPAT_LINUX_H_

#ifndef _SSIZE_T_DEFINED
#include <stdint.h>
#if defined(_WIN32)
typedef __int64 ssize_t;
#elif defined(__APPLE__)
typedef __darwin_ssize_t ssize_t;
#elif defined(__FreeBSD__)
typedef __int64_t ssize_t;
#endif
#endif

#if defined(_WIN32) || defined(__APPLE__)
#include <stddef.h>

static inline void* memrchr(const void* s,
                            int c,
                            size_t n) {
  const unsigned char* p = static_cast<const unsigned char*>(s) + n;

  while (n--) {
    if (*(--p) == static_cast<unsigned char>(c)) {
      return const_cast<unsigned char*>(p);
    }
  }

  return nullptr;
}
#endif

#if defined(_WIN32)
#include "compat/windows/linux.h"
#endif

#ifndef __LITTLE_ENDIAN
#define __LITTLE_ENDIAN 1234
#endif

#ifndef __BIG_ENDIAN
#define __BIG_ENDIAN 4321
#endif

#ifndef __BYTE_ORDER
#define __BYTE_ORDER __LITTLE_ENDIAN
#endif

#endif // SRC_COMPAT_LINUX_H_
