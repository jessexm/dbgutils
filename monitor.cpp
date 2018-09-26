/* Copyright (C) 2002, 2003, 2004 Zilog, Inc.
 *
 * $Id: monitor.cpp,v 1.3 2005/10/20 18:39:37 jnekl Exp $
 *
 * This implments the command line debugger api.
 */

#include	<string.h>
#include	<stdio.h>
#include	<assert.h>
#include	<readline/readline.h>
#include	<readline/history.h>
#include	"xmalloc.h"

#include	"version.h"
#include	"ez8dbg.h"
#include	"disassembler.h"
#include	"hexfile.h"
#include	"dump.h"
#include	"timer.h"

/**************************************************************/

extern const char *banner;
extern ez8dbg *ez8;
extern int esc_key;
extern int testmenu;
extern int repeat;
extern int verbose;
extern int show_times;

extern rl_command_func_t *tab_function;

uint8_t *filebuff = NULL;

extern bool trce_available(void);
extern void trce_subsystem(void);
extern void test_menu(void);

char *rl_err;

/**************************************************************
 * This is a readline hook. When the part is running, this
 * checks periodically to see if it is stopped.
 */

int check_if_running(void)
{
	try {
		if(!ez8->isrunning()) {
			rl_done = 1;
		}
	} catch(char *err) {
		rl_done = 1;
		rl_err = err;
	}

	return 0;
}

/**************************************************************
 * display_info()
 *
 * This monitor routine will display information about the
 * device, including hardware revision id, memory size, 
 * and memory crc checksum.
 */

void display_info(void)
{
	int baudrate;
	uint16_t revid;
	uint16_t reload;
	uint16_t crc;
	int size;
	uint8_t psi[21];

	revid = ez8->rd_revid();
	printf("REVISION IDENTIFIER:         %04X\n", revid);

	printf("PRODUCT SPECIFICATION INDEX: ");
	if(ez8->state(ez8->state_protected)) {
		printf("not available (read protect enabled)\n");
	} else {
		ez8->rd_info(0x40, psi, 20);
		psi[20] = '\0';
		if(!memcmp(psi, "Z8", 2)) {
			printf("%s\n", psi);
		} else {
			printf("invalid\n");
		}
	}

	size = ez8->memory_size();
	if(size) {
		printf("MEMORY SIZE:                 %dk = %04X-%04X\n", 
		    size / 1024, 0, size - 1);
	}

	crc = ez8->rd_crc();
	printf("CYCLIC REDUNDANCY CHECK:     %04X\n", crc);

	if(ez8->state(ez8->state_protected)) {
		printf("CODE PROTECT:                enabled\n");
	}

	baudrate = ez8->cached_baudrate();
	printf("BAUDRATE:                    %d\n", baudrate);
	if((reload = ez8->rd_reload())) {
		int sf;
		char *suffix;
		float freq;

		printf("BAUDRATE RELOAD:             %04X\n", reload);
		if(reload < 10) {
			sf = 1;
		} else if(reload < 100) {
			sf = 2;
		} else if(reload < 1000) {
			sf = 3;
		} else if(reload < 10000) {
			sf = 4;
		} else {
			sf = 5;
		}
	
		freq = ez8->cached_sysclk();
		if(freq > 1000000) {
			freq /= 1000000;
			suffix = "MHz";	
		} else if(freq > 1000) {
			freq /= 1000;
			suffix = "kHz";
		} else {
			suffix = "Hz";
		}

		printf("ESTIMATED FREQUENCY:         %.*g%s\n", 
		    sf, freq, suffix);
	}

	return;
}

/**************************************************************
 * disp_inst()
 *
 * This will disassemble and display the machine code and
 * instruction at the specified address.
 */

int disp_inst(uint16_t addr)
{
	int size;
	uint8_t inst[5];
	char buff[32];

	if(0x10000 - addr < (int)sizeof(inst)) {
		size = 0x10000 - addr;
		memset(buff, 0xff, sizeof(buff));
	} else {
		size = sizeof(inst);
	}
	
	ez8->read_mem(addr, inst, size);

	size = disassemble(buff, sizeof(buff), inst, addr);

	assert(size > 0 && size <= 5);

	if(ez8->breakpoint_set(addr)) {
		printf("B ");
	} else {
		printf("  ");
	}

	if(0x10000 - addr < size) {
		strncpy(buff, "ill", sizeof(buff)-1);
		size = 0x10000 - addr;
	}

	printf("%04X: ", addr);

	switch(size) {
	case 5:
		printf("%02X %02X %02X %02X %02X", 
		    inst[0], inst[1], inst[2], inst[3], inst[4]);
		break;
	case 4:
		printf("%02X %02X %02X %02X   ", 
		    inst[0], inst[1], inst[2], inst[3]);
		break;
	case 3:
		printf("%02X %02X %02X      ", 
		    inst[0], inst[1], inst[2]);
		break;
	case 2:
		printf("%02X %02X         ", 
		    inst[0], inst[1]);
		break;
	case 1:
		printf("%02X            ", 
		    inst[0]);
		break;
	}

	printf("   %s\n", buff);

	return size;
}

/**************************************************************
 * unassemble()
 *
 * This monitor command will disassemble instructions.
 */

void unassemble(void)
{
	char *buff;
	int addr;
	int size;
	uint16_t pc;
	int ninst;
	int num;
	char prompt[80];

	ninst = 0x10;
	pc = ez8->rd_pc();

	addr = pc;
	snprintf(prompt, sizeof(prompt) - 1, 
	    "Address to unassemble [%04X]: ", addr);
	buff = readline(prompt);
	if(!buff) {
		printf("\n");
		return;
	}
	if(esc_key) {
		esc_key = 0;
		printf("\nAbort\n");
		free(buff);
		buff = NULL;
		return;
	}

	if(*buff) {
		char *tail;

		addr = strtol(buff, &tail, 16);
		if(!tail || *tail || tail == buff) {
			printf("Invalid Number\n");
			free(buff);
			return;
		}
		free(buff);

		if(addr > 0xffff || addr < 0) {
			printf("Address out of range\n");
			return;
		}
	} 

	snprintf(prompt, sizeof(prompt) - 1, 
	    "Number of instructions to disassemble [%x]: ", ninst);

	buff = readline(prompt);
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

	if(*buff) {
		char *tail;

		num = strtol(buff, &tail, 16);
		if(!tail || *tail || tail == buff) {
			printf("Invalid number\n");
			free(buff);
			return;
		}
		free(buff);
		ninst = num;
	} else {
		num = ninst;
	}

	while(num > 0) {
		size = disp_inst(addr);
		if(size <= 0) {
			return;
		}

		addr += size;
		num--;
	}

	return;
}

/**************************************************************
 * dump_memory()
 *
 * This monitor command will display memory.
 */

void dump_memory(void)
{	
	char *prompt;
	char mem;
	char *buff;
	char *tail;
	unsigned int addr;
	unsigned int size;
	uint8_t *data;

	if(testmenu) {
		prompt = "[P]rogram, [D]ata, [R]file, [I]nfo: ";
	} else {
		prompt = "[P]rogram, [D]ata, [R]file: ";
	}
	rl_num_chars_to_read = 1;
	buff = readline(prompt);
	rl_num_chars_to_read = 0;
	if(!buff) {
		printf("\n");
		return;
	}

	mem = toupper(*buff);
	free(buff);

	switch(mem) {
	case 'P':
	case 'R':
	case 'D':
		break;
	case 'I':
		if(testmenu) {
			break;
		}
	default:
		printf("\nAbort\n");
		return;
	}

	buff = readline("Address: ");
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

	addr = strtoul(buff, &tail, 16);
	if(!tail || *tail || tail == buff) {
		printf("Invalid Address\n");
		free(buff);
		return;
	}
	free(buff);

	switch(mem) {
	case 'P':
	case 'D':
		if(addr > 0xffff) {
			printf("Address out of range\n");
			return;
		}
		break;
	case 'R':
		if(addr > 0xfff) {
			printf("Address out of range\n");
			return;
		}
		break;
	case 'I':
		if(addr > 0x1ff) {
			printf("Address out of range\n");
			return;
		}
		break;
	}

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

	size = strtoul(buff, &tail, 16);
	if(!tail || *tail || tail == buff) {
		printf("Invalid Size\n");
		free(buff);
		return;
	}
	free(buff);

	switch(mem) {
	case 'P':
	case 'D':
		if(addr + size > 0x10000) {
			printf("Range out of bounds\n");
			return;
		}
		break;
	case 'R':
		if(addr + size > 0x1000) {
			printf("Range out of bounds\n");
			return;
		}
		break;
	case 'I':
		if(addr + size > 0x200) {
			printf("Range out of bounds\n");
			return;
		}
		break;
	}

	data = (uint8_t *)xmalloc(size);

	try {
		struct timer t;

		if(show_times) {
			timerstart(&t);
		}

		switch(mem) {
		case 'P':
			ez8->rd_mem(addr, data, size);
			break;
		case 'D':
			ez8->rd_data(addr, data, size);
			break;
		case 'R':
			ez8->rd_regs(addr, data, size);
			break;
		case 'I':
			ez8->rd_info(addr, data, size);
			break;
		}
	
		if(show_times) {
			timerstop(&t);
		}

		dump_data_repeat(addr, data, size, repeat);

		if(show_times) {
			printf("Elapsed time: %s\n", timerstr(&t));
		}

	} catch(char *err) {
		free(data);
		throw err;
	}

	free(data);

	return;
}

/**************************************************************
 * alter_memory()
 *
 * This monitor command is used to alter memory.
 */

void alter_memory(void)
{
	char *msg;
	char mem;
	char *buff;
	char *tail;
	unsigned int i;
	unsigned int addr;
	unsigned int size;
	unsigned int newsize;
	uint8_t *data;
	uint8_t *newdata;
	unsigned int num;
	char prompt[16];

	if(testmenu) {
		msg = "[P]rogram, [D]ata, [R]file, [I]nfo: ";
	} else {
		msg = "[P]rogram, [D]ata, [R]file: ";
	}
	rl_num_chars_to_read = 1;
	buff = readline(msg);
	rl_num_chars_to_read = 0;
	if(!buff) {
		printf("\n");
		return;
	}

	mem = toupper(*buff);
	free(buff);

	switch(mem) {
	case 'P':
	case 'D':
	case 'R':
		break;
	case 'I':
		if(testmenu) {
			break;
		}
	default:
		printf("\nAbort\n");
		return;
	}

	buff = readline("Address: ");
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

	addr = strtoul(buff, &tail, 16);
	if(!tail || *tail || tail == buff) {
		printf("Invalid Address\n");
		free(buff);
		return;
	}
	free(buff);

	switch(mem) {
	case 'P':
	case 'D':
		if(addr > 0xffff) {
			printf("Address out of range\n");
			return;
		}
		break;
	case 'R':
		if(addr > 0x0fff) {
			printf("Address out of range\n");
			return;
		}
		break;
	case 'I':
		if(addr > 0x1ff) {
			printf("Address out of range\n");
			return;
		}
		break;
	}

	i = 0;
	size = 0;
	data = NULL;

	do {
		if(i >= size) {

			newsize = size > 0 ? size * 2 : 16;

			switch(mem) {
			case 'P':
			case 'D':
				if(addr + newsize > 0x10000) {
					newsize = 0x10000 - addr;
				}
				break;
			case 'R':
				if(addr + newsize > 0x1000) {
					newsize = 0x1000 - addr;
				}
				break;
			case 'I':
				if(addr + newsize > 0x200) {
					newsize = 0x200 - addr;
				}
				break;
			}

			newdata = (uint8_t *)xrealloc(data, newsize);
			data = newdata;
			try {
				switch(mem) {
				case 'P':
					ez8->rd_mem(addr + i, data + i, 
					    newsize - size);
					break;
				case 'D':
					ez8->rd_data(addr + i, data + i, 
					    newsize - size);
					break;
				case 'R':
					ez8->rd_regs(addr + i, data + i, 
					    newsize - size);
					break;
				case 'I':
					ez8->rd_info(addr + i, data + i,
					    newsize - size);
					break;
				}
			} catch(char *err) {
				free(data);
				throw err;
			}
			size = newsize;
		}

		snprintf(prompt, sizeof(prompt), "%04X: %02X->", 
		    addr + i, data[i]);

		rl_num_chars_to_read = 2;
		buff = readline(prompt);
		rl_num_chars_to_read = 0;
		if(!buff) {
			printf("\n");
			assert(data != NULL);
			free(data);
			printf("\nAbort\n");
			return;
		}
		if(esc_key) {
			esc_key = 0;
			free(buff);
			buff = NULL;
			assert(data != NULL);
			free(data);
			printf("\nAbort\n");
			return;
		}
		if(!*buff) {
			free(buff);
			buff = NULL;
			break;
		}

		num = strtoul(buff, &tail, 16);
		if(!tail || *tail || tail == buff) {
			printf("\nInvalid Number\n");
			printf("%04X:  ", addr + i);
			free(buff);
			continue;
		}
		free(buff);

		data[i] = num;
		i++;
	
		if((mem == 'P' || mem == 'D') && (addr + i > 0xffff)) {
			break;
		} else if((mem == 'R') && (addr + i > 0xfff)) {
			break;
		} else if((mem == 'I') && (addr + i > 0x1ff)) {
			break;
		}

	} while(1);

	if(!i) {
		return;
	}

	switch(mem) {
	case 'P':
		ez8->wr_mem(addr, data, i);
		break;
	case 'D':
		ez8->wr_data(addr, data, i);
		break;
	case 'R':
		ez8->wr_regs(addr, data, i);
		break;
	case 'I':
		ez8->wr_info(addr, data, i);
		break;
	}

	return;
}

/**************************************************************
 * fill_memory()
 *
 * This monitor command will fill a block of memory with a 
 * value.
 */

void fill_memory(void)
{	
	int c;
	char *prompt;
	char mem;
	char *buff;
	char *tail;
	unsigned int addr;
	unsigned int size;
	int value;
	uint8_t *data;

	if(testmenu) {
		prompt = "[P]rogram, [D]ata, [R]file, [I]nfo: ";
	} else {
		prompt = "[P]rogram, [D]ata, [R]file: ";
	}
	rl_num_chars_to_read = 1;
	buff = readline(prompt);
	rl_num_chars_to_read = 0;
	if(!buff) {
		printf("\n");
		return;
	}

	mem = toupper(*buff);
	free(buff);

	switch(mem) {
	case 'P':
	case 'D':
	case 'R':
		break;
	case 'I':
		if(testmenu) {
			break;
		}
	default:
		printf("\nAbort\n");
		return;
	}

	buff = readline("Address: ");
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

	addr = strtoul(buff, &tail, 16);
	if(!tail || *tail || tail == buff) {
		printf("Invalid Address\n");
		free(buff);
		return;
	}
	free(buff);

	switch(mem) {
	case 'P':
	case 'D':
		if(addr > 0xffff) {
			printf("Address out of range\n");
			return;
		}
		break;
	case 'R':
		if(addr > 0xfff) {
			printf("Address out of range\n");
			return;
		}
		break;
	case 'I':
		if(addr > 0x1ff) {
			printf("Address out of range\n");
			return;
		}
		break;
	}

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

	size = strtoul(buff, &tail, 16);
	if(!tail || *tail || tail == buff) {
		printf("Invalid Size\n");
		free(buff);
		return;
	}
	free(buff);

	switch(mem) {
	case 'P':
	case 'D':
		if(addr + size > 0x10000) {
			printf("Range out of bounds\n");
			return;
		}
		break;
	case 'R':
		if(addr + size > 0x1000) {
			printf("Range out of bounds\n");
			return;
		}
		break;
	case 'I':
		if(addr + size > 0x200) {
			printf("Range out of bounds\n");
			return;
		}
		break;
	}

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

	c = toupper(*buff);
	switch(c) {
	case 'L':	/* linear */
	case 'R':	/* random */
		break;
	default:
		c = 0;
		value = strtol(buff, &tail, 16);
		if(!tail || *tail || tail == buff) {
			printf("Invalid Data\n");
			free(buff);
			return;
		}
		if(value < 0 || value > 0xff) {
			printf("Data out of range\n");
			free(buff);
			return;
		}
	}
	free(buff);

	data = (uint8_t *)xmalloc(size);

	switch(c) {
	case 'L': {
		int i;

		for(i=0; i<(int)size; i++) {
			data[i] = i & 0xff;
		}
		break;
	}
	case 'R': {
		int i;

		for(i=0; i<(int)size; i++) {
			data[i] = rand() & 0xff;
		}
		break;
	}
	case 0:
		memset(data, value, size);
		break;
	default:
		abort();
	}

	try {
		struct timer t;

		if(show_times) {
			timerstart(&t);
		}

		switch(mem) {
		case 'P':
			ez8->wr_mem(addr, data, size);
			break;
		case 'D':
			ez8->wr_data(addr, data, size);
			break;
		case 'R':
			ez8->wr_regs(addr, data, size);
			break;
		case 'I':
			ez8->wr_info(addr, data, size);
			break;
		}

		if(show_times) {
			timerstop(&t);
			printf("Elapsed time: %s\n", timerstr(&t));
		}

	} catch(char *err) {
		free(data);
		throw err;
	}

	free(data);

	return;
}

/**************************************************************
 * display_registers()
 *
 * This monitor command will display cpu registers.
 */

void display_registers(void)
{
	int i;
	uint16_t pc;
	uint8_t special_regs[4];
	uint8_t working_regs[16];

	pc = ez8->rd_pc();

	ez8->rd_regs(0xffc, special_regs, 4);

	ez8->rd_regs(((special_regs[1] << 8) | special_regs[1]) 
	    & 0x0ff0, working_regs, 16);

	printf("PC: %04X  SP: %04X  RP: %02X  FLAGS: %02X ", pc, 
	    (special_regs[2] << 8) | special_regs[3], 
	    special_regs[1], special_regs[0]);

	printf("[%c%c%c%c%c%c]\n", 
	    special_regs[0] & 0x80 ? 'c' : ' ',
	    special_regs[0] & 0x40 ? 'z' : ' ',
	    special_regs[0] & 0x20 ? 's' : ' ',
	    special_regs[0] & 0x10 ? 'v' : ' ',
	    special_regs[0] & 0x08 ? 'd' : ' ',
	    special_regs[0] & 0x04 ? 'h' : ' ');

	printf("Regs: ");

	for(i=0; i<16; i++) {
		if(i == 8) {
			printf("- ");
		}
		printf("%02X ", working_regs[i]);
	}

	printf("\n");

	disp_inst(pc);

	return;
}

/**************************************************************
 * modify_registers()
 *
 * This monitor command will modify cpu registers.
 */

void modify_registers(void)
{
	int i;
	char *buff;
	char *tail;
	char *ptr;
	uint8_t data[2];
	uint16_t addr;
	int value;

	enum reg_t { none, pc_reg, rp_reg, sp_reg, 
	    r0_reg, r1_reg, r2_reg, r3_reg, 
	    r4_reg, r5_reg, r6_reg, r7_reg, 
	    r8_reg, r9_reg, r10_reg, r11_reg, 
	    r12_reg, r13_reg, r14_reg, r15_reg } reg;

	const struct register_t {
		char *name;
		enum reg_t reg;
	} registers[] = {
		{ "pc", pc_reg },
		{ "rp", rp_reg },
		{ "sp", sp_reg },
		{ "r0", r0_reg },
		{ "r1", r1_reg },
		{ "r2", r2_reg },
		{ "r3", r3_reg },
		{ "r4", r4_reg },
		{ "r5", r5_reg },
		{ "r6", r6_reg },
		{ "r7", r7_reg },
		{ "r8", r8_reg },
		{ "r9", r9_reg },
		{ "r10", r10_reg },
		{ "r11", r11_reg },
		{ "r12", r12_reg },
		{ "r13", r13_reg },
		{ "r14", r14_reg },
		{ "r15", r15_reg },
		{ NULL, none } };

	buff = readline("Register [PC,RP,SP,R0,...,R15]: ");
	if(!buff) {
		printf("Abort\n");
		return;
	}
	if(esc_key) {
		esc_key = 0;
		printf("\nAbort\n");
		return;
	}

	if(!*buff) {
		free(buff);
		printf("Abort\n");
		return;
	}

	ptr = strtok(buff, " \t\r\n");

	reg = none;

	for(i=0; registers[i].name; i++) {
		if(!strcasecmp(registers[i].name, ptr)) {
			reg = registers[i].reg;
			break;
		}
	}

	if(reg == none) {
		printf("Invalid register\n");
		return;
	}

	buff = readline("Value: ");
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

	value = strtol(buff, &tail, 16);
	if(!tail || *tail || tail == buff) {
		printf("Invalid Data\n");
		free(buff);
		return;
	}
	free(buff);

	switch(reg) {
	case pc_reg:
	case sp_reg:
		if(value < 0 || value > 0xffff) {
			printf("Data out of range\n");
			return;
		}
		break;
	default:
		if(value < 0 || value > 0xff) {
			printf("Data out of range\n");
			return;
		}
		break;
	}
	
	switch(reg) {
	case pc_reg:
	case sp_reg:
	case rp_reg:
		addr = 0;
		break;
	default:
		ez8->rd_regs(0xffd, data, 1);
		addr = ((data[0] << 8) | data[0]) & 0x0ff0;
		break;
	}

	switch(reg) {
	case pc_reg:
		ez8->wr_pc(value);
		break;
	case sp_reg:
		data[0] = (value >> 8) & 0x00ff;
		data[1] = value & 0x00ff;
		ez8->wr_regs(0xffe, data, 2);
		break;
	case rp_reg:
		data[0] = value;
		ez8->wr_regs(0xffd, data, 1);
		break;
	case r15_reg:
		addr += 1;
	case r14_reg:
		addr += 1;
	case r13_reg:
		addr += 1;
	case r12_reg:
		addr += 1;
	case r11_reg:
		addr += 1;
	case r10_reg:
		addr += 1;
	case r9_reg:
		addr += 1;
	case r8_reg:
		addr += 1;
	case r7_reg:
		addr += 1;
	case r6_reg:
		addr += 1;
	case r5_reg:
		addr += 1;
	case r4_reg:
		addr += 1;
	case r3_reg:
		addr += 1;
	case r2_reg:
		addr += 1;
	case r1_reg:
		addr += 1;
	case r0_reg:
		data[0] = value;
		ez8->wr_regs(addr, data, 1);
		break;
	default:
		break;
	}

	return;
}

/**************************************************************
 * erase_memory()
 *
 * This monitor command will mass erase memory.
 */

void erase_memory()
{
	char *buff;

	rl_num_chars_to_read = 1;
	buff = readline("Mass erase memory, are you sure [y/n]? ");
	rl_num_chars_to_read = 0;
	if(!buff) {
		printf("\n");
		return;
	}

	if(toupper(*buff) == 'Y') {
		struct timer t;
		printf("Erasing flash\n");
		if(show_times) {
			timerstart(&t);
		}
		ez8->flash_mass_erase();
		if(show_times) {
			timerstop(&t);
			printf("Elapsed time: %s\n", timerstr(&t));
		}

		printf("Part has been erased.\n");

		rl_num_chars_to_read = 1;
		buff = readline("Reset part [y/n]? ");
		rl_num_chars_to_read = 0;
		if(!buff) {
			printf("\n");
			return;
		}
		if(toupper(*buff) == 'Y') {
			ez8->reset_chip();
		}
		free(buff);
		return ;
	} else {
		printf("Abort\n");
		return;
	}
}

/**************************************************************
 * load_file()
 *
 * This monitor command will load a hexfile into memory.
 */

void load_file(void)
{
	int err;
	char *filename;
	char *buff;
	struct timer t;

	tab_function = rl_complete;	
	buff = readline("File: ");
	tab_function = rl_insert;
	if(!buff) {
		printf("Abort\n");
		return;
	}
	if(esc_key) {
		esc_key = 0;
		printf("\nAbort\n");
		return;
	}
	if(!*buff) {
		free(buff);
		printf("Abort\n");
		return;
	}

	add_history(buff);

	filename = strtok(buff, " \t\r\n");

	if(!filebuff) {
		filebuff = (uint8_t *)xmalloc(0x10000);
	}

	memset(filebuff, 0xff, 0x10000);

	err = rd_hexfile(filebuff, 0x10000, filename);
	if(err) {
		free(buff);
		return;
	}

	free(buff);

	if(show_times) {
		timerstart(&t);
	}

	ez8->flash_mass_erase();
	if(ez8->state(ez8->state_protected)) {
		ez8->reset_chip();
	}

	ez8->wr_mem(0x0000, filebuff, 0x10000);
	ez8->reset_chip();

	if(show_times) {
		timerstop(&t);
		printf("Elapsed time: %s\n", timerstr(&t));
	}

	if(ez8->state(ez8->state_protected)) {
		printf("Memory Read Protect enabled\n");
	} else {
		display_registers();
	}

	return;
}

/**************************************************************
 * run_program()
 *
 * This monitor command will put the cpu into run mode.
 */

void run_program(void)
{
	char *buff;
	uint16_t cntr;
	struct timer t;

	if(show_times) {
		timerstart(&t);
	}

	ez8->run();

	rl_event_hook = check_if_running;
	buff = readline("Running... ");
	rl_event_hook = NULL;
	if(buff) {
		free(buff);
		buff = NULL;
	} else {
		printf("\n");
	}
	if(esc_key) {
		esc_key = 0;
		printf("\n");
	}
	if(rl_err) {
		char *err;

		err = rl_err;
		rl_err = NULL;
		throw err;
	}

	if(!ez8->state(ez8->state_stopped)) {
		ez8->stop();
	} else {
		cntr = ez8->rd_cntr();
		if(cntr == 0xffff) {
			printf("BREAK\n");
		} else {
			printf("BREAK - runcount %04X\n", cntr);
		}
	}

	if(show_times) {
		timerstop(&t);
		printf("Elapsed time: %s\n", timerstr(&t));
	}

	display_registers();

	return;
}

/**************************************************************
 * step_inst()
 *
 * This monitor command will single step one instruction.
 */

void step_inst(void)
{
	ez8->step();

	display_registers();

	return;
}

/**************************************************************
 * next_inst()
 *
 * This monitor command will step over the next instruction.
 */

void next_inst(void)
{
	char *buff;

	ez8->next();

	if(!ez8->state(ez8->state_stopped)) {
		rl_event_hook = check_if_running;
		buff = readline("");
		rl_event_hook = NULL;
		if(buff) {
			free(buff);
			buff = NULL;
		} else {
			printf("\n");
		}
		if(esc_key) {
			esc_key = 0;
			printf("\n");
		}
		if(rl_err) {
			char *err;

			err = rl_err;
			rl_err = NULL;
			throw err;
		}

		if(!ez8->state(ez8->state_stopped)) {
			ez8->stop();
		}
	}

	display_registers();

	return;
}

/**************************************************************/

void show_breakpoints(void)
{
	int num;
	int i;

	num = ez8->get_num_breakpoints();

	for(i=0; i<num; i++) {
		disp_inst(ez8->get_breakpoint(i));
	}

	return;
}

/**************************************************************/

void set_breakpoint(void)
{
	int addr;
	char *buff;
	char *tail;

	show_breakpoints();

	buff = readline("Address: ");
	if(!buff) {
		printf("Abort\n");
		return;
	}
	if(esc_key) {
		esc_key = 0;
		free(buff);
		printf("\nAbort\n");
		return;
	}

	addr = strtol(buff, &tail, 16);
	if(!tail || *tail || tail == buff) {
		printf("Invalid address\n");
		free(buff);
		return;
	}
	free(buff);

	if(addr <= 0x0000 || addr > 0xffff) {
		printf("Address out of range\n");
		return;
	}

	if(ez8->breakpoint_set(addr)) {
		printf("Breakpoint already set\n");
		return;
	}

	ez8->set_breakpoint(addr);

	return;
}

/**************************************************************/

void clear_breakpoint(void)
{
	int addr;
	char *buff;
	char *tail;

	show_breakpoints();

	buff = readline("Address: ");
	if(!buff) {
		printf("Abort\n");
		return;
	}

	if(esc_key) {
		esc_key = 0;
		free(buff);
		printf("\nAbort\n");
		return;
	}

	addr = strtol(buff, &tail, 16);
	if(!tail || *tail || tail == buff) {
		printf("Invalid address\n");
		free(buff);
		return;
	}
	free(buff);

	if(addr <= 0x0000 || addr > 0xffff) {
		printf("Address out of range\n");
		return;
	}

	if(! ez8->breakpoint_set(addr)) {
		printf("Breakpoint not set\n");
		return;
	}

	ez8->remove_breakpoint(addr);

	return;
}

/**************************************************************/

void reset_part(void)
{
	char key;
	char *buff;

	rl_num_chars_to_read = 1;
	buff = readline("Reset [L]ink, Reset [C]hip ? ");
	rl_num_chars_to_read = 0;
	if(!buff) {
		printf("Abort\n");
	}

	key = toupper(*buff);
	free(buff);

	if(esc_key) {
		esc_key = 0;
		printf("\nAbort\n");
		return;
	}

	switch(key) {
	case 'L':
		ez8->reset_link();
		ez8->stop();
		break;
	case 'C':
		ez8->reset_chip();
		display_registers();
		break;
	default:
		printf("Abort\n");
		return;
	}

	return;
}

/**************************************************************/

#ifndef	_WIN32
#define	SHELL	"SHELL"
#else	/* _WIN32 */
#define	SHELL	"COMSPEC"
#endif	/* _WIN32 */

void sub_shell(void)
{
	char *shell;

	shell = getenv(SHELL);

	if(shell == NULL) {
		printf("Environment variable %s not defined\n", SHELL);
		return;
	}

	system(shell);

	return;
}

/**************************************************************/

void help(void)
{
	printf("%s - build %s\n\n", banner, build);

	printf("\tA - alter memory\n");
	printf("\tB - set/show breakpoints\n");
	printf("\tC - clear breakpoints\n");
	printf("\tD - display memory\n");
	printf("\tE - erase memory\n");
	printf("\tF - fill memory\n");
	printf("\tG - run program\n");
	printf("\tH - display help\n");
	printf("\tI - info\n");
	printf("\tL - load program memory from file\n");
	printf("\tM - modify registers\n");
	printf("\tN - next (step over calls)\n");
	printf("\tQ - exit debugger\n");
	printf("\tR - display working registers\n");
	printf("\tS - step (step into calls)\n");
	if(trce_available()) {
		printf("\tT - trace subsystem\n");
	}
	printf("\tU - unassemble instructions\n");
	#ifdef	TEST
	if(testmenu) {
		printf("\tX - test menu\n");
	}
	#endif
	printf("\tZ - reset\n");
	printf("\t! - shell\n");

	return;
}

/**************************************************************/

int quit(void)
{
	char *buff;

	rl_num_chars_to_read = 1;
	buff = readline("Are you sure [y/n]? ");
	rl_num_chars_to_read = 0;

	if(buff == NULL) {
		return -1;
	}

	if(toupper(*buff) == 'Y') {
		return 'Q';
	}

	return 0;
}

/**************************************************************/

int command(int key)
{
	switch(key) {
	case 'A':
		alter_memory();
		break;
	case 'B':
		set_breakpoint();
		break;
	case 'C':
		clear_breakpoint();
		break;
	case 'D':
		dump_memory();
		break;
	case 'E':
		erase_memory();
		break;
	case 'F':
		fill_memory();
		break;
	case 'G':
		run_program();
		break;
	case 'H':
	case '?':
		help();
		break;
	case 'I':
		display_info();
		break;
	case 'L':
		load_file();
		break;
	case 'M':
		modify_registers();
		break;
	case 'N':
		next_inst();
		break;
	case 'Q':
		key = quit();
		break;
	case 'R':
		display_registers();
		break;
	case 'S':
		step_inst();
		break;
	case 'U':
		unassemble();
		break;
	case 'Z':
		reset_part();
		break;
	case '!':
		sub_shell();
		break;
	case 'T':
		if(trce_available()) {
			trce_subsystem();
			break;
		}
		printf("Unknown command\n");
		break;
	case 'X':
		#ifdef TEST
		if(testmenu) {
			test_menu();
			break;
		}
		#endif
		printf("Unknown command\n");
		break;
	default:
		printf("Unknown command\n");
		break;
	}

	return key;
}

/**************************************************************/

int command_loop(void)
{
	extern int esc_key;
	char key;

	printf("%s - build %s\n", banner, build);
	printf("Type 'H' for help\n");

	do {
		key = 0;

		printf("\n");

		if(!ez8->link_open()) {
			printf("Link is closed\n");
			return -1;
		}

		while(!ez8->link_up()) {
			char *buff = NULL;

			rl_num_chars_to_read = 1;
			buff = readline("Link is down, reset link [y/n]? ");
			rl_num_chars_to_read = 0;
			if(!buff) {
				printf("\n");
			} else if(toupper(*buff) == 'Y') {
				try {
					ez8->reset_link();
					ez8->rd_revid();
				} catch(char *err) {
					fprintf(stderr, "%s\n", err);
				}
			} else if(toupper(*buff) == 'N') {
				free(buff);
				return -1;
			}
			if(buff) {
				free(buff);
			}
		}

		try {
			if(!ez8->state(ez8->state_stopped)) {
				ez8->stop();
			}
		} catch(char *err) {
			fprintf(stderr, "%s", err);
			continue;
		}

		do {
			char *buff;

			rl_num_chars_to_read = 1;
			buff = readline("ez8mon> ");
			rl_num_chars_to_read = 0;
			if(!buff) {
				key = 'Q';
				printf("\n");
			} else if(esc_key) {
				esc_key = 0;
				rl_ding();
				printf("\r");
			} else {
				key = toupper(*buff);
			}
			if(buff) {
				free(buff);
			}

		} while(!key);

		try {
			key = command(key);
		} catch(char *err) {
			fprintf(stderr, "%s", err);
		}

	} while(key != 'Q');

	return 0;
}


/**************************************************************/



