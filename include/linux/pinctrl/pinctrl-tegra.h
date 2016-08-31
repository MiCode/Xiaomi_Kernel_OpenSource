/*
 * Pincontrol driver interface.
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * Author: Laxman Dewangan <ldewangan@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _LINUX_PINCTRL_TEGRA_H__
#define _LINUX_PINCTRL_TEGRA_H__

#include <mach/pinmux-defines.h>

extern int tegra_pinctrl_gpio_to_pingroup(int gpio_nr);
extern int tegra_pinctrl_pg_set_func(const struct tegra_pingroup_config *config);
extern int tegra_pinctrl_pg_get_func(int pg);
extern int tegra_pinctrl_pg_set_tristate(int pg, int tristate);
extern int tegra_pinctrl_pg_set_io(int pg, int input);
extern int tegra_pinctrl_pg_set_lock(int pg, int lock);
extern int tegra_pinctrl_pg_set_od(int pg, int od);
extern int tegra_pinctrl_pg_set_ioreset(int pg, int ioreset);
extern int tegra_pinctrl_pg_set_rcv_sel(int pg, int rcv_sel);
extern int tegra_pinctrl_pg_set_pullupdown(int pg, int pupd);
extern void tegra_pinctrl_pg_config_pingroup(
	const struct tegra_pingroup_config *config);
extern void tegra_pinctrl_pg_config_table(
	const struct tegra_pingroup_config *config, int len);

extern int tegra_pinctrl_pg_drive_set_hsm(int pdg, int hsm);
extern int tegra_pinctrl_pg_drive_set_schmitt(int pdg, int schmitt);
extern int tegra_pinctrl_pg_drive_set_drive(int pdg, int drive);
extern int tegra_pinctrl_pg_drive_set_pull_down(int pdg, int pull_down);
extern int tegra_pinctrl_pg_drive_set_pull_up(int pdg, int pull_up);
extern int tegra_pinctrl_pg_drive_set_slew_rising(int pdg, int slew_rising);
extern int tegra_pinctrl_pg_drive_set_slew_falling(int pdg, int slew_falling);
extern int tegra_pinctrl_pg_drive_set_drive_type(int pdg, int drive_type);
extern void tegra_pinctrl_pg_drive_config_pingroup(int pdg, int hsm,
	int schmitt, int drive, int pull_down, int pull_up,
	int slew_rising, int slew_falling, int drive_type);
extern void tegra_pinctrl_pg_drive_config_table(
	struct tegra_drive_pingroup_config *config, int len);
extern int tegra_pinctrl_pg_drive_get_pingroup(struct device *dev);

extern void tegra_pinctrl_pg_set_safe_pinmux_table(
	const struct tegra_pingroup_config *config, int len);
extern void tegra_pinctrl_pg_config_pinmux_table(
	const struct tegra_pingroup_config *config, int len);
extern void tegra_pinctrl_pg_config_tristate_table(
	const struct tegra_pingroup_config *config,
	int len, int tristate);
extern void tegra_pinctrl_pg_config_pullupdown_table(
	const struct tegra_pingroup_config *config,
	int len, int pupd);


static inline int tegra_pinctrl_set_func(
	const struct tegra_pingroup_config *config)
{
	return tegra_pinctrl_pg_set_func(config);
}

static inline void tegra_pinctrl_config_pingroup(
	const struct tegra_pingroup_config *config)
{
	tegra_pinctrl_pg_config_pingroup(config);
}

static inline void tegra_pinctrl_config_table(
	const struct tegra_pingroup_config *config, int len)
{
	tegra_pinctrl_pg_config_table(config, len);
}

static inline void tegra_pinctrl_drive_config_table(
	struct tegra_drive_pingroup_config *config, int len)
{
	tegra_pinctrl_pg_drive_config_table(config, len);
}

static inline int tegra_pinctrl_drive_get_pingroup(struct device *dev)
{
	return tegra_pinctrl_pg_drive_get_pingroup(dev);
}

static inline void tegra_pinctrl_set_safe_pinmux_table(
	const struct tegra_pingroup_config *config, int len)
{
	tegra_pinctrl_pg_set_safe_pinmux_table(config, len);
}

static inline void tegra_pinctrl_config_pinmux_table(
	const struct tegra_pingroup_config *config, int len)
{
	tegra_pinctrl_pg_config_pinmux_table(config, len);
}

static inline void tegra_pinctrl_config_tristate_table(
	const struct tegra_pingroup_config *config,
	int len, int tristate)
{
	tegra_pinctrl_pg_config_tristate_table(config, len, tristate);
}

static inline void tegra_pinctrl_config_pullupdown_table(
	const struct tegra_pingroup_config *config,
	int len, int pupd)
{
	tegra_pinctrl_pg_config_pullupdown_table(config, len, pupd);
}

#endif /* _LINUX_PINCTRL_TEGRA_H__ */
