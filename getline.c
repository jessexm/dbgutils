/* $Id: getline.c,v 1.1 2004/08/03 14:23:48 jnekl Exp $
 *
 * This file contains the GNU function getline. Just calls 
 * getdelim.
 */

#define		_GNU_SOURCE
#include	<stdio.h>
#include	<unistd.h>

#ifdef	_WIN32
#include	"winunistd.h"
#endif

#include	"getdelim.h"

/****************************************************************/

#ifndef	HAVE_GETDELIM

ssize_t getline(char **lineptr, size_t *n, FILE *stream)
{
	return getdelim(lineptr, n, '\n', stream);
}

#endif				/* HAVE_GETDELIM */

/****************************************************************/


