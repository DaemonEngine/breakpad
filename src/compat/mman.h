#ifndef SRC_COMPAT_MMAN_H_
#define SRC_COMPAT_MMAN_H_

#if !defined(_WIN32)
#include <sys/mman.h>
#endif

#if defined(_WIN32)
#ifndef MAP_SHARED
#define MAP_SHARED 0x01
#endif

#ifndef MAP_PRIVATE
#define MAP_PRIVATE 0x02
#endif

#ifndef MAP_FAILED
#define MAP_FAILED ((void*)-1)
#endif

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS 0x20
#endif

#ifndef MAP_ANON
#define MAP_ANON MAP_ANONYMOUS
#endif
#endif

#if defined(__APPLE__)
#ifndef MAP_ANONYOUS
#define MAP_ANONYMOUS MAP_ANON
#endif
#endif

#if defined(_WIN32)
#ifndef PROT_READ
#define PROT_READ 0x1
#endif

#ifndef PROT_WRITE
#define PROT_WRITE 0x2
#endif

static inline void* mmap(void* /*addr*/,
                         size_t length,
                         int prot,
                         int flags,
                         int fd,
                         breakpad_off_t offset) {
  if (length == 0) {
    return MAP_FAILED;
  }

  if (fd == -1) {
    void* result = VirtualAlloc(nullptr, length, MEM_RESERVE | MEM_COMMIT,
                                (prot & PROT_WRITE) ? PAGE_READWRITE : PAGE_READONLY);

    if (result) {
      return result;
	}

	return MAP_FAILED;
  }

  HANDLE file = (HANDLE)_get_osfhandle(fd);
  if (file == INVALID_HANDLE_VALUE) {
    return MAP_FAILED;
  }

  DWORD protect;
  DWORD access;

  if (prot & PROT_WRITE) {
    if (flags & MAP_PRIVATE) {
      protect = PAGE_WRITECOPY;
      access  = FILE_MAP_COPY;
    } else {
      protect = PAGE_READWRITE;
      access  = FILE_MAP_WRITE;
    }
  } else {
    protect = PAGE_READONLY;
    access  = FILE_MAP_READ;
  }

  HANDLE mapping = CreateFileMapping(file, NULL, protect, 0, 0, NULL);

  if (!mapping) {
    return MAP_FAILED;
  }

  DWORD offset_high = static_cast<DWORD>((offset >> 32) & 0xffffffff);
  DWORD offset_low = static_cast<DWORD>(offset & 0xffffffff);

  void* result = MapViewOfFile(mapping, access, offset_high, offset_low, length);

  CloseHandle(mapping);

  if (!result) {
    return MAP_FAILED;
  }

  return result;
}

static inline int munmap(void* addr,
                         size_t /*length*/) {
  if (VirtualFree(addr, 0, MEM_RELEASE)) {
    return 0;
  }

  if (UnmapViewOfFile(addr)) {
    return 0;
  }

  return -1;
}

#define sys_mmap mmap
#define sys_munmap munmap
#endif

#endif // SRC_COMPAT_MMAN_H_
