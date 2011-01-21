/* crl.c - CRL parser
 *      Copyright (C) 2002, 2004, 2005 g10 Code GmbH
 *
 * This file is part of KSBA.
 *
 * KSBA is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Fountion; either version 3 of the License, or
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
#include <errno.h>

#include "util.h"

#include "convert.h"
#include "keyinfo.h"
#include "der-encoder.h"
#include "ber-help.h"
#include "ber-decoder.h"
#include "crl.h"


static const char oidstr_crlNumber[] = "2.5.29.20";
static const char oidstr_crlReason[] = "2.5.29.21";
static const char oidstr_issuingDistributionPoint[] = "2.5.29.28";
static const char oidstr_certificateIssuer[] = "2.5.29.29";
static const char oidstr_authorityKeyIdentifier[] = "2.5.29.35";

/* We better buffer the hashing. */
static inline void
do_hash (ksba_crl_t crl, const void *buffer, size_t length)
{
  while (length)
    {
      size_t n = length;
      
      if (crl->hashbuf.used + n > sizeof crl->hashbuf.buffer)
        n = sizeof crl->hashbuf.buffer - crl->hashbuf.used;
      memcpy (crl->hashbuf.buffer+crl->hashbuf.used, buffer, n);
      crl->hashbuf.used += n;
      if (crl->hashbuf.used == sizeof crl->hashbuf.buffer)
        {
          if (crl->hash_fnc)
            crl->hash_fnc (crl->hash_fnc_arg,
                           crl->hashbuf.buffer, crl->hashbuf.used);
          crl->hashbuf.used = 0;
        }
      buffer = (const char *)buffer + n;
      length -= n;
    }
}

#define HASH(a,b) do_hash (crl, (a), (b))



static  void
parse_skip (unsigned char const **buf, size_t *len, struct tag_info *ti)
{
  if (ti->length)
    {
      assert (ti->length <= *len);
      *len -= ti->length;
      *buf += ti->length;
    }
}

static gpg_error_t
parse_sequence (unsigned char const **buf, size_t *len, struct tag_info *ti)
{
  gpg_error_t err;

  err = _ksba_ber_parse_tl (buf, len, ti);
  if (err)
    ;
  else if (!(ti->class == CLASS_UNIVERSAL && ti->tag == TYPE_SEQUENCE
             && ti->is_constructed) )
    err = gpg_error (GPG_ERR_INV_OBJ);
  else if (ti->length > *len)
    err = gpg_error (GPG_ERR_BAD_BER);
  return err;
}

static gpg_error_t
parse_integer (unsigned char const **buf, size_t *len, struct tag_info *ti)
{
  gpg_error_t err;

  err = _ksba_ber_parse_tl (buf, len, ti);
  if (err)
     ;
  else if (!(ti->class == CLASS_UNIVERSAL && ti->tag == TYPE_INTEGER
             && !ti->is_constructed) )
    err = gpg_error (GPG_ERR_INV_OBJ);
  else if (!ti->length)
    err = gpg_error (GPG_ERR_TOO_SHORT);
  else if (ti->length > *len)
    err = gpg_error (GPG_ERR_BAD_BER);

  return err;
}

static gpg_error_t
parse_octet_string (unsigned char const **buf, size_t *len, struct tag_info *ti)
{
  gpg_error_t err;

  err= _ksba_ber_parse_tl (buf, len, ti);
  if (err)
    ;
  else if (!(ti->class == CLASS_UNIVERSAL && ti->tag == TYPE_OCTET_STRING
             && !ti->is_constructed) )
    err = gpg_error (GPG_ERR_INV_OBJ);
  else if (!ti->length)
    err = gpg_error (GPG_ERR_TOO_SHORT);
  else if (ti->length > *len)
    err = gpg_error (GPG_ERR_BAD_BER);

  return err;
}

static gpg_error_t
parse_object_id_into_str (unsigned char const **buf, size_t *len, char **oid)
{
  struct tag_info ti;
  gpg_error_t err;

  *oid = NULL;
  err = _ksba_ber_parse_tl (buf, len, &ti);
  if (err)
    ;
  else if (!(ti.class == CLASS_UNIVERSAL && ti.tag == TYPE_OBJECT_ID
                && !ti.is_constructed) )
    err = gpg_error (GPG_ERR_INV_OBJ);
  else if (!ti.length)
    err = gpg_error (GPG_ERR_TOO_SHORT);
  else if (ti.length > *len)
    err = gpg_error (GPG_ERR_BAD_BER);
  else if (!(*oid = ksba_oid_to_str (*buf, ti.length)))
    err = gpg_error_from_errno (errno);
  else
    {
      *buf += ti.length;
      *len -= ti.length;
    }
  return err;
}




/**
 * ksba_crl_new:
 * 
 * Create a new and empty CRL object
 * 
 * Return value: A CRL object or an error code.
 **/
gpg_error_t
ksba_crl_new (ksba_crl_t *r_crl)
{
  *r_crl = xtrycalloc (1, sizeof **r_crl);
  if (!*r_crl)
    return gpg_error_from_errno (errno);
  return 0;
}


/**
 * ksba_crl_release:
 * @crl: A CRL object
 * 
 * Release a CRL object.
 **/
void
ksba_crl_release (ksba_crl_t crl)
{
  if (!crl)
    return;
  xfree (crl->algo.oid);
  xfree (crl->algo.parm);
  
  _ksba_asn_release_nodes (crl->issuer.root);
  xfree (crl->issuer.image);

  xfree (crl->item.serial);

  xfree (crl->sigval);
  while (crl->extension_list)
    {
      crl_extn_t tmp = crl->extension_list->next;
      xfree (crl->extension_list->oid);
      xfree (crl->extension_list);
      crl->extension_list = tmp;
    }

  xfree (crl);
}


gpg_error_t
ksba_crl_set_reader (ksba_crl_t crl, ksba_reader_t r)
{
  if (!crl || !r)
    return gpg_error (GPG_ERR_INV_VALUE);
  
  crl->reader = r;
  return 0;
}

/* Provide a hash function so that we are able to hash the data */
void
ksba_crl_set_hash_function (ksba_crl_t crl,
                            void (*hash_fnc)(void *, const void *, size_t),
                            void *hash_fnc_arg)
{
  if (crl)
    {
      crl->hash_fnc = hash_fnc;
      crl->hash_fnc_arg = hash_fnc_arg;
    }
}



/* 
   access functions
*/


/**
 * ksba_crl_get_digest_algo:
 * @cms: CMS object
 * 
 * Figure out the the digest algorithm used for the signature and return
 * its OID.  
 *
 * Return value: NULL if the signature algorithm is not yet available
 * or there is a mismatched between "tbsCertList.signature" and
 * "signatureAlgorithm"; on success the OID is returned which is valid
 * as long as the CRL object is valid.
 **/
const char *
ksba_crl_get_digest_algo (ksba_crl_t crl)
{
  if (!crl)
    return NULL;

  /* fixme: implement the described check */

  return crl->algo.oid;
}


/**
 * ksba_crl_get_issuer:
 * @cms: CMS object
 * @r_issuer: returns the issuer
 * 
 * This functions returns the issuer of the CRL.  The caller must
 * release the returned object.
 * 
 * Return value: 0 on success or an error code
 **/
gpg_error_t
ksba_crl_get_issuer (ksba_crl_t crl, char **r_issuer)
{
  gpg_error_t err;
  AsnNode n;
  const unsigned char *image;

  if (!crl || !r_issuer)
    return gpg_error (GPG_ERR_INV_VALUE);
  if (!crl->issuer.root)
    return gpg_error (GPG_ERR_NO_DATA);

  n = crl->issuer.root;
  image = crl->issuer.image;
  
  if (!n || !n->down)
    return gpg_error (GPG_ERR_NO_VALUE); 
  n = n->down; /* dereference the choice node */
      
  if (n->off == -1)
    {
/*        fputs ("get_issuer problem at node:\n", stderr); */
/*        _ksba_asn_node_dump_all (n, stderr); */
      return gpg_error (GPG_ERR_GENERAL);
    }
  err = _ksba_dn_to_str (image, n, r_issuer);

  return err;
}


/* Return the CRL extension in OID, CRITICAL, DER and DERLEN.  The
   caller should iterate IDX from 0 upwards until GPG_ERR_EOF is
   returned. Note, that the returned values are valid as long as the
   context is valid and no new parsing has been started. */
gpg_error_t
ksba_crl_get_extension (ksba_crl_t crl, int idx, 
                        char const **oid, int *critical,
                        unsigned char const **der, size_t *derlen)
{
  crl_extn_t e;

  if (!crl)
    return gpg_error (GPG_ERR_INV_VALUE);
  if (idx < 0)
    return gpg_error (GPG_ERR_INV_INDEX);

  for (e=crl->extension_list; e && idx; e = e->next, idx-- )
    ;
  if (!e)
    return gpg_error (GPG_ERR_EOF);

  if (oid)
    *oid = e->oid;
  if (critical)
    *critical = e->critical;
  if (der)
    *der = e->der;
  if (derlen)
    *derlen = e->derlen;

  return 0;
}


/* Return the authorityKeyIdentifier in r_name and r_serial or in
   r_keyID.  GPG_ERR_NO_DATA is returned if no authorityKeyIdentifier
   or only one using the keyIdentifier method is available and R_KEYID
   is NULL.

   FIXME: This function shares a lot of code with the one in cert.c 
*/
gpg_error_t 
ksba_crl_get_auth_key_id (ksba_crl_t crl,
                          ksba_sexp_t *r_keyid,
                          ksba_name_t *r_name,
                          ksba_sexp_t *r_serial)
{
  gpg_error_t err;
  size_t derlen;
  const unsigned char *der;
  const unsigned char *keyid_der = NULL;
  size_t keyid_derlen = 0;
  struct tag_info ti;
  char numbuf[30];
  size_t numbuflen;
  crl_extn_t e;

  if (r_keyid)
    *r_keyid = NULL;
  if (!crl || !r_name || !r_serial)
    return gpg_error (GPG_ERR_INV_VALUE);
  *r_name = NULL;
  *r_serial = NULL;

  for (e=crl->extension_list; e; e = e->next)
    if (!strcmp (e->oid, oidstr_authorityKeyIdentifier))
      break;
  if (!e)
    return gpg_error (GPG_ERR_NO_DATA); /* not available */
    
  /* Check that there is only one */
  {
    crl_extn_t e2;

    for (e2 = e->next; e2; e2 = e2->next)
      if (!strcmp (e2->oid, oidstr_authorityKeyIdentifier))
        return gpg_error (GPG_ERR_DUP_VALUE); 
  }
  
  der = e->der;
  derlen = e->derlen;

  err = _ksba_ber_parse_tl (&der, &derlen, &ti);
  if (err)
    return err;
  if ( !(ti.class == CLASS_UNIVERSAL && ti.tag == TYPE_SEQUENCE
         && ti.is_constructed) )
    return gpg_error (GPG_ERR_INV_CRL_OBJ);
  if (ti.ndef)
    return gpg_error (GPG_ERR_NOT_DER_ENCODED);
  if (ti.length > derlen)
    return gpg_error (GPG_ERR_BAD_BER);

  err = _ksba_ber_parse_tl (&der, &derlen, &ti);
  if (err)
    return err;
  if (ti.class != CLASS_CONTEXT) 
    return gpg_error (GPG_ERR_INV_CRL_OBJ); /* We expected a tag. */
  if (ti.ndef)
    return gpg_error (GPG_ERR_NOT_DER_ENCODED);
  if (derlen < ti.length)
    return gpg_error (GPG_ERR_BAD_BER);

  if (ti.tag == 0)
    { /* keyIdentifier:  Just save it away for later use. */
      keyid_der = der;
      keyid_derlen = ti.length;

      der += ti.length;
      derlen -= ti.length;
      /* If the keyid has been requested but no other data follows, we
         directly jump to the end. */
      if (r_keyid && !derlen)
        goto build_keyid;
      if (!derlen)
        return gpg_error (GPG_ERR_NO_DATA); /* not available */
        
      err = _ksba_ber_parse_tl (&der, &derlen, &ti);
      if (err)
        return err;
      if (ti.class != CLASS_CONTEXT) 
        return gpg_error (GPG_ERR_INV_CRL_OBJ); /* we expected a tag */
      if (ti.ndef)
        return gpg_error (GPG_ERR_NOT_DER_ENCODED);
      if (derlen < ti.length)
        return gpg_error (GPG_ERR_BAD_BER);
    }

  if (ti.tag != 1 || !derlen)
    return gpg_error (GPG_ERR_INV_CRL_OBJ);

  err = _ksba_name_new_from_der (r_name, der, ti.length);
  if (err)
    return err;

  der += ti.length;
  derlen -= ti.length;

  /* Fixme: we should release r_name before returning on error */
  err = _ksba_ber_parse_tl (&der, &derlen, &ti);
  if (err)
    return err;
  if (ti.class != CLASS_CONTEXT) 
    return gpg_error (GPG_ERR_INV_CRL_OBJ); /* we expected a tag */
  if (ti.ndef)
    return gpg_error (GPG_ERR_NOT_DER_ENCODED);
  if (derlen < ti.length)
    return gpg_error (GPG_ERR_BAD_BER);

  if (ti.tag != 2 || !derlen)
    return gpg_error (GPG_ERR_INV_CRL_OBJ);
 
  sprintf (numbuf,"(%u:", (unsigned int)ti.length);
  numbuflen = strlen (numbuf);
  *r_serial = xtrymalloc (numbuflen + ti.length + 2);
  if (!*r_serial)
    return gpg_error_from_errno (errno);
  strcpy (*r_serial, numbuf);
  memcpy (*r_serial+numbuflen, der, ti.length);
  (*r_serial)[numbuflen + ti.length] = ')';
  (*r_serial)[numbuflen + ti.length + 1] = 0;

 build_keyid:
  if (r_keyid && keyid_der && keyid_derlen)
    {
      sprintf (numbuf,"(%u:", (unsigned int)keyid_derlen);
      numbuflen = strlen (numbuf);
      *r_keyid = xtrymalloc (numbuflen + keyid_derlen + 2);
      if (!*r_keyid)
        return gpg_error (GPG_ERR_ENOMEM);
      strcpy (*r_keyid, numbuf);
      memcpy (*r_keyid+numbuflen, keyid_der, keyid_derlen);
      (*r_keyid)[numbuflen + keyid_derlen] = ')';
      (*r_keyid)[numbuflen + keyid_derlen + 1] = 0;
    }
  return 0;
}


/* Return the optional crlNumber in NUMBER or GPG_ERR_NO_DATA if it is
   not available.  Caller must release NUMBER if the fuction retruned
   with success. */
gpg_error_t 
ksba_crl_get_crl_number (ksba_crl_t crl, ksba_sexp_t *number)
{
  gpg_error_t err;
  size_t derlen;
  const unsigned char *der;
  struct tag_info ti;
  char numbuf[30];
  size_t numbuflen;
  crl_extn_t e;

  if (!crl || !number)
    return gpg_error (GPG_ERR_INV_VALUE);
  *number = NULL;

  for (e=crl->extension_list; e; e = e->next)
    if (!strcmp (e->oid, oidstr_crlNumber))
      break;
  if (!e)
    return gpg_error (GPG_ERR_NO_DATA); /* not available */
    
  /* Check that there is only one. */
  {
    crl_extn_t e2;

    for (e2 = e->next; e2; e2 = e2->next)
      if (!strcmp (e2->oid, oidstr_crlNumber))
        return gpg_error (GPG_ERR_DUP_VALUE); 
  }
  
  der = e->der;
  derlen = e->derlen;

  err = parse_integer (&der, &derlen, &ti);
  if (err)
    return err;
 
  sprintf (numbuf,"(%u:", (unsigned int)ti.length);
  numbuflen = strlen (numbuf);
  *number = xtrymalloc (numbuflen + ti.length + 2);
  if (!*number)
    return gpg_error_from_errno (errno);
  strcpy (*number, numbuf);
  memcpy (*number+numbuflen, der, ti.length);
  (*number)[numbuflen + ti.length] = ')';
  (*number)[numbuflen + ti.length + 1] = 0;

  return 0;
}




/**
 * ksba_crl_get_update_times:
 * @crl: CRL object
 * @this: Returns the thisUpdate value
 * @next: Returns the nextUpdate value.
 * 
 * THIS and NEXT may be given as NULL if the value is not required.
 * Return value: 0 on success or an error code
 **/
gpg_error_t
ksba_crl_get_update_times (ksba_crl_t crl,
                           ksba_isotime_t this,
                           ksba_isotime_t next)
{
  if (this)
    *this = 0;
  if (next)
    *next = 0;
  if (!crl)
    return gpg_error (GPG_ERR_INV_VALUE);
  if (!*crl->this_update)
    return gpg_error (GPG_ERR_INV_TIME);
  if (this)
    _ksba_copy_time (this, crl->this_update);
  if (next)
    _ksba_copy_time (next, crl->next_update);
  return 0;
}

/**
 * ksba_crl_get_item:
 * @crl: CRL object
 * @r_serial: Returns a S-exp with the serial number; caller must free.
 * @r_revocation_date: Returns the recocation date
 * @r_reason: Return the reason for revocation
 * 
 * Return the serial number, revocation time and reason of the current
 * item.  Any of these arguments may be passed as %NULL if the value
 * is not of interest.  This function should be used after the parse
 * function came back with %KSBA_SR_GOT_ITEM.  For efficiency reasons
 * the function should be called only once, the implementation may
 * return an error for the second call.
 * 
 * Return value: 0 in success or an error code.
 **/
gpg_error_t
ksba_crl_get_item (ksba_crl_t crl, ksba_sexp_t *r_serial,
                   ksba_isotime_t r_revocation_date,
                   ksba_crl_reason_t *r_reason)
{ 
  if (r_revocation_date)
    *r_revocation_date = 0;

  if (!crl)
    return gpg_error (GPG_ERR_INV_VALUE);

  if (r_serial)
    {
      if (!crl->item.serial)
        return gpg_error (GPG_ERR_NO_DATA);
      *r_serial = crl->item.serial;
      crl->item.serial = NULL;
    }
   if (r_revocation_date)
    _ksba_copy_time (r_revocation_date, crl->item.revocation_date);
  if (r_reason)
    *r_reason = crl->item.reason;
  return 0;
}



/**
 * ksba_crl_get_sig_val:
 * @crl: CRL object
 * 
 * Return the actual signature in a format suitable to be used as
 * input to Libgcrypt's verification function.  The caller must free
 * the returned string.
 * 
 * Return value: NULL or a string with an S-Exp.
 **/
ksba_sexp_t
ksba_crl_get_sig_val (ksba_crl_t crl)
{
  ksba_sexp_t p;

  if (!crl)
    return NULL;

  if (!crl->sigval)
    return NULL;
  
  p = crl->sigval;
  crl->sigval = NULL;
  return p;
}



/*
  Parser functions 
*/

/* read one byte */
static int
read_byte (ksba_reader_t reader)
{
  unsigned char buf;
  size_t nread;
  int rc;

  do
    rc = ksba_reader_read (reader, &buf, 1, &nread);
  while (!rc && !nread);
  return rc? -1: buf;
}

/* read COUNT bytes into buffer.  Return 0 on success */
static int 
read_buffer (ksba_reader_t reader, char *buffer, size_t count)
{
  size_t nread;

  while (count)
    {
      if (ksba_reader_read (reader, buffer, count, &nread))
        return -1;
      buffer += nread;
      count -= nread;
    }
  return 0;
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


/* Parse the extension in the buffer DER or length DERLEN and return
   the result in OID, CRITICAL, OFF and LEN. */
static gpg_error_t
parse_one_extension (const unsigned char *der, size_t derlen,
                     char **oid, int *critical, size_t *off, size_t *len)
{
  gpg_error_t err;
  struct tag_info ti;
  const unsigned char *start = der;
  
  *oid = NULL;
  *critical = 0;
  *off = *len = 0;

  /* 
     Extension  ::=  SEQUENCE {
         extnID      OBJECT IDENTIFIER,
         critical    BOOLEAN DEFAULT FALSE,
         extnValue   OCTET STRING }
  */
  err = parse_sequence (&der, &derlen, &ti);
  if (err)
    goto failure;

  err = parse_object_id_into_str (&der, &derlen, oid);
  if (err)
    goto failure;

  err = _ksba_ber_parse_tl (&der, &derlen, &ti);
  if (err)
    goto failure;
  if (ti.length > derlen)
    return gpg_error (GPG_ERR_BAD_BER);
  if (ti.class == CLASS_UNIVERSAL && ti.tag == TYPE_BOOLEAN
           && !ti.is_constructed)
    { 
      if (ti.length != 1)
        goto bad_ber;
      *critical = !!*der;
      parse_skip (&der, &derlen, &ti);
    }
  else
    { /* Undo that read. */
      der -= ti.nhdr;
      derlen += ti.nhdr;
    }

  err = parse_octet_string (&der, &derlen, &ti);
  if (err)
    goto failure;
  *off = der - start;
  *len = ti.length;

  return 0;

 bad_ber:
  err = gpg_error (GPG_ERR_BAD_BER);
 failure:
  xfree (*oid);
  *oid = NULL;
  return err;
}


/* Store an extension into the context. */
static gpg_error_t
store_one_extension (ksba_crl_t crl, const unsigned char *der, size_t derlen)
{
  gpg_error_t err;
  char *oid;
  int critical;
  size_t off, len;
  crl_extn_t e;

  err = parse_one_extension (der, derlen, &oid, &critical, &off, &len);
  if (err)
    return err;
  e = xtrymalloc (sizeof *e + len - 1);
  if (!e)
    {
      err = gpg_error_from_errno (errno);
      xfree (oid);
      return err;
    }
  e->oid = oid;
  e->critical = critical;
  e->derlen = len;
  memcpy (e->der, der + off, len);
  e->next = crl->extension_list;
  crl->extension_list = e;

  return 0;
}



/* Parse the fixed block at the beginning.  We use a custom parser
   here because our BER decoder is not yet able to stop at certain
   points */
static gpg_error_t
parse_to_next_update (ksba_crl_t crl)
{
  gpg_error_t err;
  struct tag_info ti;
  unsigned long outer_len, tbs_len;
  int outer_ndef, tbs_ndef;
  int c;
  unsigned char tmpbuf[500]; /* for OID or algorithmIdentifier */
  size_t nread;

  /* read the outer sequence */
  err = _ksba_ber_read_tl (crl->reader, &ti);
  if (err)
    return err;
  if ( !(ti.class == CLASS_UNIVERSAL && ti.tag == TYPE_SEQUENCE
         && ti.is_constructed) )
    return gpg_error (GPG_ERR_INV_CRL_OBJ);
  outer_len = ti.length; 
  outer_ndef = ti.ndef;
  if (!outer_ndef && outer_len < 10)
    return gpg_error (GPG_ERR_TOO_SHORT); 

  /* read the tbs sequence */
  err = _ksba_ber_read_tl (crl->reader, &ti);
  if (err)
    return err;
  if ( !(ti.class == CLASS_UNIVERSAL && ti.tag == TYPE_SEQUENCE
         && ti.is_constructed) )
    return gpg_error (GPG_ERR_INV_CRL_OBJ);
  HASH (ti.buf, ti.nhdr);
  if (!outer_ndef)
    {
      if (outer_len < ti.nhdr)
        return gpg_error (GPG_ERR_BAD_BER); /* Triplet header larger
                                               than outer sequence */
      outer_len -= ti.nhdr;
      if (!ti.ndef && outer_len < ti.length)
        return gpg_error (GPG_ERR_BAD_BER); /* Triplet larger than
                                               outer sequence */
      outer_len -= ti.length;
    }
  tbs_len = ti.length; 
  tbs_ndef = ti.ndef;
  if (!tbs_ndef && tbs_len < 10)
    return gpg_error (GPG_ERR_TOO_SHORT); 

  /* read the optional version integer */
  crl->crl_version = -1;
  err = _ksba_ber_read_tl (crl->reader, &ti);
  if (err)
    return err;
  if ( ti.class == CLASS_UNIVERSAL && ti.tag == TYPE_INTEGER)
    {
      if ( ti.is_constructed || !ti.length )
        return gpg_error (GPG_ERR_INV_CRL_OBJ); 
      HASH (ti.buf, ti.nhdr);
      if (!tbs_ndef)
        {
          if (tbs_len < ti.nhdr)
            return gpg_error (GPG_ERR_BAD_BER);
          tbs_len -= ti.nhdr;
          if (tbs_len < ti.length)
            return gpg_error (GPG_ERR_BAD_BER); 
          tbs_len -= ti.length;
        }
      /* fixme: we should also check the outer data length here and in
         the follwing code.  It might however be easier to to thsi at
         the end of this sequence */
      if (ti.length != 1)
        return gpg_error (GPG_ERR_UNSUPPORTED_CRL_VERSION); 
      if ( (c=read_byte (crl->reader)) == -1)
        {
          err = ksba_reader_error (crl->reader);
          return err? err : gpg_error (GPG_ERR_GENERAL);
        }
      if ( !(c == 0 || c == 1) )
        return gpg_error (GPG_ERR_UNSUPPORTED_CRL_VERSION);
      { 
        unsigned char tmp = c;
        HASH (&tmp, 1);
      }
      crl->crl_version = c;
      err = _ksba_ber_read_tl (crl->reader, &ti);
      if (err)
        return err;
    }

  /* read the algorithm identifier */
  if ( !(ti.class == CLASS_UNIVERSAL && ti.tag == TYPE_SEQUENCE
         && ti.is_constructed) )
    return gpg_error (GPG_ERR_INV_CRL_OBJ);
  if (!tbs_ndef)
    {
      if (tbs_len < ti.nhdr)
        return gpg_error (GPG_ERR_BAD_BER);
      tbs_len -= ti.nhdr;
      if (!ti.ndef && tbs_len < ti.length)
        return gpg_error (GPG_ERR_BAD_BER);
      tbs_len -= ti.length;
    }
  if (ti.nhdr + ti.length >= DIM(tmpbuf))
    return gpg_error (GPG_ERR_TOO_LARGE);
  memcpy (tmpbuf, ti.buf, ti.nhdr);
  err = read_buffer (crl->reader, tmpbuf+ti.nhdr, ti.length);
  if (err)
    return err;
  HASH (tmpbuf, ti.nhdr+ti.length);

  xfree (crl->algo.oid); crl->algo.oid = NULL;
  xfree (crl->algo.parm); crl->algo.parm = NULL;
  err = _ksba_parse_algorithm_identifier2 (tmpbuf, ti.nhdr+ti.length, &nread,
                                           &crl->algo.oid,
                                           &crl->algo.parm,
                                           &crl->algo.parmlen);
  if (err)
    return err;
  assert (nread <= ti.nhdr + ti.length);
  if (nread < ti.nhdr + ti.length)
    return gpg_error (GPG_ERR_TOO_SHORT);

  
  /* read the name */
  {
    unsigned long n = ksba_reader_tell (crl->reader);
    err = create_and_run_decoder (crl->reader, 
                                  "TMTTv2.CertificateList.tbsCertList.issuer",
                                  &crl->issuer.root,
                                  &crl->issuer.image,
                                  &crl->issuer.imagelen);
    if (err)
      return err;
    /* imagelen might be larger than the valid data (due to read ahead).
       So we need to get the count from the reader */
    n = ksba_reader_tell (crl->reader) - n;
    if (n > crl->issuer.imagelen)
      return gpg_error (GPG_ERR_BUG);
    HASH (crl->issuer.image, n);

    if (!tbs_ndef)
      {
        if (tbs_len < n)
          return gpg_error (GPG_ERR_BAD_BER);
        tbs_len -= n;
      }
  }


  
  /* read the thisUpdate time */
  err = _ksba_ber_read_tl (crl->reader, &ti);
  if (err)
    return err;
  if ( !(ti.class == CLASS_UNIVERSAL
         && (ti.tag == TYPE_UTC_TIME || ti.tag == TYPE_GENERALIZED_TIME)
         && !ti.is_constructed) )
    return gpg_error (GPG_ERR_INV_CRL_OBJ);
  if (!tbs_ndef)
    {
      if (tbs_len < ti.nhdr)
        return gpg_error (GPG_ERR_BAD_BER);
      tbs_len -= ti.nhdr;
      if (!ti.ndef && tbs_len < ti.length)
        return gpg_error (GPG_ERR_BAD_BER);
      tbs_len -= ti.length;
    }
  if (ti.nhdr + ti.length >= DIM(tmpbuf))
    return gpg_error (GPG_ERR_TOO_LARGE);
  memcpy (tmpbuf, ti.buf, ti.nhdr);
  err = read_buffer (crl->reader, tmpbuf+ti.nhdr, ti.length);
  if (err)
    return err;
  HASH (tmpbuf, ti.nhdr+ti.length);
  _ksba_asntime_to_iso (tmpbuf+ti.nhdr, ti.length,
                        ti.tag == TYPE_UTC_TIME, crl->this_update);

  /* Read the optional nextUpdate time. */
  err = _ksba_ber_read_tl (crl->reader, &ti);
  if (err)
    return err;
  if ( ti.class == CLASS_UNIVERSAL
       && (ti.tag == TYPE_UTC_TIME || ti.tag == TYPE_GENERALIZED_TIME)
         && !ti.is_constructed )
    {
      if (!tbs_ndef)
        {
          if (tbs_len < ti.nhdr)
            return gpg_error (GPG_ERR_BAD_BER);
          tbs_len -= ti.nhdr;
          if (!ti.ndef && tbs_len < ti.length)
            return gpg_error (GPG_ERR_BAD_BER);
          tbs_len -= ti.length;
        }
      if (ti.nhdr + ti.length >= DIM(tmpbuf))
        return gpg_error (GPG_ERR_TOO_LARGE);
      memcpy (tmpbuf, ti.buf, ti.nhdr);
      err = read_buffer (crl->reader, tmpbuf+ti.nhdr, ti.length);
      if (err)
        return err;
      HASH (tmpbuf, ti.nhdr+ti.length);
      _ksba_asntime_to_iso (tmpbuf+ti.nhdr, ti.length,
                            ti.tag == TYPE_UTC_TIME, crl->next_update);
      err = _ksba_ber_read_tl (crl->reader, &ti);
      if (err)
        return err;
    }

  /* Read the first sequence tag of the optional SEQ of SEQ. */
  if (tbs_ndef || tbs_len)
    {
      if (ti.class == CLASS_UNIVERSAL && ti.tag == TYPE_SEQUENCE
          && ti.is_constructed )
        { /* yes, there is one */
          HASH (ti.buf, ti.nhdr);
          if (!tbs_ndef)
            {
              if (tbs_len < ti.nhdr)
            return gpg_error (GPG_ERR_BAD_BER);
              tbs_len -= ti.nhdr;
              if (!ti.ndef && tbs_len < ti.length)
                return gpg_error (GPG_ERR_BAD_BER);
              tbs_len -= ti.length; 
            }
          crl->state.have_seqseq = 1;
          crl->state.seqseq_ndef = ti.ndef;
          crl->state.seqseq_len  = ti.length;
          /* and read the next */
          err = _ksba_ber_read_tl (crl->reader, &ti);
          if (err)
            return err;
        }
    }

  /* We need to save some stuff for the next round. */
  crl->state.ti = ti;
  crl->state.outer_ndef = outer_ndef;
  crl->state.outer_len = outer_len;
  crl->state.tbs_ndef = tbs_ndef;
  crl->state.tbs_len = tbs_len;

  return 0;
}


/* Parse an enumerated value.  Note that this code is duplication of
   the one at ocsp.c.  */
static gpg_error_t
parse_enumerated (unsigned char const **buf, size_t *len, struct tag_info *ti,
                  size_t maxlen)
{
  gpg_error_t err;

  err = _ksba_ber_parse_tl (buf, len, ti);
  if (err)
     ;
  else if (!(ti->class == CLASS_UNIVERSAL && ti->tag == TYPE_ENUMERATED
             && !ti->is_constructed) )
    err = gpg_error (GPG_ERR_INV_OBJ);
  else if (!ti->length)
    err = gpg_error (GPG_ERR_TOO_SHORT);
  else if (maxlen && ti->length > maxlen)
    err = gpg_error (GPG_ERR_TOO_LARGE);
  else if (ti->length > *len)
    err = gpg_error (GPG_ERR_BAD_BER);

  return err;
}



/* Store an entry extension into the current item. */
static gpg_error_t
store_one_entry_extension (ksba_crl_t crl,
                           const unsigned char *der, size_t derlen)
{
  gpg_error_t err;
  char *oid;
  int critical;
  size_t off, len;

  err = parse_one_extension (der, derlen, &oid, &critical, &off, &len);
  if (err)
    return err;
  if (!strcmp (oid, oidstr_crlReason))
    {
      struct tag_info ti;
      const unsigned char *buf = der+off;
      size_t mylen = len;

      err = parse_enumerated (&buf, &mylen, &ti, 1);
      if (err)
        return err;
      /* Note that we OR the values so that in case this extension is
         repeated we can track all reason codes. */ 
      switch (*buf)
        { 
        case  0: crl->item.reason |= KSBA_CRLREASON_UNSPECIFIED; break;
        case  1: crl->item.reason |= KSBA_CRLREASON_KEY_COMPROMISE; break;
        case  2: crl->item.reason |= KSBA_CRLREASON_CA_COMPROMISE; break;
        case  3: crl->item.reason |= KSBA_CRLREASON_AFFILIATION_CHANGED; break;
        case  4: crl->item.reason |= KSBA_CRLREASON_SUPERSEDED; break;
        case  5: crl->item.reason |= KSBA_CRLREASON_CESSATION_OF_OPERATION;
          break;
        case  6: crl->item.reason |= KSBA_CRLREASON_CERTIFICATE_HOLD; break;
        case  8: crl->item.reason |= KSBA_CRLREASON_REMOVE_FROM_CRL; break;
        case  9: crl->item.reason |= KSBA_CRLREASON_PRIVILEGE_WITHDRAWN; break;
        case 10: crl->item.reason |= KSBA_CRLREASON_AA_COMPROMISE; break;
        default: crl->item.reason |= KSBA_CRLREASON_OTHER; break;
        }
    }
  if (!strcmp (oid, oidstr_certificateIssuer))
    {
      /* FIXME: We need to implement this. */
    }
  else if (critical)
    err = gpg_error (GPG_ERR_UNKNOWN_CRIT_EXTN);
  
  return err;
}

/* Parse the revokedCertificates SEQUENCE of SEQUENCE using a custom
   parser for efficiency and return after each entry */
static gpg_error_t
parse_crl_entry (ksba_crl_t crl, int *got_entry)
{
  gpg_error_t err;
  struct tag_info ti = crl->state.ti;
  unsigned long seqseq_len= crl->state.seqseq_len;
  int seqseq_ndef         = crl->state.seqseq_ndef;
  unsigned long len;
  int ndef;
  unsigned char tmpbuf[4096]; /* for time, serial number and extensions */
  char numbuf[22];
  int numbuflen;

  /* Check the length to see whether we are at the end of the seq but do
     this only when we know that we have this optional seq of seq. */
  if (!crl->state.have_seqseq)
    return 0; /* ready (no entries at all) */

  if (!seqseq_ndef && !seqseq_len)
    return 0; /* ready */

  /* if this is not a SEQUENCE the CRL is invalid */
  if ( !(ti.class == CLASS_UNIVERSAL && ti.tag == TYPE_SEQUENCE
         && ti.is_constructed) )
    return gpg_error (GPG_ERR_INV_CRL_OBJ);
  HASH (ti.buf, ti.nhdr);
  if (!seqseq_ndef)
    {
      if (seqseq_len < ti.nhdr)
        return gpg_error (GPG_ERR_BAD_BER);
      seqseq_len -= ti.nhdr;
      if (!ti.ndef && seqseq_len < ti.length)
        return gpg_error (GPG_ERR_BAD_BER);
      seqseq_len -= ti.length;
    }
  ndef = ti.ndef;
  len  = ti.length;

  /* get the serial number */
  err = _ksba_ber_read_tl (crl->reader, &ti);
  if (err)
    return err;
  if ( !(ti.class == CLASS_UNIVERSAL && ti.tag == TYPE_INTEGER
         && !ti.is_constructed) )
    return gpg_error (GPG_ERR_INV_CRL_OBJ);
  if (!ndef)
    {
      if (len < ti.nhdr)
        return gpg_error (GPG_ERR_BAD_BER);
      len -= ti.nhdr;
      if (!ti.ndef && len < ti.length)
        return gpg_error (GPG_ERR_BAD_BER);
      len -= ti.length;
    }
  if (ti.nhdr + ti.length >= DIM(tmpbuf))
    return gpg_error (GPG_ERR_TOO_LARGE);
  memcpy (tmpbuf, ti.buf, ti.nhdr);
  err = read_buffer (crl->reader, tmpbuf+ti.nhdr, ti.length);
  if (err)
    return err;
  HASH (tmpbuf, ti.nhdr+ti.length);

  xfree (crl->item.serial);
  sprintf (numbuf,"(%u:", (unsigned int)ti.length);
  numbuflen = strlen (numbuf);
  crl->item.serial = xtrymalloc (numbuflen + ti.length + 2);
  if (!crl->item.serial)
    return gpg_error (GPG_ERR_ENOMEM);
  strcpy (crl->item.serial, numbuf);
  memcpy (crl->item.serial+numbuflen, tmpbuf+ti.nhdr, ti.length);
  crl->item.serial[numbuflen + ti.length] = ')';
  crl->item.serial[numbuflen + ti.length + 1] = 0;
  crl->item.reason = 0;

  /* get the revocation time */
  err = _ksba_ber_read_tl (crl->reader, &ti);
  if (err)
    return err;
  if ( !(ti.class == CLASS_UNIVERSAL
         && (ti.tag == TYPE_UTC_TIME || ti.tag == TYPE_GENERALIZED_TIME)
         && !ti.is_constructed) )
    return gpg_error (GPG_ERR_INV_CRL_OBJ);
  if (!ndef)
    {
      if (len < ti.nhdr)
        return gpg_error (GPG_ERR_BAD_BER);
      len -= ti.nhdr;
      if (!ti.ndef && len < ti.length)
        return gpg_error (GPG_ERR_BAD_BER);
      len -= ti.length;
    }
  if (ti.nhdr + ti.length >= DIM(tmpbuf))
    return gpg_error (GPG_ERR_TOO_LARGE);
  memcpy (tmpbuf, ti.buf, ti.nhdr);
  err = read_buffer (crl->reader, tmpbuf+ti.nhdr, ti.length);
  if (err)
    return err;
  HASH (tmpbuf, ti.nhdr+ti.length);
  
  _ksba_asntime_to_iso (tmpbuf+ti.nhdr, ti.length,
                        ti.tag == TYPE_UTC_TIME, crl->item.revocation_date);

  /* if there is still space we must parse the optional entryExtensions */
  if (ndef)
    return gpg_error (GPG_ERR_UNSUPPORTED_ENCODING);
  else if (len)
    {
      /* read the outer sequence */
      err = _ksba_ber_read_tl (crl->reader, &ti);
      if (err)
        return err;
      if ( !(ti.class == CLASS_UNIVERSAL
             && ti.tag == TYPE_SEQUENCE && ti.is_constructed) )
        return gpg_error (GPG_ERR_INV_CRL_OBJ);
      if (ti.ndef)
        return gpg_error (GPG_ERR_UNSUPPORTED_ENCODING);
      HASH (ti.buf, ti.nhdr);
      if (len < ti.nhdr)
        return gpg_error (GPG_ERR_BAD_BER);
      len -= ti.nhdr;
      if (len < ti.length)
        return gpg_error (GPG_ERR_BAD_BER);

      /* now loop over the extensions */
      while (len)
        {
          err = _ksba_ber_read_tl (crl->reader, &ti);
          if (err)
            return err;
          if ( !(ti.class == CLASS_UNIVERSAL
                 && ti.tag == TYPE_SEQUENCE && ti.is_constructed) )
            return gpg_error (GPG_ERR_INV_CRL_OBJ);
          if (ti.ndef)
            return gpg_error (GPG_ERR_UNSUPPORTED_ENCODING);
          if (len < ti.nhdr)
            return gpg_error (GPG_ERR_BAD_BER);
          len -= ti.nhdr;
          if (len < ti.length)
            return gpg_error (GPG_ERR_BAD_BER);
          len -= ti.length;
          if (ti.nhdr + ti.length >= DIM(tmpbuf))
            return gpg_error (GPG_ERR_TOO_LARGE);
          memcpy (tmpbuf, ti.buf, ti.nhdr);
          err = read_buffer (crl->reader, tmpbuf+ti.nhdr, ti.length);
          if (err)
            return err;
          HASH (tmpbuf, ti.nhdr+ti.length);
          err = store_one_entry_extension (crl, tmpbuf, ti.nhdr+ti.length);
          if (err)
            return err;
        }
    }

  /* read ahead */
  err = _ksba_ber_read_tl (crl->reader, &ti);
  if (err)
    return err;

  *got_entry = 1;

  /* Fixme: the seqseq length is not correct if any element was ndef'd */
  crl->state.ti = ti;
  crl->state.seqseq_ndef = seqseq_ndef;
  crl->state.seqseq_len  = seqseq_len;

  return 0;
}


/* This function is used when a [0] tag was encountered to read the
   crlExtensions */
static gpg_error_t 
parse_crl_extensions (ksba_crl_t crl)
{ 
  gpg_error_t err;
  struct tag_info ti = crl->state.ti;
  unsigned long ext_len, len;
  unsigned char tmpbuf[4096]; /* for extensions */

  /* if we do not have a tag [0] we are done with this */
  if (!(ti.class == CLASS_CONTEXT && ti.tag == 0 && ti.is_constructed))
    return 0;
  if (ti.ndef)
    return gpg_error (GPG_ERR_UNSUPPORTED_ENCODING);
  HASH (ti.buf, ti.nhdr);
  ext_len = ti.length;

  /* read the outer sequence */
  err = _ksba_ber_read_tl (crl->reader, &ti);
  if (err)
    return err;
  if ( !(ti.class == CLASS_UNIVERSAL
         && ti.tag == TYPE_SEQUENCE && ti.is_constructed) )
    return gpg_error (GPG_ERR_INV_CRL_OBJ);
  if (ti.ndef)
    return gpg_error (GPG_ERR_UNSUPPORTED_ENCODING);
  HASH (ti.buf, ti.nhdr);
  if (ext_len < ti.nhdr)
    return gpg_error (GPG_ERR_BAD_BER);
  ext_len -= ti.nhdr;
  if (ext_len < ti.length)
    return gpg_error (GPG_ERR_BAD_BER);
  len = ti.length;

  /* now loop over the extensions */
  while (len)
    {
      err = _ksba_ber_read_tl (crl->reader, &ti);
      if (err)
        return err;
      if ( !(ti.class == CLASS_UNIVERSAL
             && ti.tag == TYPE_SEQUENCE && ti.is_constructed) )
        return gpg_error (GPG_ERR_INV_CRL_OBJ);
      if (ti.ndef)
        return gpg_error (GPG_ERR_UNSUPPORTED_ENCODING);
      if (len < ti.nhdr)
        return gpg_error (GPG_ERR_BAD_BER);
      len -= ti.nhdr;
      if (len < ti.length)
        return gpg_error (GPG_ERR_BAD_BER);
      len -= ti.length;
      if (ti.nhdr + ti.length >= DIM(tmpbuf))
        return gpg_error (GPG_ERR_TOO_LARGE);
      /* fixme use a larger buffer if the extension does not fit into tmpbuf */
      memcpy (tmpbuf, ti.buf, ti.nhdr);
      err = read_buffer (crl->reader, tmpbuf+ti.nhdr, ti.length);
      if (err)
        return err;
      HASH (tmpbuf, ti.nhdr+ti.length);
      err = store_one_extension (crl, tmpbuf, ti.nhdr+ti.length);
      if (err)
        return err;
    }

  /* read ahead */
  err = _ksba_ber_read_tl (crl->reader, &ti);
  if (err)
    return err;

  crl->state.ti = ti;
  return 0;
}

/* Parse the signatureAlgorithm and the signature */
static gpg_error_t
parse_signature (ksba_crl_t crl)
{
  gpg_error_t err;
  struct tag_info ti = crl->state.ti;
  unsigned char tmpbuf[2048]; /* for the sig algo and bitstr */
  size_t n, n2;

  /* We do read the stuff into a temporary buffer so that we can apply
     our parsing function for this structure */

  /* read the algorithmIdentifier sequence */
  if ( !(ti.class == CLASS_UNIVERSAL && ti.tag == TYPE_SEQUENCE
         && ti.is_constructed) )
    return gpg_error (GPG_ERR_INV_CRL_OBJ);
  if (ti.ndef)
    return gpg_error (GPG_ERR_UNSUPPORTED_ENCODING);
  n = ti.nhdr + ti.length;
  if (n >= DIM(tmpbuf))
    return gpg_error (GPG_ERR_TOO_LARGE);
  memcpy (tmpbuf, ti.buf, ti.nhdr);
  err = read_buffer (crl->reader, tmpbuf+ti.nhdr, ti.length);
  if (err)
    return err;
  
  /* and append the bit string */
  err = _ksba_ber_read_tl (crl->reader, &ti);
  if (err)
    return err;
  if ( !(ti.class == CLASS_UNIVERSAL && ti.tag == TYPE_BIT_STRING
         && !ti.is_constructed) )
    return gpg_error (GPG_ERR_INV_CRL_OBJ);
  n2 = ti.nhdr + ti.length;
  if (n + n2 >= DIM(tmpbuf))
    return gpg_error (GPG_ERR_TOO_LARGE);
  memcpy (tmpbuf+n, ti.buf, ti.nhdr);
  err = read_buffer (crl->reader, tmpbuf+n+ti.nhdr, ti.length);
  if (err)
    return err;

  /* now parse it */
  xfree (crl->sigval); crl->sigval = NULL;
  return _ksba_sigval_to_sexp (tmpbuf, n + n2, &crl->sigval);
}


/* The actual parser which should be used with a new CRL object and
   run in a loop until the the KSBA_SR_READY is encountered */
gpg_error_t 
ksba_crl_parse (ksba_crl_t crl, ksba_stop_reason_t *r_stopreason)
{
  enum { 
    sSTART,
    sCRLENTRY,
    sCRLEXT,
    sERROR
  } state = sERROR;
  ksba_stop_reason_t stop_reason;
  gpg_error_t err = 0;
  int got_entry = 0;

  if (!crl || !r_stopreason)
    return gpg_error (GPG_ERR_INV_VALUE);

  if (!crl->any_parse_done)
    { /* first time initialization of the stop reason */
      *r_stopreason = 0;
      crl->any_parse_done = 1;
    }

  /* Calculate state from last reason */
  stop_reason = *r_stopreason;
  *r_stopreason = KSBA_SR_RUNNING;
  switch (stop_reason)
    {
    case 0:
      state = sSTART;
      break;
    case KSBA_SR_BEGIN_ITEMS:
    case KSBA_SR_GOT_ITEM:
      state = sCRLENTRY;
      break;
    case KSBA_SR_END_ITEMS:
      state = sCRLEXT;
      break;
    case KSBA_SR_RUNNING:
      err = gpg_error (GPG_ERR_INV_STATE);
      break;
    default:
      err = gpg_error (GPG_ERR_BUG);
      break;
    }
  if (err)
    return err;

  /* Do the action */
  switch (state)
    {
    case sSTART:
      err = parse_to_next_update (crl);
      break;
    case sCRLENTRY:
      err = parse_crl_entry (crl, &got_entry);
      break;
    case sCRLEXT:
      err = parse_crl_extensions (crl);
      if (!err)
        {
          if (crl->hash_fnc && crl->hashbuf.used)
            crl->hash_fnc (crl->hash_fnc_arg,
                           crl->hashbuf.buffer, crl->hashbuf.used);
          crl->hashbuf.used = 0;
          err = parse_signature (crl);
        }
      break;
    default:
      err = gpg_error (GPG_ERR_INV_STATE);
      break;
    }
  if (err)
    return err;

  /* Calculate new stop reason */
  switch (state)
    {
    case sSTART:
      stop_reason = KSBA_SR_BEGIN_ITEMS;
      break;
    case sCRLENTRY:
      stop_reason = got_entry? KSBA_SR_GOT_ITEM : KSBA_SR_END_ITEMS;
      break;
    case sCRLEXT:
      stop_reason = KSBA_SR_READY;
      break;
    default:
      break;
    }
  
  *r_stopreason = stop_reason;
  return 0;
}
