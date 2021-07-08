// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/atomic.h>
#include <linux/slab.h>

#define IMX258_PDAFOTP_DEBUG

#ifdef IMX258_PDAFOTP_DEBUG
#define PFX "IMX258_pdafotp"
#define LOG_INF(format, args...) \
	pr_debug(PFX "[%s] " format, __func__, ##args)
#else
#define LOG_INF(format, args...)
#endif

#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define_v4l2.h"
#include "kd_imgsensor_errcode.h"
#include "imx258_pdafotp.h"

#include "adaptor-subdrv.h"
#include "adaptor-i2c.h"

#define USHORT             unsigned short
#define BYTE               unsigned char
#define Sleep(ms) mdelay(ms)

#define IMX258_EEPROM_READ_ID  0xA0
#define IMX258_EEPROM_WRITE_ID   0xA1
#define IMX258_I2C_SPEED        100
#define IMX258_MAX_OFFSET		0xFFFF

#define DATA_SIZE 2048
BYTE imx258_eeprom_data[DATA_SIZE] = { 0 };

static bool get_done;
static int last_size;
static int last_offset;

static bool selective_read_eeprom(struct subdrv_ctx *ctx,
		kal_uint16 addr, BYTE *data)
{
	if (addr > IMX258_MAX_OFFSET)
		return false;
	if (adaptor_i2c_rd_u8(ctx->i2c_client,
		IMX258_EEPROM_READ_ID >> 1, addr, data) < 0)
		return false;
	return true;
}

static bool _read_imx258_eeprom(struct subdrv_ctx *ctx,
		kal_uint16 addr, BYTE *data, kal_uint32 size)
{
	int i = 0;
	int offset = addr;

	for (i = 0; i < size; i++) {
		if (!selective_read_eeprom(ctx, offset, &data[i]))
			return false;
		LOG_INF("read_eeprom 0x%0x %d\n", offset, data[i]);
		offset++;
	}
	get_done = true;
	last_size = size;
	last_offset = addr;
	return true;
}

bool read_imx258_pdaf(struct subdrv_ctx *ctx,
		kal_uint16 addr, BYTE *data, kal_uint32 size)
{
	addr = 0x0763;
	size = 1404;

	LOG_INF("read imx258 eeprom, size = %d\n", size);

	if (!get_done || last_size != size || last_offset != addr) {
		if (!_read_imx258_eeprom(ctx, addr, imx258_eeprom_data, size)) {
			get_done = 0;
			last_size = 0;
			last_offset = 0;
			return false;
		}
	}

	memcpy(data, imx258_eeprom_data, size);
	return true;
}

bool read_imx258_eeprom(struct subdrv_ctx *ctx,
		kal_uint16 addr, BYTE *data, kal_uint32 size)
{
	addr = 0x0763;
	size = 1404;

	LOG_INF("read imx258 eeprom, size = %d\n", size);

	if (!get_done || last_size != size || last_offset != addr) {
		if (!_read_imx258_eeprom(ctx, addr, imx258_eeprom_data, size)) {
			get_done = 0;
			last_size = 0;
			last_offset = 0;
			return false;
		}
	}

	memcpy(data, imx258_eeprom_data, size);
	return true;
}

bool read_imx258_eeprom_SPC(struct subdrv_ctx *ctx,
		kal_uint16 addr, BYTE *data, kal_uint32 size)
{

	addr = 0x0F6D;		/* 0x0F73; */
	size = 126;

	LOG_INF("read imx258 eeprom, size = %d\n", size);

	if (!get_done || last_size != size || last_offset != addr) {
		if (!_read_imx258_eeprom(ctx, addr, imx258_eeprom_data, size)) {
			get_done = 0;
			last_size = 0;
			last_offset = 0;
			return false;
		}
	}
	/* memset(imx258_eeprom_data, 0x01, size); */
	memcpy(data, imx258_eeprom_data, size);
	return true;
}

