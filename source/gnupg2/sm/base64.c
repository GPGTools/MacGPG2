/* base64.c 
 *	Copyright (C) 2001, 2003 Free Software Foundation, Inc.
 *
 * This file is part of GnuPG.
 *
 * GnuPG is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * GnuPG is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h> 
#include <time.h>
#include <assert.h>

#include "gpgsm.h"


#include <ksba.h>

#include "i18n.h"

#ifdef HAVE_DOSISH_SYSTEM
  #define LF "\r\n"
#else
  #define LF "\n"
#endif

/* data used by the reader callbacks */
struct reader_cb_parm_s {
  FILE *fp;
  
  unsigned char line[1024];
  int linelen;
  int readpos;
  int have_lf;
  unsigned long line_counter;

  int allow_multi_pem;  /* Allow processing of multiple PEM objects. */
  int autodetect;       /* Try to detect the input encoding. */
  int assume_pem;       /* Assume input encoding is PEM. */
  int assume_base64;    /* Assume input is base64 encoded. */

  int identified;
  int is_pem;
  int is_base64;
  int stop_seen;
  int might_be_smime;

  int eof_seen;         

  struct {
    int idx;
    unsigned char val;
    int stop_seen;
  } base64;
};

/* data used by the writer callbacks */
struct writer_cb_parm_s {
  FILE *fp;            /* FP is only used if STREAM is NULL.  */
  estream_t stream;    /* Alternative output if not NULL.  */

  const char *pem_name;
  
  int wrote_begin;
  int did_finish;

  struct {
    int idx;
    int quad_count;
    unsigned char radbuf[4];
  } base64;

};


/* context for this module's functions */
struct base64_context_s {
  union {
    struct reader_cb_parm_s rparm;
    struct writer_cb_parm_s wparm;
  } u;

  union {
    ksba_reader_t reader;
    ksba_writer_t writer;
  } u2;
};


/* The base-64 character list */
static char bintoasc[64] = 
       "ABCDEFGHIJKLMNOPQRSTUVWXYZ" 
       "abcdefghijklmnopqrstuvwxyz" 
       "0123456789+/"; 
/* The reverse base-64 list */
static unsigned char asctobin[256] = {
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x3e, 0xff, 0xff, 0xff, 0x3f, 
  0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 
  0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 
  0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 
  0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 
  0x31, 0x32, 0x33, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff
};


static int
has_only_base64 (const unsigned char *line, int linelen)
{
  if (linelen < 20)
    return 0;
  for (; linelen; line++, linelen--)
    {
      if (*line == '\n' || (linelen > 1 && *line == '\r' && line[1] == '\n'))
          break;
      if ( !strchr (bintoasc, *line) )
        return 0;
    }
  return 1;  /* yes */
}

static int
is_empty_line (const unsigned char *line, int linelen)
{
  if (linelen >= 2 && *line == '\r' && line[1] == '\n')
    return 1;
  if (linelen >= 1 && *line == '\n')
    return 1;
  return 0;
}


static int
base64_reader_cb (void *cb_value, char *buffer, size_t count, size_t *nread)
{
  struct reader_cb_parm_s *parm = cb_value;
  size_t n;
  int c, c2;

  *nread = 0;
  if (!buffer)
    return -1; /* not supported */

 next:
  if (!parm->linelen)
    {
      /* read an entire line or up to the size of the buffer */
      parm->line_counter++;
      parm->have_lf = 0;
      for (n=0; n < DIM(parm->line);)
        {
          c = getc (parm->fp);
          if (c == EOF)
            {
              parm->eof_seen = 1;
              if (ferror (parm->fp))
                return -1;
              break; 
            }
          parm->line[n++] = c;
          if (c == '\n')
            {
              parm->have_lf = 1;
              /* Fixme: we need to skip overlong lines while detecting
                 the dashed lines */
              break;
            }
        }
      parm->linelen = n;
      if (!n)
        return -1; /* eof */
      parm->readpos = 0;
    }

  if (!parm->identified)
    {
      if (!parm->autodetect)
        {
          if (parm->assume_pem)
            {
              /* wait for the header line */
              parm->linelen = parm->readpos = 0;
              if (!parm->have_lf 
                  || strncmp ((char*)parm->line, "-----BEGIN ", 11)
                  || !strncmp ((char*)parm->line+11, "PGP ", 4))
                goto next;
              parm->is_pem = 1;
            }
          else if (parm->assume_base64)
            parm->is_base64 = 1;
        }
      else if (parm->line_counter == 1 && !parm->have_lf)
        {
          /* first line too long - assume DER encoding */
          parm->is_pem = 0;
        }
      else if (parm->line_counter == 1 && parm->linelen && *parm->line == 0x30)
        {
          /* the very first byte does pretty much look like a SEQUENCE tag*/
          parm->is_pem = 0;
        }
      else if ( parm->have_lf
                && !strncmp ((char*)parm->line, "-----BEGIN ", 11)
                && strncmp ((char *)parm->line+11, "PGP ", 4) )
        {
          /* Fixme: we must only compare if the line really starts at
             the beginning */
          parm->is_pem = 1;
          parm->linelen = parm->readpos = 0;
        }
      else if ( parm->have_lf && parm->line_counter == 1
                && parm->linelen >= 13
                && !ascii_memcasecmp (parm->line, "Content-Type:", 13))
        { /* might be a S/MIME body */
          parm->might_be_smime = 1;
          parm->linelen = parm->readpos = 0;
          goto next;
        }
      else if (parm->might_be_smime == 1
               && is_empty_line (parm->line, parm->linelen))
        {
          parm->might_be_smime = 2;
          parm->linelen = parm->readpos = 0;
          goto next;
        }
      else if (parm->might_be_smime == 2)
        {
          parm->might_be_smime = 0;
          if ( !has_only_base64 (parm->line, parm->linelen))
            {
              parm->linelen = parm->readpos = 0;
              goto next;
            }
          parm->is_pem = 1;
        }
      else
        {
          parm->linelen = parm->readpos = 0;
          goto next;
        }
      parm->identified = 1;
      parm->base64.stop_seen = 0;
      parm->base64.idx = 0;
    }
  

  n = 0;
  if (parm->is_pem || parm->is_base64)
    {  
      if (parm->is_pem && parm->have_lf
          && !strncmp ((char*)parm->line, "-----END ", 9))
        { 
          parm->identified = 0;
          parm->linelen = parm->readpos = 0;

          /* If the caller want to read multiple PEM objects from one
             file, we have to reset our internal state and return a
             EOF immediately. The caller is the expected to use
             ksba_reader_clear to clear the EOF condition and continue
             to read.  If we don't want to do that we just return 0
             bytes which will force the ksba_reader to skip until
             EOF. */
          if (parm->allow_multi_pem)
            {
              parm->identified = 0;
              parm->autodetect = 0;
              parm->assume_pem = 1;
              parm->stop_seen = 0;
              return -1; /* Send EOF now. */
            }
        }
      else if (parm->stop_seen)
        { /* skip the rest of the line */
          parm->linelen = parm->readpos = 0;
        }
      else
        {
          int idx = parm->base64.idx;
          unsigned char val = parm->base64.val;

          while (n < count && parm->readpos < parm->linelen )
            {
              c = parm->line[parm->readpos++];
              if (c == '\n' || c == ' ' || c == '\r' || c == '\t')
                continue;
              if (c == '=')
                { /* pad character: stop */
                  if (idx == 1)
                    buffer[n++] = val; 
                  parm->stop_seen = 1;
                  break;
                }
              if( (c = asctobin[(c2=c)]) == 255 ) 
                {
                  log_error (_("invalid radix64 character %02x skipped\n"),
                             c2);
                  continue;
                }
              switch (idx) 
                {
                case 0: 
                  val = c << 2;
                  break;
                case 1: 
                  val |= (c>>4)&3;
                  buffer[n++] = val;
                  val = (c<<4)&0xf0;
                  break;
                case 2: 
                  val |= (c>>2)&15;
                  buffer[n++] = val;
                  val = (c<<6)&0xc0;
                  break;
                case 3: 
                  val |= c&0x3f;
                  buffer[n++] = val;
                  break;
                }
              idx = (idx+1) % 4;
            }
          if (parm->readpos == parm->linelen)
            parm->linelen = parm->readpos = 0;

          parm->base64.idx = idx;
          parm->base64.val = val;
        }
    }
  else
    { /* DER encoded */
      while (n < count && parm->readpos < parm->linelen)
          buffer[n++] = parm->line[parm->readpos++];
      if (parm->readpos == parm->linelen)
        parm->linelen = parm->readpos = 0;
    }

  *nread = n;
  return 0;
}



static int
simple_reader_cb (void *cb_value, char *buffer, size_t count, size_t *nread)
{
  struct reader_cb_parm_s *parm = cb_value;
  size_t n;
  int c = 0;

  *nread = 0;
  if (!buffer)
    return -1; /* not supported */

  for (n=0; n < count; n++)
    {
      c = getc (parm->fp);
      if (c == EOF)
        {
          parm->eof_seen = 1;
          if ( ferror (parm->fp) )
            return -1;
          if (n)
            break; /* return what we have before an EOF */
          return -1;
        }
      *(byte *)buffer++ = c;
    }

  *nread = n;
  return 0;
}




/* Call either es_putc or the plain putc.  */
static void
do_putc (int value, FILE *fp, estream_t stream)
{
  if (stream)
    es_putc (value, stream);
  else
    putc (value, fp);
}

/* Call either es_fputs or the plain fputs.  */
static void
do_fputs (const char *string, FILE *fp, estream_t stream)
{
  if (stream)
    es_fputs (string, stream);
  else
    fputs (string, fp);
}


static int
base64_writer_cb (void *cb_value, const void *buffer, size_t count)
{
  struct writer_cb_parm_s *parm = cb_value;
  unsigned char radbuf[4];
  int i, c, idx, quad_count;
  const unsigned char *p;
  FILE *fp = parm->fp;
  estream_t stream = parm->stream;

  if (!count)
    return 0;

  if (!parm->wrote_begin)
    {
      if (parm->pem_name)
        {
          do_fputs ("-----BEGIN ", fp, stream);
          do_fputs (parm->pem_name, fp, stream);
          do_fputs ("-----\n", fp, stream);
        }
      parm->wrote_begin = 1;
      parm->base64.idx = 0;
      parm->base64.quad_count = 0;
    }

  idx = parm->base64.idx;
  quad_count = parm->base64.quad_count;
  for (i=0; i < idx; i++)
    radbuf[i] = parm->base64.radbuf[i];

  for (p=buffer; count; p++, count--)
    {
      radbuf[idx++] = *p;
      if (idx > 2)
        {
          idx = 0;
          c = bintoasc[(*radbuf >> 2) & 077];
          do_putc (c, fp, stream);
          c = bintoasc[(((*radbuf<<4)&060)|((radbuf[1] >> 4)&017))&077];
          do_putc (c, fp, stream);
          c = bintoasc[(((radbuf[1]<<2)&074)|((radbuf[2]>>6)&03))&077];
          do_putc (c, fp, stream);
          c = bintoasc[radbuf[2]&077];
          do_putc (c, fp, stream);
          if (++quad_count >= (64/4)) 
            {
              do_fputs (LF, fp, stream);
              quad_count = 0;
            }
        }
    }
  for (i=0; i < idx; i++)
    parm->base64.radbuf[i] = radbuf[i];
  parm->base64.idx = idx;
  parm->base64.quad_count = quad_count;

  return ((stream? es_ferror (stream) : ferror (fp)) 
          ? gpg_error_from_syserror () 
          : 0);
}

/* This callback is only used in stream mode.  Hiowever, we don't
   restrict it to this.  */
static int
plain_writer_cb (void *cb_value, const void *buffer, size_t count)
{
  struct writer_cb_parm_s *parm = cb_value;
  FILE *fp = parm->fp;
  estream_t stream = parm->stream;

  if (!count)
    return 0;

  if (stream)
    es_write (stream, buffer, count, NULL);
  else
    fwrite (buffer, count, 1, fp);

  return ((stream? es_ferror (stream) : ferror (fp)) 
          ? gpg_error_from_syserror () 
          : 0);
}

static int
base64_finish_write (struct writer_cb_parm_s *parm)
{
  unsigned char radbuf[4];
  int i, c, idx, quad_count;
  FILE *fp = parm->fp;
  estream_t stream = parm->stream;

  if (!parm->wrote_begin)
    return 0; /* Nothing written or we are not called in base-64 mode. */

  /* flush the base64 encoding */
  idx = parm->base64.idx;
  quad_count = parm->base64.quad_count;
  for (i=0; i < idx; i++)
    radbuf[i] = parm->base64.radbuf[i];

  if (idx)
    {
      c = bintoasc[(*radbuf>>2)&077];
      do_putc (c, fp, stream);
      if (idx == 1)
        {
          c = bintoasc[((*radbuf << 4) & 060) & 077];
          do_putc (c, fp, stream);
          do_putc ('=', fp, stream);
          do_putc ('=', fp, stream);
        }
      else 
        { 
          c = bintoasc[(((*radbuf<<4)&060)|((radbuf[1]>>4)&017))&077];
          do_putc (c, fp, stream);
          c = bintoasc[((radbuf[1] << 2) & 074) & 077];
          do_putc (c, fp, stream);
          do_putc ('=', fp, stream);

        }
      if (++quad_count >= (64/4)) 
        {
          do_fputs (LF, fp, stream);
          quad_count = 0;
        }
    }

  if (quad_count)
    do_fputs (LF, fp, stream);

  if (parm->pem_name)
    {
      do_fputs ("-----END ", fp, stream);
      do_fputs (parm->pem_name, fp, stream);
      do_fputs ("-----\n", fp, stream);
    }

  return ((stream? es_ferror (stream) : ferror (fp)) 
          ? gpg_error_from_syserror () 
          : 0);
}




/* Create a reader for the given file descriptor.  Depending on the
   control information an input decoding is automagically choosen.
   The function returns a Base64Context object which must be passed to
   the gpgme_destroy_reader function.  The created KsbaReader object
   is also returned, but the caller must not call the
   ksba_reader_release function on.  If ALLOW_MULTI_PEM is true, the
   reader expects that the caller uses ksba_reader_clear after EOF
   until no more objects were found. */
int
gpgsm_create_reader (Base64Context *ctx,
                     ctrl_t ctrl, FILE *fp, int allow_multi_pem,
                     ksba_reader_t *r_reader)
{
  int rc;
  ksba_reader_t r;

  *r_reader = NULL;
  *ctx = xtrycalloc (1, sizeof **ctx);
  if (!*ctx)
    return out_of_core ();
  (*ctx)->u.rparm.allow_multi_pem = allow_multi_pem;

  rc = ksba_reader_new (&r);
  if (rc)
    {
      xfree (*ctx); *ctx = NULL;
      return rc;
    }

  (*ctx)->u.rparm.fp = fp;
  if (ctrl->is_pem)
    {
      (*ctx)->u.rparm.assume_pem = 1;
      (*ctx)->u.rparm.assume_base64 = 1;
      rc = ksba_reader_set_cb (r, base64_reader_cb, &(*ctx)->u.rparm);
    }
  else if (ctrl->is_base64)
    {
      (*ctx)->u.rparm.assume_base64 = 1;
      rc = ksba_reader_set_cb (r, base64_reader_cb, &(*ctx)->u.rparm);
    }
  else if (ctrl->autodetect_encoding)
    {
      (*ctx)->u.rparm.autodetect = 1;
      rc = ksba_reader_set_cb (r, base64_reader_cb, &(*ctx)->u.rparm);
    }
  else
      rc = ksba_reader_set_cb (r, simple_reader_cb, &(*ctx)->u.rparm);

  if (rc)
    {
      ksba_reader_release (r);
      xfree (*ctx); *ctx = NULL;
      return rc;
    }

  (*ctx)->u2.reader = r;
  *r_reader = r;
  return 0;
}


int
gpgsm_reader_eof_seen (Base64Context ctx)
{
  return ctx && ctx->u.rparm.eof_seen;
}

void
gpgsm_destroy_reader (Base64Context ctx)
{
  if (!ctx)
    return;

  ksba_reader_release (ctx->u2.reader);  
  xfree (ctx);
}



/* Create a writer for the given stream FP or STREAM.  Depending on
   the control information an output encoding is automagically
   choosen.  The function returns a Base64Context object which must be
   passed to the gpgme_destroy_writer function.  The created
   KsbaWriter object is also returned, but the caller must not call
   the ksba_reader_release function on. */
int
gpgsm_create_writer (Base64Context *ctx,
                     ctrl_t ctrl, FILE *fp, estream_t stream,
                     ksba_writer_t *r_writer)
{
  int rc;
  ksba_writer_t w;

  *r_writer = NULL;
  *ctx = xtrycalloc (1, sizeof **ctx);
  if (!*ctx)
    return out_of_core ();

  rc = ksba_writer_new (&w);
  if (rc)
    {
      xfree (*ctx); *ctx = NULL;
      return rc;
    }

  if (ctrl->create_pem || ctrl->create_base64)
    {
      (*ctx)->u.wparm.fp = fp;
      (*ctx)->u.wparm.stream = stream;
      if (ctrl->create_pem)
        (*ctx)->u.wparm.pem_name = ctrl->pem_name? ctrl->pem_name
                                                 : "CMS OBJECT";
      rc = ksba_writer_set_cb (w, base64_writer_cb, &(*ctx)->u.wparm);
    }
  else if (stream)
    {
      (*ctx)->u.wparm.fp = fp;
      (*ctx)->u.wparm.stream = stream;
      rc = ksba_writer_set_cb (w, plain_writer_cb, &(*ctx)->u.wparm);
    }
  else
    rc = ksba_writer_set_file (w, fp);

  if (rc)
    {
      ksba_writer_release (w);
      xfree (*ctx); *ctx = NULL;
      return rc;
    }

  (*ctx)->u2.writer = w;
  *r_writer = w;
  return 0;
}


int
gpgsm_finish_writer (Base64Context ctx)
{
  struct writer_cb_parm_s *parm;
  
  if (!ctx)
    return gpg_error (GPG_ERR_INV_VALUE);
  parm = &ctx->u.wparm;
  if (parm->did_finish)
    return 0; /* Already done. */
  parm->did_finish = 1;
  if (!parm->fp && !parm->stream)
    return 0; /* Callback was not used.  */
  return base64_finish_write (parm);
}

void
gpgsm_destroy_writer (Base64Context ctx)
{
  if (!ctx)
    return;

  ksba_writer_release (ctx->u2.writer);
  xfree (ctx);
}
