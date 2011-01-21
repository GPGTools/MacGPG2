#serial 8
dnl Copyright (C) 2002, 2005-2006, 2009-2010 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

AC_DEFUN([gl_UNISTD_SAFER],
[
  AC_CHECK_FUNCS_ONCE([pipe])
  AC_LIBOBJ([dup-safer])
  AC_LIBOBJ([fd-safer])
  AC_LIBOBJ([pipe-safer])
])
