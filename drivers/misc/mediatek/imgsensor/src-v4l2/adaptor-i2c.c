// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2019 MediaTek Inc.

#include <linux/i2c.h>

#ifdef HAVE_MTK_I2C_TRANSFER
#include "i2c-mtk.h"
#endif

#include "adaptor-i2c.h"

#define IMGSENSOR_I2C_SPEED 400
#define IMGSENSOR_I2C_CMD_LENGTH_MAX 255

DEFINE_MUTEX(legacy_lock);

static struct IMGSENSOR_I2C_INST legacy_i2c_inst = {
	.pi2c_client = NULL,
};

static struct IMGSENSOR_I2C_CFG legacy_i2c_cfg = {
	.pinst = &legacy_i2c_inst,
	.i2c_mutex = __MUTEX_INITIALIZER(legacy_i2c_cfg.i2c_mutex),
};

int imgsensor_i2c_read(
		struct IMGSENSOR_I2C_CFG *pi2c_cfg,
		u8 *pwrite_data,
		u16 write_length,
		u8 *pread_data,
		u16 read_length,
		u16 id,
		int speed)
{
	int ret;
	struct IMGSENSOR_I2C_INST *pinst = pi2c_cfg->pinst;

	if (pinst->pi2c_client == NULL) {
		pr_err("no i2c_client\n");
		return -EINVAL;
	}

	if (speed < 0 || speed > 1000)
		speed = IMGSENSOR_I2C_SPEED;

	mutex_lock(&pi2c_cfg->i2c_mutex);

	pinst->msg[0].addr  = id >> 1;
	pinst->msg[0].flags = 0;
	pinst->msg[0].len   = write_length;
	pinst->msg[0].buf   = pwrite_data;

	pinst->msg[1].addr  = id >> 1;
	pinst->msg[1].flags = I2C_M_RD;
	pinst->msg[1].len   = read_length;
	pinst->msg[1].buf   = pread_data;

#ifdef HAVE_MTK_I2C_TRANSFER
	ret = mtk_i2c_transfer(
		pinst->pi2c_client->adapter,
		pinst->msg, 2,
		pi2c_cfg->pinst->status.filter_msg ? I2C_A_FILTER_MSG : 0,
		speed * 1000);
#else
	ret = i2c_transfer(
		pinst->pi2c_client->adapter,
		pinst->msg, 2);
#endif

	mutex_unlock(&pi2c_cfg->i2c_mutex);

	if (ret != 2)
		return -EIO;

	return 0;
}

int imgsensor_i2c_write(
		struct IMGSENSOR_I2C_CFG *pi2c_cfg,
		u8 *pwrite_data,
		u16 write_length,
		u16 write_per_cycle,
		u16 id,
		int speed)
{
	int i, ret;
	u8 *pdata = pwrite_data;
	u8 *pend  = pwrite_data + write_length;
	struct IMGSENSOR_I2C_INST *pinst = pi2c_cfg->pinst;
	struct i2c_msg *pmsg  = pinst->msg;

	if (pinst->pi2c_client == NULL) {
		pr_err("no i2c_client\n");
		return -EINVAL;
	}

	if (speed < 0 || speed > 1000)
		speed = IMGSENSOR_I2C_SPEED;

	mutex_lock(&pi2c_cfg->i2c_mutex);

	i = 0;
	while (pdata < pend && i < IMGSENSOR_I2C_CMD_LENGTH_MAX) {
		pmsg->addr  = id >> 1;
		pmsg->flags = 0;
		pmsg->len   = write_per_cycle;
		pmsg->buf   = pdata;
		i++;
		pmsg++;
		pdata += write_per_cycle;
	}

#ifdef HAVE_MTK_I2C_TRANSFER
	ret = mtk_i2c_transfer(
		pinst->pi2c_client->adapter,
		pinst->msg, i,
		pi2c_cfg->pinst->status.filter_msg ? I2C_A_FILTER_MSG : 0,
		speed * 1000);
#else
	ret = i2c_transfer(
		pinst->pi2c_client->adapter,
		pinst->msg, i);
#endif

	mutex_unlock(&pi2c_cfg->i2c_mutex);

	if (ret != i)
		return -EIO;

	return 0;
}

void kdSetI2CSpeed(u16 i2cSpeed)
{

}

int iReadRegI2C(u8 *a_pSendData, u16 a_sizeSendData,
		u8 *a_pRecvData, u16 a_sizeRecvData,
		u16 i2cId)
{
	return imgsensor_i2c_read(
			&legacy_i2c_cfg,
			a_pSendData,
			a_sizeSendData,
			a_pRecvData,
			a_sizeRecvData,
			i2cId,
			IMGSENSOR_I2C_SPEED);
}

int iReadRegI2CTiming(u8 *a_pSendData, u16 a_sizeSendData, u8 *a_pRecvData,
			u16 a_sizeRecvData, u16 i2cId, u16 timing)
{
	return imgsensor_i2c_read(
			&legacy_i2c_cfg,
			a_pSendData,
			a_sizeSendData,
			a_pRecvData,
			a_sizeRecvData,
			i2cId,
			timing);
}

int iWriteRegI2C(u8 *a_pSendData, u16 a_sizeSendData, u16 i2cId)
{
	return imgsensor_i2c_write(
			&legacy_i2c_cfg,
			a_pSendData,
			a_sizeSendData,
			a_sizeSendData,
			i2cId,
			IMGSENSOR_I2C_SPEED);
}

int iWriteRegI2CTiming(u8 *a_pSendData, u16 a_sizeSendData,
			u16 i2cId, u16 timing)
{
	return imgsensor_i2c_write(
			&legacy_i2c_cfg,
			a_pSendData,
			a_sizeSendData,
			a_sizeSendData,
			i2cId,
			timing);
}

int iBurstWriteReg(u8 *pData, u32 bytes, u16 i2cId)
{
	return imgsensor_i2c_write(
			&legacy_i2c_cfg,
			pData,
			bytes,
			bytes,
			i2cId,
			IMGSENSOR_I2C_SPEED);
}

int iBurstWriteReg_multi(u8 *pData, u32 bytes, u16 i2cId,
				u16 transfer_length, u16 timing)
{
	return imgsensor_i2c_write(
			&legacy_i2c_cfg,
			pData,
			bytes,
			transfer_length,
			i2cId,
			timing);
}

void adaptor_legacy_set_i2c_client(struct i2c_client *client)
{
	legacy_i2c_inst.pi2c_client = client;
}

void adaptor_legacy_lock(void)
{
	mutex_lock(&legacy_lock);
}

void adaptor_legacy_unlock(void)
{
	mutex_unlock(&legacy_lock);
}
