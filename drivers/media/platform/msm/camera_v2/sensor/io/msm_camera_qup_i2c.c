/* Copyright (c) 2011, 2013-2014,2016 The Linux Foundation. All rights reserved.
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

#include <soc/qcom/camera2.h>
#include "msm_camera_i2c.h"

#undef CDBG
#ifdef CONFIG_MSMB_CAMERA_DEBUG
#define CDBG(fmt, args...) pr_debug(fmt, ##args)
#define S_I2C_DBG(fmt, args...) pr_debug(fmt, ##args)
#else
#define CDBG(fmt, args...) do { } while (0)
#define S_I2C_DBG(fmt, args...) do { } while (0)
#endif

#define I2C_COMPARE_MATCH 0
#define I2C_COMPARE_MISMATCH 1
#define I2C_POLL_MAX_ITERATION 20

static int32_t msm_camera_qup_i2c_rxdata(
	struct msm_camera_i2c_client *dev_client, unsigned char *rxdata,
	int data_length)
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
		S_I2C_DBG("msm_camera_qup_i2c_rxdata failed 0x%x\n", saddr);
	return rc;
}

static int32_t msm_camera_qup_i2c_txdata(
	struct msm_camera_i2c_client *dev_client, unsigned char *txdata,
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
		S_I2C_DBG("msm_camera_qup_i2c_txdata faild 0x%x\n", saddr);
	return rc;
}

int32_t msm_camera_qup_i2c_read(struct msm_camera_i2c_client *client,
	uint32_t addr, uint16_t *data,
	enum msm_camera_i2c_data_type data_type)
{
	int32_t rc = -EFAULT;
	unsigned char *buf = NULL;

	if ((client->addr_type != MSM_CAMERA_I2C_BYTE_ADDR
		&& client->addr_type != MSM_CAMERA_I2C_WORD_ADDR)
		|| (data_type != MSM_CAMERA_I2C_BYTE_DATA
		&& data_type != MSM_CAMERA_I2C_WORD_DATA))
		return rc;

	if (client->addr_type > UINT_MAX - data_type) {
		pr_err("%s: integer overflow prevented\n", __func__);
		return rc;
	}

	buf = kzalloc(client->addr_type+data_type, GFP_KERNEL);
	if (!buf) {
		pr_err("%s:%d no memory\n", __func__, __LINE__);
		return -ENOMEM;
	}

	if (client->addr_type == MSM_CAMERA_I2C_BYTE_ADDR) {
		buf[0] = addr;
	} else if (client->addr_type == MSM_CAMERA_I2C_WORD_ADDR) {
		buf[0] = addr >> BITS_PER_BYTE;
		buf[1] = addr;
	}
	rc = msm_camera_qup_i2c_rxdata(client, buf, data_type);
	if (rc < 0) {
		S_I2C_DBG("%s fail\n", __func__);
		kfree(buf);
		buf = NULL;
		return rc;
	}

	if (data_type == MSM_CAMERA_I2C_BYTE_DATA)
		*data = buf[0];
	else
		*data = buf[0] << 8 | buf[1];

	S_I2C_DBG("%s addr = 0x%x data: 0x%x\n", __func__, addr, *data);
	kfree(buf);
	buf = NULL;
	return rc;
}

int32_t msm_camera_qup_i2c_read_seq(struct msm_camera_i2c_client *client,
	uint32_t addr, uint8_t *data, uint32_t num_byte)
{
	int32_t rc = -EFAULT;
	unsigned char *buf = NULL;
	int i;

	if ((client->addr_type != MSM_CAMERA_I2C_BYTE_ADDR
		&& client->addr_type != MSM_CAMERA_I2C_WORD_ADDR)
		|| num_byte == 0)
		return rc;

	if (num_byte > I2C_REG_DATA_MAX) {
		pr_err("%s: Error num_byte:0x%x exceeds 8K max supported:0x%x\n",
			__func__, num_byte, I2C_REG_DATA_MAX);
		return rc;
	}
	if (client->addr_type > UINT_MAX - num_byte) {
		pr_err("%s: integer overflow prevented\n", __func__);
		return rc;
	}

	buf = kzalloc(client->addr_type+num_byte, GFP_KERNEL);
	if (!buf) {
		pr_err("%s:%d no memory\n", __func__, __LINE__);
		return -ENOMEM;
	}

	if (client->addr_type == MSM_CAMERA_I2C_BYTE_ADDR) {
		buf[0] = addr;
	} else if (client->addr_type == MSM_CAMERA_I2C_WORD_ADDR) {
		buf[0] = addr >> BITS_PER_BYTE;
		buf[1] = addr;
	}
	rc = msm_camera_qup_i2c_rxdata(client, buf, num_byte);
	if (rc < 0) {
		S_I2C_DBG("%s fail\n", __func__);
		kfree(buf);
		buf = NULL;
		return rc;
	}

	S_I2C_DBG("%s addr = 0x%x", __func__, addr);
	for (i = 0; i < num_byte; i++) {
		data[i] = buf[i];
		S_I2C_DBG("Byte %d: 0x%x\n", i, buf[i]);
		S_I2C_DBG("Data: 0x%x\n", data[i]);
	}
	kfree(buf);
	buf = NULL;
	return rc;
}

int32_t msm_camera_qup_i2c_write(struct msm_camera_i2c_client *client,
	uint32_t addr, uint16_t data,
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
		S_I2C_DBG("%s byte %d: 0x%x\n", __func__,
			len, buf[len]);
		len = 1;
	} else if (client->addr_type == MSM_CAMERA_I2C_WORD_ADDR) {
		buf[0] = addr >> BITS_PER_BYTE;
		buf[1] = addr;
		S_I2C_DBG("%s byte %d: 0x%x\n", __func__,
			len, buf[len]);
		S_I2C_DBG("%s byte %d: 0x%x\n", __func__,
			len+1, buf[len+1]);
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
	rc = msm_camera_qup_i2c_txdata(client, buf, len);
	if (rc < 0)
		S_I2C_DBG("%s fail\n", __func__);
	return rc;
}

int32_t msm_camera_qup_i2c_write_seq(struct msm_camera_i2c_client *client,
	uint32_t addr, uint8_t *data, uint32_t num_byte)
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
		S_I2C_DBG("%s byte %d: 0x%x\n", __func__,
			len, buf[len]);
		len = 1;
	} else if (client->addr_type == MSM_CAMERA_I2C_WORD_ADDR) {
		buf[0] = addr >> BITS_PER_BYTE;
		buf[1] = addr;
		S_I2C_DBG("%s byte %d: 0x%x\n", __func__,
			len, buf[len]);
		S_I2C_DBG("%s byte %d: 0x%x\n", __func__,
			len+1, buf[len+1]);
		len = 2;
	}
	if (num_byte > I2C_SEQ_REG_DATA_MAX) {
		pr_err("%s: num_byte=%d clamped to max supported %d\n",
			__func__, num_byte, I2C_SEQ_REG_DATA_MAX);
		num_byte = I2C_SEQ_REG_DATA_MAX;
	}
	for (i = 0; i < num_byte; i++) {
		buf[i+len] = data[i];
		S_I2C_DBG("Byte %d: 0x%x\n", i+len, buf[i+len]);
		S_I2C_DBG("Data: 0x%x\n", data[i]);
	}
	rc = msm_camera_qup_i2c_txdata(client, buf, len+num_byte);
	if (rc < 0)
		S_I2C_DBG("%s fail\n", __func__);
	return rc;
}

int32_t msm_camera_qup_i2c_write_table(struct msm_camera_i2c_client *client,
	struct msm_camera_i2c_reg_setting *write_setting)
{
	int i;
	int32_t rc = -EFAULT;
	struct msm_camera_i2c_reg_array *reg_setting;
	uint16_t client_addr_type;

	if (!client || !write_setting)
		return rc;

	if ((write_setting->addr_type != MSM_CAMERA_I2C_BYTE_ADDR
		&& write_setting->addr_type != MSM_CAMERA_I2C_WORD_ADDR)
		|| (write_setting->data_type != MSM_CAMERA_I2C_BYTE_DATA
		&& write_setting->data_type != MSM_CAMERA_I2C_WORD_DATA))
		return rc;

	reg_setting = write_setting->reg_setting;
	client_addr_type = client->addr_type;
	client->addr_type = write_setting->addr_type;

	for (i = 0; i < write_setting->size; i++) {
		CDBG("%s addr 0x%x data 0x%x\n", __func__,
			reg_setting->reg_addr, reg_setting->reg_data);

		rc = msm_camera_qup_i2c_write(client, reg_setting->reg_addr,
			reg_setting->reg_data, write_setting->data_type);
		if (rc < 0)
			break;
		reg_setting++;
	}
	if (write_setting->delay > 20)
		msleep(write_setting->delay);
	else if (write_setting->delay)
		usleep_range(write_setting->delay * 1000, (write_setting->delay
			* 1000) + 1000);

	client->addr_type = client_addr_type;
	return rc;
}

int32_t msm_camera_qup_i2c_write_seq_table(struct msm_camera_i2c_client *client,
	struct msm_camera_i2c_seq_reg_setting *write_setting)
{
	int i;
	int32_t rc = -EFAULT;
	struct msm_camera_i2c_seq_reg_array *reg_setting;
	uint16_t client_addr_type;

	if (!client || !write_setting)
		return rc;

	if ((write_setting->addr_type != MSM_CAMERA_I2C_BYTE_ADDR
		&& write_setting->addr_type != MSM_CAMERA_I2C_WORD_ADDR)) {
		pr_err("%s Invalide addr type %d\n", __func__,
			write_setting->addr_type);
		return rc;
	}

	reg_setting = write_setting->reg_setting;
	client_addr_type = client->addr_type;
	client->addr_type = write_setting->addr_type;

	if (reg_setting->reg_data_size > I2C_SEQ_REG_DATA_MAX) {
		pr_err("%s: number of bytes %u exceeding the max supported %d\n",
		__func__, reg_setting->reg_data_size, I2C_SEQ_REG_DATA_MAX);
		return rc;
	}

	for (i = 0; i < write_setting->size; i++) {
		rc = msm_camera_qup_i2c_write_seq(client, reg_setting->reg_addr,
			reg_setting->reg_data, reg_setting->reg_data_size);
		if (rc < 0)
			break;
		reg_setting++;
	}
	if (write_setting->delay > 20)
		msleep(write_setting->delay);
	else if (write_setting->delay)
		usleep_range(write_setting->delay * 1000, (write_setting->delay
			* 1000) + 1000);

	client->addr_type = client_addr_type;
	return rc;
}

int32_t msm_camera_qup_i2c_write_table_w_microdelay(
	struct msm_camera_i2c_client *client,
	struct msm_camera_i2c_reg_setting *write_setting)
{
	int i;
	int32_t rc = -EFAULT;
	struct msm_camera_i2c_reg_array *reg_setting = NULL;

	if (!client || !write_setting)
		return rc;

	if ((client->addr_type != MSM_CAMERA_I2C_BYTE_ADDR
		&& client->addr_type != MSM_CAMERA_I2C_WORD_ADDR)
		|| (write_setting->data_type != MSM_CAMERA_I2C_BYTE_DATA
		&& write_setting->data_type != MSM_CAMERA_I2C_WORD_DATA))
		return rc;

	reg_setting = write_setting->reg_setting;
	for (i = 0; i < write_setting->size; i++) {
		rc = msm_camera_qup_i2c_write(client, reg_setting->reg_addr,
			reg_setting->reg_data, write_setting->data_type);
		if (rc < 0)
			break;
		if (reg_setting->delay)
			usleep_range(reg_setting->delay,
				reg_setting->delay + 1000);
		reg_setting++;
	}
	return rc;
}

static int32_t msm_camera_qup_i2c_compare(struct msm_camera_i2c_client *client,
	uint32_t addr, uint16_t data,
	enum msm_camera_i2c_data_type data_type)
{
	int32_t rc;
	uint16_t reg_data = 0;
	int data_len = 0;
	switch (data_type) {
	case MSM_CAMERA_I2C_BYTE_DATA:
	case MSM_CAMERA_I2C_WORD_DATA:
		data_len = data_type;
		break;
	case MSM_CAMERA_I2C_SET_BYTE_MASK:
	case MSM_CAMERA_I2C_UNSET_BYTE_MASK:
		data_len = MSM_CAMERA_I2C_BYTE_DATA;
		break;
	case MSM_CAMERA_I2C_SET_WORD_MASK:
	case MSM_CAMERA_I2C_UNSET_WORD_MASK:
		data_len = MSM_CAMERA_I2C_WORD_DATA;
		break;
	default:
		pr_err("%s: Unsupport data type: %d\n", __func__, data_type);
		break;
	}

	rc = msm_camera_qup_i2c_read(client, addr, &reg_data, data_len);
	if (rc < 0)
		return rc;

	rc = I2C_COMPARE_MISMATCH;
	switch (data_type) {
	case MSM_CAMERA_I2C_BYTE_DATA:
	case MSM_CAMERA_I2C_WORD_DATA:
		if (data == reg_data)
			rc = I2C_COMPARE_MATCH;
		break;
	case MSM_CAMERA_I2C_SET_BYTE_MASK:
	case MSM_CAMERA_I2C_SET_WORD_MASK:
		if ((reg_data & data) == data)
			rc = I2C_COMPARE_MATCH;
		break;
	case MSM_CAMERA_I2C_UNSET_BYTE_MASK:
	case MSM_CAMERA_I2C_UNSET_WORD_MASK:
		if (!(reg_data & data))
			rc = I2C_COMPARE_MATCH;
		break;
	default:
		pr_err("%s: Unsupport data type: %d\n", __func__, data_type);
		break;
	}

	S_I2C_DBG("%s: Register and data match result %d\n", __func__,
		rc);
	return rc;
}

int32_t msm_camera_qup_i2c_poll(struct msm_camera_i2c_client *client,
	uint32_t addr, uint16_t data,
	enum msm_camera_i2c_data_type data_type)
{
	int32_t rc;
	int i;
	S_I2C_DBG("%s: addr: 0x%x data: 0x%x dt: %d\n",
		__func__, addr, data, data_type);

	for (i = 0; i < I2C_POLL_MAX_ITERATION; i++) {
		rc = msm_camera_qup_i2c_compare(client,
			addr, data, data_type);
		if (rc == 0 || rc < 0)
			break;
		usleep_range(10000, 11000);
	}
	return rc;
}

static int32_t msm_camera_qup_i2c_set_mask(struct msm_camera_i2c_client *client,
	uint32_t addr, uint16_t mask,
	enum msm_camera_i2c_data_type data_type, uint16_t set_mask)
{
	int32_t rc;
	uint16_t reg_data;

	rc = msm_camera_qup_i2c_read(client, addr, &reg_data, data_type);
	if (rc < 0) {
		S_I2C_DBG("%s read fail\n", __func__);
		return rc;
	}
	S_I2C_DBG("%s addr: 0x%x data: 0x%x setmask: 0x%x\n",
			__func__, addr, reg_data, mask);

	if (set_mask)
		reg_data |= mask;
	else
		reg_data &= ~mask;
	S_I2C_DBG("%s write: 0x%x\n", __func__, reg_data);

	rc = msm_camera_qup_i2c_write(client, addr, reg_data, data_type);
	if (rc < 0)
		S_I2C_DBG("%s write fail\n", __func__);

	return rc;
}

static int32_t msm_camera_qup_i2c_set_write_mask_data(
	struct msm_camera_i2c_client *client,
	uint32_t addr, uint16_t data, int16_t mask,
	enum msm_camera_i2c_data_type data_type)
{
	int32_t rc;
	uint16_t reg_data;
	CDBG("%s\n", __func__);
	if (mask == -1)
		return 0;
	if (mask == 0) {
		rc = msm_camera_qup_i2c_write(client, addr, data, data_type);
	} else {
		rc = msm_camera_qup_i2c_read(client, addr, &reg_data,
			data_type);
		if (rc < 0) {
			CDBG("%s read fail\n", __func__);
			return rc;
		}
		reg_data &= ~mask;
		reg_data |= (data & mask);
		rc = msm_camera_qup_i2c_write(client, addr, reg_data,
			data_type);
		if (rc < 0)
			CDBG("%s write fail\n", __func__);
	}
	return rc;
}


int32_t msm_camera_qup_i2c_write_conf_tbl(
	struct msm_camera_i2c_client *client,
	struct msm_camera_i2c_reg_conf *reg_conf_tbl, uint16_t size,
	enum msm_camera_i2c_data_type data_type)
{
	int i;
	int32_t rc = -EFAULT;
	pr_err("%s, E. ", __func__);
	for (i = 0; i < size; i++) {
		enum msm_camera_i2c_data_type dt;
		if (reg_conf_tbl->cmd_type == MSM_CAMERA_I2C_CMD_POLL) {
			rc = msm_camera_qup_i2c_poll(client,
				reg_conf_tbl->reg_addr,
				reg_conf_tbl->reg_data,
				reg_conf_tbl->dt);
		} else {
			if (reg_conf_tbl->dt == 0)
				dt = data_type;
			else
				dt = reg_conf_tbl->dt;
			switch (dt) {
			case MSM_CAMERA_I2C_BYTE_DATA:
			case MSM_CAMERA_I2C_WORD_DATA:
				rc = msm_camera_qup_i2c_write(
					client,
					reg_conf_tbl->reg_addr,
					reg_conf_tbl->reg_data, dt);
				break;
			case MSM_CAMERA_I2C_SET_BYTE_MASK:
				rc = msm_camera_qup_i2c_set_mask(client,
					reg_conf_tbl->reg_addr,
					reg_conf_tbl->reg_data,
					MSM_CAMERA_I2C_BYTE_DATA, 1);
				break;
			case MSM_CAMERA_I2C_UNSET_BYTE_MASK:
				rc = msm_camera_qup_i2c_set_mask(client,
					reg_conf_tbl->reg_addr,
					reg_conf_tbl->reg_data,
					MSM_CAMERA_I2C_BYTE_DATA, 0);
				break;
			case MSM_CAMERA_I2C_SET_WORD_MASK:
				rc = msm_camera_qup_i2c_set_mask(client,
					reg_conf_tbl->reg_addr,
					reg_conf_tbl->reg_data,
					MSM_CAMERA_I2C_WORD_DATA, 1);
				break;
			case MSM_CAMERA_I2C_UNSET_WORD_MASK:
				rc = msm_camera_qup_i2c_set_mask(client,
					reg_conf_tbl->reg_addr,
					reg_conf_tbl->reg_data,
					MSM_CAMERA_I2C_WORD_DATA, 0);
				break;
			case MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA:
				rc = msm_camera_qup_i2c_set_write_mask_data(
					client,
					reg_conf_tbl->reg_addr,
					reg_conf_tbl->reg_data,
					reg_conf_tbl->mask,
					MSM_CAMERA_I2C_BYTE_DATA);
				break;
			default:
				pr_err("%s: Unsupport data type: %d\n",
					__func__, dt);
				break;
			}
		}
		if (rc < 0)
			break;
		reg_conf_tbl++;
	}
	return rc;
}

