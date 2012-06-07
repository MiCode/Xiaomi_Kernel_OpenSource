/*
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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

#ifndef __ASM_ARCH_MSM_UNCOMPRESS_H
#define __ASM_ARCH_MSM_UNCOMPRESS_H

#include <linux/io.h>
#include <asm/mach-types.h>
#include <asm/processor.h>
#include <mach/msm_iomap.h>
#include <mach/msm_serial_hsl_regs.h>

#define UART_CSR      (*(volatile uint32_t *)(MSM_DEBUG_UART_PHYS + 0x08))
#define UART_TF       (*(volatile uint32_t *)(MSM_DEBUG_UART_PHYS + 0x0c))

#define UART_DM_SR    (*((volatile uint32_t *)(MSM_DEBUG_UART_PHYS + 0x08)))
#define UART_DM_CR    (*((volatile uint32_t *)(MSM_DEBUG_UART_PHYS + 0x10)))
#define UART_DM_ISR   (*((volatile uint32_t *)(MSM_DEBUG_UART_PHYS + 0x14)))
#define UART_DM_NCHAR (*((volatile uint32_t *)(MSM_DEBUG_UART_PHYS + 0x40)))
#define UART_DM_TF    (*((volatile uint32_t *)(MSM_DEBUG_UART_PHYS + 0x70)))

#ifndef CONFIG_DEBUG_ICEDCC
static void putc(int c)
{
#if defined(MSM_DEBUG_UART_PHYS)
	unsigned long base = MSM_DEBUG_UART_PHYS;

#ifdef CONFIG_MSM_HAS_DEBUG_UART_HS
	/*
	 * Wait for TX_READY to be set; but skip it if we have a
	 * TX underrun.
	 */
	if (!(__raw_readl_no_log(base + UARTDM_SR_OFFSET) & 0x08))
		while (!(__raw_readl_no_log(base + UARTDM_ISR_OFFSET) & 0x80))
			cpu_relax();

	__raw_writel_no_log(0x300, base + UARTDM_CR_OFFSET);
	__raw_writel_no_log(0x1, base + UARTDM_NCF_TX_OFFSET);
	__raw_writel_no_log(c, base + UARTDM_TF_OFFSET);
#else
	/* Wait for TX_READY to be set */
	while (!(__raw_readl_no_log(base + 0x08) & 0x04))
		cpu_relax();
	__raw_writel_no_log(c, base + 0x0c);
#endif
#endif
}
#endif

static inline void flush(void)
{
}

static inline void arch_decomp_setup(void)
{
}

static inline void arch_decomp_wdog(void)
{
}

#endif
