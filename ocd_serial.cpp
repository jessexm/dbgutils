/* Copyright (C) 2002, 2003, 2004 Zilog, Inc.
 *
 * $Id: ocd_serial.cpp,v 1.3 2005/10/20 18:39:37 jnekl Exp $
 *
 * This class implements the serial interface module for the
 * Z8 Encore On-Chip Debugger.
 */

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<assert.h>
#include	<stdarg.h>
#include	<unistd.h>

#ifdef	_WIN32
#include	"winunistd.h"
#endif
#include	"ocd_serial.h"

#include	"err_msg.h"

/**************************************************************/

#define	AUTOBAUD_CHARACTER	0x80

/**************************************************************
 * Constructor for serial port interface.
 */

ocd_serial::ocd_serial(void)
{
	open = up = 0;

	return;
}

/**************************************************************/

ocd_serial::~ocd_serial(void)
{
	return;
}

/**************************************************************
 * This will open and setup the serial port.
 */

void ocd_serial::connect(const char *device, int baudrate)
{
	assert(device != NULL);

	serialport::open(device);

	serialport::baudrate = baudrate;
	serialport::size = serialport::eight;
	serialport::parity = serialport::none;
	serialport::stopbits = serialport::one;
	serialport::flowcontrol = 0;
	serialport::timeout = 256 * 1000 * 10 / baudrate;
	if(!serialport::timeout) {
		serialport::timeout = 1;
	}

	try {
		serialport::configure();
	} catch(char *err) {
		serialport::close();
		throw err;
	}

	open = 1;

	return;
}

/**************************************************************
 * This will read data from the serial port.
 * 
 * This function will attempt to read the specified size from
 * the serial port. If an error or timeout occurs, this function
 * will throw and exception.
 */

void ocd_serial::read(uint8_t *buff, size_t size)
{
	int t;
	int bytes_read;

	if(!open) {
		strncpy(err_msg, "Cannot read from on-chip debugger\n"
		    "serial port not open\n", err_len-1);
		throw err_msg;
	}

	if(!up) {
		strncpy(err_msg, "Cannot read from on-chip debugger\n"
		    "link needs to be reset first\n", err_len-1);
		throw err_msg;
	}

	/* intelligently increase timout only when needed */
	t = 1000 * 3 / 2 * 10 * size / serialport::baudrate;
	if(t > serialport::timeout) {
		serialport::timeout = t;
		serialport::configure();
	}

	try {
		bytes_read = serialport::read(buff, size);
	} catch(char *err) {
		up = 0;
		throw err;
	}

	if(!bytes_read) {
		up = 0;
		strncpy(err_msg, "Read from on-chip debugger failed\n"
		    "serial port read timeout\n", err_len-1);
		throw err_msg;
	}

	if((size_t)bytes_read < size) {
		up = 0;
		strncpy(err_msg, "Read from on-chip debugger failed\n"
		    "characters lost\n", err_len-1);
		throw err_msg;
	}

	return;
}

/**************************************************************
 * This will write data to the serial port. 
 *
 * Since tx/rx lines tied together, everything transmitted
 * should be available to be read. This function will 
 * automatically try to read back what it wrote, and verify
 * what it wrote is read back properly and that no transmit
 * collisions occurred.
 */

void ocd_serial::write(const uint8_t *buff, size_t size)
{
	int t;
	uint8_t verify[BUFSIZ];

	assert(buff != NULL);

	if(!open) {
		strncpy(err_msg, "Cannot write to on-chip debugger\n"
		    "serial port not open\n", err_len-1);
		throw err_msg;
	}
	if(!up) {
		strncpy(err_msg, "Cannot write to on-chip debugger\n"
		    "link needs to be reset first\n", err_len-1);
		throw err_msg;
	}

	if(serialport::error()) {
		try {
			reset();
		} catch(char *err) {
			up = 0;
			throw err;
		}
	}

	/* intelligently increase timout only when needed */
	t = 1000 * 3 / 2 * 10 * size / serialport::baudrate;
	if(t > serialport::timeout) {
		serialport::timeout = t;
		serialport::configure();
	}

	try {
		serialport::write(buff, size);
	} catch(char *err) {
		up = 0;
		throw err;
	}

	do {
		ssize_t read_size;
		ssize_t bytes_read;

		read_size = size > sizeof(verify) ? sizeof(verify) : size;

		try {
			bytes_read = serialport::read(verify, read_size);
		} catch(char *err) {
			up = 0;
			throw err;
		}

		if(bytes_read < read_size) {
			up = 0;
			strncpy(err_msg, "Write to on-chip debugger failed\n"
			    "loop readback timeout\n", err_len-1);
			throw err_msg;
		}

		for(int i=0; i < read_size; i++) {
			if(verify[i] != buff[i]) {
				up = 0;
				strncpy(err_msg, 
				    "Write to on-chip debugger failed\n"
				    "transmit collision detected\n",
				    err_len-1);
				throw err_msg;
			}
		}

		size -= bytes_read;
		buff += bytes_read;
		
	} while(size > 0);

	return;
}

/**************************************************************
 * This will reset the serial on-chip debugger connection.
 *
 * It does this reset by placing the serial port in the
 * break state (tx line held in space state for > 9 bit times).
 *
 * It will then autobaud the connection to the remote on-chip 
 * debugger.
 */

void ocd_serial::reset(void)
{
	const uint8_t autobaud[1] = { AUTOBAUD_CHARACTER };

	if(!open) {
		strncpy(err_msg, "Cannot reset on-chip debugger link\n"
		    "serial port not open\n", err_len-1);
		throw err_msg;
	}

	up = 0;

	serialport::flush();
	serialport::sendbreak();
	serialport::flush();

	up = 1;

	write(autobaud, sizeof(autobaud));

	return;
}

/**************************************************************
 * set_timeout() sets the read timeout
 */

void ocd_serial::set_timeout(int mstimeout)
{
	serialport::timeout = mstimeout;
	serialport::configure();
}

/**************************************************************
 * set_baudrate() sets the baudrate
 */

void ocd_serial::set_baudrate(int baud)
{
	serialport::baudrate = baud;
	serialport::timeout = 256 * 1000 * 10 / baud;
	if(!serialport::timeout) {
		serialport::timeout = 1;
	}
	serialport::configure();
}

/**************************************************************
 * link_open() returns true if the serial port is open
 * and configured.
 */

bool ocd_serial::link_open(void)
{
	return open;
}

/**************************************************************
 * link_up() returns true if the ocd link is up and
 * ready for communication.
 */

bool ocd_serial::link_up(void)
{
	return up;
}

/**************************************************************
 * This function will check if there is data available to be
 * read.
 */

bool ocd_serial::available(void)
{
	if(!open) {
		strncpy(err_msg, "Cannot read from on-chip debugger\n"
		    "serial port not open\n", err_len-1);
		throw err_msg;
	}

	if(!up) {
		strncpy(err_msg, "Cannot read from on-chip debugger\n"
		    "link needs to be reset first\n", err_len-1);
		throw err_msg;
	}

	return serialport::available();
}

/**************************************************************
 * This function will check if there is an error pending
 * (such as a break condition). This is used to determine
 * if we need to re-autobaud due to a break caused by a
 * clock switch.
 */

bool ocd_serial::error(void)
{
	if(!open) {
		strncpy(err_msg, "Cannot read from on-chip debugger\n"
		    "serial port not open\n", err_len-1);
		throw err_msg;
	}

	if(!up) {
		strncpy(err_msg, "Cannot read from on-chip debugger\n"
		    "link needs to be reset first\n", err_len-1);
		throw err_msg;
	}

	return serialport::error();
}

/**************************************************************/

int ocd_serial::link_speed(void)
{
	return serialport::baudrate;
}

/**************************************************************/

