/* certreq.h - Internal definitions for pkcs-10 objects
 *      Copyright (C) 2002, 2005 g10 Code GmbH
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

#ifndef CERTREQ_H
#define CERTREQ_H 1

#include "ksba.h"

#ifndef HAVE_TYPEDEFD_ASNNODE
typedef struct asn_node_struct *AsnNode;  /* FIXME: should not go here */
#define HAVE_TYPEDEFD_ASNNODE
#endif

struct extn_list_s 
{
  struct extn_list_s *next;
  const char *oid;
  int critical;
  int derlen;
  unsigned char der[1];
};


/* Object to collect information for building a GeneralNames. */
struct general_names_s 
{
  struct general_names_s *next;
  int tag;   /* The GeneralName CHOICE.  Only certain values are
                supported. This is not strictly required because DATA
                below has already been prefixed with the DER encoded
                tag. */
  size_t datalen; /* Length of the data. */
  char data[1];   /* The actual data:  encoded tag, llength and value. */
};


struct ksba_certreq_s 
{
  gpg_error_t last_error;

  ksba_writer_t writer;

  void (*hash_fnc)(void *, const void *, size_t);
  void *hash_fnc_arg;

  int any_build_done;

  struct {
    char *der;
    size_t derlen;
  } subject;
  struct {
    unsigned char *der;
    size_t derlen;
  } key;

  struct general_names_s *subject_alt_names;

  struct extn_list_s *extn_list;

  struct {
    unsigned char *der;
    size_t derlen;
  } cri;

  struct {
    char *algo;
    unsigned char *value;
    size_t valuelen;
  } sig_val;


  
};



#endif /*CERTREQ_H*/


