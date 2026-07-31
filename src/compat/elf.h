#ifndef SRC_COMPAT_ELF_H_
#define SRC_COMPAT_ELF_H_

#if defined(_WIN32) || defined(__APPLE__)
#include "third_party/musl/include/elf.h"
#else
#include <elf.h>
#endif

#endif // SRC_COMPAT_ELF_H_
