/* Copyright (C) 2002, 2003, 2004 Zilog, Inc.
 *
 * $Id: cfg.cpp,v 1.1 2004/08/03 14:23:48 jnekl Exp $
 *
 * These routines are used to read key/value pairs from a
 * config file.
 */

#include	<stdio.h>
#include	<stdlib.h>
#include	<sys/stat.h>
#include	<errno.h>
#include	<string.h>
#include	<assert.h>
#include	"xmalloc.h"

#include	"getdelim.h"
#include	"cfg.h"

/**************************************************************/

cfgfile::cfgfile(void)
{
	filename = NULL;
	file = NULL;
	data = NULL;
	size = 0;

	items = NULL;
	num_items = 0;

	return;
}

/**************************************************************/

cfgfile::~cfgfile(void)
{
	close();
}

/**************************************************************/

int cfgfile::close(void)
{
	size_t i;

	if(filename) {
		free(filename);
		filename = NULL;
	}

	if(file) {
		fclose(file);
		file = NULL;
	}

	if(data) {
		free(data);
		data = NULL;
	}

	size = 0;

	for(i=0; i<num_items; i++) {
		if(items[i].key) {
			free(items[i].key);
			items[i].key = NULL;
		}
		if(items[i].value) {
			free(items[i].value);
			items[i].value = NULL;
		}
	}

	if(items) {
		free(items);
		items = NULL;
	}

	return 0;
}

/**************************************************************/

int cfgfile::open(const char *name)
{
	int err;
	struct stat stat;

	assert(name != NULL);

	if(file) {
		close();
	}

	filename = xstrdup(name);

	file = fopen(name, "rb");
	if(file == NULL) {
		close();
		errno = ENOENT;
		return -1;
	}

	err = fstat(fileno(file), &stat);
	if(err) {
		perror("fstat");
		close();
		return -1;
	}

	data = (char *)xmalloc(stat.st_size);

	size = fread((void *)data, 1, stat.st_size, file);
	if((int)size != stat.st_size) {
		perror("fread");
		close();
		return -1;
	}

	err = parse();
	if(err) {
		return -1;
	}

	if(data) {
		free(data);
		data = NULL;
		size = 0;
	}

	return 0;
}

/**************************************************************
 * This will get the next line from our buffer. When no more
 * data is available or if an error occurs, this function 
 * returns NULL.
 */

char *cfgfile::getline(void)
{
	static char *line = NULL;
	static size_t line_size = 0;
	static char *next = NULL;
	char *end, *tmp;
	size_t next_size;

	if(data == NULL) {
		errno = 0;
		return NULL;
	}

	if(next == NULL) {
		next = data;
	}

	if(next >= data+size) {
		next = NULL;
		if(line) {
			free(line);
			line = NULL;
			line_size = 0;
		}
		errno = 0;
		return NULL;
	}

	end = (char *)memchr((void *)next, '\n', size-(next-data));
	if(end == NULL) {
		end = data+size-1;
	}

	next_size = end - next + 1;
	if(next_size >= line_size) {
		tmp = (char *)xrealloc(line, next_size+1);
		line = tmp;
		line_size = next_size+1;
	}

	memset(line, '\0', line_size);
	memcpy(line, next, next_size);

	next = end+1;
		
	return line;
}

/**************************************************************/

char *cfgfile::trim(char *data, char *delimiters)
{
	char *ptr;

	assert(data != NULL);
	assert(delimiters != NULL);

	while(*data && strchr(delimiters, *data)) {
		data++;
	}

	if(!*data) {
		return NULL;
	}

	ptr = strchr(data, '\0');

	while(strchr(delimiters, *--ptr));

	*++ptr = '\0';

	return data;
}

/**************************************************************/

int cfgfile::add_item(char *key, char *value)
{
	void *ptr;

	ptr = xrealloc(items, sizeof(struct items_t) * (num_items+1));
	items = (struct items_t *)ptr;

	items[num_items].key = xstrdup(key);

	if(!value) {
		value = "";
	}

	items[num_items].value = xstrdup(value);

	num_items++;

	return 0;
}

/**************************************************************/

int cfgfile::parse(void)
{
	int err;
	char *line;
	char *ptr;
	char *key;
	char *value;
	int lineno;

	lineno = 0;

	while((line = getline())) {

		lineno++;

		/* strip out '#' comments */
		ptr = strchr(line, '#');
		if(ptr) {
			*ptr = '\0';
		}
		/* strip out ';' comments */
		ptr = strchr(line, ';');
		if(ptr) {
			*ptr = '\0';
		}
		/* skip blank lines */
		if(*(line + strspn(line, " \t\r\n")) == '\0') {
			continue;
		}

		/* find key/value pair */
		ptr = strchr(line, '=');
		if(!ptr) {
			fprintf(stderr, "%s:%d: invalid key/value\n",
			    filename, lineno);
			continue;
		}

		*ptr++ = '\0';
		key = trim(line, " \t\r\n");
		value = trim(ptr, " \t\r\n");

		if(!key) {
			fprintf(stderr, "%s:%d: invalid key/value\n",
			    filename, lineno);
			continue;
		}

		err = add_item(key, value);
		if(err) {
			return -1;
		}
	}
	
	return 0;
}

/**************************************************************/

char *cfgfile::get(char *key)
{
	size_t i;

	for(i=0; i<num_items; i++) {
		if(strcmp(items[i].key, key) == 0) {
			return items[i].value;
		}
	}

	return NULL;
}

/**************************************************************/


