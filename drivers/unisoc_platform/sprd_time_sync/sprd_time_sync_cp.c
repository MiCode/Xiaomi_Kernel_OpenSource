// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Spreadtrum Communications Inc.
 *
 * Used to keep time in sync with AP and other systems.
 */

#include <linux/cdev.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/timekeeping.h>
#include <linux/timekeeper_internal.h>
#include <linux/uaccess.h>

#include <linux/soc/sprd/sprd_time_sync.h>
#include <trace/hooks/timekeeping.h>

#define CN_TIMEZONE_OFFSET_SEC		(8 * 60 * 60)

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "sprd_time_sync_cp: " fmt

/* realize the time sync func */
void sprd_time_sync_fn(void *data, struct timekeeper *tk)
{
#ifdef CONFIG_SPRD_DEBUG
	struct tm input_tm;
#endif
	u64 monotime_ns, realtime_s, boottime_ns;

	monotime_ns = tk->tkr_mono.base;
	boottime_ns = monotime_ns + tk->offs_boot;
	realtime_s = tk->xtime_sec;

	/* Send msg to refnotify */
	sprd_send_ap_time();

#ifdef CONFIG_SPRD_DEBUG
	time64_to_tm(realtime_s, CN_TIMEZONE_OFFSET_SEC, &input_tm);

	pr_info("Send realtime: %ld-%d-%d %d:%d:%d monotime: %lldns boottime: %lldns\n",
		1900 + input_tm.tm_year,
		input_tm.tm_mon,
		input_tm.tm_mday,
		input_tm.tm_hour,
		input_tm.tm_min,
		input_tm.tm_sec,
		monotime_ns,
		boottime_ns);
	pr_info("name: %s pid: %d\n", current->comm, current->pid);
#endif
}

static int sprd_time_sync_cp_init(void)
{
	int ret;

	ret = register_trace_android_rvh_tk_based_time_sync(sprd_time_sync_fn, NULL);
	if (ret)
		pr_info("%s: register sprd_time_sync_fn failed(ret=%d)\n",__FUNCTION__, ret);

	return ret;
}

module_init(sprd_time_sync_cp_init)

MODULE_AUTHOR("Weidong Guan && Ruifeng Zhang");
MODULE_DESCRIPTION("sprd time sync cp driver");
MODULE_LICENSE("GPL v2");
