/* arch/arm/mach-msm/include/mach/uncompress.h
 *
 * Copyright (C) 2007 Google, Inc.
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

bool msm_serial_hsl;

#ifndef CONFIG_DEBUG_ICEDCC
static void putc(int c)
{
#if defined(MSM_DEBUG_UART_PHYS)
	unsigned long base = MSM_DEBUG_UART_PHYS;

	if (msm_serial_hsl) {
		/*
		 * Wait for TX_READY to be set; but skip it if we have a
		 * TX underrun.
		 */
		if (__raw_readl(base + 0x08) & 0x08)
			while (!(__raw_readl(base + 0x14) & 0x80))
				cpu_relax();

		__raw_writel(0x300, base + 0x10);
		__raw_writel(0x1, base + 0x40);
		__raw_writel(c, base + 0x70);

	} else {
		/* Wait for TX_READY to be set */
		while (!(__raw_readl(base + 0x08) & 0x04))
			cpu_relax();
		__raw_writel(c, base + 0x0c);
	}
#endif
}
#endif

static inline void flush(void)
{
}

#define DEBUG_LL_HS_ENTRY(machine)		\
	if (machine_is_##machine()) {		\
		msm_serial_hsl = true;		\
		break;				\
	}

static inline void arch_decomp_setup(void)
{
	do {
		DEBUG_LL_HS_ENTRY(msm8x60_fluid);
		DEBUG_LL_HS_ENTRY(msm8x60_surf);
		DEBUG_LL_HS_ENTRY(msm8x60_ffa);
		DEBUG_LL_HS_ENTRY(msm8x60_fusion);
		DEBUG_LL_HS_ENTRY(msm8x60_fusn_ffa);
		DEBUG_LL_HS_ENTRY(msm8x60_qrdc);
		DEBUG_LL_HS_ENTRY(msm8x60_qt);
		DEBUG_LL_HS_ENTRY(msm8960_cdp);
		DEBUG_LL_HS_ENTRY(msm8960_mtp);
		DEBUG_LL_HS_ENTRY(msm8960_fluid);
		DEBUG_LL_HS_ENTRY(msm8960_apq);
		DEBUG_LL_HS_ENTRY(msm8960_liquid);
	} while (0);
}

static inline void arch_decomp_wdog(void)
{
}

#endif
