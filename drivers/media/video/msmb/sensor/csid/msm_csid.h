/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
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
#include <linux/platform_device.h>
#include <media/v4l2-subdev.h>
#include <media/msm_cam_sensor.h>
#include "msm_sd.h"

enum msm_csid_state_t {
	CSID_POWER_UP,
	CSID_POWER_DOWN,
};

struct csid_device {
	struct platform_device *pdev;
	struct msm_sd_subdev msm_sd;
	struct resource *mem;
	struct resource *irq;
	struct resource *io;
	struct regulator *csi_vdd;
	void __iomem *base;
	struct mutex mutex;
	struct completion reset_complete;
	uint32_t hw_version;
	enum msm_csid_state_t csid_state;

	struct clk *csid0_clk[11];
	struct clk *csid_clk[11];
};

#define VIDIOC_MSM_CSID_RELEASE \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 5, struct v4l2_subdev*)
#endif
