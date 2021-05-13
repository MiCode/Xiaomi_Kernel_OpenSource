// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/syscalls.h>

#include <swpm_module.h>

static int __init swpm_init(void)
{
	int ret = 0;

	return ret;
}

#ifdef MTK_SWPM_KERNEL_MODULE
static void __exit swpm_deinit(void)
{
}

module_init(swpm_init);
module_exit(swpm_deinit);
#else
device_initcall_sync(swpm_init);
#endif

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("mtk swpm module");
MODULE_AUTHOR("MediaTek Inc.");
