/* arch/arm/mach-msm/board-trout-keypad.c
 *
 * Copyright (C) 2008 Google, Inc.
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

#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/gpio_event.h>
#include <asm/mach-types.h>

#include "board-trout.h"

static char *keycaps = "--qwerty";
#undef MODULE_PARAM_PREFIX
#define MODULE_PARAM_PREFIX "board_trout."
module_param_named(keycaps, keycaps, charp, 0);


static unsigned int trout_col_gpios[] = { 35, 34, 33, 32, 31, 23, 30, 78 };
static unsigned int trout_row_gpios[] = { 42, 41, 40, 39, 38, 37, 36 };

#define KEYMAP_INDEX(col, row) ((col)*ARRAY_SIZE(trout_row_gpios) + (row))

static const unsigned short trout_keymap[ARRAY_SIZE(trout_col_gpios) * ARRAY_SIZE(trout_row_gpios)] = {
	[KEYMAP_INDEX(0, 0)] = KEY_BACK,
	[KEYMAP_INDEX(0, 1)] = KEY_HOME,
//	[KEYMAP_INDEX(0, 2)] = KEY_,
	[KEYMAP_INDEX(0, 3)] = KEY_BACKSPACE,
	[KEYMAP_INDEX(0, 4)] = KEY_ENTER,
	[KEYMAP_INDEX(0, 5)] = KEY_RIGHTALT,
	[KEYMAP_INDEX(0, 6)] = KEY_P,

	[KEYMAP_INDEX(1, 0)] = KEY_MENU,
//	[KEYMAP_INDEX(1, 0)] = 229, // SOFT1
	[KEYMAP_INDEX(1, 1)] = KEY_SEND,
	[KEYMAP_INDEX(1, 2)] = KEY_END,
	[KEYMAP_INDEX(1, 3)] = KEY_LEFTALT,
	[KEYMAP_INDEX(1, 4)] = KEY_A,
	[KEYMAP_INDEX(1, 5)] = KEY_LEFTSHIFT,
	[KEYMAP_INDEX(1, 6)] = KEY_Q,

	[KEYMAP_INDEX(2, 0)] = KEY_U,
	[KEYMAP_INDEX(2, 1)] = KEY_7,
	[KEYMAP_INDEX(2, 2)] = KEY_K,
	[KEYMAP_INDEX(2, 3)] = KEY_J,
	[KEYMAP_INDEX(2, 4)] = KEY_M,
	[KEYMAP_INDEX(2, 5)] = KEY_SLASH,
	[KEYMAP_INDEX(2, 6)] = KEY_8,

	[KEYMAP_INDEX(3, 0)] = KEY_5,
	[KEYMAP_INDEX(3, 1)] = KEY_6,
	[KEYMAP_INDEX(3, 2)] = KEY_B,
	[KEYMAP_INDEX(3, 3)] = KEY_H,
	[KEYMAP_INDEX(3, 4)] = KEY_N,
	[KEYMAP_INDEX(3, 5)] = KEY_SPACE,
	[KEYMAP_INDEX(3, 6)] = KEY_Y,

	[KEYMAP_INDEX(4, 0)] = KEY_4,
	[KEYMAP_INDEX(4, 1)] = KEY_R,
	[KEYMAP_INDEX(4, 2)] = KEY_V,
	[KEYMAP_INDEX(4, 3)] = KEY_G,
	[KEYMAP_INDEX(4, 4)] = KEY_C,
	//[KEYMAP_INDEX(4, 5)] = KEY_,
	[KEYMAP_INDEX(4, 6)] = KEY_T,

	[KEYMAP_INDEX(5, 0)] = KEY_2,
	[KEYMAP_INDEX(5, 1)] = KEY_W,
	[KEYMAP_INDEX(5, 2)] = KEY_COMPOSE,
	[KEYMAP_INDEX(5, 3)] = KEY_VOLUMEUP,
	[KEYMAP_INDEX(5, 4)] = KEY_S,
	[KEYMAP_INDEX(5, 5)] = KEY_Z,
	[KEYMAP_INDEX(5, 6)] = KEY_1,

	[KEYMAP_INDEX(6, 0)] = KEY_I,
	[KEYMAP_INDEX(6, 1)] = KEY_0,
	[KEYMAP_INDEX(6, 2)] = KEY_O,
	[KEYMAP_INDEX(6, 3)] = KEY_L,
	[KEYMAP_INDEX(6, 4)] = KEY_DOT,
	[KEYMAP_INDEX(6, 5)] = KEY_COMMA,
	[KEYMAP_INDEX(6, 6)] = KEY_9,

	[KEYMAP_INDEX(7, 0)] = KEY_3,
	[KEYMAP_INDEX(7, 1)] = KEY_E,
	[KEYMAP_INDEX(7, 2)] = KEY_EMAIL, // @
	[KEYMAP_INDEX(7, 3)] = KEY_VOLUMEDOWN,
	[KEYMAP_INDEX(7, 4)] = KEY_X,
	[KEYMAP_INDEX(7, 5)] = KEY_F,
	[KEYMAP_INDEX(7, 6)] = KEY_D
};

static unsigned int trout_col_gpios_evt2[] = { 35, 34, 33, 32, 31, 23, 30, 109 };
static unsigned int trout_row_gpios_evt2[] = { 42, 41, 40, 39, 38, 37, 36 };

static const unsigned short trout_keymap_evt2_1[ARRAY_SIZE(trout_col_gpios) * ARRAY_SIZE(trout_row_gpios)] = {
	[KEYMAP_INDEX(0, 0)] = KEY_BACK,
	[KEYMAP_INDEX(0, 1)] = KEY_HOME,
//	[KEYMAP_INDEX(0, 2)] = KEY_,
	[KEYMAP_INDEX(0, 3)] = KEY_BACKSPACE,
	[KEYMAP_INDEX(0, 4)] = KEY_ENTER,
	[KEYMAP_INDEX(0, 5)] = KEY_RIGHTSHIFT,
	[KEYMAP_INDEX(0, 6)] = KEY_P,

	[KEYMAP_INDEX(1, 0)] = KEY_MENU,
	[KEYMAP_INDEX(1, 1)] = KEY_SEND,
//	[KEYMAP_INDEX(1, 2)] = KEY_,
	[KEYMAP_INDEX(1, 3)] = KEY_LEFTSHIFT,
	[KEYMAP_INDEX(1, 4)] = KEY_A,
	[KEYMAP_INDEX(1, 5)] = KEY_COMPOSE,
	[KEYMAP_INDEX(1, 6)] = KEY_Q,

	[KEYMAP_INDEX(2, 0)] = KEY_U,
	[KEYMAP_INDEX(2, 1)] = KEY_7,
	[KEYMAP_INDEX(2, 2)] = KEY_K,
	[KEYMAP_INDEX(2, 3)] = KEY_J,
	[KEYMAP_INDEX(2, 4)] = KEY_M,
	[KEYMAP_INDEX(2, 5)] = KEY_SLASH,
	[KEYMAP_INDEX(2, 6)] = KEY_8,

	[KEYMAP_INDEX(3, 0)] = KEY_5,
	[KEYMAP_INDEX(3, 1)] = KEY_6,
	[KEYMAP_INDEX(3, 2)] = KEY_B,
	[KEYMAP_INDEX(3, 3)] = KEY_H,
	[KEYMAP_INDEX(3, 4)] = KEY_N,
	[KEYMAP_INDEX(3, 5)] = KEY_SPACE,
	[KEYMAP_INDEX(3, 6)] = KEY_Y,

	[KEYMAP_INDEX(4, 0)] = KEY_4,
	[KEYMAP_INDEX(4, 1)] = KEY_R,
	[KEYMAP_INDEX(4, 2)] = KEY_V,
	[KEYMAP_INDEX(4, 3)] = KEY_G,
	[KEYMAP_INDEX(4, 4)] = KEY_C,
//	[KEYMAP_INDEX(4, 5)] = KEY_,
	[KEYMAP_INDEX(4, 6)] = KEY_T,

	[KEYMAP_INDEX(5, 0)] = KEY_2,
	[KEYMAP_INDEX(5, 1)] = KEY_W,
	[KEYMAP_INDEX(5, 2)] = KEY_LEFTALT,
	[KEYMAP_INDEX(5, 3)] = KEY_VOLUMEUP,
	[KEYMAP_INDEX(5, 4)] = KEY_S,
	[KEYMAP_INDEX(5, 5)] = KEY_Z,
	[KEYMAP_INDEX(5, 6)] = KEY_1,

	[KEYMAP_INDEX(6, 0)] = KEY_I,
	[KEYMAP_INDEX(6, 1)] = KEY_0,
	[KEYMAP_INDEX(6, 2)] = KEY_O,
	[KEYMAP_INDEX(6, 3)] = KEY_L,
	[KEYMAP_INDEX(6, 4)] = KEY_COMMA,
	[KEYMAP_INDEX(6, 5)] = KEY_DOT,
	[KEYMAP_INDEX(6, 6)] = KEY_9,

	[KEYMAP_INDEX(7, 0)] = KEY_3,
	[KEYMAP_INDEX(7, 1)] = KEY_E,
	[KEYMAP_INDEX(7, 2)] = KEY_EMAIL, // @
	[KEYMAP_INDEX(7, 3)] = KEY_VOLUMEDOWN,
	[KEYMAP_INDEX(7, 4)] = KEY_X,
	[KEYMAP_INDEX(7, 5)] = KEY_F,
	[KEYMAP_INDEX(7, 6)] = KEY_D
};

static const unsigned short trout_keymap_evt2_2[ARRAY_SIZE(trout_col_gpios) * ARRAY_SIZE(trout_row_gpios)] = {
	[KEYMAP_INDEX(0, 0)] = KEY_BACK,
	[KEYMAP_INDEX(0, 1)] = KEY_HOME,
//	[KEYMAP_INDEX(0, 2)] = KEY_,
	[KEYMAP_INDEX(0, 3)] = KEY_BACKSPACE,
	[KEYMAP_INDEX(0, 4)] = KEY_ENTER,
	[KEYMAP_INDEX(0, 5)] = KEY_RIGHTSHIFT,
	[KEYMAP_INDEX(0, 6)] = KEY_P,

	[KEYMAP_INDEX(1, 0)] = KEY_MENU, /* external menu key */
	[KEYMAP_INDEX(1, 1)] = KEY_SEND,
//	[KEYMAP_INDEX(1, 2)] = KEY_,
	[KEYMAP_INDEX(1, 3)] = KEY_LEFTSHIFT,
	[KEYMAP_INDEX(1, 4)] = KEY_A,
	[KEYMAP_INDEX(1, 5)] = KEY_F1, /* qwerty menu key */
	[KEYMAP_INDEX(1, 6)] = KEY_Q,

	[KEYMAP_INDEX(2, 0)] = KEY_U,
	[KEYMAP_INDEX(2, 1)] = KEY_7,
	[KEYMAP_INDEX(2, 2)] = KEY_K,
	[KEYMAP_INDEX(2, 3)] = KEY_J,
	[KEYMAP_INDEX(2, 4)] = KEY_M,
	[KEYMAP_INDEX(2, 5)] = KEY_DOT,
	[KEYMAP_INDEX(2, 6)] = KEY_8,

	[KEYMAP_INDEX(3, 0)] = KEY_5,
	[KEYMAP_INDEX(3, 1)] = KEY_6,
	[KEYMAP_INDEX(3, 2)] = KEY_B,
	[KEYMAP_INDEX(3, 3)] = KEY_H,
	[KEYMAP_INDEX(3, 4)] = KEY_N,
	[KEYMAP_INDEX(3, 5)] = KEY_SPACE,
	[KEYMAP_INDEX(3, 6)] = KEY_Y,

	[KEYMAP_INDEX(4, 0)] = KEY_4,
	[KEYMAP_INDEX(4, 1)] = KEY_R,
	[KEYMAP_INDEX(4, 2)] = KEY_V,
	[KEYMAP_INDEX(4, 3)] = KEY_G,
	[KEYMAP_INDEX(4, 4)] = KEY_C,
	[KEYMAP_INDEX(4, 5)] = KEY_EMAIL, // @
	[KEYMAP_INDEX(4, 6)] = KEY_T,

	[KEYMAP_INDEX(5, 0)] = KEY_2,
	[KEYMAP_INDEX(5, 1)] = KEY_W,
	[KEYMAP_INDEX(5, 2)] = KEY_LEFTALT,
	[KEYMAP_INDEX(5, 3)] = KEY_VOLUMEUP,
	[KEYMAP_INDEX(5, 4)] = KEY_S,
	[KEYMAP_INDEX(5, 5)] = KEY_Z,
	[KEYMAP_INDEX(5, 6)] = KEY_1,

	[KEYMAP_INDEX(6, 0)] = KEY_I,
	[KEYMAP_INDEX(6, 1)] = KEY_0,
	[KEYMAP_INDEX(6, 2)] = KEY_O,
	[KEYMAP_INDEX(6, 3)] = KEY_L,
	[KEYMAP_INDEX(6, 4)] = KEY_COMMA,
	[KEYMAP_INDEX(6, 5)] = KEY_RIGHTALT,
	[KEYMAP_INDEX(6, 6)] = KEY_9,

	[KEYMAP_INDEX(7, 0)] = KEY_3,
	[KEYMAP_INDEX(7, 1)] = KEY_E,
	[KEYMAP_INDEX(7, 2)] = KEY_COMPOSE,
	[KEYMAP_INDEX(7, 3)] = KEY_VOLUMEDOWN,
	[KEYMAP_INDEX(7, 4)] = KEY_X,
	[KEYMAP_INDEX(7, 5)] = KEY_F,
	[KEYMAP_INDEX(7, 6)] = KEY_D
};

static struct gpio_event_matrix_info trout_keypad_matrix_info = {
	.info.func = gpio_event_matrix_func,
	.keymap = trout_keymap,
	.output_gpios = trout_col_gpios,
	.input_gpios = trout_row_gpios,
	.noutputs = ARRAY_SIZE(trout_col_gpios),
	.ninputs = ARRAY_SIZE(trout_row_gpios),
	.settle_time.tv.nsec = 40 * NSEC_PER_USEC,
	.poll_time.tv.nsec = 20 * NSEC_PER_MSEC,
	.flags = GPIOKPF_LEVEL_TRIGGERED_IRQ | GPIOKPF_REMOVE_PHANTOM_KEYS |GPIOKPF_PRINT_UNMAPPED_KEYS /*| GPIOKPF_PRINT_MAPPED_KEYS*/
};

static struct gpio_event_direct_entry trout_keypad_nav_map[] = {
	{ TROUT_POWER_KEY,              KEY_POWER    },
	{ TROUT_GPIO_CAM_BTN_STEP1_N,   KEY_CAMERA-1 }, //steal KEY_HP
	{ TROUT_GPIO_CAM_BTN_STEP2_N,   KEY_CAMERA   },
};

static struct gpio_event_direct_entry trout_keypad_nav_map_evt2[] = {
	{ TROUT_POWER_KEY,              KEY_END      },
	{ TROUT_GPIO_CAM_BTN_STEP1_N,   KEY_CAMERA-1 }, //steal KEY_HP
	{ TROUT_GPIO_CAM_BTN_STEP2_N,   KEY_CAMERA   },
};

static struct gpio_event_input_info trout_keypad_nav_info = {
	.info.func = gpio_event_input_func,
	.flags = 0,
	.type = EV_KEY,
	.keymap = trout_keypad_nav_map,
	.keymap_size = ARRAY_SIZE(trout_keypad_nav_map)
};

static struct gpio_event_direct_entry trout_keypad_switch_map[] = {
	{ TROUT_GPIO_SLIDING_DET,       SW_LID       }
};

static struct gpio_event_input_info trout_keypad_switch_info = {
	.info.func = gpio_event_input_func,
	.flags = 0,
	.type = EV_SW,
	.keymap = trout_keypad_switch_map,
	.keymap_size = ARRAY_SIZE(trout_keypad_switch_map)
};

static struct gpio_event_info *trout_keypad_info[] = {
	&trout_keypad_matrix_info.info,
	&trout_keypad_nav_info.info,
	&trout_keypad_switch_info.info,
};

static struct gpio_event_platform_data trout_keypad_data = {
	.name = "trout-keypad",
	.info = trout_keypad_info,
	.info_count = ARRAY_SIZE(trout_keypad_info)
};

static struct platform_device trout_keypad_device = {
	.name = GPIO_EVENT_DEV_NAME,
	.id = 0,
	.dev		= {
		.platform_data	= &trout_keypad_data,
	},
};

static int __init trout_init_keypad(void)
{
	if (!machine_is_trout())
		return 0;

	switch (system_rev) {
	case 0:
		/* legacy default keylayout */
		break;
	case 1:
		/* v1 has a new keyboard layout */
		trout_keypad_matrix_info.keymap = trout_keymap_evt2_1;
		trout_keypad_matrix_info.output_gpios = trout_col_gpios_evt2;
		trout_keypad_matrix_info.input_gpios = trout_row_gpios_evt2;
		
		/* v1 has new direct keys */
		trout_keypad_nav_info.keymap = trout_keypad_nav_map_evt2;
		trout_keypad_nav_info.keymap_size = ARRAY_SIZE(trout_keypad_nav_map_evt2);

		/* userspace needs to know about these changes as well */
		trout_keypad_data.name = "trout-keypad-v2";
		break;
	default: /* 2, 3, 4 currently */
		/* v2 has a new keyboard layout */
		trout_keypad_matrix_info.keymap = trout_keymap_evt2_2;
		trout_keypad_matrix_info.output_gpios = trout_col_gpios_evt2;
		trout_keypad_matrix_info.input_gpios = trout_row_gpios_evt2;
		
		/* v2 has new direct keys */
		trout_keypad_nav_info.keymap = trout_keypad_nav_map_evt2;
		trout_keypad_nav_info.keymap_size = ARRAY_SIZE(trout_keypad_nav_map_evt2);

		/* userspace needs to know about these changes as well */
		if (!strcmp(keycaps, "qwertz")) {
			trout_keypad_data.name = "trout-keypad-qwertz";
		} else {
			trout_keypad_data.name = "trout-keypad-v3";
		}
		break;
	}
	return platform_device_register(&trout_keypad_device);
}

device_initcall(trout_init_keypad);

