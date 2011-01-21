/* der-encoder.h - Definitions for the Distinguished Encoding Rules Encoder
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

#ifndef DER_ENCODER_H
#define DER_ENCODER_H 1

#include "asn1-func.h"

struct der_encoder_s;
typedef struct der_encoder_s *DerEncoder;

DerEncoder _ksba_der_encoder_new (void);
void       _ksba_der_encoder_release (DerEncoder d);

gpg_error_t _ksba_der_encoder_set_module (DerEncoder d, ksba_asn_tree_t module);
gpg_error_t _ksba_der_encoder_set_writer (DerEncoder d, ksba_writer_t w);


gpg_error_t _ksba_der_write_integer (ksba_writer_t w,
                                     const unsigned char *value);
gpg_error_t _ksba_der_write_algorithm_identifier (
            ksba_writer_t w, const char *oid, const void *parm, size_t parmlen);



gpg_error_t _ksba_der_copy_tree (AsnNode dst,
                               AsnNode src, const unsigned char *srcimage);



gpg_error_t _ksba_der_store_time (AsnNode node, const ksba_isotime_t atime);
gpg_error_t _ksba_der_store_string (AsnNode node, const char *string);
gpg_error_t _ksba_der_store_integer (AsnNode node, const unsigned char *value);
gpg_error_t _ksba_der_store_oid (AsnNode node, const char *oid);
gpg_error_t _ksba_der_store_octet_string (AsnNode node,
                                        const char *buf, size_t len);
gpg_error_t _ksba_der_store_sequence (AsnNode node,
                                      const unsigned char *buf, size_t len);
gpg_error_t _ksba_der_store_null (AsnNode node);


gpg_error_t _ksba_der_encode_tree (AsnNode root,
                                 unsigned char **r_image, size_t *r_imagelen);



#endif /*DER_ENCODER_H*/


