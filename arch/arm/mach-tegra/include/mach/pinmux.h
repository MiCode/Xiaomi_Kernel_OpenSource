/*
 * linux/arch/arm/mach-tegra/include/mach/pinmux.h
 *
 * Copyright (C) 2010 Google, Inc.
 * Copyright (c) 2011-2013, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __MACH_TEGRA_PINMUX_H
#define __MACH_TEGRA_PINMUX_H

#include <mach/pinmux-defines.h>
#include <linux/pinctrl/pinctrl-tegra.h>

static inline int tegra_pinmux_get_pingroup(int gpio_nr)
{
	return tegra_pinctrl_gpio_to_pingroup(gpio_nr);
}

static inline int tegra_pinmux_set_func(
		const struct tegra_pingroup_config *config)
{
	return tegra_pinctrl_pg_set_func(config);
}

static inline int tegra_pinmux_get_func(int pg)
{
	return tegra_pinctrl_pg_get_func(pg);
}

static inline int tegra_pinmux_set_tristate(
		int pg, enum tegra_tristate tristate)
{
	return tegra_pinctrl_pg_set_tristate(pg, tristate);
}

static inline int tegra_pinmux_set_io(int pg, enum tegra_pin_io input)
{
	return tegra_pinctrl_pg_set_io(pg, input);
}

static inline int tegra_pinmux_set_lock(int pg, enum tegra_pin_lock lock)
{
	return tegra_pinctrl_pg_set_lock(pg, lock);
}

static inline int tegra_pinmux_set_od(int pg, enum tegra_pin_od od)
{
	return tegra_pinctrl_pg_set_od(pg, od);
}

static inline int tegra_pinmux_set_ioreset(
		int pg, enum tegra_pin_ioreset ioreset)
{
	return tegra_pinctrl_pg_set_ioreset(pg, ioreset);
}

static inline int tegra_pinmux_set_rcv_sel(
		int pg, enum tegra_pin_rcv_sel rcv_sel)
{
	return tegra_pinctrl_pg_set_rcv_sel(pg, rcv_sel);
}

static inline int tegra_pinmux_set_pullupdown(
		int pg, enum tegra_pullupdown pupd)
{
	return tegra_pinctrl_pg_set_pullupdown(pg, pupd);
}

static inline void tegra_pinmux_config_pingroup(
		const struct tegra_pingroup_config *config)
{
	tegra_pinctrl_pg_config_pingroup(config);

}

static inline void tegra_pinmux_config_table(
		const struct tegra_pingroup_config *config, int len)
{
	tegra_pinctrl_pg_config_table(config, len);
}

static inline int tegra_drive_pinmux_set_hsm(
		int pg, enum tegra_hsm hsm)
{
	return tegra_pinctrl_pg_drive_set_hsm(pg, hsm);
}

static inline int tegra_drive_pinmux_set_schmitt(
		int pg, enum tegra_schmitt schmitt)
{
	return tegra_pinctrl_pg_drive_set_schmitt(pg, schmitt);
}

static inline int tegra_drive_pinmux_set_drive(
		int pg, enum tegra_drive drive)
{
	return tegra_pinctrl_pg_drive_set_drive(pg, drive);
}

static inline int tegra_drive_pinmux_set_pull_down(
		int pg, enum tegra_pull_strength pull_down)
{
	return tegra_pinctrl_pg_drive_set_pull_down(pg, pull_down);
}

static inline int tegra_drive_pinmux_set_pull_up(
		int pg, enum tegra_pull_strength pull_up)
{
	return tegra_pinctrl_pg_drive_set_pull_up(pg, pull_up);
}

static inline int tegra_drive_pinmux_set_slew_rising(
		int pg, enum tegra_slew slew_rising)
{
	return tegra_pinctrl_pg_drive_set_slew_rising(pg, slew_rising);
}

static inline int tegra_drive_pinmux_set_slew_falling(
		int pg, enum tegra_slew slew_falling)
{
	return tegra_pinctrl_pg_drive_set_slew_falling(pg, slew_falling);
}

static inline int tegra_drive_pinmux_set_drive_type(
		int pg, enum tegra_drive_type drive_type)
{
	return tegra_pinctrl_pg_drive_set_drive_type(pg, drive_type);
}

static inline void tegra_drive_pinmux_config_pingroup(
		int pingroup, enum tegra_hsm hsm,
		enum tegra_schmitt schmitt, enum tegra_drive drive,
		enum tegra_pull_strength pull_down,
		enum tegra_pull_strength pull_up,
		enum tegra_slew slew_rising,
		enum tegra_slew slew_falling,
		enum tegra_drive_type drive_type)
{
	tegra_pinctrl_pg_drive_config_pingroup(pingroup, hsm, schmitt,
			drive, pull_down, pull_up, slew_rising,
			slew_falling, drive_type);
}

static inline void tegra_drive_pinmux_config_table(
		struct tegra_drive_pingroup_config *config, int len)
{
	tegra_pinctrl_pg_drive_config_table(config, len);
}

static inline int tegra_drive_get_pingroup(struct device *dev)
{
	return tegra_pinctrl_pg_drive_get_pingroup(dev);
}

static inline void tegra_pinmux_set_safe_pinmux_table(
		const struct tegra_pingroup_config *config, int len)
{
	tegra_pinctrl_pg_set_safe_pinmux_table(config, len);
}

static inline void tegra_pinmux_config_pinmux_table(
		const struct tegra_pingroup_config *config, int len)
{
	tegra_pinctrl_pg_config_pinmux_table(config, len);
}

static inline void tegra_pinmux_config_tristate_table(
		const struct tegra_pingroup_config *config, int len,
		enum tegra_tristate tristate)
{
	tegra_pinctrl_pg_config_tristate_table(config, len, tristate);
}

static inline void tegra_pinmux_config_pullupdown_table(
		const struct tegra_pingroup_config *config,
		int len, enum tegra_pullupdown pupd)
{
	tegra_pinctrl_pg_config_pullupdown_table(config, len, pupd);
}
#endif
