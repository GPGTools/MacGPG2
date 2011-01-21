/* apdu.h - ISO 7816 APDU functions and low level I/O
 * Copyright (C) 2003, 2008 Free Software Foundation, Inc.
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
 *
 * $Id$
 */

#ifndef APDU_H
#define APDU_H

/* ISO 7816 values for the statusword are defined here because they
   should not be visible to the users of the actual ISO command
   API. */
enum {
  SW_MORE_DATA      = 0x6100, /* Note: that the low byte must be
                                 masked of.*/
  SW_EOF_REACHED    = 0x6282,
  SW_TERM_STATE     = 0x6285, /* Selected file is in termination state.  */
  SW_EEPROM_FAILURE = 0x6581,
  SW_WRONG_LENGTH   = 0x6700,
  SW_SM_NOT_SUP     = 0x6882, /* Secure Messaging is not supported.  */
  SW_CC_NOT_SUP     = 0x6884, /* Command Chaining is not supported.  */
  SW_CHV_WRONG      = 0x6982,
  SW_CHV_BLOCKED    = 0x6983,
  SW_USE_CONDITIONS = 0x6985,
  SW_BAD_PARAMETER  = 0x6a80, /* (in the data field) */
  SW_NOT_SUPPORTED  = 0x6a81,
  SW_FILE_NOT_FOUND = 0x6a82,
  SW_RECORD_NOT_FOUND = 0x6a83,
  SW_BAD_LC         = 0x6a87, /* Lc does not match command or p1/p2.  */
  SW_REF_NOT_FOUND  = 0x6a88,
  SW_BAD_P0_P1      = 0x6b00,
  SW_EXACT_LENGTH   = 0x6c00,
  SW_INS_NOT_SUP    = 0x6d00,
  SW_CLA_NOT_SUP    = 0x6e00,
  SW_SUCCESS        = 0x9000,

  /* The follwoing statuswords are no real ones but used to map host
     OS errors into status words.  A status word is 16 bit so that
     those values can't be issued by a card. */
  SW_HOST_OUT_OF_CORE = 0x10001,  /* No way yet to differentiate
                                     between errnos on a failed malloc. */
  SW_HOST_INV_VALUE     = 0x10002,
  SW_HOST_INCOMPLETE_CARD_RESPONSE = 0x10003,
  SW_HOST_NO_DRIVER     = 0x10004,
  SW_HOST_NOT_SUPPORTED = 0x10005,
  SW_HOST_LOCKING_FAILED= 0x10006,
  SW_HOST_BUSY          = 0x10007,
  SW_HOST_NO_CARD       = 0x10008,
  SW_HOST_CARD_INACTIVE = 0x10009,
  SW_HOST_CARD_IO_ERROR = 0x1000a,
  SW_HOST_GENERAL_ERROR = 0x1000b,
  SW_HOST_NO_READER     = 0x1000c,
  SW_HOST_ABORTED       = 0x1000d,
  SW_HOST_NO_KEYPAD     = 0x1000e,
  SW_HOST_ALREADY_CONNECTED = 0x1000f
};


#define SW_EXACT_LENGTH_P(a) (((a)&~0xff) == SW_EXACT_LENGTH)


/* Bit flags for the card status.  */
#define APDU_CARD_USABLE   (1)    /* Card is present and ready for use.  */
#define APDU_CARD_PRESENT  (2)    /* Card is just present.  */
#define APDU_CARD_ACTIVE   (4)    /* Card is active.  */


/* Note, that apdu_open_reader returns no status word but -1 on error. */
int apdu_open_reader (const char *portstr, int *r_no_service);
int apdu_open_remote_reader (const char *portstr,
                             const unsigned char *cookie, size_t length,
                             int (*readfnc) (void *opaque,
                                             void *buffer, size_t size),
                             void *readfnc_value,
                             int (*writefnc) (void *opaque,
                                              const void *buffer, size_t size),
                             void *writefnc_value,
                             void (*closefnc) (void *opaque),
                             void *closefnc_value);
int apdu_shutdown_reader (int slot);
int apdu_close_reader (int slot);
void apdu_prepare_exit (void);
int apdu_enum_reader (int slot, int *used);
unsigned char *apdu_get_atr (int slot, size_t *atrlen);

const char *apdu_strerror (int rc);


/* These APDU functions return status words. */

int apdu_connect (int slot);
int apdu_disconnect (int slot);

int apdu_set_progress_cb (int slot, gcry_handler_progress_t cb, void *cb_arg);

int apdu_activate (int slot);
int apdu_reset (int slot);
int apdu_get_status (int slot, int hang,
                     unsigned int *status, unsigned int *changed);
int apdu_check_keypad (int slot, int command, int pin_mode,
                       int pinlen_min, int pinlen_max, int pin_padlen);
int apdu_send_simple (int slot, int extended_mode,
                      int class, int ins, int p0, int p1,
                      int lc, const char *data);
int apdu_send_simple_kp (int slot, int class, int ins, int p0, int p1,
                         int lc, const char *data,  
                         int pin_mode,
                         int pinlen_min, int pinlen_max, int pin_padlen);
int apdu_send (int slot, int extended_mode, 
               int class, int ins, int p0, int p1, int lc, const char *data,
               unsigned char **retbuf, size_t *retbuflen);
int apdu_send_le (int slot, int extended_mode, 
                  int class, int ins, int p0, int p1,
                  int lc, const char *data, int le,
                  unsigned char **retbuf, size_t *retbuflen);
int apdu_send_direct (int slot, size_t extended_length,
                      const unsigned char *apdudata, size_t apdudatalen,
                      int handle_more,
                      unsigned char **retbuf, size_t *retbuflen);


#endif /*APDU_H*/



