// SPDX-License-Identifier: GPL-2.0
/*
 * cpufreq-dbg.c - CPUFreq debug Driver
 *
 * Copyright (c) 2020 MediaTek Inc.
 * Chienwei Chang <chienwei.chang@mediatek.com>
 */

/* system includes */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/cpu.h>
#include <linux/io.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/init.h>
#include <linux/kobject.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_opp.h>
#include <linux/cpufreq.h>
#include <linux/uaccess.h>
#include <linux/delay.h>

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "[cpuhvfs]: " fmt


static int mtk_cpuhvfs_init(void)
{
	pr_info("cpuhvfs.ko init\n");
	return 0;
}
module_init(mtk_cpuhvfs_init)

static void mtk_cpuhvfs_exit(void)
{
	pr_info("cpuhvfs.ko exit\n");
}
module_exit(mtk_cpuhvfs_exit);

MODULE_DESCRIPTION("MTK CPU DVFS Platform Driver v0.1.1");
MODULE_AUTHOR("Chienwei Chang <chiewei.chang@mediatek.com>");
MODULE_LICENSE("GPL v2");
