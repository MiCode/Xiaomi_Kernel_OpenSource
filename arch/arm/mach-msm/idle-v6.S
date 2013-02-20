/*
 * Idle processing for ARMv6-based Qualcomm SoCs.
 * Work around bugs with SWFI.
 *
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2007-2009, The Linux Foundation. All rights reserved.
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
#include <asm/assembler.h>

.extern write_to_strongly_ordered_memory

ENTRY(msm_arch_idle)
	mrs	r2, cpsr		/* save the CPSR state */
	cpsid   iaf			/* explictly disable I,A and F */

#if defined(CONFIG_ARCH_MSM7X27)
	mov     r0, #0
	mcr     p15, 0, r0, c7, c10, 0   /* flush entire data cache */
	mcr     p15, 0, r0, c7, c10, 4   /* dsb                */
	stmfd   sp!, {r2, lr}            /* preserve r2, thus CPSR and LR */
	bl      write_to_strongly_ordered_memory  /* flush AXI bus buffer */
	ldmfd   sp!, {r2, lr}
	mcr     p15, 0, r0, c7, c0, 4    /* wait for interrupt */
#else
	mrc     p15, 0, r1, c1, c0, 0    /* read current CR    */
	bic     r0, r1, #(1 << 2)        /* clear dcache bit   */
	bic     r0, r0, #(1 << 12)       /* clear icache bit   */
	mcr     p15, 0, r0, c1, c0, 0    /* disable d/i cache  */

	mov     r0, #0
	mcr     p15, 0, r0, c7, c5, 0    /* invalidate icache and flush */
	                                 /* branch target cache */
	mcr     p15, 0, r0, c7, c14, 0   /* clean and invalidate dcache */

	mcr     p15, 0, r0, c7, c10, 4   /* dsb                */
	mcr     p15, 0, r0, c7, c0, 4    /* wait for interrupt */

	mcr     p15, 0, r1, c1, c0, 0    /* restore d/i cache  */
	mcr     p15, 0, r0, c7, c5, 4    /* isb                */
#endif

	msr	cpsr_c, r2		/* restore the CPSR state */
	mov     pc, lr

ENTRY(msm_pm_collapse)
	ldr     r0, =saved_state
	stmia   r0!, {r4-r14}

	cpsid   f

	mrc     p15, 0, r1, c1, c0, 0 /* MMU control */
	mrc     p15, 0, r2, c2, c0, 0 /* ttb */
	mrc     p15, 0, r3, c3, c0, 0 /* dacr */
	mrc     p15, 0, ip, c13, c0, 1 /* context ID */
	stmia   r0!, {r1-r3, ip}
#if defined(CONFIG_OPROFILE)
	mrc     p15, 0, r1, c15, c12, 0 /* pmnc */
	mrc     p15, 0, r2, c15, c12, 1 /* ccnt */
	mrc     p15, 0, r3, c15, c12, 2 /* pmn0 */
	mrc     p15, 0, ip, c15, c12, 3 /* pmn1 */
	stmia   r0!, {r1-r3, ip}
#endif
	mrc	p15, 0, r1, c1, c0, 2	/* read CACR */
	stmia   r0!, {r1}

	mrc     p15, 0, r1, c1, c0, 0    /* read current CR    */
	bic     r0, r1, #(1 << 2)        /* clear dcache bit   */
	bic     r0, r0, #(1 << 12)       /* clear icache bit   */
	mcr     p15, 0, r0, c1, c0, 0    /* disable d/i cache  */

	mov     r0, #0
	mcr     p15, 0, r0, c7, c5, 0    /* invalidate icache and flush */
	                                 /* branch target cache */
	mcr     p15, 0, r0, c7, c14, 0   /* clean and invalidate dcache */

	mcr     p15, 0, r0, c7, c10, 4   /* dsb                */
	mcr     p15, 0, r0, c7, c0, 4    /* wait for interrupt */

	mcr     p15, 0, r1, c1, c0, 0    /* restore d/i cache  */
	mcr     p15, 0, r0, c7, c5, 4    /* isb                */

	cpsie   f

	ldr     r0, =saved_state         /* restore registers */
	ldmfd   r0, {r4-r14}
	mov     r0, #0                   /* return power collapse failed */
	mov     pc, lr

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

	ldmdb   r1!, {r2}
	mcr	p15, 0, r2, c1, c0, 2	/* restore CACR */
#if defined(CONFIG_OPROFILE)
	ldmdb   r1!, {r2-r5}
	mcr     p15, 0, r3, c15, c12, 1 /* ccnt */
	mcr     p15, 0, r4, c15, c12, 2 /* pmn0 */
	mcr     p15, 0, r5, c15, c12, 3 /* pmn1 */
	mcr     p15, 0, r2, c15, c12, 0 /* pmnc */
#endif
	ldmdb   r1!, {r2-r5}
	mcr     p15, 0, r4, c3, c0, 0 /* dacr */
	mcr     p15, 0, r3, c2, c0, 0 /* ttb */
	mcr     p15, 0, r5, c13, c0, 1	/* context ID */
	mov     r0, #0
	mcr     p15, 0, r0, c7, c5, 4   /* isb */
	ldmdb   r1!, {r4-r14}

	/* Add 1:1 map in the PMD to allow smooth switch when turning on MMU */
	and     r3, r3, #~0x7F  /* mask off lower 7 bits of TTB */
	adr     r0, msm_pm_mapped_pa /* get address of the mapped instr */
	lsr     r1, r0, #20     /* get the addr range of addr in MB */
	lsl     r1, r1, #2      /* multiply by 4 to get to the pg index */
	add     r3, r3, r1      /* pgd + pgd_index(addr) */
	ldr     r1, [r3]        /* save current entry to r1 */
	lsr     r0, #20         /* align current addr to 1MB boundary */
	lsl     r0, #20
	/* Create new entry for this 1MB page */
	orr     r0, r0, #0x400   /* PMD_SECT_AP_WRITE */
	orr     r0, r0, #0x2     /* PMD_TYPE_SECT|PMD_DOMAIN(DOMAIN_KERNEL) */
	str     r0, [r3]         /* put new entry into the MMU table */
	mov     r0, #0
	mcr     p15, 0, r0, c7, c10, 4  /* dsb */
	mcr     p15, 0, r2, c1, c0, 0   /* MMU control */
	mcr     p15, 0, r0, c7, c5, 4   /* isb */
msm_pm_mapped_pa:
	/* Switch to virtual */
	adr     r2, msm_pm_pa_to_va
	ldr     r0, =msm_pm_pa_to_va
	mov     pc, r0
msm_pm_pa_to_va:
	sub     r0, r0, r2
	/* Restore r1 in MMU table */
	add     r3, r3, r0
	str     r1, [r3]

	mov     r0, #0
	mcr     p15, 0, r0, c7, c10, 0 /* flush entire data cache */
	mcr     p15, 0, r0, c7, c10, 4 /* dsb */
	mcr     p15, 0, r0, c7, c5, 4  /* isb */
	mcr     p15, 0, r0, c8, c7, 0  /* invalidate entire unified TLB */
	mcr     p15, 0, r0, c7, c5, 6  /* invalidate entire branch target
					* cache */
	mcr     p15, 0, r0, c7, c7, 0  /* invalidate both data and instruction
					* cache */
	mcr     p15, 0, r0, c7, c10, 4 /* dsb */
	mcr     p15, 0, r0, c7, c5, 4  /* isb */

	mov     r0, #1
	mov     pc, lr
	nop
	nop
	nop
	nop
	nop
1:	b       1b


	.data

saved_state:
	.space  4 * 11 /* r4-14 */
	.space  4 * 4  /* cp15 - MMU control, ttb, dacr, context ID */
#if defined(CONFIG_OPROFILE)
	.space  4 * 4  /* more cp15 - pmnc, ccnt, pmn0, pmn1 */
#endif
	.space  4	/* cacr */
saved_state_end:

