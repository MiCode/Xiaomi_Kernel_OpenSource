/* Copyright (c) 2009, Code Aurora Forum. All rights reserved.
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

/*
 * JTAG support functions for ARMv7-based Qualcomm SoCs.
 */
#include <linux/linkage.h>
#include <asm/assembler.h>
#include <linux/threads.h>

#if (NR_CPUS > 2)
#error code only tested for 1 or 2 CPUs.
#endif

/* Add 1 slot to store the register count for JTAG state */
#define MAX_JTAG_REGS (32 + 1)
#define MAX_DEBUG_STATE_SIZE (MAX_JTAG_REGS * 4)

ENTRY(msm_save_jtag_debug)
	ldr     r3, =dbg_state        /* store state at dbg_state */
#if (NR_CPUS >= 2)
	mrc	p15, 0, r2, c0, c0, 5 /* MPIDR */
	and	r2, r2, #15	      /* What CPU am I */
	ldr     r1, =MAX_DEBUG_STATE_SIZE
	mul     r2, r2, r1
	add	r3, r3, r2
#endif

	/* save jtag state */
	ldr     r1, =0xc5ACCE55       /* set DBGOSLAR lock */
	mcr     p14, 0, r1, c1, c0, 4
	isb

	mrc     p14, 0, r1, c1, c2, 4 /* DBGOSSRR state register count */
	cmp     r1, #(MAX_JTAG_REGS)  /* check for state overflow */
	movgt   r1, #0                /* if not enough space, don't save */
	str     r1, [r3], #4          /* save count for restore */

1:	cmp     r1, #0
	mrcne   p14, 0, r2, c1, c2, 4 /* DBGOSSRR state value */
	strne   r2, [r3], #4          /* push value */
	subne   r1, r1, #1
	bne     1b

	mcr     p14, 0, r1, c1, c0, 4 /* unlock DBGOSLAR */
	isb

	bx      lr

ENTRY(msm_restore_jtag_debug)
	/* restore debug registers after power collapse  */
	ldr     r3, =dbg_state        /* load state from dbg_state */
#if (NR_CPUS >= 2)
	mrc	p15, 0, r2, c0, c0, 5 /* MPIDR */
	and	r2, r2, #15	      /* What CPU am I */
	ldr     r1, =MAX_DEBUG_STATE_SIZE
	mul     r2, r2, r1
	add	r3, r3, r2
#endif

	/* restore jtag state */
	mrc     p14, 0, r1, c1, c5, 4 /* clear sticky power down bit */
	isb
	ldr     r1, =0xc5ACCE55       /* set DBGOSLAR lock */
	mcr     p14, 0, r1, c1, c0, 4
	isb

	mrc     p14, 0, r1, c1, c2, 4 /* DBGOSSRR dummy read (required)*/
	ldr     r1, [r3], #4          /* load saved count */
	cmp     r1, #0                /* skip if none stored */
	beq     msm_pm_dbg_restore_end

	/* restores debug state except DBGDSCR */
1:	ldr     r2, [r3], #4
	cmp     r1, #0x10             /* DBGDSCR special case */
	biceq   r2, r2, #0xc000       /* DBGDSCR = DBGDSCR & ~0xc000 */
	mcr     p14, 0, r2, c1, c2, 4 /* DBGOSSRR write state value */
	subs    r1, r1, #1
	bne     1b

	ldr     r3, =dbg_state        /* load state from dbg_state */
#if (NR_CPUS >= 2)
	mrc	p15, 0, r2, c0, c0, 5 /* MPIDR */
	and	r2, r2, #15	      /* What CPU am I */
	ldr     r1, =MAX_DEBUG_STATE_SIZE
	mul     r2, r2, r1
	add	r3, r3, r2
#endif
	ldr     r1, =0xc5ACCE55       /* set DBGOSLAR lock */
	mcr     p14, 0, r1, c1, c0, 4
	isb

	mrc     p14, 0, r1, c1, c2, 4 /* DBGOSSRR dummy read (required)*/
	ldr     r1, [r3], #4          /* load saved count */
	cmp     r1, #0                /* skip if none stored */
	beq     msm_pm_dbg_restore_end

	/* second pass to restore debug state including DBGDSCR */
1:	ldr     r2, [r3], #4
	mcr     p14, 0, r2, c1, c2, 4 /* DBGOSSRR write state value */
	subs    r1,r1,#1
	bne     1b
msm_pm_dbg_restore_end:
	mcr     p14, 0, r1, c1, c0, 4 /* unlock DBGOSLAR */
	isb

	bx      lr


	.data

dbg_state:
	.space	MAX_DEBUG_STATE_SIZE * NR_CPUS
