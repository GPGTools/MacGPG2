/* ocsp.h - OCSP (rfc2560)
 *      Copyright (C) 2003 g10 Code GmbH
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

#ifndef OCSP_H
#define OCSP_H 1

#include "ksba.h"



/* A structure to store certificates read from a response. */
struct ocsp_certlist_s {
  struct ocsp_certlist_s *next;
  ksba_cert_t cert;
};

/* A structre to save a way extensions. */
struct ocsp_extension_s 
{
  struct ocsp_extension_s *next;
  size_t off;    /* Offset into DATA to the content of the octet string.  */
  size_t len;    /* Length of the octet string. */
  int crit;      /* IsCritical flag. */
  char data[1];  /* This is made up of the OID string followed by the
                    actual DER data of the extension. */
};


/* A structure to keep a information about a single status request. */
struct ocsp_reqitem_s {
  struct ocsp_reqitem_s *next;

  ksba_cert_t cert;        /* The target certificate for the request. */
  ksba_cert_t issuer_cert; /* And the certificate of the issuer. */

  /* The next 4 fields are used to match a response with a request. */
  unsigned char issuer_name_hash[20]; /* The hash as used by the request. */
  unsigned char issuer_key_hash[20];  /* The hash as used by the request. */
  unsigned char *serialno; /* A malloced copy of the serial number. */
  size_t serialnolen;      /* and its length. */

  /* The actual status as parsed from the response. */
  ksba_isotime_t this_update;  /* The thisUpdate value from the response. */
  ksba_isotime_t next_update;  /* The nextUpdate value from the response. */
  ksba_status_t  status;               /* Set to the status of the target. */
  ksba_isotime_t revocation_time;      /* The indicated revocation time. */
  ksba_crl_reason_t revocation_reason; /* The reason given for revocation. */
  struct ocsp_extension_s *single_extensions; /* List of extensions. */
};

/* A structure used as context for the ocsp subsystem. */
struct ksba_ocsp_s {
  char *digest_oid;        /* The OID of the digest algorithm to be
                              used for a request. */

  struct ocsp_reqitem_s *requestlist;  /* The list of request items. */

  size_t noncelen;          /* 0 if no nonce was sent. */
  unsigned char nonce[16];  /* The random nonce we sent; actual length
                               is NONCELEN.  Warning: If its length is
                               increased, check that the created
                               request will still be valid as we use a
                               hacked implementation. */

  unsigned char *request_buffer; /* Internal buffer to build the request. */
  size_t request_buflen;

  size_t hash_offset;      /* What area of the response is to be */
  size_t hash_length;      /* hashed. */

  ksba_ocsp_response_status_t response_status; /* Status of the response. */
  ksba_sexp_t sigval;          /* The signature value. */
  ksba_isotime_t produced_at;  /* The time the response was signed. */
  struct ocsp_certlist_s *received_certs; /* Certificates received in
                                             the response. */
  struct ocsp_extension_s *response_extensions; /* List of extensions. */
  int bad_nonce;            /* The nonce does not match the request. */
  int good_nonce;           /* The nonce does match the request. */
  struct {
    char *name;             /* Allocated DN. */
    char *keyid;            /* Allocated key ID. */
    size_t keyidlen;        /* length of the KeyID. */
  } responder_id;           /* The reponder ID from the response. */
};


#endif /*OCSP_H*/
