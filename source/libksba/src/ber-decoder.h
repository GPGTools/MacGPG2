/* ber-decoder.h - Definitions for the Basic Encoding Rules Decoder
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

#ifndef BER_DECODER_H
#define BER_DECODER_H 1

#include "asn1-func.h"

struct ber_decoder_s;
typedef struct ber_decoder_s *BerDecoder;

BerDecoder _ksba_ber_decoder_new (void);
void       _ksba_ber_decoder_release (BerDecoder d);

gpg_error_t _ksba_ber_decoder_set_module (BerDecoder d, ksba_asn_tree_t module);
gpg_error_t _ksba_ber_decoder_set_reader (BerDecoder d, ksba_reader_t r);

gpg_error_t _ksba_ber_decoder_dump (BerDecoder d, FILE *fp);
gpg_error_t _ksba_ber_decoder_decode (BerDecoder d, const char *start_name,
                                      unsigned int flags,
                                      AsnNode *r_root,
                                      unsigned char **r_image,
                                      size_t *r_imagelen);

#define BER_DECODER_FLAG_FAST_STOP 1


#endif /*BER_DECODER_H*/
