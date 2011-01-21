/* assuan-buffer.c - read and send data
   Copyright (C) 2001-2004, 2006, 2009, 2010 Free Software Foundation, Inc.

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

#include <config.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>
#ifdef HAVE_W32_SYSTEM
#include <process.h>
#endif
#include "assuan-defs.h"


/* Extended version of write(2) to guarantee that all bytes are
   written.  Returns 0 on success or -1 and ERRNO on failure.  NOTE:
   This function does not return the number of bytes written, so any
   error must be treated as fatal for this connection as the state of
   the receiver is unknown.  This works best if blocking is allowed
   (so EAGAIN cannot occur).  */
static int
writen (assuan_context_t ctx, const char *buffer, size_t length)
{
  while (length)
    {
      ssize_t nwritten = ctx->engine.writefnc (ctx, buffer, length);
      
      if (nwritten < 0)
        {
          if (errno == EINTR)
            continue;
          return -1; /* write error */
        }
      length -= nwritten;
      buffer += nwritten;
    }
  return 0;  /* okay */
}

/* Read an entire line. Returns 0 on success or -1 and ERRNO on
   failure.  EOF is indictated by setting the integer at address
   R_EOF.  Note: BUF, R_NREAD and R_EOF contain a valid result even if
   an error is returned.  */
static int
readline (assuan_context_t ctx, char *buf, size_t buflen,
	  int *r_nread, int *r_eof)
{
  size_t nleft = buflen;
  char *p;

  *r_eof = 0;
  *r_nread = 0;
  while (nleft > 0)
    {
      ssize_t n = ctx->engine.readfnc (ctx, buf, nleft);

      if (n < 0)
        {
          if (errno == EINTR)
            continue;
          return -1; /* read error */
        }
      else if (!n)
        {
          *r_eof = 1;
          break; /* allow incomplete lines */
        }

      p = buf;
      nleft -= n;
      buf += n;
      *r_nread += n;

      p = memrchr (p, '\n', n);
      if (p)
        break; /* at least one full line available - that's enough for now */
    }
  return 0;
}


/* Read a line with buffering of partial lines.  Function returns an
   Assuan error.  */
gpg_error_t
_assuan_read_line (assuan_context_t ctx)
{
  gpg_error_t rc = 0;
  char *line = ctx->inbound.line;
  int nread, atticlen;
  char *endp = 0;

  if (ctx->inbound.eof)
    return _assuan_error (ctx, GPG_ERR_EOF);

  atticlen = ctx->inbound.attic.linelen;
  if (atticlen)
    {
      memcpy (line, ctx->inbound.attic.line, atticlen);
      ctx->inbound.attic.linelen = 0;

      endp = memchr (line, '\n', atticlen);
      if (endp)
	{
	  /* Found another line in the attic.  */
	  nread = atticlen;
	  atticlen = 0;
	}
      else
        {
	  /* There is pending data but not a full line.  */
          assert (atticlen < LINELENGTH);
          rc = readline (ctx, line + atticlen,
			 LINELENGTH - atticlen, &nread, &ctx->inbound.eof);
        }
    }
  else
    /* No pending data.  */
    rc = readline (ctx, line, LINELENGTH,
                   &nread, &ctx->inbound.eof);
  if (rc)
    {
      int saved_errno = errno;
      char buf[100];

      snprintf (buf, sizeof buf, "error: %s", strerror (errno));
      _assuan_log_control_channel (ctx, 0, buf, NULL, 0, NULL, 0);

      if (saved_errno == EAGAIN)
        {
          /* We have to save a partial line.  Due to readline's
	     behaviour, we know that this is not a complete line yet
	     (no newline).  So we don't set PENDING to true.  */
          memcpy (ctx->inbound.attic.line, line, atticlen + nread);
          ctx->inbound.attic.pending = 0;
          ctx->inbound.attic.linelen = atticlen + nread;
        }

      gpg_err_set_errno (saved_errno);
      return _assuan_error (ctx, gpg_err_code_from_syserror ());
    }
  if (!nread)
    {
      assert (ctx->inbound.eof);
      _assuan_log_control_channel (ctx, 0, "eof", NULL, 0, NULL, 0);
      return _assuan_error (ctx, GPG_ERR_EOF);
    }

  ctx->inbound.attic.pending = 0;
  nread += atticlen;

  if (! endp)
    endp = memchr (line, '\n', nread);

  if (endp)
    {
      unsigned monitor_result;
      int n = endp - line + 1;

      if (n < nread)
	/* LINE contains more than one line.  We copy it to the attic
	   now as handlers are allowed to modify the passed
	   buffer.  */
	{
	  int len = nread - n;
	  memcpy (ctx->inbound.attic.line, endp + 1, len);
	  ctx->inbound.attic.pending = memrchr (endp + 1, '\n', len) ? 1 : 0;
	  ctx->inbound.attic.linelen = len;
	}

      if (endp != line && endp[-1] == '\r')
	endp --;
      *endp = 0;

      ctx->inbound.linelen = endp - line;

      monitor_result = 0;
      if (ctx->io_monitor)
	monitor_result = ctx->io_monitor (ctx, ctx->io_monitor_data, 0,
					  ctx->inbound.line,
					  ctx->inbound.linelen);
      if (monitor_result & ASSUAN_IO_MONITOR_IGNORE)
        ctx->inbound.linelen = 0;
      
      if ( !(monitor_result & ASSUAN_IO_MONITOR_NOLOG))
        _assuan_log_control_channel (ctx, 0, NULL,
                                     ctx->inbound.line, ctx->inbound.linelen,
                                     NULL, 0);
      return 0;
    }
  else
    {
      _assuan_log_control_channel (ctx, 0, "invalid line", 
                                   NULL, 0, NULL, 0);
      *line = 0;
      ctx->inbound.linelen = 0;
      return _assuan_error (ctx, ctx->inbound.eof 
			    ? GPG_ERR_ASS_INCOMPLETE_LINE
			    : GPG_ERR_ASS_LINE_TOO_LONG);
    }
}


/* Read the next line from the client or server and return a pointer
   in *LINE to a buffer holding the line.  LINELEN is the length of
   *LINE.  The buffer is valid until the next read operation on it.
   The caller may modify the buffer.  The buffer is invalid (i.e. must
   not be used) if an error is returned.

   Returns 0 on success or an assuan error code.
   See also: assuan_pending_line().
*/
gpg_error_t
assuan_read_line (assuan_context_t ctx, char **line, size_t *linelen)
{
  gpg_error_t err;

  if (!ctx)
    return _assuan_error (ctx, GPG_ERR_ASS_INV_VALUE);

  do
    {
      err = _assuan_read_line (ctx);
    }
  while (_assuan_error_is_eagain (ctx, err));

  *line = ctx->inbound.line;
  *linelen = ctx->inbound.linelen;

  return err;
}


/* Return true if a full line is buffered (i.e. an entire line may be
   read without any I/O).  */
int
assuan_pending_line (assuan_context_t ctx)
{
  return ctx && ctx->inbound.attic.pending;
}


gpg_error_t 
_assuan_write_line (assuan_context_t ctx, const char *prefix,
                    const char *line, size_t len)
{
  gpg_error_t rc = 0;
  size_t prefixlen = prefix? strlen (prefix):0;
  unsigned int monitor_result;

  /* Make sure that the line is short enough. */
  if (len + prefixlen + 2 > ASSUAN_LINELENGTH)
    {
      _assuan_log_control_channel (ctx, 1, 
                                   "supplied line too long - truncated",
                                   NULL, 0, NULL, 0);
      if (prefixlen > 5)
        prefixlen = 5;
      if (len > ASSUAN_LINELENGTH - prefixlen - 2)
        len = ASSUAN_LINELENGTH - prefixlen - 2 - 1;
    }

  monitor_result = 0;
  if (ctx->io_monitor)
    monitor_result = ctx->io_monitor (ctx, ctx->io_monitor_data, 1, line, len);

  /* Fixme: we should do some kind of line buffering.  */
  if (!(monitor_result & ASSUAN_IO_MONITOR_NOLOG))
    _assuan_log_control_channel (ctx, 1, NULL,
                                 prefixlen? prefix:NULL, prefixlen,
                                 line, len);

  if (prefixlen && !(monitor_result & ASSUAN_IO_MONITOR_IGNORE))
    {
      rc = writen (ctx, prefix, prefixlen);
      if (rc)
	rc = _assuan_error (ctx, gpg_err_code_from_syserror ());
    }
  if (!rc && !(monitor_result & ASSUAN_IO_MONITOR_IGNORE))
    {
      rc = writen (ctx, line, len);
      if (rc)
	rc = _assuan_error (ctx, gpg_err_code_from_syserror ());
      if (!rc)
        {
          rc = writen (ctx, "\n", 1);
          if (rc)
	    rc = _assuan_error (ctx, gpg_err_code_from_syserror ());
        }
    }
  return rc;
}


gpg_error_t 
assuan_write_line (assuan_context_t ctx, const char *line)
{
  size_t len;
  const char *str;

  if (! ctx)
    return _assuan_error (ctx, GPG_ERR_ASS_INV_VALUE);

  /* Make sure that we never take a LF from the user - this might
     violate the protocol. */
  str = strchr (line, '\n');
  len = str ? (str - line) : strlen (line);

  if (str)
    _assuan_log_control_channel (ctx, 1,
                                 "supplied line with LF - truncated",
                                 NULL, 0, NULL, 0);

  return _assuan_write_line (ctx, NULL, line, len);
}



/* Write out the data in buffer as datalines with line wrapping and
   percent escaping.  This function is used for GNU's custom streams. */
int
_assuan_cookie_write_data (void *cookie, const char *buffer, size_t orig_size)
{
  assuan_context_t ctx = cookie;
  size_t size = orig_size;
  char *line;
  size_t linelen;

  if (ctx->outbound.data.error)
    return 0;

  line = ctx->outbound.data.line;
  linelen = ctx->outbound.data.linelen;
  line += linelen;
  while (size)
    {
      unsigned int monitor_result;

      /* Insert data line header. */
      if (!linelen)
        {
          *line++ = 'D';
          *line++ = ' ';
          linelen += 2;
        }
      
      /* Copy data, keep space for the CRLF and to escape one character. */
      while (size && linelen < LINELENGTH-2-2)
        {
          if (*buffer == '%' || *buffer == '\r' || *buffer == '\n')
            {
              sprintf (line, "%%%02X", *(unsigned char*)buffer);
              line += 3;
              linelen += 3;
              buffer++;
            }
          else
            {
              *line++ = *buffer++;
              linelen++;
            }
          size--;
        }
      
      
      monitor_result = 0;
      if (ctx->io_monitor)
	monitor_result = ctx->io_monitor (ctx, ctx->io_monitor_data, 1,
					  ctx->outbound.data.line, linelen);

      if (linelen >= LINELENGTH-2-2)
        {
          if (!(monitor_result & ASSUAN_IO_MONITOR_NOLOG))
            _assuan_log_control_channel (ctx, 1, NULL,
                                         ctx->outbound.data.line, linelen,
                                         NULL, 0);

          *line++ = '\n';
          linelen++;
          if ( !(monitor_result & ASSUAN_IO_MONITOR_IGNORE)
               && writen (ctx, ctx->outbound.data.line, linelen))
            {
              ctx->outbound.data.error = gpg_err_code_from_syserror ();
              return 0;
            }
          line = ctx->outbound.data.line;
          linelen = 0;
        }
    }

  ctx->outbound.data.linelen = linelen;
  return (int) orig_size;
}


/* Write out any buffered data 
   This function is used for GNU's custom streams */
int
_assuan_cookie_write_flush (void *cookie)
{
  assuan_context_t ctx = cookie;
  char *line;
  size_t linelen;
  unsigned int monitor_result;

  if (ctx->outbound.data.error)
    return 0;

  line = ctx->outbound.data.line;
  linelen = ctx->outbound.data.linelen;
  line += linelen;

  monitor_result = 0;
  if (ctx->io_monitor)
    monitor_result = ctx->io_monitor (ctx, ctx->io_monitor_data, 1,
				      ctx->outbound.data.line, linelen);
  
  if (linelen)
    {
      if (!(monitor_result & ASSUAN_IO_MONITOR_NOLOG))
        _assuan_log_control_channel (ctx, 1, NULL,
                                     ctx->outbound.data.line, linelen,
                                     NULL, 0);
      *line++ = '\n';
      linelen++;
      if (! (monitor_result & ASSUAN_IO_MONITOR_IGNORE)
           && writen (ctx, ctx->outbound.data.line, linelen))
        {
          ctx->outbound.data.error = gpg_err_code_from_syserror ();
          return 0;
        }
      ctx->outbound.data.linelen = 0;
    }
  return 0;
}


/**
 * assuan_send_data:
 * @ctx: An assuan context
 * @buffer: Data to send or NULL to flush
 * @length: length of the data to send/
 * 
 * This function may be used by the server or the client to send data
 * lines.  The data will be escaped as required by the Assuan protocol
 * and may get buffered until a line is full.  To force sending the
 * data out @buffer may be passed as NULL (in which case @length must
 * also be 0); however when used by a client this flush operation does
 * also send the terminating "END" command to terminate the reponse on
 * a INQUIRE response.  However, when assuan_transact() is used, this
 * function takes care of sending END itself.
 * 
 * If BUFFER is NULL and LENGTH is 1 and we are a client, a "CAN" is
 * send instead of an "END".
 * 
 * Return value: 0 on success or an error code
 **/

gpg_error_t
assuan_send_data (assuan_context_t ctx, const void *buffer, size_t length)
{
  if (!ctx)
    return _assuan_error (ctx, GPG_ERR_ASS_INV_VALUE);
  if (!buffer && length > 1)
    return _assuan_error (ctx, GPG_ERR_ASS_INV_VALUE);

  if (!buffer)
    { /* flush what we have */
      _assuan_cookie_write_flush (ctx);
      if (ctx->outbound.data.error)
        return ctx->outbound.data.error;
      if (!ctx->is_server)
        return assuan_write_line (ctx, length == 1? "CAN":"END");
    }
  else
    {
      _assuan_cookie_write_data (ctx, buffer, length);
      if (ctx->outbound.data.error)
        return ctx->outbound.data.error;
    }

  return 0;
}

gpg_error_t
assuan_sendfd (assuan_context_t ctx, assuan_fd_t fd)
{
  /* It is explicitly allowed to use (NULL, -1) as a runtime test to
     check whether descriptor passing is available. */
  if (!ctx && fd == ASSUAN_INVALID_FD)
#ifdef USE_DESCRIPTOR_PASSING
    return 0;
#else
  return _assuan_error (ctx, GPG_ERR_NOT_IMPLEMENTED);
#endif

  if (! ctx->engine.sendfd)
    return set_error (ctx, GPG_ERR_NOT_IMPLEMENTED,
		      "server does not support sending and receiving "
		      "of file descriptors");
  return ctx->engine.sendfd (ctx, fd);
}

gpg_error_t
assuan_receivefd (assuan_context_t ctx, assuan_fd_t *fd)
{
  if (! ctx->engine.receivefd)
    return set_error (ctx, GPG_ERR_NOT_IMPLEMENTED,
		      "server does not support sending and receiving "
		      "of file descriptors");
  return ctx->engine.receivefd (ctx, fd);
}
