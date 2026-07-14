#ifndef SRC_COMPAT_LINK_H_
#define SRC_COMPAT_LINK_H_

#if defined(__linux__) || defined(__FreeBSD__)
#include <link.h>

#else
#include <cstdint>
#if INTPTR_MAX == INT64_MAX
#define ElfW(type) Elf64_##type
#else
#define ElfW(type) Elf32_##type
#endif
#endif

#endif // SRC_COMPAT_LINK_H_
