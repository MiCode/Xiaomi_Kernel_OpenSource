/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

#ifndef MSM_CCI_H
#define MSM_CCI_H

#include <linux/clk.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <media/v4l2-subdev.h>
#include <media/msm_cam_sensor.h>
#include <mach/camera2.h>
#include "msm_sd.h"

#define NUM_MASTERS 2
#define NUM_QUEUES 2

#define TRUE  1
#define FALSE 0

enum cci_i2c_queue_t {
	QUEUE_0,
	QUEUE_1,
};

struct msm_camera_cci_client {
	struct v4l2_subdev *cci_subdev;
	uint32_t freq;
	enum cci_i2c_master_t cci_i2c_master;
	uint16_t sid;
	uint16_t cid;
	uint32_t timeout;
	uint16_t retries;
	uint16_t id_map;
};

enum msm_cci_cmd_type {
	MSM_CCI_INIT,
	MSM_CCI_RELEASE,
	MSM_CCI_SET_SID,
	MSM_CCI_SET_FREQ,
	MSM_CCI_SET_SYNC_CID,
	MSM_CCI_I2C_READ,
	MSM_CCI_I2C_WRITE,
	MSM_CCI_GPIO_WRITE,
};

struct msm_camera_cci_wait_sync_cfg {
	uint16_t line;
	uint16_t delay;
};

struct msm_camera_cci_gpio_cfg {
	uint16_t gpio_queue;
	uint16_t i2c_queue;
};

struct msm_camera_cci_i2c_read_cfg {
	uint16_t addr;
	enum msm_camera_i2c_reg_addr_type addr_type;
	uint8_t *data;
	uint16_t num_byte;
};

struct msm_camera_cci_i2c_queue_info {
	uint32_t max_queue_size;
	uint32_t report_id;
	uint32_t irq_en;
	uint32_t capture_rep_data;
};

struct msm_camera_cci_ctrl {
	int32_t status;
	struct msm_camera_cci_client *cci_info;
	enum msm_cci_cmd_type cmd;
	union {
		struct msm_camera_i2c_reg_setting cci_i2c_write_cfg;
		struct msm_camera_cci_i2c_read_cfg cci_i2c_read_cfg;
		struct msm_camera_cci_wait_sync_cfg cci_wait_sync_cfg;
		struct msm_camera_cci_gpio_cfg gpio_cfg;
	} cfg;
};

struct msm_camera_cci_master_info {
	uint32_t status;
	uint8_t reset_pending;
	struct mutex mutex;
	struct completion reset_complete;
};

struct msm_cci_clk_params_t {
	uint16_t hw_thigh;
	uint16_t hw_tlow;
	uint16_t hw_tsu_sto;
	uint16_t hw_tsu_sta;
	uint16_t hw_thd_dat;
	uint16_t hw_thd_sta;
	uint16_t hw_tbuf;
	uint8_t hw_scl_stretch_en;
	uint8_t hw_trdhld;
	uint8_t hw_tsp;
};

enum msm_cci_state_t {
	CCI_STATE_ENABLED,
	CCI_STATE_DISABLED,
};

struct cci_device {
	struct platform_device *pdev;
	struct msm_sd_subdev msm_sd;
	struct v4l2_subdev subdev;
	struct resource *mem;
	struct resource *irq;
	struct resource *io;
	void __iomem *base;

	uint32_t hw_version;
	uint8_t ref_count;
	enum msm_cci_state_t cci_state;

	struct clk *cci_clk[5];
	struct msm_camera_cci_i2c_queue_info
		cci_i2c_queue_info[NUM_MASTERS][NUM_QUEUES];
	struct msm_camera_cci_master_info cci_master_info[NUM_MASTERS];
	struct msm_cci_clk_params_t cci_clk_params[MASTER_MAX];
	struct gpio *cci_gpio_tbl;
	uint8_t cci_gpio_tbl_size;
};

enum msm_cci_i2c_cmd_type {
	CCI_I2C_SET_PARAM_CMD = 1,
	CCI_I2C_WAIT_CMD,
	CCI_I2C_WAIT_SYNC_CMD,
	CCI_I2C_WAIT_GPIO_EVENT_CMD,
	CCI_I2C_TRIG_I2C_EVENT_CMD,
	CCI_I2C_LOCK_CMD,
	CCI_I2C_UNLOCK_CMD,
	CCI_I2C_REPORT_CMD,
	CCI_I2C_WRITE_CMD,
	CCI_I2C_READ_CMD,
	CCI_I2C_WRITE_DISABLE_P_CMD,
	CCI_I2C_READ_DISABLE_P_CMD,
	CCI_I2C_WRITE_CMD2,
	CCI_I2C_WRITE_CMD3,
	CCI_I2C_REPEAT_CMD,
	CCI_I2C_INVALID_CMD,
};

enum msm_cci_gpio_cmd_type {
	CCI_GPIO_SET_PARAM_CMD = 1,
	CCI_GPIO_WAIT_CMD,
	CCI_GPIO_WAIT_SYNC_CMD,
	CCI_GPIO_WAIT_GPIO_IN_EVENT_CMD,
	CCI_GPIO_WAIT_I2C_Q_TRIG_EVENT_CMD,
	CCI_GPIO_OUT_CMD,
	CCI_GPIO_TRIG_EVENT_CMD,
	CCI_GPIO_REPORT_CMD,
	CCI_GPIO_REPEAT_CMD,
	CCI_GPIO_CONTINUE_CMD,
	CCI_GPIO_INVALID_CMD,
};

struct v4l2_subdev *msm_cci_get_subdev(void);

#define VIDIOC_MSM_CCI_CFG \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 23, struct msm_camera_cci_ctrl *)

#endif
