/*
 * arch/arm/mach-tegra/include/mach/clk.h
 *
 * Copyright (C) 2010 Google, Inc.
 *
 * Author:
 *	Erik Gilling <konkers@google.com>
 *
 * Copyright (C) 2010-2012 NVIDIA Corporation
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

#ifndef __MACH_CLK_H
#define __MACH_CLK_H

struct clk;
struct dvfs;
struct notifier_block;

enum tegra_clk_ex_param {
	TEGRA_CLK_VI_INP_SEL,
	TEGRA_CLK_DTV_INVERT,
	TEGRA_CLK_NAND_PAD_DIV2_ENB,
	TEGRA_CLK_PLLD_CSI_OUT_ENB,
	TEGRA_CLK_PLLD_DSI_OUT_ENB,
	TEGRA_CLK_PLLD_MIPI_MUX_SEL,
	TEGRA_CLK_DFLL_LOCK,
};

void tegra_periph_reset_deassert(struct clk *c);
void tegra_periph_reset_assert(struct clk *c);

int tegra_dvfs_set_rate(struct clk *c, unsigned long rate);
int tegra_dvfs_override_core_voltage(int override_mv);
unsigned long clk_get_rate_all_locked(struct clk *c);

#ifdef CONFIG_ARCH_TEGRA_2x_SOC
void tegra2_sdmmc_tap_delay(struct clk *c, int delay);
#else
static inline void tegra2_sdmmc_tap_delay(struct clk *c, int delay)
{
}
#endif

#ifdef CONFIG_ARCH_TEGRA_3x_SOC
int tegra_emc_enable_eack(void);
int tegra_emc_disable_eack(void);
#else
static inline int tegra_emc_enable_eack(void) {
	return 0;
}

static inline int tegra_emc_disable_eack(void) {
	return 0;
}
#endif
int tegra_dvfs_rail_disable_by_name(const char *reg_id);
int tegra_clk_cfg_ex(struct clk *c, enum tegra_clk_ex_param p, u32 setting);
int tegra_register_clk_rate_notifier(struct clk *c, struct notifier_block *nb);
void tegra_unregister_clk_rate_notifier(
	struct clk *c, struct notifier_block *nb);

/**
 * tegra_is_clk_enabled - get info if the clk is enabled or not
 * @clk: clock source
 *
 * Returns refcnt.
 */
int tegra_is_clk_enabled(struct clk *clk);

void tegra_cpu_user_cap_set(unsigned int speed_khz);

#endif
