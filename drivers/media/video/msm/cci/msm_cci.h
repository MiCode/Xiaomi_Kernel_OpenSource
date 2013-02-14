/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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
#include <media/v4l2-subdev.h>
#include <mach/camera.h>

#define NUM_MASTERS 2
#define NUM_QUEUES 2

#define TRUE  1
#define FALSE 0

struct msm_camera_cci_master_info {
	uint32_t status;
	uint8_t reset_pending;
	struct mutex mutex;
	struct completion reset_complete;
};

struct cci_device {
	struct platform_device *pdev;
	struct v4l2_subdev subdev;
	struct resource *mem;
	struct resource *irq;
	struct resource *io;
	void __iomem *base;
	uint32_t hw_version;
	struct clk *cci_clk[5];
	struct msm_camera_cci_i2c_queue_info
		cci_i2c_queue_info[NUM_MASTERS][NUM_QUEUES];
	struct msm_camera_cci_master_info cci_master_info[NUM_MASTERS];
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

#define VIDIOC_MSM_CCI_CFG \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 23, struct msm_camera_cci_ctrl *)

#endif

