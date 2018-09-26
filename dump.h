/* Copyright (C) 2002, 2003, 2004 Zilog, Inc.
 *
 * $Id: dump.h,v 1.1 2004/08/03 14:23:48 jnekl Exp $
 */

#ifndef	UTIL_HEADER
#define	UTIL_HEADER

#include	<inttypes.h>

#ifdef	__cplusplus
extern "C" {
#endif

void dump_data(int, uint8_t *, int);
void dump_data_repeat(int, uint8_t *, int, int);

#ifdef	__cplusplus
}
#endif

#endif	/* UTIL_HEADER */

