#ifndef SRC_COMPAT_GUIDDEF_H_
#define SRC_COMPAT_GUIDDEF_H_

#if defined(_WIN32)
#include <guiddef.h>
#else
typedef MDGUID GUID;
#endif

#if defined(_WIN32)
#define COMPAT_GUID_DATA1 Data1
#define COMPAT_GUID_DATA2 Data2
#define COMPAT_GUID_DATA3 Data3
#define COMPAT_GUID_DATA4 Data4
#else
#define COMPAT_GUID_DATA1 data1
#define COMPAT_GUID_DATA2 data2
#define COMPAT_GUID_DATA3 data3
#define COMPAT_GUID_DATA4 data4
#endif

#endif // SRC_COMPAT_GUIDDEF_H_
