/* Copyright (C) 2002, 2003, 2004 Zilog, Inc.
 *
 * $Id: crcgen.c,v 1.3 2008/10/02 17:54:06 jnekl Exp $
 *
 * This program will generate the CRC of an intel hexfile
 * that is returned by the Z8 Encore on-chip debugger.
 */

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<inttypes.h>
#include	<unistd.h>
#include	"xmalloc.h"

#include	"hexfile.h"
#include	"crc.h"

/**************************************************************/

#define	PROGNAME	"crcgen"
int memsize = 64 * 1024;
int zero_fill = 0;
int debug = 0;

extern const char *build;
const char *progname;

/**************************************************************/

void help(void)
{
printf("%s - build %s\n", progname, build);
printf(
"Usage: crcgen [OPTIONS] FILE\n"
"This utility will compute the CRC of memory from an intel hexfile.\n\n"
"  -h               show this help\n"
"  -s MEMSIZE       specify memory size\n"
"  -o OUTPUT        write results to FILE\n"
"  -z               fill with 00 instead of FF\n"
"  -d               debug\n\n");
printf(
"This utility will return the CRC of an intel hexfile.\n\n"
"The CRC polynomial used is x^16+x^12+x^5+1 (CRC-CCITT). The data is\n"
"shifted through the polynomial LSB first, so the polynomial and crc are\n"
"reversed. The CRC is preset to all one's and the final crc is inverted.\n");

	return;
}

/**************************************************************/

int setup(int argc, char **argv)
{
	int c;
	char *tail, *s;

	progname = *argv;
	s = strrchr(progname, '/');
	if(s) {
		progname = s+1;
	}
	s = strrchr(progname, '\\');
	if(s) {
		progname = s+1;
	}

	while((c = getopt(argc, argv, "hs:o:zd")) != EOF) {
		switch(c) {
		case '?':
			printf("Try '%s -h' for more information.\n", PROGNAME);
			return -1;
		case 'h':
			help();
			exit(EXIT_SUCCESS);
		case 's':
			memsize = strtod(optarg, &tail);
			if(tail == NULL || tail == optarg) {
				fprintf(stderr, "Invalid memsize \"%s\"\n", 
				    optarg);
				return -1;
			}
			while(*tail && strchr(" \t", *tail)) {
				tail++;
			}
			if(*tail == 'k' || *tail == 'K') {
				memsize *= 1024;
				tail++;
			}
			if(*tail != '\0') {
				fprintf(stderr, "Invalid suffix \"%s\"\n", 
				    tail);
				return -1;
			}
			break;
		case 'o':
			if(!freopen(optarg, "wb", stdout)) {
				perror("freopen");
				return -1;
			}
			break;
		case 'z':
			zero_fill = 1;
			break;
		case 'd':
			debug = 1;
			break;
		}
	}

	if(optind >= argc) {
		fprintf(stderr, "%s: too few arguments\n", PROGNAME);
		printf("Try '%s -h' for more information.\n", PROGNAME);
		return -1;
	} 

	return 0;
}

/**************************************************************/

int main(int argc, char **argv)
{
	int err;
	int i;
	uint16_t crc;
	uint8_t *buff;

	err = setup(argc, argv);
	if(err) {
		return EXIT_FAILURE;
	}

	buff = (uint8_t *)xmalloc(memsize * sizeof(uint8_t));

	for(i=optind; i<argc; i++) {
		memset(buff, zero_fill ? 0x00 : 0xff, memsize);
		err = rd_hexfile(buff, memsize, argv[i]);
		if(err) {
			continue;
		}
		if(debug) {
			int i;
			crc = 0;
			printf("%d:%04x\n", 0, ~crc & 0xffff);
			for(i=0; i<memsize; i++) {
				crc = crc_ccitt(crc, &buff[i], 1);
				printf("%d:%02x:%04x\n", 
				    i+1, buff[i], ~crc & 0xffff);
			}
			printf("inverted -> %04x\n", crc);
				
		}
		crc = crc_ccitt(0x0000, buff, memsize);
		printf("%s: %04x\n", argv[i], crc);
	}

	return EXIT_SUCCESS;
}

/**************************************************************/

