/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef MSM_MERCURY_MACROS_H
#define MSM_MERCURY_MACROS_H

#include <media/msm_mercury.h>

#define mercury_kread(reg) \
	hw_cmd.type = MSM_MERCURY_HW_CMD_TYPE_READ; \
	hw_cmd.n = 1; \
	hw_cmd.offset = HWIO_##reg##_ADDR; \
	hw_cmd.mask = HWIO_##reg##__RMSK; \
	hw_cmd.data = 0x0; \
	msm_mercury_hw_exec_cmds(&hw_cmd, 1);

#define mercury_kwrite(reg, val) \
	hw_cmd.offset = HWIO_##reg##_ADDR; \
	hw_cmd.mask = HWIO_##reg##__RMSK; \
	hw_cmd.type = MSM_MERCURY_HW_CMD_TYPE_WRITE; \
	hw_cmd.n = 1; \
	hw_cmd.data = val; \
	msm_mercury_hw_exec_cmds(&hw_cmd, 1);

#define GET_FVAL(val, reg, field) ((val & HWIO_FMSK(reg, field)) >> \
	HWIO_SHFT(reg, field))

#define byte unsigned char
#define word unsigned short
#define dword unsigned long

#define inp(port)    (*((dword *) (port)))
#define inpb(port)     (*((byte *) (port)))
#define inpw(port)   (*((word *) (port)))
#define inpdw(port)   (*((dword *)(port)))

#define outp(port, val)   (*((dword *) (port)) = ((dword) (val)))
#define outpb(port, val)   (*((byte *) (port)) = ((byte) (val)))
#define outpw(port, val)  (*((word *) (port)) = ((word) (val)))
#define outpdw(port, val) (*((dword *) (port)) = ((dword) (val)))


#define in_byte(addr)				(inp(addr))
#define in_byte_masked(addr, mask)	(inp(addr) & (byte)mask)
#define out_byte(addr, val)			 outp(addr, val)
#define in_word(addr)				(inpw(addr))
#define in_word_masked(addr, mask)	(inpw(addr) & (word)mask)
#define out_word(addr, val)			 outpw(addr, val)
#define in_dword(addr)				(inpdw(addr))
#define in_dword_masked(addr, mask)	(inpdw(addr) & mask)
#define out_dword(addr, val)			 outpdw(addr, val)

/* shadowed, masked output for write-only registers */
#define out_byte_masked(io, mask, val, shadow)  \
	do { \
		shadow = (shadow & (word)(~(mask))) | \
		((word)((val) & (mask))); \
		(void) out_byte(io, shadow);\
	} while (0);

#define out_word_masked(io, mask, val, shadow)  \
	do { \
		shadow = (shadow & (word)(~(mask))) | \
		((word)((val) & (mask))); \
		(void) out_word(io, shadow); \
	} while (0);

#define out_dword_masked(io, mask, val, shadow)  \
	do { \
		shadow = (shadow & (dword)(~(mask))) | \
		((dword)((val) & (mask))); \
		(void) out_dword(io, shadow);\
	} while (0);

#define out_byte_masked_ns(io, mask, val, current_reg_content)  \
	(void) out_byte(io, ((current_reg_content & \
	(word)(~(mask))) | ((word)((val) & (mask)))))

#define out_word_masked_ns(io, mask, val, current_reg_content)  \
	(void) out_word(io, ((current_reg_content & \
	(word)(~(mask))) | ((word)((val) & (mask)))))

#define out_dword_masked_ns(io, mask, val, current_reg_content) \
	(void) out_dword(io, ((current_reg_content & \
	(dword)(~(mask))) | ((dword)((val) & (mask)))))

#define MEM_INF(val, reg, field)	((val & HWIO_FMSK(reg, field)) >> \
	HWIO_SHFT(reg, field))

#define MEM_OUTF(mem, reg, field, val)\
	out_dword_masked_ns(mem, HWIO_FMSK(reg, field), \
	(unsigned  int)val<<HWIO_SHFT(reg, field), in_dword(mem))

#define MEM_OUTF2(mem, reg, field2, field1, val2, val1)  \
	out_dword_masked_ns(mem, (HWIO_FMSK(reg, field2)| \
	HWIO_FMSK(reg, field1)), \
	(((unsigned int)val2<<HWIO_SHFT(reg, field2))| \
	((unsigned int)val1<<HWIO_SHFT(reg, field1))), in_dword(mem))

#define MEM_OUTF3(mem, reg, fld3, fld2, fld1, val3, val2, val1)  \
	out_dword_masked_ns(mem, (HWIO_FMSK(reg, fld3)| \
	HWIO_FMSK(reg, fld2)|HWIO_FMSK(reg, fld1)), \
	(((unsigned int)val3<<HWIO_SHFT(reg, fld3))| \
	((unsigned int)val2<<HWIO_SHFT(reg, fld2))| \
	((unsigned int)val1<<HWIO_SHFT(reg, fld1))), in_dword(mem))

#define HWIO_FMSK(reg, field)   HWIO_##reg##__##field##__BMSK
#define HWIO_SHFT(reg, field)   HWIO_##reg##__##field##__SHFT
#endif
