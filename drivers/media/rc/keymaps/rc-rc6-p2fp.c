/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

#include <media/rc-map.h>
#include <linux/module.h>

static struct rc_map_table rc_rc6_p2fp[] = {
	{ 12, KEY_POWER},            /* power */
	{ 13, KEY_MUTE},              /* mute */
	{ 1, KEY_1},
	{ 2, KEY_2},
	{ 3, KEY_3},
	{ 4, KEY_4},
	{ 5, KEY_5},
	{ 6, KEY_6},
	{ 7, KEY_7},
	{ 8, KEY_8},
	{ 9, KEY_9},
	{ 0, KEY_0},
	{ 76, KEY_CHANNELUP},
	{ 77, KEY_CHANNELDOWN},
	{ 16, KEY_VOLUMEUP},
	{ 17, KEY_VOLUMEDOWN},
	{ 92, KEY_ENTER},
	{ 88, KEY_UP},
	{ 90, KEY_LEFT},
	{ 91, KEY_RIGHT},
	{ 89, KEY_DOWN},
	{ 10, KEY_BACK},
	{ 55, KEY_RECORD},
	{ 204, KEY_PROGRAM},
	{ 44, KEY_PLAY},
	{ 48, KEY_PAUSECD},
	{ 43, KEY_REWIND},
	{ 40, KEY_FASTFORWARD},
	{ 49, KEY_STOPCD},
	{ 15, KEY_INFO},
	{ 84, KEY_HOMEPAGE},
	{ 109, KEY_RED},
	{ 110, KEY_GREEN},
	{ 111, KEY_YELLOW},
	{ 112, KEY_BLUE},
	{ 64, KEY_MENU},
	{ 210, KEY_LIST},
	{ 75, KEY_SUBTITLE},
	{ 159, KEY_TV},
	{ 38, KEY_SWITCHVIDEOMODE},
};

static struct rc_map_list rc_rc6_p2fp_map = {
	.map = {
		.scan    = rc_rc6_p2fp,
		.size    = ARRAY_SIZE(rc_rc6_p2fp),
		.rc_type = RC_TYPE_RC6_6A_20,   /*RC6 IR type */
		.name    = RC_MAP_RC6_P2FP,
	}
};

static int __init init_rc_map_rc_rc6_p2fp(void)
{
	return rc_map_register(&rc_rc6_p2fp_map);
}

static void __exit exit_rc_map_rc_rc6_p2fp(void)
{
	rc_map_unregister(&rc_rc6_p2fp_map);
}

module_init(init_rc_map_rc_rc6_p2fp)
module_exit(exit_rc_map_rc_rc6_p2fp)

MODULE_LICENSE("GPL v2");
