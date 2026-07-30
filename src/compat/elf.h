#ifndef SRC_COMPAT_ELF_H_
#define SRC_COMPAT_ELF_H_

#if defined(_WIN32) || defined(__APPLE__)
#include "third_party/musl/include/elf.h"
#else
#include <elf.h>
#endif

#ifndef SHF_COMPRESSED
#define SHF_COMPRESSED 0x800
#endif

#ifndef ELFCOMPRESS_ZLIB
#define ELFCOMPRESS_ZLIB 1
#endif

#ifndef EM_RISCV
#define EM_RISCV 243
#endif

#ifndef EM_NDS32
#define EM_NDS32 167
#endif

#endif // SRC_COMPAT_ELF_H_
