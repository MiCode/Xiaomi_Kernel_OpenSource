/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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
#include <linux/i2c.h>
#include <linux/mhl_8334.h>

#include "mhl_i2c_utils.h"

uint8_t slave_addrs[MAX_PAGES] = {
	DEV_PAGE_TPI_0    ,
	DEV_PAGE_TX_L0_0  ,
	DEV_PAGE_TX_L1_0  ,
	DEV_PAGE_TX_2_0   ,
	DEV_PAGE_TX_3_0   ,
	DEV_PAGE_CBUS     ,
	DEV_PAGE_DDC_EDID ,
	DEV_PAGE_DDC_SEGM ,
};

int mhl_i2c_reg_read(uint8_t slave_addr_index, uint8_t reg_offset)
{
	struct i2c_msg msgs[2];
	uint8_t buffer = 0;
	int ret = -1;

	pr_debug("MRR: Reading from slave_addr_index=[%x] and offset=[%x]\n",
		slave_addr_index, reg_offset);
	pr_debug("MRR: Addr slave_addr_index=[%x]\n",
		slave_addrs[slave_addr_index]);

	/* Slave addr */
	msgs[0].addr = slave_addrs[slave_addr_index] >> 1;
	msgs[1].addr = slave_addrs[slave_addr_index] >> 1;

	/* Write Command */
	msgs[0].flags = 0;
	msgs[1].flags = I2C_M_RD;

	/* Register offset for the next transaction */
	msgs[0].buf = &reg_offset;
	msgs[1].buf = &buffer;

	/* Offset is 1 Byte long */
	msgs[0].len = 1;
	msgs[1].len = 1;

	ret = i2c_transfer(mhl_msm_state->i2c_client->adapter, msgs, 2);
	if (ret < 1) {
		pr_err("I2C READ FAILED=[%d]\n", ret);
		return -EACCES;
	}
	pr_debug("Buffer is [%x]\n", buffer);
	return buffer;
}


int mhl_i2c_reg_write(uint8_t slave_addr_index, uint8_t reg_offset,
	uint8_t value)
{
	return mhl_i2c_reg_write_cmds(slave_addr_index, reg_offset, &value, 1);
}

int mhl_i2c_reg_write_cmds(uint8_t slave_addr_index, uint8_t reg_offset,
	uint8_t *value, uint16_t count)
{
	struct i2c_msg msgs[1];
	uint8_t data[2];
	int status = -EACCES;

	msgs[0].addr = slave_addrs[slave_addr_index] >> 1;
	msgs[0].flags = 0;
	msgs[0].len = 2;
	msgs[0].buf = data;
	data[0] = reg_offset;
	data[1] = *value;

	status = i2c_transfer(mhl_msm_state->i2c_client->adapter, msgs, 1);
	if (status < 1) {
		pr_err("I2C WRITE FAILED=[%d]\n", status);
		return -EACCES;
	}

	return status;
}

void mhl_i2c_reg_modify(uint8_t slave_addr_index, uint8_t reg_offset,
	uint8_t mask, uint8_t val)
{
	uint8_t temp;

	temp = mhl_i2c_reg_read(slave_addr_index, reg_offset);
	temp &= (~mask);
	temp |= (mask & val);
	mhl_i2c_reg_write(slave_addr_index, reg_offset, temp);
}

