// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/fs.h>
#include <linux/module.h>
#include <linux/of_device.h>

#include <swpm_dbg_common_v1.h>
#include <swpm_module.h>

#include <swpm_dbg_fs_common.h>

/* #include <swpm_v6893_trace_event.h> */
/* #include <swpm_v6893_dbg_fs.h> */

static int __init swpm_v6893_dbg_early_initcall(void)
{
	return 0;
}
#ifndef MTK_SWPM_KERNEL_MODULE
subsys_initcall(swpm_v6893_dbg_early_initcall);
#endif

static int __init swpm_v6893_dbg_device_initcall(void)
{
	swpm_dbg_common_fs_init();

	return 0;
}

static int __init swpm_v6893_dbg_late_initcall(void)
{
	return 0;
}
#ifndef MTK_SWPM_KERNEL_MODULE
late_initcall_sync(swpm_v6893_dbg_late_initcall);
#endif

int __init swpm_v6893_dbg_init(void)
{
	int ret = 0;
#ifdef MTK_SWPM_KERNEL_MODULE
	ret = swpm_v6893_dbg_early_initcall();
#endif
	if (ret)
		goto swpm_v6893_dbg_init_fail;

	ret = swpm_v6893_dbg_device_initcall();

	if (ret)
		goto swpm_v6893_dbg_init_fail;

#ifdef MTK_SWPM_KERNEL_MODULE
	ret = swpm_v6893_dbg_late_initcall();
#endif

	if (ret)
		goto swpm_v6893_dbg_init_fail;

	return 0;
swpm_v6893_dbg_init_fail:
	return -EAGAIN;
}

void __exit swpm_v6893_dbg_exit(void)
{
	swpm_dbg_common_fs_exit();
}

module_init(swpm_v6893_dbg_init);
module_exit(swpm_v6893_dbg_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("v6893 software power model debug module");
MODULE_AUTHOR("MediaTek Inc.");
