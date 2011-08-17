/* arch/arm/mach-msm/board-sapphire-keypad.c
 * Copyright (C) 2007-2009 HTC Corporation.
 * Author: Thomas Tsai <thomas_tsai@htc.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
*/

#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/gpio_event.h>
#include <asm/mach-types.h>
#include "gpio_chip.h"
#include "board-sapphire.h"
static char *keycaps = "--qwerty";
#undef MODULE_PARAM_PREFIX
#define MODULE_PARAM_PREFIX "board_sapphire."
module_param_named(keycaps, keycaps, charp, 0);


static unsigned int sapphire_col_gpios[] = { 35, 34 };

/* KP_MKIN2 (GPIO40) is not used? */
static unsigned int sapphire_row_gpios[] = { 42, 41 };

#define KEYMAP_INDEX(col, row) ((col)*ARRAY_SIZE(sapphire_row_gpios) + (row))

/*scan matrix key*/
/* HOME(up) MENU (up) Back Search */
static const unsigned short sapphire_keymap2[ARRAY_SIZE(sapphire_col_gpios) * ARRAY_SIZE(sapphire_row_gpios)] = {
	[KEYMAP_INDEX(0, 0)] = KEY_COMPOSE,
	[KEYMAP_INDEX(0, 1)] = KEY_BACK,

	[KEYMAP_INDEX(1, 0)] = KEY_MENU,
	[KEYMAP_INDEX(1, 1)] = KEY_SEND,
};

/* HOME(up) + MENU (down)*/
static const unsigned short sapphire_keymap1[ARRAY_SIZE(sapphire_col_gpios) *
					ARRAY_SIZE(sapphire_row_gpios)] = {
	[KEYMAP_INDEX(0, 0)] = KEY_BACK,
	[KEYMAP_INDEX(0, 1)] = KEY_MENU,

	[KEYMAP_INDEX(1, 0)] = KEY_HOME,
	[KEYMAP_INDEX(1, 1)] = KEY_SEND,
};

/* MENU(up) + HOME (down)*/
static const unsigned short sapphire_keymap0[ARRAY_SIZE(sapphire_col_gpios) *
					ARRAY_SIZE(sapphire_row_gpios)] = {
	[KEYMAP_INDEX(0, 0)] = KEY_BACK,
	[KEYMAP_INDEX(0, 1)] = KEY_HOME,

	[KEYMAP_INDEX(1, 0)] = KEY_MENU,
	[KEYMAP_INDEX(1, 1)] = KEY_SEND,
};

static struct gpio_event_matrix_info sapphire_keypad_matrix_info = {
	.info.func = gpio_event_matrix_func,
	.keymap = sapphire_keymap2,
	.output_gpios = sapphire_col_gpios,
	.input_gpios = sapphire_row_gpios,
	.noutputs = ARRAY_SIZE(sapphire_col_gpios),
	.ninputs = ARRAY_SIZE(sapphire_row_gpios),
	.settle_time.tv.nsec = 40 * NSEC_PER_USEC,
	.poll_time.tv.nsec = 20 * NSEC_PER_MSEC,
	.debounce_delay.tv.nsec = 50 * NSEC_PER_MSEC,
	.flags = GPIOKPF_LEVEL_TRIGGERED_IRQ |
		 GPIOKPF_REMOVE_PHANTOM_KEYS |
		 GPIOKPF_PRINT_UNMAPPED_KEYS /*| GPIOKPF_PRINT_MAPPED_KEYS*/
};

static struct gpio_event_direct_entry sapphire_keypad_nav_map[] = {
	{ SAPPHIRE_POWER_KEY,              KEY_END        },
	{ SAPPHIRE_VOLUME_UP,              KEY_VOLUMEUP   },
	{ SAPPHIRE_VOLUME_DOWN,            KEY_VOLUMEDOWN },
};

static struct gpio_event_input_info sapphire_keypad_nav_info = {
	.info.func = gpio_event_input_func,
	.flags = 0,
	.type = EV_KEY,
	.keymap = sapphire_keypad_nav_map,
	.debounce_time.tv.nsec = 20 * NSEC_PER_MSEC,
	.keymap_size = ARRAY_SIZE(sapphire_keypad_nav_map)
};

static struct gpio_event_info *sapphire_keypad_info[] = {
	&sapphire_keypad_matrix_info.info,
	&sapphire_keypad_nav_info.info,
};

static struct gpio_event_platform_data sapphire_keypad_data = {
	.name = "sapphire-keypad",
	.info = sapphire_keypad_info,
	.info_count = ARRAY_SIZE(sapphire_keypad_info)
};

static struct platform_device sapphire_keypad_device = {
	.name = GPIO_EVENT_DEV_NAME,
	.id = 0,
	.dev		= {
		.platform_data	= &sapphire_keypad_data,
	},
};

static int __init sapphire_init_keypad(void)
{
	if (!machine_is_sapphire())
		return 0;

	switch (sapphire_get_hwid()) {
	case 0:
		sapphire_keypad_matrix_info.keymap = sapphire_keymap0;
		break;
	default:
		if(system_rev != 0x80)
			sapphire_keypad_matrix_info.keymap = sapphire_keymap1;
		break;
	}
	return platform_device_register(&sapphire_keypad_device);
}

device_initcall(sapphire_init_keypad);

