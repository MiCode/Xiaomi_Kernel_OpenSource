// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 */

#include "cam_sensor_cmn_header.h"
#include "cam_sensor_i2c.h"
#include "cam_sensor_io.h"

#define I2C_REG_MAX_BUF_SIZE   8

static int32_t cam_qup_i2c_rxdata(
	struct i2c_client *dev_client, unsigned char *rxdata,
	enum camera_sensor_i2c_type addr_type,
	int data_length)
{
	int32_t rc = 0;
	uint16_t saddr = dev_client->addr >> 1;
	struct i2c_msg msgs[] = {
		{
			.addr  = saddr,
			.flags = 0,
			.len   = addr_type,
			.buf   = rxdata,
		},
		{
			.addr  = saddr,
			.flags = I2C_M_RD,
			.len   = data_length,
			.buf   = rxdata,
		},
	};
	rc = i2c_transfer(dev_client->adapter, msgs, 2);
	if (rc < 0)
		CAM_ERR(CAM_SENSOR, "failed 0x%x", saddr);
	return rc;
}


static int32_t cam_qup_i2c_txdata(
	struct camera_io_master *dev_client, unsigned char *txdata,
	int length)
{
	int32_t rc = 0;
	uint16_t saddr = dev_client->client->addr >> 1;
	struct i2c_msg msg[] = {
		{
			.addr = saddr,
			.flags = 0,
			.len = length,
			.buf = txdata,
		 },
	};
	rc = i2c_transfer(dev_client->client->adapter, msg, 1);
	if (rc < 0)
		CAM_ERR(CAM_SENSOR, "failed 0x%x", saddr);
	return rc;
}

int32_t cam_qup_i2c_read(struct i2c_client *client,
	uint32_t addr, uint32_t *data,
	enum camera_sensor_i2c_type addr_type,
	enum camera_sensor_i2c_type data_type)
{
	int32_t rc = -EINVAL;
	unsigned char *buf = NULL;

	if (addr_type <= CAMERA_SENSOR_I2C_TYPE_INVALID
		|| addr_type >= CAMERA_SENSOR_I2C_TYPE_MAX
		|| data_type <= CAMERA_SENSOR_I2C_TYPE_INVALID
		|| data_type >= CAMERA_SENSOR_I2C_TYPE_MAX) {
		CAM_ERR(CAM_SENSOR, "Failed with addr/data_type verfication");
		return rc;
	}

	buf = kzalloc(addr_type + data_type, GFP_KERNEL);

	if (!buf)
		return -ENOMEM;

	if (addr_type == CAMERA_SENSOR_I2C_TYPE_BYTE) {
		buf[0] = addr;
	} else if (addr_type == CAMERA_SENSOR_I2C_TYPE_WORD) {
		buf[0] = addr >> 8;
		buf[1] = addr;
	} else if (addr_type == CAMERA_SENSOR_I2C_TYPE_3B) {
		buf[0] = addr >> 16;
		buf[1] = addr >> 8;
		buf[2] = addr;
	} else {
		buf[0] = addr >> 24;
		buf[1] = addr >> 16;
		buf[2] = addr >> 8;
		buf[3] = addr;
	}

	rc = cam_qup_i2c_rxdata(client, buf, addr_type, data_type);
	if (rc < 0) {
		CAM_ERR(CAM_SENSOR, "failed rc: %d", rc);
		goto read_fail;
	}

	if (data_type == CAMERA_SENSOR_I2C_TYPE_BYTE)
		*data = buf[0];
	else if (data_type == CAMERA_SENSOR_I2C_TYPE_WORD)
		*data = buf[0] << 8 | buf[1];
	else if (data_type == CAMERA_SENSOR_I2C_TYPE_3B)
		*data = buf[0] << 16 | buf[1] << 8 | buf[2];
	else
		*data = buf[0] << 24 | buf[1] << 16 |
			buf[2] << 8 | buf[3];

	CAM_DBG(CAM_SENSOR, "addr = 0x%x data: 0x%x", addr, *data);
read_fail:
	kfree(buf);
	buf = NULL;
	return rc;
}

int32_t cam_qup_i2c_read_seq(struct i2c_client *client,
	uint32_t addr, uint8_t *data,
	enum camera_sensor_i2c_type addr_type,
	uint32_t num_byte)
{
	int32_t rc = -EFAULT;
	unsigned char *buf = NULL;
	int i;

	if (addr_type <= CAMERA_SENSOR_I2C_TYPE_INVALID
		|| addr_type >= CAMERA_SENSOR_I2C_TYPE_MAX) {
		CAM_ERR(CAM_SENSOR, "Failed with addr_type verification");
		return rc;
	}

	if ((num_byte == 0) || (num_byte > I2C_REG_DATA_MAX)) {
		CAM_ERR(CAM_SENSOR, "num_byte:0x%x max supported:0x%x",
			num_byte, I2C_REG_DATA_MAX);
		return rc;
	}

	buf = kzalloc(addr_type + num_byte, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	if (addr_type == CAMERA_SENSOR_I2C_TYPE_BYTE) {
		buf[0] = addr;
	} else if (addr_type == CAMERA_SENSOR_I2C_TYPE_WORD) {
		buf[0] = addr >> BITS_PER_BYTE;
		buf[1] = addr;
	} else if (addr_type == CAMERA_SENSOR_I2C_TYPE_3B) {
		buf[0] = addr >> 16;
		buf[1] = addr >> 8;
		buf[2] = addr;
	} else {
		buf[0] = addr >> 24;
		buf[1] = addr >> 16;
		buf[2] = addr >> 8;
		buf[3] = addr;
	}

	rc = cam_qup_i2c_rxdata(client, buf, addr_type, num_byte);
	if (rc < 0) {
		CAM_ERR(CAM_SENSOR, "failed rc: %d", rc);
		goto read_seq_fail;
	}

	for (i = 0; i < num_byte; i++)
		data[i] = buf[i];

read_seq_fail:
	kfree(buf);
	buf = NULL;
	return rc;
}

static int32_t cam_qup_i2c_compare(struct i2c_client *client,
	uint32_t addr, uint32_t data, uint16_t data_mask,
	enum camera_sensor_i2c_type data_type,
	enum camera_sensor_i2c_type addr_type)
{
	int32_t rc;
	uint32_t reg_data = 0;

	rc = cam_qup_i2c_read(client, addr, &reg_data,
		addr_type, data_type);
	if (rc < 0)
		return rc;

	reg_data = reg_data & 0xFFFF;
	if (data != (reg_data & ~data_mask))
		return I2C_COMPARE_MISMATCH;

	return I2C_COMPARE_MATCH;
}

int32_t cam_qup_i2c_poll(struct i2c_client *client,
	uint32_t addr, uint16_t data, uint16_t data_mask,
	enum camera_sensor_i2c_type addr_type,
	enum camera_sensor_i2c_type data_type,
	uint32_t delay_ms)
{
	int32_t rc = 0;
	int i = 0;

	if ((delay_ms > MAX_POLL_DELAY_MS) || (delay_ms == 0)) {
		CAM_ERR(CAM_SENSOR, "invalid delay = %d max_delay = %d",
			delay_ms, MAX_POLL_DELAY_MS);
		return -EINVAL;
	}

	if ((addr_type <= CAMERA_SENSOR_I2C_TYPE_INVALID
		|| addr_type >= CAMERA_SENSOR_I2C_TYPE_MAX
		|| data_type <= CAMERA_SENSOR_I2C_TYPE_INVALID
		|| data_type >= CAMERA_SENSOR_I2C_TYPE_MAX))
		return -EINVAL;

	for (i = 0; i < delay_ms; i++) {
		rc = cam_qup_i2c_compare(client,
			addr, data, data_mask, data_type, addr_type);
		if (rc == I2C_COMPARE_MATCH)
			return rc;

		usleep_range(1000, 1010);
	}
	/* If rc is MISMATCH then read is successful but poll is failure */
	if (rc == I2C_COMPARE_MISMATCH)
		CAM_ERR(CAM_SENSOR, "poll failed rc=%d(non-fatal)", rc);
	if (rc < 0)
		CAM_ERR(CAM_SENSOR, "poll failed rc=%d", rc);

	return rc;
}

static int32_t cam_qup_i2c_write(struct camera_io_master *client,
	struct cam_sensor_i2c_reg_array *reg_setting,
	enum camera_sensor_i2c_type addr_type,
	enum camera_sensor_i2c_type data_type)
{
	int32_t rc = 0;
	unsigned char *buf = NULL;
	uint8_t len = 0;

	buf = kzalloc(I2C_REG_MAX_BUF_SIZE, GFP_KERNEL | GFP_DMA);
	if (!buf) {
		CAM_ERR(CAM_SENSOR, "Buffer memory allocation failed");
		return -ENOMEM;
	}

	CAM_DBG(CAM_SENSOR, "reg addr = 0x%x data type: %d",
			reg_setting->reg_addr, data_type);
	if (addr_type == CAMERA_SENSOR_I2C_TYPE_BYTE) {
		buf[0] = reg_setting->reg_addr;
		CAM_DBG(CAM_SENSOR, "byte %d: 0x%x", len, buf[len]);
		len = 1;
	} else if (addr_type == CAMERA_SENSOR_I2C_TYPE_WORD) {
		buf[0] = reg_setting->reg_addr >> 8;
		buf[1] = reg_setting->reg_addr;
		CAM_DBG(CAM_SENSOR, "byte %d: 0x%x", len, buf[len]);
		CAM_DBG(CAM_SENSOR, "byte %d: 0x%x", len+1, buf[len+1]);
		len = 2;
	} else if (addr_type == CAMERA_SENSOR_I2C_TYPE_3B) {
		buf[0] = reg_setting->reg_addr >> 16;
		buf[1] = reg_setting->reg_addr >> 8;
		buf[2] = reg_setting->reg_addr;
		len = 3;
	} else if (addr_type == CAMERA_SENSOR_I2C_TYPE_DWORD) {
		buf[0] = reg_setting->reg_addr >> 24;
		buf[1] = reg_setting->reg_addr >> 16;
		buf[2] = reg_setting->reg_addr >> 8;
		buf[3] = reg_setting->reg_addr;
		len = 4;
	} else {
		CAM_ERR(CAM_SENSOR, "Invalid I2C addr type");
		rc = -EINVAL;
		goto deallocate_buffer;
	}

	CAM_DBG(CAM_SENSOR, "Data: 0x%x", reg_setting->reg_data);
	if (data_type == CAMERA_SENSOR_I2C_TYPE_BYTE) {
		buf[len] = reg_setting->reg_data;
		CAM_DBG(CAM_SENSOR, "Byte %d: 0x%x", len, buf[len]);
		len += 1;
	} else if (data_type == CAMERA_SENSOR_I2C_TYPE_WORD) {
		buf[len] = reg_setting->reg_data >> 8;
		buf[len+1] = reg_setting->reg_data;
		CAM_DBG(CAM_SENSOR, "Byte %d: 0x%x", len, buf[len]);
		CAM_DBG(CAM_SENSOR, "Byte %d: 0x%x", len+1, buf[len+1]);
		len += 2;
	} else if (data_type == CAMERA_SENSOR_I2C_TYPE_3B) {
		buf[len] = reg_setting->reg_data >> 16;
		buf[len + 1] = reg_setting->reg_data >> 8;
		buf[len + 2] = reg_setting->reg_data;
		CAM_DBG(CAM_SENSOR, "Byte %d: 0x%x", len, buf[len]);
		CAM_DBG(CAM_SENSOR, "Byte %d: 0x%x", len+1, buf[len+1]);
		CAM_DBG(CAM_SENSOR, "Byte %d: 0x%x", len+2, buf[len+2]);
		len += 3;
	} else if (data_type == CAMERA_SENSOR_I2C_TYPE_DWORD) {
		buf[len] = reg_setting->reg_data >> 24;
		buf[len + 1] = reg_setting->reg_data >> 16;
		buf[len + 2] = reg_setting->reg_data >> 8;
		buf[len + 3] = reg_setting->reg_data;
		CAM_DBG(CAM_SENSOR, "Byte %d: 0x%x", len, buf[len]);
		CAM_DBG(CAM_SENSOR, "Byte %d: 0x%x", len+1, buf[len+1]);
		CAM_DBG(CAM_SENSOR, "Byte %d: 0x%x", len+2, buf[len+2]);
		CAM_DBG(CAM_SENSOR, "Byte %d: 0x%x", len+3, buf[len+3]);
		len += 4;
	} else {
		CAM_ERR(CAM_SENSOR, "Invalid Data Type");
		rc = -EINVAL;
		goto deallocate_buffer;
	}

	rc = cam_qup_i2c_txdata(client, buf, len);
	if (rc < 0)
		CAM_ERR(CAM_SENSOR, "failed rc: %d", rc);

deallocate_buffer:
	kfree(buf);
	return rc;
}

int32_t cam_qup_i2c_write_table(struct camera_io_master *client,
	struct cam_sensor_i2c_reg_setting *write_setting)
{
	int i;
	int32_t rc = -EINVAL;
	struct cam_sensor_i2c_reg_array *reg_setting;

	if (!client || !write_setting)
		return rc;

	if ((write_setting->addr_type <= CAMERA_SENSOR_I2C_TYPE_INVALID
		|| write_setting->addr_type >= CAMERA_SENSOR_I2C_TYPE_MAX
		|| (write_setting->data_type <= CAMERA_SENSOR_I2C_TYPE_INVALID
		|| write_setting->data_type >= CAMERA_SENSOR_I2C_TYPE_MAX)))
		return rc;

	reg_setting = write_setting->reg_setting;

	for (i = 0; i < write_setting->size; i++) {
		CAM_DBG(CAM_SENSOR, "addr 0x%x data 0x%x",
			reg_setting->reg_addr, reg_setting->reg_data);

		rc = cam_qup_i2c_write(client, reg_setting,
			write_setting->addr_type, write_setting->data_type);
		if (rc < 0)
			break;
		reg_setting++;
	}

	if (write_setting->delay > 20)
		msleep(write_setting->delay);
	else if (write_setting->delay)
		usleep_range(write_setting->delay * 1000, (write_setting->delay
			* 1000) + 1000);

	return rc;
}

static int32_t cam_qup_i2c_write_seq(struct camera_io_master *client,
	struct cam_sensor_i2c_reg_setting *write_setting)
{
	int i;
	int32_t rc = 0;
	struct cam_sensor_i2c_reg_array *reg_setting;

	reg_setting = write_setting->reg_setting;

	for (i = 0; i < write_setting->size; i++) {
		reg_setting->reg_addr += i;
		rc = cam_qup_i2c_write(client, reg_setting,
			write_setting->addr_type, write_setting->data_type);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR,
				"Sequential i2c write failed: rc: %d", rc);
			break;
		}
		reg_setting++;
	}

	if (write_setting->delay > 20)
		msleep(write_setting->delay);
	else if (write_setting->delay)
		usleep_range(write_setting->delay * 1000, (write_setting->delay
			* 1000) + 1000);

	return rc;
}

static int32_t cam_qup_i2c_write_burst(struct camera_io_master *client,
	struct cam_sensor_i2c_reg_setting *write_setting)
{
	int i;
	int32_t rc = 0;
	uint32_t len = 0;
	unsigned char *buf = NULL;
	struct cam_sensor_i2c_reg_array *reg_setting;
	enum camera_sensor_i2c_type addr_type;
	enum camera_sensor_i2c_type data_type;

	buf = kzalloc((write_setting->addr_type +
			(write_setting->size * write_setting->data_type)),
			GFP_KERNEL);

	if (!buf) {
		CAM_ERR(CAM_SENSOR, "BUF is NULL");
		return -ENOMEM;
	}

	reg_setting = write_setting->reg_setting;
	addr_type = write_setting->addr_type;
	data_type = write_setting->data_type;

	CAM_DBG(CAM_SENSOR, "reg addr = 0x%x data type: %d",
			reg_setting->reg_addr, data_type);
	if (addr_type == CAMERA_SENSOR_I2C_TYPE_BYTE) {
		buf[0] = reg_setting->reg_addr;
		CAM_DBG(CAM_SENSOR, "byte %d: 0x%x", len, buf[len]);
		len = 1;
	} else if (addr_type == CAMERA_SENSOR_I2C_TYPE_WORD) {
		buf[0] = reg_setting->reg_addr >> 8;
		buf[1] = reg_setting->reg_addr;
		CAM_DBG(CAM_SENSOR, "byte %d: 0x%x", len, buf[len]);
		CAM_DBG(CAM_SENSOR, "byte %d: 0x%x", len+1, buf[len+1]);
		len = 2;
	} else if (addr_type == CAMERA_SENSOR_I2C_TYPE_3B) {
		buf[0] = reg_setting->reg_addr >> 16;
		buf[1] = reg_setting->reg_addr >> 8;
		buf[2] = reg_setting->reg_addr;
		len = 3;
	} else if (addr_type == CAMERA_SENSOR_I2C_TYPE_DWORD) {
		buf[0] = reg_setting->reg_addr >> 24;
		buf[1] = reg_setting->reg_addr >> 16;
		buf[2] = reg_setting->reg_addr >> 8;
		buf[3] = reg_setting->reg_addr;
		len = 4;
	} else {
		CAM_ERR(CAM_SENSOR, "Invalid I2C addr type");
		rc = -EINVAL;
		goto free_res;
	}

	for (i = 0; i < write_setting->size; i++) {
		if (data_type == CAMERA_SENSOR_I2C_TYPE_BYTE) {
			buf[len] = reg_setting->reg_data;
			CAM_DBG(CAM_SENSOR,
				"Byte %d: 0x%x", len, buf[len]);
			len += 1;
		} else if (data_type == CAMERA_SENSOR_I2C_TYPE_WORD) {
			buf[len] = reg_setting->reg_data >> 8;
			buf[len+1] = reg_setting->reg_data;
			CAM_DBG(CAM_SENSOR,
				"Byte %d: 0x%x", len, buf[len]);
			CAM_DBG(CAM_SENSOR,
				"Byte %d: 0x%x", len+1, buf[len+1]);
			len += 2;
		} else if (data_type == CAMERA_SENSOR_I2C_TYPE_3B) {
			buf[len] = reg_setting->reg_data >> 16;
			buf[len + 1] = reg_setting->reg_data >> 8;
			buf[len + 2] = reg_setting->reg_data;
			CAM_DBG(CAM_SENSOR,
				"Byte %d: 0x%x", len, buf[len]);
			CAM_DBG(CAM_SENSOR,
				"Byte %d: 0x%x", len+1, buf[len+1]);
			CAM_DBG(CAM_SENSOR,
				"Byte %d: 0x%x", len+2, buf[len+2]);
			len += 3;
		} else if (data_type == CAMERA_SENSOR_I2C_TYPE_DWORD) {
			buf[len] = reg_setting->reg_data >> 24;
			buf[len + 1] = reg_setting->reg_data >> 16;
			buf[len + 2] = reg_setting->reg_data >> 8;
			buf[len + 3] = reg_setting->reg_data;
			CAM_DBG(CAM_SENSOR,
				"Byte %d: 0x%x", len, buf[len]);
			CAM_DBG(CAM_SENSOR,
				"Byte %d: 0x%x", len+1, buf[len+1]);
			CAM_DBG(CAM_SENSOR,
				"Byte %d: 0x%x", len+2, buf[len+2]);
			CAM_DBG(CAM_SENSOR,
				"Byte %d: 0x%x", len+3, buf[len+3]);
			len += 4;
		} else {
			CAM_ERR(CAM_SENSOR, "Invalid Data Type");
			rc = -EINVAL;
			goto free_res;
		}
		reg_setting++;
	}

	if (len > (write_setting->addr_type +
		(write_setting->size * write_setting->data_type))) {
		CAM_ERR(CAM_SENSOR, "Invalid Length: %u | Expected length: %u",
			len, (write_setting->addr_type +
			(write_setting->size * write_setting->data_type)));
		rc = -EINVAL;
		goto free_res;
	}

	rc = cam_qup_i2c_txdata(client, buf, len);
	if (rc < 0)
		CAM_ERR(CAM_SENSOR, "failed rc: %d", rc);

free_res:
	kfree(buf);
	return rc;
}

int32_t cam_qup_i2c_write_continuous_table(struct camera_io_master *client,
	struct cam_sensor_i2c_reg_setting *write_settings,
	uint8_t cam_sensor_i2c_write_flag)
{
	int32_t rc = 0;

	if (!client || !write_settings)
		return -EINVAL;

	if ((write_settings->addr_type <= CAMERA_SENSOR_I2C_TYPE_INVALID
		|| write_settings->addr_type >= CAMERA_SENSOR_I2C_TYPE_MAX
		|| (write_settings->data_type <= CAMERA_SENSOR_I2C_TYPE_INVALID
		|| write_settings->data_type >= CAMERA_SENSOR_I2C_TYPE_MAX)))
		return -EINVAL;

	if (cam_sensor_i2c_write_flag == CAM_SENSOR_I2C_WRITE_BURST)
		rc = cam_qup_i2c_write_burst(client, write_settings);
	else if (cam_sensor_i2c_write_flag == CAM_SENSOR_I2C_WRITE_SEQ)
		rc = cam_qup_i2c_write_seq(client, write_settings);

	return rc;
}
