/* Copyright (C) 2002, 2003, 2004 Zilog, Inc.
 *
 * $Id: err_msg.h,v 1.1 2004/08/03 14:23:48 jnekl Exp $
 * 
 * Static buffer for error messages.
 */

#ifndef	ERR_MSG_HEADER
#define	ERR_MSG_HEADER

#include	<stdlib.h>

#ifdef	__cplusplus
extern "C" {
#endif

extern char *err_msg;
extern const size_t err_len;

#ifdef	__cplusplus
}
#endif

#endif	/* ERR_MSG_HEADER */

