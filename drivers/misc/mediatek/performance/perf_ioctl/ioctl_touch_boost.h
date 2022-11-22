/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/jiffies.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/utsname.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/moduleparam.h>
#include <linux/uaccess.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/notifier.h>
#include <linux/suspend.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/hrtimer.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/ioctl.h>

struct _TOUCH_BOOST_PACKAGE {
	__s32 cmd;
	__s32 enable;
	__s32 boost_duration;
	__s32 active_time;
	__s32 deboost_when_render;
	__s32 idleprefer_ta;
	__s32 idleprefer_fg;
	__s32 util_ta;
	__s32 util_fg;
	__s32 cpufreq_c0;
	__s32 cpufreq_c1;
	__s32 cpufreq_c2;
};

#define TOUCH_BOOST_GET_CMD   _IOW('g', 1, struct _TOUCH_BOOST_PACKAGE)
