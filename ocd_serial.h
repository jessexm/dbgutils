/* Copyright (C) 2002, 2003, 2004 Zilog, Inc.
 *
 * $Id: ocd_serial.h,v 1.3 2005/10/20 18:39:37 jnekl Exp $
 *
 * This class implements the serial interface module for the
 * ez8 on-chip debugger.
 */

#ifndef	OCD_SERIAL_HEADER
#define	OCD_SERIAL_HEADER

#include	<stdlib.h>
#include	<inttypes.h>
#include	"ocd.h"
#include	"serialport.h"

/**************************************************************/

class ocd_serial : public ocd, private serialport
{
private:
	bool open, up;

	/* Prohibit copy constructor */
	ocd_serial(ocd_serial &);	

public:
	ocd_serial(void);
	~ocd_serial(void);

	void connect(const char *, int);
	void reset(void);
	void set_timeout(int);
	void set_baudrate(int);

	bool link_open(void);
	bool link_up(void);
	int  link_speed(void);

	bool available(void);
	bool error(void);

	void write(const uint8_t *, size_t);
	void read(uint8_t *, size_t);
};

/**************************************************************/

#endif	/* OCD_SERIAL_HEADER */

