/* Copyright (c) 2009-2012, The Linux Foundation. All rights reserved.
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
#ifndef MSM_FLASH_H
#define MSM_FLASH_H

#include <linux/i2c.h>
#include <linux/leds.h>
#include <media/v4l2-subdev.h>
#include <mach/board.h>
#include "msm_camera_i2c.h"

#define MAX_LED_TRIGGERS 2

struct msm_flash_ctrl_t;

struct msm_flash_reg_t {
	enum msm_camera_i2c_data_type default_data_type;
	struct msm_camera_i2c_reg_conf *init_setting;
	uint8_t init_setting_size;
	struct msm_camera_i2c_reg_conf *off_setting;
	uint8_t off_setting_size;
	struct msm_camera_i2c_reg_conf *low_setting;
	uint8_t low_setting_size;
	struct msm_camera_i2c_reg_conf *high_setting;
	uint8_t high_setting_size;
};

struct msm_flash_fn_t {
	int (*flash_led_config)(struct msm_flash_ctrl_t *, uint8_t);
	int (*flash_led_init)(struct msm_flash_ctrl_t *);
	int (*flash_led_release)(struct msm_flash_ctrl_t *);
	int (*flash_led_off)(struct msm_flash_ctrl_t *);
	int (*flash_led_low)(struct msm_flash_ctrl_t *);
	int (*flash_led_high)(struct msm_flash_ctrl_t *);
};

struct msm_flash_ctrl_t {
	struct msm_camera_i2c_client *flash_i2c_client;
	struct platform_device *pdev;
	struct i2c_client *expander_client;
	struct v4l2_subdev v4l2_sdev;
	struct msm_camera_sensor_flash_data *flash_data;
	struct msm_camera_sensor_strobe_flash_data *strobe_flash_data;
	struct msm_flash_fn_t *func_tbl;
	struct msm_flash_reg_t *reg_setting;
	const char *led_trigger_name[MAX_LED_TRIGGERS];
	struct led_trigger *led_trigger[MAX_LED_TRIGGERS];
	uint32_t max_current[MAX_LED_TRIGGERS];
	void *data;
};

int msm_flash_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id);

int msm_flash_platform_probe(struct platform_device *pdev, void *data);

int msm_flash_create_v4l2_subdev(void *data, uint8_t sd_index);

int msm_camera_flash_led_config(struct msm_flash_ctrl_t *fctrl,
	uint8_t led_state);

int msm_flash_led_init(struct msm_flash_ctrl_t *fctrl);

int msm_flash_led_release(struct msm_flash_ctrl_t *fctrl);

int msm_flash_led_off(struct msm_flash_ctrl_t *fctrl);

int msm_flash_led_low(struct msm_flash_ctrl_t *fctrl);

int msm_flash_led_high(struct msm_flash_ctrl_t *fctrl);

#define VIDIOC_MSM_FLASH_LED_DATA_CFG \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 20, void __user *)

#define VIDIOC_MSM_FLASH_STROBE_DATA_CFG \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 21, void __user *)

#define VIDIOC_MSM_FLASH_CFG \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 22, void __user *)

#endif
