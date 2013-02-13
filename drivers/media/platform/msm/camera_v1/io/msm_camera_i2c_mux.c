/* Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.
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

#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/module.h>
#include <mach/board.h>
#include <mach/camera.h>
#include "msm.h"
#include "msm_camera_i2c_mux.h"

static int msm_i2c_mux_config(struct i2c_mux_device *mux_device, uint8_t *mode)
{
	uint32_t val;
	val = msm_camera_io_r(mux_device->ctl_base);
	if (*mode == MODE_DUAL) {
		msm_camera_io_w(val | 0x3, mux_device->ctl_base);
	} else if (*mode == MODE_L) {
		msm_camera_io_w(((val | 0x2) & ~(0x1)), mux_device->ctl_base);
		val = msm_camera_io_r(mux_device->ctl_base);
		CDBG("the camio mode config left value is %d\n", val);
	} else {
		msm_camera_io_w(((val | 0x1) & ~(0x2)), mux_device->ctl_base);
		val = msm_camera_io_r(mux_device->ctl_base);
		CDBG("the camio mode config right value is %d\n", val);
	}
	return 0;
}

static int msm_i2c_mux_init(struct i2c_mux_device *mux_device)
{
	int rc = 0, val = 0;
	if (mux_device->use_count == 0) {
		mux_device->ctl_base = ioremap(mux_device->ctl_mem->start,
			resource_size(mux_device->ctl_mem));
		if (!mux_device->ctl_base) {
			rc = -ENOMEM;
			return rc;
		}
		mux_device->rw_base = ioremap(mux_device->rw_mem->start,
			resource_size(mux_device->rw_mem));
		if (!mux_device->rw_base) {
			rc = -ENOMEM;
			iounmap(mux_device->ctl_base);
			return rc;
		}
		val = msm_camera_io_r(mux_device->rw_base);
		msm_camera_io_w((val | 0x200), mux_device->rw_base);
	}
	mux_device->use_count++;
	return 0;
};

static int msm_i2c_mux_release(struct i2c_mux_device *mux_device)
{
	int val = 0;
	mux_device->use_count--;
	if (mux_device->use_count == 0) {
		val = msm_camera_io_r(mux_device->rw_base);
		msm_camera_io_w((val & ~0x200), mux_device->rw_base);
		iounmap(mux_device->rw_base);
		iounmap(mux_device->ctl_base);
	}
	return 0;
}

static long msm_i2c_mux_subdev_ioctl(struct v4l2_subdev *sd,
			unsigned int cmd, void *arg)
{
	struct i2c_mux_device *mux_device;
	int rc = 0;
	mux_device = v4l2_get_subdevdata(sd);
	if (mux_device == NULL) {
		rc = -ENOMEM;
		return rc;
	}
	mutex_lock(&mux_device->mutex);
	switch (cmd) {
	case VIDIOC_MSM_I2C_MUX_CFG:
		rc = msm_i2c_mux_config(mux_device, (uint8_t *) arg);
		break;
	case VIDIOC_MSM_I2C_MUX_INIT:
		rc = msm_i2c_mux_init(mux_device);
		break;
	case VIDIOC_MSM_I2C_MUX_RELEASE:
		rc = msm_i2c_mux_release(mux_device);
		break;
	default:
		rc = -ENOIOCTLCMD;
	}
	mutex_unlock(&mux_device->mutex);
	return rc;
}

static struct v4l2_subdev_core_ops msm_i2c_mux_subdev_core_ops = {
	.ioctl = &msm_i2c_mux_subdev_ioctl,
};

static const struct v4l2_subdev_ops msm_i2c_mux_subdev_ops = {
	.core = &msm_i2c_mux_subdev_core_ops,
};

static int __devinit i2c_mux_probe(struct platform_device *pdev)
{
	struct i2c_mux_device *mux_device;
	int rc = 0;
	CDBG("%s: device id = %d\n", __func__, pdev->id);
	mux_device = kzalloc(sizeof(struct i2c_mux_device), GFP_KERNEL);
	if (!mux_device) {
		pr_err("%s: no enough memory\n", __func__);
		return -ENOMEM;
	}

	v4l2_subdev_init(&mux_device->subdev, &msm_i2c_mux_subdev_ops);
	v4l2_set_subdevdata(&mux_device->subdev, mux_device);
	platform_set_drvdata(pdev, &mux_device->subdev);
	mutex_init(&mux_device->mutex);

	mux_device->ctl_mem = platform_get_resource_byname(pdev,
					IORESOURCE_MEM, "i2c_mux_ctl");
	if (!mux_device->ctl_mem) {
		pr_err("%s: no mem resource?\n", __func__);
		rc = -ENODEV;
		goto i2c_mux_no_resource;
	}
	mux_device->ctl_io = request_mem_region(mux_device->ctl_mem->start,
		resource_size(mux_device->ctl_mem), pdev->name);
	if (!mux_device->ctl_io) {
		pr_err("%s: no valid mem region\n", __func__);
		rc = -EBUSY;
		goto i2c_mux_no_resource;
	}
	mux_device->rw_mem = platform_get_resource_byname(pdev,
					IORESOURCE_MEM, "i2c_mux_rw");
	if (!mux_device->rw_mem) {
		pr_err("%s: no mem resource?\n", __func__);
		rc = -ENODEV;
		goto i2c_mux_no_resource;
	}
	mux_device->rw_io = request_mem_region(mux_device->rw_mem->start,
		resource_size(mux_device->rw_mem), pdev->name);
	if (!mux_device->rw_io) {
		pr_err("%s: no valid mem region\n", __func__);
		rc = -EBUSY;
		goto i2c_mux_no_resource;
	}
	mux_device->pdev = pdev;
	return 0;

i2c_mux_no_resource:
	mutex_destroy(&mux_device->mutex);
	kfree(mux_device);
	return 0;
}

static struct platform_driver i2c_mux_driver = {
	.probe = i2c_mux_probe,
	.driver = {
		.name = MSM_I2C_MUX_DRV_NAME,
		.owner = THIS_MODULE,
	},
};

static int __init msm_camera_i2c_mux_init_module(void)
{
	return platform_driver_register(&i2c_mux_driver);
}

static void __exit msm_camera_i2c_mux_exit_module(void)
{
	platform_driver_unregister(&i2c_mux_driver);
}

module_init(msm_camera_i2c_mux_init_module);
module_exit(msm_camera_i2c_mux_exit_module);
MODULE_DESCRIPTION("MSM Camera I2C mux driver");
MODULE_LICENSE("GPL v2");
