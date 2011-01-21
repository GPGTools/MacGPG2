# serial 20

# Copyright (C) 1997-2001, 2003-2010 Free Software Foundation, Inc.
#
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

dnl From Jim Meyering.

AC_DEFUN([gl_FUNC_LSTAT],
[
  AC_REQUIRE([gl_SYS_STAT_H_DEFAULTS])
  dnl If lstat does not exist, the replacement <sys/stat.h> does
  dnl "#define lstat stat", and lstat.c is a no-op.
  AC_CHECK_FUNCS_ONCE([lstat])
  if test $ac_cv_func_lstat = yes; then
    AC_REQUIRE([AC_FUNC_LSTAT_FOLLOWS_SLASHED_SYMLINK])
    if test $ac_cv_func_lstat_dereferences_slashed_symlink = no; then
      dnl Note: AC_FUNC_LSTAT_FOLLOWS_SLASHED_SYMLINK does AC_LIBOBJ([lstat]).
      REPLACE_LSTAT=1
    fi
    # Prerequisites of lib/lstat.c.
    AC_REQUIRE([AC_C_INLINE])
  else
    HAVE_LSTAT=0
  fi
])
