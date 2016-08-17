/*
 * arch/arm/mach-tegra/include/mach/tegra_dc_ext.h
 *
 * Copyright (C) 2011, NVIDIA Corporation
 *
 * Author: Robert Morell <rmorell@nvidia.com>
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
 */

#ifndef __MACH_TEGRA_DC_EXT_H
#define __MACH_TEGRA_DC_EXT_H

#include <linux/nvhost.h>

struct tegra_dc_ext;

#ifdef CONFIG_TEGRA_DC_EXTENSIONS
int __init tegra_dc_ext_module_init(void);
void __exit tegra_dc_ext_module_exit(void);

struct tegra_dc_ext *tegra_dc_ext_register(struct platform_device *ndev,
					   struct tegra_dc *dc);
void tegra_dc_ext_unregister(struct tegra_dc_ext *dc_ext);

/* called by display controller on enable/disable */
void tegra_dc_ext_enable(struct tegra_dc_ext *dc_ext);
void tegra_dc_ext_disable(struct tegra_dc_ext *dc_ext);

int tegra_dc_ext_process_hotplug(int output);

#else /* CONFIG_TEGRA_DC_EXTENSIONS */

static inline
int tegra_dc_ext_module_init(void)
{
	return 0;
}
static inline
void tegra_dc_ext_module_exit(void)
{
}

static inline
struct tegra_dc_ext *tegra_dc_ext_register(struct platform_device *ndev,
					   struct tegra_dc *dc)
{
	return NULL;
}
static inline
void tegra_dc_ext_unregister(struct tegra_dc_ext *dc_ext)
{
}
static inline
void tegra_dc_ext_enable(struct tegra_dc_ext *dc_ext)
{
}
static inline
void tegra_dc_ext_disable(struct tegra_dc_ext *dc_ext)
{
}
static inline
int tegra_dc_ext_process_hotplug(int output)
{
	return 0;
}
#endif /* CONFIG_TEGRA_DC_EXTENSIONS */

#endif /* __MACH_TEGRA_DC_EXT_H */
