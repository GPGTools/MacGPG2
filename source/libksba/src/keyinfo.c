/* keyinfo.c - Parse and build a keyInfo structure
 *      Copyright (C) 2001, 2002, 2007, 2008 g10 Code GmbH
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

/* Instead of using the ASN parser - which is easily possible - we use
   a simple handcoded one to speed up the operation and to make it
   more robust. */

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "util.h"
#include "asn1-func.h"
#include "keyinfo.h"
#include "shared.h"
#include "convert.h"
#include "ber-help.h"


/* Constants used for the public key algorithms.  */
typedef enum 
  {
    PKALGO_RSA,
    PKALGO_DSA,
    PKALGO_ECC
  }
pkalgo_t;


struct algo_table_s {
  const char *oidstring;
  const unsigned char *oid;  /* NULL indicattes end of table */
  int                  oidlen;
  int supported;
  pkalgo_t pkalgo;
  const char *algo_string;
  const char *elem_string; /* parameter name or '-' */
  const char *ctrl_string; /* expected tag values (value > 127 are raw data)*/
  const char *parmelem_string; /* parameter name or '-'. */
  const char *parmctrl_string; /* expected tag values.  */
  const char *digest_string; /* The digest algo if included in the OID. */
};



static const struct algo_table_s pk_algo_table[] = {

  { /* iso.member-body.us.rsadsi.pkcs.pkcs-1.1 */
    "1.2.840.113549.1.1.1", /* rsaEncryption (RSAES-PKCA1-v1.5) */ 
    "\x2a\x86\x48\x86\xf7\x0d\x01\x01\x01", 9, 
    1, PKALGO_RSA, "rsa", "-ne", "\x30\x02\x02" },

  { /* iso.member-body.us.rsadsi.pkcs.pkcs-1.7 */
    "1.2.840.113549.1.1.7", /* RSAES-OAEP */ 
    "\x2a\x86\x48\x86\xf7\x0d\x01\x01\x07", 9, 
    0, PKALGO_RSA, "rsa", "-ne", "\x30\x02\x02"}, /* (patent problems) */

  { /* */
    "2.5.8.1.1", /* rsa (ambiguous due to missing padding rules)*/
    "\x55\x08\x01\x01", 4, 
    1, PKALGO_RSA, "ambiguous-rsa", "-ne", "\x30\x02\x02" },

  { /* iso.member-body.us.x9-57.x9cm.1 */
    "1.2.840.10040.4.1", /*  dsa */
    "\x2a\x86\x48\xce\x38\x04\x01", 7, 
    1, PKALGO_DSA, "dsa", "y", "\x02", "-pqg", "\x30\x02\x02\x02" }, 
 
  { /* iso.member-body.us.ansi-x9-62.2.1 */
    "1.2.840.10045.2.1", /*  ecPublicKey */
    "\x2a\x86\x48\xce\x3d\x02\x01", 7, 
    1, PKALGO_ECC, "ecc", "q", "\x80" }, 

  {NULL}
};


static const struct algo_table_s sig_algo_table[] = {
  {  /* iso.member-body.us.rsadsi.pkcs.pkcs-1.5 */
    "1.2.840.113549.1.1.5", /* sha1WithRSAEncryption */ 
    "\x2A\x86\x48\x86\xF7\x0D\x01\x01\x05", 9, 
    1, PKALGO_RSA, "rsa", "s", "\x82", NULL, NULL, "sha1" },
  { /* iso.member-body.us.rsadsi.pkcs.pkcs-1.4 */
    "1.2.840.113549.1.1.4", /* md5WithRSAEncryption */ 
    "\x2A\x86\x48\x86\xF7\x0D\x01\x01\x04", 9, 
    1, PKALGO_RSA, "rsa", "s", "\x82", NULL, NULL, "md5" },
  { /* iso.member-body.us.rsadsi.pkcs.pkcs-1.2 */
    "1.2.840.113549.1.1.2", /* md2WithRSAEncryption */ 
    "\x2A\x86\x48\x86\xF7\x0D\x01\x01\x02", 9, 
    0, PKALGO_RSA, "rsa", "s", "\x82", NULL, NULL, "md2" },
  { /* iso.member-body.us.x9-57.x9cm.1 */
    "1.2.840.10040.4.3", /* dsa */
    "\x2a\x86\x48\xce\x38\x04\x01", 7, 
    1, PKALGO_DSA, "dsa", "-rs", "\x30\x02\x02" }, 
  { /* iso.member-body.us.x9-57.x9cm.3 */
    "1.2.840.10040.4.3", /*  dsaWithSha1 */
    "\x2a\x86\x48\xce\x38\x04\x03", 7, 
    1, PKALGO_DSA, "dsa", "-rs", "\x30\x02\x02", NULL, NULL, "sha1" }, 
  { /* Teletrust signature algorithm.  */
    "1.3.36.8.5.1.2.2", /* dsaWithRIPEMD160 */
    "\x2b\x24\x08\x05\x01\x02\x02", 7,
    1, PKALGO_DSA, "dsa", "-rs", "\x30\x02\x02", NULL, NULL, "rmd160" }, 
  { /* NIST Algorithm */
    "2.16.840.1.101.3.4.3.1", /* dsaWithSha224 */
    "\x06\x09\x60\x86\x48\x01\x65\x03\x04\x03\x01", 11,
    1, PKALGO_DSA, "dsa", "-rs", "\x30\x02\x02", NULL, NULL, "sha224" }, 
  { /* NIST Algorithm (the draft also used .1 but we better use .2) */
    "2.16.840.1.101.3.4.3.2", /* dsaWithSha256 */
    "\x06\x09\x60\x86\x48\x01\x65\x03\x04\x03\x01", 11,
    1, PKALGO_DSA, "dsa", "-rs", "\x30\x02\x02", NULL, NULL, "sha256" }, 

  { /* iso.member-body.us.ansi-x9-62.signatures.ecdsa-with-sha1 */
    "1.2.840.10045.4.1", /*  ecdsa */
    "\x2a\x86\x48\xce\x3d\x04\x01", 7, 
    1, PKALGO_ECC, "ecdsa", "-rs", "\x30\x02\x02", NULL, NULL, "sha1" }, 

  { /* iso.member-body.us.ansi-x9-62.signatures.ecdsa-with-specified */
    "1.2.840.10045.4.3", 
    "\x2a\x86\x48\xce\x3d\x04\x03", 7, 
    1, PKALGO_ECC, "ecdsa", "-rs", "\x30\x02\x02", NULL, NULL, NULL }, 
  /* The digest algorithm is given by the parameter.  */ 


  { /* iso.member-body.us.ansi-x9-62.signatures.ecdsa-with-sha224 */
    "1.2.840.10045.4.3.1", 
    "\x2a\x86\x48\xce\x3d\x04\x03\x01", 8, 
    1, PKALGO_ECC, "ecdsa", "-rs", "\x30\x02\x02", NULL, NULL, "sha224" }, 

  { /* iso.member-body.us.ansi-x9-62.signatures.ecdsa-with-sha256 */
    "1.2.840.10045.4.3.2", 
    "\x2a\x86\x48\xce\x3d\x04\x03\x02", 8, 
    1, PKALGO_ECC, "ecdsa", "-rs", "\x30\x02\x02", NULL, NULL, "sha256" }, 

  { /* iso.member-body.us.ansi-x9-62.signatures.ecdsa-with-sha384 */
    "1.2.840.10045.4.3.3", 
    "\x2a\x86\x48\xce\x3d\x04\x03\x03", 8, 
    1, PKALGO_ECC, "ecdsa", "-rs", "\x30\x02\x02", NULL, NULL, "sha384" }, 

  { /* iso.member-body.us.ansi-x9-62.signatures.ecdsa-with-sha512 */
    "1.2.840.10045.4.3.4", 
    "\x2a\x86\x48\xce\x3d\x04\x03\x04", 8, 
    1, PKALGO_ECC, "ecdsa", "-rs", "\x30\x02\x02", NULL, NULL, "sha512" }, 

  { /* iso.member-body.us.rsadsi.pkcs.pkcs-1.1 */
    "1.2.840.113549.1.1.1", /* rsaEncryption used without hash algo*/ 
    "\x2a\x86\x48\x86\xf7\x0d\x01\x01\x01", 9, 
    1, PKALGO_RSA, "rsa", "s", "\x82" },
  { /* from NIST's OIW - actually belongs in a pure hash table */
    "1.3.14.3.2.26",  /* sha1 */
    "\x2B\x0E\x03\x02\x1A", 5,
    0, PKALGO_RSA, "sha-1", "", "", NULL, NULL, "sha1" },

  { /* As used by telesec cards */
    "1.3.36.3.3.1.2",  /* rsaSignatureWithripemd160 */
    "\x2b\x24\x03\x03\x01\x02", 6,
    1, PKALGO_RSA, "rsa", "s", "\x82", NULL, NULL, "rmd160" },

  { /* from NIST's OIW - used by TU Darmstadt */
    "1.3.14.3.2.29",  /* sha-1WithRSAEncryption */
    "\x2B\x0E\x03\x02\x1D", 5,
    1, PKALGO_RSA, "rsa", "s", "\x82", NULL, NULL, "sha1" },

  { /* from PKCS#1  */
    "1.2.840.113549.1.1.11", /* sha256WithRSAEncryption */
    "\x2a\x86\x48\x86\xf7\x0d\x01\x01\x0b", 9,
    1, PKALGO_RSA, "rsa", "s", "\x82", NULL, NULL, "sha256" },

  { /* from PKCS#1  */
    "1.2.840.113549.1.1.12", /* sha384WithRSAEncryption */
    "\x2a\x86\x48\x86\xf7\x0d\x01\x01\x0c", 9,
    1, PKALGO_RSA, "rsa", "s", "\x82", NULL, NULL, "sha384" },

  { /* from PKCS#1  */
    "1.2.840.113549.1.1.13", /* sha512WithRSAEncryption */
    "\x2a\x86\x48\x86\xf7\x0d\x01\x01\x0d", 9,
    1, PKALGO_RSA, "rsa", "s", "\x82", NULL, NULL, "sha512" },

  { /* TeleTrust signature scheme with RSA signature and DSI according
       to ISO/IEC 9796-2 with random number and RIPEMD-160.  I am not
       sure for what this is good; thus disabled. */
    "1.3.36.3.4.3.2.2",     /* sigS_ISO9796-2rndWithrsa_ripemd160 */
    "\x2B\x24\x03\x04\x03\x02\x02", 7,
    0, PKALGO_RSA, "rsa", "s", "\x82", NULL, NULL, "rmd160" },
  
  {NULL}
};

static const struct algo_table_s enc_algo_table[] = {
  { /* iso.member-body.us.rsadsi.pkcs.pkcs-1.1 */
    "1.2.840.113549.1.1.1", /* rsaEncryption (RSAES-PKCA1-v1.5) */ 
    "\x2A\x86\x48\x86\xF7\x0D\x01\x01\x01", 9, 
    1, PKALGO_RSA, "rsa", "a", "\x82" },
  {NULL}
};


/* This tables maps names of ECC curves names to OIDs.  A similar
   table is used by lib gcrypt.  */
static const struct
{
  const char *oid; 
  const char *name; 
} curve_names[] = 
  {
    { "1.2.840.10045.3.1.1", "NIST P-192" },
    { "1.2.840.10045.3.1.1", "prime192v1" }, 
    { "1.2.840.10045.3.1.1", "secp192r1"  },

    { "1.3.132.0.33",        "secp224r1" },

    { "1.2.840.10045.3.1.7", "NIST P-256", }, 
    { "1.2.840.10045.3.1.7", "prime256v1" },          
    { "1.2.840.10045.3.1.7", "secp256r1"  },          

    { "1.3.132.0.34",        "secp384r1" },

    { "1.3.132.0.35",        "secp521r1" },

    { "1.3.36.3.3.2.8.1.1.1" , "brainpoolP160r1" },
    { "1.3.36.3.3.2.8.1.1.3" , "brainpoolP192r1" },
    { "1.3.36.3.3.2.8.1.1.5" , "brainpoolP224r1" },
    { "1.3.36.3.3.2.8.1.1.7" , "brainpoolP256r1" },
    { "1.3.36.3.3.2.8.1.1.9" , "brainpoolP320r1" },
    { "1.3.36.3.3.2.8.1.1.11", "brainpoolP384r1" },
    { "1.3.36.3.3.2.8.1.1.13", "brainpoolP512r1" },

    { NULL, NULL}
  };





struct stringbuf {
  size_t len;
  size_t size;
  char *buf;
  int out_of_core;
};


#define TLV_LENGTH(prefix) do {         \
  if (!prefix ## len)                    \
    return gpg_error (GPG_ERR_INV_KEYINFO);  \
  c = *(prefix)++; prefix ## len--;           \
  if (c == 0x80)                  \
    return gpg_error (GPG_ERR_NOT_DER_ENCODED);  \
  if (c == 0xff)                  \
    return gpg_error (GPG_ERR_BAD_BER);        \
                                  \
  if ( !(c & 0x80) )              \
    len = c;                      \
  else                            \
    {                             \
      int count = c & 0x7f;       \
                                  \
      for (len=0; count; count--) \
        {                         \
          len <<= 8;              \
          if (!prefix ## len)            \
            return gpg_error (GPG_ERR_BAD_BER);\
          c = *(prefix)++; prefix ## len--;   \
          len |= c & 0xff;        \
        }                         \
    }                             \
  if (len > prefix ## len)               \
    return gpg_error (GPG_ERR_INV_KEYINFO);  \
} while (0)


/* Given a string BUF of length BUFLEN with either the name of an ECC
   curve or its OID in dotted form return the DER encoding of the OID.
   The caller must free the result.  On error NULL is returned.  */
static unsigned char *
get_ecc_curve_oid (const unsigned char *buf, size_t buflen, size_t *r_oidlen)
{
  unsigned char *der_oid;

  /* Skip an optional "oid." prefix. */
  if (buflen > 4 && buf[3] == '.' && digitp (buf+4)
      && ((buf[0] == 'o' && buf[1] == 'i' && buf[2] == 'd')
          ||(buf[0] == 'O' && buf[1] == 'I' && buf[2] == 'D')))
    {
      buf += 4;
      buflen -= 4;
    }

  /* If it does not look like an OID - map it through the table.  */
  if (buflen && !digitp (buf))
    {
      int i;

      for (i=0; curve_names[i].oid; i++)
        if (buflen == strlen (curve_names[i].name)
            && !memcmp (buf, curve_names[i].name, buflen))
          break;
      if (curve_names[i].oid)
        return NULL; /* Not found.  */
      buf = curve_names[i].name;
      buflen = strlen (curve_names[i].name);
    }

  if (_ksba_oid_from_buf (buf, buflen, &der_oid, r_oidlen))
    return NULL;
  return der_oid;
}



/* Return the OFF and the LEN of algorithm within DER.  Do some checks
   and return the number of bytes read in r_nread, adding this to der
   does point into the BIT STRING.

   mode 0: just get the algorithm identifier. FIXME: should be able to
           handle BER Encoding. 
   mode 1: as described.
 */
static gpg_error_t
get_algorithm (int mode, const unsigned char *der, size_t derlen,
               size_t *r_nread, size_t *r_pos, size_t *r_len, int *r_bitstr,
               size_t *r_parm_pos, size_t *r_parm_len, int *r_parm_type)
{
  int c;
  const unsigned char *start = der;
  const unsigned char *startseq;
  unsigned long seqlen, len;

  *r_bitstr = 0;
  if (r_parm_pos)
    *r_parm_pos = 0;
  if (r_parm_len)
    *r_parm_len = 0;
  if (r_parm_type)
    *r_parm_type = 0;
  /* get the inner sequence */
  if (!derlen)
    return gpg_error (GPG_ERR_INV_KEYINFO);
  c = *der++; derlen--;
  if ( c != 0x30 )
    return gpg_error (GPG_ERR_UNEXPECTED_TAG); /* not a SEQUENCE */
  TLV_LENGTH(der); 
  seqlen = len;
  startseq = der;

  /* get the object identifier */
  if (!derlen)
    return gpg_error (GPG_ERR_INV_KEYINFO);
  c = *der++; derlen--; 
  if ( c != 0x06 )
    return gpg_error (GPG_ERR_UNEXPECTED_TAG); /* not an OBJECT IDENTIFIER */
  TLV_LENGTH(der);

  /* der does now point to an oid of length LEN */
  *r_pos = der - start;
  *r_len = len;
/*    { */
/*      char *p = ksba_oid_to_str (der, len); */
/*      printf ("algorithm: %s\n", p); */
/*      xfree (p); */
/*    } */
  der += len;
  derlen -= len;
  seqlen -= der - startseq;;

  /* Parse the parameter.  */
  if (seqlen)
    {
      const unsigned char *startparm = der;

      if (!derlen)
        return gpg_error (GPG_ERR_INV_KEYINFO);
      c = *der++; derlen--;
      if ( c == 0x05 ) 
        {
          /*printf ("parameter: NULL \n"); the usual case */
          if (!derlen)
            return gpg_error (GPG_ERR_INV_KEYINFO);
          c = *der++; derlen--;
          if (c) 
            return gpg_error (GPG_ERR_BAD_BER);  /* NULL must have a
                                                    length of 0 */
          seqlen -= 2;
        }
      else if (r_parm_pos && r_parm_len && c == 0x04)
        { 
          /*  This is an octet string parameter and we need it.  */
          if (r_parm_type)
            *r_parm_type = TYPE_OCTET_STRING;
          TLV_LENGTH(der);
          *r_parm_pos = der - start;
          *r_parm_len = len;
          seqlen -= der - startparm;
          der += len;
          derlen -= len;
          seqlen -= len;
        }
      else if (r_parm_pos && r_parm_len && c == 0x06)
        { 
          /*  This is an object identifier.  */
          if (r_parm_type)
            *r_parm_type = TYPE_OBJECT_ID;
          TLV_LENGTH(der);
          *r_parm_pos = der - start;
          *r_parm_len = len;
          seqlen -= der - startparm;
          der += len;
          derlen -= len;
          seqlen -= len;
        }
      else if (r_parm_pos && r_parm_len && c == 0x30)
        { 
          /*  This is a sequence. */
          if (r_parm_type)
            *r_parm_type = TYPE_SEQUENCE;
          TLV_LENGTH(der);
          *r_parm_pos = startparm - start;
          *r_parm_len = len + (der - startparm);
          seqlen -= der - startparm;
          der += len;
          derlen -= len;
          seqlen -= len;
        }
      else
        {
/*            printf ("parameter: with tag %02x - ignored\n", c); */
          TLV_LENGTH(der);
          seqlen -= der - startparm;
          /* skip the value */
          der += len;
          derlen -= len;
          seqlen -= len;
        }
    }

  if (seqlen)
    return gpg_error (GPG_ERR_INV_KEYINFO);

  if (mode)
    {
      /* move forward to the BIT_STR */
      if (!derlen)
        return gpg_error (GPG_ERR_INV_KEYINFO);
      c = *der++; derlen--;
      
      if (c == 0x03)
        *r_bitstr = 1; /* BIT STRING */
      else if (c == 0x04)
        ; /* OCTECT STRING */
      else
        return gpg_error (GPG_ERR_UNEXPECTED_TAG); /* not a BIT STRING */
      TLV_LENGTH(der);
    }

  *r_nread = der - start;
  return 0;
}


gpg_error_t
_ksba_parse_algorithm_identifier (const unsigned char *der, size_t derlen,
                                  size_t *r_nread, char **r_oid)
{
  return _ksba_parse_algorithm_identifier2 (der, derlen,
                                            r_nread, r_oid, NULL, NULL);
}

gpg_error_t
_ksba_parse_algorithm_identifier2 (const unsigned char *der, size_t derlen,
                                   size_t *r_nread, char **r_oid,
                                   char **r_parm, size_t *r_parmlen)
{
  gpg_error_t err;
  int is_bitstr;
  size_t nread, off, len, off2, len2;
  int parm_type;

  /* fixme: get_algorithm might return the error invalid keyinfo -
     this should be invalid algorithm identifier */
  *r_oid = NULL;
  *r_nread = 0;
  off2 = len2 = 0;
  err = get_algorithm (0, der, derlen, &nread, &off, &len, &is_bitstr,
                       &off2, &len2, &parm_type);
  if (err)
    return err;
  *r_nread = nread;
  *r_oid = ksba_oid_to_str (der+off, len);
  if (!*r_oid)
    return gpg_error (GPG_ERR_ENOMEM);
  
  /* Special hack for ecdsaWithSpecified.  We replace the returned OID
     by the one in the parameter. */
  if (off2 && len2 && parm_type == TYPE_SEQUENCE
      && !strcmp (*r_oid, "1.2.840.10045.4.3"))
    {
      xfree (*r_oid);
      *r_oid = NULL;
      err = get_algorithm (0, der+off2, len2, &nread, &off, &len, &is_bitstr,
                           NULL, NULL, NULL);
      if (err)
        {
          *r_nread = 0;
          return err;
        }
      *r_oid = ksba_oid_to_str (der+off2+off, len);
      if (!*r_oid)
        {
          *r_nread = 0;
          return gpg_error (GPG_ERR_ENOMEM);
        }

      off2 = len2 = 0; /* So that R_PARM is set to NULL.  */
    }

  if (r_parm && r_parmlen)
    {
      if (off2 && len2)
        {
          *r_parm = xtrymalloc (len2);
          if (!*r_parm)
            {
              xfree (*r_oid); 
              *r_oid = NULL;
              return gpg_error (GPG_ERR_ENOMEM);
            }
          memcpy (*r_parm, der+off2, len2);
          *r_parmlen = len2;
        }
      else
        {
          *r_parm = NULL;
          *r_parmlen = 0;
        }
    }
  return 0;
}



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
put_stringbuf (struct stringbuf *sb, const char *text)
{
  put_stringbuf_mem (sb, text,strlen (text));
}

static void
put_stringbuf_mem_sexp (struct stringbuf *sb, const char *text, size_t length)
{
  char buf[20];
  sprintf (buf,"%u:", (unsigned int)length);
  put_stringbuf (sb, buf);
  put_stringbuf_mem (sb, text, length);
}

static void
put_stringbuf_sexp (struct stringbuf *sb, const char *text)
{
  put_stringbuf_mem_sexp (sb, text, strlen (text));
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


/* Assume that der is a buffer of length DERLEN with a DER encoded
 Asn.1 structure like this:
 
  keyInfo ::= SEQUENCE {
                 SEQUENCE { 
                    algorithm    OBJECT IDENTIFIER,
                    parameters   ANY DEFINED BY algorithm OPTIONAL }
                 publicKey  BIT STRING }
  
  The function parses this structure and create a SEXP suitable to be
  used as a public key in Libgcrypt.  The S-Exp will be returned in a
  string which the caller must free.  
  
  We don't pass an ASN.1 node here but a plain memory block.  */

gpg_error_t
_ksba_keyinfo_to_sexp (const unsigned char *der, size_t derlen,
                       ksba_sexp_t *r_string)
{
  gpg_error_t err;
  int c;
  size_t nread, off, len, parm_off, parm_len;
  int parm_type;
  char *parm_oid = NULL;
  int algoidx;
  int is_bitstr;
  const unsigned char *parmder = NULL;
  size_t parmderlen = 0;
  const unsigned char *ctrl;
  const char *elem;
  struct stringbuf sb;

  *r_string = NULL;

  /* check the outer sequence */
  if (!derlen)
    return gpg_error (GPG_ERR_INV_KEYINFO);
  c = *der++; derlen--;
  if ( c != 0x30 )
    return gpg_error (GPG_ERR_UNEXPECTED_TAG); /* not a SEQUENCE */
  TLV_LENGTH(der);
  /* and now the inner part */
  err = get_algorithm (1, der, derlen, &nread, &off, &len, &is_bitstr,
                       &parm_off, &parm_len, &parm_type);
  if (err)
    return err;
  
  /* look into our table of supported algorithms */
  for (algoidx=0; pk_algo_table[algoidx].oid; algoidx++)
    {
      if ( len == pk_algo_table[algoidx].oidlen
           && !memcmp (der+off, pk_algo_table[algoidx].oid, len))
        break;
    }
  if (!pk_algo_table[algoidx].oid)
    return gpg_error (GPG_ERR_UNKNOWN_ALGORITHM);
  if (!pk_algo_table[algoidx].supported)
    return gpg_error (GPG_ERR_UNSUPPORTED_ALGORITHM);

  if (parm_off && parm_len && parm_type == TYPE_OBJECT_ID)
    parm_oid = ksba_oid_to_str (der+parm_off, parm_len);
  else if (parm_off && parm_len)
    {
      parmder = der + parm_off;
      parmderlen = parm_len;
    }

  der += nread;
  derlen -= nread;

  if (is_bitstr)
    { /* Funny: X.509 defines the signature value as a bit string but
         CMS as an octet string - for ease of implementation we always
         allow both */
      if (!derlen)
        {
          xfree (parm_oid);
          return gpg_error (GPG_ERR_INV_KEYINFO);
        }
      c = *der++; derlen--;
      if (c) 
        fprintf (stderr, "warning: number of unused bits is not zero\n");
    }

  /* fixme: we should calculate the initial length form the size of the
     sequence, so that we don't need a realloc later */
  init_stringbuf (&sb, 100);
  put_stringbuf (&sb, "(10:public-key(");

  /* fixme: we can also use the oidstring here and prefix it with
     "oid." - this way we can pass more information into Libgcrypt or
     whatever library is used */
  put_stringbuf_sexp (&sb, pk_algo_table[algoidx].algo_string);

  /* Insert the curve name for ECC. */
  if (pk_algo_table[algoidx].pkalgo == PKALGO_ECC && parm_oid)
    {
      put_stringbuf (&sb, "(");
      put_stringbuf_sexp (&sb, "curve");
      put_stringbuf_sexp (&sb, parm_oid);
      put_stringbuf (&sb, ")");
    }

  /* If parameters are given and we have a description for them, parse
     them. */
  if (parmder && parmderlen 
      && pk_algo_table[algoidx].parmelem_string
      && pk_algo_table[algoidx].parmctrl_string)
    {
      elem = pk_algo_table[algoidx].parmelem_string; 
      ctrl = pk_algo_table[algoidx].parmctrl_string; 
      for (; *elem; ctrl++, elem++)
        {
          int is_int;
          
          if ( (*ctrl & 0x80) && !elem[1] )
            {
              /* Hack to allow reading a raw value.  */
              is_int = 1;
              len = parmderlen;
            }
          else
            {
              if (!parmderlen)
                {
                  xfree (parm_oid);
                  return gpg_error (GPG_ERR_INV_KEYINFO);
                }
              c = *parmder++; parmderlen--;
              if ( c != *ctrl )
                {
                  xfree (parm_oid);
                  return gpg_error (GPG_ERR_UNEXPECTED_TAG);
                }
              is_int = c == 0x02;
              TLV_LENGTH (parmder);
            }
          if (is_int && *elem != '-')  /* Take this integer.  */
            { 
              char tmp[2];
              
              put_stringbuf (&sb, "(");
              tmp[0] = *elem; tmp[1] = 0;
              put_stringbuf_sexp (&sb, tmp);
              put_stringbuf_mem_sexp (&sb, parmder, len);
              parmder += len;
              parmderlen -= len;
              put_stringbuf (&sb, ")");
            }
        }
    }


  /* FIXME: We don't release the stringbuf in case of error
     better let the macro jump to a label */
  elem = pk_algo_table[algoidx].elem_string; 
  ctrl = pk_algo_table[algoidx].ctrl_string; 
  for (; *elem; ctrl++, elem++)
    {
      int is_int;

      if ( (*ctrl & 0x80) && !elem[1] )
        {
          /* Hack to allow reading a raw value.  */
          is_int = 1;
          len = derlen;
        }
      else
        {
          if (!derlen)
            {
              xfree (parm_oid);
              return gpg_error (GPG_ERR_INV_KEYINFO);
            }
          c = *der++; derlen--;
          if ( c != *ctrl )
            {
              xfree (parm_oid);
              return gpg_error (GPG_ERR_UNEXPECTED_TAG);
            }
          is_int = c == 0x02;
          TLV_LENGTH (der);
        }
      if (is_int && *elem != '-')  /* Take this integer.  */
        { 
          char tmp[2];

          put_stringbuf (&sb, "(");
          tmp[0] = *elem; tmp[1] = 0;
          put_stringbuf_sexp (&sb, tmp);
          put_stringbuf_mem_sexp (&sb, der, len);
          der += len;
          derlen -= len;
          put_stringbuf (&sb, ")");
        }
    }
  put_stringbuf (&sb, "))");
  xfree (parm_oid);

  *r_string = get_stringbuf (&sb);
  if (!*r_string)
    return gpg_error (GPG_ERR_ENOMEM);

  return 0;
}


/* Match the algorithm string given in BUF which is of length BUFLEN
   with the known algorithms from our table and returns the table
   entries for the DER encoded OID.  */
static const unsigned char *
oid_from_buffer (const unsigned char *buf, int buflen, int *oidlen,
                 pkalgo_t *r_pkalgo)
{
  int i;

  /* Ignore an optional "oid." prefix. */
  if (buflen > 4 && buf[3] == '.' && digitp (buf+4)
      && ((buf[0] == 'o' && buf[1] == 'i' && buf[2] == 'd')
          ||(buf[0] == 'O' && buf[1] == 'I' && buf[2] == 'D')))
    {
      buf += 4;
      buflen -= 4;
    }

  /* Scan the table. */
  for (i=0; pk_algo_table[i].oid; i++)
    {
      if (!pk_algo_table[i].supported)
        continue;
      if (buflen == strlen (pk_algo_table[i].oidstring)
          && !memcmp (buf, pk_algo_table[i].oidstring, buflen))
        break;
      if (buflen == strlen (pk_algo_table[i].algo_string)
          && !memcmp (buf, pk_algo_table[i].algo_string, buflen))
        break;
    }
  if (!pk_algo_table[i].oid)
    return NULL;

  *r_pkalgo = pk_algo_table[i].pkalgo;
  *oidlen = pk_algo_table[i].oidlen;
  return pk_algo_table[i].oid;
}


/* Take a public-key S-Exp and convert it into a DER encoded
   publicKeyInfo */
gpg_error_t
_ksba_keyinfo_from_sexp (ksba_const_sexp_t sexp,
                         unsigned char **r_der, size_t *r_derlen)
{
  gpg_error_t err;
  const unsigned char *s;
  char *endp;
  unsigned long n, n1;
  const unsigned char *oid;
  int oidlen;
  unsigned char *curve_oid = NULL;
  size_t curve_oidlen;
  pkalgo_t pkalgo;
  int i;
  struct {
    const char *name;
    int namelen;
    const unsigned char *value;
    int valuelen;
  } parm[10]; 
  int parmidx;
  int idxtbl[10];
  int idxtbllen;
  const char *parmdesc, *algoparmdesc;
  ksba_writer_t writer = NULL;
  void *algoparmseq_value = NULL;
  size_t algoparmseq_len;
  void *bitstr_value = NULL;
  size_t bitstr_len;

  if (!sexp)
    return gpg_error (GPG_ERR_INV_VALUE);

  s = sexp;
  if (*s != '(')
    return gpg_error (GPG_ERR_INV_SEXP);
  s++;

  n = strtoul (s, &endp, 10);
  s = endp;
  if (!n || *s != ':')
    return gpg_error (GPG_ERR_INV_SEXP); /* we don't allow empty lengths */
  s++;
  if (n != 10 || memcmp (s, "public-key", 10))
    return gpg_error (GPG_ERR_UNKNOWN_SEXP);
  s += 10;
  if (*s != '(')
    return gpg_error (digitp (s)? GPG_ERR_UNKNOWN_SEXP : GPG_ERR_INV_SEXP);
  s++;

  /* Break out the algorithm ID */
  n = strtoul (s, &endp, 10);
  s = endp;
  if (!n || *s != ':')
    return gpg_error (GPG_ERR_INV_SEXP); /* we don't allow empty lengths */
  s++;
  oid = oid_from_buffer (s, n, &oidlen, &pkalgo);
  if (!oid)
    return gpg_error (GPG_ERR_UNSUPPORTED_ALGORITHM);
  s += n;

  /* Collect all the values */
  for (parmidx = 0; *s != ')' ; parmidx++)
    {
      if (parmidx >= DIM(parm))
        return gpg_error (GPG_ERR_GENERAL);
      if (*s != '(')
        return gpg_error (digitp(s)? GPG_ERR_UNKNOWN_SEXP:GPG_ERR_INV_SEXP);
      s++;
      n = strtoul (s, &endp, 10);
      s = endp;
      if (!n || *s != ':')
        return gpg_error (GPG_ERR_INV_SEXP); 
      s++;
      parm[parmidx].name = s;
      parm[parmidx].namelen = n;
      s += n; 
      if (!digitp(s))
        return gpg_error (GPG_ERR_UNKNOWN_SEXP); /* ... or invalid S-Exp. */

      n = strtoul (s, &endp, 10);
      s = endp;
      if (!n || *s != ':')
        return gpg_error (GPG_ERR_INV_SEXP); 
      s++;
      parm[parmidx].value = s;
      parm[parmidx].valuelen = n;
      s += n;
      if ( *s != ')')
        return gpg_error (GPG_ERR_UNKNOWN_SEXP); /* ... or invalid S-Exp. */
      s++;
    }
  s++;
  /* We need another closing parenthesis. */
  if ( *s != ')' )
    return gpg_error (GPG_ERR_INV_SEXP); 

  /* Describe the parameters in the order we want them and construct
     IDXTBL to access them.  For DSA wie also set algoparmdesc so
     that we can later build the parameters for the
     algorithmIdentifier.  */
  algoparmdesc = NULL;
  switch (pkalgo)
    {
    case PKALGO_RSA: parmdesc = "ne"; break;
    case PKALGO_DSA: parmdesc = "y" ; algoparmdesc = "pqg"; break;
    case PKALGO_ECC: parmdesc = "Cq"; break;
    default: return gpg_error (GPG_ERR_UNKNOWN_ALGORITHM);
    }      

  idxtbllen = 0;
  for (s = parmdesc; *s; s++)
    {
      for (i=0; i < parmidx; i++)
        {
          assert (idxtbllen < DIM (idxtbl));
          switch (*s)
            {
            case 'C': /* Magic value for "curve".  */
              if (parm[i].namelen == 5 && !memcmp (parm[i].name, "curve", 5))
                {
                  idxtbl[idxtbllen++] = i;
                  i = parmidx; /* Break inner loop.  */
                }
              break;
            default:
              if (parm[i].namelen == 1 && parm[i].name[0] == *s)
                {
                  idxtbl[idxtbllen++] = i;
                  i = parmidx; /* Break inner loop.  */
                }
              break;
            }
        }
    }
  if (idxtbllen != strlen (parmdesc))
    return gpg_error (GPG_ERR_UNKNOWN_SEXP);

  if (pkalgo == PKALGO_ECC)
    {
      curve_oid = get_ecc_curve_oid (parm[idxtbl[0]].value,
                                     parm[idxtbl[0]].valuelen,
                                     &curve_oidlen);
      if (!curve_oid)
        return gpg_error (GPG_ERR_UNKNOWN_SEXP);
    }

      
  /* Create write object. */
  err = ksba_writer_new (&writer);
  if (err)
    goto leave;
  err = ksba_writer_set_mem (writer, 1024);
  if (err)
    goto leave;
  
  /* We create the keyinfo in 2 steps:
     
     1. We build the inner one and encapsulate it in a bit string.
  
     2. We create the outer sequence include the algorithm identifier
        and the bit string from step 1. 
   */
  if (pkalgo == PKALGO_ECC)
    {
      /* Write the bit string header and the number of unused bits. */
      err = _ksba_ber_write_tl (writer, TYPE_BIT_STRING, CLASS_UNIVERSAL, 
                                0, parm[idxtbl[1]].valuelen + 1);
      if (!err)
        err = ksba_writer_write (writer, "", 1);
      /* And the actual raw value.  */
      if (!err)
        err = ksba_writer_write (writer, parm[idxtbl[1]].value,
                                 parm[idxtbl[1]].valuelen);
      if (err)
        goto leave;

    }
  else /* RSA and DSA */
    {
      /* Calculate the size of the sequence value and the size of the
         bit string value.  NOt ethat in case there is only one
         integer to write, no sequence is used.  */
      for (n=0, i=0; i < idxtbllen; i++ )
        {
          n += _ksba_ber_count_tl (TYPE_INTEGER, CLASS_UNIVERSAL, 0,
                                   parm[idxtbl[i]].valuelen);
          n += parm[idxtbl[i]].valuelen;
        }
  
      n1 = 1; /* # of unused bits.  */
      if (idxtbllen > 1)
        n1 += _ksba_ber_count_tl (TYPE_SEQUENCE, CLASS_UNIVERSAL, 1, n);
      n1 += n;
  
      /* Write the bit string header and the number of unused bits. */
      err = _ksba_ber_write_tl (writer, TYPE_BIT_STRING, CLASS_UNIVERSAL, 
                                0, n1);
      if (!err)
        err = ksba_writer_write (writer, "", 1);
      if (err)
        goto leave;
      
      /* Write the sequence tag and the integers. */
      if (idxtbllen > 1)
        err = _ksba_ber_write_tl (writer, TYPE_SEQUENCE, CLASS_UNIVERSAL, 1,n);
      if (err)
        goto leave;
      for (i=0; i < idxtbllen; i++)
        {
          /* fixme: we should make sure that the integer conforms to the
             ASN.1 encoding rules. */
          err  = _ksba_ber_write_tl (writer, TYPE_INTEGER, CLASS_UNIVERSAL, 0, 
                                     parm[idxtbl[i]].valuelen);
          if (!err)
            err = ksba_writer_write (writer, parm[idxtbl[i]].value, 
                                     parm[idxtbl[i]].valuelen);
          if (err)
            goto leave;
        }
    }

  /* Get the encoded bit string. */
  bitstr_value = ksba_writer_snatch_mem (writer, &bitstr_len);
  if (!bitstr_value)
    {
      err = gpg_error (GPG_ERR_ENOMEM);
      goto leave;
    }
  
  /* If the algorithmIdentifier requires a sequence with parameters,
     build them now.  We can reuse the IDXTBL for that.  */
  if (algoparmdesc)
    {
      idxtbllen = 0;
      for (s = algoparmdesc; *s; s++)
        {
          for (i=0; i < parmidx; i++)
            {
              assert (idxtbllen < DIM (idxtbl));
              if (parm[i].namelen == 1 && parm[i].name[0] == *s)
                {
                  idxtbl[idxtbllen++] = i;
                  break;
                }
            }
        }
      if (idxtbllen != strlen (algoparmdesc))
        return gpg_error (GPG_ERR_UNKNOWN_SEXP);

      err = ksba_writer_set_mem (writer, 1024);
      if (err)
        goto leave;

      /* Calculate the size of the sequence.  */
      for (n=0, i=0; i < idxtbllen; i++ )
        {
          n += _ksba_ber_count_tl (TYPE_INTEGER, CLASS_UNIVERSAL, 0,
                                   parm[idxtbl[i]].valuelen);
          n += parm[idxtbl[i]].valuelen;
        }
      //      n += _ksba_ber_count_tl (TYPE_SEQUENCE, CLASS_UNIVERSAL, 1, n);
  
      /* Write the sequence tag followed by the integers. */
      err = _ksba_ber_write_tl (writer, TYPE_SEQUENCE, CLASS_UNIVERSAL, 1, n);
      if (err)
        goto leave;
      for (i=0; i < idxtbllen; i++)
        {
          err = _ksba_ber_write_tl (writer, TYPE_INTEGER, CLASS_UNIVERSAL, 0, 
                                    parm[idxtbl[i]].valuelen);
          if (!err)
            err = ksba_writer_write (writer, parm[idxtbl[i]].value, 
                                     parm[idxtbl[i]].valuelen);
          if (err)
            goto leave;
        }

      /* Get the encoded sequence.  */
      algoparmseq_value = ksba_writer_snatch_mem (writer, &algoparmseq_len);
      if (!algoparmseq_value)
        {
          err = gpg_error (GPG_ERR_ENOMEM);
          goto leave;
        }
    }
  else
    algoparmseq_len = 0;

  /* Reinitialize the buffer to create the outer sequence. */
  err = ksba_writer_set_mem (writer, 1024);
  if (err)
    goto leave;

  /* Calulate lengths. */
  n  = _ksba_ber_count_tl (TYPE_OBJECT_ID, CLASS_UNIVERSAL, 0, oidlen);
  n += oidlen;
  if (algoparmseq_len)
    {
      n += algoparmseq_len;
    }
  else if (pkalgo == PKALGO_ECC)
    {
      n += _ksba_ber_count_tl (TYPE_OBJECT_ID, CLASS_UNIVERSAL,
                               0, curve_oidlen);
      n += curve_oidlen;
    }
  else if (pkalgo == PKALGO_RSA)
    {
      n += _ksba_ber_count_tl (TYPE_NULL, CLASS_UNIVERSAL, 0, 0);
    }

  n1 = n;
  n1 += _ksba_ber_count_tl (TYPE_SEQUENCE, CLASS_UNIVERSAL, 1, n);
  n1 += bitstr_len;

  /* The outer sequence.  */
  err = _ksba_ber_write_tl (writer, TYPE_SEQUENCE, CLASS_UNIVERSAL, 1, n1);
  if (err)
    goto leave;

  /* The sequence.  */
  err = _ksba_ber_write_tl (writer, TYPE_SEQUENCE, CLASS_UNIVERSAL, 1, n);
  if (err)
    goto leave;

  /* The object id.  */
  err = _ksba_ber_write_tl (writer, TYPE_OBJECT_ID,CLASS_UNIVERSAL, 0, oidlen);
  if (!err)
    err = ksba_writer_write (writer, oid, oidlen);
  if (err)
    goto leave;

  /* The parameter. */
  if (algoparmseq_len)
    {
      err = ksba_writer_write (writer, algoparmseq_value, algoparmseq_len);
    }
  else if (pkalgo == PKALGO_ECC)
    {
      err = _ksba_ber_write_tl (writer, TYPE_OBJECT_ID, CLASS_UNIVERSAL,
                                0, curve_oidlen);
      if (!err)
        err = ksba_writer_write (writer, curve_oid, curve_oidlen);
    }
  else if (pkalgo == PKALGO_RSA)
    {
      err = _ksba_ber_write_tl (writer, TYPE_NULL, CLASS_UNIVERSAL, 0, 0);
    }
  if (err)
    goto leave;

  /* Append the pre-constructed bit string.  */
  err = ksba_writer_write (writer, bitstr_value, bitstr_len);
  if (err)
    goto leave;
  
  /* Get the result. */
  *r_der = ksba_writer_snatch_mem (writer, r_derlen);
  if (!*r_der)
    err = gpg_error (GPG_ERR_ENOMEM);

 leave:
  ksba_writer_release (writer);
  xfree (bitstr_value);
  xfree (curve_oid);
  return err;
}



/* Mode 0: work as described under _ksba_sigval_to_sexp
   mode 1: work as described under _ksba_encval_to_sexp */
static gpg_error_t
cryptval_to_sexp (int mode, const unsigned char *der, size_t derlen,
                  ksba_sexp_t *r_string)
{
  gpg_error_t err;
  const struct algo_table_s *algo_table;
  int c;
  size_t nread, off, len;
  int algoidx;
  int is_bitstr;
  const unsigned char *ctrl;
  const char *elem;
  struct stringbuf sb;

  /* FIXME: The entire function is very similar to keyinfo_to_sexp */
  *r_string = NULL;

  if (!mode)
    algo_table = sig_algo_table;
  else
    algo_table = enc_algo_table;
  

  err = get_algorithm (1, der, derlen, &nread, &off, &len, &is_bitstr,
                       NULL, NULL, NULL);
  if (err)
    return err;
  
  /* look into our table of supported algorithms */
  for (algoidx=0; algo_table[algoidx].oid; algoidx++)
    {
      if ( len == algo_table[algoidx].oidlen
           && !memcmp (der+off, algo_table[algoidx].oid, len))
        break;
    }
  if (!algo_table[algoidx].oid)
    return gpg_error (GPG_ERR_UNKNOWN_ALGORITHM);
  if (!algo_table[algoidx].supported)
    return gpg_error (GPG_ERR_UNSUPPORTED_ALGORITHM);

  der += nread;
  derlen -= nread;

  if (is_bitstr)
    { /* Funny: X.509 defines the signature value as a bit string but
         CMS as an octet string - for ease of implementation we always
         allow both */
      if (!derlen)
        return gpg_error (GPG_ERR_INV_KEYINFO);
      c = *der++; derlen--;
      if (c) 
        fprintf (stderr, "warning: number of unused bits is not zero\n");
    }

  /* fixme: we should calculate the initial length form the size of the
     sequence, so that we don't neen a realloc later */
  init_stringbuf (&sb, 100);
  put_stringbuf (&sb, mode? "(7:enc-val(":"(7:sig-val(");
  put_stringbuf_sexp (&sb, algo_table[algoidx].algo_string);

  /* FIXME: We don't release the stringbuf in case of error
     better let the macro jump to a label */
  elem = algo_table[algoidx].elem_string; 
  ctrl = algo_table[algoidx].ctrl_string; 
  for (; *elem; ctrl++, elem++)
    {
      int is_int;

      if ( (*ctrl & 0x80) && !elem[1] )
        {  /* Hack to allow a raw value */
          is_int = 1;
          len = derlen;
        }
      else
        {
          if (!derlen)
            return gpg_error (GPG_ERR_INV_KEYINFO);
          c = *der++; derlen--;
          if ( c != *ctrl )
            return gpg_error (GPG_ERR_UNEXPECTED_TAG);
          is_int = c == 0x02;
          TLV_LENGTH (der);
        }
      if (is_int && *elem != '-')
        { /* take this integer */
          char tmp[2];
          
          put_stringbuf (&sb, "(");
          tmp[0] = *elem; tmp[1] = 0;
          put_stringbuf_sexp (&sb, tmp);
          put_stringbuf_mem_sexp (&sb, der, len);
          der += len;
          derlen -= len;
          put_stringbuf (&sb, ")");
        }
    }
  put_stringbuf (&sb, ")");
  if (!mode && algo_table[algoidx].digest_string)
    {
      /* Insert the hash algorithm if included in the OID.  */
      put_stringbuf (&sb, "(4:hash");
      put_stringbuf_sexp (&sb, algo_table[algoidx].digest_string);
      put_stringbuf (&sb, ")");
    }
  put_stringbuf (&sb, ")");
  
  *r_string = get_stringbuf (&sb);
  if (!*r_string)
    return gpg_error (GPG_ERR_ENOMEM);

  return 0;
}

/* Assume that DER is a buffer of length DERLEN with a DER encoded
   Asn.1 structure like this:
 
     SEQUENCE { 
        algorithm    OBJECT IDENTIFIER,
        parameters   ANY DEFINED BY algorithm OPTIONAL }
     signature  BIT STRING 
  
  We only allow parameters == NULL.

  The function parses this structure and creates a S-Exp suitable to be
  used as signature value in Libgcrypt:
  
  (sig-val
    (<algo>
      (<param_name1> <mpi>)
      ...
      (<param_namen> <mpi>))
    (hash algo))

 The S-Exp will be returned in a string which the caller must free.
 We don't pass an ASN.1 node here but a plain memory block.  */
gpg_error_t
_ksba_sigval_to_sexp (const unsigned char *der, size_t derlen,
                      ksba_sexp_t *r_string)
{
  return cryptval_to_sexp (0, der, derlen, r_string);
}


/* Assume that der is a buffer of length DERLEN with a DER encoded
 Asn.1 structure like this:
 
     SEQUENCE { 
        algorithm    OBJECT IDENTIFIER,
        parameters   ANY DEFINED BY algorithm OPTIONAL }
     encryptedKey  OCTET STRING 
  
  We only allow parameters == NULL.

  The function parses this structure and creates a S-Exp suitable to be
  used as encrypted value in Libgcrypt's public key functions:
  
  (enc-val
    (<algo>
      (<param_name1> <mpi>)
      ...
      (<param_namen> <mpi>)
    ))

 The S-Exp will be returned in a string which the caller must free.
 We don't pass an ASN.1 node here but a plain memory block.  */
gpg_error_t
_ksba_encval_to_sexp (const unsigned char *der, size_t derlen,
                      ksba_sexp_t *r_string)
{
  return cryptval_to_sexp (1, der, derlen, r_string);
}

