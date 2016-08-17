/*
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __POWERGATE_PRIV_H__
#define __POWERGATE_PRIV_H__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/spinlock.h>
#include <linux/io.h>

#include <mach/clk.h>
#include <mach/iomap.h>
#include <mach/powergate.h>

#include "clock.h"
#include "fuse.h"

#define MAX_CLK_EN_NUM			9
#define MAX_HOTRESET_CLIENT_NUM		4

#define PWRGATE_CLAMP_STATUS	0x2c
#define PWRGATE_TOGGLE		0x30
#define PWRGATE_TOGGLE_START	(1 << 8)
#define REMOVE_CLAMPING		0x34
#define PWRGATE_STATUS		0x38

/* MC register read/write */
static void __iomem *mc = IO_ADDRESS(TEGRA_MC_BASE);
static inline u32 mc_read(unsigned long reg)
{
	return readl(mc + reg);
}

static inline void mc_write(u32 val, unsigned long reg)
{
	writel_relaxed(val, mc + reg);
}

/* PMC register read/write */
static void __iomem *pmc = IO_ADDRESS(TEGRA_PMC_BASE);
static inline u32 pmc_read(unsigned long reg)
{
	return readl(pmc + reg);
}

static inline void pmc_write(u32 val, unsigned long reg)
{
	writel_relaxed(val, pmc + reg);
}

enum clk_type {
	CLK_AND_RST,
	RST_ONLY,
	CLK_ONLY,
};

struct partition_clk_info {
	const char *clk_name;
	enum clk_type clk_type;
	struct clk *clk_ptr;
};

struct powergate_partition_info {
	const char *name;
	struct partition_clk_info clk_info[MAX_CLK_EN_NUM];
};

struct powergate_ops {
	const char *soc_name;

	int num_powerdomains;
	int num_cpu_domains;
	u8 *cpu_domains;

	spinlock_t *(*get_powergate_lock)(void);

	const char *(*get_powergate_domain_name)(int id);

	int (*powergate_partition)(int);
	int (*unpowergate_partition)(int id);

	int (*powergate_partition_with_clk_off)(int);
	int (*unpowergate_partition_with_clk_on)(int);

	int (*powergate_mc_enable)(int id);
	int (*powergate_mc_disable)(int id);

	int (*powergate_mc_flush)(int id);
	int (*powergate_mc_flush_done)(int id);

	int (*powergate_init_refcount)(void);
	bool (*powergate_check_clamping)(int id);
};

void get_clk_info(struct powergate_partition_info *pg_info);
int tegra_powergate_remove_clamping(int id);
int partition_clk_enable(struct powergate_partition_info *pg_info);
void partition_clk_disable(struct powergate_partition_info *pg_info);
int is_partition_clk_disabled(struct powergate_partition_info *pg_info);
void powergate_partition_deassert_reset(struct powergate_partition_info *pg_info);
void powergate_partition_assert_reset(struct powergate_partition_info *pg_info);
int tegra_powergate_reset_module(struct powergate_partition_info *pg_info);
int powergate_module(int id);
int unpowergate_module(int id);
int tegra_powergate_set(int id, bool new_state);

/* INIT APIs: New SoC needs to add its support here */
#if defined(CONFIG_ARCH_TEGRA_2x_SOC)
struct powergate_ops *tegra2_powergate_init_chip_support(void);
#else
static inline struct powergate_ops *tegra2_powergate_init_chip_support(void)
{
	return NULL;
}
#endif

#if defined(CONFIG_ARCH_TEGRA_3x_SOC)
struct powergate_ops *tegra3_powergate_init_chip_support(void);
#else
static inline struct powergate_ops *tegra3_powergate_init_chip_support(void)
{
	return NULL;
}
#endif

#if defined(CONFIG_ARCH_TEGRA_11x_SOC)
struct powergate_ops *tegra11x_powergate_init_chip_support(void);
#else
static inline struct powergate_ops *tegra11x_powergate_init_chip_support(void)
{
	return NULL;
}
#endif

#if defined(CONFIG_ARCH_TEGRA_14x_SOC)
struct powergate_ops *tegra14x_powergate_init_chip_support(void);
#else
static inline struct powergate_ops *tegra14x_powergate_init_chip_support(void)
{
	return NULL;
}
#endif

#endif /* __POWERGATE_PRIV_H__ */
