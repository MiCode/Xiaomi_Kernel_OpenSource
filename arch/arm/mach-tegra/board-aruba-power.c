/*
 * Copyright (C) 2010 NVIDIA, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */
#include <linux/i2c.h>
#include <linux/pda_power.h>
#include <linux/platform_device.h>
#include <linux/resource.h>
#include <linux/io.h>

#include <mach/iomap.h>
#include <mach/irqs.h>

#include "pm.h"
#include "board.h"

static int ac_online(void)
{
	return 1;
}

static struct resource aruba_pda_resources[] = {
	[0] = {
		.name	= "ac",
	},
};

static struct pda_power_pdata aruba_pda_data = {
	.is_ac_online	= ac_online,
};

static struct platform_device aruba_pda_power_device = {
	.name		= "pda-power",
	.id		= -1,
	.resource	= aruba_pda_resources,
	.num_resources	= ARRAY_SIZE(aruba_pda_resources),
	.dev	= {
		.platform_data	= &aruba_pda_data,
	},
};

static struct tegra_suspend_platform_data aruba_suspend_data = {
	.cpu_timer	= 2000,
	.cpu_off_timer	= 0,
	.suspend_mode	= TEGRA_SUSPEND_NONE,
	.core_timer	= 0x7e7e,
	.core_off_timer = 0,
	.corereq_high	= false,
	.sysclkreq_high	= true,
};

int __init aruba_regulator_init(void)
{
	platform_device_register(&aruba_pda_power_device);
	tegra_init_suspend(&aruba_suspend_data);
	return 0;
}

void __init tegra_tsensor_init(void)
{
	/* No tsensor on FPGAs */
}
