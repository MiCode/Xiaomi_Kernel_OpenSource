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

static struct rc_map_table tvx_HD_nec[] = {
	{ 1768554496, KEY_POWER},            /* power */
	{ 1769603072, KEY_MUTE},              /* mute */
	{ 1775763456, KEY_1},
	{ 1776549888, KEY_2},
	{ 1775501312, KEY_3},
	{ 1776812032, KEY_4},
	{ 1762000896, KEY_5},
	{ 1764098048, KEY_6},
	{ 1763311616, KEY_7},
	{ 1765408768, KEY_8},
	{ 1763049472, KEY_9},
	{ 1774714880, KEY_0},
	{ 1770258432, KEY_CHANNELUP},
	{ 1771569152, KEY_CHANNELDOWN},
	{ 1772355584, KEY_VOLUMEUP},
	{ 1772617728, KEY_VOLUMEDOWN},
	{ 1769340928, KEY_ENTER},
	{ 1770389504, KEY_UP},
	{ 1775632384, KEY_LEFT},
	{ 1777729536, KEY_RIGHT},
	{ 1774583808, KEY_DOWN},
	{ 1766457344, KEY_BACK},
};

static struct rc_map_list tvx_HD_nec_map = {
	.map = {
		.scan    = tvx_HD_nec,
		.size    = ARRAY_SIZE(tvx_HD_nec),
		.rc_type = RC_TYPE_NEC,	/* NEC IR type */
		.name    = RC_MAP_TVX_HD_NEC,
	}
};

static int __init init_rc_map_tvx_HD_nec(void)
{
	return rc_map_register(&tvx_HD_nec_map);
}

static void __exit exit_rc_map_tvx_HD_nec(void)
{
	rc_map_unregister(&tvx_HD_nec_map);
}

module_init(init_rc_map_tvx_HD_nec)
module_exit(exit_rc_map_tvx_HD_nec)

MODULE_LICENSE("GPL v2");
