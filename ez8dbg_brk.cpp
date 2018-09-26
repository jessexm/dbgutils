/* Copyright (C) 2002, 2003, 2004 Zilog, Inc.
 *
 * $Id: ez8dbg_brk.cpp,v 1.2 2004/12/01 01:26:49 jnekl Exp $
 *
 * This implements breakpoints for the debugger. 
 */

#define		_REENTRANT

#include	<stdio.h>
#include	<unistd.h>
#include	<string.h>
#include	<errno.h>
#include	<inttypes.h>
#include	<ctype.h>
#include	<sys/stat.h>
#include	<assert.h>
#include	"xmalloc.h"

#ifdef	_WIN32
#include	"winunistd.h"
#endif

#include	"ez8dbg.h"
#include	"ez8.h"
#include	"err_msg.h"

/**************************************************************
 * This will determine if a breakpoint is currently set
 * at the specified address. Returns 0 if a breakpoint
 * is not set, 1 if a breakpoint is set.
 */

bool ez8dbg::breakpoint_set(uint16_t address)
{
	int i;

	for(i=0; i<num_breakpoints; i++) {
		if(breakpoints[i].address == address) {
			return 1;
		}
	}

	return 0;
}

/**************************************************************
 * This will return the number of breakpoints set.
 */

int ez8dbg::get_num_breakpoints(void)
{
	return num_breakpoints;
}

/**************************************************************
 * This will return the address for a set breakpoint.
 */

uint16_t ez8dbg::get_breakpoint(int index)
{
	if(index >= num_breakpoints) {
		strncpy(err_msg, "Could not get breakpoint address\n"
		    "index out of range\n", err_len-1);
		abort();
		throw err_msg;
	}

	return breakpoints[index].address;
}

/**************************************************************
 * This will add a breakpoint to our breakpoint array.
 */

void ez8dbg::set_breakpoint(uint16_t address) 
{
	uint8_t data[1];
	const uint8_t brk[1] = { 0x00 };
	struct breakpoint_t *bp;

	if(!state(state_stopped)) {
		strncpy(err_msg, "Could not set breakpoint\n"
		    "device is running\n", err_len-1);
		throw err_msg;
	}

	if(state(state_protected)) {
		strncpy(err_msg, "Could not set breakpoint\n"
		    "memory read protect is enabled\n", err_len-1);
		throw err_msg;
	}

	if(breakpoint_set(address)) {
		strncpy(err_msg, "Could not set breakpoint\n"
		    "breakpoint already set\n", err_len-1);
		throw err_msg;
	}

	if(address == 0x0000) {
		strncpy(err_msg, "Could not set breakpoint\n"
		    "NULL address\n", err_len-1);
		throw err_msg;
	}

	rd_mem(address, data, 1);

	bp = (struct breakpoint_t *)xrealloc(breakpoints, 
	    sizeof(struct breakpoint_t) * (num_breakpoints + 1));

	breakpoints = bp;
	breakpoints[num_breakpoints].address = address;
	breakpoints[num_breakpoints].data = data[0];
	num_breakpoints++;

	flash_setup(0x00);

	cache &= ~(MEMCRC_CACHED | CRC_CACHED);
	main_mem[address] = *brk;
	ez8ocd::wr_mem(address, brk, 1);

	flash_lock();

	rd_mem(address, data, 1);
	if(*data != *brk) {
		strncpy(err_msg, "Set breakpoint failed\n"
		    "readback verify failed\n", err_len-1);
		throw err_msg;
	}

	return;
}

/**************************************************************
 * This will remove a breakpoint from our breakpoint array.
 */

void ez8dbg::delete_breakpoint(int index)
{
	size_t move_size;

	assert(num_breakpoints == 0 || breakpoints != NULL);

	if(index >= num_breakpoints) {
		strncpy(err_msg, "Could not delete breakpoint\n"
		    "index out of range\n", err_len-1);
		abort();
		throw err_msg;
	}

	move_size = num_breakpoints - 1 - index;
	if(move_size) {
		memmove(breakpoints + index, breakpoints + index + 1, 
		    move_size * sizeof(struct breakpoint_t));
	}

	num_breakpoints--;
	breakpoints = (struct breakpoint_t *)xrealloc(breakpoints, 
	    sizeof(struct breakpoint_t) * num_breakpoints);

	return;
}

/**************************************************************
 * This will remove a breakpoint and write the origional
 * opcode back to memory.
 */

void ez8dbg::remove_breakpoint(uint16_t address)
{
	int index;
	uint8_t buff[1];

	if(!state(state_stopped)) {
		strncpy(err_msg, "Could not remove breakpoint\n"
		    "device is running\n", err_len-1);
		throw err_msg;
	}
	if(state(state_protected)) {
		strncpy(err_msg, "Could not remove breakpoint\n"
		    "memory read protect is enabled\n", err_len-1);
		throw err_msg;
	}

	assert(num_breakpoints == 0 || breakpoints != NULL);

	for(index = 0; index < num_breakpoints; index++) {
		if(breakpoints[index].address == address) {
			break;
		}
	}

	if(index >= num_breakpoints) {
		strncpy(err_msg, "Remove breakpoint failed\n"
		    "breakpoint not set\n", err_len-1);
		throw err_msg;
	}

	buff[0] = breakpoints[index].data;

	/* program data back into flash */
	wr_mem(address, buff, 1);

	/* remove breakpoint from array */
	delete_breakpoint(index);

	return;
}

/**************************************************************
 * This will read the specified memory block, replacing 
 * breakpoints with the opcode that should be there.
 */

void ez8dbg::read_mem(uint16_t address, uint8_t *data, size_t size)
{
	int i;

	rd_mem(address, data, size);

	for(i=0; i<num_breakpoints; i++) {
		int offset;

		offset = breakpoints[i].address - address;
		if(offset >= 0 && offset < (int)size) {
			data[offset] = breakpoints[i].data;
		}
	}

	return;
}

/**************************************************************/


