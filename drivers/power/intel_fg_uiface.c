/*
 * intel_fg_usr_iface.c - Intel FG algo interface
 *
 * Copyright (C) 2014 Intel Corporation
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the GNU
 * General Public License for more details.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Author: Ramakrishna Pallala <ramakrishna.pallala@intel.com>
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/power/intel_fuel_gauge.h>
#include <linux/kdev_t.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/miscdevice.h>

#define DRIVER_NAME		"intel_fg_iface"

struct fg_iface_info {
	struct platform_device *pdev;
	struct mutex lock;

	int vbatt_boot;
	int ibat_boot;
	int vbatt;
	int vavg;
	int vocv;
	int ibatt;
	int iavg;
	int bat_temp;
	int delta_q;

	int soc;
	int nac;
	int fcc;
	int cycle_count;
	int calib_cc;

	/* Coulomb Counter params */
	int up_cc;
	int down_cc;
	int acc_err;
	int delta_thr;
	int long_avg;
	int long_avg_at_ocv;
	int ocv_accuracy;

	struct set_cc_val reset_acc_err;
	struct set_cc_val set_delta_thr;
	struct set_cc_val clr_latched_ibat_avg;

	wait_queue_head_t wait;
	bool uevent_ack;
	bool suspended;

	struct miscdevice intel_fg_misc_device;
};

static struct fg_iface_info *info_ptr;


static ssize_t fg_iface_get_volt_now(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	int ret, val;

	mutex_lock(&info_ptr->lock);
	val = info_ptr->vbatt;
	mutex_unlock(&info_ptr->lock);
	ret = sprintf(buf, "%d\n", val);
	return ret;
}

static ssize_t fg_iface_get_volt_ocv(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	int ret, val;

	mutex_lock(&info_ptr->lock);
	val = info_ptr->vocv;
	mutex_unlock(&info_ptr->lock);
	ret = sprintf(buf, "%d\n", val);
	return ret;
}

static ssize_t fg_iface_get_volt_boot(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	int ret, val;

	mutex_lock(&info_ptr->lock);
	val = info_ptr->vbatt_boot;
	mutex_unlock(&info_ptr->lock);
	ret = sprintf(buf, "%d\n", val);
	return ret;
}

static ssize_t fg_iface_get_ibat_boot(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	int ret, val;

	mutex_lock(&info_ptr->lock);
	val = info_ptr->ibat_boot;
	mutex_unlock(&info_ptr->lock);
	ret = sprintf(buf, "%d\n", val);
	return ret;
}

static ssize_t fg_iface_get_cur_now(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	int ret, val;

	mutex_lock(&info_ptr->lock);
	val = info_ptr->ibatt;
	mutex_unlock(&info_ptr->lock);
	ret = sprintf(buf, "%d\n", val);
	return ret;
}

static ssize_t fg_iface_get_cur_avg(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	int ret, val;

	mutex_lock(&info_ptr->lock);
	val = info_ptr->iavg;
	mutex_unlock(&info_ptr->lock);
	ret = sprintf(buf, "%d\n", val);
	return ret;
}

static ssize_t fg_iface_get_batt_temp(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	int ret, val;

	mutex_lock(&info_ptr->lock);
	val = info_ptr->bat_temp;
	mutex_unlock(&info_ptr->lock);
	ret = sprintf(buf, "%d\n", val);
	return ret;
}

static ssize_t fg_iface_get_delta_q(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	int ret, val;

	mutex_lock(&info_ptr->lock);
	val = info_ptr->delta_q;
	mutex_unlock(&info_ptr->lock);
	ret = sprintf(buf, "%d\n", val);
	return ret;
}

static ssize_t fg_iface_get_capacity(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	int ret, val;

	mutex_lock(&info_ptr->lock);
	val = info_ptr->soc;
	mutex_unlock(&info_ptr->lock);
	ret = sprintf(buf, "%d\n", val);
	return ret;
}

static ssize_t fg_iface_set_capacity(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	long val;

	if (kstrtol(buf, 10, &val) < 0)
		return -EINVAL;

	mutex_lock(&info_ptr->lock);
	info_ptr->soc = val;
	mutex_unlock(&info_ptr->lock);
	return count;
}

static ssize_t fg_iface_get_nac(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int ret, val;

	mutex_lock(&info_ptr->lock);
	val = info_ptr->nac;
	mutex_unlock(&info_ptr->lock);
	ret = sprintf(buf, "%d\n", val);
	return ret;
}

static ssize_t fg_iface_set_nac(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	long val;

	if (kstrtol(buf, 10, &val) < 0)
		return -EINVAL;

	mutex_lock(&info_ptr->lock);
	info_ptr->nac = val;
	mutex_unlock(&info_ptr->lock);
	return count;
}

static ssize_t fg_iface_get_fcc(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int ret, val;

	mutex_lock(&info_ptr->lock);
	val = info_ptr->fcc;
	mutex_unlock(&info_ptr->lock);
	ret = sprintf(buf, "%d\n", val);
	return ret;
}

static ssize_t fg_iface_set_fcc(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	long val;

	if (kstrtol(buf, 10, &val) < 0)
		return -EINVAL;

	mutex_lock(&info_ptr->lock);
	info_ptr->fcc = val;
	mutex_unlock(&info_ptr->lock);
	return count;
}

static ssize_t fg_iface_get_cyc_cnt(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int ret, val;

	mutex_lock(&info_ptr->lock);
	val = info_ptr->cycle_count;
	mutex_unlock(&info_ptr->lock);
	ret = sprintf(buf, "%d\n", val);
	return ret;
}

static ssize_t fg_iface_set_cyc_cnt(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	long val;

	if (kstrtol(buf, 10, &val) < 0)
		return -EINVAL;

	mutex_lock(&info_ptr->lock);
	info_ptr->cycle_count = val;
	mutex_unlock(&info_ptr->lock);
	return count;
}


static ssize_t fg_iface_get_cc_calib(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int ret, val;

	mutex_lock(&info_ptr->lock);
	val = info_ptr->calib_cc;
	mutex_unlock(&info_ptr->lock);
	ret = sprintf(buf, "%d\n", val);
	return ret;
}

static ssize_t fg_iface_set_cc_calib(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	long val;

	if (kstrtol(buf, 10, &val) < 0)
		return -EINVAL;

	mutex_lock(&info_ptr->lock);
	info_ptr->calib_cc = val;
	info_ptr->uevent_ack = true;
	wake_up(&info_ptr->wait);
	mutex_unlock(&info_ptr->lock);
	return count;
}

/* Coulomb Counter input attribute APIs */

static ssize_t fg_iface_get_up_cc(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ret, val;

	mutex_lock(&info_ptr->lock);
	val = info_ptr->up_cc;
	mutex_unlock(&info_ptr->lock);
	ret = sprintf(buf, "%d\n", val);
	return ret;
}

static ssize_t fg_iface_get_down_cc(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ret, val;

	mutex_lock(&info_ptr->lock);
	val = info_ptr->down_cc;
	mutex_unlock(&info_ptr->lock);
	ret = sprintf(buf, "%d\n", val);
	return ret;
}

static ssize_t fg_iface_get_acc_err(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ret, val;

	mutex_lock(&info_ptr->lock);
	val = info_ptr->acc_err;
	mutex_unlock(&info_ptr->lock);
	ret = sprintf(buf, "%d\n", val);
	return ret;
}

static ssize_t fg_iface_get_delta_thr(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ret, val;

	mutex_lock(&info_ptr->lock);
	val = info_ptr->delta_thr;
	mutex_unlock(&info_ptr->lock);
	ret = sprintf(buf, "%d\n", val);
	return ret;
}

static ssize_t fg_iface_get_long_avg(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ret, val;

	mutex_lock(&info_ptr->lock);
	val = info_ptr->long_avg;
	mutex_unlock(&info_ptr->lock);
	ret = sprintf(buf, "%d\n", val);
	return ret;
}

static ssize_t fg_iface_get_long_avg_ocv(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ret, val;

	mutex_lock(&info_ptr->lock);
	val = info_ptr->long_avg_at_ocv;
	mutex_unlock(&info_ptr->lock);
	ret = sprintf(buf, "%d\n", val);
	return ret;
}

static ssize_t fg_iface_get_ocv_accuracy(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ret, val;

	mutex_lock(&info_ptr->lock);
	val = info_ptr->ocv_accuracy;
	mutex_unlock(&info_ptr->lock);
	ret = sprintf(buf, "%d\n", val);
	return ret;
}

/* Coulomb Counter Output attributes */

static ssize_t fg_iface_set_delta_thr(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	long val;

	if (kstrtol(buf, 10, &val) < 0)
		return -EINVAL;

	mutex_lock(&info_ptr->lock);
	info_ptr->set_delta_thr.val = val;
	info_ptr->set_delta_thr.is_set = true;
	mutex_unlock(&info_ptr->lock);
	return count;
}

static ssize_t fg_iface_reset_acc_err(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	long val;

	if (kstrtol(buf, 10, &val) < 0)
		return -EINVAL;

	mutex_lock(&info_ptr->lock);
	info_ptr->reset_acc_err.val = val;
	info_ptr->reset_acc_err.is_set = true;
	mutex_unlock(&info_ptr->lock);
	return count;
}

static ssize_t fg_iface_clr_latched_ibat_avg(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	long val;

	if (kstrtol(buf, 10, &val) < 0)
		return -EINVAL;

	mutex_lock(&info_ptr->lock);
	info_ptr->clr_latched_ibat_avg.val = val;
	info_ptr->clr_latched_ibat_avg.is_set = true;
	mutex_unlock(&info_ptr->lock);
	return count;
}

static DEVICE_ATTR(volt_now, S_IRUGO,
			fg_iface_get_volt_now, NULL);
static DEVICE_ATTR(volt_ocv, S_IRUGO,
			fg_iface_get_volt_ocv, NULL);
static DEVICE_ATTR(volt_boot, S_IRUGO,
			fg_iface_get_volt_boot, NULL);
static DEVICE_ATTR(ibat_boot, S_IRUGO,
			fg_iface_get_ibat_boot, NULL);
static DEVICE_ATTR(cur_now, S_IRUGO,
			fg_iface_get_cur_now, NULL);
static DEVICE_ATTR(cur_avg, S_IRUGO,
			fg_iface_get_cur_avg, NULL);
static DEVICE_ATTR(batt_temp, S_IRUGO,
			fg_iface_get_batt_temp, NULL);
static DEVICE_ATTR(delta_q, S_IRUGO,
			fg_iface_get_delta_q, NULL);


static DEVICE_ATTR(capacity, S_IWUSR | S_IRUGO,
		fg_iface_get_capacity, fg_iface_set_capacity);
static DEVICE_ATTR(nac, S_IWUSR | S_IRUGO,
		fg_iface_get_nac, fg_iface_set_nac);
static DEVICE_ATTR(fcc, S_IWUSR | S_IRUGO,
		fg_iface_get_fcc, fg_iface_set_fcc);
static DEVICE_ATTR(cyc_cnt, S_IWUSR | S_IRUGO,
		fg_iface_get_cyc_cnt, fg_iface_set_cyc_cnt);
static DEVICE_ATTR(cc_calib, S_IWUSR | S_IRUGO,
		fg_iface_get_cc_calib, fg_iface_set_cc_calib);


/* Coulomb Counter Read-only attribute */
static DEVICE_ATTR(up_cc, S_IRUGO,
			fg_iface_get_up_cc, NULL);
static DEVICE_ATTR(down_cc, S_IRUGO,
			fg_iface_get_down_cc, NULL);
static DEVICE_ATTR(long_avg, S_IRUGO,
			fg_iface_get_long_avg, NULL);
static DEVICE_ATTR(long_avg_ocv, S_IRUGO,
			fg_iface_get_long_avg_ocv, NULL);
static DEVICE_ATTR(ocv_accuracy, S_IRUGO,
			fg_iface_get_ocv_accuracy, NULL);
static DEVICE_ATTR(acc_err, S_IRUGO,
			fg_iface_get_acc_err, NULL);

/* Coulomb Counter Read/Write attributes */
static DEVICE_ATTR(reset_acc_err, S_IWUSR | S_IRUGO,
		NULL, fg_iface_reset_acc_err);
static DEVICE_ATTR(delta_thr, S_IWUSR | S_IRUGO,
		fg_iface_get_delta_thr, fg_iface_set_delta_thr);
static DEVICE_ATTR(clr_latched_ibat_avg, S_IWUSR | S_IRUGO,
		NULL, fg_iface_clr_latched_ibat_avg);

static struct attribute *fg_iface_sysfs_attributes[] = {
	&dev_attr_volt_now.attr,
	&dev_attr_volt_ocv.attr,
	&dev_attr_volt_boot.attr,
	&dev_attr_ibat_boot.attr,
	&dev_attr_cur_now.attr,
	&dev_attr_cur_avg.attr,
	&dev_attr_batt_temp.attr,
	&dev_attr_delta_q.attr,
	&dev_attr_capacity.attr,
	&dev_attr_nac.attr,
	&dev_attr_fcc.attr,
	&dev_attr_cyc_cnt.attr,
	&dev_attr_cc_calib.attr,
	/* Coulomb Counter attributes */
	&dev_attr_up_cc.attr,
	&dev_attr_down_cc.attr,
	&dev_attr_long_avg.attr,
	&dev_attr_long_avg_ocv.attr,
	&dev_attr_ocv_accuracy.attr,
	&dev_attr_acc_err.attr,

	&dev_attr_reset_acc_err.attr,
	&dev_attr_delta_thr.attr,
	&dev_attr_clr_latched_ibat_avg.attr,
	NULL,
};

static const struct attribute_group fg_iface_sysfs_attr_group = {
	.attrs = fg_iface_sysfs_attributes,
};

static int fg_iface_sysfs_init(struct fg_iface_info *info)
{
	int ret;

	info->intel_fg_misc_device.minor = MISC_DYNAMIC_MINOR;
	info->intel_fg_misc_device.name = DRIVER_NAME;
	info->intel_fg_misc_device.mode = (S_IWUSR | S_IRUGO);
	ret = misc_register(&info->intel_fg_misc_device);
	if (ret) {
		dev_err(&info->pdev->dev,
			"\nErr %d in registering misc class", ret);
		return ret;
	}
	ret = sysfs_create_group(&info->intel_fg_misc_device.this_device->kobj,
					&fg_iface_sysfs_attr_group);
	if (ret) {
		dev_err(&info->pdev->dev,
			"\nError %d in creating sysfs group", ret);
		misc_deregister(&info->intel_fg_misc_device);
	}
	return ret;
}

static void fg_iface_sysfs_exit(struct fg_iface_info *info)
{
	sysfs_remove_group(&info->pdev->dev.kobj,
					&fg_iface_sysfs_attr_group);
	if (info->intel_fg_misc_device.this_device)
		misc_deregister(&info->intel_fg_misc_device);
}

static int intel_fg_iface_algo_process(struct fg_algo_ip_params *ip,
						struct fg_algo_op_params *op)
{
	int ret;

	mutex_lock(&info_ptr->lock);
	info_ptr->vbatt = ip->vbatt;
	info_ptr->vavg = ip->vavg;
	info_ptr->vocv = ip->vocv;
	info_ptr->ibatt = ip->ibatt;
	info_ptr->iavg = ip->iavg;
	info_ptr->bat_temp = ip->bat_temp;
	info_ptr->delta_q = ip->delta_q;

	/* Coulomb Counter I/P params assignment */
	info_ptr->up_cc = ip->up_cc;
	info_ptr->down_cc = ip->down_cc;
	info_ptr->acc_err = ip->acc_err;
	info_ptr->delta_thr = ip->delta_thr;
	info_ptr->long_avg = ip->long_avg;
	info_ptr->long_avg_at_ocv = ip->long_avg_at_ocv;
	info_ptr->ocv_accuracy = ip->ocv_accuracy;

	/* Add user space event generation mechanism */
	dev_dbg(&info_ptr->pdev->dev, "Sending uevent from intel_fg_uiface\n");

	if (!IS_ERR_OR_NULL(info_ptr->intel_fg_misc_device.this_device))
		sysfs_notify(&info_ptr->intel_fg_misc_device.this_device->kobj,
				NULL, "uevent");

	/*Wait for user space to write back*/
	info_ptr->uevent_ack = false;
	mutex_unlock(&info_ptr->lock);

	if (info_ptr->suspended) {
		dev_err(&info_ptr->pdev->dev, "Error SUSPENDED\n");
		return -ESHUTDOWN;
	}

	/*
	 * Since we need to wait for user space event and since the user space
	 * scheduling depends on the system load and other high priority tasks,
	 * hence, the safe margin to wait for timeout would be 9secs
	 */
	ret = wait_event_timeout(info_ptr->wait,
			info_ptr->uevent_ack == true, 9 * HZ);
	if (0 == ret) {
		dev_err(&info_ptr->pdev->dev,
				"\n Error TIMEOUT waiting for user space write back");
		return -ETIMEDOUT;
	}

	mutex_lock(&info_ptr->lock);
	op->soc = info_ptr->soc;
	op->nac = info_ptr->nac;
	op->fcc = info_ptr->fcc;
	op->cycle_count = info_ptr->cycle_count;
	op->calib_cc = info_ptr->calib_cc;

	/* Coulomb Counter O/P params assignment */
	op->set_delta_thr.val = info_ptr->set_delta_thr.val;
	op->set_delta_thr.is_set = info_ptr->set_delta_thr.is_set;

	op->reset_acc_err.val =
				info_ptr->reset_acc_err.val;
	op->reset_acc_err.is_set =
				info_ptr->reset_acc_err.is_set;

	op->clr_latched_ibat_avg.val =
				info_ptr->clr_latched_ibat_avg.val;
	op->clr_latched_ibat_avg.is_set =
				info_ptr->clr_latched_ibat_avg.is_set;

	mutex_unlock(&info_ptr->lock);

	return 0;
}

static int intel_fg_iface_algo_init(struct fg_batt_params *bat_params)
{
	mutex_lock(&info_ptr->lock);
	info_ptr->vbatt_boot = bat_params->v_ocv_bootup;
	info_ptr->ibat_boot = bat_params->i_bat_bootup;
	info_ptr->vbatt = bat_params->vbatt_now;
	info_ptr->vocv = bat_params->v_ocv_now;
	info_ptr->ibatt = bat_params->i_batt_now;
	info_ptr->iavg = bat_params->i_batt_avg;
	info_ptr->bat_temp = bat_params->batt_temp_now;

	/* Coulomb Counter params initialization */
	info_ptr->up_cc = bat_params->up_cc;
	info_ptr->down_cc = bat_params->down_cc;
	info_ptr->delta_thr = bat_params->delta_thr;
	info_ptr->acc_err = bat_params->acc_err;
	info_ptr->long_avg = bat_params->long_avg;
	info_ptr->long_avg_at_ocv = bat_params->long_avg_ocv;
	info_ptr->ocv_accuracy = bat_params->ocv_accuracy;

	mutex_unlock(&info_ptr->lock);

	fg_iface_sysfs_init(info_ptr);

	return 0;
}

struct intel_fg_algo algo = {
	.type = INTEL_FG_ALGO_PRIMARY,
	.fg_algo_init = intel_fg_iface_algo_init,
	.fg_algo_process = intel_fg_iface_algo_process,
};

static int intel_fg_iface_probe(struct platform_device *pdev)
{
	int ret;
	struct fg_iface_info *info;

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info) {
		dev_err(&pdev->dev, "mem alloc failed\n");
		return -ENOMEM;
	}

	info->pdev = pdev;
	mutex_init(&info->lock);
	info_ptr = info;
	info_ptr->suspended = false;

	init_waitqueue_head(&info->wait);

	ret = intel_fg_register_algo(&algo);
	if (ret < 0) {
		dev_err(&pdev->dev, "FG algo registration error\n");
		mutex_destroy(&info->lock);
	}

	return ret;
}

static int intel_fg_iface_remove(struct platform_device *pdev)
{
	intel_fg_unregister_algo(&algo);
	fg_iface_sysfs_exit(info_ptr);
	return 0;
}

#ifdef CONFIG_PM
static int intel_fg_iface_suspend(struct device *dev)
{
	/* Force ACK uevent to avoid blocking as user space freezing */
	info_ptr->suspended = true;
	mutex_lock(&info_ptr->lock);
	info_ptr->uevent_ack = true;
	wake_up(&info_ptr->wait);
	mutex_unlock(&info_ptr->lock);
	return 0;
}

static int intel_fg_iface_resume(struct device *dev)
{
	info_ptr->suspended = false;
	return 0;
}
#else
#define intel_fg_iface_suspend		NULL
#define intel_fg_iface_resume		NULL
#endif

static const struct dev_pm_ops intel_fg_iface_driver_pm_ops = {
	.suspend	= intel_fg_iface_suspend,
	.resume		= intel_fg_iface_resume,
};

static struct platform_driver intel_fg_iface_driver = {
	.probe = intel_fg_iface_probe,
	.remove = intel_fg_iface_remove,
	.driver = {
		.name = DRIVER_NAME,
		.pm = &intel_fg_iface_driver_pm_ops,
	},
};

static int __init intel_fg_iface_init(void)
{
	return platform_driver_register(&intel_fg_iface_driver);
}
late_initcall(intel_fg_iface_init);

static void __exit intel_fg_iface_exit(void)
{
	platform_driver_unregister(&intel_fg_iface_driver);
}
module_exit(intel_fg_iface_exit);

MODULE_AUTHOR("Ramakrishna Pallala <ramakrishna.pallala@intel.com>");
MODULE_DESCRIPTION("Intel Fuel Gauge interface driver");
MODULE_LICENSE("GPL");
