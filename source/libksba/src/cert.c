/* cert.c - main function for the certificate handling
 *      Copyright (C) 2001, 2002, 2003, 2004, 2005 g10 Code GmbH
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
#include <errno.h>

#include "util.h"
#include "ber-decoder.h"
#include "ber-help.h"
#include "convert.h"
#include "keyinfo.h"
#include "sexp-parse.h"
#include "cert.h"


static const char oidstr_subjectKeyIdentifier[] = "2.5.29.14";
static const char oidstr_keyUsage[]         = "2.5.29.15";
static const char oidstr_subjectAltName[]   = "2.5.29.17";
static const char oidstr_issuerAltName[]    = "2.5.29.18";
static const char oidstr_basicConstraints[] = "2.5.29.19";
static const char oidstr_crlDistributionPoints[] = "2.5.29.31";
static const char oidstr_certificatePolicies[] = "2.5.29.32";
static const char oidstr_authorityKeyIdentifier[] = "2.5.29.35";
static const char oidstr_extKeyUsage[] = "2.5.29.37";
static const char oidstr_authorityInfoAccess[] = "1.3.6.1.5.5.7.1.1";
static const char oidstr_subjectInfoAccess[]   = "1.3.6.1.5.5.7.1.11";


/**
 * ksba_cert_new:
 * 
 * Create a new and empty certificate object
 * 
 * Return value: 0 on success or error code.  For a successful
 * operation, ACERT is set to the new certifixate obbject, otherwise
 * it is set to NULL.
 **/
gpg_error_t
ksba_cert_new (ksba_cert_t *acert)
{
  *acert = xtrycalloc (1, sizeof **acert);
  if (!*acert)
    return gpg_error_from_errno (errno);
  (*acert)->ref_count++;

  return 0;
}

void
ksba_cert_ref (ksba_cert_t cert)
{
  if (!cert)
    fprintf (stderr, "BUG: ksba_cert_ref for NULL\n");
  else
    ++cert->ref_count;
}

/**
 * ksba_cert_release:
 * @cert: A certificate object
 * 
 * Release a certificate object.
 **/
void
ksba_cert_release (ksba_cert_t cert)
{
  int i;

  if (!cert)
    return;
  if (cert->ref_count < 1)
    {
      fprintf (stderr, "BUG: trying to release an already released cert\n");
      return;
    }
  if (--cert->ref_count)
    return;

  if (cert->udata)
    {
      struct cert_user_data *ud = cert->udata;
      cert->udata = NULL;  
      do
        {
          struct cert_user_data *ud2 = ud->next;
          if (ud->data && ud->data != ud->databuf)
            xfree (ud->data);
          xfree (ud);
          ud = ud2;
        }
      while (ud);
    }
  
  xfree (cert->cache.digest_algo);
  if (cert->cache.extns_valid)
    {
      for (i=0; i < cert->cache.n_extns; i++)
        xfree (cert->cache.extns[i].oid);
      xfree (cert->cache.extns);
    }

  _ksba_asn_release_nodes (cert->root);
  ksba_asn_tree_release (cert->asn_tree);

  xfree (cert->image);

  xfree (cert);
}


/* Store arbitrary data along with a certificate.  The DATA of length
   DATALEN will be stored under the string KEY.  If some data is
   already stored under this key it will be replaced by the new data.
   Using NULL for DATA will effectivly delete the data.

   On error (i.e. out or memory) an already existing data object
   stored under KEY may get deleted.

   This function is not thread safe because we don't employ any
   locking. */
gpg_error_t
ksba_cert_set_user_data (ksba_cert_t cert,
                         const char *key, const void *data, size_t datalen)
{
  struct cert_user_data *ud;

  if (!cert || !key || !*key)
    return gpg_error (GPG_ERR_INV_VALUE);

  for (ud=cert->udata; ud; ud = ud->next)
    if (!strcmp (ud->key, key))
      break;
  if (ud)  /* Update the data stored under this key or reuse this item. */
    {
      if (ud->data && ud->data != ud->databuf)
        xfree (ud->data);
      ud->data = NULL;
      if (data && datalen <= sizeof ud->databuf)
        {
          memcpy (ud->databuf, data, datalen);
          ud->data = ud->databuf;
          ud->datalen = datalen;
        }
      else if (data)
        {
          ud->data = xtrymalloc (datalen);
          if (!ud->data)
            return gpg_error_from_errno (errno);
          memcpy (ud->data, data, datalen);
          ud->datalen = datalen;
        }
    }
  else if (data) /* Insert as a new item. */
    {
      ud = xtrycalloc (1, sizeof *ud + strlen (key));
      if (!ud)
        return gpg_error_from_errno (errno);
      strcpy (ud->key, key);
      if (datalen <= sizeof ud->databuf)
        {
          memcpy (ud->databuf, data, datalen);
          ud->data = ud->databuf;
          ud->datalen = datalen;
        }
      else
        {
          ud->data = xtrymalloc (datalen);
          if (!ud->data)
            {
              xfree (ud);
              return gpg_error_from_errno (errno);
            }
          memcpy (ud->data, data, datalen);
          ud->datalen = datalen;
        }
      ud->next = cert->udata;
      cert->udata = ud;
    }

  return 0;
}



/* Return user data for certificate CERT stored under the string
   KEY. The caller needs to provide a suitable large BUFFER and pass
   the usable length of the buffer in BUFFERLEN.  If DATALEN is not
   NULL, the length of the data stored at BUFFER will be stored there.

   If BUFFER is NULL, BUFFERLEN will be ignored and the required
   length of the buffer will be returned at DATALEN.

   On success 0 is returned.  If no data is stored under KEY
   GPG_ERR_NOT_FOUND is returned.  If the provided buffer is too
   short, GPG_ERR_BUFFER_TOO_SHORT will be returned (note, that this
   is not the case if BUFFER is NULL).
*/
gpg_error_t
ksba_cert_get_user_data (ksba_cert_t cert, const char *key,
                         void *buffer, size_t bufferlen, size_t *datalen)
{
  struct cert_user_data *ud;

  if (!cert || !key || !*key)
    return gpg_error (GPG_ERR_INV_VALUE);

  for (ud=cert->udata; ud; ud = ud->next)
    if (!strcmp (ud->key, key))
      break;
  if (!ud || !ud->data)
    return gpg_error (GPG_ERR_NOT_FOUND);
  if (datalen)
    *datalen = ud->datalen;
  if (buffer)
    {
      if (ud->datalen > bufferlen)
        return gpg_error (GPG_ERR_BUFFER_TOO_SHORT);
      memcpy (buffer, ud->data, ud->datalen);
    }
  return 0;
}


/**
 * ksba_cert_read_der:
 * @cert: An unitialized certificate object
 * @reader: A KSBA Reader object
 * 
 * Read the next certificate from the reader and store it in the
 * certificate object for future access.  The certificate is parsed
 * and rejected if it has any syntactical or semantical error
 * (i.e. does not match the ASN.1 description).
 * 
 * Return value: 0 on success or an error value
 **/
gpg_error_t
ksba_cert_read_der (ksba_cert_t cert, ksba_reader_t reader)
{
  gpg_error_t err = 0;
  BerDecoder decoder = NULL;

  if (!cert || !reader)
    return gpg_error (GPG_ERR_INV_VALUE);
  if (cert->initialized)
    return gpg_error (GPG_ERR_CONFLICT); /* Fixme: should remove the old one */

  _ksba_asn_release_nodes (cert->root);
  ksba_asn_tree_release (cert->asn_tree);
  cert->root = NULL;
  cert->asn_tree = NULL;

  err = ksba_asn_create_tree ("tmttv2", &cert->asn_tree);
  if (err)
    goto leave;

  decoder = _ksba_ber_decoder_new ();
  if (!decoder)
    {
      err = gpg_error (GPG_ERR_ENOMEM);
      goto leave;
    }

  err = _ksba_ber_decoder_set_reader (decoder, reader);
  if (err)
    goto leave;
  
  err = _ksba_ber_decoder_set_module (decoder, cert->asn_tree);
  if (err)
     goto leave;

  err = _ksba_ber_decoder_decode (decoder, "TMTTv2.Certificate", 0,
                                  &cert->root, &cert->image, &cert->imagelen);
  if (!err)
      cert->initialized = 1;
  
 leave:
  _ksba_ber_decoder_release (decoder);

  return err;
}


gpg_error_t
ksba_cert_init_from_mem (ksba_cert_t cert, const void *buffer, size_t length)
{
  gpg_error_t err;
  ksba_reader_t reader;

  err = ksba_reader_new (&reader);
  if (err)
    return err;
  err = ksba_reader_set_mem (reader, buffer, length);
  if (err)
    {
      ksba_reader_release (reader);
      return err;
    }
  err = ksba_cert_read_der (cert, reader);
  ksba_reader_release (reader);
  return err;
}



const unsigned char *
ksba_cert_get_image (ksba_cert_t cert, size_t *r_length )
{
  AsnNode n;

  if (!cert)
    return NULL;
  if (!cert->initialized)
    return NULL;

  n = _ksba_asn_find_node (cert->root, "Certificate");
  if (!n) 
    return NULL;

  if (n->off == -1)
    {
/*        fputs ("ksba_cert_get_image problem at node:\n", stderr); */
/*        _ksba_asn_node_dump_all (n, stderr); */
      return NULL;
    }

  /* Due to minor problems in our parser we might hit the assertion
     below.  Thus we better return a error, proper. */
  if ( !(n->nhdr + n->len + n->off <= cert->imagelen) )
    {
      fprintf (stderr,"\nOops, ksba_cert_get_image failed: "
               "imagelen=%lu  hdr=%d len=%d off=%d\n",
               (unsigned long)cert->imagelen, n->nhdr, (int)n->len, n->off);
      return NULL;
    }
  /*assert (n->nhdr + n->len + n->off <= cert->imagelen);*/

  if (r_length)
    *r_length = n->nhdr + n->len;
  return cert->image + n->off;
}

/* Check whether certificates A and B are identical and return o in
   this case. */
int
_ksba_cert_cmp (ksba_cert_t a, ksba_cert_t b)
{
  const unsigned char *img_a, *img_b;
  size_t len_a, len_b;

  img_a = ksba_cert_get_image (a, &len_a);
  if (!img_a)
    return 1;
  img_b = ksba_cert_get_image (b, &len_b);
  if (!img_b)
    return 1;
  return !(len_a == len_b && !memcmp (img_a, img_b, len_a));
}




gpg_error_t
ksba_cert_hash (ksba_cert_t cert, int what,
                void (*hasher)(void *, const void *, size_t length), 
                void *hasher_arg)
{
  AsnNode n;

  if (!cert /*|| !hasher*/)
    return gpg_error (GPG_ERR_INV_VALUE);
  if (!cert->initialized)
    return gpg_error (GPG_ERR_NO_DATA);

  n = _ksba_asn_find_node (cert->root,
                           what == 1? "Certificate.tbsCertificate"
                                    : "Certificate");
  if (!n)
    return gpg_error (GPG_ERR_NO_VALUE); /* oops - should be there */
  if (n->off == -1)
    {
/*        fputs ("ksba_cert_hash problem at node:\n", stderr); */
/*        _ksba_asn_node_dump_all (n, stderr); */
      return gpg_error (GPG_ERR_NO_VALUE);
    }

  hasher (hasher_arg, cert->image + n->off,  n->nhdr + n->len);


  return 0;
}



/**
 * ksba_cert_get_digest_algo:
 * @cert: Initialized certificate object
 * 
 * Figure out the the digest algorithm used for the signature and
 * return its OID
 *
 * This function is intended as a helper for the ksba_cert_hash().
 * 
 * Return value: NULL for error otherwise a constant string with the OID.
 * This string is valid as long the certificate object is valid.
 **/
const char *
ksba_cert_get_digest_algo (ksba_cert_t cert)
{
  gpg_error_t err;
  AsnNode n;
  char *algo;
  size_t nread;

  if (!cert)
    {
       cert->last_error = gpg_error (GPG_ERR_INV_VALUE);
       return NULL;
    }
  if (!cert->initialized)
    {
       cert->last_error = gpg_error (GPG_ERR_NO_DATA);
       return NULL;
    }

  if (cert->cache.digest_algo)
    return cert->cache.digest_algo;
  
/*   n = _ksba_asn_find_node (cert->root, */
/*                            "Certificate.signatureAlgorithm.algorithm"); */
/*   algo = _ksba_oid_node_to_str (cert->image, n); */
/*   if (!algo) */
/*     cert->last_error = gpg_error (GPG_ERR_UNKNOWN_ALGORITHM); */
/*   else  */
/*     cert->cache.digest_algo = algo; */

  n = _ksba_asn_find_node (cert->root, "Certificate.signatureAlgorithm");
  if (!n || n->off == -1)
    err = gpg_error (GPG_ERR_UNKNOWN_ALGORITHM);
  else
    err = _ksba_parse_algorithm_identifier (cert->image + n->off,
                                            n->nhdr + n->len, &nread, &algo);
  if (err)
    cert->last_error = err;
  else 
    cert->cache.digest_algo = algo;

  return algo;
}




/**
 * ksba_cert_get_serial:
 * @cert: certificate object 
 * 
 * This function returnes the serial number of the certificate.  The
 * serial number is an integer returned as an canonical encoded
 * S-expression with just one element.
 * 
 * Return value: An allocated S-Exp or NULL for no value.
 **/
ksba_sexp_t
ksba_cert_get_serial (ksba_cert_t cert)
{
  AsnNode n;
  char *p;
  char numbuf[22];
  int numbuflen;

  if (!cert || !cert->initialized)
    return NULL;
  
  n = _ksba_asn_find_node (cert->root,
                           "Certificate.tbsCertificate.serialNumber");
  if (!n)
    return NULL; /* oops - should be there */

  if (n->off == -1)
    {
/*        fputs ("get_serial problem at node:\n", stderr); */
/*        _ksba_asn_node_dump_all (n, stderr); */
      return NULL;
    }
  
  sprintf (numbuf,"(%u:", (unsigned int)n->len);
  numbuflen = strlen (numbuf);
  p = xtrymalloc (numbuflen + n->len + 2);
  if (!p)
    return NULL;
  strcpy (p, numbuf);
  memcpy (p+numbuflen, cert->image + n->off + n->nhdr, n->len);
  p[numbuflen + n->len] = ')';
  p[numbuflen + n->len + 1] = 0;
  return p;
}


/* Return a pointer to the DER encoding of the serial number in CERT in
   PTR and the length of that field in LENGTH.  */
gpg_error_t
_ksba_cert_get_serial_ptr (ksba_cert_t cert,
                           unsigned char const **ptr, size_t *length)
{
  asn_node_t n;

  if (!cert || !cert->initialized || !ptr || !length)
    return gpg_error (GPG_ERR_INV_VALUE);
  n = _ksba_asn_find_node (cert->root,
                           "Certificate.tbsCertificate.serialNumber");
  if (!n || n->off == -1)
    return gpg_error (GPG_ERR_NO_VALUE);

  *ptr = cert->image + n->off + n->nhdr;
  *length = n->len;
  return 0;
}



/* Return a pointer to the DER encoding of the subject's DN in CERT in
   PTR and the length of that object in LENGTH.  */
gpg_error_t
_ksba_cert_get_subject_dn_ptr (ksba_cert_t cert,
                               unsigned char const **ptr, size_t *length)
{
  asn_node_t n;

  if (!cert || !cert->initialized || !ptr || !length)
    return gpg_error (GPG_ERR_INV_VALUE);

  n = _ksba_asn_find_node (cert->root, "Certificate.tbsCertificate.subject");
  if (!n || !n->down)
    return gpg_error (GPG_ERR_NO_VALUE); /* oops - should be there */
  n = n->down; /* dereference the choice node */
  if (n->off == -1)
    return gpg_error (GPG_ERR_NO_VALUE);
  *ptr = cert->image + n->off;
  *length = n->nhdr + n->len;
  return 0;
}



/* Worker function for get_isssuer and get_subject. */
static gpg_error_t
get_name (ksba_cert_t cert, int idx, int use_subject, char **result)
{
  gpg_error_t err;
  char *p;
  int i;
  const char *oid;
  struct tag_info ti;
  const unsigned char *der;
  size_t off, derlen, seqlen;

  if (!cert || !cert->initialized || !result)
    return gpg_error (GPG_ERR_INV_VALUE);
  if (idx < 0)
    return gpg_error (GPG_ERR_INV_INDEX);

  *result = NULL;
  if (!idx)
    { /* Get the required DN */
      AsnNode n;

      n = _ksba_asn_find_node (cert->root,
                               (use_subject?
                                "Certificate.tbsCertificate.subject":
                                "Certificate.tbsCertificate.issuer") );
      if (!n || !n->down)
        return gpg_error (GPG_ERR_NO_VALUE); /* oops - should be there */
      n = n->down; /* dereference the choice node */
      if (n->off == -1)
        return gpg_error (GPG_ERR_NO_VALUE);
      
      err = _ksba_dn_to_str (cert->image, n, &p);
      if (err)
        return err;
      *result = p;
      return 0;
    }

  /* get {issuer,subject}AltName */
  for (i=0; !(err=ksba_cert_get_extension (cert, i, &oid, NULL,
                                           &off, &derlen)); i++)
    {
      if (!strcmp (oid, (use_subject?
                         oidstr_subjectAltName:oidstr_issuerAltName)))
        break;
    }
  if (err)
      return err; /* no alt name or error*/
  
  der = cert->image + off;


  /* FIXME: We should use _ksba_name_new_from_der and ksba_name_enum here */ 

  err = _ksba_ber_parse_tl (&der, &derlen, &ti);
  if (err)
    return err;
  if ( !(ti.class == CLASS_UNIVERSAL && ti.tag == TYPE_SEQUENCE
         && ti.is_constructed) )
    return gpg_error (GPG_ERR_INV_CERT_OBJ);
  if (ti.ndef)
    return gpg_error (GPG_ERR_NOT_DER_ENCODED);
  seqlen = ti.length;
  if (seqlen > derlen)
    return gpg_error (GPG_ERR_BAD_BER);
  if (!seqlen)
    return gpg_error (GPG_ERR_INV_CERT_OBJ); /* empty sequence is not allowed */

  while (seqlen)
    {
      err = _ksba_ber_parse_tl (&der, &derlen, &ti);
      if (err)
        return err;
      if (ti.class != CLASS_CONTEXT) 
        return gpg_error (GPG_ERR_INV_CERT_OBJ); /* we expected a tag */
      if (ti.ndef)
        return gpg_error (GPG_ERR_NOT_DER_ENCODED);
      if (seqlen < ti.nhdr)
        return gpg_error (GPG_ERR_BAD_BER);
      seqlen -= ti.nhdr;
      if (seqlen < ti.length)
        return gpg_error (GPG_ERR_BAD_BER);
      seqlen -= ti.length;
      if (derlen < ti.length)
        return gpg_error (GPG_ERR_BAD_BER);
      if (!(ti.tag == 1 || ti.tag == 2 || ti.tag == 6))
        ; /* Not a supported tag: Do not change IDX. */
      else if (--idx)
        ; /* not yet at the desired index */
      else if (ti.tag == 1)
        { /* rfc822Name - this is an implicit IA5_STRING */
          p = xtrymalloc (ti.length+3);
          if (!p)
            return gpg_error (GPG_ERR_ENOMEM);
          *p = '<';
          memcpy (p+1, der, ti.length);
          p[ti.length+1] = '>';
          p[ti.length+2] = 0;
          *result = p;
          return 0;
        }
      else if (ti.tag == 2 || ti.tag == 6)
        { /* dNSName or URI - this are implicit IA5_STRINGs */
          char numbuf[20], *numbufp;
          size_t numbuflen;

          numbufp = smklen (numbuf, DIM(numbuf), ti.length, &numbuflen);
          p = xtrymalloc (11 + numbuflen + ti.length + 3);
          if (!p)
            return gpg_error (GPG_ERR_ENOMEM);
          *result = p;
          p = stpcpy (p, ti.tag == 2? "(8:dns-name" : "(3:uri");
          p = stpcpy (p, numbufp);
          memcpy (p, der, ti.length);
          p += ti.length;
          *p++ = ')';
          *p = 0;
          return 0;
        } 

      /* advance pointer */
      der += ti.length;
      derlen -= ti.length;
    }
  
  return gpg_error (GPG_ERR_EOF);     
}



/**
 * ksba_cert_get_issuer:
 * @cert: certificate object
 * 
 * With @idx == 0 this function returns the Distinguished Name (DN) of
 * the certificate issuer which in most cases is a CA.  The format of
 * the returned string is in accordance with RFC-2253.  NULL is
 * returned if the DN is not available which is an error and should
 * have been catched by the certificate reading function.
 * 
 * With @idx > 0 the function may be used to enumerate alternate
 * issuer names. The function returns NULL if there are no more
 * alternate names.  The function does only return alternate names
 * which are recognized by libksba and ignores others.  The format of
 * the returned name is either a RFC-2253 formated one which can be
 * detected by checking whether the first character is letter or
 * digit.  rfc-2822 conform email addresses are returned enclosed in
 * angle brackets, the opening angle bracket should be used to
 * indicate this.  Other formats are returned as an S-Expression in
 * canonical format, so a opening parenthesis may be used to detect
 * this encoding, the name may include binary null characters, so
 * strlen may return a length shorther than actually used, the real
 * length is implictly given by the structure of the S-Exp, an extra
 * null is appended for safety reasons.
 * 
 * The caller must free the returned string using ksba_free() or the
 * function he has registered as a replacement.
 * 
 * Return value: An allocated string or NULL for error.
 **/
char *
ksba_cert_get_issuer (ksba_cert_t cert, int idx)
{
  gpg_error_t err;
  char *name;

  err = get_name (cert, idx, 0, &name);
  if (err)
    {
      cert->last_error = err;
      return NULL;
    }
  return name;
}

/* See ..get_issuer */
char *
ksba_cert_get_subject (ksba_cert_t cert, int idx)
{
  gpg_error_t err;
  char *name;

  err = get_name (cert, idx, 1, &name);
  if (err)
    {
      cert->last_error = err;
      return NULL;
    }
  return name;
}



/**
 * ksba_cert_get_valididy:
 * @cert: certificate object
 * @what: 0 for notBefore, 1 for notAfter
 * @timebuf: Returns the time.
 * 
 * Return the validity object from the certificate.  If no value is
 * available 0 is returned because we can safely assume that this is
 * not a valid date.
 * 
 * Return value: The time value an 0 or an error code.
 **/
gpg_error_t
ksba_cert_get_validity (ksba_cert_t cert, int what, ksba_isotime_t timebuf)
{
  AsnNode n, n2;

  if (!cert || what < 0 || what > 1)
    return gpg_error (GPG_ERR_INV_VALUE);
  *timebuf = 0;
  if (!cert->initialized)
    return gpg_error (GPG_ERR_NO_DATA);
  
  n = _ksba_asn_find_node (cert->root,
        what == 0? "Certificate.tbsCertificate.validity.notBefore"
                 : "Certificate.tbsCertificate.validity.notAfter");
  if (!n)
    return 0; /* no value available */

  /* Fixme: We should remove the choice node and don't use this ugly hack */
  for (n2=n->down; n2; n2 = n2->right)
    {
      if ((n2->type == TYPE_UTC_TIME || n2->type == TYPE_GENERALIZED_TIME)
          && n2->off != -1)
        break;
    }
  n = n2;
  if (!n)
    return 0; /* no value available */

  return_val_if_fail (n->off != -1, gpg_error (GPG_ERR_BUG));

  return _ksba_asntime_to_iso (cert->image + n->off + n->nhdr, n->len,
                               n->type == TYPE_UTC_TIME, timebuf);
}



ksba_sexp_t
ksba_cert_get_public_key (ksba_cert_t cert)
{
  AsnNode n;
  gpg_error_t err;
  ksba_sexp_t string;

  if (!cert)
    return NULL;
  if (!cert->initialized)
    return NULL;

  n = _ksba_asn_find_node (cert->root,
                           "Certificate"
                           ".tbsCertificate.subjectPublicKeyInfo");
  if (!n)
    {
      cert->last_error = gpg_error (GPG_ERR_NO_VALUE);
      return NULL;
    }

  err = _ksba_keyinfo_to_sexp (cert->image + n->off, n->nhdr + n->len,
                               &string);
  if (err)
    {
      cert->last_error = err;
      return NULL;
    }

  return string;
}

/* Return a pointer to the DER encoding of the actual public key
   (i.e. the bit string) in PTR and the length of that object in
   LENGTH.  */
gpg_error_t
_ksba_cert_get_public_key_ptr (ksba_cert_t cert,
                               unsigned char const **ptr, size_t *length)
{
  asn_node_t n;

  if (!cert || !cert->initialized || !ptr || !length)
    return gpg_error (GPG_ERR_INV_VALUE);

  n = _ksba_asn_find_node (cert->root, 
                           "Certificate.tbsCertificate.subjectPublicKeyInfo");
  if (!n || !n->down || !n->down->right)
    return gpg_error (GPG_ERR_NO_VALUE); /* oops - should be there */
  n = n->down->right;
  if (n->off == -1)
    return gpg_error (GPG_ERR_NO_VALUE);
  *ptr = cert->image + n->off + n->nhdr;
  *length = n->len;
  /* Somehow we end up at the preceding NULL value, and not at a
     sequence, we hack it way here. */
  if (*length && !**ptr)
    {
      (*length)--;
      (*ptr)++;
    }

  return 0;
}



ksba_sexp_t
ksba_cert_get_sig_val (ksba_cert_t cert)
{
  AsnNode n, n2;
  gpg_error_t err;
  ksba_sexp_t string;

  if (!cert)
    return NULL;
  if (!cert->initialized)
    return NULL;

  n = _ksba_asn_find_node (cert->root,
                           "Certificate.signatureAlgorithm");
  if (!n)
    {
      cert->last_error = gpg_error (GPG_ERR_NO_VALUE);
      return NULL;
    }
  if (n->off == -1)
    {
/*        fputs ("ksba_cert_get_sig_val problem at node:\n", stderr); */
/*        _ksba_asn_node_dump_all (n, stderr); */
      cert->last_error = gpg_error (GPG_ERR_NO_VALUE);
      return NULL;
    }

  n2 = n->right;
  err = _ksba_sigval_to_sexp (cert->image + n->off,
                              n->nhdr + n->len
                              + ((!n2||n2->off == -1)? 0:(n2->nhdr+n2->len)),
                              &string);
  if (err)
    {
      cert->last_error = err;
      return NULL;
    }

  return string;
}


/* Read all extensions into the cache */
static gpg_error_t
read_extensions (ksba_cert_t cert) 
{
  AsnNode start, n;
  int count;

  assert (!cert->cache.extns_valid);
  assert (!cert->cache.extns);

  start = _ksba_asn_find_node (cert->root,
                               "Certificate.tbsCertificate.extensions..");
  for (count=0, n=start; n; n = n->right)
    count++;
  if (!count)
    {
      cert->cache.n_extns = 0;
      cert->cache.extns_valid = 1;
      return 0; /* no extensions at all */
    }
  cert->cache.extns = xtrycalloc (count, sizeof *cert->cache.extns);
  if (!cert->cache.extns)
    return gpg_error (GPG_ERR_ENOMEM);
  cert->cache.n_extns = count;

  {
    for (count=0; start; start = start->right, count++)
      {
        n = start->down;
        if (!n || n->type != TYPE_OBJECT_ID)
          goto no_value;
        
        cert->cache.extns[count].oid = _ksba_oid_node_to_str (cert->image, n);
        if (!cert->cache.extns[count].oid)
          goto no_value;
        
        n = n->right;
        if (n && n->type == TYPE_BOOLEAN)
          {
            if (n->off != -1 && n->len && cert->image[n->off + n->nhdr])
              cert->cache.extns[count].crit = 1;
            n = n->right;
          }
        
        if (!n || n->type != TYPE_OCTET_STRING || n->off == -1)
          goto no_value;
        
        cert->cache.extns[count].off = n->off + n->nhdr;
        cert->cache.extns[count].len = n->len;
      }
    
    assert (count == cert->cache.n_extns);
    cert->cache.extns_valid = 1;
    return 0;
    
  no_value:
    for (count=0; count < cert->cache.n_extns; count++)
      xfree (cert->cache.extns[count].oid);
    xfree (cert->cache.extns);
    cert->cache.extns = NULL;
    return gpg_error (GPG_ERR_NO_VALUE);
  }
}


/* Return information about the IDX nth extension */
gpg_error_t
ksba_cert_get_extension (ksba_cert_t cert, int idx,
                         char const **r_oid, int *r_crit,
                         size_t *r_deroff, size_t *r_derlen)
{
  gpg_error_t err;

  if (!cert)
    return gpg_error (GPG_ERR_INV_VALUE);
  if (!cert->initialized)
    return gpg_error (GPG_ERR_NO_DATA);

  if (!cert->cache.extns_valid)
    {
      err = read_extensions (cert);
      if (err)
        return err;
      assert (cert->cache.extns_valid);
    }

  if (idx == cert->cache.n_extns)
    return gpg_error (GPG_ERR_EOF); /* No more extensions. */

  if (idx < 0 || idx >= cert->cache.n_extns)
    return gpg_error (GPG_ERR_INV_INDEX);
  
  if (r_oid)
    *r_oid = cert->cache.extns[idx].oid;
  if (r_crit)
    *r_crit = cert->cache.extns[idx].crit;
  if (r_deroff)
    *r_deroff = cert->cache.extns[idx].off;
  if (r_derlen)
    *r_derlen = cert->cache.extns[idx].len;
  return 0;
}



/* Return information on the basicConstraint (2.5.19.19) of CERT.
   R_CA receives true if this is a CA and only in that case R_PATHLEN
   is set to the maximim certification path length or -1 if there is
   nosuch limitation */
gpg_error_t
ksba_cert_is_ca (ksba_cert_t cert, int *r_ca, int *r_pathlen)
{
  gpg_error_t err;
  const char *oid;
  int idx, crit;
  size_t off, derlen, seqlen;
  const unsigned char *der;
  struct tag_info ti;
  unsigned long value;

  /* set default values */
  if (r_ca)
    *r_ca = 0;
  if (r_pathlen)
    *r_pathlen = -1;
  for (idx=0; !(err=ksba_cert_get_extension (cert, idx, &oid, &crit,
                                             &off, &derlen)); idx++)
    {
      if (!strcmp (oid, oidstr_basicConstraints))
        break;
    }
  if (gpg_err_code (err) == GPG_ERR_EOF)
      return 0; /* no such constraint */
  if (err)
    return err;
    
  /* check that there is only one */
  for (idx++; !(err=ksba_cert_get_extension (cert, idx, &oid, NULL,
                                             NULL, NULL)); idx++)
    {
      if (!strcmp (oid, oidstr_basicConstraints))
        return gpg_error (GPG_ERR_DUP_VALUE); 
    }
  
  der = cert->image + off;
 
  err = _ksba_ber_parse_tl (&der, &derlen, &ti);
  if (err)
    return err;
  if ( !(ti.class == CLASS_UNIVERSAL && ti.tag == TYPE_SEQUENCE
         && ti.is_constructed) )
    return gpg_error (GPG_ERR_INV_CERT_OBJ);
  if (ti.ndef)
    return gpg_error (GPG_ERR_NOT_DER_ENCODED);
  seqlen = ti.length;
  if (seqlen > derlen)
    return gpg_error (GPG_ERR_BAD_BER);
  if (!seqlen)
    return 0; /* an empty sequence is allowed because both elements
                 are optional */

  err = _ksba_ber_parse_tl (&der, &derlen, &ti);
  if (err)
    return err;
  if (seqlen < ti.nhdr)
    return gpg_error (GPG_ERR_BAD_BER);
  seqlen -= ti.nhdr;
  if (seqlen < ti.length)
    return gpg_error (GPG_ERR_BAD_BER); 
  seqlen -= ti.length;

  if (ti.class == CLASS_UNIVERSAL && ti.tag == TYPE_BOOLEAN)
    { 
      if (ti.length != 1)
        return gpg_error (GPG_ERR_ENCODING_PROBLEM);
      if (r_ca)
        *r_ca = !!*der;
      der++; derlen--;
      if (!seqlen)
        return 0; /* ready (no pathlength) */

      err = _ksba_ber_parse_tl (&der, &derlen, &ti);
      if (err)
        return err;
      if (seqlen < ti.nhdr)
        return gpg_error (GPG_ERR_BAD_BER);
      seqlen -= ti.nhdr;
      if (seqlen < ti.length)
        return gpg_error (GPG_ERR_BAD_BER);
      seqlen -= ti.length;
    }

  if (!(ti.class == CLASS_UNIVERSAL && ti.tag == TYPE_INTEGER))
    return gpg_error (GPG_ERR_INV_CERT_OBJ);
  
  for (value=0; ti.length; ti.length--)
    {
      value <<= 8;
      value |= (*der++) & 0xff; 
      derlen--;
    }
  if (r_pathlen)
    *r_pathlen = value;

  /* if the extension is marked as critical and any stuff is still
     left we better return an error */
  if (crit && seqlen)
    return gpg_error (GPG_ERR_INV_CERT_OBJ);

  return 0;
}



/* Get the key usage flags. The function returns Ksba_No_Data if no
   key usage is specified. */
gpg_error_t
ksba_cert_get_key_usage (ksba_cert_t cert, unsigned int *r_flags)
{
  gpg_error_t err;
  const char *oid;
  int idx, crit;
  size_t off, derlen;
  const unsigned char *der;
  struct tag_info ti;
  unsigned int bits, mask;
  int i, unused, full;

  if (!r_flags)
    return gpg_error (GPG_ERR_INV_VALUE);
  *r_flags = 0;
  for (idx=0; !(err=ksba_cert_get_extension (cert, idx, &oid, &crit,
                                             &off, &derlen)); idx++)
    {
      if (!strcmp (oid, oidstr_keyUsage))
        break;
    }
  if (gpg_err_code (err) == GPG_ERR_EOF
      || gpg_err_code (err) == GPG_ERR_NO_VALUE)
      return gpg_error (GPG_ERR_NO_DATA); /* no key usage */
  if (err)
    return err;
    
  /* check that there is only one */
  for (idx++; !(err=ksba_cert_get_extension (cert, idx, &oid, NULL,
                                             NULL, NULL)); idx++)
    {
      if (!strcmp (oid, oidstr_keyUsage))
        return gpg_error (GPG_ERR_DUP_VALUE); 
    }
  
  der = cert->image + off;
 
  err = _ksba_ber_parse_tl (&der, &derlen, &ti);
  if (err)
    return err;
  if ( !(ti.class == CLASS_UNIVERSAL && ti.tag == TYPE_BIT_STRING
         && !ti.is_constructed) )
    return gpg_error (GPG_ERR_INV_CERT_OBJ);
  if (ti.ndef)
    return gpg_error (GPG_ERR_NOT_DER_ENCODED);
  if (!ti.length || ti.length > derlen)
    return gpg_error (GPG_ERR_ENCODING_PROBLEM); /* number of unused bits missing */
  unused = *der++; derlen--;
  ti.length--;
  if ((!ti.length && unused) || unused/8 > ti.length)
    return gpg_error (GPG_ERR_ENCODING_PROBLEM);

  full = ti.length - (unused+7)/8;
  unused %= 8;
  mask = 0;
  for (i=1; unused; i <<= 1, unused--)
    mask |= i;

  /* the first octet */
  if (!ti.length)
    return 0; /* no bits set */
  bits = *der++; derlen--; ti.length--;
  if (full)
    full--;
  else {
    bits &= ~mask;
    mask = 0; 
  }
  if (bits & 0x80)
    *r_flags |= KSBA_KEYUSAGE_DIGITAL_SIGNATURE;
  if (bits & 0x40)
    *r_flags |= KSBA_KEYUSAGE_NON_REPUDIATION;
  if (bits & 0x20)
    *r_flags |= KSBA_KEYUSAGE_KEY_ENCIPHERMENT;
  if (bits & 0x10)
    *r_flags |= KSBA_KEYUSAGE_DATA_ENCIPHERMENT;
  if (bits & 0x08)
    *r_flags |= KSBA_KEYUSAGE_KEY_AGREEMENT;
  if (bits & 0x04)
    *r_flags |= KSBA_KEYUSAGE_KEY_CERT_SIGN;
  if (bits & 0x02)
    *r_flags |= KSBA_KEYUSAGE_CRL_SIGN;
  if (bits & 0x01)
    *r_flags |= KSBA_KEYUSAGE_ENCIPHER_ONLY;

  /* the second octet */
  if (!ti.length)
    return 0; /* no bits set */
  bits = *der++; derlen--; ti.length--;
  if (full)
    full--;
  else {
    bits &= mask;
    mask = ~0; 
  }
  if (bits & 0x80)
    *r_flags |= KSBA_KEYUSAGE_DECIPHER_ONLY;

  return 0;
}



/* Note, that this helper is also used for ext_key_usage. */
static gpg_error_t
append_cert_policy (char **policies, const char *oid, int crit)
{
  char *p;

  if (!*policies)
    {
      *policies = xtrymalloc (strlen (oid) + 4);
      if (!*policies)
        return gpg_error (GPG_ERR_ENOMEM);
      p = *policies;
    }
  else
    {
      char *tmp = xtryrealloc (*policies,
                               strlen(*policies) + 1 + strlen (oid) + 4);
      if (!tmp)
        return gpg_error (GPG_ERR_ENOMEM);
      *policies = tmp;
      p = stpcpy (tmp+strlen (tmp), "\n");;
    }
  
  strcpy (stpcpy (p, oid), crit? ":C:": ":N:");
  return 0;   
}


/* Return a string with the certificatePolicies delimited by
   linefeeds.  The return values may be extended to carry more
   information er line, so the caller should only use the first
   white-space delimited token per line.  The function returns
   GPG_ERR_NO_DATA when this extension is not used.  Caller must free
   the returned value.  */
gpg_error_t
ksba_cert_get_cert_policies (ksba_cert_t cert, char **r_policies)
{
  gpg_error_t err;
  const char *oid;
  int idx, crit;
  size_t off, derlen, seqlen;
  const unsigned char *der;
  struct tag_info ti;

  if (!cert || !r_policies)
    return gpg_error (GPG_ERR_INV_VALUE);
  *r_policies = NULL;

  for (idx=0; !(err=ksba_cert_get_extension (cert, idx, &oid, &crit,
                                             &off, &derlen)); idx++)
    {
      if (!strcmp (oid, oidstr_certificatePolicies))
        {
          char *suboid;

          der = cert->image + off;
 
          err = _ksba_ber_parse_tl (&der, &derlen, &ti);
          if (err)
            goto leave;
          if ( !(ti.class == CLASS_UNIVERSAL && ti.tag == TYPE_SEQUENCE
                 && ti.is_constructed) )
            {
              err = gpg_error (GPG_ERR_INV_CERT_OBJ);
              goto leave;
            }
          if (ti.ndef)
            {
              err = gpg_error (GPG_ERR_NOT_DER_ENCODED);
              goto leave;
            }
          seqlen = ti.length;
          if (seqlen > derlen)
            {
              err = gpg_error (GPG_ERR_BAD_BER);
              goto leave;
            }
          while (seqlen)
            {
              size_t seqseqlen;

              err = _ksba_ber_parse_tl (&der, &derlen, &ti);
              if (err)
                goto leave;
              if ( !(ti.class == CLASS_UNIVERSAL && ti.tag == TYPE_SEQUENCE
                     && ti.is_constructed) )
                {
                  err = gpg_error (GPG_ERR_INV_CERT_OBJ);
                  goto leave;
                }
              if (ti.ndef)
                {
                  err = gpg_error (GPG_ERR_NOT_DER_ENCODED);
                  goto leave;
                }
              if (!ti.length)
                {
                  err = gpg_error (GPG_ERR_INV_CERT_OBJ); /* no empty inner SEQ */
                  goto leave;
                }
              if (ti.nhdr+ti.length > seqlen)
                {
                  err = gpg_error (GPG_ERR_BAD_BER);
                  goto leave;
                }
              seqlen -= ti.nhdr + ti.length;
              seqseqlen = ti.length;

              err = _ksba_ber_parse_tl (&der, &derlen, &ti);
              if (err)
                goto leave;
              if ( !(ti.class == CLASS_UNIVERSAL && ti.tag == TYPE_OBJECT_ID))
                {
                  err = gpg_error (GPG_ERR_INV_CERT_OBJ);
                  goto leave;
                }
              if (ti.nhdr+ti.length > seqseqlen)
                {
                  err = gpg_error (GPG_ERR_BAD_BER);
                  goto leave;
                }
              seqseqlen -= ti.nhdr;
              
              suboid = ksba_oid_to_str (der, ti.length);
              if (!suboid)
                {
                  err = gpg_error (GPG_ERR_ENOMEM);
                  goto leave;
                }
              der       += ti.length;
              derlen    -= ti.length;
              seqseqlen -= ti.length;

              err = append_cert_policy (r_policies, suboid, crit);
              xfree (suboid);
              if (err)
                goto leave;

              /* skip the rest of the seq which is more or less optional */
              der    += seqseqlen;
              derlen -= seqseqlen;
            }
        }
    }

  if (gpg_err_code (err) == GPG_ERR_EOF)
    err = 0;
  if (!*r_policies || gpg_err_code (err) == GPG_ERR_NO_VALUE)
    err = gpg_error (GPG_ERR_NO_DATA);

 leave:
  if (err)
    {
      xfree (*r_policies);
      *r_policies = NULL;
    }
  return err;
}


/* Return a string with the extendedKeyUsageOIDs delimited by
   linefeeds.  The return values may be extended to carry more
   information per line, so the caller should only use the first
   white-space delimited token per line.  The function returns
   GPG_ERR_NO_DATA when this extension is not used.  Caller must free
   the returned value.  */
gpg_error_t
ksba_cert_get_ext_key_usages (ksba_cert_t cert, char **result)
{
  gpg_error_t err;
  const char *oid;
  int idx, crit;
  size_t off, derlen;
  const unsigned char *der;
  struct tag_info ti;

  if (!cert || !result)
    return gpg_error (GPG_ERR_INV_VALUE);
  *result = NULL;

  for (idx=0; !(err=ksba_cert_get_extension (cert, idx, &oid, &crit,
                                             &off, &derlen)); idx++)
    {
      if (!strcmp (oid, oidstr_extKeyUsage))
        {
          char *suboid;

          der = cert->image + off;
 
          err = _ksba_ber_parse_tl (&der, &derlen, &ti);
          if (err)
            goto leave;
          if ( !(ti.class == CLASS_UNIVERSAL && ti.tag == TYPE_SEQUENCE
                 && ti.is_constructed) )
            {
              err = gpg_error (GPG_ERR_INV_CERT_OBJ);
              goto leave;
            }
          if (ti.ndef)
            {
              err = gpg_error (GPG_ERR_NOT_DER_ENCODED);
              goto leave;
            }
          if (ti.length > derlen)
            {
              err = gpg_error (GPG_ERR_BAD_BER);
              goto leave;
            }
          while (derlen)
            {
              err = _ksba_ber_parse_tl (&der, &derlen, &ti);
              if (err)
                goto leave;
              if ( !(ti.class == CLASS_UNIVERSAL && ti.tag == TYPE_OBJECT_ID))
                {
                  err = gpg_error (GPG_ERR_INV_CERT_OBJ);
                  goto leave;
                }
              
              suboid = ksba_oid_to_str (der, ti.length);
              if (!suboid)
                {
                  err = gpg_error (GPG_ERR_ENOMEM);
                  goto leave;
                }
              der       += ti.length;
              derlen    -= ti.length;

              err = append_cert_policy (result, suboid, crit);
              xfree (suboid);
              if (err)
                goto leave;
            }
        }
    }

  if (gpg_err_code (err) == GPG_ERR_EOF)
    err = 0;
  if (!*result || gpg_err_code (err) == GPG_ERR_NO_VALUE)
    err = gpg_error (GPG_ERR_NO_DATA);

 leave:
  if (err)
    {
      xfree (*result);
      *result = NULL;
    }
  return err;
}




/* Helper function for ksba_cert_get_crl_dist_point */
static gpg_error_t
parse_distribution_point (const unsigned char *der, size_t derlen, 
                          ksba_name_t *distpoint, ksba_name_t *issuer,
                          ksba_crl_reason_t *reason)
{
  gpg_error_t err;
  struct tag_info ti;

  err = _ksba_ber_parse_tl (&der, &derlen, &ti);
  if (err)
    return err;
  if (ti.class != CLASS_CONTEXT) 
    return gpg_error (GPG_ERR_INV_CERT_OBJ); /* we expected a tag */
  if (ti.ndef)
    return gpg_error (GPG_ERR_NOT_DER_ENCODED);
  if (derlen < ti.length)
    return gpg_error (GPG_ERR_BAD_BER);

  if (ti.tag == 0 && derlen)
    { /* distributionPointName */
      err = _ksba_ber_parse_tl (&der, &derlen, &ti);
      if (err)
        return err;
      if (ti.class != CLASS_CONTEXT) 
        return gpg_error (GPG_ERR_INV_CERT_OBJ); /* we expected a tag */
      if (ti.ndef)
        return gpg_error (GPG_ERR_NOT_DER_ENCODED);
      if (derlen < ti.nhdr)
        return gpg_error (GPG_ERR_BAD_BER);
      if (derlen < ti.length)
        return gpg_error (GPG_ERR_BAD_BER);

      if (ti.tag == 0)
        {
          if (distpoint)
            {
              err = _ksba_name_new_from_der (distpoint, der, ti.length);
              if (err)
                return err;
            }
        }
      else
        {
          /* We don't support nameRelativeToCRLIssuer yet*/
        }
      der += ti.length;
      derlen -= ti.length;

      if (!derlen)
        return 0;

      /* read the next optional element */
      err = _ksba_ber_parse_tl (&der, &derlen, &ti);
      if (err)
        return err;
      if (ti.class != CLASS_CONTEXT) 
        return gpg_error (GPG_ERR_INV_CERT_OBJ); /* we expected a tag */
      if (ti.ndef)
        return gpg_error (GPG_ERR_NOT_DER_ENCODED);
      if (derlen < ti.length)
        return gpg_error (GPG_ERR_BAD_BER);
    }

  if (ti.tag == 1 && derlen)
    { /* reasonFlags */
      unsigned int bits, mask;
      int i, unused, full;
      
      unused = *der++; derlen--;
      ti.length--;
      if ((!ti.length && unused) || unused/8 > ti.length)
        return gpg_error (GPG_ERR_ENCODING_PROBLEM);

      full = ti.length - (unused+7)/8;
      unused %= 8;
      mask = 0;
      for (i=1; unused; i <<= 1, unused--)
        mask |= i;
      
      /* we are only required to look at the first octect */
      if (ti.length && reason)
        {
          bits = *der; 
          if (full)
            full--;
          else {
            bits &= ~mask;
            mask = 0; 
          }
          
          if (bits & 0x80)
            *reason |= KSBA_CRLREASON_UNSPECIFIED;
          if (bits & 0x40)
            *reason |= KSBA_CRLREASON_KEY_COMPROMISE;
          if (bits & 0x20)
            *reason |= KSBA_CRLREASON_CA_COMPROMISE;
          if (bits & 0x10)
            *reason |= KSBA_CRLREASON_AFFILIATION_CHANGED;
          if (bits & 0x08)
            *reason |= KSBA_CRLREASON_SUPERSEDED;
          if (bits & 0x04)
            *reason |= KSBA_CRLREASON_CESSATION_OF_OPERATION;
          if (bits & 0x02)
            *reason |= KSBA_CRLREASON_CERTIFICATE_HOLD;
        }
        
      der += ti.length;
      derlen -= ti.length;

      if (!derlen)
        return 0;

      /* read the next optional element */
      err = _ksba_ber_parse_tl (&der, &derlen, &ti);
      if (err)
        return err;
      if (ti.class != CLASS_CONTEXT) 
        return gpg_error (GPG_ERR_INV_CERT_OBJ); /* we expected a tag */
      if (ti.ndef)
        return gpg_error (GPG_ERR_NOT_DER_ENCODED);
      if (derlen < ti.length)
        return gpg_error (GPG_ERR_BAD_BER);
    }

  if (ti.tag == 2 && derlen)
    { /* crlIssuer */
      if (issuer)
        {
          err = _ksba_name_new_from_der (issuer, der, ti.length);
          if (err)
            return err;
        }

      der += ti.length;
      derlen -= ti.length;
    }

  if (derlen)
    return gpg_error (GPG_ERR_INV_CERT_OBJ);

  return 0;
}

/* Return the CRLDistPoints given in the cert extension.  IDX should
   be iterated started from 0 until the function returns -1.
   R_DISTPOINT returns a ksba_name_t object with the distribution point
   name(s) the return value may be NULL to indicate that this name is
   not available.  R_ISSUER returns the CRL issuer; if the returned
   value is NULL the caller should assume that the CRL issuer is the
   same as the certificate issuer.  R_REASON returns the reason for
   the CRL.  This is a bit encoded value with no bit set if this has
   not been specified in the cert.

   The caller may pass NULL to any of the pointer arguments if he is
   not interested in this value.  The return values for R_DISTPOINT
   and R_ISSUER must be released by the caller using
   ksba_name_release(). */

gpg_error_t 
ksba_cert_get_crl_dist_point (ksba_cert_t cert, int idx,
                              ksba_name_t *r_distpoint,
                              ksba_name_t *r_issuer,
                              ksba_crl_reason_t *r_reason)
{
  gpg_error_t err;
  const char *oid;
  size_t off, derlen;
  int myidx, crit;

  if (r_distpoint)
    *r_distpoint = NULL;
  if (r_issuer)
    *r_issuer = NULL;
  if (r_reason)
    *r_reason = 0;

  for (myidx=0; !(err=ksba_cert_get_extension (cert, myidx, &oid, &crit,
                                               &off, &derlen)); myidx++)
    {
      if (!strcmp (oid, oidstr_crlDistributionPoints))
        {
          const unsigned char *der;
          struct tag_info ti;
           size_t seqlen;

          der = cert->image + off;
          
          err = _ksba_ber_parse_tl (&der, &derlen, &ti);
          if (err)
            return err;
          if ( !(ti.class == CLASS_UNIVERSAL && ti.tag == TYPE_SEQUENCE
                 && ti.is_constructed) )
            return gpg_error (GPG_ERR_INV_CERT_OBJ);
          if (ti.ndef)
            return gpg_error (GPG_ERR_NOT_DER_ENCODED);
          seqlen = ti.length;
          if (seqlen > derlen)
            return gpg_error (GPG_ERR_BAD_BER);

          /* Note: an empty sequence is actually not allowed but we
             better don't care */

          while (seqlen)
            {
              err = _ksba_ber_parse_tl (&der, &derlen, &ti);
              if (err)
                return err;
              if ( !(ti.class == CLASS_UNIVERSAL && ti.tag == TYPE_SEQUENCE
                     && ti.is_constructed) )
                return gpg_error (GPG_ERR_INV_CERT_OBJ);
              if (derlen < ti.length)
                return gpg_error (GPG_ERR_BAD_BER);
              if (seqlen < ti.nhdr)
                return gpg_error (GPG_ERR_BAD_BER);
              seqlen -= ti.nhdr;
              if (seqlen < ti.length)
                return gpg_error (GPG_ERR_BAD_BER); 

              if (idx)
                { /* skip because we are not yet at the desired index */
                  der    += ti.length;
                  derlen -= ti.length;
                  seqlen -= ti.length;
                  idx--;
                  continue;  
                }
              
              if (!ti.length)
                return 0;  

              err = parse_distribution_point (der, ti.length, 
                                              r_distpoint, r_issuer, r_reason);
              if (err && r_distpoint)
                {
                  ksba_name_release (*r_distpoint);
                  *r_distpoint = NULL;
                }
              if (err && r_issuer)
                {
                  ksba_name_release (*r_issuer);
                  *r_issuer = NULL;
                }
              if (err && r_reason)
                *r_reason = 0;
              
              return err;
            }
        }
    }

  return err;
}


/* Return the authorityKeyIdentifier in R_NAME and R_SERIAL or/and in
   R_KEYID.  GPG_ERR_NO_DATA is returned if no authorityKeyIdentifier
   or only one using the keyIdentifier method is available and R_KEYID
   is NULL. */
gpg_error_t 
ksba_cert_get_auth_key_id (ksba_cert_t cert,
                           ksba_sexp_t *r_keyid,
                           ksba_name_t *r_name,
                           ksba_sexp_t *r_serial)
{
  gpg_error_t err;
  const char *oid;
  size_t off, derlen;
  const unsigned char *der;
  const unsigned char *keyid_der = NULL;
  size_t keyid_derlen = 0;
  int idx, crit;
  struct tag_info ti;
  char numbuf[30];
  size_t numbuflen;
 
  if (r_keyid)
    *r_keyid = NULL;
  if (!r_name || !r_serial)
    return gpg_error (GPG_ERR_INV_VALUE);
  *r_name = NULL;
  *r_serial = NULL;

  for (idx=0; !(err=ksba_cert_get_extension (cert, idx, &oid, &crit,
                                             &off, &derlen)); idx++)
    {
      if (!strcmp (oid, oidstr_authorityKeyIdentifier))
        break;
    }
  if (gpg_err_code (err) == GPG_ERR_EOF
      || gpg_err_code (err) == GPG_ERR_NO_VALUE)
    return gpg_error (GPG_ERR_NO_DATA); /* not available */
  if (err)
    return err;
    
  /* check that there is only one */
  for (idx++; !(err=ksba_cert_get_extension (cert, idx, &oid, NULL,
                                             NULL, NULL)); idx++)
    {
      if (!strcmp (oid, oidstr_authorityKeyIdentifier))
        return gpg_error (GPG_ERR_DUP_VALUE); 
    }
  
  der = cert->image + off;

  err = _ksba_ber_parse_tl (&der, &derlen, &ti);
  if (err)
    return err;
  if ( !(ti.class == CLASS_UNIVERSAL && ti.tag == TYPE_SEQUENCE
         && ti.is_constructed) )
    return gpg_error (GPG_ERR_INV_CERT_OBJ);
  if (ti.ndef)
    return gpg_error (GPG_ERR_NOT_DER_ENCODED);
  if (ti.length > derlen)
    return gpg_error (GPG_ERR_BAD_BER);

  err = _ksba_ber_parse_tl (&der, &derlen, &ti);
  if (err)
    return err;
  if (ti.class != CLASS_CONTEXT) 
    return gpg_error (GPG_ERR_INV_CERT_OBJ); /* we expected a tag */
  if (ti.ndef)
    return gpg_error (GPG_ERR_NOT_DER_ENCODED);
  if (derlen < ti.length)
    return gpg_error (GPG_ERR_BAD_BER);

  if (ti.tag == 0)
    { /* keyIdentifier:  Save it away and skip over it. */
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
        return gpg_error (GPG_ERR_INV_CERT_OBJ); /* we expected a tag */
      if (ti.ndef)
        return gpg_error (GPG_ERR_NOT_DER_ENCODED);
      if (derlen < ti.length)
        return gpg_error (GPG_ERR_BAD_BER);
    }

  if (ti.tag != 1 || !derlen)
    return gpg_error (GPG_ERR_INV_CERT_OBJ);

  err = _ksba_name_new_from_der (r_name, der, ti.length);
  if (err)
    return err;

  der += ti.length;
  derlen -= ti.length;

  /* fixme: we should release r_name before returning on error */
  err = _ksba_ber_parse_tl (&der, &derlen, &ti);
  if (err)
    return err;
  if (ti.class != CLASS_CONTEXT) 
    return gpg_error (GPG_ERR_INV_CERT_OBJ); /* we expected a tag */
  if (ti.ndef)
    return gpg_error (GPG_ERR_NOT_DER_ENCODED);
  if (derlen < ti.length)
    return gpg_error (GPG_ERR_BAD_BER);

  if (ti.tag != 2 || !derlen)
    return gpg_error (GPG_ERR_INV_CERT_OBJ);
 
  sprintf (numbuf,"(%u:", (unsigned int)ti.length);
  numbuflen = strlen (numbuf);
  *r_serial = xtrymalloc (numbuflen + ti.length + 2);
  if (!*r_serial)
    return gpg_error (GPG_ERR_ENOMEM);
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


/* Return a simple octet string extension at the object identifier OID
   from certificate CERT.  The data is return as a simple S-expression
   and stored at R_DATA.  Returns 0 on success or an error code.
   common error codes are: GPG_ERR_NO_DATA if no such extension is
   available, GPG_ERR_DUP_VALUE if more than one is available.  If
   R_CRIT is not NULL, the critical extension flag will be stored at
   that address. */
static gpg_error_t 
get_simple_octet_string_ext (ksba_cert_t cert, const char *oid,
                             int *r_crit, ksba_sexp_t *r_data)
{
  gpg_error_t err;
  const char *tmpoid;
  size_t off, derlen;
  const unsigned char *der;
  int idx, crit;
  struct tag_info ti;
  char numbuf[30];
  size_t numbuflen;
 
  if (!r_data)
    return gpg_error (GPG_ERR_INV_VALUE);
  *r_data = NULL;

  for (idx=0; !(err=ksba_cert_get_extension (cert, idx, &tmpoid, &crit,
                                             &off, &derlen)); idx++)
    {
      if (!strcmp (tmpoid, oid))
        break;
    }
  if (err)
    {
      if (gpg_err_code (err) == GPG_ERR_EOF
          || gpg_err_code (err) == GPG_ERR_NO_VALUE)
        return gpg_error (GPG_ERR_NO_DATA);
      return err;
    }
    
  /* Check that there is only one */
  for (idx++; !(err=ksba_cert_get_extension (cert, idx, &tmpoid, NULL,
                                             NULL, NULL)); idx++)
    {
      if (!strcmp (tmpoid, oid))
        return gpg_error (GPG_ERR_DUP_VALUE); 
    }
  
  der = cert->image + off;

  err = _ksba_ber_parse_tl (&der, &derlen, &ti);
  if (err)
    return err;
  if ( !(ti.class == CLASS_UNIVERSAL && ti.tag == TYPE_OCTET_STRING
         && !ti.is_constructed) )
    return gpg_error (GPG_ERR_INV_CERT_OBJ);
  if (ti.ndef)
    return gpg_error (GPG_ERR_NOT_DER_ENCODED);
  if (ti.length > derlen)
    return gpg_error (GPG_ERR_BAD_BER);
  if (ti.length != derlen)
    return gpg_error (GPG_ERR_INV_CERT_OBJ); /* Garbage follows. */

  sprintf (numbuf,"(%u:", (unsigned int)ti.length);
  numbuflen = strlen (numbuf);
  *r_data = xtrymalloc (numbuflen + ti.length + 2);
  if (!*r_data)
    return gpg_error (GPG_ERR_ENOMEM);
  strcpy (*r_data, numbuf);
  memcpy (*r_data+numbuflen, der, ti.length);
  (*r_data)[numbuflen + ti.length] = ')';
  (*r_data)[numbuflen + ti.length + 1] = 0;
  if (r_crit)
    *r_crit = crit;
  return 0;
}


/* Return the subjectKeyIdentifier extension as a simple allocated
   S-expression at the address of R_KEYID. 0 is returned on success,
   GPG_ERR_NO_DATA if no such extension is available or any other
   error code.  If R_CRIT is not passed as NULL, the criticla flag of
   this is extension is stored there. */
gpg_error_t 
ksba_cert_get_subj_key_id (ksba_cert_t cert, int *r_crit, ksba_sexp_t *r_keyid)
{
  return get_simple_octet_string_ext (cert, oidstr_subjectKeyIdentifier,
                                      r_crit, r_keyid);
}



/* MODE 0 := authorityInfoAccess
        1 := subjectInfoAccess

   Caller must release METHOD and LOCATION if the function returned
   with success; on error both variables will point to NULL.
 */   
static gpg_error_t
get_info_access (ksba_cert_t cert, int idx, int mode,
                 char **method, ksba_name_t *location)
{
  gpg_error_t err;
  const char *oid;
  size_t off, derlen;
  int myidx, crit;

  *method = NULL;
  *location = NULL;

  if (!cert || !cert->initialized)
    return gpg_error (GPG_ERR_INV_VALUE);
  if (idx < 0)
    return gpg_error (GPG_ERR_INV_INDEX);

  for (myidx=0; !(err=ksba_cert_get_extension (cert, myidx, &oid, &crit,
                                               &off, &derlen)); myidx++)
    {
      if (!strcmp (oid,(mode == 0)? oidstr_authorityInfoAccess
                                  : oidstr_subjectInfoAccess) )
        {
          const unsigned char *der;
          struct tag_info ti;
          size_t seqlen;

          der = cert->image + off;

          /* What we are going to parse is:
           *   
           *    AuthorityInfoAccessSyntax  ::=
           *            SEQUENCE SIZE (1..MAX) OF AccessDescription
           *   
           *    AccessDescription  ::=  SEQUENCE {
           *            accessMethod          OBJECT IDENTIFIER,
           *            accessLocation        GeneralName  }
           */
          err = _ksba_ber_parse_tl (&der, &derlen, &ti);
          if (err)
            return err;
          if ( !(ti.class == CLASS_UNIVERSAL && ti.tag == TYPE_SEQUENCE
                 && ti.is_constructed) )
            return gpg_error (GPG_ERR_INV_CERT_OBJ);
          if (ti.ndef)
            return gpg_error (GPG_ERR_NOT_DER_ENCODED);
          seqlen = ti.length;
          if (seqlen > derlen)
            return gpg_error (GPG_ERR_BAD_BER);

          /* Note: an empty sequence is actually not allowed but we
             better don't care. */

          while (seqlen)
            {
              err = _ksba_ber_parse_tl (&der, &derlen, &ti);
              if (err)
                return err;
              if ( !(ti.class == CLASS_UNIVERSAL && ti.tag == TYPE_SEQUENCE
                     && ti.is_constructed) )
                return gpg_error (GPG_ERR_INV_CERT_OBJ);
              if (derlen < ti.length)
                return gpg_error (GPG_ERR_BAD_BER);
              if (seqlen < ti.nhdr)
                return gpg_error (GPG_ERR_BAD_BER);
              seqlen -= ti.nhdr;
              if (seqlen < ti.length)
                return gpg_error (GPG_ERR_BAD_BER); 

              if (idx)
                { /* Skip because we are not yet at the desired index. */
                  der    += ti.length;
                  derlen -= ti.length;
                  seqlen -= ti.length;
                  idx--;
                  continue;  
                }
              /* We only need the next object, thus we can (and
                 actually need to) limit the DERLEN to the length of
                 the current sequence. */
              derlen = ti.length;
              if (!derlen)
                return gpg_error (GPG_ERR_INV_CERT_OBJ);

              err = _ksba_ber_parse_tl (&der, &derlen, &ti);
              if (err)
                return err;

              if ( !(ti.class == CLASS_UNIVERSAL && ti.tag == TYPE_OBJECT_ID
                     && !ti.is_constructed))
                return gpg_error (GPG_ERR_INV_CERT_OBJ);
              if (ti.ndef)
                return gpg_error (GPG_ERR_NOT_DER_ENCODED);
              if (derlen < ti.length)
                return gpg_error (GPG_ERR_BAD_BER);

              *method = ksba_oid_to_str (der, ti.length);
              if (!*method)
                return gpg_error (GPG_ERR_ENOMEM);
              der       += ti.length;
              derlen    -= ti.length;

              err = _ksba_name_new_from_der (location, der, derlen);
              if (err)
                {
                  ksba_free (*method);
                  *method = NULL;
                  return err;
                }
              return 0;
            }
        }
    }

  return err;
}


/* Return the authorityInfoAccess attributes. IDX should be iterated
   starting from 0 until the function returns GPG_ERR_EOF.  R_METHOD
   returns an allocated string with the OID of one item and R_LOCATION
   return the GeneralName for that OID.  The return values for
   R_METHOD and R_LOCATION must be released by the caller unless the
   function returned an error; the function will however make sure
   that R_METHOD and R_LOCATION will point to NULL if the function
   returns an error.  See RFC 2459, section 4.2.2.1 */
gpg_error_t
ksba_cert_get_authority_info_access (ksba_cert_t cert, int idx,
                                     char **r_method, ksba_name_t *r_location)
{
  if (!r_method || !r_location)
    return gpg_error (GPG_ERR_INV_VALUE);
  return get_info_access (cert, idx, 0, r_method, r_location);
}

/* Return the subjectInfoAccess attributes. IDX should be iterated
   starting from 0 until the function returns GPG_ERR_EOF.  R_METHOD
   returns an allocated string with the OID of one item and R_LOCATION
   return the GeneralName for that OID.  The return values for
   R_METHOD and R_LOCATION must be released by the caller unless the
   function returned an error; the function will however make sure
   that R_METHOD and R_LOCATION will point to NULL if the function
   returns an error.  See RFC 2459, section 4.2.2.2 */
gpg_error_t
ksba_cert_get_subject_info_access (ksba_cert_t cert, int idx,
                                   char **r_method, ksba_name_t *r_location)
{
  if (!r_method || !r_location)
    return gpg_error (GPG_ERR_INV_VALUE);
  return get_info_access (cert, idx, 1, r_method, r_location);
}


