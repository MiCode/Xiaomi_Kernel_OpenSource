/*
 * arch/arm/mach-tegra/include/mach/mc.h
 *
 * Copyright (C) 2010-2012 Google, Inc.
 * Copyright (C) 2013, NVIDIA Corporation.  All rights reserved.
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

#ifndef __MACH_TEGRA_MC_H
#define __MACH_TEGRA_MC_H

/* !!!FIXME!!! IMPLEMENT ME */
#define tegra_mc_set_priority(client, prio) \
	do { /* nothing for now */ } while (0)

/*
 * Number of unique interrupts we have for this chip.
 */
#if defined(CONFIG_ARCH_TEGRA_11x_SOC)
#define INTR_COUNT	6
#elif defined(CONFIG_ARCH_TEGRA_14x_SOC)
#define INTR_COUNT	8
#elif defined(CONFIG_ARCH_TEGRA_12x_SOC)
#define INTR_COUNT	8
#else
#define INTR_COUNT	4
#endif

struct mc_client {
	const char *name;
	const char *swgid;
	unsigned int intr_counts[INTR_COUNT];
};

#ifdef CONFIG_ARCH_TEGRA_11x_SOC
#define MC_DUAL_CHANNEL
#endif

extern void __iomem *mc;
#ifdef MC_DUAL_CHANNEL
extern void __iomem *mc1;
#endif

#include <linux/io.h>
#include <linux/debugfs.h>

/*
 * Read and write functions for hitting the MC. mc_ind corresponds to the MC
 * you wish to write to: 0 -> MC0, 1 -> MC1. If a chip does not have a
 * secondary MC then reads/writes to said MC are silently dropped.
 */
static inline u32 __mc_readl(int mc_ind, u32 reg)
{
	if (!mc_ind)
		return readl(mc + reg);
#ifdef MC_DUAL_CHANNEL
	else
		return readl(mc1 + reg);
#endif
	return 0;
}

static inline void __mc_writel(int mc_ind, u32 val, u32 reg)
{
	if (!mc_ind)
		writel(val, mc + reg);
#ifdef MC_DUAL_CHANNEL
	else
		writel(val, mc1 + reg);
#endif
}

static inline u32 __mc_raw_readl(int mc_ind, u32 reg)
{
	if (!mc_ind)
		return __raw_readl(mc + reg);
#ifdef MC_DUAL_CHANNEL
	else
		return __raw_readl(mc1 + reg);
#endif
	return 0;
}

static inline void __mc_raw_writel(int mc_ind, u32 val, u32 reg)
{
	if (!mc_ind)
		__raw_writel(val, mc + reg);
#ifdef MC_DUAL_CHANNEL
	else
		__raw_writel(val, mc1 + reg);
#endif
}

#define mc_readl(reg)       __mc_readl(0, reg)
#define mc_writel(val, reg) __mc_writel(0, val, reg)

int tegra_mc_get_tiled_memory_bandwidth_multiplier(void);

/*
 * Tegra11 has dual 32-bit memory channels, while
 * Tegra12 has single 64-bit memory channel.
 * MC effectively operates as 64-bit bus.
 */
static inline int tegra_mc_get_effective_bytes_width(void)
{
#if defined(CONFIG_ARCH_TEGRA_12x_SOC) || \
	defined(CONFIG_ARCH_TEGRA_11x_SOC)
	return 8;
#else
	return 4;
#endif
}

unsigned int tegra_emc_bw_to_freq_req(unsigned int bw_kbps);
unsigned int tegra_emc_freq_req_to_bw(unsigned int freq_kbps);
#if defined(CONFIG_ARCH_TEGRA_12x_SOC)
void         tegra12_mc_latency_allowance_save(u32 **pctx);
void         tegra12_mc_latency_allowance_restore(u32 **pctx);
#endif

/* API to get freqency switch latency at given MC freq.
 * freq_khz: Frequncy in KHz.
 * retruns latency in microseconds.
 */
static inline unsigned tegra_emc_dvfs_latency(unsigned int freq_khz)
{
	/* The latency data is not available based on freq.
	 * H/W expects it to be around 3 to 4us.
	 */
	return 4;
}

#define TEGRA_MC_CLIENT_AFI		0
#define TEGRA_MC_CLIENT_DC		2
#define TEGRA_MC_CLIENT_DCB		3
#define TEGRA_MC_CLIENT_EPP		4
#define TEGRA_MC_CLIENT_G2		5
#define TEGRA_MC_CLIENT_ISP		8
#define TEGRA_MC_CLIENT_MSENC		11
#define TEGRA_MC_CLIENT_MPE		11
#define TEGRA_MC_CLIENT_NV		12
#define TEGRA_MC_CLIENT_SATA		15
#define TEGRA_MC_CLIENT_VDE		16
#define TEGRA_MC_CLIENT_VI		17
#define TEGRA_MC_CLIENT_VIC		18
#define TEGRA_MC_CLIENT_XUSB_HOST	19
#define TEGRA_MC_CLIENT_XUSB_DEV	20
#define TEGRA_MC_CLIENT_TSEC		22
#define TEGRA_MC_CLIENT_ISPB		33
#define TEGRA_MC_CLIENT_GPU		34

int tegra_mc_flush(int id);
int tegra_mc_flush_done(int id);

#endif
