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

#include "idle.h"
#include "idle-macros.S"

#ifdef CONFIG_MSM_SCM
#define SCM_SVC_BOOT 0x1
#define SCM_CMD_TERMINATE_PC 0x2
#define SCM_CMD_CORE_HOTPLUGGED 0x10
#endif

ENTRY(msm_arch_idle)
#ifdef CONFIG_ARCH_MSM_KRAIT
	mrc 	p15, 0, r0, c0, c0, 0
	bic	r1, r0, #0xff
	movw	r2, #0x0400
	movt	r2, #0x511F
	movw	r3, #0x0600
	movt	r3, #0x510F
	cmp	r2, r1
	cmpne	r3, r1
	bne	go_wfi

	mrs	r0, cpsr
	cpsid	if

	mrc	p15, 7, r1, c15, c0, 5
	bic	r2, r1, #0x20000
	mcr	p15, 7, r2, c15, c0, 5
	isb

go_wfi:
	wfi
	bne	wfi_done
	mcr	p15, 7, r1, c15, c0, 5
	isb
	msr	cpsr_c, r0

wfi_done:
	bx	lr
#else
	wfi
#ifdef CONFIG_ARCH_MSM8X60
	mrc	p14, 1, r1, c1, c5, 4 /* read ETM PDSR to clear sticky bit */
	mrc     p14, 0, r1, c1, c5, 4 /* read DBG PRSR to clear sticky bit */
	isb
#endif
	bx	lr
#endif
ENDPROC(msm_arch_idle)

ENTRY(msm_pm_pc_hotplug)
	stmfd	sp!, {lr}
#if defined(CONFIG_MSM_FIQ_SUPPORT)
	cpsid   f
#endif

#if defined(CONFIG_MSM_JTAG) || defined(CONFIG_MSM_JTAG_MM)
	bl      msm_jtag_save_state
#endif
	mov	r1, #0
	mcr	p15, 2, r1, c0, c0, 0 /*CCSELR*/
	isb
	mrc	p15, 1, r1, c0, c0, 0 /*CCSIDR*/
	mov	r2, #1
	and	r1, r2, r1, ASR #30 /* Check if the cache is write back */
	cmp	r1, #1
	bleq	v7_flush_kern_cache_all

	mrc	p15, 0, r0, c0, c0, 5	/* MPIDR */
	and	r0, r0, #15		/* what CPU am I */

	ldr	r1, =msm_pc_debug_counters /*load the IMEM debug location */
	ldr	r1, [r1]
	cmp	r1, #0
	beq	skip_hp_debug1
	add	r1, r1, r0, LSL #4	/* debug location for this CPU */
	ldr	r2, [r1]
	add	r2, #1
	str	r2, [r1]
skip_hp_debug1:

#ifdef CONFIG_MSM_SCM
	ldr	r0, =SCM_SVC_BOOT
	ldr	r1, =SCM_CMD_TERMINATE_PC
	ldr	r2, =SCM_CMD_CORE_HOTPLUGGED
	bl	scm_call_atomic1
#else
	mrc     p15, 0, r3, c1, c0, 0    /* read current CR    */
	bic     r0, r3, #(1 << 2)        /* clear dcache bit   */
	bic     r0, r0, #(1 << 12)       /* clear icache bit   */
	mcr     p15, 0, r0, c1, c0, 0    /* disable d/i cache  */
	isb
	wfi
	mcr     p15, 0, r3, c1, c0, 0    /* restore d/i cache  */
	isb
#endif

#if defined(CONFIG_MSM_FIQ_SUPPORT)
	cpsie   f
#endif
	mrc	p15, 0, r0, c0, c0, 5 /* MPIDR */
	and	r0, r0, #15              /* what CPU am I                  */

	ldr	r1, =msm_pc_debug_counters /*load the IMEM debug location */
	ldr	r1, [r1]
	cmp	r1, #0
	beq	skip_hp_debug2
	add	r1, r1, r0, LSL #4	/* debug location for this CPU */
	add	r1, #8
	ldr	r2, [r1]
	add	r2, #1
	str	r2, [r1]
skip_hp_debug2:
#if defined(CONFIG_MSM_JTAG) || defined(CONFIG_MSM_JTAG_MM)
	bl	msm_jtag_restore_state
#endif
	mov     r0, #0                   /* return power collapse failed */
	ldmfd	sp!, {lr}
	bx      lr
ENDPROC(msm_pm_pc_hotplug)

ENTRY(msm_pm_collapse)
#if defined(CONFIG_MSM_FIQ_SUPPORT)
	cpsid   f
#endif

	ldr     r0, =msm_saved_state	/* address of msm_saved_state ptr */
	ldr	r0, [r0]		/* load ptr */
#if (NR_CPUS >= 2)
	mrc	p15, 0, r1, c0, c0, 5	/* MPIDR */
	ands	r1, r1, #15		/* What CPU am I */
	mov	r2, #CPU_SAVED_STATE_SIZE
	mul	r1, r1, r2
	add	r0, r0, r1
#endif

	PUSH	{r4-r12}
ARM(	stmia   r0!, {r13-r14}	)
THUMB(	str	r13, [r0],#4	)
THUMB(	str	r14, [r0],#4	)
#ifdef	CONFIG_ARM_LPAE
	mrc	p15, 0, r1, c1, c0, 0 /* MMU control */
	mrc	p15, 0, r2, c2, c0, 2 /*TTBCR*/
	mrc	p15, 0, r3, c3, c0, 0 /* dacr */
	mrrc	p15, 0, r4, r5, c2 /* TTBR0 */
	mrrc	p15, 1, r6, r7, c2 /* TTBR1 */
	mrc	p15, 0, r8, c10, c2, 0 /* MAIR0 */
	mrc	p15, 0, r9, c10, c2, 1  /* MAIR1 */
	mrc	p15, 0, r10, c1, c0, 1 /* ACTLR */
	mrc	p15, 0, r11, c13, c0, 3 /* TPIDRURO */
	mrc	p15, 0, r12, c13, c0, 1 /* context ID */
	stmia	r0!, {r1-r12}
#else
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

#endif
#if defined(CONFIG_MSM_JTAG) || defined(CONFIG_MSM_JTAG_MM)
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
	ldr	r1, =msm_pm_flush_l2_fn
	ldr	r1, [r1]
	cmp	r1, #0
	blxne	r1

skip:
	ldr	r1, =msm_pm_disable_l2_fn
	ldr	r1, [r1]
	cmp	r1, #0
	blxne	r1
	dmb

	mrc	p15, 0, r0, c0, c0, 5	/* MPIDR */
	and	r0, r0, #15		/* what CPU am I */

	ldr	r1, =msm_pc_debug_counters /*load the IMEM debug location */
	ldr	r1, [r1]
	cmp	r1, #0
	beq	skip_pc_debug1
	add	r1, r1, r0, LSL #4	/* debug location for this CPU */
	ldr	r2, [r1]
	add	r2, #1
	str	r2, [r1]
skip_pc_debug1:

#ifdef CONFIG_MSM_SCM
	ldr	r0, =SCM_SVC_BOOT
	ldr	r1, =SCM_CMD_TERMINATE_PC
	ldr	r2, =msm_pm_flush_l2_flag
	ldr	r2, [r2]
	bl	scm_call_atomic1
#else
	mrc     p15, 0, r4, c1, c0, 0    /* read current CR    */
	bic     r0, r4, #(1 << 2)        /* clear dcache bit   */
	bic     r0, r0, #(1 << 12)       /* clear icache bit   */
	mcr     p15, 0, r0, c1, c0, 0    /* disable d/i cache  */
	isb

	SUSPEND_8x25_L2
	SET_SMP_COHERENCY OFF
	wfi
	DELAY 300

	mcr     p15, 0, r4, c1, c0, 0    /* restore d/i cache  */
	isb
	ENABLE_8x25_L2 /* enable only l2, no need to restore the reg back */
	SET_SMP_COHERENCY ON
#endif

#if defined(CONFIG_MSM_FIQ_SUPPORT)
	cpsie   f
#endif
	mrc	p15, 0, r0, c0, c0, 5 /* MPIDR */
	and	r0, r0, #15              /* what CPU am I                  */

	ldr	r1, =msm_pc_debug_counters /*load the IMEM debug location */
	ldr	r1, [r1]
	cmp	r1, #0
	beq	skip_pc_debug2
	add	r1, r1, r0, LSL #4	/* debug location for this CPU */
	add	r1, #8
	ldr	r2, [r1]
	add	r2, #1
	str	r2, [r1]

skip_pc_debug2:
	ldr	r1, =msm_pm_enable_l2_fn
	ldr	r1, [r1]
	cmp	r1, #0
	blxne	r1
	dmb

#if defined(CONFIG_MSM_JTAG) || defined(CONFIG_MSM_JTAG_MM)
	bl	msm_jtag_restore_state
#endif
	ldr     r0, =msm_saved_state	/* address of msm_saved_state ptr */
	ldr	r0, [r0]		/* load ptr */
#if (NR_CPUS >= 2)
	mrc	p15, 0, r1, c0, c0, 5	/* MPIDR */
	ands	r1, r1, #15		/* What CPU am I */
	mov	r2, #CPU_SAVED_STATE_SIZE
	mul	r2, r2, r1
	add	r0, r0, r2
#endif
ARM(	ldmfd   r0, {r13-r14}	)		 /* restore sp and lr */
THUMB(	ldr	r13, [r0],#4	)
THUMB(	ldr	r14, [r0],#4	)
	POP 	{r4-r12}		 /* restore r4-r12 registers */
	mov     r0, #0                   /* return power collapse failed */
	bx      lr
ENDPROC(msm_pm_collapse)

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
	ldr     r1, =msm_saved_state_phys
	ldr     r2, =msm_pm_collapse_exit
	adr     r3, msm_pm_collapse_exit
	add     r1, r1, r3
	sub     r1, r1, r2
	ldr	r1, [r1]
	add	r1, r1, #CPU_SAVED_STATE_SIZE
#if (NR_CPUS >= 2)
	mrc	p15, 0, r2, c0, c0, 5	/* MPIDR */
	ands	r2, r2, #15		/* What CPU am I */
	mov	r3, #CPU_SAVED_STATE_SIZE
	mul	r2, r2, r3
	add	r1, r1, r2
#endif

#ifdef CONFIG_ARM_LPAE
	ldmdb   r1!, {r2-r13}
	mcr	p15, 0, r3, c2, c0, 2 /*TTBCR*/
	mcr     p15, 0, r4, c3, c0, 0 /* dacr */
	mcrr    p15, 0, r5, r6, c2 /* TTBR0 */
	mcrr    p15, 1, r7, r8, c2 /* TTBR1 */
	mcr     p15, 0, r9, c10, c2, 0 /* MAIR0 */
	mcr     p15, 0, r10, c10, c2, 1 /* MAIR1 */
	mcr     p15, 0, r11, c1, c0, 1 /* ACTLR */
	mcr     p15, 0, r12, c13, c0, 3 /* TPIDRURO */
	mcr     p15, 0, r13, c13, c0, 1 /* context ID */
	isb
	ldmdb   r1!, {r13, r14}
	ldr	r0, =msm_pm_pc_pgd
	ldr	r4, =msm_pm_collapse_exit
	adr	r3, msm_pm_collapse_exit
	add	r0, r0, r3
	sub	r0, r0, r4
	ldr	r0, [r0]
	ldr	r3, [r0]
	mrrc	p15, 0, r4, r5, c2     /* save current TTBR0 */
	and	r6, r4, #0x7f /* mask to get TTB flags */
	mcr	p15, 0, r3, c2, c0, 0 /* temporary switch TTBR0 */
	isb
	mcr	p15, 0, r2, c1, c0, 0   /* MMU control */
	isb
msm_pm_mapped_pa:
	/* Switch to virtual */
	ldr     r2, =msm_pm_pa_to_va
	mov     pc, r2
msm_pm_pa_to_va:
	mcrr    p15, 0, r4, r5, c2	/* restore TTBR0 */
	isb
	mcr     p15, 0, r6, c8, c7, 0   /* UTLBIALL */
	mcr     p15, 0, r6, c7, c5, 6   /* BPIALL */
	dsb
	isb
#else
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
ARM(	ldmdb   r1!, {r13-r14}	)
THUMB(	ldr	r14, [r1, #-4]!	)
THUMB(	ldr	r13, [r1, #-4]!	)
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
#endif
	POP	{r4-r12}

#ifdef CONFIG_ARCH_MSM_KRAIT
	mrc	p15, 0, r1, c0, c0, 0
	ldr	r3, =0xffffff0d
	and	r3, r1, r3
	ldr 	r1, =0x511f0400
	cmp	r3, r1
	mrceq	p15, 7, r3, c15, c0, 2
	biceq	r3, r3, #0x400
	mcreq	p15, 7, r3, c15, c0, 2
#else
	RESUME_8x25_L2
	SET_SMP_COHERENCY ON
#endif

	ldr	r1, =msm_pm_enable_l2_fn
	ldr	r1, [r1]
	cmp	r1, #0
	stmfd   sp!, {lr}
	blxne	r1
	dmb
#if defined(CONFIG_MSM_JTAG) || defined(CONFIG_MSM_JTAG_MM)
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
ENDPROC(msm_pm_collapse_exit)

ENTRY(msm_pm_boot_entry)
	mrc     p15, 0, r0, c0, c0, 5    /* MPIDR                          */
	and     r0, r0, #15              /* what CPU am I                  */

	ldr	r1, =msm_pc_debug_counters_phys /*phys addr for IMEM reg */
	ldr	r2, =msm_pm_boot_entry
	adr	r3, msm_pm_boot_entry
	add	r1, r1, r3               /* translate virt to phys addr    */
	sub	r1, r1, r2
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
	ldr     r2, =msm_pm_boot_entry
	adr     r3, msm_pm_boot_entry
	add     r1, r1, r3               /* translate virt to phys addr    */
	sub     r1, r1, r2

	add     r1, r1, r0, LSL #2       /* locate boot vector for our cpu */
	ldr     pc, [r1]                 /* jump                           */
ENDPROC(msm_pm_boot_entry)

ENTRY(msm_pm_set_l2_flush_flag)
	ldr     r1, =msm_pm_flush_l2_flag
	str     r0, [r1]
	bx      lr
ENDPROC(msm_pm_set_l2_flush_flag)

ENTRY(msm_pm_get_l2_flush_flag)
	ldr     r1, =msm_pm_flush_l2_flag
	ldr     r0, [r1]
	bx      lr
ENDPROC(msm_pm_get_l2_flush_flag)

	.data

	.globl msm_pm_pc_pgd
msm_pm_pc_pgd:
	.long	0x0

	.globl msm_saved_state
msm_saved_state:
	.long	0x0

	.globl msm_saved_state_phys
msm_saved_state_phys:
	.long	0x0

	.globl msm_pm_boot_vector
msm_pm_boot_vector:
	.space  4 * NR_CPUS

	.globl target_type
target_type:
	.long  0x0

	.globl apps_power_collapse
apps_power_collapse:
	.long 0x0

	.globl l2x0_base_addr
l2x0_base_addr:
	.long 0x0

	.globl msm_pc_debug_counters_phys
msm_pc_debug_counters_phys:
	.long 0x0

	.globl msm_pc_debug_counters
msm_pc_debug_counters:
	.long 0x0

	.globl msm_pm_enable_l2_fn
msm_pm_enable_l2_fn:
	.long 0x0

	.globl msm_pm_disable_l2_fn
msm_pm_disable_l2_fn:
	.long 0x0

	.globl msm_pm_flush_l2_fn
msm_pm_flush_l2_fn:
	.long 0x0

/*
 * Default the l2 flush flag to 1 so that caches are flushed during power
 * collapse unless the  L2 driver decides to flush them only during L2
 * Power collapse.
 */
msm_pm_flush_l2_flag:
	.long 0x1

/*
 * Save & restore l2x0 registers while system is entering and resuming
 * from Power Collapse.
 * 1. aux_ctrl_save (0x0)
 * 2. data_latency_ctrl (0x4)
 * 3. prefetch control (0x8)
 */
l2x0_saved_ctrl_reg_val:
	.space 4 * 3
