## w32ce-add.h - Include fragment to build assuan.h.
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

HANDLE _assuan_w32ce_prepare_pipe (int *r_rvid, int write_end);
HANDLE _assuan_w32ce_finish_pipe (int rvid, int write_end);
DWORD _assuan_w32ce_create_pipe (HANDLE *read_hd, HANDLE *write_hd,
                                 LPSECURITY_ATTRIBUTES sec_attr, DWORD size);
#define CreatePipe(a,b,c,d) _assuan_w32ce_create_pipe ((a),(b),(c),(d))

/* Magic handle values.  Let's hope those never occur legitimately as
   handles or sockets.  (Sockets are numbered sequentially from 0,
   while handles seem aligned to wordsize.  */
#define ASSUAN_STDIN (void*)0x7ffffffd
#define ASSUAN_STDOUT (void*)0x7fffffff
