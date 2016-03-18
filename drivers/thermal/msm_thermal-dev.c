/* Copyright (c) 2013-2016, The Linux Foundation. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/msm_thermal_ioctl.h>
#include <linux/msm_thermal.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/semaphore.h>
#include <linux/module.h>

struct msm_thermal_ioctl_dev {
	struct semaphore sem;
	struct cdev char_dev;
};

static int msm_thermal_major;
static struct class *thermal_class;
static struct msm_thermal_ioctl_dev *msm_thermal_dev;
static unsigned int freq_table_len[NR_CPUS], freq_table_set[NR_CPUS];
static unsigned int voltage_table_set[NR_CPUS];
static unsigned int *freq_table_ptr[NR_CPUS];
static uint32_t *voltage_table_ptr[NR_CPUS];

static int msm_thermal_ioctl_open(struct inode *node, struct file *filep)
{
	int ret = 0;
	struct msm_thermal_ioctl_dev *dev;

	dev = container_of(node->i_cdev, struct msm_thermal_ioctl_dev,
		char_dev);
	filep->private_data = dev;

	return ret;
}

static int msm_thermal_ioctl_release(struct inode *node, struct file *filep)
{
	pr_debug("%s: IOCTL: release\n", KBUILD_MODNAME);
	return 0;
}

static long validate_and_copy(unsigned int *cmd, unsigned long *arg,
	struct msm_thermal_ioctl *query)
{
	long ret = 0, err_val = 0;

	if ((_IOC_TYPE(*cmd) != MSM_THERMAL_MAGIC_NUM) ||
		(_IOC_NR(*cmd) >= MSM_CMD_MAX_NR)) {
		ret = -ENOTTY;
		goto validate_exit;
	}

	if (_IOC_DIR(*cmd) & _IOC_READ) {
		err_val = !access_ok(VERIFY_WRITE, (void __user *)*arg,
				_IOC_SIZE(*cmd));
	} else if (_IOC_DIR(*cmd) & _IOC_WRITE) {
		err_val = !access_ok(VERIFY_READ, (void __user *)*arg,
				_IOC_SIZE(*cmd));
	}
	if (err_val) {
		ret = -EFAULT;
		goto validate_exit;
	}

	if (copy_from_user(query, (void __user *)(*arg),
		sizeof(struct msm_thermal_ioctl))) {
		ret = -EACCES;
		goto validate_exit;
	}

	if (query->size != sizeof(struct msm_thermal_ioctl)) {
		pr_err("%s: Invalid input argument size\n", __func__);
		ret = -EINVAL;
		goto validate_exit;
	}

	switch (*cmd) {
	case MSM_THERMAL_SET_CPU_MAX_FREQUENCY:
	case MSM_THERMAL_SET_CPU_MIN_FREQUENCY:
		if (query->cpu_freq.cpu_num >= num_possible_cpus()) {
			pr_err("%s: Invalid CPU number: %u\n", __func__,
				query->cpu_freq.cpu_num);
			ret = -EINVAL;
			goto validate_exit;
		}
		break;
	default:
		break;
	}

validate_exit:
	return ret;
}

static long msm_thermal_process_freq_table_req(struct msm_thermal_ioctl *query,
		unsigned long *arg)
{
	long ret = 0;
	uint32_t table_idx, idx = 0, cluster_id = query->clock_freq.cluster_num;
	struct clock_plan_arg *clock_freq = &(query->clock_freq);

	if (cluster_id >= num_possible_cpus())
		return -EINVAL;

	if (!freq_table_len[cluster_id]) {
		ret = msm_thermal_get_freq_plan_size(cluster_id,
			&freq_table_len[cluster_id]);
		if (ret) {
			pr_err("%s: Cluster%d freq table length get err:%ld\n",
				KBUILD_MODNAME, cluster_id, ret);
			goto process_freq_exit;
		}
		if (!freq_table_len[cluster_id]) {
			pr_err("%s: Cluster%d freq table empty\n",
				KBUILD_MODNAME, cluster_id);
			ret = -EAGAIN;
			goto process_freq_exit;
		}

		freq_table_set[cluster_id] = freq_table_len[cluster_id]
						/ MSM_IOCTL_FREQ_SIZE;
		if (freq_table_len[cluster_id] % MSM_IOCTL_FREQ_SIZE)
			freq_table_set[cluster_id]++;

		if (!freq_table_ptr[cluster_id]) {
			freq_table_ptr[cluster_id] = kzalloc(
				sizeof(unsigned int) *
				freq_table_len[cluster_id], GFP_KERNEL);
			if (!freq_table_ptr[cluster_id]) {
				pr_err("%s: memory alloc failed\n",
						KBUILD_MODNAME);
				freq_table_len[cluster_id] = 0;
				ret = -ENOMEM;
				goto process_freq_exit;
			}
		}
		ret = msm_thermal_get_cluster_freq_plan(cluster_id,
			freq_table_ptr[cluster_id]);
		if (ret) {
			pr_err("%s: Error getting frequency table. err:%ld\n",
					KBUILD_MODNAME, ret);
			freq_table_len[cluster_id] = 0;
			freq_table_set[cluster_id] = 0;
			kfree(freq_table_ptr[cluster_id]);
			freq_table_ptr[cluster_id] = NULL;
			goto process_freq_exit;
		}
	}

	if (!clock_freq->freq_table_len) {
		clock_freq->freq_table_len = freq_table_len[cluster_id];
		goto copy_and_return;
	}
	if (clock_freq->set_idx >= freq_table_set[cluster_id]) {
		pr_err("%s: Invalid freq table set%d for cluster%d\n",
			KBUILD_MODNAME, clock_freq->set_idx,
			cluster_id);
		ret = -EINVAL;
		goto process_freq_exit;
	}

	table_idx = MSM_IOCTL_FREQ_SIZE * clock_freq->set_idx;
	for (; table_idx < freq_table_len[cluster_id]
		&& idx < MSM_IOCTL_FREQ_SIZE; idx++, table_idx++) {
		clock_freq->freq_table[idx] =
			freq_table_ptr[cluster_id][table_idx];
	}
	clock_freq->freq_table_len = idx;

copy_and_return:
	ret = copy_to_user((void __user *)(*arg), query,
		sizeof(struct msm_thermal_ioctl));
	if (ret) {
		pr_err("%s: copy_to_user error:%ld.\n", KBUILD_MODNAME, ret);
		goto process_freq_exit;
	}

process_freq_exit:
	return ret;
}

static long msm_thermal_process_voltage_table_req(
		struct msm_thermal_ioctl *query,
		unsigned long *arg)
{
	long ret = 0;
	uint32_t table_idx = 0, idx = 0;
	uint32_t cluster_id = query->voltage.cluster_num;
	struct voltage_plan_arg *voltage = &(query->voltage);

	if (cluster_id >= num_possible_cpus())
		return -EINVAL;

	if (!voltage_table_ptr[cluster_id]) {
		if (!freq_table_len[cluster_id]) {
			ret = msm_thermal_get_freq_plan_size(cluster_id,
				&freq_table_len[cluster_id]);
			if (ret) {
				pr_err(
				"%s: Cluster%d freq table len err:%ld\n",
				KBUILD_MODNAME, cluster_id, ret);
				goto process_volt_exit;
			}
			if (!freq_table_len[cluster_id]) {
				pr_err("%s: Cluster%d freq table empty\n",
					KBUILD_MODNAME, cluster_id);
				ret = -EAGAIN;
				goto process_volt_exit;
			}
		}
		voltage_table_ptr[cluster_id] = kzalloc(
			sizeof(uint32_t) *
			freq_table_len[cluster_id], GFP_KERNEL);
		if (!voltage_table_ptr[cluster_id]) {
			pr_err("%s: memory alloc failed\n",
				KBUILD_MODNAME);
			ret = -ENOMEM;
			goto process_volt_exit;
		}
		ret = msm_thermal_get_cluster_voltage_plan(cluster_id,
			voltage_table_ptr[cluster_id]);
		if (ret) {
			pr_err("%s: Error getting voltage table. err:%ld\n",
				KBUILD_MODNAME, ret);
			kfree(voltage_table_ptr[cluster_id]);
			voltage_table_ptr[cluster_id] = NULL;
			goto process_volt_exit;
		}
	}

	if (!voltage->voltage_table_len) {
		voltage->voltage_table_len = freq_table_len[cluster_id];
		goto copy_and_return;
	}

	voltage_table_set[cluster_id] = freq_table_len[cluster_id]
					/ MSM_IOCTL_FREQ_SIZE;
	if (freq_table_len[cluster_id] % MSM_IOCTL_FREQ_SIZE)
		voltage_table_set[cluster_id]++;

	if (voltage->set_idx >= voltage_table_set[cluster_id]) {
		pr_err("%s: Invalid voltage table set%d for cluster%d\n",
			KBUILD_MODNAME, voltage->set_idx,
			cluster_id);
		ret = -EINVAL;
		goto process_volt_exit;
	}

	table_idx = MSM_IOCTL_FREQ_SIZE * voltage->set_idx;
	for (; table_idx < freq_table_len[cluster_id]
		&& idx < MSM_IOCTL_FREQ_SIZE; idx++, table_idx++) {
		voltage->voltage_table[idx] =
			voltage_table_ptr[cluster_id][table_idx];
	}
	voltage->voltage_table_len = idx;

copy_and_return:
	ret = copy_to_user((void __user *)(*arg), query,
		sizeof(struct msm_thermal_ioctl));
	if (ret) {
		pr_err("%s: copy_to_user error:%ld.\n", KBUILD_MODNAME, ret);
		goto process_volt_exit;
	}

process_volt_exit:
	return ret;
}

static long msm_thermal_ioctl_process(struct file *filep, unsigned int cmd,
	unsigned long arg)
{
	long ret = 0;
	struct msm_thermal_ioctl query;

	pr_debug("%s: IOCTL: processing cmd:%u\n", KBUILD_MODNAME, cmd);

	ret = validate_and_copy(&cmd, &arg, &query);
	if (ret)
		goto process_exit;

	switch (cmd) {
	case MSM_THERMAL_SET_CPU_MAX_FREQUENCY:
		ret = msm_thermal_set_frequency(query.cpu_freq.cpu_num,
			query.cpu_freq.freq_req, true);
		break;
	case MSM_THERMAL_SET_CPU_MIN_FREQUENCY:
		ret = msm_thermal_set_frequency(query.cpu_freq.cpu_num,
			query.cpu_freq.freq_req, false);
		break;
	case MSM_THERMAL_SET_CLUSTER_MAX_FREQUENCY:
		ret = msm_thermal_set_cluster_freq(query.cpu_freq.cpu_num,
			query.cpu_freq.freq_req, true);
		break;
	case MSM_THERMAL_SET_CLUSTER_MIN_FREQUENCY:
		ret = msm_thermal_set_cluster_freq(query.cpu_freq.cpu_num,
			query.cpu_freq.freq_req, false);
		break;
	case MSM_THERMAL_GET_CLUSTER_FREQUENCY_PLAN:
		ret = msm_thermal_process_freq_table_req(&query, &arg);
		break;
	case MSM_THERMAL_GET_CLUSTER_VOLTAGE_PLAN:
		ret = msm_thermal_process_voltage_table_req(&query, &arg);
		break;
	default:
		ret = -ENOTTY;
		goto process_exit;
	}
process_exit:
	return ret;
}

#ifdef CONFIG_COMPAT
static long msm_thermal_compat_ioctl_process(struct file *filep,
				   unsigned int cmd, unsigned long arg)
{
	arg = (unsigned long)compat_ptr(arg);
	return msm_thermal_ioctl_process(filep, cmd, arg);
}
#endif	/* CONFIG_COMPAT */

static const struct file_operations msm_thermal_fops = {
	.owner = THIS_MODULE,
	.open = msm_thermal_ioctl_open,
	.unlocked_ioctl = msm_thermal_ioctl_process,
#ifdef CONFIG_COMPAT
	.compat_ioctl = msm_thermal_compat_ioctl_process,
#endif  /* CONFIG_COMPAT */
	.release = msm_thermal_ioctl_release,
};

int msm_thermal_ioctl_init()
{
	int ret = 0;
	dev_t thermal_dev;
	struct device *therm_device;

	ret = alloc_chrdev_region(&thermal_dev, 0, 1,
		MSM_THERMAL_IOCTL_NAME);
	if (ret < 0) {
		pr_err("%s: Error in allocating char device region. Err:%d\n",
			KBUILD_MODNAME, ret);
		goto ioctl_init_exit;
	}

	msm_thermal_major = MAJOR(thermal_dev);

	thermal_class = class_create(THIS_MODULE, "msm_thermal");
	if (IS_ERR(thermal_class)) {
		pr_err("%s: Error in creating class\n",
			KBUILD_MODNAME);
		ret = PTR_ERR(thermal_class);
		goto ioctl_class_fail;
	}

	therm_device = device_create(thermal_class, NULL, thermal_dev, NULL,
				MSM_THERMAL_IOCTL_NAME);
	if (IS_ERR(therm_device)) {
		pr_err("%s: Error in creating character device\n",
			KBUILD_MODNAME);
		ret = PTR_ERR(therm_device);
		goto ioctl_dev_fail;
	}
	msm_thermal_dev = kmalloc(sizeof(struct msm_thermal_ioctl_dev),
				GFP_KERNEL);
	if (!msm_thermal_dev) {
		pr_err("%s: Error allocating memory\n",
			KBUILD_MODNAME);
		ret = -ENOMEM;
		goto ioctl_clean_all;
	}

	memset(msm_thermal_dev, 0, sizeof(struct msm_thermal_ioctl_dev));
	sema_init(&msm_thermal_dev->sem, 1);
	cdev_init(&msm_thermal_dev->char_dev, &msm_thermal_fops);
	ret = cdev_add(&msm_thermal_dev->char_dev, thermal_dev, 1);
	if (ret < 0) {
		pr_err("%s: Error in adding character device\n",
			KBUILD_MODNAME);
		goto ioctl_clean_all;
	}

	return ret;

ioctl_clean_all:
	device_destroy(thermal_class, thermal_dev);
ioctl_dev_fail:
	class_destroy(thermal_class);
ioctl_class_fail:
	unregister_chrdev_region(thermal_dev, 1);
ioctl_init_exit:
	return ret;
}

void msm_thermal_ioctl_cleanup()
{
	uint32_t idx = 0;
	dev_t thermal_dev = MKDEV(msm_thermal_major, 0);

	if (!msm_thermal_dev) {
		pr_err("%s: Thermal IOCTL cleanup already done\n",
			KBUILD_MODNAME);
		return;
	}

	for (; idx < num_possible_cpus(); idx++) {
		kfree(freq_table_ptr[idx]);
		kfree(voltage_table_ptr[idx]);
	}
	device_destroy(thermal_class, thermal_dev);
	class_destroy(thermal_class);
	cdev_del(&msm_thermal_dev->char_dev);
	unregister_chrdev_region(thermal_dev, 1);
	kfree(msm_thermal_dev);
	msm_thermal_dev = NULL;
	thermal_class = NULL;
}
