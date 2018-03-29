/*
 * Macros for accessing system registers with older binutils.
 *
 * Copyright (C) 2014 ARM Ltd.
 * Copyright (C) 2018 XiaoMi, Inc.
 * Author: Catalin Marinas <catalin.marinas@arm.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __ASM_SYSREG_H
#define __ASM_SYSREG_H

#include <linux/stringify.h>

#define sys_reg(op0, op1, crn, crm, op2) \
	((((op0)-2)<<19)|((op1)<<16)|((crn)<<12)|((crm)<<8)|((op2)<<5))

#ifdef __ASSEMBLY__

	.irp	num,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30
	.equ	__reg_num_x\num, \num
	.endr
	.equ	__reg_num_xzr, 31

	.macro	mrs_s, rt, sreg
	.inst	0xd5300000|(\sreg)|(__reg_num_\rt)
	.endm

	.macro	msr_s, sreg, rt
	.inst	0xd5100000|(\sreg)|(__reg_num_\rt)
	.endm

#else

#include <linux/types.h>

asm(
"	.irp	num,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30\n"
"	.equ	__reg_num_x\\num, \\num\n"
"	.endr\n"
"	.equ	__reg_num_xzr, 31\n"
"\n"
"	.macro	mrs_s, rt, sreg\n"
"	.inst	0xd5300000|(\\sreg)|(__reg_num_\\rt)\n"
"	.endm\n"
"\n"
"	.macro	msr_s, sreg, rt\n"
"	.inst	0xd5100000|(\\sreg)|(__reg_num_\\rt)\n"
"	.endm\n"
);

/*
 * Unlike read_cpuid, calls to read_sysreg are never expected to be
 * optimized away or replaced with synthetic values.
 */
#define read_sysreg(r) ({                                      \
	u64 __val;                                              \
	asm volatile("mrs %0, " __stringify(r) : "=r" (__val)); \
	__val;                                                  \
})

#define write_sysreg(v, r) do {                                        \
	u64 __val = (u64)v;                                     \
	asm volatile("msr " __stringify(r) ", %0"               \
		: : "r" (__val));                          \
} while (0)


#endif

#endif	/* __ASM_SYSREG_H */
