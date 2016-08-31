/*
 * arch/arm/mach-tegra/board-loki-kbc.c
 * Keys configuration for Nvidia tegra4 loki platform.
 *
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
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
#include <linux/io.h>
#include <linux/input/tegra_kbc.h>
#include <linux/gpio.h>
#include <linux/gpio_keys.h>
#include <linux/mfd/palmas.h>

#include "tegra-board-id.h"
#include "board.h"
#include "board-loki.h"
#include "devices.h"
#include "iomap.h"
#include "wakeups-t12x.h"


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

#define GPIO_IKEY(_id, _irq, _iswake, _deb)	\
	{					\
		.code = _id,			\
		.gpio = -1,			\
		.irq = _irq,			\
		.desc = #_id,			\
		.type = EV_KEY,			\
		.wakeup = _iswake,		\
		.debounce_interval = _deb,	\
	}

#define PMC_WAKE_STATUS         0x14
#define TEGRA_WAKE_PWR_INT      (1UL << 18)
#define PMC_WAKE2_STATUS        0x168

static int loki_wakeup_key(void);

static struct gpio_keys_button loki_int_keys[] = {
	[0] = GPIO_KEY(KEY_POWER, PQ0, 1),
	[1] = {
		.code = SW_LID,
		.gpio = TEGRA_GPIO_HALL,
		.irq = -1,
		.type = EV_SW,
		.desc = "Hall Effect Sensor",
		.active_low = 1,
		.wakeup = 1,
		.debounce_interval = 0,
	},
	[2] = {
		.code = KEY_WAKEUP,
		.gpio = TEGRA_GPIO_PS6,
		.irq = -1,
		.type = EV_KEY,
		.desc = "Gamepad",
		.active_low = 1,
		.wakeup = 1,
		.debounce_interval = 0,
	},
};

static struct gpio_keys_platform_data loki_int_keys_pdata = {
	.buttons	= loki_int_keys,
	.nbuttons	= ARRAY_SIZE(loki_int_keys),
	.wakeup_key	= loki_wakeup_key,
};

static struct platform_device loki_int_keys_device = {
	.name	= "gpio-keys",
	.id	= 0,
	.dev	= {
		.platform_data  = &loki_int_keys_pdata,
	},
};

static int loki_wakeup_key(void)
{
	int wakeup_key;
	u32 status;
	status = readl(IO_ADDRESS(TEGRA_PMC_BASE) + PMC_WAKE_STATUS)
		| (u64)readl(IO_ADDRESS(TEGRA_PMC_BASE)
		+ PMC_WAKE2_STATUS) << 32;

	if (status & TEGRA_WAKE_GPIO_PQ0)
		wakeup_key = KEY_POWER;
	else if (status & (1UL << TEGRA_WAKE_GPIO_PS0))
		wakeup_key = SW_LID;
	else if (status & (1UL << TEGRA_WAKE_GPIO_PS6))
		wakeup_key = KEY_WAKEUP;
	else
		wakeup_key = -1;

	return wakeup_key;
}

int __init loki_kbc_init(void)
{
	struct board_info board_info;
	int ret;

	tegra_get_board_info(&board_info);
	pr_info("Boardid:SKU = 0x%04x:0x%04x\n",
			board_info.board_id, board_info.sku);

	ret = platform_device_register(&loki_int_keys_device);
	return ret;
}
