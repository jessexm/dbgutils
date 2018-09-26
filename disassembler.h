/* Copyright (C) 2002, 2003, 2004 Zilog, Inc.
 *
 * $Id: disassembler.h,v 1.1 2004/08/03 14:23:48 jnekl Exp $
 */

#ifndef	DISASSEMBLER_HEADER
#define	DISASSEMBLER_HEADER

#ifdef	__cplusplus
extern "C" {
#endif


int disassemble(char *, size_t, uint8_t *, uint16_t);


#ifdef	__cplusplus
}
#endif

#endif	/* DISASSEMBLER_HEADER */


