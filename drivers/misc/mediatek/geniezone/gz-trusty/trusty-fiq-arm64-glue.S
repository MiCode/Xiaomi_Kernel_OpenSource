/*
 * Copyright (C) 2013 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <asm/asm-offsets.h>
#include <asm/ptrace.h>
#include <linux/linkage.h>
#include <gz-trusty/smcall.h>

.macro	push	reg1,reg2,remregs:vararg
	.ifnb \remregs
	push \remregs
	.endif
	stp	\reg1, \reg2, [sp, #-16]!
.endm

.macro	pop	reg1,reg2,remregs:vararg
	ldp	\reg1, \reg2, [sp], #16
	.ifnb \remregs
	pop \remregs
	.endif
.endm

ENTRY(trusty_fiq_glue_arm64)
	sub	sp, sp, #S_FRAME_SIZE - S_LR
	push	x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, \
		x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, \
		x26, x27, x28, x29
	ldr	x0, =SMC_FC64_GZ_GET_FIQ_REGS
	smc	#0
	stp	x0, x1, [sp, #S_PC] /* original pc, cpsr */
	tst	x1, PSR_MODE_MASK
	csel	x2, x2, x3, eq		/* sp el0, sp el1 */
	stp	x30, x2, [sp, #S_LR]	/* lr, original sp */
	mov	x0, sp
	mov	x1, x3
	bl	trusty_fiq_handler
	pop	x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, \
		x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, \
		x26, x27, x28, x29
	ldr	x30, [sp], #S_FRAME_SIZE - S_LR	/* load LR and restore SP */
	ldr	x0, =SMC_FC_GZ_FIQ_EXIT
	smc	#0
	b	.	/* should not get here */

ENTRY(trusty_fiq_cpu_resume)
	ldr	x0, =SMC_FC_GZ_FIQ_RESUME
	smc	#0
	ret
