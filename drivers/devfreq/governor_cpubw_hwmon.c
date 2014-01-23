/*
 * Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "cpubw-hwmon: " fmt

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
#include "governor_cpubw_hwmon.h"

#define show_attr(name) \
static ssize_t show_##name(struct device *dev,				\
			struct device_attribute *attr, char *buf)	\
{									\
	return sprintf(buf, "%u\n", name);				\
}

#define store_attr(name, _min, _max) \
static ssize_t store_##name(struct device *dev,				\
			struct device_attribute *attr, const char *buf,	\
			size_t count)					\
{									\
	int ret;							\
	unsigned int val;						\
	ret = sscanf(buf, "%u", &val);					\
	if (ret != 1)							\
		return -EINVAL;						\
	val = max(val, _min);				\
	val = min(val, _max);				\
	name = val;							\
	return count;							\
}

#define gov_attr(__attr, min, max)	\
show_attr(__attr)			\
store_attr(__attr, min, max)		\
static DEVICE_ATTR(__attr, 0644, show_##__attr, store_##__attr)


static struct cpubw_hwmon *hw;
static unsigned int tolerance_percent = 10;
static unsigned int guard_band_mbps = 100;
static unsigned int decay_rate = 90;
static unsigned int io_percent = 16;
static unsigned int bw_step = 190;

#define MIN_MS	10U
#define MAX_MS	500U
static unsigned int sample_ms = 50;
static unsigned long prev_ab;
static ktime_t prev_ts;

static unsigned long measure_bw_and_set_irq(struct devfreq *df)
{
	ktime_t ts;
	unsigned int us;
	unsigned long mbps;

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

	mbps = hw->meas_bw_and_set_irq(df, tolerance_percent, us);
	prev_ts = ts;

	preempt_enable();

	pr_debug("BW MBps = %6lu, period = %u\n", mbps, us);

	return mbps;
}

static void compute_bw(int mbps, unsigned long *freq, unsigned long *ab)
{
	int new_bw;

	mbps += guard_band_mbps;

	if (mbps > prev_ab) {
		new_bw = mbps;
	} else {
		new_bw = mbps * decay_rate + prev_ab * (100 - decay_rate);
		new_bw /= 100;
	}

	prev_ab = new_bw;
	*ab = roundup(new_bw, bw_step);
	*freq = (new_bw * 100) / io_percent;
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
	 * Don't recalc bandwidth if the interrupt comes right after a
	 * previous bandwidth calculation.  This is done for two reasons:
	 *
	 * 1. Sampling the BW during a very short duration can result in a
	 *    very inaccurate measurement due to very short bursts.
	 * 2. This can only happen if the limit was hit very close to the end
	 *    of the previous sample period. Which means the current BW
	 *    estimate is not very off and doesn't need to be readjusted.
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

static int start_monitoring(struct devfreq *df)
{
	int ret;
	unsigned long mbps;

	ret = request_threaded_irq(hw->irq, NULL, mon_intr_handler,
			  IRQF_ONESHOT | IRQF_SHARED,
			  "cpubw_hwmon", df);
	if (ret) {
		pr_err("Unable to register interrupt handler!\n");
		return ret;
	}

	prev_ts = ktime_get();
	prev_ab = 0;

	mbps = (df->previous_freq * io_percent) / 100;

	ret = hw->start_hwmon(df, mbps);
	if (ret) {
		pr_err("Unable to start HW monitor!\n");
		free_irq(hw->irq, df);
		return ret;
	}

	return 0;
}

static void stop_monitoring(struct devfreq *df)
{
	hw->stop_hwmon(df);
	disable_irq(hw->irq);
	free_irq(hw->irq, df);
}

static int devfreq_cpubw_hwmon_get_freq(struct devfreq *df,
					unsigned long *freq,
					u32 *flag)
{
	unsigned long mbps;

	mbps = measure_bw_and_set_irq(df);
	compute_bw(mbps, freq, df->data);

	return 0;
}

gov_attr(tolerance_percent, 0U, 30U);
gov_attr(guard_band_mbps, 0U, 2000U);
gov_attr(decay_rate, 0U, 100U);
gov_attr(io_percent, 1U, 100U);
gov_attr(bw_step, 50U, 1000U);

static struct attribute *dev_attr[] = {
	&dev_attr_tolerance_percent.attr,
	&dev_attr_guard_band_mbps.attr,
	&dev_attr_decay_rate.attr,
	&dev_attr_io_percent.attr,
	&dev_attr_bw_step.attr,
	NULL,
};

static struct attribute_group dev_attr_group = {
	.name = "cpubw_hwmon",
	.attrs = dev_attr,
};

static int devfreq_cpubw_hwmon_ev_handler(struct devfreq *df,
					unsigned int event, void *data)
{
	int ret;

	switch (event) {
	case DEVFREQ_GOV_START:
		ret = start_monitoring(df);
		if (ret)
			return ret;
		ret = sysfs_create_group(&df->dev.kobj, &dev_attr_group);
		if (ret)
			return ret;

		sample_ms = df->profile->polling_ms;
		sample_ms = max(MIN_MS, sample_ms);
		sample_ms = min(MAX_MS, sample_ms);
		df->profile->polling_ms = sample_ms;
		devfreq_monitor_start(df);

		pr_debug("Enabled CPU BW HW monitor governor\n");
		break;

	case DEVFREQ_GOV_STOP:
		sysfs_remove_group(&df->dev.kobj, &dev_attr_group);
		devfreq_monitor_stop(df);
		*(unsigned long *)df->data = 0;
		stop_monitoring(df);
		pr_debug("Disabled CPU BW HW monitor governor\n");
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

static struct devfreq_governor devfreq_cpubw_hwmon = {
	.name = "cpubw_hwmon",
	.get_target_freq = devfreq_cpubw_hwmon_get_freq,
	.event_handler = devfreq_cpubw_hwmon_ev_handler,
};

int register_cpubw_hwmon(struct cpubw_hwmon *hwmon)
{
	int ret;

	if (hw != NULL) {
		pr_err("cpubw hwmon already registered\n");
		return -EBUSY;
	}

	hw = hwmon;

	ret = devfreq_add_governor(&devfreq_cpubw_hwmon);
	if (ret) {
		pr_err("devfreq governor registration failed\n");
		return ret;
	}

	return 0;
}

MODULE_DESCRIPTION("HW monitor based CPU DDR bandwidth voting driver");
MODULE_LICENSE("GPL v2");
