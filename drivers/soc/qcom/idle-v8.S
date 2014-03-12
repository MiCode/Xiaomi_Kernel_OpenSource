/*
 * Idle processing for ARMv8-based Qualcomm SoCs.
 *
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2007-2009, 2011-2014 The Linux Foundation. All rights reserved.
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

ENTRY(msm_pm_boot_entry)
	mrs	x0, mpidr_el1
	and     x0, x0, #15              /* what CPU am I */

	adr	x3, align
	ldr	x1, [x3]
	sub	x3, x1, x3
	ldr	x1, =msm_pc_debug_counters_phys /*phys addr for IMEM reg */
	sub	x1, x1, x3			/* translate virt to phys */
	ldr	x1,[x1]

	cmp	x1, #0
	beq	skip_pc_debug3
	add	x1, x1, x0, LSL #4	/* debug location for this CPU */
	add	x1, x1, #4		/* warmboot entry counter*/
	ldr	x2, [x1]
	add	x2, x2, #1
	str	x2, [x1]

skip_pc_debug3:
	ldr     x1, =msm_pm_boot_vector
	sub	x1, x1, x3		/* translate virt to phys */

	add     x1, x1, x0, LSL #3      /* locate boot vector for our cpu */
	ldr x1, [x1]
	ret	x1                 /* jump                           */
ENDPROC(msm_pm_boot_entry)

	__ALIGN
align:  .quad .
	.data
	.globl msm_pm_boot_vector
msm_pm_boot_vector:
	.space  8 * NR_CPUS

	.globl msm_pc_debug_counters_phys
msm_pc_debug_counters_phys:
	.long 0x0
