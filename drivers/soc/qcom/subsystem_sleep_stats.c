// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/ioctl.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/soc/qcom/smem.h>
#include <linux/uaccess.h>

#define STATS_BASEMINOR				0
#define STATS_MAX_MINOR				1
#define STATS_DEVICE_NAME			"stats"
#define SUBSYSTEM_STATS_MAGIC_NUM		(0x9d)

#define APSS_IOCTL		_IOR(SUBSYSTEM_STATS_MAGIC_NUM, 0, \
				     struct subsystem_stats *)
#define MODEM_IOCTL		_IOR(SUBSYSTEM_STATS_MAGIC_NUM, 1, \
				     struct subsystem_stats *)
#define WPSS_IOCTL		_IOR(SUBSYSTEM_STATS_MAGIC_NUM, 2, \
				     struct subsystem_stats *)
#define ADSP_IOCTL		_IOR(SUBSYSTEM_STATS_MAGIC_NUM, 3, \
				     struct subsystem_stats *)
#define ADSP_ISLAND_IOCTL	_IOR(SUBSYSTEM_STATS_MAGIC_NUM, 4, \
				     struct subsystem_stats *)
#define CDSP_IOCTL		_IOR(SUBSYSTEM_STATS_MAGIC_NUM, 5, \
				     struct subsystem_stats *)
#define SLPI_IOCTL		_IOR(SUBSYSTEM_STATS_MAGIC_NUM, 6, \
				     struct subsystem_stats *)
#define GPU_IOCTL		_IOR(SUBSYSTEM_STATS_MAGIC_NUM, 7, \
				     struct subsystem_stats *)
#define DISPLAY_IOCTL		_IOR(SUBSYSTEM_STATS_MAGIC_NUM, 8, \
				     struct subsystem_stats *)
#define SLPI_ISLAND_IOCTL	_IOR(SUBSYSTEM_STATS_MAGIC_NUM, 9, \
				     struct subsystem_stats *)

struct subsystem_stats {
	u32 version;
	u32 count;
	u64 last_entered_at;
	u64 last_exited_at;
	u64 accumulated;
};

enum subsystem_smem_id {
	MPSS = 605,
	ADSP,
	CDSP,
	SLPI,
	GPU,
	DISPLAY,
	SLPI_ISLAND = 613,
	APSS = 631,
};

enum subsystem_pid {
	PID_APSS = 0,
	PID_MPSS = 1,
	PID_ADSP = 2,
	PID_SLPI = 3,
	PID_CDSP = 5,
	PID_WPSS = 13,
	PID_GPU = PID_APSS,
	PID_DISPLAY = PID_APSS,
};

struct sleep_stats_data {
	dev_t		dev_no;
	struct class	*stats_class;
	struct device	*stats_device;
	struct cdev	stats_cdev;
};

static DEFINE_MUTEX(sleep_stats_mutex);

static long stats_data_ioctl(struct file *file, unsigned int cmd,
			     unsigned long arg)
{
	struct subsystem_stats *temp, *subsystem_stats_data;
	int ret = -ENOMEM;
	unsigned int pid, smem_item;

	mutex_lock(&sleep_stats_mutex);

	temp = kzalloc(sizeof(struct subsystem_stats), GFP_KERNEL);
	if (!temp)
		goto out_unlock;

	switch (cmd) {
	case APSS_IOCTL:
		pid = QCOM_SMEM_HOST_ANY;
		smem_item = APSS;
		break;
	case MODEM_IOCTL:
		pid = PID_MPSS;
		smem_item = MPSS;
		break;
	case WPSS_IOCTL:
		pid = PID_MPSS;
		smem_item = MPSS;
		break;
	case ADSP_IOCTL:
		pid = PID_ADSP;
		smem_item = ADSP;
		break;
	case ADSP_ISLAND_IOCTL:
		pid = PID_ADSP;
		smem_item = SLPI_ISLAND;
		break;
	case CDSP_IOCTL:
		pid = PID_CDSP;
		smem_item = CDSP;
		break;
	case SLPI_IOCTL:
		pid = PID_SLPI;
		smem_item = SLPI;
		break;
	case GPU_IOCTL:
		pid = PID_GPU;
		smem_item = GPU;
		break;
	case DISPLAY_IOCTL:
		pid = PID_DISPLAY;
		smem_item = DISPLAY;
		break;
	case SLPI_ISLAND_IOCTL:
		pid = PID_SLPI;
		smem_item = SLPI_ISLAND;
		break;
	default:
		pr_err("Incorrect command error\n");
		ret = -EINVAL;
		goto out_free;
	}

	subsystem_stats_data = qcom_smem_get(pid, smem_item, NULL);
	if (IS_ERR(subsystem_stats_data)) {
		ret =  -ENODEV;
		goto out_free;
	}

	temp->version = subsystem_stats_data->version;
	temp->count = subsystem_stats_data->count;
	temp->last_entered_at = subsystem_stats_data->last_entered_at;
	temp->last_exited_at = subsystem_stats_data->last_exited_at;
	temp->accumulated = subsystem_stats_data->accumulated;

	/*
	 * If a subsystem is in sleep when reading the sleep stats from SMEM
	 * adjust the accumulated sleep duration to show actual sleep time.
	 * This ensures that the displayed stats are real when used for
	 * the purpose of computing battery utilization.
	 */
	if (temp->last_entered_at > temp->last_exited_at) {
		temp->accumulated +=
				(__arch_counter_get_cntvct()
				- temp->last_entered_at);
	}

	ret = copy_to_user((void __user *)arg, temp, sizeof(struct subsystem_stats));

	kfree(temp);
	mutex_unlock(&sleep_stats_mutex);

	return ret;
out_free:
	kfree(temp);
out_unlock:
	mutex_unlock(&sleep_stats_mutex);
	return ret;

}

static const struct file_operations stats_data_fops = {
	.owner		=	THIS_MODULE,
	.open		=	simple_open,
	.unlocked_ioctl =	stats_data_ioctl,
};

static int subsystem_stats_probe(struct platform_device *pdev)
{
	struct sleep_stats_data *stats_data;
	int ret = -ENOMEM;

	stats_data = devm_kzalloc(&pdev->dev, sizeof(struct sleep_stats_data), GFP_KERNEL);
	if (!stats_data)
		return ret;

	ret = alloc_chrdev_region(&stats_data->dev_no, STATS_BASEMINOR,
				  STATS_MAX_MINOR, STATS_DEVICE_NAME);
	if (ret)
		goto fail_alloc_chrdev;

	cdev_init(&stats_data->stats_cdev, &stats_data_fops);
	ret = cdev_add(&stats_data->stats_cdev, stats_data->dev_no, 1);
	if (ret)
		goto fail_cdev_add;

	stats_data->stats_class = class_create(THIS_MODULE, STATS_DEVICE_NAME);
	if (IS_ERR_OR_NULL(stats_data->stats_class)) {
		ret =  -EINVAL;
		goto fail_class_create;
	}

	stats_data->stats_device = device_create(stats_data->stats_class, NULL,
						 stats_data->dev_no, NULL,
						 STATS_DEVICE_NAME);
	if (IS_ERR_OR_NULL(stats_data->stats_device)) {
		ret = -EINVAL;
		goto fail_device_create;
	}

	platform_set_drvdata(pdev, stats_data);

	return 0;

fail_device_create:
	class_destroy(stats_data->stats_class);
fail_class_create:
	cdev_del(&stats_data->stats_cdev);
fail_cdev_add:
	unregister_chrdev_region(stats_data->dev_no, 1);
fail_alloc_chrdev:
	return ret;
}

static int subsystem_stats_remove(struct platform_device *pdev)
{
	struct sleep_stats_data *stats_data;

	stats_data = platform_get_drvdata(pdev);
	if (!stats_data)
		return 0;

	class_destroy(stats_data->stats_class);
	cdev_del(&stats_data->stats_cdev);
	unregister_chrdev_region(stats_data->dev_no, 1);

	return 0;
}

static const struct of_device_id subsystem_stats_table[] = {
	{.compatible = "qcom,subsystem-sleep-stats"},
	{},
};

static struct platform_driver subsystem_sleep_stats_driver = {
	.probe	= subsystem_stats_probe,
	.remove	= subsystem_stats_remove,
	.driver	= {
		.name	= "subsystem_sleep_stats",
		.of_match_table	= subsystem_stats_table,
	},
};

module_platform_driver(subsystem_sleep_stats_driver);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Qualcomm Technologies, Inc. subsystem sleep stats driver");
