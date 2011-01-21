/* Internal implementation of access control lists.

   Copyright (C) 2002-2003, 2005-2010 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.

   Written by Paul Eggert, Andreas Grünbacher, and Bruno Haible.  */

#include "acl.h"

#include <stdbool.h>
#include <stdlib.h>

/* All systems define the ACL related API in <sys/acl.h>.  */
#if HAVE_SYS_ACL_H
# include <sys/acl.h>
#endif
#if defined HAVE_ACL && ! defined GETACLCNT && defined ACL_CNT
# define GETACLCNT ACL_CNT
#endif

/* On Linux, additional ACL related API is available in <acl/libacl.h>.  */
#ifdef HAVE_ACL_LIBACL_H
# include <acl/libacl.h>
#endif

#include "error.h"
#include "quote.h"

#include <errno.h>
#ifndef ENOSYS
# define ENOSYS (-1)
#endif
#ifndef ENOTSUP
# define ENOTSUP (-1)
#endif

#ifndef HAVE_FCHMOD
# define HAVE_FCHMOD false
# define fchmod(fd, mode) (-1)
#endif


#if USE_ACL

# if HAVE_ACL_GET_FILE
/* POSIX 1003.1e (draft 17 -- abandoned) specific version.  */
/* Linux, FreeBSD, MacOS X, IRIX, Tru64 */

#  ifndef MIN_ACL_ENTRIES
#   define MIN_ACL_ENTRIES 4
#  endif

/* POSIX 1003.1e (draft 17) */
#  ifdef HAVE_ACL_GET_FD
/* Most platforms have a 1-argument acl_get_fd, only OSF/1 has a 2-argument
   macro(!).  */
#   if HAVE_ACL_FREE_TEXT /* OSF/1 */
static inline acl_t
rpl_acl_get_fd (int fd)
{
  return acl_get_fd (fd, ACL_TYPE_ACCESS);
}
#    undef acl_get_fd
#    define acl_get_fd rpl_acl_get_fd
#   endif
#  else
#   define HAVE_ACL_GET_FD false
#   undef acl_get_fd
#   define acl_get_fd(fd) (NULL)
#  endif

/* POSIX 1003.1e (draft 17) */
#  ifdef HAVE_ACL_SET_FD
/* Most platforms have a 2-argument acl_set_fd, only OSF/1 has a 3-argument
   macro(!).  */
#   if HAVE_ACL_FREE_TEXT /* OSF/1 */
static inline int
rpl_acl_set_fd (int fd, acl_t acl)
{
  return acl_set_fd (fd, ACL_TYPE_ACCESS, acl);
}
#    undef acl_set_fd
#    define acl_set_fd rpl_acl_set_fd
#   endif
#  else
#   define HAVE_ACL_SET_FD false
#   undef acl_set_fd
#   define acl_set_fd(fd, acl) (-1)
#  endif

/* POSIX 1003.1e (draft 13) */
#  if ! HAVE_ACL_FREE_TEXT
#   define acl_free_text(buf) acl_free (buf)
#  endif

/* Linux-specific */
#  ifndef HAVE_ACL_EXTENDED_FILE
#   define HAVE_ACL_EXTENDED_FILE false
#   define acl_extended_file(name) (-1)
#  endif

/* Linux-specific */
#  ifndef HAVE_ACL_FROM_MODE
#   define HAVE_ACL_FROM_MODE false
#   define acl_from_mode(mode) (NULL)
#  endif

/* Set to 1 if a file's mode is implicit by the ACL.
   Set to 0 if a file's mode is stored independently from the ACL.  */
#  if HAVE_ACL_COPY_EXT_NATIVE && HAVE_ACL_CREATE_ENTRY_NP /* MacOS X */
#   define MODE_INSIDE_ACL 0
#  else
#   define MODE_INSIDE_ACL 1
#  endif

#  if defined __APPLE__ && defined __MACH__ /* MacOS X */
#   define ACL_NOT_WELL_SUPPORTED(Err) \
     ((Err) == ENOTSUP || (Err) == ENOSYS || (Err) == EINVAL || (Err) == EBUSY || (Err) == ENOENT)
#  elif defined EOPNOTSUPP /* Tru64 NFS */
#   define ACL_NOT_WELL_SUPPORTED(Err) \
     ((Err) == ENOTSUP || (Err) == ENOSYS || (Err) == EINVAL || (Err) == EBUSY || (Err) == EOPNOTSUPP)
#  else
#   define ACL_NOT_WELL_SUPPORTED(Err) \
     ((Err) == ENOTSUP || (Err) == ENOSYS || (Err) == EINVAL || (Err) == EBUSY)
#  endif

/* Return the number of entries in ACL.
   Return -1 and set errno upon failure to determine it.  */
/* Define a replacement for acl_entries if needed. (Only Linux has it.)  */
#  if !HAVE_ACL_ENTRIES
#   define acl_entries rpl_acl_entries
extern int acl_entries (acl_t);
#  endif

#  if HAVE_ACL_TYPE_EXTENDED /* MacOS X */
/* ACL is an ACL, from a file, stored as type ACL_TYPE_EXTENDED.
   Return 1 if the given ACL is non-trivial.
   Return 0 if it is trivial.  */
extern int acl_extended_nontrivial (acl_t);
#  else
/* ACL is an ACL, from a file, stored as type ACL_TYPE_ACCESS.
   Return 1 if the given ACL is non-trivial.
   Return 0 if it is trivial, i.e. equivalent to a simple stat() mode.
   Return -1 and set errno upon failure to determine it.  */
extern int acl_access_nontrivial (acl_t);
#  endif

# elif HAVE_ACL && defined GETACL /* Solaris, Cygwin, not HP-UX */

/* Set to 1 if a file's mode is implicit by the ACL.
   Set to 0 if a file's mode is stored independently from the ACL.  */
#  if defined __CYGWIN__ /* Cygwin */
#   define MODE_INSIDE_ACL 0
#  else /* Solaris */
#   define MODE_INSIDE_ACL 1
#  endif

#  if !defined ACL_NO_TRIVIAL /* Solaris <= 10, Cygwin */

/* Return 1 if the given ACL is non-trivial.
   Return 0 if it is trivial, i.e. equivalent to a simple stat() mode.  */
extern int acl_nontrivial (int count, aclent_t *entries);

#   ifdef ACE_GETACL /* Solaris 10 */

/* Test an ACL retrieved with ACE_GETACL.
   Return 1 if the given ACL, consisting of COUNT entries, is non-trivial.
   Return 0 if it is trivial, i.e. equivalent to a simple stat() mode.  */
extern int acl_ace_nontrivial (int count, ace_t *entries);

/* Definitions for when the built executable is executed on Solaris 10
   (newer version) or Solaris 11.  */
/* For a_type.  */
#    define ACE_ACCESS_ALLOWED_ACE_TYPE 0 /* replaces ALLOW */
#    define ACE_ACCESS_DENIED_ACE_TYPE  1 /* replaces DENY */
/* For a_flags.  */
#    define NEW_ACE_OWNER            0x1000
#    define NEW_ACE_GROUP            0x2000
#    define NEW_ACE_IDENTIFIER_GROUP 0x0040
#    define ACE_EVERYONE             0x4000
/* For a_access_mask.  */
#    define NEW_ACE_READ_DATA  0x001 /* corresponds to 'r' */
#    define NEW_ACE_WRITE_DATA 0x002 /* corresponds to 'w' */
#    define NEW_ACE_EXECUTE    0x004 /* corresponds to 'x' */

#   endif

#  endif

# elif HAVE_GETACL /* HP-UX */

/* Return 1 if the given ACL is non-trivial.
   Return 0 if it is trivial, i.e. equivalent to a simple stat() mode.  */
extern int acl_nontrivial (int count, struct acl_entry *entries, struct stat *sb);

# elif HAVE_ACLX_GET && 0 /* AIX */

/* TODO */

# elif HAVE_STATACL /* older AIX */

/* Return 1 if the given ACL is non-trivial.
   Return 0 if it is trivial, i.e. equivalent to a simple stat() mode.  */
extern int acl_nontrivial (struct acl *a);

# endif

#endif
