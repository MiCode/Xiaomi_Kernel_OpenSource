/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#define IMX258_PDAFOTP_DEBUG
#ifdef IMX258_PDAFOTP_DEBUG
#define PFX "IMX258_pdafotp"
#define pr_fmt(fmt) PFX "[%s] " fmt, __func__
#endif

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
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
#include "imx258_pdafotp.h"

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


static bool selective_read_eeprom(kal_uint16 addr, BYTE *data)
{
	char pu_send_cmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };

	if (addr > IMX258_MAX_OFFSET)
		return false;

if (iReadRegI2C(pu_send_cmd, 2, (u8 *) data, 1, IMX258_EEPROM_READ_ID) < 0) {
	/*20171116ken : fix coding style*/
	return false;
}

	return true;
}

static bool _read_imx258_eeprom(kal_uint16 addr, BYTE *data, kal_uint32 size)
{
	int i = 0;
	int offset = addr;

	for (i = 0; i < size; i++) {
		if (!selective_read_eeprom(offset, &data[i]))
			return false;
		pr_debug("read_eeprom 0x%0x %d\n", offset, data[i]);
		offset++;
	}
	get_done = true;
	last_size = size;
	last_offset = addr;
	return true;
}

bool read_imx258_pdaf(kal_uint16 addr, BYTE *data, kal_uint32 size)
{
	addr = 0x0763;
	size = 1404;

	pr_debug("read imx258 eeprom, size = %d\n", size);

	if (!get_done || last_size != size || last_offset != addr) {
		if (!_read_imx258_eeprom(addr, imx258_eeprom_data, size)) {
			get_done = 0;
			last_size = 0;
			last_offset = 0;
			return false;
		}
	}

	memcpy(data, imx258_eeprom_data, size);
	return true;
}

bool read_imx258_eeprom(kal_uint16 addr, BYTE *data, kal_uint32 size)
{
	addr = 0x0763;
	size = 1404;

	pr_debug("read imx258 eeprom, size = %d\n", size);

	if (!get_done || last_size != size || last_offset != addr) {
		if (!_read_imx258_eeprom(addr, imx258_eeprom_data, size)) {
			get_done = 0;
			last_size = 0;
			last_offset = 0;
			return false;
		}
	}

	memcpy(data, imx258_eeprom_data, size);
	return true;
}

bool read_imx258_eeprom_SPC(kal_uint16 addr, BYTE *data, kal_uint32 size)
{

	addr = 0x0F6D;		/* 0x0F73; */
	size = 126;

	pr_debug("read imx258 eeprom, size = %d\n", size);

	if (!get_done || last_size != size || last_offset != addr) {
		if (!_read_imx258_eeprom(addr, imx258_eeprom_data, size)) {
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
