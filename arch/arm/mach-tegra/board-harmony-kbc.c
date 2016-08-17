/*
 * arch/arm/mach-tegra/board-harmony-kbc.c
 * Keys configuration for Nvidia tegra2 harmony platform.
 *
 * Copyright (C) 2011 NVIDIA, Inc.
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

#include "board.h"
#include "board-harmony.h"
#include "devices.h"

#define HARMONY_ROW_COUNT	16
#define HARMONY_COL_COUNT	8

static const u32 kbd_keymap[] = {
	KEY(0, 0, KEY_RESERVED),
	KEY(0, 1, KEY_RESERVED),
	KEY(0, 2, KEY_W),
	KEY(0, 3, KEY_S),
	KEY(0, 4, KEY_A),
	KEY(0, 5, KEY_Z),
	KEY(0, 6, KEY_RESERVED),
	KEY(0, 7, KEY_FN),

	KEY(1, 0, KEY_RESERVED),
	KEY(1, 1, KEY_RESERVED),
	KEY(1, 2, KEY_RESERVED),
	KEY(1, 3, KEY_RESERVED),
	KEY(1, 4, KEY_RESERVED),
	KEY(1, 5, KEY_RESERVED),
	KEY(1, 6, KEY_RESERVED),
	KEY(1, 7, KEY_MENU),

	KEY(2, 0, KEY_RESERVED),
	KEY(2, 1, KEY_RESERVED),
	KEY(2, 2, KEY_RESERVED),
	KEY(2, 3, KEY_RESERVED),
	KEY(2, 4, KEY_RESERVED),
	KEY(2, 5, KEY_RESERVED),
	KEY(2, 6, KEY_LEFTALT),
	KEY(2, 7, KEY_RIGHTALT),

	KEY(3, 0, KEY_5),
	KEY(3, 1, KEY_4),
	KEY(3, 2, KEY_R),
	KEY(3, 3, KEY_E),
	KEY(3, 4, KEY_F),
	KEY(3, 5, KEY_D),
	KEY(3, 6, KEY_X),
	KEY(3, 7, KEY_RESERVED),

	KEY(4, 0, KEY_7),
	KEY(4, 1, KEY_6),
	KEY(4, 2, KEY_T),
	KEY(4, 3, KEY_H),
	KEY(4, 4, KEY_G),
	KEY(4, 5, KEY_V),
	KEY(4, 6, KEY_C),
	KEY(4, 7, KEY_SPACE),

	KEY(5, 0, KEY_9),
	KEY(5, 1, KEY_8),
	KEY(5, 2, KEY_U),
	KEY(5, 3, KEY_Y),
	KEY(5, 4, KEY_J),
	KEY(5, 5, KEY_N),
	KEY(5, 6, KEY_B),
	KEY(5, 7, KEY_BACKSLASH),

	KEY(6, 0, KEY_MINUS),
	KEY(6, 1, KEY_0),
	KEY(6, 2, KEY_O),
	KEY(6, 3, KEY_I),
	KEY(6, 4, KEY_L),
	KEY(6, 5, KEY_K),
	KEY(6, 6, KEY_COMMA),
	KEY(6, 7, KEY_M),

	KEY(7, 0, KEY_RESERVED),
	KEY(7, 1, KEY_EQUAL),
	KEY(7, 2, KEY_RIGHTBRACE),
	KEY(7, 3, KEY_ENTER),
	KEY(7, 4, KEY_RESERVED),
	KEY(7, 5, KEY_RESERVED),
	KEY(7, 6, KEY_RESERVED),
	KEY(7, 7, KEY_MENU),

	KEY(8, 0, KEY_RESERVED),
	KEY(8, 1, KEY_RESERVED),
	KEY(8, 2, KEY_RESERVED),
	KEY(8, 3, KEY_RESERVED),
	KEY(8, 4, KEY_LEFTSHIFT),
	KEY(8, 5, KEY_RIGHTSHIFT),
	KEY(8, 6, KEY_RESERVED),
	KEY(8, 7, KEY_RESERVED),

	KEY(9, 0, KEY_RESERVED),
	KEY(9, 1, KEY_RESERVED),
	KEY(9, 2, KEY_RESERVED),
	KEY(9, 3, KEY_RESERVED),
	KEY(9, 4, KEY_RESERVED),
	KEY(9, 5, KEY_LEFTCTRL),
	KEY(9, 6, KEY_RESERVED),
	KEY(9, 7, KEY_RIGHTCTRL),

	KEY(10, 0, KEY_RESERVED),
	KEY(10, 1, KEY_RESERVED),
	KEY(10, 2, KEY_RESERVED),
	KEY(10, 3, KEY_RESERVED),
	KEY(10, 4, KEY_RESERVED),
	KEY(10, 5, KEY_RESERVED),
	KEY(10, 6, KEY_RESERVED),
	KEY(10, 7, KEY_RESERVED),

	KEY(11, 0, KEY_LEFTBRACE),
	KEY(11, 1, KEY_P),
	KEY(11, 2, KEY_APOSTROPHE),
	KEY(11, 3, KEY_SEMICOLON),
	KEY(11, 4, KEY_SLASH),
	KEY(11, 5, KEY_DOT),
	KEY(11, 6, KEY_RESERVED),
	KEY(11, 7, KEY_RESERVED),

	KEY(12, 0, KEY_F10),
	KEY(12, 1, KEY_F9),
	KEY(12, 2, KEY_BACKSPACE),
	KEY(12, 3, KEY_3),
	KEY(12, 4, KEY_2),
	KEY(12, 5, KEY_UP),
	KEY(12, 6, KEY_PRINT),
	KEY(12, 7, KEY_PAUSE),

	KEY(13, 0, KEY_INSERT),
	KEY(13, 1, KEY_DELETE),
	KEY(13, 2, KEY_RESERVED),
	KEY(13, 3, KEY_PAGEUP),
	KEY(13, 4, KEY_PAGEDOWN),
	KEY(13, 5, KEY_RIGHT),
	KEY(13, 6, KEY_DOWN),
	KEY(13, 7, KEY_LEFT),

	KEY(14, 0, KEY_F11),
	KEY(14, 1, KEY_F12),
	KEY(14, 2, KEY_F8),
	KEY(14, 3, KEY_Q),
	KEY(14, 4, KEY_F4),
	KEY(14, 5, KEY_F3),
	KEY(14, 6, KEY_1),
	KEY(14, 7, KEY_F7),

	KEY(15, 0, KEY_ESC),
	KEY(15, 1, KEY_GRAVE),
	KEY(15, 2, KEY_F5),
	KEY(15, 3, KEY_TAB),
	KEY(15, 4, KEY_F1),
	KEY(15, 5, KEY_F2),
	KEY(15, 6, KEY_CAPSLOCK),
	KEY(15, 7, KEY_F6),

	KEY(16, 0, KEY_RESERVED),
	KEY(16, 1, KEY_RESERVED),
	KEY(16, 2, KEY_RESERVED),
	KEY(16, 3, KEY_RESERVED),
	KEY(16, 4, KEY_RESERVED),
	KEY(16, 5, KEY_RESERVED),
	KEY(16, 6, KEY_RESERVED),
	KEY(16, 7, KEY_RESERVED),

	KEY(17, 0, KEY_RESERVED),
	KEY(17, 1, KEY_RESERVED),
	KEY(17, 2, KEY_RESERVED),
	KEY(17, 3, KEY_RESERVED),
	KEY(17, 4, KEY_RESERVED),
	KEY(17, 5, KEY_RESERVED),
	KEY(17, 6, KEY_RESERVED),
	KEY(17, 7, KEY_RESERVED),

	KEY(18, 0, KEY_RESERVED),
	KEY(18, 1, KEY_RESERVED),
	KEY(18, 2, KEY_RESERVED),
	KEY(18, 3, KEY_RESERVED),
	KEY(18, 4, KEY_RESERVED),
	KEY(18, 5, KEY_RESERVED),
	KEY(18, 6, KEY_RESERVED),
	KEY(18, 7, KEY_RESERVED),

	KEY(19, 0, KEY_RESERVED),
	KEY(19, 1, KEY_RESERVED),
	KEY(19, 2, KEY_RESERVED),
	KEY(19, 3, KEY_RESERVED),
	KEY(19, 4, KEY_RESERVED),
	KEY(19, 5, KEY_RESERVED),
	KEY(19, 6, KEY_RESERVED),
	KEY(19, 7, KEY_RESERVED),

	KEY(20, 0, KEY_7),
	KEY(20, 1, KEY_RESERVED),
	KEY(20, 2, KEY_RESERVED),
	KEY(20, 3, KEY_RESERVED),
	KEY(20, 4, KEY_RESERVED),
	KEY(20, 5, KEY_RESERVED),
	KEY(20, 6, KEY_RESERVED),
	KEY(20, 7, KEY_RESERVED),

	KEY(21, 0, KEY_9),
	KEY(21, 1, KEY_8),
	KEY(21, 2, KEY_4),
	KEY(21, 3, KEY_RESERVED),
	KEY(21, 4, KEY_1),
	KEY(21, 5, KEY_RESERVED),
	KEY(21, 6, KEY_RESERVED),
	KEY(21, 7, KEY_RESERVED),

	KEY(22, 0, KEY_RESERVED),
	KEY(22, 1, KEY_SLASH),
	KEY(22, 2, KEY_6),
	KEY(22, 3, KEY_5),
	KEY(22, 4, KEY_3),
	KEY(22, 5, KEY_2),
	KEY(22, 6, KEY_RESERVED),
	KEY(22, 7, KEY_0),

	KEY(23, 0, KEY_RESERVED),
	KEY(23, 1, KEY_RESERVED),
	KEY(23, 2, KEY_RESERVED),
	KEY(23, 3, KEY_RESERVED),
	KEY(23, 4, KEY_RESERVED),
	KEY(23, 5, KEY_RESERVED),
	KEY(23, 6, KEY_RESERVED),
	KEY(23, 7, KEY_RESERVED),

	KEY(24, 0, KEY_RESERVED),
	KEY(24, 1, KEY_RESERVED),
	KEY(24, 2, KEY_RESERVED),
	KEY(24, 3, KEY_RESERVED),
	KEY(24, 4, KEY_RESERVED),
	KEY(24, 5, KEY_RESERVED),
	KEY(24, 6, KEY_RESERVED),
	KEY(24, 7, KEY_RESERVED),

	KEY(25, 0, KEY_RESERVED),
	KEY(25, 1, KEY_RESERVED),
	KEY(25, 2, KEY_RESERVED),
	KEY(25, 3, KEY_RESERVED),
	KEY(25, 4, KEY_RESERVED),
	KEY(25, 5, KEY_RESERVED),
	KEY(25, 6, KEY_RESERVED),
	KEY(25, 7, KEY_RESERVED),

	KEY(26, 0, KEY_RESERVED),
	KEY(26, 1, KEY_RESERVED),
	KEY(26, 2, KEY_RESERVED),
	KEY(26, 3, KEY_RESERVED),
	KEY(26, 4, KEY_RESERVED),
	KEY(26, 5, KEY_RESERVED),
	KEY(26, 6, KEY_RESERVED),
	KEY(26, 7, KEY_RESERVED),

	KEY(27, 0, KEY_RESERVED),
	KEY(27, 1, KEY_KPASTERISK),
	KEY(27, 2, KEY_RESERVED),
	KEY(27, 3, KEY_KPMINUS),
	KEY(27, 4, KEY_KPPLUS),
	KEY(27, 5, KEY_DOT),
	KEY(27, 6, KEY_RESERVED),
	KEY(27, 7, KEY_RESERVED),

	KEY(28, 0, KEY_RESERVED),
	KEY(28, 1, KEY_RESERVED),
	KEY(28, 2, KEY_RESERVED),
	KEY(28, 3, KEY_RESERVED),
	KEY(28, 4, KEY_RESERVED),
	KEY(28, 5, KEY_VOLUMEUP),
	KEY(28, 6, KEY_RESERVED),
	KEY(28, 7, KEY_RESERVED),

	KEY(29, 0, KEY_RESERVED),
	KEY(29, 1, KEY_RESERVED),
	KEY(29, 2, KEY_RESERVED),
	KEY(29, 3, KEY_HOME),
	KEY(29, 4, KEY_END),
	KEY(29, 5, KEY_BRIGHTNESSUP),
	KEY(29, 6, KEY_VOLUMEDOWN),
	KEY(29, 7, KEY_BRIGHTNESSDOWN),

	KEY(30, 0, KEY_NUMLOCK),
	KEY(30, 1, KEY_SCROLLLOCK),
	KEY(30, 2, KEY_MUTE),
	KEY(30, 3, KEY_RESERVED),
	KEY(30, 4, KEY_RESERVED),
	KEY(30, 5, KEY_RESERVED),
	KEY(30, 6, KEY_RESERVED),
	KEY(30, 7, KEY_RESERVED),

	KEY(31, 0, KEY_RESERVED),
	KEY(31, 1, KEY_RESERVED),
	KEY(31, 2, KEY_RESERVED),
	KEY(31, 3, KEY_RESERVED),
	KEY(31, 4, KEY_QUESTION),
	KEY(31, 5, KEY_RESERVED),
	KEY(31, 6, KEY_RESERVED),
	KEY(31, 7, KEY_RESERVED),
};

static const struct matrix_keymap_data keymap_data = {
	.keymap		= kbd_keymap,
	.keymap_size	= ARRAY_SIZE(kbd_keymap),
};

static struct tegra_kbc_wake_key harmony_wake_cfg[] = {
	[0] = {
		.row = 1,
		.col = 7,
	},
	[1] = {
		.row = 15,
		.col = 0,
	},
};

static struct tegra_kbc_platform_data harmony_kbc_platform_data = {
	.debounce_cnt = 2,
	.repeat_cnt = 5 * 32,
	.wakeup = true,
	.keymap_data = &keymap_data,
	.use_fn_map = true,
	.wake_cnt = 2,
	.wake_cfg = &harmony_wake_cfg[0],
#ifdef CONFIG_ANDROID
	.disable_ev_rep = true,
#endif
};

int __init harmony_kbc_init(void)
{
	struct tegra_kbc_platform_data *data = &harmony_kbc_platform_data;
	int i;
	tegra_kbc_device.dev.platform_data = &harmony_kbc_platform_data;
	pr_info("Registering tegra-kbc\n");

	BUG_ON((KBC_MAX_ROW + KBC_MAX_COL) > KBC_MAX_GPIO);
	for (i = 0; i < KBC_MAX_ROW; i++) {
		data->pin_cfg[i].num = i;
		data->pin_cfg[i].type = PIN_CFG_ROW;
	}

	for (i = 0; i < KBC_MAX_COL; i++)
		data->pin_cfg[i + KBC_MAX_ROW].num = i;

	platform_device_register(&tegra_kbc_device);
	pr_info("Registering successful tegra-kbc\n");
	return 0;
}

