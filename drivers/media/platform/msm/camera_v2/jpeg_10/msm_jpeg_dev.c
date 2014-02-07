/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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



#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <media/msm_jpeg.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>

#include "msm_jpeg_sync.h"
#include "msm_jpeg_common.h"

#define MSM_JPEG_NAME "jpeg"
#define DEV_NAME_LEN 10

static int msm_jpeg_open(struct inode *inode, struct file *filp)
{
	int rc = 0;

	struct msm_jpeg_device *pgmn_dev = container_of(inode->i_cdev,
		struct msm_jpeg_device, cdev);
	filp->private_data = pgmn_dev;

	JPEG_DBG("%s:%d]\n", __func__, __LINE__);

	rc = __msm_jpeg_open(pgmn_dev);

	JPEG_DBG(KERN_INFO "%s:%d] %s open_count = %d\n", __func__, __LINE__,
		filp->f_path.dentry->d_name.name, pgmn_dev->open_count);

	return rc;
}

static int msm_jpeg_release(struct inode *inode, struct file *filp)
{
	int rc;

	struct msm_jpeg_device *pgmn_dev = filp->private_data;

	JPEG_DBG(KERN_INFO "%s:%d]\n", __func__, __LINE__);

	rc = __msm_jpeg_release(pgmn_dev);

	JPEG_DBG(KERN_INFO "%s:%d] %s open_count = %d\n", __func__, __LINE__,
		filp->f_path.dentry->d_name.name, pgmn_dev->open_count);
	return rc;
}
#ifdef CONFIG_COMPAT
static long msm_jpeg_compat_ioctl(struct file *filp, unsigned int cmd,
	unsigned long arg)
{
	int rc;
	struct msm_jpeg_device *pgmn_dev = filp->private_data;

	JPEG_DBG("%s:%d] cmd=%d pgmn_dev=0x%x arg=0x%x\n", __func__,
		__LINE__, _IOC_NR(cmd), (uint32_t)pgmn_dev, (uint32_t)arg);

	rc = __msm_jpeg_compat_ioctl(pgmn_dev, cmd, arg);

	JPEG_DBG("%s:%d]\n", __func__, __LINE__);
	return rc;
}
#endif
static long msm_jpeg_ioctl(struct file *filp, unsigned int cmd,
	unsigned long arg)
{
	int rc;
	struct msm_jpeg_device *pgmn_dev = filp->private_data;

	JPEG_DBG("%s:%d] cmd=%d pgmn_dev=0x%x arg=0x%x\n", __func__,
		__LINE__, _IOC_NR(cmd), (uint32_t)pgmn_dev, (uint32_t)arg);

	rc = __msm_jpeg_ioctl(pgmn_dev, cmd, arg);

	JPEG_DBG("%s:%d]\n", __func__, __LINE__);
	return rc;
}

static const struct file_operations msm_jpeg_fops = {
	.owner		= THIS_MODULE,
	.open		 = msm_jpeg_open,
	.release	= msm_jpeg_release,
	.unlocked_ioctl = msm_jpeg_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = msm_jpeg_compat_ioctl,
#endif
};


int msm_jpeg_subdev_init(struct v4l2_subdev *jpeg_sd)
{
	int rc;
	struct msm_jpeg_device *pgmn_dev =
		(struct msm_jpeg_device *)jpeg_sd->host_priv;

	JPEG_DBG("%s:%d: jpeg_sd=0x%x pgmn_dev=0x%x\n",
		__func__, __LINE__, (uint32_t)jpeg_sd, (uint32_t)pgmn_dev);
	rc = __msm_jpeg_open(pgmn_dev);
	JPEG_DBG("%s:%d: rc=%d\n",
		__func__, __LINE__, rc);
	return rc;
}

static long msm_jpeg_subdev_ioctl(struct v4l2_subdev *sd,
	unsigned int cmd, void *arg)
{
	long rc;
	struct msm_jpeg_device *pgmn_dev =
		(struct msm_jpeg_device *)sd->host_priv;

	JPEG_DBG("%s: cmd=%d\n", __func__, cmd);

	JPEG_DBG("%s: pgmn_dev 0x%x", __func__, (uint32_t)pgmn_dev);

	JPEG_DBG("%s: Calling __msm_jpeg_ioctl\n", __func__);

	rc = __msm_jpeg_ioctl(pgmn_dev, cmd, (unsigned long)arg);
	pr_debug("%s: X\n", __func__);
	return rc;
}

void msm_jpeg_subdev_release(struct v4l2_subdev *jpeg_sd)
{
	int rc;
	struct msm_jpeg_device *pgmn_dev =
		(struct msm_jpeg_device *)jpeg_sd->host_priv;
	JPEG_DBG("%s:pgmn_dev=0x%x", __func__, (uint32_t)pgmn_dev);
	rc = __msm_jpeg_release(pgmn_dev);
	JPEG_DBG("%s:rc=%d", __func__, rc);
}

static const struct v4l2_subdev_core_ops msm_jpeg_subdev_core_ops = {
	.ioctl = msm_jpeg_subdev_ioctl,
};

static const struct v4l2_subdev_ops msm_jpeg_subdev_ops = {
	.core = &msm_jpeg_subdev_core_ops,
};

static int msm_jpeg_init_dev(struct platform_device *pdev)
{
	int rc = -1;
	struct device *dev;
	struct msm_jpeg_device *msm_jpeg_device_p;
	char devname[DEV_NAME_LEN];

	msm_jpeg_device_p = kzalloc(sizeof(struct msm_jpeg_device), GFP_ATOMIC);
	if (!msm_jpeg_device_p) {
		JPEG_PR_ERR("%s: no mem\n", __func__);
		return -EFAULT;
	}

	msm_jpeg_device_p->pdev = pdev;

	if (pdev->dev.of_node)
		of_property_read_u32((&pdev->dev)->of_node, "cell-index",
			&pdev->id);

	snprintf(devname, sizeof(devname), "%s%d", MSM_JPEG_NAME, pdev->id);

	rc = __msm_jpeg_init(msm_jpeg_device_p);
	if (rc < -1) {
		JPEG_PR_ERR("%s: initialization failed\n", __func__);
		goto fail;
	}

	v4l2_subdev_init(&msm_jpeg_device_p->subdev, &msm_jpeg_subdev_ops);
	v4l2_set_subdev_hostdata(&msm_jpeg_device_p->subdev, msm_jpeg_device_p);
	JPEG_DBG("%s: msm_jpeg_device_p 0x%x", __func__,
			(uint32_t)msm_jpeg_device_p);

	rc = alloc_chrdev_region(&msm_jpeg_device_p->msm_jpeg_devno, 0, 1,
				devname);
	if (rc < 0) {
		JPEG_PR_ERR("%s: failed to allocate chrdev\n", __func__);
		goto fail_1;
	}

	if (!msm_jpeg_device_p->msm_jpeg_class) {
		msm_jpeg_device_p->msm_jpeg_class =
				class_create(THIS_MODULE, devname);
		if (IS_ERR(msm_jpeg_device_p->msm_jpeg_class)) {
			rc = PTR_ERR(msm_jpeg_device_p->msm_jpeg_class);
			JPEG_PR_ERR("%s: create device class failed\n",
				__func__);
			goto fail_2;
		}
	}

	dev = device_create(msm_jpeg_device_p->msm_jpeg_class, NULL,
		MKDEV(MAJOR(msm_jpeg_device_p->msm_jpeg_devno),
		MINOR(msm_jpeg_device_p->msm_jpeg_devno)), NULL,
		"%s%d", MSM_JPEG_NAME, pdev->id);
	if (IS_ERR(dev)) {
		JPEG_PR_ERR("%s: error creating device\n", __func__);
		rc = -ENODEV;
		goto fail_3;
	}

	cdev_init(&msm_jpeg_device_p->cdev, &msm_jpeg_fops);
	msm_jpeg_device_p->cdev.owner = THIS_MODULE;
	msm_jpeg_device_p->cdev.ops	 =
		(const struct file_operations *) &msm_jpeg_fops;
	rc = cdev_add(&msm_jpeg_device_p->cdev,
			msm_jpeg_device_p->msm_jpeg_devno, 1);
	if (rc < 0) {
		JPEG_PR_ERR("%s: error adding cdev\n", __func__);
		rc = -ENODEV;
		goto fail_4;
	}

	platform_set_drvdata(pdev, &msm_jpeg_device_p);

	JPEG_DBG("%s %s%d: success\n", __func__, MSM_JPEG_NAME, pdev->id);

	return rc;

fail_4:
	device_destroy(msm_jpeg_device_p->msm_jpeg_class,
			msm_jpeg_device_p->msm_jpeg_devno);

fail_3:
	class_destroy(msm_jpeg_device_p->msm_jpeg_class);

fail_2:
	unregister_chrdev_region(msm_jpeg_device_p->msm_jpeg_devno, 1);

fail_1:
	__msm_jpeg_exit(msm_jpeg_device_p);
	return rc;

fail:
	kfree(msm_jpeg_device_p);
	return rc;

}

static void msm_jpeg_exit(struct msm_jpeg_device *msm_jpeg_device_p)
{
	cdev_del(&msm_jpeg_device_p->cdev);
	device_destroy(msm_jpeg_device_p->msm_jpeg_class,
			msm_jpeg_device_p->msm_jpeg_devno);
	class_destroy(msm_jpeg_device_p->msm_jpeg_class);
	unregister_chrdev_region(msm_jpeg_device_p->msm_jpeg_devno, 1);

	__msm_jpeg_exit(msm_jpeg_device_p);
}

static int __msm_jpeg_probe(struct platform_device *pdev)
{
	return msm_jpeg_init_dev(pdev);
}

static int __msm_jpeg_remove(struct platform_device *pdev)
{
	struct msm_jpeg_device *msm_jpegd_device_p;

	msm_jpegd_device_p = platform_get_drvdata(pdev);
	if (msm_jpegd_device_p)
		msm_jpeg_exit(msm_jpegd_device_p);

	return 0;
}

static const struct of_device_id msm_jpeg_dt_match[] = {
			{.compatible = "qcom,jpeg"},
			{},
};

MODULE_DEVICE_TABLE(of, msm_jpeg_dt_match);

static struct platform_driver msm_jpeg_driver = {
	.probe	= __msm_jpeg_probe,
	.remove = __msm_jpeg_remove,
	.driver = {
		.name = "msm_jpeg",
		.owner = THIS_MODULE,
		.of_match_table = msm_jpeg_dt_match,
	},
};

static int __init msm_jpeg_driver_init(void)
{
	int rc;
	rc = platform_driver_register(&msm_jpeg_driver);
	return rc;
}

static void __exit msm_jpeg_driver_exit(void)
{
	platform_driver_unregister(&msm_jpeg_driver);
}

MODULE_DESCRIPTION("msm jpeg jpeg driver");

module_init(msm_jpeg_driver_init);
module_exit(msm_jpeg_driver_exit);
