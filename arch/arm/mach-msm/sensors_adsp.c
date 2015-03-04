/* Copyright (c) 2012-2013, 2015 The Linux Foundation. All rights reserved.
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

#include <linux/msm_dsps.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/of_device.h>
#include <asm/arch_timer.h>
#include <linux/uaccess.h>

#define CLASS_NAME	"ssc"
#define DRV_NAME	"sensors"
#define DRV_VERSION	"2.00"

struct sns_adsp_control_s {
	struct class *dev_class;
	dev_t dev_num;
	struct device *dev;
	struct cdev *cdev;
};
static struct sns_adsp_control_s sns_ctl;

/*
 * Read QTimer clock ticks and scale down to 32KHz clock as used
 * in DSPS
 */
static u32 sns_read_qtimer(void)
{
	u64 val;
	val = arch_counter_get_cntpct();
	/*
	 * To convert ticks from 19.2 Mhz clock to 32768 Hz clock:
	 * x = (value * 32768) / 19200000
	 * This is same as first left shift the value by 4 bits, i.e. mutiply
	 * by 16, and then divide by 9375. The latter is preferable since
	 * QTimer tick (value) is 56-bit, so (value * 32768) could overflow,
	 * while (value * 16) will never do
	 */
	val <<= 4;
	do_div(val, 9375);

	return (u32)val;
}

static int sensors_adsp_open(struct inode *ip, struct file *fp)
{
	return 0;
}

static int sensors_adsp_release(struct inode *inode, struct file *file)
{
	return 0;
}

static long sensors_adsp_ioctl(struct file *file,
			unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	u32 val = 0;

	switch (cmd) {
	case DSPS_IOCTL_READ_SLOW_TIMER:
		val = sns_read_qtimer();
		ret = put_user(val, (u32 __user *) arg);
		break;

	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

const struct file_operations sensors_adsp_fops = {
	.owner = THIS_MODULE,
	.open = sensors_adsp_open,
	.release = sensors_adsp_release,
	.unlocked_ioctl = sensors_adsp_ioctl
};

static int sensors_adsp_probe(struct platform_device *pdev)
{
	int ret = 0;

	sns_ctl.dev_class = class_create(THIS_MODULE, CLASS_NAME);
	if (sns_ctl.dev_class == NULL) {
		pr_err("%s: class_create fail.\n", __func__);
		goto res_err;
	}

	ret = alloc_chrdev_region(&sns_ctl.dev_num, 0, 1, DRV_NAME);
	if (ret) {
		pr_err("%s: alloc_chrdev_region fail.\n", __func__);
		goto alloc_chrdev_region_err;
	}

	sns_ctl.dev = device_create(sns_ctl.dev_class, NULL,
				     sns_ctl.dev_num,
				     &sns_ctl, DRV_NAME);
	if (IS_ERR(sns_ctl.dev)) {
		pr_err("%s: device_create fail.\n", __func__);
		goto device_create_err;
	}

	sns_ctl.cdev = cdev_alloc();
	if (sns_ctl.cdev == NULL) {
		pr_err("%s: cdev_alloc fail.\n", __func__);
		goto cdev_alloc_err;
	}
	cdev_init(sns_ctl.cdev, &sensors_adsp_fops);
	sns_ctl.cdev->owner = THIS_MODULE;

	ret = cdev_add(sns_ctl.cdev, sns_ctl.dev_num, 1);
	if (ret) {
		pr_err("%s: cdev_add fail.\n", __func__);
		goto cdev_add_err;
	}

	return 0;

cdev_add_err:
	kfree(sns_ctl.cdev);
cdev_alloc_err:
	device_destroy(sns_ctl.dev_class, sns_ctl.dev_num);
device_create_err:
	unregister_chrdev_region(sns_ctl.dev_num, 1);
alloc_chrdev_region_err:
	class_destroy(sns_ctl.dev_class);
res_err:
	return -ENODEV;
}

static int sensors_adsp_remove(struct platform_device *pdev)
{
	cdev_del(sns_ctl.cdev);
	kfree(sns_ctl.cdev);
	sns_ctl.cdev = NULL;
	device_destroy(sns_ctl.dev_class, sns_ctl.dev_num);
	unregister_chrdev_region(sns_ctl.dev_num, 1);
	class_destroy(sns_ctl.dev_class);

	return 0;
}

static const struct of_device_id msm_adsp_sensors_dt_match[] = {
	{.compatible = "qcom,msm-adsp-sensors"}
};
MODULE_DEVICE_TABLE(of, msm_adsp_sensors_dt_match);

static struct platform_driver sensors_adsp_driver = {
	.driver = {
		.name = "sensors-adsp",
		.owner = THIS_MODULE,
		.of_match_table = msm_adsp_sensors_dt_match,
	},
	.probe = sensors_adsp_probe,
	.remove = sensors_adsp_remove,
};

static int __init sensors_adsp_init(void)
{
	int rc;

	pr_debug("%s driver version %s.\n", DRV_NAME, DRV_VERSION);
	rc = platform_driver_register(&sensors_adsp_driver);
	if (rc) {
		pr_err("%s: Failed to register sensors adsp driver\n",
			__func__);
		return rc;
	}

	return 0;
}

static void __exit sensors_adsp_exit(void)
{
	platform_driver_unregister(&sensors_adsp_driver);
}

module_init(sensors_adsp_init);
module_exit(sensors_adsp_exit);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Sensors ADSP driver");
