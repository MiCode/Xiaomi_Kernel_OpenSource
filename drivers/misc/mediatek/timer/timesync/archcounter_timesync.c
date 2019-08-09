/*
 * Copyright (C) 2017 MediaTek Inc.
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

#include <linux/module.h>
#include <asm/arch_timer.h>
#include <linux/init.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>

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
	int32_t avg;
	int64_t ret_avg = 0;

	if (base_time < filter->last_time + FILTER_FREQ)
		return;

	filter->last_time = base_time;

	filter->input[filter->tail++] = base_time - archcounter_time;
	filter->tail &= (FILTER_DATAPOINTS - 1);
	if (filter->cnt < FILTER_DATAPOINTS)
		filter->cnt++;

	for (i = 1, avg = 0; i < filter->cnt; i++)
		avg += (int32_t)(filter->input[i] - filter->input[0]);
	ret_avg = (avg / filter->cnt) + filter->input[0];
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

void archcounter_timesync_init(uint8_t status)
{
	if (status) {
		filter_algo_init(&moving_average_algo_mono);
		filter_algo_init(&moving_average_algo_boot);
	}
}

uint64_t archcounter_timesync_to_monotonic(uint64_t hwclock)
{
	unsigned long flags = 0;
	uint64_t base_time = 0;
	uint64_t archcounter_time = 0;
	uint64_t reslut_time = 0;

	spin_lock(&moving_average_lock);

	local_irq_save(flags);
	base_time = ktime_to_ns(ktime_get());
	archcounter_time = arch_counter_to_ns(arch_counter_get_cntvct());
	local_irq_restore(flags);

	moving_average_filter(&moving_average_algo_mono,
		base_time, archcounter_time);

	reslut_time = arch_counter_to_ns(hwclock) +
		get_filter_output(&moving_average_algo_mono);

	spin_unlock(&moving_average_lock);
	return reslut_time;
}

uint64_t archcounter_timesync_to_boot(uint64_t hwclock)
{
	unsigned long flags = 0;
	uint64_t base_time = 0;
	uint64_t archcounter_time = 0;
	uint64_t reslut_time = 0;

	spin_lock(&moving_average_lock);

	local_irq_save(flags);
	base_time = ktime_get_boot_ns();
	archcounter_time = arch_counter_to_ns(arch_counter_get_cntvct());
	local_irq_restore(flags);

	moving_average_filter(&moving_average_algo_boot,
		base_time, archcounter_time);

	reslut_time = arch_counter_to_ns(hwclock) +
		get_filter_output(&moving_average_algo_boot);

	spin_unlock(&moving_average_lock);
	return reslut_time;
}

/* #define CONFIG_TIMESYNC_TEST */
#ifdef CONFIG_TIMESYNC_TEST
struct timer_list timesync_test_timer;
struct work_struct timesync_test_work;
#define TIMESYNC_TEST_TIME 33

static void timesync_test_work_func(struct work_struct *work)
{
	unsigned long flags = 0;
	uint64_t base_time = 0;
	uint64_t archcounter_time = 0;
	uint64_t algo_offset = 0;

	local_irq_save(flags);
	base_time = ktime_to_ns(ktime_get());
	archcounter_time = arch_counter_to_ns(arch_counter_get_cntvct());
	local_irq_restore(flags);

	moving_average_filter(&moving_average_algo_mono,
		base_time, archcounter_time);
	algo_offset = get_filter_output(&moving_average_algo_mono);

	pr_debug("[archcounter_timesync] monotonic=%lld, archcounter=%lld\n",
		base_time, archcounter_time);
	pr_debug("[archcounter_timesync] raw_offset=%lld\n",
		base_time - archcounter_time);
	pr_debug("[archcounter_timesync] algo_offset=%lld\n", algo_offset);


	local_irq_save(flags);
	base_time = ktime_get_boot_ns();
	archcounter_time = arch_counter_to_ns(arch_counter_get_cntvct());
	local_irq_restore(flags);

	moving_average_filter(&moving_average_algo_mono,
		base_time, archcounter_time);
	algo_offset = get_filter_output(&moving_average_algo_mono);

	pr_debug("[archcounter_timesync] boot=%lld, archcounter=%lld\n",
		base_time, archcounter_time);
	pr_debug("[archcounter_timesync] raw_offset=%lld\n",
		base_time - archcounter_time);
	pr_debug("[archcounter_timesync] algo_offset=%lld\n", algo_offset);
}

static void timesync_test_timer_timeout(unsigned long data)
{
	schedule_work(&timesync_test_work);
	mod_timer(&timesync_test_timer,
		jiffies + msecs_to_jiffies(TIMESYNC_TEST_TIME));
}
#endif

static int __init archcounter_timesync_entry(void)
{
	pr_debug("[archcounter_timesync] archcounter_timesync_entry\n");

	filter_algo_init(&moving_average_algo_mono);
	filter_algo_init(&moving_average_algo_boot);

#ifdef CONFIG_TIMESYNC_TEST
	INIT_WORK(&timesync_test_work, timesync_test_work_func);
	init_timer(&timesync_test_timer);
	timesync_test_timer.function = timesync_test_timer_timeout;
	mod_timer(&timesync_test_timer,
		jiffies + msecs_to_jiffies(TIMESYNC_TEST_TIME));
#endif
	return 0;
}

module_init(archcounter_timesync_entry);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("archcounter timesync driver");
MODULE_AUTHOR("MTK");
