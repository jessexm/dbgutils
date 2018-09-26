/* $Id: getdelim.c,v 1.1 2004/08/03 14:23:48 jnekl Exp $
 *
 * This file contains the GNU functions getline & getdelim. These
 * functions are not part of the standard libc, so they may not
 * exist on all systems (such as solaris or windoze). Therfore 
 * they're included here.
 *
 * This file is DERIVED from the GNU glibc implementation of 
 * getdelim. Therfore it is governed under the GNU license.
 *
 * This file differs from the gnu implementation in that a lot
 * of the buffering optimizations (which are compiler and system
 * dependent) have been omitted.
 */

#define		_GNU_SOURCE
#include	<stdio.h>
#include	<stdlib.h>
#include	<unistd.h>

#include	<string.h>
#include	<errno.h>
#include	"xmalloc.h"

#ifdef	_WIN32
#include	"winunistd.h"
#endif

#include	"limits.h"

/****************************************************************/

#ifndef	HAVE_GETDELIM

ssize_t getdelim(char **lineptr, size_t *n, int delimiter, FILE *stream)
{
	char *line;
	char *p;
	size_t size;
	size_t copy;

	if (lineptr == NULL || n == NULL || stream == NULL) {
		errno = EINVAL;
		return -1;
	}

	if (ferror(stream)) {
		return -1;
	}

	if (*lineptr == NULL || *n < 2) {

		line = xrealloc(*lineptr, MAX_CANON);

		*lineptr = line;
		*n = MAX_CANON;
	}

	line = *lineptr;
	size = *n;

	copy = size;
	p = line;

	while (1) {

		size_t len;

		while (--copy > 0) {

			register int c = getc(stream);

			if (c == EOF) {
				/* GNU "DOCS" say that getdelim returns -1
				 * on EOF. However, the actual implemenataton
				 * will only return -1 on EOF if it did not
				 * read anything. If it read something before
				 * reaching EOF, it will return what it read 
				 * up to that point.
				 */
				if (p == line) {
					return -1;
				}
				*p = '\0';
				return p - line;
			}

			*p++ = c;
			if (c == delimiter) {
				*p = '\0';
				return p - line;
			}
		}

		len = p - line;
		size *= 2;
		line = xrealloc(line, size);

		*lineptr = line;
		*n = size;
		p = line + len;
		copy = size - len;
	}
}

#endif				/* HAVE_GETDELIM */

/****************************************************************/

