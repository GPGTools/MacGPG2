/* assuan-inquire.c - handle inquire stuff
   Copyright (C) 2001-2003, 2005, 2007, 2009 Free Software Foundation, Inc.

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

#include "assuan-defs.h"

#define digitp(a) ((a) >= '0' && (a) <= '9')
#define xtoi_1(p)   (*(p) <= '9'? (*(p)- '0'): \
                     *(p) <= 'F'? (*(p)-'A'+10):(*(p)-'a'+10))
#define xtoi_2(p)   ((xtoi_1(p) * 16) + xtoi_1((p)+1))


struct membuf
{
  size_t len;
  size_t size;
  char *buf;
  int out_of_core;
  int too_large;
  size_t maxlen;
};



/* A simple implementation of a dynamic buffer.  Use init_membuf() to
   create a buffer, put_membuf to append bytes and get_membuf to
   release and return the buffer.  Allocation errors are detected but
   only returned at the final get_membuf(), this helps not to clutter
   the code with out of core checks.  */

static void
init_membuf (assuan_context_t ctx,
	     struct membuf *mb, int initiallen, size_t maxlen)
{
  mb->len = 0;
  mb->size = initiallen;
  mb->out_of_core = 0;
  mb->too_large = 0;
  mb->maxlen = maxlen;
  /* we need to allocate one byte more for get_membuf */
  mb->buf = _assuan_malloc (ctx, initiallen + 1);
  if (!mb->buf)
      mb->out_of_core = 1;
}

static void
put_membuf (assuan_context_t ctx,
	    struct membuf *mb, const void *buf, size_t len)
{
  if (mb->out_of_core || mb->too_large)
    return;

  if (mb->maxlen && mb->len + len > mb->maxlen)
    {
      mb->too_large = 1;
      return;
    }

  if (mb->len + len >= mb->size)
    {
      char *p;
      
      mb->size += len + 1024;
      /* we need to allocate one byte more for get_membuf */
      p = _assuan_realloc (ctx, mb->buf, mb->size + 1);
      if (!p)
        {
          mb->out_of_core = 1;
          return;
        }
      mb->buf = p;
    }
  memcpy (mb->buf + mb->len, buf, len);
  mb->len += len;
}

static void *
get_membuf (assuan_context_t ctx, struct membuf *mb, size_t *len)
{
  char *p;

  if (mb->out_of_core || mb->too_large)
    {
      _assuan_free (ctx, mb->buf);
      mb->buf = NULL;
      return NULL;
    }

  mb->buf[mb->len] = 0; /* there is enough space for the hidden eos */
  p = mb->buf;
  *len = mb->len;
  mb->buf = NULL;
  mb->out_of_core = 1; /* don't allow a reuse */
  return p;
}

static void
free_membuf (assuan_context_t ctx, struct membuf *mb)
{
  _assuan_free (ctx, mb->buf);
  mb->buf = NULL;
}


/**
 * assuan_inquire:
 * @ctx: An assuan context
 * @keyword: The keyword used for the inquire
 * @r_buffer: Returns an allocated buffer
 * @r_length: Returns the length of this buffer
 * @maxlen: If not 0, the size limit of the inquired data.
 * 
 * A Server may use this to Send an inquire.  r_buffer, r_length and
 * maxlen may all be NULL/0 to indicate that no real data is expected.
 * 
 * Return value: 0 on success or an ASSUAN error code
 **/
gpg_error_t
assuan_inquire (assuan_context_t ctx, const char *keyword,
                unsigned char **r_buffer, size_t *r_length, size_t maxlen)
{
  gpg_error_t rc;
  struct membuf mb;
  char cmdbuf[LINELENGTH-10]; /* (10 = strlen ("INQUIRE ")+CR,LF) */
  unsigned char *line, *p;
  int linelen;
  int nodataexpected;

  if (!ctx || !keyword || (10 + strlen (keyword) >= sizeof (cmdbuf)))
    return _assuan_error (ctx, GPG_ERR_ASS_INV_VALUE);
  nodataexpected = !r_buffer && !r_length && !maxlen;
  if (!nodataexpected && (!r_buffer || !r_length))
    return _assuan_error (ctx, GPG_ERR_ASS_INV_VALUE);
  if (!ctx->is_server)
    return _assuan_error (ctx, GPG_ERR_ASS_NOT_A_SERVER);
  if (ctx->in_inquire)
    return _assuan_error (ctx, GPG_ERR_ASS_NESTED_COMMANDS);
  
  ctx->in_inquire = 1;
  if (nodataexpected)
    memset (&mb, 0, sizeof mb); /* avoid compiler warnings */
  else
    init_membuf (ctx, &mb, maxlen? maxlen:1024, maxlen);

  strcpy (stpcpy (cmdbuf, "INQUIRE "), keyword);
  rc = assuan_write_line (ctx, cmdbuf);
  if (rc)
    goto leave;

  for (;;)
    {
      do 
        {
	  do
	    rc = _assuan_read_line (ctx);
	  while (_assuan_error_is_eagain (ctx, rc));
          if (rc)
            goto leave;
          line = (unsigned char *) ctx->inbound.line;
          linelen = ctx->inbound.linelen;
        }    
      while (*line == '#' || !linelen);

      /* Note: As a convenience for manual testing we allow case
         insensitive keywords.  */
      if ((line[0] == 'E'||line[0] == 'e')
          && (line[1] == 'N' || line[1] == 'n')
          && (line[2] == 'D' || line[2] == 'd')
          && (!line[3] || line[3] == ' '))
        break; /* END command received*/
      if ((line[0] == 'C' || line[0] == 'c')
          && (line[1] == 'A' || line[1] == 'a')
          && (line[2] == 'N' || line[2] == 'n'))
        {
          rc = _assuan_error (ctx, GPG_ERR_ASS_CANCELED);
          goto leave;
        }
      if ((line[0] != 'D' && line[0] != 'd') 
          || line[1] != ' ' || nodataexpected)
        {
          rc = _assuan_error (ctx, GPG_ERR_ASS_UNEXPECTED_CMD);
          goto leave;
        }
      if (linelen < 3)
        continue;
      line += 2;
      linelen -= 2;

      p = line;
      while (linelen)
        {
          for (;linelen && *p != '%'; linelen--, p++)
            ;
          put_membuf (ctx, &mb, line, p-line);
          if (linelen > 2)
            { /* handle escaping */
              unsigned char tmp[1];
              p++;
              *tmp = xtoi_2 (p);
              p += 2;
              linelen -= 3;
              put_membuf (ctx, &mb, tmp, 1);
            }
          line = p;
        }
      if (mb.too_large)
        {
          rc = _assuan_error (ctx, GPG_ERR_ASS_TOO_MUCH_DATA);
          goto leave;
        }
    }

  if (!nodataexpected)
    {
      *r_buffer = get_membuf (ctx, &mb, r_length);
      if (!*r_buffer)
	rc = _assuan_error (ctx, gpg_err_code_from_syserror ());
    }

 leave:
  if (!nodataexpected)
    free_membuf (ctx, &mb);
  ctx->in_inquire = 0;
  return rc;
}


void
_assuan_inquire_release (assuan_context_t ctx)
{
  if (ctx->in_inquire)
    {
      if (ctx->inquire_membuf)
	{
	  free_membuf (ctx, ctx->inquire_membuf);
	  free (ctx->inquire_membuf);
	}
      ctx->in_inquire = 0;
    }
}


gpg_error_t
_assuan_inquire_ext_cb (assuan_context_t ctx)
{
  gpg_error_t rc;
  unsigned char *line;
  int linelen;
  struct membuf *mb;
  unsigned char *p;

  line = (unsigned char *) ctx->inbound.line;
  linelen = ctx->inbound.linelen;
  mb = ctx->inquire_membuf;

  if ((line[0] == 'C' || line[0] == 'c')
      && (line[1] == 'A' || line[1] == 'a')
      && (line[2] == 'N' || line[2] == 'n'))
    {
      rc = _assuan_error (ctx, GPG_ERR_ASS_CANCELED);
      goto leave;
    }
  if ((line[0] == 'E'||line[0] == 'e')
      && (line[1] == 'N' || line[1] == 'n')
      && (line[2] == 'D' || line[2] == 'd')
      && (!line[3] || line[3] == ' '))
    {
      rc = 0;
      goto leave;
    }

  if ((line[0] != 'D' && line[0] != 'd') || line[1] != ' ' || mb == NULL)
    {
      rc = _assuan_error (ctx, GPG_ERR_ASS_UNEXPECTED_CMD);
      goto leave;
    }
  
  if (linelen < 3)
    return 0;
  line += 2;
  linelen -= 2;
  
  p = line;
  while (linelen)
    {
      for (;linelen && *p != '%'; linelen--, p++)
	;
      put_membuf (ctx, mb, line, p-line);
      if (linelen > 2)
	{ /* handle escaping */
	  unsigned char tmp[1];
	  p++;
	  *tmp = xtoi_2 (p);
	  p += 2;
	  linelen -= 3;
	  put_membuf (ctx, mb, tmp, 1);
	}
      line = p;
    }
  if (mb->too_large)
    {
      rc = _assuan_error (ctx, GPG_ERR_ASS_TOO_MUCH_DATA);
      goto leave;
    }

  return 0;

 leave:
  {
    size_t buf_len = 0;
    unsigned char *buf = NULL;
    
    if (mb)
      {
	buf = get_membuf (ctx, mb, &buf_len);
	if (!buf)
	  rc = _assuan_error (ctx, gpg_err_code_from_syserror ());
	free_membuf (ctx, mb);
	free (mb);
	ctx->inquire_membuf = NULL;
      }
    ctx->in_inquire = 0;
    rc = (ctx->inquire_cb) (ctx->inquire_cb_data, rc, buf, buf_len);
  }
  return rc;
}

/**
 * assuan_inquire_ext:
 * @ctx: An assuan context
 * @keyword: The keyword used for the inquire
 * @maxlen: If not 0, the size limit of the inquired data.
 * @cb: A callback handler which is invoked after the operation completed.
 * @cb_data: A user-provided value passed to the callback handler.
 * 
 * A server may use this to send an inquire.  R_BUFFER, R_LENGTH and
 * MAXLEN may all be NULL/0 to indicate that no real data is expected.
 * When this function returns, 
 *
 * Return value: 0 on success or an ASSUAN error code
 **/
gpg_error_t
assuan_inquire_ext (assuan_context_t ctx, const char *keyword, size_t maxlen,
		    gpg_error_t (*cb) (void *cb_data, gpg_error_t rc,
				       unsigned char *buf, size_t len),
		    void *cb_data)
{
  gpg_error_t rc;
  struct membuf *mb = NULL;
  char cmdbuf[LINELENGTH-10]; /* (10 = strlen ("INQUIRE ")+CR,LF) */

  if (!ctx || !keyword || (10 + strlen (keyword) >= sizeof (cmdbuf)))
    return _assuan_error (ctx, GPG_ERR_ASS_INV_VALUE);
  if (!ctx->is_server)
    return _assuan_error (ctx, GPG_ERR_ASS_NOT_A_SERVER);
  if (ctx->in_inquire)
    return _assuan_error (ctx, GPG_ERR_ASS_NESTED_COMMANDS);

  mb = malloc (sizeof (struct membuf));
  if (!mb)
    return _assuan_error (ctx, gpg_err_code_from_syserror ());
  init_membuf (ctx, mb, maxlen ? maxlen : 1024, maxlen);

  strcpy (stpcpy (cmdbuf, "INQUIRE "), keyword);
  rc = assuan_write_line (ctx, cmdbuf);
  if (rc)
    {
      free_membuf (ctx, mb); 
      free (mb);
      return rc;
    }

  ctx->in_inquire = 1;

  /* Set up the continuation.  */
  ctx->inquire_cb = cb;
  ctx->inquire_cb_data = cb_data;
  ctx->inquire_membuf = mb;

  return 0;
}
