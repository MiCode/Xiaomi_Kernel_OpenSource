/* Copyright (c) 2012-2017, The Linux Foundation. All rights reserved.
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

#include <linux/types.h>
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

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/sysfs.h>
#include <linux/workqueue.h>
#include <linux/io.h>

#include <soc/qcom/subsystem_restart.h>

#define IMAGE_LOAD_CMD 1
#define IMAGE_UNLOAD_CMD 0
#define CLASS_NAME	"ssc"
#define DRV_NAME	"sensors"
#define DRV_VERSION	"2.00"
#ifdef CONFIG_COMPAT
#define DSPS_IOCTL_READ_SLOW_TIMER32 _IOR(DSPS_IOCTL_MAGIC, 3, compat_uint_t)
#endif

static void __iomem *qdsp6ss_qtmr_base;
static uint32_t qdsp6ss_qtmr_hi_offset;
static uint32_t qdsp6ss_qtmr_lo_offset;

struct msm_ssc_sensors_data {
	uint32_t qtmr_base;
	uint32_t qtmr_length;
	uint32_t qtmr_hi_offset;
	uint32_t qtmr_lo_offset;
};

static inline uint64_t qdsp6_get_counter_value(void)
{
	uint32_t cvall = 0;
	uint32_t cvalh = 0;
	uint32_t thigh = 0;

	if (qdsp6ss_qtmr_base != NULL) {
		do {
			cvalh = __raw_readl(qdsp6ss_qtmr_base +
			    qdsp6ss_qtmr_hi_offset);
			cvall = __raw_readl(qdsp6ss_qtmr_base +
			    qdsp6ss_qtmr_lo_offset);
			thigh = __raw_readl(qdsp6ss_qtmr_base +
			    qdsp6ss_qtmr_hi_offset);
		} while (cvalh != thigh);
	} else {
		pr_err("qdsp6ss_qtmr_base is NULL\n");
	}

	return ((uint64_t) cvalh << 32) | cvall;
}

static ssize_t qdsp_qtimer_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	uint64_t qdsp_qtimer_value = 0;

	qdsp_qtimer_value = qdsp6_get_counter_value();
	return snprintf(buf, 20, "%llx\n", qdsp_qtimer_value);
}

static DEVICE_ATTR(qdsp_qtimer, S_IRUGO , qdsp_qtimer_show, NULL);

struct sns_ssc_control_s {
	struct class *dev_class;
	dev_t dev_num;
	struct device *dev;
	struct cdev *cdev;
};
static struct sns_ssc_control_s sns_ctl;

static ssize_t slpi_boot_store(struct kobject *kobj,
	struct kobj_attribute *attr,
	const char *buf, size_t count);

struct slpi_loader_private {
	void *pil_h;
	struct kobject *boot_slpi_obj;
	struct attribute_group *attr_group;
};

static struct kobj_attribute slpi_boot_attribute =
	__ATTR(boot, 0220, NULL, slpi_boot_store);

static struct attribute *attrs[] = {
	&slpi_boot_attribute.attr,
	&dev_attr_qdsp_qtimer.attr,
	NULL,
};

static struct platform_device *slpi_private;
static struct work_struct slpi_ldr_work;

static void slpi_load_fw(struct work_struct *slpi_ldr_work)
{
	struct platform_device *pdev = slpi_private;
	struct slpi_loader_private *priv = NULL;

	if (!pdev) {
		dev_err(&pdev->dev, "%s: Platform device null\n", __func__);
		goto fail;
	}

	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev,
			"%s: Device tree information missing\n", __func__);
		goto fail;
	}

	priv = platform_get_drvdata(pdev);
	if (!priv) {
		dev_err(&pdev->dev,
		" %s: Private data get failed\n", __func__);
		goto fail;
	}

	priv->pil_h = subsystem_get("slpi");
	if (IS_ERR(priv->pil_h)) {
		dev_err(&pdev->dev, "%s: pil get failed,\n",
			__func__);
		goto fail;
	}

	dev_err(&pdev->dev, "%s: SLPI image is loaded\n", __func__);
	return;

fail:
	dev_err(&pdev->dev, "%s: SLPI image loading failed\n", __func__);
}

static void slpi_loader_do(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "%s: scheduling work to load SLPI fw\n", __func__);
	schedule_work(&slpi_ldr_work);
}

static void slpi_loader_unload(struct platform_device *pdev)
{
	struct slpi_loader_private *priv = NULL;

	priv = platform_get_drvdata(pdev);

	if (!priv)
		return;

	if (priv->pil_h) {
		dev_dbg(&pdev->dev, "%s: calling subsystem put\n", __func__);
		subsystem_put(priv->pil_h);
		priv->pil_h = NULL;
	}
}

static ssize_t slpi_boot_store(struct kobject *kobj,
	struct kobj_attribute *attr,
	const char *buf,
	size_t count)
{
	int boot = 0;

	if (sscanf(buf, "%du", &boot) != 1)
		return -EINVAL;

	if (boot == IMAGE_LOAD_CMD) {
		pr_debug("%s: going to call slpi_loader_do\n", __func__);
		slpi_loader_do(slpi_private);
	} else if (boot == IMAGE_UNLOAD_CMD) {
		pr_debug("%s: going to call slpi_unloader\n", __func__);
		slpi_loader_unload(slpi_private);
	}
	return count;
}

static int msm_ssc_sensors_dt_parse(struct platform_device *pdev,
		struct msm_ssc_sensors_data *ssc_sensors_data)
{
	int ret = -EINVAL;
	uint32_t qtimer_prop[2];

	ret = of_property_read_u32(pdev->dev.of_node,
		    "qcom,qtimer-cntpct-hi-offset",
		    &ssc_sensors_data->qtmr_hi_offset);
	if (ret) {
		dev_err(&pdev->dev, "%s: get qdsp timer cntpct hi offset fail\n",
			__func__);
		return ret;
	}

	ret = of_property_read_u32(pdev->dev.of_node,
		    "qcom,qtimer-cntpct-lo-offset",
		    &ssc_sensors_data->qtmr_lo_offset);
	if (ret) {
		dev_err(&pdev->dev, "%s: get qdsp timer cntpct lo offset fail\n",
			__func__);
		return ret;
	}

	ret = of_property_read_u32_array(pdev->dev.of_node,
			"qcom,qdsp-timer-base", &qtimer_prop[0], 2);
	if (!ret) {
		ssc_sensors_data->qtmr_base = qtimer_prop[0];
		ssc_sensors_data->qtmr_length = qtimer_prop[1];
	} else {
		dev_err(&pdev->dev, "%s: get qdsp timer base fail\n",
			__func__);
	}

	return ret;
}

static int slpi_loader_init_sysfs(struct platform_device *pdev)
{
	int ret = -EINVAL;
	struct slpi_loader_private *priv = NULL;
	struct msm_ssc_sensors_data ssc_sensors_data;

	slpi_private = NULL;

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		ret = -ENOMEM;
		return ret;
	}

	platform_set_drvdata(pdev, priv);

	priv->pil_h = NULL;
	priv->boot_slpi_obj = NULL;
	priv->attr_group = devm_kzalloc(&pdev->dev,
				sizeof(*(priv->attr_group)),
				GFP_KERNEL);
	if (!priv->attr_group) {
		dev_err(&pdev->dev, "%s: malloc attr_group failed\n",
						__func__);
		ret = -ENOMEM;
		goto error_return;
	}

	priv->attr_group->attrs = attrs;

	priv->boot_slpi_obj = kobject_create_and_add("boot_slpi", kernel_kobj);
	if (!priv->boot_slpi_obj) {
		dev_err(&pdev->dev, "%s: sysfs create and add failed\n",
						__func__);
		ret = -ENOMEM;
		goto error_return;
	}

	ret = msm_ssc_sensors_dt_parse(pdev, &ssc_sensors_data);
	if (!ret) {
		qdsp6ss_qtmr_hi_offset = ssc_sensors_data.qtmr_hi_offset;
		qdsp6ss_qtmr_lo_offset = ssc_sensors_data.qtmr_lo_offset;
		qdsp6ss_qtmr_base = ioremap(ssc_sensors_data.qtmr_base,
			ssc_sensors_data.qtmr_length);
		if (qdsp6ss_qtmr_base == NULL) {
			dev_err(&pdev->dev, "%s: qdsp timer ioremap fail\n",
				__func__);
		}
	} else {
		dev_info(&pdev->dev, "%s: Could not parse dt\n",
			__func__);
	}

	ret = sysfs_create_group(priv->boot_slpi_obj, priv->attr_group);
	if (ret) {
		dev_err(&pdev->dev, "%s: sysfs create group failed %d\n",
							__func__, ret);
		goto error_return;
	}

	slpi_private = pdev;

	return 0;

error_return:

	if (priv->boot_slpi_obj) {
		kobject_del(priv->boot_slpi_obj);
		priv->boot_slpi_obj = NULL;
	}

	return ret;
}

static int slpi_loader_remove(struct platform_device *pdev)
{
	struct slpi_loader_private *priv = NULL;

	priv = platform_get_drvdata(pdev);

	if (!priv)
		return 0;

	if (priv->pil_h) {
		subsystem_put(priv->pil_h);
		priv->pil_h = NULL;
	}

	if (priv->boot_slpi_obj) {
		sysfs_remove_group(priv->boot_slpi_obj, priv->attr_group);
		kobject_del(priv->boot_slpi_obj);
		priv->boot_slpi_obj = NULL;
	}

	return 0;
}

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

static int sensors_ssc_open(struct inode *ip, struct file *fp)
{
	return 0;
}

static int sensors_ssc_release(struct inode *inode, struct file *file)
{
	return 0;
}

static long sensors_ssc_ioctl(struct file *file,
			unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	u32 val = 0;

	switch (cmd) {
	case DSPS_IOCTL_READ_SLOW_TIMER:
#ifdef CONFIG_COMPAT
	case DSPS_IOCTL_READ_SLOW_TIMER32:
#endif
		val = sns_read_qtimer();
		ret = put_user(val, (u32 __user *) arg);
		break;

	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

const struct file_operations sensors_ssc_fops = {
	.owner = THIS_MODULE,
	.open = sensors_ssc_open,
	.release = sensors_ssc_release,
#ifdef CONFIG_COMPAT
	.compat_ioctl = sensors_ssc_ioctl,
#endif
	.unlocked_ioctl = sensors_ssc_ioctl
};

static int sensors_ssc_probe(struct platform_device *pdev)
{
	int ret = slpi_loader_init_sysfs(pdev);

	if (ret != 0) {
		dev_err(&pdev->dev, "%s: Error in initing sysfs\n", __func__);
		return ret;
	}

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
	cdev_init(sns_ctl.cdev, &sensors_ssc_fops);
	sns_ctl.cdev->owner = THIS_MODULE;

	ret = cdev_add(sns_ctl.cdev, sns_ctl.dev_num, 1);
	if (ret) {
		pr_err("%s: cdev_add fail.\n", __func__);
		goto cdev_add_err;
	}

	INIT_WORK(&slpi_ldr_work, slpi_load_fw);

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

static int sensors_ssc_remove(struct platform_device *pdev)
{
	slpi_loader_remove(pdev);
	cdev_del(sns_ctl.cdev);
	kfree(sns_ctl.cdev);
	sns_ctl.cdev = NULL;
	device_destroy(sns_ctl.dev_class, sns_ctl.dev_num);
	unregister_chrdev_region(sns_ctl.dev_num, 1);
	class_destroy(sns_ctl.dev_class);

	return 0;
}

static const struct of_device_id msm_ssc_sensors_dt_match[] = {
	{.compatible = "qcom,msm-ssc-sensors"},
	{},
};
MODULE_DEVICE_TABLE(of, msm_ssc_sensors_dt_match);

static struct platform_driver sensors_ssc_driver = {
	.driver = {
		.name = "sensors-ssc",
		.owner = THIS_MODULE,
		.of_match_table = msm_ssc_sensors_dt_match,
	},
	.probe = sensors_ssc_probe,
	.remove = sensors_ssc_remove,
};

static int __init sensors_ssc_init(void)
{
	int rc;

	pr_debug("%s driver version %s.\n", DRV_NAME, DRV_VERSION);
	rc = platform_driver_register(&sensors_ssc_driver);
	if (rc) {
		pr_err("%s: Failed to register sensors ssc driver\n",
			__func__);
		return rc;
	}

	return 0;
}

static void __exit sensors_ssc_exit(void)
{
	platform_driver_unregister(&sensors_ssc_driver);
}

module_init(sensors_ssc_init);
module_exit(sensors_ssc_exit);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Sensors SSC driver");
