# stdio_h.m4 serial 31
dnl Copyright (C) 2007-2010 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

AC_DEFUN([gl_STDIO_H],
[
  AC_REQUIRE([gl_STDIO_H_DEFAULTS])
  AC_REQUIRE([AC_C_INLINE])
  AC_REQUIRE([gl_ASM_SYMBOL_PREFIX])
  gl_CHECK_NEXT_HEADERS([stdio.h])
  dnl No need to create extra modules for these functions. Everyone who uses
  dnl <stdio.h> likely needs them.
  GNULIB_FPRINTF=1
  GNULIB_PRINTF=1
  GNULIB_VFPRINTF=1
  GNULIB_VPRINTF=1
  GNULIB_FPUTC=1
  GNULIB_PUTC=1
  GNULIB_PUTCHAR=1
  GNULIB_FPUTS=1
  GNULIB_PUTS=1
  GNULIB_FWRITE=1
  dnl This ifdef is just an optimization, to avoid performing a configure
  dnl check whose result is not used. It does not make the test of
  dnl GNULIB_STDIO_H_SIGPIPE or GNULIB_SIGPIPE redundant.
  m4_ifdef([gl_SIGNAL_SIGPIPE], [
    gl_SIGNAL_SIGPIPE
    if test $gl_cv_header_signal_h_SIGPIPE != yes; then
      REPLACE_STDIO_WRITE_FUNCS=1
      AC_LIBOBJ([stdio-write])
    fi
  ])

  dnl Check for declarations of anything we want to poison if the
  dnl corresponding gnulib module is not in use, and which is not
  dnl guaranteed by C89.
  gl_WARN_ON_USE_PREPARE([[#include <stdio.h>
    ]], [dprintf fpurge fseeko ftello getdelim getline popen renameat
    snprintf tmpfile vdprintf vsnprintf])
])

AC_DEFUN([gl_STDIO_MODULE_INDICATOR],
[
  dnl Use AC_REQUIRE here, so that the default settings are expanded once only.
  AC_REQUIRE([gl_STDIO_H_DEFAULTS])
  gl_MODULE_INDICATOR_SET_VARIABLE([$1])
  dnl Define it also as a C macro, for the benefit of the unit tests.
  gl_MODULE_INDICATOR_FOR_TESTS([$1])
])

AC_DEFUN([gl_STDIO_H_DEFAULTS],
[
  GNULIB_DPRINTF=0;              AC_SUBST([GNULIB_DPRINTF])
  GNULIB_FCLOSE=0;               AC_SUBST([GNULIB_FCLOSE])
  GNULIB_FFLUSH=0;               AC_SUBST([GNULIB_FFLUSH])
  GNULIB_FOPEN=0;                AC_SUBST([GNULIB_FOPEN])
  GNULIB_FPRINTF=0;              AC_SUBST([GNULIB_FPRINTF])
  GNULIB_FPRINTF_POSIX=0;        AC_SUBST([GNULIB_FPRINTF_POSIX])
  GNULIB_FPURGE=0;               AC_SUBST([GNULIB_FPURGE])
  GNULIB_FPUTC=0;                AC_SUBST([GNULIB_FPUTC])
  GNULIB_FPUTS=0;                AC_SUBST([GNULIB_FPUTS])
  GNULIB_FREOPEN=0;              AC_SUBST([GNULIB_FREOPEN])
  GNULIB_FSEEK=0;                AC_SUBST([GNULIB_FSEEK])
  GNULIB_FSEEKO=0;               AC_SUBST([GNULIB_FSEEKO])
  GNULIB_FTELL=0;                AC_SUBST([GNULIB_FTELL])
  GNULIB_FTELLO=0;               AC_SUBST([GNULIB_FTELLO])
  GNULIB_FWRITE=0;               AC_SUBST([GNULIB_FWRITE])
  GNULIB_GETDELIM=0;             AC_SUBST([GNULIB_GETDELIM])
  GNULIB_GETLINE=0;              AC_SUBST([GNULIB_GETLINE])
  GNULIB_OBSTACK_PRINTF=0;       AC_SUBST([GNULIB_OBSTACK_PRINTF])
  GNULIB_OBSTACK_PRINTF_POSIX=0; AC_SUBST([GNULIB_OBSTACK_PRINTF_POSIX])
  GNULIB_PERROR=0;               AC_SUBST([GNULIB_PERROR])
  GNULIB_POPEN=0;                AC_SUBST([GNULIB_POPEN])
  GNULIB_PRINTF=0;               AC_SUBST([GNULIB_PRINTF])
  GNULIB_PRINTF_POSIX=0;         AC_SUBST([GNULIB_PRINTF_POSIX])
  GNULIB_PUTC=0;                 AC_SUBST([GNULIB_PUTC])
  GNULIB_PUTCHAR=0;              AC_SUBST([GNULIB_PUTCHAR])
  GNULIB_PUTS=0;                 AC_SUBST([GNULIB_PUTS])
  GNULIB_REMOVE=0;               AC_SUBST([GNULIB_REMOVE])
  GNULIB_RENAME=0;               AC_SUBST([GNULIB_RENAME])
  GNULIB_RENAMEAT=0;             AC_SUBST([GNULIB_RENAMEAT])
  GNULIB_SNPRINTF=0;             AC_SUBST([GNULIB_SNPRINTF])
  GNULIB_SPRINTF_POSIX=0;        AC_SUBST([GNULIB_SPRINTF_POSIX])
  GNULIB_STDIO_H_SIGPIPE=0;      AC_SUBST([GNULIB_STDIO_H_SIGPIPE])
  GNULIB_TMPFILE=0;              AC_SUBST([GNULIB_TMPFILE])
  GNULIB_VASPRINTF=0;            AC_SUBST([GNULIB_VASPRINTF])
  GNULIB_VDPRINTF=0;             AC_SUBST([GNULIB_VDPRINTF])
  GNULIB_VFPRINTF=0;             AC_SUBST([GNULIB_VFPRINTF])
  GNULIB_VFPRINTF_POSIX=0;       AC_SUBST([GNULIB_VFPRINTF_POSIX])
  GNULIB_VPRINTF=0;              AC_SUBST([GNULIB_VPRINTF])
  GNULIB_VPRINTF_POSIX=0;        AC_SUBST([GNULIB_VPRINTF_POSIX])
  GNULIB_VSNPRINTF=0;            AC_SUBST([GNULIB_VSNPRINTF])
  GNULIB_VSPRINTF_POSIX=0;       AC_SUBST([GNULIB_VSPRINTF_POSIX])
  dnl Assume proper GNU behavior unless another module says otherwise.
  HAVE_DECL_FPURGE=1;            AC_SUBST([HAVE_DECL_FPURGE])
  HAVE_DECL_GETDELIM=1;          AC_SUBST([HAVE_DECL_GETDELIM])
  HAVE_DECL_GETLINE=1;           AC_SUBST([HAVE_DECL_GETLINE])
  HAVE_DECL_OBSTACK_PRINTF=1;    AC_SUBST([HAVE_DECL_OBSTACK_PRINTF])
  HAVE_DECL_SNPRINTF=1;          AC_SUBST([HAVE_DECL_SNPRINTF])
  HAVE_DECL_VSNPRINTF=1;         AC_SUBST([HAVE_DECL_VSNPRINTF])
  HAVE_DPRINTF=1;                AC_SUBST([HAVE_DPRINTF])
  HAVE_FSEEKO=1;                 AC_SUBST([HAVE_FSEEKO])
  HAVE_FTELLO=1;                 AC_SUBST([HAVE_FTELLO])
  HAVE_RENAMEAT=1;               AC_SUBST([HAVE_RENAMEAT])
  HAVE_VASPRINTF=1;              AC_SUBST([HAVE_VASPRINTF])
  HAVE_VDPRINTF=1;               AC_SUBST([HAVE_VDPRINTF])
  REPLACE_DPRINTF=0;             AC_SUBST([REPLACE_DPRINTF])
  REPLACE_FCLOSE=0;              AC_SUBST([REPLACE_FCLOSE])
  REPLACE_FFLUSH=0;              AC_SUBST([REPLACE_FFLUSH])
  REPLACE_FOPEN=0;               AC_SUBST([REPLACE_FOPEN])
  REPLACE_FPRINTF=0;             AC_SUBST([REPLACE_FPRINTF])
  REPLACE_FPURGE=0;              AC_SUBST([REPLACE_FPURGE])
  REPLACE_FREOPEN=0;             AC_SUBST([REPLACE_FREOPEN])
  REPLACE_FSEEK=0;               AC_SUBST([REPLACE_FSEEK])
  REPLACE_FSEEKO=0;              AC_SUBST([REPLACE_FSEEKO])
  REPLACE_FTELL=0;               AC_SUBST([REPLACE_FTELL])
  REPLACE_FTELLO=0;              AC_SUBST([REPLACE_FTELLO])
  REPLACE_GETDELIM=0;            AC_SUBST([REPLACE_GETDELIM])
  REPLACE_GETLINE=0;             AC_SUBST([REPLACE_GETLINE])
  REPLACE_OBSTACK_PRINTF=0;      AC_SUBST([REPLACE_OBSTACK_PRINTF])
  REPLACE_PERROR=0;              AC_SUBST([REPLACE_PERROR])
  REPLACE_POPEN=0;               AC_SUBST([REPLACE_POPEN])
  REPLACE_PRINTF=0;              AC_SUBST([REPLACE_PRINTF])
  REPLACE_REMOVE=0;              AC_SUBST([REPLACE_REMOVE])
  REPLACE_RENAME=0;              AC_SUBST([REPLACE_RENAME])
  REPLACE_RENAMEAT=0;            AC_SUBST([REPLACE_RENAMEAT])
  REPLACE_SNPRINTF=0;            AC_SUBST([REPLACE_SNPRINTF])
  REPLACE_SPRINTF=0;             AC_SUBST([REPLACE_SPRINTF])
  REPLACE_STDIO_WRITE_FUNCS=0;   AC_SUBST([REPLACE_STDIO_WRITE_FUNCS])
  REPLACE_TMPFILE=0;             AC_SUBST([REPLACE_TMPFILE])
  REPLACE_VASPRINTF=0;           AC_SUBST([REPLACE_VASPRINTF])
  REPLACE_VDPRINTF=0;            AC_SUBST([REPLACE_VDPRINTF])
  REPLACE_VFPRINTF=0;            AC_SUBST([REPLACE_VFPRINTF])
  REPLACE_VPRINTF=0;             AC_SUBST([REPLACE_VPRINTF])
  REPLACE_VSNPRINTF=0;           AC_SUBST([REPLACE_VSNPRINTF])
  REPLACE_VSPRINTF=0;            AC_SUBST([REPLACE_VSPRINTF])
])

dnl Code shared by fseeko and ftello.  Determine if large files are supported,
dnl but stdin does not start as a large file by default.
AC_DEFUN([gl_STDIN_LARGE_OFFSET],
  [
    AC_CACHE_CHECK([whether stdin defaults to large file offsets],
      [gl_cv_var_stdin_large_offset],
      [AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <stdio.h>]],
[[#if defined __SL64 && defined __SCLE /* cygwin */
  /* Cygwin 1.5.24 and earlier fail to put stdin in 64-bit mode, making
     fseeko/ftello needlessly fail.  This bug was fixed in 1.5.25, and
     it is easier to do a version check than building a runtime test.  */
# include <cygwin/version.h>
# if CYGWIN_VERSION_DLL_COMBINED < CYGWIN_VERSION_DLL_MAKE_COMBINED (1005, 25)
  choke me
# endif
#endif]])],
        [gl_cv_var_stdin_large_offset=yes],
        [gl_cv_var_stdin_large_offset=no])])
])
