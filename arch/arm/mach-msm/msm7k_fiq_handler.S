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

#include <linux/linkage.h>
#include <asm/assembler.h>

#define VERSION_ID 0x1
#define MAGIC 0xDEAD0000 | VERSION_ID
	.text
	.align 3

ENTRY(msm7k_fiq_start)
	sub    r14, r14, #4 @return address
	ldr    r8, Lmsm_fiq_stack
	ldr    sp, [r8]       @get stack
	stmfa  sp!, {r0-r7, lr}
	stmfa  sp!, {r8-r9}
	ldr    r8, Ldump_cpu_ctx
	@ store magic to indicate a valid dump
	ldr    r9, Lmagic
	str    r9, [r8], #4
	@ get the current cpsr
	mrs    r9, cpsr
	str    r9, [r8],#4
	stmia  r8!, {r0-r7}   @ get the USR r0-r7
	mov    r4, r8
	mov    r5, #PSR_I_BIT | PSR_F_BIT | SYSTEM_MODE
	msr    cpsr_c, r5     @ select SYSTEM mode
	stmia  r4!, {r8-r14}
	mov    r5, #PSR_I_BIT | PSR_F_BIT | IRQ_MODE
	msr    cpsr_c, r5     @ select IRQ mode
	mrs    r5, spsr
	str    r5, [r4], #4
	stmia  r4!, {r13-r14}
	mov    r5, #PSR_I_BIT | PSR_F_BIT | SVC_MODE
	msr    cpsr_c, r5     @ select SVC mode
	mrs    r5, spsr
	str    r5, [r4], #4
	stmia  r4!, {r13-r14}
	mov    r0, r13
	mov    r1, r14
	mov    r5, #PSR_I_BIT | PSR_F_BIT | FIQ_MODE
	msr    cpsr_c, r5     @ select FIQ mode
	stmfa  sp!, {r0-r1}
	mov    r5, #PSR_I_BIT | PSR_F_BIT | ABT_MODE
	msr    cpsr_c, r5     @ select ABT mode
	mrs    r5, spsr
	str    r5, [r4], #4
	stmia  r4!, {r13-r14}
	mov    r5, #PSR_I_BIT | PSR_F_BIT | UND_MODE
	msr    cpsr_c, r5     @ select UND mode
	mrs    r5, spsr
	str    r5, [r4], #4
	stmia  r4!, {r13-r14}
	mov    r5, #PSR_I_BIT | PSR_F_BIT | FIQ_MODE
	msr    cpsr_c, r5     @ select FIQ mode
	mrs    r5, spsr
	str    r5, [r4], #4
	stmia  r4!, {r8-r14}
	dsb
	mov    r5, #PSR_I_BIT | PSR_F_BIT | SVC_MODE
	msr    cpsr_c, r5     @ select SVC mode
	ldr    r2, Lmsm_fiq_handler
	blx    r2
	mov    r5, #PSR_I_BIT | PSR_F_BIT | FIQ_MODE
	msr    cpsr_c, r5     @ select FIQ mode
	ldmfa sp!, {r0, r1}
	mov    r5, #PSR_I_BIT | PSR_F_BIT | SVC_MODE
	msr    cpsr_c, r5     @ select SVC mode
	mov    r13, r0
	mov    r14, r1
	mov    r5, #PSR_I_BIT | PSR_F_BIT | FIQ_MODE
	msr    cpsr_c, r5     @ select SVC mode
	ldmfa sp!, {r8-r9}
	ldmfa sp!, {r0-r7, pc}^
Ldump_cpu_ctx:
	.word  msm_dump_cpu_ctx
Lmsm_fiq_stack:
	.word  msm7k_fiq_stack
Lmagic:
	.word  MAGIC
Lmsm_fiq_handler:
	.word  msm7k_fiq_handler
ENTRY(msm7k_fiq_length)
	.word  . - msm7k_fiq_start
