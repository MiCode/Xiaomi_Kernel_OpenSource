/* Copyright (c) 2009, 2013, The Linux Foundation. All rights reserved.
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
#include <linux/cpu.h>

static int set_nohalt(void *data, u64 val)
{
	if (val)
		cpu_idle_poll_ctrl(true);
	else
		cpu_idle_poll_ctrl(false);
	return 0;
}

extern int cpu_idle_force_poll;

static int get_nohalt(void *data, u64 *val)
{
	*val = (unsigned int)cpu_idle_force_poll;

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(nohalt_ops, get_nohalt, set_nohalt, "%llu\n");

static int __init init_hlt_debug(void)
{
	debugfs_create_file("nohlt", 0600, NULL, NULL, &nohalt_ops);

	return 0;
}

late_initcall(init_hlt_debug);
