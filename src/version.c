#include <stdio.h>
#include <config.h>

#if defined(__GNUC__) || defined(__clang__)
#  if !defined(__APPLE__)
const char elf_interp[] __attribute__((section(".interp"))) = ELF_INTERP;
#  endif
#include <unistd.h>

void show_version(void)
{
	fprintf(stderr, "libarib25.so - ARIB STD-B25 shared library version %s (%s)\n", ARIB25_VERSION_STRING, BUILD_GIT_REVISION);
	fprintf(stderr, "  built with %s %s on %s\n", BUILD_CC_NAME, BUILD_CC_VERSION, BUILD_OS_NAME);
	_exit(0);
}

#endif
