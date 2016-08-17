/*
 * arch/arm/mach-tegra/board-macallan-kbc.c
 * Keys configuration for Nvidia t114 macallan platform.
 *
 * Copyright (C) 2013 NVIDIA, Inc.
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

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <mach/io.h>
#include <linux/io.h>
#include <mach/iomap.h>
#include <mach/kbc.h>
#include <linux/gpio.h>
#include <linux/gpio_keys.h>
#include <linux/mfd/palmas.h>
#include "wakeups-t11x.h"

#include "tegra-board-id.h"
#include "board.h"
#include "board-macallan.h"
#include "devices.h"

#define GPIO_KEY(_id, _gpio, _iswake)           \
	{                                       \
		.code = _id,                    \
		.gpio = TEGRA_GPIO_##_gpio,     \
		.active_low = 1,                \
		.desc = #_id,                   \
		.type = EV_KEY,                 \
		.wakeup = _iswake,              \
		.debounce_interval = 10,        \
	}

static struct gpio_keys_button macallan_e1545_keys[] = {
	[0] = GPIO_KEY(KEY_POWER, PQ0, 1),
	[1] = GPIO_KEY(KEY_VOLUMEUP, PR2, 0),
	[2] = GPIO_KEY(KEY_VOLUMEDOWN, PR1, 0),
	[3] = GPIO_KEY(KEY_HOME, PI5, 0),
	[4] = GPIO_KEY(KEY_BACK, PQ2, 0), /* Todo : CAMF */
	[5] = GPIO_KEY(KEY_MENU, PR0, 0), /* Todo : CAMS */
};

static int macallan_wakeup_key(void)
{
	int wakeup_key;
	u64 status = readl(IO_ADDRESS(TEGRA_PMC_BASE) + PMC_WAKE_STATUS)
		| (u64)readl(IO_ADDRESS(TEGRA_PMC_BASE)
		+ PMC_WAKE2_STATUS) << 32;
	if (status & ((u64)1 << TEGRA_WAKE_GPIO_PQ0))
		wakeup_key = KEY_POWER;
	else if (status & ((u64)1 << TEGRA_WAKE_GPIO_PS0))
		wakeup_key = SW_LID;
	else
		wakeup_key = KEY_RESERVED;

	return wakeup_key;
}

static struct gpio_keys_platform_data macallan_e1545_keys_pdata = {
	.buttons	= macallan_e1545_keys,
	.nbuttons	= ARRAY_SIZE(macallan_e1545_keys),
	.wakeup_key	= macallan_wakeup_key,
};

static struct platform_device macallan_e1545_keys_device = {
	.name	= "gpio-keys",
	.id	= 0,
	.dev	= {
		.platform_data  = &macallan_e1545_keys_pdata,
	},
};

int __init macallan_kbc_init(void)
{
	platform_device_register(&macallan_e1545_keys_device);

	return 0;
}

