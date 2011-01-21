/* oid.c - Object identifier helper functions
 *      Copyright (C) 2001, 2009 g10 Code GmbH
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

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "util.h"
#include "asn1-func.h"
#include "convert.h"



/**
 * ksba_oid_to_str:
 * @buffer: A BER encoded OID
 * @length: The length of this OID
 * 
 * Take a buffer with an object identifier in BER encoding and return
 * a string representing this OID.  We do not use the ASN.1 syntax
 * here but delimit the arcs with dots, so it is easier to parse in
 * most cases.  This dotted-decimal notation is also known as LDAPOID
 * string and described in RFC-2251.
 *
 * The function returns an empty string for an empty buffer and does
 * no interpretation of the OID.  The caller must free the returned
 * string using ksba_free() or the function he has registered as a
 * replacement.
 * 
 *
 * Return value: A allocated string or NULL in case of memory problem.
 **/
char *
ksba_oid_to_str (const char *buffer, size_t length)
{
  const unsigned char *buf = buffer;
  char *string, *p;
  int n = 0;
  unsigned long val, valmask;

  valmask = (unsigned long)0xfe << (8 * (sizeof (valmask) - 1));

  /* To calculate the length of the string we can safely assume an
     upper limit of 3 decimal characters per byte.  Two extra bytes
     account for the special first octect */
  string = p = xtrymalloc (length*(1+3)+2+1);
  if (!string)
    return NULL;
  if (!length)
    {
      *p = 0;
      return string;
    }

  if (buf[0] < 40)
    p += sprintf (p, "0.%d", buf[n]);
  else if (buf[0] < 80)
    p += sprintf (p, "1.%d", buf[n]-40);
  else {
    val = buf[n] & 0x7f;
    while ( (buf[n]&0x80) && ++n < length )
      {
        if ( (val & valmask) )
          goto badoid;  /* Overflow.  */
        val <<= 7;
        val |= buf[n] & 0x7f;
      }
    val -= 80;
    sprintf (p, "2.%lu", val);
    p += strlen (p);
  }
  for (n++; n < length; n++)
    {
      val = buf[n] & 0x7f;
      while ( (buf[n]&0x80) && ++n < length )
        {
          if ( (val & valmask) )
            goto badoid;  /* Overflow.  */
          val <<= 7;
          val |= buf[n] & 0x7f;
        }
      sprintf (p, ".%lu", val);
      p += strlen (p);
    }
    
  *p = 0;
  return string;

 badoid:
  /* Return a special OID (gnu.gnupg.badoid) to indicate the error
     case.  The OID is broken and thus we return one which can't do
     any harm.  Formally this does not need to be a bad OID but an OID
     with an arc that can't be represented in a 32 bit word is more
     than likely corrupt.  */
  xfree (string);
  return xtrystrdup ("1.3.6.1.4.1.11591.2.12242973"); 
}


/* Take the OID at NODE and return it in string format */
char *
_ksba_oid_node_to_str (const unsigned char *image, AsnNode node) 
{
  if (!node || node->type != TYPE_OBJECT_ID || node->off == -1)
    return NULL;
  return ksba_oid_to_str (image+node->off + node->nhdr, node->len);
}



static size_t
make_flagged_int (unsigned long value, char *buf, size_t buflen)
{
  int more = 0;
  int shift;

  /* fixme: figure out the number of bits in an ulong and start with
     that value as shift (after making it a multiple of 7) a more
     straigtforward implementation is to do it in reverse order using
     a temporary buffer - saves a lot of compares */
  for (more=0, shift=28; shift > 0; shift -= 7)
    {
      if (more || value >= (1<<shift))
        {
          buf[buflen++] = 0x80 | (value >> shift);
          value -= (value >> shift) << shift;
          more = 1;
        }
    }
  buf[buflen++] = value;
  return buflen;
}


/**
 * ksba_oid_from_str:
 * @string: A string with the OID in dotted decimal form
 * @rbuf:   Returns the DER encoded OID
 * @rlength: and its length
 * 
 * Convertes the OID given in dotted decimal form to an DER encoding
 * and returns it in allocated buffer rbuf and its length in rlength.
 * rbuf is set to NULL in case of an error is returned.
 * Scanning stops at the first white space.

 * The caller must free the returned buffer using ksba_free() or the
 * function he has registered as a replacement.
 * 
 * Return value: 0 on success or an error value
 **/
gpg_error_t
ksba_oid_from_str (const char *string, unsigned char **rbuf, size_t *rlength)
{
  unsigned char *buf;
  size_t buflen;
  unsigned long val1, val;
  const char *endp;
  int arcno;

  if (!string || !rbuf || !rlength)
    return gpg_error (GPG_ERR_INV_VALUE);
  *rbuf = NULL;
  *rlength = 0;

  /* we allow the OID to be prefixed with either "oid." or "OID." */
  if ( !strncmp (string, "oid.", 4) || !strncmp (string, "OID.", 4))
    string += 4;

  if (!*string)
    return gpg_error (GPG_ERR_INV_VALUE);

  /* we can safely assume that the encoded OID is shorter than the string */
  buf = xtrymalloc ( strlen(string) + 2);
  if (!buf)
    return gpg_error (GPG_ERR_ENOMEM);
  buflen = 0;

  val1 = 0; /* avoid compiler warnings */
  arcno = 0;
  do {
    arcno++;
    val = strtoul (string, (char**)&endp, 10);
    if (!digitp (string) || !(*endp == '.' || !*endp))
      {
        xfree (buf);
        return gpg_error (GPG_ERR_INV_OID_STRING);
      }
    if (*endp == '.')
      string = endp+1;

    if (arcno == 1)
      {
        if (val > 2)
          break; /* not allowed, error catched below */
        val1 = val;
      }
    else if (arcno == 2)
      { /* need to combine the first to arcs in one octet */
        if (val1 < 2)
          {
            if (val > 39)
              {
                xfree (buf);
                return gpg_error (GPG_ERR_INV_OID_STRING);
              }
            buf[buflen++] = val1*40 + val;
          }
        else
          {
            val += 80;
            buflen = make_flagged_int (val, buf, buflen);
          }
      }
    else
      {
        buflen = make_flagged_int (val, buf, buflen);
      }
  } while (*endp == '.');

  if (arcno == 1)
    { /* it is not possible to encode only the first arc */
      xfree (buf);
      return gpg_error (GPG_ERR_INV_OID_STRING);
    }

  *rbuf = buf;
  *rlength = buflen;
  return 0; 
}


/* Convert the string in BUFFER which is of length BUFLEN to its DER
   encoding and returns it in a new allocated buffer RBUF and its
   length in RLENGTH.  RBUF is set to NULL if an error is returned.
   The caller must free the returned buffer using ksba_free() or the
   function he has registered as a replacement.  */
gpg_error_t
_ksba_oid_from_buf (const void *buffer, size_t buflen,
                    unsigned char **rbuf, size_t *rlength)
{
  gpg_error_t err;
  char *string;

  string = xtrymalloc (buflen+1);
  if (!string)
    {
      *rbuf = NULL;
      *rlength = 0;
      return gpg_error_from_syserror ();
    }
  memcpy (string, buffer, buflen);
  string[buflen] = 0;
  err = ksba_oid_from_str (string, rbuf, rlength);
  xfree (string);
  return err;
}
