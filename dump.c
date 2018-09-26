/* Copyright (C) 2002, 2003, 2004 Zilog, Inc.
 *
 * $Id: dump.c,v 1.1 2004/08/03 14:23:48 jnekl Exp $
 *
 * Functions for displaying data blocks to terminal screen.
 */

#include	<stdio.h>
#include	<inttypes.h>
#include	<ctype.h>
#include	<string.h>
#include	<assert.h>

#include	"dump.h"

/**************************************************************
 * This function will display one line in hex and ascii.
 */

static void dump_line(int addr, uint8_t *data, char seek, char size)
{
	int i;

	printf("%04X: ", addr & 0xfff0);

	for(i=0; i<16; i++) {
		if(!(i & 7)) {
			printf(" ");
		}

		if(i < seek || i >= seek + size) {
			printf("   ");
		} else {
			printf("%02X ", data[i-seek]);
		}
	}

	printf(" ");

	for(i=0; i<16; i++) {

		if(i < seek || i >= seek + size) {
			printf(" ");
		} else {
			int c;

			c = data[i-seek];
			if(c & 0x80 || !isprint(c)) {
				printf(".");
			} else {
				printf("%c", c);
			}
		}
	}

	printf("\n");

	return;
}

/**************************************************************
 * This function will dump data to stdout in hex and ascii.
 */

void dump_data(int addr, uint8_t *data, int size)
{
	int seek;

	seek = addr & 0xf;

	while(size > 0) {
		int length;

		length = seek + size > 16 ? 16 - seek : size;
		dump_line(addr, data, seek, length);
		addr += length;
		data += length;
		size -= length;
		seek = 0;
	}

	return;
}

/**************************************************************
 * This function returns the number of 
 */

static size_t memcspn(uint8_t *data, int c, size_t n)
{
	int i;

	for(i=0; n; n--, i++) {
		if(*data++ != c) {
			return i;
		}
	}

	return i;
}

/**************************************************************
 * This function will dump data to stdout in hex and ascii.
 *
 * If there are large secions of repeating data, this
 * function will abbreviate the output and note the number
 * of times the data repeats.
 */

void dump_data_repeat(int addr, uint8_t *data, int size, int minrepeat)
{
	int seek;

	seek = addr & 0xf;

	while(size > 0) {
		int length;

		if(minrepeat) {
			int c, repeat;

			c = *data;
			repeat = memcspn(data, c, size);
			if(repeat >= minrepeat) {
				if(repeat < size) {
					if(seek) {
						repeat = ((repeat - 16 + seek) 
						    & ~0xf) + 16 - seek;
					} else {
						repeat &= ~0xf;
					}
				}
				printf("(addresses %04X thru %04X, "
				    "data %02X repeats %X times)\n", 
				    addr, addr+repeat-1, c, repeat);
				addr += repeat;
				data += repeat;
				size -= repeat;
				continue;
			}
		}

		length = seek + size > 16 ? 16 - seek : size;
		dump_line(addr, data, seek, length);
		addr += length;
		data += length;
		size -= length;
		seek = 0;
	}

	return;
}


/**************************************************************/

