/*
 * arch/arm/mach-tegra/nvdumper.c
 *
 * Copyright (C) 2011 NVIDIA Corporation
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

#include <linux/errno.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/reboot.h>
#include "board.h"

#define NVDUMPER_CLEAN 0xf000caf3U
#define NVDUMPER_DIRTY 0xdeadbeefU

static uint32_t *nvdumper_ptr;

static int get_dirty_state(void)
{
	uint32_t val;

	val = ioread32(nvdumper_ptr);
	if (val == NVDUMPER_DIRTY)
		return 1;
	else if (val == NVDUMPER_CLEAN)
		return 0;
	else
		return -1;
}

static void set_dirty_state(int dirty)
{
	if (dirty)
		iowrite32(NVDUMPER_DIRTY, nvdumper_ptr);
	else
		iowrite32(NVDUMPER_CLEAN, nvdumper_ptr);
}

static int nvdumper_reboot_cb(struct notifier_block *nb,
		unsigned long event, void *unused)
{
	printk(KERN_INFO "nvdumper: rebooting cleanly.\n");
	set_dirty_state(0);
	return NOTIFY_DONE;
}

struct notifier_block nvdumper_reboot_notifier = {
	.notifier_call = nvdumper_reboot_cb,
};

static int __init nvdumper_init(void)
{
	int ret, dirty;

	if (!nvdumper_reserved) {
		printk(KERN_INFO "nvdumper: not configured\n");
		return -ENOTSUPP;
	}
	nvdumper_ptr = ioremap_nocache(nvdumper_reserved,
			NVDUMPER_RESERVED_SIZE);
	if (!nvdumper_ptr) {
		printk(KERN_INFO "nvdumper: failed to ioremap memory "
			"at 0x%08lx\n", nvdumper_reserved);
		return -EIO;
	}
	ret = register_reboot_notifier(&nvdumper_reboot_notifier);
	if (ret)
		return ret;
	dirty = get_dirty_state();
	switch (dirty) {
	case 0:
		printk(KERN_INFO "nvdumper: last reboot was clean\n");
		break;
	case 1:
		printk(KERN_INFO "nvdumper: last reboot was dirty\n");
		break;
	default:
		printk(KERN_INFO "nvdumper: last reboot was unknown\n");
		break;
	}
	set_dirty_state(1);
	return 0;
}

static void __exit nvdumper_exit(void)
{
	unregister_reboot_notifier(&nvdumper_reboot_notifier);
	set_dirty_state(0);
	iounmap(nvdumper_ptr);
}

module_init(nvdumper_init);
module_exit(nvdumper_exit);

MODULE_LICENSE("GPL");
