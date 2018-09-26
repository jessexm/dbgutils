/* Copyright (C) 2002, 2003, 2004 Zilog, Inc.
 *
 * $Id: ez8.h,v 1.2 2004/12/01 01:26:49 jnekl Exp $
 */

#ifndef	EZ8_HEADER
#define	EZ8_HEADER


#define EZ8_PERIPHERIAL_BASE    0xf00
#define EZ8_FIF_BASE            0xff8

#define EZ8_FIF_LOCK            0x00
#define EZ8_FIF_UNLOCK_0        0x73
#define EZ8_FIF_UNLOCK_1        0x8c
#define EZ8_FIF_PROT_REG        0x5e
#define EZ8_FIF_PAGE_ERASE      0x95
#define EZ8_FIF_MASS_ERASE      0x63
#define EZ8_FIF_UNLOCKED        0x02

#define	EZ8_IRQCTL		0xfcf
#define	EZ8_FLAGS		0xffc
#define	EZ8_RP			0xffd
#define	EZ8_SPH			0xffe
#define	EZ8_SPL			0xfff

#define	EZ8_EI_OPCODE		0x9f
#define	EZ8_DI_OPCODE		0x8f

/* on-chip debugger commands */
#define DBG_CMD_AUTOBAUD        0x80
#define DBG_CMD_RD_REVID        0x00
#define DBG_CMD_WR_CNTR         0x01
#define DBG_CMD_RD_DBGSTAT      0x02
#define DBG_CMD_RD_CNTR         0x03
#define DBG_CMD_WR_DBGCTL       0x04
#define DBG_CMD_RD_DBGCTL       0x05
#define DBG_CMD_WR_PC           0x06
#define DBG_CMD_RD_PC           0x07
#define DBG_CMD_WR_REG          0x08
#define DBG_CMD_RD_REG          0x09
#define DBG_CMD_WR_MEM          0x0a
#define DBG_CMD_RD_MEM          0x0b
#define DBG_CMD_WR_EDATA        0x0c
#define DBG_CMD_RD_EDATA        0x0d
#define DBG_CMD_RD_MEMCRC       0x0e
#define DBG_CMD_STEP_INST       0x10
#define DBG_CMD_STUFF_INST      0x11
#define DBG_CMD_EXEC_INST       0x12
#define DBG_CMD_RD_RELOAD       0x1b
#define DBG_CMD_TRCE_CMD        0x40

#define TRCE_CMD_RD_TRCE_STATUS 0x01
#define TRCE_CMD_WR_TRCE_CTL    0x02
#define TRCE_CMD_RD_TRCE_CTL    0x03
#define TRCE_CMD_WR_TRCE_EVENT  0x04
#define TRCE_CMD_RD_TRCE_EVENT  0x05
#define TRCE_CMD_RD_TRCE_WR_PTR 0x06
#define TRCE_CMD_RD_TRCE_BUFF   0x08

/* DBGCTL register */
#define DBGCTL_DBG_MODE         0x80
#define DBGCTL_BRK_EN           0x40
#define DBGCTL_BRK_ACK          0x20
#define DBGCTL_BRK_LOOP         0x10
#define DBGCTL_BRK_PC           0x08
#define DBGCTL_BRK_CNTR         0x04
#define DBGCTL_RST              0x01

/* DBGSTAT register */
#define DBGSTAT_STOPPED         0x80
#define DBGSTAT_HALT_MODE       0x40
#define DBGSTAT_RD_PROTECT      0x20
#define DBGSTAT_PARAM_UL        0x10

/* eZ8 limits */
#define	EZ8MEM_SIZE	0x10000
#define	EZ8REG_SIZE	0x1000
#define	EZ8MEM_PAGESIZE	512

#define	EZ8MEM_BUFSIZ	512
#define	EZ8REG_BUFSIZ	256

#endif	/* EZ8_HEADER */

