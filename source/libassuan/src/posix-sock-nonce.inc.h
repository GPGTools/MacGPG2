## posix-sock-nonce.inc.h - Include fragment to build assuan.h.
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
/* Under Windows Assuan features an emulation of Unix domain sockets
   based on a local TCP connections.  To implement access permissions
   based on file permissions a nonce is used which is expected by the
   server as the first bytes received.  On POSIX systems this is a
   dummy structure. */  
struct assuan_sock_nonce_s
{
  size_t length;
};
typedef struct assuan_sock_nonce_s assuan_sock_nonce_t;
##EOF##
