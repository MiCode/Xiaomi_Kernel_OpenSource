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

#ifndef MSM_SENSOR_BAYER_H
#define MSM_SENSOR_BAYER_H

#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <mach/camera.h>
#include <mach/gpio.h>
#include <media/msm_camera.h>
#include <media/v4l2-subdev.h>
#include "msm_camera_i2c.h"
#include "msm_camera_eeprom.h"
#include "msm_sensor_common.h"

struct sensor_driver_t {
	struct platform_driver *platform_pdriver;
	int32_t (*platform_probe)(struct platform_device *pdev);
};

int32_t msm_sensor_bayer_config(struct msm_sensor_ctrl_t *s_ctrl,
			void __user *argp);
int32_t msm_sensor_bayer_power_up(struct msm_sensor_ctrl_t *s_ctrl);
int32_t msm_sensor_bayer_power_down(struct msm_sensor_ctrl_t *s_ctrl);

int32_t msm_sensor_bayer_match_id(struct msm_sensor_ctrl_t *s_ctrl);
int msm_sensor_bayer_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id);
int32_t msm_sensor_delay_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id);
int32_t msm_sensor_bayer_power(struct v4l2_subdev *sd, int on);

int32_t msm_sensor_bayer_v4l2_s_ctrl(struct v4l2_subdev *sd,
	struct v4l2_control *ctrl);

int32_t msm_sensor_bayer_v4l2_query_ctrl(
	struct v4l2_subdev *sd, struct v4l2_queryctrl *qctrl);

int msm_sensor_bayer_s_ctrl_by_enum(struct msm_sensor_ctrl_t *s_ctrl,
		struct msm_sensor_v4l2_ctrl_info_t *ctrl_info, int value);

int msm_sensor_bayer_v4l2_enum_fmt(struct v4l2_subdev *sd, unsigned int index,
			enum v4l2_mbus_pixelcode *code);

long msm_sensor_bayer_subdev_ioctl(struct v4l2_subdev *sd,
			unsigned int cmd, void *arg);

int32_t msm_sensor_bayer_get_csi_params(struct msm_sensor_ctrl_t *s_ctrl,
		struct csi_lane_params_t *sensor_output_info);

int32_t msm_sensor_bayer_eeprom_read(struct msm_sensor_ctrl_t *s_ctrl);

#define VIDIOC_MSM_SENSOR_CFG \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 4, void __user *)

#define VIDIOC_MSM_SENSOR_RELEASE \
	_IO('V', BASE_VIDIOC_PRIVATE + 11)

#define VIDIOC_MSM_SENSOR_CSID_INFO\
	_IOWR('V', BASE_VIDIOC_PRIVATE + 12, struct msm_sensor_csi_info *)
#endif
