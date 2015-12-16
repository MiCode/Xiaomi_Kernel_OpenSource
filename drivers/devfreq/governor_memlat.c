/*
 * Copyright (c) 2015, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "mem_lat: " fmt

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
#include "governor_memlat.h"

#include <trace/events/power.h>

struct memlat_node {
	unsigned int ratio_ceil;
	unsigned int freq_thresh_mhz;
	unsigned int mult_factor;
	bool mon_started;
	struct list_head list;
	void *orig_data;
	struct memlat_hwmon *hw;
	struct devfreq_governor *gov;
	struct attribute_group *attr_grp;
};

static LIST_HEAD(memlat_list);
static DEFINE_MUTEX(list_lock);

static int use_cnt;
static DEFINE_MUTEX(state_lock);

#define show_attr(name) \
static ssize_t show_##name(struct device *dev,				\
			struct device_attribute *attr, char *buf)	\
{									\
	struct devfreq *df = to_devfreq(dev);				\
	struct memlat_node *hw = df->data;				\
	return snprintf(buf, PAGE_SIZE, "%u\n", hw->name);		\
}

#define store_attr(name, _min, _max) \
static ssize_t store_##name(struct device *dev,				\
			struct device_attribute *attr, const char *buf,	\
			size_t count)					\
{									\
	struct devfreq *df = to_devfreq(dev);				\
	struct memlat_node *hw = df->data;				\
	int ret;							\
	unsigned int val;						\
	ret = kstrtouint(buf, 10, &val);				\
	if (ret)							\
		return ret;						\
	val = max(val, _min);						\
	val = min(val, _max);						\
	hw->name = val;							\
	return count;							\
}

#define gov_attr(__attr, min, max)	\
show_attr(__attr)			\
store_attr(__attr, min, max)		\
static DEVICE_ATTR(__attr, 0644, show_##__attr, store_##__attr)

static unsigned long compute_dev_vote(struct devfreq *df)
{
	int i, lat_dev;
	struct memlat_node *node = df->data;
	struct memlat_hwmon *hw = node->hw;
	unsigned long max_freq = 0;
	unsigned int ratio;

	hw->get_cnt(hw);

	for (i = 0; i < hw->num_cores; i++) {
		ratio = hw->core_stats[i].inst_count;

		if (hw->core_stats[i].mem_count)
			ratio /= hw->core_stats[i].mem_count;

		trace_memlat_dev_meas(dev_name(df->dev.parent),
					hw->core_stats[i].id,
					hw->core_stats[i].inst_count,
					hw->core_stats[i].mem_count,
					hw->core_stats[i].freq, ratio);

		if (ratio && ratio <= node->ratio_ceil
		    && hw->core_stats[i].freq >= node->freq_thresh_mhz
		    && hw->core_stats[i].freq > max_freq) {
			lat_dev = i;
			max_freq = hw->core_stats[i].freq;
		}
	}

	if (max_freq)
		trace_memlat_dev_update(dev_name(df->dev.parent),
					hw->core_stats[lat_dev].id,
					hw->core_stats[lat_dev].inst_count,
					hw->core_stats[lat_dev].mem_count,
					hw->core_stats[lat_dev].freq,
					max_freq * node->mult_factor);

	return max_freq;
}

static struct memlat_node *find_memlat_node(struct devfreq *df)
{
	struct memlat_node *node, *found = NULL;

	mutex_lock(&list_lock);
	list_for_each_entry(node, &memlat_list, list)
		if (node->hw->dev == df->dev.parent ||
		    node->hw->of_node == df->dev.parent->of_node) {
			found = node;
			break;
		}
	mutex_unlock(&list_lock);

	return found;
}

static int start_monitor(struct devfreq *df)
{
	struct memlat_node *node = df->data;
	struct memlat_hwmon *hw = node->hw;
	struct device *dev = df->dev.parent;
	int ret;

	ret = hw->start_hwmon(hw);

	if (ret) {
		dev_err(dev, "Unable to start HW monitor! (%d)\n", ret);
		return ret;
	}

	devfreq_monitor_start(df);

	node->mon_started = true;

	return 0;
}

static void stop_monitor(struct devfreq *df)
{
	struct memlat_node *node = df->data;
	struct memlat_hwmon *hw = node->hw;

	node->mon_started = false;

	devfreq_monitor_stop(df);
	hw->stop_hwmon(hw);
}

static int gov_start(struct devfreq *df)
{
	int ret = 0;
	struct device *dev = df->dev.parent;
	struct memlat_node *node;
	struct memlat_hwmon *hw;

	node = find_memlat_node(df);
	if (!node) {
		dev_err(dev, "Unable to find HW monitor!\n");
		return -ENODEV;
	}
	hw = node->hw;

	hw->df = df;
	node->orig_data = df->data;
	df->data = node;

	if (start_monitor(df))
		goto err_start;

	ret = sysfs_create_group(&df->dev.kobj, node->attr_grp);
	if (ret)
		goto err_sysfs;

	return 0;

err_sysfs:
	stop_monitor(df);
err_start:
	df->data = node->orig_data;
	node->orig_data = NULL;
	hw->df = NULL;
	return ret;
}

static void gov_stop(struct devfreq *df)
{
	struct memlat_node *node = df->data;
	struct memlat_hwmon *hw = node->hw;

	sysfs_remove_group(&df->dev.kobj, node->attr_grp);
	stop_monitor(df);
	df->data = node->orig_data;
	node->orig_data = NULL;
	hw->df = NULL;
}

static int devfreq_memlat_get_freq(struct devfreq *df,
					unsigned long *freq,
					u32 *flag)
{
	unsigned long mhz;
	struct memlat_node *node = df->data;

	mhz = compute_dev_vote(df);
	*freq = mhz ? (mhz * node->mult_factor) : 0;

	return 0;
}

gov_attr(ratio_ceil, 1U, 1000U);
gov_attr(freq_thresh_mhz, 300U, 5000U);
gov_attr(mult_factor, 1U, 10U);

static struct attribute *dev_attr[] = {
	&dev_attr_ratio_ceil.attr,
	&dev_attr_freq_thresh_mhz.attr,
	&dev_attr_mult_factor.attr,
	NULL,
};

static struct attribute_group dev_attr_group = {
	.name = "mem_latency",
	.attrs = dev_attr,
};

#define MIN_MS	10U
#define MAX_MS	500U
static int devfreq_memlat_ev_handler(struct devfreq *df,
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

		ret = gov_start(df);
		if (ret)
			return ret;

		dev_dbg(df->dev.parent,
			"Enabled Memory Latency governor\n");
		break;

	case DEVFREQ_GOV_STOP:
		gov_stop(df);
		dev_dbg(df->dev.parent,
			"Disabled Memory Latency governor\n");
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

static struct devfreq_governor devfreq_gov_memlat = {
	.name = "mem_latency",
	.get_target_freq = devfreq_memlat_get_freq,
	.event_handler = devfreq_memlat_ev_handler,
};

int register_memlat(struct device *dev, struct memlat_hwmon *hw)
{
	int ret = 0;
	struct memlat_node *node;

	if (!hw->dev && !hw->of_node)
		return -EINVAL;

	node = devm_kzalloc(dev, sizeof(*node), GFP_KERNEL);
	if (!node)
		return -ENOMEM;

	node->gov = &devfreq_gov_memlat;
	node->attr_grp = &dev_attr_group;

	node->ratio_ceil = 10;
	node->freq_thresh_mhz = 900;
	node->mult_factor = 8;
	node->hw = hw;

	mutex_lock(&list_lock);
	list_add_tail(&node->list, &memlat_list);
	mutex_unlock(&list_lock);

	mutex_lock(&state_lock);
	if (!use_cnt)
		ret = devfreq_add_governor(&devfreq_gov_memlat);
	if (!ret)
		use_cnt++;
	mutex_unlock(&state_lock);

	if (!ret)
		dev_info(dev, "Memory Latency governor registered.\n");
	else
		dev_err(dev, "Memory Latency governor registration failed!\n");

	return ret;
}

MODULE_DESCRIPTION("HW monitor based dev DDR bandwidth voting driver");
MODULE_LICENSE("GPL v2");
