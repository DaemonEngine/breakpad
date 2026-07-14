#ifndef SRC_COMPAT_INET_H_
#define SRC_COMPAT_INET_H_

#if defined(_WIN32)
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

#endif // SRC_COMPAT_INET_H_
