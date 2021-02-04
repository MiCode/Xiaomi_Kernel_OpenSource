/*
 * Copyright (C) 2016 MediaTek Inc.
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

#define PFX "IMX230_pdafotp"
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

/*
 * #define LOG_INF(format, args...) pr_debug(\
 * PFX "[%s] " format, __func__, ##args)
 */


#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
#include "imx230mipi_Sensor.h"


#define USHORT             unsigned short
#define BYTE               unsigned char
#define Sleep(ms) mdelay(ms)

#define IMX230_EEPROM_READ_ID  0xA0
#define IMX230_EEPROM_WRITE_ID   0xA1
#define IMX230_I2C_SPEED        100
#define IMX230_MAX_OFFSET		0xFFFF

#define DATA_SIZE 2048

BYTE IMX230_DCC_data[96] = { 0 };
BYTE IMX230_SPC_data[352] = { 0 };


static bool get_done;
static int last_size;
static int last_offset;


static bool selective_read_eeprom(kal_uint16 addr, BYTE *data)
{
	char pu_send_cmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };

	if (addr > IMX230_MAX_OFFSET)
		return false;

if (iReadRegI2C(pu_send_cmd, 2, (u8 *) data, 1, IMX230_EEPROM_READ_ID) < 0) {
	/*20171213 : fix coding style*/
	return false;
}
	return true;
}

static bool _read_imx230_eeprom(kal_uint16 addr, BYTE *data, int size)
{
	int i = 0;
	int offset = addr;

	pr_debug("enter _read_eeprom size = %d\n", size);
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


void read_imx230_SPC(BYTE *data)
{

	int addr = 0x763 + 96;
	int size = 352;

	pr_debug("read imx230 SPC, size = %d\n", size);
#if 1
	if (!get_done || last_size != size || last_offset != addr) {
		if (!_read_imx230_eeprom(addr, IMX230_SPC_data, size)) {
			get_done = 0;
			last_size = 0;
			last_offset = 0;
		}
	}
#endif
	memcpy(data, IMX230_SPC_data, size);
	/* return true; */
}


void read_imx230_DCC(kal_uint16 addr, BYTE *data, kal_uint32 size)
{
	/* int i; */
	addr = 0x448;
	size = 96;

	pr_debug("read imx230 DCC, size = %d\n", size);

	if (!get_done || last_size != size || last_offset != addr) {
		if (!_read_imx230_eeprom(addr, IMX230_DCC_data, size)) {
			get_done = 0;
			last_size = 0;
			last_offset = 0;
			/* return false; */
		}
	}

	memcpy(data, IMX230_DCC_data, size);
	/* return true; */
}
