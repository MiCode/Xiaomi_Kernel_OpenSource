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

#define pr_fmt(fmt) "bw-hwmon: " fmt

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
#include "governor_bw_hwmon.h"

struct hwmon_node {
	unsigned int tolerance_percent;
	unsigned int guard_band_mbps;
	unsigned int decay_rate;
	unsigned int io_percent;
	unsigned int bw_step;
	unsigned long prev_ab;
	unsigned long *dev_ab;
	ktime_t prev_ts;
	bool mon_started;
	struct list_head list;
	void *orig_data;
	struct bw_hwmon *hw;
	struct devfreq_governor *gov;
	struct attribute_group *attr_grp;
};

static LIST_HEAD(hwmon_list);
static DEFINE_MUTEX(list_lock);

static int use_cnt;
static DEFINE_MUTEX(state_lock);

#define show_attr(name) \
static ssize_t show_##name(struct device *dev,				\
			struct device_attribute *attr, char *buf)	\
{									\
	struct devfreq *df = to_devfreq(dev);				\
	struct hwmon_node *hw = df->data;				\
	return snprintf(buf, PAGE_SIZE, "%u\n", hw->name);		\
}

#define store_attr(name, _min, _max) \
static ssize_t store_##name(struct device *dev,				\
			struct device_attribute *attr, const char *buf,	\
			size_t count)					\
{									\
	struct devfreq *df = to_devfreq(dev);				\
	struct hwmon_node *hw = df->data;				\
	int ret;							\
	unsigned int val;						\
	ret = sscanf(buf, "%u", &val);					\
	if (ret != 1)							\
		return -EINVAL;						\
	val = max(val, _min);						\
	val = min(val, _max);						\
	hw->name = val;							\
	return count;							\
}

#define gov_attr(__attr, min, max)	\
show_attr(__attr)			\
store_attr(__attr, min, max)		\
static DEVICE_ATTR(__attr, 0644, show_##__attr, store_##__attr)

#define MIN_MS	10U
#define MAX_MS	500U

static unsigned long measure_bw_and_set_irq(struct hwmon_node *node)
{
	ktime_t ts;
	unsigned int us;
	unsigned long mbps;
	struct bw_hwmon *hw = node->hw;

	/*
	 * Since we are stopping the counters, we don't want this short work
	 * to be interrupted by other tasks and cause the measurements to be
	 * wrong. Not blocking interrupts to avoid affecting interrupt
	 * latency and since they should be short anyway because they run in
	 * atomic context.
	 */
	preempt_disable();

	ts = ktime_get();
	us = ktime_to_us(ktime_sub(ts, node->prev_ts));
	if (!us)
		us = 1;

	mbps = hw->meas_bw_and_set_irq(hw, node->tolerance_percent, us);
	node->prev_ts = ts;

	preempt_enable();

	dev_dbg(hw->df->dev.parent, "BW MBps = %6lu, period = %u\n", mbps, us);

	return mbps;
}

static void compute_bw(struct hwmon_node *node, int mbps,
			unsigned long *freq, unsigned long *ab)
{
	int new_bw;

	mbps += node->guard_band_mbps;

	if (mbps > node->prev_ab) {
		new_bw = mbps;
	} else {
		new_bw = mbps * node->decay_rate
			+ node->prev_ab * (100 - node->decay_rate);
		new_bw /= 100;
	}

	node->prev_ab = new_bw;
	if (ab)
		*ab = roundup(new_bw, node->bw_step);
	*freq = (new_bw * 100) / node->io_percent;
}

static struct hwmon_node *find_hwmon_node(struct devfreq *df)
{
	struct hwmon_node *node, *found = NULL;

	mutex_lock(&list_lock);
	list_for_each_entry(node, &hwmon_list, list)
		if (node->hw->dev == df->dev.parent ||
		    node->hw->of_node == df->dev.parent->of_node ||
		    (!node->hw->dev && !node->hw->of_node &&
		     node->gov == df->governor)) {
			found = node;
			break;
		}
	mutex_unlock(&list_lock);

	return found;
}

#define TOO_SOON_US	(1 * USEC_PER_MSEC)
int update_bw_hwmon(struct bw_hwmon *hwmon)
{
	struct devfreq *df;
	struct hwmon_node *node;
	ktime_t ts;
	unsigned int us;
	int ret;

	if (!hwmon)
		return -EINVAL;
	df = hwmon->df;
	if (!df)
		return -ENODEV;
	node = find_hwmon_node(df);
	if (!node)
		return -ENODEV;

	if (!node->mon_started)
		return -EBUSY;

	dev_dbg(df->dev.parent, "Got update request\n");
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
	us = ktime_to_us(ktime_sub(ts, node->prev_ts));
	if (us > TOO_SOON_US) {
		mutex_lock(&df->lock);
		ret = update_devfreq(df);
		if (ret)
			dev_err(df->dev.parent,
				"Unable to update freq on request!\n");
		mutex_unlock(&df->lock);
	}

	devfreq_monitor_start(df);

	return 0;
}

static int start_monitoring(struct devfreq *df)
{
	int ret = 0;
	unsigned long mbps;
	struct device *dev = df->dev.parent;
	struct hwmon_node *node;
	struct bw_hwmon *hw;
	struct devfreq_dev_status stat;

	node = find_hwmon_node(df);
	if (!node) {
		dev_err(dev, "Unable to find HW monitor!\n");
		return -ENODEV;
	}
	hw = node->hw;

	stat.private_data = NULL;
	if (df->profile->get_dev_status)
		ret = df->profile->get_dev_status(df->dev.parent, &stat);
	if (ret || !stat.private_data)
		dev_warn(dev, "Device doesn't take AB votes!\n");
	else
		node->dev_ab = stat.private_data;

	hw->df = df;
	node->orig_data = df->data;
	df->data = node;

	node->prev_ts = ktime_get();
	node->prev_ab = 0;
	mbps = (df->previous_freq * node->io_percent) / 100;
	ret = hw->start_hwmon(hw, mbps);
	if (ret) {
		dev_err(dev, "Unable to start HW monitor!\n");
		goto err_start;
	}

	devfreq_monitor_start(df);
	node->mon_started = true;

	ret = sysfs_create_group(&df->dev.kobj, node->attr_grp);
	if (ret)
		goto err_sysfs;

	return 0;

err_sysfs:
	node->mon_started = false;
	devfreq_monitor_stop(df);
	hw->stop_hwmon(hw);
err_start:
	df->data = node->orig_data;
	node->orig_data = NULL;
	hw->df = NULL;
	node->dev_ab = NULL;
	return ret;
}

static void stop_monitoring(struct devfreq *df)
{
	struct hwmon_node *node = df->data;
	struct bw_hwmon *hw = node->hw;

	sysfs_remove_group(&df->dev.kobj, node->attr_grp);
	node->mon_started = false;
	devfreq_monitor_stop(df);
	hw->stop_hwmon(hw);
	df->data = node->orig_data;
	node->orig_data = NULL;
	hw->df = NULL;
	/*
	 * Not all governors know about this additional extended device
	 * configuration. To avoid leaving the extended configuration at a
	 * stale state, set it to 0 and let the next governor take it from
	 * there.
	 */
	if (node->dev_ab)
		*node->dev_ab = 0;
	node->dev_ab = NULL;
}

static int devfreq_bw_hwmon_get_freq(struct devfreq *df,
					unsigned long *freq,
					u32 *flag)
{
	unsigned long mbps;
	struct hwmon_node *node = df->data;

	mbps = measure_bw_and_set_irq(node);
	compute_bw(node, mbps, freq, node->dev_ab);

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
	.name = "bw_hwmon",
	.attrs = dev_attr,
};

static int devfreq_bw_hwmon_ev_handler(struct devfreq *df,
					unsigned int event, void *data)
{
	int ret;
	unsigned int sample_ms;

	switch (event) {
	case DEVFREQ_GOV_START:
		sample_ms = df->profile->polling_ms;
		sample_ms = max(MIN_MS, sample_ms);
		sample_ms = min(MAX_MS, sample_ms);
		df->profile->polling_ms = sample_ms;

		ret = start_monitoring(df);
		if (ret)
			return ret;

		dev_dbg(df->dev.parent,
			"Enabled dev BW HW monitor governor\n");
		break;

	case DEVFREQ_GOV_STOP:
		stop_monitoring(df);
		dev_dbg(df->dev.parent,
			"Disabled dev BW HW monitor governor\n");
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

static struct devfreq_governor devfreq_gov_bw_hwmon = {
	.name = "bw_hwmon",
	.get_target_freq = devfreq_bw_hwmon_get_freq,
	.event_handler = devfreq_bw_hwmon_ev_handler,
};

int register_bw_hwmon(struct device *dev, struct bw_hwmon *hwmon)
{
	int ret = 0;
	struct hwmon_node *node;
	struct attribute_group *attr_grp;

	if (!hwmon->gov && !hwmon->dev && !hwmon->of_node)
		return -EINVAL;

	node = devm_kzalloc(dev, sizeof(*node), GFP_KERNEL);
	if (!node) {
		dev_err(dev, "Unable to register gov. Out of memory!\n");
		return -ENOMEM;
	}

	if (hwmon->gov) {
		attr_grp = devm_kzalloc(dev, sizeof(*attr_grp), GFP_KERNEL);
		if (!attr_grp)
			return -ENOMEM;

		hwmon->gov->get_target_freq = devfreq_bw_hwmon_get_freq;
		hwmon->gov->event_handler = devfreq_bw_hwmon_ev_handler;
		attr_grp->name = hwmon->gov->name;
		attr_grp->attrs = dev_attr;

		node->gov = hwmon->gov;
		node->attr_grp = attr_grp;
	} else {
		node->gov = &devfreq_gov_bw_hwmon;
		node->attr_grp = &dev_attr_group;
	}

	node->tolerance_percent = 10;
	node->guard_band_mbps = 100;
	node->decay_rate = 90;
	node->io_percent = 16;
	node->bw_step = 190;
	node->hw = hwmon;

	mutex_lock(&list_lock);
	list_add_tail(&node->list, &hwmon_list);
	mutex_unlock(&list_lock);

	if (hwmon->gov) {
		ret = devfreq_add_governor(hwmon->gov);
	} else {
		mutex_lock(&state_lock);
		if (!use_cnt)
			ret = devfreq_add_governor(&devfreq_gov_bw_hwmon);
		if (!ret)
			use_cnt++;
		mutex_unlock(&state_lock);
	}

	if (!ret)
		dev_info(dev, "BW HWmon governor registered.\n");
	else
		dev_err(dev, "BW HWmon governor registration failed!\n");

	return ret;
}

MODULE_DESCRIPTION("HW monitor based dev DDR bandwidth voting driver");
MODULE_LICENSE("GPL v2");
