// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2017 MediaTek Inc.
 */

#include <linux/module.h>
#include <asm/arch_timer.h>
#include <linux/init.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/math64.h>

#define FILTER_DATAPOINTS	16
#define FILTER_FREQ		10000000ULL /* 10 ms */

struct moving_average {
	uint64_t last_time;
	int64_t input[FILTER_DATAPOINTS];
	int64_t output;
	uint8_t cnt;
	uint8_t tail;
};

static struct moving_average moving_average_algo_mono;
static struct moving_average moving_average_algo_boot;
static DEFINE_SPINLOCK(moving_average_lock);

/* arch counter is 13M, mult is 161319385, shift is 21 */
static uint64_t arch_counter_to_ns(uint64_t cyc)
{
	u64 num, max = ULLONG_MAX;
	u32 mult = 161319385;
	u32 shift = 21;
	s64 nsec = 0;

	do_div(max, mult);
	if (cyc > max) {
		num = div64_u64(cyc, max);
		nsec = (((u64) max * mult) >> shift) * num;
		cyc -= num * max;
	}
	nsec += ((u64) cyc * mult) >> shift;
	return nsec;
}

static void moving_average_filter(struct moving_average *filter,
	uint64_t base_time, uint64_t archcounter_time)
{
	int i = 0;
	int64_t avg = 0;
	int64_t ret_avg = 0;

	if (base_time < filter->last_time + FILTER_FREQ)
		return;

	filter->last_time = base_time;

	filter->input[filter->tail++] = base_time - archcounter_time;
	filter->tail &= (FILTER_DATAPOINTS - 1);
	if (filter->cnt < FILTER_DATAPOINTS)
		filter->cnt++;

	for (i = 1, avg = 0; i < filter->cnt; i++)
		avg += (filter->input[i] - filter->input[0]);
	ret_avg = div_s64(avg, filter->cnt) + filter->input[0];
	WRITE_ONCE(filter->output, ret_avg);
}

static uint64_t get_filter_output(struct moving_average *filter)
{
	return READ_ONCE(filter->output);
}

static void filter_algo_init(struct moving_average *filter)
{
	spin_lock(&moving_average_lock);
	memset(filter, 0, sizeof(*filter));
	spin_unlock(&moving_average_lock);
}

void mtk_cam_timesync_init(uint8_t status)
{
	if (status) {
		filter_algo_init(&moving_average_algo_mono);
		filter_algo_init(&moving_average_algo_boot);
	}
}

u64 mtk_cam_get_time(u64 cyc)
{
	return arch_counter_to_ns(cyc);
}

uint64_t mtk_cam_timesync_to_monotonic(uint64_t hwclock)
{
	unsigned long flags = 0;
	uint64_t base_time = 0;
	uint64_t archcounter_time = 0;
	uint64_t reslut_time = 0;

	spin_lock(&moving_average_lock);

	local_irq_save(flags);
	base_time = ktime_to_ns(ktime_get());
	archcounter_time =
		arch_counter_to_ns(__arch_counter_get_cntvct_stable());
	local_irq_restore(flags);

	moving_average_filter(&moving_average_algo_mono,
		base_time, archcounter_time);

	reslut_time = arch_counter_to_ns(hwclock) +
		get_filter_output(&moving_average_algo_mono);

	spin_unlock(&moving_average_lock);
	return reslut_time;
}

uint64_t mtk_cam_timesync_to_boot(uint64_t hwclock)
{
	unsigned long flags = 0;
	uint64_t base_time = 0;
	uint64_t archcounter_time = 0;
	uint64_t reslut_time = 0;

	spin_lock(&moving_average_lock);

	local_irq_save(flags);
	base_time = ktime_get_boottime_ns();
	archcounter_time =
		arch_counter_to_ns(__arch_counter_get_cntvct_stable());
	local_irq_restore(flags);

	moving_average_filter(&moving_average_algo_boot,
		base_time, archcounter_time);

	reslut_time = arch_counter_to_ns(hwclock) +
		get_filter_output(&moving_average_algo_boot);

	spin_unlock(&moving_average_lock);
	return reslut_time;
}

