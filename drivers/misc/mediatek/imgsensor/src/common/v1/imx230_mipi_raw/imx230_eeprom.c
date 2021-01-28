// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
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
#define SPC_START_ADDR (0x763 + 96)
#define DCC_START_ADDR 0x448

BYTE IMX230_DCC_data[96] = { 0 };

static bool get_done_dcc;
static int last_size_dcc;

static bool get_done_spc;
static int last_size_spc;


#if 0
static bool selective_read_eeprom(kal_uint16 addr, BYTE *data)
{
	char pu_send_cmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };

	if (addr > IMX230_MAX_OFFSET)
		return false;

	if (iReadRegI2C(pu_send_cmd,
		2,
		(u8 *) data,
		1,
		IMX230_EEPROM_READ_ID) < 0)
		return false;
	return true;
}
#endif
static bool _read_imx230_eeprom(kal_uint16 addr, BYTE *data, int size)
{
	int i = 0;
	int offset = addr;
	int ret;
	u8 pu_send_cmd[2];

	#define MAX_READ_WRITE_SIZE 255
	for (i = 0; i < size; i += MAX_READ_WRITE_SIZE) {
		pu_send_cmd[0] = (u8) (offset >> 8);
		pu_send_cmd[1] = (u8) (offset & 0xFF);

		if (i + MAX_READ_WRITE_SIZE > size) {
			ret = iReadRegI2C(pu_send_cmd, 2,
					 (u8 *) (data + i),
					 (size - i),
					 IMX230_EEPROM_READ_ID);

		} else {
			ret = iReadRegI2C(pu_send_cmd, 2,
					 (u8 *) (data + i),
					 MAX_READ_WRITE_SIZE,
					 IMX230_EEPROM_READ_ID);
		}
		if (ret < 0) {
			pr_debug("read spc failed!\n");
			return false;
		}

		offset += MAX_READ_WRITE_SIZE;
	}

	if (addr == SPC_START_ADDR) {
		get_done_spc = true;
		last_size_spc = size;
	} else {
		get_done_dcc = true;
		last_size_dcc = size;
	}
	pr_debug("exit _read_eeprom size = %d\n", size);
	return true;
}


void read_imx230_SPC(BYTE *data)
{

	int addr = SPC_START_ADDR;
	int size = 352;

	pr_debug("read imx230 SPC, size = %d\n", size);

#if 1
	if (!get_done_spc || last_size_spc != size) {
		if (!_read_imx230_eeprom(addr, data, size)) {
			get_done_spc = 0;
			last_size_spc = 0;
		}
	}
#endif
	/* return true; */
}


void read_imx230_DCC(kal_uint16 addr, BYTE *data, kal_uint32 size)
{
	/* int i; */
	addr = DCC_START_ADDR;
	size = 96;

	pr_debug("read imx230 DCC, size = %d\n", size);

	if (!get_done_dcc || last_size_dcc != size) {
		if (!_read_imx230_eeprom(addr, IMX230_DCC_data, size)) {
			get_done_dcc = 0;
			last_size_dcc = 0;
		}
	}

	memcpy(data, IMX230_DCC_data, size);
	/* return true; */
}
