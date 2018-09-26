/* $Id: winunistd.h,v 1.2 2004/10/08 15:05:30 jnekl Exp $
 */

#ifndef	UNISTD_HEADER
#define	UNISTD_HEADER

#include	<unistd.h>

#ifdef	_WIN32

#include	<windows.h>

#define	usleep(time)	Sleep((time)%1000?((time)/1000)+1:(time)/1000)
#define	sleep(time)	Sleep((time)*1000)

#endif	/* _WIN32 */

#endif	/* UNISTD_HEADER */

/**************************************************************/

