/* Copyright (C) 2002, 2003, 2004 Zilog, Inc.
 *
 * $Id: hexfile.h,v 1.1 2004/08/03 14:23:48 jnekl Exp $
 */

#ifndef	HEXFILE_HEADER
#define	HEXFILE_HEADER

#include	<inttypes.h>

#ifdef	__cplusplus
extern "C" {
#endif

int rd_hexfile(uint8_t *, size_t, const char *);
int wr_hexfile(uint8_t *, size_t, size_t, const char *);

#ifdef	__cplusplus
}
#endif

#endif	/* HEXFILE_HEADER */

