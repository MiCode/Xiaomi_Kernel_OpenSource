/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "%s:%s " fmt, KBUILD_MODNAME, __func__

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/sysfs.h>
#include <linux/rwsem.h>
#include <linux/debugfs.h>
#include <linux/thermal.h>
#include <linux/slab.h>
#include "lmh_interface.h"

#define LMH_MON_NAME			"lmh_monitor"
#define LMH_ISR_POLL_DELAY		"interrupt_poll_delay_msec"
#define LMH_TRACE_ENABLE		"hw_trace_enable"
#define LMH_TRACE_INTERVAL		"hw_trace_interval"
#define LMH_TRACE_INTERVAL_XO_TICKS	250

struct lmh_mon_threshold {
	long				value;
	bool				active;
};

struct lmh_device_data {
	char				device_name[LMH_NAME_MAX];
	struct lmh_device_ops		*device_ops;
	uint32_t			max_level;
	int				curr_level;
	int				*levels;
	struct dentry			*dev_parent;
	struct dentry			*max_lvl_fs;
	struct dentry			*curr_lvl_fs;
	struct dentry			*avail_lvl_fs;
	struct list_head		list_ptr;
	struct rw_semaphore		lock;
};

struct lmh_mon_sensor_data {
	char				sensor_name[LMH_NAME_MAX];
	struct lmh_sensor_ops		*sensor_ops;
	struct delayed_work		isr_poll;
	struct list_head		list_ptr;
	struct rw_semaphore		lock;
	struct lmh_mon_threshold	trip[LMH_TRIP_MAX];
	enum lmh_monitor_state		state;
	struct thermal_zone_device	*tzdev;
	enum thermal_device_mode	mode;
};

struct lmh_mon_driver_data {
	struct dentry			*debugfs_parent;
	struct dentry			*poll_fs;
	struct dentry			*enable_hw_log;
	struct dentry			*hw_log_delay;
	uint32_t			hw_log_enable;
	uint64_t			hw_log_interval;
};

static struct lmh_mon_driver_data	*lmh_mon_data;
static DECLARE_RWSEM(lmh_mon_access_lock);
static LIST_HEAD(lmh_sensor_list);
static DECLARE_RWSEM(lmh_dev_access_lock);
static LIST_HEAD(lmh_device_list);

#define LMH_CREATE_DEBUGFS_FILE(_node, _name, _mode, _parent, _data, _ops, \
	_ret) do { \
		_node = debugfs_create_file(_name, _mode, _parent, \
				_data, _ops); \
		if (IS_ERR(_node)) { \
			_ret = PTR_ERR(_node); \
			pr_err("Error creating debugfs file:%s. err:%d\n", \
					_name, _ret); \
		} \
	} while (0)

#define LMH_CREATE_DEBUGFS_DIR(_node, _name, _parent, _ret) \
	do { \
		_node = debugfs_create_dir(_name, _parent); \
		if (IS_ERR(_node)) { \
			_ret = PTR_ERR(_node); \
			pr_err("Error creating debugfs dir:%s. err:%d\n", \
					_name, _ret); \
		} \
	} while (0)

#define LMH_HW_LOG_FS(_name) \
static int _name##_get(void *data, u64 *val) \
{ \
	*val = lmh_mon_data->_name; \
	return 0; \
} \
static int _name##_set(void *data, u64 val) \
{ \
	struct lmh_mon_sensor_data *lmh_sensor = data; \
	int ret = 0; \
	lmh_mon_data->_name = val; \
	if (lmh_mon_data->hw_log_enable) \
		ret = lmh_sensor->sensor_ops->enable_hw_log( \
			lmh_mon_data->hw_log_interval \
				, lmh_mon_data->hw_log_enable); \
	else \
		ret = lmh_sensor->sensor_ops->disable_hw_log(); \
	return ret; \
} \
DEFINE_SIMPLE_ATTRIBUTE(_name##_fops, _name##_get, _name##_set, \
	"%llu\n");

#define LMH_DEV_GET(_name) \
static int _name##_get(void *data, u64 *val) \
{ \
	struct lmh_device_data *lmh_dev = (struct lmh_device_data *) data; \
	*val = lmh_dev->_name; \
	return 0; \
}

LMH_HW_LOG_FS(hw_log_enable);
LMH_HW_LOG_FS(hw_log_interval);
LMH_DEV_GET(max_level);
LMH_DEV_GET(curr_level);

static int curr_level_set(void *data, u64 val)
{
	struct lmh_device_data *lmh_dev = (struct lmh_device_data *) data;

	return lmh_set_dev_level(lmh_dev->device_name, val);
}

static ssize_t avail_level_get(struct file *fp, char __user *user_buffer,
	       size_t buffer_length, loff_t *position)
{
	struct lmh_device_data *lmh_dev = list_first_entry(&lmh_device_list,
		       struct lmh_device_data, list_ptr);
	int idx = 0, count = 0, ret = 0;
	char *profile_buf = NULL;
	char *buf_start = NULL;

	if (!lmh_dev || !lmh_dev->max_level)
		return ret;

	profile_buf = kzalloc(LMH_NAME_MAX * lmh_dev->max_level, GFP_KERNEL);
	if (!profile_buf) {
		pr_err("Error allocating memory\n");
		return -ENOMEM;
	}
	buf_start = profile_buf;
	for (idx = 0; idx < lmh_dev->max_level; idx++) {
		count = snprintf(buf_start, LMH_NAME_MAX, "%d ",
			       lmh_dev->levels[idx]);
		if (count <= 0) {
			pr_err("Error getting freq value idx:%d. err:%d\n",
				       idx, count);
			return count;
		}
		buf_start += count;
	}
	if (buf_start != profile_buf) {
		buf_start[-1] = '\n';
		ret = simple_read_from_buffer(user_buffer, buffer_length,
				position, profile_buf,
				buf_start - profile_buf - 1);
		if (ret)
			pr_err("populating the buffer failed. err:%d\n", ret);
	} else {
		ret = -ENODEV;
	}

	kfree(profile_buf);
	return ret;
}

DEFINE_SIMPLE_ATTRIBUTE(curr_level_fops, curr_level_get, curr_level_set,
	"%llu\n");

DEFINE_SIMPLE_ATTRIBUTE(max_level_fops, max_level_get, NULL, "%llu\n");

static const struct file_operations avail_level_fops  = {
	.read = avail_level_get,
};

static int lmh_create_dev_debugfs(struct lmh_device_data *lmh_dev)
{
	int ret = 0;

	LMH_CREATE_DEBUGFS_DIR(lmh_dev->dev_parent, lmh_dev->device_name,
		       lmh_mon_data->debugfs_parent, ret);
	if (ret)
		goto debugfs_exit;
	LMH_CREATE_DEBUGFS_FILE(lmh_dev->curr_lvl_fs, "level",
		0600, lmh_dev->dev_parent, (void *)lmh_dev,
		&curr_level_fops, ret);
	if (ret)
		goto debugfs_exit;
	LMH_CREATE_DEBUGFS_FILE(lmh_dev->max_lvl_fs, "total_levels",
		0400, lmh_dev->dev_parent, (void *)lmh_dev,
		&max_level_fops, ret);
	if (ret)
		goto debugfs_exit;
	LMH_CREATE_DEBUGFS_FILE(lmh_dev->avail_lvl_fs, "available_levels",
		0400, lmh_dev->dev_parent, (void *)lmh_dev,
		&avail_level_fops, ret);
	if (ret)
		goto debugfs_exit;

debugfs_exit:
	if (ret)
		debugfs_remove_recursive(lmh_dev->dev_parent);
	return ret;
}

static int lmh_create_debugfs_nodes(struct lmh_mon_sensor_data *lmh_sensor)
{
	int ret = 0;

	lmh_mon_data->hw_log_enable = 0;
	lmh_mon_data->hw_log_interval = LMH_TRACE_INTERVAL_XO_TICKS;
	LMH_CREATE_DEBUGFS_FILE(lmh_mon_data->enable_hw_log, LMH_TRACE_ENABLE,
		0600, lmh_mon_data->debugfs_parent, (void *)lmh_sensor,
		&hw_log_enable_fops, ret);
	if (ret)
		goto create_debugfs_exit;
	LMH_CREATE_DEBUGFS_FILE(lmh_mon_data->hw_log_delay, LMH_TRACE_INTERVAL,
		0600, lmh_mon_data->debugfs_parent, (void *)lmh_sensor,
		&hw_log_interval_fops, ret);
	if (ret)
		goto create_debugfs_exit;

create_debugfs_exit:
	if (ret)
		debugfs_remove_recursive(lmh_mon_data->debugfs_parent);
	return ret;
}

static struct lmh_mon_sensor_data *lmh_match_sensor_ops(
		struct lmh_sensor_ops *ops)
{
	struct lmh_mon_sensor_data *lmh_sensor = NULL;

	list_for_each_entry(lmh_sensor, &lmh_sensor_list, list_ptr) {
		if (lmh_sensor->sensor_ops == ops)
			return lmh_sensor;
	}

	return NULL;
}

static struct lmh_mon_sensor_data *lmh_match_sensor_name(char *sensor_name)
{
	struct lmh_mon_sensor_data *lmh_sensor = NULL;

	list_for_each_entry(lmh_sensor, &lmh_sensor_list, list_ptr) {
		if (!strnicmp(lmh_sensor->sensor_name, sensor_name,
			LMH_NAME_MAX))
			return lmh_sensor;
	}

	return NULL;
}

static void lmh_evaluate_and_notify(struct lmh_mon_sensor_data *lmh_sensor,
	       long val)
{
	int idx = 0, trip = 0;
	bool cond = false;

	for (idx = 0; idx < LMH_TRIP_MAX; idx++) {
		if (!lmh_sensor->trip[idx].active)
			continue;
		if (idx == LMH_HIGH_TRIP) {
			trip = THERMAL_TRIP_CONFIGURABLE_HI;
			cond = (val >= lmh_sensor->trip[idx].value);
		} else {
			trip = THERMAL_TRIP_CONFIGURABLE_LOW;
			cond = (val <= lmh_sensor->trip[idx].value);
		}
		if (cond) {
			lmh_sensor->trip[idx].active = false;
			thermal_sensor_trip(lmh_sensor->tzdev, trip, val);
		}
	}
}

static void lmh_interrupt_monitor(struct work_struct *work)
{
	int ret = 0;
	long val = 0;
	struct lmh_mon_sensor_data *lmh_sensor = container_of(work,
				struct lmh_mon_sensor_data, isr_poll.work);

	down_read(&lmh_sensor->lock);
	ret = lmh_sensor->sensor_ops->read(lmh_sensor->sensor_ops, &val);
	if (ret) {
		pr_err("Error reading the sensor:[%s]. err:%d\n",
			lmh_sensor->sensor_name, ret);
		goto exit_monitor;
	}
	lmh_evaluate_and_notify(lmh_sensor, val);
	if (val <= 0) {
		up_read(&lmh_sensor->lock);
		down_write(&lmh_sensor->lock);
		pr_debug("Rearm sensor:[%s] interrupt\n",
			lmh_sensor->sensor_name);
		lmh_sensor->state = LMH_ISR_MONITOR;
		ret = lmh_sensor->sensor_ops->reset_interrupt(
				lmh_sensor->sensor_ops);
		if (ret)
			pr_err("Sensor:[%s] interrupt reset failed. err:%d\n",
				lmh_sensor->sensor_name, ret);
		up_write(&lmh_sensor->lock);
		return;
	}
	schedule_delayed_work(&lmh_sensor->isr_poll,
		msecs_to_jiffies(lmh_poll_interval));

exit_monitor:
	up_read(&lmh_sensor->lock);
}

void lmh_interrupt_notify(struct lmh_sensor_ops *ops, long trip_val)
{
	struct lmh_mon_sensor_data *lmh_sensor = NULL;

	if (!ops) {
		pr_err("Invalid input\n");
		return;
	}

	down_read(&lmh_mon_access_lock);
	lmh_sensor = lmh_match_sensor_ops(ops);
	if (!lmh_sensor) {
		pr_err("Invalid ops\n");
		goto interrupt_exit;
	}
	down_write(&lmh_sensor->lock);
	if (lmh_sensor->state != LMH_ISR_MONITOR)
		goto interrupt_exit;
	pr_debug("Sensor:[%s] interrupt triggered with intensity:%ld\n",
			lmh_sensor->sensor_name, trip_val);
	lmh_sensor->state = LMH_ISR_POLLING;
	lmh_evaluate_and_notify(lmh_sensor, trip_val);
	schedule_delayed_work(&lmh_sensor->isr_poll,
		msecs_to_jiffies(lmh_poll_interval));
interrupt_exit:
	if (lmh_sensor)
		up_write(&lmh_sensor->lock);
	up_read(&lmh_mon_access_lock);
	return;
}

static int lmh_sensor_read(struct thermal_zone_device *dev, unsigned long *val)
{
	int ret = 0;
	struct lmh_mon_sensor_data *lmh_sensor;

	if (!val || !dev || !dev->devdata) {
		pr_err("Invalid input\n");
		return -EINVAL;
	}
	lmh_sensor = dev->devdata;
	down_read(&lmh_mon_access_lock);
	down_read(&lmh_sensor->lock);
	ret = lmh_sensor->sensor_ops->read(lmh_sensor->sensor_ops, val);
	if (ret) {
		pr_err("Error reading sensor:%s. err:%d\n",
				lmh_sensor->sensor_name, ret);
		goto unlock_and_exit;
	}
unlock_and_exit:
	up_read(&lmh_sensor->lock);
	up_read(&lmh_mon_access_lock);

	return ret;
}

static int lmh_get_mode(struct thermal_zone_device *dev,
		enum thermal_device_mode *mode)
{
	struct lmh_mon_sensor_data *lmh_sensor;

	if (!dev || !dev->devdata || !mode) {
		pr_err("Invalid input\n");
		return -EINVAL;
	}
	lmh_sensor = dev->devdata;
	*mode = lmh_sensor->mode;

	return 0;
}

static int lmh_get_trip_type(struct thermal_zone_device *dev,
		int trip, enum thermal_trip_type *type)
{
	if (!type || !dev || !dev->devdata || trip < 0
		|| trip >= LMH_TRIP_MAX) {
		pr_err("Invalid input\n");
		return -EINVAL;
	}

	switch (trip) {
	case LMH_HIGH_TRIP:
		*type = THERMAL_TRIP_CONFIGURABLE_HI;
		break;
	case LMH_LOW_TRIP:
		*type = THERMAL_TRIP_CONFIGURABLE_LOW;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int lmh_activate_trip(struct thermal_zone_device *dev,
		int trip, enum thermal_trip_activation_mode mode)
{
	struct lmh_mon_sensor_data *lmh_sensor;

	if (!dev || !dev->devdata || trip < 0 || trip >= LMH_TRIP_MAX) {
		pr_err("Invalid input\n");
		return -EINVAL;
	}

	lmh_sensor = dev->devdata;
	down_read(&lmh_mon_access_lock);
	down_write(&lmh_sensor->lock);
	lmh_sensor->trip[trip].active = (mode ==
					THERMAL_TRIP_ACTIVATION_ENABLED);
	up_write(&lmh_sensor->lock);
	up_read(&lmh_mon_access_lock);

	return 0;
}

static int lmh_get_trip_value(struct thermal_zone_device *dev,
		int trip, unsigned long *value)
{
	struct lmh_mon_sensor_data *lmh_sensor;

	if (!dev || !dev->devdata || trip < 0 || trip >= LMH_TRIP_MAX
		|| !value) {
		pr_err("Invalid input\n");
		return -EINVAL;
	}

	lmh_sensor = dev->devdata;
	down_read(&lmh_mon_access_lock);
	down_read(&lmh_sensor->lock);
	*value = lmh_sensor->trip[trip].value;
	up_read(&lmh_sensor->lock);
	up_read(&lmh_mon_access_lock);

	return 0;
}

static int lmh_set_trip_value(struct thermal_zone_device *dev,
		int trip, unsigned long value)
{
	struct lmh_mon_sensor_data *lmh_sensor;

	if (!dev || !dev->devdata || trip < 0 || trip >= LMH_TRIP_MAX) {
		pr_err("Invalid input\n");
		return -EINVAL;
	}

	lmh_sensor = dev->devdata;
	down_read(&lmh_mon_access_lock);
	down_write(&lmh_sensor->lock);
	lmh_sensor->trip[trip].value = value;
	up_write(&lmh_sensor->lock);
	up_read(&lmh_mon_access_lock);

	return 0;
}

static struct thermal_zone_device_ops lmh_sens_ops = {
	.get_temp = lmh_sensor_read,
	.get_mode = lmh_get_mode,
	.get_trip_type = lmh_get_trip_type,
	.activate_trip_type = lmh_activate_trip,
	.get_trip_temp = lmh_get_trip_value,
	.set_trip_temp = lmh_set_trip_value,
};

static int lmh_register_sensor(struct lmh_mon_sensor_data *lmh_sensor)
{
	int ret = 0;

	lmh_sensor->tzdev = thermal_zone_device_register(
			lmh_sensor->sensor_name, LMH_TRIP_MAX,
			(1 << LMH_TRIP_MAX) - 1, lmh_sensor, &lmh_sens_ops,
			NULL, 0 , 0);
	if (IS_ERR_OR_NULL(lmh_sensor->tzdev)) {
		ret = PTR_ERR(lmh_sensor->tzdev);
		pr_err("Error registering sensor:[%s] with thermal. err:%d\n",
			lmh_sensor->sensor_name, ret);
		return ret;
	}

	return ret;
}

static int lmh_sensor_init(struct lmh_mon_sensor_data *lmh_sensor,
	       char *sensor_name, struct lmh_sensor_ops *ops)
{
	int idx = 0, ret = 0;

	lmh_sensor->state = LMH_ISR_MONITOR;
	strlcpy(lmh_sensor->sensor_name, sensor_name, LMH_NAME_MAX);
	lmh_sensor->sensor_ops = ops;
	ops->interrupt_notify = lmh_interrupt_notify;
	INIT_DELAYED_WORK(&lmh_sensor->isr_poll, lmh_interrupt_monitor);
	for (idx = 0; idx < LMH_TRIP_MAX; idx++) {
		lmh_sensor->trip[idx].value = 0;
		lmh_sensor->trip[idx].active = false;
	}
	init_rwsem(&lmh_sensor->lock);
	if (list_empty(&lmh_sensor_list)
		&& !lmh_mon_data->enable_hw_log)
		lmh_create_debugfs_nodes(lmh_sensor);
	list_add_tail(&lmh_sensor->list_ptr, &lmh_sensor_list);

	return ret;
}

int lmh_sensor_register(char *sensor_name, struct lmh_sensor_ops *ops)
{
	int ret = 0;
	struct lmh_mon_sensor_data *lmh_sensor = NULL;

	if (!sensor_name || !ops) {
		pr_err("Invalid input\n");
		return -EINVAL;
	}

	if (!ops->read || !ops->reset_interrupt || !ops->enable_hw_log
		|| !ops->disable_hw_log) {
		pr_err("Invalid ops input for sensor:%s\n", sensor_name);
		return -EINVAL;
	}
	down_write(&lmh_mon_access_lock);
	if (lmh_match_sensor_name(sensor_name)
		|| lmh_match_sensor_ops(ops)) {
		ret = -EEXIST;
		pr_err("Sensor[%s] exists\n", sensor_name);
		goto register_exit;
	}
	lmh_sensor = kzalloc(sizeof(struct lmh_mon_sensor_data), GFP_KERNEL);
	if (!lmh_sensor) {
		pr_err("kzalloc failed\n");
		ret = -ENOMEM;
		goto register_exit;
	}
	ret = lmh_sensor_init(lmh_sensor, sensor_name, ops);
	if (ret) {
		pr_err("Error registering sensor:%s. err:%d\n", sensor_name,
			ret);
		kfree(lmh_sensor);
		goto register_exit;
	}

	pr_debug("Registered Sensor:[%s]\n", sensor_name);

register_exit:
	up_write(&lmh_mon_access_lock);
	if (ret)
		return ret;
	ret = lmh_register_sensor(lmh_sensor);
	if (ret) {
		pr_err("Thermal Zone register failed for Sensor:[%s]\n"
			, sensor_name);
		return ret;
	}
	pr_debug("Registered Sensor:[%s]\n", sensor_name);
	return ret;
}

static void lmh_sensor_remove(struct lmh_sensor_ops *ops)
{
	struct lmh_mon_sensor_data *lmh_sensor = NULL;

	lmh_sensor = lmh_match_sensor_ops(ops);
	if (!lmh_sensor) {
		pr_err("No match for the sensor\n");
		goto deregister_exit;
	}
	down_write(&lmh_sensor->lock);
	cancel_delayed_work_sync(&lmh_sensor->isr_poll);
	lmh_sensor->state = LMH_ISR_DISABLED;
	thermal_zone_device_unregister(lmh_sensor->tzdev);
	list_del(&lmh_sensor->list_ptr);
	up_write(&lmh_sensor->lock);
	pr_debug("Deregistered sensor:[%s]\n", lmh_sensor->sensor_name);
	kfree(lmh_sensor);

deregister_exit:
	return;
}

void lmh_sensor_deregister(struct lmh_sensor_ops *ops)
{
	if (!ops) {
		pr_err("Invalid input\n");
		return;
	}

	down_write(&lmh_mon_access_lock);
	lmh_sensor_remove(ops);
	up_write(&lmh_mon_access_lock);

	return;
}

static struct lmh_device_data *lmh_match_device_name(char *device_name)
{
	struct lmh_device_data *lmh_device = NULL;

	list_for_each_entry(lmh_device, &lmh_device_list, list_ptr) {
		if (!strnicmp(lmh_device->device_name, device_name,
			LMH_NAME_MAX))
			return lmh_device;
	}

	return NULL;
}

static struct lmh_device_data *lmh_match_device_ops(struct lmh_device_ops *ops)
{
	struct lmh_device_data *lmh_device = NULL;

	list_for_each_entry(lmh_device, &lmh_device_list, list_ptr) {
		if (lmh_device->device_ops == ops)
			return lmh_device;
	}

	return NULL;
}

static int lmh_device_init(struct lmh_device_data *lmh_device,
		char *device_name, struct lmh_device_ops *ops)
{
	int ret = 0;

	ret = ops->get_curr_level(ops, &lmh_device->curr_level);
	if (ret) {
		pr_err("Error getting curr level for Device:[%s]. err:%d\n",
			device_name, ret);
		goto dev_init_exit;
	}
	ret = ops->get_available_levels(ops, NULL);
	if (ret <= 0) {
		pr_err("Error getting max level for Device:[%s]. err:%d\n",
			device_name, ret);
		ret = (!ret) ? -EINVAL : ret;
		goto dev_init_exit;
	}
	lmh_device->max_level = ret;
	lmh_device->levels = kzalloc(lmh_device->max_level * sizeof(int),
				GFP_KERNEL);
	if (!lmh_device->levels) {
		pr_err("No memory\n");
		ret = -ENOMEM;
		goto dev_init_exit;
	}
	ret = ops->get_available_levels(ops, lmh_device->levels);
	if (ret) {
		pr_err("Error getting device:[%s] levels. err:%d\n",
			device_name, ret);
		goto dev_init_exit;
	}
	init_rwsem(&lmh_device->lock);
	lmh_device->device_ops = ops;
	strlcpy(lmh_device->device_name, device_name, LMH_NAME_MAX);
	list_add_tail(&lmh_device->list_ptr, &lmh_device_list);
	lmh_create_dev_debugfs(lmh_device);

dev_init_exit:
	if (ret)
		kfree(lmh_device->levels);
	return ret;
}

int lmh_get_all_dev_levels(char *device_name, int *val)
{
	int ret = 0;
	struct lmh_device_data *lmh_device = NULL;

	if (!device_name) {
		pr_err("Invalid input\n");
		return -EINVAL;
	}
	down_read(&lmh_dev_access_lock);
	lmh_device = lmh_match_device_name(device_name);
	if (!lmh_device) {
		pr_err("Invalid device:%s\n", device_name);
		ret = -EINVAL;
		goto get_all_lvl_exit;
	}
	down_read(&lmh_device->lock);
	if (!val) {
		ret = lmh_device->max_level;
		goto get_all_lvl_exit;
	}
	memcpy(val, lmh_device->levels,
		sizeof(int) * lmh_device->max_level);

get_all_lvl_exit:
	if (lmh_device)
		up_read(&lmh_device->lock);
	up_read(&lmh_dev_access_lock);
	return ret;
}

int lmh_set_dev_level(char *device_name, int curr_lvl)
{
	int ret = 0;
	struct lmh_device_data *lmh_device = NULL;

	if (!device_name) {
		pr_err("Invalid input\n");
		return -EINVAL;
	}
	down_read(&lmh_dev_access_lock);
	lmh_device = lmh_match_device_name(device_name);
	if (!lmh_device) {
		pr_err("Invalid device:%s\n", device_name);
		ret = -EINVAL;
		goto set_dev_exit;
	}
	down_write(&lmh_device->lock);
	curr_lvl = min(curr_lvl, lmh_device->levels[lmh_device->max_level - 1]);
	curr_lvl = max(curr_lvl, lmh_device->levels[0]);
	if (curr_lvl == lmh_device->curr_level)
		goto set_dev_exit;
	ret = lmh_device->device_ops->set_level(lmh_device->device_ops,
			curr_lvl);
	if (ret) {
		pr_err("Error setting current level%d for device[%s]. err:%d\n",
			curr_lvl, device_name, ret);
		goto set_dev_exit;
	}
	pr_debug("Device:[%s] configured to level %d\n", device_name, curr_lvl);
	lmh_device->curr_level = curr_lvl;

set_dev_exit:
	if (lmh_device)
		up_write(&lmh_device->lock);
	up_read(&lmh_dev_access_lock);
	return ret;
}

int lmh_get_curr_level(char *device_name, int *val)
{
	int ret = 0;
	struct lmh_device_data *lmh_device = NULL;

	if (!device_name || !val) {
		pr_err("Invalid input\n");
		return -EINVAL;
	}
	down_read(&lmh_dev_access_lock);
	lmh_device = lmh_match_device_name(device_name);
	if (!lmh_device) {
		pr_err("Invalid device:%s\n", device_name);
		ret = -EINVAL;
		goto get_curr_level;
	}
	down_read(&lmh_device->lock);
	ret = lmh_device->device_ops->get_curr_level(lmh_device->device_ops,
			&lmh_device->curr_level);
	if (ret) {
		pr_err("Error getting device[%s] current level. err:%d\n",
			device_name, ret);
		goto get_curr_level;
	}
	*val = lmh_device->curr_level;
	pr_debug("Device:%s current level:%d\n", device_name, *val);

get_curr_level:
	if (lmh_device)
		up_read(&lmh_device->lock);
	up_read(&lmh_dev_access_lock);
	return ret;
}

int lmh_device_register(char *device_name, struct lmh_device_ops *ops)
{
	int ret = 0;
	struct lmh_device_data *lmh_device = NULL;

	if (!device_name || !ops) {
		pr_err("Invalid input\n");
		return -EINVAL;
	}

	if (!ops->get_available_levels || !ops->get_curr_level
		|| !ops->set_level) {
		pr_err("Invalid ops input for device:%s\n", device_name);
		return -EINVAL;
	}

	down_write(&lmh_dev_access_lock);
	if (lmh_match_device_name(device_name)
		|| lmh_match_device_ops(ops)) {
		ret = -EEXIST;
		pr_err("Device[%s] allready exists\n", device_name);
		goto register_exit;
	}
	lmh_device = kzalloc(sizeof(struct lmh_device_data), GFP_KERNEL);
	if (!lmh_device) {
		pr_err("kzalloc failed\n");
		ret = -ENOMEM;
		goto register_exit;
	}
	ret = lmh_device_init(lmh_device, device_name, ops);
	if (ret) {
		pr_err("Error registering device:%s. err:%d\n", device_name,
			ret);
		kfree(lmh_device);
		goto register_exit;
	}

	pr_debug("Registered Device:[%s] with %d levels\n", device_name,
			lmh_device->max_level);

register_exit:
	up_write(&lmh_dev_access_lock);
	return ret;
}

static void lmh_device_remove(struct lmh_device_ops *ops)
{
	struct lmh_device_data *lmh_device = NULL;

	lmh_device = lmh_match_device_ops(ops);
	if (!lmh_device) {
		pr_err("No match for the device\n");
		goto deregister_exit;
	}
	down_write(&lmh_device->lock);
	list_del(&lmh_device->list_ptr);
	pr_debug("Deregistered device:[%s]\n", lmh_device->device_name);
	kfree(lmh_device->levels);
	up_write(&lmh_device->lock);
	kfree(lmh_device);

deregister_exit:
	return;
}

void lmh_device_deregister(struct lmh_device_ops *ops)
{
	if (!ops) {
		pr_err("Invalid input\n");
		return;
	}

	down_write(&lmh_dev_access_lock);
	lmh_device_remove(ops);
	up_write(&lmh_dev_access_lock);
	return;
}

static int lmh_mon_init_driver(void)
{
	int ret = 0;

	lmh_mon_data = kzalloc(sizeof(lmh_mon_data), GFP_KERNEL);
	if (!lmh_mon_data) {
		pr_err("No memory\n");
		return -ENOMEM;
	}

	LMH_CREATE_DEBUGFS_DIR(lmh_mon_data->debugfs_parent, LMH_MON_NAME,
				NULL, ret);
	if (ret)
		goto init_exit;
	lmh_mon_data->poll_fs = debugfs_create_u32(LMH_ISR_POLL_DELAY, 0600,
			lmh_mon_data->debugfs_parent, &lmh_poll_interval);
	if (IS_ERR(lmh_mon_data->poll_fs))
		pr_err("Error creating debugfs:[%s]. err:%ld\n",
			LMH_ISR_POLL_DELAY, PTR_ERR(lmh_mon_data->poll_fs));

init_exit:
	if (ret == -ENODEV)
		ret = 0;
	return ret;
}

static int __init lmh_mon_init_call(void)
{
	int ret = 0;

	ret = lmh_mon_init_driver();
	if (ret) {
		pr_err("Error initializing the debugfs. err:%d\n", ret);
		goto lmh_init_exit;
	}

lmh_init_exit:
	return ret;
}

static void lmh_mon_cleanup(void)
{
	down_write(&lmh_mon_access_lock);
	while (!list_empty(&lmh_sensor_list)) {
		lmh_sensor_remove(list_first_entry(&lmh_sensor_list,
			struct lmh_mon_sensor_data, list_ptr)->sensor_ops);
	}
	up_write(&lmh_mon_access_lock);
	debugfs_remove_recursive(lmh_mon_data->debugfs_parent);
	kfree(lmh_mon_data);
}

static void lmh_device_cleanup(void)
{
	down_write(&lmh_dev_access_lock);
	while (!list_empty(&lmh_device_list)) {
		lmh_device_remove(list_first_entry(&lmh_device_list,
			struct lmh_device_data, list_ptr)->device_ops);
	}
	up_write(&lmh_dev_access_lock);
}

static void __exit lmh_mon_exit(void)
{
	lmh_mon_cleanup();
	lmh_device_cleanup();
}

module_init(lmh_mon_init_call);
module_exit(lmh_mon_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("LMH monitor driver");
MODULE_ALIAS("platform:" LMH_MON_NAME);
