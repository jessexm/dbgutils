/* $Id: getdelim.h,v 1.1 2004/08/03 14:23:48 jnekl Exp $
 */

#ifndef	GETDELIM_HEADER
#define	GETDELIM_HEADER

#if (! defined __sun__) && (! defined _WIN32)
#ifndef		_GNU_SOURCE
#define		_GNU_SOURCE
#endif
#include	<stdio.h>
#define		HAVE_GETDELIM
#else	

#include	<stdlib.h>
#include	<stddef.h>
#include	<unistd.h>
#include	"winunistd.h"

#ifdef	__cplusplus
extern "C" {
#endif

ssize_t getdelim(char **, size_t *, int, FILE *);
ssize_t getline(char **, size_t *, FILE *);

#ifdef	__cplusplus
}
#endif

#endif	/* ! defined __sun__ && ! defined _WIN32 */
#endif	/* STDIO_HEADER */

