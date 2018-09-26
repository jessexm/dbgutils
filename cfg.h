/* Copyright (C) 2002, 2003, 2004 Zilog, Inc.
 *
 * $Id: cfg.h,v 1.1 2004/08/03 14:23:48 jnekl Exp $
 *
 * These routines are used to read/write a config file.
 */

#ifndef	CFG_HEADER
#define	CFG_HEADER

#include	<stdio.h>
#include	<stdlib.h>

class cfgfile
{
private:
	struct items_t {
		char *key;
		char *value;
	};

	char *filename;
	FILE *file;
	char *data;
	size_t size;

	struct items_t *items;
	size_t num_items;

	char *trim(char *, char *);
	char *getline(void);
	int parse(void);


	int add_item(char *, char *);

public:
	cfgfile(void);
	~cfgfile(void);

	int open(const char *);
	int close(void);
	char *get(char *);
};


#endif	/* CFG_HEADER */

