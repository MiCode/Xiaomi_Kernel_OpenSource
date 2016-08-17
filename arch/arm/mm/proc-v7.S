/*
 *  linux/arch/arm/mm/proc-v7.S
 *
 *  Copyright (C) 2001 Deep Blue Solutions Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  This is the "shell" of the ARMv7 processor support.
 */
#include <linux/init.h>
#include <linux/linkage.h>
#include <asm/assembler.h>
#include <asm/asm-offsets.h>
#include <asm/hwcap.h>
#include <asm/pgtable-hwdef.h>
#include <asm/pgtable.h>

#include "proc-macros.S"

#ifdef CONFIG_ARM_LPAE
#include "proc-v7-3level.S"
#else
#include "proc-v7-2level.S"
#endif

#define TEGRA_CLK_RESET_BOND_OUT 0x60006070

ENTRY(cpu_v7_proc_init)
	mov	pc, lr
ENDPROC(cpu_v7_proc_init)

ENTRY(cpu_v7_proc_fin)
	mrc	p15, 0, r0, c1, c0, 0		@ ctrl register
	bic	r0, r0, #0x1000			@ ...i............
	bic	r0, r0, #0x0006			@ .............ca.
	mcr	p15, 0, r0, c1, c0, 0		@ disable caches
	mov	pc, lr
ENDPROC(cpu_v7_proc_fin)

/*
 *	cpu_v7_reset(loc)
 *
 *	Perform a soft reset of the system.  Put the CPU into the
 *	same state as it would be if it had been reset, and branch
 *	to what would be the reset vector.
 *
 *	- loc   - location to jump to for soft reset
 *
 *	This code must be executed using a flat identity mapping with
 *      caches disabled.
 */
	.align	5
	.pushsection	.idmap.text, "ax"
ENTRY(cpu_v7_reset)
	mrc	p15, 0, r1, c1, c0, 0		@ ctrl register
	bic	r1, r1, #0x1			@ ...............m
 THUMB(	bic	r1, r1, #1 << 30 )		@ SCTLR.TE (Thumb exceptions)
	mcr	p15, 0, r1, c1, c0, 0		@ disable MMU
	isb
	mov	pc, r0
ENDPROC(cpu_v7_reset)
	.popsection

/*
 *	cpu_v7_do_idle()
 *
 *	Idle the processor (eg, wait for interrupt).
 *
 *	IRQs are already disabled.
 */
ENTRY(cpu_v7_do_idle)
	dsb					@ WFI may enter a low-power mode
	wfi
	mov	pc, lr
ENDPROC(cpu_v7_do_idle)

ENTRY(cpu_v7_dcache_clean_area)
#ifndef TLB_CAN_READ_FROM_L1_CACHE
	dcache_line_size r2, r3
1:	mcr	p15, 0, r0, c7, c10, 1		@ clean D entry
	add	r0, r0, r2
	subs	r1, r1, r2
	bhi	1b
	dsb
#endif
	mov	pc, lr
ENDPROC(cpu_v7_dcache_clean_area)

	string	cpu_v7_name, "ARMv7 Processor"
	.align

/* Suspend/resume support: derived from arch/arm/mach-s5pv210/sleep.S */
.local	cpu_v7_debug_suspend_size
#ifdef CONFIG_ARM_SAVE_DEBUG_CONTEXT
/*
 * Debug context:
 *	8 CP14 registers
 *	16x2 CP14 breakpoint registers (maximum)
 *	16x2 CP14 watchpoint registers (maximum)
 */
.equ	cpu_v7_debug_suspend_size, (4 * (8 + (16 * 2) + (16 * 2)))

.macro  save_brkpt cm
	mrc	p14, 0, r4, c0, \cm, 4
	mrc	p14, 0, r5, c0, \cm, 5
	stmia	r0!, {r4 - r5}
.endm

.macro  restore_brkpt cm
	ldmia	r0!, {r4 - r5}
	mcr	p14, 0, r4, c0, \cm, 4
	mcr	p14, 0, r5, c0, \cm, 5
.endm

.macro  save_wpt cm
	mrc	p14, 0, r4, c0, \cm, 6
	mrc	p14, 0, r5, c0, \cm, 7
	stmia	r0!, {r4 - r5}
.endm

.macro  restore_wpt cm
	ldmia	r0!, {r4 - r5}
	mcr	p14, 0, r4, c0, \cm, 6
	mcr	p14, 0, r5, c0, \cm, 7
.endm

#else
.equ	cpu_v7_debug_suspend_size, 0
#endif

.globl	cpu_v7_suspend_size
.equ	cpu_v7_suspend_size, (4 * 17) + cpu_v7_debug_suspend_size
#ifdef CONFIG_ARM_CPU_SUSPEND
ENTRY(cpu_v7_do_suspend)
	stmfd	sp!, {r3 - r10, lr}
	mrc	p15, 0, r4, c13, c0, 0	@ FCSE/PID
	mrc	p15, 0, r5, c13, c0, 3	@ User r/o thread ID
	mrc	p15, 0, r6, c15, c0, 1	@ diag
	stmia	r0!, {r4 - r6}

	mrc	p15, 0, r4, c9, c14, 2	@ PMINTENCLR
	mrc	p15, 0, r5, c9, c14, 0	@ PMUSEREN
	mrc	p15, 0, r6, c9, c12, 5	@ PMSELR, event counter selection
	mrc	p15, 0, r7, c9, c13, 2	@ PMXEVCNTR, event counter
	mrc	p15, 0, r8, c9, c13, 1	@ PMXEVTYPER or PMCCFILTR
	mrc	p15, 0, r9, c9, c13, 0	@ PMCCNTR, cycle counter
	mrc	p15, 0, r10, c9, c12, 0	@ PMCR, control register
	mrc	p15, 0, r11, c9, c12, 1	@ PMCNTENSET, counter enable set
	stmia	r0!, {r4 - r11}

	mrc	p15, 0, r6, c3, c0, 0	@ Domain ID
	mrc	p15, 0, r7, c2, c0, 1	@ TTB 1
	mrc	p15, 0, r11, c2, c0, 2	@ TTB control register
	mrc	p15, 0, r8, c1, c0, 0	@ Control register
	mrc	p15, 0, r9, c1, c0, 1	@ Auxiliary control register
	mrc	p15, 0, r10, c1, c0, 2	@ Co-processor access control
	stmia	r0!, {r6 - r11}

#ifdef CONFIG_ARM_SAVE_DEBUG_CONTEXT
	/* If Debug Architecture v7.1 or later, set OS lock. */
	mrc	p15, 0, r11, c0, c1, 2	@ ID_DFR0
	and	r11, r11, #0xF		@ coprocessor debug model
	cmp	r11, #5			@ debug arch >= v7.1?
#ifndef CONFIG_ARM_SAVE_DEBUG_CONTEXT_NO_LOCK
	ldrge	r4, =0xC5ACCE55		@ Lock value
	mcrge	p14, 0, r4, c1, c0, 4	@ DBGOSLAR
	isb
#endif
	/* Save CP14 debug controller context */

	mrc	p14, 0, r4, c0, c2, 2	@ DBGDSCRext
	mrc	p14, 0, r5, c0, c6, 0	@ DBGWFAR
	mrc	p14, 0, r6, c0, c7, 0	@ DBGVCR
	mrc	p14, 0, r7, c7, c9, 6	@ DBGCLAIMCLR
	stmia	r0!, {r4-r7}

	mrclt	p14, 0, r4, c0, c10, 0	@ DBGDSCCR (debug arch v7 only)
	mrclt	p14, 0, r5, c0, c11, 0	@ DBGDSMCR (debug arch v7 only)
	stmltia	r0!, {r4-r5}		@ (debug arch v7 only)

	tst	r4, #(1 << 29)		@ DBGDSCRext.TXfull
	mrcne	p14, 0, r4, c0, c3, 2	@ DBGDTRTXext
	strne	r4, [r0], #4

	tst	r4, #(1 << 30)		@ DBGDSCRext.RXfull
	mrcne	p14, 0, r4, c0, c0, 2	@ DBGDTRRXext
	strne	r4, [r0], #4

	mrc	p14, 0, r8, c0, c0, 0   @ read IDR
	mov	r3, r8, lsr #24
	and	r3, r3, #0xf            @ r3 has the number of brkpt
	rsb	r3, r3, #0xf

	/* r3 = (15 - #of brkpt) ;
	   switch offset = r3*12 - 4 = (r3*3 - 1)<<2
	*/
	add	r3, r3, r3, lsl #1
	sub	r3, r3, #1
	add	pc, pc, r3, lsl #2

	save_brkpt	c15
	save_brkpt	c14
	save_brkpt	c13
	save_brkpt	c12
	save_brkpt	c11
	save_brkpt	c10
	save_brkpt	c9
	save_brkpt	c8
	save_brkpt	c7
	save_brkpt	c6
	save_brkpt	c5
	save_brkpt	c4
	save_brkpt	c3
	save_brkpt	c2
	save_brkpt	c1
	save_brkpt	c0

	mov	r3, r8, lsr #28         @ r3 has the number of wpt
	rsb	r3, r3, #0xf

	/* r3 = (15 - #of wpt) ;
	   switch offset = r3*12 - 4 = (r3*3 - 1)<<2
	*/
	add	r3, r3, r3, lsl #1
	sub	r3, r3, #1
	add	pc, pc, r3, lsl #2

	save_wpt	c15
	save_wpt	c14
	save_wpt	c13
	save_wpt	c12
	save_wpt	c11
	save_wpt	c10
	save_wpt	c9
	save_wpt	c8
	save_wpt	c7
	save_wpt	c6
	save_wpt	c5
	save_wpt	c4
	save_wpt	c3
	save_wpt	c2
	save_wpt	c1
	save_wpt	c0
#endif
	ldmfd	sp!, {r3 - r10, pc}
ENDPROC(cpu_v7_do_suspend)

ENTRY(cpu_v7_do_resume)
	mov	ip, #0
	mcr	p15, 0, ip, c8, c7, 0	@ invalidate TLBs
	mcr	p15, 0, ip, c7, c5, 0	@ invalidate I cache
	mcr	p15, 0, ip, c13, c0, 1	@ set reserved context ID
	ldmia	r0!, {r4 - r6}
	mcr	p15, 0, r4, c13, c0, 0	@ FCSE/PID
	mcr	p15, 0, r5, c13, c0, 3	@ User r/o thread ID
#ifndef CONFIG_TRUSTED_FOUNDATIONS
	mcr	p15, 0, r6, c15, c0, 1	@ diag
#endif

	ldmia	r0!, {r4 - r11}
	mcr	p15, 0, r4, c9, c14, 2	@ PMINTENCLR
	mcr	p15, 0, r5, c9, c14, 0	@ PMUSEREN
	mcr	p15, 0, r6, c9, c12, 5	@ PMSELR, event counter selection
	mcr	p15, 0, r7, c9, c13, 2	@ PMXEVCNTR, event counter
	mcr	p15, 0, r8, c9, c13, 1	@ PMXEVTYPER or PMCCFILTR
	mcr	p15, 0, r9, c9, c13, 0	@ PMCCNTR, cycle counter
	mcr	p15, 0, r10, c9, c12, 0	@ PMCR, control register
	mcr	p15, 0, r11, c9, c12, 1	@ PMCNTENSET, counter enable set

	ldmia	r0!, {r6 - r11}
	mcr	p15, 0, r6, c3, c0, 0	@ Domain ID
#ifndef CONFIG_ARM_LPAE
	ALT_SMP(orr	r1, r1, #TTB_FLAGS_SMP)
	ALT_UP(orr	r1, r1, #TTB_FLAGS_UP)
#endif
	mcr	p15, 0, r1, c2, c0, 0	@ TTB 0
	mcr	p15, 0, r7, c2, c0, 1	@ TTB 1
	mcr	p15, 0, r11, c2, c0, 2	@ TTB control register
	mrc	p15, 0, r4, c1, c0, 1	@ Read Auxiliary control register
	teq	r4, r9			@ Is it already set?
#ifdef CONFIG_ARM_ERRATA_799270
	ldr	r4, =TEGRA_CLK_RESET_BOND_OUT
	ldr	r4, [r4]
	and	r4, r4, #0
	orr	r9, r9, r4
#endif
	mcrne	p15, 0, r9, c1, c0, 1	@ No, so write it
	mcr	p15, 0, r10, c1, c0, 2	@ Co-processor access control
	ldr	r4, =PRRR		@ PRRR
	ldr	r5, =NMRR		@ NMRR
	mcr	p15, 0, r4, c10, c2, 0	@ write PRRR
	mcr	p15, 0, r5, c10, c2, 1	@ write NMRR
	isb

#ifdef CONFIG_ARM_SAVE_DEBUG_CONTEXT
	/* If Debug Architecture v7.1 or later, set OS lock. */
	mrc	p15, 0, r11, c0, c1, 2	@ ID_DFR0
	and	r11, r11, #0xF		@ coprocessor debug model
	cmp	r11, #5			@ debug arch >= v7.1?
#ifndef CONFIG_ARM_SAVE_DEBUG_CONTEXT_NO_LOCK
	ldrge	r4, =0xC5ACCE55		@ Lock value
	mcrge	p14, 0, r4, c1, c0, 4	@ DBGOSLAR
	isb
#endif

	/* Restore CP14 debug controller context */

	ldmia	r0!, {r2 - r5}
	mcr	p14, 0, r3, c0, c6, 0	@ DBGWFAR
	mcr	p14, 0, r4, c0, c7, 0	@ DBGVCR
	mcr	p14, 0, r5, c7, c8, 6	@ DBGCLAIMSET

	ldmltia	r0!, {r4-r5}		@ (debug arch v7 only)
	mcrlt	p14, 0, r4, c0, c10, 0	@ DBGDSCCR (debug arch v7 only)
	mcrlt	p14, 0, r5, c0, c11, 0	@ DBGDSMCR (debug arch v7 only)

	tst	r2, #(1 << 29)		@ DBGDSCRext.TXfull
	ldrne	r4, [r0], #4
	mcrne	p14, 0, r4, c0, c3, 2	@ DBGDTRTXext

	tst	r2, #(1 << 30)		@ DBGDSCRext.RXfull
	ldrne	r4, [r0], #4
	mcrne	p14, 0, r4, c0, c0, 2	@ DBGDTRRXext

	mrc	p14, 0, r5, c0, c0, 0   @ read IDR
	mov	r3, r5, lsr #24
	and	r3, r3, #0xf            @ r3 has the number of brkpt
	rsb	r3, r3, #0xf

	/* r3 = (15 - #of wpt) ;
	   switch offset = r3*12 - 4 = (r3*3 - 1)<<2
	*/
	add     r3, r3, r3, lsl #1
	sub	r3, r3, #1
	add	pc, pc, r3, lsl #2

	restore_brkpt	c15
	restore_brkpt	c14
	restore_brkpt	c13
	restore_brkpt	c12
	restore_brkpt	c11
	restore_brkpt	c10
	restore_brkpt	c9
	restore_brkpt	c8
	restore_brkpt	c7
	restore_brkpt	c6
	restore_brkpt	c5
	restore_brkpt	c4
	restore_brkpt	c3
	restore_brkpt	c2
	restore_brkpt	c1
	restore_brkpt	c0

	mov	r3, r5, lsr #28         @ r3 has the number of wpt
	rsb	r3, r3, #0xf

	/* r3 = (15 - #of wpt) ;
	   switch offset = r3*12 - 4 = (r3*3 - 1)<<2
	*/
	add     r3, r3, r3, lsl #1
	sub	r3, r3, #1
	add	pc, pc, r3, lsl #2

start_restore_wpt:
	restore_wpt	c15
	restore_wpt	c14
	restore_wpt	c13
	restore_wpt	c12
	restore_wpt	c11
	restore_wpt	c10
	restore_wpt	c9
	restore_wpt	c8
	restore_wpt	c7
	restore_wpt	c6
	restore_wpt	c5
	restore_wpt	c4
	restore_wpt	c3
	restore_wpt	c2
	restore_wpt	c1
	restore_wpt	c0
	isb

	mcr	p14, 0, r2, c0, c2, 2	@ DSCR
	isb

#ifndef CONFIG_ARM_SAVE_DEBUG_CONTEXT_NO_LOCK
	mov	r4, #0			@ non-lock value
	cmp	r11, #5			@ debug arch >= v7.1?
	mcrge	p14, 0, r4, c1, c0, 4	@ DBGOSLAR
	isb
#endif
#endif
	dsb
	mov	r0, r8			@ control register
	b	cpu_resume_mmu
ENDPROC(cpu_v7_do_resume)
#endif

	__CPUINIT

/*
 *	__v7_setup
 *
 *	Initialise TLB, Caches, and MMU state ready to switch the MMU
 *	on.  Return in r0 the new CP15 C1 control register setting.
 *
 *	This should be able to cover all ARMv7 cores.
 *
 *	It is assumed that:
 *	- cache type register is implemented
 */
__v7_ca5mp_setup:
__v7_ca9mp_setup:
	mov	r10, #(1 << 0)			@ TLB ops broadcasting
	b	1f
__v7_ca15mp_setup:
#ifdef CONFIG_ARCH_TEGRA
	mrc	p15, 0, r0, c1, c0, 1
	orr	r0, #(1<<24)			@ Enable NCSE in ACTLR
	mcr	p15, 0, r0, c1, c0, 1

	mrc	p15, 1, r0, c15, c0, 3		@ L2PCR
	tst	r0, #0x1000
	orreq	r0, r0, #0x1000			@ disable prefetch throttling
	mcreq	p15, 1, r0, c15, c0, 3

	ALT_SMP(mrc	p15, 0, r0, c1, c0, 1)
	ALT_UP(mov	r0, #(1 << 6))		@ fake it for UP
	tst	r0, #(1 << 6)			@ SMP/nAMP mode enabled?
	orreq	r0, r0, #(1 << 6)		@ Enable SMP/nAMP mode
#ifdef CONFIG_ARM_ERRATA_799270
	ldr	r10, =TEGRA_CLK_RESET_BOND_OUT
	ldr	r10, [r10]
	and	r10, r10, #0
	orr	r0, r0, r10
#endif
	mcreq	p15, 0, r0, c1, c0, 1

	b	__v7_setup
#endif
__v7_ca7mp_setup:
	mov	r10, #0
1:
#ifdef CONFIG_SMP
	ALT_SMP(mrc	p15, 0, r0, c1, c0, 1)
	ALT_UP(mov	r0, #(1 << 6))		@ fake it for UP
	tst	r0, #(1 << 6)			@ SMP/nAMP mode enabled?
	orreq	r0, r0, #(1 << 6)		@ Enable SMP/nAMP mode
	orreq	r0, r0, r10			@ Enable CPU-specific SMP bits
	mcreq	p15, 0, r0, c1, c0, 1
#endif
__v7_setup:
	adr	r12, __v7_setup_stack		@ the local stack
	mrc	p15, 0, r0, c0, c0, 0		@ read main ID register
	ubfx	r10, r0, #4, #28
	ldr	r6, =0x00f0000
	bic	r10, r10, r6
	ldr	r6, =0x410fc0f			@ ARM cortex A15
	teq	r10, r6
	biceq	r10, r10, r10			@ clear r10 if it's A15
	beq	4f
	stmia	r12, {r0-r5, r7, r9, r11, lr}
	bl	v7_flush_dcache_all
	ldmia	r12, {r0-r5, r7, r9, r11, lr}

	and	r10, r0, #0xff000000		@ ARM?
	teq	r10, #0x41000000
	bne	3f
	and	r5, r0, #0x00f00000		@ variant
	and	r6, r0, #0x0000000f		@ revision
	orr	r6, r6, r5, lsr #20-4		@ combine variant and revision
	ubfx	r0, r0, #4, #12			@ primary part number

	/* Cortex-A8 Errata */
	ldr	r10, =0x00000c08		@ Cortex-A8 primary part number
	teq	r0, r10
	bne	2f
#ifdef CONFIG_ARM_ERRATA_430973
	teq	r5, #0x00100000			@ only present in r1p*
	mrceq	p15, 0, r10, c1, c0, 1		@ read aux control register
	orreq	r10, r10, #(1 << 6)		@ set IBE to 1
	mcreq	p15, 0, r10, c1, c0, 1		@ write aux control register
#endif
#ifdef CONFIG_ARM_ERRATA_458693
	teq	r6, #0x20			@ only present in r2p0
	mrceq	p15, 0, r10, c1, c0, 1		@ read aux control register
	orreq	r10, r10, #(1 << 5)		@ set L1NEON to 1
	orreq	r10, r10, #(1 << 9)		@ set PLDNOP to 1
	mcreq	p15, 0, r10, c1, c0, 1		@ write aux control register
#endif
#ifdef CONFIG_ARM_ERRATA_460075
	teq	r6, #0x20			@ only present in r2p0
	mrceq	p15, 1, r10, c9, c0, 2		@ read L2 cache aux ctrl register
	tsteq	r10, #1 << 22
	orreq	r10, r10, #(1 << 22)		@ set the Write Allocate disable bit
	mcreq	p15, 1, r10, c9, c0, 2		@ write the L2 cache aux ctrl register
#endif
	b	3f

	/* Cortex-A9 Errata */
2:	ldr	r10, =0x00000c09		@ Cortex-A9 primary part number
	teq	r0, r10
	bne	3f
#ifndef CONFIG_TRUSTED_FOUNDATIONS
	cmp	r6, #0x10			@ power ctrl reg added r1p0
	mrcge	p15, 0, r10, c15, c0, 0		@ read power control register
	orrge	r10, r10, #1			@ enable dynamic clock gating
	mcrge	p15, 0, r10, c15, c0, 0		@ write power control register
#ifdef CONFIG_ARM_ERRATA_716044
	cmp     r6, #0x12                       @ present in r1p0 - r1p2
	mrcle   p15, 0, r10, c1, c0, 0
	orrle   r10, r10, #(1 << 14)            @ set SCTLR.RR
	mcrle   p15, 0, r10, c1, c0, 0
#endif
#ifdef CONFIG_ARM_ERRATA_720791
	teq	r5, #0x00100000			@ only present in r1p*
	mrceq	p15, 0, r10, c15, c0, 2		@ read "chicken power ctrl" reg
	orreq	r10, r10, #0x30			@ disable core clk gate on
	mcreq	p15, 0, r10, c15, c0, 2		@ instr-side waits
#endif
#ifdef CONFIG_ARM_ERRATA_742230
	cmp	r6, #0x22			@ only present up to r2p2
	mrcle	p15, 0, r10, c15, c0, 1		@ read diagnostic register
	orrle	r10, r10, #1 << 4		@ set bit #4
	mcrle	p15, 0, r10, c15, c0, 1		@ write diagnostic register
#endif
#ifdef CONFIG_ARM_ERRATA_742231
	teq	r6, #0x20			@ present in r2p0
	teqne	r6, #0x21			@ present in r2p1
	teqne	r6, #0x22			@ present in r2p2
	mrceq	p15, 0, r10, c15, c0, 1		@ read diagnostic register
	orreq	r10, r10, #1 << 12		@ set bit #12
	orreq	r10, r10, #1 << 22		@ set bit #22
	mcreq	p15, 0, r10, c15, c0, 1		@ write diagnostic register
#endif
#ifdef CONFIG_ARM_ERRATA_743622
	teq	r5, #0x00200000			@ only present in r2p*
	mrceq	p15, 0, r10, c15, c0, 1		@ read diagnostic register
	orreq	r10, r10, #1 << 6		@ set bit #6
	mcreq	p15, 0, r10, c15, c0, 1		@ write diagnostic register
#endif
#if defined(CONFIG_ARM_ERRATA_751472) && defined(CONFIG_SMP)
	ALT_SMP(cmp r6, #0x30)			@ present prior to r3p0
	ALT_UP_B(1f)
	mrclt	p15, 0, r10, c15, c0, 1		@ read diagnostic register
	orrlt	r10, r10, #1 << 11		@ set bit #11
	mcrlt	p15, 0, r10, c15, c0, 1		@ write diagnostic register
1:
#endif
#ifdef CONFIG_ARM_ERRATA_752520
	cmp	r6, #0x28			@ present prior to r2p9
	teqlt	r5, #0x00200000			@ present from r2p0
	mrceq	p15, 0, r10, c15, c0, 1		@ read diagnostic register
	orreq	r10, r10, #1 << 20		@ set bit #20
	mcreq	p15, 0, r10, c15, c0, 1		@ write diagnostic register
#endif
#endif

3:	mov	r10, #0
	mcr	p15, 0, r10, c7, c5, 0		@ I+BTB cache invalidate
	dsb
4:
#ifdef CONFIG_MMU
	mcr	p15, 0, r10, c8, c7, 0		@ invalidate I + D TLBs
	v7_ttb_setup r10, r4, r8, r5		@ TTBCR, TTBRx setup
	ldr	r5, =PRRR			@ PRRR
	ldr	r6, =NMRR			@ NMRR
	mcr	p15, 0, r5, c10, c2, 0		@ write PRRR
	mcr	p15, 0, r6, c10, c2, 1		@ write NMRR
#endif
#ifndef CONFIG_ARM_THUMBEE
	mrc	p15, 0, r0, c0, c1, 0		@ read ID_PFR0 for ThumbEE
	and	r0, r0, #(0xf << 12)		@ ThumbEE enabled field
	teq	r0, #(1 << 12)			@ check if ThumbEE is present
	bne	1f
	mov	r5, #0
	mcr	p14, 6, r5, c1, c0, 0		@ Initialize TEEHBR to 0
	mrc	p14, 6, r0, c0, c0, 0		@ load TEECR
	orr	r0, r0, #1			@ set the 1st bit in order to
	mcr	p14, 6, r0, c0, c0, 0		@ stop userspace TEEHBR access
1:
#endif
	adr	r5, v7_crval
	ldmia	r5, {r5, r6}
#ifdef CONFIG_CPU_ENDIAN_BE8
	orr	r6, r6, #1 << 25		@ big-endian page tables
#endif
#ifdef CONFIG_SWP_EMULATE
	orr     r5, r5, #(1 << 10)              @ set SW bit in "clear"
	bic     r6, r6, #(1 << 10)              @ clear it in "mmuset"
#endif
   	mrc	p15, 0, r0, c1, c0, 0		@ read control register
	bic	r0, r0, r5			@ clear bits them
	orr	r0, r0, r6			@ set them
 THUMB(	orr	r0, r0, #1 << 30	)	@ Thumb exceptions
	mov	pc, lr				@ return to head.S:__ret
ENDPROC(__v7_setup)

	.align	2
__v7_setup_stack:
	.space	4 * 11				@ 11 registers

	__INITDATA

	@ define struct processor (see <asm/proc-fns.h> and proc-macros.S)
	define_processor_functions v7, dabort=v7_early_abort, pabort=v7_pabort, suspend=1

	.section ".rodata"

	string	cpu_arch_name, "armv7"
	string	cpu_elf_name, "v7"
	.align

	.section ".proc.info.init", #alloc, #execinstr

	/*
	 * Standard v7 proc info content
	 */
.macro __v7_proc initfunc, mm_mmuflags = 0, io_mmuflags = 0, hwcaps = 0
	ALT_SMP(.long	PMD_TYPE_SECT | PMD_SECT_AP_WRITE | PMD_SECT_AP_READ | \
			PMD_SECT_AF | PMD_FLAGS_SMP | \mm_mmuflags)
	ALT_UP(.long	PMD_TYPE_SECT | PMD_SECT_AP_WRITE | PMD_SECT_AP_READ | \
			PMD_SECT_AF | PMD_FLAGS_UP | \mm_mmuflags)
	.long	PMD_TYPE_SECT | PMD_SECT_AP_WRITE | \
		PMD_SECT_AP_READ | PMD_SECT_AF | \io_mmuflags
	W(b)	\initfunc
	.long	cpu_arch_name
	.long	cpu_elf_name
	.long	HWCAP_SWP | HWCAP_HALF | HWCAP_THUMB | HWCAP_FAST_MULT | \
		HWCAP_EDSP | HWCAP_TLS | \hwcaps
	.long	cpu_v7_name
	.long	v7_processor_functions
	.long	v7wbi_tlb_fns
	.long	v6_user_fns
	.long	v7_cache_fns
.endm

#ifndef CONFIG_ARM_LPAE
	/*
	 * ARM Ltd. Cortex A5 processor.
	 */
	.type   __v7_ca5mp_proc_info, #object
__v7_ca5mp_proc_info:
	.long	0x410fc050
	.long	0xff0ffff0
	__v7_proc __v7_ca5mp_setup
	.size	__v7_ca5mp_proc_info, . - __v7_ca5mp_proc_info

	/*
	 * ARM Ltd. Cortex A9 processor.
	 */
	.type   __v7_ca9mp_proc_info, #object
__v7_ca9mp_proc_info:
	.long	0x410fc090
	.long	0xff0ffff0
	__v7_proc __v7_ca9mp_setup
	.size	__v7_ca9mp_proc_info, . - __v7_ca9mp_proc_info
#endif	/* CONFIG_ARM_LPAE */

	/*
	 * ARM Ltd. Cortex A7 processor.
	 */
	.type	__v7_ca7mp_proc_info, #object
__v7_ca7mp_proc_info:
	.long	0x410fc070
	.long	0xff0ffff0
	__v7_proc __v7_ca7mp_setup, hwcaps = HWCAP_IDIV
	.size	__v7_ca7mp_proc_info, . - __v7_ca7mp_proc_info

	/*
	 * ARM Ltd. Cortex A15 processor.
	 */
	.type	__v7_ca15mp_proc_info, #object
__v7_ca15mp_proc_info:
	.long	0x410fc0f0
	.long	0xff0ffff0
	__v7_proc __v7_ca15mp_setup, hwcaps = HWCAP_IDIV
	.size	__v7_ca15mp_proc_info, . - __v7_ca15mp_proc_info

	/*
	 * Match any ARMv7 processor core.
	 */
	.type	__v7_proc_info, #object
__v7_proc_info:
	.long	0x000f0000		@ Required ID value
	.long	0x000f0000		@ Mask for ID
	__v7_proc __v7_setup
	.size	__v7_proc_info, . - __v7_proc_info
