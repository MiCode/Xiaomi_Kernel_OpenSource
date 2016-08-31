/*
 * Platform definitions for tegra-kbc keyboard input driver
 *
 * Copyright (c) 2010-2011, NVIDIA Corporation.
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
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef ASMARM_ARCH_TEGRA_KBC_H
#define ASMARM_ARCH_TEGRA_KBC_H

#include <linux/types.h>
#include <linux/input/matrix_keypad.h>

#define KBC_MAX_GPIO	24
#define KBC_MAX_KPENT	8

#define KBC_MAX_ROW	16
#define KBC_MAX_COL	8
#define KBC_MAX_KEY	(KBC_MAX_ROW * KBC_MAX_COL)

#define KBC_PIN_GPIO_0		0
#define KBC_PIN_GPIO_1		1
#define KBC_PIN_GPIO_2		2
#define KBC_PIN_GPIO_3		3
#define KBC_PIN_GPIO_4		4
#define KBC_PIN_GPIO_5		5
#define KBC_PIN_GPIO_6		6
#define KBC_PIN_GPIO_7		7
#define KBC_PIN_GPIO_8		8
#define KBC_PIN_GPIO_9		9
#define KBC_PIN_GPIO_10		10
#define KBC_PIN_GPIO_11		11
#define KBC_PIN_GPIO_12		12
#define KBC_PIN_GPIO_13		13
#define KBC_PIN_GPIO_14		14
#define KBC_PIN_GPIO_15		15
#define KBC_PIN_GPIO_16		16
#define KBC_PIN_GPIO_17		17
#define KBC_PIN_GPIO_18		18
#define KBC_PIN_GPIO_19		19
#define KBC_PIN_GPIO_20		20
#define KBC_PIN_GPIO_21		21
#define KBC_PIN_GPIO_22		22
#define KBC_PIN_GPIO_23		23

enum tegra_pin_type {
	PIN_CFG_IGNORE,
	PIN_CFG_COL,
	PIN_CFG_ROW,
};

struct tegra_kbc_pin_cfg {
	enum tegra_pin_type type;
	unsigned char num;
};

struct tegra_kbc_wake_key {
	u8 row:4;
	u8 col:4;
};

struct tegra_kbc_platform_data {
	unsigned int debounce_cnt;
	unsigned int repeat_cnt;
	unsigned int scan_count;

	unsigned int wake_cnt; /* 0:wake on any key >1:wake on wake_cfg */
	const struct tegra_kbc_wake_key *wake_cfg;

	struct tegra_kbc_pin_cfg pin_cfg[KBC_MAX_GPIO];
	const struct matrix_keymap_data *keymap_data;

	u32 wakeup_key;
	bool wakeup;
	bool use_fn_map;
	bool use_ghost_filter;
	bool disable_ev_rep;
};
#endif
