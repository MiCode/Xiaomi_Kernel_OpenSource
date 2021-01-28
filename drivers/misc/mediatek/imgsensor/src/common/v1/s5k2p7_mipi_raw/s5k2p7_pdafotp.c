// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define PFX "S5K2P7_pdafotp"
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
#include "s5k2p7_pdafotp.h"





#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"


#define USHORT             unsigned short
#define BYTE               unsigned char
#define Sleep(ms) mdelay(ms)

#define S5K2P7_EEPROM_READ_ID  0xA1
#define S5K2P7_EEPROM_WRITE_ID   0xA0
#define S5K2P7_I2C_SPEED        100
#define S5K2P7_MAX_OFFSET		0xFFFF

#define DATA_SIZE 2048
BYTE s5k2P7_eeprom_data[DATA_SIZE] = { 0 };

static bool get_done;
static int last_size;
static int last_offset;


static bool selective_read_eeprom(kal_uint16 addr, BYTE *data)
{
	char pu_send_cmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };

	if (addr > S5K2P7_MAX_OFFSET)
		return false;

	if (iReadRegI2C(pu_send_cmd, 2, (u8 *) data,
			1, S5K2P7_EEPROM_WRITE_ID) < 0)
		return false;


	return true;
}

static bool _read_2P7_eeprom(kal_uint16 addr, BYTE *data, kal_uint32 size)
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

bool read_2P7_eeprom(kal_uint16 addr, BYTE *data, kal_uint32 size)
{
	addr = 0x763;
	size = 1404;
	/* BYTE header[9]= {0}; */
	/* _read_2P7_eeprom(0x0000, header, 9); */

	pr_debug("read 2P7 eeprom, size = %d\n", size);

	if (!get_done || last_size != size || last_offset != addr) {
		if (!_read_2P7_eeprom(addr, s5k2P7_eeprom_data, size)) {
			get_done = 0;
			last_size = 0;
			last_offset = 0;
			return false;
		}
	}

	memcpy(data, s5k2P7_eeprom_data, size);
	return true;
}
