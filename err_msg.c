/* Copyright (C) 2002, 2003, 2004 Zilog, Inc.
 *
 * $Id: err_msg.c,v 1.1 2004/08/03 14:23:48 jnekl Exp $
 *
 * Buffer used for error messages.
 */

#include	<stdio.h>
#include	"err_msg.h"

static char err_buff[BUFSIZ];
char *err_msg = err_buff;
const size_t err_len = sizeof(err_buff);

