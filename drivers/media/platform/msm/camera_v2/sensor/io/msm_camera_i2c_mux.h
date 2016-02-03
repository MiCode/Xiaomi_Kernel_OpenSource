/* Copyright (c) 2011-2014, The Linux Foundation. All rights reserved.
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

#ifndef MSM_I2C_MUX_H
#define MSM_I2C_MUX_H

#include <linux/io.h>
#include <media/v4l2-subdev.h>

struct i2c_mux_device {
	struct platform_device *pdev;
	struct v4l2_subdev subdev;
	struct resource *ctl_mem;
	struct resource *ctl_io;
	void __iomem *ctl_base;
	struct resource *rw_mem;
	struct resource *rw_io;
	void __iomem *rw_base;
	struct mutex mutex;
	unsigned use_count;
};

struct i2c_mux_cfg_params {
	struct v4l2_subdev *subdev;
	void *parms;
};

#define VIDIOC_MSM_I2C_MUX_CFG \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 13, struct i2c_mux_cfg_params)

#define VIDIOC_MSM_I2C_MUX_INIT \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 14, struct v4l2_subdev*)

#define VIDIOC_MSM_I2C_MUX_RELEASE \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 15, struct v4l2_subdev*)

#endif
