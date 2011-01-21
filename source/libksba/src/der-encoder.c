/* der-decoder.c - Distinguished Encoding Rules Encoder
 *      Copyright (C) 2001, 2004, 2008 g10 Code GmbH
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

#include "ksba.h"
#include "asn1-func.h"
#include "ber-help.h"
#include "der-encoder.h"
#include "convert.h"

struct der_encoder_s {
  AsnNode module;    /* the ASN.1 structure */
  ksba_writer_t writer;
  const char *last_errdesc; /* string with the error description */
  AsnNode root;   /* of the expanded parse tree */
  int debug;
};

/* To be useful for the DER encoder we store all data direct as the
   binary image, so we use the VALTYPE_MEM */
static gpg_error_t
store_value (AsnNode node, const void *buffer, size_t length)
{
  _ksba_asn_set_value (node, VALTYPE_MEM, buffer, length);
  return 0;
}

static void
clear_value (AsnNode node)
{
  _ksba_asn_set_value (node, VALTYPE_NULL, NULL, 0);
}




DerEncoder
_ksba_der_encoder_new (void)
{
  DerEncoder d;

  d = xtrycalloc (1, sizeof *d);
  if (!d)
    return NULL;

  return d;
}

void
_ksba_der_encoder_release (DerEncoder d)
{
  xfree (d);
}


/**
 * _ksba_der_encoder_set_module:
 * @d: Decoder object 
 * @module: ASN.1 Parse tree
 * 
 * Initialize the decoder with the ASN.1 module.  Note, that this is a
 * shallow copy of the module.  Fixme: What about ref-counting of
 * AsnNodes?
 * 
 * Return value: 0 on success or an error code
 **/
gpg_error_t
_ksba_der_encoder_set_module (DerEncoder d, ksba_asn_tree_t module)
{
  if (!d || !module)
    return gpg_error (GPG_ERR_INV_VALUE);
  if (d->module)
    return gpg_error (GPG_ERR_CONFLICT); /* module already set */

  d->module = module->parse_tree;
  return 0;
}


gpg_error_t
_ksba_der_encoder_set_writer (DerEncoder d, ksba_writer_t w)
{
  if (!d || !w)
    return gpg_error (GPG_ERR_INV_VALUE);
  if (d->writer)
    return gpg_error (GPG_ERR_CONFLICT); /* reader already set */
  
  d->writer = w;
  return 0;
}



/*
  Helpers to construct and write out objects
*/


/* Create and write a

  AlgorithmIdentifier ::= SEQUENCE {
      algorithm    OBJECT IDENTIFIER,
      parameters   ANY DEFINED BY algorithm OPTIONAL 
  }

  where parameters will be set to NULL if parm is NULL or to an octet
  string with the given parm.  As a special hack parameter will not be
  written if PARM is given but parmlen is 0.  */
gpg_error_t
_ksba_der_write_algorithm_identifier (ksba_writer_t w, const char *oid,
                                      const void *parm, size_t parmlen)
{
  gpg_error_t err;
  unsigned char *buf;
  size_t len;
  int no_null = (parm && !parmlen);

  err = ksba_oid_from_str (oid, &buf, &len);
  if (err)
    return err;

  /* write the sequence */
  /* fixme: the the length to encode the TLV values are actually not
     just 2 byte each but depend on the length of the values - for
     our purposes the static values do work.  */
  err = _ksba_ber_write_tl (w, TYPE_SEQUENCE, CLASS_UNIVERSAL, 1,
                            (no_null? 2:4) + len + (parm? parmlen:0));
  if (err)
    goto leave;

  /* the OBJECT ID header and the value */
  err = _ksba_ber_write_tl (w, TYPE_OBJECT_ID, CLASS_UNIVERSAL, 0, len);
  if (!err)
    err = ksba_writer_write (w, buf, len);
  if (err)
    goto leave;

  /* Write the parameter */
  if (no_null)
    ;
  else if (parm)
    {
      err = _ksba_ber_write_tl (w, TYPE_OCTET_STRING, CLASS_UNIVERSAL,
                                0, parmlen);
      if (!err)
        err = ksba_writer_write (w, parm, parmlen);
    }
  else
    {
      err = _ksba_ber_write_tl (w, TYPE_NULL, CLASS_UNIVERSAL, 0, 0);
    }

 leave:
  xfree (buf);
  return err;
}





/*************************************************
 ***  Copy data from a tree image to the tree  ***
 *************************************************/

/* Copy all values from the tree SRC (with values store in SRCIMAGE)
   to the tree DST */
gpg_error_t
_ksba_der_copy_tree (AsnNode dst_root,
                     AsnNode src_root, const unsigned char *src_image)
{
  AsnNode s, d;

  s = src_root;
  d = dst_root;
  /* note: we use the is_any flags becuase an inserted copy may have
     already changed the any tag to the actual type */
  while (s && d && (s->type == d->type || d->flags.is_any))
    {
      if (d->flags.is_any)
        d->type = s->type;

      if (s->flags.in_array && s->right)
        {
          if (!_ksba_asn_insert_copy (d))
            return gpg_error (GPG_ERR_ENOMEM);
        }

      if ( !_ksba_asn_is_primitive (s->type) )
        ;
      else if (s->off == -1)
        clear_value (d);
      else
        store_value (d, src_image + s->off + s->nhdr, s->len);

      s = _ksba_asn_walk_tree (src_root, s);
      d = _ksba_asn_walk_tree (dst_root, d);
    }

  if (s || d)
    {
/*        fputs ("ksba_der_copy_tree: trees don't match\nSOURCE TREE:\n", stderr); */
/*        _ksba_asn_node_dump_all (src_root, stderr); */
/*        fputs ("DESTINATION TREE:\n", stderr); */
/*        _ksba_asn_node_dump_all (dst_root, stderr); */
      return gpg_error (GPG_ERR_ENCODING_PROBLEM);
    }
  return 0;
}



/*********************************************
 ********** Store data in a tree *************
 *********************************************/


gpg_error_t
_ksba_der_store_time (AsnNode node, const ksba_isotime_t atime)
{
  char buf[50], *p;
  int need_gen;
  gpg_error_t err;

  /* First check that ATIME is indeed as formatted as expected. */
  err = _ksba_assert_time_format (atime);
  if (err)
    return err;

  memcpy (buf, atime, 8);
  memcpy (buf+8, atime+9, 6);
  strcpy (buf+14, "Z");

  /* We need to use generalized time beginning with the year 2050. */
  need_gen = (_ksba_cmp_time (atime, "20500101T000000") >= 0);

  if (node->type == TYPE_ANY)
    node->type = need_gen? TYPE_GENERALIZED_TIME : TYPE_UTC_TIME;
  else if (node->type == TYPE_CHOICE)
    { /* find a suitable choice to store the value */
      AsnNode n;

      for (n=node->down; n; n=n->right)
        {
          if ( (need_gen && n->type == TYPE_GENERALIZED_TIME)
               || (!need_gen && n->type == TYPE_UTC_TIME))
            {
              node = n;
              break;
            }
        }
    }
  
  if (node->type == TYPE_GENERALIZED_TIME
      || node->type == TYPE_UTC_TIME)
    {
      p = node->type == TYPE_UTC_TIME? (buf+2):buf;
      return store_value (node, p, strlen (p));
    }
  else
    return gpg_error (GPG_ERR_INV_VALUE);
}

/* Store the utf-8 STRING in NODE. */
gpg_error_t
_ksba_der_store_string (AsnNode node, const char *string)
{
  if (node->type == TYPE_CHOICE)
    {
      /* find a suitable choice to store the value */
    }


  if (node->type == TYPE_PRINTABLE_STRING)
    {
      return store_value (node, string, strlen (string));
    }
  else
    return gpg_error (GPG_ERR_INV_VALUE);
}


/* Store the integer VALUE in NODE.  VALUE is assumed to be a DER
   encoded integer prefixed with 4 bytes given its length in network
   byte order. */
gpg_error_t
_ksba_der_store_integer (AsnNode node, const unsigned char *value)
{
  if (node->type == TYPE_INTEGER)
    {
      size_t len;
      
      len = (value[0] << 24) | (value[1] << 16) | (value[2] << 8) | value[3];
      return store_value (node, value+4, len);
    }
  else
    return gpg_error (GPG_ERR_INV_VALUE);
}

gpg_error_t
_ksba_der_store_oid (AsnNode node, const char *oid)
{
  gpg_error_t err;

  if (node->type == TYPE_ANY)
    node->type = TYPE_OBJECT_ID;

  if (node->type == TYPE_OBJECT_ID)
    {
      unsigned char *buf;
      size_t len;

      err = ksba_oid_from_str (oid, &buf, &len);
      if (err)
        return err;
      err = store_value (node, buf, len);
      xfree (buf);
      return err;
    }
  else
    return gpg_error (GPG_ERR_INV_VALUE);
}


gpg_error_t
_ksba_der_store_octet_string (AsnNode node, const char *buf, size_t len)
{
  if (node->type == TYPE_ANY)
    node->type = TYPE_OCTET_STRING;

  if (node->type == TYPE_OCTET_STRING)
    {
      return store_value (node, buf, len);
    }
  else
    return gpg_error (GPG_ERR_INV_VALUE);
}


gpg_error_t
_ksba_der_store_sequence (AsnNode node, const unsigned char *buf, size_t len)
{
  if (node->type == TYPE_ANY)
    node->type = TYPE_PRE_SEQUENCE;

  if (node->type == TYPE_SEQUENCE || node->type == TYPE_PRE_SEQUENCE)
    {
      return store_value (node, buf, len);
    }
  else
    return gpg_error (GPG_ERR_INV_VALUE);
}


gpg_error_t
_ksba_der_store_null (AsnNode node)
{
  if (node->type == TYPE_ANY)
    node->type = TYPE_NULL;

  if (node->type == TYPE_NULL)
    {
      clear_value (node);
      return 0;
    }
  else
    return gpg_error (GPG_ERR_INV_VALUE);
}


/* 
   Actual DER encoder
*/

/* We have a value for this node.  Calculate the length of the header
   and store it in node->nhdr and store the length of the value in
   node->value. We assume that this is a primitive node and has a
   value of type VALTYPE_MEM. */
static void
set_nhdr_and_len (AsnNode node, unsigned long length)
{
  int buflen = 0;

  if (node->type == TYPE_SET_OF || node->type == TYPE_SEQUENCE_OF)
    buflen++;
  else if (node->type == TYPE_TAG)
    buflen++; 
  else if (node->type < 0x1f || node->type == TYPE_PRE_SEQUENCE)
    buflen++;
  else
    {
      never_reached ();
      /* Fixme: tags with values above 31 are not yet implemented */
    }

  if (!node->type /*&& !class*/)
    buflen++; /* end tag */
  else if (node->type == TYPE_NULL /*&& !class*/)
    buflen++; /* NULL tag */
  else if (!length)
    buflen++; /* indefinite length */
  else if (length < 128)
    buflen++; 
  else 
    {
      buflen += (length <= 0xff ? 2:
                 length <= 0xffff ? 3: 
                 length <= 0xffffff ? 4: 5);
    }        

  node->len = length;
  node->nhdr = buflen;
}

/* Like above but put now put it into buffer.  return the number of
   bytes copied.  There is no need to do length checking here */
static size_t
copy_nhdr_and_len (unsigned char *buffer, AsnNode node)
{
  unsigned char *p = buffer;
  int tag, class;
  unsigned long length;

  tag = node->type;
  class = CLASS_UNIVERSAL;
  length = node->len;

  if (tag == TYPE_SET_OF)
    tag = TYPE_SET;
  else if (tag == TYPE_SEQUENCE_OF)
    tag = TYPE_SEQUENCE;
  else if (tag == TYPE_PRE_SEQUENCE)
    tag = TYPE_SEQUENCE;
  else if (tag == TYPE_TAG)
    {
      class = CLASS_CONTEXT;  /* Hmmm: we no way to handle other classes */
      tag = node->value.v_ulong;
    }
  if (tag < 0x1f)
    {
      *p = (class << 6) | tag;
      if (!_ksba_asn_is_primitive (tag))
        *p |= 0x20;
      p++;
    }
  else
    {
      /* fixme: Not_Implemented*/
    }

  if (!tag && !class)
    *p++ = 0; /* end tag */
  else if (tag == TYPE_NULL && !class)
    *p++ = 0; /* NULL tag */
  else if (!length)
    *p++ = 0x80; /* indefinite length - can't happen! */
  else if (length < 128)
    *p++ = length; 
  else 
    {
      int i;

      /* fixme: if we know the sizeof an ulong we could support larger
         objects - however this is pretty ridiculous */
      i = (length <= 0xff ? 1:
           length <= 0xffff ? 2: 
           length <= 0xffffff ? 3: 4);
      
      *p++ = (0x80 | i);
      if (i > 3)
        *p++ = length >> 24;
      if (i > 2)
        *p++ = length >> 16;
      if (i > 1)
        *p++ = length >> 8;
      *p++ = length;
    }        

  return p - buffer;
}



static unsigned long
sum_up_lengths (AsnNode root)
{
  AsnNode n;
  unsigned long len = 0;

  if (root->type == TYPE_NULL)
    return root->nhdr;

  if (!(n=root->down) || _ksba_asn_is_primitive (root->type))
    len = root->len;
  else
    {
      for (; n; n = n->right)
        len += sum_up_lengths (n);
    }
  if ( !_ksba_asn_is_primitive (root->type)
       && root->type != TYPE_CHOICE
       && len
       && !root->flags.is_implicit)
    { /* this is a constructed one */
      set_nhdr_and_len (root, len);
    }

  return len? (len + root->nhdr):0;
}

/* Create a DER encoding from the value tree ROOT and return an
   allocated image of appropriate length in r_image and r_imagelen.
   The value tree is modified so that it can be used the same way as a
   parsed one, i.e the elements off, and len are set to point into
   image.  */
gpg_error_t
_ksba_der_encode_tree (AsnNode root,
                       unsigned char **r_image, size_t *r_imagelen)
{
  AsnNode n;
  unsigned char *image;
  size_t imagelen, len;

  /* clear out all fields */
  for (n=root; n ; n = _ksba_asn_walk_tree (root, n))
    {
      n->off = -1;
      n->len = 0;
      n->nhdr = 0;
    }
     
  /* Set default values */
  /* FIXME */

  /* calculate the length of the headers.  These are the tag and
     length fields of all primitive elements */
  for (n=root; n ; n = _ksba_asn_walk_tree (root, n))
    {
      if (_ksba_asn_is_primitive (n->type)
          && !n->flags.is_implicit
          && ((n->valuetype == VALTYPE_MEM && n->value.v_mem.len)
              || n->type == TYPE_NULL))
        set_nhdr_and_len (n, n->value.v_mem.len);
    }

  /* Now calculate the length of all constructed types */
  imagelen = sum_up_lengths (root);

#if 0
  /* set off to zero, so that it can be dumped */
  for (n=root; n ; n = _ksba_asn_walk_tree (root, n))
      n->off = 0;
  fputs ("DER encoded value Tree:\n", stderr); 
  _ksba_asn_node_dump_all (root, stderr); 
  for (n=root; n ; n = _ksba_asn_walk_tree (root, n))
      n->off = -1;
#endif
  
  /* now we can create an encoding in image */
  image = xtrymalloc (imagelen);
  if (!image)
    return gpg_error (GPG_ERR_ENOMEM);
  len = 0;
  for (n=root; n ; n = _ksba_asn_walk_tree (root, n))
    {
      size_t nbytes;

      if (!n->nhdr)
        continue;
      assert (n->off == -1);
      assert (len < imagelen);
      n->off = len;
      nbytes = copy_nhdr_and_len (image+len, n);
      len += nbytes;
      if ( _ksba_asn_is_primitive (n->type)
           && n->valuetype == VALTYPE_MEM
           && n->value.v_mem.len )
        {
          nbytes = n->value.v_mem.len;
          assert (len + nbytes <= imagelen);
          memcpy (image+len, n->value.v_mem.buf, nbytes);
          len += nbytes;
        }
    }

  assert (len == imagelen);

  *r_image = image;
  if (r_imagelen)
    *r_imagelen = imagelen;
  return 0;
}
