/* Copyright (C) 2002, 2003, 2004 Zilog, Inc.
 *
 * $Id: disassembler.c,v 1.1 2004/08/03 14:23:48 jnekl Exp $
 *
 * This is the disassembler.
 */

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<inttypes.h>
#include	<assert.h>

#include	"opcodes.h"

#define		OP_BUF_SIZ 10

/****************************************************************
 * This will return the size of an opcode.
 *
 */

int opcode_size(const struct opcode_t *op_ptr)
{

	switch(op_ptr->am) {

	case am_ER2_ER1:
	case am_IM_ER1:
		return 4;

	case am_R2_R1:
	case am_IR2_R1:
	case am_R2_IR1:
	case am_R1_IM:
	case am_IR1_IM:
	case am_IRR2_R1:
	case am_R2_IRR1:
	case am_IRR2_IR1:
	case am_IR2_IRR1:
	case am_DA:
	case am_r1_r2_X:
	case am_r2_r1_X:
	case am_r1_rr2_X:
	case am_rr1_r2_X:
	case am_rr1_rr2_X:
	case am_ER1:
	case am_cc_DA:
	case am_r2_ER1:
	case am_r1_ER2:
	case am_Ir2_ER1:
	case am_Ir1_ER2:
	case am_p_bit_r1_RA:
	case am_p_bit_Ir1_RA:
		return 3;

	case am_IM:
	case am_R1:
	case am_RR1:
	case am_IR1:
	case am_IRR1:
	case am_r1_r2:
	case am_r1_Ir2:
	case am_Ir1_r2:
	case am_r1_Irr2:
	case am_r2_Irr1:
	case am_Ir1_Irr2:
	case am_Ir2_Irr1:
	case am_r1_RA:
	case am_cc_RA:
	case am_r1_IM:
	case am_p_bit_r1:
	case am_vec:
		return 2;

	case am_none:
	case am_r1:
		return 1;
	default:
		abort();
	}
}

/****************************************************************
 * This finds and returns the index of our opcode within 
 * the z9_opcodes array.
 *
 * This function will return NULL if the instruction is not 
 * found ('ill' instruction).
 */

const struct opcode_t *find_opcode(uint8_t opcode)
{
	int i;

	if((opcode & 0x0f) >= 0x0a && (opcode & 0x0f) <= 0x0e) {
		opcode &= 0x0f;
	}

	for(i=0; z9_opcodes[i].mnemonic; i++) {
		if(opcode == z9_opcodes[i].opcode) {
			return &z9_opcodes[i];
		}
	}

	return NULL;
}

/****************************************************************
 * This finds and returns the index of our opcode within 
 * the z9_opcodes array.
 *
 * This function will return NULL if the instruction is not 
 * found ('ill' instruction).
 */

const struct opcode_t *find_alt_opcode(uint8_t opcode)
{
	int i;

	for(i=0; z9_alt_opcodes[i].mnemonic; i++) {
		if(opcode == z9_alt_opcodes[i].opcode) {
			return &z9_alt_opcodes[i];
		}
	}

	return NULL;
}

/****************************************************************
 *
 */

char *reg_str_ER(char *buff, int operand)
{
	if((operand & 0xff0) == 0xee0) {
		snprintf(buff, OP_BUF_SIZ, "r%d", operand & 0x00f);
	} else if((operand & 0xf00) == 0xe00) {
		snprintf(buff, OP_BUF_SIZ, ".R(%%%02X)", 
		          operand & 0x0ff);
	} else {
		snprintf(buff, OP_BUF_SIZ, "%%%03X", operand);
	}

	return buff;
}

char *reg_str_R(char *buff, int operand)
{
	if((operand & 0xf0) == 0xe0) {
		snprintf(buff, OP_BUF_SIZ, "r%d", operand & 0x0f);
	} else {
		snprintf(buff, OP_BUF_SIZ, "%%%02X", operand);
	}

	return buff;
}

char *reg_str_RR(char *buff, int operand)
{
	if((operand & 0xf0) == 0xe0) {
		snprintf(buff, OP_BUF_SIZ, "rr%d", operand & 0x0e);
	} else {
		snprintf(buff, OP_BUF_SIZ, ".RR(%%%02X)", operand & 0xfe);
	}

	return buff;
}

/****************************************************************
 * This will convert an operand into its mnemonic and save it
 * in *buff.
 */

static int append_operands(char *buff, size_t size, uint8_t *op, 
                           enum address_mode_t am, uint16_t pc)
{
	char *s;
	size_t r_size;
	int len;
	int op1;
	int op2;
	int opX;
	char op1_buf[OP_BUF_SIZ];
	char op2_buf[OP_BUF_SIZ];

	len = strlen(buff);
	r_size = size - len;
	s = buff + len;

	switch(am) {
	case am_ER2_ER1:
		op1 = ((op[2] & 0x0f) << 8) | op[3];
		op2 = (op[1] << 4) | ((op[2] & 0xf0) >> 4);
		snprintf(s, r_size, "%s,%s", 
		          reg_str_ER(op1_buf, op1), reg_str_ER(op2_buf, op2));
		break;
	case am_IM_ER1:
		op1 = ((op[2] & 0x0f) << 8) | op[3];
		op2 = op[1];
		snprintf(s, r_size, "%s,#%%%02X", 
		          reg_str_ER(op1_buf, op1), op2);
		break;
	case am_R2_R1:
		op1 = op[2];
		op2 = op[1];
		snprintf(s, r_size, "%s,%s", 
		          reg_str_R(op1_buf, op1), reg_str_R(op2_buf, op2));
		break;
	case am_IR2_R1:
		op1 = op[2];
		op2 = op[1];
		snprintf(s, r_size, "%s,@%s", 
		          reg_str_R(op1_buf, op1), reg_str_R(op2_buf, op2));
		break;
	case am_R2_IR1:
		op1 = op[2];
		op2 = op[1];
		snprintf(s, r_size, "@%s,%s", 
		          reg_str_R(op1_buf, op1), reg_str_R(op2_buf, op2));
		break;

	case am_R1_IM:
		op1 = op[1];
		op2 = op[2];
		snprintf(s, r_size, "%s,#%%%02X", 
		          reg_str_R(op1_buf, op1), op2);
		break;
	case am_IR1_IM:
		op1 = op[1];
		op2 = op[2];
		snprintf(s, r_size, "@%s,#%%%02X", 
		          reg_str_R(op1_buf, op1), op2);
		break;
	case am_IRR2_R1:
		op1 = op[2];
		op2 = op[1] & 0xfe;
		snprintf(s, r_size, "%s,@%s", 
		          reg_str_R(op1_buf, op1), reg_str_RR(op2_buf, op2));
		break;
	case am_R2_IRR1:
		op1 = op[2] & 0xfe;
		op2 = op[1];
		snprintf(s, r_size, "@%s,%s",
			  reg_str_RR(op1_buf, op1), reg_str_R(op2_buf, op2));
		break;
	case am_IRR2_IR1:
		op1 = op[2];
		op2 = op[1] & 0xfe;
		snprintf(s, r_size, "@%s,@%s", 
		          reg_str_R(op1_buf, op1), reg_str_RR(op2_buf, op2));
		break;
	case am_IR2_IRR1:
		op1 = op[2] & 0xfe;
		op2 = op[1];
		snprintf(s, r_size, "@%s,@%s",
		          reg_str_RR(op1_buf, op1), reg_str_R(op2_buf, op2));
		break;
	case am_DA:
		op1 = (op[1] << 8) | op[2];
		snprintf(s, r_size, "%%%04X", op1);
		break;
	case am_r1_r2_X:
		op1 = (op[1] & 0xf0) >> 4;
		op2 = op[1] & 0x0f;
		opX = op[2];
		snprintf(s, r_size, "r%d,%+d(r%d)", op1, (int8_t)opX, op2);
		break;
	case am_r2_r1_X:
		op1 = op[1] & 0x0f;
		op2 = (op[1] & 0xf0) >> 4;
		opX = op[2];
		snprintf(s, r_size, "%+d(r%d),r%d", (int8_t)opX, op1, op2);
		break;
	case am_r1_rr2_X:
		op1 = (op[1] & 0xf0) >> 4;
		op2 = op[1] & 0x0e;
		opX = op[2];
		snprintf(s, r_size, "r%d,%+d(rr%d)", op1, (int8_t)opX, op2);
		break;
	case am_rr1_r2_X:
		op1 = (op[1] & 0xe0) >> 4;
		op2 = op[1] & 0x0f;
		opX = op[2];
		snprintf(s, r_size, "%+d(rr%d),r%d", (int8_t)opX, op1, op2);
		break;
	case am_rr1_rr2_X:
		op1 = (op[1] & 0xf0) >> 4;
		op2 = op[1] & 0x0f;
		opX = op[2];
		snprintf(s, r_size, "rr%d,%+d(rr%d)", op1, (int8_t)opX, op2);
		break;
	case am_cc_DA:
		op1 = (op[1] << 8) | op[2];
		op2 = (op[0] & 0xf0) >> 4;
		if(op2 == 0x08) {
			snprintf(s, r_size, "%%%04X", op1);
		} else {
			snprintf(s, r_size, "%s,%%%04X",
			          cc_mnem[op2], op1);
		}
		break;
	case am_r2_ER1:
		op1 = ((op[1] & 0x0f) << 8) | op[2];
		op2 = (op[1] & 0xf0) >> 4;
		snprintf(s, r_size, "%s,r%d", 
		          reg_str_ER(op1_buf, op1), op2);
		break;
	case am_r1_ER2:
		op1 = (op[1] & 0xf0) >> 4;
		op2 = ((op[1] & 0x0f) << 8) | op[2];
		snprintf(s, r_size, "r%d,%s", 
		          op1, reg_str_ER(op2_buf, op2));
		break;
	case am_Ir2_ER1:
		op1 = ((op[1] & 0x0f) << 8) | op[2];
		op2 = (op[1] & 0xf0) >> 4;
		snprintf(s, r_size, "%s,@r%d", 
		          reg_str_ER(op1_buf, op1), op2);
		break;
	case am_Ir1_ER2:
		op1 = (op[1] & 0xf0) >> 4;
		op2 = ((op[1] & 0x0f) << 8) | op[2];
		snprintf(s, r_size, "@r%d,%s", 
		          op1, reg_str_ER(op2_buf, op2));
		break;
	case am_IM:
		op1 = op[1];
		snprintf(s, r_size, "#%%%02X", op1);
		break;
	case am_R1:
		op1 = op[1];
		snprintf(s, r_size, "%s", reg_str_R(op1_buf, op1));
		break;
	case am_RR1:
		op1 = op[1] & 0xfe;
		snprintf(s, r_size, "%s", reg_str_RR(op1_buf, op1));
		break;
	case am_IR1:
		op1 = op[1];
		snprintf(s, r_size, "@%s", reg_str_R(op1_buf, op1));
		break;
	case am_IRR1:
		op1 = op[1] & 0xfe;
		snprintf(s, r_size, "@%s", reg_str_RR(op1_buf, op1));
		break;
	case am_r1_r2:
		op1 = (op[1] & 0xf0) >> 4;
		op2 = op[1] & 0x0f;
		snprintf(s, r_size, "r%d,r%d", op1, op2);
		break;
	case am_r1_Ir2:
		op1 = (op[1] & 0xf0) >> 4;
		op2 = op[1] & 0x0f;
		snprintf(s, r_size, "r%d,@r%d", op1, op2);
		break;
	case am_Ir1_r2:
		op1 = (op[1] & 0xf0) >> 4;
		op2 = op[1] & 0x0f;
		snprintf(s, r_size, "@r%d,r%d", op1, op2);
		break;
	case am_r1_Irr2:
		op1 = (op[1] & 0xf0) >> 4;
		op2 = op[1] & 0x0e;
		snprintf(s, r_size, "r%d,@rr%d", op1, op2);
		break;
	case am_r2_Irr1:
		op1 = op[1] & 0x0e;
		op2 = (op[1] & 0xf0) >> 4;
		snprintf(s, r_size, "@rr%d,r%d", op1, op2);
		break;
	case am_Ir1_Irr2:
		op1 = (op[1] & 0xf0) >> 4;
		op2 = op[1] & 0x0e;
		snprintf(s, r_size, "@r%d,@rr%d", op1, op2);
		break;
	case am_Ir2_Irr1:
		op1 = op[1] & 0x0e;
		op2 = (op[1] & 0xf0) >> 4;
		snprintf(s, r_size, "@rr%d,@r%d", op1, op2);
		break;
	case am_r1_RA:
		op1 = (op[0] & 0xf0) >> 4;
		op2 = op[1];
		snprintf(s, r_size, "r%d,%%%04X", op1, pc + (int8_t)op2);
		break;
	case am_cc_RA:
		op1 = op[1];
		op2 = (op[0] & 0xf0) >> 4;
		if(op2 == 0x08) {
			snprintf(s, r_size, "%%%04X", pc + (int8_t)op1);
		} else {
			snprintf(s, r_size, "%s,%%%04X",
			          cc_mnem[op2], pc + (int8_t)op1);
		}
		break;
	case am_r1_IM:
		op1 = (op[0] & 0xf0) >> 4;
		op2 = op[1];
		snprintf(s, r_size, "r%d,#%%%02X", op1, op2);
		break;
	case am_p_bit_r1:
		op1 = (op[1] & 0x70) >> 4;
		op2 = op[1] & 0x0f;
		if(op[1] & 0x80) {
			snprintf(buff, size, "bset  %d,r%d", op1, op2);
		} else {
			snprintf(buff, size, "bclr  %d,r%d", op1, op2);
		}
		break;
	case am_p_bit_r1_RA:
		op1 = (op[1] & 0x70) >> 4;
		op2 = op[1] & 0x0f;
		if(op[1] & 0x80) {
			snprintf(buff, size, "btjnz %d,r%d,%%%04X", 
			          op1, op2, pc + (int8_t)op[2]);
		} else {
			snprintf(buff, size, "btjz  %d,r%d,%%%04X", 
			          op1, op2, pc + (int8_t)op[2]);
		}
		break;
	case am_p_bit_Ir1_RA:
		op1 = (op[1] & 0x70) >> 4;
		op2 = op[1] & 0x0f;
		if(op[1] & 0x80) {
			snprintf(buff, size, "btjnz %d,@r%d,%%%04X", 
			          op1, op2, pc+(uint8_t)op[2]);
		} else {
			snprintf(buff, size, "btjz  %d,@r%d,%%%04X", 
			          op1, op2, pc+(uint8_t)op[2]);
		}
		break;
	case am_vec:
		op1 = op[1];
		snprintf(s, r_size, "#%%%02X", op1);
		break;
	case am_r1:
		op1 = (op[0] & 0xf0) >> 4;
		snprintf(s, r_size, "r%d", op1);
		break;
	case am_ER1:
		op1 = (op[1] << 4) | ((op[2] & 0xf0) >> 4);
		snprintf(s, r_size, "%s", reg_str_ER(op1_buf, op1));
		break;
	case am_none:
		break;
	default:
		fprintf(stderr, "Internal Error\n");
		abort();
		break;
	}

	return 0;
}

/****************************************************************
 * This is the disassembler.
 *
 * If *buff is not null, it will decode the mnemonic and save
 * it in *buff.
 *
 * This function will return the size of the instruction.
 */

int disassemble(char *buff, size_t size, uint8_t *op, uint16_t pc)
{
	int op_size;
	const struct opcode_t *op_ptr;

	if(op[0] == ALT_OPCODE) {
		op_ptr = find_alt_opcode(op[1]);
		op = &op[1];
		op_size = 1;
	} else {
		op_ptr = find_opcode(op[0]);
		op_size = 0;
	}

	if(op_ptr != NULL) {
		op_size += opcode_size(op_ptr);
	} else {
		op_size = 1;
	}

	if(buff == NULL) {
		return size;
	}

	pc += op_size;
	memset(buff, 0, size);

	if(op_ptr) {
		snprintf(buff, size-1, "%-6s", op_ptr->mnemonic);
		append_operands(buff, size, op, op_ptr->am, pc);
	} else {
		strncpy(buff, "ill", size-1);
	}

	return op_size;
}

/****************************************************************/


