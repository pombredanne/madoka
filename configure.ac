#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([madoka], [0.1], [devel@dacci.org])
AM_INIT_AUTOMAKE([foreign subdir-objects])
AC_CONFIG_SRCDIR([include/madoka/common.h])
AC_CONFIG_HEADERS([config.h])
AC_CANONICAL_HOST

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_RANLIB

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([limits.h netdb.h stdint.h sys/ioctl.h sys/socket.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_ERROR_AT_LINE
AC_CHECK_FUNCS([memmove socket])

AC_ARG_ENABLE([win32],
              [AS_HELP_STRING([--enable-win32], [build windows specific codes (requires mingw)])],
              [enable_win32=true],
              [enable_win32=false])
AM_CONDITIONAL([ENABLE_WIN32], [test x$enable_win32 = xtrue])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
