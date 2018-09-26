/* $Id: xmalloc.h,v 1.1 2004/08/03 14:23:48 jnekl Exp $
 */

#ifndef	XMALLOC_HEADER
#define	XMALLOC_HEADER

#include	<stdlib.h>

#ifdef	__cplusplus
extern "C" {
#endif

void *xmalloc(size_t);
void *xcalloc(size_t, size_t);
void *xrealloc(void *, size_t);
char *xstrdup(const char *);

#ifdef	__cplusplus
};
#endif

#endif	/* XMALLOC_HEADER */

