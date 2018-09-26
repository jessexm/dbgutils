/* Copyright (C) 2002, 2003, 2004 Zilog, Inc.
 *
 * $Id: trace.cpp,v 1.2 2005/10/20 18:39:37 jnekl Exp $
 *
 * These routines are used to display trace frames for the
 * Z8 Encore emulator. This is used for diagnostic purposes only.
 */

#include	<string.h>
#include	<stdio.h>
#include	<signal.h>
#include	<ctype.h>
#include	<assert.h>
#include	<unistd.h>
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<readline/readline.h>
#include	<readline/history.h>
#include	"xmalloc.h"

#include	"ez8dbg.h"
#include	"ocd.h"
#include	"ocd_serial.h"
#include	"ocd_parport.h"
#include	"server.h"
#include	"disassembler.h"
#include	"cfg.h"
#include	"hexfile.h"
#include	"dump.h"
#include	"md5.h"

/**************************************************************/

extern int esc_key;
extern ez8dbg *ez8;
extern char *get_inst_str(uint16_t);

/**************************************************************/

bool trce_available(void)
{
	uint16_t revid;

	revid = ez8->rd_revid();

	if(revid & 0x8000) {
		return 1;
	} else {
		return 0;
	}
}

/**************************************************************
 * This routine will read and disassembe the instruction at
 * the specified address.
 */

char *get_inst_str(uint16_t addr)
{
	int size;
	uint8_t inst[5];
	static char buff[16];

	memset(buff, 0x00, sizeof(buff));

	if(0x10000 - addr < (int)sizeof(inst)) {
		size = 0x10000 - addr;
	} else {
		size = sizeof(inst);
	}
	
	ez8->read_mem(addr, inst, size);

	size = disassemble(buff, sizeof(buff), inst, addr);

	assert(size > 0 && size <= 5);

	if(0x10000 - addr < size) {
		strncpy(buff, "ill", sizeof(buff)-1);
		size = 0x10000 - addr;
	}

	return buff;
}

/**************************************************************/

void trce_go(void)
{
	if(ez8->isrunning() == 0) {
		printf("Running program\n");
		ez8->run();
	} else {
		printf("Program already running\n");
	}

	return;
}

/**************************************************************/

void trce_stop(void)
{
	if(ez8->isrunning() > 0) {
		printf("Stopping program\n");
		ez8->stop();
	} else {
		printf("Program already stopped\n");
	}

	return;
}

/**************************************************************/

void display_trce_registers(void)
{
	int i;
	uint8_t trce_status;
	uint8_t trce_ctl;
	uint16_t trce_wr_ptr;
	struct trce_event event;

	trce_status = ez8->rd_trce_status();
	trce_ctl = ez8->rd_trce_ctl();
	trce_wr_ptr = ez8->rd_trce_wr_ptr();

	if(!ez8->state(ez8->state_stopped)) {
		printf("CPU is running\n");
	} else {
		printf("CPU is stopped\n");
	}

	printf("\tTRCESTAT:  %02X\n", trce_status);
	printf("\tTRCECTL:   %02X\n", trce_ctl);
	printf("\tTRCEWRPTR: %04X\n", trce_wr_ptr);

	for(i=0; i<4; i++) {
		ez8->rd_trce_event(i, &event);

		printf("\tTRCEEVENT%d:\n", i);
		printf("\t    CTL:   %02X\n", event.ctl);
		printf("\t    MASK:  %04X%02X%02X%04X\n", 
		    event.mask.reg_addr, event.mask.reg_data,
		    event.mask.cpu_flags, event.mask.pc);
		printf("\t    DATA:  %04X%02X%02X%04X\n", 
		    event.data.reg_addr, event.data.reg_data,
		    event.data.cpu_flags, event.data.pc);
	}

	return;
}

/**************************************************************/

void write_trce_registers(void)
{
	char key;
	char *buff;
	char *tail;
	unsigned int data;
	uint8_t index;
	struct trce_event event;

	rl_num_chars_to_read = 1;
	buff = readline("Write Trce[C]tl, Trce[E]vent ? ");
	rl_num_chars_to_read = 0;
	if(!buff) {
		printf("Abort\n");
		return;
	}
	key = toupper(*buff);
	free(buff);
	if(esc_key) {
		esc_key = 0;
		printf("\nAbort\n");
		return;
	}

	switch(key) {
	case 'C':

		buff = readline("Data: ");
		if(!buff) {
			printf("\n");
			return;
		}
		if(esc_key) {
			esc_key = 0;
			printf("\nAbort\n");
			free(buff);
			return;
		}

		data = strtoul(buff, &tail, 16);
		if(!tail || *tail || tail == buff) {
			printf("Invalid Number\n");
			free(buff);
			return;
		}
		free(buff);

		if(data > 0xff) {
			printf("Number out of range\n");
			return;
		}

		ez8->wr_trce_ctl(data);

		break;
	case 'E':

		buff = readline("Event Number: ");
		if(!buff) {
			printf("\n");
			return;
		}
		if(esc_key) {
			esc_key = 0;
			printf("\nAbort\n");
			free(buff);
			return;
		}

		data = strtoul(buff, &tail, 16);
		if(!tail || *tail || tail == buff) {
			printf("Invalid Number\n");
			free(buff);
			return;
		}
		free(buff);

		if(data >= 4) {
			printf("Event index out of range\n");
			return;
		}

		index = data;

		buff = readline("Ctl: ");
		if(!buff) {
			printf("\n");
			return;
		}
		if(esc_key) {
			esc_key = 0;
			printf("\nAbort\n");
			free(buff);
			return;
		}

		data = strtoul(buff, &tail, 16);
		if(!tail || *tail || tail == buff) {
			printf("Invalid Number\n");
			free(buff);
			return;
		}
		free(buff);

		if(data > 0xff) {
			printf("Out of range\n");
			return;
		}

		event.ctl = data;

		buff = readline("RegAddr Mask: ");
		if(!buff) {
			printf("\n");
			return;
		}
		if(esc_key) {
			esc_key = 0;
			printf("\nAbort\n");
			free(buff);
			return;
		}

		data = strtoul(buff, &tail, 16);
		if(!tail || *tail) {
			printf("Invalid Number\n");
			free(buff);
			return;
		}
		free(buff);

		if(data > 0xffff) {
			printf("Out of range\n");
			return;
		}

		event.mask.reg_addr = data;

		buff = readline("RegAddr Data: ");
		if(!buff) {
			printf("\n");
			return;
		}
		if(esc_key) {
			esc_key = 0;
			printf("\nAbort\n");
			free(buff);
			return;
		}

		data = strtoul(buff, &tail, 16);
		if(!tail || *tail) {
			printf("Invalid Number\n");
			free(buff);
			return;
		}
		free(buff);

		if(data > 0xffff) {
			printf("Out of range\n");
			return;
		}

		event.data.reg_addr = data;

		buff = readline("RegData Mask: ");
		if(!buff) {
			printf("\n");
			return;
		}
		if(esc_key) {
			esc_key = 0;
			printf("\nAbort\n");
			free(buff);
			return;
		}

		data = strtoul(buff, &tail, 16);
		if(!tail || *tail || tail == NULL) {
			printf("Invalid Number\n");
			free(buff);
			return;
		}
		free(buff);

		if(data > 0xff) {
			printf("Out of range\n");
			return;
		}

		event.mask.reg_data = data;

		buff = readline("RegData Data: ");
		if(!buff) {
			printf("\n");
			return;
		}
		if(esc_key) {
			esc_key = 0;
			printf("\nAbort\n");
			free(buff);
			return;
		}

		data = strtoul(buff, &tail, 16);
		if(!tail || *tail || tail == NULL) {
			printf("Invalid Number\n");
			free(buff);
			return;
		}
		free(buff);

		if(data > 0xff) {
			printf("Out of range\n");
			return;
		}

		event.data.reg_data = data;

		buff = readline("CpuFlags Mask: ");
		if(!buff) {
			printf("\n");
			return;
		}
		if(esc_key) {
			esc_key = 0;
			printf("\nAbort\n");
			free(buff);
			return;
		}

		data = strtoul(buff, &tail, 16);
		if(!tail || *tail) {
			printf("Invalid Number\n");
			free(buff);
			return;
		}
		free(buff);

		if(data > 0xff) {
			printf("Out of range\n");
			return;
		}

		event.mask.cpu_flags = data;

		buff = readline("CpuFlags Data: ");
		if(!buff) {
			printf("\n");
			return;
		}
		if(esc_key) {
			esc_key = 0;
			printf("\nAbort\n");
			free(buff);
			return;
		}

		data = strtoul(buff, &tail, 16);
		if(!tail || *tail) {
			printf("Invalid Number\n");
			free(buff);
			return;
		}
		free(buff);

		if(data > 0xff) {
			printf("Out of range\n");
			return;
		}

		event.data.cpu_flags = data;

		buff = readline("ProgCntr Mask: ");
		if(!buff) {
			printf("\n");
			return;
		}
		if(esc_key) {
			esc_key = 0;
			printf("\nAbort\n");
			free(buff);
			return;
		}

		data = strtoul(buff, &tail, 16);
		if(!tail || *tail) {
			printf("Invalid Number\n");
			free(buff);
			return;
		}
		free(buff);

		if(data > 0xffff) {
			printf("Out of range\n");
			return;
		}

		event.mask.pc = data;

		buff = readline("ProgCntr Data: ");
		if(!buff) {
			printf("\n");
			return;
		}
		if(esc_key) {
			esc_key = 0;
			printf("\nAbort\n");
			free(buff);
			return;
		}

		data = strtoul(buff, &tail, 16);
		if(!tail || *tail) {
			printf("Invalid Number\n");
			free(buff);
			return;
		}
		free(buff);

		if(data > 0xffff) {
			printf("Out of range\n");
			return;
		}

		event.data.pc = data;

		ez8->wr_trce_event(index, &event);

		break;
	default:
		printf("\nAbort\n");
		return;
	}

	return;
}

/**************************************************************/

void read_trce_buffer(void)
{
	int i;
	char *buff;
	char *tail;
	unsigned int data;
	uint16_t start;
	uint16_t size;
	struct trce_frame *frames;
	
	buff = readline("Start: ");
	if(!buff) {
		printf("\n");
		return;
	}
	if(esc_key) {
		esc_key = 0;
		printf("\nAbort\n");
		free(buff);
		return;
	}

	data = strtoul(buff, &tail, 16);
	if(!tail || *tail || tail == buff) {
		printf("Invalid Number\n");
		free(buff);
		return;
	}
	free(buff);

	if(data > 0xffff) {
		printf("Number out of range\n");
		return;
	}

	start = data;
	
	buff = readline("Size: ");
	if(!buff) {
		printf("\n");
		return;
	}
	if(esc_key) {
		esc_key = 0;
		printf("\nAbort\n");
		free(buff);
		return;
	}

	data = strtoul(buff, &tail, 16);
	if(!tail || *tail || tail == buff) {
		printf("Invalid Number\n");
		free(buff);
		return;
	}
	free(buff);

	if(data > 0xffff) {
		printf("Number out of range\n");
		return;
	}

	size = data;

	frames = (struct trce_frame *)
	    xmalloc(sizeof(struct trce_frame) * size);

	try {
		ez8->rd_trce_buff(start, frames, size);
	} catch(char *err) {
		free(frames);
		throw err;
	}

	for(i=0; i<size; i++) {
		if((frames[i].data[2] & 0xf0) == 0xf0) {
			printf("Timestamp: %08X\n", 
			    (frames[i].data[4] << 24) |
			    (frames[i].data[5] << 16) |
			    (frames[i].data[6] <<  8) |
			    (frames[i].data[7]));
			continue;
		}

		switch(frames[i].data[2] & 0xc0) {
		case 0x40:
			printf("PC:%04X  ", (frames[i].data[6] << 8) |
			    frames[i].data[7]);
			break;
		case 0x80:
			printf("IntAck   ");
			break;
		case 0xc0:
			printf("DmaAck   ");
			break;
		default:
			printf("Idle     ");
			break;
		}

		printf("Flags:%02X  ", frames[i].data[5]);
		printf("SP:%04X  ", (frames[i].data[0] << 8)
		    | frames[i].data[1]);

		switch(frames[i].data[2] & 0x30) {
		case 0x10:
			printf("RegWr A:%03X D:%02X  ", frames[i].data[3] |
			    ((frames[i].data[2] & 0x0f) << 8), 
			    frames[i].data[4]);
			break;
		case 0x20:
			printf("RegRd A:%03X D:%02X  ", frames[i].data[3] |
			    ((frames[i].data[2] & 0x0f) << 8), 
			    frames[i].data[4]);
			break;
		case 0x30:
		default:
			printf("                  ");
		}

		switch(frames[i].data[2] & 0xc0) {
		case 0x40:
			printf("%-16s", get_inst_str(
			    (frames[i].data[6] << 8) | frames[i].data[7]));
		default:
			putchar('\n');
			break;
		}
	}

	free(frames);

	return;
}

/**************************************************************/

void dump_trce_buffer(void)
{
	int i;
	char *buff;
	char *tail;
	unsigned int data;
	uint16_t start;
	uint16_t size;
	struct trce_frame *frames;
	
	buff = readline("Start: ");
	if(!buff) {
		printf("\n");
		return;
	}
	if(esc_key) {
		esc_key = 0;
		printf("\nAbort\n");
		free(buff);
		return;
	}

	data = strtoul(buff, &tail, 16);
	if(!tail || *tail || tail == NULL) {
		printf("Invalid Number\n");
		free(buff);
		return;
	}
	free(buff);

	if(data > 0xffff) {
		printf("Number out of range\n");
		return;
	}

	start = data;
	
	buff = readline("Size: ");
	if(!buff) {
		printf("\n");
		return;
	}
	if(esc_key) {
		esc_key = 0;
		printf("\nAbort\n");
		free(buff);
		return;
	}

	data = strtoul(buff, &tail, 16);
	if(!tail || *tail || tail == buff) {
		printf("Invalid Number\n");
		free(buff);
		return;
	}
	free(buff);

	if(data > 0xffff) {
		printf("Number out of range\n");
		return;
	}

	size = data;

	frames = (struct trce_frame *)
	    xmalloc(sizeof(struct trce_frame) * size);

	try {
		ez8->rd_trce_buff(start, frames, size);
	} catch(char *err) {
		free(frames);
		throw err;
	}

	for(i=0; i<size; i++) {
		printf("%08X: %02X%02X-%02X%02X-%02X%02X-%02X%02X\n", i,
		    frames[i].data[0], frames[i].data[1],
		    frames[i].data[2], frames[i].data[3],
		    frames[i].data[4], frames[i].data[5],
		    frames[i].data[6], frames[i].data[7]);
	}

	free(frames);

	return;
}

/**************************************************************/

void display_trce_help(void)
{
	printf("Trace Subsystem Help\n\n");
	printf("\tG - go\n");
	printf("\tH - display trace help\n");
	printf("\tS - stop\n");
	printf("\tT - display all trace registers\n");
	printf("\tW - write trace registers\n");
	printf("\tR - read trace buffer\n");
	printf("\tD - dump raw trace frames\n");
	printf("\tQ - exit trace subsystem\n");

	return;
}

/**************************************************************/

void trce_subsystem(void)
{
	char *buff;
	char key;

	printf("Type 'H' for help on Trace Subsystem\n");

	buff = NULL;

	do {
		printf("\n");

		do {
			if(buff != NULL) {
				free(buff);
				buff = NULL;
			}

			rl_num_chars_to_read = 1;
			buff = readline("ez8mon-trce> ");
			rl_num_chars_to_read = 0;

			if(esc_key) {
				esc_key = 0;
				rl_ding();
				printf("\r");
			}
		} while(buff == NULL || *buff == '\0');

		key = toupper(*buff);
		free(buff);
		buff = NULL;

		switch(key) {
		case 'G':
			trce_go();
			break;
		case 'D':
			dump_trce_buffer();
			break;
		case 'H':
			display_trce_help();
			break;
		case 'R':
			read_trce_buffer();
			break;
		case 'T':
			display_trce_registers();
			break;
		case 'S':
			trce_stop();
			break;
		case 'Q':
			if(ez8->isrunning() > 0) {
				trce_stop();
			}
			break;
		case 'W':
			write_trce_registers();
			break;
		default:
			printf("Unknown command\n");
			break;
		}

	} while(key != 'Q');

	return;
}

/**************************************************************/


