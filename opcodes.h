/* Copyright (C) 2002, 2003, 2004 Zilog, Inc.
 *
 * $Id: opcodes.h,v 1.1 2004/08/03 14:23:48 jnekl Exp $
 */

#ifndef	OPCODES_HEADER
#define	OPCODES_HEADER

#include	"inttypes.h"
#include	<sys/types.h>

#define	ALT_OPCODE	0x1f


extern const char *cc_mnem[];

enum address_mode_t {
	am_none = 0,
	am_IM,
	am_r1,
	am_R1,
	am_IR1,
	am_RR1,
	am_IRR1,
	am_ER1,
	am_r1_IM,
	am_r1_RA, 
	am_r1_r2,
	am_r1_Irr2,
	am_r2_Irr1,
	am_r1_Ir2,
	am_Ir1_r2,
	am_Ir1_Irr2,
	am_Ir2_Irr1,
	am_r1_ER2,
	am_r2_ER1,
	am_Ir1_ER2,
	am_Ir2_ER1,
	am_r1_r2_X,
	am_r2_r1_X,
	am_r1_rr2_X,
	am_rr1_r2_X,
	am_rr1_rr2_X,
	am_R1_IM,
	am_IR1_IM,
	am_R2_R1,
	am_IR2_R1,
	am_R2_IR1,
	am_IRR2_R1,
	am_R2_IRR1,
	am_IRR2_IR1,
	am_IR2_IRR1,
	am_ER2_ER1,
	am_IM_ER1,
	am_vec,
	am_DA,
	am_cc_RA,
	am_cc_DA,
	am_p_bit_r1,
	am_p_bit_r1_RA,
	am_p_bit_Ir1_RA
};

struct opcode_t {
	char *mnemonic;
	uint8_t opcode;
	enum address_mode_t am;
};

extern const struct opcode_t z9_opcodes[];
extern const struct opcode_t z9_alt_opcodes[];


#endif	/* OPCODES_HEADER */


