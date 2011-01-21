/* cms-parse.c - parse cryptographic message syntax
 *      Copyright (C) 2001 g10 Code GmbH
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

/*
   We handle CMS by using a handcrafted parser for the outer
   structures and the generic parser of the parts we can handle in
   memory.  Extending the generic parser to allow hooks for indefinite
   length objects and to auto select the object depending on the
   content type OID is too complicated.
*/


#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "util.h"

#include "cms.h"
#include "asn1-func.h" /* need some constants */
#include "ber-decoder.h"
#include "ber-help.h"
#include "keyinfo.h"

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
static gpg_error_t
create_and_run_decoder (ksba_reader_t reader, const char *elem_name,
                        unsigned int flags,
                        AsnNode *r_root,
                        unsigned char **r_image, size_t *r_imagelen)
{
  gpg_error_t err;
  ksba_asn_tree_t cms_tree;
  BerDecoder decoder;

  err = ksba_asn_create_tree ("cms", &cms_tree);
  if (err)
    return err;

  decoder = _ksba_ber_decoder_new ();
  if (!decoder)
    {
      ksba_asn_tree_release (cms_tree);
      return gpg_error (GPG_ERR_ENOMEM);
    }

  err = _ksba_ber_decoder_set_reader (decoder, reader);
  if (err)
    {
      ksba_asn_tree_release (cms_tree);
      _ksba_ber_decoder_release (decoder);
      return err;
    }

  err = _ksba_ber_decoder_set_module (decoder, cms_tree);
  if (err)
    {
      ksba_asn_tree_release (cms_tree);
      _ksba_ber_decoder_release (decoder);
      return err;
    }
  
  err = _ksba_ber_decoder_decode (decoder, elem_name, flags,
                                  r_root, r_image, r_imagelen);
  
  _ksba_ber_decoder_release (decoder);
  ksba_asn_tree_release (cms_tree);
  return err;
}



/* Parse this structure and return the oid of the content.  The read
   position is then located at the value of content.  This fucntion is
   the core for parsing ContentInfo and EncapsulatedContentInfo.

   ContentInfo ::= SEQUENCE {
      contentType ContentType, 
      content [0] EXPLICIT ANY DEFINED BY contentType 
   }
   ContentType ::= OBJECT IDENTIFIER

   Returns: 0 on success or an error code. Other values are returned
   by the parameters.

*/
static gpg_error_t
parse_content_info (ksba_reader_t reader,
                    unsigned long *r_len, int *r_ndef,
                    char **r_oid, int *has_content)
{
  struct tag_info ti;
  gpg_error_t err;
  int content_ndef;
  unsigned long content_len;
  unsigned char oidbuf[100]; /* pretty large for an OID */
  char *oid = NULL;

  /* read the sequence triplet */
  err = _ksba_ber_read_tl (reader, &ti);
  if (err)
    return err;
  if ( !(ti.class == CLASS_UNIVERSAL && ti.tag == TYPE_SEQUENCE
         && ti.is_constructed) )
    return gpg_error (GPG_ERR_INV_CMS_OBJ);
  content_len = ti.length; 
  content_ndef = ti.ndef;
  if (!content_ndef && content_len < 3)
    return gpg_error (GPG_ERR_TOO_SHORT); /* to encode an OID */

  /* read the OID */
  err = _ksba_ber_read_tl (reader, &ti);
  if (err)
    return err;
  if ( !(ti.class == CLASS_UNIVERSAL && ti.tag == TYPE_OBJECT_ID
         && !ti.is_constructed && ti.length) )
    return gpg_error (GPG_ERR_INV_CMS_OBJ); 
  if (!content_ndef)
    {
      if (content_len < ti.nhdr)
        return gpg_error (GPG_ERR_BAD_BER); /* triplet header larger that sequence */
      content_len -= ti.nhdr;
      if (content_len < ti.length)
        return gpg_error (GPG_ERR_BAD_BER); /* triplet larger that sequence */
      content_len -= ti.length;
    }

  if (ti.length >= DIM(oidbuf))
    return gpg_error (GPG_ERR_TOO_LARGE);
  err = read_buffer (reader, oidbuf, ti.length);
  if (err)
    return err;
  oid = ksba_oid_to_str (oidbuf, ti.length);
  if (!oid)
    return gpg_error (GPG_ERR_ENOMEM);

  if (!content_ndef && !content_len)
    { /* no data */
      *has_content = 0;
    }
  else
    { /* now read the explicit tag 0 which is optional */
      err = _ksba_ber_read_tl (reader, &ti);
      if (err)
        {
          xfree (oid);
          return err;
        }

      if ( ti.class == CLASS_CONTEXT && ti.tag == 0 && ti.is_constructed )
        {
          *has_content = 1;
        }
      else if ( ti.class == CLASS_UNIVERSAL && ti.tag == 0 && !ti.is_constructed )
        {
          *has_content = 0; /* this is optional - allow NUL tag */
        }
      else /* neither [0] nor NULL */
        {
          xfree (oid);
          return gpg_error (GPG_ERR_INV_CMS_OBJ); 
        }
      if (!content_ndef)
        {
          if (content_len < ti.nhdr)
            return gpg_error (GPG_ERR_BAD_BER); /* triplet header larger that sequence */
          content_len -= ti.nhdr;
          if (!ti.ndef && content_len < ti.length)
            return gpg_error (GPG_ERR_BAD_BER); /* triplet larger that sequence */
        }
    }
  *r_len = content_len;
  *r_ndef = content_ndef;
  *r_oid = oid;
  return 0;
}


/* Parse this structure and return the oid of the content as well as
   the algorithm identifier.  The read position is then located at the
   value of the octect string.

   EncryptedContentInfo ::= SEQUENCE {
     contentType OBJECT IDENTIFIER,
     contentEncryptionAlgorithm ContentEncryptionAlgorithmIdentifier,
     encryptedContent [0] IMPLICIT OCTET STRING OPTIONAL }

   Returns: 0 on success or an error code. Other values are returned
   by the parameters.
*/
static gpg_error_t
parse_encrypted_content_info (ksba_reader_t reader,
                              unsigned long *r_len, int *r_ndef,
                              char **r_cont_oid, char **r_algo_oid,
                              char **r_algo_parm, size_t *r_algo_parmlen,
                              int *has_content)
{
  struct tag_info ti;
  gpg_error_t err;
  int content_ndef;
  unsigned long content_len;
  unsigned char tmpbuf[500]; /* for OID or algorithmIdentifier */
  char *cont_oid = NULL;
  char *algo_oid = NULL;
  char *algo_parm = NULL;
  size_t algo_parmlen;
  size_t nread;

  /* Fixme: release oids in case of errors */

  /* read the sequence triplet */
  err = _ksba_ber_read_tl (reader, &ti);
  if (err)
    return err;
  if ( !(ti.class == CLASS_UNIVERSAL && ti.tag == TYPE_SEQUENCE
         && ti.is_constructed) )
    return gpg_error (GPG_ERR_INV_CMS_OBJ);
  content_len = ti.length; 
  content_ndef = ti.ndef;
  if (!content_ndef && content_len < 3)
    return gpg_error (GPG_ERR_TOO_SHORT); /* to encode an OID */

  /* read the OID */
  err = _ksba_ber_read_tl (reader, &ti);
  if (err)
    return err;
  if ( !(ti.class == CLASS_UNIVERSAL && ti.tag == TYPE_OBJECT_ID
         && !ti.is_constructed && ti.length) )
    return gpg_error (GPG_ERR_INV_CMS_OBJ); 
  if (!content_ndef)
    {
      if (content_len < ti.nhdr)
        return gpg_error (GPG_ERR_BAD_BER); /* triplet header larger that sequence */
      content_len -= ti.nhdr;
      if (content_len < ti.length)
        return gpg_error (GPG_ERR_BAD_BER); /* triplet larger that sequence */
      content_len -= ti.length;
    }
  if (ti.length >= DIM(tmpbuf))
    return gpg_error (GPG_ERR_TOO_LARGE);
  err = read_buffer (reader, tmpbuf, ti.length);
  if (err)
    return err;
  cont_oid = ksba_oid_to_str (tmpbuf, ti.length);
  if (!cont_oid)
    return gpg_error (GPG_ERR_ENOMEM);

  /* read the algorithmIdentifier */
  err = _ksba_ber_read_tl (reader, &ti);
  if (err)
    return err;
  if ( !(ti.class == CLASS_UNIVERSAL && ti.tag == TYPE_SEQUENCE
         && ti.is_constructed) )
    return gpg_error (GPG_ERR_INV_CMS_OBJ);
  if (!content_ndef)
    {
      if (content_len < ti.nhdr)
        return gpg_error (GPG_ERR_BAD_BER); /* triplet header larger that sequence */
      content_len -= ti.nhdr;
      if (content_len < ti.length)
        return gpg_error (GPG_ERR_BAD_BER); /* triplet larger that sequence */
      content_len -= ti.length;
    }
  if (ti.nhdr + ti.length >= DIM(tmpbuf))
    return gpg_error (GPG_ERR_TOO_LARGE);
  memcpy (tmpbuf, ti.buf, ti.nhdr);
  err = read_buffer (reader, tmpbuf+ti.nhdr, ti.length);
  if (err)
    return err;
  err = _ksba_parse_algorithm_identifier2 (tmpbuf, ti.nhdr+ti.length,
                                           &nread,&algo_oid,
                                           &algo_parm, &algo_parmlen);
  if (err)
    return err;
  assert (nread <= ti.nhdr + ti.length);
  if (nread < ti.nhdr + ti.length)
    return gpg_error (GPG_ERR_TOO_SHORT);

  /* the optional encryptedDataInfo */
  *has_content = 0;
  if (content_ndef || content_len)
    { /* now read the implicit tag 0.  Actually this is optional but
         in that case we don't expect to have a content_len - well, it
         may be the end tag */
      err = _ksba_ber_read_tl (reader, &ti);
      if (err)
        {
          xfree (cont_oid);
          xfree (algo_oid);
          return err;
        }

      /* Note: the tag may either denote a constructed or a primitve
         object.  Actually this should match the use of NDEF header
         but we don't ceck that */
      if ( ti.class == CLASS_CONTEXT && ti.tag == 0 )
        {
          *has_content = 1;
          if (!content_ndef)
            {
              if (content_len < ti.nhdr)
                return gpg_error (GPG_ERR_BAD_BER); 
              content_len -= ti.nhdr;
              if (!ti.ndef && content_len < ti.length)
                return gpg_error (GPG_ERR_BAD_BER); 
            }
        }
      else /* not what we want - push it back */
        {
          *has_content = 0;
          err = ksba_reader_unread (reader, ti.buf, ti.nhdr);
          if (err)
            return err;
        }
    }
  *r_len = content_len;
  *r_ndef = content_ndef;
  *r_cont_oid = cont_oid;
  *r_algo_oid = algo_oid;
  *r_algo_parm = algo_parm;
  *r_algo_parmlen = algo_parmlen;
  return 0;
}



/* Parse this structure and return the oid of the content.  The read
   position is then located at the value of content.

   ContentInfo ::= SEQUENCE {
      contentType ContentType, 
      content [0] EXPLICIT ANY DEFINED BY contentType 
   }
   ContentType ::= OBJECT IDENTIFIER

   Returns: 0 on success or an error code.  On success the OID and the
   length values are stored in the cms structure.
*/
gpg_error_t
_ksba_cms_parse_content_info (ksba_cms_t cms)
{
  gpg_error_t err;
  int has_content;
  int content_ndef;
  unsigned long content_len;
  char *oid;

  err = parse_content_info (cms->reader, &content_len, &content_ndef,
                            &oid, &has_content);
  if (err)
    { /* return a more meaningful error message.  This way the caller
         can pass arbitrary data to the function and get back an error
         that this is not CMS instead of the the not very detailed BER
         Error. */
      if (gpg_err_code (err) == GPG_ERR_BAD_BER
          || gpg_err_code (err) == GPG_ERR_INV_CMS_OBJ
          || gpg_err_code (err) == GPG_ERR_TOO_SHORT)
        err = gpg_error (GPG_ERR_NO_CMS_OBJ);
      return err;
    }
  if (!has_content)
    return gpg_error (GPG_ERR_NO_CMS_OBJ); /* It is not optional here */
  cms->content.length = content_len;
  cms->content.ndef = content_ndef;
  xfree (cms->content.oid);
  cms->content.oid = oid;
  return 0;
}



/* parse a SEQUENCE and the first element which is expected to be the
   CMS version.  Return the version and the length info */
static gpg_error_t
parse_cms_version (ksba_reader_t reader, int *r_version,
                   unsigned long *r_len, int *r_ndef)
{
  struct tag_info ti;
  gpg_error_t err;
  unsigned long data_len;
  int data_ndef;
  int c;

  /* read the sequence triplet */
  err = _ksba_ber_read_tl (reader, &ti);
  if (err)
    return err;
  if ( !(ti.class == CLASS_UNIVERSAL && ti.tag == TYPE_SEQUENCE
         && ti.is_constructed) )
    return gpg_error (GPG_ERR_INV_CMS_OBJ);
  data_len = ti.length; 
  data_ndef = ti.ndef;
  if (!data_ndef && data_len < 3)
    return gpg_error (GPG_ERR_TOO_SHORT); /*to encode the version*/

  /* read the version integer */
  err = _ksba_ber_read_tl (reader, &ti);
  if (err)
    return err;
  if ( !(ti.class == CLASS_UNIVERSAL && ti.tag == TYPE_INTEGER
         && !ti.is_constructed && ti.length) )
    return gpg_error (GPG_ERR_INV_CMS_OBJ); 
  if (!data_ndef)
    {
      if (data_len < ti.nhdr)
        return gpg_error (GPG_ERR_BAD_BER); /* triplet header larger that sequence */
      data_len -= ti.nhdr;
      if (data_len < ti.length)
        return gpg_error (GPG_ERR_BAD_BER); /* triplet larger that sequence */
      data_len -= ti.length;
    }
  if (ti.length != 1)
    return gpg_error (GPG_ERR_UNSUPPORTED_CMS_VERSION); 
  if ( (c=read_byte (reader)) == -1)
    {
      err = ksba_reader_error (reader);
      return err? err : gpg_error (GPG_ERR_GENERAL);
    }
  if ( !(c == 0 || c == 1 || c == 2 || c == 3 || c == 4) )
    return gpg_error (GPG_ERR_UNSUPPORTED_CMS_VERSION);
  *r_version = c;
  *r_len = data_len;
  *r_ndef = data_ndef;
  return 0;
}




/* Parse a structure:

   SignedData ::= SEQUENCE {
     version INTEGER  { v0(0), v1(1), v2(2), v3(3), v4(4) }),
     digestAlgorithms SET OF AlgorithmIdentifier,
     encapContentInfo EncapsulatedContentInfo,
     certificates [0] IMPLICIT CertificateSet OPTIONAL,
     crls [1] IMPLICIT CertificateRevocationLists OPTIONAL,
     signerInfos SignerInfos }
   
   AlgorithmIdentifier ::= SEQUENCE {
    algorithm    OBJECT IDENTIFIER,
    parameters   ANY DEFINED BY algorithm OPTIONAL
   }

*/
gpg_error_t
_ksba_cms_parse_signed_data_part_1 (ksba_cms_t cms)
{
  struct tag_info ti;
  gpg_error_t err;
  int signed_data_ndef;
  unsigned long signed_data_len;
  int algo_set_ndef;
  unsigned long algo_set_len;
  int encap_cont_ndef;
  unsigned long encap_cont_len;
  int has_content;
  char *oid;
  char *p, *buffer;
  unsigned long off, len;

  err = parse_cms_version (cms->reader, &cms->cms_version,
                           &signed_data_len, &signed_data_ndef);
  if (err)
    return err;

  /* read the SET OF algorithmIdentifiers */
  err = _ksba_ber_read_tl (cms->reader, &ti);
  if (err)
    return err;
  if ( !(ti.class == CLASS_UNIVERSAL && ti.tag == TYPE_SET
         && ti.is_constructed) )
    return gpg_error (GPG_ERR_INV_CMS_OBJ);  /* not the expected SET tag */
  if (!signed_data_ndef)
    {
      if (signed_data_len < ti.nhdr)
        return gpg_error (GPG_ERR_BAD_BER); /* triplet header larger that sequence */
      signed_data_len -= ti.nhdr;
      if (!ti.ndef && signed_data_len < ti.length)
        return gpg_error (GPG_ERR_BAD_BER); /* triplet larger that sequence */
      signed_data_len -= ti.length;
    }
  algo_set_len = ti.length; 
  algo_set_ndef = ti.ndef;
  
  /* fixme: we are not able to read ndef length algorithm indentifiers. */
  if (algo_set_ndef)
    return gpg_error (GPG_ERR_UNSUPPORTED_ENCODING);
  /* read the entire sequence into a buffer (add one to avoid malloc(0)) */
  buffer = xtrymalloc (algo_set_len + 1);
  if (!buffer)
    return gpg_error (GPG_ERR_ENOMEM);
  if (read_buffer (cms->reader, buffer, algo_set_len))
    {
      xfree (buffer);
      err = ksba_reader_error (cms->reader);
      return err? err: gpg_error (GPG_ERR_GENERAL);
    }
  p = buffer;
  while (algo_set_len)
    {
      size_t nread;
      struct oidlist_s *ol;

      err = _ksba_parse_algorithm_identifier (p, algo_set_len, &nread, &oid);
      if (err)
        {
          xfree (buffer);
          return err;
        }
      assert (nread <= algo_set_len);
      algo_set_len -= nread;
      p += nread;
      /* store the oid */
      ol = xtrymalloc (sizeof *ol);
      if (!ol)
        {
          xfree (oid);
          return gpg_error (GPG_ERR_ENOMEM);
        }
      ol->oid = oid;
      ol->next = cms->digest_algos;
      cms->digest_algos = ol;
    }
  xfree (buffer); buffer = NULL;

  /* Now for the encapsulatedContentInfo */
  off = ksba_reader_tell (cms->reader);
  err = parse_content_info (cms->reader,
                            &encap_cont_len, &encap_cont_ndef, 
                            &oid, &has_content);
  if (err)
    return err;
  cms->inner_cont_len = encap_cont_len;
  cms->inner_cont_ndef = encap_cont_ndef;
  cms->inner_cont_oid = oid; 
  cms->detached_data = !has_content;
  if (!signed_data_ndef)
    {
      len = ksba_reader_tell (cms->reader) - off;
      if (signed_data_len < len)
        return gpg_error (GPG_ERR_BAD_BER); /* parsed content info larger that sequence */
      signed_data_len -= len;
      if (!encap_cont_ndef && signed_data_len < encap_cont_len)
        return gpg_error (GPG_ERR_BAD_BER); /* triplet larger that sequence */
    }

  /* We have to stop here so that the caller can set up the hashing etc. */
  return 0;
}

/* Continue parsing of the structure we started to parse with the
   part_1 function.  We expect to be right at the certificates tag.  */
gpg_error_t
_ksba_cms_parse_signed_data_part_2 (ksba_cms_t cms)
{
  struct tag_info ti;
  gpg_error_t err;
  struct signer_info_s *si, **si_tail;

  /* read the next triplet which is either a [0], a [1] or a SET OF
     (signerInfo) */
  err = _ksba_ber_read_tl (cms->reader, &ti);
  if (err)
    return err;
  if (ti.class == CLASS_UNIVERSAL && !ti.tag && !ti.is_constructed)
    {
      /* well, there might be still an end tag pending; eat it -
         fixme: we should keep track of this to catch invalid
         encodings */
      err = _ksba_ber_read_tl (cms->reader, &ti);
      if (err)
        return err;
    }

  if (ti.class == CLASS_CONTEXT && ti.tag == 0 && ti.is_constructed)
    {  /* Implicit SET OF certificateSet with elements of CHOICE, but
          we assume the first choice which is a Certificate; all other
          choices are obsolete.  We are now parsing a set of
          certificates which we do by utilizing the ksba_cert code. */
      ksba_cert_t cert;
      int expect_endtag;

      expect_endtag = !!ti.ndef;
      
      for (;;)
        {
          struct certlist_s *cl;

          /* First see whether this is really a sequence */
          err = _ksba_ber_read_tl (cms->reader, &ti);
          if (err)
            return err;
          if (expect_endtag && !ti.class && ti.tag == TYPE_NULL )
            {
              /* This is an end tag.  Read the next tag but don't fail
                 if this is just an EOF.  */
              err = _ksba_ber_read_tl (cms->reader, &ti);
              if (err)
                {
                  if (gpg_err_code (err) == GPG_ERR_EOF)
                    err = 0;
                  return err;
                }
              break;
            }
          if (!(ti.class == CLASS_UNIVERSAL && ti.tag == TYPE_SEQUENCE
                && ti.is_constructed))
            break; /* not a sequence, so we are ready with the set */

          /* We must unread so that the standard parser sees the sequence */
          err = ksba_reader_unread (cms->reader, ti.buf, ti.nhdr);
          if (err)
            return err;
          /* Use the standard certificate parser */
          err = ksba_cert_new (&cert);
          if (err)
            return err;
          err = ksba_cert_read_der (cert, cms->reader);
          if (err)
            {
              ksba_cert_release (cert);
              return err;
            }
          cl = xtrycalloc (1, sizeof *cl);
          if (!cl)
            {
              ksba_cert_release (cert);
              return gpg_error (GPG_ERR_ENOMEM);
            }
          cl->cert = cert;
          cl->next = cms->cert_list;
          cms->cert_list = cl;
        }
    }
      
  if (ti.class == CLASS_CONTEXT && ti.tag == 1 && ti.is_constructed)
    {  /* implicit SET OF certificateList.  We should delegate the
          parsing to a - not yet existing - ksba_crl module.  CRLs are
          quite important for other applications too so we should
          provide a nice interface */
      int expect_endtag;


      expect_endtag = !!ti.ndef;

      /* FIXME this is just dummy read code */
       /* fprintf (stderr,"WARNING: Can't handle CRLs yet\n"); */
      for (;;)
        {
          /* first see whether this is really a sequence */
          err = _ksba_ber_read_tl (cms->reader, &ti);
          if (err)
            return err;
          if (expect_endtag && !ti.class && ti.tag == TYPE_NULL )
            {
              /* This is an end tag.  Read the next tag but don't fail
                 if this is just an EOF.  */
              err = _ksba_ber_read_tl (cms->reader, &ti);
              if (err)
                {
                  if (gpg_err_code (err) == GPG_ERR_EOF)
                    err = 0;
                  return err;
                }
              break;
            }
          if ( !(ti.class == CLASS_UNIVERSAL && ti.tag == TYPE_SEQUENCE
                 && ti.is_constructed))
            break; /* not a sequence, so we are ready with the set */
          
          while (ti.length)
            {
              size_t n, nread;
              char dummy[256];

              n = ti.length > DIM(dummy) ? DIM(dummy): ti.length;
              err = ksba_reader_read (cms->reader, dummy, n, &nread);
              if (err)
                return err;
              ti.length -= nread;
            }
        }
    }

  /* expect a SET OF signerInfo */
  if ( !(ti.class == CLASS_UNIVERSAL
         && ti.tag == TYPE_SET && ti.is_constructed))
    return gpg_error (GPG_ERR_INV_CMS_OBJ); 

  si_tail = &cms->signer_info;

  while (ti.length)
    {
      size_t off1, off2;
      
      off1 = ksba_reader_tell (cms->reader);
      si = xtrycalloc (1, sizeof *si);
      if (!si)
        return gpg_error (GPG_ERR_ENOMEM);

      err = create_and_run_decoder (cms->reader, 
                                    "CryptographicMessageSyntax.SignerInfo",
                                    0,
                                    &si->root, &si->image, &si->imagelen);
      /* The signerInfo might be an empty set in the case of a certs-only
         signature.  Thus we have to allow for EOF here */
      if (gpg_err_code (err) == GPG_ERR_EOF)
        {
	  xfree (si);
          err = 0;
          break;
        }
      if (err)
	{
	  xfree (si);
	  return err;
	}

      *si_tail = si;
      si_tail = &si->next;

      off2 = ksba_reader_tell (cms->reader);
      if ( (off2 - off1) > ti.length )
        ti.length = 0;
      else
        ti.length -= off2 - off1;
    }

  return 0;
}





/* Parse the structure:

   EnvelopedData ::= SEQUENCE {
     version INTEGER  { v0(0), v1(1), v2(2), v3(3), v4(4) }),
     originatorInfo [0] IMPLICIT OriginatorInfo OPTIONAL,
     recipientInfos RecipientInfos,
     encryptedContentInfo EncryptedContentInfo,
     unprotectedAttrs [1] IMPLICIT UnprotectedAttributes OPTIONAL }

   OriginatorInfo ::= SEQUENCE {
     certs [0] IMPLICIT CertificateSet OPTIONAL,
     crls [1] IMPLICIT CertificateRevocationLists OPTIONAL }

   RecipientInfos ::= SET OF RecipientInfo

   EncryptedContentInfo ::= SEQUENCE {
     contentType ContentType,
     contentEncryptionAlgorithm ContentEncryptionAlgorithmIdentifier,
     encryptedContent [0] IMPLICIT EncryptedContent OPTIONAL }

   EncryptedContent ::= OCTET STRING

 We stop parsing so that the next read will be the first byte of the
 encryptedContent or (if there is no content) the unprotectedAttrs.
*/
gpg_error_t
_ksba_cms_parse_enveloped_data_part_1 (ksba_cms_t cms)
{
  struct tag_info ti;
  gpg_error_t err;
  int env_data_ndef;
  unsigned long env_data_len;
  int encr_cont_ndef;
  unsigned long encr_cont_len;
  int has_content;
  unsigned long off, len;
  char *cont_oid = NULL;
  char *algo_oid = NULL;
  char *algo_parm = NULL;
  size_t algo_parmlen;
  struct value_tree_s *vt, **vtend;

  /* get the version */
  err = parse_cms_version (cms->reader, &cms->cms_version,
                           &env_data_len, &env_data_ndef);
  if (err)
    return err;

  /* read the next triplet which is either a [0] for originatorInfos
     or a SET_OF (recipientInfo) */
  err = _ksba_ber_read_tl (cms->reader, &ti);
  if (err)
    return err;

  if (ti.class == CLASS_CONTEXT && ti.tag == 0 && ti.is_constructed)
    { /* originatorInfo - but we skip it for now */
      /* well, raise an error */
      return gpg_error (GPG_ERR_UNSUPPORTED_CMS_OBJ);
    }

  /* Next one is the SET OF recipientInfos */
  if ( !(ti.class == CLASS_UNIVERSAL
         && ti.tag == TYPE_SET && ti.is_constructed))
    return gpg_error (GPG_ERR_INV_CMS_OBJ); 

  vtend = &cms->recp_info;
  if (ti.ndef)
    {
      for (;;)
        {
          struct tag_info ti2;

          err = _ksba_ber_read_tl (cms->reader, &ti2);
          if (err)
            return err;

          if (!ti2.class && !ti2.tag)
            break; /* End tag found: ready.  */

          /* Not an end tag:  Push it back and run the decoder.  */
          err = ksba_reader_unread (cms->reader, ti2.buf, ti2.nhdr);
          if (err)
            return err;
          
          vt = xtrycalloc (1, sizeof *vt);
          if (!vt)
            return gpg_error_from_syserror ();
          
          err = create_and_run_decoder
            (cms->reader,
             "CryptographicMessageSyntax.KeyTransRecipientInfo",
             BER_DECODER_FLAG_FAST_STOP,
             &vt->root, &vt->image, &vt->imagelen);
          if (err)
            {
              xfree (vt);
              return err;
            }
          
          *vtend = vt;
          vtend = &vt->next;
        }
    }
  else
    {
      while (ti.length)
        {
          size_t off1, off2;
          
          off1 = ksba_reader_tell (cms->reader);
          vt = xtrycalloc (1, sizeof *vt);
          if (!vt)
            return gpg_error_from_syserror ();
          
          err = create_and_run_decoder
            (cms->reader,
             "CryptographicMessageSyntax.KeyTransRecipientInfo",
             0,
             &vt->root, &vt->image, &vt->imagelen);
          if (err)
            {
              xfree (vt);
              return err;
            }
          
          *vtend = vt;
          vtend = &vt->next;

          off2 = ksba_reader_tell (cms->reader);
          if ( (off2 - off1) > ti.length )
            ti.length = 0;
          else
            ti.length -= off2 - off1;
        }
    }

  /* Now for the encryptedContentInfo */
  off = ksba_reader_tell (cms->reader);
  err = parse_encrypted_content_info (cms->reader,
                                      &encr_cont_len, &encr_cont_ndef, 
                                      &cont_oid,
                                      &algo_oid,
                                      &algo_parm, &algo_parmlen,
                                      &has_content);
  if (err)
    return err;
  cms->inner_cont_len = encr_cont_len;
  cms->inner_cont_ndef = encr_cont_ndef;
  cms->inner_cont_oid = cont_oid; 
  cms->detached_data = !has_content;
  cms->encr_algo_oid = algo_oid; 
  cms->encr_iv = algo_parm; algo_parm = NULL;
  cms->encr_ivlen = algo_parmlen;
  if (!env_data_ndef)
    {
      len = ksba_reader_tell (cms->reader) - off;
      if (env_data_len < len)
        return gpg_error (GPG_ERR_BAD_BER); /* parsed content info larger that sequence */
      env_data_len -= len;
      if (!encr_cont_ndef && env_data_len < encr_cont_len)
        return gpg_error (GPG_ERR_BAD_BER); /* triplet larger that sequence */
    }

  return 0;
}


/* handle the unprotected attributes */
gpg_error_t
_ksba_cms_parse_enveloped_data_part_2 (ksba_cms_t cms)
{
  (void)cms;
  /* FIXME */
  return 0;
}
