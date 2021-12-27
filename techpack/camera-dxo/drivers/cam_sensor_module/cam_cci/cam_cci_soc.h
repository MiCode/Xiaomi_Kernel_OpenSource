/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 */

#ifndef _CAM_CCI_SOC_H_
#define _CAM_CCI_SOC_H_

#include "cam_cci_core.h"
#include "cam_soc_util.h"

/**
 * @sd: V4L2 sub device
 * @c_ctrl: CCI control structure
 *
 * This API initializes the CCI and acquires SOC resources
 */
int cam_cci_init(struct v4l2_subdev *sd,
	struct cam_cci_ctrl *c_ctrl);

/**
 * @cci_dev: CCI device structure
 *
 * This API releases the CCI and its SOC resources
 */
int cam_cci_soc_release(struct cci_device *cci_dev);

/**
 * @pdev: Platform device
 * @new_cci_dev: CCI device structure
 *
 * This API parses CCI device tree
 */
int cam_cci_parse_dt_info(struct platform_device *pdev,
	struct cci_device *new_cci_dev);

/**
 * @pdev: Platform device
 * @cci_dev: CCI device structure
 *
 * This API puts all SOC resources
 */
void cam_cci_soc_remove(struct platform_device *pdev,
	struct cci_device *cci_dev);
#endif /* _CAM_CCI_SOC_H_ */
