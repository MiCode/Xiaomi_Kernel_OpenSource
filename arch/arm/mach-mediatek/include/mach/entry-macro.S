/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/irqchip/arm-gic.h>
#include <mach/irqs.h>

/* leave disable_fiq as an empty macro */
.macro disable_fiq
.endm

.macro  get_irqnr_preamble, base, tmp
    ldr \base, =GIC_CPU_BASE
    ldr \base, [\base]
.endm

.macro  arch_ret_to_user, tmp1, tmp2
.endm

.macro get_irqnr_and_base, irqnr, irqstat, base, tmp
#ifdef CONFIG_MTK_KERNEL_IN_SECURE_MODE
    ldr \irqstat, [\base, #GIC_CPU_AIAR] /* bits 12-10 = src CPU, 9-0 = int # */
#else
    ldr \irqstat, [\base, #GIC_CPU_INTACK] /* bits 12-10 = src CPU, 9-0 = int # */
#endif
    ldr \tmp, =NR_IRQS
    bic \irqnr, \irqstat, #0x1c00

    /* if (irqnr >= NR_IRQS) return NO_IRQ (0) */
    cmp \irqnr, \tmp
    movcs \tmp, #0
    bcs BSYM(702f)

    /* if (irqnr >= 27) return HAVE_IRQ (1) */
    cmp \irqnr, #(27)
    movcs \tmp, #1
    bcs BSYM(702f)

    /* otherwise, return NO_IRQ (0) */
    mov \tmp, #0

702:
    cmp \tmp, #0
    cmpeq \irqnr, \irqnr
    .endm

.macro test_for_ipi, irqnr, irqstat, base, tmp
    bic \irqnr, \irqstat, #0x1c00
    cmp \irqnr, #16
#ifdef CONFIG_MTK_KERNEL_IN_SECURE_MODE
    strcc \irqstat, [\base, #GIC_CPU_AEOI]
#else
    strcc \irqstat, [\base, #GIC_CPU_EOI]
#endif
    cmpcs \irqnr, \irqnr
.endm

.macro test_for_ltirq, irqnr, irqstat, base, tmp
    bic     \irqnr, \irqstat, #0x1c00
    mov     \tmp, #0
    cmp     \irqnr, #29
    moveq   \tmp, #1
#ifdef  CONFIG_FIQ
    streq   \irqstat, [\base, #GIC_CPU_AEOI]
#else
    streq   \irqstat, [\base, #GIC_CPU_EOI]
#endif
    cmp     \tmp, #0
.endm
.macro test_for_wdtirq, irqnr, irqstat, base, tmp
    bic     \irqnr, \irqstat, #0x1c00
    mov     \tmp, #0
    cmp     \irqnr, #30
    moveq   \tmp, #1
#ifdef CONFIG_MTK_KERNEL_IN_SECURE_MODE
    streq   \irqstat, [\base, #GIC_CPU_AEOI]
#else
    streq   \irqstat, [\base, #GIC_CPU_EOI]
#endif
    cmp     \tmp, #0
.endm

