/*
 * Idle processing for ARMv7-based Qualcomm SoCs.
 *
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2007-2009, 2011-2013 The Linux Foundation. All rights reserved.
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
	and     r0, r0, #15              /* what CPU am I                  */

	adr	r3, 3f
	ldr	r1, [r3]
	sub	r3, r1, r3
	ldr	r1, =msm_pc_debug_counters_phys /*phys addr for IMEM reg */
	sub	r1, r1, r3			/* translate virt to phys */
	ldr	r1,[r1]

	cmp	r1, #0
	beq	skip_pc_debug3
	add	r1, r1, r0, LSL #4	/* debug location for this CPU */
	add	r1, #4			/* warmboot entry counter*/
	ldr	r2, [r1]
	add	r2, #1
	str	r2, [r1]

skip_pc_debug3:
	ldr     r1, =msm_pm_boot_vector
	sub	r1, r1, r3		/* translate virt to phys */

	add     r1, r1, r0, LSL #2       /* locate boot vector for our cpu */
	ldr     pc, [r1]                 /* jump                           */
ENDPROC(msm_pm_boot_entry)

3:	.long	.

	.data

	.globl msm_pm_boot_vector
msm_pm_boot_vector:
	.space  4 * NR_CPUS

	.globl msm_pc_debug_counters_phys
msm_pc_debug_counters_phys:
	.long 0x0
