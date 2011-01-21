/* cms.c - cryptographic message syntax main functions
 *      Copyright (C) 2001, 2003, 2004, 2008 g10 Code GmbH
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

#include "cms.h"
#include "convert.h"
#include "keyinfo.h"
#include "der-encoder.h"
#include "ber-help.h"
#include "sexp-parse.h"
#include "cert.h" /* need to access cert->root and cert->image */

static gpg_error_t ct_parse_data (ksba_cms_t cms);
static gpg_error_t ct_parse_signed_data (ksba_cms_t cms);
static gpg_error_t ct_parse_enveloped_data (ksba_cms_t cms);
static gpg_error_t ct_parse_digested_data (ksba_cms_t cms);
static gpg_error_t ct_parse_encrypted_data (ksba_cms_t cms);
static gpg_error_t ct_build_data (ksba_cms_t cms);
static gpg_error_t ct_build_signed_data (ksba_cms_t cms);
static gpg_error_t ct_build_enveloped_data (ksba_cms_t cms);
static gpg_error_t ct_build_digested_data (ksba_cms_t cms);
static gpg_error_t ct_build_encrypted_data (ksba_cms_t cms);

static struct { 
  const char *oid;
  ksba_content_type_t ct;
  gpg_error_t (*parse_handler)(ksba_cms_t);
  gpg_error_t (*build_handler)(ksba_cms_t);
} content_handlers[] = {
  {  "1.2.840.113549.1.7.1", KSBA_CT_DATA,
     ct_parse_data   , ct_build_data                  },
  {  "1.2.840.113549.1.7.2", KSBA_CT_SIGNED_DATA,
     ct_parse_signed_data   , ct_build_signed_data    },
  {  "1.2.840.113549.1.7.3", KSBA_CT_ENVELOPED_DATA,
     ct_parse_enveloped_data, ct_build_enveloped_data },
  {  "1.2.840.113549.1.7.5", KSBA_CT_DIGESTED_DATA, 
     ct_parse_digested_data , ct_build_digested_data  },
  {  "1.2.840.113549.1.7.6", KSBA_CT_ENCRYPTED_DATA, 
     ct_parse_encrypted_data, ct_build_encrypted_data },
  {  "1.2.840.113549.1.9.16.1.2", KSBA_CT_AUTH_DATA   },
  { NULL }
};

static const char oidstr_contentType[] = "1.2.840.113549.1.9.3";
/*static char oid_contentType[9] = "\x2A\x86\x48\x86\xF7\x0D\x01\x09\x03";*/

static const char oidstr_messageDigest[] = "1.2.840.113549.1.9.4";
static const char oid_messageDigest[9] ="\x2A\x86\x48\x86\xF7\x0D\x01\x09\x04";

static const char oidstr_signingTime[] = "1.2.840.113549.1.9.5";
static const char oid_signingTime[9] = "\x2A\x86\x48\x86\xF7\x0D\x01\x09\x05";

static const char oidstr_smimeCapabilities[] = "1.2.840.113549.1.9.15";



/* Helper for read_and_hash_cont().  */
static gpg_error_t
read_hash_block (ksba_cms_t cms, unsigned long nleft)
{
  gpg_error_t err;
  char buffer[4096];
  size_t n, nread;

  while (nleft)
    {
      n = nleft < sizeof (buffer)? nleft : sizeof (buffer);
      err = ksba_reader_read (cms->reader, buffer, n, &nread);
      if (err)
        return err;
      nleft -= nread;
      if (cms->hash_fnc)
        cms->hash_fnc (cms->hash_fnc_arg, buffer, nread); 
      if (cms->writer)
        err = ksba_writer_write (cms->writer, buffer, nread);
      if (err)
        return err;
    }
  return 0;
}


/* Copy all the bytes from the reader to the writer and hash them if a
   a hash function has been set.  The writer may be NULL to just do
   the hashing */
static gpg_error_t
read_and_hash_cont (ksba_cms_t cms)
{
  gpg_error_t err = 0;
  unsigned long nleft;
  struct tag_info ti;

  if (cms->inner_cont_ndef)
    {
      for (;;)
        {
          err = _ksba_ber_read_tl (cms->reader, &ti);
          if (err)
            return err;
          
          if (ti.class == CLASS_UNIVERSAL && ti.tag == TYPE_OCTET_STRING
              && !ti.is_constructed)
            { /* next chunk */
              nleft = ti.length;
              err = read_hash_block (cms, nleft);
              if (err)
                return err;
            }
          else if (ti.class == CLASS_UNIVERSAL && ti.tag == TYPE_OCTET_STRING
                   && ti.is_constructed)
            { /* next chunk is constructed */
              for (;;)
                {
                  err = _ksba_ber_read_tl (cms->reader, &ti);
                  if (err)
                    return err;
                  if (ti.class == CLASS_UNIVERSAL
                      && ti.tag == TYPE_OCTET_STRING
                      && !ti.is_constructed)
                    {
                      nleft = ti.length;
                      err = read_hash_block (cms, nleft);
                      if (err)
                        return err;
                    }
                  else if (ti.class == CLASS_UNIVERSAL && !ti.tag
                           && !ti.is_constructed)
                    break; /* ready with this chunk */ 
                  else
                    return gpg_error (GPG_ERR_ENCODING_PROBLEM);
                }
            }
          else if (ti.class == CLASS_UNIVERSAL && !ti.tag
                   && !ti.is_constructed)
            return 0; /* ready */ 
          else
            return gpg_error (GPG_ERR_ENCODING_PROBLEM);
        }
    }
  else
    {
      /* This is basically the same as above but we allow for
         arbitrary types.  Not sure whether it is really needed but
         right in the beginning of gnupg 1.9 we had at least one
         message with didn't used octet strings.  Not ethat we don't
         do proper NLEFT checking but well why should we validate
         these things?  Well, it might be nice to have such a feature
         but then we should write a more general mechanism to do
         that.  */
      nleft = cms->inner_cont_len;
      /* First read the octet string but allow all types here */
      err = _ksba_ber_read_tl (cms->reader, &ti);
      if (err)
        return err;
      if (nleft < ti.nhdr)
        return gpg_error (GPG_ERR_ENCODING_PROBLEM);
      nleft -= ti.nhdr;

      if (ti.class == CLASS_UNIVERSAL && ti.tag == TYPE_OCTET_STRING
          && ti.is_constructed)
        { /* Next chunk is constructed */
          for (;;)
            {
              err = _ksba_ber_read_tl (cms->reader, &ti);
              if (err)
                return err;
              if (ti.class == CLASS_UNIVERSAL
                  && ti.tag == TYPE_OCTET_STRING
                  && !ti.is_constructed)
                {
                  nleft = ti.length;
                  err = read_hash_block (cms, nleft);
                  if (err)
                    return err;
                }
              else if (ti.class == CLASS_UNIVERSAL && !ti.tag
                       && !ti.is_constructed)
                break; /* Ready with this chunk */ 
              else
                return gpg_error (GPG_ERR_ENCODING_PROBLEM);
            }
        }
      else if (ti.class == CLASS_UNIVERSAL && !ti.tag
               && !ti.is_constructed)
        return 0; /* ready */ 
      else
        {
          err = read_hash_block (cms, nleft);
          if (err)
            return err;
        }
    }
  return 0;
}



/* Copy all the encrypted bytes from the reader to the writer.
   Handles indefinite length encoding */
static gpg_error_t
read_encrypted_cont (ksba_cms_t cms)
{
  gpg_error_t err = 0;
  unsigned long nleft;
  char buffer[4096];
  size_t n, nread;

  if (cms->inner_cont_ndef)
    {
      struct tag_info ti;

      /* fixme: this ist mostly a duplicate of the code in
         read_and_hash_cont(). */
      for (;;)
        {
          err = _ksba_ber_read_tl (cms->reader, &ti);
          if (err)
            return err;
          
          if (ti.class == CLASS_UNIVERSAL && ti.tag == TYPE_OCTET_STRING
              && !ti.is_constructed)
            { /* next chunk */
              nleft = ti.length;
              while (nleft)
                {
                  n = nleft < sizeof (buffer)? nleft : sizeof (buffer);
                  err = ksba_reader_read (cms->reader, buffer, n, &nread);
                  if (err)
                    return err;
                  nleft -= nread;
                  err = ksba_writer_write (cms->writer, buffer, nread);
                  if (err)
                    return err;
                }
            }
          else if (ti.class == CLASS_UNIVERSAL && ti.tag == TYPE_OCTET_STRING
                   && ti.is_constructed)
            { /* next chunk is constructed */
              for (;;)
                {
                  err = _ksba_ber_read_tl (cms->reader, &ti);
                  if (err)
                    return err;
                  if (ti.class == CLASS_UNIVERSAL
                      && ti.tag == TYPE_OCTET_STRING
                      && !ti.is_constructed)
                    {
                      nleft = ti.length;
                      while (nleft)
                        {
                          n = nleft < sizeof (buffer)? nleft : sizeof (buffer);
                          err = ksba_reader_read (cms->reader, buffer, n, &nread);
                          if (err)
                            return err;
                          nleft -= nread;
                          if (cms->writer)
                            err = ksba_writer_write (cms->writer, buffer, nread);
                          if (err)
                            return err;
                        }
                    }
                  else if (ti.class == CLASS_UNIVERSAL && !ti.tag
                           && !ti.is_constructed)
                    break; /* ready with this chunk */ 
                  else
                    return gpg_error (GPG_ERR_ENCODING_PROBLEM);
                }
            }
          else if (ti.class == CLASS_UNIVERSAL && !ti.tag
                   && !ti.is_constructed)
            return 0; /* ready */ 
          else
            return gpg_error (GPG_ERR_ENCODING_PROBLEM);
        }
    }
  else
    {
      nleft = cms->inner_cont_len;
      while (nleft)
        {
          n = nleft < sizeof (buffer)? nleft : sizeof (buffer);
          err = ksba_reader_read (cms->reader, buffer, n, &nread);
          if (err)
            return err;
          nleft -= nread;
          err = ksba_writer_write (cms->writer, buffer, nread);
          if (err)
            return err;
        }
    }
  return 0;
}

/* copy data from reader to writer.  Assume that it is an octet string
   and insert undefinite length headers where needed */
static gpg_error_t
write_encrypted_cont (ksba_cms_t cms)
{
  gpg_error_t err = 0;
  char buffer[4096];
  size_t nread;

  /* we do it the simple way: the parts are made up from the chunks we
     got from the read function.

     Fixme: We should write the tag here, and write a definite length
     header if everything fits into our local buffer.  Actually pretty
     simple to do, but I am too lazy right now. */
  while (!(err = ksba_reader_read (cms->reader, buffer,
                                   sizeof buffer, &nread)) )
    {
      err = _ksba_ber_write_tl (cms->writer, TYPE_OCTET_STRING,
                                CLASS_UNIVERSAL, 0, nread);
      if (!err)
        err = ksba_writer_write (cms->writer, buffer, nread);
    }
  if (gpg_err_code (err) == GPG_ERR_EOF) /* write the end tag */
      err = _ksba_ber_write_tl (cms->writer, 0, 0, 0, 0);

  return err;
}


/* Figure out whether the data read from READER is a CMS object and
   return its content type.  This function does only peek at the
   READER and tries to identify the type with best effort.  Because of
   the ubiquity of the stupid and insecure pkcs#12 format, the
   function will also identify those files and return KSBA_CT_PKCS12;
   there is and will be no other pkcs#12 support in this library. */
ksba_content_type_t
ksba_cms_identify (ksba_reader_t reader) 
{
  struct tag_info ti;
  unsigned char buffer[24];
  const unsigned char*p;
  size_t n, count;
  char *oid;
  int i;
  int maybe_p12 = 0;

  if (!reader)
    return KSBA_CT_NONE; /* oops */

  /* This is a common example of a CMS object - it is obvious that we
     only need to read a few bytes to get to the OID:
  30 82 0B 59 06 09 2A 86 48 86 F7 0D 01 07 02 A0 82 0B 4A 30 82 0B 46 02
  ----------- ++++++++++++++++++++++++++++++++
  SEQUENCE    OID (signedData)
  (2 byte len)
 
     For a pkcs12 message we have this:

  30 82 08 59 02 01 03 30 82 08 1F 06 09 2A 86 48 86 F7 0D 01 07 01 A0 82
  ----------- ++++++++ ----------- ++++++++++++++++++++++++++++++++ 
  SEQUENCE    INTEGER  SEQUENCE    OID (data)
  
    This we need to read at least 22 bytes, we add 2 bytes to cope with 
    length headers store with 4 bytes.
  */
  
  for (count = sizeof buffer; count; count -= n)
    {
      if (ksba_reader_read (reader, buffer+sizeof (buffer)-count, count, &n))
        return KSBA_CT_NONE; /* too short */
    }
  n = sizeof buffer;
  if (ksba_reader_unread (reader, buffer, n))
    return KSBA_CT_NONE; /* oops */

  p = buffer;
  if (_ksba_ber_parse_tl (&p, &n, &ti))
    return KSBA_CT_NONE;
  if ( !(ti.class == CLASS_UNIVERSAL && ti.tag == TYPE_SEQUENCE
         && ti.is_constructed) )
    return KSBA_CT_NONE;
  if (_ksba_ber_parse_tl (&p, &n, &ti))
    return KSBA_CT_NONE;
  if ( ti.class == CLASS_UNIVERSAL && ti.tag == TYPE_INTEGER
       && !ti.is_constructed && ti.length == 1 && n && *p == 3)
    {
      maybe_p12 = 1;
      p++;
      n--;
      if (_ksba_ber_parse_tl (&p, &n, &ti))
        return KSBA_CT_NONE;
      if ( !(ti.class == CLASS_UNIVERSAL && ti.tag == TYPE_SEQUENCE
             && ti.is_constructed) )
        return KSBA_CT_NONE;
      if (_ksba_ber_parse_tl (&p, &n, &ti))
        return KSBA_CT_NONE;
    }
  if ( !(ti.class == CLASS_UNIVERSAL && ti.tag == TYPE_OBJECT_ID
         && !ti.is_constructed && ti.length) || ti.length > n)
    return KSBA_CT_NONE;
  oid = ksba_oid_to_str (p, ti.length);
  if (!oid)
    return KSBA_CT_NONE; /* out of core */
  for (i=0; content_handlers[i].oid; i++)
    {
      if (!strcmp (content_handlers[i].oid, oid))
        break;
    }
  if (!content_handlers[i].oid)
    return KSBA_CT_NONE; /* unknown */
  if (maybe_p12 && (content_handlers[i].ct == KSBA_CT_DATA
                    || content_handlers[i].ct == KSBA_CT_SIGNED_DATA))
      return KSBA_CT_PKCS12;
  return content_handlers[i].ct;
}



/**
 * ksba_cms_new:
 * 
 * Create a new and empty CMS object
 * 
 * Return value: A CMS object or an error code.
 **/
gpg_error_t
ksba_cms_new (ksba_cms_t *r_cms)
{
  *r_cms = xtrycalloc (1, sizeof **r_cms);
  if (!*r_cms)
    return gpg_error_from_errno (errno);
  return 0;
}

/* Release a list of value trees. */
static void
release_value_tree (struct value_tree_s *tree)
{
  while (tree)
    {
      struct value_tree_s *tmp = tree->next;
      _ksba_asn_release_nodes (tree->root);
      xfree (tree->image);
      xfree (tree);
      tree = tmp;
    }
}

/**
 * ksba_cms_release:
 * @cms: A CMS object
 * 
 * Release a CMS object.
 **/
void
ksba_cms_release (ksba_cms_t cms)
{
  if (!cms)
    return;
  xfree (cms->content.oid);
  while (cms->digest_algos)
    {
      struct oidlist_s *ol = cms->digest_algos->next;
      xfree (cms->digest_algos->oid);
      xfree (cms->digest_algos);
      cms->digest_algos = ol;
    }
  while (cms->cert_list)
    {
      struct certlist_s *cl = cms->cert_list->next;
      ksba_cert_release (cms->cert_list->cert);
      xfree (cms->cert_list->enc_val.algo);
      xfree (cms->cert_list->enc_val.value);
      xfree (cms->cert_list);
      cms->cert_list = cl;
    }
  while (cms->cert_info_list)
    {
      struct certlist_s *cl = cms->cert_info_list->next;
      ksba_cert_release (cms->cert_info_list->cert);
      xfree (cms->cert_info_list->enc_val.algo);
      xfree (cms->cert_info_list->enc_val.value);
      xfree (cms->cert_info_list);
      cms->cert_info_list = cl;
    }
  xfree (cms->inner_cont_oid);
  xfree (cms->encr_algo_oid);
  xfree (cms->encr_iv);
  xfree (cms->data.digest);
  while (cms->signer_info)
    {
      struct signer_info_s *tmp = cms->signer_info->next;
      _ksba_asn_release_nodes (cms->signer_info->root);
      xfree (cms->signer_info->image);
      xfree (cms->signer_info->cache.digest_algo);
      xfree (cms->signer_info);
      cms->signer_info = tmp;
    }
  release_value_tree (cms->recp_info);
  while (cms->sig_val)
    {
      struct sig_val_s *tmp = cms->sig_val->next;
      xfree (cms->sig_val->algo);
      xfree (cms->sig_val->value);
      xfree (cms->sig_val);
      cms->sig_val = tmp;
    }
  while (cms->capability_list)
    { 
      struct oidparmlist_s *tmp = cms->capability_list->next;
      xfree (cms->capability_list->oid);
      xfree (cms->capability_list);
      cms->capability_list = tmp;
    }

  xfree (cms);
}


gpg_error_t
ksba_cms_set_reader_writer (ksba_cms_t cms, ksba_reader_t r, ksba_writer_t w)
{
  if (!cms || !(r || w))
    return gpg_error (GPG_ERR_INV_VALUE);
  if ((r && cms->reader) || (w && cms->writer) )
    return gpg_error (GPG_ERR_CONFLICT); /* already set */
  
  cms->reader = r;
  cms->writer = w;
  return 0;
}



gpg_error_t
ksba_cms_parse (ksba_cms_t cms, ksba_stop_reason_t *r_stopreason)
{
  gpg_error_t err;
  int i;

  if (!cms || !r_stopreason)
    return gpg_error (GPG_ERR_INV_VALUE);

  *r_stopreason = KSBA_SR_RUNNING;
  if (!cms->stop_reason)
    { /* Initial state: start parsing */
      err = _ksba_cms_parse_content_info (cms);
      if (err)
        return err;
      for (i=0; content_handlers[i].oid; i++)
        {
          if (!strcmp (content_handlers[i].oid, cms->content.oid))
            break;
        }
      if (!content_handlers[i].oid)
        return gpg_error (GPG_ERR_UNKNOWN_CMS_OBJ);
      if (!content_handlers[i].parse_handler)
        return gpg_error (GPG_ERR_UNSUPPORTED_CMS_OBJ);
      cms->content.ct      = content_handlers[i].ct;
      cms->content.handler = content_handlers[i].parse_handler;
      cms->stop_reason = KSBA_SR_GOT_CONTENT;
    }
  else if (cms->content.handler)
    {
      err = cms->content.handler (cms);
      if (err)
        return err;
    }
  else
    return gpg_error (GPG_ERR_UNSUPPORTED_CMS_OBJ);
  
  *r_stopreason = cms->stop_reason;
  return 0;
}

gpg_error_t
ksba_cms_build (ksba_cms_t cms, ksba_stop_reason_t *r_stopreason)
{
  gpg_error_t err;

  if (!cms || !r_stopreason)
    return gpg_error (GPG_ERR_INV_VALUE);

  *r_stopreason = KSBA_SR_RUNNING;
  if (!cms->stop_reason)
    { /* Initial state: check that the content handler is known */
      if (!cms->writer)
        return gpg_error (GPG_ERR_MISSING_ACTION);
      if (!cms->content.handler)
        return gpg_error (GPG_ERR_MISSING_ACTION);
      if (!cms->inner_cont_oid)
        return gpg_error (GPG_ERR_MISSING_ACTION);
      cms->stop_reason = KSBA_SR_GOT_CONTENT;
    }
  else if (cms->content.handler)
    {
      err = cms->content.handler (cms);
      if (err)
        return err;
    }
  else
    return gpg_error (GPG_ERR_UNSUPPORTED_CMS_OBJ);
  
  *r_stopreason = cms->stop_reason;
  return 0;
}




/* Return the content type.  A WHAT of 0 returns the real content type
   whereas a 1 returns the inner content type.
*/
ksba_content_type_t
ksba_cms_get_content_type (ksba_cms_t cms, int what)
{
  int i;

  if (!cms)
    return 0;
  if (!what)
    return cms->content.ct;

  if (what == 1 && cms->inner_cont_oid)
    {
      for (i=0; content_handlers[i].oid; i++)
        {
          if (!strcmp (content_handlers[i].oid, cms->inner_cont_oid))
            return content_handlers[i].ct;
        }
    }
  return 0;
}


/* Return the object ID of the current cms.  This is a constant string
   valid as long as the context is valid and no new parse is
   started. */
const char *
ksba_cms_get_content_oid (ksba_cms_t cms, int what)
{
  if (!cms)
    return NULL;
  if (!what)
    return cms->content.oid;
  if (what == 1)
    return cms->inner_cont_oid;
  if (what == 2)
    return cms->encr_algo_oid;
  return NULL;
}


/* Copy the initialization vector into iv and its len into ivlen.
   The caller should provide a suitable large buffer */
gpg_error_t
ksba_cms_get_content_enc_iv (ksba_cms_t cms, void *iv,
                             size_t maxivlen, size_t *ivlen)
{
  if (!cms || !iv || !ivlen)
    return gpg_error (GPG_ERR_INV_VALUE);
  if (!cms->encr_ivlen)
    return gpg_error (GPG_ERR_NO_DATA);
  if (cms->encr_ivlen > maxivlen)
    return gpg_error (GPG_ERR_BUFFER_TOO_SHORT);
  memcpy (iv, cms->encr_iv, cms->encr_ivlen);
  *ivlen = cms->encr_ivlen;
  return 0;
}


/**
 * ksba_cert_get_digest_algo_list:
 * @cert: Initialized certificate object
 * @idx: enumerator
 * 
 * Figure out the the digest algorithm used for the signature and
 * return its OID.  Note that the algos returned are just hints on
 * what to hash.
 *
 * Return value: NULL for no more algorithms or a string valid as long
 * as the the cms object is valid.
 **/
const char *
ksba_cms_get_digest_algo_list (ksba_cms_t cms, int idx)
{
  struct oidlist_s *ol;

  if (!cms)
    return NULL;

  for (ol=cms->digest_algos; ol && idx; ol = ol->next, idx-- )
    ;
  if (!ol)
    return NULL;
  return ol->oid;
}


/**
 * ksba_cms_get_issuer_serial:
 * @cms: CMS object
 * @idx: index number
 * @r_issuer: returns the issuer
 * @r_serial: returns the serial number
 * 
 * This functions returns the issuer and serial number either from the
 * sid or the rid elements of a CMS object.
 * 
 * Return value: 0 on success or an error code.  An error code of -1
 * is returned to indicate that there is no issuer with that idx,
 * GPG_ERR_No_Data is returned to indicate that there is no issuer at
 * all.
 **/
gpg_error_t
ksba_cms_get_issuer_serial (ksba_cms_t cms, int idx,
                            char **r_issuer, ksba_sexp_t *r_serial)
{
  gpg_error_t err;
  const char *issuer_path, *serial_path;
  AsnNode root;
  const unsigned char *image;
  AsnNode n;

  if (!cms)
    return gpg_error (GPG_ERR_INV_VALUE);
  if (idx < 0)
    return gpg_error (GPG_ERR_INV_INDEX);

  if (cms->signer_info)
    {
      struct signer_info_s *si;

      for (si=cms->signer_info; si && idx; si = si->next, idx-- )
        ;
      if (!si)
        return -1;

      issuer_path = "SignerInfo.sid.issuerAndSerialNumber.issuer";
      serial_path = "SignerInfo.sid.issuerAndSerialNumber.serialNumber";
      root = si->root;
      image = si->image;
    }
  else if (cms->recp_info)
    {
      struct value_tree_s *tmp;

      issuer_path = "KeyTransRecipientInfo.rid.issuerAndSerialNumber.issuer";
      serial_path = "KeyTransRecipientInfo.rid.issuerAndSerialNumber.serialNumber";
      for (tmp=cms->recp_info; tmp && idx; tmp=tmp->next, idx-- )
        ;
      if (!tmp)
        return -1;
      root = tmp->root;
      image = tmp->image;
    }
  else
    return gpg_error (GPG_ERR_NO_DATA);
  
  if (r_issuer)
    {
      n = _ksba_asn_find_node (root, issuer_path);
      if (!n || !n->down)
        return gpg_error (GPG_ERR_NO_VALUE); 
      n = n->down; /* dereference the choice node */
      
      if (n->off == -1)
        {
/*            fputs ("get_issuer problem at node:\n", stderr); */
/*            _ksba_asn_node_dump_all (n, stderr); */
          return gpg_error (GPG_ERR_GENERAL);
        }
      err = _ksba_dn_to_str (image, n, r_issuer);
      if (err)
        return err;
    }

  if (r_serial)
    {
      char numbuf[22];
      int numbuflen;
      unsigned char *p;

      /* fixme: we do not release the r_issuer stuff on error */
      n = _ksba_asn_find_node (root, serial_path);
      if (!n)
        return gpg_error (GPG_ERR_NO_VALUE); 
      
      if (n->off == -1)
        {
/*            fputs ("get_serial problem at node:\n", stderr); */
/*            _ksba_asn_node_dump_all (n, stderr); */
          return gpg_error (GPG_ERR_GENERAL);
        }

      sprintf (numbuf,"(%u:", (unsigned int)n->len);
      numbuflen = strlen (numbuf);
      p = xtrymalloc (numbuflen + n->len + 2);
      if (!p)
        return gpg_error (GPG_ERR_ENOMEM);
      strcpy (p, numbuf);
      memcpy (p+numbuflen, image + n->off + n->nhdr, n->len);
      p[numbuflen + n->len] = ')';
      p[numbuflen + n->len + 1] = 0;
      *r_serial = p;
    }

  return 0;
}



/**
 * ksba_cms_get_digest_algo:
 * @cms: CMS object
 * @idx: index of signer
 * 
 * Figure out the the digest algorithm used by the signer @idx return
 * its OID.  This is the algorithm acually used to calculate the
 * signature.
 *
 * Return value: NULL for no such signer or a constn string valid as
 * long as the CMS object lives.
 **/
const char *
ksba_cms_get_digest_algo (ksba_cms_t cms, int idx)
{
  AsnNode n;
  char *algo;
  struct signer_info_s *si;

  if (!cms)
    return NULL;
  if (!cms->signer_info)
    return NULL;
  if (idx < 0)
    return NULL; 

  for (si=cms->signer_info; si && idx; si = si->next, idx-- )
    ;
  if (!si)
    return NULL;

  if (si->cache.digest_algo)
    return si->cache.digest_algo;
  
  n = _ksba_asn_find_node (si->root, "SignerInfo.digestAlgorithm.algorithm");
  algo = _ksba_oid_node_to_str (si->image, n);
  if (algo)
    {
      si->cache.digest_algo = algo;
    }
  return algo;
}


/**
 * ksba_cms_get_cert:
 * @cms: CMS object
 * @idx: enumerator
 * 
 * Get the certificate out of a CMS.  The caller should use this in a
 * loop to get all certificates.  The returned certificate is a
 * shallow copy of the original one; the caller must still use
 * ksba_cert_release() to free it.
 * 
 * Return value: A Certificate object or NULL for end of list or error
 **/
ksba_cert_t
ksba_cms_get_cert (ksba_cms_t cms, int idx)
{
  struct certlist_s *cl;

  if (!cms || idx < 0)
    return NULL;

  for (cl=cms->cert_list; cl && idx; cl = cl->next, idx--)
    ;
  if (!cl)
    return NULL;
  ksba_cert_ref (cl->cert);
  return cl->cert;
}


/* 
   Return the extension attribute messageDigest 
*/
gpg_error_t
ksba_cms_get_message_digest (ksba_cms_t cms, int idx,
                             char **r_digest, size_t *r_digest_len)
{ 
  AsnNode nsiginfo, n;
  struct signer_info_s *si;

  if (!cms || !r_digest || !r_digest_len)
    return gpg_error (GPG_ERR_INV_VALUE);
  if (!cms->signer_info)
    return gpg_error (GPG_ERR_NO_DATA);
  if (idx < 0)
    return gpg_error (GPG_ERR_INV_INDEX);

  for (si=cms->signer_info; si && idx; si = si->next, idx-- )
    ;
  if (!si)
    return -1;

  
  *r_digest = NULL;
  *r_digest_len = 0;
  nsiginfo = _ksba_asn_find_node (si->root, "SignerInfo.signedAttrs");
  if (!nsiginfo)
    return gpg_error (GPG_ERR_BUG);

  n = _ksba_asn_find_type_value (si->image, nsiginfo, 0,
                                 oid_messageDigest, DIM(oid_messageDigest));
  if (!n)
    return 0; /* this is okay, because the element is optional */

  /* check that there is only one */
  if (_ksba_asn_find_type_value (si->image, nsiginfo, 1,
                                 oid_messageDigest, DIM(oid_messageDigest)))
    return gpg_error (GPG_ERR_DUP_VALUE);

  /* the value is is a SET OF OCTECT STRING but the set must have
     excactly one OCTECT STRING.  (rfc2630 11.2) */
  if ( !(n->type == TYPE_SET_OF && n->down
         && n->down->type == TYPE_OCTET_STRING && !n->down->right))
    return gpg_error (GPG_ERR_INV_CMS_OBJ);
  n = n->down;
  if (n->off == -1)
    return gpg_error (GPG_ERR_BUG);

  *r_digest_len = n->len;
  *r_digest = xtrymalloc (n->len);
  if (!*r_digest)
    return gpg_error (GPG_ERR_ENOMEM);
  memcpy (*r_digest, si->image + n->off + n->nhdr, n->len);
  return 0;
}


/* Return the extension attribute signing time, which may be empty for no
   signing time available. */
gpg_error_t
ksba_cms_get_signing_time (ksba_cms_t cms, int idx, ksba_isotime_t r_sigtime)
{ 
  AsnNode nsiginfo, n;
  struct signer_info_s *si;

  if (!cms)
    return gpg_error (GPG_ERR_INV_VALUE);
  *r_sigtime = 0;
  if (!cms->signer_info)
    return gpg_error (GPG_ERR_NO_DATA);
  if (idx < 0)
    return gpg_error (GPG_ERR_INV_INDEX);

  for (si=cms->signer_info; si && idx; si = si->next, idx-- )
    ;
  if (!si)
    return -1;
  
  *r_sigtime = 0;
  nsiginfo = _ksba_asn_find_node (si->root, "SignerInfo.signedAttrs");
  if (!nsiginfo)
    return 0; /* This is okay because signedAttribs are optional. */

  n = _ksba_asn_find_type_value (si->image, nsiginfo, 0,
                                 oid_signingTime, DIM(oid_signingTime));
  if (!n)
    return 0; /* This is okay because signing time is optional. */

  /* check that there is only one */
  if (_ksba_asn_find_type_value (si->image, nsiginfo, 1,
                                 oid_signingTime, DIM(oid_signingTime)))
    return gpg_error (GPG_ERR_DUP_VALUE);

  /* the value is is a SET OF CHOICE but the set must have
     excactly one CHOICE of generalized or utctime.  (rfc2630 11.3) */
  if ( !(n->type == TYPE_SET_OF && n->down
         && (n->down->type == TYPE_GENERALIZED_TIME
             || n->down->type == TYPE_UTC_TIME)
         && !n->down->right))
    return gpg_error (GPG_ERR_INV_CMS_OBJ);
  n = n->down;
  if (n->off == -1)
    return gpg_error (GPG_ERR_BUG);

  return _ksba_asntime_to_iso (si->image + n->off + n->nhdr, n->len,
                               n->type == TYPE_UTC_TIME, r_sigtime);
}


/* Return a list of OIDs stored as signed attributes for the signature
   number IDX.  All the values (OIDs) for the the requested OID REQOID
   are returned delimited by a linefeed.  Caller must free that
   list. -1 is returned when IDX is larger than the number of
   signatures, GPG_ERR_No_Data is returned when there is no such
   attribute for the given signer. */
gpg_error_t
ksba_cms_get_sigattr_oids (ksba_cms_t cms, int idx,
                           const char *reqoid, char **r_value)
{ 
  gpg_error_t err;
  AsnNode nsiginfo, n;
  struct signer_info_s *si;
  unsigned char *reqoidbuf;
  size_t reqoidlen;
  char *retstr = NULL;
  int i;

  if (!cms || !r_value)
    return gpg_error (GPG_ERR_INV_VALUE);
  if (!cms->signer_info)
    return gpg_error (GPG_ERR_NO_DATA);
  if (idx < 0)
    return gpg_error (GPG_ERR_INV_INDEX);
  *r_value = NULL;

  for (si=cms->signer_info; si && idx; si = si->next, idx-- )
    ;
  if (!si)
    return -1; /* no more signers */
  
  nsiginfo = _ksba_asn_find_node (si->root, "SignerInfo.signedAttrs");
  if (!nsiginfo)
    return -1; /* this is okay, because signedAttribs are optional */

  err = ksba_oid_from_str (reqoid, &reqoidbuf, &reqoidlen);
  if(err)
    return err;
  
  for (i=0; (n = _ksba_asn_find_type_value (si->image, nsiginfo,
                                            i, reqoidbuf, reqoidlen)); i++)
    {
      char *line, *p;

      /* the value is is a SET OF OBJECT ID but the set must have
         excactly one OBJECT ID.  (rfc2630 11.1) */
      if ( !(n->type == TYPE_SET_OF && n->down
             && n->down->type == TYPE_OBJECT_ID && !n->down->right))
        {
          xfree (reqoidbuf);
          xfree (retstr);
          return gpg_error (GPG_ERR_INV_CMS_OBJ);
        }
      n = n->down;
      if (n->off == -1)
        {
          xfree (reqoidbuf);
          xfree (retstr);
          return gpg_error (GPG_ERR_BUG);
        }

      p = _ksba_oid_node_to_str (si->image, n);
      if (!p)
        {
          xfree (reqoidbuf);
          xfree (retstr);
          return gpg_error (GPG_ERR_INV_CMS_OBJ);
        }

      if (!retstr)
        line = retstr = xtrymalloc (strlen (p) + 2);
      else
        {
          char *tmp = xtryrealloc (retstr,
                                   strlen (retstr) + 1 + strlen (p) + 2);
          if (!tmp)
            line = NULL;
          else
            {
              retstr = tmp;
              line = stpcpy (retstr + strlen (retstr), "\n");
            }
        }
      if (!line)
        {
          xfree (reqoidbuf);
          xfree (retstr);
          xfree (p);
          return gpg_error (GPG_ERR_ENOMEM);
        }
      strcpy (line, p);
      xfree (p);
    }
  xfree (reqoidbuf);
  if (!n && !i)
    return -1; /* no such attribute */
  *r_value = retstr;
  return 0;
}


/**
 * ksba_cms_get_sig_val:
 * @cms: CMS object
 * @idx: index of signer
 * 
 * Return the actual signature of signer @idx in a format suitable to
 * be used as input to Libgcrypt's verification function.  The caller
 * must free the returned string.
 * 
 * Return value: NULL or a string with a S-Exp.
 **/
ksba_sexp_t
ksba_cms_get_sig_val (ksba_cms_t cms, int idx)
{
  AsnNode n, n2;
  gpg_error_t err;
  ksba_sexp_t string;
  struct signer_info_s *si;

  if (!cms)
    return NULL;
  if (!cms->signer_info)
    return NULL;
  if (idx < 0)
    return NULL; 

  for (si=cms->signer_info; si && idx; si = si->next, idx-- )
    ;
  if (!si)
    return NULL;

  n = _ksba_asn_find_node (si->root, "SignerInfo.signatureAlgorithm");
  if (!n)
      return NULL;
  if (n->off == -1)
    {
/*        fputs ("ksba_cms_get_sig_val problem at node:\n", stderr); */
/*        _ksba_asn_node_dump_all (n, stderr); */
      return NULL;
    }

  n2 = n->right; /* point to the actual value */
  err = _ksba_sigval_to_sexp (si->image + n->off,
                              n->nhdr + n->len
                              + ((!n2||n2->off == -1)? 0:(n2->nhdr+n2->len)),
                              &string);
  if (err)
      return NULL;

  return string;
}


/**
 * ksba_cms_get_enc_val:
 * @cms: CMS object
 * @idx: index of recipient info
 * 
 * Return the encrypted value (the session key) of recipient @idx in a
 * format suitable to be used as input to Libgcrypt's decryption
 * function.  The caller must free the returned string.
 * 
 * Return value: NULL or a string with a S-Exp.
 **/
ksba_sexp_t
ksba_cms_get_enc_val (ksba_cms_t cms, int idx)
{
  AsnNode n, n2;
  gpg_error_t err;
  ksba_sexp_t string;
  struct value_tree_s *vt;

  if (!cms)
    return NULL;
  if (!cms->recp_info)
    return NULL;
  if (idx < 0)
    return NULL;

  for (vt=cms->recp_info; vt && idx; vt=vt->next, idx--)
    ;
  if (!vt)
    return NULL; /* No value at this IDX */


  n = _ksba_asn_find_node (vt->root,
                           "KeyTransRecipientInfo.keyEncryptionAlgorithm");
  if (!n)
      return NULL;
  if (n->off == -1)
    {
/*        fputs ("ksba_cms_get_enc_val problem at node:\n", stderr); */
/*        _ksba_asn_node_dump_all (n, stderr); */
      return NULL;
    }

  n2 = n->right; /* point to the actual value */
  err = _ksba_encval_to_sexp (vt->image + n->off,
                              n->nhdr + n->len
                              + ((!n2||n2->off == -1)? 0:(n2->nhdr+n2->len)),
                              &string);
  if (err)
      return NULL;

  return string;
}





/* Provide a hash function so that we are able to hash the data */
void
ksba_cms_set_hash_function (ksba_cms_t cms,
                            void (*hash_fnc)(void *, const void *, size_t),
                            void *hash_fnc_arg)
{
  if (cms)
    {
      cms->hash_fnc = hash_fnc;
      cms->hash_fnc_arg = hash_fnc_arg;
    }
}


/* hash the signed attributes of the given signer */
gpg_error_t
ksba_cms_hash_signed_attrs (ksba_cms_t cms, int idx)
{
  AsnNode n;
  struct signer_info_s *si;

  if (!cms)
    return gpg_error (GPG_ERR_INV_VALUE);
  if (!cms->hash_fnc)
    return gpg_error (GPG_ERR_MISSING_ACTION);
  if (idx < 0)
    return -1;

  for (si=cms->signer_info; si && idx; si = si->next, idx-- )
    ;
  if (!si)
    return -1;
      
  n = _ksba_asn_find_node (si->root, "SignerInfo.signedAttrs");
  if (!n || n->off == -1)
    return gpg_error (GPG_ERR_NO_VALUE); 

  /* We don't hash the implicit tag [0] but a SET tag */
  cms->hash_fnc (cms->hash_fnc_arg, "\x31", 1); 
  cms->hash_fnc (cms->hash_fnc_arg,
                 si->image + n->off + 1, n->nhdr + n->len - 1);

  return 0;
}


/* 
  Code to create CMS structures
*/


/**
 * ksba_cms_set_content_type:
 * @cms: A CMS object
 * @what: 0 for content type, 1 for inner content type
 * @type: Tyep constant
 * 
 * Set the content type used for build operations.  This should be the
 * first operation before starting to create a CMS message.
 * 
 * Return value: 0 on success or an error code
 **/
gpg_error_t
ksba_cms_set_content_type (ksba_cms_t cms, int what, ksba_content_type_t type)
{
  int i;
  char *oid;

  if (!cms || what < 0 || what > 1 )
    return gpg_error (GPG_ERR_INV_VALUE);

  for (i=0; content_handlers[i].oid; i++)
    {
      if (content_handlers[i].ct == type)
        break;
    }
  if (!content_handlers[i].oid)
    return gpg_error (GPG_ERR_UNKNOWN_CMS_OBJ);
  if (!content_handlers[i].build_handler)
    return gpg_error (GPG_ERR_UNSUPPORTED_CMS_OBJ);
  oid = xtrystrdup (content_handlers[i].oid);
  if (!oid)
    return gpg_error (GPG_ERR_ENOMEM);

  if (!what)
    {
      cms->content.oid     = oid;
      cms->content.ct      = content_handlers[i].ct;
      cms->content.handler = content_handlers[i].build_handler;
    }
  else
    {
      cms->inner_cont_oid = oid;
    }

  return 0;
}


/**
 * ksba_cms_add_digest_algo:
 * @cms:  A CMS object 
 * @oid: A stringified object OID describing the hash algorithm
 * 
 * Set the algorithm to be used for creating the hash. Note, that we
 * currently can't do a per-signer hash.
 * 
 * Return value: 0 on success or an error code
 **/
gpg_error_t
ksba_cms_add_digest_algo (ksba_cms_t cms, const char *oid)
{
  struct oidlist_s *ol;

  if (!cms || !oid)
    return gpg_error (GPG_ERR_INV_VALUE);

  ol = xtrymalloc (sizeof *ol);
  if (!ol)
    return gpg_error (GPG_ERR_ENOMEM);
  
  ol->oid = xtrystrdup (oid);
  if (!ol->oid)
    {
      xfree (ol);
      return gpg_error (GPG_ERR_ENOMEM);
    }
  ol->next = cms->digest_algos;
  cms->digest_algos = ol;
  return 0;
}


/**
 * ksba_cms_add_signer:
 * @cms: A CMS object
 * @cert: A certificate used to describe the signer.
 * 
 * This functions starts assembly of a new signed data content or adds
 * another signer to the list of signers.
 *
 * Return value: 0 on success or an error code.
 **/
gpg_error_t
ksba_cms_add_signer (ksba_cms_t cms, ksba_cert_t cert)
{
  struct certlist_s *cl, *cl2;

  if (!cms)
    return gpg_error (GPG_ERR_INV_VALUE);
  
  cl = xtrycalloc (1,sizeof *cl);
  if (!cl)
      return gpg_error (GPG_ERR_ENOMEM);

  ksba_cert_ref (cert);
  cl->cert = cert;
  if (!cms->cert_list)
    cms->cert_list = cl;
  else
    {
      for (cl2=cms->cert_list; cl2->next; cl2 = cl2->next)
        ;
      cl2->next = cl;
    }
  return 0;
}

/**
 * ksba_cms_add_cert:
 * @cms: A CMS object
 * @cert: A certificate to be send along with the signed data.
 * 
 * This functions adds a certificate to the list of certificates send
 * along with the signed data.  Using this is optional but it is very
 * common to include at least the certificate of the signer it self.
 *
 * Return value: 0 on success or an error code.
 **/
gpg_error_t
ksba_cms_add_cert (ksba_cms_t cms, ksba_cert_t cert)
{
  struct certlist_s *cl;

  if (!cms || !cert)
    return gpg_error (GPG_ERR_INV_VALUE);

  /* first check whether this is a duplicate. */
  for (cl = cms->cert_info_list; cl; cl = cl->next)
    {
      if (!_ksba_cert_cmp (cert, cl->cert))
        return 0; /* duplicate */
    }

  /* Okay, add it. */
  cl = xtrycalloc (1,sizeof *cl);
  if (!cl)
      return gpg_error (GPG_ERR_ENOMEM);

  ksba_cert_ref (cert);
  cl->cert = cert;
  cl->next = cms->cert_info_list;
  cms->cert_info_list = cl;
  return 0;
}


/* Add an S/MIME capability as an extended attribute to the message.
   This function is to be called for each capability in turn. The
   first capability added will receive the highest priority.  CMS is
   the context, OID the object identifier of the capability and if DER
   is not NULL it is used as the DER-encoded parameters of the
   capability; the length of that DER object is given in DERLEN.
   DERLEN should be 0 if DER is NULL.

   The function returns 0 on success or an error code.
*/
gpg_error_t
ksba_cms_add_smime_capability (ksba_cms_t cms, const char *oid,
                               const unsigned char *der, size_t derlen)
{
  gpg_error_t err;
  struct oidparmlist_s *opl, *opl2;

  if (!cms || !oid)
    return gpg_error (GPG_ERR_INV_VALUE);

  if (!der)
    derlen = 0;

  opl = xtrymalloc (sizeof *opl + derlen - 1);
  if (!opl)
    return gpg_error_from_errno (errno);
  opl->next = NULL;
  opl->oid = xtrystrdup (oid);
  if (!opl->oid)
    {
      err = gpg_error_from_errno (errno);
      xfree (opl);
      return err;
    }
  opl->parmlen = derlen;
  if (der)
    memcpy (opl->parm, der, derlen);

  /* Append it to maintain the desired order. */
  if (!cms->capability_list)
    cms->capability_list = opl;
  else
    {
      for (opl2=cms->capability_list; opl2->next; opl2 = opl2->next)
        ;
      opl2->next = opl;
    }

  return 0;
}



/**
 * ksba_cms_set_message_digest:
 * @cms: A CMS object
 * @idx: The index of the signer
 * @digest: a message digest
 * @digest_len: the length of the message digest
 * 
 * Set a message digest into the signedAttributes of the signer with
 * the index IDX.  The index of a signer is determined by the sequence
 * of ksba_cms_add_signer() calls; the first signer has the index 0.
 * This function is to be used when the hash value of the data has
 * been calculated and before the create function requests the sign
 * operation.
 * 
 * Return value: 0 on success or an error code
 **/
gpg_error_t
ksba_cms_set_message_digest (ksba_cms_t cms, int idx, 
                             const unsigned char *digest, size_t digest_len)
{ 
  struct certlist_s *cl;

  if (!cms || !digest)
    return gpg_error (GPG_ERR_INV_VALUE);
  if (!digest_len || digest_len > DIM(cl->msg_digest))
    return gpg_error (GPG_ERR_INV_VALUE);
  if (idx < 0)
    return gpg_error (GPG_ERR_INV_INDEX);

  for (cl=cms->cert_list; cl && idx; cl = cl->next, idx--)
    ;
  if (!cl)
    return gpg_error (GPG_ERR_INV_INDEX); /* no certificate to store it */
  cl->msg_digest_len = digest_len;
  memcpy (cl->msg_digest, digest, digest_len);
  return 0;
}

/**
 * ksba_cms_set_signing_time:
 * @cms: A CMS object
 * @idx: The index of the signer
 * @sigtime: a time or an empty value to use the current time
 * 
 * Set a signing time into the signedAttributes of the signer with
 * the index IDX.  The index of a signer is determined by the sequence
 * of ksba_cms_add_signer() calls; the first signer has the index 0.
 * 
 * Return value: 0 on success or an error code
 **/
gpg_error_t
ksba_cms_set_signing_time (ksba_cms_t cms, int idx, const ksba_isotime_t sigtime)
{ 
  struct certlist_s *cl;

  if (!cms)
    return gpg_error (GPG_ERR_INV_VALUE);
  if (idx < 0)
    return gpg_error (GPG_ERR_INV_INDEX);

  for (cl=cms->cert_list; cl && idx; cl = cl->next, idx--)
    ;
  if (!cl)
    return gpg_error (GPG_ERR_INV_INDEX); /* no certificate to store it */
  
  /* Fixme: We might want to check the validity of the passed time
     string. */
  if (!*sigtime)
    _ksba_current_time (cl->signing_time);
  else
    _ksba_copy_time (cl->signing_time, sigtime); 
  return 0;
}


/*
  r_sig  = (sig-val
 	      (<algo>
 		(<param_name1> <mpi>)
 		...
 		(<param_namen> <mpi>)
 	      ))
  The sexp must be in canonical form.
  Note the <algo> must be given as a stringified OID or the special
  string "rsa".
 
  Note that IDX is only used for consistency checks.
 */
gpg_error_t
ksba_cms_set_sig_val (ksba_cms_t cms, int idx, ksba_const_sexp_t sigval)
{
  const unsigned char *s;
  unsigned long n;
  struct sig_val_s *sv, **sv_tail;
  int i;

  if (!cms)
    return gpg_error (GPG_ERR_INV_VALUE);
  if (idx < 0)
    return gpg_error (GPG_ERR_INV_INDEX); /* only one signer for now */

  s = sigval;
  if (*s != '(')
    return gpg_error (GPG_ERR_INV_SEXP);
  s++;

  for (i=0, sv_tail=&cms->sig_val; *sv_tail; sv_tail=&(*sv_tail)->next, i++)
    ;
  if (i != idx)
    return gpg_error (GPG_ERR_INV_INDEX); 

  if (!(n = snext (&s)))
    return gpg_error (GPG_ERR_INV_SEXP); 
  if (!smatch (&s, 7, "sig-val"))
    return gpg_error (GPG_ERR_UNKNOWN_SEXP);
  if (*s != '(')
    return gpg_error (digitp (s)? GPG_ERR_UNKNOWN_SEXP : GPG_ERR_INV_SEXP);
  s++;
 
  /* Break out the algorithm ID. */
  if (!(n = snext (&s)))
    return gpg_error (GPG_ERR_INV_SEXP); 

  sv = xtrycalloc (1, sizeof *sv);
  if (!sv)
    return gpg_error (GPG_ERR_ENOMEM);
  if (n==3 && s[0] == 'r' && s[1] == 's' && s[2] == 'a')
    { /* kludge to allow "rsa" to be passed as algorithm name */
      sv->algo = xtrystrdup ("1.2.840.113549.1.1.1");
      if (!sv->algo)
        {
          xfree (sv);
          return gpg_error (GPG_ERR_ENOMEM);
        }
    }
  else
    {
      sv->algo = xtrymalloc (n+1);
      if (!sv->algo)
        {
          xfree (sv);
          return gpg_error (GPG_ERR_ENOMEM);
        }
      memcpy (sv->algo, s, n);
      sv->algo[n] = 0;
    }
  s += n;

  /* And now the values - FIXME: For now we only support one */
  /* fixme: start loop */
  if (*s != '(')
    {
      xfree (sv->algo);
      xfree (sv);
      return gpg_error (digitp (s)? GPG_ERR_UNKNOWN_SEXP : GPG_ERR_INV_SEXP);
    }
  s++;

  if (!(n = snext (&s)))
    {
      xfree (sv->algo);
      xfree (sv);
      return gpg_error (GPG_ERR_INV_SEXP); 
    }
  s += n; /* ignore the name of the parameter */
  
  if (!digitp(s))
    {
      xfree (sv->algo);
      xfree (sv);
      /* May also be an invalid S-EXP.  */
      return gpg_error (GPG_ERR_UNKNOWN_SEXP);
    }

  if (!(n = snext (&s)))
    {
      xfree (sv->algo);
      xfree (sv);
      return gpg_error (GPG_ERR_INV_SEXP); 
    }

  if (n > 1 && !*s)
    { /* We might have a leading zero due to the way we encode 
         MPIs - this zero should not go into the OCTECT STRING.  */
      s++;
      n--;
    }
  sv->value = xtrymalloc (n);
  if (!sv->value)
    {
      xfree (sv->algo);
      xfree (sv);
      return gpg_error (GPG_ERR_ENOMEM);
    }
  memcpy (sv->value, s, n);
  sv->valuelen = n;
  s += n;
  if ( *s != ')')
    {
      xfree (sv->value);
      xfree (sv->algo);
      xfree (sv);
      return gpg_error (GPG_ERR_UNKNOWN_SEXP); /* but may also be an invalid one */
    }
  s++;
  /* fixme: end loop over parameters */

  /* we need 2 closing parenthesis */
  if ( *s != ')' || s[1] != ')')
    {
      xfree (sv->value);
      xfree (sv->algo);
      xfree (sv);
      return gpg_error (GPG_ERR_INV_SEXP); 
    }

  *sv_tail = sv;
  return 0;
}


/* Set the content encryption algorithm to OID and optionally set the
   initialization vector to IV */
gpg_error_t
ksba_cms_set_content_enc_algo (ksba_cms_t cms,
                               const char *oid,
                               const void *iv, size_t ivlen)
{
  if (!cms || !oid)
    return gpg_error (GPG_ERR_INV_VALUE);

  xfree (cms->encr_iv);
  cms->encr_iv = NULL;
  cms->encr_ivlen = 0;

  cms->encr_algo_oid = xtrystrdup (oid);
  if (!cms->encr_algo_oid)
    return gpg_error (GPG_ERR_ENOMEM);

  if (iv)
    {
      cms->encr_iv = xtrymalloc (ivlen);
      if (!cms->encr_iv)
        return gpg_error (GPG_ERR_ENOMEM);
      memcpy (cms->encr_iv, iv, ivlen);
      cms->encr_ivlen = ivlen;
    }
  return 0;
}


/* 
 * encval is expected to be a canonical encoded  S-Exp of this form:
 *  (enc-val
 *	(<algo>
 *	   (<param_name1> <mpi>)
 *	    ...
 *         (<param_namen> <mpi>)
 *	))
 *
 * Note the <algo> must be given as a stringified OID or the special
 * string "rsa" */
gpg_error_t
ksba_cms_set_enc_val (ksba_cms_t cms, int idx, ksba_const_sexp_t encval)
{
  /*FIXME: This shares most code with ...set_sig_val */
  struct certlist_s *cl;
  const char *s, *endp;
  unsigned long n;

  if (!cms)
    return gpg_error (GPG_ERR_INV_VALUE);
  if (idx < 0)
    return gpg_error (GPG_ERR_INV_INDEX); 
  for (cl=cms->cert_list; cl && idx; cl = cl->next, idx--)
    ;
  if (!cl)
    return gpg_error (GPG_ERR_INV_INDEX); /* no certificate to store the value */

  s = encval;
  if (*s != '(')
    return gpg_error (GPG_ERR_INV_SEXP);
  s++;

  n = strtoul (s, (char**)&endp, 10);
  s = endp;
  if (!n || *s!=':')
    return gpg_error (GPG_ERR_INV_SEXP); /* we don't allow empty lengths */
  s++;
  if (n != 7 || memcmp (s, "enc-val", 7))
    return gpg_error (GPG_ERR_UNKNOWN_SEXP);
  s += 7;
  if (*s != '(')
    return gpg_error (digitp (s)? GPG_ERR_UNKNOWN_SEXP : GPG_ERR_INV_SEXP);
  s++;

  /* break out the algorithm ID */
  n = strtoul (s, (char**)&endp, 10);
  s = endp;
  if (!n || *s != ':')
    return gpg_error (GPG_ERR_INV_SEXP); /* we don't allow empty lengths */
  s++;
  xfree (cl->enc_val.algo);
  if (n==3 && s[0] == 'r' && s[1] == 's' && s[2] == 'a')
    { /* kludge to allow "rsa" to be passed as algorithm name */
      cl->enc_val.algo = xtrystrdup ("1.2.840.113549.1.1.1");
      if (!cl->enc_val.algo)
        return gpg_error (GPG_ERR_ENOMEM);
    }
  else
    {
      cl->enc_val.algo = xtrymalloc (n+1);
      if (!cl->enc_val.algo)
        return gpg_error (GPG_ERR_ENOMEM);
      memcpy (cl->enc_val.algo, s, n);
      cl->enc_val.algo[n] = 0;
    }
  s += n;

  /* And now the values - FIXME: For now we only support one */
  /* fixme: start loop */
  if (*s != '(')
    return gpg_error (digitp (s)? GPG_ERR_UNKNOWN_SEXP : GPG_ERR_INV_SEXP);
  s++;
  n = strtoul (s, (char**)&endp, 10);
  s = endp;
  if (!n || *s != ':')
    return gpg_error (GPG_ERR_INV_SEXP); 
  s++;
  s += n; /* ignore the name of the parameter */
  
  if (!digitp(s))
    return gpg_error (GPG_ERR_UNKNOWN_SEXP); /* but may also be an invalid one */
  n = strtoul (s, (char**)&endp, 10);
  s = endp;
  if (!n || *s != ':')
    return gpg_error (GPG_ERR_INV_SEXP); 
  s++;
  if (n > 1 && !*s)
    { /* We might have a leading zero due to the way we encode 
         MPIs - this zero should not go into the OCTECT STRING.  */
      s++;
      n--;
    }
  xfree (cl->enc_val.value);
  cl->enc_val.value = xtrymalloc (n);
  if (!cl->enc_val.value)
    return gpg_error (GPG_ERR_ENOMEM);
  memcpy (cl->enc_val.value, s, n);
  cl->enc_val.valuelen = n;
  s += n;
  if ( *s != ')')
    return gpg_error (GPG_ERR_UNKNOWN_SEXP); /* but may also be an invalid one */
  s++;
  /* fixme: end loop over parameters */

  /* we need 2 closing parenthesis */
  if ( *s != ')' || s[1] != ')')
    return gpg_error (GPG_ERR_INV_SEXP); 

  return 0;
}




/**
 * ksba_cms_add_recipient:
 * @cms: A CMS object
 * @cert: A certificate used to describe the recipient.
 * 
 * This functions starts assembly of a new enveloped data content or adds
 * another recipient to the list of recipients.
 *
 * Note: after successful completion of this function ownership of
 * @cert is transferred to @cms.  
 * 
 * Return value: 0 on success or an error code.
 **/
gpg_error_t
ksba_cms_add_recipient (ksba_cms_t cms, ksba_cert_t cert)
{
  /* for now we use the same structure */
  return ksba_cms_add_signer (cms, cert);
}




/*
   Content handler for parsing messages
*/

static gpg_error_t 
ct_parse_data (ksba_cms_t cms)
{
  (void)cms;
  return gpg_error (GPG_ERR_NOT_IMPLEMENTED);
}


static gpg_error_t 
ct_parse_signed_data (ksba_cms_t cms)
{
  enum { 
    sSTART,
    sGOT_HASH,
    sIN_DATA,
    sERROR
  } state = sERROR;
  ksba_stop_reason_t stop_reason = cms->stop_reason;
  gpg_error_t err = 0;

  cms->stop_reason = KSBA_SR_RUNNING;

  /* Calculate state from last reason and do some checks */
  if (stop_reason == KSBA_SR_GOT_CONTENT)
    {
      state = sSTART;
    }
  else if (stop_reason == KSBA_SR_NEED_HASH)
    {
      state = sGOT_HASH;
    }
  else if (stop_reason == KSBA_SR_BEGIN_DATA)
    {
      if (!cms->hash_fnc)
        err = gpg_error (GPG_ERR_MISSING_ACTION);
      else
        state = sIN_DATA;
    }
  else if (stop_reason == KSBA_SR_END_DATA)
    {
      state = sGOT_HASH;
    }
  else if (stop_reason == KSBA_SR_RUNNING)
    err = gpg_error (GPG_ERR_INV_STATE);
  else if (stop_reason)
    err = gpg_error (GPG_ERR_BUG);
  
  if (err)
    return err;

  /* Do the action */
  if (state == sSTART)
    err = _ksba_cms_parse_signed_data_part_1 (cms);
  else if (state == sGOT_HASH)
    err = _ksba_cms_parse_signed_data_part_2 (cms);
  else if (state == sIN_DATA)
    err = read_and_hash_cont (cms);
  else
    err = gpg_error (GPG_ERR_INV_STATE);

  if (err)
    return err;

  /* Calculate new stop reason */
  if (state == sSTART)
    {
      if (cms->detached_data && !cms->data.digest)
        { /* We use this stop reason to inform the caller about a
             detached signatures.  Actually there is no need for him
             to hash the data now, he can do this also later. */
          stop_reason = KSBA_SR_NEED_HASH;
        }
      else 
        { /* The user must now provide a hash function so that we can 
             hash the data in the next round */
          stop_reason = KSBA_SR_BEGIN_DATA;
        }
    }
  else if (state == sIN_DATA)
    stop_reason = KSBA_SR_END_DATA;
  else if (state ==sGOT_HASH)
    stop_reason = KSBA_SR_READY;
    
  cms->stop_reason = stop_reason;
  return 0;
}


static gpg_error_t 
ct_parse_enveloped_data (ksba_cms_t cms)
{
  enum { 
    sSTART,
    sREST,
    sINDATA,
    sERROR
  } state = sERROR;
  ksba_stop_reason_t stop_reason = cms->stop_reason;
  gpg_error_t err = 0;

  cms->stop_reason = KSBA_SR_RUNNING;

  /* Calculate state from last reason and do some checks */
  if (stop_reason == KSBA_SR_GOT_CONTENT)
    {
      state = sSTART;
    }
  else if (stop_reason == KSBA_SR_DETACHED_DATA)
    {
      state = sREST;
    }
  else if (stop_reason == KSBA_SR_BEGIN_DATA)
    {
      state = sINDATA;
    }
  else if (stop_reason == KSBA_SR_END_DATA)
    {
      state = sREST;
    }
  else if (stop_reason == KSBA_SR_RUNNING)
    err = gpg_error (GPG_ERR_INV_STATE);
  else if (stop_reason)
    err = gpg_error (GPG_ERR_BUG);
  
  if (err)
    return err;

  /* Do the action */
  if (state == sSTART)
    err = _ksba_cms_parse_enveloped_data_part_1 (cms);
  else if (state == sREST)
    err = _ksba_cms_parse_enveloped_data_part_2 (cms);
  else if (state == sINDATA)
    err = read_encrypted_cont (cms);
  else
    err = gpg_error (GPG_ERR_INV_STATE);

  if (err)
    return err;

  /* Calculate new stop reason */
  if (state == sSTART)
    {
      stop_reason = cms->detached_data? KSBA_SR_DETACHED_DATA
                                      : KSBA_SR_BEGIN_DATA;
    }
  else if (state == sINDATA)
    stop_reason = KSBA_SR_END_DATA;
  else if (state ==sREST)
    stop_reason = KSBA_SR_READY;
    
  cms->stop_reason = stop_reason;
  return 0;
}


static gpg_error_t 
ct_parse_digested_data (ksba_cms_t cms)
{
  (void)cms;
  return gpg_error (GPG_ERR_NOT_IMPLEMENTED);
}


static gpg_error_t 
ct_parse_encrypted_data (ksba_cms_t cms)
{
  (void)cms;
  return gpg_error (GPG_ERR_NOT_IMPLEMENTED);
}



/*
   Content handlers for building messages
*/

static gpg_error_t 
ct_build_data (ksba_cms_t cms)
{
  (void)cms;
  return gpg_error (GPG_ERR_NOT_IMPLEMENTED);
}



/* Write everything up to the encapsulated data content type. */
static gpg_error_t
build_signed_data_header (ksba_cms_t cms)
{
  gpg_error_t err;
  unsigned char *buf;
  const char *s;
  size_t len;
  int i;

  /* Write the outer contentInfo. */
  err = _ksba_ber_write_tl (cms->writer, TYPE_SEQUENCE, CLASS_UNIVERSAL, 1, 0);
  if (err)
    return err;
  err = ksba_oid_from_str (cms->content.oid, &buf, &len);
  if (err)
    return err;
  err = _ksba_ber_write_tl (cms->writer,
                            TYPE_OBJECT_ID, CLASS_UNIVERSAL, 0, len);
  if (!err)
    err = ksba_writer_write (cms->writer, buf, len);
  xfree (buf);
  if (err)
    return err;
  
  err = _ksba_ber_write_tl (cms->writer, 0, CLASS_CONTEXT, 1, 0);
  if (err)
    return err;
  
  /* The SEQUENCE */
  err = _ksba_ber_write_tl (cms->writer, TYPE_SEQUENCE, CLASS_UNIVERSAL, 1, 0);
  if (err)
    return err;

  /* figure out the CMSVersion to be used */
  if (0 /* fixme: have_attribute_certificates 
           || encapsulated_content != data
           || any_signer_info_is_version_3*/ )
    s = "\x03";
  else
    s = "\x01";
  err = _ksba_ber_write_tl (cms->writer, TYPE_INTEGER, CLASS_UNIVERSAL, 0, 1);
  if (err)
    return err;
  err = ksba_writer_write (cms->writer, s, 1);
  if (err)
    return err;
  
  /* SET OF DigestAlgorithmIdentifier */
  {
    unsigned char *value;
    size_t valuelen;
    ksba_writer_t tmpwrt;

    err = ksba_writer_new (&tmpwrt);
    if (err)
      return err;
    err = ksba_writer_set_mem (tmpwrt, 512);
    if (err)
      {
        ksba_writer_release (tmpwrt);
        return err;
      }
    
    for (i=0; (s = ksba_cms_get_digest_algo_list (cms, i)); i++)
      {
        int j;
        const char *s2;

        /* (make sure not to write duplicates) */
        for (j=0; j < i && (s2=ksba_cms_get_digest_algo_list (cms, j)); j++)
          {
            if (!strcmp (s, s2))
              break;
          }
        if (j == i)
          {
            err = _ksba_der_write_algorithm_identifier (tmpwrt, s, NULL, 0);
            if (err)
              {
                ksba_writer_release (tmpwrt);
                return err;
              }
          }
      }
      
    value = ksba_writer_snatch_mem (tmpwrt, &valuelen);
    ksba_writer_release (tmpwrt);
    if (!value)
      {
        err = gpg_error (GPG_ERR_ENOMEM);
        return err;
      }
    err = _ksba_ber_write_tl (cms->writer, TYPE_SET, CLASS_UNIVERSAL,
                              1, valuelen);
    if (!err)
      err = ksba_writer_write (cms->writer, value, valuelen);
    xfree (value);
    if (err)
      return err;
  }



  /* Write the (inner) encapsulatedContentInfo */
  /* if we have a detached signature we don't need to use undefinite
     length here - but it doesn't matter either */
  err = _ksba_ber_write_tl (cms->writer, TYPE_SEQUENCE, CLASS_UNIVERSAL, 1, 0);
  if (err)
    return err;
  err = ksba_oid_from_str (cms->inner_cont_oid, &buf, &len);
  if (err)
    return err;
  err = _ksba_ber_write_tl (cms->writer,
                            TYPE_OBJECT_ID, CLASS_UNIVERSAL, 0, len);
  if (!err)
    err = ksba_writer_write (cms->writer, buf, len);
  xfree (buf);
  if (err)
    return err;

  if ( !cms->detached_data)
    { /* write the tag */
      err = _ksba_ber_write_tl (cms->writer, 0, CLASS_CONTEXT, 1, 0);
      if (err)
        return err;
    }

  return err;
}

/* Set the issuer/serial from the cert to the node.
   mode 0: sid 
   mode 1: rid
 */
static gpg_error_t
set_issuer_serial (AsnNode info, ksba_cert_t cert, int mode)
{
  gpg_error_t err;
  AsnNode dst, src;

  if (!info || !cert)
    return gpg_error (GPG_ERR_INV_VALUE);

  src = _ksba_asn_find_node (cert->root,
                             "Certificate.tbsCertificate.serialNumber");
  dst = _ksba_asn_find_node (info,
                             mode?
                             "rid.issuerAndSerialNumber.serialNumber":
                             "sid.issuerAndSerialNumber.serialNumber");
  err = _ksba_der_copy_tree (dst, src, cert->image);
  if (err)
    return err;

  src = _ksba_asn_find_node (cert->root,
                             "Certificate.tbsCertificate.issuer");
  dst = _ksba_asn_find_node (info,
                             mode?
                             "rid.issuerAndSerialNumber.issuer":
                             "sid.issuerAndSerialNumber.issuer");
  err = _ksba_der_copy_tree (dst, src, cert->image);
  if (err)
    return err;

  return 0;
}


/* Store the sequence of capabilities at NODE */
static gpg_error_t
store_smime_capability_sequence (AsnNode node,
                                 struct oidparmlist_s *capabilities)
{
  gpg_error_t err;
  struct oidparmlist_s *cap, *cap2;
  unsigned char *value;
  size_t valuelen;
  ksba_writer_t tmpwrt;

  err = ksba_writer_new (&tmpwrt);
  if (err)
    return err;
  err = ksba_writer_set_mem (tmpwrt, 512);
  if (err)
    {
      ksba_writer_release (tmpwrt);
      return err;
    }
    
  for (cap=capabilities; cap; cap = cap->next)
    {
      /* (avoid writing duplicates) */
      for (cap2=capabilities; cap2 != cap; cap2 = cap2->next)
        {
          if (!strcmp (cap->oid, cap2->oid)
              && cap->parmlen && cap->parmlen == cap2->parmlen
              && !memcmp (cap->parm, cap2->parm, cap->parmlen))
            break; /* Duplicate found. */
        }
      if (cap2 == cap)
        {
          /* RFC3851 requires that a missing parameter must not be
             encoded as NULL.  This is in contrast to all other usages
             of the algorithm identifier where ist is allowed and in
             some profiles (e.g. tmttv2) even explicitly suggested to
             use NULL.  */
          err = _ksba_der_write_algorithm_identifier
                 (tmpwrt, cap->oid, 
                  cap->parmlen?cap->parm:(const void*)"", cap->parmlen);
          if (err)
            {
              ksba_writer_release (tmpwrt);
              return err;
            }
        }
    }
  
  value = ksba_writer_snatch_mem (tmpwrt, &valuelen);
  if (!value)
    err = gpg_error (GPG_ERR_ENOMEM);
  if (!err)
    err = _ksba_der_store_sequence (node, value, valuelen);
  xfree (value);
  ksba_writer_release (tmpwrt);
  return err;
}


/* An object used to construct the signed attributes. */
struct attrarray_s {
  AsnNode root;
  unsigned char *image;
  size_t imagelen;
};


/* Thank you ASN.1 committee for allowing us to employ a sort to make
   that DER encoding even more complicate. */
static int
compare_attrarray (const void *a_v, const void *b_v)
{
  const struct attrarray_s *a = a_v;
  const struct attrarray_s *b = b_v;
  const unsigned char *ap, *bp;
  size_t an, bn;

  ap = a->image;
  an = a->imagelen;
  bp = b->image;
  bn = b->imagelen;
  for (; an && bn; an--, bn--, ap++, bp++ )
    if (*ap != *bp)
      return *ap - *bp;

  return (an == bn)? 0 : (an > bn)? 1 : -1;
}     




/* Write the END of data NULL tag and everything we can write before
   the user can calculate the signature */
static gpg_error_t
build_signed_data_attributes (ksba_cms_t cms) 
{
  gpg_error_t err;
  int signer;
  ksba_asn_tree_t cms_tree = NULL;
  struct certlist_s *certlist;
  struct oidlist_s *digestlist;
  struct signer_info_s *si, **si_tail;
  AsnNode root = NULL;
  struct attrarray_s attrarray[4]; 
  int attridx = 0;
  int i;

  memset (attrarray, 0, sizeof (attrarray));

  /* Write the End tag */
  err = _ksba_ber_write_tl (cms->writer, 0, 0, 0, 0);
  if (err)
    return err;

  if (cms->signer_info)
    return gpg_error (GPG_ERR_CONFLICT); /* This list must be empty at
                                            this point. */

  /* Write optional certificates */
  if (cms->cert_info_list)
    {
      unsigned long totallen = 0;
      const unsigned char *der;
      size_t n;
      
      for (certlist = cms->cert_info_list; certlist; certlist = certlist->next)
        {
          if (!ksba_cert_get_image (certlist->cert, &n))
            return gpg_error (GPG_ERR_GENERAL); /* User passed an
                                                   unitialized cert */
          totallen += n;
        }

      err = _ksba_ber_write_tl (cms->writer, 0, CLASS_CONTEXT, 1, totallen);
      if (err)
        return err;

      for (certlist = cms->cert_info_list; certlist; certlist = certlist->next)
        {
          if (!(der=ksba_cert_get_image (certlist->cert, &n)))
            return gpg_error (GPG_ERR_BUG); 
          err = ksba_writer_write (cms->writer, der, n);
          if (err )
            return err;
        }
    }

  /* If we ever support it, here is the right place to do it:
     Write the optional CRLs */

  /* Now we have to prepare the signer info.  For now we will just build the
     signedAttributes, so that the user can do the signature calculation */
  err = ksba_asn_create_tree ("cms", &cms_tree);
  if (err)
    return err;

  certlist = cms->cert_list;
  if (!certlist)
    {
      err = gpg_error (GPG_ERR_MISSING_VALUE); /* oops */
      goto leave;
    }
  digestlist = cms->digest_algos;
  if (!digestlist)
    {
      err = gpg_error (GPG_ERR_MISSING_VALUE); /* oops */
      goto leave;
    }

  si_tail = &cms->signer_info;
  for (signer=0; certlist;
       signer++, certlist = certlist->next, digestlist = digestlist->next)
    {
      AsnNode attr;
      AsnNode n;
      unsigned char *image;
      size_t imagelen;

      for (i = 0; i < attridx; i++)
        {
          _ksba_asn_release_nodes (attrarray[i].root);
          xfree (attrarray[i].image);
        }
      attridx = 0;
      memset (attrarray, 0, sizeof (attrarray));

      if (!digestlist)
        {
	  err = gpg_error (GPG_ERR_MISSING_VALUE); /* oops */
	  goto leave;
	}

      if (!certlist->cert || !digestlist->oid)
	{
	  err = gpg_error (GPG_ERR_BUG);
	  goto leave;
	}

      /* Include the pretty important message digest. */
      attr = _ksba_asn_expand_tree (cms_tree->parse_tree, 
                                    "CryptographicMessageSyntax.Attribute");
      if (!attr)
        {
	  err = gpg_error (GPG_ERR_ELEMENT_NOT_FOUND);
	  goto leave;
	}
      n = _ksba_asn_find_node (attr, "Attribute.attrType");
      if (!n)
        {
	  err = gpg_error (GPG_ERR_ELEMENT_NOT_FOUND);
	  goto leave;
	}
      err = _ksba_der_store_oid (n, oidstr_messageDigest);
      if (err)
        goto leave;
      n = _ksba_asn_find_node (attr, "Attribute.attrValues");
      if (!n || !n->down)
        return gpg_error (GPG_ERR_ELEMENT_NOT_FOUND);
      n = n->down; /* fixme: ugly hack */
      assert (certlist && certlist->msg_digest_len);
      err = _ksba_der_store_octet_string (n, certlist->msg_digest,
                                          certlist->msg_digest_len);
      if (err)
        goto leave;
      err = _ksba_der_encode_tree (attr, &image, &imagelen);
      if (err)
        goto leave;
      attrarray[attridx].root = attr;
      attrarray[attridx].image = image;
      attrarray[attridx].imagelen = imagelen;
      attridx++;

      /* Include the content-type attribute. */
      attr = _ksba_asn_expand_tree (cms_tree->parse_tree, 
                                    "CryptographicMessageSyntax.Attribute");
      if (!attr)
        {
	  err = gpg_error (GPG_ERR_ELEMENT_NOT_FOUND);
	  goto leave;
	}
      n = _ksba_asn_find_node (attr, "Attribute.attrType");
      if (!n)
        {
	  err = gpg_error (GPG_ERR_ELEMENT_NOT_FOUND);
	  goto leave;
	}
      err = _ksba_der_store_oid (n, oidstr_contentType);
      if (err)
	goto leave;
      n = _ksba_asn_find_node (attr, "Attribute.attrValues");
      if (!n || !n->down)
        {
	  err = gpg_error (GPG_ERR_ELEMENT_NOT_FOUND);
	  goto leave;
	}
      n = n->down; /* fixme: ugly hack */
      err = _ksba_der_store_oid (n, cms->inner_cont_oid);
      if (err)
        goto leave;
      err = _ksba_der_encode_tree (attr, &image, &imagelen);
      if (err)
        goto leave;
      attrarray[attridx].root = attr;
      attrarray[attridx].image = image;
      attrarray[attridx].imagelen = imagelen;
      attridx++;

      /* Include the signing time */
      if (certlist->signing_time)
        {
          attr = _ksba_asn_expand_tree (cms_tree->parse_tree, 
                                     "CryptographicMessageSyntax.Attribute");
          if (!attr)
            {
	      err = gpg_error (GPG_ERR_ELEMENT_NOT_FOUND);
	      goto leave;
	    }
          n = _ksba_asn_find_node (attr, "Attribute.attrType");
          if (!n)
            {
	      err = gpg_error (GPG_ERR_ELEMENT_NOT_FOUND);
	      goto leave;
	    }
          err = _ksba_der_store_oid (n, oidstr_signingTime);
          if (err)
            goto leave;
          n = _ksba_asn_find_node (attr, "Attribute.attrValues");
          if (!n || !n->down)
            {
	      err = gpg_error (GPG_ERR_ELEMENT_NOT_FOUND);
	      goto leave;
	    }
          n = n->down; /* fixme: ugly hack */
          err = _ksba_der_store_time (n, certlist->signing_time);
          if (err)
            goto leave;
          err = _ksba_der_encode_tree (attr, &image, &imagelen);
          if (err)
            goto leave;
          /* We will use the attributes again - so save them */
          attrarray[attridx].root = attr;
          attrarray[attridx].image = image;
          attrarray[attridx].imagelen = imagelen;
          attridx++;
        }

      /* Include the S/MIME capabilities with the first signer. */
      if (cms->capability_list && !signer)
        {
          attr = _ksba_asn_expand_tree (cms_tree->parse_tree, 
                                    "CryptographicMessageSyntax.Attribute");
          if (!attr)
            {
	      err = gpg_error (GPG_ERR_ELEMENT_NOT_FOUND);
	      goto leave;
	    }
          n = _ksba_asn_find_node (attr, "Attribute.attrType");
          if (!n)
            {
	      err = gpg_error (GPG_ERR_ELEMENT_NOT_FOUND);
	      goto leave;
	    }
          err = _ksba_der_store_oid (n, oidstr_smimeCapabilities);
          if (err)
            goto leave;
          n = _ksba_asn_find_node (attr, "Attribute.attrValues");
          if (!n || !n->down)
            {
	      err = gpg_error (GPG_ERR_ELEMENT_NOT_FOUND);
	      goto leave;
	    }
          n = n->down; /* fixme: ugly hack */
          err = store_smime_capability_sequence (n, cms->capability_list);
          if (err)
            goto leave;
          err = _ksba_der_encode_tree (attr, &image, &imagelen);
          if (err)
            goto leave;
          attrarray[attridx].root = attr;
          attrarray[attridx].image = image;
          attrarray[attridx].imagelen = imagelen;
          attridx++;
        }

      /* Arggh.  That silly ASN.1 DER encoding rules: We need to sort
         the SET values. */
      qsort (attrarray, attridx, sizeof (struct attrarray_s),
             compare_attrarray);

      /* Now copy them to an SignerInfo tree.  This tree is not
         complete but suitable for ksba_cms_hash_signed_attributes() */
      root = _ksba_asn_expand_tree (cms_tree->parse_tree,  
                                    "CryptographicMessageSyntax.SignerInfo"); 
      n = _ksba_asn_find_node (root, "SignerInfo.signedAttrs");
      if (!n || !n->down) 
        {
	  err = gpg_error (GPG_ERR_ELEMENT_NOT_FOUND); 
	  goto leave;
	}
      /* This is another ugly hack to move to the element we want */
      for (n = n->down->down; n && n->type != TYPE_SEQUENCE; n = n->right)
        ;
      if (!n) 
        {
	  err = gpg_error (GPG_ERR_ELEMENT_NOT_FOUND); 
	  goto leave;
	}

      assert (attridx <= DIM (attrarray));
      for (i=0; i < attridx; i++)
        {
          if (i)
            {
              if ( !(n=_ksba_asn_insert_copy (n)))
                {
		  err = gpg_error (GPG_ERR_ENOMEM);
		  goto leave;
		}
            }
          err = _ksba_der_copy_tree (n, attrarray[i].root, attrarray[i].image);
          if (err)
            goto leave;
	  _ksba_asn_release_nodes (attrarray[i].root);
	  free (attrarray[i].image);
	  attrarray[i].root = NULL;
	  attrarray[i].image = NULL;
        }

      err = _ksba_der_encode_tree (root, &image, NULL);
      if (err)
        goto leave;
      
      si = xtrycalloc (1, sizeof *si);
      if (!si)
        return gpg_error (GPG_ERR_ENOMEM);
      si->root = root;
      root = NULL;
      si->image = image;
      /* Hmmm, we don't set the length of the image. */
      *si_tail = si;
      si_tail = &si->next;
    }

 leave:
  _ksba_asn_release_nodes (root);
  ksba_asn_tree_release (cms_tree);
  for (i = 0; i < attridx; i++)
    {
      _ksba_asn_release_nodes (attrarray[i].root);
      xfree (attrarray[i].image);
    }

  return err;
}




/* The user has calculated the signatures and we can therefore write
   everything left over to do. */
static gpg_error_t 
build_signed_data_rest (ksba_cms_t cms) 
{
  gpg_error_t err;
  int signer;
  ksba_asn_tree_t cms_tree = NULL;
  struct certlist_s *certlist;
  struct oidlist_s *digestlist;
  struct signer_info_s *si;
  struct sig_val_s *sv;
  ksba_writer_t tmpwrt = NULL;
  AsnNode root = NULL;

  /* Now we can really write the signer info */
  err = ksba_asn_create_tree ("cms", &cms_tree);
  if (err)
    return err;

  certlist = cms->cert_list;
  if (!certlist)
    {
      err = gpg_error (GPG_ERR_MISSING_VALUE); /* oops */
      return err;
    }

  /* To construct the set we use a temporary writer object. */
  err = ksba_writer_new (&tmpwrt);
  if (err)
    goto leave;
  err = ksba_writer_set_mem (tmpwrt, 2048);
  if (err)
    goto leave;

  digestlist = cms->digest_algos;
  si = cms->signer_info;
  sv = cms->sig_val;

  for (signer=0; certlist;
       signer++,
         certlist = certlist->next,
         digestlist = digestlist->next,
         si = si->next,
         sv = sv->next)
    {
      AsnNode n, n2;
      unsigned char *image;
      size_t imagelen;

      if (!digestlist || !si || !sv)
        {
	  err = gpg_error (GPG_ERR_MISSING_VALUE); /* oops */
	  goto leave;
	}
      if (!certlist->cert || !digestlist->oid)
        {
	  err = gpg_error (GPG_ERR_BUG);
	  goto leave;
	}

      root = _ksba_asn_expand_tree (cms_tree->parse_tree, 
                                    "CryptographicMessageSyntax.SignerInfo");

      /* We store a version of 1 because we use the issuerAndSerialNumber */
      n = _ksba_asn_find_node (root, "SignerInfo.version");
      if (!n)
	{
	  err = gpg_error (GPG_ERR_ELEMENT_NOT_FOUND);
	  goto leave;
	}
      err = _ksba_der_store_integer (n, "\x00\x00\x00\x01\x01");
      if (err)
        goto leave;

      /* Store the sid */
      n = _ksba_asn_find_node (root, "SignerInfo.sid");
      if (!n)
        {
	  err = gpg_error (GPG_ERR_ELEMENT_NOT_FOUND);
	  goto leave;
	}

      err = set_issuer_serial (n, certlist->cert, 0);
      if (err)
        goto leave;

      /* store the digestAlgorithm */
      n = _ksba_asn_find_node (root, "SignerInfo.digestAlgorithm.algorithm");
      if (!n)
	{
	  err = gpg_error (GPG_ERR_ELEMENT_NOT_FOUND);
	  goto leave;
	}
      err = _ksba_der_store_oid (n, digestlist->oid);
      if (err)
        goto leave;
      n = _ksba_asn_find_node (root, "SignerInfo.digestAlgorithm.parameters");
      if (!n)
        {
	  err = gpg_error (GPG_ERR_ELEMENT_NOT_FOUND);
	  goto leave;
	}
      err = _ksba_der_store_null (n);
      if (err)
        goto leave;

      /* and the signed attributes */
      n = _ksba_asn_find_node (root, "SignerInfo.signedAttrs");
      if (!n || !n->down) 
        {
	  err = gpg_error (GPG_ERR_ELEMENT_NOT_FOUND); 
	  goto leave;
	}
      assert (si->root);
      assert (si->image);
      n2 = _ksba_asn_find_node (si->root, "SignerInfo.signedAttrs");
      if (!n2 || !n->down) 
        {
	  err = gpg_error (GPG_ERR_ELEMENT_NOT_FOUND);
	  goto leave;
	}
      err = _ksba_der_copy_tree (n, n2, si->image);
      if (err)
        goto leave;
      image = NULL;

      /* store the signatureAlgorithm */
      n = _ksba_asn_find_node (root,
			       "SignerInfo.signatureAlgorithm.algorithm");
      if (!n)
        {
	  err = gpg_error (GPG_ERR_ELEMENT_NOT_FOUND);
	  goto leave;
	}
      if (!sv->algo)
        {
	  err = gpg_error (GPG_ERR_MISSING_VALUE);
	  goto leave;
	}
      err = _ksba_der_store_oid (n, sv->algo);
      if (err)
	goto leave;
      n = _ksba_asn_find_node (root,
			       "SignerInfo.signatureAlgorithm.parameters");
      if (!n)
        {
	  err = gpg_error (GPG_ERR_ELEMENT_NOT_FOUND);
	  goto leave;
	}
      err = _ksba_der_store_null (n);
      if (err)
	goto leave;

      /* store the signature  */
      if (!sv->value)
        {
	  err = gpg_error (GPG_ERR_MISSING_VALUE);
	  goto leave;
	}
      n = _ksba_asn_find_node (root, "SignerInfo.signature");
      if (!n)
	{
	  err = gpg_error (GPG_ERR_ELEMENT_NOT_FOUND);
	  goto leave;
	}
      err = _ksba_der_store_octet_string (n, sv->value, sv->valuelen);
      if (err)
	goto leave;

      /* Make the DER encoding and write it out. */
      err = _ksba_der_encode_tree (root, &image, &imagelen);
      if (err)
	goto leave;

      err = ksba_writer_write (tmpwrt, image, imagelen);
      xfree (image);
      if (err)
	goto leave;
    }

  /* Write out the SET filled with all signer infos */
  {
    unsigned char *value;
    size_t valuelen;

    value = ksba_writer_snatch_mem (tmpwrt, &valuelen);
    if (!value)
      {
        err = gpg_error (GPG_ERR_ENOMEM);
	goto leave;
      }
    err = _ksba_ber_write_tl (cms->writer, TYPE_SET, CLASS_UNIVERSAL,
                              1, valuelen);
    if (!err)
      err = ksba_writer_write (cms->writer, value, valuelen);
    xfree (value);
    if (err)
      goto leave;
  }

  /* Write 3 end tags */
  err = _ksba_ber_write_tl (cms->writer, 0, 0, 0, 0);
  if (!err)
    err = _ksba_ber_write_tl (cms->writer, 0, 0, 0, 0);
  if (!err)
    err = _ksba_ber_write_tl (cms->writer, 0, 0, 0, 0);

 leave:
  ksba_asn_tree_release (cms_tree);
  _ksba_asn_release_nodes (root);
  ksba_writer_release (tmpwrt);

  return err;
}




static gpg_error_t 
ct_build_signed_data (ksba_cms_t cms)
{
  enum { 
    sSTART,
    sDATAREADY,
    sGOTSIG,
    sERROR
  } state = sERROR;
  ksba_stop_reason_t stop_reason;
  gpg_error_t err = 0;

  stop_reason = cms->stop_reason;
  cms->stop_reason = KSBA_SR_RUNNING;

  /* Calculate state from last reason and do some checks */
  if (stop_reason == KSBA_SR_GOT_CONTENT)
    {
      state = sSTART;
    }
  else if (stop_reason == KSBA_SR_BEGIN_DATA)
    {
      /* fixme: check that the message digest has been set */
      state = sDATAREADY;
    }
  else if (stop_reason == KSBA_SR_END_DATA)
    state = sDATAREADY;
  else if (stop_reason == KSBA_SR_NEED_SIG)
    {
      if (!cms->sig_val)
        err = gpg_error (GPG_ERR_MISSING_ACTION); /* No ksba_cms_set_sig_val () called */
      state = sGOTSIG;
    }
  else if (stop_reason == KSBA_SR_RUNNING)
    err = gpg_error (GPG_ERR_INV_STATE);
  else if (stop_reason)
    err = gpg_error (GPG_ERR_BUG);
  
  if (err)
    return err;

  /* Do the action */
  if (state == sSTART)
    {
      /* figure out whether a detached signature is requested */
      if (cms->cert_list && cms->cert_list->msg_digest_len)
        cms->detached_data = 1;
      else
        cms->detached_data = 0;
      /* and start encoding */
      err = build_signed_data_header (cms);
    }
  else if (state == sDATAREADY)
    {
      if (!cms->detached_data)
        err = _ksba_ber_write_tl (cms->writer, 0, 0, 0, 0);
      if (!err)
        err = build_signed_data_attributes (cms);
    }
  else if (state == sGOTSIG)
    err = build_signed_data_rest (cms);
  else
    err = gpg_error (GPG_ERR_INV_STATE);

  if (err)
    return err;

  /* Calculate new stop reason */
  if (state == sSTART)
    {
      /* user should write the data and calculate the hash or do
         nothing in case of END_DATA */
      stop_reason = cms->detached_data? KSBA_SR_END_DATA
                                      : KSBA_SR_BEGIN_DATA;
    }
  else if (state == sDATAREADY)
    stop_reason = KSBA_SR_NEED_SIG;
  else if (state == sGOTSIG)
    stop_reason = KSBA_SR_READY;
    
  cms->stop_reason = stop_reason;
  return 0;
}


/* write everything up to the encryptedContentInfo including the tag */
static gpg_error_t
build_enveloped_data_header (ksba_cms_t cms)
{
  gpg_error_t err;
  int recpno;
  ksba_asn_tree_t cms_tree = NULL;
  struct certlist_s *certlist;
  unsigned char *buf;
  const char *s;
  size_t len;
  ksba_writer_t tmpwrt = NULL;

  /* Write the outer contentInfo */
  /* fixme: code is shared with signed_data_header */
  err = _ksba_ber_write_tl (cms->writer, TYPE_SEQUENCE, CLASS_UNIVERSAL, 1, 0);
  if (err)
    return err;
  err = ksba_oid_from_str (cms->content.oid, &buf, &len);
  if (err)
    return err;
  err = _ksba_ber_write_tl (cms->writer,
                            TYPE_OBJECT_ID, CLASS_UNIVERSAL, 0, len);
  if (!err)
    err = ksba_writer_write (cms->writer, buf, len);
  xfree (buf);
  if (err)
    return err;
  
  err = _ksba_ber_write_tl (cms->writer, 0, CLASS_CONTEXT, 1, 0);
  if (err)
    return err;
  
  /* The SEQUENCE */
  err = _ksba_ber_write_tl (cms->writer, TYPE_SEQUENCE, CLASS_UNIVERSAL, 1, 0);
  if (err)
    return err;

  /* figure out the CMSVersion to be used (from rfc2630):
     version is the syntax version number.  If originatorInfo is
     present, then version shall be 2.  If any of the RecipientInfo
     structures included have a version other than 0, then the version
     shall be 2.  If unprotectedAttrs is present, then version shall
     be 2.  If originatorInfo is absent, all of the RecipientInfo
     structures are version 0, and unprotectedAttrs is absent, then
     version shall be 0. 
     
     For SPHINX the version number must be 0.
  */
  s = "\x00"; 
  err = _ksba_ber_write_tl (cms->writer, TYPE_INTEGER, CLASS_UNIVERSAL, 0, 1);
  if (err)
    return err;
  err = ksba_writer_write (cms->writer, s, 1);
  if (err)
    return err;

  /* Note: originatorInfo is not yet implemented and must not be used
     for SPHINX */

  /* Now we write the recipientInfo */
  err = ksba_asn_create_tree ("cms", &cms_tree);
  if (err)
    return err;

  certlist = cms->cert_list;
  if (!certlist)
    {
      err = gpg_error (GPG_ERR_MISSING_VALUE); /* oops */
      goto leave;
    }

  /* To construct the set we use a temporary writer object */
  err = ksba_writer_new (&tmpwrt);
  if (err)
    goto leave;
  err = ksba_writer_set_mem (tmpwrt, 2048);
  if (err)
    goto leave;

  for (recpno=0; certlist; recpno++, certlist = certlist->next)
    {
      AsnNode root, n;
      unsigned char *image;
      size_t imagelen;

      if (!certlist->cert)
        {
          err = gpg_error (GPG_ERR_BUG);
          goto leave;
        }

      root = _ksba_asn_expand_tree (cms_tree->parse_tree, 
                                "CryptographicMessageSyntax.RecipientInfo");

      /* We store a version of 0 because we are only allowed to use
         the issuerAndSerialNumber for SPHINX */
      n = _ksba_asn_find_node (root, "RecipientInfo.ktri.version");
      if (!n)
        {
          err = gpg_error (GPG_ERR_ELEMENT_NOT_FOUND);
          goto leave;
        }
      err = _ksba_der_store_integer (n, "\x00\x00\x00\x01\x00");
      if (err)
        goto leave;

      /* Store the rid */
      n = _ksba_asn_find_node (root, "RecipientInfo.ktri.rid");
      if (!n)
        {
          err = gpg_error (GPG_ERR_ELEMENT_NOT_FOUND);
          goto leave;
        }

      err = set_issuer_serial (n, certlist->cert, 1);
      if (err)
        goto leave;

      /* store the keyEncryptionAlgorithm */
      if (!certlist->enc_val.algo || !certlist->enc_val.value)
        return gpg_error (GPG_ERR_MISSING_VALUE);
      n = _ksba_asn_find_node (root, 
                  "RecipientInfo.ktri.keyEncryptionAlgorithm.algorithm");
      if (!n)
        {
          err = gpg_error (GPG_ERR_ELEMENT_NOT_FOUND);
          goto leave;
        }
      err = _ksba_der_store_oid (n, certlist->enc_val.algo);
      if (err)
        goto leave;
      n = _ksba_asn_find_node (root, 
                  "RecipientInfo.ktri.keyEncryptionAlgorithm.parameters");
      if (!n)
        {
          err = gpg_error (GPG_ERR_ELEMENT_NOT_FOUND);
          goto leave;
        }

      /* Now store NULL for the optional parameters.  From Peter
       * Gutmann's X.509 style guide:
       *
       *   Another pitfall to be aware of is that algorithms which
       *   have no parameters have this specified as a NULL value
       *   rather than omitting the parameters field entirely.  The
       *   reason for this is that when the 1988 syntax for
       *   AlgorithmIdentifier was translated into the 1997 syntax,
       *   the OPTIONAL associated with the AlgorithmIdentifier
       *   parameters got lost.  Later it was recovered via a defect
       *   report, but by then everyone thought that algorithm
       *   parameters were mandatory.  Because of this the algorithm
       *   parameters should be specified as NULL, regardless of what
       *   you read elsewhere.
       *
       *        The trouble is that things *never* get better, they just
       *        stay the same, only more so
       *            -- Terry Pratchett, "Eric"
       *
       * Although this is about signing, we always do it.  Versions of
       * Libksba before 1.0.6 had a bug writing out the NULL tag here,
       * thus in reality we used to be correct according to the
       * standards despite we didn't intended so.
       */

      err = _ksba_der_store_null (n); 
      if (err)
        goto leave;

      /* store the encryptedKey  */
      if (!certlist->enc_val.value)
        {
          err = gpg_error (GPG_ERR_MISSING_VALUE);
          goto leave;
        }
      n = _ksba_asn_find_node (root, "RecipientInfo.ktri.encryptedKey");
      if (!n)
        {
          err = gpg_error (GPG_ERR_ELEMENT_NOT_FOUND);
          goto leave;
        }
      err = _ksba_der_store_octet_string (n,
                                          certlist->enc_val.value,
                                          certlist->enc_val.valuelen);
      if (err)
        goto leave;


      /* Make the DER encoding and write it out */
      err = _ksba_der_encode_tree (root, &image, &imagelen);
      if (err)
          goto leave;

      err = ksba_writer_write (tmpwrt, image, imagelen);
      if (err)
        goto leave;

      xfree (image);
      _ksba_asn_release_nodes (root);
    }

  ksba_asn_tree_release (cms_tree);
  cms_tree = NULL;

  /* Write out the SET filled with all recipient infos */
  {
    unsigned char *value;
    size_t valuelen;

    value = ksba_writer_snatch_mem (tmpwrt, &valuelen);
    if (!value)
      {
        err = gpg_error (GPG_ERR_ENOMEM);
        goto leave;
      }
    ksba_writer_release (tmpwrt);
    tmpwrt = NULL;
    err = _ksba_ber_write_tl (cms->writer, TYPE_SET, CLASS_UNIVERSAL,
                              1, valuelen);
    if (!err)
      err = ksba_writer_write (cms->writer, value, valuelen);
    xfree (value);
    if (err)
      goto leave;
  }

  
  /* Write the (inner) encryptedContentInfo */
  err = _ksba_ber_write_tl (cms->writer, TYPE_SEQUENCE, CLASS_UNIVERSAL, 1, 0);
  if (err)
    return err;
  err = ksba_oid_from_str (cms->inner_cont_oid, &buf, &len);
  if (err)
    return err;
  err = _ksba_ber_write_tl (cms->writer,
                            TYPE_OBJECT_ID, CLASS_UNIVERSAL, 0, len);
  if (!err)
    err = ksba_writer_write (cms->writer, buf, len);
  xfree (buf);
  if (err)
    return err;

  /* and the encryptionAlgorithm */
  err = _ksba_der_write_algorithm_identifier (cms->writer, 
                                              cms->encr_algo_oid,
                                              cms->encr_iv,
                                              cms->encr_ivlen);
  if (err)
    return err;

  /* write the tag for the encrypted data, it is an implicit octect
     string in constructed form and indefinite length */
  err = _ksba_ber_write_tl (cms->writer, 0, CLASS_CONTEXT, 1, 0);
  if (err)
    return err;

  /* Now the encrypted data should be written */

 leave:
  ksba_writer_release (tmpwrt);
  ksba_asn_tree_release (cms_tree);
  return err;
}


static gpg_error_t 
ct_build_enveloped_data (ksba_cms_t cms)
{
  enum { 
    sSTART,
    sINDATA,
    sREST,
    sERROR
  } state = sERROR;
  ksba_stop_reason_t stop_reason;
  gpg_error_t err = 0;

  stop_reason = cms->stop_reason;
  cms->stop_reason = KSBA_SR_RUNNING;

  /* Calculate state from last reason and do some checks */
  if (stop_reason == KSBA_SR_GOT_CONTENT)
    state = sSTART;
  else if (stop_reason == KSBA_SR_BEGIN_DATA)
    state = sINDATA;
  else if (stop_reason == KSBA_SR_END_DATA)
    state = sREST;
  else if (stop_reason == KSBA_SR_RUNNING)
    err = gpg_error (GPG_ERR_INV_STATE);
  else if (stop_reason)
    err = gpg_error (GPG_ERR_BUG);
  
  if (err)
    return err;

  /* Do the action */
  if (state == sSTART)
    err = build_enveloped_data_header (cms);
  else if (state == sINDATA)
    err = write_encrypted_cont (cms);
  else if (state == sREST)
    {
      /* SPHINX does not allow for unprotectedAttributes */

      /* Write 5 end tags */
      err = _ksba_ber_write_tl (cms->writer, 0, 0, 0, 0);
      if (!err)
        err = _ksba_ber_write_tl (cms->writer, 0, 0, 0, 0);
      if (!err)
        err = _ksba_ber_write_tl (cms->writer, 0, 0, 0, 0);
      if (!err)
        err = _ksba_ber_write_tl (cms->writer, 0, 0, 0, 0);
    }
  else
    err = gpg_error (GPG_ERR_INV_STATE);

  if (err)
    return err;

  /* Calculate new stop reason */
  if (state == sSTART)
    { /* user should now write the encrypted data */
      stop_reason = KSBA_SR_BEGIN_DATA;
    }
  else if (state == sINDATA)
    { /* tell the user that we wrote everything */
      stop_reason = KSBA_SR_END_DATA;
    }
  else if (state == sREST)
    {
      stop_reason = KSBA_SR_READY;
    }

  cms->stop_reason = stop_reason;
  return 0;
}


static gpg_error_t 
ct_build_digested_data (ksba_cms_t cms)
{
  (void)cms;
  return gpg_error (GPG_ERR_NOT_IMPLEMENTED);
}


static gpg_error_t 
ct_build_encrypted_data (ksba_cms_t cms)
{
  (void)cms;
  return gpg_error (GPG_ERR_NOT_IMPLEMENTED);
}


