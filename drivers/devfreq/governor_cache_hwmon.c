/*
 * Copyright (c) 2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "cache-hwmon: " fmt

#include <linux/kernel.h>
#include <linux/sizes.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/ktime.h>
#include <linux/time.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/devfreq.h>
#include "governor.h"
#include "governor_cache_hwmon.h"

#define show_attr(name) \
static ssize_t show_##name(struct device *dev,				\
			struct device_attribute *attr, char *buf)	\
{									\
	return snprintf(buf, PAGE_SIZE, "%u\n", name);			\
}

#define store_attr(name, _min, _max) \
static ssize_t store_##name(struct device *dev,				\
			struct device_attribute *attr, const char *buf,	\
			size_t count)					\
{									\
	int ret;							\
	unsigned int val;						\
	ret = kstrtoint(buf, 10, &val);					\
	if (ret)							\
		return ret;						\
	val = max(val, _min);						\
	val = min(val, _max);						\
	name = val;							\
	return count;							\
}

#define gov_attr(__attr, min, max)	\
show_attr(__attr)			\
store_attr(__attr, (min), (max))	\
static DEVICE_ATTR(__attr, 0644, show_##__attr, store_##__attr)


static struct cache_hwmon *hw;
static unsigned int cycles_per_low_req;
static unsigned int cycles_per_med_req = 20;
static unsigned int cycles_per_high_req = 35;
static unsigned int min_busy = 100;
static unsigned int max_busy = 100;
static unsigned int tolerance_mrps = 5;
static unsigned int guard_band_mhz = 100;
static unsigned int decay_rate = 90;

#define MIN_MS	10U
#define MAX_MS	500U
static unsigned int sample_ms = 50;
static unsigned long prev_mhz;
static ktime_t prev_ts;

static unsigned long measure_mrps_and_set_irq(struct devfreq *df,
			struct mrps_stats *stat)
{
	ktime_t ts;
	unsigned int us;

	/*
	 * Since we are stopping the counters, we don't want this short work
	 * to be interrupted by other tasks and cause the measurements to be
	 * wrong. Not blocking interrupts to avoid affecting interrupt
	 * latency and since they should be short anyway because they run in
	 * atomic context.
	 */
	preempt_disable();

	ts = ktime_get();
	us = ktime_to_us(ktime_sub(ts, prev_ts));
	if (!us)
		us = 1;

	hw->meas_mrps_and_set_irq(df, tolerance_mrps, us, stat);
	prev_ts = ts;

	preempt_enable();

	pr_debug("stat H=%3lu, M=%3lu, T=%3lu, b=%3u, f=%4lu, us=%d\n",
		 stat->high, stat->med, stat->high + stat->med,
		 stat->busy_percent, df->previous_freq / 1000, us);

	return 0;
}

static void compute_cache_freq(struct mrps_stats *mrps, unsigned long *freq)
{
	unsigned long new_mhz;
	unsigned int busy;

	new_mhz = mrps->high * cycles_per_high_req
		+ mrps->med * cycles_per_med_req
		+ mrps->low * cycles_per_low_req;

	busy = max(min_busy, mrps->busy_percent);
	busy = min(max_busy, busy);

	new_mhz *= 100;
	new_mhz /= busy;

	if (new_mhz < prev_mhz) {
		new_mhz = new_mhz * decay_rate + prev_mhz * (100 - decay_rate);
		new_mhz /= 100;
	}
	prev_mhz = new_mhz;

	new_mhz += guard_band_mhz;
	*freq = new_mhz * 1000;
}

#define TOO_SOON_US	(1 * USEC_PER_MSEC)
static irqreturn_t mon_intr_handler(int irq, void *dev)
{
	struct devfreq *df = dev;
	ktime_t ts;
	unsigned int us;
	int ret;

	if (!hw->is_valid_irq(df))
		return IRQ_NONE;

	pr_debug("Got interrupt\n");
	devfreq_monitor_stop(df);

	/*
	 * Don't recalc cache freq if the interrupt comes right after a
	 * previous cache freq calculation.  This is done for two reasons:
	 *
	 * 1. Sampling the cache request during a very short duration can
	 *    result in a very inaccurate measurement due to very short
	 *    bursts.
	 * 2. This can only happen if the limit was hit very close to the end
	 *    of the previous sample period. Which means the current cache
	 *    request estimate is not very off and doesn't need to be
	 *    readjusted.
	 */
	ts = ktime_get();
	us = ktime_to_us(ktime_sub(ts, prev_ts));
	if (us > TOO_SOON_US) {
		mutex_lock(&df->lock);
		ret = update_devfreq(df);
		if (ret)
			pr_err("Unable to update freq on IRQ!\n");
		mutex_unlock(&df->lock);
	}

	devfreq_monitor_start(df);

	return IRQ_HANDLED;
}

static int devfreq_cache_hwmon_get_freq(struct devfreq *df,
					unsigned long *freq)
{
	struct mrps_stats stat;

	measure_mrps_and_set_irq(df, &stat);
	compute_cache_freq(&stat, freq);

	return 0;
}

gov_attr(cycles_per_low_req, 1U, 100U);
gov_attr(cycles_per_med_req, 1U, 100U);
gov_attr(cycles_per_high_req, 1U, 100U);
gov_attr(min_busy, 1U, 100U);
gov_attr(max_busy, 1U, 100U);
gov_attr(tolerance_mrps, 0U, 100U);
gov_attr(guard_band_mhz, 0U, 500U);
gov_attr(decay_rate, 0U, 100U);

static struct attribute *dev_attr[] = {
	&dev_attr_cycles_per_low_req.attr,
	&dev_attr_cycles_per_med_req.attr,
	&dev_attr_cycles_per_high_req.attr,
	&dev_attr_min_busy.attr,
	&dev_attr_max_busy.attr,
	&dev_attr_tolerance_mrps.attr,
	&dev_attr_guard_band_mhz.attr,
	&dev_attr_decay_rate.attr,
	NULL,
};

static struct attribute_group dev_attr_group = {
	.name = "cache_hwmon",
	.attrs = dev_attr,
};

static int start_monitoring(struct devfreq *df)
{
	int ret;
	struct mrps_stats mrps;

	prev_ts = ktime_get();
	prev_mhz = 0;
	mrps.high = (df->previous_freq / 1000) - guard_band_mhz;
	mrps.high /= cycles_per_high_req;

	ret = hw->start_hwmon(df, &mrps);
	if (ret) {
		pr_err("Unable to start HW monitor!\n");
		return ret;
	}

	devfreq_monitor_start(df);

	ret = request_threaded_irq(hw->irq, NULL, mon_intr_handler,
			  IRQF_ONESHOT | IRQF_SHARED,
			  "cache_hwmon", df);
	if (ret) {
		pr_err("Unable to register interrupt handler!\n");
		goto req_irq_fail;
	}

	ret = sysfs_create_group(&df->dev.kobj, &dev_attr_group);
	if (ret) {
		pr_err("Error creating sys entries!\n");
		goto sysfs_fail;
	}

	return 0;

sysfs_fail:
	disable_irq(hw->irq);
	free_irq(hw->irq, df);
req_irq_fail:
	devfreq_monitor_stop(df);
	hw->stop_hwmon(df);
	return ret;
}

static void stop_monitoring(struct devfreq *df)
{
	sysfs_remove_group(&df->dev.kobj, &dev_attr_group);
	disable_irq(hw->irq);
	free_irq(hw->irq, df);
	devfreq_monitor_stop(df);
	hw->stop_hwmon(df);
}

static int devfreq_cache_hwmon_ev_handler(struct devfreq *df,
					unsigned int event, void *data)
{
	int ret;

	switch (event) {
	case DEVFREQ_GOV_START:
		sample_ms = df->profile->polling_ms;
		sample_ms = max(MIN_MS, sample_ms);
		sample_ms = min(MAX_MS, sample_ms);
		df->profile->polling_ms = sample_ms;

		ret = start_monitoring(df);
		if (ret)
			return ret;

		pr_debug("Enabled Cache HW monitor governor\n");
		break;

	case DEVFREQ_GOV_STOP:
		stop_monitoring(df);
		pr_debug("Disabled Cache HW monitor governor\n");
		break;

	case DEVFREQ_GOV_INTERVAL:
		sample_ms = *(unsigned int *)data;
		sample_ms = max(MIN_MS, sample_ms);
		sample_ms = min(MAX_MS, sample_ms);
		devfreq_interval_update(df, &sample_ms);
		break;
	}

	return 0;
}

static struct devfreq_governor devfreq_cache_hwmon = {
	.name = "cache_hwmon",
	.get_target_freq = devfreq_cache_hwmon_get_freq,
	.event_handler = devfreq_cache_hwmon_ev_handler,
};

int register_cache_hwmon(struct cache_hwmon *hwmon)
{
	int ret;

	hw = hwmon;
	ret = devfreq_add_governor(&devfreq_cache_hwmon);
	if (ret) {
		pr_err("devfreq governor registration failed\n");
		return ret;
	}

	return 0;
}

MODULE_DESCRIPTION("HW monitor based cache freq driver");
MODULE_LICENSE("GPL v2");
