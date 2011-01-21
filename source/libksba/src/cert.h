/* cert.h - Internal definitions for cert.c
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

#ifndef CERT_H
#define CERT_H 1

#include "asn1-func.h"

/* An object to keep parsed information about an extension. */
struct cert_extn_info 
{
  char *oid;
  int crit;
  int off, len;
};


/* An object to store user supplied data to be associated with a
   certificates.  This is implemented as a linked list with the
   constrained that a given key may only occur once. */
struct cert_user_data
{
  struct cert_user_data *next;  /* Link to next item. */

  /* The length of the data store at DATA. */
  size_t datalen;

  /* The stored data.  This is either malloced storage or points to
     DATABUF below.  If it is NULL, no data is actually stored under
     the key. */
  void *data;

 /* Often we need to store only a few bytes as data.  By providing a
    fixed buffer we are able to avoid an extra malloc in this case. */
  char databuf[sizeof (int)];  

  /* The key used to store the data object.  Dynamically allocated at
     object creation time.  */
  char key[1];
};


/* The internal certificate object. */
struct ksba_cert_s 
{
  /* Certificate objects often play a central role and applications
     might want to associate other data with the certificate to avoid
     wrapping the certificate object into an own object.  This UDATA
     linked list provides the means to do that.  It gets accessed by
     ksba_cert_set_user_data and ksba_cert_get_user_data.  */
  struct cert_user_data *udata;

  /* This object has been initialized with an actual certificate.
     Note that UDATA may be used even without an initialized
     certificate. */
  int initialized;    

  /* Because we often need to pass certificate objects to other
     functions, we use reference counting to keep resource overhead
     low.  Note, that this object usually gets only read and not
     modified. */
  int ref_count;

  ksba_asn_tree_t asn_tree;
  AsnNode root;              /* Root of the tree with the values */

  unsigned char *image;
  size_t imagelen;

  gpg_error_t last_error;
  struct {
    char *digest_algo;
    int  extns_valid;
    int  n_extns;
    struct cert_extn_info *extns;
  } cache;
};


/*** Internal functions ***/

int _ksba_cert_cmp (ksba_cert_t a, ksba_cert_t b);

gpg_error_t _ksba_cert_get_serial_ptr (ksba_cert_t cert,
                                       unsigned char const **ptr,
                                       size_t *length);
gpg_error_t _ksba_cert_get_subject_dn_ptr (ksba_cert_t cert,
                                          unsigned char const **ptr,
                                          size_t *length);
gpg_error_t _ksba_cert_get_public_key_ptr (ksba_cert_t cert,
                                           unsigned char const **ptr,
                                           size_t *length);


#endif /*CERT_H*/

