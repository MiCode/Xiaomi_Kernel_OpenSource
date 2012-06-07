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

#ifndef MSM_CSIC_H
#define MSM_CSIC_H

#include <linux/clk.h>
#include <linux/io.h>
#include <media/v4l2-subdev.h>

#define CSIC_7X 0x1
#define CSIC_8X (0x1 << 1)

struct csic_device {
	struct platform_device *pdev;
	struct v4l2_subdev subdev;
	struct resource *mem;
	struct resource *irq;
	struct resource *io;
	void __iomem *base;
	struct mutex mutex;
	uint32_t hw_version;

	struct clk *csic_clk[5];
};

struct csic_cfg_params {
	struct v4l2_subdev *subdev;
	void *parms;
};

#define VIDIOC_MSM_CSIC_CFG \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 4, struct csic_cfg_params)

#define VIDIOC_MSM_CSIC_INIT \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 5, struct v4l2_subdev*)

#define VIDIOC_MSM_CSIC_RELEASE \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 6, struct v4l2_subdev*)

#endif

