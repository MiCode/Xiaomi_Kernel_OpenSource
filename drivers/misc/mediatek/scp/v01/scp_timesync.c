/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/fs.h>           /* needed by file_operations* */
#include <linux/miscdevice.h>   /* needed by miscdevice* */
#include <linux/sysfs.h>
#include <linux/device.h>       /* needed by device_* */
#include <linux/fs.h>           /* needed by file_operations* */
#include <mt-plat/sync_write.h>
#include <scp_helper.h>
#include <scp_ipi.h>
#include <linux/delay.h>
#include "scp_helper.h"
#include "scp_excep.h"
#include <linux/notifier.h>
#include <asm/arch_timer.h>
#include <linux/suspend.h>

#define mtk_timer_src_count(...)    arch_counter_get_cntvct(__VA_ARGS__)
#define TIMESYNC_TIMEOUT      (60 * 60 * HZ)

MODULE_LICENSE("GPL");

static void tinysys_time_sync(void);
static struct timer_list scp_timesync_timer;
static struct scp_work_struct scp_timesync_work;

struct timesync_ctrl_s {
	unsigned int base;
	unsigned int size;
	unsigned int ts_h;
	unsigned int ts_l;
	unsigned int clk_h;
	unsigned int clk_l;
};
static struct timesync_ctrl_s ts_ctl;
static void scp_timesync_timestamp(unsigned long long src, unsigned int *ts_h
							, unsigned int *ts_l)
{
	*ts_l = (unsigned int)(src & 0x00000000FFFFFFFF);
	*ts_h = (unsigned int)((src & 0xFFFFFFFF00000000) >> 32);
}

void scp_timesync_ts_get(unsigned int *ts_h, unsigned int *ts_l)
{
	unsigned long long ap_ts;

	ap_ts = sched_clock();

	scp_timesync_timestamp(ap_ts, ts_h, ts_l);
}

void scp_timesync_clk_get(unsigned int *clk_h, unsigned int *clk_l)
{
	unsigned long long ap_clk;

	ap_clk = mtk_timer_src_count();

	scp_timesync_timestamp(ap_clk, clk_h, clk_l);
}
static void tinysys_time_sync(void)
{
	int ret;
	int timeout = 10;

	do {
		scp_timesync_ts_get(&ts_ctl.ts_h, &ts_ctl.ts_l);
		scp_timesync_clk_get(&ts_ctl.clk_h, &ts_ctl.clk_l);
		ret = scp_ipi_send(IPI_SCP_TIMER, &ts_ctl,
				sizeof(struct timesync_ctrl_s), 0, SCP_A_ID);

		if (ret == DONE)
			break;
		timeout--;
		mdelay(5);
	} while (timeout > 0);
#if 0
	pr_notice("SCP: timer sync log:%d, %u, %u\n", ret, ts_ctl.ts_h,
			ts_ctl.ts_l);
	pr_notice("SCP: timer sync log:%d, %u, %u\n", ret, ts_ctl.clk_h,
			ts_ctl.clk_l);
#endif

}

/*
 * TODO: what should we do when hibernation ?
 */
static int scp_pm_event(struct notifier_block *notifier
			, unsigned long pm_event, void *unused)
{
	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		tinysys_time_sync();
		return NOTIFY_DONE;
	case PM_POST_SUSPEND:
		tinysys_time_sync();
		return NOTIFY_DONE;
	}
	return NOTIFY_OK;
}

static struct notifier_block scp_pm_notifier_block = {
	.notifier_call = scp_pm_event,
	.priority = 0,
};

static int timesync_event(struct notifier_block *this
			, unsigned long event, void *ptr)
{
	switch (event) {
	case SCP_EVENT_READY:
		if (is_scp_ready(SCP_A_ID))
			tinysys_time_sync();
		break;
	}
	return NOTIFY_DONE;
}

static struct notifier_block timesync_notifier = {
	.notifier_call = timesync_event,
};


static void scp_ts_timeout(unsigned long data)
{
	scp_schedule_work(&scp_timesync_work);
	scp_timesync_timer.expires = jiffies + TIMESYNC_TIMEOUT;
	add_timer(&scp_timesync_timer);
}

static void timesync_ws(struct work_struct *ws)
{
	/*
	 * static unsigned int scp_sync_cnt = 0;
	 * pr_debug("resync time about %d sec (%d)\n",
	 * TIMESYNC_TIMEOUT, scp_sync_cnt++);
	 */
	tinysys_time_sync();
}

static int __init init_scp_timesync(void)
{
	int ret;

	ret = register_pm_notifier(&scp_pm_notifier_block);
	if (unlikely(ret != 0)) {
		pr_err("[SCP] misc register failed\n");
		return ret;
	}
	scp_A_register_notify(&timesync_notifier);

	INIT_WORK(&scp_timesync_work.work, timesync_ws);
	setup_timer(&scp_timesync_timer, &scp_ts_timeout, 0);
	scp_timesync_timer.expires = jiffies + TIMESYNC_TIMEOUT;
	add_timer(&scp_timesync_timer);


	return 0;
}

static void __exit exit_scp_timesync(void)
{

}

module_init(init_scp_timesync);
module_exit(exit_scp_timesync);

