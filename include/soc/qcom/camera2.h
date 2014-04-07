/* Copyright (c) 2011-2014, The Linux Foundation. All rights reserved.
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

#ifndef __CAMERA2_H__
#define __CAMERA2_H__

#include <media/msm_cam_sensor.h>
#include <mach/board.h>

enum msm_camera_device_type_t {
	MSM_CAMERA_I2C_DEVICE,
	MSM_CAMERA_PLATFORM_DEVICE,
	MSM_CAMERA_SPI_DEVICE,
};

enum msm_bus_perf_setting {
	S_INIT,
	S_PREVIEW,
	S_VIDEO,
	S_CAPTURE,
	S_ZSL,
	S_STEREO_VIDEO,
	S_STEREO_CAPTURE,
	S_DEFAULT,
	S_LIVESHOT,
	S_DUAL,
	S_EXIT
};

struct msm_camera_slave_info {
	uint16_t sensor_slave_addr;
	uint16_t sensor_id_reg_addr;
	uint16_t sensor_id;
};

struct msm_cam_clk_info {
	const char *clk_name;
	long clk_rate;
	uint32_t delay;
};

struct msm_pinctrl_info {
	struct pinctrl *pinctrl;
	struct pinctrl_state *gpio_state_active;
	struct pinctrl_state *gpio_state_suspend;
	bool use_pinctrl;
};

struct msm_cam_clk_setting {
	struct msm_cam_clk_info *clk_info;
	uint16_t num_clk_info;
	uint8_t enable;
};

struct v4l2_subdev_info {
	enum v4l2_mbus_pixelcode code;
	enum v4l2_colorspace colorspace;
	uint16_t fmt;
	uint16_t order;
};

struct msm_camera_power_ctrl_t {
	struct device *dev;
	struct msm_sensor_power_setting *power_setting;
	uint16_t power_setting_size;
	struct msm_sensor_power_setting *power_down_setting;
	uint16_t power_down_setting_size;
	struct msm_camera_gpio_conf *gpio_conf;
	struct camera_vreg_t *cam_vreg;
	int num_vreg;
	struct msm_camera_i2c_conf *i2c_conf;
	struct msm_cam_clk_info *clk_info;
	struct msm_pinctrl_info pinctrl_info;
	uint8_t cam_pinctrl_status;
	uint16_t clk_info_size;
};

struct msm_camera_sensor_board_info {
	const char *sensor_name;
	const char *eeprom_name;
	const char *actuator_name;
	struct msm_camera_slave_info *slave_info;
	struct msm_camera_csi_lane_params *csi_lane_params;
	struct msm_camera_sensor_strobe_flash_data *strobe_flash_data;
	struct msm_actuator_info *actuator_info;
	struct msm_sensor_info_t *sensor_info;
	const char *misc_regulator;
	struct msm_camera_power_ctrl_t power_info;
	struct msm_camera_sensor_slave_info *cam_slave_info;
};

enum msm_camera_i2c_cmd_type {
	MSM_CAMERA_I2C_CMD_WRITE,
	MSM_CAMERA_I2C_CMD_POLL,
};

struct msm_camera_i2c_reg_conf {
	uint16_t reg_addr;
	uint16_t reg_data;
	enum msm_camera_i2c_data_type dt;
	enum msm_camera_i2c_cmd_type cmd_type;
	int16_t mask;
};

struct msm_camera_i2c_conf_array {
	struct msm_camera_i2c_reg_conf *conf;
	uint16_t size;
	uint16_t delay;
	enum msm_camera_i2c_data_type data_type;
};

struct eeprom_map_t {
	uint32_t valid_size;
	uint32_t addr;
	uint32_t addr_t;
	uint32_t data;
	uint32_t data_t;
	uint32_t delay;
};

struct eeprom_slave_add_t {
	uint32_t addr;
};

struct msm_eeprom_memory_map_t {
	struct eeprom_map_t page;
	struct eeprom_map_t pageen;
	struct eeprom_map_t poll;
	struct eeprom_map_t mem;
	struct eeprom_slave_add_t saddr;
};

struct msm_eeprom_memory_block_t {
	struct msm_eeprom_memory_map_t *map;
	uint32_t num_map;	/* number of map blocks */
	uint8_t *mapdata;
	uint32_t num_data;	/* size of total mapdata */
};

struct msm_eeprom_cmm_t {
	uint32_t cmm_support;
	uint32_t cmm_compression;
	uint32_t cmm_offset;
	uint32_t cmm_size;
};

struct msm_eeprom_board_info {
	const char *eeprom_name;
	uint16_t i2c_slaveaddr;
	struct msm_camera_power_ctrl_t power_info;
	struct msm_eeprom_cmm_t cmm_data;
};

#endif
