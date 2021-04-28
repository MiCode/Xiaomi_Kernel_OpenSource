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

/*****************************************************************************
 *
 * Filename:
 * ---------
 *     s5k2l7_setting_mode1.h
 *
 * Project:
 * --------
 *     ALPS
 *
 * Description:
 * ------------
 *     CMOS sensor setting file
 *
 * Setting Release Date:
 * ------------
 *     2016.09.01
 *
 ****************************************************************************/
#ifndef _s5k2l7MIPI_SETTING_MODE1_V2_H_
#define _s5k2l7MIPI_SETTING_MODE1_V2_H_

#define _S5K2L7_MODE1_V2_SENSOR_INFO_                                          \
static struct imgsensor_info_struct _imgsensor_info_m1_v2 =                    \
{                                                                              \
	/* record sensor id defined in Kd_imgsensor.h */                       \
	.sensor_id = S5K2L7_SENSOR_ID,                                         \
	.checksum_value = 0xb4cb9203,/* checksum value for Camera Auto Test  */\
	.pre = {                                                               \
		.pclk = 960000000, /* record different mode's pclk */          \
		.linelength = 10160, /* record different mode's linelength */  \
		.framelength = 3149, /* record different mode's framelength */ \
		.startx = 0, /* record different mode's startx of grabwindow */\
		.starty = 0, /* record different mode's starty of grabwindow */\
	/* Dual PD: need to tg grab width / 2, p1 drv will * 2 itself */       \
		.grabwindow_width = 2016,                                      \
		/* record different mode's height of grabwindow */             \
		.grabwindow_height = 1512,                                     \
		.mipi_data_lp2hs_settle_dc = 85, /* unit , ns */               \
		.max_framerate = 300                                           \
	},                                                                     \
	.cap = {                                                               \
		.pclk = 960000000, /* record different mode's pclk */          \
		.linelength = 10256, /* record different mode's linelength */  \
		.framelength = 3120, /* record different mode's framelength */ \
		.startx = 0, /* record different mode's startx of grabwindow */\
		.starty = 0, /* record different mode's starty of grabwindow */\
	/* Dual PD: need to tg grab width / 2, p1 drv will * 2 itself */       \
		.grabwindow_width = 4032,                                      \
		/* record different mode's height of grabwindow */             \
		.grabwindow_height = 3024,                                     \
		.mipi_data_lp2hs_settle_dc = 85, /* unit , ns */               \
		.max_framerate = 300                                           \
	},                                                                     \
	.cap1 = {                                                              \
		.pclk = 960000000, /* record different mode's pclk  */         \
		.linelength = 10256, /* record different mode's linelength  */ \
		.framelength = 3120, /* record different mode's framelength */ \
		.startx = 0, /* record different mode's startx of grabwindow */\
		.starty = 0, /* record different mode's starty of grabwindow */\
	/* Dual PD: need to tg grab width / 2, p1 drv will * 2 itself */       \
		.grabwindow_width = 4032,                                      \
		/* record different mode's height of grabwindow */             \
		.grabwindow_height = 3024,                                     \
		.mipi_data_lp2hs_settle_dc = 85, /* unit , ns */               \
		.max_framerate = 300                                           \
	},                                                                     \
	.normal_video = {                                                      \
		.pclk = 960000000, /* record different mode's pclk  */         \
		.linelength = 10256, /* record different mode's linelength */  \
		.framelength = 3120, /* record different mode's framelength */ \
		.startx = 0, /* record different mode's startx of grabwindow */\
		.starty = 0, /* record different mode's starty of grabwindow */\
	/* Dual PD: need to tg grab width / 2, p1 drv will * 2 itself */       \
		.grabwindow_width = 4032,                                      \
		/* record different mode's height of grabwindow */             \
		.grabwindow_height = 3024,                                     \
		.mipi_data_lp2hs_settle_dc = 85,  /* unit , ns */              \
		.max_framerate = 300                                           \
	},                                                                     \
	.hs_video = {                                                          \
		.pclk = 960000000,                                             \
		.linelength = 10160,                                           \
		.framelength = 1049,                                           \
		.startx = 0,                                                   \
		.starty = 0,                                                   \
		/* record different mode's width of grabwindow */              \
		.grabwindow_width = 1344,                                      \
		/* record different mode's height of grabwindow */             \
		.grabwindow_height = 756,                                      \
		.mipi_data_lp2hs_settle_dc = 85, /* unit , ns */               \
		.max_framerate = 900,                                          \
	},                                                                     \
	.slim_video = {                                                        \
		.pclk = 960000000,                                             \
		.linelength = 10160,                                           \
		.framelength = 3149,                                           \
		.startx = 0,                                                   \
		.starty = 0,                                                   \
		/* record different mode's width of grabwindow */              \
		.grabwindow_width = 1344,                                      \
		/* record different mode's height of grabwindow */             \
		.grabwindow_height = 756,                                      \
		.mipi_data_lp2hs_settle_dc = 85, /* unit , ns */               \
		.max_framerate = 300,                                          \
	},                                                                     \
	.margin = 16,                                                          \
	.min_shutter = 1,                                                      \
	.max_frame_length = 0xffff,                                            \
	.ae_shut_delay_frame = 0,                                              \
	.ae_sensor_gain_delay_frame = 0,                                       \
	.ae_ispGain_delay_frame = 2,                                           \
	.ihdr_support = 0,       /* 1, support; 0,not support */               \
	.ihdr_le_firstline = 0,  /* 1,le first ; 0, se first */                \
	.sensor_mode_num = 5,    /* support sensor mode num */                 \
	.cap_delay_frame = 3,                                                  \
	.pre_delay_frame = 3,                                                  \
	.video_delay_frame = 3,                                                \
	.hs_video_delay_frame = 3,                                             \
	.slim_video_delay_frame = 3,                                           \
	.isp_driving_current = ISP_DRIVING_8MA,                                \
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,                   \
	/* 0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2 */                             \
	.mipi_sensor_type = MIPI_OPHY_NCSI2,                                   \
	/* 0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL */              \
	.mipi_settle_delay_mode = MIPI_SETTLEDELAY_AUTO,                       \
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_Gr,               \
	.mclk = 24,                                                            \
	.mipi_lane_num = SENSOR_MIPI_4_LANE,                                   \
	.i2c_addr_table = { 0x20, 0x5A, 0xFF},                                 \
	.i2c_speed = 300,                                                      \
}

/* full_w; full_h; x0_offset; y0_offset; w0_size; h0_size; scale_w; scale_h;
 * x1_offset;  y1_offset;  w1_size;  h1_size;
 * x2_tg_offset;	 y2_tg_offset;	w2_tg_size;  h2_tg_size;
 */
#define struct_SENSOR_WINSIZE_INFO struct SENSOR_WINSIZE_INFO_STRUCT
#define _S5K2L7_MODE1_V2_WINSIZE_INFO_                                         \
static struct_SENSOR_WINSIZE_INFO _imgsensor_winsize_info_m1_v2[5] =           \
{                                                                              \
	{ 4032, 3024, 0,   0, 4032, 3024, 2016,                                \
	  1512, 0, 0, 2016, 1512, 0, 0, 2016, 1512}, /* Preview */             \
	{ 4032, 3024, 0,   0, 4032, 3024, 4032,                                \
	  3024, 0, 0, 4032, 3024, 0, 0, 4032, 3024}, /* capture */             \
	{ 4032, 3024, 0,   0, 4032, 3024, 4032,                                \
	  3024, 0, 0, 4032, 3024, 0, 0, 4032, 3024}, /* normal_video */        \
	{ 4032, 3024, 0, 378, 4032, 2268, 1344,                                \
	   756, 0, 0, 1344,  756, 0, 0, 1344,  756}, /* hs_video */            \
	{ 4032, 3024, 0, 378, 4032, 2268, 1344,                                \
	   756, 0, 0, 1336,  756, 0, 0, 1344,  756}, /* slim_video */          \
}

#define _SET_MODE1_V2_SENSOR_INFO_AND_WINSIZE_ do {                            \
	memcpy((void *)&imgsensor_info,                                        \
		(void *)&_imgsensor_info_m1_v2,                                \
		sizeof(struct imgsensor_info_struct));                         \
	memcpy((void *)&imgsensor_winsize_info,                                \
		(void *)&_imgsensor_winsize_info_m1_v2,                        \
	       sizeof(struct SENSOR_WINSIZE_INFO_STRUCT)*5);                   \
} while (0)


/*****************************************************************************
 *
 * Description:
 * ------------
 *     mode 1 initial setting
 *
 ****************************************************************************/
#define _S5K2L7_MODE1_INIT_MODULE_V2_ do {         \
	write_cmos_sensor_twobyte(0X6028, 0X2000); \
	write_cmos_sensor_twobyte(0X602A, 0XBBF4); \
	write_cmos_sensor_twobyte(0X6F12, 0X0000); \
	write_cmos_sensor_twobyte(0X6028, 0X4000); \
	write_cmos_sensor_twobyte(0X6010, 0X0001); \
	mdelay(3);                                 \
	write_cmos_sensor_twobyte(0X6214, 0X7970); \
	write_cmos_sensor_twobyte(0X6218, 0X7150); \
	write_cmos_sensor_twobyte(0X6028, 0X4000); \
	write_cmos_sensor_twobyte(0XF466, 0X000C); \
	write_cmos_sensor_twobyte(0XF468, 0X000D); \
	write_cmos_sensor_twobyte(0XF488, 0X0008); \
	write_cmos_sensor_twobyte(0XF414, 0X0007); \
	write_cmos_sensor_twobyte(0XF416, 0X0004); \
	write_cmos_sensor_twobyte(0X30C6, 0X0100); \
	write_cmos_sensor_twobyte(0X30CA, 0X0300); \
	write_cmos_sensor_twobyte(0X30C8, 0X05DC); \
	write_cmos_sensor_twobyte(0X6B36, 0X5200); \
	write_cmos_sensor_twobyte(0X6B38, 0X0000); \
	write_cmos_sensor_twobyte(0X0B04, 0X0101); \
	write_cmos_sensor_twobyte(0X3094, 0X2800); \
	write_cmos_sensor_twobyte(0X3096, 0X5400); \
	write_cmos_sensor_twobyte(0X6028, 0X2000); \
	write_cmos_sensor_twobyte(0X602A, 0X16C0); \
	write_cmos_sensor_twobyte(0X6F12, 0X004B); \
	write_cmos_sensor_twobyte(0X6F12, 0X004B); \
	write_cmos_sensor_twobyte(0X602A, 0X09E4); \
	write_cmos_sensor_twobyte(0X6F12, 0XF484); \
	write_cmos_sensor_twobyte(0X602A, 0X16BA); \
	write_cmos_sensor_twobyte(0X6F12, 0X2608); \
	write_cmos_sensor_twobyte(0X602A, 0X16BE); \
	write_cmos_sensor_twobyte(0X6F12, 0X2608); \
	write_cmos_sensor_twobyte(0X602A, 0X15F4); \
	write_cmos_sensor_twobyte(0X6F12, 0X0001); \
	write_cmos_sensor_twobyte(0X6F12, 0X0101); \
	write_cmos_sensor_twobyte(0X602A, 0X1282); \
	write_cmos_sensor_twobyte(0X6F12, 0X0013); \
	write_cmos_sensor_twobyte(0X602A, 0X127A); \
	write_cmos_sensor_twobyte(0X6F12, 0X0009); \
	write_cmos_sensor_twobyte(0X602A, 0X14C8); \
	write_cmos_sensor_twobyte(0X6F12, 0X00BE); \
	write_cmos_sensor_twobyte(0X602A, 0X15EE); \
	write_cmos_sensor_twobyte(0X6F12, 0X0107); \
	write_cmos_sensor_twobyte(0X602A, 0X4C64); \
	write_cmos_sensor_twobyte(0X6F12, 0X0101); \
	write_cmos_sensor_twobyte(0X602A, 0X4C6A); \
	write_cmos_sensor_twobyte(0X6F12, 0X0011); \
	write_cmos_sensor_twobyte(0X602A, 0X09A2); \
	write_cmos_sensor_twobyte(0X6F12, 0X00F8); \
	write_cmos_sensor_twobyte(0X6F12, 0X007F); \
	write_cmos_sensor_twobyte(0X6F12, 0X00FF); \
	write_cmos_sensor_twobyte(0X6F12, 0X0084); \
	write_cmos_sensor_twobyte(0X602A, 0X09B0); \
	write_cmos_sensor_twobyte(0X6F12, 0X0011); \
	write_cmos_sensor_twobyte(0X6F12, 0X0013); \
	write_cmos_sensor_twobyte(0X6F12, 0XD0D0); \
	write_cmos_sensor_twobyte(0X6F12, 0X0011); \
	write_cmos_sensor_twobyte(0X6F12, 0X0013); \
	write_cmos_sensor_twobyte(0X6F12, 0XD8D0); \
	write_cmos_sensor_twobyte(0X602A, 0X09E6); \
	write_cmos_sensor_twobyte(0X6F12, 0X0000); \
	write_cmos_sensor_twobyte(0X6F12, 0X0101); \
	write_cmos_sensor_twobyte(0X6F12, 0XD004); \
	write_cmos_sensor_twobyte(0X6F12, 0X0000); \
	write_cmos_sensor_twobyte(0X6F12, 0X0101); \
	write_cmos_sensor_twobyte(0X6F12, 0XD804); \
	write_cmos_sensor_twobyte(0X602A, 0X0AF4); \
	write_cmos_sensor_twobyte(0X6F12, 0X0004); \
	write_cmos_sensor_twobyte(0X602A, 0X4B2E); \
	write_cmos_sensor_twobyte(0X6F12, 0X00CC); \
	write_cmos_sensor_twobyte(0X602A, 0X4B60); \
	write_cmos_sensor_twobyte(0X6F12, 0X0133); \
	write_cmos_sensor_twobyte(0X602A, 0X4B92); \
	write_cmos_sensor_twobyte(0X6F12, 0X00CC); \
	write_cmos_sensor_twobyte(0X602A, 0X4B56); \
	write_cmos_sensor_twobyte(0X6F12, 0X0000); \
	write_cmos_sensor_twobyte(0X602A, 0X4B88); \
	write_cmos_sensor_twobyte(0X6F12, 0X0000); \
	write_cmos_sensor_twobyte(0X602A, 0X4B4C); \
	write_cmos_sensor_twobyte(0X6F12, 0X0000); \
	write_cmos_sensor_twobyte(0X602A, 0X4B7E); \
	write_cmos_sensor_twobyte(0X6F12, 0X0000); \
	write_cmos_sensor_twobyte(0X602A, 0X4B42); \
	write_cmos_sensor_twobyte(0X6F12, 0X0000); \
	write_cmos_sensor_twobyte(0X602A, 0X4B74); \
	write_cmos_sensor_twobyte(0X6F12, 0X0000); \
	write_cmos_sensor_twobyte(0X602A, 0X15EC); \
	write_cmos_sensor_twobyte(0X6F12, 0X0001); \
	write_cmos_sensor_twobyte(0X602A, 0X0AEC); \
	write_cmos_sensor_twobyte(0X6F12, 0X0207); \
	write_cmos_sensor_twobyte(0X602A, 0X0B02); \
	write_cmos_sensor_twobyte(0X6F12, 0X0100); \
	write_cmos_sensor_twobyte(0X602A, 0X0BD8); \
	write_cmos_sensor_twobyte(0X6F12, 0X06E0); \
	/* out pedestal 32 */                      \
} while (0)


/*****************************************************************************
 *
 * Description:
 * ------------
 *     mode 1 preview setting
 *     $MIPI[Width:4032,Height:1512,Format:Raw10,Lane:4,ErrorCheck:0,
 *     PolarityData:0,PolarityClock:0,Buffer:4,DataRate:2034,useEmbData:0]
 *     $MV1[MCLK:24,Width:4032,Height:1512,
 *     Format:MIPI_Raw10,mipi_lane:4,mipi_datarate:2034,pvi_pclk_inverse:0]
 *
 ****************************************************************************/
#define _S5K2L7_MODE1_PREVIEW_MODULE_V2_ do {          \
	write_cmos_sensor_twobyte(0X6028, 0X4000);     \
	write_cmos_sensor_twobyte(0X6214, 0X7970);     \
	write_cmos_sensor_twobyte(0X6218, 0X7150);     \
	write_cmos_sensor_twobyte(0X0344, 0X0000);     \
	write_cmos_sensor_twobyte(0X0346, 0X0000);     \
	write_cmos_sensor_twobyte(0X0348, 0X1FBF);     \
	write_cmos_sensor_twobyte(0X034A, 0X0BDF);     \
	write_cmos_sensor_twobyte(0X034C, 0X0FC0);     \
	write_cmos_sensor_twobyte(0X034E, 0X05E8);     \
	write_cmos_sensor_twobyte(0X0408, 0X0010);     \
	write_cmos_sensor_twobyte(0X040A, 0X0004);     \
	write_cmos_sensor_twobyte(0X0900, 0X0122);     \
	write_cmos_sensor_twobyte(0X0380, 0X0001);     \
	write_cmos_sensor_twobyte(0X0382, 0X0003);     \
	write_cmos_sensor_twobyte(0X0384, 0X0001);     \
	write_cmos_sensor_twobyte(0X0386, 0X0003);     \
	write_cmos_sensor_twobyte(0X0400, 0X0000);     \
	write_cmos_sensor_twobyte(0X0404, 0X0010);     \
	write_cmos_sensor_twobyte(0X3060, 0X0100);     \
	write_cmos_sensor_twobyte(0X0114, 0X0300);     \
	write_cmos_sensor_twobyte(0X0110, 0X1002);     \
	write_cmos_sensor_twobyte(0X0136, 0X1800);     \
	write_cmos_sensor_twobyte(0X0304, 0X0006);     \
	write_cmos_sensor_twobyte(0X0306, 0X01E0);     \
	write_cmos_sensor_twobyte(0X0302, 0X0001);     \
	write_cmos_sensor_twobyte(0X0300, 0X0004);     \
	write_cmos_sensor_twobyte(0X030C, 0X0001);     \
	write_cmos_sensor_twobyte(0X030E, 0X0004);     \
	write_cmos_sensor_twobyte(0X0310, 0X00A9);     \
	write_cmos_sensor_twobyte(0X0312, 0X0000);     \
	write_cmos_sensor_twobyte(0X030A, 0X0001);     \
	write_cmos_sensor_twobyte(0X0308, 0X0008);     \
	write_cmos_sensor_twobyte(0X0342, 0X27B0);     \
	write_cmos_sensor_twobyte(0X0340, 0X0C4D);     \
	write_cmos_sensor_twobyte(0X021E, 0X0000);     \
	write_cmos_sensor_twobyte(0X3098, 0X0400);     \
	write_cmos_sensor_twobyte(0X309A, 0X0002);     \
	write_cmos_sensor_twobyte(0X30BC, 0X0031);     \
	write_cmos_sensor_twobyte(0X30A6, 0X0000);     \
	write_cmos_sensor_twobyte(0X30A8, 0X0000);     \
	write_cmos_sensor_twobyte(0X30AA, 0X0000);     \
	write_cmos_sensor_twobyte(0X30AC, 0X0000);     \
	write_cmos_sensor_twobyte(0X30A0, 0X0000);     \
	write_cmos_sensor_twobyte(0X30A4, 0X0000);     \
	write_cmos_sensor_twobyte(0X6A0C, 0XFFFF);     \
	write_cmos_sensor_twobyte(0XF41E, 0X2180);     \
	write_cmos_sensor_twobyte(0X6028, 0X2000);     \
	write_cmos_sensor_twobyte(0X602A, 0X0990);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0020);     \
	write_cmos_sensor_twobyte(0X602A, 0X0AF8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0008);     \
	write_cmos_sensor_twobyte(0X602A, 0X27B8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0000);     \
	write_cmos_sensor_twobyte(0X602A, 0X2AA8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0001);     \
	write_cmos_sensor_twobyte(0X602A, 0X1698);     \
	write_cmos_sensor_twobyte(0X6F12, 0X00A0);     \
	write_cmos_sensor_twobyte(0X602A, 0X169C);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0028);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0030);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0C18);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0C18);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0C28);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0C28);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0C20);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0C20);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0C30);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0C30);     \
	write_cmos_sensor_twobyte(0X602A, 0X16C6);     \
	write_cmos_sensor_twobyte(0X6F12, 0X12AF);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0328);     \
	write_cmos_sensor_twobyte(0X602A, 0X1604);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0004);     \
	write_cmos_sensor_twobyte(0X602A, 0X14CA);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0096);     \
	write_cmos_sensor_twobyte(0X602A, 0X16CE);     \
	write_cmos_sensor_twobyte(0X6F12, 0X06C0);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0101);     \
	write_cmos_sensor_twobyte(0X602A, 0X13AA);     \
	write_cmos_sensor_twobyte(0X6F12, 0X02A0);     \
	write_cmos_sensor_twobyte(0X602A, 0X13A2);     \
	write_cmos_sensor_twobyte(0X6F12, 0X028C);     \
	write_cmos_sensor_twobyte(0X602A, 0X139A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0278);     \
	write_cmos_sensor_twobyte(0X602A, 0X1392);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0264);     \
	write_cmos_sensor_twobyte(0X602A, 0X138A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0250);     \
	write_cmos_sensor_twobyte(0X602A, 0X1382);     \
	write_cmos_sensor_twobyte(0X6F12, 0X023C);     \
	write_cmos_sensor_twobyte(0X602A, 0X137A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0228);     \
	write_cmos_sensor_twobyte(0X602A, 0X1372);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0214);     \
	write_cmos_sensor_twobyte(0X602A, 0X136A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0200);     \
	write_cmos_sensor_twobyte(0X602A, 0X1362);     \
	write_cmos_sensor_twobyte(0X6F12, 0X01EC);     \
	write_cmos_sensor_twobyte(0X602A, 0X135A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X01D8);     \
	write_cmos_sensor_twobyte(0X602A, 0X1352);     \
	write_cmos_sensor_twobyte(0X6F12, 0X01C4);     \
	write_cmos_sensor_twobyte(0X602A, 0X134A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X01B0);     \
	write_cmos_sensor_twobyte(0X602A, 0X1342);     \
	write_cmos_sensor_twobyte(0X6F12, 0X029B);     \
	write_cmos_sensor_twobyte(0X602A, 0X133A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0291);     \
	write_cmos_sensor_twobyte(0X602A, 0X1332);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0287);     \
	write_cmos_sensor_twobyte(0X602A, 0X132A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X027D);     \
	write_cmos_sensor_twobyte(0X602A, 0X1322);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0273);     \
	write_cmos_sensor_twobyte(0X602A, 0X131A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0269);     \
	write_cmos_sensor_twobyte(0X602A, 0X1312);     \
	write_cmos_sensor_twobyte(0X6F12, 0X025F);     \
	write_cmos_sensor_twobyte(0X602A, 0X130A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0255);     \
	write_cmos_sensor_twobyte(0X602A, 0X1302);     \
	write_cmos_sensor_twobyte(0X6F12, 0X024B);     \
	write_cmos_sensor_twobyte(0X602A, 0X12FA);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0241);     \
	write_cmos_sensor_twobyte(0X602A, 0X12F2);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0237);     \
	write_cmos_sensor_twobyte(0X602A, 0X12EA);     \
	write_cmos_sensor_twobyte(0X6F12, 0X022D);     \
	write_cmos_sensor_twobyte(0X602A, 0X12E2);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0223);     \
	write_cmos_sensor_twobyte(0X602A, 0X12DA);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0219);     \
	write_cmos_sensor_twobyte(0X602A, 0X12D2);     \
	write_cmos_sensor_twobyte(0X6F12, 0X020F);     \
	write_cmos_sensor_twobyte(0X602A, 0X12CA);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0205);     \
	write_cmos_sensor_twobyte(0X602A, 0X12C2);     \
	write_cmos_sensor_twobyte(0X6F12, 0X01FB);     \
	write_cmos_sensor_twobyte(0X602A, 0X12BA);     \
	write_cmos_sensor_twobyte(0X6F12, 0X01F1);     \
	write_cmos_sensor_twobyte(0X602A, 0X12B2);     \
	write_cmos_sensor_twobyte(0X6F12, 0X01E7);     \
	write_cmos_sensor_twobyte(0X602A, 0X12AA);     \
	write_cmos_sensor_twobyte(0X6F12, 0X01DD);     \
	write_cmos_sensor_twobyte(0X602A, 0X12A2);     \
	write_cmos_sensor_twobyte(0X6F12, 0X01D3);     \
	write_cmos_sensor_twobyte(0X602A, 0X129A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X01C9);     \
	write_cmos_sensor_twobyte(0X602A, 0X1292);     \
	write_cmos_sensor_twobyte(0X6F12, 0X01BF);     \
	write_cmos_sensor_twobyte(0X602A, 0X128A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X01B5);     \
	write_cmos_sensor_twobyte(0X602A, 0X11E2);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0258);     \
	write_cmos_sensor_twobyte(0X602A, 0X0C32);     \
	write_cmos_sensor_twobyte(0X6F12, 0X000A);     \
	write_cmos_sensor_twobyte(0X602A, 0X0B16);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0100);     \
	write_cmos_sensor_twobyte(0X602A, 0X0C20);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0361);     \
	write_cmos_sensor_twobyte(0X602A, 0X0D78);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0361);     \
	write_cmos_sensor_twobyte(0X602A, 0X0D98);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0361);     \
	write_cmos_sensor_twobyte(0X602A, 0X0DA8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0361);     \
	write_cmos_sensor_twobyte(0X602A, 0X0A96);     \
	write_cmos_sensor_twobyte(0X6F12, 0X1000);     \
	write_cmos_sensor_twobyte(0X602A, 0X0AE4);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0FE0);     \
	write_cmos_sensor_twobyte(0X602A, 0X29E6);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0FE0);     \
	write_cmos_sensor_twobyte(0X602A, 0X29F0);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0FE0);     \
	write_cmos_sensor_twobyte(0X602A, 0X2966);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0FE0);     \
	write_cmos_sensor_twobyte(0X602A, 0X29A6);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0FE0);     \
	write_cmos_sensor_twobyte(0X602A, 0X2970);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0FE0);     \
	write_cmos_sensor_twobyte(0X602A, 0X29B0);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0FE0);     \
	write_cmos_sensor_twobyte(0X602A, 0X0AE6);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0BE0);     \
	write_cmos_sensor_twobyte(0X602A, 0X29E8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0BE0);     \
	write_cmos_sensor_twobyte(0X602A, 0X29F2);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0BE0);     \
	write_cmos_sensor_twobyte(0X602A, 0X2968);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0BE0);     \
	write_cmos_sensor_twobyte(0X602A, 0X29A8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0BE0);     \
	write_cmos_sensor_twobyte(0X602A, 0X2972);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0BE0);     \
	write_cmos_sensor_twobyte(0X602A, 0X29B2);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0BE0);     \
	write_cmos_sensor_twobyte(0X602A, 0X0BB0);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0800);     \
	write_cmos_sensor_twobyte(0X602A, 0X0B62);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0146);     \
	write_cmos_sensor_twobyte(0X602A, 0X5160);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0100);     \
	write_cmos_sensor_twobyte(0X602A, 0X09F2);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0180);     \
	write_cmos_sensor_twobyte(0X6F12, 0X2180);     \
	write_cmos_sensor_twobyte(0X6F12, 0XF41E);     \
	write_cmos_sensor_twobyte(0X602A, 0X0B06);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0800);     \
	write_cmos_sensor_twobyte(0X602A, 0X5694);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0000);     \
	write_cmos_sensor_twobyte(0X602A, 0X569C);     \
	write_cmos_sensor_twobyte(0X6F12, 0X000C);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0100);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0200);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0100);     \
	write_cmos_sensor_twobyte(0X602A, 0X56D8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0000);     \
	write_cmos_sensor_twobyte(0X602A, 0X56E0);     \
	write_cmos_sensor_twobyte(0X6F12, 0X000C);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0100);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0200);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0100);     \
	write_cmos_sensor_twobyte(0X6028, 0X4000);     \
	write_cmos_sensor_twobyte(0X6214, 0X79F0);     \
	write_cmos_sensor_twobyte(0X6218, 0X79F0);     \
	sensor_WDR_zhdr();                             \
} while (0)


/*****************************************************************************
 *
 * Description:
 * ------------
 *     mode 1 capture setting (M1_fullsize_setting)
 *     $MIPI[Width:8064,Height:3024,Format:Raw10,Lane:4,ErrorCheck:0,
 *     PolarityData:0,PolarityClock:0,Buffer:4,DataRate:2034,useEmbData:0]
 *     $MV1[MCLK:24,Width:8064,Height:3024,
 *     Format:MIPI_Raw10,mipi_lane:4,mipi_datarate:2034,pvi_pclk_inverse:0]
 *
 ****************************************************************************/
#define _S5K2L7_MODE1_CAPTURE_MODULE_V2_ do {          \
	write_cmos_sensor_twobyte(0X6028, 0X4000);     \
	write_cmos_sensor_twobyte(0X6214, 0X7970);     \
	write_cmos_sensor_twobyte(0X6218, 0X7150);     \
	write_cmos_sensor_twobyte(0X0344, 0X0000);     \
	write_cmos_sensor_twobyte(0X0346, 0X0000);     \
	write_cmos_sensor_twobyte(0X0348, 0X1FBF);     \
	write_cmos_sensor_twobyte(0X034A, 0X0BDF);     \
	write_cmos_sensor_twobyte(0X034C, 0X1F80);     \
	write_cmos_sensor_twobyte(0X034E, 0X0BD0);     \
	write_cmos_sensor_twobyte(0X0408, 0X0020);     \
	write_cmos_sensor_twobyte(0X040A, 0X0008);     \
	write_cmos_sensor_twobyte(0X0900, 0X0011);     \
	write_cmos_sensor_twobyte(0X0380, 0X0001);     \
	write_cmos_sensor_twobyte(0X0382, 0X0001);     \
	write_cmos_sensor_twobyte(0X0384, 0X0001);     \
	write_cmos_sensor_twobyte(0X0386, 0X0001);     \
	write_cmos_sensor_twobyte(0X0400, 0X0000);     \
	write_cmos_sensor_twobyte(0X0404, 0X0010);     \
	write_cmos_sensor_twobyte(0X3060, 0X0100);     \
	write_cmos_sensor_twobyte(0X0114, 0X0300);     \
	write_cmos_sensor_twobyte(0X0110, 0X1002);     \
	write_cmos_sensor_twobyte(0X0136, 0X1800);     \
	write_cmos_sensor_twobyte(0X0304, 0X0006);     \
	write_cmos_sensor_twobyte(0X0306, 0X01E0);     \
	write_cmos_sensor_twobyte(0X0302, 0X0001);     \
	write_cmos_sensor_twobyte(0X0300, 0X0004);     \
	write_cmos_sensor_twobyte(0X030C, 0X0001);     \
	write_cmos_sensor_twobyte(0X030E, 0X0004);     \
	write_cmos_sensor_twobyte(0X0310, 0X014F);     \
	write_cmos_sensor_twobyte(0X0312, 0X0000);     \
	write_cmos_sensor_twobyte(0X030A, 0X0001);     \
	write_cmos_sensor_twobyte(0X0308, 0X0008);     \
	write_cmos_sensor_twobyte(0X0342, 0X2810);     \
	write_cmos_sensor_twobyte(0X0340, 0X0C30);     \
	write_cmos_sensor_twobyte(0X021E, 0X0000);     \
	write_cmos_sensor_twobyte(0X3098, 0X0400);     \
	write_cmos_sensor_twobyte(0X309A, 0X0002);     \
	write_cmos_sensor_twobyte(0X30BC, 0X0031);     \
	write_cmos_sensor_twobyte(0X30A6, 0X0000);     \
	write_cmos_sensor_twobyte(0X30A8, 0X0000);     \
	write_cmos_sensor_twobyte(0X30AA, 0X0000);     \
	write_cmos_sensor_twobyte(0X30AC, 0X0000);     \
	write_cmos_sensor_twobyte(0X30A0, 0X0000);     \
	write_cmos_sensor_twobyte(0X30A4, 0X0000);     \
	write_cmos_sensor_twobyte(0X6A0C, 0XFFFF);     \
	write_cmos_sensor_twobyte(0XF41E, 0X2180);     \
	write_cmos_sensor_twobyte(0X6028, 0X2000);     \
	write_cmos_sensor_twobyte(0X602A, 0X0990);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0020);     \
	write_cmos_sensor_twobyte(0X602A, 0X0AF8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0004);     \
	write_cmos_sensor_twobyte(0X602A, 0X27B8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0100);     \
	write_cmos_sensor_twobyte(0X602A, 0X2AA8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0001);     \
	write_cmos_sensor_twobyte(0X602A, 0X1698);     \
	write_cmos_sensor_twobyte(0X6F12, 0X00A2);     \
	write_cmos_sensor_twobyte(0X602A, 0X169C);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0028);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0030);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0C18);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0C18);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0C28);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0C28);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0C20);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0C20);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0C30);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0C30);     \
	write_cmos_sensor_twobyte(0X602A, 0X16C6);     \
	write_cmos_sensor_twobyte(0X6F12, 0X122F);     \
	write_cmos_sensor_twobyte(0X6F12, 0X4328);     \
	write_cmos_sensor_twobyte(0X602A, 0X1604);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0004);     \
	write_cmos_sensor_twobyte(0X602A, 0X14CA);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0096);     \
	write_cmos_sensor_twobyte(0X602A, 0X16CE);     \
	write_cmos_sensor_twobyte(0X6F12, 0X06C0);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0101);     \
	write_cmos_sensor_twobyte(0X602A, 0X13AA);     \
	write_cmos_sensor_twobyte(0X6F12, 0X02A0);     \
	write_cmos_sensor_twobyte(0X602A, 0X13A2);     \
	write_cmos_sensor_twobyte(0X6F12, 0X028C);     \
	write_cmos_sensor_twobyte(0X602A, 0X139A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0278);     \
	write_cmos_sensor_twobyte(0X602A, 0X1392);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0264);     \
	write_cmos_sensor_twobyte(0X602A, 0X138A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0250);     \
	write_cmos_sensor_twobyte(0X602A, 0X1382);     \
	write_cmos_sensor_twobyte(0X6F12, 0X023C);     \
	write_cmos_sensor_twobyte(0X602A, 0X137A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0228);     \
	write_cmos_sensor_twobyte(0X602A, 0X1372);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0214);     \
	write_cmos_sensor_twobyte(0X602A, 0X136A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0200);     \
	write_cmos_sensor_twobyte(0X602A, 0X1362);     \
	write_cmos_sensor_twobyte(0X6F12, 0X01EC);     \
	write_cmos_sensor_twobyte(0X602A, 0X135A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X01D8);     \
	write_cmos_sensor_twobyte(0X602A, 0X1352);     \
	write_cmos_sensor_twobyte(0X6F12, 0X01C4);     \
	write_cmos_sensor_twobyte(0X602A, 0X134A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X01B0);     \
	write_cmos_sensor_twobyte(0X602A, 0X1342);     \
	write_cmos_sensor_twobyte(0X6F12, 0X029B);     \
	write_cmos_sensor_twobyte(0X602A, 0X133A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0291);     \
	write_cmos_sensor_twobyte(0X602A, 0X1332);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0287);     \
	write_cmos_sensor_twobyte(0X602A, 0X132A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X027D);     \
	write_cmos_sensor_twobyte(0X602A, 0X1322);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0273);     \
	write_cmos_sensor_twobyte(0X602A, 0X131A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0269);     \
	write_cmos_sensor_twobyte(0X602A, 0X1312);     \
	write_cmos_sensor_twobyte(0X6F12, 0X025F);     \
	write_cmos_sensor_twobyte(0X602A, 0X130A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0255);     \
	write_cmos_sensor_twobyte(0X602A, 0X1302);     \
	write_cmos_sensor_twobyte(0X6F12, 0X024B);     \
	write_cmos_sensor_twobyte(0X602A, 0X12FA);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0241);     \
	write_cmos_sensor_twobyte(0X602A, 0X12F2);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0237);     \
	write_cmos_sensor_twobyte(0X602A, 0X12EA);     \
	write_cmos_sensor_twobyte(0X6F12, 0X022D);     \
	write_cmos_sensor_twobyte(0X602A, 0X12E2);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0223);     \
	write_cmos_sensor_twobyte(0X602A, 0X12DA);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0219);     \
	write_cmos_sensor_twobyte(0X602A, 0X12D2);     \
	write_cmos_sensor_twobyte(0X6F12, 0X020F);     \
	write_cmos_sensor_twobyte(0X602A, 0X12CA);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0205);     \
	write_cmos_sensor_twobyte(0X602A, 0X12C2);     \
	write_cmos_sensor_twobyte(0X6F12, 0X01FB);     \
	write_cmos_sensor_twobyte(0X602A, 0X12BA);     \
	write_cmos_sensor_twobyte(0X6F12, 0X01F1);     \
	write_cmos_sensor_twobyte(0X602A, 0X12B2);     \
	write_cmos_sensor_twobyte(0X6F12, 0X01E7);     \
	write_cmos_sensor_twobyte(0X602A, 0X12AA);     \
	write_cmos_sensor_twobyte(0X6F12, 0X01DD);     \
	write_cmos_sensor_twobyte(0X602A, 0X12A2);     \
	write_cmos_sensor_twobyte(0X6F12, 0X01D3);     \
	write_cmos_sensor_twobyte(0X602A, 0X129A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X01C9);     \
	write_cmos_sensor_twobyte(0X602A, 0X1292);     \
	write_cmos_sensor_twobyte(0X6F12, 0X01BF);     \
	write_cmos_sensor_twobyte(0X602A, 0X128A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X01B5);     \
	write_cmos_sensor_twobyte(0X602A, 0X11E2);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0258);     \
	write_cmos_sensor_twobyte(0X602A, 0X0C32);     \
	write_cmos_sensor_twobyte(0X6F12, 0X000A);     \
	write_cmos_sensor_twobyte(0X602A, 0X0B16);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0200);     \
	write_cmos_sensor_twobyte(0X602A, 0X0C20);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0368);     \
	write_cmos_sensor_twobyte(0X602A, 0X0D78);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0368);     \
	write_cmos_sensor_twobyte(0X602A, 0X0D98);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0368);     \
	write_cmos_sensor_twobyte(0X602A, 0X0DA8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0368);     \
	write_cmos_sensor_twobyte(0X602A, 0X0A96);     \
	write_cmos_sensor_twobyte(0X6F12, 0X2000);     \
	write_cmos_sensor_twobyte(0X602A, 0X0AE4);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0FE0);     \
	write_cmos_sensor_twobyte(0X602A, 0X29E6);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0FE0);     \
	write_cmos_sensor_twobyte(0X602A, 0X29F0);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0FE0);     \
	write_cmos_sensor_twobyte(0X602A, 0X2966);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0FE0);     \
	write_cmos_sensor_twobyte(0X602A, 0X29A6);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0FE0);     \
	write_cmos_sensor_twobyte(0X602A, 0X2970);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0FE0);     \
	write_cmos_sensor_twobyte(0X602A, 0X29B0);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0FE0);     \
	write_cmos_sensor_twobyte(0X602A, 0X0AE6);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0BE0);     \
	write_cmos_sensor_twobyte(0X602A, 0X29E8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0BE0);     \
	write_cmos_sensor_twobyte(0X602A, 0X29F2);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0BE0);     \
	write_cmos_sensor_twobyte(0X602A, 0X2968);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0BE0);     \
	write_cmos_sensor_twobyte(0X602A, 0X29A8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0BE0);     \
	write_cmos_sensor_twobyte(0X602A, 0X2972);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0BE0);     \
	write_cmos_sensor_twobyte(0X602A, 0X29B2);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0BE0);     \
	write_cmos_sensor_twobyte(0X602A, 0X0BB0);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0800);     \
	write_cmos_sensor_twobyte(0X602A, 0X0B62);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0186);     \
	write_cmos_sensor_twobyte(0X602A, 0X5160);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0000);     \
	write_cmos_sensor_twobyte(0X602A, 0X09F2);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0180);     \
	write_cmos_sensor_twobyte(0X6F12, 0X2180);     \
	write_cmos_sensor_twobyte(0X6F12, 0XF41E);     \
	write_cmos_sensor_twobyte(0X602A, 0X0B06);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0800);     \
	write_cmos_sensor_twobyte(0X602A, 0X5694);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0000);     \
	write_cmos_sensor_twobyte(0X602A, 0X569C);     \
	write_cmos_sensor_twobyte(0X6F12, 0X000C);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0100);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0200);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0100);     \
	write_cmos_sensor_twobyte(0X602A, 0X56D8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0000);     \
	write_cmos_sensor_twobyte(0X602A, 0X56E0);     \
	write_cmos_sensor_twobyte(0X6F12, 0X000C);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0100);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0200);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0100);     \
	write_cmos_sensor_twobyte(0X6028, 0X4000);     \
	write_cmos_sensor_twobyte(0X6214, 0X79F0);     \
	write_cmos_sensor_twobyte(0X6218, 0X79F0);     \
	sensor_WDR_zhdr();                             \
} while (0)


/*****************************************************************************
 *
 * Description:
 * ------------
 *     mode 1 high speed video setting
 *     $MIPI[Width:2688,Height:756,Format:Raw10,Lane:4,ErrorCheck:0,
 *     PolarityData:0,PolarityClock:0,Buffer:4,DataRate:2034,useEmbData:0]
 *     $MV1[MCLK:24,Width:2688,Height:756,
 *     Format:MIPI_Raw10,mipi_lane:4,mipi_datarate:2034,pvi_pclk_inverse:0]
 *
 ****************************************************************************/
#define _S5K2L7_MODE1_HS_VIDEO_MODULE_V2_ do {         \
	write_cmos_sensor_twobyte(0X6028, 0X4000);     \
	write_cmos_sensor_twobyte(0X6214, 0X7970);     \
	write_cmos_sensor_twobyte(0X6218, 0X7150);     \
	write_cmos_sensor_twobyte(0X0344, 0X0000);     \
	write_cmos_sensor_twobyte(0X0346, 0X016E);     \
	write_cmos_sensor_twobyte(0X0348, 0X1FDF);     \
	write_cmos_sensor_twobyte(0X034A, 0X0A61);     \
	write_cmos_sensor_twobyte(0X034C, 0X0A80);     \
	write_cmos_sensor_twobyte(0X034E, 0X02F4);     \
	write_cmos_sensor_twobyte(0X0408, 0X0010);     \
	write_cmos_sensor_twobyte(0X040A, 0X0004);     \
	write_cmos_sensor_twobyte(0X0900, 0X0113);     \
	write_cmos_sensor_twobyte(0X0380, 0X0001);     \
	write_cmos_sensor_twobyte(0X0382, 0X0001);     \
	write_cmos_sensor_twobyte(0X0384, 0X0001);     \
	write_cmos_sensor_twobyte(0X0386, 0X0005);     \
	write_cmos_sensor_twobyte(0X0400, 0X0000);     \
	write_cmos_sensor_twobyte(0X0404, 0X0010);     \
	write_cmos_sensor_twobyte(0X3060, 0X0103);     \
	write_cmos_sensor_twobyte(0X0114, 0X0300);     \
	write_cmos_sensor_twobyte(0X0110, 0X1002);     \
	write_cmos_sensor_twobyte(0X0136, 0X1800);     \
	write_cmos_sensor_twobyte(0X0304, 0X0006);     \
	write_cmos_sensor_twobyte(0X0306, 0X01E0);     \
	write_cmos_sensor_twobyte(0X0302, 0X0001);     \
	write_cmos_sensor_twobyte(0X0300, 0X0004);     \
	write_cmos_sensor_twobyte(0X030C, 0X0001);     \
	write_cmos_sensor_twobyte(0X030E, 0X0004);     \
	write_cmos_sensor_twobyte(0X0310, 0X008C);     \
	write_cmos_sensor_twobyte(0X0312, 0X0000);     \
	write_cmos_sensor_twobyte(0X030A, 0X0001);     \
	write_cmos_sensor_twobyte(0X0308, 0X0008);     \
	write_cmos_sensor_twobyte(0X0342, 0X27B0);     \
	write_cmos_sensor_twobyte(0X0340, 0X0419);     \
	write_cmos_sensor_twobyte(0X021E, 0X0000);     \
	write_cmos_sensor_twobyte(0X3098, 0X0400);     \
	write_cmos_sensor_twobyte(0X309A, 0X0002);     \
	write_cmos_sensor_twobyte(0X30BC, 0X0031);     \
	write_cmos_sensor_twobyte(0X30A6, 0X0000);     \
	write_cmos_sensor_twobyte(0X30A8, 0X0000);     \
	write_cmos_sensor_twobyte(0X30AA, 0X0000);     \
	write_cmos_sensor_twobyte(0X30AC, 0X0000);     \
	write_cmos_sensor_twobyte(0X30A0, 0X0000);     \
	write_cmos_sensor_twobyte(0X30A4, 0X0000);     \
	write_cmos_sensor_twobyte(0X6A0C, 0XFFFF);     \
	write_cmos_sensor_twobyte(0XF41E, 0X2180);     \
	write_cmos_sensor_twobyte(0X6028, 0X2000);     \
	write_cmos_sensor_twobyte(0X602A, 0X0990);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0020);     \
	write_cmos_sensor_twobyte(0X602A, 0X0AF8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X000C);     \
	write_cmos_sensor_twobyte(0X602A, 0X27B8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0000);     \
	write_cmos_sensor_twobyte(0X602A, 0X2AA8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0001);     \
	write_cmos_sensor_twobyte(0X602A, 0X1698);     \
	write_cmos_sensor_twobyte(0X6F12, 0X00A0);     \
	write_cmos_sensor_twobyte(0X602A, 0X169C);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0028);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0034);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0C16);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0C16);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0C1C);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0C1C);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0C22);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0C22);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0C28);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0C28);     \
	write_cmos_sensor_twobyte(0X602A, 0X16C6);     \
	write_cmos_sensor_twobyte(0X6F12, 0X122F);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0328);     \
	write_cmos_sensor_twobyte(0X602A, 0X1604);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0004);     \
	write_cmos_sensor_twobyte(0X602A, 0X14CA);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0096);     \
	write_cmos_sensor_twobyte(0X602A, 0X16CE);     \
	write_cmos_sensor_twobyte(0X6F12, 0X06C0);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0101);     \
	write_cmos_sensor_twobyte(0X602A, 0X13AA);     \
	write_cmos_sensor_twobyte(0X6F12, 0X02A0);     \
	write_cmos_sensor_twobyte(0X602A, 0X13A2);     \
	write_cmos_sensor_twobyte(0X6F12, 0X028C);     \
	write_cmos_sensor_twobyte(0X602A, 0X139A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0278);     \
	write_cmos_sensor_twobyte(0X602A, 0X1392);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0264);     \
	write_cmos_sensor_twobyte(0X602A, 0X138A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0250);     \
	write_cmos_sensor_twobyte(0X602A, 0X1382);     \
	write_cmos_sensor_twobyte(0X6F12, 0X023C);     \
	write_cmos_sensor_twobyte(0X602A, 0X137A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0228);     \
	write_cmos_sensor_twobyte(0X602A, 0X1372);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0214);     \
	write_cmos_sensor_twobyte(0X602A, 0X136A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0200);     \
	write_cmos_sensor_twobyte(0X602A, 0X1362);     \
	write_cmos_sensor_twobyte(0X6F12, 0X01EC);     \
	write_cmos_sensor_twobyte(0X602A, 0X135A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X01D8);     \
	write_cmos_sensor_twobyte(0X602A, 0X1352);     \
	write_cmos_sensor_twobyte(0X6F12, 0X01C4);     \
	write_cmos_sensor_twobyte(0X602A, 0X134A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X01B0);     \
	write_cmos_sensor_twobyte(0X602A, 0X1342);     \
	write_cmos_sensor_twobyte(0X6F12, 0X029B);     \
	write_cmos_sensor_twobyte(0X602A, 0X133A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0291);     \
	write_cmos_sensor_twobyte(0X602A, 0X1332);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0287);     \
	write_cmos_sensor_twobyte(0X602A, 0X132A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X027D);     \
	write_cmos_sensor_twobyte(0X602A, 0X1322);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0273);     \
	write_cmos_sensor_twobyte(0X602A, 0X131A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0269);     \
	write_cmos_sensor_twobyte(0X602A, 0X1312);     \
	write_cmos_sensor_twobyte(0X6F12, 0X025F);     \
	write_cmos_sensor_twobyte(0X602A, 0X130A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0255);     \
	write_cmos_sensor_twobyte(0X602A, 0X1302);     \
	write_cmos_sensor_twobyte(0X6F12, 0X024B);     \
	write_cmos_sensor_twobyte(0X602A, 0X12FA);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0241);     \
	write_cmos_sensor_twobyte(0X602A, 0X12F2);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0237);     \
	write_cmos_sensor_twobyte(0X602A, 0X12EA);     \
	write_cmos_sensor_twobyte(0X6F12, 0X022D);     \
	write_cmos_sensor_twobyte(0X602A, 0X12E2);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0223);     \
	write_cmos_sensor_twobyte(0X602A, 0X12DA);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0219);     \
	write_cmos_sensor_twobyte(0X602A, 0X12D2);     \
	write_cmos_sensor_twobyte(0X6F12, 0X020F);     \
	write_cmos_sensor_twobyte(0X602A, 0X12CA);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0205);     \
	write_cmos_sensor_twobyte(0X602A, 0X12C2);     \
	write_cmos_sensor_twobyte(0X6F12, 0X01FB);     \
	write_cmos_sensor_twobyte(0X602A, 0X12BA);     \
	write_cmos_sensor_twobyte(0X6F12, 0X01F1);     \
	write_cmos_sensor_twobyte(0X602A, 0X12B2);     \
	write_cmos_sensor_twobyte(0X6F12, 0X01E7);     \
	write_cmos_sensor_twobyte(0X602A, 0X12AA);     \
	write_cmos_sensor_twobyte(0X6F12, 0X01DD);     \
	write_cmos_sensor_twobyte(0X602A, 0X12A2);     \
	write_cmos_sensor_twobyte(0X6F12, 0X01D3);     \
	write_cmos_sensor_twobyte(0X602A, 0X129A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X01C9);     \
	write_cmos_sensor_twobyte(0X602A, 0X1292);     \
	write_cmos_sensor_twobyte(0X6F12, 0X01BF);     \
	write_cmos_sensor_twobyte(0X602A, 0X128A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X01B5);     \
	write_cmos_sensor_twobyte(0X602A, 0X11E2);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0258);     \
	write_cmos_sensor_twobyte(0X602A, 0X0C32);     \
	write_cmos_sensor_twobyte(0X6F12, 0X000A);     \
	write_cmos_sensor_twobyte(0X602A, 0X0B16);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0100);     \
	write_cmos_sensor_twobyte(0X602A, 0X0C20);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0361);     \
	write_cmos_sensor_twobyte(0X602A, 0X0D78);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0361);     \
	write_cmos_sensor_twobyte(0X602A, 0X0D98);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0361);     \
	write_cmos_sensor_twobyte(0X602A, 0X0DA8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0361);     \
	write_cmos_sensor_twobyte(0X602A, 0X0A96);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0800);     \
	write_cmos_sensor_twobyte(0X602A, 0X0AE4);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0FF0);     \
	write_cmos_sensor_twobyte(0X602A, 0X29E6);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0FF0);     \
	write_cmos_sensor_twobyte(0X602A, 0X29F0);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0FF0);     \
	write_cmos_sensor_twobyte(0X602A, 0X2966);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0FF0);     \
	write_cmos_sensor_twobyte(0X602A, 0X29A6);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0FF0);     \
	write_cmos_sensor_twobyte(0X602A, 0X2970);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0FF0);     \
	write_cmos_sensor_twobyte(0X602A, 0X29B0);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0FF0);     \
	write_cmos_sensor_twobyte(0X602A, 0X0AE6);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0BD0);     \
	write_cmos_sensor_twobyte(0X602A, 0X29E8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0BD0);     \
	write_cmos_sensor_twobyte(0X602A, 0X29F2);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0BD0);     \
	write_cmos_sensor_twobyte(0X602A, 0X2968);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0BD0);     \
	write_cmos_sensor_twobyte(0X602A, 0X29A8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0BD0);     \
	write_cmos_sensor_twobyte(0X602A, 0X2972);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0BD0);     \
	write_cmos_sensor_twobyte(0X602A, 0X29B2);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0BD0);     \
	write_cmos_sensor_twobyte(0X602A, 0X0BB0);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0C00);     \
	write_cmos_sensor_twobyte(0X602A, 0X0B62);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0110);     \
	write_cmos_sensor_twobyte(0X602A, 0X5160);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0100);     \
	write_cmos_sensor_twobyte(0X602A, 0X09F2);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0180);     \
	write_cmos_sensor_twobyte(0X6F12, 0X2180);     \
	write_cmos_sensor_twobyte(0X6F12, 0XF41E);     \
	write_cmos_sensor_twobyte(0X602A, 0X0B06);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0800);     \
	write_cmos_sensor_twobyte(0X602A, 0X5694);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0000);     \
	write_cmos_sensor_twobyte(0X602A, 0X569C);     \
	write_cmos_sensor_twobyte(0X6F12, 0X000C);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0100);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0200);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0100);     \
	write_cmos_sensor_twobyte(0X602A, 0X56D8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0000);     \
	write_cmos_sensor_twobyte(0X602A, 0X56E0);     \
	write_cmos_sensor_twobyte(0X6F12, 0X000C);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0100);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0200);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0100);     \
	write_cmos_sensor_twobyte(0X6028, 0X4000);     \
	write_cmos_sensor_twobyte(0X6214, 0X79F0);     \
	write_cmos_sensor_twobyte(0X6218, 0X79F0);     \
} while (0)

/*****************************************************************************
 *
 * Description:
 * ------------
 *     mode 1 slim video setting
 *     $MIPI[Width:2688,Height:756,Format:Raw10,Lane:4,ErrorCheck:0,
 *     PolarityData:0,PolarityClock:0,Buffer:4,DataRate:2034,useEmbData:0]
 *     $MV1[MCLK:24,Width:2688,Height:756,
 *     Format:MIPI_Raw10,mipi_lane:4,mipi_datarate:2034,pvi_pclk_inverse:0]
 *
 ****************************************************************************/
#define _S5K2L7_MODE1_SLIM_VIDEO_MODULE_V2_ do {       \
	write_cmos_sensor_twobyte(0X6028, 0X4000);     \
	write_cmos_sensor_twobyte(0X6214, 0X7970);     \
	write_cmos_sensor_twobyte(0X6218, 0X7150);     \
	write_cmos_sensor_twobyte(0X0344, 0X0000);     \
	write_cmos_sensor_twobyte(0X0346, 0X016E);     \
	write_cmos_sensor_twobyte(0X0348, 0X1FDF);     \
	write_cmos_sensor_twobyte(0X034A, 0X0A61);     \
	write_cmos_sensor_twobyte(0X034C, 0X0A80);     \
	write_cmos_sensor_twobyte(0X034E, 0X02F4);     \
	write_cmos_sensor_twobyte(0X0408, 0X0010);     \
	write_cmos_sensor_twobyte(0X040A, 0X0004);     \
	write_cmos_sensor_twobyte(0X0900, 0X0113);     \
	write_cmos_sensor_twobyte(0X0380, 0X0001);     \
	write_cmos_sensor_twobyte(0X0382, 0X0001);     \
	write_cmos_sensor_twobyte(0X0384, 0X0001);     \
	write_cmos_sensor_twobyte(0X0386, 0X0005);     \
	write_cmos_sensor_twobyte(0X0400, 0X0000);     \
	write_cmos_sensor_twobyte(0X0404, 0X0010);     \
	write_cmos_sensor_twobyte(0X3060, 0X0103);     \
	write_cmos_sensor_twobyte(0X0114, 0X0300);     \
	write_cmos_sensor_twobyte(0X0110, 0X1002);     \
	write_cmos_sensor_twobyte(0X0136, 0X1800);     \
	write_cmos_sensor_twobyte(0X0304, 0X0006);     \
	write_cmos_sensor_twobyte(0X0306, 0X01E0);     \
	write_cmos_sensor_twobyte(0X0302, 0X0001);     \
	write_cmos_sensor_twobyte(0X0300, 0X0004);     \
	write_cmos_sensor_twobyte(0X030C, 0X0001);     \
	write_cmos_sensor_twobyte(0X030E, 0X0004);     \
	write_cmos_sensor_twobyte(0X0310, 0X008C);     \
	write_cmos_sensor_twobyte(0X0312, 0X0000);     \
	write_cmos_sensor_twobyte(0X030A, 0X0001);     \
	write_cmos_sensor_twobyte(0X0308, 0X0008);     \
	write_cmos_sensor_twobyte(0X0342, 0X27B0);     \
	write_cmos_sensor_twobyte(0X0340, 0X0C4D);     \
	write_cmos_sensor_twobyte(0X021E, 0X0000);     \
	write_cmos_sensor_twobyte(0X3098, 0X0400);     \
	write_cmos_sensor_twobyte(0X309A, 0X0002);     \
	write_cmos_sensor_twobyte(0X30BC, 0X0031);     \
	write_cmos_sensor_twobyte(0X30A6, 0X0000);     \
	write_cmos_sensor_twobyte(0X30A8, 0X0000);     \
	write_cmos_sensor_twobyte(0X30AA, 0X0000);     \
	write_cmos_sensor_twobyte(0X30AC, 0X0000);     \
	write_cmos_sensor_twobyte(0X30A0, 0X0000);     \
	write_cmos_sensor_twobyte(0X30A4, 0X0000);     \
	write_cmos_sensor_twobyte(0X6A0C, 0XFFFF);     \
	write_cmos_sensor_twobyte(0XF41E, 0X2180);     \
	write_cmos_sensor_twobyte(0X6028, 0X2000);     \
	write_cmos_sensor_twobyte(0X602A, 0X0990);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0020);     \
	write_cmos_sensor_twobyte(0X602A, 0X0AF8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X000C);     \
	write_cmos_sensor_twobyte(0X602A, 0X27B8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0000);     \
	write_cmos_sensor_twobyte(0X602A, 0X2AA8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0001);     \
	write_cmos_sensor_twobyte(0X602A, 0X1698);     \
	write_cmos_sensor_twobyte(0X6F12, 0X00A0);     \
	write_cmos_sensor_twobyte(0X602A, 0X169C);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0028);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0034);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0C16);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0C16);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0C1C);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0C1C);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0C22);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0C22);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0C28);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0C28);     \
	write_cmos_sensor_twobyte(0X602A, 0X16C6);     \
	write_cmos_sensor_twobyte(0X6F12, 0X122F);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0328);     \
	write_cmos_sensor_twobyte(0X602A, 0X1604);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0004);     \
	write_cmos_sensor_twobyte(0X602A, 0X14CA);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0096);     \
	write_cmos_sensor_twobyte(0X602A, 0X16CE);     \
	write_cmos_sensor_twobyte(0X6F12, 0X06C0);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0101);     \
	write_cmos_sensor_twobyte(0X602A, 0X13AA);     \
	write_cmos_sensor_twobyte(0X6F12, 0X02A0);     \
	write_cmos_sensor_twobyte(0X602A, 0X13A2);     \
	write_cmos_sensor_twobyte(0X6F12, 0X028C);     \
	write_cmos_sensor_twobyte(0X602A, 0X139A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0278);     \
	write_cmos_sensor_twobyte(0X602A, 0X1392);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0264);     \
	write_cmos_sensor_twobyte(0X602A, 0X138A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0250);     \
	write_cmos_sensor_twobyte(0X602A, 0X1382);     \
	write_cmos_sensor_twobyte(0X6F12, 0X023C);     \
	write_cmos_sensor_twobyte(0X602A, 0X137A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0228);     \
	write_cmos_sensor_twobyte(0X602A, 0X1372);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0214);     \
	write_cmos_sensor_twobyte(0X602A, 0X136A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0200);     \
	write_cmos_sensor_twobyte(0X602A, 0X1362);     \
	write_cmos_sensor_twobyte(0X6F12, 0X01EC);     \
	write_cmos_sensor_twobyte(0X602A, 0X135A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X01D8);     \
	write_cmos_sensor_twobyte(0X602A, 0X1352);     \
	write_cmos_sensor_twobyte(0X6F12, 0X01C4);     \
	write_cmos_sensor_twobyte(0X602A, 0X134A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X01B0);     \
	write_cmos_sensor_twobyte(0X602A, 0X1342);     \
	write_cmos_sensor_twobyte(0X6F12, 0X029B);     \
	write_cmos_sensor_twobyte(0X602A, 0X133A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0291);     \
	write_cmos_sensor_twobyte(0X602A, 0X1332);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0287);     \
	write_cmos_sensor_twobyte(0X602A, 0X132A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X027D);     \
	write_cmos_sensor_twobyte(0X602A, 0X1322);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0273);     \
	write_cmos_sensor_twobyte(0X602A, 0X131A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0269);     \
	write_cmos_sensor_twobyte(0X602A, 0X1312);     \
	write_cmos_sensor_twobyte(0X6F12, 0X025F);     \
	write_cmos_sensor_twobyte(0X602A, 0X130A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0255);     \
	write_cmos_sensor_twobyte(0X602A, 0X1302);     \
	write_cmos_sensor_twobyte(0X6F12, 0X024B);     \
	write_cmos_sensor_twobyte(0X602A, 0X12FA);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0241);     \
	write_cmos_sensor_twobyte(0X602A, 0X12F2);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0237);     \
	write_cmos_sensor_twobyte(0X602A, 0X12EA);     \
	write_cmos_sensor_twobyte(0X6F12, 0X022D);     \
	write_cmos_sensor_twobyte(0X602A, 0X12E2);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0223);     \
	write_cmos_sensor_twobyte(0X602A, 0X12DA);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0219);     \
	write_cmos_sensor_twobyte(0X602A, 0X12D2);     \
	write_cmos_sensor_twobyte(0X6F12, 0X020F);     \
	write_cmos_sensor_twobyte(0X602A, 0X12CA);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0205);     \
	write_cmos_sensor_twobyte(0X602A, 0X12C2);     \
	write_cmos_sensor_twobyte(0X6F12, 0X01FB);     \
	write_cmos_sensor_twobyte(0X602A, 0X12BA);     \
	write_cmos_sensor_twobyte(0X6F12, 0X01F1);     \
	write_cmos_sensor_twobyte(0X602A, 0X12B2);     \
	write_cmos_sensor_twobyte(0X6F12, 0X01E7);     \
	write_cmos_sensor_twobyte(0X602A, 0X12AA);     \
	write_cmos_sensor_twobyte(0X6F12, 0X01DD);     \
	write_cmos_sensor_twobyte(0X602A, 0X12A2);     \
	write_cmos_sensor_twobyte(0X6F12, 0X01D3);     \
	write_cmos_sensor_twobyte(0X602A, 0X129A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X01C9);     \
	write_cmos_sensor_twobyte(0X602A, 0X1292);     \
	write_cmos_sensor_twobyte(0X6F12, 0X01BF);     \
	write_cmos_sensor_twobyte(0X602A, 0X128A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X01B5);     \
	write_cmos_sensor_twobyte(0X602A, 0X11E2);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0258);     \
	write_cmos_sensor_twobyte(0X602A, 0X0C32);     \
	write_cmos_sensor_twobyte(0X6F12, 0X000A);     \
	write_cmos_sensor_twobyte(0X602A, 0X0B16);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0100);     \
	write_cmos_sensor_twobyte(0X602A, 0X0C20);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0361);     \
	write_cmos_sensor_twobyte(0X602A, 0X0D78);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0361);     \
	write_cmos_sensor_twobyte(0X602A, 0X0D98);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0361);     \
	write_cmos_sensor_twobyte(0X602A, 0X0DA8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0361);     \
	write_cmos_sensor_twobyte(0X602A, 0X0A96);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0800);     \
	write_cmos_sensor_twobyte(0X602A, 0X0AE4);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0FF0);     \
	write_cmos_sensor_twobyte(0X602A, 0X29E6);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0FF0);     \
	write_cmos_sensor_twobyte(0X602A, 0X29F0);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0FF0);     \
	write_cmos_sensor_twobyte(0X602A, 0X2966);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0FF0);     \
	write_cmos_sensor_twobyte(0X602A, 0X29A6);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0FF0);     \
	write_cmos_sensor_twobyte(0X602A, 0X2970);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0FF0);     \
	write_cmos_sensor_twobyte(0X602A, 0X29B0);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0FF0);     \
	write_cmos_sensor_twobyte(0X602A, 0X0AE6);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0BD0);     \
	write_cmos_sensor_twobyte(0X602A, 0X29E8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0BD0);     \
	write_cmos_sensor_twobyte(0X602A, 0X29F2);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0BD0);     \
	write_cmos_sensor_twobyte(0X602A, 0X2968);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0BD0);     \
	write_cmos_sensor_twobyte(0X602A, 0X29A8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0BD0);     \
	write_cmos_sensor_twobyte(0X602A, 0X2972);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0BD0);     \
	write_cmos_sensor_twobyte(0X602A, 0X29B2);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0BD0);     \
	write_cmos_sensor_twobyte(0X602A, 0X0BB0);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0C00);     \
	write_cmos_sensor_twobyte(0X602A, 0X0B62);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0110);     \
	write_cmos_sensor_twobyte(0X602A, 0X5160);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0100);     \
	write_cmos_sensor_twobyte(0X602A, 0X09F2);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0180);     \
	write_cmos_sensor_twobyte(0X6F12, 0X2180);     \
	write_cmos_sensor_twobyte(0X6F12, 0XF41E);     \
	write_cmos_sensor_twobyte(0X602A, 0X0B06);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0800);     \
	write_cmos_sensor_twobyte(0X602A, 0X5694);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0000);     \
	write_cmos_sensor_twobyte(0X602A, 0X569C);     \
	write_cmos_sensor_twobyte(0X6F12, 0X000C);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0100);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0200);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0100);     \
	write_cmos_sensor_twobyte(0X602A, 0X56D8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0000);     \
	write_cmos_sensor_twobyte(0X602A, 0X56E0);     \
	write_cmos_sensor_twobyte(0X6F12, 0X000C);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0100);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0200);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0100);     \
	write_cmos_sensor_twobyte(0X6028, 0X4000);     \
	write_cmos_sensor_twobyte(0X6214, 0X79F0);     \
	write_cmos_sensor_twobyte(0X6218, 0X79F0);     \
} while (0)

/*****************************************************************************
 *
 * Description:
 * ------------
 *     mode 1 cpature with WDR setting
 *     $MIPI[Width:8064,Height:3024,Format:Raw10,Lane:4,ErrorCheck:0,
 *     PolarityData:0,PolarityClock:0,Buffer:4,DataRate:2034,useEmbData:0]
 *     $MV1[MCLK:24,Width:8064,Height:3024,
 *     Format:MIPI_Raw10,mipi_lane:4,mipi_datarate:2034,pvi_pclk_inverse:0]
 *
 ****************************************************************************/
#define _S5K2L7_MODE1_CAPTURE_WDR_MODULE_V2_ do {      \
	write_cmos_sensor_twobyte(0X6028, 0X4000);     \
	write_cmos_sensor_twobyte(0X6214, 0X7970);     \
	write_cmos_sensor_twobyte(0X6218, 0X7150);     \
	write_cmos_sensor_twobyte(0X0344, 0X0000);     \
	write_cmos_sensor_twobyte(0X0346, 0X0000);     \
	write_cmos_sensor_twobyte(0X0348, 0X1FBF);     \
	write_cmos_sensor_twobyte(0X034A, 0X0BDF);     \
	write_cmos_sensor_twobyte(0X034C, 0X1F80);     \
	write_cmos_sensor_twobyte(0X034E, 0X0BD0);     \
	write_cmos_sensor_twobyte(0X0408, 0X0020);     \
	write_cmos_sensor_twobyte(0X040A, 0X0008);     \
	write_cmos_sensor_twobyte(0X0900, 0X0011);     \
	write_cmos_sensor_twobyte(0X0380, 0X0001);     \
	write_cmos_sensor_twobyte(0X0382, 0X0001);     \
	write_cmos_sensor_twobyte(0X0384, 0X0001);     \
	write_cmos_sensor_twobyte(0X0386, 0X0001);     \
	write_cmos_sensor_twobyte(0X0400, 0X0000);     \
	write_cmos_sensor_twobyte(0X0404, 0X0010);     \
	write_cmos_sensor_twobyte(0X3060, 0X0100);     \
	write_cmos_sensor_twobyte(0X0114, 0X0300);     \
	write_cmos_sensor_twobyte(0X0110, 0X1002);     \
	write_cmos_sensor_twobyte(0X0136, 0X1800);     \
	write_cmos_sensor_twobyte(0X0304, 0X0006);     \
	write_cmos_sensor_twobyte(0X0306, 0X01E0);     \
	write_cmos_sensor_twobyte(0X0302, 0X0001);     \
	write_cmos_sensor_twobyte(0X0300, 0X0004);     \
	write_cmos_sensor_twobyte(0X030C, 0X0001);     \
	write_cmos_sensor_twobyte(0X030E, 0X0004);     \
	write_cmos_sensor_twobyte(0X0310, 0X014F);     \
	write_cmos_sensor_twobyte(0X0312, 0X0000);     \
	write_cmos_sensor_twobyte(0X030A, 0X0001);     \
	write_cmos_sensor_twobyte(0X0308, 0X0008);     \
	write_cmos_sensor_twobyte(0X0342, 0X2810);     \
	write_cmos_sensor_twobyte(0X0340, 0X0C30);     \
	write_cmos_sensor_twobyte(0X021E, 0X0000);     \
	write_cmos_sensor_twobyte(0X3098, 0X0400);     \
	write_cmos_sensor_twobyte(0X309A, 0X0002);     \
	write_cmos_sensor_twobyte(0X30BC, 0X0031);     \
	write_cmos_sensor_twobyte(0X30A6, 0X0000);     \
	write_cmos_sensor_twobyte(0X30A8, 0X0000);     \
	write_cmos_sensor_twobyte(0X30AA, 0X0000);     \
	write_cmos_sensor_twobyte(0X30AC, 0X0000);     \
	write_cmos_sensor_twobyte(0X30A0, 0X0000);     \
	write_cmos_sensor_twobyte(0X30A4, 0X0000);     \
	write_cmos_sensor_twobyte(0X6A0C, 0XFFFF);     \
	write_cmos_sensor_twobyte(0XF41E, 0X2180);     \
	write_cmos_sensor_twobyte(0X6028, 0X2000);     \
	write_cmos_sensor_twobyte(0X602A, 0X0990);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0020);     \
	write_cmos_sensor_twobyte(0X602A, 0X0AF8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0004);     \
	write_cmos_sensor_twobyte(0X602A, 0X27B8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0100);     \
	write_cmos_sensor_twobyte(0X602A, 0X2AA8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0001);     \
	write_cmos_sensor_twobyte(0X602A, 0X1698);     \
	write_cmos_sensor_twobyte(0X6F12, 0X00A2);     \
	write_cmos_sensor_twobyte(0X602A, 0X169C);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0028);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0030);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0C18);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0C18);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0C28);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0C28);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0C20);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0C20);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0C30);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0C30);     \
	write_cmos_sensor_twobyte(0X602A, 0X16C6);     \
	write_cmos_sensor_twobyte(0X6F12, 0X122F);     \
	write_cmos_sensor_twobyte(0X6F12, 0X4328);     \
	write_cmos_sensor_twobyte(0X602A, 0X1604);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0004);     \
	write_cmos_sensor_twobyte(0X602A, 0X14CA);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0096);     \
	write_cmos_sensor_twobyte(0X602A, 0X16CE);     \
	write_cmos_sensor_twobyte(0X6F12, 0X06C0);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0101);     \
	write_cmos_sensor_twobyte(0X602A, 0X13AA);     \
	write_cmos_sensor_twobyte(0X6F12, 0X02A0);     \
	write_cmos_sensor_twobyte(0X602A, 0X13A2);     \
	write_cmos_sensor_twobyte(0X6F12, 0X028C);     \
	write_cmos_sensor_twobyte(0X602A, 0X139A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0278);     \
	write_cmos_sensor_twobyte(0X602A, 0X1392);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0264);     \
	write_cmos_sensor_twobyte(0X602A, 0X138A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0250);     \
	write_cmos_sensor_twobyte(0X602A, 0X1382);     \
	write_cmos_sensor_twobyte(0X6F12, 0X023C);     \
	write_cmos_sensor_twobyte(0X602A, 0X137A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0228);     \
	write_cmos_sensor_twobyte(0X602A, 0X1372);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0214);     \
	write_cmos_sensor_twobyte(0X602A, 0X136A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0200);     \
	write_cmos_sensor_twobyte(0X602A, 0X1362);     \
	write_cmos_sensor_twobyte(0X6F12, 0X01EC);     \
	write_cmos_sensor_twobyte(0X602A, 0X135A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X01D8);     \
	write_cmos_sensor_twobyte(0X602A, 0X1352);     \
	write_cmos_sensor_twobyte(0X6F12, 0X01C4);     \
	write_cmos_sensor_twobyte(0X602A, 0X134A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X01B0);     \
	write_cmos_sensor_twobyte(0X602A, 0X1342);     \
	write_cmos_sensor_twobyte(0X6F12, 0X029B);     \
	write_cmos_sensor_twobyte(0X602A, 0X133A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0291);     \
	write_cmos_sensor_twobyte(0X602A, 0X1332);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0287);     \
	write_cmos_sensor_twobyte(0X602A, 0X132A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X027D);     \
	write_cmos_sensor_twobyte(0X602A, 0X1322);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0273);     \
	write_cmos_sensor_twobyte(0X602A, 0X131A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0269);     \
	write_cmos_sensor_twobyte(0X602A, 0X1312);     \
	write_cmos_sensor_twobyte(0X6F12, 0X025F);     \
	write_cmos_sensor_twobyte(0X602A, 0X130A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0255);     \
	write_cmos_sensor_twobyte(0X602A, 0X1302);     \
	write_cmos_sensor_twobyte(0X6F12, 0X024B);     \
	write_cmos_sensor_twobyte(0X602A, 0X12FA);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0241);     \
	write_cmos_sensor_twobyte(0X602A, 0X12F2);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0237);     \
	write_cmos_sensor_twobyte(0X602A, 0X12EA);     \
	write_cmos_sensor_twobyte(0X6F12, 0X022D);     \
	write_cmos_sensor_twobyte(0X602A, 0X12E2);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0223);     \
	write_cmos_sensor_twobyte(0X602A, 0X12DA);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0219);     \
	write_cmos_sensor_twobyte(0X602A, 0X12D2);     \
	write_cmos_sensor_twobyte(0X6F12, 0X020F);     \
	write_cmos_sensor_twobyte(0X602A, 0X12CA);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0205);     \
	write_cmos_sensor_twobyte(0X602A, 0X12C2);     \
	write_cmos_sensor_twobyte(0X6F12, 0X01FB);     \
	write_cmos_sensor_twobyte(0X602A, 0X12BA);     \
	write_cmos_sensor_twobyte(0X6F12, 0X01F1);     \
	write_cmos_sensor_twobyte(0X602A, 0X12B2);     \
	write_cmos_sensor_twobyte(0X6F12, 0X01E7);     \
	write_cmos_sensor_twobyte(0X602A, 0X12AA);     \
	write_cmos_sensor_twobyte(0X6F12, 0X01DD);     \
	write_cmos_sensor_twobyte(0X602A, 0X12A2);     \
	write_cmos_sensor_twobyte(0X6F12, 0X01D3);     \
	write_cmos_sensor_twobyte(0X602A, 0X129A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X01C9);     \
	write_cmos_sensor_twobyte(0X602A, 0X1292);     \
	write_cmos_sensor_twobyte(0X6F12, 0X01BF);     \
	write_cmos_sensor_twobyte(0X602A, 0X128A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X01B5);     \
	write_cmos_sensor_twobyte(0X602A, 0X11E2);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0258);     \
	write_cmos_sensor_twobyte(0X602A, 0X0C32);     \
	write_cmos_sensor_twobyte(0X6F12, 0X000A);     \
	write_cmos_sensor_twobyte(0X602A, 0X0B16);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0200);     \
	write_cmos_sensor_twobyte(0X602A, 0X0C20);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0368);     \
	write_cmos_sensor_twobyte(0X602A, 0X0D78);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0368);     \
	write_cmos_sensor_twobyte(0X602A, 0X0D98);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0368);     \
	write_cmos_sensor_twobyte(0X602A, 0X0DA8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0368);     \
	write_cmos_sensor_twobyte(0X602A, 0X0A96);     \
	write_cmos_sensor_twobyte(0X6F12, 0X2000);     \
	write_cmos_sensor_twobyte(0X602A, 0X0AE4);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0FE0);     \
	write_cmos_sensor_twobyte(0X602A, 0X29E6);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0FE0);     \
	write_cmos_sensor_twobyte(0X602A, 0X29F0);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0FE0);     \
	write_cmos_sensor_twobyte(0X602A, 0X2966);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0FE0);     \
	write_cmos_sensor_twobyte(0X602A, 0X29A6);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0FE0);     \
	write_cmos_sensor_twobyte(0X602A, 0X2970);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0FE0);     \
	write_cmos_sensor_twobyte(0X602A, 0X29B0);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0FE0);     \
	write_cmos_sensor_twobyte(0X602A, 0X0AE6);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0BE0);     \
	write_cmos_sensor_twobyte(0X602A, 0X29E8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0BE0);     \
	write_cmos_sensor_twobyte(0X602A, 0X29F2);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0BE0);     \
	write_cmos_sensor_twobyte(0X602A, 0X2968);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0BE0);     \
	write_cmos_sensor_twobyte(0X602A, 0X29A8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0BE0);     \
	write_cmos_sensor_twobyte(0X602A, 0X2972);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0BE0);     \
	write_cmos_sensor_twobyte(0X602A, 0X29B2);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0BE0);     \
	write_cmos_sensor_twobyte(0X602A, 0X0BB0);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0800);     \
	write_cmos_sensor_twobyte(0X602A, 0X0B62);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0186);     \
	write_cmos_sensor_twobyte(0X602A, 0X5160);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0000);     \
	write_cmos_sensor_twobyte(0X602A, 0X09F2);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0180);     \
	write_cmos_sensor_twobyte(0X6F12, 0X2180);     \
	write_cmos_sensor_twobyte(0X6F12, 0XF41E);     \
	write_cmos_sensor_twobyte(0X602A, 0X0B06);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0800);     \
	write_cmos_sensor_twobyte(0X602A, 0X5694);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0000);     \
	write_cmos_sensor_twobyte(0X602A, 0X569C);     \
	write_cmos_sensor_twobyte(0X6F12, 0X000C);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0100);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0200);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0100);     \
	write_cmos_sensor_twobyte(0X602A, 0X56D8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0000);     \
	write_cmos_sensor_twobyte(0X602A, 0X56E0);     \
	write_cmos_sensor_twobyte(0X6F12, 0X000C);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0100);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0200);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0100);     \
	write_cmos_sensor_twobyte(0X6028, 0X4000);     \
	write_cmos_sensor_twobyte(0X6214, 0X79F0);     \
	write_cmos_sensor_twobyte(0X6218, 0X79F0);     \
} while (0)

#endif
