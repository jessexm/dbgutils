/* Copyright (C) 2002, 2003, 2004 Zilog, Inc.
 *
 * $Id: ez8dbg_trce.cpp,v 1.1 2004/08/03 14:23:48 jnekl Exp $
 *
 * This implements the debugger trace api. Trace is only
 * available on the fpga emulator. 
 *
 * Trace is only minimally supported by the command line 
 * debugger. It's more for diagnostic info.
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
 * This will return the trace status register.
 */

uint8_t ez8dbg::rd_trce_status(void)
{
	uint8_t status;

	if(!state(state_trace)) {
		strncpy(err_msg, "Could not read trace status register\n"
		    "trace not available\n", err_len-1);
		throw err_msg;
	}
		
	status = ez8ocd::rd_trce_status();

	return status;
}

/**************************************************************
 * This will write the trace control register.
 */

void ez8dbg::wr_trce_ctl(uint8_t control)
{
	if(!state(state_trace)) {
		strncpy(err_msg, "Could not write trace control register\n"
		    "trace not available\n", err_len-1);
		throw err_msg;
	}

	ez8ocd::wr_trce_ctl(control);

	return;
}

/**************************************************************
 * This will return the trace control register.
 */

uint8_t ez8dbg::rd_trce_ctl(void)
{
	uint8_t control;

	if(!state(state_trace)) {
		strncpy(err_msg, "Could not read trace control register\n"
		    "trace not available\n", err_len-1);
		throw err_msg;
	}

	control = ez8ocd::rd_trce_ctl();

	return control;
}

/**************************************************************
 * This will configure a trace event.
 */

void ez8dbg::wr_trce_event(uint8_t num, struct trce_event *event)
{
	if(!state(state_trace)) {
		strncpy(err_msg, "Could not write trace event\n"
		    "trace not available\n", err_len-1);
		throw err_msg;
	}

	ez8ocd::wr_trce_event(num, event);

	return;
}

/**************************************************************
 * This will read a trace event.
 */

void ez8dbg::rd_trce_event(uint8_t num, struct trce_event *event)
{
	if(!state(state_trace)) {
		strncpy(err_msg, "Could not read trace event\n"
		    "trace not available\n", err_len-1);
		throw err_msg;
	}

	ez8ocd::rd_trce_event(num, event);

	return;
}

/**************************************************************
 * This will read the trace write pointer.
 */

uint16_t ez8dbg::rd_trce_wr_ptr(void)
{
	uint16_t wr_ptr;

	if(!state(state_trace)) {
		strncpy(err_msg, "Could not read trace pointer\n"
		    "trace not available\n", err_len-1);
		throw err_msg;
	}

	wr_ptr = ez8ocd::rd_trce_wr_ptr();

	return wr_ptr;
}

/**************************************************************
 * This will read the trace buffer.
 */

void ez8dbg::rd_trce_buff(uint16_t address, struct trce_frame *frames, 
	size_t size)
{
	if(!state(state_trace)) {
		strncpy(err_msg, "Could not read trace buffer\n"
		    "trace not available\n", err_len-1);
		throw err_msg;
	}

	ez8ocd::rd_trce_buff(address, frames, size);

	return;
}

/**************************************************************/


