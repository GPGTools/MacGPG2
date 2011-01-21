/* Output of mkstrtable.awk.  DO NOT EDIT.  */

/* status.h - Status codes
 *	Copyright (C) 2007 Free Software Foundation, Inc.
 *
 * This file is part of GnuPG.
 *
 * GnuPG is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * GnuPG is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

/* The purpose of this complex string table is to produce
   optimal code with a minimum of relocations.  */

static const char statusstr_msgstr[] = 
  "ENTER" "\0"
  "LEAVE" "\0"
  "ABORT" "\0"
  "GOODSIG" "\0"
  "BADSIG" "\0"
  "ERRSIG" "\0"
  "BADARMOR" "\0"
  "RSA_OR_IDEA" "\0"
  "TRUST_UNDEFINED" "\0"
  "TRUST_NEVER" "\0"
  "TRUST_MARGINAL" "\0"
  "TRUST_FULLY" "\0"
  "TRUST_ULTIMATE" "\0"
  "NEED_PASSPHRASE" "\0"
  "VALIDSIG" "\0"
  "SIG_ID" "\0"
  "ENC_TO" "\0"
  "NODATA" "\0"
  "BAD_PASSPHRASE" "\0"
  "NO_PUBKEY" "\0"
  "NO_SECKEY" "\0"
  "NEED_PASSPHRASE_SYM" "\0"
  "DECRYPTION_FAILED" "\0"
  "DECRYPTION_OKAY" "\0"
  "MISSING_PASSPHRASE" "\0"
  "GOOD_PASSPHRASE" "\0"
  "GOODMDC" "\0"
  "BADMDC" "\0"
  "ERRMDC" "\0"
  "IMPORTED" "\0"
  "IMPORT_OK" "\0"
  "IMPORT_PROBLEM" "\0"
  "IMPORT_RES" "\0"
  "IMPORT_CHECK" "\0"
  "FILE_START" "\0"
  "FILE_DONE" "\0"
  "FILE_ERROR" "\0"
  "BEGIN_DECRYPTION" "\0"
  "END_DECRYPTION" "\0"
  "BEGIN_ENCRYPTION" "\0"
  "END_ENCRYPTION" "\0"
  "BEGIN_SIGNING" "\0"
  "DELETE_PROBLEM" "\0"
  "GET_BOOL" "\0"
  "GET_LINE" "\0"
  "GET_HIDDEN" "\0"
  "GOT_IT" "\0"
  "PROGRESS" "\0"
  "SIG_CREATED" "\0"
  "SESSION_KEY" "\0"
  "NOTATION_NAME" "\0"
  "NOTATION_DATA" "\0"
  "POLICY_URL" "\0"
  "BEGIN_STREAM" "\0"
  "END_STREAM" "\0"
  "KEY_CREATED" "\0"
  "USERID_HINT" "\0"
  "UNEXPECTED" "\0"
  "INV_RECP" "\0"
  "INV_SGNR" "\0"
  "NO_RECP" "\0"
  "NO_SGNR" "\0"
  "ALREADY_SIGNED" "\0"
  "KEYEXPIRED" "\0"
  "KEYREVOKED" "\0"
  "SIGEXPIRED" "\0"
  "EXPSIG" "\0"
  "EXPKEYSIG" "\0"
  "ATTRIBUTE" "\0"
  "REVKEYSIG" "\0"
  "NEWSIG" "\0"
  "SIG_SUBPACKET" "\0"
  "PLAINTEXT" "\0"
  "PLAINTEXT_LENGTH" "\0"
  "KEY_NOT_CREATED" "\0"
  "NEED_PASSPHRASE_PIN" "\0"
  "CARDCTRL" "\0"
  "SC_OP_FAILURE" "\0"
  "SC_OP_SUCCESS" "\0"
  "BACKUP_KEY_CREATED" "\0"
  "PKA_TRUST_BAD" "\0"
  "PKA_TRUST_GOOD" "\0"
  "TRUNCATED" "\0"
  "ERROR" "\0"
  "SUCCESS";

static const int statusstr_msgidx[] =
  {
    0,
    6,
    12,
    18,
    26,
    33,
    40,
    49,
    61,
    77,
    89,
    104,
    116,
    131,
    147,
    156,
    163,
    170,
    177,
    192,
    202,
    212,
    232,
    250,
    266,
    285,
    301,
    309,
    316,
    323,
    332,
    342,
    357,
    368,
    381,
    392,
    402,
    413,
    430,
    445,
    462,
    477,
    491,
    506,
    515,
    524,
    535,
    542,
    551,
    563,
    575,
    589,
    603,
    614,
    627,
    638,
    650,
    662,
    673,
    682,
    691,
    699,
    707,
    722,
    733,
    744,
    755,
    762,
    772,
    782,
    792,
    799,
    813,
    823,
    840,
    856,
    876,
    885,
    899,
    913,
    932,
    946,
    961,
    971,
    977,
    
  };

#define statusstr_msgidxof(code) (0 ? -1 \
  : ((code >= 0) && (code <= 84)) ? (code - 0) \
  : -1)
