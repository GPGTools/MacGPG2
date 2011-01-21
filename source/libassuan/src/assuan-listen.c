/* assuan-listen.c - Wait for a connection (server) 
   Copyright (C) 2001, 2002, 2004, 2009 Free Software Foundation, Inc.

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
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "assuan-defs.h"

gpg_error_t
assuan_set_hello_line (assuan_context_t ctx, const char *line)
{
  if (!ctx)
    return _assuan_error (ctx, GPG_ERR_ASS_INV_VALUE);
  if (!line)
    {
      _assuan_free (ctx, ctx->hello_line);
      ctx->hello_line = NULL;
    }
  else
    {
      char *buf = _assuan_malloc (ctx, 3 + strlen (line) + 1);
      if (!buf)
	return _assuan_error (ctx, gpg_err_code_from_syserror ());
      if (strchr (line, '\n'))
        strcpy (buf, line);
      else
        {
          strcpy (buf, "OK ");
          strcpy (buf+3, line);
        }
      _assuan_free (ctx, ctx->hello_line);
      ctx->hello_line = buf;
    }
  return 0;
}


/**
 * assuan_accept:
 * @ctx: context
 * 
 * Cancel any existing connection and wait for a connection from a
 * client.  The initial handshake is performed which may include an
 * initial authentication or encryption negotiation.
 * 
 * Return value: 0 on success or an error if the connection could for
 * some reason not be established.
 **/
gpg_error_t
assuan_accept (assuan_context_t ctx)
{
  gpg_error_t rc = 0;
  const char *p, *pend;

  if (!ctx)
    return _assuan_error (ctx, GPG_ERR_ASS_INV_VALUE);

  if (ctx->max_accepts != -1)
    {
      if (ctx->max_accepts-- == 0)
	return -1; /* second invocation for pipemode -> terminate */
    }
  if (ctx->accept_handler)
    {
      /* FIXME: This should be superfluous, if everything else is
	 correct.  */
      ctx->finish_handler (ctx);
      rc = ctx->accept_handler (ctx);
      if (rc)
	return rc;
    }

  /* Send the hello. */
  p = ctx->hello_line;
  if (p && (pend = strchr (p, '\n')))
    { /* This is a multi line hello.  Send all but the last line as
         comments. */
      do
        {
          rc = _assuan_write_line (ctx, "# ", p, pend - p);
          if (rc)
            return rc;
          p = pend + 1;
          pend = strchr (p, '\n');
        }
      while (pend);
      rc = _assuan_write_line (ctx, "OK ", p, strlen (p));
    }
  else if (p)
    rc = assuan_write_line (ctx, p);
  else
    {
      static char const okstr[] = "OK Pleased to meet you";
      pid_t apid = assuan_get_pid (ctx);
      if (apid != ASSUAN_INVALID_PID)
        {
          char tmpbuf[50];
          snprintf (tmpbuf, sizeof tmpbuf, "%s, process %i", okstr, (int)apid);
          rc = assuan_write_line (ctx, tmpbuf);
        }
      else
        rc = assuan_write_line (ctx, okstr);
    }
  if (rc)
    return rc;
    
  return 0;
}



assuan_fd_t
assuan_get_input_fd (assuan_context_t ctx)
{
  return ctx ? ctx->input_fd : ASSUAN_INVALID_FD;
}


assuan_fd_t
assuan_get_output_fd (assuan_context_t ctx)
{
  return ctx ? ctx->output_fd : ASSUAN_INVALID_FD;
}


/* Close the fd descriptor set by the command INPUT FD=n.  We handle
   this fd inside assuan so that we can do some initial checks */
gpg_error_t
assuan_close_input_fd (assuan_context_t ctx)
{
  if (!ctx || ctx->input_fd == ASSUAN_INVALID_FD)
    return _assuan_error (ctx, GPG_ERR_ASS_INV_VALUE);
  _assuan_close (ctx, ctx->input_fd);
  ctx->input_fd = ASSUAN_INVALID_FD;
  return 0;
}

/* Close the fd descriptor set by the command OUTPUT FD=n.  We handle
   this fd inside assuan so that we can do some initial checks */
gpg_error_t
assuan_close_output_fd (assuan_context_t ctx)
{
  if (!ctx || ctx->output_fd == ASSUAN_INVALID_FD)
    return _assuan_error (ctx, GPG_ERR_ASS_INV_VALUE);

  _assuan_close (ctx, ctx->output_fd);
  ctx->output_fd = ASSUAN_INVALID_FD;
  return 0;
}

