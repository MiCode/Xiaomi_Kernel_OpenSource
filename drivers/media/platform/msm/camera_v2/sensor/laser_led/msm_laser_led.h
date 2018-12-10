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
 *
 */

#ifndef MSM_LASER_LED_H
#define MSM_LASER_LED_H

#include <linux/i2c.h>
#include <linux/gpio.h>
#include <soc/qcom/camera2.h>
#include <media/v4l2-subdev.h>
#include <media/msmb_camera.h>
#include <linux/platform_device.h>
#include <media/v4l2-ioctl.h>
#include <media/msm_cam_sensor.h>
#include "msm_camera_i2c.h"
#include "msm_camera_dt_util.h"
#include "msm_camera_io_util.h"
#include "msm_sd.h"


#define DEFINE_MSM_MUTEX(mutexname) \
	static struct mutex mutexname = __MUTEX_INITIALIZER(mutexname)

enum msm_camera_laser_led_state_t {
	MSM_CAMERA_LASER_LED_INIT,
	MSM_CAMERA_LASER_LED_RELEASE,
};

struct msm_laser_led_ctrl_t;

struct msm_laser_led_ctrl_t {
	struct msm_sd_subdev msm_sd;
	struct platform_device *pdev;
	struct msm_laser_led_func_t *func_tbl;
	struct msm_camera_power_ctrl_t power_info;
	struct i2c_driver *i2c_driver;
	struct platform_driver *pdriver;
	struct msm_camera_i2c_client i2c_client;
	enum msm_camera_device_type_t laser_led_device_type;
	struct v4l2_subdev sdev;
	struct v4l2_subdev_ops *laser_led_v4l2_subdev_ops;
	struct mutex *laser_led_mutex;
	enum msm_camera_laser_led_state_t laser_led_state;
	enum cci_i2c_master_t cci_master;
};

#endif
