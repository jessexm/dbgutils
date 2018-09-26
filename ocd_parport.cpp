/* Copyright (C) 2002, 2003, 2004 Zilog, Inc.
 * ocd_parport.cpp
 *
 * Z8 Encore On-Chip Debugger parallel module.
 */

#include	<stdio.h>
#include	<assert.h>
#include	<string.h>

#include	"ocd_parport.h"
#include	"err_msg.h"

/**************************************************************/

ocd_parport::ocd_parport(void)
{
	strncpy(err_msg, "Parallel port not implemented yet\n",
	    err_len-1);
	throw err_msg;
}

/**************************************************************/

ocd_parport::~ocd_parport(void)
{
	return;
}

/**************************************************************/

void ocd_parport::connect(const char *device)
{
	strncpy(err_msg, "Parallel port not implemented yet\n", err_len-1);
	throw err_msg;
}

/**************************************************************/

void ocd_parport::write(const uint8_t *data, size_t size)
{
	strncpy(err_msg, "Parallel port not implemented yet\n", err_len-1);
	throw err_msg;
}

/**************************************************************/

void ocd_parport::read(uint8_t *data, size_t size)
{
	strncpy(err_msg, "Parallel port not implemented yet\n", err_len-1);
	throw err_msg;
}

/**************************************************************/

void ocd_parport::reset(void)
{
	strncpy(err_msg, "Parallel port not implemented yet\n", err_len-1);
	throw err_msg;
}

/**************************************************************/

bool ocd_parport::link_open(void)
{
	return 0;
}

bool ocd_parport::link_up(void)
{
	return 0;
}

int ocd_parport::link_speed(void)
{
	return 0;
}

void ocd_parport::set_baudrate(int baudrate)
{
	strncpy(err_msg, "Parallel port not implemented yet\n", err_len-1);
	throw err_msg;
}

void ocd_parport::set_timeout(int mstimeout)
{
	strncpy(err_msg, "Parallel port not implemented yet\n", err_len-1);
	throw err_msg;
}

bool ocd_parport::available(void)
{
	return 0;
}

bool ocd_parport::error(void)
{
	return 0;
}

/**************************************************************/


