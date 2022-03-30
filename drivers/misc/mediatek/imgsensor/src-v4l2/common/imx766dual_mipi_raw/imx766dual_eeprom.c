// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2019 MediaTek Inc.
/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 imx766dual_eeprom.c
 *
 * Project:
 * --------
 * Description:
 * ------------
 *	 Add APIs to read from EEPROM
 *
 ****************************************************************************/
#define PFX "IMX766DUAL_pdafotp"
#define LOG_INF(format, args...) pr_debug(PFX "[%s] " format, __func__, ##args)

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/atomic.h>
#include <linux/slab.h>

#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define_v4l2.h"
#include "kd_imgsensor_errcode.h"
#include "imx766dualmipiraw_Sensor.h"

#include "adaptor-subdrv.h"
#include "adaptor-i2c.h"

#define Sleep(ms) mdelay(ms)

#define IMX766DUAL_EEPROM_READ_ID  0xA2
#define IMX766DUAL_EEPROM_WRITE_ID 0xA3
#define IMX766DUAL_I2C_SPEED       100
#define IMX766DUAL_MAX_OFFSET      0x4000

#define MTK_IDENTITY_VALUE 0x010B00FF
#define LRC_SIZE 140
#define DCC_SIZE 96

struct EEPROM_PDAF_INFO {
	kal_uint16 LRC_addr;
	unsigned int LRC_size;
	kal_uint16 DCC_addr;
	unsigned int DCC_size;
};

enum EEPROM_PDAF_INFO_FMT {
	MTK_FMT = 0,
	OP_FMT,
	FMT_MAX
};

static struct EEPROM_PDAF_INFO eeprom_pdaf_info[] = {
	{/* MTK_FMT */
		.LRC_addr = 0x14FE,
		.LRC_size = LRC_SIZE,
		.DCC_addr = 0x763,
		.DCC_size = DCC_SIZE
	},
	{/* OP_FMT */
		.LRC_addr = 0x700,
		.LRC_size = LRC_SIZE,
		.DCC_addr = 0xB06,
		.DCC_size = DCC_SIZE
	},
};

static DEFINE_MUTEX(gimx766dual_eeprom_mutex);

static bool selective_read_eeprom(struct subdrv_ctx *ctx, kal_uint16 addr, BYTE *data)
{
	if (addr > IMX766DUAL_MAX_OFFSET)
		return false;

	if (adaptor_i2c_rd_u8(ctx->i2c_client,
			IMX766DUAL_EEPROM_READ_ID >> 1, addr, data) < 0) {
		return false;
	}
	return true;
}

static bool read_imx766dual_eeprom(struct subdrv_ctx *ctx, kal_uint16 addr, BYTE *data, int size)
{
	int i = 0;
	int offset = addr;

	for (i = 0; i < size; i++) {
		if (!selective_read_eeprom(ctx, offset, &data[i]))
			return false;
		offset++;
	}
	return true;
}

static struct EEPROM_PDAF_INFO *get_eeprom_pdaf_info(struct subdrv_ctx *ctx)
{
	static struct EEPROM_PDAF_INFO *pinfo;
	BYTE read_data[4];

	mutex_lock(&gimx766dual_eeprom_mutex);
	if (pinfo == NULL) {
		read_imx766dual_eeprom(ctx, 0x1, read_data, 4);
		if (((read_data[3] << 24) |
		     (read_data[2] << 16) |
		     (read_data[1] << 8) |
		     read_data[0]) == MTK_IDENTITY_VALUE) {
			pinfo = &eeprom_pdaf_info[MTK_FMT];
		} else {
			pinfo = &eeprom_pdaf_info[OP_FMT];
		}
	}
	mutex_unlock(&gimx766dual_eeprom_mutex);

	return pinfo;
}

unsigned int read_imx766dual_LRC(struct subdrv_ctx *ctx, BYTE *data)
{
	static BYTE IMX766DUAL_LRC_data[LRC_SIZE] = { 0 };
	static unsigned int readed_size;
	struct EEPROM_PDAF_INFO *pinfo = get_eeprom_pdaf_info(ctx);

	LOG_INF("read imx766dual LRC, addr = %d, size = %u\n",
		pinfo->LRC_addr, pinfo->LRC_size);

	mutex_lock(&gimx766dual_eeprom_mutex);
	if ((readed_size == 0) &&
	    read_imx766dual_eeprom(ctx, pinfo->LRC_addr,
			       IMX766DUAL_LRC_data, pinfo->LRC_size)) {
		readed_size = pinfo->LRC_size;
	}
	mutex_unlock(&gimx766dual_eeprom_mutex);

	memcpy(data, IMX766DUAL_LRC_data, pinfo->LRC_size);
	return readed_size;
}

unsigned int read_imx766dual_DCC(struct subdrv_ctx *ctx, BYTE *data)
{
	static BYTE IMX766DUAL_DCC_data[DCC_SIZE] = { 0 };
	static unsigned int readed_size;
	struct EEPROM_PDAF_INFO *pinfo = get_eeprom_pdaf_info(ctx);

	LOG_INF("read imx766dual DCC, addr = %d, size = %u\n",
		pinfo->DCC_addr, pinfo->DCC_size);

	mutex_lock(&gimx766dual_eeprom_mutex);
	if ((readed_size == 0) &&
	    read_imx766dual_eeprom(ctx, pinfo->DCC_addr,
			       IMX766DUAL_DCC_data, pinfo->DCC_size)) {
		readed_size = pinfo->DCC_size;
	}
	mutex_unlock(&gimx766dual_eeprom_mutex);

	memcpy(data, IMX766DUAL_DCC_data, pinfo->DCC_size);
	return readed_size;
}

