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

#ifndef MSM_CSID_H
#define MSM_CSID_H

#include <linux/clk.h>
#include <linux/io.h>
#include <media/v4l2-subdev.h>
#include <media/msm_camera.h>

enum msm_csid_state_t {
	CSID_POWER_UP,
	CSID_POWER_DOWN,
};

struct csid_device {
	struct platform_device *pdev;
	struct v4l2_subdev subdev;
	struct resource *mem;
	struct resource *irq;
	struct resource *io;
	struct regulator *csi_vdd;
	void __iomem *base;
	struct mutex mutex;
	struct completion reset_complete;
	uint32_t hw_version;
	enum msm_csid_state_t csid_state;

	struct clk *csid0_clk[6];
	struct clk *csid_clk[6];
};

#define VIDIOC_MSM_CSID_CFG \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 4, struct csic_cfg_data*)

#define VIDIOC_MSM_CSID_RELEASE \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 5, struct v4l2_subdev*)
#endif

