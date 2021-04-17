// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#define pr_fmt(fmt) "timesync " fmt

#include <linux/slab.h>
#include <linux/timekeeping.h>
#include <linux/timer.h>
#include <linux/suspend.h>
#include <asm/arch_timer.h>
#include <linux/workqueue.h>
#include <linux/atomic.h>

#include "timesync.h"
#include "sensor_comm.h"

#define TIMESYNC_DURATION	10000
#define TIMESYNC_START_DURATION	3000
#define FILTER_DATA_POINTS	16
#define FILTER_TIMEOUT		10000000000ULL /* 10 seconds, ~100us drift */
#define FILTER_FREQ		10000000ULL /* 10 ms */

struct moving_average {
	int64_t last_time;
	int64_t input[FILTER_DATA_POINTS];
	atomic64_t output;
	int64_t output_debug;
	uint8_t cnt;
	uint8_t tail;
};

static struct moving_average timesync_filter;
static bool timesync_suspend_flag;
static struct timer_list timesync_timer;
static struct work_struct timesync_work;
static struct wakeup_source wakeup_src;

/* arch counter is 13M, mult is 161319385, shift is 21 */
static inline int64_t arch_counter_to_ns(int64_t cyc)
{
#define ARCH_TIMER_MULT 161319385
#define ARCH_TIMER_SHIFT 21

	return (cyc * ARCH_TIMER_MULT) >> ARCH_TIMER_SHIFT;
}

static void moving_average_filter(struct moving_average *filter,
		int64_t host_time, int64_t scp_time)
{
	int i = 0;
	int64_t avg = 0, ret_avg = 0, delta = 0;

	if (host_time > filter->last_time + FILTER_TIMEOUT ||
	    filter->last_time == 0) {
		filter->tail = 0;
		filter->cnt = 0;
	} else if (host_time < filter->last_time + FILTER_FREQ) {
		return;
	}
	filter->last_time = host_time;

	filter->input[filter->tail++] = host_time - scp_time;
	filter->tail &= (FILTER_DATA_POINTS - 1);
	if (filter->cnt < FILTER_DATA_POINTS)
		filter->cnt++;

	for (i = 1, avg = 0; i < filter->cnt; i++)
		avg += (filter->input[i] - filter->input[0]);
	ret_avg = div_s64(avg, filter->cnt) + filter->input[0];
	if (!filter->output_debug) {
		filter->output_debug = ret_avg;
	} else {
		delta = filter->output_debug - ret_avg;
		if (unlikely(delta >= 2500000) || unlikely(delta <= -2500000))
			pr_err("host with scp jump too large %lld\n", delta);
		filter->output_debug = ret_avg;
	}

	atomic64_set(&filter->output, ret_avg);
}

static int timesync_comm_with_nolock(void)
{
	int ret = 0;
	unsigned long flags;
	struct sensor_comm_ctrl *ctrl = NULL;
	struct sensor_comm_timesync *time = NULL;
	int64_t now_time = 0, arch_counter = 0;

	if (READ_ONCE(timesync_suspend_flag))
		return 0;

	ctrl = kzalloc(sizeof(*ctrl) + sizeof(*time), GFP_KERNEL);
	if (!ctrl)
		return -ENOMEM;
	ctrl->sensor_type = 0;
	ctrl->command = SENS_COMM_CTRL_TIMESYNC_CMD;
	ctrl->length = sizeof(*time);
	time = (struct sensor_comm_timesync *)ctrl->data;

	local_irq_save(flags);
	now_time = ktime_get_boot_ns();
	arch_counter = arch_counter_get_cntvct();
	local_irq_restore(flags);
	pr_info("host boottime %lld\n", now_time);

	time->host_timestamp = now_time;
	time->host_archcounter = arch_counter;
	ret = sensor_comm_ctrl_send(ctrl, sizeof(*ctrl) + ctrl->length);
	kfree(ctrl);

	return ret;
}

static int timesync_comm_with(void)
{
	int ret = 0;

	if (READ_ONCE(timesync_suspend_flag))
		return 0;

	__pm_stay_awake(&wakeup_src);
	ret = timesync_comm_with_nolock();
	__pm_relax(&wakeup_src);
	return ret;
}

static void timesync_work_func(struct work_struct *work)
{
	timesync_comm_with();
}

static void timesync_timer_func(unsigned long data)
{
	schedule_work(&timesync_work);
	timesync_start();
}

void timesync_filter_set(int64_t scp_timestamp, int64_t scp_archcounter)
{
	unsigned long flags;
	int64_t host_timestamp = 0, host_archcounter = 0;
	int64_t ipi_transfer_time = 0;

	if (!timekeeping_rtc_skipresume()) {
		if (READ_ONCE(timesync_suspend_flag))
			return;
	}

	local_irq_save(flags);
	host_timestamp = ktime_get_boot_ns();
	host_archcounter = arch_counter_get_cntvct();
	local_irq_restore(flags);
	ipi_transfer_time = arch_counter_to_ns(host_archcounter -
		scp_archcounter);
	scp_timestamp = scp_timestamp + ipi_transfer_time;

	moving_average_filter(&timesync_filter, host_timestamp, scp_timestamp);
}

int64_t timesync_filter_get(void)
{
	return atomic64_read(&timesync_filter.output);
}

void timesync_start(void)
{
	mod_timer(&timesync_timer,
		  jiffies + msecs_to_jiffies(TIMESYNC_DURATION));
}

void timesync_stop(void)
{
	del_timer_sync(&timesync_timer);
}

void timesync_resume(void)
{
	pr_info("host resume boottime %lld\n", ktime_get_boot_ns());
	WRITE_ONCE(timesync_suspend_flag, false);
	timesync_comm_with();
}

void timesync_suspend(void)
{
	pr_info("host suspend boottime %lld\n", ktime_get_boot_ns());
	WRITE_ONCE(timesync_suspend_flag, true);
}

int timesync_init(void)
{
	INIT_WORK(&timesync_work, timesync_work_func);
	timesync_timer.expires =
		jiffies + msecs_to_jiffies(TIMESYNC_START_DURATION);
	timesync_timer.function = timesync_timer_func;
	init_timer(&timesync_timer);
	wakeup_source_init(&wakeup_src, "timesync");
	return 0;
}

void timesync_exit(void)
{
	timesync_stop();
	flush_work(&timesync_work);
}
