/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#define PFX "CAM_CAL"
#define pr_fmt(fmt) PFX "[%s] " fmt, __func__


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/of.h>
#include "cam_cal.h"
#include "cam_cal_define.h"
#include "cam_cal_list.h"
#include <linux/dma-mapping.h>
#ifdef CONFIG_COMPAT
/* 64 bit */
#include <linux/fs.h>
#include <linux/compat.h>
#endif

static DEFINE_SPINLOCK(g_spinLock);
static struct i2c_client *g_pstI2CclientG;

/************************************************************
 * I2C read function (Custom)
 * Customer's driver can put on here
 * Below is an example
 ************************************************************/
 #define PAGE_SIZE_ 256
static int iReadRegI2C(u8 *a_pSendData, u16 a_sizeSendData,
		u8 *a_pRecvData, u16 a_sizeRecvData, u16 i2cId)
{
	int  i4RetValue = 0;

	spin_lock(&g_spinLock);
	g_pstI2CclientG->addr = (i2cId >> 1);

	spin_unlock(&g_spinLock);
	i4RetValue = i2c_master_send(g_pstI2CclientG,
		a_pSendData, a_sizeSendData);
	if (i4RetValue != a_sizeSendData) {
		pr_debug("I2C send failed!!, Addr = 0x%x\n", a_pSendData[0]);
		return -1;
	}
	i4RetValue = i2c_master_recv(g_pstI2CclientG,
		(char *)a_pRecvData, a_sizeRecvData);
	if (i4RetValue != a_sizeRecvData) {
		pr_debug("I2C read failed!!\n");
		return -1;
	}
	return 0;
}

static int custom_read_region(u32 addr, u8 *data, u16 i2c_id, u32 size)
{
	u8 *buff = data;
	u32 size_to_read = size;

	int ret = 0;

	while (size_to_read > 0) {
		u8 page = addr / PAGE_SIZE_;
		u8 offset = addr % PAGE_SIZE_;
		char *Buff = data;

		if (iReadRegI2C(&offset, 1, (u8 *)Buff, 1,
			i2c_id + (page << 1)) < 0) {
			pr_debug("fail addr=0x%x 0x%x, P=%d, offset=0x%x",
				addr, *Buff, page, offset);
			break;
		}
		addr++;
		buff++;
		size_to_read--;
		ret++;
	}
	pr_debug("addr =%x size %d data read = %d\n", addr, size, ret);
	return ret;
}



unsigned int Custom_read_region(struct i2c_client *client, unsigned int addr,
				unsigned char *data, unsigned int size)
{
	g_pstI2CclientG = client;
	if (custom_read_region(addr, data, g_pstI2CclientG->addr, size) == 0)
		return size;
	else
		return 0;
}


