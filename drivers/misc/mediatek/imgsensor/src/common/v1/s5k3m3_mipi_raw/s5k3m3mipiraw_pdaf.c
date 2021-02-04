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


/****************************Modify Following Strings for Debug***************/
#define PFX "S5K3M3PDAF"
#define pr_fmt(fmt) PFX "[%s] " fmt, __func__

/****************************	 Modify end    ******************************/

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


/*===FEATURE SWITH===*/
/* #define FPTPDAFSUPPORT   //for pdaf switch */

/*===FEATURE SWITH===*/



#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
#include "s5k3m3mipiraw_pdaf.h"


#define USHORT             unsigned short
#define BYTE               unsigned char
#define Sleep(ms)          mdelay(ms)

/**************  CONFIG BY SENSOR >>> ************/
#define EEPROM_WRITE_ID   0xa0
#define I2C_SPEED        100
#define MAX_OFFSET		    0xFFFF
#define DATA_SIZE         1404
#define START_ADDR        0X0763
BYTE S5K3M3_eeprom_data[DATA_SIZE] = { 0 };

/**************  CONFIG BY SENSOR <<< ************/

static kal_uint16 read_cmos_sensor_byte(kal_uint16 addr)
{
	kal_uint16 get_byte = 0;
	char pu_send_cmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };

	/* kdSetI2CSpeed(I2C_SPEED);*/
	/*Add this func to set i2c speed by each sensor */
	iReadRegI2C(pu_send_cmd, 2, (u8 *) &get_byte, 1, EEPROM_WRITE_ID);
	return get_byte;
}

static bool _read_eeprom(kal_uint16 addr, kal_uint32 size)
{
	/* continue read reg by byte: */
	int i = 0;

	for (; i < size; i++) {
		S5K3M3_eeprom_data[i] = read_cmos_sensor_byte(addr + i);
	/* pr_debug("add = 0x%x,\tvalue = 0x%x",i, S5K3M3_eeprom_data[i]); */
	}
	return true;
}

bool S5K3M3_read_eeprom(kal_uint16 addr, BYTE *data, kal_uint32 size)
{
	addr = START_ADDR;
	size = DATA_SIZE;

	/* pr_debug("Read EEPROM, addr = 0x%x, size = 0d%d\n", addr, size); */

	if (!_read_eeprom(addr, size)) {
		/* pr_debug("error:read_eeprom fail!\n"); */
		return false;
	}

	memcpy(data, S5K3M3_eeprom_data, size);
	return true;
}
