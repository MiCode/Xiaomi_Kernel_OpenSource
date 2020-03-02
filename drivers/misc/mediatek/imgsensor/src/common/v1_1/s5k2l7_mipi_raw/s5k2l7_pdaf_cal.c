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

#define PFX "s5k2l7_camera_pdaf"
#define pr_fmt(fmt) PFX "[%s] " fmt, __func__

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/atomic.h>
/* #include <linux/xlog.h> */
/* #include <asm/system.h> */

#include <linux/proc_fs.h>


#include <linux/dma-mapping.h>
#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
#include "s5k2l7_pdaf_cal.h"


#include "s5k2l7mipiraw_Sensor.h"
/*********************Modify Following Strings for Debug***********************/


/* #define LOG_1 pr_debug("s5k2l7,MIPI 4LANE\n") */
/* #define LOG_2 pr_debug
 * ("preview 2096*1552@30fps,640Mbps/lane; video 4192*3104@30fps,1.2Gbps/lane;
 * capture 13M@30fps,1.2Gbps/lane\n")
 */
/****************************   Modify end  ***********************************/



/* #include "ov13850_otp.h" */
struct otp_pdaf_struct {
	unsigned char pdaf_flag;	/* bit[7]--0:empty; 1:Valid */
	unsigned char data1[496];	/* output data1 */
	unsigned char data2[806];	/* output data2 */
	unsigned char data3[102];	/* output data3 */

	/* checksum of pd, SUM(0x0801~0x0D7C)%255+1 */
	unsigned char pdaf_checksum;

};


/* #define EEPROM_READ_ID  0xA1 */
/* #define EEPROM_WRITE_ID   0xA0 */
#define I2C_SPEED        400	/* CAT24C512 can support 1Mhz */

#define START_OFFSET     0x800

#define Delay(ms)  mdelay(ms)
/* static unsigned char OV13853MIPI_WRITE_ID = (0xA0 >> 1); */
#define EEPROM_READ_ID  0xA1
#define EEPROM_WRITE_ID   0xA0
#define MAX_OFFSET       0x1e59
#define DATA_SIZE 4096
/* BYTE eeprom_data[DATA_SIZE]= {0}; */
static bool get_done;
static int last_size;
static int last_offset;
static unsigned int eeprom_rd_id;


static bool s5k2l7_selective_read_eeprom(kal_uint16 addr, BYTE *data)
{
	char pu_send_cmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };

	if (addr > MAX_OFFSET)
		return false;

	if (iReadRegI2C(pu_send_cmd, 2, (u8 *) data, 1, eeprom_rd_id) < 0)
		return false;
	return true;
}


static bool s5k2l7_read_eeprom(kal_uint16 addr, BYTE *data, kal_uint32 size)
{
	int i = 0;
	/* int offset = addr; */
	int offset = 0x763;
	/* for(i = 0; i < 1404; i++) { */
	for (i = 0; i < 2048; i++) {
		if (!s5k2l7_selective_read_eeprom(offset, &data[i])) {
			pr_debug("read_eeprom 0x%0x %d fail\n",
				offset, data[i]);

			return false;
		}
		pr_debug("read_eeprom 0x%0x 0x%x.\n", offset, data[i]);
		offset++;
	}
	get_done = true;
	last_size = size;
	last_offset = addr;
	return true;
}

bool s5k2l7_read_otp_pdaf_data(kal_uint16 addr, BYTE *data,
					  kal_uint32 size, kal_uint32 sensor_id)
{

	pr_debug("read_otp_pdaf_data enter [%x]", sensor_id);

	if (sensor_id == 0x20C7)
		eeprom_rd_id = 0xA1;
	else
		eeprom_rd_id = 0xA2;

	if (!get_done || last_size != size || last_offset != addr) {
		if (!s5k2l7_read_eeprom(addr, data, size)) {
			get_done = 0;
			last_size = 0;
			last_offset = 0;
			pr_debug("read_otp_pdaf_data fail");
			return false;
		}
	}
	/* memcpy(data, eeprom_data, size); */
	pr_debug("read_otp_pdaf_data end");
	return true;
}
