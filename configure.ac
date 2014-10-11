#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
AC_PREREQ([2.69])
AC_INIT([ARIB STD-B25], [0.2.5], [kazhiro@marumo.ne.jp])
AM_INIT_AUTOMAKE([foreign subdir-objects])
AC_CONFIG_MACRO_DIR([m4])

AC_USE_SYSTEM_EXTENSIONS

AC_PROG_CC
AC_PROG_CC_STDC
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
LT_INIT([win32-dll shared disable-static])

AC_DEFINE([WIN32_LEAN_AND_MEAN],, [Define to limit the scope of <windows.h>.])

case "${host_os}" in
  *mingw32* | *cygwin*)
    SYS=mingw32
    ;;
esac

# Checks for libraries.
AS_IF([test "$SYS" = mingw32],[
  LDFLAGS="-no-undefined -lwinscard"
],[
  PKG_CHECK_MODULES(pcsclite, "libpcsclite", [
    AC_DEFINE(HAVE_PCSC, 1, "have libpcsclite")
  ], [AC_MSG_ERROR([${pcsclite_PKG_ERRORS}.])])
])

AC_CONFIG_FILES([Makefile src/arib25.pc])
AC_OUTPUT
