/* certreq.c - create pkcs-10 messages
 *      Copyright (C) 2002 g10 Code GmbH
 *
 * This file is part of KSBA.
 *
 * KSBA is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Fountion; either version 3 of the License, or
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "util.h"

#include "cms.h"
#include "convert.h"
#include "keyinfo.h"
#include "der-encoder.h"
#include "ber-help.h"
#include "certreq.h"

static const char oidstr_subjectAltName[] = "2.5.29.17";
static const char oidstr_extensionReq[] = "1.2.840.113549.1.9.14";



/**
 * ksba_cms_new:
 * 
 * Create a new and empty CMS object
 * 
 * Return value: A CMS object or an error code.
 **/
gpg_error_t
ksba_certreq_new (ksba_certreq_t *r_cr)
{
  *r_cr = xtrycalloc (1, sizeof **r_cr);
  if (!*r_cr)
    return gpg_error_from_errno (errno);

  return 0;
}

/**
 * ksba_certreq_release:
 * @cms: A Certreq object
 * 
 * Release a Certreq object.
 **/
void
ksba_certreq_release (ksba_certreq_t cr)
{
  if (!cr)
    return;
  xfree (cr->subject.der);
  xfree (cr->key.der);
  xfree (cr->cri.der);
  xfree (cr->sig_val.algo);
  xfree (cr->sig_val.value);
  while (cr->subject_alt_names)
    {
      struct general_names_s *tmp = cr->subject_alt_names->next;
      xfree (cr->subject_alt_names);
      cr->subject_alt_names = tmp;
    }
  while (cr->extn_list)
    {
      struct extn_list_s *e = cr->extn_list->next;
      xfree (cr->extn_list);
      cr->extn_list = e;
    }

  xfree (cr);
}


gpg_error_t
ksba_certreq_set_writer (ksba_certreq_t cr, ksba_writer_t w)
{
  if (!cr || !w)
    return gpg_error (GPG_ERR_INV_VALUE);
  cr->writer = w;
  return 0;
}


/* Provide a hash function so that we are able to hash the data */
void
ksba_certreq_set_hash_function (ksba_certreq_t cr,
                                void (*hash_fnc)(void *, const void *, size_t),
                                void *hash_fnc_arg)
{
  if (cr)
    {
      cr->hash_fnc = hash_fnc;
      cr->hash_fnc_arg = hash_fnc_arg;
    }
}


/* Store the subject's name.  Does perform some syntactic checks on
   the name.  The first added subject is the real one, all subsequent
   calls add subjectAltNames.
   
   NAME must be a valid RFC-2253 encoded DN name for the first one or an
   email address enclosed in angle brackets for all further calls.
 */
gpg_error_t
ksba_certreq_add_subject (ksba_certreq_t cr, const char *name)
{
  unsigned long namelen;
  size_t n, n1;
  struct general_names_s *gn;
  unsigned char *der;
  int tag;
  const char *endp;


  if (!cr || !name)
    return gpg_error (GPG_ERR_INV_VALUE);
  if (!cr->subject.der)
    return _ksba_dn_from_str (name, &cr->subject.der, &cr->subject.derlen);
  /* This is assumed to be an subjectAltName. */

  /* Note that the way we pass the name should match what
     ksba_cert_get_subject() returns.  In particular we expect that it
     is a real string and thus a canonical S-expression is
     additionally terminated by a 0. */
  namelen = strlen (name);
  if (*name == '<' && name[namelen-1] == '>'
      && namelen >= 4 && strchr (name, '@'))
    {
      name++;
      namelen -= 2;
      tag = 1;  /* rfc822Name */
    }
  else if (!strncmp (name, "(8:dns-name", 11))
    {
      tag = 2; /* dNSName */
      namelen = strtoul (name+11, (char**)&endp, 10);
      name = endp;
      if (!namelen || *name != ':')
        return gpg_error (GPG_ERR_INV_SEXP); 
      name++;
    }
  else if (!strncmp (name, "(3:uri", 6))
    {
      tag = 6; /* uRI */
      namelen = strtoul (name+6, (char**)&endp, 10);
      name = endp;
      if (!namelen || *name != ':')
        return gpg_error (GPG_ERR_INV_SEXP); 
      name++;
    }
  else
    return gpg_error (GPG_ERR_INV_VALUE);

  n1  = _ksba_ber_count_tl (tag, CLASS_CONTEXT, 0, namelen);
  n1 += namelen;
  
  gn = xtrymalloc (sizeof *gn + n1 - 1);
  if (!gn)
    return gpg_error_from_errno (errno);
  gn->tag = tag;
  gn->datalen = n1;
  der = (unsigned char *)gn->data;
  n = _ksba_ber_encode_tl (der, tag, CLASS_CONTEXT, 0, namelen);
  if (!n)
    return gpg_error (GPG_ERR_BUG); 
  der += n;
  memcpy (der, name, namelen);
  assert (der + namelen - (unsigned char*)gn->data == n1);
  
  gn->next = cr->subject_alt_names;
  cr->subject_alt_names = gn;

  return 0;
}


/* Add the GeneralNames object GNAMES to the list of extensions in CR.
   Use OID as object identifier for the extensions. */
static gpg_error_t
add_general_names_to_extn (ksba_certreq_t cr, struct general_names_s *gnames,
                           const char *oid)
{
  struct general_names_s *g;
  size_t n, n1, n2;
  struct extn_list_s *e;
  unsigned char *der;

  /* Calculate the required size. */
  n1 = 0;
  for (g=gnames; g; g = g->next)
    n1 += g->datalen;

  n2  = _ksba_ber_count_tl (TYPE_SEQUENCE, CLASS_UNIVERSAL, 1, n1);
  n2 += n1;

  /* Allocate memory and encode all. */
  e = xtrymalloc (sizeof *e + n2 - 1);
  if (!e)
    return gpg_error_from_errno (errno);
  e->oid = oid;
  e->critical = 0;
  e->derlen = n2;
  der = e->der;
  n = _ksba_ber_encode_tl (der, TYPE_SEQUENCE, CLASS_UNIVERSAL, 1, n1);
  if (!n)
    return gpg_error (GPG_ERR_BUG); /* (no need to cleanup after a bug) */
  der += n;

  for (g=gnames; g; g = g->next)
    {
      memcpy (der, g->data, g->datalen);
      der += g->datalen;
    }
  assert (der - e->der == n2);
  
  e->next = cr->extn_list;
  cr->extn_list = e;
  return 0;
}

/* Store the subject's publickey. */
gpg_error_t
ksba_certreq_set_public_key (ksba_certreq_t cr, ksba_const_sexp_t key)
{
  if (!cr)
    return gpg_error (GPG_ERR_INV_VALUE);
  xfree (cr->key.der);
  cr->key.der = NULL;
  return _ksba_keyinfo_from_sexp (key, &cr->key.der, &cr->key.derlen);
}


/* Generic function to add an extension to a certificate request.  The
   extension must be provided readily encoded in the buffer DER of
   length DERLEN bytes; the OID is to be given in OID and IS_CRIT
   should be set to true if that extension shall be marked
   critical. */
gpg_error_t
ksba_certreq_add_extension (ksba_certreq_t cr,
                            const char *oid, int is_crit,
                            const void *der, size_t derlen)
{
  size_t oidlen;
  struct extn_list_s *e;

  if (!cr || !oid|| !*oid || !der || !derlen)
    return gpg_error (GPG_ERR_INV_VALUE);

  oidlen = strlen (oid);
  e = xtrymalloc (sizeof *e + derlen + oidlen);
  if (!e)
    return gpg_error_from_errno (errno);
  e->critical = is_crit;
  e->derlen = derlen;
  memcpy (e->der, der, derlen);
  strcpy (e->der+derlen, oid);
  e->oid = e->der + derlen;

  e->next = cr->extn_list;
  cr->extn_list = e;

  return 0;
}




/*
 * r_sig  = (sig-val
 *	      (<algo>
 *		(<param_name1> <mpi>)
 *		...
 *		(<param_namen> <mpi>)
 *	      ))
 * The sexp must be in canocial form. 
 * Fixme:  The code is mostly duplicated from cms.c
 * Note, that <algo> must be given as a stringified OID or the special
 * string "rsa" which is translated to sha1WithRSAEncryption
*/
gpg_error_t
ksba_certreq_set_sig_val (ksba_certreq_t cr, ksba_const_sexp_t sigval)
{
  const char *s, *endp;
  unsigned long n;

  if (!cr)
    return gpg_error (GPG_ERR_INV_VALUE);

  s = sigval;
  if (*s != '(')
    return gpg_error (GPG_ERR_INV_SEXP);
  s++;

  n = strtoul (s, (char**)&endp, 10);
  s = endp;
  if (!n || *s!=':')
    return gpg_error (GPG_ERR_INV_SEXP); /* we don't allow empty lengths */
  s++;
  if (n != 7 || memcmp (s, "sig-val", 7))
    return gpg_error (GPG_ERR_UNKNOWN_SEXP);
  s += 7;
  if (*s != '(')
    return gpg_error (digitp (s)? GPG_ERR_UNKNOWN_SEXP : GPG_ERR_INV_SEXP);
  s++;

  /* break out the algorithm ID */
  n = strtoul (s, (char**)&endp, 10);
  s = endp;
  if (!n || *s != ':')
    return gpg_error (GPG_ERR_INV_SEXP); /* we don't allow empty lengths */
  s++;
  xfree (cr->sig_val.algo);
  if (n==3 && s[0] == 'r' && s[1] == 's' && s[2] == 'a')
    { /* kludge to allow "rsa" to be passed as algorithm name */
      cr->sig_val.algo = xtrystrdup ("1.2.840.113549.1.1.5");
      if (!cr->sig_val.algo)
        return gpg_error (GPG_ERR_ENOMEM);
    }
  else
    {
      cr->sig_val.algo = xtrymalloc (n+1);
      if (!cr->sig_val.algo)
        return gpg_error (GPG_ERR_ENOMEM);
      memcpy (cr->sig_val.algo, s, n);
      cr->sig_val.algo[n] = 0;
    }
  s += n;

  /* And now the values - FIXME: For now we only support one */
  /* fixme: start loop */
  if (*s != '(')
    return gpg_error (digitp (s)? GPG_ERR_UNKNOWN_SEXP : GPG_ERR_INV_SEXP);
  s++;
  n = strtoul (s, (char**)&endp, 10);
  s = endp;
  if (!n || *s != ':')
    return gpg_error (GPG_ERR_INV_SEXP); 
  s++;
  s += n; /* ignore the name of the parameter */
  
  if (!digitp(s))
    return gpg_error (GPG_ERR_UNKNOWN_SEXP); /* but may also be an invalid one */
  n = strtoul (s, (char**)&endp, 10);
  s = endp;
  if (!n || *s != ':')
    return gpg_error (GPG_ERR_INV_SEXP); 
  s++;
  if (n > 1 && !*s)
    { /* We might have a leading zero due to the way we encode 
         MPIs - this zero should not go into the BIT STRING.  */
      s++;
      n--;
    }
  xfree (cr->sig_val.value);
  cr->sig_val.value = xtrymalloc (n);
  if (!cr->sig_val.value)
    return gpg_error (GPG_ERR_ENOMEM);
  memcpy (cr->sig_val.value, s, n);
  cr->sig_val.valuelen = n;
  s += n;
  if ( *s != ')')
    return gpg_error (GPG_ERR_UNKNOWN_SEXP); /* but may also be an invalid one */
  s++;
  /* fixme: end loop over parameters */

  /* we need 2 closing parenthesis */
  if ( *s != ')' || s[1] != ')')
    return gpg_error (GPG_ERR_INV_SEXP); 

  return 0;
}



/* Build the extension block and return it in R_DER and R_DERLEN */
static gpg_error_t
build_extensions (ksba_certreq_t cr, void **r_der, size_t *r_derlen)
{
  gpg_error_t err;
  ksba_writer_t writer, w=NULL;
  struct extn_list_s *e;
  unsigned char *value = NULL;
  size_t valuelen;
  unsigned char *p;
  size_t n;

  *r_der = NULL;
  *r_derlen = 0;
  err = ksba_writer_new (&writer);
  if (err)
    goto leave;
  err = ksba_writer_set_mem (writer, 2048);
  if (err)
    goto leave;
  err = ksba_writer_new (&w);
  if (err)
    goto leave;

  for (e=cr->extn_list; e; e = e->next)
    {
      err = ksba_writer_set_mem (w, e->derlen + 100);
      if (err)
        goto leave;

      err = ksba_oid_from_str (e->oid, &p, &n);
      if(err)
        goto leave;
      err = _ksba_ber_write_tl (w, TYPE_OBJECT_ID, CLASS_UNIVERSAL, 0, n);
      if (!err)
        err = ksba_writer_write (w, p, n);
      xfree (p);
      
      if (e->critical)
        {
          err = _ksba_ber_write_tl (w, TYPE_BOOLEAN, CLASS_UNIVERSAL, 0, 1);
          if (!err)
            err = ksba_writer_write (w, "\xff", 1);
          if(err)
            goto leave;
        }

      err = _ksba_ber_write_tl (w, TYPE_OCTET_STRING, CLASS_UNIVERSAL,
                                0, e->derlen);
      if (!err)
        err = ksba_writer_write (w, e->der, e->derlen);
      if(err)
        goto leave;
      
      p = ksba_writer_snatch_mem (w, &n);
      if (!p)
        {
          err = gpg_error (GPG_ERR_ENOMEM);
          goto leave;
        }
      err = _ksba_ber_write_tl (writer, TYPE_SEQUENCE, CLASS_UNIVERSAL,
                                1, n);
      if (!err)
        err = ksba_writer_write (writer, p, n);
      xfree (p); p = NULL;
      if (err)
        goto leave;
    }

  /* Embed all the sequences into another sequence */
  value = ksba_writer_snatch_mem (writer, &valuelen);
  if (!value)
    {
      err = gpg_error (GPG_ERR_ENOMEM);
      goto leave;
    }
  err = ksba_writer_set_mem (writer, valuelen+10);
  if (err)
    goto leave;
  err = _ksba_ber_write_tl (writer, TYPE_SEQUENCE, CLASS_UNIVERSAL,
                            1, valuelen);
  if (!err)
    err = ksba_writer_write (writer, value, valuelen);
  if (err)
    goto leave;

  xfree (value);
  value = ksba_writer_snatch_mem (writer, &valuelen);
  if (!value)
    {
      err = gpg_error (GPG_ERR_ENOMEM);
      goto leave;
    }

  /* Now create the extension request sequence content */
  err = ksba_writer_set_mem (writer, valuelen+100);
  if (err)
    goto leave;
  err = ksba_oid_from_str (oidstr_extensionReq, &p, &n);
  if(err)
    goto leave;
  err = _ksba_ber_write_tl (writer, TYPE_OBJECT_ID, CLASS_UNIVERSAL, 0, n);
  if (!err)
    err = ksba_writer_write (writer, p, n);
  xfree (p); p = NULL;
  if (err)
    return err;
  err = _ksba_ber_write_tl (writer, TYPE_SET, CLASS_UNIVERSAL, 1, valuelen);
  if (!err)
    err = ksba_writer_write (writer, value, valuelen);

  /* put this all into a SEQUENCE */
  xfree (value);
  value = ksba_writer_snatch_mem (writer, &valuelen);
  if (!value)
    {
      err = gpg_error (GPG_ERR_ENOMEM);
      goto leave;
    }
  err = ksba_writer_set_mem (writer, valuelen+10);
  if (err)
    goto leave;
  err = _ksba_ber_write_tl (writer, TYPE_SEQUENCE, CLASS_UNIVERSAL,
                            1, valuelen);
  if (!err)
    err = ksba_writer_write (writer, value, valuelen);
  if (err)
    goto leave;

  xfree (value);
  value = ksba_writer_snatch_mem (writer, &valuelen);
  if (!value)
    {
      err = gpg_error (GPG_ERR_ENOMEM);
      goto leave;
    }
  *r_der = value;
  *r_derlen = valuelen;
  value = NULL;


 leave:
  ksba_writer_release (writer);
  ksba_writer_release (w);
  xfree (value);
  return err;
}


/* Build a value tree from the already stored values. */
static gpg_error_t
build_cri (ksba_certreq_t cr)
{
  gpg_error_t err;
  ksba_writer_t writer;
  void *value = NULL;
  size_t valuelen;

  err = ksba_writer_new (&writer);
  if (err)
    goto leave;
  err = ksba_writer_set_mem (writer, 2048);
  if (err)
    goto leave;

  /* We write all stuff out to a temporary writer object, then use
     this object to create the cri and store the cri image */

  /* store version v1 (which is a 0) */
  err = _ksba_ber_write_tl (writer, TYPE_INTEGER, CLASS_UNIVERSAL, 0, 1);
  if (!err)
    err = ksba_writer_write (writer, "", 1);
  if (err)
    goto leave;

  /* store the subject */
  if (!cr->subject.der)
    {
      err = gpg_error (GPG_ERR_MISSING_VALUE);
      goto leave;
    }
  err = ksba_writer_write (writer, cr->subject.der, cr->subject.derlen);
  if (err)
    goto leave;

  /* store the public key info */
  if (!cr->key.der)
    {
      err = gpg_error (GPG_ERR_MISSING_VALUE);
      goto leave;
    }
  err = ksba_writer_write (writer, cr->key.der, cr->key.derlen);
  if (err)
    goto leave;

  /* Copy generalNames objects to the extension list. */
  if (cr->subject_alt_names)
    {
      err = add_general_names_to_extn (cr, cr->subject_alt_names, 
                                       oidstr_subjectAltName);
      if (err)
        goto leave;
      while (cr->subject_alt_names)
        {
          struct general_names_s *tmp = cr->subject_alt_names->next;
          xfree (cr->subject_alt_names);
          cr->subject_alt_names = tmp;
        }
      cr->subject_alt_names = NULL;
    }
  

  /* Write the extensions.  Note that the implicit SET OF is REQUIRED */
  xfree (value); value = NULL;
  valuelen = 0;
  if (cr->extn_list)
    {
      err = build_extensions (cr, &value, &valuelen);
      if (err)
        goto leave;
      err = _ksba_ber_write_tl (writer, 0, CLASS_CONTEXT, 1, valuelen);
      if (!err)
        err = ksba_writer_write (writer, value, valuelen);
      if (err)
        goto leave;
    }
  else
    { /* We can't write an object of length zero using our ber_write
         function.  So we must open encode it. */
      err = ksba_writer_write (writer, "\xa0\x02\x30", 4);
      if (err)
        goto leave;
    }


  /* pack it into the sequence */
  xfree (value); 
  value = ksba_writer_snatch_mem (writer, &valuelen);
  if (!value)
    {
      err = gpg_error (GPG_ERR_ENOMEM);
      goto leave;
    }
  /* reinitialize the buffer to create the outer sequence */
  err = ksba_writer_set_mem (writer, valuelen+10);
  if (err)
    goto leave;
  /* write outer sequence */
  err = _ksba_ber_write_tl (writer, TYPE_SEQUENCE, CLASS_UNIVERSAL,
                            1, valuelen);
  if (!err)
    err = ksba_writer_write (writer, value, valuelen);
  if (err)
    goto leave;
  
  /* and store the final result */
  cr->cri.der = ksba_writer_snatch_mem (writer, &cr->cri.derlen);
  if (!cr->cri.der)
    err = gpg_error (GPG_ERR_ENOMEM);

 leave:
  ksba_writer_release (writer);
  xfree (value);
  return err;
}

static gpg_error_t
hash_cri (ksba_certreq_t cr)
{
  if (!cr->hash_fnc)
    return gpg_error (GPG_ERR_MISSING_ACTION);
  if (!cr->cri.der)
    return gpg_error (GPG_ERR_INV_STATE);
  cr->hash_fnc (cr->hash_fnc_arg, cr->cri.der, cr->cri.derlen);
  return 0;
}


/* The user has calculated the signatures and we can now write
   the signature */
static gpg_error_t 
sign_and_write (ksba_certreq_t cr) 
{
  gpg_error_t err;
  ksba_writer_t writer;
  void *value = NULL;
  size_t valuelen;

  err = ksba_writer_new (&writer);
  if (err)
    goto leave;
  err = ksba_writer_set_mem (writer, 2048);
  if (err)
    goto leave;

  /* store the cri */
  if (!cr->cri.der)
    {
      err = gpg_error (GPG_ERR_MISSING_VALUE);
      goto leave;
    }
  err = ksba_writer_write (writer, cr->cri.der, cr->cri.derlen);
  if (err) 
    goto leave;
  
  /* store the signatureAlgorithm */
  if (!cr->sig_val.algo)
    return gpg_error (GPG_ERR_MISSING_VALUE);
  err = _ksba_der_write_algorithm_identifier (writer, 
                                              cr->sig_val.algo, NULL, 0);
  if (err) 
    goto leave;

  /* write the signature */
  err = _ksba_ber_write_tl (writer, TYPE_BIT_STRING, CLASS_UNIVERSAL, 0,
                            1 + cr->sig_val.valuelen);
  if (!err)
    err = ksba_writer_write (writer, "", 1);
  if (!err)
    err = ksba_writer_write (writer, cr->sig_val.value, cr->sig_val.valuelen);
  if (err)
    goto leave;

  /* pack it into the outer sequence */
  value = ksba_writer_snatch_mem (writer, &valuelen);
  if (!value)
    {
      err = gpg_error (GPG_ERR_ENOMEM);
      goto leave;
    }
  err = ksba_writer_set_mem (writer, valuelen+10);
  if (err)
    goto leave;
  /* write outer sequence */
  err = _ksba_ber_write_tl (writer, TYPE_SEQUENCE, CLASS_UNIVERSAL,
                            1, valuelen);
  if (!err)
    err = ksba_writer_write (writer, value, valuelen);
  if (err)
    goto leave;

  /* and finally write the result */
  xfree (value);
  value = ksba_writer_snatch_mem (writer, &valuelen);
  if (!value)
    err = gpg_error (GPG_ERR_ENOMEM);
  else if (!cr->writer)
    err = gpg_error (GPG_ERR_MISSING_ACTION);
  else
    err = ksba_writer_write (cr->writer, value, valuelen);

 leave:
  ksba_writer_release (writer);
  xfree (value);
  return err;
}



/* The main function to build a certificate request.  It used used in
   a loop so allow for interaction between the function and the caller */
gpg_error_t
ksba_certreq_build (ksba_certreq_t cr, ksba_stop_reason_t *r_stopreason)
{
  enum { 
    sSTART,
    sHASHING,
    sGOTSIG,
    sERROR
  } state = sERROR;
  gpg_error_t err = 0;
  ksba_stop_reason_t stop_reason;

  if (!cr || !r_stopreason)
    return gpg_error (GPG_ERR_INV_VALUE);

  if (!cr->any_build_done)
    { /* first time initialization of the stop reason */
      *r_stopreason = 0;
      cr->any_build_done = 1;
    }

  /* Calculate state from last reason */
  stop_reason = *r_stopreason;
  *r_stopreason = KSBA_SR_RUNNING;
  switch (stop_reason)
    {
    case 0:
      state = sSTART;
      break;
    case KSBA_SR_NEED_HASH:
      state = sHASHING;
      break;
    case KSBA_SR_NEED_SIG:
      if (!cr->sig_val.algo)
        err = gpg_error (GPG_ERR_MISSING_ACTION); 
      else
        state = sGOTSIG;
      break;
    case KSBA_SR_RUNNING:
      err = gpg_error (GPG_ERR_INV_STATE);
      break;
    default:
      err = gpg_error (GPG_ERR_BUG);
      break;
    }
  if (err)
    return err;

  /* Do the action */
  switch (state)
    {
    case sSTART:
      err = build_cri (cr);
      break;
    case sHASHING:
      err = hash_cri (cr);
      break;
    case sGOTSIG:
      err = sign_and_write (cr);
      break;
    default:
      err = gpg_error (GPG_ERR_INV_STATE);
      break;
    }
  if (err)
    return err;

  /* Calculate new stop reason */
  switch (state)
    {
    case sSTART:
      stop_reason = KSBA_SR_NEED_HASH; /* caller should set the hash function*/
      break;
    case sHASHING:
      stop_reason = KSBA_SR_NEED_SIG;
      break;
    case sGOTSIG:
      stop_reason = KSBA_SR_READY;
      break;
    default:
      break;
    }
    
  *r_stopreason = stop_reason;
  return 0;
}
