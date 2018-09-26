/* Copyright (C) 2002, 2003, 2004 Zilog, Inc.
 *
 * $Id: ez8ocd.cpp,v 1.4 2005/10/20 18:39:37 jnekl Exp $
 *
 * This implements the low level functions of the Z8 Encore
 * On-Chip Debugger.
 */

#include	<stdio.h>
#include	<string.h>
#include	<stdlib.h>
#include	<assert.h>
#include	<stdarg.h>

#include	"xmalloc.h"
#include	"err_msg.h"

#include	"ocd.h"
#include	"ocd_serial.h"
#include	"ocd_parport.h"
#include	"ocd_tcpip.h"
#include	"ez8ocd.h"
#include	"ez8.h"

/**************************************************************
 * Constructor for our debugger. Only need to initialize status.
 */

ez8ocd::ez8ocd()
{
	log_proto = NULL;
	dbg = NULL;
	cache = 0;
	mtu = 0;
	callback = NULL;

	return;
}

/**************************************************************
 * Destructor for our debugger. If connected to serial port,
 * disconnect us.
 */

ez8ocd::~ez8ocd()
{
	if(dbg) {
		delete dbg;
		dbg = NULL;
	}

	return;
}

/**************************************************************
 * This will connect the debugger to the specified serial port.
 */

void ez8ocd::connect_serial(const char *device, int baudrate)
{
	ocd_serial *ocdptr;

	if(dbg) {
		strncpy(err_msg, "Cannot connect to serial port\n"
		    "already connected\n", err_len-1);
		throw err_msg;
	}

	ocdptr = new ocd_serial();

	try {
		ocdptr->connect(device, baudrate);
	} catch(char *err) {
		delete ocdptr;
		throw err;
	}

	dbg = ocdptr;

	return;
}

/**************************************************************
 * This will connect the debugger to the specified parallel port.
 */

void ez8ocd::connect_parport(const char *device)
{
	ocd_parport *ocdptr;

	if(dbg) {
		strncpy(err_msg, "Cannot connect to parallel port\n"
		    "already connected\n", err_len-1);
		throw err_msg;
	}

	ocdptr = new ocd_parport();

	try {
		ocdptr->connect(device);
	} catch(char *err) {
		delete ocdptr;
		throw err;
	}

	dbg = ocdptr;

	return;
}

/**************************************************************
 * This will connect the debugger to the specified serial port.
 */

void ez8ocd::connect_tcpip(const char *device)
{
	ocd_tcpip *ocdptr;

	if(dbg) {
		strncpy(err_msg, "Cannot connect to server\n"
		    "already connected\n", err_len-1);
		throw err_msg;
	}

	ocdptr = new ocd_tcpip();

	try {
		ocdptr->connect(device);
	} catch(char *err) {
		delete ocdptr;
		throw err;
	}

	dbg = ocdptr;

	return;
}

/**************************************************************
 * If we are currently connected to an interface, disconnect
 * from it.
 */

void ez8ocd::disconnect(void)
{
	if(!dbg) {
		strncpy(err_msg, "Cannot disconnect from on-chip debugger\n"
		    "not connected\n", err_len-1);
		throw err_msg;
	}

	delete dbg;
	dbg = NULL;

	return;
}

/**************************************************************
 * This will read data from the on-chip debugger.
 */

void ez8ocd::read(uint8_t *buff, size_t size)
{
	assert(buff != NULL);
	assert(size != 0);

	if(!dbg) {
		strncpy(err_msg, "Cannot read from on-chip debugger\n"
		    "not connected\n", err_len-1);
		throw err_msg;
	}

	if(callback) {
		callback();
	}
	try {
		dbg->read(buff, size);
	} catch(char *err) {
		if(log_proto) {
			fprintf(log_proto, "%s", err);
		}
		throw err;
	}

	/* if protocol logging enabled, log what we read */
	if(log_proto) {
		size_t i;

		fprintf(log_proto, "dbg ->\t");

		for(i=0; i<size; i++) {
			if(i % 16 == 0 && i) {
				fprintf(log_proto, "\n\t");
			} else if(i % 8 == 0 && i) {
				fprintf(log_proto, " ");
			}
			fprintf(log_proto, "%02X ", buff[i]);
		}
		fprintf(log_proto, "\n");
	}

	return;
}

/**************************************************************
 * This will write data to the on-chip debugger. 
 */

void ez8ocd::write(const uint8_t *buff, size_t size)
{
	assert(buff != NULL);
	assert(size != 0);

	if(!dbg) {
		strncpy(err_msg, "Cannot write to on-chip debugger\n"
		    "not connected\n", err_len-1);
		throw err_msg;
	}

	while(size > 0) {
		int len;

		if(mtu > 0) {
			len = mtu < size ? mtu : size;
		} else {
			len = size;
		}
		if(log_proto) {
			int i;

			fprintf(log_proto, "dbg <-\t");

			for(i=0; i<len; i++) {
				if(i % 16 == 0 && i) {
					fprintf(log_proto, "\n\t");
				} else if(i % 8 == 0 && i) {
					fprintf(log_proto, " ");
				}
				fprintf(log_proto, "%02X ", buff[i]);
			}
			fprintf(log_proto, "\n");
		}

		if(callback) {
			callback();
		}
		try {
			dbg->write(buff, len);
		} catch(char *err) {
			if(log_proto) {
				fprintf(log_proto, "%s", err);
			}
			throw err;
		}

		buff += len;
		size -= len;
	}

	return;
}

/**************************************************************
 * This will determine if the on-chip debugger link is open.
 */

bool ez8ocd::link_open(void)
{
	if(!dbg) {
		strncpy(err_msg, "Cannot retrieve link status\n"
		    "not connected\n", err_len-1);
		throw err_msg;
	}

	return dbg->link_open();
}


/**************************************************************
 * This will determine if the on-chip debugger link is up.
 */

bool ez8ocd::link_up(void)
{
	if(!dbg) {
		strncpy(err_msg, "Cannot retrieve link status\n"
		    "not connected\n", err_len-1);
		throw err_msg;
	}

	return dbg->link_up();
}

/**************************************************************
 * This will return the speed (baudrate) of the link.
 */

int ez8ocd::link_speed(void)
{
	if(!dbg) {
		strncpy(err_msg, "Cannot retrieve link speed\n"
		    "not connected\n", err_len-1);
		throw err_msg;
	}

	return dbg->link_speed();
}

/**************************************************************
 * This will set the baudrate
 */

void ez8ocd::set_baudrate(int baud)
{
	dbg->set_baudrate(baud);
}

/**************************************************************
 * Set serial port timeout
 */

void ez8ocd::set_timeout(int timeout)
{
	dbg->set_timeout(timeout);
}

/**************************************************************
 * This will autobaud and initialize the on-chip debugger.
 */

void ez8ocd::reset_link(void)
{
	if(!dbg) {
		strncpy(err_msg, "Cannot reset link to on-chip debugger\n"
		    "not connected\n", err_len-1);
		throw err_msg;
	}

	dbg->reset();

	return;
}

/**************************************************************
 * Possibly reset link if needed.
 */

void ez8ocd::new_command(void)
{
	if(dbg->error()) {
		dbg->reset();
		cache = 0;
	}
}

/**************************************************************
 * This will retrieve the ez8 DBG RevID.
 */

uint16_t ez8ocd::rd_revid(void)
{
	uint16_t revid;
	uint8_t command[1];
	uint8_t data[2];

	command[0] = DBG_CMD_RD_REVID;

	new_command();
	write(command, 1);
	read(data, 2);

	revid = (data[0] << 8) | data[1];

	return revid;
}

/**************************************************************
 * This willl retrieve the dbgstat register.
 */

uint8_t ez8ocd::rd_dbgstat(void)
{
	uint8_t command[1];
	uint8_t data[1];

	command[0] = DBG_CMD_RD_DBGSTAT;

	new_command();
	write(command, 1);
	read(data, 1);

	return *data;
}

/**************************************************************
 * This will attempt to read an acknowledge character.
 *
 * Returns non-zero if an acknowledge was read. Throws an
 * exception if an error occurs.
 */

bool ez8ocd::rd_ack(void)
{
	uint8_t data[1];

	if(!dbg->available()) {
		return 0;
	}

	read(data, 1);

	if(*data != 0xff) {
		strncpy(err_msg, 
		    "Read acknowledge form on-chip debugger failed\n"
		    "acknowledge not FF\n", err_len-1);
		throw err_msg;
	}

	return 1;
}

/**************************************************************
 * This will write to the debug control register.
 */

void ez8ocd::wr_dbgctl(uint8_t data)
{
	uint8_t command[2];

	command[0] = DBG_CMD_WR_DBGCTL;
	command[1] = data;

	new_command();
	write(command, 2);

	return;
}

/**************************************************************
 * This will read the debug control register.
 */

uint8_t ez8ocd::rd_dbgctl(void)
{
	uint8_t command[1];
	uint8_t data[1];

	command[0] = DBG_CMD_RD_DBGCTL;

	new_command();
	write(command, 1);
	read(data, 1);

	return *data;
}

/**************************************************************
 * This will write the 16 bit debug cntrreg.
 */

void ez8ocd::wr_cntr(uint16_t cntr) 
{
	uint8_t command[3];

	command[0] = DBG_CMD_WR_CNTR;
	command[1] = (cntr >> 8) & 0xff;
	command[2] = cntr & 0xff;

	new_command();
	write(command, 3);

	return;
}

/**************************************************************
 * This will read the 16 bit debug cntrreg.
 */

uint16_t ez8ocd::rd_cntr(void) 
{
	uint16_t cntr;
	uint8_t command[1];
	uint8_t data[2];

	command[0] = DBG_CMD_RD_CNTR;

	new_command();
	write(command, 1);
	read(data, 2);

	cntr = (data[0] << 8) | data[1];

	return cntr;
}

/**************************************************************
 * This will write the program counter.
 */

void ez8ocd::wr_pc(uint16_t pc)
{
	uint8_t command[3];

	command[0] = DBG_CMD_WR_PC;
	command[1] = (pc >> 8) & 0xff;
	command[2] = pc & 0xff;

	new_command();
	write(command, 3);

	return;
}

/**************************************************************
 * This will read the program counter.
 */

uint16_t ez8ocd::rd_pc(void)
{
	uint16_t pc;
	uint8_t command[1];
	uint8_t data[2];

	command[0] = DBG_CMD_RD_PC;

	new_command();
	write(command, 1);
	read(data, 2);

	pc = (data[0] << 8) | data[1];

	return pc;
}

/**************************************************************
 * This will write data to ez8 register memory.
 */

void ez8ocd::wr_regs(uint16_t address, const uint8_t *buff, size_t size)
{
	uint8_t command[4];

	assert((long)address + size <= EZ8REG_SIZE);
	assert(size != 0);

	command[0] = DBG_CMD_WR_REG;

	while(size > 0) {
		size_t len;

		len = size > EZ8REG_BUFSIZ ?  EZ8REG_BUFSIZ : size;

		command[1] = (address >> 8) & 0xff;
		command[2] = address & 0xff;
		command[3] = len < EZ8REG_BUFSIZ ? len & 0xff : 0;

		new_command();
		write(command, 4);
		write(buff, len);

		address += len;
		buff += len;
		size -= len;
	}

	return;
}

/**************************************************************
 * This will read data from ez8 register memory.
 */

void ez8ocd::rd_regs(uint16_t address, uint8_t *buff, size_t size)
{
	uint8_t command[4];

	assert((long)address + size <= EZ8REG_SIZE);
	assert(size != 0);

	command[0] = DBG_CMD_RD_REG;

	while(size > 0) {
		size_t len;

		len = size > EZ8REG_BUFSIZ ? EZ8REG_BUFSIZ : size;
		if(mtu > 0 && len > mtu-1) {
			len = mtu-1;
		}

		command[1] = (address >> 8) & 0xff;
		command[2] = address & 0xff;
		command[3] = len < EZ8REG_BUFSIZ ?  len & 0xff : 0;

		if(mtu > 0 && len+4 > mtu) {
			new_command();
			write(command, 3);
			write(command+3, 1);
		} else {
			new_command();
			write(command, 4);
		}

		read(buff, len);

		address += len;
		buff += len;
		size -= len;
	}

	return;
}

/**************************************************************
 * This will write data to ez8 program memory.
 */

void ez8ocd::wr_mem(uint16_t address, const uint8_t *buff, size_t size)
{
	uint8_t command[5];

	assert((long)address + size <= EZ8MEM_SIZE);

	if(size > 0) {

		command[0] = DBG_CMD_WR_MEM;
		command[1] = (address >> 8) & 0xff;
		command[2] = address & 0xff;
		command[3] = (size >> 8) & 0xff;
		command[4] = size & 0xff;

		write(command, 5);
		write(buff, size);
	}

	return;
}

/**************************************************************
 * This will read data from ez8 program memory.
 */

void ez8ocd::rd_mem(uint16_t address, uint8_t *buff, size_t size)
{
	uint8_t command[5];

	assert(address + size <= EZ8MEM_SIZE);

	while(size > 0) {
		size_t len;

		len = size;
		if(mtu > 0 && len > mtu-1) { 
			len = mtu-1;
		} 

		command[0] = DBG_CMD_RD_MEM;
		command[1] = (address >> 8) & 0xff;
		command[2] = address & 0xff;
		command[3] = (len >> 8) & 0xff;
		command[4] = len & 0xff;
	
		if(mtu > 0 && len+5 > mtu) {
			write(command, 4);
			write(command+4, 1);
		} else {
			write(command, 5);
		}

		read(buff, len);

		address += len;
		buff += len;
		size -= len;
	}

	return;
}

/**************************************************************
 * This will write data to ez8 external data memory.
 */

void ez8ocd::wr_data(uint16_t address, const uint8_t *buff, size_t size)
{
	uint8_t command[5];

	assert((long)address + size <= EZ8MEM_SIZE);

	if(size > 0) {

		command[0] = DBG_CMD_WR_EDATA;
		command[1] = (address >> 8) & 0xff;
		command[2] = address & 0xff;
		command[3] = (size >> 8) & 0xff;
		command[4] = size & 0xff;
	
		write(command, 5);
		write(buff, size);
	}

	return;
}

/**************************************************************
 * This will read data from ez8 external data memory.
 */

void ez8ocd::rd_data(uint16_t address, uint8_t *buff, size_t size)
{
	uint8_t command[5];

	assert((long)address + size <= EZ8MEM_SIZE);

	while(size > 0) {
		size_t len;

		len = size;
		if(mtu > 0 && len > mtu-1) {
			len = mtu-1;
		} 

		command[0] = DBG_CMD_RD_EDATA;
		command[1] = (address >> 8) & 0xff;
		command[2] = address & 0xff;
		command[3] = (len >> 8) & 0xff;
		command[4] = len & 0xff;

		if(mtu > 0 && len+5 > mtu) {
			write(command, 4);
			write(command+4, 1);
		} else {
			write(command, 5);
		}

		read(buff, len);

		address += len;
		buff += len;
		size -= len;
	}

	return;
}

/**************************************************************
 * This will retrieve the program memory crc.
 */

uint16_t ez8ocd::rd_crc(void)
{
	uint16_t crc;
	uint8_t command[1];
	uint8_t data[2];

	command[0] = DBG_CMD_RD_MEMCRC;
		
	write(command, 1);
	read(data, 2);

	crc = (data[0] << 8) | data[1];

	return crc;
}

/**************************************************************
 * This will single step one instruction.
 */

void ez8ocd::step_inst(void)
{
	uint8_t command[1];

	command[0] = DBG_CMD_STEP_INST;

	write(command, 1);

	return;
}

/**************************************************************
 * This will step one instruction, stuffing the first byte of
 * the opcode.
 */

void ez8ocd::stuf_inst(uint8_t opcode)
{
	uint8_t command[2];

	command[0] = DBG_CMD_STUFF_INST;
	command[1] = opcode;

	write(command, 2);

	return;
}

/**************************************************************
 * This will stuff the given opcode and execute it.
 */

void ez8ocd::exec_inst(const uint8_t *opcodes, size_t size)
{
	uint8_t command[6];

	assert(size <= 5);

	command[0] = DBG_CMD_EXEC_INST;
	memcpy(command+1, opcodes, size);

	write(command, size+1);

	return;
}

/**************************************************************/

uint16_t ez8ocd::rd_reload(void)
{
	uint8_t command[1];
	uint8_t data[2];
	uint16_t reload;

	command[0] = DBG_CMD_RD_RELOAD;

	new_command();
	write(command, 1);
	read(data, 2);

	reload = (data[0] << 8) | data[1];

	return reload;
}

/**************************************************************/

uint8_t ez8ocd::rd_memsize(void)
{
	uint8_t command[2];
	uint8_t data[1];

	command[0] = 0xf3;
	command[1] = 0x84;

	write(command, 2);
	read(data, 1);

	return *data;
}

/**************************************************************/

uint8_t ez8ocd::rd_trce_status(void)
{
	uint8_t command[2];
	uint8_t data[1];

	command[0] = DBG_CMD_TRCE_CMD;
	command[1] = TRCE_CMD_RD_TRCE_STATUS;

	write(command, 2);
	read(data, 1);

	return *data;
}

/**************************************************************/

void ez8ocd::wr_trce_ctl(uint8_t ctl)
{
	uint8_t command[3];

	command[0] = DBG_CMD_TRCE_CMD;
	command[1] = TRCE_CMD_WR_TRCE_CTL;
	command[2] = ctl;

	write(command, 3);

	return;
}

/**************************************************************/

uint8_t ez8ocd::rd_trce_ctl(void)
{
	uint8_t command[2];
	uint8_t data[1];

	command[0] = DBG_CMD_TRCE_CMD;
	command[1] = TRCE_CMD_RD_TRCE_CTL;

	write(command, 2);
	read(data, 1);

	return *data;
}

/**************************************************************/

void ez8ocd::wr_trce_event(uint8_t event_num, struct trce_event *event)
{
	uint8_t command[16];

	assert(event != NULL);	

	command[0] = DBG_CMD_TRCE_CMD;
	command[1] = TRCE_CMD_WR_TRCE_EVENT;

	command[2] = event_num;
	command[3] = event->ctl;

	command[4] = (event->mask.reg_addr >> 8) & 0xff;
	command[5] = event->mask.reg_addr & 0xff;
	command[6] = event->mask.reg_data;
	command[7] = event->mask.cpu_flags;
	command[8] = (event->mask.pc >> 8) & 0xff;
	command[9] = event->mask.pc & 0xff;

	command[10] = (event->data.reg_addr >> 8) & 0xff;
	command[11] = event->data.reg_addr & 0xff;
	command[12] = event->data.reg_data;
	command[13] = event->data.cpu_flags;
	command[14] = (event->data.pc >> 8) & 0xff;
	command[15] = event->data.pc & 0xff;

	write(command, 16);

	return;
}

/**************************************************************/

void ez8ocd::rd_trce_event(uint8_t event_num, struct trce_event *event)
{
	uint8_t command[3];
	uint8_t data[13];

	command[0] = DBG_CMD_TRCE_CMD;
	command[1] = TRCE_CMD_RD_TRCE_EVENT;
	command[2] = event_num;

	write(command, 3);
	read(data, 13);

	assert(event != NULL);
	event->ctl = data[0];

	event->mask.reg_addr = (data[1] << 8) | data[2];
	event->mask.reg_data = data[3];
	event->mask.cpu_flags = data[4];
	event->mask.pc = (data[5] << 8) | data[6];

	event->data.reg_addr = (data[7] << 8) | data[8];
	event->data.reg_data = data[9];
	event->data.cpu_flags = data[10];
	event->data.pc = (data[11] << 8) | data[12];

	return;
}

/**************************************************************/

uint16_t ez8ocd::rd_trce_wr_ptr(void)
{
	uint16_t wr_ptr;
	uint8_t command[2];
	uint8_t data[2];

	command[0] = DBG_CMD_TRCE_CMD;
	command[1] = TRCE_CMD_RD_TRCE_WR_PTR;

	write(command, 2);
	read(data, 2);

	wr_ptr = (data[0] << 8) | data[1];

	return wr_ptr;
}

/**************************************************************/

void ez8ocd::rd_trce_buff(uint16_t addr, struct trce_frame *buff, 
                           size_t size)
{
	unsigned int i;
	uint8_t command[6];
	uint8_t *data;

	assert(buff != NULL);

	size &= 0xffff;

	command[0] = DBG_CMD_TRCE_CMD;
	command[1] = TRCE_CMD_RD_TRCE_BUFF;
	command[2] = (addr >> 8) & 0xff;
	command[3] = addr & 0xff;
	command[4] = (size >> 8) & 0xff;
	command[5] = size & 0xff;

	write(command, 6);

	if(!size) {
		size = 0x10000;
	}

	data = (uint8_t *)xmalloc(size * 8);

	read(data, size * 8);

	for(i=0; i<size; i++) {
		int j;

		for(j=0; j<8; j++) {
			buff[i].data[j] = *data++;
		}
	}

	free(data);

	return;
}

/**************************************************************
 * Return pointer to ocd link.
 */

ocd *ez8ocd::iflink(void)
{
	return dbg;
}

/**************************************************************/



