/* Substitute for <sys/select.h>.
   Copyright (C) 2007-2010 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

# if __GNUC__ >= 3
@PRAGMA_SYSTEM_HEADER@
# endif

/* On OSF/1, <sys/types.h> and <sys/time.h> include <sys/select.h>.
   Simply delegate to the system's header in this case.  */
#if @HAVE_SYS_SELECT_H@ && defined __osf__ && (defined _SYS_TYPES_H_ && !defined _GL_SYS_SELECT_H_REDIRECT_FROM_SYS_TYPES_H) && defined _OSF_SOURCE

# define _GL_SYS_SELECT_H_REDIRECT_FROM_SYS_TYPES_H
# @INCLUDE_NEXT@ @NEXT_SYS_SELECT_H@

#elif @HAVE_SYS_SELECT_H@ && defined __osf__ && (defined _SYS_TIME_H_ && !defined _GL_SYS_SELECT_H_REDIRECT_FROM_SYS_TIME_H) && defined _OSF_SOURCE

# define _GL_SYS_SELECT_H_REDIRECT_FROM_SYS_TIME_H
# @INCLUDE_NEXT@ @NEXT_SYS_SELECT_H@

#else

#ifndef _GL_SYS_SELECT_H

#if @HAVE_SYS_SELECT_H@

/* On many platforms, <sys/select.h> assumes prior inclusion of
   <sys/types.h>.  */
# include <sys/types.h>

/* On OSF/1 4.0, <sys/select.h> provides only a forward declaration
   of 'struct timeval', and no definition of this type.
   But avoid namespace pollution on glibc systems.  */
# ifndef __GLIBC__
#  include <sys/time.h>
# endif

/* On Solaris 10, <sys/select.h> provides an FD_ZERO implementation
   that relies on memset(), but without including <string.h>.
   But avoid namespace pollution on glibc systems.  */
# ifndef __GLIBC__
#  include <string.h>
# endif

/* The include_next requires a split double-inclusion guard.  */
# @INCLUDE_NEXT@ @NEXT_SYS_SELECT_H@

#endif

#ifndef _GL_SYS_SELECT_H
#define _GL_SYS_SELECT_H

#if !@HAVE_SYS_SELECT_H@ || @REPLACE_SELECT@
/* A platform that lacks <sys/select.h>.  */
# include <sys/socket.h>
#endif

/* The definitions of _GL_FUNCDECL_RPL etc. are copied here.  */

/* The definition of _GL_WARN_ON_USE is copied here.  */


#if @GNULIB_SELECT@
# if @HAVE_WINSOCK2_H@ || @REPLACE_SELECT@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef select
#   define select rpl_select
#  endif
_GL_FUNCDECL_RPL (select, int,
                  (int, fd_set *, fd_set *, fd_set *, struct timeval *));
_GL_CXXALIAS_RPL (select, int,
                  (int, fd_set *, fd_set *, fd_set *, struct timeval *));
# else
_GL_CXXALIAS_SYS (select, int,
                  (int, fd_set *, fd_set *, fd_set *, struct timeval *));
# endif
_GL_CXXALIASWARN (select);
#elif @HAVE_WINSOCK2_H@
# undef select
# define select select_used_without_requesting_gnulib_module_select
#elif defined GNULIB_POSIXCHECK
# undef select
# if HAVE_RAW_DECL_SELECT
_GL_WARN_ON_USE (select, "select is not always POSIX compliant - "
                 "use gnulib module select for portability");
# endif
#endif


#endif /* _GL_SYS_SELECT_H */
#endif /* _GL_SYS_SELECT_H */
#endif /* OSF/1 */
