## w32-sock-nonce.inc.h - Include fragment to build assuan.h.
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

/* Assuan features an emulation of Unix domain sockets based on local
   TCP connections.  To implement access permissions based on file
   permissions a nonce is used which is expected by the server as the
   first bytes received.  This structure is used by the server to save
   the nonce created initially by bind.  */
struct assuan_sock_nonce_s
{
  size_t length;
  char nonce[16];
};
typedef struct assuan_sock_nonce_s assuan_sock_nonce_t;

/* Define the Unix domain socket structure for Windows.  */
#ifndef _ASSUAN_NO_SOCKET_WRAPPER
# ifndef AF_LOCAL
#  define AF_LOCAL AF_UNIX
# endif
# define EADDRINUSE WSAEADDRINUSE
struct sockaddr_un
{
  short          sun_family;
  unsigned short sun_port;
  struct         in_addr sun_addr;
  char           sun_path[108-2-4]; 
};
#endif

##EOF##
