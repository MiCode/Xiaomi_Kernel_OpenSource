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
 *
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
#include <linux/mutex.h>
#include <linux/msm_bcl.h>
#include <linux/slab.h>

#define BCL_PARAM_MAX_ATTR      3

#define BCL_DEFINE_RO_PARAM(_attr, _name, _attr_gp, _index) \
	_attr.attr.name = __stringify(_name); \
	_attr.attr.mode = 0444; \
	_attr.show = _name##_show; \
	_attr_gp.attrs[_index] = &_attr.attr;

static struct bcl_param_data *bcl[BCL_PARAM_MAX];

static ssize_t high_trip_show(struct kobject *kobj, struct kobj_attribute *attr,
				char *buf)
{
	int val = 0, ret = 0;
	struct bcl_param_data *dev_param = container_of(attr,
			struct bcl_param_data, high_trip_attr);

	if (!dev_param->registered)
		return -ENODEV;

	ret = dev_param->ops->get_high_trip(&val);
	if (ret) {
		pr_err("High trip value read failed. err:%d\n", ret);
		return ret;
	}

	return snprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t low_trip_show(struct kobject *kobj, struct kobj_attribute *attr,
				char *buf)
{
	int val = 0, ret = 0;
	struct bcl_param_data *dev_param = container_of(attr,
			struct bcl_param_data, low_trip_attr);

	if (!dev_param->registered)
		return -ENODEV;

	ret = dev_param->ops->get_low_trip(&val);
	if (ret) {
		pr_err("Low trip value read failed. err:%d\n", ret);
		return ret;
	}

	return snprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t value_show(struct kobject *kobj, struct kobj_attribute *attr,
				char *buf)
{
	int32_t val = 0, ret = 0;
	struct bcl_param_data *dev_param = container_of(attr,
			struct bcl_param_data, val_attr);

	if (!dev_param->registered)
		return -ENODEV;

	ret = dev_param->ops->read(&val);
	if (ret) {
		pr_err("Value read failed. err:%d\n", ret);
		return ret;
	}
	dev_param->last_read_val = val;

	return snprintf(buf, PAGE_SIZE, "%d\n", val);
}

int msm_bcl_set_threshold(enum bcl_param param_type,
	enum bcl_trip_type trip_type, struct bcl_threshold *inp_thresh)
{
	int ret = 0;

	if (!bcl[param_type] || !bcl[param_type]->registered) {
		pr_err("BCL not initialized\n");
		return -EINVAL;
	}
	if ((!inp_thresh)
		|| (inp_thresh->trip_value < 0)
		|| (!inp_thresh->trip_notify)
		|| (param_type >= BCL_PARAM_MAX)
		|| (trip_type >= BCL_TRIP_MAX)) {
		pr_err("Invalid Input\n");
		return -EINVAL;
	}

	bcl[param_type]->thresh[trip_type] = inp_thresh;
	if (trip_type == BCL_HIGH_TRIP) {
		bcl[param_type]->high_trip = inp_thresh->trip_value;
		ret = bcl[param_type]->ops->set_high_trip(
			inp_thresh->trip_value);
	} else {
		bcl[param_type]->low_trip = inp_thresh->trip_value;
		ret = bcl[param_type]->ops->set_low_trip(
			inp_thresh->trip_value);
	}
	if (ret) {
		pr_err("Error setting trip%d for param%d. err:%d\n", trip_type,
				 param_type, ret);
		return ret;
	}

	return ret;
}

static int bcl_thresh_notify(struct bcl_param_data *param_data, int val,
					enum bcl_trip_type trip_type)
{
	if (!param_data || trip_type >= BCL_TRIP_MAX
		|| !param_data->registered) {
		pr_err("Invalid input\n");
		return -EINVAL;
	}

	param_data->thresh[trip_type]->trip_notify(trip_type, val,
		param_data->thresh[trip_type]->trip_data);

	return 0;
}

static int bcl_add_sysfs_nodes(enum bcl_param param_type);
struct bcl_param_data *msm_bcl_register_param(enum bcl_param param_type,
	struct bcl_driver_ops *param_ops, char *name)
{
	int ret = 0;

	if (!bcl[param_type]
		|| param_type >= BCL_PARAM_MAX || !param_ops || !name
		|| !param_ops->read || !param_ops->set_high_trip
		|| !param_ops->get_high_trip || !param_ops->set_low_trip
		|| !param_ops->get_low_trip || !param_ops->enable
		|| !param_ops->disable) {
		pr_err("Invalid input\n");
		return NULL;
	}
	if (bcl[param_type]->registered) {
		pr_err("param%d already initialized\n", param_type);
		return NULL;
	}

	ret = bcl_add_sysfs_nodes(param_type);
	if (ret) {
		pr_err("Error creating sysfs nodes. err:%d\n", ret);
		return NULL;
	}
	bcl[param_type]->ops = param_ops;
	bcl[param_type]->registered = true;
	strlcpy(bcl[param_type]->name, name, BCL_NAME_MAX_LEN);
	param_ops->notify = bcl_thresh_notify;

	return bcl[param_type];
}

int msm_bcl_unregister_param(struct bcl_param_data *param_data)
{
	int i = 0, ret = -EINVAL;

	if (!bcl[i] || !param_data) {
		pr_err("Invalid input\n");
		return ret;
	}
	for (; i < BCL_PARAM_MAX; i++) {
		if (param_data != bcl[i])
			continue;
		bcl[i]->ops->disable();
		bcl[i]->registered = false;
		ret = 0;
		break;
	}

	return ret;
}

int msm_bcl_disable(void)
{
	int ret = 0, i = 0;

	if (!bcl[i]) {
		pr_err("BCL not initialized\n");
		return -EINVAL;
	}

	for (; i < BCL_PARAM_MAX; i++) {
		if (!bcl[i]->registered)
			continue;
		ret = bcl[i]->ops->disable();
		if (ret) {
			pr_err("Error in disabling interrupt. param:%d err%d\n",
				i, ret);
			return ret;
		}
	}

	return ret;
}

int msm_bcl_enable(void)
{
	int ret = 0, i = 0;
	struct bcl_param_data *param_data = NULL;

	if (!bcl[i] || !bcl[BCL_PARAM_VOLTAGE]->thresh
		|| !bcl[BCL_PARAM_CURRENT]->thresh) {
		pr_err("BCL not initialized\n");
		return -EINVAL;
	}

	for (; i < BCL_PARAM_MAX; i++) {
		if (!bcl[i]->registered)
			continue;
		param_data = bcl[i];
		ret = param_data->ops->set_high_trip(param_data->high_trip);
		if (ret) {
			pr_err("Error setting high trip. param:%d. err:%d",
				i, ret);
			return ret;
		}
		ret = param_data->ops->set_low_trip(param_data->low_trip);
		if (ret) {
			pr_err("Error setting low trip. param:%d. err:%d",
				i, ret);
			return ret;
		}
		ret = param_data->ops->enable();
		if (ret) {
			pr_err("Error enabling interrupt. param:%d. err:%d",
				i, ret);
			return ret;
		}
	}

	return ret;
}

int msm_bcl_read(enum bcl_param param_type, int *value)
{
	int ret = 0;

	if (!value || param_type >= BCL_PARAM_MAX) {
		pr_err("Invalid input\n");
		return -EINVAL;
	}
	if (!bcl[param_type] || !bcl[param_type]->registered) {
		pr_err("BCL driver not initialized\n");
		return -ENOSYS;
	}

	ret = bcl[param_type]->ops->read(value);
	if (ret) {
		pr_err("Error reading param%d. err:%d\n", param_type, ret);
		return ret;
	}
	bcl[param_type]->last_read_val = *value;

	return ret;
}

static struct class msm_bcl_class = {
	.name = "msm_bcl",
};

static int bcl_add_sysfs_nodes(enum bcl_param param_type)
{
	char *param_name[BCL_PARAM_MAX] = {"voltage", "current"};
	int ret = 0;

	bcl[param_type]->device.class = &msm_bcl_class;
	dev_set_name(&bcl[param_type]->device, "%s", param_name[param_type]);
	ret = device_register(&bcl[param_type]->device);
	if (ret) {
		pr_err("Error registering device %s. err:%d\n",
			param_name[param_type], ret);
		return ret;
	}
	bcl[param_type]->bcl_attr_gp.attrs = kzalloc(sizeof(struct attribute *)
		* (BCL_PARAM_MAX_ATTR + 1), GFP_KERNEL);
	if (!bcl[param_type]->bcl_attr_gp.attrs) {
		pr_err("Sysfs attribute create failed.\n");
		ret = -ENOMEM;
		goto add_sysfs_exit;
	}
	BCL_DEFINE_RO_PARAM(bcl[param_type]->val_attr, value,
		bcl[param_type]->bcl_attr_gp, 0);
	BCL_DEFINE_RO_PARAM(bcl[param_type]->high_trip_attr, high_trip,
		bcl[param_type]->bcl_attr_gp, 1);
	BCL_DEFINE_RO_PARAM(bcl[param_type]->low_trip_attr, low_trip,
		bcl[param_type]->bcl_attr_gp, 2);
	bcl[param_type]->bcl_attr_gp.attrs[BCL_PARAM_MAX_ATTR] = NULL;

	ret = sysfs_create_group(&bcl[param_type]->device.kobj,
		&bcl[param_type]->bcl_attr_gp);
	if (ret) {
		pr_err("Failure to create sysfs nodes. err:%d", ret);
		goto add_sysfs_exit;
	}

add_sysfs_exit:
	return ret;
}

static int msm_bcl_init(void)
{
	int ret = 0, i = 0;

	for (; i < BCL_PARAM_MAX; i++) {
		bcl[i] = kzalloc(sizeof(struct bcl_param_data),
			GFP_KERNEL);
		if (!bcl[i]) {
			pr_err("kzalloc failed\n");
			while ((--i) >= 0)
				kfree(bcl[i]);
			return -ENOMEM;
		}
	}

	return ret;
}


static int __init msm_bcl_init_driver(void)
{
	int ret = 0;

	ret = msm_bcl_init();
	if (ret) {
		pr_err("msm bcl init failed. err:%d\n", ret);
		return ret;
	}
	return class_register(&msm_bcl_class);
}

static void __exit bcl_exit(void)
{
	int i = 0;

	for (; i < BCL_PARAM_MAX; i++) {
		sysfs_remove_group(&bcl[i]->device.kobj,
			&bcl[i]->bcl_attr_gp);
		kfree(bcl[i]->bcl_attr_gp.attrs);
		kfree(bcl[i]);
	}
	class_unregister(&msm_bcl_class);
}

fs_initcall(msm_bcl_init_driver);
module_exit(bcl_exit);
