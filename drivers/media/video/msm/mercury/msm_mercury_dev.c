/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <media/msm_mercury.h>
#include <mach/board.h>
#include "msm_mercury_sync.h"
#include "msm_mercury_common.h"
#include "msm.h"

#define MSM_MERCURY_NAME "mercury"

static int msm_mercury_open(struct inode *inode, struct file *filp)
{
	int rc;

	struct msm_mercury_device *pmercury_dev = container_of(inode->i_cdev,
		struct msm_mercury_device, cdev);
	filp->private_data = pmercury_dev;

	MCR_DBG("\n---(%d)%s()\n", __LINE__, __func__);

	rc = __msm_mercury_open(pmercury_dev);

	MCR_DBG("%s:%d] %s open_count = %d\n", __func__, __LINE__,
		filp->f_path.dentry->d_name.name, pmercury_dev->open_count);

	return rc;
}

static int msm_mercury_release(struct inode *inode, struct file *filp)
{
	int rc;

	struct msm_mercury_device *pmercury_dev = filp->private_data;

	MCR_DBG("\n---(%d)%s()\n", __LINE__, __func__);

	rc = __msm_mercury_release(pmercury_dev);

	MCR_DBG("%s:%d] %s open_count = %d\n", __func__, __LINE__,
		filp->f_path.dentry->d_name.name, pmercury_dev->open_count);
	return rc;
}

static long msm_mercury_ioctl(struct file *filp, unsigned int cmd,
	unsigned long arg) {
	int rc;
	struct msm_mercury_device *pmercury_dev = filp->private_data;
	rc = __msm_mercury_ioctl(pmercury_dev, cmd, arg);
	return rc;
}

static const struct file_operations msm_mercury_fops = {
	.owner     = THIS_MODULE,
	.open    = msm_mercury_open,
	.release = msm_mercury_release,
	.unlocked_ioctl = msm_mercury_ioctl,
};

static struct class *msm_mercury_class;
static dev_t msm_mercury_devno;
static struct msm_mercury_device *msm_mercury_device_p;

int msm_mercury_subdev_init(struct v4l2_subdev *mercury_sd)
{
	int rc;
	struct msm_mercury_device *pgmn_dev =
		(struct msm_mercury_device *)mercury_sd->host_priv;

	MCR_DBG("%s:%d: mercury_sd=0x%x pgmn_dev=0x%x\n",
		__func__, __LINE__, (uint32_t)mercury_sd, (uint32_t)pgmn_dev);
	rc = __msm_mercury_open(pgmn_dev);
	MCR_DBG("%s:%d: rc=%d\n",
		__func__, __LINE__, rc);
	return rc;
}

static long msm_mercury_subdev_ioctl(struct v4l2_subdev *sd,
	unsigned int cmd, void *arg)
{
	long rc;
	struct msm_mercury_device *pgmn_dev =
		(struct msm_mercury_device *)sd->host_priv;

	MCR_DBG("%s: cmd=%d\n", __func__, cmd);

	MCR_DBG("%s: pgmn_dev 0x%x", __func__, (uint32_t)pgmn_dev);

	MCR_DBG("%s: Calling __msm_mercury_ioctl\n", __func__);

	rc = __msm_mercury_ioctl(pgmn_dev, cmd, (unsigned long)arg);
	pr_debug("%s: X\n", __func__);
	return rc;
}

void msm_mercury_subdev_release(struct v4l2_subdev *mercury_sd)
{
	int rc;
	struct msm_mercury_device *pgmn_dev =
		(struct msm_mercury_device *)mercury_sd->host_priv;
	MCR_DBG("%s:pgmn_dev=0x%x", __func__, (uint32_t)pgmn_dev);
	rc = __msm_mercury_release(pgmn_dev);
	MCR_DBG("%s:rc=%d", __func__, rc);
}

static const struct v4l2_subdev_core_ops msm_mercury_subdev_core_ops = {
	.ioctl = msm_mercury_subdev_ioctl,
};

static const struct v4l2_subdev_ops msm_mercury_subdev_ops = {
	.core = &msm_mercury_subdev_core_ops,
};

static int msm_mercury_init(struct platform_device *pdev)
{
	int rc = -1;
	struct device *dev;

	MCR_DBG("%s:\n", __func__);
	msm_mercury_device_p = __msm_mercury_init(pdev);
	if (msm_mercury_device_p == NULL) {
		MCR_PR_ERR("%s: initialization failed\n", __func__);
		goto fail;
	}

	v4l2_subdev_init(&msm_mercury_device_p->subdev,
		&msm_mercury_subdev_ops);
	v4l2_set_subdev_hostdata(&msm_mercury_device_p->subdev,
		msm_mercury_device_p);
	pr_debug("%s: msm_mercury_device_p 0x%x", __func__,
		(uint32_t)msm_mercury_device_p);
	MCR_DBG("%s:mercury: platform_set_drvdata\n", __func__);
	platform_set_drvdata(pdev, &msm_mercury_device_p->subdev);

	rc = alloc_chrdev_region(&msm_mercury_devno, 0, 1, MSM_MERCURY_NAME);
	if (rc < 0) {
		MCR_PR_ERR("%s: failed to allocate chrdev\n", __func__);
		goto fail_1;
	}

	if (!msm_mercury_class) {
		msm_mercury_class = class_create(THIS_MODULE, MSM_MERCURY_NAME);
		if (IS_ERR(msm_mercury_class)) {
			rc = PTR_ERR(msm_mercury_class);
			MCR_PR_ERR("%s: create device class failed\n",
				__func__);
			goto fail_2;
		}
	}

	dev = device_create(msm_mercury_class, NULL,
		MKDEV(MAJOR(msm_mercury_devno), MINOR(msm_mercury_devno)), NULL,
		"%s%d", MSM_MERCURY_NAME, 0);

	if (IS_ERR(dev)) {
		MCR_PR_ERR("%s: error creating device\n", __func__);
		rc = -ENODEV;
		goto fail_3;
	}

	cdev_init(&msm_mercury_device_p->cdev, &msm_mercury_fops);
	msm_mercury_device_p->cdev.owner = THIS_MODULE;
	msm_mercury_device_p->cdev.ops   =
		(const struct file_operations *) &msm_mercury_fops;
	rc = cdev_add(&msm_mercury_device_p->cdev, msm_mercury_devno, 1);
	if (rc < 0) {
		MCR_PR_ERR("%s: error adding cdev\n", __func__);
		rc = -ENODEV;
		goto fail_4;
	}

	MCR_DBG("%s %s: success\n", __func__, MSM_MERCURY_NAME);

	return rc;

fail_4:
	device_destroy(msm_mercury_class, msm_mercury_devno);

fail_3:
	class_destroy(msm_mercury_class);

fail_2:
	unregister_chrdev_region(msm_mercury_devno, 1);

fail_1:
	__msm_mercury_exit(msm_mercury_device_p);

fail:
	return rc;
}

static void msm_mercury_exit(void)
{
	cdev_del(&msm_mercury_device_p->cdev);
	device_destroy(msm_mercury_class, msm_mercury_devno);
	class_destroy(msm_mercury_class);
	unregister_chrdev_region(msm_mercury_devno, 1);

	__msm_mercury_exit(msm_mercury_device_p);
}

static int __msm_mercury_probe(struct platform_device *pdev)
{
	return msm_mercury_init(pdev);
}

static int __msm_mercury_remove(struct platform_device *pdev)
{
	msm_mercury_exit();
	return 0;
}

static struct platform_driver msm_mercury_driver = {
	.probe  = __msm_mercury_probe,
	.remove = __msm_mercury_remove,
	.driver = {
		.name = MSM_MERCURY_DRV_NAME,
		.owner = THIS_MODULE,
	},
};

static int __init msm_mercury_driver_init(void)
{
	int rc;
	rc = platform_driver_register(&msm_mercury_driver);
	return rc;
}

static void __exit msm_mercury_driver_exit(void)
{
	platform_driver_unregister(&msm_mercury_driver);
}

MODULE_DESCRIPTION("msm mercury jpeg driver");

module_init(msm_mercury_driver_init);
module_exit(msm_mercury_driver_exit);
