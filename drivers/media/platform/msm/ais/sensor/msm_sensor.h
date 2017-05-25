/* Copyright (c) 2011-2017, The Linux Foundation. All rights reserved.
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

#ifndef MSM_SENSOR_H
#define MSM_SENSOR_H

#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <soc/qcom/ais.h>
#include <media/ais/msm_ais_sensor.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-ioctl.h>
#include "msm_camera_i2c.h"
#include "msm_camera_dt_util.h"
#include "msm_sd.h"
#include "msm_sensor_init.h"

#define DEFINE_MSM_MUTEX(mutexname) \
	static struct mutex mutexname = __MUTEX_INITIALIZER(mutexname)

enum msm_sensor_sensor_slave_info_type {
	MSM_SENSOR_SLAVEADDR_DATA,
	MSM_SENSOR_IDREGADDR_DATA,
	MSM_SENSOR_SENSOR_ID_DATA,
	MSM_SENSOR_SENIDMASK_DATA,
	MSM_SENSOR_NUM_ID_INFO_DATA,
};

struct msm_sensor_ctrl_t;

enum msm_sensor_state_t {
	MSM_SENSOR_POWER_DOWN,
	MSM_SENSOR_POWER_UP,
};

struct msm_sensor_fn_t {
	int (*sensor_config)(struct msm_sensor_ctrl_t *, void __user *);
#ifdef CONFIG_COMPAT
	int (*sensor_config32)(struct msm_sensor_ctrl_t *, void __user *);
#endif
	int (*sensor_power_down)(struct msm_sensor_ctrl_t *);
	int (*sensor_power_up)(struct msm_sensor_ctrl_t *);
	int (*sensor_match_id)(struct msm_sensor_ctrl_t *);
};

struct msm_sensor_ctrl_t {
	struct platform_device *pdev;
	struct mutex *msm_sensor_mutex;

	enum msm_camera_device_type_t sensor_device_type;
	struct msm_camera_sensor_board_info *sensordata;
	struct msm_sensor_power_setting_array power_setting_array;
	struct msm_sensor_packed_cfg_t *cfg_override;
	struct msm_sd_subdev msm_sd;
	enum cci_i2c_master_t cci_i2c_master;

	struct msm_camera_i2c_client *sensor_i2c_client;
	struct v4l2_subdev_info *sensor_v4l2_subdev_info;
	uint8_t sensor_v4l2_subdev_info_size;
	struct v4l2_subdev_ops *sensor_v4l2_subdev_ops;
	struct msm_sensor_fn_t *func_tbl;
	struct msm_camera_i2c_reg_setting stop_setting;
	void *misc_regulator;
	enum msm_sensor_state_t sensor_state;
	uint8_t is_probe_succeed;
	uint32_t id;
	struct device_node *of_node;
	enum msm_camera_stream_type_t camera_stream_type;
	uint32_t set_mclk_23880000;
	uint8_t is_csid_tg_mode;
	uint32_t is_secure;

	struct msm_sensor_init_t s_init;
};

int msm_sensor_config(struct msm_sensor_ctrl_t *s_ctrl, void __user *argp);

int msm_sensor_power_up(struct msm_sensor_ctrl_t *s_ctrl);

int msm_sensor_power_down(struct msm_sensor_ctrl_t *s_ctrl);

int msm_sensor_check_id(struct msm_sensor_ctrl_t *s_ctrl);

int msm_sensor_match_id(struct msm_sensor_ctrl_t *s_ctrl);

int msm_sensor_update_cfg(struct msm_sensor_ctrl_t *s_ctrl);

int msm_sensor_free_sensor_data(struct msm_sensor_ctrl_t *s_ctrl);

int32_t msm_sensor_init_default_params(struct msm_sensor_ctrl_t *s_ctrl);

int32_t msm_sensor_get_dt_gpio_req_tbl(struct device_node *of_node,
	struct msm_camera_gpio_conf *gconf, uint16_t *gpio_array,
	uint16_t gpio_array_size);

int32_t msm_sensor_get_dt_gpio_set_tbl(struct device_node *of_node,
	struct msm_camera_gpio_conf *gconf, uint16_t *gpio_array,
	uint16_t gpio_array_size);

int32_t msm_sensor_init_gpio_pin_tbl(struct device_node *of_node,
	struct msm_camera_gpio_conf *gconf, uint16_t *gpio_array,
	uint16_t gpio_array_size);
#ifdef CONFIG_COMPAT
long msm_sensor_subdev_fops_ioctl(struct file *file,
	unsigned int cmd,
	unsigned long arg);
#endif
#endif
