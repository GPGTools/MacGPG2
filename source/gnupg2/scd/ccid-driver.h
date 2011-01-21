/* ccid-driver.c - USB ChipCardInterfaceDevices driver
 *	Copyright (C) 2003 Free Software Foundation, Inc.
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
 * ALTERNATIVELY, this file may be distributed under the terms of the
 * following license, in which case the provisions of this license are
 * required INSTEAD OF the GNU General Public License. If you wish to
 * allow use of your version of this file only under the terms of the
 * GNU General Public License, and not to allow others to use your
 * version of this file under the terms of the following license,
 * indicate your decision by deleting this paragraph and the license
 * below.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, and the entire permission notice in its entirety,
 *    including the disclaimer of warranties.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id$
 */

#ifndef CCID_DRIVER_H
#define CCID_DRIVER_H

/* The CID driver returns the same error codes as the status words
   used by GnuPG's apdu.h.  For ease of maintenance they should always
   match.  */
#define CCID_DRIVER_ERR_OUT_OF_CORE    0x10001 
#define CCID_DRIVER_ERR_INV_VALUE      0x10002
#define CCID_DRIVER_ERR_INCOMPLETE_CARD_RESPONSE = 0x10003
#define CCID_DRIVER_ERR_NO_DRIVER      0x10004
#define CCID_DRIVER_ERR_NOT_SUPPORTED  0x10005
#define CCID_DRIVER_ERR_LOCKING_FAILED 0x10006
#define CCID_DRIVER_ERR_BUSY           0x10007
#define CCID_DRIVER_ERR_NO_CARD        0x10008
#define CCID_DRIVER_ERR_CARD_INACTIVE  0x10009
#define CCID_DRIVER_ERR_CARD_IO_ERROR  0x1000a
#define CCID_DRIVER_ERR_GENERAL_ERROR  0x1000b
#define CCID_DRIVER_ERR_NO_READER      0x1000c
#define CCID_DRIVER_ERR_ABORTED        0x1000d
#define CCID_DRIVER_ERR_NO_KEYPAD      0x1000e

struct ccid_driver_s;
typedef struct ccid_driver_s *ccid_driver_t;

int ccid_set_debug_level (int level);
char *ccid_get_reader_list (void);
int ccid_open_reader (ccid_driver_t *handle, const char *readerid);
int ccid_set_progress_cb (ccid_driver_t handle, 
                          void (*cb)(void *, const char *, int, int, int),
                          void *cb_arg);
int ccid_shutdown_reader (ccid_driver_t handle);
int ccid_close_reader (ccid_driver_t handle);
int ccid_get_atr (ccid_driver_t handle,
                  unsigned char *atr, size_t maxatrlen, size_t *atrlen);
int ccid_slot_status (ccid_driver_t handle, int *statusbits);
int ccid_transceive (ccid_driver_t handle,
                     const unsigned char *apdu, size_t apdulen,
                     unsigned char *resp, size_t maxresplen, size_t *nresp);
int ccid_transceive_secure (ccid_driver_t handle,
                     const unsigned char *apdu, size_t apdulen,
                     int pin_mode, 
                     int pinlen_min, int pinlen_max, int pin_padlen, 
                     unsigned char *resp, size_t maxresplen, size_t *nresp);
int ccid_transceive_escape (ccid_driver_t handle,
                            const unsigned char *data, size_t datalen,
                            unsigned char *resp, size_t maxresplen,
                            size_t *nresp);



#endif /*CCID_DRIVER_H*/



