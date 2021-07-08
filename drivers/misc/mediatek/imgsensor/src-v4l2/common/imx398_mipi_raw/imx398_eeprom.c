// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2019 MediaTek Inc.

#define PFX "imx398_pdafotp"
#define pr_fmt(fmt) PFX "[%s] " fmt, __func__

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
#include "imx398_eeprom.h"

#include "adaptor-subdrv.h"
#include "adaptor-i2c.h"

#define USHORT             unsigned short
#define BYTE               unsigned char
#define Sleep(ms) mdelay(ms)

#define imx398_EEPROM_READ_ID  0xA0
#define imx398_EEPROM_WRITE_ID   0xA1
#define imx398_I2C_SPEED        100
#define imx398_MAX_OFFSET		0xFFFF

#define DATA_SIZE 2048

BYTE imx398_DCC_data[96] = {0};
BYTE imx398_SPC_data[252] = {0};


static bool get_done;
static int last_size;
static int last_offset;

static bool selective_read_eeprom(struct subdrv_ctx *ctx,
		kal_uint16 addr, BYTE *data)
{
	if (addr > imx398_MAX_OFFSET)
		return false;

	if (adaptor_i2c_rd_u8(ctx->i2c_client,
		imx398_EEPROM_READ_ID >> 1,
		addr, data) < 0)
		return false;

	return true;
}

static bool _read_imx398_eeprom(struct subdrv_ctx *ctx,
		kal_uint16 addr, BYTE *data, int size)
{
	int i = 0;
	int offset = addr;

	pr_debug("enter _read_eeprom size = %d\n", size);

	for (i = 0; i < size; i++) {
		if (!selective_read_eeprom(ctx, offset, &data[i]))
			return false;

		pr_debug("read_eeprom 0x%0x %d\n", offset, data[i]);
		offset++;
	}
	get_done = true;
	last_size = size;
	last_offset = addr;
	return true;
}


void read_imx398_SPC(struct subdrv_ctx *ctx, BYTE *data)
{
	int size = 252;

	pr_debug("read imx398 SPC, size = %d\n", size);
	/**********************************************************
	 * if(!get_done || last_size != size || last_offset != addr) {
	 * if(!_read_imx398_eeprom(ctx, addr, imx398_SPC_data, size)){
	 * get_done = 0;
	 * last_size = 0;
	 * last_offset = 0;
	 * return false;
	 * }
	 * }
	 **********************************************************/
	memcpy(data, imx398_SPC_data, size);
}


void read_imx398_DCC(struct subdrv_ctx *ctx,
		kal_uint16 addr, BYTE *data, kal_uint32 size)
{

	addr = 0x960;
	size = 96;

	pr_debug("read imx398 DCC, size = %d\n", size);

	if (!get_done || last_size != size || last_offset != addr) {
		if (!_read_imx398_eeprom(ctx, addr, imx398_DCC_data, size)) {
			get_done = 0;
			last_size = 0;
			last_offset = 0;

		}
	}

	memcpy(data, imx398_DCC_data, size);

}
