/* ce-createpipe.c - Test the W32CE CreatePipe implementation.
   Copyright (C) 2010 Free Software Foundation, Inc.

   This file is part of Assuan.

   Assuan is free software; you can redistribute it and/or modify it
   under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 3 of
   the License, or (at your option) any later version.

   Assuan is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "common.h"

#include "../src/assuan.h" /* We need the CreatePipe prototype.  */


static DWORD
reader_thread (void *arg)
{
  HANDLE hd = arg;
  DWORD nread;
  char buffer[16];

  for (;;)
    {
      if (!ReadFile (hd, buffer, sizeof buffer, &nread, FALSE))
	{
	  log_error ("reader: ReadFile failed: rc=%d\n", (int)GetLastError ());
	  break;
	}
      log_info ("reader: read %d bytes\n", (int)nread);
      log_printhex ("got: ", buffer, nread);
    }

  log_info ("reader: finished\n");
  CloseHandle (hd);
  return 0;
}


static DWORD
writer_thread (void *arg)
{
  HANDLE hd = arg;
  DWORD nwritten;
  int i = 0;
  int j;
  char buffer[20];
  int count;

  for (count=0; count < 30; count++)
    {
      for (j=0; j < sizeof buffer; j++)
        buffer[j] = i++;

      if (!WriteFile (hd, buffer, sizeof buffer, &nwritten, NULL))
	{
	  log_error ("writer: WriteFile failed: rc=%d\n", (int)GetLastError ());
	  break;
	}
      if (nwritten != sizeof buffer)
        log_info ("writer: wrote only %d bytes\n", (int)nwritten);
    }

  log_info ("writer: finished\n");
  CloseHandle (hd);
  return 0;
}



static void
run_test (void)
{
  HANDLE hd[2];
  HANDLE threads[2];

  if (!CreatePipe (&hd[0], &hd[1], NULL, 0))
    {
      log_error ("CreatePipe failed: rc=%d\n", (int)GetLastError ());
      return;
    }
  log_info ("pipe created read=%p write=%p\n", hd[0], hd[1]);
  
  threads[0] = CreateThread (NULL, 0, reader_thread, hd[0], 0, NULL);
  if (!threads[0])
    log_fatal ("error creating reader thread: rc=%d\n", (int)GetLastError ());
  else
    log_info ("reader thread created\n");
  threads[1] = CreateThread (NULL, 0, writer_thread, hd[1], 0, NULL);
  if (!threads[0])
    log_fatal ("error creating writer thread: rc=%d\n", (int)GetLastError ());
  else
    log_info ("writer thread created\n");

  switch (WaitForMultipleObjects (2, threads, FALSE, INFINITE))
    {
    case WAIT_OBJECT_0:
      log_info ("reader thread finished first\n");
      break;
    case WAIT_OBJECT_0 + 1:
      log_info ("writer thread finished first\n");
      break;
    default:
      log_error ("WFMO failed: rc=%d\n", (int)GetLastError ());
      break;
    }

  

  CloseHandle (threads[0]);
  CloseHandle (threads[1]);
}



/* 
     M A I N
 */
int 
main (int argc, char **argv)
{
  int last_argc = -1;

  if (argc)
    {
      log_set_prefix (*argv);
      argc--; argv++;
    }
  while (argc && last_argc != argc )
    {
      last_argc = argc;
      if (!strcmp (*argv, "--help"))
        {
          printf ("usage: %s [options]\n"
                  "\n"
                  "Options:\n"
                  "  --verbose      Show what is going on\n",
                  log_get_prefix ());
          exit (0);
        }
      if (!strcmp (*argv, "--verbose"))
        {
          verbose = 1;
          argc--; argv++;
        }
      else if (!strcmp (*argv, "--debug"))
        {
          verbose = debug = 1;
          argc--; argv++;
        }
    }

  run_test ();

  return errorcount ? 1 : 0;
}

