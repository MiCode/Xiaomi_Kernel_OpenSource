// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define PFX "S5K3P8_pdafotp"
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
/* #include <linux/xlog.h> */


/* #define LOG_INF(fmt, args...) pr_debug(PFX "[%s] " fmt, __func__, ##args)*/
#include "kd_camera_typedef.h"
/* #include "kd_camera_hw.h "*/
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
#include "s5k3P8_pdafotp.h"

#define USHORT             unsigned short
#define BYTE               unsigned char
#define Sleep(ms) mdelay(ms)

#define S5K3P8_EEPROM_READ_ID  0xA3
#define S5K3P8_EEPROM_WRITE_ID   0xA2
#define S5K3P8_I2C_SPEED        100
#define S5K3P8_MAX_OFFSET		0xFFFF

#define DATA_SIZE 2048
BYTE S5K3P8_eeprom_data[DATA_SIZE] = {0};
static bool get_done;
static int last_size;
static int last_offset;


static bool selective_read_eeprom(kal_uint16 addr, BYTE *data)
{
	char pu_send_cmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };

	if (addr > S5K3P8_MAX_OFFSET)
		return false;
	kdSetI2CSpeed(S5K3P8_I2C_SPEED);

	if (iReadRegI2C(pu_send_cmd, 2, (u8 *)data, 1,
		S5K3P8_EEPROM_WRITE_ID) < 0)
		return false;


	return true;
}

static bool _read_S5K3P8_eeprom(kal_uint16 addr, BYTE *data, kal_uint32 size)
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

bool read_3P8_eeprom(kal_uint16 addr, BYTE *data, kal_uint32 size)
{
	#ifdef VENDOR_EDIT
	/*zhengjiang.zhu@Camera.driver, 2017/06/30 add for camera otp*/
	int i = 0;
	int proc1_flag = 0;
	int proc2_flag = 0;
	int proc3_flag = 0;

	addr = 0x1400;
	size = 0x1A67-0x1400;
	#else
	addr = 0x0801;
	size = 1404;
	#endif

	pr_debug("read 3P8 eeprom, size = %d\n", size);

	if (!get_done || last_size != size || last_offset != addr) {
		if (!_read_S5K3P8_eeprom(addr, S5K3P8_eeprom_data, size)) {
			get_done = 0;
	    last_size = 0;
	    last_offset = 0;
			return false;
		}
	}
	#ifdef VENDOR_EDIT
	/*zhengjiang.zhu@Camera.driver, 2017/06/30 add for camera otp*/
	proc1_flag = S5K3P8_eeprom_data[0x15F0-0x1400];
	proc2_flag = S5K3P8_eeprom_data[0x1926-0x1400];
	proc3_flag = S5K3P8_eeprom_data[0x1A66-0x1400];

	for (i = 0; i < 496; i++) {
		data[i] = S5K3P8_eeprom_data[i];
		pr_debug("data[%d] = %x\n", i, data[i]);
	}

	/* proc2 data */
	for (i = 496; i < 1302; i++) {
		data[i] = S5K3P8_eeprom_data[i+16];
		pr_debug("data[%d] = %x\n", i, data[i]);
	}
	/* proc3 data */
	for (i = 1302; i < 1404; i++) {
		data[i] = S5K3P8_eeprom_data[i+16+218];
		pr_debug("data[%d] = %x\n", i, data[i]);
	}
	#else
	memcpy(data, S5K3P8_eeprom_data, size);
	#endif
	return true;
}


