#ifndef PORTABLE_H
#define PORTABLE_H

#if (defined(_WIN32) && defined(_MSC_VER) && _MSC_VER < 1800)

typedef unsigned char     uint8_t;
typedef   signed char      int8_t;
typedef unsigned short   uint16_t;
typedef   signed short    int16_t;
typedef unsigned int     uint32_t;
typedef   signed int      int32_t;
typedef unsigned __int64 uint64_t;
typedef   signed __int64  int64_t;

#else

#include <stdint.h>
#include <inttypes.h>

#endif

#if !defined(_WIN32)
	#define _open  open
	#define _close close
	#define _read  read
	#define _write write
	#define _lseeki64 lseek
	#define _telli64(fd)  (lseek(fd,0,SEEK_CUR))
	#define _O_BINARY     (0)
	#define _O_RDONLY     (O_RDONLY)
	#define _O_WRONLY     (O_WRONLY)
	#define _O_SEQUENTIAL (0)
	#define _O_CREAT      (O_CREAT)
	#define _O_TRUNC      (O_TRUNC)
	#define _S_IREAD      (S_IRUSR|S_IRGRP|S_IROTH)
	#define _S_IWRITE     (S_IWUSR|S_IWGRP|S_IWOTH)
#endif

#if defined(__GNUC__) && (__GNUC__ < 5 && !(__GNUC__ == 4 &&  9 <= __GNUC_MINOR__))
	#define NO_MM_UNDEFINED
#endif
#if defined(__clang__) && (__clang_major__ < 4 && !(__clang_major__ == 3 &&  8 <= __clang_minor__))
	#define NO_MM_UNDEFINED
#endif

#endif /* PORTABLE_H */
