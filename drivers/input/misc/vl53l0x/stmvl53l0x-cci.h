/*
 *  stmvl53l0-cci.h - Linux kernel modules for STM VL53L0 FlightSense TOF sensor
 *
 *  Copyright (C) 2016 STMicroelectronics Imaging Division
 *  Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */


#ifndef STMVL_CCI_H
#define STMVL_CCI_H
#include <linux/types.h>

#ifdef CAMERA_CCI
#include <soc/qcom/camera2.h>
#include "msm_camera_i2c.h"
#include "msm_camera_dt_util.h"
#include "msm_camera_io_util.h"
#include "msm_cci.h"

#define	MSM_TOF_MAX_VREGS (10)

struct msm_tof_vreg {
	struct camera_vreg_t *cam_vreg;
	void *data[MSM_TOF_MAX_VREGS];
	int num_vreg;
};

struct cci_data {
	struct msm_camera_i2c_client g_client;
	struct msm_camera_i2c_client *client;
	struct platform_device *pdev;
	enum msm_camera_device_type_t device_type;
	enum cci_i2c_master_t cci_master;
	struct msm_tof_vreg vreg_cfg;
	struct msm_sd_subdev msm_sd;
	struct v4l2_subdev sdev;
	struct v4l2_subdev_ops *subdev_ops;
	char subdev_initialized;
	uint32_t subdev_id;
	uint8_t power_up;
};
int stmvl53l0x_init_cci(void);
void stmvl53l0x_exit_cci(void *);
int stmvl53l0x_power_down_cci(void *);
int stmvl53l0x_power_up_cci(void *, unsigned int *);
#endif /* CAMERA_CCI */
#endif /* STMVL_CCI_H */
