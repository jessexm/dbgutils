/* Copyright (C) 2002, 2003, 2004 Zilog, Inc.
 *
 * $Id: ocd_parport.h,v 1.3 2005/10/20 18:39:37 jnekl Exp $
 *
 * This implements the parallel interface module for the 
 * ez8 on-chip debugger.
 */

#ifndef	OCD_PARPORT_HEADER
#define	OCD_PARPORT_HEADER

#include	<stdlib.h>
#include	<inttypes.h>

#include	"ocd.h"

/**************************************************************/

class ocd_parport : public ocd
{
private:
	/* Prohibit use of copy constructor */
	ocd_parport(ocd_parport &);	

public:
	ocd_parport(void);
	~ocd_parport(void);

	void connect(const char *);
	void reset(void);

	bool link_open(void);
	bool link_up(void);
	int  link_speed(void);
	void set_baudrate(int);
	void set_timeout(int);

	bool available(void);
	bool error(void);

	void write(const uint8_t *, size_t);
	void read(uint8_t *, size_t);
};

/**************************************************************/

#endif	/* OCD_PARPORT_HEADER */

