/* cms.h - Internal definitions for the CMS functions
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

#ifndef CMS_H
#define CMS_H 1

#include "ksba.h"

#ifndef HAVE_TYPEDEFD_ASNNODE
typedef struct asn_node_struct *AsnNode;  /* FIXME: should not go here */
#define HAVE_TYPEDEFD_ASNNODE
#endif


/* This structure is used to store the results of a BER parser run. */
struct value_tree_s {
  struct value_tree_s *next;
  AsnNode root;  /* root of the tree with the values */
  unsigned char *image;
  size_t imagelen;
};


struct enc_val_s {
  char *algo;
  unsigned char *value;
  size_t valuelen;
};


struct oidlist_s {
  struct oidlist_s *next;
  char *oid;
};

/* A structure to store an OID and a parameter. */
struct oidparmlist_s {
  struct oidparmlist_s *next;
  char *oid;
  size_t parmlen;
  unsigned char parm[1];
};


struct certlist_s {
  struct certlist_s *next;
  ksba_cert_t cert;
  int  msg_digest_len;  /* used length of .. */
  char msg_digest[64];  /* enough space to store a SHA-512 hash */
  ksba_isotime_t signing_time;
  struct {
    AsnNode root;
    unsigned char *image;
  } sa;
  struct enc_val_s enc_val; /* used for creating enveloped data */
};


struct signer_info_s {
  struct signer_info_s *next;
  AsnNode root;  /* root of the tree with the values */
  unsigned char *image;
  size_t imagelen;
  struct {
    char *digest_algo;
  } cache;
};

struct sig_val_s {
  struct sig_val_s *next;
  char *algo;
  unsigned char *value;
  size_t valuelen;
};

struct ksba_cms_s {
  gpg_error_t last_error;

  ksba_reader_t reader;
  ksba_writer_t writer;

  void (*hash_fnc)(void *, const void *, size_t);
  void *hash_fnc_arg;

  ksba_stop_reason_t stop_reason;
  
  struct {
    char *oid;
    unsigned long length;
    int ndef;
    ksba_content_type_t ct;
    gpg_error_t (*handler)(ksba_cms_t);
  } content;

  struct {
    unsigned char *digest;
    int digest_len;
  } data;

  int cms_version;   
  
  struct oidlist_s *digest_algos;
  struct certlist_s *cert_list;
  char *inner_cont_oid; /* Encapsulated or Encrypted
                           ContentInfo.contentType as string */
  unsigned long inner_cont_len;
  int inner_cont_ndef;
  int detached_data; /* no actual data */
  char *encr_algo_oid;
  char *encr_iv;
  size_t encr_ivlen;

  struct certlist_s *cert_info_list; /* A list with certificates intended
                                        to be send with a signed message */

  struct oidparmlist_s *capability_list; /* A list of S/MIME capabilities. */
  
  struct signer_info_s *signer_info;

  struct value_tree_s *recp_info;

  struct sig_val_s *sig_val;

  struct enc_val_s *enc_val;
};


/*-- cms.c --*/


/*-- cms-parser.c --*/
gpg_error_t _ksba_cms_parse_content_info (ksba_cms_t cms);
gpg_error_t _ksba_cms_parse_signed_data_part_1 (ksba_cms_t cms);
gpg_error_t _ksba_cms_parse_signed_data_part_2 (ksba_cms_t cms);
gpg_error_t _ksba_cms_parse_enveloped_data_part_1 (ksba_cms_t cms);
gpg_error_t _ksba_cms_parse_enveloped_data_part_2 (ksba_cms_t cms);



#endif /*CMS_H*/


