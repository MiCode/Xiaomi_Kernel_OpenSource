/*
 * arch/arm/mach-tegra/include/mach/io_dpd.h
 *
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

#ifndef __MACH_TEGRA_IO_DPD_H
#define __MACH_TEGRA_IO_DPD_H
#include <linux/delay.h>

/* Tegra io dpd entry - for each supported driver */
struct tegra_io_dpd {
	const char *name;	/* driver name */
	u8 io_dpd_reg_index;	/* io dpd register index */
	u8 io_dpd_bit;		/* bit position for driver in dpd register */
	u8 need_delay_dpd;	/* work around to delay dpd after lp0*/
	struct delayed_work	delay_dpd;
	struct mutex		delay_lock;
};


/* Tegra io dpd APIs */
struct tegra_io_dpd *tegra_io_dpd_get(struct device *dev); /* get handle */
void tegra_io_dpd_enable(struct tegra_io_dpd *hnd); /* enable dpd */
void tegra_io_dpd_disable(struct tegra_io_dpd *hnd); /* disable dpd */
int tegra_io_dpd_init(void);

#endif /* end __MACH_TEGRA_IO_DPD_H */
