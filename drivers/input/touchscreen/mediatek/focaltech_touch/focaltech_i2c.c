/*
 *
 * FocalTech TouchScreen driver.
 *
 * Copyright (c) 2012-2019, FocalTech Systems, Ltd., all rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/************************************************************************
 *
 * File Name: focaltech_i2c.c
 *
 * Author: Focaltech Driver Team
 *
 * Created: 2016-08-04
 *
 * Abstract: i2c communication with TP
 *
 * Version: v1.0
 *
 * Revision History:
 *
 ************************************************************************/

/***************************************************************************
 * Included header files
 ***************************************************************************/
#include "focaltech_core.h"

/*****************************************************************************
 * Private constant and macro definitions using #define
 *****************************************************************************/
#define I2C_RETRY_NUMBER 3
#define I2C_BUF_LENGTH 256

/*****************************************************************************
 * Private enumerations, structures and unions using typedef
 *****************************************************************************/

/*****************************************************************************
 * Static variables
 *****************************************************************************/
#ifdef CONFIG_MTK_I2C_EXTENSION
u8 *g_dma_buff_va;
dma_addr_t g_dma_buff_pa;
#endif

/*****************************************************************************
 * Global variable or extern global variabls/functions
 *****************************************************************************/

/*****************************************************************************
 * Static function prototypes
 *****************************************************************************/

/*****************************************************************************
 * functions body
 *****************************************************************************/
#ifdef CONFIG_MTK_I2C_EXTENSION
static void fts_i2c_msg_dma_alloct(void)
{
	FTS_FUNC_ENTER();
	if (g_dma_buff_va == NULL) {
		fts_data->input_dev->dev.coherent_dma_mask = DMA_BIT_MASK(32);
		g_dma_buff_va = (u8 *)dma_alloc_coherent(
			&fts_data->input_dev->dev, I2C_BUF_LENGTH,
			&g_dma_buff_pa, GFP_KERNEL);
		if (!g_dma_buff_va)
			FTS_ERROR("Allocate I2C DMA Buffer fail");

	}
	FTS_FUNC_EXIT();
}

static void fts_i2c_msg_dma_release(void)
{
	FTS_FUNC_ENTER();
	if (g_dma_buff_va) {
		dma_free_coherent(NULL, I2C_BUF_LENGTH, g_dma_buff_va,
				  g_dma_buff_pa);
		g_dma_buff_va = NULL;
		g_dma_buff_pa = 0;
	}
	FTS_FUNC_EXIT();
}

int fts_read(u8 *cmd, u32 cmdlen, u8 *data, u32 datalen)
{
	int ret = 0;
	int i = 0;
	struct fts_ts_data *ts_data = fts_data;
	struct i2c_client *client = NULL;

	/* must have data when read */
	if (!ts_data || !ts_data->client || !data || !datalen ||
	    (datalen >= I2C_BUF_LENGTH) || (cmdlen >= I2C_BUF_LENGTH)) {
		FTS_ERROR(
			"fts_data/client/cmdlen(%d)/data/datalen(%d) is invalid",
			cmdlen, datalen);
		return -EINVAL;
	}
	client = ts_data->client;

	mutex_lock(&ts_data->bus_lock);
	memcpy(g_dma_buff_va, cmd, cmdlen);
	client->addr = ((client->addr & I2C_MASK_FLAG) | I2C_DMA_FLAG);
	if (cmd && (cmdlen > 0)) {
		for (i = 0; i < I2C_RETRY_NUMBER; i++) {
			ret = i2c_master_send(client, (u8 *)g_dma_buff_pa,
					      cmdlen);
			if (ret != cmdlen) {
				FTS_ERROR("[IIC]: i2c_master_send fail,ret=%d",
					  ret);
			} else
				break;
		}
	}

	for (i = 0; i < I2C_RETRY_NUMBER; i++) {
		ret = i2c_master_recv(client, (u8 *)g_dma_buff_pa, datalen);
		if (ret != datalen)
			FTS_ERROR("i2c_master_recv fail,ret=%d", ret);
		else
			break;
	}
	client->addr = client->addr & I2C_MASK_FLAG & (~I2C_DMA_FLAG);
	memcpy(data, g_dma_buff_va, datalen);

	mutex_unlock(&ts_data->bus_lock);
	return ret;
}

int fts_write(u8 *writebuf, u32 writelen)
{
	int ret = 0;
	int i = 0;
	struct fts_ts_data *ts_data = fts_data;
	struct i2c_client *client = NULL;

	if (!ts_data || !ts_data->client || !writebuf || !writelen ||
	    (writelen >= I2C_BUF_LENGTH)) {
		FTS_ERROR("fts_data/client/data/datalen(%d) is invalid",
			  writelen);
		return -EINVAL;
	}
	client = ts_data->client;

	mutex_lock(&ts_data->bus_lock);
	memcpy(g_dma_buff_va, writebuf, writelen);
	client->addr = ((client->addr & I2C_MASK_FLAG) | I2C_DMA_FLAG);
	for (i = 0; i < I2C_RETRY_NUMBER; i++) {
		ret = i2c_master_send(client, (u8 *)g_dma_buff_pa, writelen);
		if (ret != writelen)
			FTS_ERROR("i2c_master_send fail,ret=%d", ret);
		else
			break;
	}
	client->addr = client->addr & I2C_MASK_FLAG & (~I2C_DMA_FLAG);
	mutex_unlock(&ts_data->bus_lock);

	return ret;
}
#else
int fts_read(u8 *cmd, u32 cmdlen, u8 *data, u32 datalen)
{
	int ret = 0;
	int i = 0;
	struct fts_ts_data *ts_data = fts_data;
	struct i2c_msg msg_list[2];
	struct i2c_msg *msg = NULL;
	int msg_num = 0;

	/* must have data when read */
	if (!ts_data || !ts_data->client || !data || !datalen ||
	    (datalen >= I2C_BUF_LENGTH) || (cmdlen >= I2C_BUF_LENGTH)) {
		FTS_ERROR(
			"fts_data/client/cmdlen(%d)/data/datalen(%d) is invalid",
			cmdlen, datalen);
		return -EINVAL;
	}

	mutex_lock(&ts_data->bus_lock);
	memset(&msg_list[0], 0, sizeof(struct i2c_msg));
	memset(&msg_list[1], 0, sizeof(struct i2c_msg));
	memcpy(ts_data->bus_tx_buf, cmd, cmdlen);
	msg_list[0].addr = ts_data->client->addr;
	msg_list[0].flags = 0;
	msg_list[0].len = cmdlen;
	msg_list[0].buf = ts_data->bus_tx_buf;
	msg_list[1].addr = ts_data->client->addr;
	msg_list[1].flags = I2C_M_RD;
	msg_list[1].len = datalen;
	msg_list[1].buf = ts_data->bus_rx_buf;
	if (cmd && cmdlen) {
		msg = &msg_list[0];
		msg_num = 2;
	} else {
		msg = &msg_list[1];
		msg_num = 1;
	}

	for (i = 0; i < I2C_RETRY_NUMBER; i++) {
		ret = i2c_transfer(ts_data->client->adapter, msg, msg_num);
		if (ret < 0) {
			FTS_ERROR("i2c_transfer(read) fail,ret:%d", ret);
		} else {
			memcpy(data, ts_data->bus_rx_buf, datalen);
			break;
		}
	}

	mutex_unlock(&ts_data->bus_lock);
	return ret;
}

int fts_write(u8 *writebuf, u32 writelen)
{
	int ret = 0;
	int i = 0;
	struct fts_ts_data *ts_data = fts_data;
	struct i2c_msg msgs;

	if (!ts_data || !ts_data->client || !writebuf || !writelen ||
	    (writelen >= I2C_BUF_LENGTH)) {
		FTS_ERROR("fts_data/client/data/datalen(%d) is invalid",
			  writelen);
		return -EINVAL;
	}

	mutex_lock(&ts_data->bus_lock);
	memset(&msgs, 0, sizeof(struct i2c_msg));
	memcpy(ts_data->bus_tx_buf, writebuf, writelen);
	msgs.addr = ts_data->client->addr;
	msgs.flags = 0;
	msgs.len = writelen;
	msgs.buf = ts_data->bus_tx_buf;
	for (i = 0; i < I2C_RETRY_NUMBER; i++) {
		ret = i2c_transfer(ts_data->client->adapter, &msgs, 1);
		if (ret < 0)
			FTS_ERROR("i2c_transfer(write) fail,ret:%d", ret);
		else
			break;

	}
	mutex_unlock(&ts_data->bus_lock);
	return ret;
}
#endif

int fts_read_reg(u8 addr, u8 *value)
{
	return fts_read(&addr, 1, value, 1);
}

int fts_write_reg(u8 addr, u8 value)
{
	u8 buf[2] = {0};

	buf[0] = addr;
	buf[1] = value;
	return fts_write(buf, sizeof(buf));
}

int fts_bus_init(struct fts_ts_data *ts_data)
{
	FTS_FUNC_ENTER();
#ifdef CONFIG_MTK_I2C_EXTENSION
	fts_i2c_msg_dma_alloct();
#endif

	ts_data->bus_tx_buf = kzalloc(I2C_BUF_LENGTH, GFP_KERNEL);
	if (ts_data->bus_tx_buf == NULL) {
		FTS_ERROR("failed to allocate memory for bus_tx_buf");
		return -ENOMEM;
	}

	ts_data->bus_rx_buf = kzalloc(I2C_BUF_LENGTH, GFP_KERNEL);
	if (ts_data->bus_rx_buf == NULL) {
		FTS_ERROR("failed to allocate memory for bus_rx_buf");
		return -ENOMEM;
	}
	FTS_FUNC_EXIT();
	return 0;
}

int fts_bus_exit(struct fts_ts_data *ts_data)
{
	FTS_FUNC_ENTER();
#ifdef CONFIG_MTK_I2C_EXTENSION
	fts_i2c_msg_dma_release();
#endif

	if (ts_data && ts_data->bus_tx_buf) {
		kfree(ts_data->bus_tx_buf);
		ts_data->bus_tx_buf = NULL;
	}

	if (ts_data && ts_data->bus_rx_buf) {
		kfree(ts_data->bus_rx_buf);
		ts_data->bus_rx_buf = NULL;
	}
	FTS_FUNC_EXIT();
	return 0;
}
