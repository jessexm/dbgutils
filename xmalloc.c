/* $Id: xmalloc.c,v 1.1 2004/08/03 14:23:48 jnekl Exp $
 *
 * Memory allocation routines.
 *
 * These routines are used in place of the standard
 * memory allocation routines. They automatically
 * check for out-of-memory failures and will display
 * a message and exit if they do fail.
 */

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<errno.h>

void *xmalloc(size_t n)
{
	void *p;

	p = malloc(n);
	if(!p && n) {
		fprintf(stderr, "malloc failed allocating %d bytes\n", n);
		exit(EXIT_FAILURE);
	}

	return p;
}

void *xcalloc(size_t elem, size_t size)
{
	void *p;

	p = calloc(elem, size);
	if(!p && size) {
		size_t n;

		n = elem * size;
		fprintf(stderr, "calloc failed allocating %d bytes\n", n);
		exit(EXIT_FAILURE);
	}

	return p;
}

void *xrealloc(void *p, size_t n)
{
	void *r;

	r = realloc(p, n);
	if(!r && n) {
		fprintf(stderr, "realloc failed allocating %d bytes\n", n);
		exit(EXIT_FAILURE);
	}

	return r;

}

char *xstrdup(char *s)
{
	char *p;

	p = strdup(s);
	if(!p) {
		size_t n;

		n = strlen(s) + 1;
		fprintf(stderr, "strdup failed duplicating %d bytes\n", n);
		exit(EXIT_FAILURE);
	}

	return p;
}


