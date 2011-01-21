/* assuan-uds.c - Assuan unix domain socket utilities
   Copyright (C) 2006, 2009 Free Software Foundation, Inc.

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
#include <stddef.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#ifndef HAVE_W32_SYSTEM
#include <sys/socket.h>
#include <sys/un.h>
#else
#include <windows.h>
#endif
#if HAVE_SYS_UIO_H
#include <sys/uio.h>
#endif
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>

#include "assuan-defs.h"
#include "debug.h"

#ifdef USE_DESCRIPTOR_PASSING
/* Provide replacement for missing CMSG maccros.  We assume that
   size_t matches the alignment requirement.  NOTE: This is not true
   on Mac OS X, so be extra careful to define _DARWIN_C_SOURCE to get
   those definitions instead of using these.  */
#define MY_ALIGN(n) ((((n))+ sizeof(size_t)-1) & (size_t)~(sizeof(size_t)-1))
#ifndef CMSG_SPACE
#define CMSG_SPACE(n) (MY_ALIGN(sizeof(struct cmsghdr)) + MY_ALIGN((n)))
#endif 
#ifndef CMSG_LEN
#define CMSG_LEN(n) (MY_ALIGN(sizeof(struct cmsghdr)) + (n))
#endif 
#ifndef CMSG_FIRSTHDR
#define CMSG_FIRSTHDR(mhdr) \
  ((size_t)(mhdr)->msg_controllen >= sizeof (struct cmsghdr)		      \
   ? (struct cmsghdr*) (mhdr)->msg_control : (struct cmsghdr*)NULL)
#endif
#ifndef CMSG_DATA
#define CMSG_DATA(cmsg) ((unsigned char*)((struct cmsghdr*)(cmsg)+1))
#endif
#endif /*USE_DESCRIPTOR_PASSING*/


/* Read from a unix domain socket using sendmsg.  */
static ssize_t
uds_reader (assuan_context_t ctx, void *buf, size_t buflen)
{
#ifndef HAVE_W32_SYSTEM
  int len = 0;
  /* This loop should be OK.  As FDs are followed by data, the
     readable status of the socket does not change and no new
     select/event-loop round is necessary.  */
  while (!len)  /* No data is buffered.  */
    {
      struct msghdr msg;
      struct iovec iovec;
#ifdef USE_DESCRIPTOR_PASSING
      union {
        struct cmsghdr cm;
        char control[CMSG_SPACE(sizeof (int))];
      } control_u;
      struct cmsghdr *cmptr;
#endif /*USE_DESCRIPTOR_PASSING*/

      memset (&msg, 0, sizeof (msg));

      msg.msg_name = NULL;
      msg.msg_namelen = 0;
      msg.msg_iov = &iovec;
      msg.msg_iovlen = 1;
      iovec.iov_base = buf;
      iovec.iov_len = buflen;
#ifdef USE_DESCRIPTOR_PASSING
      msg.msg_control = control_u.control;
      msg.msg_controllen = sizeof (control_u.control);
#endif

      len = _assuan_recvmsg (ctx, ctx->inbound.fd, &msg, 0);
      if (len < 0)
        return -1;
      if (len == 0)
	return 0;

#ifdef USE_DESCRIPTOR_PASSING
      cmptr = CMSG_FIRSTHDR (&msg);
      if (cmptr && cmptr->cmsg_len == CMSG_LEN (sizeof(int)))
        {
          if (cmptr->cmsg_level != SOL_SOCKET
              || cmptr->cmsg_type != SCM_RIGHTS)
            TRACE0 (ctx, ASSUAN_LOG_SYSIO, "uds_reader", ctx,
		    "unexpected ancillary data received");
          else
            {
              int fd;

	      memcpy (&fd, CMSG_DATA (cmptr), sizeof (fd));

              if (ctx->uds.pendingfdscount >= DIM (ctx->uds.pendingfds))
                {
		  TRACE1 (ctx, ASSUAN_LOG_SYSIO, "uds_reader", ctx,
			  "too many descriptors pending - "
			  "closing received descriptor %d", fd);
                  _assuan_close (ctx, fd);
                }
              else
                ctx->uds.pendingfds[ctx->uds.pendingfdscount++] = fd;
            }
	}
#endif /*USE_DESCRIPTOR_PASSING*/
    }

  return len;
#else /*HAVE_W32_SYSTEM*/
  int res = recvfrom (HANDLE2SOCKET(ctx->inbound.fd), buf, buflen, 0, NULL, NULL);
  if (res < 0)
    gpg_err_set_errno (_assuan_sock_wsa2errno (WSAGetLastError ()));
  return res;
#endif /*HAVE_W32_SYSTEM*/
}


/* Write to the domain server.  */
static ssize_t
uds_writer (assuan_context_t ctx, const void *buf, size_t buflen)
{
#ifndef HAVE_W32_SYSTEM
  struct msghdr msg;
  struct iovec iovec;
  ssize_t len;

  memset (&msg, 0, sizeof (msg));

  msg.msg_name = NULL;
  msg.msg_namelen = 0;
  msg.msg_iovlen = 1;
  msg.msg_iov = &iovec;
  iovec.iov_base = (void*)buf;
  iovec.iov_len = buflen;

  len = _assuan_sendmsg (ctx, ctx->outbound.fd, &msg, 0);

  return len;
#else /*HAVE_W32_SYSTEM*/
  int res = sendto (HANDLE2SOCKET(ctx->outbound.fd), buf, buflen, 0,
		    (struct sockaddr *)&ctx->serveraddr,
		    sizeof (struct sockaddr_in));
  if (res < 0)
    gpg_err_set_errno ( _assuan_sock_wsa2errno (WSAGetLastError ()));
  return res;
#endif /*HAVE_W32_SYSTEM*/
}


static gpg_error_t
uds_sendfd (assuan_context_t ctx, assuan_fd_t fd)
{
#ifdef USE_DESCRIPTOR_PASSING
  struct msghdr msg;
  struct iovec iovec;
  union {
    struct cmsghdr cm;
    char control[CMSG_SPACE(sizeof (int))];
  } control_u;
  struct cmsghdr *cmptr;
  int len;
  char buffer[80];

  /* We need to send some real data so that a read won't return 0
     which will be taken as an EOF.  It also helps with debugging. */ 
  snprintf (buffer, sizeof(buffer)-1, "# descriptor %d is in flight\n", fd);
  buffer[sizeof(buffer)-1] = 0;

  memset (&msg, 0, sizeof (msg));

  msg.msg_name = NULL;
  msg.msg_namelen = 0;
  msg.msg_iovlen = 1;
  msg.msg_iov = &iovec;
  iovec.iov_base = buffer;
  iovec.iov_len = strlen (buffer);

  msg.msg_control = control_u.control;
  msg.msg_controllen = sizeof (control_u.control);
  cmptr = CMSG_FIRSTHDR (&msg);
  cmptr->cmsg_len = CMSG_LEN(sizeof(int));
  cmptr->cmsg_level = SOL_SOCKET;
  cmptr->cmsg_type = SCM_RIGHTS;

  memcpy (CMSG_DATA (cmptr), &fd, sizeof (fd));

  len = _assuan_sendmsg (ctx, ctx->outbound.fd, &msg, 0);
  if (len < 0)
    {
      int saved_errno = errno;
      TRACE1 (ctx, ASSUAN_LOG_SYSIO, "uds_sendfd", ctx,
	      "uds_sendfd: %s", strerror (errno));
      errno = saved_errno;
      return _assuan_error (ctx, gpg_err_code_from_syserror ());
    }
  else
    return 0;
#else
  return _assuan_error (ctx, GPG_ERR_NOT_IMPLEMENTED);
#endif
}


static gpg_error_t
uds_receivefd (assuan_context_t ctx, assuan_fd_t *fd)
{
#ifdef USE_DESCRIPTOR_PASSING
  int i;

  if (!ctx->uds.pendingfdscount)
    {
      TRACE0 (ctx, ASSUAN_LOG_SYSIO, "uds_receivefd", ctx,
	      "no pending file descriptors");
      return _assuan_error (ctx, GPG_ERR_ASS_GENERAL);
    }
  assert (ctx->uds.pendingfdscount <= DIM(ctx->uds.pendingfds));

  *fd = ctx->uds.pendingfds[0];
  for (i=1; i < ctx->uds.pendingfdscount; i++)
    ctx->uds.pendingfds[i-1] = ctx->uds.pendingfds[i];
  ctx->uds.pendingfdscount--;

  return 0;
#else
  return _assuan_error (ctx, GPG_ERR_NOT_IMPLEMENTED);
#endif
}


/* Close all pending fds. */
void
_assuan_uds_close_fds (assuan_context_t ctx)
{
  int i;

  for (i = 0; i < ctx->uds.pendingfdscount; i++)
    _assuan_close (ctx, ctx->uds.pendingfds[i]);
  ctx->uds.pendingfdscount = 0;
}

/* Deinitialize the unix domain socket I/O functions.  */
void
_assuan_uds_deinit (assuan_context_t ctx)
{
  _assuan_uds_close_fds (ctx);
}


/* Helper function to initialize a context for domain I/O.  */
void
_assuan_init_uds_io (assuan_context_t ctx)
{
  ctx->engine.readfnc = uds_reader;
  ctx->engine.writefnc = uds_writer;
  ctx->engine.sendfd = uds_sendfd;
  ctx->engine.receivefd = uds_receivefd;

  ctx->uds.pendingfdscount = 0;
}

