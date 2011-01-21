## posix-sys-pth-impl.h - Include fragment to build assuan.h.
## Copyright (C) 2009, 2010  Free Software Foundation, Inc.
##
## This file is part of Assuan.
##
## Assuan is free software; you can redistribute it and/or modify it
## under the terms of the GNU Lesser General Public License as
## published by the Free Software Foundation; either version 2.1 of
## the License, or (at your option) any later version.
##
## Assuan is distributed in the hope that it will be useful, but
## WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
## Lesser General Public License for more details.
##
## You should have received a copy of the GNU Lesser General Public
## License along with this program; if not, see <http://www.gnu.org/licenses/>.
##
##
## This file is included by the mkheader tool.  Lines starting with
## a double hash mark are not copied to the destination file.
##
## Warning: This is a fragment of a macro - no empty lines please.
  static int _assuan_pth_recvmsg (assuan_context_t ctx, assuan_fd_t fd, \
				  assuan_msghdr_t msg, int flags)	\
  {									\
    /* Pth does not provide a recvmsg function.  We implement it.  */	\
    int ret;								\
    int fdmode;								\
									\
    (void) ctx;								\
    fdmode = pth_fdmode (fd, PTH_FDMODE_POLL);				\
    if (fdmode == PTH_FDMODE_ERROR)					\
      {									\
        errno = EBADF;							\
        return -1;							\
      }									\
    if (fdmode == PTH_FDMODE_BLOCK)					\
      {									\
        fd_set fds;							\
									\
	FD_ZERO (&fds);							\
	FD_SET (fd, &fds);						\
	while ((ret = pth_select (fd + 1, &fds, NULL, NULL, NULL)) < 0	\
	       && errno == EINTR)					\
	  ;								\
	if (ret < 0)							\
	  return -1;							\
      }									\
    									\
    while ((ret = recvmsg (fd, msg, flags)) == -1 && errno == EINTR)	\
      ;									\
    return ret;								\
  }									\
  static int _assuan_pth_sendmsg (assuan_context_t ctx, assuan_fd_t fd, \
				  const assuan_msghdr_t msg, int flags) \
  {									\
    /* Pth does not provide a sendmsg function.  We implement it.  */	\
    int ret;								\
    int fdmode;								\
									\
    (void) ctx;								\
    fdmode = pth_fdmode (fd, PTH_FDMODE_POLL);				\
    if (fdmode == PTH_FDMODE_ERROR)					\
      {									\
        errno = EBADF;							\
	return -1;							\
      }									\
    if (fdmode == PTH_FDMODE_BLOCK)					\
      {									\
        fd_set fds;							\
									\
	FD_ZERO (&fds);							\
	FD_SET (fd, &fds);						\
	while ((ret = pth_select (fd + 1, NULL, &fds, NULL, NULL)) < 0	\
	       && errno == EINTR)					\
	  ;								\
	if (ret < 0)							\
	  return -1;							\
	}								\
									\
    while ((ret = sendmsg (fd, msg, flags)) == -1 && errno == EINTR)	\
      ;									\
    return ret;								\
  }                                                                     \
##EOF## Force end-of file.


