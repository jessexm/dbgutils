/* Copyright (C) 2002, 2003, 2004 Zilog, Inc.
 *
 * $Id: ez8dbg.h,v 1.4 2005/10/20 18:40:40 jnekl Exp $
 *
 * This implements a debugger api for the ez8 on-chip debugger.
 */

#ifndef	EZ8DBG_HEADER
#define	EZ8DBG_HEADER

#include	<stdlib.h>
#include	<inttypes.h>

#include	"ez8ocd.h"

/**************************************************************/

/* cache status */
#define	REVID_CACHED	0x0001
#define	DBGCTL_CACHED	0x0002
#define	DBGSTAT_CACHED	0x0004
#define	PC_CACHED	0x0008
#define	CRC_CACHED	0x0010
#define	MEMCRC_CACHED	0x0020
#define	MEMSIZE_CACHED	0x0040
#define	FCTL_CACHED	0x0080
#define	BAUDRATE_CACHED	0x0100
#define	RELOAD_CACHED	0x0200
#define	SYSCLK_CACHED	0x0400
#define	FREQ_CACHED	0x0800
#define	TIMEOUT_CACHED	0x1000

/* 5 second reset timeout (typical reset is 10ms) */
#define	RESET_TIMEOUT	5

/**************************************************************/

class ez8dbg : public ez8ocd
{
private:
	/* pending error */
	int error;

	/* copy constructor prohibited */
	ez8dbg(ez8dbg &);	

	/* cached data */
	uint16_t revid;
	uint8_t dbgctl;
	uint8_t dbgstat;
	uint16_t pc;
	uint16_t crc;
	uint16_t memcrc;
	uint8_t memsize;
	uint16_t reload;
	uint16_t freq;
	int baudrate;
	int timeout;

	/* buffers */
	uint8_t *main_mem;
	uint8_t *info_mem;
	uint8_t *reg_mem;
	uint8_t *buffer;

	/* breakpoints */
	struct breakpoint_t {
		uint16_t address;
		uint8_t data;
	};
	struct breakpoint_t *breakpoints;
	int num_breakpoints;
	uint16_t tbreak;
	void delete_breakpoint(int);

	/* internal functions */
	uint8_t  cached_dbgctl(void);
	uint8_t  cached_dbgstat(void);
	uint16_t cached_memcrc(void);
	uint8_t  cached_memsize(void);
	void cache_freq(void);
	void set_timeout(void);

public:
	int sysclk;

	void save_flash_state(uint8_t *);
	void restore_flash_state(uint8_t *);
	void flash_setup(uint8_t);
	void flash_lock(void);
	void flash_page_erase(uint8_t);

	ez8dbg();
	~ez8dbg();

	bool memcache_enabled;

	enum dbg_state {
		state_stopped = 1,
		state_running,
		state_protected,
		state_trace,
	};

	bool state(enum dbg_state);
	void flush_cache(void);

	void reset_chip(void);
	void reset_link(void);

	void stop(void);
	void run(void);
	void run_to(uint16_t);
	void run_clks(uint16_t);
	int isrunning(void);

	void step(void);
	void next(void);

	uint16_t rd_revid(void);
	uint16_t rd_crc(void);
	uint16_t rd_pc(void);
	uint16_t rd_cntr(void);
	uint16_t rd_reload(void);
	void wr_pc(uint16_t);
	void rd_regs(uint16_t, uint8_t *, size_t);
	void wr_regs(uint16_t, const uint8_t *, size_t);
	void rd_data(uint16_t, uint8_t *, size_t);
	void wr_data(uint16_t, const uint8_t *, size_t);
	void rd_mem(uint16_t, uint8_t *, size_t);
	void wr_mem(uint16_t, const uint8_t *, size_t);

	void rd_info(uint16_t, uint8_t *, size_t);
	void wr_info(uint16_t, const uint8_t *, size_t);

	void set_sysclk(int);
	void mass_erase(bool);
	void flash_mass_erase(void);
	void write_flash(uint16_t, const uint8_t *, size_t);

	bool breakpoint_set(uint16_t);
	int get_num_breakpoints(void);
	uint16_t get_breakpoint(int);
	void set_breakpoint(uint16_t);
	void remove_breakpoint(uint16_t);
	void read_mem(uint16_t, uint8_t *, size_t);

	int memory_size(void);
	int cached_baudrate(void);
	int cached_sysclk(void);
	uint16_t cached_revid(void);
	uint16_t cached_crc(void);
	uint16_t cached_reload(void);
	uint16_t cached_pc(void);

	uint8_t rd_trce_status(void);
	void wr_trce_ctl(uint8_t);	
	uint8_t rd_trce_ctl(void);
	void wr_trce_event(uint8_t, struct trce_event *);
	void rd_trce_event(uint8_t, struct trce_event *);
	uint16_t rd_trce_wr_ptr(void);
	void rd_trce_buff(uint16_t, struct trce_frame *, size_t);

};

/**************************************************************/

#endif	/* EZ8DBG_HEADER */

