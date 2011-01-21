/* dn.c - Distinguished Name helper functions
 *      Copyright (C) 2001, 2006 g10 Code GmbH
 *
 * This file is part of KSBA.
 *
 * KSBA is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * KSBA is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

/* Reference is RFC-2253 */

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "util.h"
#include "asn1-func.h"
#include "ber-help.h"
#include "ber-decoder.h"

static const struct {
  const char *name;
  int source; /* 0 = unknown
                 1 = rfc2253
                 2 = David Chadwick, July 2003
                 <draft-ietf-pkix-dnstrings-02.txt> 
                 3 = Peter Gutmann
              */
  const char *description;
  size_t      oidlen;
  const unsigned char *oid;  /* DER encoded OID.  */
  const char *oidstr;        /* OID as dotted string.  */
} oid_name_tbl[] = { 
{"CN", 1, "CommonName",            3, "\x55\x04\x03", "2.5.4.3" },
{"SN", 2, "Surname",               3, "\x55\x04\x04", "2.5.4.4" },
{"SERIALNUMBER", 2, "SerialNumber",3, "\x55\x04\x05", "2.5.4.5" },
{"C",  1, "CountryName",           3, "\x55\x04\x06", "2.5.4.6" },
{"L" , 1, "LocalityName",          3, "\x55\x04\x07", "2.5.4.7" },
{"ST", 1, "StateOrProvince",       3, "\x55\x04\x08", "2.5.4.8" },
{"STREET", 1, "StreetAddress",     3, "\x55\x04\x09", "2.5.4.9" },
{"O",  1, "OrganizationName",      3, "\x55\x04\x0a", "2.5.4.10" },
{"OU", 1, "OrganizationalUnit",    3, "\x55\x04\x0b", "2.5.4.11" },
{"T",  2, "Title",                 3, "\x55\x04\x0c", "2.5.4.12" },
{"D",  3, "Description",           3, "\x55\x04\x0d", "2.5.4.13" },
{"BC", 3, "BusinessCategory",      3, "\x55\x04\x0f", "2.5.4.15" },
{"ADDR", 2, "PostalAddress",       3, "\x55\x04\x11", "2.5.4.16" },
{"POSTALCODE" , 0, "PostalCode",   3, "\x55\x04\x11", "2.5.4.17" },
{"GN", 2, "GivenName",             3, "\x55\x04\x2a", "2.5.4.42" },
{"PSEUDO", 2, "Pseudonym",         3, "\x55\x04\x41", "2.5.4.65" },
{"DC", 1, "domainComponent",      10, 
    "\x09\x92\x26\x89\x93\xF2\x2C\x64\x01\x19", "0.9.2342.19200300.100.1.25" },
{"UID", 1, "userid",              10,
    "\x09\x92\x26\x89\x93\xF2\x2C\x64\x01\x01", "0.9.2342.19200300.100.1.1 " },
{"EMAIL", 3, "emailAddress",       9,
    "\x2A\x86\x48\x86\xF7\x0D\x01\x09\x01",     "1.2.840.113549.1.9.1" },
{ NULL }
};


#define N 0x00
#define P 0x01
static unsigned char charclasses[128] = {
  N, N, N, N, N, N, N, N,  N, N, N, N, N, N, N, N, 
  N, N, N, N, N, N, N, N,  N, N, N, N, N, N, N, N, 
  P, N, N, N, N, N, N, P,  P, P, N, P, P, P, P, P, 
  P, P, P, P, P, P, P, P,  P, P, P, N, N, P, N, P, 
  N, P, P, P, P, P, P, P,  P, P, P, P, P, P, P, P, 
  P, P, P, P, P, P, P, P,  P, P, P, N, N, N, N, N, 
  N, P, P, P, P, P, P, P,  P, P, P, P, P, P, P, P, 
  P, P, P, P, P, P, P, P,  P, P, P, N, N, N, N, N
};
#undef N
#undef P

struct stringbuf {
  size_t len;
  size_t size;
  char *buf;
  int out_of_core;
};



static void
init_stringbuf (struct stringbuf *sb, int initiallen)
{
  sb->len = 0;
  sb->size = initiallen;
  sb->out_of_core = 0;
  /* allocate one more, so that get_stringbuf can append a nul */
  sb->buf = xtrymalloc (initiallen+1);
  if (!sb->buf)
      sb->out_of_core = 1;
}

static void
deinit_stringbuf (struct stringbuf *sb)
{
  xfree (sb->buf); 
  sb->buf = NULL;
  sb->out_of_core = 1; /* make sure the caller does an init before reuse */
}


static void
put_stringbuf (struct stringbuf *sb, const char *text)
{
  size_t n = strlen (text);

  if (sb->out_of_core)
    return;

  if (sb->len + n >= sb->size)
    {
      char *p;
      
      sb->size += n + 100;
      p = xtryrealloc (sb->buf, sb->size);
      if ( !p)
        {
          sb->out_of_core = 1;
          return;
        }
      sb->buf = p;
    }
  memcpy (sb->buf+sb->len, text, n);
  sb->len += n;
}

static void
put_stringbuf_mem (struct stringbuf *sb, const char *text, size_t n)
{
  if (sb->out_of_core)
    return;

  if (sb->len + n >= sb->size)
    {
      char *p;
      
      sb->size += n + 100;
      p = xtryrealloc (sb->buf, sb->size);
      if ( !p)
        {
          sb->out_of_core = 1;
          return;
        }
      sb->buf = p;
    }
  memcpy (sb->buf+sb->len, text, n);
  sb->len += n;
}

static void
put_stringbuf_mem_skip (struct stringbuf *sb, const char *text, size_t n,
                        int skip)
{
  char *p;
      
  if (!skip)
    {
      put_stringbuf_mem (sb, text, n);
      return;
    }
  if (sb->out_of_core)
    return;

  if (sb->len + n >= sb->size)
    {
      /* Note: we allocate too much here, but we don't care. */
      sb->size += n + 100;
      p = xtryrealloc (sb->buf, sb->size);
      if ( !p)
        {
          sb->out_of_core = 1;
          return;
        }
      sb->buf = p;
    }
  p = sb->buf+sb->len;
  while (n > skip)
    {
      text += skip;
      n -= skip;
      *p++ = *text++;
      n--;
      sb->len++;
    }
}

static char *
get_stringbuf (struct stringbuf *sb)
{
  char *p;

  if (sb->out_of_core)
    {
      xfree (sb->buf); sb->buf = NULL;
      return NULL;
    }

  sb->buf[sb->len] = 0;
  p = sb->buf;
  sb->buf = NULL;
  sb->out_of_core = 1; /* make sure the caller does an init before reuse */
  return p;
}


/* This function is used for 1 byte encodings to insert any required
   quoting.  It does not do the quoting for a space or hash mark at
   the beginning of a string or a space as the last character of a
   string.  It will do steps of SKIP+1 characters, assuming that these
   SKIP characters are null octets. */
static void
append_quoted (struct stringbuf *sb, const unsigned char *value, size_t length,
               int skip)
{
  unsigned char tmp[4];
  const unsigned char *s = value;
  size_t n = 0;

  for (;;)
    {
      for (value = s; n+skip < length; n++, s++)
        {
          s += skip;
          n += skip;
          if (*s < ' ' || *s > 126 || strchr (",+\"\\<>;", *s) )
            break;
        }

      if (s != value)
        put_stringbuf_mem_skip (sb, value, s-value, skip);
      if (n+skip >= length)
        return; /* ready */
      s += skip;
      n += skip;
      if ( *s < ' ' || *s > 126 )
        {
          sprintf (tmp, "\\%02X", *s);
          put_stringbuf_mem (sb, tmp, 3);
        }
      else
        {
          tmp[0] = '\\';
          tmp[1] = *s;
          put_stringbuf_mem (sb, tmp, 2);
        }
      n++; s++;
    }
}


/* Append VALUE of LENGTH and TYPE to SB.  Do the required quoting. */
static void
append_utf8_value (const unsigned char *value, size_t length,
                   struct stringbuf *sb)
{
  unsigned char tmp[6];
  const unsigned char *s;
  size_t n;
  int i, nmore;

  if (length && (*value == ' ' || *value == '#'))
    {
      tmp[0] = '\\';
      tmp[1] = *value;
      put_stringbuf_mem (sb, tmp, 2);
      value++;
      length--;
    }
  if (length && value[length-1] == ' ')
    {
      tmp[0] = '\\';
      tmp[1] = ' ';
      put_stringbuf_mem (sb, tmp, 2);
      length--;
    }

  /* FIXME: check that the invalid encoding handling is correct */
  for (s=value, n=0;;)
    {
      for (value = s; n < length && !(*s & 0x80); n++, s++)
        ;
      if (s != value)
        append_quoted (sb, value, s-value, 0);
      if (n==length)
        return; /* ready */
      assert ((*s & 0x80));
      if ( (*s & 0xe0) == 0xc0 )      /* 110x xxxx */
        nmore = 1;
      else if ( (*s & 0xf0) == 0xe0 ) /* 1110 xxxx */
        nmore = 2;
      else if ( (*s & 0xf8) == 0xf0 ) /* 1111 0xxx */
        nmore = 3;
      else if ( (*s & 0xfc) == 0xf8 ) /* 1111 10xx */
        nmore = 4;
      else if ( (*s & 0xfe) == 0xfc ) /* 1111 110x */
        nmore = 5;
      else /* invalid encoding */
        nmore = 5;  /* we will reduce the check length anyway */

      if (n+nmore > length)
        nmore = length - n; /* oops, encoding to short */ 

      tmp[0] = *s++; n++;
      for (i=1; i <= nmore; i++)
        {
          if ( (*s & 0xc0) != 0x80)
            break; /* invalid encoding - stop */
          tmp[i] = *s++;
          n++;
        }
      put_stringbuf_mem (sb, tmp, i);
    }
}

/* Append VALUE of LENGTH and TYPE to SB.  Do character set conversion
   and quoting */
static void
append_latin1_value (const unsigned char *value, size_t length,
                     struct stringbuf *sb)
{
  unsigned char tmp[2];
  const unsigned char *s;
  size_t n;

  if (length && (*value == ' ' || *value == '#'))
    {
      tmp[0] = '\\';
      tmp[1] = *value;
      put_stringbuf_mem (sb, tmp, 2);
      value++;
      length--;
    }
  if (length && value[length-1] == ' ')
    {
      tmp[0] = '\\';
      tmp[1] = ' ';
      put_stringbuf_mem (sb, tmp, 2);
      length--;
    }

  for (s=value, n=0;;)
    {
      for (value = s; n < length && !(*s & 0x80); n++, s++)
        ;
      if (s != value)
        append_quoted (sb, value, s-value, 0);
      if (n==length)
        return; /* ready */
      assert ((*s & 0x80));
      tmp[0] = 0xc0 | ((*s >> 6) & 3);
      tmp[1] = 0x80 | ( *s & 0x3f );
      put_stringbuf_mem (sb, tmp, 2);
      n++; s++;
    }
}

/* Append VALUE of LENGTH and TYPE to SB.  Do UCS-4 to utf conversion
   and and quoting */
static void
append_ucs4_value (const unsigned char *value, size_t length,
                   struct stringbuf *sb)
{
  unsigned char tmp[7];
  const unsigned char *s;
  size_t n;
  unsigned int c;
  int i;

  if (length>3 && !value[0] && !value[1] && !value[2]
      && (value[3] == ' ' || value[3] == '#'))
    {
      tmp[0] = '\\';
      tmp[1] = *value;
      put_stringbuf_mem (sb, tmp, 2);
      value += 4;
      length -= 4;
    }
  if (length>3 && !value[0] && !value[1] && !value[2] && value[3] == ' ')
    {
      tmp[0] = '\\';
      tmp[1] = ' ';
      put_stringbuf_mem (sb, tmp, 2);
      length -= 4;
    }

  for (s=value, n=0;;)
    {
      for (value = s; n+3 < length
             && !s[0] && !s[1] && !s[2] && !(s[3] & 0x80); n += 4, s += 4)
        ;
      if (s != value)
        append_quoted (sb, value, s-value, 3);
      if (n>=length)
        return; /* ready */
      if (n < 4)
        { /* This is an invalid encoding - better stop after adding
             one impossible characater */
          put_stringbuf_mem (sb, "\xff", 1);
          return;
        }
      c = *s++ << 24;
      c |= *s++ << 16;
      c |= *s++ << 8;
      c |= *s++;
      n += 4;
      i=0;
      if (c < (1<<11))
        {
          tmp[i++] = 0xc0 | ( c >>  6);
          tmp[i++] = 0x80 | ( c        & 0x3f);
        }
      else if (c < (1<<16))
        {
          tmp[i++] = 0xe0 | ( c >> 12);
          tmp[i++] = 0x80 | ((c >>  6) & 0x3f);
          tmp[i++] = 0x80 | ( c        & 0x3f);
        }
      else if (c < (1<<21))
        {
          tmp[i++] = 0xf0 | ( c >> 18);
          tmp[i++] = 0x80 | ((c >> 12) & 0x3f);
          tmp[i++] = 0x80 | ((c >>  6) & 0x3f);
          tmp[i++] = 0x80 | ( c        & 0x3f);
        }
      else if (c < (1<<26))
        {
          tmp[i++] = 0xf8 | ( c >> 24);
          tmp[i++] = 0x80 | ((c >> 18) & 0x3f);
          tmp[i++] = 0x80 | ((c >> 12) & 0x3f);
          tmp[i++] = 0x80 | ((c >>  6) & 0x3f);
          tmp[i++] = 0x80 | ( c        & 0x3f);
        }
      else 
        {
          tmp[i++] = 0xfc | ( c >> 30);
          tmp[i++] = 0x80 | ((c >> 24) & 0x3f);
          tmp[i++] = 0x80 | ((c >> 18) & 0x3f);
          tmp[i++] = 0x80 | ((c >> 12) & 0x3f);
          tmp[i++] = 0x80 | ((c >>  6) & 0x3f);
          tmp[i++] = 0x80 | ( c        & 0x3f);
        }
      put_stringbuf_mem (sb, tmp, i);
    }
}

/* Append VALUE of LENGTH and TYPE to SB.  Do UCS-2 to utf conversion
   and and quoting */
static void
append_ucs2_value (const unsigned char *value, size_t length,
                   struct stringbuf *sb)
{
  unsigned char tmp[3];
  const unsigned char *s;
  size_t n;
  unsigned int c;
  int i;

  if (length>1 && !value[0] && (value[1] == ' ' || value[1] == '#'))
    {
      tmp[0] = '\\';
      tmp[1] = *value;
      put_stringbuf_mem (sb, tmp, 2);
      value += 2;
      length -= 2;
    }
  if (length>1 && !value[0] && value[1] == ' ')
    {
      tmp[0] = '\\';
      tmp[1] = ' ';
      put_stringbuf_mem (sb, tmp, 2);
      length -=2;
    }

  for (s=value, n=0;;)
    {
      for (value = s; n+1 < length && !s[0] && !(s[1] & 0x80); n += 2, s += 2)
        ;
      if (s != value)
        append_quoted (sb, value, s-value, 1);
      if (n>=length)
        return; /* ready */
      if (n < 2)
        { /* This is an invalid encoding - better stop after adding
             one impossible characater */
          put_stringbuf_mem (sb, "\xff", 1);
          return;
        }
      c  = *s++ << 8;
      c |= *s++;
      n += 2;
      i=0;
      if (c < (1<<11))
        {
          tmp[i++] = 0xc0 | ( c >>  6);
          tmp[i++] = 0x80 | ( c        & 0x3f);
        }
      else 
        {
          tmp[i++] = 0xe0 | ( c >> 12);
          tmp[i++] = 0x80 | ((c >>  6) & 0x3f);
          tmp[i++] = 0x80 | ( c        & 0x3f);
        }
      put_stringbuf_mem (sb, tmp, i);
    }
}


/* Append attribute and value.  ROOT is the sequence */
static gpg_error_t
append_atv (const unsigned char *image, AsnNode root, struct stringbuf *sb)
{
  AsnNode node = root->down;
  const char *name;
  int use_hex = 0;
  int i;
  
  if (!node || node->type != TYPE_OBJECT_ID)
    return gpg_error (GPG_ERR_UNEXPECTED_TAG);
  if (node->off == -1)
    return gpg_error (GPG_ERR_NO_VALUE); /* Hmmm, this might lead to misunderstandings */

  name = NULL;
  for (i=0; oid_name_tbl[i].name; i++)
    {
      if (oid_name_tbl[i].source == 1
          && node->len == oid_name_tbl[i].oidlen
          && !memcmp (image+node->off+node->nhdr,
                      oid_name_tbl[i].oid, node->len))
        {
          name = oid_name_tbl[i].name;
          break;
        }
    }
  if (name)
    put_stringbuf (sb, name);
  else
    { /* No name for the OID in the table; at least not DER encoded.
         Now convert the OID to a string, try to find it in the table
         again and use the string as last resort.  */
      char *p;

      p = ksba_oid_to_str (image+node->off+node->nhdr, node->len);
      if (!p)
        return gpg_error (GPG_ERR_ENOMEM);

      for (i=0; *p && oid_name_tbl[i].name; i++)
        {
          if (oid_name_tbl[i].source == 1 
              && !strcmp (p, oid_name_tbl[i].oidstr))
            {
              name = oid_name_tbl[i].name;
              break;
            }
        }
      if (name)
        put_stringbuf (sb, name);
      else
        {
          put_stringbuf (sb, p);
          use_hex = 1;
        }
      xfree (p);
    }
  put_stringbuf (sb, "=");
  node = node->right;
  if (!node || node->off == -1)
    return gpg_error (GPG_ERR_NO_VALUE);

  switch (use_hex? 0 : node->type)
    {
    case TYPE_UTF8_STRING:
      append_utf8_value (image+node->off+node->nhdr, node->len, sb);
      break;
    case TYPE_PRINTABLE_STRING:
    case TYPE_IA5_STRING:
      /* we assume that wrong encodings are latin-1 */
    case TYPE_TELETEX_STRING: /* Not correct, but mostly used as latin-1 */
      append_latin1_value (image+node->off+node->nhdr, node->len, sb);
      break;

    case TYPE_UNIVERSAL_STRING: 
      append_ucs4_value (image+node->off+node->nhdr, node->len, sb);
      break;

    case TYPE_BMP_STRING: 
      append_ucs2_value (image+node->off+node->nhdr, node->len, sb);
      break;

    case 0: /* forced usage of hex */
    default:
      put_stringbuf (sb, "#");
      for (i=0; i < node->len; i++)
        { 
          char tmp[3];
          sprintf (tmp, "%02X", image[node->off+node->nhdr+i]);
          put_stringbuf (sb, tmp);
        }
      break;
    }

  return 0;
}

static gpg_error_t
dn_to_str (const unsigned char *image, AsnNode root, struct stringbuf *sb)
{
  gpg_error_t err;
  AsnNode nset;

  if (!root )
    return 0; /* empty DN */
  nset = root->down;
  if (!nset)
    return 0; /* consider this as empty */
  if (nset->type != TYPE_SET_OF)
    return gpg_error (GPG_ERR_UNEXPECTED_TAG);

  /* output in reverse order */
  while (nset->right)
    nset = nset->right;

  for (;;)
    {
      AsnNode nseq;

      if (nset->type != TYPE_SET_OF)
        return gpg_error (GPG_ERR_UNEXPECTED_TAG);
      for (nseq = nset->down; nseq; nseq = nseq->right)
        {
          if (nseq->type != TYPE_SEQUENCE)
            return gpg_error (GPG_ERR_UNEXPECTED_TAG);
          if (nseq != nset->down)
            put_stringbuf (sb, "+");
          err = append_atv (image, nseq, sb);
          if (err)
            return err;
        }
      if (nset == root->down)
        break;
      put_stringbuf (sb, ",");
      nset = nset->left;
    }
      
  return 0;
}


gpg_error_t
_ksba_dn_to_str (const unsigned char *image, AsnNode node, char **r_string)
{
  gpg_error_t err;
  struct stringbuf sb;

  *r_string = NULL;
  if (!node || node->type != TYPE_SEQUENCE_OF)
    return gpg_error (GPG_ERR_INV_VALUE);

  init_stringbuf (&sb, 100);
  err = dn_to_str (image, node, &sb);
  if (!err)
    {
      *r_string = get_stringbuf (&sb);
      if (!*r_string)
        err = gpg_error (GPG_ERR_ENOMEM);
    }
  deinit_stringbuf (&sb);

  return err;
}


/* Create a new decoder and run it for the given element */
/* Fixme: this code is duplicated from cms-parser.c */
static gpg_error_t
create_and_run_decoder (ksba_reader_t reader, const char *elem_name,
                        AsnNode *r_root,
                        unsigned char **r_image, size_t *r_imagelen)
{
  gpg_error_t err;
  ksba_asn_tree_t crl_tree;
  BerDecoder decoder;

  err = ksba_asn_create_tree ("tmttv2", &crl_tree);
  if (err)
    return err;

  decoder = _ksba_ber_decoder_new ();
  if (!decoder)
    {
      ksba_asn_tree_release (crl_tree);
      return gpg_error (GPG_ERR_ENOMEM);
    }

  err = _ksba_ber_decoder_set_reader (decoder, reader);
  if (err)
    {
      ksba_asn_tree_release (crl_tree);
      _ksba_ber_decoder_release (decoder);
      return err;
    }

  err = _ksba_ber_decoder_set_module (decoder, crl_tree);
  if (err)
    {
      ksba_asn_tree_release (crl_tree);
      _ksba_ber_decoder_release (decoder);
      return err;
    }
  
  err = _ksba_ber_decoder_decode (decoder, elem_name, 0,
                                  r_root, r_image, r_imagelen);
  
  _ksba_ber_decoder_release (decoder);
  ksba_asn_tree_release (crl_tree);
  return err;
}


gpg_error_t
_ksba_derdn_to_str (const unsigned char *der, size_t derlen, char **r_string)
{
  gpg_error_t err;
  AsnNode root; 
  unsigned char *image;
  size_t imagelen;
  ksba_reader_t reader;

  err = ksba_reader_new (&reader);
  if (err)
    return err;
  err = ksba_reader_set_mem (reader, der, derlen);
  if (err)
    {
      ksba_reader_release (reader);
      return err;
    }
  err = create_and_run_decoder (reader, 
                                "TMTTv2.CertificateList.tbsCertList.issuer",
                                &root, &image, &imagelen);
  ksba_reader_release (reader);
  if (!err)
    {
      err = _ksba_dn_to_str (image, root->down, r_string);
      _ksba_asn_release_nodes (root);
      xfree (image);
    }
  return err;
}


/*
   Convert a string back to DN 
*/

/* Count the number of bytes in a quoted string, return a pointer to
   the character after the string or NULL in case of an paring error.
   The number of bytes needed to store the string verbatim will be
   return as RESULT.  With V2COMAP true, the string is assumed to be 
   in v2 quoting (but w/o the leading quote character)
 */
static const char *
count_quoted_string (const char *string, size_t *result,
                     int v2compat, int *stringtype)
{
  const unsigned char *s;
  int nbytes = 0;
  int highbit = 0;
  int nonprint = 0;
  int atsign = 0;

  *stringtype = 0;
  for (s=string; *s; s++)
    {
      if (*s == '\\')
        { /* pair */
          s++;
          if (*s == ',' || *s == '=' || *s == '+'
              || *s == '<' || *s == '>' || *s == '#' || *s == ';' 
              || *s == '\\' || *s == '\"' || *s == ' ')
            {
              if (!charclasses[*s])
                nonprint = 1;
              nbytes++;
            }
          else if (hexdigitp (s) && hexdigitp (s+1))
            {
              int c = xtoi_2 (s);
              if ((c & 0x80))
                highbit = 1;
              else if (c == '@')
                atsign = 1;
              else if (!charclasses[c])
                nonprint = 1;

              s++;
              nbytes++;
            }
          else
            return NULL; /* invalid escape sequence */
        }
      else if (*s == '\"')
        {
          if (v2compat)
            break;
          return NULL; /* invalid encoding */
        }
      else if (!v2compat
               && (*s == ',' || *s == '=' || *s == '+'
                   || *s == '<' || *s == '>' || *s == '#' || *s == ';') )
        {
          break; 
        }
      else
        {
          nbytes++;
          if ((*s & 0x80))
            highbit = 1;
          else if (*s == '@')
            atsign = 1;
          else if (!charclasses[*s])
            nonprint = 1;
        }
    }

  /* Fixme: Should be remove spaces or white spces from the end unless
     they are not escaped or we are in v2compat mode?  See TODO */

  if (highbit || nonprint)
    *stringtype = TYPE_UTF8_STRING;
  else if (atsign)
    *stringtype = TYPE_IA5_STRING;
  else 
    *stringtype = TYPE_PRINTABLE_STRING;

  *result = nbytes;
  return s;
}


/* Write out the data to W and do the required escaping.  Note that
   NBYTES is the number of bytes actually to be written, i.e. it is
   the result from count_quoted_string */
static gpg_error_t
write_escaped (ksba_writer_t w, const unsigned char *buffer, size_t nbytes)
{
  const unsigned char *s;
  gpg_error_t err;

  for (s=buffer; nbytes; s++)
    {
      if (*s == '\\')
        { 
          s++;
          if (hexdigitp (s) && hexdigitp (s+1))
            {
              unsigned char buf = xtoi_2 (s);
              err = ksba_writer_write (w, &buf, 1);
              if (err)
                return err;
              s++;
              nbytes--;
            }
          else
            {
              err = ksba_writer_write (w, s, 1);
              if (err)
                return err;
              nbytes--;
            }
        }
      else
        {
          err = ksba_writer_write (w, s, 1);
          if (err)
            return err;
          nbytes--;
        }
    }

  return 0;
}


/* Parse one RDN, and write it to WRITER.  Returns a pointer to the
   next RDN part where the comma has already been skipped or NULL in
   case of an error.  When NULL is passed as WRITER, the function does
   not allocate any memory but just parses the string and returns the
   ENDP. If ROFF or RLEN are not NULL, they will receive informaion
   useful for error reporting. */
static gpg_error_t
parse_rdn (const unsigned char *string, const char **endp, 
           ksba_writer_t writer, size_t *roff, size_t *rlen)
{
  const unsigned char *orig_string = string;
  const unsigned char *s, *s1;
  size_t n, n1;
  int i;
  unsigned char *p;
  unsigned char *oidbuf = NULL;
  unsigned char *valuebuf = NULL;
  const unsigned char *oid = NULL;
  size_t oidlen;
  const unsigned char *value = NULL;
  int valuelen;
  int valuetype;
  int need_escaping = 0;
  gpg_error_t err = 0;
  size_t dummy_roff, dummy_rlen;

  if (!roff)
    roff = &dummy_roff;
  if (!rlen)
    rlen = &dummy_rlen;

  *roff = *rlen = 0;

  if (!string)
    return gpg_error (GPG_ERR_INV_VALUE);
  while (*string == ' ')
    string++;
  *roff = string - orig_string;
  if (!*string)
    return gpg_error (GPG_ERR_SYNTAX); /* empty elements are not allowed */
  s = string;
  
  if ( ((*s == 'o' && s[1] == 'i' && s[2] == 'd')
        ||(*s == 'O' && s[1] == 'I' && s[2] == 'D'))
       && s[3] == '.' && digitp (s+4))
    s += 3; /* skip a prefixed oid */

  /* parse attributeType */
  string = s;
  *roff = string - orig_string;
  if (digitp (s))
    { /* oid */
      for (s++; digitp (s) || (*s == '.' && s[1] != '.'); s++)
        ;
      n = s - string;
      while (*s == ' ')
        s++;
      if (*s != '=')
        return gpg_error (GPG_ERR_SYNTAX);

      if (writer)
        {
          p = xtrymalloc (n+1);
          if (!p)
            return gpg_error (GPG_ERR_ENOMEM);
          memcpy (p, string, n);
          p[n] = 0;
          err = ksba_oid_from_str (p, &oidbuf, &oidlen);
          xfree (p);
          if (err)
            return err;
          oid = oidbuf;
        }
    }
  else if ((*s >= 'A' && *s <= 'Z') || (*s >= 'a' && *s <= 'z') )
    { /* name */
      for (s++; ((*s >= 'A' && *s <= 'Z') || (*s >= 'a' && *s <= 'z')
                 || digitp (s) || *s == '-'); s++)
        ;
      n = s - string;
      while (*s == ' ')
        s++;
      if (*s != '=')
        return gpg_error (GPG_ERR_SYNTAX); 
      
      for (i=0; oid_name_tbl[i].name; i++)
        {
          if ( n == strlen (oid_name_tbl[i].name)
               && !ascii_memcasecmp (string, oid_name_tbl[i].name, n))
            break;
        }
      if (!oid_name_tbl[i].name)
        {
          *roff = string - orig_string;
          *rlen = n;
          return gpg_error (GPG_ERR_UNKNOWN_NAME);
        }
      oid = oid_name_tbl[i].oid;
      oidlen = oid_name_tbl[i].oidlen;
    }
  s++;
  while (*s == ' ')
    s++;
  string = s;

  *roff = string - orig_string;

  /* parse attributeValue */
  if (!*s)
    {
      err = gpg_error (GPG_ERR_SYNTAX); /* missing value */
      goto leave;
    }

  if (*s == '#')
    { /* hexstring */
      int need_utf8 = 0;
      int need_ia5 = 0;

      string = ++s;
      for (; hexdigitp (s); s++)
        s++;
      n = s - string;
      if (!n || (n & 1))
        {
          *rlen = n;
          err = gpg_error (GPG_ERR_SYNTAX); /* no hex digits or odd number */
          goto leave;
        }
      while (*s == ' ')
        s++;
      n /= 2;
      valuelen = n;
      if (writer)
        {
          valuebuf = xtrymalloc (valuelen);
          if (!valuebuf)
            {
              err = gpg_error (GPG_ERR_ENOMEM);
              goto leave;
            }
          for (p=valuebuf, s1=string; n; p++, s1 += 2, n--)
            {
              *p = xtoi_2 (s1);
              if (*p == '@')
                need_ia5 = 1;
              else if ((*p & 0x80) || !charclasses[*p])
                need_utf8 = 1;
            }
          value = valuebuf;
        }
      else
        {
          for (s1=string; n; s1 += 2, n--)
            {
              unsigned int c;

              c = xtoi_2 (s1);
              if (c == '@')
                need_ia5 = 1;
              else if ((c & 0x80) || !charclasses[c])
                need_utf8 = 1;
            }
        }
      valuetype = need_utf8? TYPE_UTF8_STRING :
                  need_ia5 ? TYPE_IA5_STRING : TYPE_PRINTABLE_STRING;
    }
  else if (*s == '\"')
    { /* old style quotation */
      string = s+1;
      s = count_quoted_string (string, &n, 1, &valuetype);
      if (!s || *s != '\"')
        {
          *rlen = s - orig_string;
          err = gpg_error (GPG_ERR_SYNTAX); /* error or quote not closed */
          goto leave;
        }
      s++;
      while (*s == ' ')
        s++;
      value = string;
      valuelen = n;
      need_escaping = 1;
    }
  else
    { /* regular v3 quoted string */
      s = count_quoted_string (string, &n, 0, &valuetype);
      if (!s)
        {
          err = gpg_error (GPG_ERR_SYNTAX); /* error */
          goto leave;
        }
      while (*s == ' ')
        s++;
      value = string;
      valuelen = n;
      need_escaping = 1;
    }

  if (!valuelen)
    {
      err = gpg_error (GPG_ERR_SYNTAX); /* empty elements are not allowed */
      goto leave;
    }
  if ( *s && *s != ',' && *s != ';' && *s != '+')
    {
      *roff = s - orig_string;
      err = gpg_error (GPG_ERR_SYNTAX); /* invalid delimiter */
      goto leave;
    }
  if (*s == '+') /* fixme: implement this */
    {
      *roff = s - orig_string;
      *rlen = 1;
      err = gpg_error (GPG_ERR_NOT_IMPLEMENTED); 
      goto leave;
    }
  *endp = *s? (s+1):s;
  
  if (writer)
    { /* write out the data */

      /* need to calculate the length in advance */
      n1  = _ksba_ber_count_tl (TYPE_OBJECT_ID, CLASS_UNIVERSAL, 0, oidlen);
      n1 += oidlen;
      n1 += _ksba_ber_count_tl (valuetype, CLASS_UNIVERSAL, 0, valuelen);
      n1 += valuelen;
      
      /* The SET tag */
      n  = _ksba_ber_count_tl (TYPE_SET, CLASS_UNIVERSAL, 1, n);
      n += n1;
      err = _ksba_ber_write_tl (writer, TYPE_SET, CLASS_UNIVERSAL, 1, n);
      
      /* The sequence tag */
      n = n1;
      err = _ksba_ber_write_tl (writer, TYPE_SEQUENCE, CLASS_UNIVERSAL, 1, n);
      
      /* the OBJECT ID */
      err = _ksba_ber_write_tl (writer, TYPE_OBJECT_ID, CLASS_UNIVERSAL,
                                0, oidlen);
      if (!err)
        err = ksba_writer_write (writer, oid, oidlen);
      if (err)
        goto leave;
      
      /* the value.  Note that we don't need any conversion to the target
         characters set because the input is expected to be utf8 and the
         target type is either utf8, IA5 or printable string where the last
         two are subsets of utf8 */
      err = _ksba_ber_write_tl (writer, valuetype,
                                CLASS_UNIVERSAL, 0, valuelen);
      if (!err)
        err = need_escaping? write_escaped (writer, value, valuelen)
          : ksba_writer_write (writer, value, valuelen);
    }
  
 leave:
  xfree (oidbuf);
  xfree (valuebuf);
  return err;
}



gpg_error_t
_ksba_dn_from_str (const char *string, char **rbuf, size_t *rlength)
{
  gpg_error_t err;
  ksba_writer_t writer;
  const char *s, *endp;
  void *buf = NULL;
  size_t buflen;
  char const **part_array = NULL;
  int part_array_size, nparts;

  *rbuf = NULL; *rlength = 0;
  /* We are going to build the object using a writer object.  */
  err = ksba_writer_new (&writer);
  if (!err)
    err = ksba_writer_set_mem (writer, 1024);
  if (err)
    return err;

  /* We must assign it in reverse order so we do it in 2 passes. */
  part_array_size = 0;
  for (nparts=0, s=string; s && *s;)
    {
      err = parse_rdn (s, &endp, NULL, NULL, NULL);
      if (err)
        goto leave;
      if (nparts >= part_array_size)
        {
          char const **tmp;

          part_array_size += 2;
          tmp = part_array_size? xtryrealloc (part_array,
                                              part_array_size * sizeof *tmp)
                                : xtrymalloc (part_array_size * sizeof *tmp);
          if (!tmp)
            {
              err = gpg_error (GPG_ERR_ENOMEM);
              goto leave;
            }
          part_array = tmp;
        }
      part_array[nparts++] = s;
      s = endp;
    }
  if (!nparts)
    {
      err = gpg_error (GPG_ERR_SYNTAX);
      goto leave;
    }

  while (--nparts >= 0)
    {
      err = parse_rdn (part_array[nparts], &endp, writer, NULL, NULL);
      if (err)
        goto leave;
    }

  /* Now get the memory.  */
  buf = ksba_writer_snatch_mem (writer, &buflen);
  if (!buf)
    {
      err = gpg_error (GPG_ERR_ENOMEM);
      goto leave;
    }
  /* Reinitialize the buffer to create the outer sequence.  */
  err = ksba_writer_set_mem (writer, buflen + 10);
  if (err)
    goto leave;
  
  /* write the outer sequence */
  err = _ksba_ber_write_tl (writer, TYPE_SEQUENCE,
                            CLASS_UNIVERSAL, 1, buflen);
  if (err)
    goto leave;
  /* write the collected sets */
  err = ksba_writer_write (writer, buf, buflen);
  if (err)
    goto leave;

  /* and get the result */
  *rbuf = ksba_writer_snatch_mem (writer, rlength);
  if (!*rbuf)
    {
      err = gpg_error (GPG_ERR_ENOMEM);
      goto leave;
    }
  
 leave:
  xfree (part_array);
  ksba_writer_release (writer);
  xfree (buf);
  return err;
}



gpg_error_t
ksba_dn_der2str (const void *der, size_t derlen, char **rstring)
{
  return _ksba_derdn_to_str (der, derlen, rstring);
}


gpg_error_t
ksba_dn_str2der (const char *string, unsigned char **rder, size_t *rderlen)
{
  return _ksba_dn_from_str (string, (char**)rder, rderlen);
}



/* Assuming that STRING contains an rfc2253 encoded string, test
   whether this string may be passed as a valid DN to libksba.  On
   success the functions returns 0.  On error the function returns an
   error code and stores the offset within STRING of the erroneous
   part at RERROFF. RERRLEN will then receive the length of the
   erroneous part.  This function is most useful to test whether a
   symbolic name (like SN) is supported. SEQ should be passed as 0 for
   now.  RERROFF and RERRLEN may be passed as NULL if the caller is
   not interested at this value. */
gpg_error_t 
ksba_dn_teststr (const char *string, int seq, 
                 size_t *rerroff, size_t *rerrlen)
{
  size_t dummy_erroff, dummy_errlen;
  gpg_error_t err;
  int nparts;
  const char *s, *endp;
  size_t off, len;

  if (!rerroff)
    rerroff = &dummy_erroff;
  if (!rerrlen)
    rerrlen = &dummy_errlen;

  *rerrlen = *rerroff = 0;

  for (nparts=0, s=string; s && *s; nparts++)
    {
      err = parse_rdn (s, &endp, NULL, &off, &len);
      if (err && !seq--)
        {
          *rerroff = s - string + off;
          *rerrlen = len? len : strlen (s);
          return err;
        }
      s = endp;
    }
  if (!nparts)
    return gpg_error (GPG_ERR_SYNTAX);
  return 0;
}
