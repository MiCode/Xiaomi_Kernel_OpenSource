// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2011-2014, 2016, 2018, 2020 The Linux Foundation. All rights reserved.
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
#include "msm_camera_i2c_mux.h"

/* TODO move this somewhere else */
#define MSM_I2C_MUX_DRV_NAME "msm_cam_i2c_mux"
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

static int i2c_mux_probe(struct platform_device *pdev)
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

	mux_device->ctl_base = msm_camera_get_reg_base(pdev,
		"i2c_mux_ctl", true);
	if (!mux_device->ctl_base) {
		pr_err("%s: no mem resource?\n", __func__);
		rc = -ENODEV;
		goto ctl_base_failed;
	}
	mux_device->rw_base = msm_camera_get_reg_base(pdev, "i2c_mux_rw", true);
	if (!mux_device->rw_mem) {
		pr_err("%s: no mem resource?\n", __func__);
		rc = -ENODEV;
		goto rw_base_failed;
	}
	mux_device->pdev = pdev;
	return 0;

rw_base_failed:
	msm_camera_put_reg_base(pdev, mux_device->ctl_base,
		"i2c_mux_ctl", true);
ctl_base_failed:
	mutex_destroy(&mux_device->mutex);
	kfree(mux_device);
	return 0;
}

static int i2c_mux_remove(struct platform_device *pdev)
{
	struct v4l2_subdev *sub_dev = platform_get_drvdata(pdev);
	struct i2c_mux_device *mux_device;

	if (!sub_dev) {
		pr_err("%s: sub device is NULL\n", __func__);
		return 0;
	}

	mux_device = (struct mux_device *)v4l2_get_subdevdata(sub_dev);
	if (!mux_device) {
		pr_err("%s: sub device is NULL\n", __func__);
		return 0;
	}

	msm_camera_put_reg_base(pdev, mux_device->rw_base, "i2c_mux_ctl", true);
	msm_camera_put_reg_base(pdev, mux_device->ctl_base, "i2c_mux_rw", true);
}

static struct platform_driver i2c_mux_driver = {
	.probe = i2c_mux_probe,
	.remove = i2c_mux_remove,
	.driver = {
		.name = MSM_I2C_MUX_DRV_NAME,
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
