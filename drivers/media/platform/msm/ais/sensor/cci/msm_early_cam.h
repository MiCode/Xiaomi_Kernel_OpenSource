/* Copyright (c) 2012-2017, The Linux Foundation. All rights reserved.
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

#ifndef MSM_EARLY_CAM_H
#define MSM_EARLY_CAM_H

#include <linux/clk.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <media/v4l2-subdev.h>
#include <linux/workqueue.h>
#include <media/ais/msm_ais_sensor.h>
#include <soc/qcom/ais.h>
#include "msm_sd.h"
#include "cam_soc_api.h"

#define NUM_MASTERS 2
#define NUM_QUEUES 2

#define TRUE  1
#define FALSE 0


enum msm_early_cam_state_t {
	STATE_DISABLED,
	STATE_ENABLED,
};

struct early_cam_device {
	struct platform_device *pdev;
	uint8_t ref_count;
	enum msm_early_cam_state_t early_cam_state;
	size_t num_clk;
	size_t num_clk_cases;
	struct clk **early_cam_clk;
	uint32_t **early_cam_clk_rates;
	struct msm_cam_clk_info *early_cam_clk_info;
	struct camera_vreg_t *early_cam_vreg;
	struct regulator *early_cam_reg_ptr[MAX_REGULATOR];
	int32_t regulator_count;
};

int msm_early_cam_disable_clocks(void);
#endif
