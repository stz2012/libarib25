#ifndef PORTABLE_H
#define PORTABLE_H

#if (MSC_VER < 1300) 

typedef unsigned char     uint8_t;
typedef   signed char      int8_t;
typedef unsigned short   uint16_t;
typedef   signed short    int16_t;
typedef unsigned int     uint32_t;
typedef   signed int      int32_t;
typedef unsigned __int64 uint64_t;
typedef   signed __int64  int64_t;

#else

#include <inttypes.h>

#endif

#endif /* PORTABLE_H */
