/*
 * arch/arm/mach-tegra/include/mach/pm_domains.h
 *
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
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

#ifndef _MACH_TEGRA_PM_DOMAINS_H_
#define _MACH_TEGRA_PM_DOMAINS_H_

#include <linux/clk.h>
#include <linux/pm_domain.h>

struct tegra_pm_domain {
	struct generic_pm_domain gpd;
	struct clk *clk;
};

#define to_tegra_pd(_pd) container_of(_pd, struct tegra_pm_domain, gpd);

int tegra_dma_restore(void);
int tegra_dma_save(void);
int tegra_actmon_save(void);
int tegra_actmon_restore(void);

#ifdef CONFIG_TEGRA_MC_DOMAINS
void tegra_pd_add_device(struct device *dev);
void tegra_pd_remove_device(struct device *dev);
void tegra_pd_add_sd(struct generic_pm_domain *sd);
#else
#define tegra_pd_add_device(dev) do { } while (0)
#define tegra_pd_remove_device(dev) do { } while (0)
#define tegra_pd_add_sd(sd) do { } while (0)
#endif /* CONFIG_TEGRA_MC_DOMAINS */

#endif /* _MACH_TEGRA_PM_DOMAINS_H_ */
