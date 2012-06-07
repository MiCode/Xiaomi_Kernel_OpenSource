/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <media/rc-map.h>

static struct rc_map_table philips[] = {

	{ 0x00, KEY_NUMERIC_0 },
	{ 0x01, KEY_NUMERIC_1 },
	{ 0x02, KEY_NUMERIC_2 },
	{ 0x03, KEY_NUMERIC_3 },
	{ 0x04, KEY_NUMERIC_4 },
	{ 0x05, KEY_NUMERIC_5 },
	{ 0x06, KEY_NUMERIC_6 },
	{ 0x07, KEY_NUMERIC_7 },
	{ 0x08, KEY_NUMERIC_8 },
	{ 0x09, KEY_NUMERIC_9 },
	{ 0xF4, KEY_SOUND },
	{ 0xF3, KEY_SCREEN },	/* Picture */

	{ 0x10, KEY_VOLUMEUP },
	{ 0x11, KEY_VOLUMEDOWN },
	{ 0x0d, KEY_MUTE },
	{ 0x20, KEY_CHANNELUP },
	{ 0x21, KEY_CHANNELDOWN },
	{ 0x0A, KEY_BACK },
	{ 0x0f, KEY_INFO },
	{ 0x5c, KEY_OK },
	{ 0x58, KEY_UP },
	{ 0x59, KEY_DOWN },
	{ 0x5a, KEY_LEFT },
	{ 0x5b, KEY_RIGHT },
	{ 0xcc, KEY_PAUSE },
	{ 0x6d, KEY_PVR },	/* Demo */
	{ 0x40, KEY_EXIT },
	{ 0x6e, KEY_PROG1 },	/* Scenea */
	{ 0x6f, KEY_MODE },	/* Dual */
	{ 0x70, KEY_SLEEP },
	{ 0xf5, KEY_TUNER },	/* Format */

	{ 0x4f, KEY_TV },
	{ 0x3c, KEY_NEW },	/* USB */
	{ 0x38, KEY_COMPOSE },	/* Source */
	{ 0x54, KEY_MENU },

	{ 0x0C, KEY_POWER },
};

static struct rc_map_list rc6_philips_map = {
	.map = {
		.scan    = philips,
		.size    = ARRAY_SIZE(philips),
		.rc_type = RC_TYPE_RC6,
		.name    = RC_MAP_RC6_PHILIPS,
	}
};

static int __init init_rc_map_rc6_philips(void)
{
	return rc_map_register(&rc6_philips_map);
}

static void __exit exit_rc_map_rc6_philips(void)
{
	rc_map_unregister(&rc6_philips_map);
}

module_init(init_rc_map_rc6_philips)
module_exit(exit_rc_map_rc6_philips)

MODULE_DESCRIPTION("Philips Remote Keymap ");
MODULE_LICENSE("GPL v2");
