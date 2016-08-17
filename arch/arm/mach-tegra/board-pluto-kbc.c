/*
 * arch/arm/mach-tegra/board-pluto-kbc.c
 * Keys configuration for Nvidia tegra3 pluto platform.
 *
 * Copyright (C) 2012 NVIDIA, Inc.
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
#include <mach/iomap.h>
#include <mach/kbc.h>
#include <linux/gpio.h>
#include <linux/gpio_keys.h>

#include "board.h"
#include "board-pluto.h"
#include "devices.h"

#define PLUTO_ROW_COUNT	3
#define PLUTO_COL_COUNT	3

static const u32 kbd_keymap[] = {
	KEY(0, 0, KEY_POWER),
	KEY(0, 1, KEY_VOLUMEUP),
	KEY(0, 2, KEY_VOLUMEDOWN),

	KEY(1, 0, KEY_SEARCH),
	KEY(1, 1, KEY_CAMERA),
	KEY(1, 2, KEY_CAMERA_FOCUS),

	KEY(2, 0, KEY_HOME),
	KEY(2, 1, KEY_BACK),
	KEY(2, 2, KEY_MENU),
};

static const struct matrix_keymap_data keymap_data = {
	.keymap		= kbd_keymap,
	.keymap_size	= ARRAY_SIZE(kbd_keymap),
};

static struct tegra_kbc_wake_key pluto_wake_cfg[] = {
	[0] = {
		.row = 0,
		.col = 0,
	},
};

static struct tegra_kbc_platform_data pluto_kbc_platform_data = {
	.debounce_cnt = 20 * 32, /* 20 ms debaunce time */
	.repeat_cnt = 1,
	.scan_count = 30,
	.wakeup = true,
	.keymap_data = &keymap_data,
	.wake_cnt = 1,
	.wake_cfg = &pluto_wake_cfg[0],
	.wakeup_key = KEY_POWER,
#ifdef CONFIG_ANDROID
	.disable_ev_rep = true,
#endif
};

static struct gpio_keys_button pluto_keys[] = {
	[0] = {
		.code = KEY_MUTE,
		.gpio = TEGRA_GPIO_PI5,
		.irq = -1,
		.type = EV_KEY,
		.desc = "RINGER",
		.active_low = 0,
		.wakeup = 0,
		.debounce_interval = 100,
	},
};

static struct gpio_keys_platform_data pluto_keys_pdata = {
	.buttons	= pluto_keys,
	.nbuttons	= ARRAY_SIZE(pluto_keys),
};

static struct platform_device pluto_keys_device = {
	.name	= "gpio-keys",
	.id	= 0,
	.dev	= {
		.platform_data  = &pluto_keys_pdata,
	},
};

int __init pluto_kbc_init(void)
{
	struct tegra_kbc_platform_data *data = &pluto_kbc_platform_data;
	int i;

	tegra_kbc_device.dev.platform_data = &pluto_kbc_platform_data;
	pr_info("Registering tegra-kbc\n");

	BUG_ON((KBC_MAX_ROW + KBC_MAX_COL) > KBC_MAX_GPIO);
	for (i = 0; i < PLUTO_ROW_COUNT; i++) {
		data->pin_cfg[i].num = i;
		data->pin_cfg[i].type = PIN_CFG_ROW;
	}
	for (i = 0; i < PLUTO_COL_COUNT; i++) {
		data->pin_cfg[i + KBC_PIN_GPIO_11].num = i;
		data->pin_cfg[i + KBC_PIN_GPIO_11].type = PIN_CFG_COL;
	}

	platform_device_register(&tegra_kbc_device);
	pr_info("Registering successful tegra-kbc\n");

	platform_device_register(&pluto_keys_device);
	pr_info("Registering successful gpio-keys\n");

	return 0;
}

