/* visibility.c - Wrapper for all public functions
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

#include <config.h>
#include <stdarg.h>

#define _KSBA_INCLUDED_BY_VISIBILITY_C 
#include "util.h"

/*--version.c --*/
const char *
ksba_check_version (const char *req_version)
{
  return _ksba_check_version (req_version);
}


/*-- util.c --*/
void 
ksba_set_malloc_hooks ( void *(*new_alloc_func)(size_t n),
                        void *(*new_realloc_func)(void *p, size_t n),
                        void (*new_free_func)(void*) )
{
  _ksba_set_malloc_hooks (new_alloc_func, new_realloc_func, new_free_func);
}


void 
ksba_set_hash_buffer_function ( gpg_error_t (*fnc)
                                (void *arg, const char *oid,
                                 const void *buffer, size_t length,
                                 size_t resultsize,
                                 unsigned char *result,
                                 size_t *resultlen),
                                void *fnc_arg)
{
  _ksba_set_hash_buffer_function (fnc, fnc_arg);
}

void *
ksba_malloc (size_t n )
{
  return _ksba_malloc (n);
}

void *
ksba_calloc (size_t n, size_t m )
{
  return _ksba_calloc (n, m);
}

void *
ksba_realloc (void *p, size_t n)
{
  return _ksba_realloc (p, n);
}

char *
ksba_strdup (const char *p)
{
  return _ksba_strdup (p);
}

void
ksba_free ( void *a )
{
  if (a)
    _ksba_free (a);
}


/*-- cert.c --*/
gpg_error_t 
ksba_cert_new (ksba_cert_t *acert)
{
  return _ksba_cert_new (acert);
}


void
ksba_cert_ref (ksba_cert_t cert)
{
  _ksba_cert_ref (cert);
}


void
ksba_cert_release (ksba_cert_t cert)
{
  _ksba_cert_release (cert);
}


gpg_error_t
ksba_cert_set_user_data (ksba_cert_t cert, const char *key,
                         const void *data, size_t datalen)
{
  return _ksba_cert_set_user_data (cert, key, data, datalen);
}


gpg_error_t
ksba_cert_get_user_data (ksba_cert_t cert, const char *key,
                         void *buffer, size_t bufferlen,
                         size_t *datalen)
{
  return _ksba_cert_get_user_data (cert, key, buffer, bufferlen, datalen);
}



gpg_error_t
ksba_cert_read_der (ksba_cert_t cert, ksba_reader_t reader)
{
  return _ksba_cert_read_der (cert, reader);
}


gpg_error_t
ksba_cert_init_from_mem (ksba_cert_t cert,
                         const void *buffer, size_t length)
{
  return _ksba_cert_init_from_mem (cert, buffer, length);
}


const unsigned char *
ksba_cert_get_image (ksba_cert_t cert, size_t *r_length)
{
  return _ksba_cert_get_image (cert, r_length);
}


gpg_error_t
ksba_cert_hash (ksba_cert_t cert,
                int what,
                void (*hasher)(void *,
                               const void *,
                               size_t length), 
                void *hasher_arg)
{
  return _ksba_cert_hash (cert, what, hasher, hasher_arg);
}


const char *
ksba_cert_get_digest_algo (ksba_cert_t cert)
{
  return _ksba_cert_get_digest_algo (cert);
}


ksba_sexp_t
ksba_cert_get_serial (ksba_cert_t cert)
{
  return _ksba_cert_get_serial (cert);
}


char *
ksba_cert_get_issuer (ksba_cert_t cert, int idx)
{
  return _ksba_cert_get_issuer (cert, idx);
}


gpg_error_t 
ksba_cert_get_validity (ksba_cert_t cert, int what,
                        ksba_isotime_t r_time)
{
  return _ksba_cert_get_validity (cert, what, r_time);
}


char *
ksba_cert_get_subject (ksba_cert_t cert, int idx)
{
  return _ksba_cert_get_subject (cert, idx);
}


ksba_sexp_t 
ksba_cert_get_public_key (ksba_cert_t cert)
{
  return _ksba_cert_get_public_key (cert);
}


ksba_sexp_t
ksba_cert_get_sig_val (ksba_cert_t cert)
{
  return _ksba_cert_get_sig_val (cert);
}



gpg_error_t 
ksba_cert_get_extension (ksba_cert_t cert, int idx,
                         char const **r_oid, int *r_crit,
                         size_t *r_deroff, size_t *r_derlen)
{
  return _ksba_cert_get_extension (cert, idx, r_oid, r_crit, 
                                   r_deroff, r_derlen);
}



gpg_error_t
ksba_cert_is_ca (ksba_cert_t cert, int *r_ca, int *r_pathlen)
{
  return _ksba_cert_is_ca (cert, r_ca, r_pathlen);
}


gpg_error_t
ksba_cert_get_key_usage (ksba_cert_t cert, unsigned int *r_flags)
{
  return _ksba_cert_get_key_usage (cert, r_flags);
}


gpg_error_t
ksba_cert_get_cert_policies (ksba_cert_t cert, char **r_policies)
{
  return _ksba_cert_get_cert_policies (cert, r_policies);
}


gpg_error_t
ksba_cert_get_ext_key_usages (ksba_cert_t cert, char **result)
{
  return _ksba_cert_get_ext_key_usages (cert, result);
}


gpg_error_t 
ksba_cert_get_crl_dist_point (ksba_cert_t cert, int idx,
                              ksba_name_t *r_distpoint,
                              ksba_name_t *r_issuer,
                              ksba_crl_reason_t *r_reason)
{
  return _ksba_cert_get_crl_dist_point (cert, idx, r_distpoint, r_issuer,
                                        r_reason);
}


gpg_error_t
ksba_cert_get_auth_key_id (ksba_cert_t cert,
                           ksba_sexp_t *r_keyid,
                           ksba_name_t *r_name,
                           ksba_sexp_t *r_serial)
{
  return _ksba_cert_get_auth_key_id (cert, r_keyid, r_name, r_serial);
}


gpg_error_t
ksba_cert_get_subj_key_id (ksba_cert_t cert,
                           int *r_crit,
                           ksba_sexp_t *r_keyid)
{
  return _ksba_cert_get_subj_key_id (cert, r_crit, r_keyid);
}


gpg_error_t
ksba_cert_get_authority_info_access (ksba_cert_t cert, int idx,
                                     char **r_method,
                                     ksba_name_t *r_location)
{
  return _ksba_cert_get_authority_info_access (cert, idx, 
                                               r_method, r_location);
}


gpg_error_t
ksba_cert_get_subject_info_access (ksba_cert_t cert, int idx,
                                   char **r_method,
                                   ksba_name_t *r_location)
{
  return _ksba_cert_get_subject_info_access (cert, idx, r_method, r_location);
}




/*-- cms.c --*/
ksba_content_type_t
ksba_cms_identify (ksba_reader_t reader)
{
  return _ksba_cms_identify (reader);
}



gpg_error_t
ksba_cms_new (ksba_cms_t *r_cms)
{
  return _ksba_cms_new (r_cms);
}


void
ksba_cms_release (ksba_cms_t cms)
{
  _ksba_cms_release (cms);
}


gpg_error_t
ksba_cms_set_reader_writer (ksba_cms_t cms,
                            ksba_reader_t r, ksba_writer_t w)
{
  return _ksba_cms_set_reader_writer (cms, r, w);
}



gpg_error_t
ksba_cms_parse (ksba_cms_t cms, ksba_stop_reason_t *r_stopreason)
{
  return _ksba_cms_parse (cms, r_stopreason);
}


gpg_error_t
ksba_cms_build (ksba_cms_t cms, ksba_stop_reason_t *r_stopreason)
{
  return _ksba_cms_build (cms, r_stopreason);
}


ksba_content_type_t
ksba_cms_get_content_type (ksba_cms_t cms, int what)
{
  return _ksba_cms_get_content_type (cms, what);
}


const char *
ksba_cms_get_content_oid (ksba_cms_t cms, int what)
{
  return _ksba_cms_get_content_oid (cms, what);
}


gpg_error_t
ksba_cms_get_content_enc_iv (ksba_cms_t cms, void *iv,
                             size_t maxivlen, size_t *ivlen)
{
  return _ksba_cms_get_content_enc_iv (cms, iv, maxivlen, ivlen);
}


const char *
ksba_cms_get_digest_algo_list (ksba_cms_t cms, int idx)
{
  return _ksba_cms_get_digest_algo_list (cms, idx);
}


gpg_error_t
ksba_cms_get_issuer_serial (ksba_cms_t cms, int idx,
                            char **r_issuer,
                            ksba_sexp_t *r_serial)
{
  return _ksba_cms_get_issuer_serial (cms, idx, r_issuer, r_serial);
}


const char *
ksba_cms_get_digest_algo (ksba_cms_t cms, int idx)
{
  return _ksba_cms_get_digest_algo (cms, idx);
}


ksba_cert_t
ksba_cms_get_cert (ksba_cms_t cms, int idx)
{
  return _ksba_cms_get_cert (cms, idx);
}


gpg_error_t
ksba_cms_get_message_digest (ksba_cms_t cms, int idx,
                             char **r_digest, size_t *r_digest_len)
{
  return _ksba_cms_get_message_digest (cms, idx, r_digest, r_digest_len);
}


gpg_error_t
ksba_cms_get_signing_time (ksba_cms_t cms, int idx,
                           ksba_isotime_t r_sigtime)
{
  return _ksba_cms_get_signing_time (cms, idx, r_sigtime);
}


gpg_error_t
ksba_cms_get_sigattr_oids (ksba_cms_t cms, int idx,
                           const char *reqoid, char **r_value)
{
  return _ksba_cms_get_sigattr_oids (cms, idx, reqoid, r_value);
}


ksba_sexp_t
ksba_cms_get_sig_val (ksba_cms_t cms, int idx)
{
  return _ksba_cms_get_sig_val (cms, idx);
}


ksba_sexp_t
ksba_cms_get_enc_val (ksba_cms_t cms, int idx)
{
  return _ksba_cms_get_enc_val (cms, idx);
}


void
ksba_cms_set_hash_function (ksba_cms_t cms,
                            void (*hash_fnc)(void *, const void *, size_t),
                            void *hash_fnc_arg)
{
  _ksba_cms_set_hash_function (cms, hash_fnc, hash_fnc_arg);
}


gpg_error_t
ksba_cms_hash_signed_attrs (ksba_cms_t cms, int idx)
{
  return _ksba_cms_hash_signed_attrs (cms, idx);
}



gpg_error_t
ksba_cms_set_content_type (ksba_cms_t cms, int what,
                           ksba_content_type_t type)
{
  return _ksba_cms_set_content_type (cms, what, type);
}


gpg_error_t
ksba_cms_add_digest_algo (ksba_cms_t cms, const char *oid)
{
  return _ksba_cms_add_digest_algo (cms, oid);
}


gpg_error_t
ksba_cms_add_signer (ksba_cms_t cms, ksba_cert_t cert)
{
  return _ksba_cms_add_signer (cms, cert);
}


gpg_error_t
ksba_cms_add_cert (ksba_cms_t cms, ksba_cert_t cert)
{
  return _ksba_cms_add_cert (cms, cert);
}


gpg_error_t
ksba_cms_add_smime_capability (ksba_cms_t cms, const char *oid,
                               const unsigned char *der,
                               size_t derlen)
{
  return _ksba_cms_add_smime_capability (cms, oid, der, derlen);
}


gpg_error_t
ksba_cms_set_message_digest (ksba_cms_t cms, int idx,
                             const unsigned char *digest,
                             size_t digest_len)
{
  return _ksba_cms_set_message_digest (cms, idx, digest, digest_len);
}


gpg_error_t
ksba_cms_set_signing_time (ksba_cms_t cms, int idx,
                           const ksba_isotime_t sigtime)
{
  return _ksba_cms_set_signing_time (cms, idx, sigtime);
}


gpg_error_t
ksba_cms_set_sig_val (ksba_cms_t cms,
                      int idx, ksba_const_sexp_t sigval)
{
  return _ksba_cms_set_sig_val (cms, idx, sigval);
}



gpg_error_t
ksba_cms_set_content_enc_algo (ksba_cms_t cms,
                               const char *oid,
                               const void *iv,
                               size_t ivlen)
{
  return _ksba_cms_set_content_enc_algo (cms, oid, iv, ivlen);
}


gpg_error_t
ksba_cms_add_recipient (ksba_cms_t cms, ksba_cert_t cert)
{
  return _ksba_cms_add_recipient (cms, cert);
}


gpg_error_t
ksba_cms_set_enc_val (ksba_cms_t cms,
                      int idx, ksba_const_sexp_t encval)
{
  return _ksba_cms_set_enc_val (cms, idx, encval);
}




/*-- crl.c --*/
gpg_error_t
ksba_crl_new (ksba_crl_t *r_crl)
{
  return _ksba_crl_new (r_crl);
}


void
ksba_crl_release (ksba_crl_t crl)
{
  _ksba_crl_release (crl);
}


gpg_error_t
ksba_crl_set_reader (ksba_crl_t crl, ksba_reader_t r)
{
  return _ksba_crl_set_reader (crl, r);
}


void
ksba_crl_set_hash_function (ksba_crl_t crl,
                            void (*hash_fnc)(void *,
                                             const void *, size_t),
                            void *hash_fnc_arg)
{
  _ksba_crl_set_hash_function (crl, hash_fnc, hash_fnc_arg);
}


const char *
ksba_crl_get_digest_algo (ksba_crl_t crl)
{
  return _ksba_crl_get_digest_algo (crl);
}


gpg_error_t
ksba_crl_get_issuer (ksba_crl_t crl, char **r_issuer)
{
  return _ksba_crl_get_issuer (crl, r_issuer);
}


gpg_error_t
ksba_crl_get_extension (ksba_crl_t crl, int idx, 
                        char const **oid, int *critical,
                        unsigned char const **der, size_t *derlen)
{
  return _ksba_crl_get_extension (crl, idx, oid, critical, der, derlen);
}


gpg_error_t
ksba_crl_get_auth_key_id (ksba_crl_t crl,
                          ksba_sexp_t *r_keyid,
                          ksba_name_t *r_name,
                          ksba_sexp_t *r_serial)
{
  return _ksba_crl_get_auth_key_id (crl, r_keyid, r_name, r_serial);
}


gpg_error_t
ksba_crl_get_crl_number (ksba_crl_t crl, ksba_sexp_t *number)
{
  return _ksba_crl_get_crl_number (crl, number);
}


gpg_error_t
ksba_crl_get_update_times (ksba_crl_t crl,
                           ksba_isotime_t this_update,
                           ksba_isotime_t next_update)
{
  return _ksba_crl_get_update_times (crl, this_update, next_update);
}


gpg_error_t
ksba_crl_get_item (ksba_crl_t crl,
                   ksba_sexp_t *r_serial,
                   ksba_isotime_t r_revocation_date,
                   ksba_crl_reason_t *r_reason)
{
  return _ksba_crl_get_item (crl, r_serial, r_revocation_date, r_reason);
}


ksba_sexp_t
ksba_crl_get_sig_val (ksba_crl_t crl)
{
  return _ksba_crl_get_sig_val (crl);
}


gpg_error_t
ksba_crl_parse (ksba_crl_t crl, ksba_stop_reason_t *r_stopreason)
{
  return _ksba_crl_parse (crl, r_stopreason);
}




/*-- ocsp.c --*/
gpg_error_t
ksba_ocsp_new (ksba_ocsp_t *r_oscp)
{
  return _ksba_ocsp_new (r_oscp);
}


void
ksba_ocsp_release (ksba_ocsp_t ocsp)
{
  _ksba_ocsp_release (ocsp);
}


gpg_error_t
ksba_ocsp_set_digest_algo (ksba_ocsp_t ocsp, const char *oid)
{
  return _ksba_ocsp_set_digest_algo (ocsp, oid);
}


gpg_error_t
ksba_ocsp_set_requestor (ksba_ocsp_t ocsp, ksba_cert_t cert)
{
  return _ksba_ocsp_set_requestor (ocsp, cert);
}


gpg_error_t
ksba_ocsp_add_target (ksba_ocsp_t ocsp,
                      ksba_cert_t cert, ksba_cert_t issuer_cert)
{
  return _ksba_ocsp_add_target (ocsp, cert, issuer_cert);
}


size_t
ksba_ocsp_set_nonce (ksba_ocsp_t ocsp,
                     unsigned char *nonce, size_t noncelen)
{
  return _ksba_ocsp_set_nonce (ocsp, nonce, noncelen);
}



gpg_error_t
ksba_ocsp_prepare_request (ksba_ocsp_t ocsp)
{
  return _ksba_ocsp_prepare_request (ocsp);
}


gpg_error_t
ksba_ocsp_hash_request (ksba_ocsp_t ocsp,
                        void (*hasher)(void *, const void *,
                                       size_t length), 
                        void *hasher_arg)
{
  return _ksba_ocsp_hash_request (ocsp, hasher, hasher_arg);
}


gpg_error_t
ksba_ocsp_set_sig_val (ksba_ocsp_t ocsp,
                       ksba_const_sexp_t sigval)
{
  return _ksba_ocsp_set_sig_val (ocsp, sigval);
}


gpg_error_t
ksba_ocsp_add_cert (ksba_ocsp_t ocsp, ksba_cert_t cert)
{
  return _ksba_ocsp_add_cert (ocsp, cert);
}


gpg_error_t
ksba_ocsp_build_request (ksba_ocsp_t ocsp,
                         unsigned char **r_buffer,
                         size_t *r_buflen)
{
  return _ksba_ocsp_build_request (ocsp, r_buffer, r_buflen);
}



gpg_error_t
ksba_ocsp_parse_response (ksba_ocsp_t ocsp,
                          const unsigned char *msg, size_t msglen,
                          ksba_ocsp_response_status_t *resp_status)
{
  return _ksba_ocsp_parse_response (ocsp, msg, msglen, resp_status);
}



const char *
ksba_ocsp_get_digest_algo (ksba_ocsp_t ocsp)
{
  return _ksba_ocsp_get_digest_algo (ocsp);
}


gpg_error_t
ksba_ocsp_hash_response (ksba_ocsp_t ocsp,
                         const unsigned char *msg, size_t msglen,
                         void (*hasher)(void *, const void *,
                                        size_t length), 
                         void *hasher_arg)
{
  return _ksba_ocsp_hash_response (ocsp, msg, msglen, hasher, hasher_arg);
}


ksba_sexp_t
ksba_ocsp_get_sig_val (ksba_ocsp_t ocsp,
                       ksba_isotime_t produced_at)
{
  return _ksba_ocsp_get_sig_val (ocsp, produced_at);
}


gpg_error_t
ksba_ocsp_get_responder_id (ksba_ocsp_t ocsp,
                            char **r_name,
                            ksba_sexp_t *r_keyid)
{
  return _ksba_ocsp_get_responder_id (ocsp, r_name, r_keyid);
}


ksba_cert_t
ksba_ocsp_get_cert (ksba_ocsp_t ocsp, int idx)
{
  return _ksba_ocsp_get_cert (ocsp, idx);
}


gpg_error_t
ksba_ocsp_get_status (ksba_ocsp_t ocsp, ksba_cert_t cert,
                      ksba_status_t *r_status,
                      ksba_isotime_t r_this_update,
                      ksba_isotime_t r_next_update,
                      ksba_isotime_t r_revocation_time,
                      ksba_crl_reason_t *r_reason)
{
  return _ksba_ocsp_get_status (ocsp, cert, r_status, r_this_update,
                                r_next_update, r_revocation_time, r_reason);
}


gpg_error_t
ksba_ocsp_get_extension (ksba_ocsp_t ocsp, ksba_cert_t cert,
                         int idx,
                         char const **r_oid, int *r_crit,
                         unsigned char const **r_der, 
                         size_t *r_derlen)
{
  return _ksba_ocsp_get_extension (ocsp, cert, idx, r_oid, r_crit, 
                                   r_der, r_derlen);
}




/*-- certreq.c --*/
gpg_error_t
ksba_certreq_new (ksba_certreq_t *r_cr)
{
  return _ksba_certreq_new (r_cr);
}


void
ksba_certreq_release (ksba_certreq_t cr)
{
  _ksba_certreq_release (cr);
}


gpg_error_t
ksba_certreq_set_writer (ksba_certreq_t cr, ksba_writer_t w)
{
  return _ksba_certreq_set_writer (cr, w);
}


void
ksba_certreq_set_hash_function (ksba_certreq_t cr,
                                void (*hash_fnc)(void *, const void *, size_t),
                                void *hash_fnc_arg)
{
  _ksba_certreq_set_hash_function (cr, hash_fnc, hash_fnc_arg);
}


gpg_error_t
ksba_certreq_add_subject (ksba_certreq_t cr, const char *name)
{
  return _ksba_certreq_add_subject (cr, name);
}


gpg_error_t
ksba_certreq_set_public_key (ksba_certreq_t cr,
                             ksba_const_sexp_t key)
{
  return _ksba_certreq_set_public_key (cr, key);
}


gpg_error_t
ksba_certreq_add_extension (ksba_certreq_t cr,
                            const char *oid, int is_crit,
                            const void *der,
                            size_t derlen)
{
  return _ksba_certreq_add_extension (cr, oid, is_crit, der, derlen);
}


gpg_error_t
ksba_certreq_set_sig_val (ksba_certreq_t cr,
                          ksba_const_sexp_t sigval)
{
  return _ksba_certreq_set_sig_val (cr, sigval);
}


gpg_error_t
ksba_certreq_build (ksba_certreq_t cr,
                    ksba_stop_reason_t *r_stopreason)
{
  return _ksba_certreq_build (cr, r_stopreason);
}




/*-- reader.c --*/
gpg_error_t
ksba_reader_new (ksba_reader_t *r_r)
{
  return _ksba_reader_new (r_r);
}


void
ksba_reader_release (ksba_reader_t r)
{
  _ksba_reader_release (r);
}


gpg_error_t
ksba_reader_clear (ksba_reader_t r,
                   unsigned char **buffer, size_t *buflen)
{
  return _ksba_reader_clear (r, buffer, buflen);
}


gpg_error_t
ksba_reader_error (ksba_reader_t r)
{
  return _ksba_reader_error (r);
}



gpg_error_t
ksba_reader_set_mem (ksba_reader_t r,
                     const void *buffer, size_t length)
{
  return _ksba_reader_set_mem (r, buffer, length);
}


gpg_error_t
ksba_reader_set_fd (ksba_reader_t r, int fd)
{
  return _ksba_reader_set_fd (r, fd);
}


gpg_error_t
ksba_reader_set_file (ksba_reader_t r, FILE *fp)
{
  return _ksba_reader_set_file (r, fp);
}


gpg_error_t
ksba_reader_set_cb (ksba_reader_t r, 
                    int (*cb)(void*,char *,size_t,size_t*),
                    void *cb_value )
{
  return _ksba_reader_set_cb (r, cb, cb_value);
}



gpg_error_t
ksba_reader_read (ksba_reader_t r,
                  char *buffer, size_t length, size_t *nread)
{
  return _ksba_reader_read (r, buffer, length, nread);
}


gpg_error_t
ksba_reader_unread (ksba_reader_t r, const void *buffer, size_t count)
{
  return _ksba_reader_unread (r, buffer, count);
}


unsigned long
ksba_reader_tell (ksba_reader_t r)
{
  return _ksba_reader_tell (r);
}



/*-- writer.c --*/
gpg_error_t
ksba_writer_new (ksba_writer_t *r_w)
{
  return _ksba_writer_new (r_w);
}


void
ksba_writer_release (ksba_writer_t w)
{
  _ksba_writer_release (w);
}


int
ksba_writer_error (ksba_writer_t w)
{
  return _ksba_writer_error (w);
}


unsigned long
ksba_writer_tell (ksba_writer_t w)
{
  return _ksba_writer_tell (w);
}


gpg_error_t
ksba_writer_set_fd (ksba_writer_t w, int fd)
{
  return _ksba_writer_set_fd (w, fd);
}


gpg_error_t
ksba_writer_set_file (ksba_writer_t w, FILE *fp)
{
  return _ksba_writer_set_file (w, fp);
}


gpg_error_t
ksba_writer_set_cb (ksba_writer_t w, 
                    int (*cb)(void*,const void *,size_t),
                    void *cb_value)
{
  return _ksba_writer_set_cb (w, cb, cb_value);
}


gpg_error_t
ksba_writer_set_mem (ksba_writer_t w, size_t initial_size)
{
  return _ksba_writer_set_mem (w, initial_size);
}


const void *
ksba_writer_get_mem (ksba_writer_t w, size_t *nbytes)
{
  return _ksba_writer_get_mem (w, nbytes);
}


void *
ksba_writer_snatch_mem (ksba_writer_t w, size_t *nbytes)
{
  return _ksba_writer_snatch_mem (w, nbytes);
}


gpg_error_t
ksba_writer_set_filter (ksba_writer_t w, 
                        gpg_error_t (*filter)(void*,
                                              const void *,size_t, size_t *,
                                              void *, size_t, size_t *),
                        void *filter_arg)
{
  return _ksba_writer_set_filter (w, filter, filter_arg);
}



gpg_error_t
ksba_writer_write (ksba_writer_t w, const void *buffer, size_t length)
{
  return _ksba_writer_write (w, buffer, length);
}


gpg_error_t
ksba_writer_write_octet_string (ksba_writer_t w,
                                const void *buffer, size_t length,
                                int flush)
{
  return _ksba_writer_write_octet_string (w, buffer, length, flush);
}



/*-- asn1-parse.y --*/
int 
ksba_asn_parse_file (const char *filename, ksba_asn_tree_t *result,
                     int debug)
{
  return _ksba_asn_parse_file (filename, result, debug);
}


void
ksba_asn_tree_release (ksba_asn_tree_t tree)
{
  _ksba_asn_tree_release (tree);
}


/*-- asn1-func.c --*/
void
ksba_asn_tree_dump (ksba_asn_tree_t tree, const char *name, FILE *fp)
{
  _ksba_asn_tree_dump (tree, name, fp);
}


gpg_error_t
ksba_asn_create_tree (const char *mod_name, ksba_asn_tree_t *result)
{
  return _ksba_asn_create_tree (mod_name, result);
}


/* This is a dummy function which we only include because it was
   accidently put into the public interface.  */
int
ksba_asn_delete_structure (void *dummy)
{
  (void)dummy;
  fprintf (stderr, "BUG: ksba_asn_delete_structure called\n");
  return -1;
}


/*-- oid.c --*/
char *
ksba_oid_to_str (const char *buffer, size_t length)
{
  return _ksba_oid_to_str (buffer, length);
}


gpg_error_t
ksba_oid_from_str (const char *string,
                   unsigned char **rbuf, size_t *rlength)
{
  return _ksba_oid_from_str (string, rbuf, rlength);
}



/*-- dn.c --*/
gpg_error_t
ksba_dn_der2str (const void *der, size_t derlen, char **r_string)
{
  return _ksba_dn_der2str (der, derlen, r_string);
}


gpg_error_t
ksba_dn_str2der (const char *string,
                 unsigned char **rder, size_t *rderlen)
{
  return _ksba_dn_str2der (string, rder, rderlen);
}


gpg_error_t
ksba_dn_teststr (const char *string, int seq, 
                 size_t *rerroff, size_t *rerrlen)
{
  return _ksba_dn_teststr (string, seq, rerroff, rerrlen);
}




/*-- name.c --*/
gpg_error_t
ksba_name_new (ksba_name_t *r_name)
{
  return _ksba_name_new (r_name);
}


void
ksba_name_ref (ksba_name_t name)
{
  _ksba_name_ref (name);
}


void
ksba_name_release (ksba_name_t name)
{
  _ksba_name_release (name);
}


const char *
ksba_name_enum (ksba_name_t name, int idx)
{
  return _ksba_name_enum (name, idx);
}


char *
ksba_name_get_uri (ksba_name_t name, int idx)
{
  return _ksba_name_get_uri (name, idx);
}


