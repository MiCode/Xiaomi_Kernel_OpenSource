/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
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

#ifndef _CAM_CSIPHY_SOC_H_
#define _CAM_CSIPHY_SOC_H_

#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/irqreturn.h>
#include <linux/ion.h>
#include <linux/iommu.h>
#include <linux/timer.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <media/v4l2-event.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-subdev.h>
#include "cam_csiphy_dev.h"
#include "cam_csiphy_core.h"

#undef CDBG
#define CDBG(fmt, args...) pr_debug(fmt, ##args)

#define CSI_3PHASE_HW                               1
#define CSIPHY_VERSION_V35                        0x35
#define CSIPHY_VERSION_V10                        0x10
#define CSIPHY_VERSION_V11                        0x11
#define CSIPHY_VERSION_V20                        0x20

/**
 * @csiphy_dev: CSIPhy device structure
 *
 * This API release SOC related parameters
 */
int cam_csiphy_soc_release(struct csiphy_device *csiphy_dev);

/**
 * @pdev: Platform device
 * @csiphy_dev: CSIPhy device structure
 *
 * This API parses csiphy device tree information
 */
int cam_csiphy_parse_dt_info(struct platform_device *pdev,
	struct csiphy_device *csiphy_dev);

/**
 * @csiphy_dev: CSIPhy device structure
 *
 * This API enables SOC related parameters
 */
int cam_csiphy_enable_hw(struct csiphy_device *csiphy_dev);

/**
 * @csiphy_dev: CSIPhy device structure
 *
 * This API disables SOC related parameters
 */
int cam_csiphy_disable_hw(struct csiphy_device *csiphy_dev);

/**
 * @soc_info: Soc info of cam hw driver module
 *
 * This API dumps memory for the entire mapped region
 * (needs to be macro enabled before use)
 */
int cam_csiphy_mem_dmp(struct cam_hw_soc_info *soc_info);

#endif /* _CAM_CSIPHY_SOC_H_ */
