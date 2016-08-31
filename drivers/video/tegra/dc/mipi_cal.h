/*
 * drivers/video/tegra/dc/mipi_cal.h
 *
 * Copyright (c) 2012-2013, NVIDIA CORPORATION, All rights reserved.
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

#ifndef __DRIVERS_VIDEO_TEGRA_DC_MIPI_CAL_H__
#define __DRIVERS_VIDEO_TEGRA_DC_MIPI_CAL_H__

#include "mipi_cal_regs.h"

struct tegra_mipi_cal {
	struct tegra_dc *dc;
	struct resource *res;
	struct resource *base_res;
	struct clk *clk;
	struct clk *fixed_clk;
	void __iomem *base;
	struct mutex lock;
};

#ifdef CONFIG_TEGRA_MIPI_CAL
static inline void tegra_mipi_cal_clk_enable(struct tegra_mipi_cal *mipi_cal)
{
	BUG_ON(IS_ERR_OR_NULL(mipi_cal));
	clk_prepare_enable(mipi_cal->fixed_clk);
	clk_prepare_enable(mipi_cal->clk);
}

static inline void tegra_mipi_cal_clk_disable(struct tegra_mipi_cal *mipi_cal)
{
	BUG_ON(IS_ERR_OR_NULL(mipi_cal));
	clk_disable_unprepare(mipi_cal->clk);
	clk_disable_unprepare(mipi_cal->fixed_clk);
}

/* reg is word offset */
static inline unsigned long tegra_mipi_cal_read(
					struct tegra_mipi_cal *mipi_cal,
					unsigned long reg)
{
	BUG_ON(IS_ERR_OR_NULL(mipi_cal) ||
		!tegra_is_clk_enabled(mipi_cal->clk));
	return readl(mipi_cal->base + reg);
}

/* reg is word offset */
static inline void tegra_mipi_cal_write(struct tegra_mipi_cal *mipi_cal,
							unsigned long val,
							unsigned long reg)
{
	BUG_ON(IS_ERR_OR_NULL(mipi_cal) ||
		!tegra_is_clk_enabled(mipi_cal->clk));
	writel(val, mipi_cal->base + reg);
}

extern struct tegra_mipi_cal *tegra_mipi_cal_init_sw(struct tegra_dc *dc);
extern int tegra_mipi_cal_init_hw(struct tegra_mipi_cal *mipi_cal);
extern void tegra_mipi_cal_destroy(struct tegra_dc *dc);
#else
static inline void tegra_mipi_cal_clk_enable(struct tegra_mipi_cal *mipi_cal)
{
	/* dummy */
}

static inline void tegra_mipi_cal_clk_disable(struct tegra_mipi_cal *mipi_cal)
{
	/* dummy */
}

static inline unsigned long tegra_mipi_cal_read(
						struct tegra_mipi_cal *mipi_cal,
						unsigned long reg)
{
	/* dummy */
	return 0;
}

static inline void tegra_mipi_cal_write(struct tegra_mipi_cal *mipi_cal,
						unsigned long val,
						unsigned long reg)
{
	/* dummy */
}

struct tegra_mipi_cal *tegra_mipi_cal_init_sw(struct tegra_dc *dc)
{
	/* dummy */
	return NULL;
}

int tegra_mipi_cal_init_hw(struct tegra_mipi_cal *mipi_cal)
{
	/* dummy */
	return 0;
}

void tegra_mipi_cal_destroy(struct tegra_dc *dc)
{
	/* dummy */
}
#endif
#endif
