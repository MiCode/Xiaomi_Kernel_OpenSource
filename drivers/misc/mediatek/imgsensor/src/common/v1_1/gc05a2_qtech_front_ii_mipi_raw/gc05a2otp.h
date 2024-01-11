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
 *     gc05a2mipi_Sensor.h
 *
 * Project:
 * --------
 *     ALPS
 *
 * Description:
 * ------------
 *     CMOS sensor header file
 *
 ****************************************************************************/
#ifndef _GC05A2MIPI_OTP_H
#define _GC05A2MIPI_OTP_H

#define GC05A2_MULTI_WRITE             1
#if GC05A2_MULTI_WRITE
#define I2C_BUFFER_LEN  765 /* Max is 255, each 3 bytes */
#else
#define I2C_BUFFER_LEN  3
#endif

#define GC05A2_OTP_DEBUG 0
#define GC05A2_I2C_SPEED 400

#define OTP_START_ADDR            0x0000
#define OTP_DATA_LENGTH           8192 //8192

/*begin 20220402 add for otp check*/
#define OTP_GROUP_FLAG 0x900
#define OTP_GROUP_FLAG2 0x8518

#define MODULE_GROUP_FLAG 0x908
#define MODULE_GROUP_FLAG2 0x8520

#define AWB_GROUP_FLAG 0x4928
#define AWB_GROUP_FLAG2 0xBE40

#define LSC_GROUP_FLAG 0x49E0
#define LSC_GROUP_FLAG2 0x2018


//#define WAFER_INFO_FLAG 0x2000
#define MODULE_INFO_FLAG 0x0910
#define AWB_INFO_FLAG 0x4948
#define LSC_INFO_FLAG 0x4A08

#define MODULE_INFO_FLAG2 0x8528
#define AWB_INFO_FLAG2 0xBE60
#define LSC_INFO_FLAG2 0xBF20


#define GROUP_LENGTH 1899

//#define WAFER_LENGTH 15
#define MODULE_LENGTH 8
#define AWB_LENGTH 16
#define LSC_LENGTH 1868

/* 20231012 longcheer zhangfeng5 edit. add checksum code begin */
#define DATA_BIT_TO_LENGTH  	8

#define DATA_INFO_ADDR_START   	0x0910
#define DATA_INFO_ADDR_END  	0x0A20
#define DATA_INFO_ADDR_CHKSUM  	0x0A28
#define DATA_INFO_LENGTH  		(((DATA_INFO_ADDR_END -DATA_INFO_ADDR_START )/DATA_BIT_TO_LENGTH)+1)

#define DATA_AWB_ADDR_START   	0x4930
#define DATA_AWB_ADDR_END  		0x49C0
#define DATA_AWB_ADDR_CHKSUM  	0x49C8
#define DATA_AWB_LENGTH  		(((DATA_AWB_ADDR_END -DATA_AWB_ADDR_START )/DATA_BIT_TO_LENGTH)+1)

#define DATA_LSC_ADDR_START   	0x49E8
#define DATA_LSC_ADDR_END  		0x8460
#define DATA_LSC_ADDR_CHKSUM  	0x8468
#define DATA_LSC_LENGTH  		(((DATA_LSC_ADDR_END -DATA_LSC_ADDR_START )/DATA_BIT_TO_LENGTH)+1)

#define DATA_INFO2_ADDR_START   	0x8528
#define DATA_INFO2_ADDR_END  		0x8638
#define DATA_INFO2_ADDR_CHKSUM  	0x8640
#define DATA_INFO2_LENGTH  			(((DATA_INFO2_ADDR_END -DATA_INFO2_ADDR_START )/DATA_BIT_TO_LENGTH)+1)

#define DATA_AWB2_ADDR_START   		0xBE48
#define DATA_AWB2_ADDR_END  		0xBED8
#define DATA_AWB2_ADDR_CHKSUM  		0xBEE0
#define DATA_AWB2_LENGTH  		(((DATA_AWB2_ADDR_END -DATA_AWB2_ADDR_START )/DATA_BIT_TO_LENGTH)+1)

#define DATA_LSC2_ADDR_START   		0xBF00
#define DATA_LSC2_ADDR_END  		0xF978
#define DATA_LSC2_ADDR_CHKSUM  		0xF980
#define DATA_LSC2_LENGTH  		(((DATA_LSC2_ADDR_END -DATA_LSC2_ADDR_START )/DATA_BIT_TO_LENGTH)+1)
/* 20231012 longcheer zhangfeng5 edit. add checksum code end */

struct gc05a2_otp_t {
	kal_uint8  group_flag;
	kal_uint8  group_param[1];	
	kal_uint8  group_param1[1];
	kal_uint8  module_flag;
	kal_uint8  module_param[7];	
		kal_uint8  moduleChksum;
	kal_uint8  awb_flag;
	kal_uint8  awb_param[16];
        kal_uint8  awbChksum;
	kal_uint8  lsc_flag;
	kal_uint8  lsc_param[1898];
        kal_uint8  lscChksum;
	kal_uint8  af_flag;
	kal_uint8  af_param[8];
        kal_uint8  afChksum;
	/* 20231012 longcheer zhangfeng5 edit. add checksum code begin */
	kal_uint8  data_info[DATA_INFO_LENGTH];
	kal_uint8  data_info_chksum;
	kal_uint8  data_awb[DATA_AWB_LENGTH];
	kal_uint8  data_awb_chksum;
	kal_uint8  data_lsc[DATA_LSC_LENGTH];
	kal_uint8  data_lsc_chksum;
	kal_uint8  data_info2[DATA_INFO2_LENGTH];
	kal_uint8  data_info2_chksum;
	kal_uint8  data_awb2[DATA_AWB2_LENGTH];
	kal_uint8  data_awb2_chksum;
	kal_uint8  data_lsc2[DATA_LSC2_LENGTH];
	kal_uint8  data_lsc2_chksum;
	/* 20231012 longcheer zhangfeng5 edit. add checksum code end */
};
/*end 20220402 add for otp check*/
unsigned int gc05a2_read_all_data(void);
extern int iReadRegI2C(u8 *a_pSendData, u16 a_sizeSendData,
	u8 *a_pRecvData, u16 a_sizeRecvData,
		       u16 i2cId);
extern int iWriteRegI2C(u8 *a_pSendData, u16 a_sizeSendData, u16 i2cId);
#endif
