/*
 * Idle processing for ARMv7-based Qualcomm SoCs.
 *
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2007-2009, 2011-2012 Code Aurora Forum. All rights reserved.
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

#ifdef CONFIG_MSM_CPU_AVS
/* 11 general purpose registers (r4-r14), 10 cp15 registers, 3 AVS registers */
#define CPU_SAVED_STATE_SIZE (4 * 11 + 4 * 10 + 4 * 3)
#else
/* 11 general purpose registers (r4-r14), 10 cp15 registers */
#define CPU_SAVED_STATE_SIZE (4 * 11 + 4 * 10)
#endif
#ifdef CONFIG_ARCH_MSM_KRAIT
#define SCM_SVC_BOOT 0x1
#define SCM_CMD_TERMINATE_PC 0x2
#endif

ENTRY(msm_arch_idle)
	wfi
#ifdef CONFIG_ARCH_MSM8X60
	mrc	p14, 1, r1, c1, c5, 4 /* read ETM PDSR to clear sticky bit */
	mrc     p14, 0, r1, c1, c5, 4 /* read DBG PRSR to clear sticky bit */
	isb
#endif
	bx	lr

ENTRY(msm_pm_collapse)
#if defined(CONFIG_MSM_FIQ_SUPPORT)
	cpsid   f
#endif

	ldr     r0, =saved_state
#if (NR_CPUS >= 2)
	mrc	p15, 0, r1, c0, c0, 5	/* MPIDR */
	ands	r1, r1, #15		/* What CPU am I */
	addne	r0, r0, #CPU_SAVED_STATE_SIZE
#endif

	stmia   r0!, {r4-r14}
	mrc     p15, 0, r1, c1, c0, 0 /* MMU control */
	mrc     p15, 0, r2, c2, c0, 0 /* TTBR0 */
	mrc     p15, 0, r3, c3, c0, 0 /* dacr */
#ifdef CONFIG_ARCH_MSM_SCORPION
	/* This instruction is not valid for non scorpion processors */
	mrc     p15, 3, r4, c15, c0, 3 /* L2CR1 is the L2 cache control reg 1 */
#endif
	mrc     p15, 0, r5, c10, c2, 0 /* PRRR */
	mrc     p15, 0, r6, c10, c2, 1 /* NMRR */
	mrc     p15, 0, r7, c1, c0, 1 /* ACTLR */
	mrc     p15, 0, r8, c2, c0, 1 /* TTBR1 */
	mrc     p15, 0, r9, c13, c0, 3 /* TPIDRURO */
	mrc     p15, 0, ip, c13, c0, 1 /* context ID */
	stmia   r0!, {r1-r9, ip}
#ifdef CONFIG_MSM_CPU_AVS
	mrc     p15, 7, r1, c15, c1, 7 /* AVSCSR is the Adaptive Voltage Scaling
	                                * Control and Status Register */
	mrc     p15, 7, r2, c15, c0, 6 /* AVSDSCR is the Adaptive Voltage
	                                * Scaling Delay Synthesizer Control
					* Register */
#ifndef CONFIG_ARCH_MSM_KRAIT
	mrc     p15, 7, r3, c15, c1, 0 /* TSCSR is the Temperature Status and
	                                * Control Register
					*/
#endif

	stmia   r0!, {r1-r3}
#endif

#ifdef CONFIG_MSM_JTAG
	bl      msm_jtag_save_state
#endif

	ldr	r0, =msm_pm_flush_l2_flag
	ldr	r0, [r0]
	mov	r1, #0
	mcr	p15, 2, r1, c0, c0, 0 /*CCSELR*/
	isb
	mrc	p15, 1, r1, c0, c0, 0 /*CCSIDR*/
	mov	r2, #1
	and	r1, r2, r1, ASR #30 /* Check if the cache is write back */
	orr	r1, r0, r1
	cmp	r1, #1
	bne	skip
	bl	v7_flush_dcache_all

skip:	ldr	r0, =saved_state
	ldr	r1, =saved_state_end
	sub	r1, r1, r0
	bl	v7_flush_kern_dcache_area

	mrc     p15, 0, r4, c1, c0, 0    /* read current CR    */
	bic     r0, r4, #(1 << 2)        /* clear dcache bit   */
	bic     r0, r0, #(1 << 12)       /* clear icache bit   */
	mcr     p15, 0, r0, c1, c0, 0    /* disable d/i cache  */

	dsb
#ifdef CONFIG_ARCH_MSM_KRAIT
	ldr	r0, =SCM_SVC_BOOT
	ldr	r1, =SCM_CMD_TERMINATE_PC
	ldr	r2, =msm_pm_flush_l2_flag
	ldr	r2, [r2]
	bl	scm_call_atomic1
#else
	wfi
#endif
	mcr     p15, 0, r4, c1, c0, 0    /* restore d/i cache  */
	isb

#if defined(CONFIG_MSM_FIQ_SUPPORT)
	cpsie   f
#endif
#ifdef CONFIG_MSM_JTAG
	bl	msm_jtag_restore_state
#endif
	ldr     r0, =saved_state        /* restore registers */
#if (NR_CPUS >= 2)
	mrc	p15, 0, r1, c0, c0, 5	/* MPIDR */
	ands	r1, r1, #15		/* What CPU am I */
	addne	r0, r0, #CPU_SAVED_STATE_SIZE
#endif

	ldmfd   r0, {r4-r14}
	mov     r0, #0                   /* return power collapse failed */
	bx      lr

ENTRY(msm_pm_collapse_exit)
#if 0 /* serial debug */
	mov     r0, #0x80000016
	mcr     p15, 0, r0, c15, c2, 4
	mov     r0, #0xA9000000
	add     r0, r0, #0x00A00000 /* UART1 */
	/*add     r0, r0, #0x00C00000*/ /* UART3 */
	mov     r1, #'A'
	str     r1, [r0, #0x00C]
#endif
	ldr     r1, =saved_state_end
	ldr     r2, =msm_pm_collapse_exit
	adr     r3, msm_pm_collapse_exit
	add     r1, r1, r3
	sub     r1, r1, r2
#if (NR_CPUS >= 2)
	mrc	p15, 0, r2, c0, c0, 5	/* MPIDR */
	ands	r2, r2, #15		/* What CPU am I */
	subeq	r1, r1, #CPU_SAVED_STATE_SIZE
#endif

#ifdef CONFIG_MSM_CPU_AVS
	ldmdb   r1!, {r2-r4}
#ifndef CONFIG_ARCH_MSM_KRAIT
	mcr     p15, 7, r4, c15, c1, 0 /* TSCSR */
#endif
	mcr     p15, 7, r3, c15, c0, 6 /* AVSDSCR */
	mcr     p15, 7, r2, c15, c1, 7 /* AVSCSR */
#endif
	ldmdb   r1!, {r2-r11}
	mcr     p15, 0, r4, c3, c0, 0 /* dacr */
	mcr     p15, 0, r3, c2, c0, 0 /* TTBR0 */
#ifdef CONFIG_ARCH_MSM_SCORPION
	/* This instruction is not valid for non scorpion processors */
	mcr     p15, 3, r5, c15, c0, 3 /* L2CR1 */
#endif
	mcr     p15, 0, r6, c10, c2, 0 /* PRRR */
	mcr     p15, 0, r7, c10, c2, 1 /* NMRR */
	mcr     p15, 0, r8, c1, c0, 1 /* ACTLR */
	mcr     p15, 0, r9, c2, c0, 1 /* TTBR1 */
	mcr     p15, 0, r10, c13, c0, 3 /* TPIDRURO */
	mcr     p15, 0, r11, c13, c0, 1 /* context ID */
	isb
	ldmdb   r1!, {r4-r14}
	ldr	r0, =msm_pm_pc_pgd
	ldr	r1, =msm_pm_collapse_exit
	adr	r3, msm_pm_collapse_exit
	add	r0, r0, r3
	sub	r0, r0, r1
	ldr	r0, [r0]
	mrc     p15, 0, r1, c2, c0, 0 /* save current TTBR0 */
	and	r3, r1, #0x7f /* mask to get TTB flags */
	orr	r0, r0, r3 /* add TTB flags to switch TTBR value */
	mcr     p15, 0, r0, c2, c0, 0 /* temporary switch TTBR0 */
	isb
	mcr     p15, 0, r2, c1, c0, 0   /* MMU control */
	isb
msm_pm_mapped_pa:
	/* Switch to virtual */
	ldr     r0, =msm_pm_pa_to_va
	mov     pc, r0
msm_pm_pa_to_va:
	mcr     p15, 0, r1, c2, c0, 0 /* restore TTBR0 */
	isb
	mcr     p15, 0, r3, c8, c7, 0   /* UTLBIALL */
	mcr     p15, 0, r3, c7, c5, 6   /* BPIALL */
	dsb
	isb
#ifdef CONFIG_ARCH_MSM_KRAIT
	mrc	p15, 0, r1, c0, c0, 0
	ldr	r3, =0xff00fc00
	and	r3, r1, r3
	ldr 	r1, =0x51000400
	cmp	r3, r1
	mrceq	p15, 7, r3, c15, c0, 2
	biceq	r3, r3, #0x400
	mcreq	p15, 7, r3, c15, c0, 2
#endif
	stmfd   sp!, {lr}
	bl      v7_flush_kern_cache_all
#ifdef CONFIG_MSM_JTAG
	bl      msm_jtag_restore_state
#endif
	ldmfd   sp!, {lr}
	mov     r0, #1
	bx      lr
	nop
	nop
	nop
	nop
	nop
1:	b       1b

ENTRY(msm_pm_boot_entry)
	mrc     p15, 0, r0, c0, c0, 5    /* MPIDR                          */
	and     r0, r0, #15              /* what CPU am I                  */

	ldr     r1, =msm_pm_boot_vector
	ldr     r2, =msm_pm_boot_entry
	adr     r3, msm_pm_boot_entry
	add     r1, r1, r3               /* translate virt to phys addr    */
	sub     r1, r1, r2

	add     r1, r1, r0, LSL #2       /* locate boot vector for our cpu */
	ldr     pc, [r1]                 /* jump                           */

ENTRY(msm_pm_write_boot_vector)
	ldr     r2, =msm_pm_boot_vector
	add     r2, r2, r0, LSL #2       /* locate boot vector for our cpu */
	str     r1, [r2]
	mov	r0, r2
	ldr	r1, =4
	stmfd	sp!, {lr}
	bl	v7_flush_kern_dcache_area
	ldmfd	sp!, {lr}
	bx      lr

ENTRY(msm_pm_set_l2_flush_flag)
	ldr r1, =msm_pm_flush_l2_flag
	str r0, [r1]
	bx lr

	.data

	.globl msm_pm_pc_pgd
msm_pm_pc_pgd:
	.long	0x0

saved_state:
#if (NR_CPUS >= 2)
	.space	CPU_SAVED_STATE_SIZE * 2 /* This code only supports 2 cores */
#else
	.space	CPU_SAVED_STATE_SIZE
#endif
saved_state_end:

msm_pm_boot_vector:
	.space  4 * NR_CPUS

/*
 * Default the l2 flush flag to 1 so that caches are flushed during power
 * collapse unless the  L2 driver decides to flush them only during L2
 * Power collapse.
 */
msm_pm_flush_l2_flag:
	.long 0x1
