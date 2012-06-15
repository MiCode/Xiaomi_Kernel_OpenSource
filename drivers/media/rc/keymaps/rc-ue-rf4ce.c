/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

static struct rc_map_table ue_rf4ce[] = {
	{ 0x0a, KEY_SETUP },
	{ 0x6b, KEY_POWER },
	{ 0x00, KEY_OK },
	{ 0x03, KEY_LEFT },
	{ 0x04, KEY_RIGHT },
	{ 0x01, KEY_UP },
	{ 0x02, KEY_DOWN },
	{ 0x53, KEY_HOMEPAGE },
	{ 0x0d, KEY_EXIT },
	{ 0x72, KEY_TV },
	{ 0x73, KEY_VIDEO },
	{ 0x74, KEY_PC },
	{ 0x71, KEY_AUX },
	{ 0x45, KEY_STOP },
	{ 0x0b, KEY_LIST },
	{ 0x47, KEY_RECORD },
	{ 0x48, KEY_REWIND },
	{ 0x44, KEY_PLAY },
	{ 0x49, KEY_FASTFORWARD },
	{ 0x4c, KEY_BACK },
	{ 0x46, KEY_PAUSE },
	{ 0x4b, KEY_NEXT },
	{ 0x41, KEY_VOLUMEUP },
	{ 0x42, KEY_VOLUMEDOWN },
	{ 0x32, KEY_LAST },
	{ 0x43, KEY_MUTE },
	{ 0x30, KEY_CHANNELUP },
	{ 0x31, KEY_CHANNELDOWN },

	{ 0x20, KEY_0 },
	{ 0x21, KEY_1 },
	{ 0x22, KEY_2 },
	{ 0x23, KEY_3 },
	{ 0x24, KEY_4 },
	{ 0x25, KEY_5 },
	{ 0x26, KEY_6 },
	{ 0x27, KEY_7 },
	{ 0x28, KEY_8 },
	{ 0x29, KEY_9 },
	{ 0x34, KEY_TV2 },
	{ 0x2b, KEY_ENTER },
	{ 0x35, KEY_INFO },
	{ 0x09, KEY_MENU },
};

static struct rc_map_list ue_rf4ce_map = {
	.map = {
		.scan    = ue_rf4ce,
		.size    = ARRAY_SIZE(ue_rf4ce),
		.rc_type = RC_TYPE_OTHER,
		.name    = RC_MAP_UE_RF4CE,
	}
};

static int __init init_rc_map_ue_rf4ce(void)
{
	return rc_map_register(&ue_rf4ce_map);
}

static void __exit exit_rc_map_ue_rf4ce(void)
{
	rc_map_unregister(&ue_rf4ce_map);
}

module_init(init_rc_map_ue_rf4ce)
module_exit(exit_rc_map_ue_rf4ce)

MODULE_DESCRIPTION("UE RF4CE Remote Keymap ");
MODULE_LICENSE("GPL v2");
