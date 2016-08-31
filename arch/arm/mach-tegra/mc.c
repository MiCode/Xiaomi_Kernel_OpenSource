/*
 * arch/arm/mach-tegra/mc.c
 *
 * Copyright (C) 2010 Google, Inc.
 * Copyright (C) 2011-2013, NVIDIA Corporation.  All rights reserved.
 *
 * Author:
 *	Erik Gilling <konkers@google.com>
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/export.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/tegra-soc.h>

#include <mach/mc.h>
#include <mach/mcerr.h>

#include "iomap.h"

#define MC_CLIENT_HOTRESET_CTRL		0x200
#define MC_CLIENT_HOTRESET_STAT		0x204
#define MC_CLIENT_HOTRESET_CTRL_1	0x970
#define MC_CLIENT_HOTRESET_STAT_1	0x974

#define MC_TIMING_REG_NUM1					\
	((MC_EMEM_ARB_TIMING_W2R - MC_EMEM_ARB_CFG) / 4 + 1)
#define MC_TIMING_REG_NUM2					\
	((MC_EMEM_ARB_MISC1 - MC_EMEM_ARB_DA_TURNS) / 4 + 1)
#if defined(CONFIG_ARCH_TEGRA_12x_SOC)
#define MC_TIMING_REG_NUM3	T12X_MC_LATENCY_ALLOWANCE_NUM_REGS
#else
#define MC_TIMING_REG_NUM3						\
	((MC_LATENCY_ALLOWANCE_VI_2 - MC_LATENCY_ALLOWANCE_BASE) / 4 + 1)
#endif

static DEFINE_SPINLOCK(tegra_mc_lock);
void __iomem *mc = (void __iomem *)IO_ADDRESS(TEGRA_MC_BASE);
#ifdef MC_DUAL_CHANNEL
void __iomem *mc1 = (void __iomem *)IO_ADDRESS(TEGRA_MC1_BASE);
#endif

#ifdef CONFIG_PM_SLEEP
static u32 mc_boot_timing[MC_TIMING_REG_NUM1 + MC_TIMING_REG_NUM2
			  + MC_TIMING_REG_NUM3 + 4];

static void tegra_mc_timing_save(void)
{
	u32 off;
	u32 *ctx = mc_boot_timing;

	for (off = MC_EMEM_ARB_CFG; off <= MC_EMEM_ARB_TIMING_W2R; off += 4)
		*ctx++ = mc_readl(off);

	for (off = MC_EMEM_ARB_DA_TURNS; off <= MC_EMEM_ARB_MISC1; off += 4)
		*ctx++ = mc_readl(off);

	*ctx++ = mc_readl(MC_EMEM_ARB_RING3_THROTTLE);
	*ctx++ = mc_readl(MC_EMEM_ARB_OVERRIDE);
	*ctx++ = mc_readl(MC_RESERVED_RSV);

#if defined(CONFIG_ARCH_TEGRA_12x_SOC)
	tegra12_mc_latency_allowance_save(&ctx);
#else
	for (off = MC_LATENCY_ALLOWANCE_BASE; off <= MC_LATENCY_ALLOWANCE_VI_2;
		off += 4)
		*ctx++ = mc_readl(off);
#endif

	*ctx++ = mc_readl(MC_INT_MASK);
}

void tegra_mc_timing_restore(void)
{
	u32 off;
	u32 *ctx = mc_boot_timing;

	for (off = MC_EMEM_ARB_CFG; off <= MC_EMEM_ARB_TIMING_W2R; off += 4)
		__mc_raw_writel(0, *ctx++, off);

	for (off = MC_EMEM_ARB_DA_TURNS; off <= MC_EMEM_ARB_MISC1; off += 4)
		__mc_raw_writel(0, *ctx++, off);

	__mc_raw_writel(0, *ctx++, MC_EMEM_ARB_RING3_THROTTLE);
	__mc_raw_writel(0, *ctx++, MC_EMEM_ARB_OVERRIDE);
	__mc_raw_writel(0, *ctx++, MC_RESERVED_RSV);

#if defined(CONFIG_ARCH_TEGRA_12x_SOC)
	tegra12_mc_latency_allowance_restore(&ctx);
#else
	for (off = MC_LATENCY_ALLOWANCE_BASE; off <= MC_LATENCY_ALLOWANCE_VI_2;
		off += 4)
		__mc_raw_writel(0, *ctx++, off);
#endif

	mc_writel(*ctx++, MC_INT_MASK);
	off = mc_readl(MC_INT_MASK);

	mc_writel(0x1, MC_TIMING_CONTROL);
	off = mc_readl(MC_TIMING_CONTROL);
#if defined(CONFIG_ARCH_TEGRA_3x_SOC)
	/* Bug 1059264
	 * Set extra snap level to avoid VI starving and dropping data.
	 */
	mc_writel(1, MC_VE_EXTRA_SNAP_LEVELS);
#endif
}
#else
#define tegra_mc_timing_save()
#endif

/*
 * If using T30/DDR3, the 2nd 16 bytes part of DDR3 atom is 2nd line and is
 * discarded in tiling mode.
 */
int tegra_mc_get_tiled_memory_bandwidth_multiplier(void)
{
	int type;

	type = tegra_emc_get_dram_type();

	if (type == DRAM_TYPE_DDR3)
		return 2;
	else
		return 1;
}

/* API to get EMC freq to be requested, for Bandwidth.
 * bw_kbps: BandWidth passed is in KBps.
 * returns freq in KHz
 */
unsigned int tegra_emc_bw_to_freq_req(unsigned int bw_kbps)
{
	unsigned int freq;
	unsigned int bytes_per_emc_clk;

	bytes_per_emc_clk = tegra_mc_get_effective_bytes_width() * 2;
	freq = (bw_kbps + bytes_per_emc_clk - 1) / bytes_per_emc_clk *
		CONFIG_TEGRA_EMC_TO_DDR_CLOCK;
	return freq;
}
EXPORT_SYMBOL_GPL(tegra_emc_bw_to_freq_req);

/* API to get EMC bandwidth, for freq that can be requested.
 * freq_khz: Frequency passed is in KHz.
 * returns bandwidth in KBps
 */
unsigned int tegra_emc_freq_req_to_bw(unsigned int freq_khz)
{
	unsigned int bw;
	unsigned int bytes_per_emc_clk;

	bytes_per_emc_clk = tegra_mc_get_effective_bytes_width() * 2;
	bw = freq_khz * bytes_per_emc_clk / CONFIG_TEGRA_EMC_TO_DDR_CLOCK;
	return bw;
}
EXPORT_SYMBOL_GPL(tegra_emc_freq_req_to_bw);

#define HOTRESET_READ_COUNT	5
static bool tegra_stable_hotreset_check(u32 stat_reg, u32 *stat)
{
	int i;
	u32 cur_stat;
	u32 prv_stat;
	unsigned long flags;

	spin_lock_irqsave(&tegra_mc_lock, flags);
	prv_stat = mc_readl(stat_reg);
	for (i = 0; i < HOTRESET_READ_COUNT; i++) {
		cur_stat = mc_readl(stat_reg);
		if (cur_stat != prv_stat) {
			spin_unlock_irqrestore(&tegra_mc_lock, flags);
			return false;
		}
	}
	*stat = cur_stat;
	spin_unlock_irqrestore(&tegra_mc_lock, flags);
	return true;
}

int tegra_mc_flush(int id)
{
	u32 rst_ctrl, rst_stat;
	u32 rst_ctrl_reg, rst_stat_reg;
	unsigned long flags;
	bool ret;

	if (id < 32) {
		rst_ctrl_reg = MC_CLIENT_HOTRESET_CTRL;
		rst_stat_reg = MC_CLIENT_HOTRESET_STAT;
	} else {
		id %= 32;
		rst_ctrl_reg = MC_CLIENT_HOTRESET_CTRL_1;
		rst_stat_reg = MC_CLIENT_HOTRESET_STAT_1;
	}

	spin_lock_irqsave(&tegra_mc_lock, flags);

	rst_ctrl = mc_readl(rst_ctrl_reg);
	rst_ctrl |= (1 << id);
	mc_writel(rst_ctrl, rst_ctrl_reg);

	spin_unlock_irqrestore(&tegra_mc_lock, flags);

	do {
		udelay(10);
		rst_stat = 0;
		ret = tegra_stable_hotreset_check(rst_stat_reg, &rst_stat);
		if (!ret)
			continue;
	} while (!(rst_stat & (1 << id)));

	return 0;
}
EXPORT_SYMBOL(tegra_mc_flush);

int tegra_mc_flush_done(int id)
{
	u32 rst_ctrl;
	u32 rst_ctrl_reg, rst_stat_reg;
	unsigned long flags;

	if (id < 32) {
		rst_ctrl_reg = MC_CLIENT_HOTRESET_CTRL;
		rst_stat_reg = MC_CLIENT_HOTRESET_STAT;
	} else {
		id %= 32;
		rst_ctrl_reg = MC_CLIENT_HOTRESET_CTRL_1;
		rst_stat_reg = MC_CLIENT_HOTRESET_STAT_1;
	}

	spin_lock_irqsave(&tegra_mc_lock, flags);

	rst_ctrl = mc_readl(rst_ctrl_reg);
	rst_ctrl &= ~(1 << id);
	mc_writel(rst_ctrl, rst_ctrl_reg);

	spin_unlock_irqrestore(&tegra_mc_lock, flags);

	return 0;
}
EXPORT_SYMBOL(tegra_mc_flush_done);

/*
 * MC driver init.
 */
static int __init tegra_mc_init(void)
{
	u32 reg;
	struct dentry *mc_debugfs_dir;

	tegra_mc_timing_save();

#if defined(CONFIG_ARCH_TEGRA_3x_SOC)
	reg = 0x0f7f1010;
	mc_writel(reg, MC_RESERVED_RSV);
#endif

#if defined(CONFIG_TEGRA_MC_EARLY_ACK)
	reg = mc_readl(MC_EMEM_ARB_OVERRIDE);
	reg |= 3;
#if defined(CONFIG_TEGRA_ERRATA_1157520)
	if (tegra_revision == TEGRA_REVISION_A01)
		reg &= ~2;
#endif
	mc_writel(reg, MC_EMEM_ARB_OVERRIDE);
#endif

	mc_debugfs_dir = debugfs_create_dir("mc", NULL);
	if (mc_debugfs_dir == NULL) {
		pr_err("Failed to make debugfs node: %ld\n",
		       PTR_ERR(mc_debugfs_dir));
		return PTR_ERR(mc_debugfs_dir);
	}

	tegra_mcerr_init(mc_debugfs_dir);

	return 0;
}
arch_initcall(tegra_mc_init);
