/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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
#ifndef MSM_CAMERA_EEPROM_H
#define MSM_CAMERA_EEPROM_H

#include <linux/delay.h>
#include <mach/camera.h>
#include <media/v4l2-subdev.h>
#include "msm_camera_i2c.h"

#define TRUE  1
#define FALSE 0

struct msm_eeprom_ctrl_t;

struct msm_camera_eeprom_fn_t {
	int32_t (*eeprom_init)
		(struct msm_eeprom_ctrl_t *ectrl,
		struct i2c_adapter *adapter);
	int32_t (*eeprom_release)
		(struct msm_eeprom_ctrl_t *ectrl);
	int32_t (*eeprom_get_info)
		(struct msm_eeprom_ctrl_t *ectrl,
		 struct msm_camera_eeprom_info_t *einfo);
	int32_t (*eeprom_get_data)
		(struct msm_eeprom_ctrl_t *ectrl,
		 struct msm_eeprom_data_t *edata);
	void (*eeprom_set_dev_addr)
		(struct msm_eeprom_ctrl_t*, uint32_t*);
	void (*eeprom_format_data)
		(void);
};

struct msm_camera_eeprom_read_t {
	uint32_t reg_addr;
	void *dest_ptr;
	uint32_t num_byte;
	uint16_t convert_endian;
};

struct msm_camera_eeprom_data_t {
	void *data;
	uint16_t size;
};

struct msm_eeprom_ctrl_t {
	struct msm_camera_i2c_client i2c_client;
	uint16_t i2c_addr;
	struct i2c_driver *i2c_driver;
	struct mutex *eeprom_mutex;
	struct v4l2_subdev sdev;
	struct v4l2_subdev_ops *eeprom_v4l2_subdev_ops;
	struct msm_camera_eeprom_fn_t func_tbl;
	struct msm_camera_eeprom_info_t *info;
	uint16_t info_size;
	struct msm_camera_eeprom_read_t *read_tbl;
	uint16_t read_tbl_size;
	struct msm_camera_eeprom_data_t *data_tbl;
	uint16_t data_tbl_size;
};

int32_t msm_camera_eeprom_get_data(struct msm_eeprom_ctrl_t *ectrl,
	struct msm_eeprom_data_t *edata);
int32_t msm_camera_eeprom_get_info(struct msm_eeprom_ctrl_t *ectrl,
	struct msm_camera_eeprom_info_t *einfo);
int32_t msm_eeprom_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id);
long msm_eeprom_subdev_ioctl(struct v4l2_subdev *sd,
	unsigned int cmd, void *arg);

#define VIDIOC_MSM_EEPROM_CFG \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 12, void __user *)
#endif
