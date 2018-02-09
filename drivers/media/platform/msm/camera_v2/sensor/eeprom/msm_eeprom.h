/* Copyright (c) 2011-2015, The Linux Foundation. All rights reserved.
 * Copyright (C) 2018 XiaoMi, Inc.
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
#ifndef MSM_EEPROM_H
#define MSM_EEPROM_H

#include <linux/i2c.h>
#include <linux/gpio.h>
#include <soc/qcom/camera2.h>
#include <media/v4l2-subdev.h>
#include <media/msmb_camera.h>
#include "msm_camera_i2c.h"
#include "msm_camera_spi.h"
#include "msm_camera_io_util.h"
#include "msm_camera_dt_util.h"

struct msm_eeprom_ctrl_t;

#define DEFINE_MSM_MUTEX(mutexname) \
	static struct mutex mutexname = __MUTEX_INITIALIZER(mutexname)

#define PROPERTY_MAXSIZE 32

/* define eeprom information */
#define SEMCO 0x03
#define LITEON 0x01
#define SUNNY 0x15
#define QTECH 0x06
#define OFILM 0x07
#define PRIMAX1 0x09
#define PRIMAX 0x25

#define IMX268 0x03
#define DUAL_IMX386_S5K3M3 0x0B
#define IMX386 0x0D
#define S5K3M3 0x0F
#define S5K4H9 0x10
#define OV5675 0x0A

#define SAGIT 0x07
#define CENTAUR 0x08
#define CHIRON 0x10

struct module_info_t {
	uint16_t module_num;
	char name[16];
};

struct project_info_t {
	uint16_t pro_num;
	char name[16];
};

struct sensor_info_t {
	uint16_t sensor_num;
	char name[16];
};

struct module_info_t module_info[] = {
	{SEMCO,  "_semco"},
	{LITEON, "_liteon"},
	{SUNNY,  "_sunny"},
	{QTECH,  "_qtech"},
	{OFILM,  "_ofilm"},
	{PRIMAX, "_primax"},
	{PRIMAX1, "_primax"}, /* for sagit*/
};

struct project_info_t project_info[] = {
	{SAGIT, "sagit_"},
	{CHIRON, "chiron_"},
};

struct sensor_info_t sensor_info[] = {
	{IMX268, "imx268"},
	{DUAL_IMX386_S5K3M3, "imx386"},
	{IMX386, "imx386"},
	{S5K3M3, "s5k3m3"},
	{S5K4H9, "s5k4h9"},
	{OV5675, "ov5675"},
};

struct msm_eeprom_ctrl_t {
	struct platform_device *pdev;
	struct mutex *eeprom_mutex;

	struct v4l2_subdev sdev;
	struct v4l2_subdev_ops *eeprom_v4l2_subdev_ops;
	enum msm_camera_device_type_t eeprom_device_type;
	struct msm_sd_subdev msm_sd;
	enum cci_i2c_master_t cci_master;
	enum i2c_freq_mode_t i2c_freq_mode;

	struct msm_camera_i2c_client i2c_client;
	struct msm_eeprom_board_info *eboard_info;
	uint32_t subdev_id;
	int32_t userspace_probe;
	struct msm_eeprom_memory_block_t cal_data;
	uint8_t is_supported;
};

#endif
