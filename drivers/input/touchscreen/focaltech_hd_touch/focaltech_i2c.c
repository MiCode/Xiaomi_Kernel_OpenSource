// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2012-2019, FocalTech Systems, Ltd., all rights reserved.
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

/*****************************************************************************
 * Included header files
 *****************************************************************************/
#include "focaltech_core.h"

/*****************************************************************************
 * Private constant and macro definitions using #define
 *****************************************************************************/
#define DMA_BUFFER_LENGTH 256
#define I2C_RETRY_NUMBER 3

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
int fts_read(u8 *cmd, u32 cmdlen, u8 *data, u32 datalen)
{
	int ret = 0;
	int i = 0;
	struct i2c_client *client = NULL;

	/* must have data when read */
	if (!fts_data || !fts_data->client || !data || !datalen) {
		FTS_ERROR("fts_data/client/data/datalen(%d) is invalid",
			  datalen);
		return -EINVAL;
	}
	client = fts_data->client;

	mutex_lock(&fts_data->bus_lock);
	if ((writelen > 0) && (writelen <= DMA_BUFFER_LENGTH)) {
		memcpy(g_dma_buff_va, cmd, cmdlen);
		client->addr = ((client->addr & I2C_MASK_FLAG) | I2C_DMA_FLAG);
		for (i = 0; i < I2C_RETRY_NUMBER; i++) {
			ret = i2c_master_send(client,
					      (unsigned char *)g_dma_buff_pa,
					      writelen);
			if (ret != writelen) {
				FTS_ERROR(
					"[IIC]: i2c_master_send failed, ret=%d!!",
					ret);
			} else
				break;
		}
		client->addr = client->addr & I2C_MASK_FLAG & (~I2C_DMA_FLAG);
	}

	if ((readlen > 0) && (readlen <= DMA_BUFFER_LENGTH)) {
		client->addr = ((client->addr & I2C_MASK_FLAG) | I2C_DMA_FLAG);
		for (i = 0; i < I2C_RETRY_NUMBER; i++) {
			ret = i2c_master_recv(client,
					      (unsigned char *)g_dma_buff_pa,
					      datalen);
			if (ret != readlen)
				FTS_ERROR("i2c_master_recv failed,ret=%d", ret);
			else
				break;
		}
		memcpy(data, g_dma_buff_va, readlen);
		client->addr = client->addr & I2C_MASK_FLAG & (~I2C_DMA_FLAG);
	} else
		FTS_ERROR("i2c read len(%d) fail", readlen);
	mutex_unlock(&fts_data->bus_lock);
	return ret;
}

int fts_i2c_write(u8 *writebuf, int writelen)
{
	int ret = 0;
	int i = 0;
	struct i2c_client *client = NULL;

	if (!fts_data || !fts_data->client || !writebuf || !writelen) {
		FTS_ERROR("fts_data/client/data/datalen(%d) is invalid",
			  writelen);
		return -EINVAL;
	}
	client = fts_data->client;

	mutex_lock(&fts_data->bus_lock);
	if ((writelen > 0) && (writelen <= DMA_BUFFER_LENGTH)) {
		memcpy(g_dma_buff_va, writebuf, writelen);
		client->addr = ((client->addr & I2C_MASK_FLAG) | I2C_DMA_FLAG);
		for (i = 0; i < I2C_RETRY_NUMBER; i++) {
			ret = i2c_master_send(client,
					      (unsigned char *)g_dma_buff_pa,
					      writelen);
			if (ret != writelen)
				FTS_ERROR("i2c_master_send failed,ret=%d", ret);
			else
				break;
		}
		client->addr = client->addr & I2C_MASK_FLAG & (~I2C_DMA_FLAG);
	} else
		FTS_ERROR("i2c write len(%d) fail", writelen);
	mutex_unlock(&fts_data->bus_lock);

	return ret;
}

static void fts_i2c_msg_dma_alloct(void)
{
	FTS_FUNC_ENTER();
	if (g_dma_buff_va == NULL) {
		fts_data->input_dev->dev.coherent_dma_mask = DMA_BIT_MASK(32);
		g_dma_buff_va = (u8 *)dma_alloc_coherent(
			&fts_data->input_dev->dev, DMA_BUFFER_LENGTH,
			&g_dma_buff_pa, GFP_KERNEL);

		if (!g_dma_buff_va)
			FTS_ERROR("Allocate DMA I2C Buffer failed");

	}
	FTS_FUNC_EXIT();
}

static void fts_i2c_msg_dma_release(void)
{
	FTS_FUNC_ENTER();
	if (g_dma_buff_va) {
		dma_free_coherent(NULL, DMA_BUFFER_LENGTH, g_dma_buff_va,
				  g_dma_buff_pa);
		g_dma_buff_va = NULL;
		g_dma_buff_pa = 0;
		FTS_ERROR("[IIC]: Allocated DMA I2C Buffer release!!");
	}
	FTS_FUNC_EXIT();
}
#else
int fts_read(u8 *cmd, u32 cmdlen, u8 *data, u32 datalen)
{
	int ret = 0;
	int i = 0;
	struct i2c_client *client = NULL;
	struct i2c_msg msg_list[2];
	struct i2c_msg *msg = NULL;
	int msg_num = 0;

	/* must have data when read */
	if (!fts_data || !fts_data->client || !data || !datalen) {
		FTS_ERROR("fts_data/client/data/datalen(%d) is invalid",
			  datalen);
		return -EINVAL;
	}
	client = fts_data->client;

	mutex_lock(&fts_data->bus_lock);
	memset(&msg_list[0], 0, sizeof(struct i2c_msg));
	memset(&msg_list[1], 0, sizeof(struct i2c_msg));
	msg_list[0].addr = client->addr;
	msg_list[0].flags = 0;
	msg_list[0].len = cmdlen;
	msg_list[0].buf = cmd;
	msg_list[1].addr = client->addr;
	msg_list[1].flags = I2C_M_RD;
	msg_list[1].len = datalen;
	msg_list[1].buf = data;
	if (cmd && cmdlen) {
		msg = &msg_list[0];
		msg_num = 2;
	} else {
		msg = &msg_list[1];
		msg_num = 1;
	}

	for (i = 0; i < I2C_RETRY_NUMBER; i++) {
		ret = i2c_transfer(client->adapter, msg, msg_num);
		if (ret < 0)
			FTS_ERROR("i2c_transfer(read) fail,ret:%d", ret);
		else
			break;

	}

	mutex_unlock(&fts_data->bus_lock);
	return ret;
}

int fts_write(u8 *writebuf, u32 writelen)
{
	int ret = 0;
	int i = 0;
	struct i2c_client *client = NULL;
	struct i2c_msg msgs;

	if (!fts_data || !fts_data->client || !writebuf || !writelen) {
		FTS_ERROR("fts_data/client/data/datalen(%d) is invalid",
			  writelen);
		return -EINVAL;
	}
	client = fts_data->client;

	mutex_lock(&fts_data->bus_lock);
	memset(&msgs, 0, sizeof(struct i2c_msg));
	msgs.addr = client->addr;
	msgs.flags = 0;
	msgs.len = writelen;
	msgs.buf = writebuf;
	for (i = 0; i < I2C_RETRY_NUMBER; i++) {
		ret = i2c_transfer(client->adapter, &msgs, 1);
		if (ret < 0)
			FTS_ERROR("i2c_transfer(write) fail,reg:%d", ret);
		else
			break;

	}
	mutex_unlock(&fts_data->bus_lock);
	return ret;
}

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
#endif

int fts_bus_init(struct fts_ts_data *ts_data)
{
	FTS_FUNC_ENTER();
#ifdef CONFIG_MTK_I2C_EXTENSION
	fts_i2c_msg_dma_alloct();
#endif
	ts_data->bus_buf = NULL;
	FTS_FUNC_EXIT();
	return 0;
}

int fts_bus_exit(struct fts_ts_data *ts_data)
{
	FTS_FUNC_ENTER();
#ifdef CONFIG_MTK_I2C_EXTENSION
	fts_i2c_msg_dma_release();
#endif
	FTS_FUNC_EXIT();
	return 0;
}
