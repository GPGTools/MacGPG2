# putenv.m4 serial 16
dnl Copyright (C) 2002-2010 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

dnl From Jim Meyering.
dnl
dnl Check whether putenv ("FOO") removes FOO from the environment.
dnl The putenv in libc on at least SunOS 4.1.4 does *not* do that.

AC_DEFUN([gl_FUNC_PUTENV],
[
  AC_REQUIRE([gl_STDLIB_H_DEFAULTS])
  AC_CACHE_CHECK([for putenv compatible with GNU and SVID],
   [gl_cv_func_svid_putenv],
   [AC_RUN_IFELSE([AC_LANG_PROGRAM([AC_INCLUDES_DEFAULT],[[
    /* Put it in env.  */
    if (putenv ("CONFTEST_putenv=val"))
      return 1;

    /* Try to remove it.  */
    if (putenv ("CONFTEST_putenv"))
      return 1;

    /* Make sure it was deleted.  */
    if (getenv ("CONFTEST_putenv") != 0)
      return 1;

    return 0;
              ]])],
             gl_cv_func_svid_putenv=yes,
             gl_cv_func_svid_putenv=no,
             dnl When crosscompiling, assume putenv is broken.
             gl_cv_func_svid_putenv=no)
   ])
  if test $gl_cv_func_svid_putenv = no; then
    REPLACE_PUTENV=1
    AC_LIBOBJ([putenv])
  fi
])
