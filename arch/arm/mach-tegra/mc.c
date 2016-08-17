/*
 * arch/arm/mach-tegra/mc.c
 *
 * Copyright (C) 2010 Google, Inc.
 * Copyright (C) 2011-2013 NVIDIA Corporation. All rights reserved.
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

#include <linux/export.h>
#include <linux/io.h>
#include <linux/spinlock.h>

#include <mach/iomap.h>
#include <mach/mc.h>

#if defined(CONFIG_ARCH_TEGRA_2x_SOC)
static DEFINE_SPINLOCK(tegra_mc_lock);

void tegra_mc_set_priority(unsigned long client, unsigned long prio)
{
	void __iomem *mc_base = (void __iomem *)IO_TO_VIRT(TEGRA_MC_BASE);
	unsigned long reg = client >> 8;
	int field = client & 0xff;
	unsigned long val;
	unsigned long flags;

	spin_lock_irqsave(&tegra_mc_lock, flags);
	val = readl(mc_base + reg);
	val &= ~(TEGRA_MC_PRIO_MASK << field);
	val |= prio << field;
	writel(val, mc_base + reg);
	spin_unlock_irqrestore(&tegra_mc_lock, flags);

}

int tegra_mc_get_tiled_memory_bandwidth_multiplier(void)
{
	return 1;
}

#else
	/* !!!FIXME!!! IMPLEMENT tegra_mc_set_priority() */

#include "tegra3_emc.h"

# if defined(CONFIG_ARCH_TEGRA_11x_SOC)
/* T11x has big line buffers for rotation */
int tegra_mc_get_tiled_memory_bandwidth_multiplier(void)
{
	return 1;
}
# else
/*
 * If using T30/DDR3, the 2nd 16 bytes part of DDR3 atom is 2nd line and is
 * discarded in tiling mode.
 */
int tegra_mc_get_tiled_memory_bandwidth_multiplier(void)
{
	int type;

	type = tegra_emc_get_dram_type();
	WARN_ONCE(type == -1, "unknown type DRAM because DVFS is disabled\n");

	if (type == DRAM_TYPE_DDR3)
		return 2;
	else
		return 1;
}
# endif
#endif

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
