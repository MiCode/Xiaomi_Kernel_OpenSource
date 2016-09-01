/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
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

#ifndef MSM_IR_LED_H
#define MSM_IR_LED_H

#include <linux/platform_device.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-ioctl.h>
#include <media/msm_cam_sensor.h>
#include <soc/qcom/camera2.h>
#include "msm_sd.h"

#define DEFINE_MSM_MUTEX(mutexname) \
	static struct mutex mutexname = __MUTEX_INITIALIZER(mutexname)

/* Default frequency is taken as 15KHz*/
#define DEFAULT_PWM_TIME_PERIOD_NS 66667
#define DEFAULT_PWM_DUTY_CYCLE_NS 0

enum msm_camera_ir_led_state_t {
	MSM_CAMERA_IR_LED_INIT,
	MSM_CAMERA_IR_LED_RELEASE,
};

enum msm_ir_led_driver_type {
	IR_LED_DRIVER_GPIO,
	IR_LED_DRIVER_DEFAULT,
};

struct msm_ir_led_ctrl_t;

struct msm_ir_led_func_t {
	int32_t (*camera_ir_led_init)(struct msm_ir_led_ctrl_t *,
		struct msm_ir_led_cfg_data_t *);
	int32_t (*camera_ir_led_release)(struct msm_ir_led_ctrl_t *,
		struct msm_ir_led_cfg_data_t *);
	int32_t (*camera_ir_led_off)(struct msm_ir_led_ctrl_t *,
		struct msm_ir_led_cfg_data_t *);
	int32_t (*camera_ir_led_on)(struct msm_ir_led_ctrl_t *,
		struct msm_ir_led_cfg_data_t *);
};

struct msm_ir_led_table {
	enum msm_ir_led_driver_type ir_led_driver_type;
	struct msm_ir_led_func_t func_tbl;
};

struct msm_ir_led_ctrl_t {
	struct msm_sd_subdev msm_sd;
	struct platform_device *pdev;
	struct pwm_device       *pwm_dev;
	struct msm_ir_led_func_t *func_tbl;
	struct msm_camera_power_ctrl_t power_info;

	enum msm_camera_device_type_t ir_led_device_type;
	struct mutex *ir_led_mutex;

	/* ir_led driver type */
	enum msm_ir_led_driver_type ir_led_driver_type;

	/* ir_led state */
	enum msm_camera_ir_led_state_t ir_led_state;
};

#endif
