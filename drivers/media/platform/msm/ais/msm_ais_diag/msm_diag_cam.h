/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
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

#ifndef MSM_DIAG_CAM_H
#define MSM_DIAG_CAM_H

#include <linux/clk.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <media/v4l2-subdev.h>
#include <linux/workqueue.h>
#include <media/ais/msm_ais_sensor.h>
#include <soc/qcom/ais.h>
#include <media/ais/msm_ais.h>
#include "msm_sd.h"
#include "cam_soc_api.h"

#define NUM_MASTERS 2
#define NUM_QUEUES 2

#define TRUE  1
#define FALSE 0


enum msm_diag_cam_state_t {
	AIS_DIAG_STATE_DISABLED,
	AIS_DIAG_STATE_ENABLED,
};

struct diag_cam_device {
	struct platform_device *pdev;
	uint8_t ref_count;
	enum msm_diag_cam_state_t diag_cam_state;
	size_t num_clk;
	size_t num_clk_cases;
	struct clk **diag_cam_clk;
	uint32_t **diag_cam_clk_rates;
	struct msm_cam_clk_info *diag_cam_clk_info;
	struct camera_vreg_t *diag_cam_vreg;
	struct regulator *diag_cam_reg_ptr[MAX_REGULATOR];
	int32_t regulator_count;
};

int msm_ais_enable_allclocks(void);
int msm_ais_disable_allclocks(void);
int msm_diag_camera_get_vreginfo_list(
		struct msm_ais_diag_regulator_info_list_t *p_vreglist);
#endif
