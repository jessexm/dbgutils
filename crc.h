/* Copyright (C) 2002, 2003, 2004 Zilog, Inc.
 *
 * $Id: crc.h,v 1.1 2004/08/03 14:23:48 jnekl Exp $
 *
 * Cyclic redundancy check routines.
 */

#ifndef	CRC_HEADER
#define	CRC_HEADER

#include	<inttypes.h>

#ifdef	__cplusplus
extern "C" {
#endif

uint16_t crc_ccitt(uint16_t, uint8_t *, size_t);

#ifdef	__cplusplus
}
#endif

#endif	/* CRC_HEADER */

