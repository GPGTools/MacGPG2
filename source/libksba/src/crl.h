/* crl.h - Internal definitions for the CRL Parser
 *      Copyright (C) 2002 g10 Code GmbH
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

#ifndef CRL_H
#define CRL_H 1

#include "ksba.h"

#ifndef HAVE_TYPEDEFD_ASNNODE
typedef struct asn_node_struct *AsnNode;  /* FIXME: should not go here */
#define HAVE_TYPEDEFD_ASNNODE
#endif


struct crl_extn_s {
  struct crl_extn_s *next;
  char *oid;
  int critical;
  size_t derlen;
  unsigned char der[1];
};
typedef struct crl_extn_s *crl_extn_t;

struct ksba_crl_s {
  gpg_error_t last_error;

  ksba_reader_t reader;
  int any_parse_done;

  void (*hash_fnc)(void *, const void *, size_t);
  void *hash_fnc_arg;

  struct {
    struct tag_info ti;
    unsigned long outer_len, tbs_len, seqseq_len;
    int outer_ndef, tbs_ndef, seqseq_ndef;
    int have_seqseq;
  } state;

  int crl_version;
  struct {
    char *oid;
    char *parm;
    size_t parmlen;
  } algo;
  struct {
    AsnNode root;  /* root of the tree with the values */
    unsigned char *image;
    size_t imagelen;
  } issuer;
  ksba_isotime_t this_update;
  ksba_isotime_t next_update;

  struct {
    ksba_sexp_t serial;
    ksba_crl_reason_t reason;
    ksba_isotime_t revocation_date;
  } item;

  crl_extn_t extension_list;
  ksba_sexp_t sigval;

  struct {
    int used;
    char buffer[8192]; 
  } hashbuf;

};


/*-- crl.c --*/


#endif /*CRL_H*/


