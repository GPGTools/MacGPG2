## w32-fd-t.inc.h - Include fragment to build assuan.h.
## Copyright (C) 2010  Free Software Foundation, Inc.
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

/* Because we use system handles and not libc low level file
   descriptors on W32, we need to declare them as HANDLE (which
   actually is a plain pointer).  This is required to eventually
   support 64 bit Windows systems.  */
typedef void *assuan_fd_t;
#define ASSUAN_INVALID_FD ((void*)(-1))
#define ASSUAN_INVALID_PID ((pid_t) -1)
static inline assuan_fd_t
assuan_fd_from_posix_fd (int fd)
{
  if (fd < 0)
    return ASSUAN_INVALID_FD;
  else
    return (assuan_fd_t) _get_osfhandle (fd);
}

##EOF##
