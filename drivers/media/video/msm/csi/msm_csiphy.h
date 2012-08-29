/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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

#ifndef MSM_CSIPHY_H
#define MSM_CSIPHY_H

#include <linux/clk.h>
#include <linux/io.h>
#include <media/v4l2-subdev.h>
#include <media/msm_camera.h>

#define MAX_CSIPHY 3

enum msm_csiphy_state_t {
	CSIPHY_POWER_UP,
	CSIPHY_POWER_DOWN,
};

struct csiphy_device {
	struct platform_device *pdev;
	struct v4l2_subdev subdev;
	struct resource *mem;
	struct resource *irq;
	struct resource *io;
	void __iomem *base;
	struct mutex mutex;
	uint32_t hw_version;
	enum msm_csiphy_state_t csiphy_state;

	struct clk *csiphy_clk[3];
	uint8_t ref_count;
	uint16_t lane_mask[MAX_CSIPHY];
};

#define VIDIOC_MSM_CSIPHY_CFG \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 7, struct csiphy_cfg_data*)

#define VIDIOC_MSM_CSIPHY_RELEASE \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 9, void *)
#endif
