/*
 * Copyright (c) 2014, 2016, The Linux Foundation. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/notifier.h>
#include <linux/debugfs.h>
#include <linux/fs.h>
#include <linux/init.h>

ATOMIC_NOTIFIER_HEAD(show_mem_notifier);

int show_mem_notifier_register(struct notifier_block *nb)
{
	return atomic_notifier_chain_register(&show_mem_notifier, nb);
}

int show_mem_notifier_unregister(struct notifier_block *nb)
{
	return  atomic_notifier_chain_unregister(&show_mem_notifier, nb);
}

void show_mem_call_notifiers(void)
{
	atomic_notifier_call_chain(&show_mem_notifier, 0, NULL);
}

static int show_mem_notifier_get(void *dat, u64 *val)
{
	show_mem_call_notifiers();
	*val = 0;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(show_mem_notifier_debug_ops, show_mem_notifier_get,
				NULL, "%llu\n");

int show_mem_notifier_debugfs_register(void)
{
	debugfs_create_file("show_mem_notifier", 0664, NULL, NULL,
				&show_mem_notifier_debug_ops);

	return 0;
}
late_initcall(show_mem_notifier_debugfs_register);
