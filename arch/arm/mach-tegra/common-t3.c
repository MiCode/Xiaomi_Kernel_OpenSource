/*
 * arch/arm/mach-tegra/common-t3.c
 *
 * Tegra 3 SoC-specific initialization.
 *
 * Copyright (c) 2009-2012 NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/kernel.h>
#include <linux/io.h>

#include <mach/iomap.h>

#include "mcerr.h"

#define MC_TIMING_REG_NUM1					\
	((MC_EMEM_ARB_TIMING_W2R - MC_EMEM_ARB_CFG) / 4 + 1)
#define MC_TIMING_REG_NUM2					\
	((MC_EMEM_ARB_MISC1 - MC_EMEM_ARB_DA_TURNS) / 4 + 1)
#define MC_TIMING_REG_NUM3						\
	((MC_LATENCY_ALLOWANCE_VI_2 - MC_LATENCY_ALLOWANCE_BASE) / 4 + 1)

#ifdef CONFIG_PM_SLEEP
static u32 mc_boot_timing[MC_TIMING_REG_NUM1 + MC_TIMING_REG_NUM2
			  + MC_TIMING_REG_NUM3 + 4];

static void tegra_mc_timing_save(void)
{
	u32 off;
	u32 *ctx = mc_boot_timing;

	for (off = MC_EMEM_ARB_CFG; off <= MC_EMEM_ARB_TIMING_W2R; off += 4)
		*ctx++ = readl(mc + off);

	for (off = MC_EMEM_ARB_DA_TURNS; off <= MC_EMEM_ARB_MISC1; off += 4)
		*ctx++ = readl(mc + off);

	*ctx++ = readl(mc + MC_EMEM_ARB_RING3_THROTTLE);
	*ctx++ = readl(mc + MC_EMEM_ARB_OVERRIDE);
	*ctx++ = readl(mc + MC_RESERVED_RSV);

	for (off = MC_LATENCY_ALLOWANCE_BASE; off <= MC_LATENCY_ALLOWANCE_VI_2;
		off += 4)
		*ctx++ = readl((u32)mc + off);

	*ctx++ = readl((u32)mc + MC_INT_MASK);
}

void tegra_mc_timing_restore(void)
{
	u32 off;
	u32 *ctx = mc_boot_timing;

	for (off = MC_EMEM_ARB_CFG; off <= MC_EMEM_ARB_TIMING_W2R; off += 4)
		__raw_writel(*ctx++, mc + off);

	for (off = MC_EMEM_ARB_DA_TURNS; off <= MC_EMEM_ARB_MISC1; off += 4)
		__raw_writel(*ctx++, mc + off);

	__raw_writel(*ctx++, mc + MC_EMEM_ARB_RING3_THROTTLE);
	__raw_writel(*ctx++, mc + MC_EMEM_ARB_OVERRIDE);
	__raw_writel(*ctx++, mc + MC_RESERVED_RSV);

	for (off = MC_LATENCY_ALLOWANCE_BASE; off <= MC_LATENCY_ALLOWANCE_VI_2;
		off += 4)
		__raw_writel(*ctx++, (u32)mc + off);

	writel(*ctx++, (u32)mc + MC_INT_MASK);
	off = readl((u32)mc + MC_INT_MASK);

	writel(0x1, mc + MC_TIMING_CONTROL);
	off = readl(mc + MC_TIMING_CONTROL);
#if defined(CONFIG_ARCH_TEGRA_3x_SOC)
	/* Bug 1059264
	 * Set extra snap level to avoid VI starving and dropping data.
	 */
	writel(1, mc + MC_VE_EXTRA_SNAP_LEVELS);
#endif
}
#else
#define tegra_mc_timing_save()
#endif

static int __init tegra_mc_timing_init(void)
{
	tegra_mc_timing_save();
	return 0;
}
arch_initcall(tegra_mc_timing_init);
