/* Copyright (C) 2002, 2003, 2004 Zilog, Inc.
 *
 * $Id: ez8ocd.h,v 1.4 2005/10/20 18:39:37 jnekl Exp $
 *
 * This class implements the basic ez8 on-chip debugger 
 * commands.
 */

#ifndef	EZ8OCD_HEADER
#define	EZ8OCD_HEADER

#include	<stdio.h>
#include	<stdlib.h>
#include	<inttypes.h>
#include	"ocd.h"

/**************************************************************/

struct trce_data {
	uint16_t reg_addr;
	uint8_t  reg_data;
	uint8_t  cpu_flags;
	uint16_t pc;
};

struct trce_event {
	uint8_t ctl;
	struct trce_data mask;
	struct trce_data data;
};

struct trce_frame {
	uint8_t data[8];
};

enum ocd_exception {
	ocd_none,
	ocd_init,
};

/**************************************************************/

class ez8ocd
{
private:
	/* Prohibit use of copy constructor */
	ez8ocd(ez8ocd &);

protected:
	int cache;

public:
	/* polymorphic class for ocd link */
	ocd *dbg;

	void new_command(void);
	bool rd_ack(void);

	void wr_dbgctl(uint8_t);
	uint8_t rd_dbgctl(void);

	void wr_cntr(uint16_t);
	uint16_t rd_cntr(void);

	void wr_pc(uint16_t);
	uint16_t rd_pc(void);

	void wr_regs(uint16_t, const uint8_t *, size_t);
	void rd_regs(uint16_t, uint8_t *, size_t);

	void wr_mem(uint16_t, const uint8_t *, size_t);
	void rd_mem(uint16_t, uint8_t *, size_t);

	void wr_data(uint16_t, const uint8_t *, size_t);
	void rd_data(uint16_t, uint8_t *, size_t);

	uint16_t rd_crc(void);

	void step_inst(void);
	void stuf_inst(uint8_t);
	void exec_inst(const uint8_t *, size_t);

	uint8_t rd_memsize(void);

	/* emulator trace buffer commands */
	uint8_t rd_trce_status(void);
	void wr_trce_ctl(uint8_t);
	uint8_t rd_trce_ctl(void);
	void wr_trce_event(uint8_t, struct trce_event *);
	void rd_trce_event(uint8_t, struct trce_event *);
	uint16_t rd_trce_wr_ptr(void);
	void rd_trce_buff(uint16_t, struct trce_frame *, size_t);

public:
	size_t mtu;
	FILE *log_proto;

	ez8ocd();
	~ez8ocd();

	void (*callback)(void);

	void connect_serial(const char *, int);
	void connect_parport(const char *);
	void connect_tcpip(const char *);
	void disconnect(void);
	ocd *iflink(void);

	bool link_open(void);
	bool link_up(void);
	int  link_speed(void);
	void reset_link(void);
	void set_timeout(int);
	void set_baudrate(int);

	void read(uint8_t *, size_t);
	void write(const uint8_t *, size_t);

	uint16_t rd_revid(void);
	uint16_t rd_reload(void);
	uint8_t rd_dbgstat(void);
};

/**************************************************************/

#endif	/* EZ8OCD_HEADER */

