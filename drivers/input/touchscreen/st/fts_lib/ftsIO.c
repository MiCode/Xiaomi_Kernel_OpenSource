// SPDX-License-Identifier: GPL-2.0-only
/*
 * FTS Capacitive touch screen controller (FingerTipS)
 *
 * Copyright (C) 2016-2019, STMicroelectronics Limited.
 * Authors: AMG(Analog Mems Group) <marco.cali@st.com>
 *
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 *
 **************************************************************************
 **                        STMicroelectronics                            **
 **************************************************************************
 **                        marco.cali@st.com                             **
 **************************************************************************
 *                                                                        *
 *                        I2C/SPI Communication                          **
 *                                                                        *
 **************************************************************************
 **************************************************************************
 *
 */

#include <linux/pm_runtime.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <stdarg.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/serio.h>
#include <linux/init.h>
#include <linux/pm.h>
#include <linux/delay.h>
#include <linux/ctype.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/power_supply.h>
#include <linux/firmware.h>
#include <linux/regulator/consumer.h>
#include <linux/of_gpio.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/spi/spidev.h>
#include <linux/timekeeping.h>

#include "ftsSoftware.h"
#include "ftsCrossCompile.h"
#include "ftsError.h"
#include "ftsHardware.h"
#include "ftsIO.h"
#include "ftsTool.h"
#include "../fts.h"

static char tag[8] = "[ FTS ]\0";
static struct i2c_client *client;
static u16 I2CSAD;

int openChannel(struct i2c_client *clt)
{
	client = clt;
	I2CSAD = clt->addr;
	logError(0, "%s %s: SAD: %02X\n", tag, __func__, I2CSAD);
	return OK;
}

struct device *getDev()
{
	if (client != NULL)
		return &(client->dev);
	else
		return NULL;
}

struct i2c_client *getClient()
{
	if (client != NULL)
		return client;
	else
		return NULL;
}

int fts_readCmd(u8 *cmd, int cmdLength, u8 *outBuf, int byteToRead)
{
	int ret = -1;
	int retry = 0;
	struct i2c_msg I2CMsg[2];
	struct fts_ts_info *info = i2c_get_clientdata(client);
	uint8_t *buf = info->i2c_data;

	/*
	 * Reassign memory for i2c_data in case length is greater than 32 bytes
	 */
	if (info->i2c_data_len < cmdLength + byteToRead + 1) {
		kfree(info->i2c_data);
		info->i2c_data = kmalloc(cmdLength + byteToRead + 1,
							GFP_KERNEL);
		if (!info->i2c_data) {
			info->i2c_data_len = 0;
			return -ENOMEM;
		}
		info->i2c_data_len = cmdLength + byteToRead + 1;
		buf = info->i2c_data;
	}

	//write msg
	I2CMsg[0].addr = (__u16)I2CSAD;
	I2CMsg[0].flags = (__u16)0;
	I2CMsg[0].len = (__u16)cmdLength;
	I2CMsg[0].buf = buf;

	//read msg
	I2CMsg[1].addr = (__u16)I2CSAD;
	I2CMsg[1].flags = I2C_M_RD;
	I2CMsg[1].len = byteToRead;
	I2CMsg[1].buf = buf + cmdLength;

	memcpy(buf, cmd, cmdLength);

	if (client == NULL)
		return ERROR_I2C_O;
	while (retry < I2C_RETRY && ret < OK) {
		ret = i2c_transfer(client->adapter, I2CMsg, 2);
		retry++;
		if (ret < OK) {
#ifdef CONFIG_ST_TRUSTED_TOUCH
#ifdef CONFIG_ARCH_QTI_VM
			if (atomic_read(&info->trusted_touch_enabled) &&
					ret == -ECONNRESET){
				pr_err("failed i2c read reacquiring session\n");
				pm_runtime_put_sync(
					info->client->adapter->dev.parent);
				pm_runtime_get_sync(
					info->client->adapter->dev.parent);
			}
#endif
#endif
			msleep(I2C_WAIT_BEFORE_RETRY);
		}
	}
	if (ret < 0) {
#ifdef CONFIG_ST_TRUSTED_TOUCH
#ifdef CONFIG_ARCH_QTI_VM
		pr_err("initiating abort due to i2c xfer failure\n");
		fts_trusted_touch_tvm_i2c_failure_report(info);
#endif
#endif
		logError(1, "%s %s: ERROR %02X\n",
			tag, __func__, ERROR_I2C_R);
		return ret;
	}

	memcpy(outBuf, buf + cmdLength, byteToRead);

	return OK;
}

int fts_writeCmd(u8 *cmd, int cmdLength)
{
	int ret = -1;
	int retry = 0;
	struct i2c_msg I2CMsg[1];
	struct fts_ts_info *info = i2c_get_clientdata(client);
	uint8_t *buf = info->i2c_data;

	/*
	 * Reassign memory for i2c_data in case length is greater than 32 bytes
	 */
	if (info->i2c_data_len < cmdLength + 1) {
		kfree(info->i2c_data);
		info->i2c_data = kmalloc(cmdLength + 1, GFP_KERNEL);
		if (!info->i2c_data) {
			info->i2c_data_len = 0;
			return -ENOMEM;
		}
		info->i2c_data_len = cmdLength + 1;
		buf = info->i2c_data;
	}

	I2CMsg[0].addr = (__u16)I2CSAD;
	I2CMsg[0].flags = (__u16)0;
	I2CMsg[0].len = (__u16)cmdLength;
	I2CMsg[0].buf = buf;

	memcpy(buf, cmd, cmdLength);

	if (client == NULL)
		return ERROR_I2C_O;
	while (retry < I2C_RETRY && ret < OK) {
		ret = i2c_transfer(client->adapter, I2CMsg, 1);
		retry++;
		if (ret < OK) {
#ifdef CONFIG_ST_TRUSTED_TOUCH
#ifdef CONFIG_ARCH_QTI_VM
			if (atomic_read(&info->trusted_touch_enabled) &&
					ret == -ECONNRESET){
				pr_err("failed i2c write reacquiring session\n");
				pm_runtime_put_sync(
					info->client->adapter->dev.parent);
				pm_runtime_get_sync(
					info->client->adapter->dev.parent);

			}
#endif
#endif
			msleep(I2C_WAIT_BEFORE_RETRY);
			logError(1, "ERROR: %d\n", ret);
		}
		//logError(1,"%s fts_writeCmd: attempt %d\n", tag, retry);
	}
	if (ret < 0) {
#ifdef CONFIG_ST_TRUSTED_TOUCH
#ifdef CONFIG_ARCH_QTI_VM
		pr_err("initiating abort due to i2c xfer failure\n");
		fts_trusted_touch_tvm_i2c_failure_report(info);
#endif
#endif
		logError(1, "%s %s: ERROR %02X\n", tag, __func__, ERROR_I2C_W);
		return ret;
	}
	return OK;
}

int fts_writeFwCmd(u8 *cmd, int cmdLength)
{
	int ret = -1;
	int ret2 = -1;
	int retry = 0;
	struct i2c_msg I2CMsg[1];
	struct fts_ts_info *info = i2c_get_clientdata(client);
	uint8_t *buf = info->i2c_data;

	/*
	 * Reassign memory for i2c_data in case length is greater than 32 bytes
	 */
	if (info->i2c_data_len < cmdLength + 1) {
		kfree(info->i2c_data);
		info->i2c_data = kmalloc(cmdLength + 1, GFP_KERNEL);
		if (!info->i2c_data) {
			info->i2c_data_len = 0;
			return -ENOMEM;
		}
		info->i2c_data_len = cmdLength + 1;
		buf = info->i2c_data;
	}

	I2CMsg[0].addr = (__u16)I2CSAD;
	I2CMsg[0].flags = (__u16)0;
	I2CMsg[0].len = (__u16)cmdLength;
	I2CMsg[0].buf = buf;

	memcpy(buf, cmd, cmdLength);

	if (client == NULL)
		return ERROR_I2C_O;
	while (retry < I2C_RETRY && (ret < OK || ret2 < OK)) {
		ret = i2c_transfer(client->adapter, I2CMsg, 1);
		retry++;
		if (ret >= 0)
			ret2 = checkEcho(cmd, cmdLength);
		if (ret < OK || ret2 < OK)
			msleep(I2C_WAIT_BEFORE_RETRY);
		//logError(1,"%s fts_writeCmd: attempt %d\n", tag, retry);
	}
	if (ret < 0) {
		logError(1, "%s %s: ERROR %02X\n",
			tag, __func__, ERROR_I2C_W);
		return ERROR_I2C_W;
	}
	if (ret2 < OK) {
		logError(1, "%s %s: check echo ERROR %02X\n",
			tag, __func__, ret2);
		return (ret | ERROR_I2C_W);
	}
	return OK;
}


int writeReadCmd(u8 *writeCmd1, int writeCmdLength, u8 *readCmd1,
	int readCmdLength, u8 *outBuf, int byteToRead)
{
	int ret = -1;
	int retry = 0;
	struct i2c_msg I2CMsg[3];
	struct fts_ts_info *info = i2c_get_clientdata(client);
	uint8_t *buf = info->i2c_data;
	uint8_t wr_len = writeCmdLength + readCmdLength + byteToRead;

	/*
	 * Reassign memory for i2c_data in case length is greater than 32 bytes
	 */
	if (info->i2c_data_len < wr_len + 1) {
		kfree(info->i2c_data);
		info->i2c_data = kmalloc(wr_len + 1, GFP_KERNEL);
		if (!info->i2c_data) {
			info->i2c_data_len = 0;
			return -ENOMEM;
		}
		info->i2c_data_len = wr_len + 1;
		buf = info->i2c_data;
	}

	//write msg
	I2CMsg[0].addr = (__u16)I2CSAD;
	I2CMsg[0].flags = (__u16)0;
	I2CMsg[0].len = (__u16)writeCmdLength;
	I2CMsg[0].buf = buf;

	//write msg
	I2CMsg[1].addr = (__u16)I2CSAD;
	I2CMsg[1].flags = (__u16)0;
	I2CMsg[1].len = (__u16)readCmdLength;
	I2CMsg[1].buf = buf + writeCmdLength;

	//read msg
	I2CMsg[2].addr = (__u16)I2CSAD;
	I2CMsg[2].flags = I2C_M_RD;
	I2CMsg[2].len = byteToRead;
	I2CMsg[2].buf = buf + writeCmdLength + readCmdLength;

	memcpy(buf, writeCmd1, writeCmdLength);
	memcpy(buf + writeCmdLength, readCmd1, readCmdLength);

	if (client == NULL)
		return ERROR_I2C_O;
	while (retry < I2C_RETRY && ret < OK) {
		ret = i2c_transfer(client->adapter, I2CMsg, 3);
		retry++;
		if (ret < OK)
			msleep(I2C_WAIT_BEFORE_RETRY);
	}

	if (ret < 0) {
		logError(1, "%s %s: ERROR %02X\n",
			tag, __func__, ERROR_I2C_WR);
		return ERROR_I2C_WR;
	}

	memcpy(outBuf, buf + writeCmdLength + readCmdLength, byteToRead);

	return OK;
}


int readCmdU16(u8 cmd, u16 address, u8 *outBuf, int byteToRead,
	int hasDummyByte)
{
	int remaining = byteToRead;
	int toRead = 0;
	u8 rCmd[3] = { cmd, 0x00, 0x00 };

	u8 *buff = (u8 *)kmalloc_array(READ_CHUNK + 1, sizeof(u8), GFP_KERNEL);

	if (buff == NULL) {
		logError(1, "%s %s: ERROR %02X\n", tag, __func__, ERROR_ALLOC);
		return ERROR_ALLOC;
	}

	while (remaining > 0) {
		if (remaining >= READ_CHUNK) {
			toRead = READ_CHUNK;
			remaining -= READ_CHUNK;
		} else {
			toRead = remaining;
			remaining = 0;
		}

		rCmd[1] = (u8)((address & 0xFF00) >> 8);
		rCmd[2] = (u8)(address & 0xFF);

		if (hasDummyByte) {
			if (fts_readCmd(rCmd, 3, buff, toRead + 1) < 0) {
				logError(1, "%s %s: ERROR %02X\n",
					tag, __func__, ERROR_I2C_R);
				kfree(buff);
				return ERROR_I2C_R;
			}
			memcpy(outBuf, buff + 1, toRead);
		} else {
			if (fts_readCmd(rCmd, 3, buff, toRead) < 0)
				return ERROR_I2C_R;
			memcpy(outBuf, buff, toRead);
		}

		address += toRead;
		outBuf += toRead;
	}
	kfree(buff);

	return OK;
}


int writeCmdU16(u8 WriteCmd, u16 address, u8 *dataToWrite, int byteToWrite)
{
	int remaining = byteToWrite;
	int toWrite = 0;

	u8 *buff = (u8 *)kmalloc_array(WRITE_CHUNK + 3, sizeof(u8), GFP_KERNEL);

	if (buff == NULL) {
		logError(1, "%s %s: ERROR %02X\n", tag, __func__, ERROR_ALLOC);
		return ERROR_ALLOC;
	}

	buff[0] = WriteCmd;

	while (remaining > 0) {
		if (remaining >= WRITE_CHUNK) {
			toWrite = WRITE_CHUNK;
			remaining -= WRITE_CHUNK;
		} else {
			toWrite = remaining;
			remaining = 0;
		}

		buff[1] = (u8)((address & 0xFF00) >> 8);
		buff[2] = (u8)(address & 0xFF);
		memcpy(buff + 3, dataToWrite, toWrite);
		if (fts_writeCmd(buff, 3 + toWrite) < 0) {
			logError(1, "%s %s: ERROR %02\n",
				tag, __func__, ERROR_I2C_W);
			kfree(buff);
			return ERROR_I2C_W;
		}
		address += toWrite;
		dataToWrite += toWrite;
	}

	kfree(buff);
	return OK;
}

int writeCmdU32(u8 writeCmd1, u8 writeCmd2, u32 address, u8 *dataToWrite,
	int byteToWrite)
{
	int remaining = byteToWrite;
	int toWrite = 0;
	int ret;

	u8 buff1[3] = { writeCmd1, 0x00, 0x00 };
	u8 *buff2 = (u8 *)kmalloc_array(WRITE_CHUNK + 3,
				sizeof(u8), GFP_KERNEL);

	if (buff2 == NULL) {
		logError(1, "%s %s: ERROR %02X\n",
			tag, __func__, ERROR_ALLOC);
		return ERROR_ALLOC;
	}
	buff2[0] = writeCmd2;


	while (remaining > 0) {
		if (remaining >= WRITE_CHUNK) {
			toWrite = WRITE_CHUNK;
			remaining -= WRITE_CHUNK;
		} else {
			toWrite = remaining;
			remaining = 0;
		}

		buff1[1] = (u8)((address & 0xFF000000) >> 24);
		buff1[2] = (u8)((address & 0x00FF0000) >> 16);
		buff2[1] = (u8)((address & 0x0000FF00) >> 8);
		buff2[2] = (u8)(address & 0xFF);
		memcpy(buff2 + 3, dataToWrite, toWrite);

		if (fts_writeCmd(buff1, 3) < 0) {
			logError(1, "%s %s: ERROR %02X\n",
				tag, __func__, ERROR_I2C_W);
			ret = ERROR_I2C_W;
			goto END;
		}
		if (fts_writeCmd(buff2, 3 + toWrite) < 0) {
			logError(1, "%s %s: ERROR %02X\n",
				tag, __func__, ERROR_I2C_W);
			ret = ERROR_I2C_W;
			goto END;
		}

		address += toWrite;
		dataToWrite += toWrite;
	}

	ret = OK;
END:
	kfree(buff2);
	return ret;
}

int writeReadCmdU32(u8 wCmd, u8 rCmd, u32 address, u8 *outBuf,
			int byteToRead, int hasDummyByte)
{
	int remaining = byteToRead;
	int toRead = 0;
	u8 reaCmd[3];
	u8 wriCmd[3];

	u8 *buff = (u8 *)kmalloc_array(READ_CHUNK + 1, sizeof(u8), GFP_KERNEL);

	if (buff == NULL) {
		logError(1, "%s writereadCmd32: ERROR %02X\n",
			tag, ERROR_ALLOC);
		return ERROR_ALLOC;
	}

	reaCmd[0] = rCmd;
	wriCmd[0] = wCmd;

	while (remaining > 0) {
		if (remaining >= READ_CHUNK) {
			toRead = READ_CHUNK;
			remaining -= READ_CHUNK;
		} else {
			toRead = remaining;
			remaining = 0;
		}

		wriCmd[1] = (u8)((address & 0xFF000000) >> 24);
		wriCmd[2] = (u8)((address & 0x00FF0000) >> 16);

		reaCmd[1] = (u8)((address & 0x0000FF00) >> 8);
		reaCmd[2] = (u8)(address & 0x000000FF);

		if (hasDummyByte) {
			if (writeReadCmd(wriCmd, 3, reaCmd, 3,
				buff, toRead + 1) < 0) {
				logError(1, "%s writeCmdU32: ERROR %02X\n",
					tag, ERROR_I2C_WR);
				kfree(buff);
				return ERROR_I2C_WR;
			}
			memcpy(outBuf, buff + 1, toRead);
		} else {
			if (writeReadCmd(wriCmd, 3, reaCmd,
				3, buff, toRead) < 0)
				return ERROR_I2C_WR;
			memcpy(outBuf, buff, toRead);
		}

		address += toRead;
		outBuf += toRead;
	}

	kfree(buff);
	return OK;
}
