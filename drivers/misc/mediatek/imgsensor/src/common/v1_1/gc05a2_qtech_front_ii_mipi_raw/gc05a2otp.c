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

/*****************************************************************************
 *
 * Filename:
 * ---------
 *     gc05a2mipi_Sensor.c
 *
 * Project:
 * --------
 *	 ALPS
 *
 * Description:
 * ------------
 *	 Source code of Sensor driver
 *
 *
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 *
 * Version:  V20220613184323 by GC-S-TEAM
 *

 */
#define PFX "gc05a2_Otp"
#define pr_fmt(fmt) PFX "[%s] " fmt, __func__

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/atomic.h>
#include <linux/types.h>
#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "gc05a2otp.h"

static kal_uint8 gc05a2_slave_addr = 0x7e;

static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char pu_send_cmd[2] = { 
		(char)((addr >> 8) & 0xff), 
		(char)(addr & 0xff) 
	};

	iReadRegI2C(pu_send_cmd, 2, (u8 *)&get_byte, 1, gc05a2_slave_addr);

	return get_byte;
}

static void write_cmos_sensor_8bit(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[3] = { 
		(char)((addr >> 8) & 0xff), 
		(char)(addr & 0xff), 
		(char)(para & 0xff) 
	};

	iWriteRegI2C(pu_send_cmd, 3, gc05a2_slave_addr);
}

/*begin 20220402 add for otp check*/
struct gc05a2_otp_t gc05a2_otp_info;
EXPORT_SYMBOL(gc05a2_otp_info);
void gc05a2_otp_init(void)
{
	write_cmos_sensor_8bit(0x031c, 0x60);
	write_cmos_sensor_8bit(0x0315, 0x80);
	write_cmos_sensor_8bit(0x0af4, 0x01);

	write_cmos_sensor_8bit(0x0af6, 0x00);
	write_cmos_sensor_8bit(0x0b90, 0x10);
	write_cmos_sensor_8bit(0x0b91, 0x00);
	write_cmos_sensor_8bit(0x0b92, 0x00);
	write_cmos_sensor_8bit(0x0ba0, 0x17);
	write_cmos_sensor_8bit(0x0ba1, 0x00);
	write_cmos_sensor_8bit(0x0ba2, 0x00);
	write_cmos_sensor_8bit(0x0ba4, 0x03);
	write_cmos_sensor_8bit(0x0ba5, 0x00);
	write_cmos_sensor_8bit(0x0ba6, 0x00);
	write_cmos_sensor_8bit(0x0ba8, 0x40);
	write_cmos_sensor_8bit(0x0ba9, 0x00);
	write_cmos_sensor_8bit(0x0baa, 0x00);
	write_cmos_sensor_8bit(0x0bac, 0x40);
	write_cmos_sensor_8bit(0x0bad, 0x00);
	write_cmos_sensor_8bit(0x0bae, 0x00);
	write_cmos_sensor_8bit(0x0bb0, 0x02);
	write_cmos_sensor_8bit(0x0bb1, 0x00);
	write_cmos_sensor_8bit(0x0bb2, 0x00);
	write_cmos_sensor_8bit(0x0bb8, 0x02);
	write_cmos_sensor_8bit(0x0bb9, 0x00);
	write_cmos_sensor_8bit(0x0bba, 0x00);
	write_cmos_sensor_8bit(0x0a70, 0x80);
	write_cmos_sensor_8bit(0x0a71, 0x00);
	write_cmos_sensor_8bit(0x0a72, 0x00);
	write_cmos_sensor_8bit(0x0a66, 0x00);
	write_cmos_sensor_8bit(0x0a67, 0x80);
	write_cmos_sensor_8bit(0x0a4d, 0x0e);
	write_cmos_sensor_8bit(0x0a45, 0x02);
	write_cmos_sensor_8bit(0x0a47, 0x02);
	write_cmos_sensor_8bit(0x0a50, 0x00);
	write_cmos_sensor_8bit(0x0a4f, 0x0c);
	mdelay(10);	
}

static void gc05a2_otp_close(void)
{
	write_cmos_sensor_8bit(0x0a70, 0x00);
	write_cmos_sensor_8bit(0x0a67, 0x00);
}

static kal_uint16 gc05a2_otp_read_group(kal_uint16 addr, kal_uint8 *data, kal_uint16 length)
{
	kal_uint16 i = 0;

	write_cmos_sensor_8bit(0x0a67, 0x84);
	write_cmos_sensor_8bit(0x0a69, (addr >> 8) & 0xff);
	write_cmos_sensor_8bit(0x0a6a, addr & 0xff);
	write_cmos_sensor_8bit(0x0a66, 0x20);
	write_cmos_sensor_8bit(0x0a66, 0x12);

	for (i = 0; i < length; i++) {
		data[i] = read_cmos_sensor(0x0a6c);
//#if GC05A2_OTP_DEBUG
	//pr_info("[wpc]GC05A2 OTP ACC Read : addr = 0x%x, data = 0x%x\n", addr + i * 8, data[i]);
//#endif
	}
	return 0;
}

kal_uint16 gc05a2_otp_read_byte(kal_uint16 addr)
{
	kal_uint16 val = 0;
	
	write_cmos_sensor_8bit(0x0a69, (addr >> 8) & 0xff);
	write_cmos_sensor_8bit(0x0a6a, addr & 0xff);
	write_cmos_sensor_8bit(0x0a66, 0x20);

	val = read_cmos_sensor(0x0a6c);
	return val;
}

int gc05a2_iReadData(unsigned int ui4_offset, unsigned int ui4_length, unsigned char *pinputdata)
{
	int i4RetValue = 0;
	int i4ResidueDataLength;
	u32 u4CurrentOffset;
	kal_uint8 *pBuff;


	pr_info("ui4_offset = 0x%x, ui4_length = %d \n", ui4_offset, ui4_length);

	i4ResidueDataLength = (int)ui4_length;
	u4CurrentOffset = ui4_offset;
	pBuff = pinputdata;

	i4RetValue =gc05a2_otp_read_group((kal_uint16) u4CurrentOffset, pBuff, i4ResidueDataLength);
	if (i4RetValue != 0) {
		pr_info("I2C iReadData failed!!\n");
		return -1;
	}


	return 0;
}

static bool check_sum(kal_uint8 *buf, unsigned int size, kal_uint8 chksum)
{
	int i, sum = 0;

	for (i = 0; i < size; i++)
	{
		sum += buf[i];
		//pr_info("buf[%d] = 0x%x %d", i, buf[i], buf[i]);
	}

	if ((sum % 255 + 1) != chksum)
	{
		pr_info("chksum fail size = %d sum=%d (sum)=%d sum-in-eeprom=%d", size, sum , (sum % 255 + 1), chksum);
		return false;
	}
	return true;
}

bool check_gc05a2_otp(void)
{
	//kal_uint8 waferflag =0;
	kal_uint8 moduleflag =0;
	kal_uint8 groupflag =0;
	kal_uint8 groupflag2 =0;
	kal_uint8 awbflag =0;
	kal_uint8 lscflag =0;

	kal_uint8 checksum_module = 0;
	kal_uint8 checksum_awb = 0;
	kal_uint8 checksum_lsc = 0;

	pr_err("robe_debug: %s run.\n", __func__);
    gc05a2_otp_init();

	groupflag = gc05a2_otp_read_byte(OTP_GROUP_FLAG);
	gc05a2_otp_info.group_param[0]=groupflag;
	groupflag2 = gc05a2_otp_read_byte(OTP_GROUP_FLAG2);
	gc05a2_otp_info.group_param1[0]=groupflag2;
	pr_err("wpc read groupflag=0x%0x,groupflag2=0x%0x",groupflag,groupflag2);

	if(groupflag==0x55&&groupflag2!=0x55){
		moduleflag= gc05a2_otp_read_byte(MODULE_GROUP_FLAG);
		awbflag = gc05a2_otp_read_byte(AWB_GROUP_FLAG);
		lscflag = gc05a2_otp_read_byte(LSC_GROUP_FLAG);		
		//for module info otp read
		if ((moduleflag & 0x03) == 0x01) {
			pr_info("wpc group1_module, size %d, block flag 0x01", MODULE_LENGTH);
			gc05a2_iReadData(MODULE_INFO_FLAG, MODULE_LENGTH,&gc05a2_otp_info.module_param[0]);
			gc05a2_iReadData(MODULE_INFO_FLAG + (MODULE_LENGTH-1) * 8, 1, &gc05a2_otp_info.moduleChksum);
		} else if ((moduleflag & 0x0c) == 0x04) {
			pr_info("wpc group2_module, size %d, block flag 0x03", MODULE_LENGTH);
			gc05a2_iReadData(MODULE_INFO_FLAG + GROUP_LENGTH * 8, MODULE_LENGTH,&gc05a2_otp_info.module_param[0]);
			gc05a2_iReadData(MODULE_INFO_FLAG + GROUP_LENGTH * 8 + (MODULE_LENGTH-1) * 8, 1, &gc05a2_otp_info.moduleChksum);
		} else if ((moduleflag & 0x0f) == 0x00) {
			pr_info("wpc module info is empty");
		} else {
			pr_info("invalid block module flag 0x%x", moduleflag);
		}

		gc05a2_iReadData(DATA_INFO_ADDR_START, DATA_INFO_LENGTH,&gc05a2_otp_info.data_info[0]);
		gc05a2_iReadData(DATA_INFO_ADDR_CHKSUM, 1,&gc05a2_otp_info.data_info_chksum);

		//for muduleinfo checksum
		if (check_sum(&gc05a2_otp_info.data_info[0], DATA_INFO_LENGTH, gc05a2_otp_info.data_info_chksum))
		{
			pr_info("[wpc]gc05a2OTP:module flag chksum pass");
			checksum_module = 1;

			pr_info("module id = 0x%x", gc05a2_otp_info.module_param[0]);
			pr_info("Year = 0x%x", gc05a2_otp_info.module_param[1]);
			pr_info("Month = 0x%x", gc05a2_otp_info.module_param[2]);
			pr_info("Day = 0x%x", gc05a2_otp_info.module_param[3]);
			pr_info("LENSID = 0x%x", gc05a2_otp_info.module_param[4]);
			pr_info("VCMID = 0x%x", gc05a2_otp_info.module_param[5]);
			pr_info("DriverICID = 0x%x", gc05a2_otp_info.module_param[6]);
			
		}

		//for awb otp read
		if ((awbflag & 0x03) == 0x01) {
			pr_info("group1_awb, size %d, block flag 0x01", AWB_LENGTH);
			gc05a2_iReadData(AWB_INFO_FLAG, AWB_LENGTH,&gc05a2_otp_info.awb_param[0]);
			gc05a2_iReadData(AWB_INFO_FLAG + (AWB_LENGTH-1) * 8, 1, &gc05a2_otp_info.awbChksum);
		} else if ((awbflag & 0x0c) == 0x04) {
			pr_info("group2_awb, size %d, block flag 0x03", AWB_LENGTH);
			gc05a2_iReadData(AWB_INFO_FLAG + AWB_LENGTH * 8, AWB_LENGTH,&gc05a2_otp_info.awb_param[0]);
			gc05a2_iReadData(AWB_INFO_FLAG + AWB_LENGTH * 8 + (AWB_LENGTH-1) * 8, 1, &gc05a2_otp_info.awbChksum);
		} else if ((awbflag & 0x0f) == 0x00) {
			pr_info("awb info is empty");
		} else {
			pr_info("invalid block awb flag 0x%x", awbflag);
		}

		gc05a2_iReadData(DATA_AWB_ADDR_START, DATA_AWB_LENGTH,&gc05a2_otp_info.data_awb[0]);
		gc05a2_iReadData(DATA_AWB_ADDR_CHKSUM, 1,&gc05a2_otp_info.data_awb_chksum);

		//for awb checksum
		if (check_sum(&gc05a2_otp_info.data_awb[0], DATA_AWB_LENGTH, gc05a2_otp_info.data_awb_chksum))
		{
			checksum_awb = 1;
			pr_info("[wpc]gc05a2OTP:awb flag chksum pass");
		}
		else
		{
			int i;
			for (i = 0; i < AWB_LENGTH-1; i++)
				pr_info("[wpc]gc05a2OTP:awb[%d]=0x%x  %d\n", i, gc05a2_otp_info.awb_param[i], gc05a2_otp_info.awb_param[i]);
		}

		//for lsc otp read
		if ((lscflag & 0x03) == 0x01) {
			pr_info("group1_lsc, size %d, block flag 0x01", LSC_LENGTH);
			gc05a2_iReadData(LSC_INFO_FLAG, LSC_LENGTH,&gc05a2_otp_info.lsc_param[0]);
			gc05a2_iReadData(LSC_INFO_FLAG + (LSC_LENGTH-1) * 8, 1, &gc05a2_otp_info.lscChksum);
		} else if ((lscflag & 0x0c) == 0x04) {
			pr_info("group2_lsc, size %d, block flag 0x03", LSC_LENGTH);
			gc05a2_iReadData(LSC_INFO_FLAG + LSC_LENGTH * 8, LSC_LENGTH,&gc05a2_otp_info.lsc_param[0]);
			gc05a2_iReadData(LSC_INFO_FLAG + LSC_LENGTH * 8 + (LSC_LENGTH-1) * 8, 1, &gc05a2_otp_info.lscChksum);
		} else if ((lscflag & 0x0f) == 0x00) {
			pr_info("lsc info is empty");
		} else {
			pr_info("invalid block lsc flag 0x%x", lscflag);
		}

		gc05a2_iReadData(DATA_LSC_ADDR_START, DATA_LSC_LENGTH,&gc05a2_otp_info.data_lsc[0]);
		gc05a2_iReadData(DATA_LSC_ADDR_CHKSUM, 1,&gc05a2_otp_info.data_lsc_chksum);

		//for lsc checksum
		if (check_sum(&gc05a2_otp_info.data_lsc[0], DATA_LSC_LENGTH, gc05a2_otp_info.data_lsc_chksum))
		{
			checksum_lsc = 1;
			pr_info("[wpc]gc05a2OTP:lsc flag chksum pass");
		}			
	}else{
		//read group2 otp 
		//for module info otp read
		moduleflag= gc05a2_otp_read_byte(MODULE_GROUP_FLAG2);
		awbflag = gc05a2_otp_read_byte(AWB_GROUP_FLAG2);
		lscflag = gc05a2_otp_read_byte(LSC_GROUP_FLAG2);		
		if ((moduleflag & 0x03) == 0x01) {
			pr_info("group1_module, size %d, block flag 0x01", MODULE_LENGTH);
			gc05a2_iReadData(MODULE_INFO_FLAG2, MODULE_LENGTH,&gc05a2_otp_info.module_param[0]);
			gc05a2_iReadData(MODULE_INFO_FLAG2 + (MODULE_LENGTH-1) * 8, 1, &gc05a2_otp_info.moduleChksum);
		} else if ((moduleflag & 0x0c) == 0x04) {
			pr_info("group2_module, size %d, block flag 0x03", MODULE_LENGTH);
			gc05a2_iReadData(MODULE_INFO_FLAG2 + GROUP_LENGTH * 8, MODULE_LENGTH,&gc05a2_otp_info.module_param[0]);
			gc05a2_iReadData(MODULE_INFO_FLAG2 + GROUP_LENGTH * 8 + (MODULE_LENGTH-1) * 8, 1, &gc05a2_otp_info.moduleChksum);
		} else if ((moduleflag & 0x0f) == 0x00) {
			pr_info("module info is empty");
		} else {
			pr_info("invalid block module flag 0x%x", moduleflag);
		}

		gc05a2_iReadData(DATA_INFO2_ADDR_START, DATA_INFO2_LENGTH,&gc05a2_otp_info.data_info2[0]);
		gc05a2_iReadData(DATA_INFO2_ADDR_CHKSUM, 1,&gc05a2_otp_info.data_info2_chksum);

		//for muduleinfo checksum
		if (check_sum(&gc05a2_otp_info.data_info2[0],DATA_INFO2_LENGTH, gc05a2_otp_info.data_info2_chksum))
		{
			pr_info("[wpc]gc05a2OTP:module flag chksum pass");
			checksum_module = 1;

			pr_info("module id = 0x%x", gc05a2_otp_info.module_param[0]);
			pr_info("Year = 0x%x", gc05a2_otp_info.module_param[1]);
			pr_info("Month = 0x%x", gc05a2_otp_info.module_param[2]);
			pr_info("Day = 0x%x", gc05a2_otp_info.module_param[3]);
			pr_info("LENSID = 0x%x", gc05a2_otp_info.module_param[4]);
			pr_info("VCMID = 0x%x", gc05a2_otp_info.module_param[5]);
			pr_info("DriverICID = 0x%x", gc05a2_otp_info.module_param[6]);
			
		}

		//for awb otp read
		if ((awbflag & 0x03) == 0x01) {
			pr_info("group2_awb, size %d, block flag 0x01", AWB_LENGTH);
			gc05a2_iReadData(AWB_INFO_FLAG2, AWB_LENGTH,&gc05a2_otp_info.awb_param[0]);
			gc05a2_iReadData(AWB_INFO_FLAG2 + (AWB_LENGTH-1) * 8, 1, &gc05a2_otp_info.awbChksum);
		} else if ((awbflag & 0x0c) == 0x04) {
			pr_info("group2_awb, size %d, block flag 0x03", AWB_LENGTH);
			gc05a2_iReadData(AWB_INFO_FLAG2 + AWB_LENGTH * 8, AWB_LENGTH,&gc05a2_otp_info.awb_param[0]);
			gc05a2_iReadData(AWB_INFO_FLAG2 + AWB_LENGTH * 8 + (AWB_LENGTH-1) * 8, 1, &gc05a2_otp_info.awbChksum);
		} else if ((awbflag & 0x0f) == 0x00) {
			pr_info("awb info is empty");
		} else {
			pr_info("invalid block awb flag 0x%x", awbflag);
		}

		gc05a2_iReadData(DATA_AWB2_ADDR_START, DATA_AWB2_LENGTH,&gc05a2_otp_info.data_awb2[0]);
		gc05a2_iReadData(DATA_AWB2_ADDR_CHKSUM, 1,&gc05a2_otp_info.data_awb2_chksum);

		//for awb checksum
		if (check_sum(&gc05a2_otp_info.data_awb2[0], DATA_AWB2_LENGTH, gc05a2_otp_info.data_awb2_chksum))
		{
			checksum_awb = 1;
			pr_info("[wpc]gc05a2OTP:awb flag chksum pass");
		}
		else
		{
			int i;
			for (i = 0; i < AWB_LENGTH-1; i++)
				pr_info("[wpc]gc05a2OTP:awb[%d]=0x%x  %d\n", i, gc05a2_otp_info.awb_param[i], gc05a2_otp_info.awb_param[i]);
		}

		//for lsc otp read
		if ((lscflag & 0x03) == 0x01) {
			pr_info("group1_lsc, size %d, block flag 0x01", LSC_LENGTH);
			gc05a2_iReadData(LSC_INFO_FLAG2, LSC_LENGTH,&gc05a2_otp_info.lsc_param[0]);
			gc05a2_iReadData(LSC_INFO_FLAG2 + (LSC_LENGTH-1) * 8, 1, &gc05a2_otp_info.lscChksum);
		} else if ((lscflag & 0x0c) == 0x04) {
			pr_info("group2_lsc, size %d, block flag 0x03", LSC_LENGTH);
			gc05a2_iReadData(LSC_INFO_FLAG2 + LSC_LENGTH * 8, LSC_LENGTH,&gc05a2_otp_info.lsc_param[0]);
			gc05a2_iReadData(LSC_INFO_FLAG2 + LSC_LENGTH * 8 + (LSC_LENGTH-1) * 8, 1, &gc05a2_otp_info.lscChksum);
		} else if ((lscflag & 0x0f) == 0x00) {
			pr_info("lsc info is empty");
		} else {
			pr_info("invalid block lsc flag 0x%x", lscflag);
		}

		gc05a2_iReadData(DATA_LSC2_ADDR_START, DATA_LSC2_LENGTH,&gc05a2_otp_info.data_lsc2[0]);
		gc05a2_iReadData(DATA_LSC2_ADDR_CHKSUM, 1,&gc05a2_otp_info.data_lsc2_chksum);

		//for lsc checksum
		if (check_sum(&gc05a2_otp_info.data_lsc2[0], DATA_LSC2_LENGTH, gc05a2_otp_info.data_lsc2_chksum))
		{
			checksum_lsc = 1;
			pr_info("[wpc]gc05a2OTP:lsc flag chksum pass");
		}		
	}
	

	

    gc05a2_otp_close();
	pr_err("robe_debug: %s end.\n", __func__);
	if (1 == (checksum_module & checksum_awb & checksum_lsc))
	{
		return true;
	}
	else
	{
		pr_info("otp check fail");
		return false;
	}
}

/*end 20220402 add for otp check*/
unsigned int GC05A2_OTP_Read_Data(u16 addr, u8 *data, u32 size)
{
	u8 ManufacturerId[2] = {0};
	int i=0;
	pr_info("wpc GC05A2_OTP_Read_Data, size=%0x,addr=0x%0x\n", size,(u16)addr);

	if (addr == 0x900) { //read CalLayoutTbl match ID
		ManufacturerId[0] = gc05a2_otp_info.group_param[0];
		memcpy(data, ManufacturerId, size);
		pr_info("wpc addr = 0x%x,read data, 0x%x ,ManufacturerId[0]=0x%x\n", addr, data,ManufacturerId[0]);
	}
	else if (addr ==0x8518) { //read CalLayoutTbl match ID
		ManufacturerId[0] = gc05a2_otp_info.group_param1[0];
		memcpy(data, ManufacturerId, size);
		pr_info("wpc addr = 0x%x,read data, 0x%x ,ManufacturerId[0]=0x%x\n", addr, data,ManufacturerId[0]);
	}
	else if (addr ==0x4948) { //read single awb group 1 data
        for(i=0; i<size; i++){
            data[i] = gc05a2_otp_info.awb_param[i];
			pr_info("wpc awb data = 0x%x, read awb\n", data[i]);
        }
	}
	else if (addr == 0x4A08) { //read  single lsc group 1 data
        for(i=0; i<size; i++){
            data[i] = gc05a2_otp_info.lsc_param[i];
        }	
	}
	else if (addr ==0xBE60) { //read single awb group 2 data
        for(i=0; i<size; i++){
            data[i] = gc05a2_otp_info.awb_param[i];
			pr_info("wpc awb data = 0x%x, read awb\n", data[i]);
        }
	}else if (addr == 0xBF20) { //read  single lsc group 2 data
	pr_info("wpc lsc data 11111 = 0x%x, read lsc\n");
        for(i=0; i<size; i++){
            data[i] = gc05a2_otp_info.lsc_param[i];
        }	
	}else if (addr == 0x754) {
        for(i=0; i<size; i++){
            data[i] = gc05a2_otp_info.lsc_param[i];
			pr_info("wpc lsc data = 0x%x, read lsc\n", data[i]);
        }
	}

	return 0;
}
EXPORT_SYMBOL(GC05A2_OTP_Read_Data);
