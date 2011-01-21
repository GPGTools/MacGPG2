/* gpgcedrv.c - WindowsCE device driver to implement a pipe.
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


#include <stdio.h>
#include <stdarg.h>
#include <windows.h>
#include <devload.h>
#include <winioctl.h>

#define ENABLE_DEBUG
#warning Cancel not handled.


/* Missing IOCTLs in the current mingw32ce.  */
#ifndef IOCTL_PSL_NOTIFY
# define FILE_DEVICE_PSL 259
# define IOCTL_PSL_NOTIFY                               \
  CTL_CODE (259, 255, METHOD_NEITHER, FILE_ANY_ACCESS)
#endif /*IOCTL_PSL_NOTIFY*/


/* The IOCTL to return the rendezvous id of the handle.

   The required outbuf parameter is the address of a variable to store
   the rendezvous ID, which is a LONG value.  */
#define GPGCEDEV_IOCTL_GET_RVID \
  CTL_CODE (FILE_DEVICE_STREAMS, 2048, METHOD_BUFFERED, FILE_ANY_ACCESS)

/* The IOCTL used to create the pipe. 

   The caller sends this IOCTL to the read or the write handle.  The
   required inbuf parameter is address of a variable holding the
   rendezvous id of the pipe's other end.  There is one possible
   problem with the code: If a pipe is kept in non-rendezvous state
   until after the rendezvous ids overflow, it is possible that the
   wrong end will be used.  However this is not a realistic scenario.  */
#define GPGCEDEV_IOCTL_MAKE_PIPE \
  CTL_CODE (FILE_DEVICE_STREAMS, 2049, METHOD_BUFFERED, FILE_ANY_ACCESS)

/* The IOCTL used to unblock a blocking thread.

   The caller sends this IOCTL to the read or the write handle.  No
   parameter is required.  The effect is that a reader or writer
   blocked on the same handle is unblocked (and will return
   ERROR_BUSY).  Note that the operation can be repeated, if so
   desired.  The state of the pipe itself will not be affected in any
   way.  */
#define GPGCEDEV_IOCTL_UNBLOCK \
  CTL_CODE (FILE_DEVICE_STREAMS, 2050, METHOD_BUFFERED, FILE_ANY_ACCESS)

/* The IOCTL to assign a rendezvous id to a process.

   The required inbuf parameters are the rendezvous ID to assign and
   the process ID of the process receiving the RVID.  The handle on
   which this is called is not really used at all!  */
#define GPGCEDEV_IOCTL_ASSIGN_RVID \
  CTL_CODE (FILE_DEVICE_STREAMS, 2051, METHOD_BUFFERED, FILE_ANY_ACCESS)


struct pipeimpl_s
{
  CRITICAL_SECTION critsect;  /* Lock for all members.  */

  int refcnt;
  char *buffer;       
  size_t buffer_size;
  size_t buffer_len;  /* The valid length of the bufer.  */
  size_t buffer_pos;  /* The actual read position.  */

#define PIPE_FLAG_NO_READER 1
#define PIPE_FLAG_NO_WRITER 2
#define PIPE_FLAG_UNBLOCK_READER 4
#define PIPE_FLAG_UNBLOCK_WRITER 8
#define PIPE_FLAG_HALT_MONITOR 16
  int flags;

  HANDLE space_available; /* Set if space is available.  */
  HANDLE data_available;  /* Set if data is available.  */ 

  /* For the monitor thread started by ASSIGN_RVID.  */
  HANDLE monitor_proc;
  int monitor_access;
};
typedef struct pipeimpl_s *pipeimpl_t;


/* An object to store information pertaining to an open-context.  */
struct opnctx_s;
typedef struct opnctx_s *opnctx_t;
struct opnctx_s
{
  int inuse;        /* True if this object has valid data.  */
  LONG rvid;        /* The unique rendezvous identifier.  */
  DWORD access_code;/* Value from OpenFile.  */
  DWORD share_mode; /* Value from OpenFile.  */

  /* The state shared by all users.  */
  pipeimpl_t pipeimpl;
};

/* A malloced table of open-context and the number of allocated slots.  */
static opnctx_t opnctx_table;
static size_t   opnctx_table_size;
/* The macros make sure that 0 is never a valid openctx_arg.  */
#define OPNCTX_TO_IDX(ctx_arg) (((ctx_arg) - opnctx_table) + 1)
#define OPNCTX_FROM_IDX(idx) (&opnctx_table[(idx) - 1])
#define OPNCTX_VALID_IDX_P(idx) ((idx) > 0 && (idx) <= opnctx_table_size)

/* A criticial section object used to protect the OPNCTX_TABLE.  */
static CRITICAL_SECTION opnctx_table_cs;

/* We don't need a device context thus we use the adress of the
   critical section object for it.  */
#define DEVCTX_VALUE ((DWORD)(&opnctx_table_cs))

/* Constants used for our lock functions.  */
#define LOCK_TRY  0
#define LOCK_WAIT 1



static void
log_debug (const char *fmt, ...)
{
#ifndef ENABLE_DEBUG
  (void)fmt;
#else
  va_list arg_ptr;
  FILE *fp;

  fp = fopen ("\\gpgcedev.log", "a+");
  if (!fp)
    return;
  va_start (arg_ptr, fmt);
  vfprintf (fp, fmt, arg_ptr);
  va_end (arg_ptr);
  fclose (fp);
#endif
}


/* Return a new rendezvous next command id.  Command Ids are used to group
   resources of one command.  We will never return an RVID of 0. */
static LONG
create_rendezvous_id (void)
{
  static LONG rendezvous_id;
  LONG rvid;

  while (!(rvid = InterlockedIncrement (&rendezvous_id)))
    ;
  return rvid;
}



pipeimpl_t
pipeimpl_new (void)
{
  pipeimpl_t pimpl;

  pimpl = malloc (sizeof (*pimpl));
  if (!pimpl)
    return NULL;

  InitializeCriticalSection (&pimpl->critsect);
  pimpl->refcnt = 1;
  pimpl->buffer_size = 512;
  pimpl->buffer = malloc (pimpl->buffer_size);
  pimpl->buffer_len = 0;
  pimpl->buffer_pos = 0;
  pimpl->flags = 0;
  pimpl->space_available = CreateEvent (NULL, FALSE, FALSE, NULL);
  pimpl->data_available = CreateEvent (NULL, FALSE, FALSE, NULL);
  pimpl->monitor_proc = INVALID_HANDLE_VALUE;
  pimpl->monitor_access = 0;
  return pimpl;
}


/* PIMPL must be locked.  It is unlocked at exit.  */
void
pipeimpl_unref (pipeimpl_t pimpl)
{
  int release = 0;

  log_debug ("pipeimpl_unref (%p): dereference\n", pimpl);

  if (--pimpl->refcnt == 0)
    release = 1;
  LeaveCriticalSection (&pimpl->critsect);

  if (! release)
    return;

  log_debug ("pipeimpl_unref (%p): release\n", pimpl);

  DeleteCriticalSection (&pimpl->critsect);
  if (pimpl->buffer)
    {
      free (pimpl->buffer);
      pimpl->buffer = NULL;
      pimpl->buffer_size = 0;
    }
  if (pimpl->space_available != INVALID_HANDLE_VALUE)
    {
      CloseHandle (pimpl->space_available);
      pimpl->space_available = INVALID_HANDLE_VALUE;
    }
  if (pimpl->data_available != INVALID_HANDLE_VALUE)
    {
      CloseHandle (pimpl->data_available);
      pimpl->data_available = INVALID_HANDLE_VALUE;
    }
}



/* Return a new opnctx handle and mark it as used.  Returns NULL and
   sets LastError on memory failure etc.  opnctx_table_cs must be
   locked on entry and is locked on exit.  Note that the returned
   pointer is only valid as long as opnctx_table_cs stays locked, as
   it is not stable under table reallocation.  */
static opnctx_t
allocate_opnctx (void)
{
  opnctx_t opnctx = NULL;
  int idx;

  for (idx = 0; idx < opnctx_table_size; idx++)
    if (! opnctx_table[idx].inuse)
      break;
  if (idx == opnctx_table_size)
    {
      /* We need to increase the size of the table.  The approach we
         take is straightforward to minimize the risk of bugs.  */
      opnctx_t newtbl;
      size_t newsize = opnctx_table_size + 64;

      newtbl = calloc (newsize, sizeof *newtbl);
      if (!newtbl)
        goto leave;
      memcpy (newtbl, opnctx_table, opnctx_table_size * sizeof (*newtbl));
      free (opnctx_table);
      opnctx_table = newtbl;
      idx = opnctx_table_size;
      opnctx_table_size = newsize;
    }
  opnctx = &opnctx_table[idx];
  opnctx->inuse = 1;
  opnctx->rvid = 0;
  opnctx->access_code = 0;
  opnctx->share_mode = 0;
  opnctx->pipeimpl = 0;

 leave:
  return opnctx;
}


/* Verify context CTX, returns NULL if not valid and the pointer to
   the context if valid.  opnctx_table_cs must be locked on entry and
   is locked on exit.  Note that the returned pointer is only valid as
   long as opnctx_table_cs remains locked.  */
opnctx_t
verify_opnctx (DWORD ctx_arg)
{
  opnctx_t ctx;

  if (! OPNCTX_VALID_IDX_P (ctx_arg))
    {
      SetLastError (ERROR_INVALID_HANDLE);
      return NULL;
    }
  ctx = OPNCTX_FROM_IDX (ctx_arg);

  if (! ctx->inuse)
    {
      SetLastError (ERROR_INVALID_HANDLE);
      return NULL;
    }
  return ctx;
}


static pipeimpl_t
assert_pipeimpl (opnctx_t ctx)
{
  DWORD ctx_arg = OPNCTX_TO_IDX (ctx);

  if (! ctx->pipeimpl)
    {
      ctx->pipeimpl = pipeimpl_new ();
      if (! ctx->pipeimpl)
	{
	  log_debug ("  assert_pipeimpl (ctx=%i): error: can't create pipe\n",
		     ctx_arg);
	  return NULL;
	}
      log_debug ("  assert_pipeimpl (ctx=%i): created pipe 0x%p\n",
		 ctx_arg, ctx->pipeimpl);
    }
  return ctx->pipeimpl;
}


/* Verify access CODE for context CTX_ARG, returning a reference to
   the locked pipe implementation.  opnctx_table_cs must be unlocked
   on entry and is unlocked on exit.  */
pipeimpl_t
access_opnctx (DWORD ctx_arg, DWORD code)
{
  opnctx_t ctx;
  pipeimpl_t pimpl;

  EnterCriticalSection (&opnctx_table_cs);
  ctx = verify_opnctx (ctx_arg);
  if (! ctx)
    {
      /* Error is set by verify_opnctx.  */
      LeaveCriticalSection (&opnctx_table_cs);
      return NULL;
    }

  if (! (ctx->access_code & code))
    {
      SetLastError (ERROR_INVALID_ACCESS);
      LeaveCriticalSection (&opnctx_table_cs);
      return NULL;
    }

  pimpl = assert_pipeimpl (ctx);
  if (! pimpl)
    {
      LeaveCriticalSection (&opnctx_table_cs);
      return NULL;
    }

  EnterCriticalSection (&pimpl->critsect);
  pimpl->refcnt++;
  LeaveCriticalSection (&opnctx_table_cs);
  return pimpl;
}



static char *
wchar_to_utf8 (const wchar_t *string)
{
  int n;
  size_t length = wcslen (string);
  char *result;

  n = WideCharToMultiByte (CP_UTF8, 0, string, length, NULL, 0, NULL, NULL);
  if (n < 0 || (n+1) <= 0)
    abort ();

  result = malloc (n+1);
  if (!result)
    abort ();
  n = WideCharToMultiByte (CP_ACP, 0, string, length, result, n, NULL, NULL);
  if (n < 0)
    abort ();
  
  result[n] = 0;
  return result;
}


/* Initialize the device and return a device specific context.  */
DWORD
GPG_Init (LPCTSTR active_key, DWORD bus_context)
{
  char *tmpbuf;
  (void)bus_context;
  
  tmpbuf = wchar_to_utf8 (active_key);
  log_debug ("GPG_Init (devctx=0x%p, %s)\n", DEVCTX_VALUE, tmpbuf);
  free (tmpbuf);

  /* We don't need any global data.  However, we need to return
     something.  */
  return DEVCTX_VALUE;
}



/* Deinitialize this device driver.  */
BOOL
GPG_Deinit (DWORD devctx)
{
  log_debug ("GPG_Deinit (devctx=0x%p)\n", (void*)devctx);
  if (devctx != DEVCTX_VALUE)
    {
      SetLastError (ERROR_INVALID_PARAMETER);
      return FALSE; /* Error.  */
    }
  
  /* FIXME: Release resources.  */

  return TRUE; /* Success.  */
}



/* Create a new open context.  This fucntion is called due to a
   CreateFile from the application.  */
DWORD
GPG_Open (DWORD devctx, DWORD access_code, DWORD share_mode)
{
  opnctx_t opnctx;
  DWORD ctx_arg = 0;

  log_debug ("GPG_Open (devctx=%p)\n", (void*)devctx);
  if (devctx != DEVCTX_VALUE)
    {
      log_debug ("GPG_Open (devctx=%p): error: wrong devctx (expected 0x%p)\n",
		 (void*) devctx);
      SetLastError (ERROR_INVALID_PARAMETER);
      return 0; /* Error.  */
    }

  EnterCriticalSection (&opnctx_table_cs);
  opnctx = allocate_opnctx ();
  if (!opnctx)
    {
      log_debug ("GPG_Open (devctx=%p): error: could not allocate context\n",
		 (void*) devctx);
      goto leave;
    }

  opnctx->access_code = access_code;
  opnctx->share_mode = share_mode;

  ctx_arg = OPNCTX_TO_IDX (opnctx);

  log_debug ("GPG_Open (devctx=%p): success: created context %i\n",
	     (void*) devctx, ctx_arg);

 leave:
  LeaveCriticalSection (&opnctx_table_cs);
  return ctx_arg;
}



BOOL
GPG_Close (DWORD opnctx_arg)
{
  opnctx_t opnctx;
  BOOL result = FALSE;

  log_debug ("GPG_Close (ctx=%i)\n", opnctx_arg);

  EnterCriticalSection (&opnctx_table_cs);
  opnctx = verify_opnctx (opnctx_arg);
  if (!opnctx)
    {
      log_debug ("GPG_Close (ctx=%i): could not find context\n", opnctx_arg);
      goto leave;
    }

  if (opnctx->pipeimpl)
    {
      pipeimpl_t pimpl = opnctx->pipeimpl;
      EnterCriticalSection (&pimpl->critsect);
      /* This needs to be adjusted if there can be multiple
	 reader/writers.  */
      if (opnctx->access_code & GENERIC_READ)
	{
	  pimpl->flags |= PIPE_FLAG_NO_READER;
	  SetEvent (pimpl->space_available);
	}
      else if (opnctx->access_code & GENERIC_WRITE)
	{
	  pimpl->flags |= PIPE_FLAG_NO_WRITER;
	  SetEvent (pimpl->data_available);
	}
      pipeimpl_unref (pimpl);
      opnctx->pipeimpl = 0;
    }
  opnctx->access_code = 0;
  opnctx->share_mode = 0;
  opnctx->rvid = 0;
  opnctx->inuse = 0;
  result = TRUE;
  log_debug ("GPG_Close (ctx=%i): success\n", opnctx_arg);

 leave:
  LeaveCriticalSection (&opnctx_table_cs);
  return result;
}



DWORD
GPG_Read (DWORD opnctx_arg, void *buffer, DWORD count)
{
  pipeimpl_t pimpl;
  const char *src;
  char *dst;
  int result = -1;

  log_debug ("GPG_Read (ctx=%i, buffer=0x%p, count=%d)\n",
	     opnctx_arg, buffer, count);

  pimpl = access_opnctx (opnctx_arg, GENERIC_READ);
  if (!pimpl)
    {
      log_debug ("GPG_Read (ctx=%i): error: could not access context\n",
		 opnctx_arg);
      return -1;
    }

 retry:
  if (pimpl->buffer_pos == pimpl->buffer_len)
    {
      HANDLE data_available = pimpl->data_available;

      /* Check for end of file.  */
      if (pimpl->flags & PIPE_FLAG_NO_WRITER)
	{
	  log_debug ("GPG_Read (ctx=%i): success: EOF\n", opnctx_arg);
	  result = 0;
	  goto leave;
	}

      /* Check for request to unblock once.  */
      if (pimpl->flags & PIPE_FLAG_UNBLOCK_READER)
	{
	  log_debug ("GPG_Read (ctx=%i): success: EBUSY (due to unblock)\n",
		     opnctx_arg);
	  pimpl->flags &= ~PIPE_FLAG_UNBLOCK_READER;
	  SetLastError (ERROR_BUSY);
	  result = -1;
	  goto leave;
	}

      LeaveCriticalSection (&pimpl->critsect);
      log_debug ("GPG_Read (ctx=%i): waiting: data_available\n", opnctx_arg);
      WaitForSingleObject (data_available, INFINITE);
      log_debug ("GPG_Read (ctx=%i): resuming: data_available\n", opnctx_arg);
      EnterCriticalSection (&pimpl->critsect);
      goto retry;
    }
  
  dst = buffer;
  src = pimpl->buffer + pimpl->buffer_pos;
  while (count > 0 && pimpl->buffer_pos < pimpl->buffer_len)
    {
      *dst++ = *src++;
      count--;
      pimpl->buffer_pos++;
    }
  result = (dst - (char*)buffer);
  if (pimpl->buffer_pos == pimpl->buffer_len)
    pimpl->buffer_pos = pimpl->buffer_len = 0;
  
  /* Now there should be some space available.  Signal the write end.
     Even if COUNT was passed as NULL and no space is available,
     signaling must be done.  */
  if (!SetEvent (pimpl->space_available))
    log_debug ("GPG_Read (ctx=%i): warning: SetEvent(space_available) "
	       "failed: rc=%d\n", opnctx_arg, (int)GetLastError ());

  log_debug ("GPG_Read (ctx=%i): success: result=%d\n", opnctx_arg, result);

 leave:
  pipeimpl_unref (pimpl);
  return result;
}



DWORD
GPG_Write (DWORD opnctx_arg, const void *buffer, DWORD count)
{
  pipeimpl_t pimpl;
  int result = -1;
  const char *src;
  char *dst;
  size_t nwritten = 0;

  log_debug ("GPG_Write (ctx=%i, buffer=0x%p, count=%d)\n", opnctx_arg,
	     buffer, count);

  pimpl = access_opnctx (opnctx_arg, GENERIC_WRITE);
  if (!pimpl)
    {
      log_debug ("GPG_Write (ctx=%i): error: could not access context\n",
		 opnctx_arg);
      return -1;
    }

  if (!count)
    {
      log_debug ("GPG_Write (ctx=%i): success\n", opnctx_arg);
      result = 0;
      goto leave;
    }

 retry:
  /* Check for broken pipe.  */
  if (pimpl->flags & PIPE_FLAG_NO_READER)
    {
      log_debug ("GPG_Write (ctx=%i): error: broken pipe\n", opnctx_arg);
      SetLastError (ERROR_BROKEN_PIPE);
      goto leave;
    }

  /* Check for request to unblock once.  */
  if (pimpl->flags & PIPE_FLAG_UNBLOCK_WRITER)
    {
      log_debug ("GPG_Write (ctx=%i): success: EBUSY (due to unblock)\n",
		 opnctx_arg);
      pimpl->flags &= ~PIPE_FLAG_UNBLOCK_WRITER;
      SetLastError (ERROR_BUSY);
      result = -1;
      goto leave;
    }

  /* Write to our buffer.  */
  if (pimpl->buffer_len == pimpl->buffer_size)
    {
      /* Buffer is full.  */
      HANDLE space_available = pimpl->space_available;
      LeaveCriticalSection (&pimpl->critsect);
      log_debug ("GPG_Write (ctx=%i): waiting: space_available\n", opnctx_arg);
      WaitForSingleObject (space_available, INFINITE);
      log_debug ("GPG_Write (ctx=%i): resuming: space_available\n",
		 opnctx_arg);
      EnterCriticalSection (&pimpl->critsect);
      goto retry;
    }

  src = buffer;
  dst = pimpl->buffer + pimpl->buffer_len;
  while (count > 0 && pimpl->buffer_len < pimpl->buffer_size)
    {
      *dst++ = *src++;
      count--;
      pimpl->buffer_len++;
      nwritten++;
    }
  result = nwritten;

  if (!SetEvent (pimpl->data_available))
    log_debug ("GPG_Write (ctx=%i): warning: SetEvent(data_available) "
	       "failed: rc=%d\n", opnctx_arg, (int)GetLastError ());

  log_debug ("GPG_Write (ctx=%i): success: result=%d\n", opnctx_arg, result);

 leave:
  pipeimpl_unref (pimpl);
  return result;
}



DWORD
GPG_Seek (DWORD opnctx_arg, long amount, WORD type)
{
  SetLastError (ERROR_SEEK_ON_DEVICE);
  return -1; /* Error.  */
}



/* opnctx_table_s is locked on entering and on exit.  */
static BOOL
make_pipe (opnctx_t ctx, LONG rvid)
{
  DWORD ctx_arg = OPNCTX_TO_IDX (ctx);
  opnctx_t peerctx = NULL;
  int idx;
  pipeimpl_t pimpl;

  log_debug ("  make_pipe (ctx=%i, rvid=%08lx)\n", ctx_arg, rvid);

  if (ctx->pipeimpl)
    {
      log_debug ("  make_pipe (ctx=%i): error: already assigned\n", ctx_arg);
      SetLastError (ERROR_ALREADY_ASSIGNED);
      return FALSE;
    }

  for (idx = 0; idx < opnctx_table_size; idx++)
    if (opnctx_table[idx].inuse && opnctx_table[idx].rvid == rvid)
      {
        peerctx = &opnctx_table[idx];
        break;
      }
  if (! peerctx)
    {
      log_debug ("  make_pipe (ctx=%i): error: not found\n", ctx_arg);
      SetLastError (ERROR_NOT_FOUND);
      return FALSE;
    }

  if (peerctx == ctx)
    {
      log_debug ("  make_pipe (ctx=%i): error: target and source identical\n",
		 ctx_arg);
      SetLastError (ERROR_INVALID_TARGET_HANDLE);
      return FALSE;
    }

  if ((ctx->access_code & GENERIC_READ))
    {
      /* Check that the peer is a write end.  */
      if (!(peerctx->access_code & GENERIC_WRITE))
        {
          SetLastError (ERROR_INVALID_ACCESS);
	  log_debug ("  make_pipe (ctx=%i): error: peer is not writer\n",
		     ctx_arg);
          return FALSE;
        }
    }
  else if ((ctx->access_code & GENERIC_WRITE))
    {
      /* Check that the peer is a read end.  */
      if (!(peerctx->access_code & GENERIC_READ))
        {
          SetLastError (ERROR_INVALID_ACCESS);
	  log_debug ("  make_pipe (ctx=%i): error: peer is not reader\n",
		     ctx_arg);
          return FALSE;
        }
    }
  else
    {
      SetLastError (ERROR_INVALID_ACCESS);
      log_debug ("  make_pipe (ctx=%i): error: invalid access\n", ctx_arg);
      return FALSE;
    }

  /* Make sure the peer has a pipe implementation to be shared.  If
     not yet, create one.  */
  pimpl = assert_pipeimpl (peerctx);
  if (! pimpl)
    return FALSE;

  EnterCriticalSection (&pimpl->critsect);
  pimpl->refcnt++;
  if (pimpl->monitor_proc != INVALID_HANDLE_VALUE)
    {
      /* If there is a monitor to the pipe, then it's now about time
	 to ask it to go away.  */
      log_debug ("  make_pipe (ctx=%i): waking up monitor for pipe 0x%p\n",
		 ctx_arg, pimpl);
      pimpl->flags |= PIPE_FLAG_HALT_MONITOR;
      if (pimpl->monitor_access & GENERIC_READ)
	SetEvent (pimpl->data_available);
      else
	SetEvent (pimpl->space_available);
    }
  LeaveCriticalSection (&pimpl->critsect);

  ctx->pipeimpl = pimpl;

  log_debug ("  make_pipe (ctx=%i): success: combined with peer ctx=%i "
	     "(pipe 0x%p)\n", ctx_arg, OPNCTX_TO_IDX (peerctx), pimpl);

  return TRUE;
}


/* opnctx_table_s is locked on entering and on exit.  */
static BOOL
unblock_call (opnctx_t ctx)
{
  /* If there is no pipe object, no thread can be blocked.  */
  if (!ctx->pipeimpl)
    return TRUE;

  EnterCriticalSection (&ctx->pipeimpl->critsect);
  if (ctx->access_code & GENERIC_READ)
    {
      ctx->pipeimpl->flags |= PIPE_FLAG_UNBLOCK_READER;
      SetEvent (ctx->pipeimpl->data_available);
    }
  else if (ctx->access_code & GENERIC_WRITE)
    {
      ctx->pipeimpl->flags |= PIPE_FLAG_UNBLOCK_WRITER;
      SetEvent (ctx->pipeimpl->space_available);
    }
  LeaveCriticalSection (&ctx->pipeimpl->critsect);

  return TRUE;
}


static DWORD CALLBACK 
monitor (void *arg)
{
  pipeimpl_t pimpl = (pipeimpl_t) arg;
  HANDLE handles[2];

  log_debug ("starting monitor (pimpl=0x%p)\n", pimpl);
  
  EnterCriticalSection (&pimpl->critsect);
  /* Putting proc first in the array is convenient, as this is a hard
     break-out condition (and thus takes precedence in WFMO).  The
     reader/writer event is a soft condition, which also requires a
     flag to be set.  */
  handles[0] = pimpl->monitor_proc;
  if (pimpl->monitor_access & GENERIC_READ)
    handles[1] = pimpl->data_available;
  else
    handles[1] = pimpl->space_available;

 retry:
  /* First check if the peer has not gone away.  If it has, we are done.  */
  if (pimpl->flags & PIPE_FLAG_HALT_MONITOR)
    {
      log_debug ("monitor (pimpl=0x%p): done: monitored process taking over\n",
		 pimpl);
    }    
  else
    {
      DWORD res;

      LeaveCriticalSection (&pimpl->critsect);
      log_debug ("monitor (pimpl=0x%p): waiting\n", pimpl);
      res = WaitForMultipleObjects (2, handles, FALSE, INFINITE);
      log_debug ("monitor (pimpl=0x%p): resuming\n", pimpl);
      EnterCriticalSection (&pimpl->critsect);

      if (res == WAIT_FAILED)
	{
	  log_debug ("monitor (pimpl=0x%p): WFMO failed: %i\n",
		     pimpl, GetLastError ());
	}
      else if (res == WAIT_OBJECT_0)
	{
	  log_debug ("monitor (pimpl=0x%p): done: monitored process died\n",
		     pimpl);
	}	  
      else if (res == WAIT_OBJECT_0 + 1)
	goto retry;
      else
	{
	  log_debug ("monitor (pimpl=0x%p): unexpected result of WFMO: %i\n",
		     pimpl, res);
	}
    }

  log_debug ("ending monitor (pimpl=0x%p)\n", pimpl);
  CloseHandle (pimpl->monitor_proc);
  pimpl->monitor_proc = INVALID_HANDLE_VALUE;
  pimpl->monitor_access = 0;
  pimpl->flags &= ~PIPE_FLAG_HALT_MONITOR;
  pipeimpl_unref (pimpl);

  return 0;
}


/* opnctx_table_s is locked on entering and on exit.  */
static BOOL
assign_rvid (opnctx_t ctx, DWORD rvid, DWORD pid)
{
  DWORD ctx_arg = OPNCTX_TO_IDX (ctx);
  int idx;
  opnctx_t peerctx;
  HANDLE monitor_hnd;
  HANDLE proc;
  pipeimpl_t pimpl;

  log_debug ("  assign_rvid (ctx=%i, rvid=%08lx, pid=%08lx)\n",
	     ctx_arg, rvid, pid);

  for (idx = 0; idx < opnctx_table_size; idx++)
    if (opnctx_table[idx].inuse && opnctx_table[idx].rvid == rvid)
      {
        peerctx = &opnctx_table[idx];
        break;
      }
  if (! peerctx)
    {
      log_debug ("  assign_rvid (ctx=%i): error: not found\n", ctx_arg);
      SetLastError (ERROR_NOT_FOUND);
      return FALSE;
    }
  
  if (peerctx->pipeimpl
      && peerctx->pipeimpl->monitor_proc != INVALID_HANDLE_VALUE)
    {
      log_debug ("  assign_rvid (ctx=%i): error: rvid already assigned\n",
		 ctx_arg);
      SetLastError (ERROR_ALREADY_ASSIGNED);
      return FALSE;
    }

  proc = OpenProcess (0, FALSE, pid);
  if (proc == NULL)
    {
      log_debug ("  assign_rvid (ctx=%i): error: process not found\n",
		 ctx_arg);
      return FALSE;
    }

  /* Acquire a reference to the pipe.  We don't want accesss to be
     checked.  */
  pimpl = assert_pipeimpl (peerctx);
  if (! pimpl)
    {
      CloseHandle (proc);
      return FALSE;
    }

  EnterCriticalSection (&pimpl->critsect);
  pimpl->refcnt++;

  /* The monitor shadows the peer, so it takes its access.  Our access
     is the opposite of that of the peer.  */
  pimpl->monitor_proc = proc;
  if (peerctx->access_code == GENERIC_READ)
    pimpl->monitor_access = GENERIC_WRITE;
  else
    pimpl->monitor_access = GENERIC_READ;

  monitor_hnd = CreateThread (NULL, 0, monitor, pimpl, 0, NULL);
  if (monitor_hnd == INVALID_HANDLE_VALUE)
    {
      pimpl->monitor_access = 0;
      pimpl->monitor_proc = INVALID_HANDLE_VALUE;
      LeaveCriticalSection (&pimpl->critsect);

      CloseHandle (proc);
      log_debug ("  assign_rvid (ctx=%i): error: can not create monitor\n",
		 ctx_arg);
      return FALSE;
    }
  CloseHandle (monitor_hnd);

  /* Consume the pimpl reference.  */
  LeaveCriticalSection (&pimpl->critsect);

  return TRUE;
}


BOOL
GPG_IOControl (DWORD opnctx_arg, DWORD code, void *inbuf, DWORD inbuflen,
               void *outbuf, DWORD outbuflen, DWORD *actualoutlen)
{
  opnctx_t opnctx;
  BOOL result = FALSE;
  LONG rvid;
  LONG pid;

  log_debug ("GPG_IOControl (ctx=%i, %08x)\n", opnctx_arg, code);

  EnterCriticalSection (&opnctx_table_cs);
  opnctx = verify_opnctx (opnctx_arg);
  if (!opnctx)
    {
      log_debug ("GPG_IOControl (ctx=%i): error: could not access context\n", 
		 opnctx_arg);
      goto leave;
    }

  switch (code)
    {
    case GPGCEDEV_IOCTL_GET_RVID:
      log_debug ("GPG_IOControl (ctx=%i): code: GET_RVID\n", opnctx_arg);
      if (inbuf || inbuflen || !outbuf || outbuflen < sizeof (LONG))
        {
	  log_debug ("GPG_IOControl (ctx=%i): error: invalid parameter\n", 
		     opnctx_arg);
          SetLastError (ERROR_INVALID_PARAMETER);
          goto leave;
        }

      if (! opnctx->rvid) 
	opnctx->rvid = create_rendezvous_id ();
      log_debug ("GPG_IOControl (ctx=%i): returning rvid: %08lx\n",
		 opnctx_arg, opnctx->rvid);

      memcpy (outbuf, &opnctx->rvid, sizeof (LONG));
      if (actualoutlen)
        *actualoutlen = sizeof (LONG);
      result = TRUE;
      break;

    case GPGCEDEV_IOCTL_MAKE_PIPE:
      log_debug ("GPG_IOControl (ctx=%i): code: MAKE_PIPE\n", opnctx_arg);
      if (!inbuf || inbuflen < sizeof (LONG) 
          || outbuf || outbuflen || actualoutlen)
        {
	  log_debug ("GPG_IOControl (ctx=%i): error: invalid parameter\n", 
		     opnctx_arg);
          SetLastError (ERROR_INVALID_PARAMETER);
          goto leave;
        }
      memcpy (&rvid, inbuf, sizeof (LONG));
      log_debug ("GPG_IOControl (ctx=%i): make pipe for rvid: %08lx\n", 
		 opnctx_arg, rvid);
      if (make_pipe (opnctx, rvid))
        result = TRUE;
      break;

    case GPGCEDEV_IOCTL_UNBLOCK:
      log_debug ("GPG_IOControl (ctx=%i): code: UNBLOCK\n", opnctx_arg);
      if (inbuf || inbuflen || outbuf || outbuflen || actualoutlen)
        {
	  log_debug ("GPG_IOControl (ctx=%i): error: invalid parameter\n", 
		     opnctx_arg);
          SetLastError (ERROR_INVALID_PARAMETER);
          goto leave;
        }
      
      if (unblock_call (opnctx))
        result = TRUE;
      break;

    case GPGCEDEV_IOCTL_ASSIGN_RVID:
      log_debug ("GPG_IOControl (ctx=%i): code: ASSIGN_RVID\n", opnctx_arg);
      if (!inbuf || inbuflen < 2 * sizeof (DWORD)
          || outbuf || outbuflen || actualoutlen)
        {
	  log_debug ("GPG_IOControl (ctx=%i): error: invalid parameter\n", 
		     opnctx_arg);
          SetLastError (ERROR_INVALID_PARAMETER);
          goto leave;
        }
      memcpy (&rvid, inbuf, sizeof (DWORD));
      memcpy (&pid, ((char *) inbuf) + sizeof (DWORD), sizeof (DWORD));
      log_debug ("GPG_IOControl (ctx=%i): assign rvid %08lx to pid %08lx\n", 
		 opnctx_arg, rvid, pid);
      if (assign_rvid (opnctx, rvid, pid))
        result = TRUE;
      break;

    case IOCTL_PSL_NOTIFY:
      /* This notification is received if the application's main
         thread exits and the application has other threads running
         and the application has open handles for this device.  What
         we do is to unblock them all simialr to an explicit unblock
         call.  */
      log_debug ("GPG_IOControl (ctx=%i): code: NOTIFY\n", opnctx_arg);

      if (unblock_call (opnctx))
        result = TRUE;
      break;

    default:
      log_debug ("GPG_IOControl (ctx=%i): code: (unknown)\n", opnctx_arg);
      SetLastError (ERROR_INVALID_PARAMETER);
      break;
    }

  log_debug ("GPG_IOControl (ctx=%i): success: result=%d\n", opnctx_arg,
	     result);

 leave:
  LeaveCriticalSection (&opnctx_table_cs);
  return result;
}



void
GPG_PowerUp (DWORD devctx)
{
}



void
GPG_PowerDown (DWORD devctx)
{
}



/* Entry point called by the DLL loader.  */
int WINAPI
DllMain (HINSTANCE hinst, DWORD reason, LPVOID reserved)
{
  (void)reserved;

  switch (reason)
    {
    case DLL_PROCESS_ATTACH:
      InitializeCriticalSection (&opnctx_table_cs);
      break;

    case DLL_THREAD_ATTACH:
      break;

    case DLL_THREAD_DETACH:
      break;

    case DLL_PROCESS_DETACH:
      DeleteCriticalSection (&opnctx_table_cs);
      break;

    default:
      break;
    }
  
  return TRUE;
}
