/* Copyright (C) 2002, 2003, 2004 Zilog, Inc.
 *
 * $Id: sockstream.h,v 1.1 2004/08/03 14:23:48 jnekl Exp $
 *
 * These are stream buffered socket i/o routines. They 
 * perform the same as their file stream i/o counterparts.
 */

#ifndef	SOCKSTREAM_HEADER
#define	SOCKSTREAM_HEADER

#include	<stdio.h>
#include	<stdlib.h>
#include	<unistd.h>
#ifdef	_WIN32
#include	"winunistd.h"
#endif

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct _SOCK {
	int fd;
	void *txbuff;
	void *rxbuff;
	ssize_t txcnt;
	ssize_t rxcnt;
} SOCK;

int ss_open(int, SOCK *);
int ss_close(SOCK *);

char *ss_gets(char *, size_t, SOCK *);
int ss_printf(SOCK *, const char *, ...);
int ss_flush(SOCK *);

#ifdef	__cplusplus
}
#endif

#endif	/* SOCKSTREAM_HEADER */

