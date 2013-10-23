/* Copyright (c) 2009-2013, The Linux Foundation. All rights reserved.
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
#ifndef MSM_LED_FLASH_H
#define MSM_LED_FLASH_H

#include <linux/leds.h>
#include <linux/platform_device.h>
#include <media/v4l2-subdev.h>
#include <media/msm_cam_sensor.h>
#include <mach/camera2.h>
#include "msm_camera_i2c.h"
#include "msm_sd.h"

#define MAX_LED_TRIGGERS 3

struct msm_led_flash_ctrl_t;

struct msm_flash_fn_t {
	int32_t (*flash_get_subdev_id)(struct msm_led_flash_ctrl_t *, void *);
	int32_t (*flash_led_config)(struct msm_led_flash_ctrl_t *, void *);
	int32_t (*flash_led_init)(struct msm_led_flash_ctrl_t *);
	int32_t (*flash_led_release)(struct msm_led_flash_ctrl_t *);
	int32_t (*flash_led_off)(struct msm_led_flash_ctrl_t *);
	int32_t (*flash_led_low)(struct msm_led_flash_ctrl_t *);
	int32_t (*flash_led_high)(struct msm_led_flash_ctrl_t *);
};

struct msm_led_flash_reg_t {
	struct msm_camera_i2c_reg_setting *init_setting;
	struct msm_camera_i2c_reg_setting *off_setting;
	struct msm_camera_i2c_reg_setting *release_setting;
	struct msm_camera_i2c_reg_setting *low_setting;
	struct msm_camera_i2c_reg_setting *high_setting;
};

struct msm_led_flash_ctrl_t {
	struct msm_camera_i2c_client *flash_i2c_client;
	struct msm_sd_subdev msm_sd;
	struct platform_device *pdev;
	struct msm_flash_fn_t *func_tbl;
	struct msm_camera_sensor_board_info *flashdata;
	struct msm_led_flash_reg_t *reg_setting;
	const char *flash_trigger_name[MAX_LED_TRIGGERS];
	struct led_trigger *flash_trigger[MAX_LED_TRIGGERS];
	uint32_t flash_op_current[MAX_LED_TRIGGERS];
	uint32_t flash_max_current[MAX_LED_TRIGGERS];
	const char *torch_trigger_name;
	struct led_trigger *torch_trigger;
	uint32_t torch_op_current;
	uint32_t torch_max_current;
	void *data;
	uint32_t num_sources;
	enum msm_camera_device_type_t flash_device_type;
	uint32_t subdev_id;
};

int msm_flash_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id);

int32_t msm_led_flash_create_v4lsubdev(struct platform_device *pdev,
	void *data);
int32_t msm_led_i2c_flash_create_v4lsubdev(void *data);

int32_t msm_led_i2c_trigger_get_subdev_id(struct msm_led_flash_ctrl_t *fctrl,
	void *arg);

int32_t msm_led_i2c_trigger_config(struct msm_led_flash_ctrl_t *fctrl,
	void *data);

int msm_flash_led_init(struct msm_led_flash_ctrl_t *fctrl);
int msm_flash_led_release(struct msm_led_flash_ctrl_t *fctrl);
int msm_flash_led_off(struct msm_led_flash_ctrl_t *fctrl);
int msm_flash_led_low(struct msm_led_flash_ctrl_t *fctrl);
int msm_flash_led_high(struct msm_led_flash_ctrl_t *fctrl);
#endif
