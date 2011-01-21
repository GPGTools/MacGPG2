/* assuan-socket.c
   Copyright (C) 2004, 2005, 2009 Free Software Foundation, Inc.

   This file is part of Assuan.

   Assuan is free software; you can redistribute it and/or modify it
   under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 2.1 of
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
#ifdef HAVE_W32_SYSTEM
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>
#include <io.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#endif
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>

#include "assuan-defs.h"
#include "debug.h"

/* Hacks for Slowaris.  */
#ifndef PF_LOCAL
# ifdef PF_UNIX
#  define PF_LOCAL PF_UNIX
# else
#  define PF_LOCAL AF_UNIX
# endif
#endif
#ifndef AF_LOCAL
# define AF_LOCAL AF_UNIX
#endif

#ifdef HAVE_W32_SYSTEM
#ifndef S_IRGRP
# define S_IRGRP 0
# define S_IWGRP 0
#endif
#endif


#ifdef HAVE_W32_SYSTEM
int
_assuan_sock_wsa2errno (int err)
{
  switch (err)
    {
    case WSAENOTSOCK:
      return EINVAL;
    case WSAEWOULDBLOCK:
      return EAGAIN;
    case ERROR_BROKEN_PIPE:
      return EPIPE;
    case WSANOTINITIALISED:
      return ENOSYS;
    default:
      return EIO;
    }
}


/* W32: Fill BUFFER with LENGTH bytes of random.  Returns -1 on
   failure, 0 on success.  Sets errno on failure.  */
static int
get_nonce (char *buffer, size_t nbytes) 
{
  HCRYPTPROV prov;
  int ret = -1;

  if (!CryptAcquireContext (&prov, NULL, NULL, PROV_RSA_FULL, 
                            (CRYPT_VERIFYCONTEXT|CRYPT_SILENT)) )
    gpg_err_set_errno (ENODEV);
  else 
    {
      if (!CryptGenRandom (prov, nbytes, (unsigned char *) buffer))
        gpg_err_set_errno (ENODEV);
      else
        ret = 0;
      CryptReleaseContext (prov, 0);
    }
  return ret;
}


/* W32: The buffer for NONCE needs to be at least 16 bytes.  Returns 0 on
   success and sets errno on failure. */
static int
read_port_and_nonce (const char *fname, unsigned short *port, char *nonce)
{
  FILE *fp;
  char buffer[50], *p;
  size_t nread;
  int aval;

  fp = fopen (fname, "rb");
  if (!fp)
    return -1;
  nread = fread (buffer, 1, sizeof buffer - 1, fp);
  fclose (fp);
  if (!nread)
    {
      gpg_err_set_errno (ENOENT);
      return -1;
    }
  buffer[nread] = 0;
  aval = atoi (buffer);
  if (aval < 1 || aval > 65535)
    {
      gpg_err_set_errno (EINVAL);
      return -1;
    }
  *port = (unsigned int)aval;
  for (p=buffer; nread && *p != '\n'; p++, nread--)
    ;
  if (*p != '\n' || nread != 17)
    {
      gpg_err_set_errno (EINVAL);
      return -1;
    }
  p++; nread--;
  memcpy (nonce, p, 16);
  return 0;
}
#endif /*HAVE_W32_SYSTEM*/


/* Return a new socket.  Note that under W32 we consider a socket the
   same as an System Handle; all functions using such a handle know
   about this dual use and act accordingly. */ 
assuan_fd_t
_assuan_sock_new (assuan_context_t ctx, int domain, int type, int proto)
{
#ifdef HAVE_W32_SYSTEM
  assuan_fd_t res;
  if (domain == AF_UNIX || domain == AF_LOCAL)
    domain = AF_INET;
  res = SOCKET2HANDLE(socket (domain, type, proto));
  if (res == ASSUAN_INVALID_FD)
    gpg_err_set_errno (_assuan_sock_wsa2errno (WSAGetLastError ()));
  return res;
#else
  return socket (domain, type, proto);
#endif
}


int
_assuan_sock_connect (assuan_context_t ctx, assuan_fd_t sockfd,
		      struct sockaddr *addr, int addrlen)
{
#ifdef HAVE_W32_SYSTEM
  if (addr->sa_family == AF_LOCAL || addr->sa_family == AF_UNIX)
    {
      struct sockaddr_in myaddr;
      struct sockaddr_un *unaddr;
      unsigned short port;
      char nonce[16];
      int ret;
      
      unaddr = (struct sockaddr_un *)addr;
      if (read_port_and_nonce (unaddr->sun_path, &port, nonce))
        return -1;
      
      myaddr.sin_family = AF_INET;
      myaddr.sin_port = htons (port); 
      myaddr.sin_addr.s_addr = htonl (INADDR_LOOPBACK);
  
      /* Set return values.  */
      unaddr->sun_family = myaddr.sin_family;
      unaddr->sun_port = myaddr.sin_port;
      unaddr->sun_addr.s_addr = myaddr.sin_addr.s_addr;
  
      ret = connect (HANDLE2SOCKET(sockfd), 
                     (struct sockaddr *)&myaddr, sizeof myaddr);
      if (!ret)
        {
          /* Send the nonce. */
          ret = _assuan_write (ctx, sockfd, nonce, 16);
          if (ret >= 0 && ret != 16)
            {
              gpg_err_set_errno (EIO);
              ret = -1;
            }
        }
      return ret;
    }
  else
    {
      int res;
      res = connect (HANDLE2SOCKET (sockfd), addr, addrlen);
      if (res < 0)
	gpg_err_set_errno (_assuan_sock_wsa2errno (WSAGetLastError ()));
      return res;
    }      
#else
  return connect (sockfd, addr, addrlen);
#endif
}


int
_assuan_sock_bind (assuan_context_t ctx, assuan_fd_t sockfd,
		   struct sockaddr *addr, int addrlen)
{
#ifdef HAVE_W32_SYSTEM
  if (addr->sa_family == AF_LOCAL || addr->sa_family == AF_UNIX)
    {
      struct sockaddr_in myaddr;
      struct sockaddr_un *unaddr;
      int filefd;
      FILE *fp;
      int len = sizeof myaddr;
      int rc;
      char nonce[16];

      if (get_nonce (nonce, 16))
        return -1;

      unaddr = (struct sockaddr_un *)addr;

      myaddr.sin_port = 0;
      myaddr.sin_family = AF_INET;
      myaddr.sin_addr.s_addr = htonl (INADDR_LOOPBACK);

      filefd = open (unaddr->sun_path, 
                     (O_WRONLY|O_CREAT|O_EXCL|O_BINARY), 
                     (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP));
      if (filefd == -1)
        {
          if (errno == EEXIST)
            gpg_err_set_errno (WSAEADDRINUSE);
          return -1;
        }
      fp = fdopen (filefd, "wb");
      if (!fp)
        { 
          int save_e = errno;
          close (filefd);
          gpg_err_set_errno (save_e);
          return -1;
        }

      rc = bind (HANDLE2SOCKET (sockfd), (struct sockaddr *)&myaddr, len);
      if (!rc)
        rc = getsockname (HANDLE2SOCKET (sockfd), 
                          (struct sockaddr *)&myaddr, &len);
      if (rc)
        {
          int save_e = errno;
          fclose (fp);
          DeleteFile (unaddr->sun_path);
          gpg_err_set_errno (save_e);
          return rc;
        }
      fprintf (fp, "%d\n", ntohs (myaddr.sin_port));
      fwrite (nonce, 16, 1, fp);
      fclose (fp);

      return 0;
    }
  else
    {
      int res = bind (HANDLE2SOCKET(sockfd), addr, addrlen);
      if (res < 0)
	gpg_err_set_errno ( _assuan_sock_wsa2errno (WSAGetLastError ()));
      return res;
    }
#else
  return bind (sockfd, addr, addrlen);
#endif
}


int
_assuan_sock_get_nonce (assuan_context_t ctx, struct sockaddr *addr,
			int addrlen, assuan_sock_nonce_t *nonce)
{
#ifdef HAVE_W32_SYSTEM
  if (addr->sa_family == AF_LOCAL || addr->sa_family == AF_UNIX)
    {
      struct sockaddr_un *unaddr;
      unsigned short port;

      if (sizeof nonce->nonce != 16)
        {
          gpg_err_set_errno (EINVAL);
          return -1;
        }
      nonce->length = 16;
      unaddr = (struct sockaddr_un *)addr;
      if (read_port_and_nonce (unaddr->sun_path, &port, nonce->nonce))
        return -1;
    }
  else
    {
      nonce->length = 42; /* Arbitrary value to detect unitialized nonce. */
      nonce->nonce[0] = 42;
    }
#else
  (void)addr;
  (void)addrlen;
  nonce->length = 0;
#endif
  return 0;
}
 
 
int
_assuan_sock_check_nonce (assuan_context_t ctx, assuan_fd_t fd,
			  assuan_sock_nonce_t *nonce)
{
#ifdef HAVE_W32_SYSTEM
  char buffer[16], *p;
  size_t nleft;
  int n;

  if (sizeof nonce->nonce != 16)
    {
      gpg_err_set_errno (EINVAL);
      return -1;
    }

  if (nonce->length == 42 && nonce->nonce[0] == 42)
    return 0; /* Not a Unix domain socket.  */

  if (nonce->length != 16)
    {
      gpg_err_set_errno (EINVAL);
      return -1;
    }

  p = buffer;
  nleft = 16;
  while (nleft)
    {
      n = _assuan_read (ctx, SOCKET2HANDLE(fd), p, nleft);
      if (n < 0 && errno == EINTR)
        ;
      else if (n < 0 && errno == EAGAIN)
        Sleep (100);
      else if (n < 0)
        return -1;
      else if (!n)
        {
          gpg_err_set_errno (EIO);
          return -1;
        }
      else
        {
          p += n;
          nleft -= n;
        }
    }
  if (memcmp (buffer, nonce->nonce, 16))
    {
      gpg_err_set_errno (EACCES);
      return -1;
    }
#else
  (void)fd;
  (void)nonce;
#endif
  return 0;
}


/* Public API.  */

/* In the future, we can allow access to sock_ctx, if that context's
   hook functions need to be overridden.  There can only be one global
   assuan_sock_* user (one library or one application) with this
   convenience interface, if non-standard hook functions are
   needed.  */
static assuan_context_t sock_ctx;

gpg_error_t
assuan_sock_init ()
{
  gpg_error_t err;
#ifdef HAVE_W32_SYSTEM
  WSADATA wsadat;
#endif

  if (sock_ctx != NULL)
    return 0;

  err = assuan_new (&sock_ctx);
	
#ifdef HAVE_W32_SYSTEM
  if (! err)
    WSAStartup (0x202, &wsadat);
#endif

  return err;
}


void
assuan_sock_deinit ()
{
  if (sock_ctx == NULL)
    return;

#ifdef HAVE_W32_SYSTEM
  WSACleanup ();
#endif

  assuan_release (sock_ctx);
  sock_ctx = NULL;
}
  

int
assuan_sock_close (assuan_fd_t fd)
{
  return _assuan_close (sock_ctx, fd);
}

assuan_fd_t 
assuan_sock_new (int domain, int type, int proto)
{
  return _assuan_sock_new (sock_ctx, domain, type, proto);
}

int
assuan_sock_connect (assuan_fd_t sockfd, struct sockaddr *addr, int addrlen)
{
  return _assuan_sock_connect (sock_ctx, sockfd, addr, addrlen);
}

int
assuan_sock_bind (assuan_fd_t sockfd, struct sockaddr *addr, int addrlen)
{
  return _assuan_sock_bind (sock_ctx, sockfd, addr, addrlen);
}

int
assuan_sock_get_nonce (struct sockaddr *addr, int addrlen, 
                       assuan_sock_nonce_t *nonce)
{     
  return _assuan_sock_get_nonce (sock_ctx, addr, addrlen, nonce);
}

int
assuan_sock_check_nonce (assuan_fd_t fd, assuan_sock_nonce_t *nonce)
{
  return _assuan_sock_check_nonce (sock_ctx, fd, nonce);
}
