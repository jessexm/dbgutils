/* Copyright (C) 2002, 2003, 2004 Zilog, Inc.
 *
 * $Id: sockstream.c,v 1.1 2004/08/03 14:23:48 jnekl Exp $
 *
 * These functions implement streaming i/o buffers for a
 * network socket interface. 
 * 
 * Unix applications can simply use the normal stream i/o 
 * routines since a socket is a real file descriptor. Just
 * dup() the descriptor, then open up one stream for writing 
 * and another for reading.
 *
 * Microsoft screwed everything up. A socket is not a real 
 * file descriptor, it is a socket descriptor. This means the 
 * standard file i/o stream buffering functions do not work.
 * We have to use our own stream buffering routines on 
 * windows systems.
 */

#include	<stdio.h>
#include	<stdlib.h>
#include	<stdarg.h>
#include	<unistd.h>
#include	<string.h>
#include	<errno.h>
#include	"xmalloc.h"

#ifndef	_WIN32
#include	<sys/types.h>
#include	<sys/socket.h>
#endif

#include	"sockstream.h"

/**************************************************************
 * ss_open
 *
 * This function works similar to fdopen(). It creates a
 * stream buffer from an already open socket.
 *
 * This function return 0 upon success, -1 on error.
 */

int ss_open(int fd, SOCK *h)
{
	h->fd = -1;
	h->txbuff = NULL;
	h->rxbuff = NULL;
	h->txcnt = 0;
	h->rxcnt = 0;

	h->txbuff = xmalloc(BUFSIZ);
	h->rxbuff = xmalloc(BUFSIZ);

	h->fd = fd;

	return 0;
}

/**************************************************************
 * ss_close
 *
 * This function works similar to fclose(). It will flush any
 * pending data on the output buffer, close the descriptor, 
 * and free any buffers allocated for the stream.
 */

int ss_close(SOCK *h)
{
	int err1, err2, fd;

	err1 = ss_flush(h);

	free(h->txbuff);
	h->txbuff = NULL;
	free(h->rxbuff);
	h->rxbuff = NULL;
	fd = h->fd;
	h->fd = -1;
#ifndef	_WIN32
	err2 = close(fd);
#else
	err2 = closesocket(fd);
#endif
	if(err1 || err2) {
		return -1;
	}

	return 0;
}

/**************************************************************
 * ss_flush
 *
 * This function works similar to fflush(). It will write
 * any data waiting in the output buffer to the descriptor.
 *
 * This function return -1 on error, 0 upon success.
 */

int ss_flush(SOCK *h)
{
	int n;

	n = 1;

	while(h->txcnt > 0 && (n > 0 || (n < 0 && errno == EINTR))) {
		n = send(h->fd, h->txbuff, h->txcnt, 0);
		if(n > 0) {
			h->txcnt -= n;
			if(h->txcnt > 0) {
				memmove(h->txbuff, (char *)(h->txbuff)+n, 
				    h->txcnt);
			}
		}
	}

	if(n <= 0) {
		return -1;
	}

	return 0;
}

/**************************************************************
 * ss_gets
 * 
 * This functions works similar to fgets. It copies the data up 
 * to and including the first '\n' charater into *s, or the 
 * first n bytes of data if '\n' does not occur. This function
 * will NULL terminate *s if less than n bytes were copied 
 * to *s.
 */

char *ss_gets(char *s, size_t n, SOCK *h)
{
	void *ptr;
	char *p;
	int cnt;

	p = s;

	while(n > 0) {
		if(h->rxcnt > 0) {
			ptr = memchr(h->rxbuff, '\n', h->rxcnt);
			if(ptr) {
				cnt = (char *)ptr - (char *)h->rxbuff + 1;
			} else {
				cnt = h->rxcnt;
			}
			memcpy(p, h->rxbuff, cnt);
			h->rxcnt -= cnt;
			if(h->rxcnt > 0) {
				memmove(h->rxbuff, (char *)(h->rxbuff)+cnt, 
				    h->rxcnt);
			}

			n -= cnt;
			p += cnt;
			if(ptr || n <= 0) {
				if(n > 0) {
					*p = '\0';
				}
				return s;
			}
		}
		cnt = recv(h->fd, h->rxbuff, BUFSIZ, 0);
		if((cnt < 0 && errno != EINTR) || (cnt == 0)) {
			return NULL;
		}
		h->rxcnt = cnt;
	}

	return s;
}

/**************************************************************
 * ss_printf
 *
 * This function works similar to fprintf(). It writes a 
 * formatted string to the output buffer. If the output buffer
 * becomes full, this function will call ss_flush() to write
 * the output buffer.
 */

int ss_printf(SOCK *h, const char *fmt, ...)
{
	int err;
	va_list ap;
	ssize_t n, cnt;

	va_start(ap, fmt);

	n = BUFSIZ - h->txcnt;

	cnt = vsnprintf((char *)(h->txbuff)+h->txcnt, n, fmt, ap);
	if(cnt < 0 || cnt >= n) {
		err = ss_flush(h);
		if(err) {
			return -1;
		}
		n = BUFSIZ;
		cnt = vsnprintf(h->txbuff, n, fmt, ap);
		if(cnt >= n) {
			err = ss_flush(h);
			if(err) {
				return -1;
			}
		} else {
			h->txcnt = cnt;
		}
	} else {
		h->txcnt += cnt; 
	}

	va_end(ap);

	return 0;
}

/**************************************************************/


