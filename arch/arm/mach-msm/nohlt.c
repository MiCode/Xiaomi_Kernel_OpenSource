/* Copyright (c) 2009, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
/*
 * MSM architecture driver to control arm halt behavior
 */

#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/fs.h>
#include <asm/system.h>

static int set_nohalt(void *data, u64 val)
{
	if (val)
		disable_hlt();
	else
		enable_hlt();
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(nohalt_ops, NULL, set_nohalt, "%llu\n");

static int __init init_hlt_debug(void)
{
	debugfs_create_file("nohlt", 0200, NULL, NULL, &nohalt_ops);

	return 0;
}

late_initcall(init_hlt_debug);
