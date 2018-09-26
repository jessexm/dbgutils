/* $Id: gencrctable.c,v 1.1 2004/08/03 14:23:48 jnekl Exp $
 *
 * Utility to generate 'const uint8_t *crctable'.
 */

#include	<stdio.h>
#include	<stdlib.h>
#include	<inttypes.h>
#include	<ctype.h>

/**************************************************************/

#define CRC_CCITT_POLY 0x8408

int main(int argc, char **argv)
{
	int i;
	int j;
	uint16_t crc;
	
	printf("\nconst uint16_t *crctable = {");

	for(i=0; i<256; i++) {
		crc = i;
		for(j=0; j<8; j++) {
			if(crc & 0x01) {
				crc >>= 1;
				crc ^= CRC_CCITT_POLY;
			} else {
				crc >>= 1;
			}
		}
		if(i % 8 == 0) {
			printf("\n\t");
		}
		printf("0x%04X, ", crc);
	}
	printf("\n};\n\n");

	return EXIT_SUCCESS;	
}

/**************************************************************/

