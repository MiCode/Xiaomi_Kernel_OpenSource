/*
 * Idle processing for ARMv7-based Qualcomm Technologies, Inc. SoCs.
 *
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2007-2009, 2011-2014, 2019 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/linkage.h>
#include <linux/threads.h>
#include <asm/assembler.h>

	.arm
ENTRY(msm_pm_boot_entry)
THUMB(	adr	r9, BSYM(2f)	)	/* Kernel is always entered in ARM. */
THUMB(	bx	r9		)	/* If this is a Thumb-2 kernel, */
THUMB(	.thumb			)	/* switch to Thumb now.		*/
THUMB(2:			)
	mrc     p15, 0, r0, c0, c0, 5    /* MPIDR                          */
	bic     r0, #0xff000000         /* what CPU am I */
	bic     r0, #0x00ff0000         /* clear bits 31-16 */

	adr	r3, 3f
	ldr	r1, [r3]
	sub	r3, r1, r3		/* r3 holds the virt to phy offset */
	ldr	r1, =msm_pc_debug_counters_phys /* phys addr for IMEM reg */
	sub	r1, r1, r3			/* translate virt to phys */
	ldr	r1,[r1]
	cmp	r1, #0
	beq	skip_pc_debug3
	mov     r2, r0, lsr #8		/* get cluster num */
	add 	r1, r1, r2, lsl #6	/* get offset of cluster */
	and	r2, r0, #0xff
	add	r1, r1, r2, lsl #4	/* get offset for the cpu */
	add	r1, #4			/* warmboot entry counter */
	ldr	r2, [r1]
	add	r2, #1
	str	r2, [r1]
skip_pc_debug3:
	ldr     r1, =msm_pm_boot_vector
	sub	r1, r1, r3		/* translate virt to phys */
	mov     r2, r0, lsr #8		/* get cluster num */
	add	r1, r1, r2, lsl #4	/* Get offset for the cluster */
	and	r0, r0, #0xff		/* cpu id */
	add	r1, r1, r0, lsl #2	/* Get offset for the cpu */
	ldr     pc, [r1]                 /* jump                    */
ENDPROC(msm_pm_boot_entry)

3:	.long	.

	.data

	.globl msm_pm_boot_vector
msm_pm_boot_vector:
	.space  4 * 4 * 4

	.globl msm_pc_debug_counters_phys
msm_pc_debug_counters_phys:
	.long 0x0
