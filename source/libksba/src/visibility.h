/* visibility.h - Set the ELF visibility attribute
 *      Copyright (C) 2008 g10 Code GmbH
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

#ifndef VISIBILITY_H
#define VISIBILITY_H

/* Redefine all public symbols.  */
#define ksba_check_version                 _ksba_check_version
#define ksba_set_hash_buffer_function      _ksba_set_hash_buffer_function
#define ksba_set_malloc_hooks              _ksba_set_malloc_hooks
#define ksba_free                          _ksba_free
#define ksba_malloc                        _ksba_malloc
#define ksba_calloc                        _ksba_calloc
#define ksba_realloc                       _ksba_realloc
#define ksba_strdup                        _ksba_strdup
#define ksba_asn_create_tree               _ksba_asn_create_tree
#define ksba_asn_parse_file                _ksba_asn_parse_file
#define ksba_asn_tree_dump                 _ksba_asn_tree_dump
#define ksba_asn_tree_release              _ksba_asn_tree_release

#define ksba_cert_get_auth_key_id          _ksba_cert_get_auth_key_id
#define ksba_cert_get_cert_policies        _ksba_cert_get_cert_policies
#define ksba_cert_get_crl_dist_point       _ksba_cert_get_crl_dist_point
#define ksba_cert_get_digest_algo          _ksba_cert_get_digest_algo
#define ksba_cert_get_ext_key_usages       _ksba_cert_get_ext_key_usages
#define ksba_cert_get_extension            _ksba_cert_get_extension
#define ksba_cert_get_image                _ksba_cert_get_image
#define ksba_cert_get_issuer               _ksba_cert_get_issuer
#define ksba_cert_get_key_usage            _ksba_cert_get_key_usage
#define ksba_cert_get_public_key           _ksba_cert_get_public_key
#define ksba_cert_get_serial               _ksba_cert_get_serial
#define ksba_cert_get_sig_val              _ksba_cert_get_sig_val
#define ksba_cert_get_subject              _ksba_cert_get_subject
#define ksba_cert_get_validity             _ksba_cert_get_validity
#define ksba_cert_hash                     _ksba_cert_hash
#define ksba_cert_init_from_mem            _ksba_cert_init_from_mem
#define ksba_cert_is_ca                    _ksba_cert_is_ca
#define ksba_cert_new                      _ksba_cert_new
#define ksba_cert_read_der                 _ksba_cert_read_der
#define ksba_cert_ref                      _ksba_cert_ref
#define ksba_cert_release                  _ksba_cert_release
#define ksba_cert_get_authority_info_access \
                                           _ksba_cert_get_authority_info_access
#define ksba_cert_get_subject_info_access  _ksba_cert_get_subject_info_access
#define ksba_cert_get_subj_key_id          _ksba_cert_get_subj_key_id
#define ksba_cert_set_user_data            _ksba_cert_set_user_data
#define ksba_cert_get_user_data            _ksba_cert_get_user_data

#define ksba_certreq_add_subject           _ksba_certreq_add_subject
#define ksba_certreq_build                 _ksba_certreq_build
#define ksba_certreq_new                   _ksba_certreq_new
#define ksba_certreq_release               _ksba_certreq_release
#define ksba_certreq_set_hash_function     _ksba_certreq_set_hash_function
#define ksba_certreq_set_public_key        _ksba_certreq_set_public_key
#define ksba_certreq_set_sig_val           _ksba_certreq_set_sig_val
#define ksba_certreq_set_writer            _ksba_certreq_set_writer
#define ksba_certreq_add_extension         _ksba_certreq_add_extension

#define ksba_cms_add_cert                  _ksba_cms_add_cert
#define ksba_cms_add_digest_algo           _ksba_cms_add_digest_algo
#define ksba_cms_add_recipient             _ksba_cms_add_recipient
#define ksba_cms_add_signer                _ksba_cms_add_signer
#define ksba_cms_build                     _ksba_cms_build
#define ksba_cms_get_cert                  _ksba_cms_get_cert
#define ksba_cms_get_content_enc_iv        _ksba_cms_get_content_enc_iv
#define ksba_cms_get_content_oid           _ksba_cms_get_content_oid
#define ksba_cms_get_content_type          _ksba_cms_get_content_type
#define ksba_cms_get_digest_algo           _ksba_cms_get_digest_algo
#define ksba_cms_get_digest_algo_list      _ksba_cms_get_digest_algo_list
#define ksba_cms_get_enc_val               _ksba_cms_get_enc_val
#define ksba_cms_get_issuer_serial         _ksba_cms_get_issuer_serial
#define ksba_cms_get_message_digest        _ksba_cms_get_message_digest
#define ksba_cms_get_sig_val               _ksba_cms_get_sig_val
#define ksba_cms_get_sigattr_oids          _ksba_cms_get_sigattr_oids
#define ksba_cms_get_signing_time          _ksba_cms_get_signing_time
#define ksba_cms_hash_signed_attrs         _ksba_cms_hash_signed_attrs
#define ksba_cms_identify                  _ksba_cms_identify
#define ksba_cms_new                       _ksba_cms_new
#define ksba_cms_parse                     _ksba_cms_parse
#define ksba_cms_release                   _ksba_cms_release
#define ksba_cms_set_content_enc_algo      _ksba_cms_set_content_enc_algo
#define ksba_cms_set_content_type          _ksba_cms_set_content_type
#define ksba_cms_set_enc_val               _ksba_cms_set_enc_val
#define ksba_cms_set_hash_function         _ksba_cms_set_hash_function
#define ksba_cms_set_message_digest        _ksba_cms_set_message_digest
#define ksba_cms_set_reader_writer         _ksba_cms_set_reader_writer
#define ksba_cms_set_sig_val               _ksba_cms_set_sig_val
#define ksba_cms_set_signing_time          _ksba_cms_set_signing_time
#define ksba_cms_add_smime_capability      _ksba_cms_add_smime_capability

#define ksba_crl_get_digest_algo           _ksba_crl_get_digest_algo
#define ksba_crl_get_issuer                _ksba_crl_get_issuer
#define ksba_crl_get_item                  _ksba_crl_get_item
#define ksba_crl_get_sig_val               _ksba_crl_get_sig_val
#define ksba_crl_get_update_times          _ksba_crl_get_update_times
#define ksba_crl_new                       _ksba_crl_new
#define ksba_crl_parse                     _ksba_crl_parse
#define ksba_crl_release                   _ksba_crl_release
#define ksba_crl_set_hash_function         _ksba_crl_set_hash_function
#define ksba_crl_set_reader                _ksba_crl_set_reader
#define ksba_crl_get_extension             _ksba_crl_get_extension
#define ksba_crl_get_auth_key_id           _ksba_crl_get_auth_key_id
#define ksba_crl_get_crl_number            _ksba_crl_get_crl_number

#define ksba_name_enum                     _ksba_name_enum
#define ksba_name_get_uri                  _ksba_name_get_uri
#define ksba_name_new                      _ksba_name_new
#define ksba_name_ref                      _ksba_name_ref
#define ksba_name_release                  _ksba_name_release

#define ksba_ocsp_add_cert                 _ksba_ocsp_add_cert
#define ksba_ocsp_add_target               _ksba_ocsp_add_target
#define ksba_ocsp_build_request            _ksba_ocsp_build_request
#define ksba_ocsp_get_cert                 _ksba_ocsp_get_cert
#define ksba_ocsp_get_digest_algo          _ksba_ocsp_get_digest_algo
#define ksba_ocsp_get_responder_id         _ksba_ocsp_get_responder_id
#define ksba_ocsp_get_sig_val              _ksba_ocsp_get_sig_val
#define ksba_ocsp_get_status               _ksba_ocsp_get_status
#define ksba_ocsp_hash_request             _ksba_ocsp_hash_request
#define ksba_ocsp_hash_response            _ksba_ocsp_hash_response
#define ksba_ocsp_new                      _ksba_ocsp_new
#define ksba_ocsp_parse_response           _ksba_ocsp_parse_response
#define ksba_ocsp_prepare_request          _ksba_ocsp_prepare_request
#define ksba_ocsp_release                  _ksba_ocsp_release
#define ksba_ocsp_set_digest_algo          _ksba_ocsp_set_digest_algo
#define ksba_ocsp_set_nonce                _ksba_ocsp_set_nonce
#define ksba_ocsp_set_requestor            _ksba_ocsp_set_requestor
#define ksba_ocsp_set_sig_val              _ksba_ocsp_set_sig_val
#define ksba_ocsp_get_extension            _ksba_ocsp_get_extension

#define ksba_oid_from_str                  _ksba_oid_from_str
#define ksba_oid_to_str                    _ksba_oid_to_str

#define ksba_dn_der2str                    _ksba_dn_der2str
#define ksba_dn_str2der                    _ksba_dn_str2der
#define ksba_dn_teststr                    _ksba_dn_teststr

#define ksba_reader_clear                  _ksba_reader_clear
#define ksba_reader_error                  _ksba_reader_error
#define ksba_reader_new                    _ksba_reader_new
#define ksba_reader_read                   _ksba_reader_read
#define ksba_reader_release                _ksba_reader_release
#define ksba_reader_set_cb                 _ksba_reader_set_cb
#define ksba_reader_set_fd                 _ksba_reader_set_fd
#define ksba_reader_set_file               _ksba_reader_set_file
#define ksba_reader_set_mem                _ksba_reader_set_mem
#define ksba_reader_tell                   _ksba_reader_tell
#define ksba_reader_unread                 _ksba_reader_unread

#define ksba_writer_error                  _ksba_writer_error
#define ksba_writer_get_mem                _ksba_writer_get_mem
#define ksba_writer_new                    _ksba_writer_new
#define ksba_writer_release                _ksba_writer_release
#define ksba_writer_set_cb                 _ksba_writer_set_cb
#define ksba_writer_set_fd                 _ksba_writer_set_fd
#define ksba_writer_set_file               _ksba_writer_set_file
#define ksba_writer_set_filter             _ksba_writer_set_filter
#define ksba_writer_set_mem                _ksba_writer_set_mem
#define ksba_writer_snatch_mem             _ksba_writer_snatch_mem
#define ksba_writer_tell                   _ksba_writer_tell
#define ksba_writer_write                  _ksba_writer_write
#define ksba_writer_write_octet_string     _ksba_writer_write_octet_string


/* Include the main header file to map the public symbols to the
   internal underscore prefixed symbols.  */
#include "ksba.h"

/* Our use of the ELF visibility feature works by passing
   -fvisibiliy=hidden on the command line and by explicitly marking
   all exported functions as visible.

   Note: When adding new functions, you need to add them to
         libksba.vers and libksba.def as well.  */
#ifdef KSBA_USE_VISIBILITY
#  define _KSBA_VISIBILITY_DEFAULT  __attribute__ ((visibility("default")))
#else
#  define _KSBA_VISIBILITY_DEFAULT
#endif

#ifdef _KSBA_INCLUDED_BY_VISIBILITY_C
# ifdef KSBA_USE_VISIBILITY
#  define MARK_VISIBLE(name) \
     extern __typeof__ (_##name) name _KSBA_VISIBILITY_DEFAULT;
#  define MARK_VISIBLEX(name) \
     extern __typeof__ (name) name _KSBA_VISIBILITY_DEFAULT;
# else
#  define MARK_VISIBLE(name)  /* */
#  define MARK_VISIBLEX(name) /* */
# endif


/* Prototype for a dummy function we once exported accidently.  */
int ksba_asn_delete_structure (void *dummy);


/* Undef all redefined symbols so that we set the attribute on the
   exported name of the symbol.  */
#undef ksba_check_version
#undef ksba_set_hash_buffer_function
#undef ksba_set_malloc_hooks
#undef ksba_free
#undef ksba_malloc
#undef ksba_calloc
#undef ksba_realloc
#undef ksba_strdup
#undef ksba_asn_create_tree
#undef ksba_asn_parse_file
#undef ksba_asn_tree_dump
#undef ksba_asn_tree_release

#undef ksba_cert_get_auth_key_id
#undef ksba_cert_get_cert_policies
#undef ksba_cert_get_crl_dist_point
#undef ksba_cert_get_digest_algo
#undef ksba_cert_get_ext_key_usages
#undef ksba_cert_get_extension
#undef ksba_cert_get_image
#undef ksba_cert_get_issuer
#undef ksba_cert_get_key_usage
#undef ksba_cert_get_public_key
#undef ksba_cert_get_serial
#undef ksba_cert_get_sig_val
#undef ksba_cert_get_subject
#undef ksba_cert_get_validity
#undef ksba_cert_hash
#undef ksba_cert_init_from_mem
#undef ksba_cert_is_ca
#undef ksba_cert_new
#undef ksba_cert_read_der
#undef ksba_cert_ref
#undef ksba_cert_release
#undef ksba_cert_get_authority_info_access
#undef ksba_cert_get_subject_info_access
#undef ksba_cert_get_subj_key_id
#undef ksba_cert_set_user_data
#undef ksba_cert_get_user_data

#undef ksba_certreq_add_subject
#undef ksba_certreq_build
#undef ksba_certreq_new
#undef ksba_certreq_release
#undef ksba_certreq_set_hash_function
#undef ksba_certreq_set_public_key
#undef ksba_certreq_set_sig_val
#undef ksba_certreq_set_writer
#undef ksba_certreq_add_extension

#undef ksba_cms_add_cert
#undef ksba_cms_add_digest_algo
#undef ksba_cms_add_recipient
#undef ksba_cms_add_signer
#undef ksba_cms_build
#undef ksba_cms_get_cert
#undef ksba_cms_get_content_enc_iv
#undef ksba_cms_get_content_oid
#undef ksba_cms_get_content_type
#undef ksba_cms_get_digest_algo
#undef ksba_cms_get_digest_algo_list
#undef ksba_cms_get_enc_val
#undef ksba_cms_get_issuer_serial
#undef ksba_cms_get_message_digest
#undef ksba_cms_get_sig_val
#undef ksba_cms_get_sigattr_oids
#undef ksba_cms_get_signing_time
#undef ksba_cms_hash_signed_attrs
#undef ksba_cms_identify
#undef ksba_cms_new
#undef ksba_cms_parse
#undef ksba_cms_release
#undef ksba_cms_set_content_enc_algo
#undef ksba_cms_set_content_type
#undef ksba_cms_set_enc_val
#undef ksba_cms_set_hash_function
#undef ksba_cms_set_message_digest
#undef ksba_cms_set_reader_writer
#undef ksba_cms_set_sig_val
#undef ksba_cms_set_signing_time
#undef ksba_cms_add_smime_capability

#undef ksba_crl_get_digest_algo
#undef ksba_crl_get_issuer
#undef ksba_crl_get_item
#undef ksba_crl_get_sig_val
#undef ksba_crl_get_update_times
#undef ksba_crl_new
#undef ksba_crl_parse
#undef ksba_crl_release
#undef ksba_crl_set_hash_function
#undef ksba_crl_set_reader
#undef ksba_crl_get_extension
#undef ksba_crl_get_auth_key_id
#undef ksba_crl_get_crl_number

#undef ksba_name_enum
#undef ksba_name_get_uri
#undef ksba_name_new
#undef ksba_name_ref
#undef ksba_name_release

#undef ksba_ocsp_add_cert
#undef ksba_ocsp_add_target
#undef ksba_ocsp_build_request
#undef ksba_ocsp_get_cert
#undef ksba_ocsp_get_digest_algo
#undef ksba_ocsp_get_responder_id
#undef ksba_ocsp_get_sig_val
#undef ksba_ocsp_get_status
#undef ksba_ocsp_hash_request
#undef ksba_ocsp_hash_response
#undef ksba_ocsp_new
#undef ksba_ocsp_parse_response
#undef ksba_ocsp_prepare_request
#undef ksba_ocsp_release
#undef ksba_ocsp_set_digest_algo
#undef ksba_ocsp_set_nonce
#undef ksba_ocsp_set_requestor
#undef ksba_ocsp_set_sig_val
#undef ksba_ocsp_get_extension

#undef ksba_oid_from_str
#undef ksba_oid_to_str

#undef ksba_dn_der2str
#undef ksba_dn_str2der
#undef ksba_dn_teststr

#undef ksba_reader_clear
#undef ksba_reader_error
#undef ksba_reader_new
#undef ksba_reader_read
#undef ksba_reader_release
#undef ksba_reader_set_cb
#undef ksba_reader_set_fd
#undef ksba_reader_set_file
#undef ksba_reader_set_mem
#undef ksba_reader_tell
#undef ksba_reader_unread

#undef ksba_writer_error
#undef ksba_writer_get_mem
#undef ksba_writer_new
#undef ksba_writer_release
#undef ksba_writer_set_cb
#undef ksba_writer_set_fd
#undef ksba_writer_set_file
#undef ksba_writer_set_filter
#undef ksba_writer_set_mem
#undef ksba_writer_snatch_mem
#undef ksba_writer_tell
#undef ksba_writer_write
#undef ksba_writer_write_octet_string



/* Mark all symbols.  */
MARK_VISIBLE (ksba_check_version)
MARK_VISIBLE (ksba_set_hash_buffer_function)
MARK_VISIBLE (ksba_set_malloc_hooks)
MARK_VISIBLE (ksba_free)
MARK_VISIBLE (ksba_malloc)
MARK_VISIBLE (ksba_calloc)
MARK_VISIBLE (ksba_realloc)
MARK_VISIBLE (ksba_strdup)
MARK_VISIBLE (ksba_asn_create_tree)
MARK_VISIBLE (ksba_asn_parse_file)
MARK_VISIBLE (ksba_asn_tree_dump)
MARK_VISIBLE (ksba_asn_tree_release)
MARK_VISIBLEX (ksba_asn_delete_structure) /* Dummy for ABI compatibility. */

MARK_VISIBLE (ksba_cert_get_auth_key_id)
MARK_VISIBLE (ksba_cert_get_cert_policies)
MARK_VISIBLE (ksba_cert_get_crl_dist_point)
MARK_VISIBLE (ksba_cert_get_digest_algo)
MARK_VISIBLE (ksba_cert_get_ext_key_usages)
MARK_VISIBLE (ksba_cert_get_extension)
MARK_VISIBLE (ksba_cert_get_image)
MARK_VISIBLE (ksba_cert_get_issuer)
MARK_VISIBLE (ksba_cert_get_key_usage)
MARK_VISIBLE (ksba_cert_get_public_key)
MARK_VISIBLE (ksba_cert_get_serial)
MARK_VISIBLE (ksba_cert_get_sig_val)
MARK_VISIBLE (ksba_cert_get_subject)
MARK_VISIBLE (ksba_cert_get_validity)
MARK_VISIBLE (ksba_cert_hash)
MARK_VISIBLE (ksba_cert_init_from_mem)
MARK_VISIBLE (ksba_cert_is_ca)
MARK_VISIBLE (ksba_cert_new)
MARK_VISIBLE (ksba_cert_read_der)
MARK_VISIBLE (ksba_cert_ref)
MARK_VISIBLE (ksba_cert_release)
MARK_VISIBLE (ksba_cert_get_authority_info_access)
MARK_VISIBLE (ksba_cert_get_subject_info_access)
MARK_VISIBLE (ksba_cert_get_subj_key_id)
MARK_VISIBLE (ksba_cert_set_user_data)
MARK_VISIBLE (ksba_cert_get_user_data)

MARK_VISIBLE (ksba_certreq_add_subject)
MARK_VISIBLE (ksba_certreq_build)
MARK_VISIBLE (ksba_certreq_new)
MARK_VISIBLE (ksba_certreq_release)
MARK_VISIBLE (ksba_certreq_set_hash_function)
MARK_VISIBLE (ksba_certreq_set_public_key)
MARK_VISIBLE (ksba_certreq_set_sig_val)
MARK_VISIBLE (ksba_certreq_set_writer)
MARK_VISIBLE (ksba_certreq_add_extension)

MARK_VISIBLE (ksba_cms_add_cert)
MARK_VISIBLE (ksba_cms_add_digest_algo)
MARK_VISIBLE (ksba_cms_add_recipient)
MARK_VISIBLE (ksba_cms_add_signer)
MARK_VISIBLE (ksba_cms_build)
MARK_VISIBLE (ksba_cms_get_cert)
MARK_VISIBLE (ksba_cms_get_content_enc_iv)
MARK_VISIBLE (ksba_cms_get_content_oid)
MARK_VISIBLE (ksba_cms_get_content_type)
MARK_VISIBLE (ksba_cms_get_digest_algo)
MARK_VISIBLE (ksba_cms_get_digest_algo_list)
MARK_VISIBLE (ksba_cms_get_enc_val)
MARK_VISIBLE (ksba_cms_get_issuer_serial)
MARK_VISIBLE (ksba_cms_get_message_digest)
MARK_VISIBLE (ksba_cms_get_sig_val)
MARK_VISIBLE (ksba_cms_get_sigattr_oids)
MARK_VISIBLE (ksba_cms_get_signing_time)
MARK_VISIBLE (ksba_cms_hash_signed_attrs)
MARK_VISIBLE (ksba_cms_identify)
MARK_VISIBLE (ksba_cms_new)
MARK_VISIBLE (ksba_cms_parse)
MARK_VISIBLE (ksba_cms_release)
MARK_VISIBLE (ksba_cms_set_content_enc_algo)
MARK_VISIBLE (ksba_cms_set_content_type)
MARK_VISIBLE (ksba_cms_set_enc_val)
MARK_VISIBLE (ksba_cms_set_hash_function)
MARK_VISIBLE (ksba_cms_set_message_digest)
MARK_VISIBLE (ksba_cms_set_reader_writer)
MARK_VISIBLE (ksba_cms_set_sig_val)
MARK_VISIBLE (ksba_cms_set_signing_time)
MARK_VISIBLE (ksba_cms_add_smime_capability)

MARK_VISIBLE (ksba_crl_get_digest_algo)
MARK_VISIBLE (ksba_crl_get_issuer)
MARK_VISIBLE (ksba_crl_get_item)
MARK_VISIBLE (ksba_crl_get_sig_val)
MARK_VISIBLE (ksba_crl_get_update_times)
MARK_VISIBLE (ksba_crl_new)
MARK_VISIBLE (ksba_crl_parse)
MARK_VISIBLE (ksba_crl_release)
MARK_VISIBLE (ksba_crl_set_hash_function)
MARK_VISIBLE (ksba_crl_set_reader)
MARK_VISIBLE (ksba_crl_get_extension)
MARK_VISIBLE (ksba_crl_get_auth_key_id)
MARK_VISIBLE (ksba_crl_get_crl_number)

MARK_VISIBLE (ksba_name_enum)
MARK_VISIBLE (ksba_name_get_uri)
MARK_VISIBLE (ksba_name_new)
MARK_VISIBLE (ksba_name_ref)
MARK_VISIBLE (ksba_name_release)

MARK_VISIBLE (ksba_ocsp_add_cert)
MARK_VISIBLE (ksba_ocsp_add_target)
MARK_VISIBLE (ksba_ocsp_build_request)
MARK_VISIBLE (ksba_ocsp_get_cert)
MARK_VISIBLE (ksba_ocsp_get_digest_algo)
MARK_VISIBLE (ksba_ocsp_get_responder_id)
MARK_VISIBLE (ksba_ocsp_get_sig_val)
MARK_VISIBLE (ksba_ocsp_get_status)
MARK_VISIBLE (ksba_ocsp_hash_request)
MARK_VISIBLE (ksba_ocsp_hash_response)
MARK_VISIBLE (ksba_ocsp_new)
MARK_VISIBLE (ksba_ocsp_parse_response)
MARK_VISIBLE (ksba_ocsp_prepare_request)
MARK_VISIBLE (ksba_ocsp_release)
MARK_VISIBLE (ksba_ocsp_set_digest_algo)
MARK_VISIBLE (ksba_ocsp_set_nonce)
MARK_VISIBLE (ksba_ocsp_set_requestor)
MARK_VISIBLE (ksba_ocsp_set_sig_val)
MARK_VISIBLE (ksba_ocsp_get_extension)

MARK_VISIBLE (ksba_oid_from_str)
MARK_VISIBLE (ksba_oid_to_str)

MARK_VISIBLE (ksba_dn_der2str)
MARK_VISIBLE (ksba_dn_str2der)
MARK_VISIBLE (ksba_dn_teststr)

MARK_VISIBLE (ksba_reader_clear)
MARK_VISIBLE (ksba_reader_error)
MARK_VISIBLE (ksba_reader_new)
MARK_VISIBLE (ksba_reader_read)
MARK_VISIBLE (ksba_reader_release)
MARK_VISIBLE (ksba_reader_set_cb)
MARK_VISIBLE (ksba_reader_set_fd)
MARK_VISIBLE (ksba_reader_set_file)
MARK_VISIBLE (ksba_reader_set_mem)
MARK_VISIBLE (ksba_reader_tell)
MARK_VISIBLE (ksba_reader_unread)

MARK_VISIBLE (ksba_writer_error)
MARK_VISIBLE (ksba_writer_get_mem)
MARK_VISIBLE (ksba_writer_new)
MARK_VISIBLE (ksba_writer_release)
MARK_VISIBLE (ksba_writer_set_cb)
MARK_VISIBLE (ksba_writer_set_fd)
MARK_VISIBLE (ksba_writer_set_file)
MARK_VISIBLE (ksba_writer_set_filter)
MARK_VISIBLE (ksba_writer_set_mem)
MARK_VISIBLE (ksba_writer_snatch_mem)
MARK_VISIBLE (ksba_writer_tell)
MARK_VISIBLE (ksba_writer_write)
MARK_VISIBLE (ksba_writer_write_octet_string)


#  undef MARK_VISIBLE
#endif /*_KSBA_INCLUDED_BY_VISIBILITY_C*/

#endif /*VISIBILITY_H*/
