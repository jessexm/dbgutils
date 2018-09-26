/* Copyright (C) 2002, 2003, 2004 Zilog, Inc.
 *
 * $Id: ez8dbg_flash.cpp,v 1.3 2005/10/20 18:39:37 jnekl Exp $
 *
 * This implements the flash routines of the debugger api.
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
#include	<time.h>
#include	"xmalloc.h"

#ifdef	_WIN32
#include	"winunistd.h"
#endif

#include	"ez8dbg.h"
#include	"ez8.h"
#include	"crc.h"
#include	"err_msg.h"

/**************************************************************
 * This function does the opposite of the memchr function. It
 * returns a pointer to the first non-occurance of c within 
 * the block of data s of size n. If c is the only character 
 * that occurs within s, then NULL is returned.
 */

static void *memnchr(void *s, char c, size_t n)
{
	char *p;

	for(p=(char *)s; n>0; n--, p++) {
		if(*p != c) {
			return p;
		}
	}

	return NULL;
}

/**************************************************************
 * This will read the specified range of program memory.
 * 
 * If memory cache is enabled, this function will copy data
 * from the local cache if CRC checksums matches.
 */

void ez8dbg::rd_mem(uint16_t address, uint8_t *data, size_t size)
{
	assert(data != NULL);

	if(address + size > EZ8MEM_SIZE) {
		strncpy(err_msg, "Cannot read memory\n"
		    "invalid address range\n", err_len-1);
		throw err_msg;
	}

	if(!state(state_stopped)) {
		strncpy(err_msg, "Cannot read memory\n"
		    "device is running\n", err_len-1);
		throw err_msg;
	}

	if(state(state_protected)) {
		strncpy(err_msg, "Cannot read memory\n"
		    "memory read protect is enabled\n", err_len-1);
		throw err_msg;
	}

	if(memcache_enabled && memory_size()) {

		if(cached_crc() == cached_memcrc()) {
			size_t length;

			length = memory_size();
			if(address + size > length) {
				memset(data, 0xff, size);
				if(address < length) {
					size = length - address;
				} else {
					size = 0;
				}
			}
			memcpy(data, main_mem + address, size);
			return;
		}
	}

	cache &= ~MEMCRC_CACHED;
	ez8ocd::rd_mem(address, main_mem+address, size);
	memcpy(data, main_mem+address, size);

	return;
}

/**************************************************************
 * This routine will write data to memory, skipping
 * blocks of data that are blank.
 * 
 * This function assumes the flash has already been erased by
 * the calling function.  
 *
 * This function assumes the calling function will verify
 * that programming was successful, possibly by using a CRC.
 */

#define	BLOCK_SIZE	8

void ez8dbg::write_flash(uint16_t address, const uint8_t *buff, size_t size)
{
	const uint8_t *next, *last;

	if(!state(state_stopped)) {
		strncpy(err_msg, "Cannot write flash\n"
		    "device is running\n", err_len-1);
		throw err_msg;
	}

	if(state(state_protected)) {
		strncpy(err_msg, "Cannot write flash\n"
		    "memory read protect is enabled\n", err_len-1);
		throw err_msg;
	}

	/* skip initial blank block */
	next = (uint8_t *)memnchr((void *)buff, 0xff, size);
	last = buff + size;

	while(next) {
		uint16_t block_addr;
		size_t block_len;
		const uint8_t *start, *end;

		/* find start and end of block to write */
		start = next;
		do {
			end = (uint8_t *)memchr((void *)next, 0xff, 
			    last - next);
			if(end) {
				next = (uint8_t *)memnchr((void *)end, 0xff, 
				    last - end);
			} else {
				next = NULL;
			}
		} while(end && next && next - end < BLOCK_SIZE);

		/* compute block address and length */
		block_addr = address + start - buff;
		if(end) {
			block_len = end - start;
		} else {
			block_len = last - start;
		}

		/* write block to flash */
		ez8ocd::wr_mem(block_addr, start, block_len);
	}

	return;
}

/**************************************************************
 * This will program data into flash memory.
 *
 * This routine will erase the flash if needed before 
 * writing it. 
 */

void ez8dbg::wr_mem(uint16_t address, const uint8_t *data, size_t size)
{
	uint16_t addr, block_start, offset;
	size_t block_length;
	uint8_t pages;
	uint8_t flash_state[4];

	/* check arguments and state */
	if(address + size > EZ8MEM_SIZE) {
		strncpy(err_msg, "Could not write memory\n"
		    "invalid address range\n", err_len-1);
		throw err_msg;
	}

	if(!state(state_stopped)) {
		strncpy(err_msg, "Could not write memory\n"
		    "device is running\n", err_len-1);
		throw err_msg;
	}

	if(state(state_protected)) {
		strncpy(err_msg, "Could not write memory\n"
		    "memory read protect is enabled\n", err_len-1);
		throw err_msg;
	}

	save_flash_state(flash_state);

	/* calculate block address (must start on page boundary) */
	offset = address % EZ8MEM_PAGESIZE;
	block_start = address - offset;

	/* calculate block length (must be interval of page size) */
	block_length = (address + size) - block_start;
	offset = block_length % EZ8MEM_PAGESIZE;
	if(offset) {
		block_length += EZ8MEM_PAGESIZE - offset;
	}

	/* read existing data block
	 * - needed to restore data at beginning and end of modified pages
	 * - used to check if page erase needed or not
	 */

	/* validate memory cache */
	if(memcache_enabled && memory_size()) {
		cached_crc();
		cached_memcrc();
	}

	/* if memory cache not enabled or cache is stale,
	 * read memory out of device */
	if(!memcache_enabled || !memory_size() || crc != memcrc) {
		rd_mem(block_start, main_mem + block_start, block_length);
	}

	/* if pages not blank, erase them */
	cache &= ~CRC_CACHED;
	memset(buffer, 0xff, EZ8MEM_SIZE);
	for(addr = block_start, pages = block_length >> 9; 
	    pages > 0; addr+=EZ8MEM_PAGESIZE, pages--) {
		if(memcmp((void *)(main_mem+addr), buffer, EZ8MEM_PAGESIZE)) {
			flash_page_erase(addr>>9);
		}
	}

	/* copy data into block */
	cache &= ~MEMCRC_CACHED;
	memcpy(main_mem+address, data, size); 

	/* write data block to memory */
	flash_setup(0x00);
	write_flash(block_start, main_mem+block_start, block_length);
	flash_lock();

	/* verify data */
	if(memcache_enabled && memory_size() && crc == memcrc) {
		if(cached_crc() != cached_memcrc()) {
			strncpy(err_msg, "Write memory failed\n"
			    "verify with crc failed\n", err_len-1);
			throw err_msg;
		}
	} else {
		rd_mem(block_start, buffer, block_length);
		if(memcmp(buffer, main_mem+block_start, block_length)) {
			strncpy(err_msg, "Write memory failed\n"
			    "verify failed\n", err_len-1);
			throw err_msg;
		}
	}

	restore_flash_state(flash_state);

	return;
}

/**************************************************************
 * This will read from info memory.
 */

void ez8dbg::rd_info(uint16_t address, uint8_t *buff, size_t size)
{
	const uint8_t fpsel[1] = { 0x80 };
	uint8_t regs[4];

	if(address + size > EZ8MEM_PAGESIZE)  {
		strncpy(err_msg, "Could not read information area\n"
		    "invalid address range\n", err_len-1);
		throw err_msg;
	}

	if(!state(state_stopped)) {
		strncpy(err_msg, "Could not read information area\n"
		    "device is running\n", err_len-1);
		throw err_msg;
	}

	if(state(state_protected)) {
		strncpy(err_msg, "Could not read information area\n"
		    "memory read protect is enabled\n", err_len-1);
		throw err_msg;
	}

	save_flash_state(regs);
	wr_regs(EZ8_FIF_BASE + 1, fpsel, 1);

	
	switch(cached_revid()) {
	case 0x0100:
	case 0x8100:
		ez8ocd::rd_data(address+0xff80, buff, size);
		break;
	default:
		ez8ocd::rd_mem(address+0xfe00, buff, size);
		break;
	}

	restore_flash_state(regs);

	return;
}

/**************************************************************
 * This will program data into the information area.
 */

void ez8dbg::wr_info(uint16_t address, const uint8_t *data, size_t size)
{
	const uint8_t fpsel[1] = { 0x80 };
	uint8_t flash_state[4];

	/* check arguments and state */
	if(address + size > EZ8MEM_PAGESIZE) {
		strncpy(err_msg, "Could not write parameter area\n"
		    "invalid address range\n", err_len-1);
		throw err_msg;
	}

	if(!state(state_stopped)) {
		strncpy(err_msg, "Could not write parameter area\n"
		    "device is running\n", err_len-1);
		throw err_msg;
	}

	if(state(state_protected)) {
		strncpy(err_msg, "Could not write parameter area\n"
		    "memory read protect is enabled\n", err_len-1);
		throw err_msg;
	}

	save_flash_state(flash_state);
	wr_regs(EZ8_FIF_BASE + 1, fpsel, 1);

	/* read existing data block
	 * - needed to restore data at beginning and end of page
	 * - used to check if page erase needed or not
	 */

	memset(info_mem, 0xff, EZ8MEM_PAGESIZE);

	switch(cached_revid()) {
	case 0x0100:
	case 0x8100:
		ez8ocd::rd_data(0xff80, info_mem, 0x80);
		break;
	default:
		ez8ocd::rd_mem(0xfe00, info_mem, EZ8MEM_PAGESIZE);
		break;
	}

	/* if not blank, erase info page */
	memset(buffer, 0xff, EZ8MEM_PAGESIZE);
	if(memcmp((void *)(info_mem), buffer, EZ8MEM_PAGESIZE)) {
		flash_page_erase(0x80);
	}

	/* copy data into block */
	memcpy(info_mem+address, data, size); 

	/* write data block to memory */
	flash_setup(0x80);
	switch(cached_revid()) {
	case 0x0100:
	case 0x8100:
		ez8ocd::wr_data(0xff80, info_mem, 0x80);
		break;
	default:
		write_flash(0xfe00, info_mem, EZ8MEM_PAGESIZE);
		break;
	}

	flash_lock();

	/* verify data */
	switch(cached_revid()) {
	case 0x0100:
	case 0x8100:
		ez8ocd::rd_data(0xff80, buffer, 0x80);
		break;
	default:
		ez8ocd::rd_mem(0xfe00, buffer, EZ8MEM_PAGESIZE);
		break;
	}

	if(memcmp(buffer, info_mem, EZ8MEM_PAGESIZE)) {
		strncpy(err_msg, "Write parameter memory failed\n"
		    "verify failed\n", err_len-1);
		throw err_msg;
	}

	restore_flash_state(flash_state);

	return;
}

/**************************************************************
 * This will set the sysclk used for writing the flash.
 */

void ez8dbg::set_sysclk(int clk)
{
	if(clk / 1000 > 0x10000 || clk < 0) {
		sysclk = 0x0000;
		strncpy(err_msg, "Set sysclk failed\n"
		    "value out of range\n", err_len-1);
		throw err_msg;
	}

	sysclk = clk;

	return;
}

/**************************************************************
 * This will save the flash registers for restoring later.
 */

void ez8dbg::save_flash_state(uint8_t *regs)
{
	rd_regs(EZ8_FIF_BASE, regs, 4);
	return;
}

/**************************************************************
 * This will restore the flash registers.
 */

void ez8dbg::restore_flash_state(uint8_t *state)
{
	const uint8_t lock[1] = { EZ8_FIF_LOCK };
	const uint8_t unlock0[1] = { EZ8_FIF_UNLOCK_0 };
	const uint8_t unlock1[1] = { EZ8_FIF_UNLOCK_1 };
	const uint8_t protect[1] = { EZ8_FIF_PROT_REG };
	uint8_t regs[4];
	uint8_t buff[1];

	/* read current state */
	rd_regs(EZ8_FIF_BASE, regs, 4);

	/* if program or erase still in progress, abort */
	if(state[0] > 0x04 || regs[0] > 0x04) {
		strncpy(err_msg, "Could not restore flash controller state\n"
		    "program or erase operation still in progress\n", 
		    err_len-1);
		throw err_msg;
	}

	/* if control register changed, restore */
	if(state[0] != regs[0]) {
		wr_regs(EZ8_FIF_BASE, lock, 1);

		if(regs[0] & 0x03) {
			wr_regs(EZ8_FIF_BASE, unlock0, 1);
			if(regs[0] & 0x02) {
				wr_regs(EZ8_FIF_BASE, unlock1, 1);
			}
		} else if(regs[0] & 0x04) {
			wr_regs(EZ8_FIF_BASE, protect, 1);
		}
	}

	if(state[1] != regs[1]) {	
	/* if page select changed, restore */
		wr_regs(EZ8_FIF_BASE+1, state+1, 1);
		if(state[1] & 0x80) {
		/* if page selects info area, clear info read */
			ez8ocd::rd_mem(0x0000, buff, 1);
		}
	}

	/* if sysclk registers changed, restore */
	if(!memcmp(state+2, regs+2, 2)) {
		wr_regs(EZ8_FIF_BASE+2, state+2, 2);
	}

	return;
}

/**************************************************************
 * This will setup the flash controller for programming/erasure.
 */

void ez8dbg::flash_setup(uint8_t page)
{
	uint8_t data[4];
	const uint8_t unlock0[1] = { EZ8_FIF_UNLOCK_0 };
	const uint8_t unlock1[1] = { EZ8_FIF_UNLOCK_1 };

	if(!state(state_stopped)) {
		strncpy(err_msg, "Could not setup flash controller\n"
		    "device is running\n", err_len-1);
		throw err_msg;
	}

	cache_freq();

	data[0] = EZ8_FIF_LOCK;
	data[1] = page;
	data[2] = (freq >> 8) & 0xff;
	data[3] = freq & 0xff;

	wr_regs(EZ8_FIF_BASE, data, 4);
	wr_regs(EZ8_FIF_BASE, unlock0, 1);
	wr_regs(EZ8_FIF_BASE, unlock1, 1);

	return;
}

/**************************************************************
 * This will lock the flash controller.
 */

void ez8dbg::flash_lock(void)
{
	const uint8_t lock[1] = { EZ8_FIF_LOCK };

	wr_regs(EZ8_FIF_BASE, lock, 1);

	return;
}

/**************************************************************
 * This will erase the specified page of flash.
 *
 * This function does not verify the erasure. It is assumed
 * the calling function will do this (possibly after programming
 * and possibly using a CRC checksum for speed).
 */

#define	PAGE_ERASE_TIMEOUT	4

void ez8dbg::flash_page_erase(uint8_t page)
{
	const uint8_t erase[1] = { EZ8_FIF_PAGE_ERASE };
	uint8_t status[1];
	time_t start;

	if(!state(state_stopped)) {
		strncpy(err_msg, "Could not erase flash\n"
		    "device is running\n", err_len-1);
		throw err_msg;
	}
	if(state(state_protected)) {
		strncpy(err_msg, "Could not erase flash\n"
		    "memory read protect is enabled\n", err_len-1);
		throw err_msg;
	}

	/* execute page erase */
	flash_setup(page);
	wr_regs(EZ8_FIF_BASE, erase, 1);

	start = time(NULL);

	/* wait till erasure done */
	do {
		usleep(10000);
		rd_regs(EZ8_FIF_BASE, status, 1);
	} while(*status & 0x10 && time(NULL)-start < PAGE_ERASE_TIMEOUT);

	if(*status & 0x10) {
		strncpy(err_msg, "Flash erase failed\n"
		    "timeout waiting for erase to finish\n", err_len-1);
		throw err_msg;
	}

	return;	
}

/**************************************************************
 * This will mass erase the flash.
 */

#define	MASS_ERASE_TIMEOUT	20

void ez8dbg::mass_erase(bool info)
{
	const uint8_t erase[1] = { EZ8_FIF_MASS_ERASE };
	uint8_t status[1];
	time_t start;

	if(!state(state_stopped)) {
		strncpy(err_msg, "Could not erase flash\n"
		    "device is running\n", err_len-1);
		throw err_msg;
	}

	/* clear saved breakpoints */
	if(breakpoints) {
		free(breakpoints);
		breakpoints = NULL;
		num_breakpoints = 0;
	}

	/* clear cache memory */
	memset(main_mem, 0xff, EZ8MEM_SIZE);
	if(info) {
		memset(info_mem, 0xff, EZ8MEM_PAGESIZE);
	}

	/* invalidate cache */
	cache &= ~(CRC_CACHED | MEMCRC_CACHED);

	/* execute mass erase */
	flash_setup(info?0x80:0x00);
	wr_regs(EZ8_FIF_BASE, erase, 1);

	if(state(state_protected)) {
		usleep(500000);
		return;
	}

	start = time(NULL);

	do {
		usleep(200000);
		rd_regs(EZ8_FIF_BASE, status, 1);
	} while(*status & 0x20 && time(NULL)-start < MASS_ERASE_TIMEOUT);

	if(*status & 0x20) {
		strncpy(err_msg, "Erase flash failed\n"
		    "timeout waiting for erase to finish\n", err_len-1);
		throw err_msg;
	}

	return;	
}

/**************************************************************/

void ez8dbg::flash_mass_erase(void)
{
	mass_erase(0);
}

/**************************************************************/


