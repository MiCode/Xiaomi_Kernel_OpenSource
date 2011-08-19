/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

#include "msm_camera_i2c.h"

int32_t msm_camera_i2c_rxdata(struct msm_camera_i2c_client *dev_client,
	unsigned char *rxdata, int data_length)
{
	int32_t rc = 0;
	uint16_t saddr = dev_client->client->addr >> 1;
	struct i2c_msg msgs[] = {
		{
			.addr  = saddr,
			.flags = 0,
			.len   = dev_client->addr_type,
			.buf   = rxdata,
		},
		{
			.addr  = saddr,
			.flags = I2C_M_RD,
			.len   = data_length,
			.buf   = rxdata,
		},
	};
	rc = i2c_transfer(dev_client->client->adapter, msgs, 2);
	if (rc < 0)
		S_I2C_DBG("msm_camera_i2c_rxdata failed 0x%x\n", saddr);
	return rc;
}

int32_t msm_camera_i2c_txdata(struct msm_camera_i2c_client *dev_client,
				unsigned char *txdata, int length)
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
		S_I2C_DBG("msm_camera_i2c_txdata faild 0x%x\n", saddr);
	return 0;
}

int32_t msm_camera_i2c_write(struct msm_camera_i2c_client *client,
	uint16_t addr, uint16_t data,
	enum msm_camera_i2c_data_type data_type)
{
	int32_t rc = -EFAULT;
	unsigned char buf[client->addr_type+data_type];
	uint8_t len = 0;
	if ((client->addr_type != MSM_CAMERA_I2C_BYTE_ADDR
		&& client->addr_type != MSM_CAMERA_I2C_WORD_ADDR)
		|| (data_type != MSM_CAMERA_I2C_BYTE_DATA
		&& data_type != MSM_CAMERA_I2C_WORD_DATA))
		return rc;

	S_I2C_DBG("%s reg addr = 0x%x data type: %d\n",
			  __func__, addr, data_type);
	if (client->addr_type == MSM_CAMERA_I2C_BYTE_ADDR) {
		buf[0] = addr;
		S_I2C_DBG("%s byte %d: 0x%x\n", __func__, len, buf[len]);
		len = 1;
	} else if (client->addr_type == MSM_CAMERA_I2C_WORD_ADDR) {
		buf[0] = addr >> BITS_PER_BYTE;
		buf[1] = addr;
		S_I2C_DBG("%s byte %d: 0x%x\n", __func__, len, buf[len]);
		S_I2C_DBG("%s byte %d: 0x%x\n", __func__, len+1, buf[len+1]);
		len = 2;
	}
	S_I2C_DBG("Data: 0x%x\n", data);
	if (data_type == MSM_CAMERA_I2C_BYTE_DATA) {
		buf[len] = data;
		S_I2C_DBG("Byte %d: 0x%x\n", len, buf[len]);
		len += 1;
	} else if (data_type == MSM_CAMERA_I2C_WORD_DATA) {
		buf[len] = data >> BITS_PER_BYTE;
		buf[len+1] = data;
		S_I2C_DBG("Byte %d: 0x%x\n", len, buf[len]);
		S_I2C_DBG("Byte %d: 0x%x\n", len+1, buf[len+1]);
		len += 2;
	}

	rc = msm_camera_i2c_txdata(client, buf, len);
	if (rc < 0)
		S_I2C_DBG("%s fail\n", __func__);
	return rc;
}

int32_t msm_camera_i2c_write_seq(struct msm_camera_i2c_client *client,
	uint16_t addr, uint8_t *data, uint16_t num_byte)
{
	int32_t rc = -EFAULT;
	unsigned char buf[client->addr_type+num_byte];
	uint8_t len = 0, i = 0;

	if ((client->addr_type != MSM_CAMERA_I2C_BYTE_ADDR
		&& client->addr_type != MSM_CAMERA_I2C_WORD_ADDR)
		|| num_byte == 0)
		return rc;

	S_I2C_DBG("%s reg addr = 0x%x num bytes: %d\n",
			  __func__, addr, num_byte);
	if (client->addr_type == MSM_CAMERA_I2C_BYTE_ADDR) {
		buf[0] = addr;
		S_I2C_DBG("%s byte %d: 0x%x\n", __func__, len, buf[len]);
		len = 1;
	} else if (client->addr_type == MSM_CAMERA_I2C_WORD_ADDR) {
		buf[0] = addr >> BITS_PER_BYTE;
		buf[1] = addr;
		S_I2C_DBG("%s byte %d: 0x%x\n", __func__, len, buf[len]);
		S_I2C_DBG("%s byte %d: 0x%x\n", __func__, len+1, buf[len+1]);
		len = 2;
	}
	for (i = 0; i < num_byte; i++) {
		buf[i+len] = data[i];
		S_I2C_DBG("Byte %d: 0x%x\n", i+len, buf[i+len]);
		S_I2C_DBG("Data: 0x%x\n", data[i]);
	}

	rc = msm_camera_i2c_txdata(client, buf, len+num_byte);
	if (rc < 0)
		S_I2C_DBG("%s fail\n", __func__);
	return rc;
}

int32_t msm_camera_i2c_write_tbl(struct msm_camera_i2c_client *client,
	struct msm_camera_i2c_reg_conf *reg_conf_tbl, uint8_t size,
	enum msm_camera_i2c_data_type data_type)
{
	int i;
	int32_t rc = -EFAULT;
	for (i = 0; i < size; i++) {
		rc = msm_camera_i2c_write(
			client,
			reg_conf_tbl->reg_addr,
			reg_conf_tbl->reg_data, data_type);
		if (rc < 0)
			break;
		reg_conf_tbl++;
	}
	return rc;
}

int32_t msm_camera_i2c_write_dt_tbl(struct msm_camera_i2c_client *client,
	struct msm_camera_i2c_reg_dt_conf *reg_conf_tbl, uint16_t size)
{
	int i;
	int32_t rc = -EFAULT;
	for (i = 0; i < size; i++) {
		rc = msm_camera_i2c_write(
			client,
			reg_conf_tbl->reg_addr,
			reg_conf_tbl->reg_data, reg_conf_tbl->dt);
		if (rc < 0)
			break;
		reg_conf_tbl++;
	}
	return rc;
}

int32_t msm_camera_i2c_read(struct msm_camera_i2c_client *client,
	uint16_t addr, uint16_t *data,
	enum msm_camera_i2c_data_type data_type)
{
	int32_t rc = -EFAULT;
	unsigned char buf[client->addr_type+data_type];

	if ((client->addr_type != MSM_CAMERA_I2C_BYTE_ADDR
		&& client->addr_type != MSM_CAMERA_I2C_WORD_ADDR)
		|| (data_type != MSM_CAMERA_I2C_BYTE_DATA
		&& data_type != MSM_CAMERA_I2C_WORD_DATA))
		return rc;

	if (client->addr_type == MSM_CAMERA_I2C_BYTE_ADDR) {
		buf[0] = addr;
	} else if (client->addr_type == MSM_CAMERA_I2C_WORD_ADDR) {
		buf[0] = addr >> BITS_PER_BYTE;
		buf[1] = addr;
	}
	rc = msm_camera_i2c_rxdata(client, buf, data_type);
	if (rc < 0) {
		S_I2C_DBG("%s fail\n", __func__);
		return rc;
	}
	if (data_type == MSM_CAMERA_I2C_BYTE_DATA)
		*data = buf[0];
	else
		*data = buf[0] << 8 | buf[1];

	S_I2C_DBG("%s addr = 0x%x data: 0x%x", __func__, addr, *data);
	return rc;
}

int32_t msm_camera_i2c_read_seq(struct msm_camera_i2c_client *client,
	uint16_t addr, uint8_t *data, uint16_t num_byte)
{
	int32_t rc = -EFAULT;
	unsigned char buf[client->addr_type+num_byte];
	int i;

	if ((client->addr_type != MSM_CAMERA_I2C_BYTE_ADDR
		&& client->addr_type != MSM_CAMERA_I2C_WORD_ADDR)
		|| num_byte == 0)
		return rc;

	if (client->addr_type == MSM_CAMERA_I2C_BYTE_ADDR) {
		buf[0] = addr;
	} else if (client->addr_type == MSM_CAMERA_I2C_WORD_ADDR) {
		buf[0] = addr >> BITS_PER_BYTE;
		buf[1] = addr;
	}
	rc = msm_camera_i2c_rxdata(client, buf, num_byte);
	if (rc < 0) {
		S_I2C_DBG("%s fail\n", __func__);
		return rc;
	}

	S_I2C_DBG("%s addr = 0x%x", __func__, addr);
	for (i = 0; i < num_byte; i++) {
		data[i] = buf[i];
		S_I2C_DBG("Byte %d: 0x%x\n", i, buf[i]);
		S_I2C_DBG("Data: 0x%x\n", data[i]);
	}
	return rc;
}

int32_t msm_sensor_write_conf_array(struct msm_camera_i2c_client *client,
			struct msm_camera_i2c_conf_array *array, uint16_t index)
{
	int32_t rc;
	if (array[index].data_type != MSM_CAMERA_I2C_HYBRID_DATA) {
		rc = msm_camera_i2c_write_tbl(client,
			(struct msm_camera_i2c_reg_conf *) array[index].conf,
			array[index].size, array[index].data_type);
	} else {
		rc = msm_camera_i2c_write_dt_tbl(client,
			(struct msm_camera_i2c_reg_dt_conf *) array[index].conf,
			array[index].size);
	}
	if (array[index].delay > 20)
		msleep(array[index].delay);
	else
		usleep_range(array[index].delay*1000,
					(array[index].delay+1)*1000);
	return rc;
}

int32_t msm_sensor_write_all_conf_array(struct msm_camera_i2c_client *client,
			struct msm_camera_i2c_conf_array *array, uint16_t size)
{
	int32_t rc = 0, i;
	for (i = 0; i < size; i++) {
		rc = msm_sensor_write_conf_array(client, array, i);
		if (rc < 0)
			break;
	}
	return rc;
}


