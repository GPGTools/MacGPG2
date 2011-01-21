/* client.c - Functions common to all clients.
   Copyright (C) 2009 Free Software Foundation, Inc.

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

#include "assuan-defs.h"
#include "debug.h"

#define xtoi_1(p)   (*(p) <= '9'? (*(p)- '0'): \
                     *(p) <= 'F'? (*(p)-'A'+10):(*(p)-'a'+10))
#define xtoi_2(p)   ((xtoi_1(p) * 16) + xtoi_1((p)+1))


void
_assuan_client_finish (assuan_context_t ctx)
{
  if (ctx->inbound.fd != ASSUAN_INVALID_FD)
    {
      _assuan_close (ctx, ctx->inbound.fd);
      if (ctx->inbound.fd == ctx->outbound.fd)
        ctx->outbound.fd = ASSUAN_INVALID_FD;
      ctx->inbound.fd = ASSUAN_INVALID_FD;
    }
  if (ctx->outbound.fd != ASSUAN_INVALID_FD)
    {
      _assuan_close (ctx, ctx->outbound.fd);
      ctx->outbound.fd = ASSUAN_INVALID_FD;
    }
  if (ctx->pid != ASSUAN_INVALID_PID && ctx->pid)
    {
      _assuan_waitpid (ctx, ctx->pid, ctx->flags.no_waitpid, NULL, 0);
      ctx->pid = ASSUAN_INVALID_PID;
    }

  _assuan_uds_deinit (ctx);
}


/* Disconnect and release the context CTX.  */
void
_assuan_client_release (assuan_context_t ctx)
{
  assuan_write_line (ctx, "BYE");

  _assuan_client_finish (ctx);
}


/* This function also does deescaping for data lines.  */
gpg_error_t
assuan_client_read_response (assuan_context_t ctx,
			     char **line_r, int *linelen_r)
{
  gpg_error_t rc;
  char *line = NULL;
  int linelen = 0;

  *line_r = NULL;
  *linelen_r = 0;

  do 
    {
      do
	{
	  rc = _assuan_read_line (ctx);
	}
      while (_assuan_error_is_eagain (ctx, rc));
      if (rc)
        return rc;
      line = ctx->inbound.line;
      linelen = ctx->inbound.linelen;
    }    
  while (!linelen);

  /* For data lines, we deescape immediately.  The user will never
     have to worry about it.  */
  if (linelen >= 1 && line[0] == 'D' && line[1] == ' ')
    {
      char *s, *d;
      for (s=d=line; linelen; linelen--)
	{
	  if (*s == '%' && linelen > 2)
	    { /* handle escaping */
	      s++;
	      *d++ = xtoi_2 (s);
	      s += 2;
	      linelen -= 2;
	    }
	  else
	    *d++ = *s++;
	}
      *d = 0; /* add a hidden string terminator */

      linelen = d - line;
      ctx->inbound.linelen = linelen;
    }

  *line_r = line;
  *linelen_r = linelen;

  return 0;
}


gpg_error_t
assuan_client_parse_response (assuan_context_t ctx, char *line, int linelen,
			      assuan_response_t *response, int *off)
{
  *response = ASSUAN_RESPONSE_ERROR;
  *off = 0;

  if (linelen >= 1
      && line[0] == 'D' && line[1] == ' ')
    {
      *response = ASSUAN_RESPONSE_DATA; /* data line */
      *off = 2;
    }
  else if (linelen >= 1
           && line[0] == 'S' 
           && (line[1] == '\0' || line[1] == ' '))
    {
      *response = ASSUAN_RESPONSE_STATUS;
      *off = 1;
      while (line[*off] == ' ')
        ++*off;
    }  
  else if (linelen >= 2
           && line[0] == 'O' && line[1] == 'K'
           && (line[2] == '\0' || line[2] == ' '))
    {
      *response = ASSUAN_RESPONSE_OK;
      *off = 2;
      while (line[*off] == ' ')
        ++*off;
    }
  else if (linelen >= 3
           && line[0] == 'E' && line[1] == 'R' && line[2] == 'R'
           && (line[3] == '\0' || line[3] == ' '))
    {
      *response = ASSUAN_RESPONSE_ERROR;
      *off = 3;
      while (line[*off] == ' ')
        ++*off;
    }  
  else if (linelen >= 7
           && line[0] == 'I' && line[1] == 'N' && line[2] == 'Q'
           && line[3] == 'U' && line[4] == 'I' && line[5] == 'R'
           && line[6] == 'E' 
           && (line[7] == '\0' || line[7] == ' '))
    {
      *response = ASSUAN_RESPONSE_INQUIRE;
      *off = 7;
      while (line[*off] == ' ')
        ++*off;
    }
  else if (linelen >= 3
           && line[0] == 'E' && line[1] == 'N' && line[2] == 'D'
           && (line[3] == '\0' || line[3] == ' '))
    {
      *response = ASSUAN_RESPONSE_END;
      *off = 3;
    }
  else if (linelen >= 1 && line[0] == '#')
    {
      *response = ASSUAN_RESPONSE_COMMENT;
      *off = 1;
    }
  else
    return _assuan_error (ctx, GPG_ERR_ASS_INV_RESPONSE);

  return 0;
}


gpg_error_t
_assuan_read_from_server (assuan_context_t ctx, assuan_response_t *response,
			  int *off)
{
  gpg_error_t rc;
  char *line;
  int linelen;

  do
    {
      *response = ASSUAN_RESPONSE_ERROR;
      *off = 0;
      rc = assuan_client_read_response (ctx, &line, &linelen);
      if (!rc)
        rc = assuan_client_parse_response (ctx, line, linelen, response, off);
    }
  while (!rc && *response == ASSUAN_RESPONSE_COMMENT);

  return rc;
}


/**
 * assuan_transact:
 * @ctx: The Assuan context
 * @command: Command line to be send to the server
 * @data_cb: Callback function for data lines
 * @data_cb_arg: first argument passed to @data_cb
 * @inquire_cb: Callback function for a inquire response
 * @inquire_cb_arg: first argument passed to @inquire_cb
 * @status_cb: Callback function for a status response
 * @status_cb_arg: first argument passed to @status_cb
 * 
 * FIXME: Write documentation
 * 
 * Return value: 0 on success or error code.  The error code may be
 * the one one returned by the server in error lines or from the
 * callback functions.  Take care: When a callback returns an error
 * this function returns immediately with an error and thus the caller
 * will altter return an Assuan error (write erro in most cases).
 **/
gpg_error_t
assuan_transact (assuan_context_t ctx,
                 const char *command,
                 gpg_error_t (*data_cb)(void *, const void *, size_t),
                 void *data_cb_arg,
                 gpg_error_t (*inquire_cb)(void*, const char *),
                 void *inquire_cb_arg,
                 gpg_error_t (*status_cb)(void*, const char *),
                 void *status_cb_arg)
{
  gpg_error_t rc;
  assuan_response_t response;
  int off;
  char *line;
  int linelen;

  rc = assuan_write_line (ctx, command);
  if (rc)
    return rc;

  if (*command == '#' || !*command)
    return 0; /* Don't expect a response for a comment line.  */

 again:
  rc = _assuan_read_from_server (ctx, &response, &off);
  if (rc)
    return rc; /* error reading from server */

  line = ctx->inbound.line + off;
  linelen = ctx->inbound.linelen - off;

  if (response == ASSUAN_RESPONSE_ERROR)
    rc = atoi (line);
  else if (response == ASSUAN_RESPONSE_DATA)
    {
      if (!data_cb)
        rc = _assuan_error (ctx, GPG_ERR_ASS_NO_DATA_CB);
      else 
        {
          rc = data_cb (data_cb_arg, line, linelen);
          if (!rc)
            goto again;
        }
    }
  else if (response == ASSUAN_RESPONSE_INQUIRE)
    {
      if (!inquire_cb)
        {
          assuan_write_line (ctx, "END"); /* get out of inquire mode */
          _assuan_read_from_server (ctx, &response, &off); /* dummy read */
          rc = _assuan_error (ctx, GPG_ERR_ASS_NO_INQUIRE_CB);
        }
      else
        {
          rc = inquire_cb (inquire_cb_arg, line);
          if (!rc)
            rc = assuan_send_data (ctx, NULL, 0); /* flush and send END */
          if (!rc)
            goto again;
        }
    }
  else if (response == ASSUAN_RESPONSE_STATUS)
    {
      if (status_cb)
        rc = status_cb (status_cb_arg, line);
      if (!rc)
        goto again;
    }
  else if (response == ASSUAN_RESPONSE_END)
    {
      if (!data_cb)
        rc = _assuan_error (ctx, GPG_ERR_ASS_NO_DATA_CB);
      else 
        {
          rc = data_cb (data_cb_arg, NULL, 0);
          if (!rc)
            goto again;
        }
    }

  return rc;
}
