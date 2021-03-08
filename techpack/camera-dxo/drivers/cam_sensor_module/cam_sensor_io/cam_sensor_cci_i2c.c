// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 */

#include "cam_sensor_cmn_header.h"
#include "cam_sensor_i2c.h"
#include "cam_cci_dev.h"

int32_t cam_cci_i2c_read(struct cam_sensor_cci_client *cci_client,
	uint32_t addr, uint32_t *data,
	enum camera_sensor_i2c_type addr_type,
	enum camera_sensor_i2c_type data_type)
{
	int32_t rc = -EINVAL;
	unsigned char buf[CAMERA_SENSOR_I2C_TYPE_DWORD];
	struct cam_cci_ctrl cci_ctrl;

	if (addr_type <= CAMERA_SENSOR_I2C_TYPE_INVALID
		|| addr_type >= CAMERA_SENSOR_I2C_TYPE_MAX
		|| data_type <= CAMERA_SENSOR_I2C_TYPE_INVALID
		|| data_type >= CAMERA_SENSOR_I2C_TYPE_MAX)
		return rc;

	cci_ctrl.cmd = MSM_CCI_I2C_READ;
	cci_ctrl.cci_info = cci_client;
	cci_ctrl.cfg.cci_i2c_read_cfg.addr = addr;
	cci_ctrl.cfg.cci_i2c_read_cfg.addr_type = addr_type;
	cci_ctrl.cfg.cci_i2c_read_cfg.data_type = data_type;
	cci_ctrl.cfg.cci_i2c_read_cfg.data = buf;
	cci_ctrl.cfg.cci_i2c_read_cfg.num_byte = data_type;
	rc = v4l2_subdev_call(cci_client->cci_subdev,
		core, ioctl, VIDIOC_MSM_CCI_CFG, &cci_ctrl);
	if (rc < 0) {
		CAM_ERR(CAM_SENSOR, "rc = %d", rc);
		return rc;
	}

	rc = cci_ctrl.status;
	if (data_type == CAMERA_SENSOR_I2C_TYPE_BYTE)
		*data = buf[0];
	else if (data_type == CAMERA_SENSOR_I2C_TYPE_WORD)
		*data = buf[0] << 8 | buf[1];
	else if (data_type == CAMERA_SENSOR_I2C_TYPE_3B)
		*data = buf[0] << 16 | buf[1] << 8 | buf[2];
	else
		*data = buf[0] << 24 | buf[1] << 16 |
			buf[2] << 8 | buf[3];

	return rc;
}

int32_t cam_camera_cci_i2c_read_seq(struct cam_sensor_cci_client *cci_client,
	uint32_t addr, uint8_t *data,
	enum camera_sensor_i2c_type addr_type,
	enum camera_sensor_i2c_type data_type,
	uint32_t num_byte)
{
	int32_t                    rc = -EFAULT;
	unsigned char             *buf = NULL;
	int                        i = 0;
	struct cam_cci_ctrl        cci_ctrl;

	if ((addr_type >= CAMERA_SENSOR_I2C_TYPE_MAX)
		|| (data_type >= CAMERA_SENSOR_I2C_TYPE_MAX)
		|| (num_byte > I2C_REG_DATA_MAX)) {
		CAM_ERR(CAM_SENSOR, "addr_type %d num_byte %d", addr_type,
			num_byte);
		return rc;
	}

	buf = kzalloc(num_byte, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	cci_ctrl.cmd = MSM_CCI_I2C_READ;
	cci_ctrl.cci_info = cci_client;
	cci_ctrl.cfg.cci_i2c_read_cfg.addr = addr;
	cci_ctrl.cfg.cci_i2c_read_cfg.addr_type = addr_type;
	cci_ctrl.cfg.cci_i2c_read_cfg.data_type = data_type;
	cci_ctrl.cfg.cci_i2c_read_cfg.data = buf;
	cci_ctrl.cfg.cci_i2c_read_cfg.num_byte = num_byte;
	cci_ctrl.status = -EFAULT;
	rc = v4l2_subdev_call(cci_client->cci_subdev,
		core, ioctl, VIDIOC_MSM_CCI_CFG, &cci_ctrl);
	rc = cci_ctrl.status;
	CAM_DBG(CAM_SENSOR, "addr = 0x%x, rc = %d", addr, rc);
	for (i = 0; i < num_byte; i++) {
		data[i] = buf[i];
		CAM_DBG(CAM_SENSOR, "Byte %d: Data: 0x%x\n", i, data[i]);
	}
	kfree(buf);
	return rc;
}

static int32_t cam_cci_i2c_write_table_cmd(
	struct camera_io_master *client,
	struct cam_sensor_i2c_reg_setting *write_setting,
	enum cam_cci_cmd_type cmd)
{
	int32_t rc = -EINVAL;
	struct cam_cci_ctrl cci_ctrl;

	if (!client || !write_setting)
		return rc;

	if (write_setting->addr_type <= CAMERA_SENSOR_I2C_TYPE_INVALID
		|| write_setting->addr_type >= CAMERA_SENSOR_I2C_TYPE_MAX
		|| write_setting->data_type <= CAMERA_SENSOR_I2C_TYPE_INVALID
		|| write_setting->data_type >= CAMERA_SENSOR_I2C_TYPE_MAX)
		return rc;

	cci_ctrl.cmd = cmd;
	cci_ctrl.cci_info = client->cci_client;
	cci_ctrl.cfg.cci_i2c_write_cfg.reg_setting =
		write_setting->reg_setting;
	cci_ctrl.cfg.cci_i2c_write_cfg.data_type = write_setting->data_type;
	cci_ctrl.cfg.cci_i2c_write_cfg.addr_type = write_setting->addr_type;
	cci_ctrl.cfg.cci_i2c_write_cfg.size = write_setting->size;
	rc = v4l2_subdev_call(client->cci_client->cci_subdev,
		core, ioctl, VIDIOC_MSM_CCI_CFG, &cci_ctrl);
	if (rc < 0) {
		CAM_ERR(CAM_SENSOR, "Failed rc = %d", rc);
		return rc;
	}
	rc = cci_ctrl.status;
	if (write_setting->delay > 20)
		msleep(write_setting->delay);
	else if (write_setting->delay)
		usleep_range(write_setting->delay * 1000, (write_setting->delay
			* 1000) + 1000);

	return rc;
}


int32_t cam_cci_i2c_write_table(
	struct camera_io_master *client,
	struct cam_sensor_i2c_reg_setting *write_setting)
{
	return cam_cci_i2c_write_table_cmd(client, write_setting,
		MSM_CCI_I2C_WRITE);
}

int32_t cam_cci_i2c_write_continuous_table(
	struct camera_io_master *client,
	struct cam_sensor_i2c_reg_setting *write_setting,
	uint8_t cam_sensor_i2c_write_flag)
{
	int32_t rc = 0;

	if (cam_sensor_i2c_write_flag == 1)
		rc = cam_cci_i2c_write_table_cmd(client, write_setting,
			MSM_CCI_I2C_WRITE_BURST);
	else if (cam_sensor_i2c_write_flag == 0)
		rc = cam_cci_i2c_write_table_cmd(client, write_setting,
			MSM_CCI_I2C_WRITE_SEQ);

	return rc;
}

static int32_t cam_cci_i2c_compare(struct cam_sensor_cci_client *client,
	uint32_t addr, uint32_t data, uint32_t data_mask,
	enum camera_sensor_i2c_type data_type,
	enum camera_sensor_i2c_type addr_type)
{
	int32_t rc;
	uint32_t reg_data = 0;

	rc = cam_cci_i2c_read(client, addr, &reg_data,
		addr_type, data_type);
	if (rc < 0)
		return rc;
	CAM_DBG(CAM_SENSOR, "addr %04x, %04x,compare data = %d", addr, reg_data, (int16_t)reg_data);

	reg_data = reg_data & 0xFFFFFFFF;
	if (data == (reg_data & ~data_mask))
		return I2C_COMPARE_MATCH;
	return I2C_COMPARE_MISMATCH;
}

int32_t cam_cci_i2c_poll(struct cam_sensor_cci_client *client,
	uint32_t addr, uint32_t data, uint32_t data_mask,
	enum camera_sensor_i2c_type data_type,
	enum camera_sensor_i2c_type addr_type,
	uint32_t delay_ms)
{
	int32_t rc = -EINVAL;
	int32_t i = 0;

	CAM_DBG(CAM_SENSOR, "addr: 0x%x data: 0x%x dt: %d",
		addr, data, data_type);

	if (delay_ms > MAX_POLL_DELAY_MS) {
		CAM_ERR(CAM_SENSOR, "invalid delay = %d max_delay = %d",
			delay_ms, MAX_POLL_DELAY_MS);
		return -EINVAL;
	}
	for (i = 0; i < delay_ms; i++) {
		rc = cam_cci_i2c_compare(client,
			addr, data, data_mask, data_type, addr_type);
		if (!rc)
			return rc;

		usleep_range(1000, 1010);
	}

	/* If rc is 1 then read is successful but poll is failure */
	if (rc == 1)
		CAM_ERR(CAM_SENSOR, "poll failed rc=%d(non-fatal)",	rc);

	if (rc < 0)
		CAM_ERR(CAM_SENSOR, "poll failed rc=%d", rc);

	return rc;
}

int32_t cam_sensor_cci_i2c_util(struct cam_sensor_cci_client *cci_client,
	uint16_t cci_cmd)
{
	int32_t rc = 0;
	struct cam_cci_ctrl cci_ctrl;

	cci_ctrl.cmd = cci_cmd;
	cci_ctrl.cci_info = cci_client;
	rc = v4l2_subdev_call(cci_client->cci_subdev,
		core, ioctl, VIDIOC_MSM_CCI_CFG, &cci_ctrl);
	if (rc < 0) {
		CAM_ERR(CAM_SENSOR, "Failed rc = %d", rc);
		return rc;
	}
	return cci_ctrl.status;
}
