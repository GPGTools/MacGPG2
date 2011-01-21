/* assuan-socket-server.c - Assuan socket based server
   Copyright (C) 2002, 2007, 2009 Free Software Foundation, Inc.

   This file is part of Assuan.

   Assuan is free software; you can redistribute it and/or modify it
   under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 2.1 of
   the License, or (at your option) any later version.

   Assuan is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#ifdef HAVE_W32_SYSTEM
# include <windows.h>
# if HAVE_SYS_SOCKET_H
#  include <sys/socket.h>
# elif HAVE_WS2TCPIP_H
#  include <ws2tcpip.h>
# endif
#else
# include <sys/socket.h>
# include <sys/un.h>
#endif

#include "debug.h"
#include "assuan-defs.h"

static gpg_error_t
accept_connection_bottom (assuan_context_t ctx)
{
  assuan_fd_t fd = ctx->connected_fd;

  TRACE (ctx, ASSUAN_LOG_SYSIO, "accept_connection_bottom", ctx);

  ctx->peercred_valid = 0;
#ifdef HAVE_SO_PEERCRED
  {
    struct ucred cr; 
    socklen_t cl = sizeof cr;

    if ( !getsockopt (fd, SOL_SOCKET, SO_PEERCRED, &cr, &cl))
      {
         ctx->peercred.pid = cr.pid;
         ctx->peercred.uid = cr.uid;
         ctx->peercred.gid = cr.gid;
         ctx->peercred_valid = 1;

         /* This overrides any already set PID if the function returns
            a valid one. */
         if (cr.pid != ASSUAN_INVALID_PID && cr.pid) 
           ctx->pid = cr.pid;
      }
  }
#endif

  ctx->inbound.fd = fd;
  ctx->inbound.eof = 0;
  ctx->inbound.linelen = 0;
  ctx->inbound.attic.linelen = 0;
  ctx->inbound.attic.pending = 0;

  ctx->outbound.fd = fd;
  ctx->outbound.data.linelen = 0;
  ctx->outbound.data.error = 0;
  
  ctx->flags.confidential = 0;

  return 0;
}


static gpg_error_t
accept_connection (assuan_context_t ctx)
{
  assuan_fd_t fd;
  struct sockaddr_un clnt_addr;
  socklen_t len = sizeof clnt_addr;

  TRACE1 (ctx, ASSUAN_LOG_SYSIO, "accept_connection", ctx, 
         "listen_fd=0x%x", ctx->listen_fd);

  fd = SOCKET2HANDLE(accept (HANDLE2SOCKET(ctx->listen_fd), 
                             (struct sockaddr*)&clnt_addr, &len ));
  if (fd == ASSUAN_INVALID_FD)
    {
      return _assuan_error (ctx, gpg_err_code_from_syserror ());
    }
  TRACE1 (ctx, ASSUAN_LOG_SYSIO, "accept_connection", ctx, 
          "fd->0x%x", fd);
  if (_assuan_sock_check_nonce (ctx, fd, &ctx->listen_nonce))
    {
      _assuan_close (ctx, fd);
      return _assuan_error (ctx, GPG_ERR_ASS_ACCEPT_FAILED);
    }

  ctx->connected_fd = fd;
  return accept_connection_bottom (ctx);
}


/* 
   Flag bits: 0 - use sendmsg/recvmsg to allow descriptor passing
              1 - FD has already been accepted.
*/
gpg_error_t
assuan_init_socket_server (assuan_context_t ctx, assuan_fd_t fd,
			   unsigned int flags)
{
  gpg_error_t rc;
  TRACE_BEG2 (ctx, ASSUAN_LOG_CTX, "assuan_init_socket_server", ctx,
	      "fd=0x%x, flags=0x%x", fd, flags);
  
  rc = _assuan_register_std_commands (ctx);
  if (rc)
    return TRACE_ERR (rc);

  ctx->engine.release = _assuan_server_release;
  ctx->engine.readfnc = _assuan_simple_read;
  ctx->engine.writefnc = _assuan_simple_write;
  ctx->engine.sendfd = NULL;
  ctx->engine.receivefd = NULL;
  ctx->is_server = 1;
  if (flags & ASSUAN_SOCKET_SERVER_ACCEPTED)
    /* We want a second accept to indicate EOF. */
    ctx->max_accepts = 1;
  else
    ctx->max_accepts = -1;
  ctx->input_fd = ASSUAN_INVALID_FD;
  ctx->output_fd = ASSUAN_INVALID_FD;

  ctx->inbound.fd = ASSUAN_INVALID_FD;
  ctx->outbound.fd = ASSUAN_INVALID_FD;

  if (flags & ASSUAN_SOCKET_SERVER_ACCEPTED)
    {
      ctx->listen_fd = ASSUAN_INVALID_FD;
      ctx->connected_fd = fd;
    }
  else
    {
      ctx->listen_fd = fd;
      ctx->connected_fd = ASSUAN_INVALID_FD;
    }
  ctx->accept_handler = ((flags & ASSUAN_SOCKET_SERVER_ACCEPTED)
                         ? accept_connection_bottom 
                         : accept_connection);
  ctx->finish_handler = _assuan_server_finish;

  if (flags & ASSUAN_SOCKET_SERVER_FDPASSING)
    _assuan_init_uds_io (ctx);

  rc = _assuan_register_std_commands (ctx);
  if (rc)
    _assuan_reset (ctx);
  return TRACE_ERR (rc);
}


/* Save a copy of NONCE in context CTX.  This should be used to
   register the server's nonce with an context established by
   assuan_init_socket_server.  */
void
assuan_set_sock_nonce (assuan_context_t ctx, assuan_sock_nonce_t *nonce)
{
  if (ctx && nonce)
    ctx->listen_nonce = *nonce;
}
