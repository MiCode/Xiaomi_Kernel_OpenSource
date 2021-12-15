/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*****************************************************************************
 *
 * Filename:
 * ---------
 *     s5k2l7_setting_mode2.h
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
#ifndef _s5k2l7MIPI_SETTING_MODE2_V2_H_
#define _s5k2l7MIPI_SETTING_MODE2_V2_H_

#define _S5K2L7_MODE2_V2_SENSOR_INFO_                                          \
static struct imgsensor_info_struct _imgsensor_info_m2_v2 =                    \
{                                                                              \
	/* record sensor id defined in Kd_imgsensor.h */		       \
	.sensor_id = S5K2L7_SENSOR_ID,                                         \
	.checksum_value = 0xb4cb9203,/* checksum value for Camera Auto Test  */\
	.pre = {                                                               \
		.pclk = 960000000, /* record different mode's pclk */          \
		.linelength = 7800, /* record different mode's linelength */   \
		.framelength = 4100, /* record different mode's framelength */ \
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
		.pclk = 960000000,    /* record different mode's pclk */       \
		.linelength = 10160,  /* record different mode's linelength */ \
		.framelength = 3149,  /* record different mode's framelength */\
		.startx = 0, /* record different mode's startx of grabwindow */\
		.starty = 0, /* record different mode's starty of grabwindow */\
	/* Dual PD: need to tg grab width / 2, p1 drv will * 2 itself */       \
		.grabwindow_width = 4032,				       \
		/* record different mode's height of grabwindow */             \
		.grabwindow_height = 3024,                                     \
		.mipi_data_lp2hs_settle_dc = 85, /* unit , ns */               \
		.max_framerate = 300                                           \
	},                                                                     \
	.cap1 = {                                                              \
		.pclk = 960000000, /* record different mode's pclk  */         \
		.linelength = 10160, /* record different mode's linelength  */ \
		.framelength = 3149, /* record different mode's framelength */ \
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
		.linelength = 10160, /* record different mode's linelength */  \
		.framelength = 3149, /* record different mode's framelength */ \
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
		.linelength = 7800,                                            \
		.framelength = 1024,                                           \
		.startx = 0,                                                   \
		.starty = 0,                                                   \
		/* record different mode's width of grabwindow */              \
		.grabwindow_width = 1344,                                      \
		/* record different mode's height of grabwindow */             \
		.grabwindow_height = 756,                                      \
		.mipi_data_lp2hs_settle_dc = 85, /* unit , ns */               \
		.max_framerate = 1200,                                         \
	},                                                                     \
	.slim_video = {                                                        \
		.pclk = 960000000,                                             \
		.linelength = 7800,                                            \
		.framelength = 4096,                                           \
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
	.min_gain = 64,                                                        \
	.max_gain = 1024,                                                      \
	.min_gain_iso = 100,                                                   \
	.gain_step = 2,                                                       \
	.gain_type = 2,                                                        \
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
#define _S5K2L7_MODE2_V2_WINSIZE_INFO_                                         \
static struct_SENSOR_WINSIZE_INFO _imgsensor_winsize_info_m2_v2[5] =           \
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

#define _SET_MODE2_V2_SENSOR_INFO_AND_WINSIZE_ do {                            \
	memcpy((void *)&imgsensor_info,                                        \
		(void *)&_imgsensor_info_m2_v2,                                \
		sizeof(struct imgsensor_info_struct));                         \
	memcpy((void *)&imgsensor_winsize_info,			               \
		(void *)&_imgsensor_winsize_info_m2_v2,                        \
		sizeof(struct SENSOR_WINSIZE_INFO_STRUCT)*5);                  \
} while (0)


/*****************************************************************************
 *
 * Description:
 * ------------
 *     mode 2 initial setting
 *
 ****************************************************************************/
#define _S5K2L7_MODE2_INIT_MODULE_V2_ do {         \
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
	write_cmos_sensor_twobyte(0X602A, 0X1638); \
	write_cmos_sensor_twobyte(0X6F12, 0X0808); \
	write_cmos_sensor_twobyte(0X602A, 0X1630); \
	write_cmos_sensor_twobyte(0X6F12, 0X0808); \
	write_cmos_sensor_twobyte(0X602A, 0X160E); \
	write_cmos_sensor_twobyte(0X6F12, 0X0000); \
	write_cmos_sensor_twobyte(0X602A, 0X16CE); \
	write_cmos_sensor_twobyte(0X6F12, 0X0800); \
	write_cmos_sensor_twobyte(0X6F12, 0X0100); \
	write_cmos_sensor_twobyte(0X602A, 0X0BD8); \
	write_cmos_sensor_twobyte(0X6F12, 0X0420); \
} while (0)


/*****************************************************************************
 *
 * Description:
 * ------------
 *     mode 2 preview setting
 *
 ****************************************************************************/
#define _S5K2L7_MODE2_PREVIEW_MODULE_V2_ do {          \
	write_cmos_sensor_twobyte(0X6028, 0X4000);     \
	write_cmos_sensor_twobyte(0X6214, 0X7970);     \
	write_cmos_sensor_twobyte(0X6218, 0X7150);     \
	write_cmos_sensor_twobyte(0X0344, 0X0000);     \
	write_cmos_sensor_twobyte(0X0346, 0X0000);     \
	write_cmos_sensor_twobyte(0X0348, 0X1FBF);     \
	write_cmos_sensor_twobyte(0X034A, 0X0BDF);     \
	write_cmos_sensor_twobyte(0X034C, 0X07E0);     \
	write_cmos_sensor_twobyte(0X034E, 0X05E8);     \
	write_cmos_sensor_twobyte(0X0408, 0X0008);     \
	write_cmos_sensor_twobyte(0X040A, 0X0004);     \
	write_cmos_sensor_twobyte(0X0900, 0X0242);     \
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
	write_cmos_sensor_twobyte(0X0310, 0X00EA);     \
	write_cmos_sensor_twobyte(0X0312, 0X0001);     \
	write_cmos_sensor_twobyte(0X030A, 0X0001);     \
	write_cmos_sensor_twobyte(0X0308, 0X0008);     \
	write_cmos_sensor_twobyte(0X0342, 0X1E78);     \
	write_cmos_sensor_twobyte(0X0340, 0X1004);     \
	write_cmos_sensor_twobyte(0X021E, 0X0000);     \
	write_cmos_sensor_twobyte(0X3098, 0X0100);     \
	write_cmos_sensor_twobyte(0X309A, 0X0002);     \
	write_cmos_sensor_twobyte(0X30BC, 0X0031);     \
	write_cmos_sensor_twobyte(0X30A6, 0X0000);     \
	write_cmos_sensor_twobyte(0X30A8, 0X0000);     \
	write_cmos_sensor_twobyte(0X30AA, 0X0000);     \
	write_cmos_sensor_twobyte(0X30AC, 0X0000);     \
	write_cmos_sensor_twobyte(0X30A0, 0X0000);     \
	write_cmos_sensor_twobyte(0X30A4, 0X0000);     \
	write_cmos_sensor_twobyte(0X6A0C, 0XFFFF);     \
	write_cmos_sensor_twobyte(0XF41E, 0X2100);     \
	write_cmos_sensor_twobyte(0X6028, 0X2000);     \
	write_cmos_sensor_twobyte(0X602A, 0X0990);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0040);     \
	write_cmos_sensor_twobyte(0X602A, 0X0AF8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0008);     \
	write_cmos_sensor_twobyte(0X602A, 0X27B8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0000);     \
	write_cmos_sensor_twobyte(0X602A, 0X2AA8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0000);     \
	write_cmos_sensor_twobyte(0X602A, 0X09AC);     \
	write_cmos_sensor_twobyte(0X6F12, 0X07FF);     \
	write_cmos_sensor_twobyte(0X602A, 0X09AA);     \
	write_cmos_sensor_twobyte(0X6F12, 0X07FF);     \
	write_cmos_sensor_twobyte(0X602A, 0X0B02);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0180);     \
	write_cmos_sensor_twobyte(0X602A, 0X1698);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0080);     \
	write_cmos_sensor_twobyte(0X602A, 0X16C6);     \
	write_cmos_sensor_twobyte(0X6F12, 0X12AF);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0328);     \
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
	write_cmos_sensor_twobyte(0X602A, 0X1604);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0104);     \
	write_cmos_sensor_twobyte(0X602A, 0X14CA);     \
	write_cmos_sensor_twobyte(0X6F12, 0X027C);     \
	write_cmos_sensor_twobyte(0X602A, 0X0DAA);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0418);     \
	write_cmos_sensor_twobyte(0X602A, 0X0D9A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0418);     \
	write_cmos_sensor_twobyte(0X602A, 0X0D7A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0418);     \
	write_cmos_sensor_twobyte(0X602A, 0X0C22);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0418);     \
	write_cmos_sensor_twobyte(0X602A, 0X0B16);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0100);     \
	write_cmos_sensor_twobyte(0X602A, 0X0CE8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0353);     \
	write_cmos_sensor_twobyte(0X602A, 0X0CD8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0355);     \
	write_cmos_sensor_twobyte(0X602A, 0X0C20);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0361);     \
	write_cmos_sensor_twobyte(0X602A, 0X0D78);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0361);     \
	write_cmos_sensor_twobyte(0X602A, 0X0D98);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0361);     \
	write_cmos_sensor_twobyte(0X602A, 0X1168);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0352);     \
	write_cmos_sensor_twobyte(0X602A, 0X1070);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0353);     \
	write_cmos_sensor_twobyte(0X602A, 0X1038);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0355);     \
	write_cmos_sensor_twobyte(0X602A, 0X1030);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0352);     \
	write_cmos_sensor_twobyte(0X602A, 0X0FF8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0357);     \
	write_cmos_sensor_twobyte(0X602A, 0X0F48);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0354);     \
	write_cmos_sensor_twobyte(0X602A, 0X0F40);     \
	write_cmos_sensor_twobyte(0X6F12, 0X033E);     \
	write_cmos_sensor_twobyte(0X602A, 0X0F38);     \
	write_cmos_sensor_twobyte(0X6F12, 0X033C);     \
	write_cmos_sensor_twobyte(0X602A, 0X0F30);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0326);     \
	write_cmos_sensor_twobyte(0X602A, 0X0F28);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0324);     \
	write_cmos_sensor_twobyte(0X602A, 0X0F20);     \
	write_cmos_sensor_twobyte(0X6F12, 0X030E);     \
	write_cmos_sensor_twobyte(0X602A, 0X0E88);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0353);     \
	write_cmos_sensor_twobyte(0X602A, 0X0DE8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0351);     \
	write_cmos_sensor_twobyte(0X602A, 0X0DC8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0355);     \
	write_cmos_sensor_twobyte(0X602A, 0X0DB8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0355);     \
	write_cmos_sensor_twobyte(0X602A, 0X0DA8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0361);     \
	write_cmos_sensor_twobyte(0X602A, 0X15EC);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0006);     \
	write_cmos_sensor_twobyte(0X602A, 0X0AEC);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0208);     \
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
	write_cmos_sensor_twobyte(0X6F12, 0X0260);     \
	write_cmos_sensor_twobyte(0X602A, 0X5160);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0100);     \
	write_cmos_sensor_twobyte(0X602A, 0X09F2);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0000);     \
	write_cmos_sensor_twobyte(0X6F12, 0X2000);     \
	write_cmos_sensor_twobyte(0X6F12, 0XF41E);     \
	write_cmos_sensor_twobyte(0X602A, 0X0B06);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0800);     \
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
 *
 ****************************************************************************/
#define _S5K2L7_MODE2_CAPTURE_MODULE_V2_ do {          \
	write_cmos_sensor_twobyte(0X6028, 0X4000);     \
	write_cmos_sensor_twobyte(0X6214, 0X7970);     \
	write_cmos_sensor_twobyte(0X6218, 0X7150);     \
	write_cmos_sensor_twobyte(0X0344, 0X0000);     \
	write_cmos_sensor_twobyte(0X0346, 0X0000);     \
	write_cmos_sensor_twobyte(0X0348, 0X1FBF);     \
	write_cmos_sensor_twobyte(0X034A, 0X0BDF);     \
	write_cmos_sensor_twobyte(0X034C, 0X0FC0);     \
	write_cmos_sensor_twobyte(0X034E, 0X0BD0);     \
	write_cmos_sensor_twobyte(0X0408, 0X0010);     \
	write_cmos_sensor_twobyte(0X040A, 0X0008);     \
	write_cmos_sensor_twobyte(0X0900, 0X0221);     \
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
	write_cmos_sensor_twobyte(0X0310, 0X00AF);     \
	write_cmos_sensor_twobyte(0X0312, 0X0000);     \
	write_cmos_sensor_twobyte(0X030A, 0X0001);     \
	write_cmos_sensor_twobyte(0X0308, 0X0008);     \
	write_cmos_sensor_twobyte(0X0342, 0X27B0);     \
	write_cmos_sensor_twobyte(0X0340, 0X0C4D);     \
	write_cmos_sensor_twobyte(0X021E, 0X0000);     \
	write_cmos_sensor_twobyte(0X3098, 0X0100);     \
	write_cmos_sensor_twobyte(0X309A, 0X0002);     \
	write_cmos_sensor_twobyte(0X30BC, 0X0031);     \
	write_cmos_sensor_twobyte(0X30A6, 0X0000);     \
	write_cmos_sensor_twobyte(0X30A8, 0X0000);     \
	write_cmos_sensor_twobyte(0X30AA, 0X0000);     \
	write_cmos_sensor_twobyte(0X30AC, 0X0000);     \
	write_cmos_sensor_twobyte(0X30A0, 0X0000);     \
	write_cmos_sensor_twobyte(0X30A4, 0X0000);     \
	write_cmos_sensor_twobyte(0X6A0C, 0XFFFF);     \
	write_cmos_sensor_twobyte(0XF41E, 0X2100);     \
	write_cmos_sensor_twobyte(0X6028, 0X2000);     \
	write_cmos_sensor_twobyte(0X602A, 0X0990);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0040);     \
	write_cmos_sensor_twobyte(0X602A, 0X0AF8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0004);     \
	write_cmos_sensor_twobyte(0X602A, 0X27B8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0100);     \
	write_cmos_sensor_twobyte(0X602A, 0X2AA8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0000);     \
	write_cmos_sensor_twobyte(0X602A, 0X09AC);     \
	write_cmos_sensor_twobyte(0X6F12, 0X07FF);     \
	write_cmos_sensor_twobyte(0X602A, 0X09AA);     \
	write_cmos_sensor_twobyte(0X6F12, 0X07FF);     \
	write_cmos_sensor_twobyte(0X602A, 0X0B02);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0100);     \
	write_cmos_sensor_twobyte(0X602A, 0X1698);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0082);     \
	write_cmos_sensor_twobyte(0X602A, 0X16C6);     \
	write_cmos_sensor_twobyte(0X6F12, 0X122F);     \
	write_cmos_sensor_twobyte(0X6F12, 0X4328);     \
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
	write_cmos_sensor_twobyte(0X602A, 0X1604);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0004);     \
	write_cmos_sensor_twobyte(0X602A, 0X14CA);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0096);     \
	write_cmos_sensor_twobyte(0X602A, 0X0DAA);     \
	write_cmos_sensor_twobyte(0X6F12, 0X028A);     \
	write_cmos_sensor_twobyte(0X602A, 0X0D9A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X028A);     \
	write_cmos_sensor_twobyte(0X602A, 0X0D7A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X028A);     \
	write_cmos_sensor_twobyte(0X602A, 0X0C22);     \
	write_cmos_sensor_twobyte(0X6F12, 0X028A);     \
	write_cmos_sensor_twobyte(0X602A, 0X0B16);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0200);     \
	write_cmos_sensor_twobyte(0X602A, 0X0CE8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X030D);     \
	write_cmos_sensor_twobyte(0X602A, 0X0CD8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X030F);     \
	write_cmos_sensor_twobyte(0X602A, 0X0C20);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0570);     \
	write_cmos_sensor_twobyte(0X602A, 0X0D78);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0570);     \
	write_cmos_sensor_twobyte(0X602A, 0X0D98);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0570);     \
	write_cmos_sensor_twobyte(0X602A, 0X1168);     \
	write_cmos_sensor_twobyte(0X6F12, 0X030C);     \
	write_cmos_sensor_twobyte(0X602A, 0X1070);     \
	write_cmos_sensor_twobyte(0X6F12, 0X030D);     \
	write_cmos_sensor_twobyte(0X602A, 0X1038);     \
	write_cmos_sensor_twobyte(0X6F12, 0X030F);     \
	write_cmos_sensor_twobyte(0X602A, 0X1030);     \
	write_cmos_sensor_twobyte(0X6F12, 0X030C);     \
	write_cmos_sensor_twobyte(0X602A, 0X0FF8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0311);     \
	write_cmos_sensor_twobyte(0X602A, 0X0F48);     \
	write_cmos_sensor_twobyte(0X6F12, 0X030E);     \
	write_cmos_sensor_twobyte(0X602A, 0X0F40);     \
	write_cmos_sensor_twobyte(0X6F12, 0X02F8);     \
	write_cmos_sensor_twobyte(0X602A, 0X0F38);     \
	write_cmos_sensor_twobyte(0X6F12, 0X02F6);     \
	write_cmos_sensor_twobyte(0X602A, 0X0F30);     \
	write_cmos_sensor_twobyte(0X6F12, 0X02E0);     \
	write_cmos_sensor_twobyte(0X602A, 0X0F28);     \
	write_cmos_sensor_twobyte(0X6F12, 0X02DE);     \
	write_cmos_sensor_twobyte(0X602A, 0X0F20);     \
	write_cmos_sensor_twobyte(0X6F12, 0X02C8);     \
	write_cmos_sensor_twobyte(0X602A, 0X0E88);     \
	write_cmos_sensor_twobyte(0X6F12, 0X030D);     \
	write_cmos_sensor_twobyte(0X602A, 0X0DE8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X030B);     \
	write_cmos_sensor_twobyte(0X602A, 0X0DC8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X030F);     \
	write_cmos_sensor_twobyte(0X602A, 0X0DB8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X030F);     \
	write_cmos_sensor_twobyte(0X602A, 0X0DA8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0570);     \
	write_cmos_sensor_twobyte(0X602A, 0X15EC);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0001);     \
	write_cmos_sensor_twobyte(0X602A, 0X0AEC);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0207);     \
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
	write_cmos_sensor_twobyte(0X6F12, 0X02C0);     \
	write_cmos_sensor_twobyte(0X602A, 0X5160);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0000);     \
	write_cmos_sensor_twobyte(0X602A, 0X09F2);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0000);     \
	write_cmos_sensor_twobyte(0X6F12, 0X2000);     \
	write_cmos_sensor_twobyte(0X6F12, 0XF41E);     \
	write_cmos_sensor_twobyte(0X602A, 0X0B06);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0800);     \
	write_cmos_sensor_twobyte(0X6028, 0X4000);     \
	write_cmos_sensor_twobyte(0X6214, 0X79F0);     \
	write_cmos_sensor_twobyte(0X6218, 0X79F0);     \
	sensor_WDR_zhdr();                             \
} while (0)


/*****************************************************************************
 *
 * Description:
 * ------------
 *     mode 2 high speed video setting
 *
 ****************************************************************************/
#define _S5K2L7_MODE2_HS_VIDEO_MODULE_V2_ do {         \
	write_cmos_sensor_twobyte(0X6028, 0X4000);     \
	write_cmos_sensor_twobyte(0X6214, 0X7970);     \
	write_cmos_sensor_twobyte(0X6218, 0X7150);     \
	write_cmos_sensor_twobyte(0X0344, 0X0000);     \
	write_cmos_sensor_twobyte(0X0346, 0X016E);     \
	write_cmos_sensor_twobyte(0X0348, 0X1FDF);     \
	write_cmos_sensor_twobyte(0X034A, 0X0A61);     \
	write_cmos_sensor_twobyte(0X034C, 0X0540);     \
	write_cmos_sensor_twobyte(0X034E, 0X02F4);     \
	write_cmos_sensor_twobyte(0X0408, 0X0008);     \
	write_cmos_sensor_twobyte(0X040A, 0X0004);     \
	write_cmos_sensor_twobyte(0X0900, 0X0223);     \
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
	write_cmos_sensor_twobyte(0X0342, 0X1E78);     \
	write_cmos_sensor_twobyte(0X0340, 0X0400);     \
	write_cmos_sensor_twobyte(0X021E, 0X0000);     \
	write_cmos_sensor_twobyte(0X3098, 0X0100);     \
	write_cmos_sensor_twobyte(0X309A, 0X0002);     \
	write_cmos_sensor_twobyte(0X30BC, 0X0031);     \
	write_cmos_sensor_twobyte(0X30A6, 0X0000);     \
	write_cmos_sensor_twobyte(0X30A8, 0X0000);     \
	write_cmos_sensor_twobyte(0X30AA, 0X0000);     \
	write_cmos_sensor_twobyte(0X30AC, 0X0000);     \
	write_cmos_sensor_twobyte(0X30A0, 0X0000);     \
	write_cmos_sensor_twobyte(0X30A4, 0X0000);     \
	write_cmos_sensor_twobyte(0X6A0C, 0XFFFF);     \
	write_cmos_sensor_twobyte(0XF41E, 0X2100);     \
	write_cmos_sensor_twobyte(0X6028, 0X2000);     \
	write_cmos_sensor_twobyte(0X602A, 0X0990);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0040);     \
	write_cmos_sensor_twobyte(0X602A, 0X0AF8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X000C);     \
	write_cmos_sensor_twobyte(0X602A, 0X27B8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0000);     \
	write_cmos_sensor_twobyte(0X602A, 0X2AA8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0000);     \
	write_cmos_sensor_twobyte(0X602A, 0X09AC);     \
	write_cmos_sensor_twobyte(0X6F12, 0X07FF);     \
	write_cmos_sensor_twobyte(0X602A, 0X09AA);     \
	write_cmos_sensor_twobyte(0X6F12, 0X07FF);     \
	write_cmos_sensor_twobyte(0X602A, 0X0B02);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0180);     \
	write_cmos_sensor_twobyte(0X602A, 0X1698);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0080);     \
	write_cmos_sensor_twobyte(0X602A, 0X16C6);     \
	write_cmos_sensor_twobyte(0X6F12, 0X122F);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0328);     \
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
	write_cmos_sensor_twobyte(0X602A, 0X1604);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0104);     \
	write_cmos_sensor_twobyte(0X602A, 0X14CA);     \
	write_cmos_sensor_twobyte(0X6F12, 0X027C);     \
	write_cmos_sensor_twobyte(0X602A, 0X0DAA);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0418);     \
	write_cmos_sensor_twobyte(0X602A, 0X0D9A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0418);     \
	write_cmos_sensor_twobyte(0X602A, 0X0D7A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0418);     \
	write_cmos_sensor_twobyte(0X602A, 0X0C22);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0418);     \
	write_cmos_sensor_twobyte(0X602A, 0X0B16);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0100);     \
	write_cmos_sensor_twobyte(0X602A, 0X0CE8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0353);     \
	write_cmos_sensor_twobyte(0X602A, 0X0CD8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0355);     \
	write_cmos_sensor_twobyte(0X602A, 0X0C20);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0361);     \
	write_cmos_sensor_twobyte(0X602A, 0X0D78);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0361);     \
	write_cmos_sensor_twobyte(0X602A, 0X0D98);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0361);     \
	write_cmos_sensor_twobyte(0X602A, 0X1168);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0352);     \
	write_cmos_sensor_twobyte(0X602A, 0X1070);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0353);     \
	write_cmos_sensor_twobyte(0X602A, 0X1038);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0355);     \
	write_cmos_sensor_twobyte(0X602A, 0X1030);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0352);     \
	write_cmos_sensor_twobyte(0X602A, 0X0FF8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0357);     \
	write_cmos_sensor_twobyte(0X602A, 0X0F48);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0354);     \
	write_cmos_sensor_twobyte(0X602A, 0X0F40);     \
	write_cmos_sensor_twobyte(0X6F12, 0X033E);     \
	write_cmos_sensor_twobyte(0X602A, 0X0F38);     \
	write_cmos_sensor_twobyte(0X6F12, 0X033C);     \
	write_cmos_sensor_twobyte(0X602A, 0X0F30);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0326);     \
	write_cmos_sensor_twobyte(0X602A, 0X0F28);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0324);     \
	write_cmos_sensor_twobyte(0X602A, 0X0F20);     \
	write_cmos_sensor_twobyte(0X6F12, 0X030E);     \
	write_cmos_sensor_twobyte(0X602A, 0X0E88);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0353);     \
	write_cmos_sensor_twobyte(0X602A, 0X0DE8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0351);     \
	write_cmos_sensor_twobyte(0X602A, 0X0DC8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0355);     \
	write_cmos_sensor_twobyte(0X602A, 0X0DB8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0355);     \
	write_cmos_sensor_twobyte(0X602A, 0X0DA8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0361);     \
	write_cmos_sensor_twobyte(0X602A, 0X15EC);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0006);     \
	write_cmos_sensor_twobyte(0X602A, 0X0AEC);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0207);     \
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
	write_cmos_sensor_twobyte(0X6F12, 0X00F0);     \
	write_cmos_sensor_twobyte(0X602A, 0X5160);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0100);     \
	write_cmos_sensor_twobyte(0X602A, 0X09F2);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0000);     \
	write_cmos_sensor_twobyte(0X6F12, 0X2000);     \
	write_cmos_sensor_twobyte(0X6F12, 0XF41E);     \
	write_cmos_sensor_twobyte(0X602A, 0X0B06);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0800);     \
	write_cmos_sensor_twobyte(0X6028, 0X4000);     \
	write_cmos_sensor_twobyte(0X6214, 0X79F0);     \
	write_cmos_sensor_twobyte(0X6218, 0X79F0);     \
} while (0)

/*****************************************************************************
 *
 * Description:
 * ------------
 *     mode 2 slim video setting
 *
 ****************************************************************************/
#define _S5K2L7_MODE2_SLIM_VIDEO_MODULE_V2_ do {       \
	write_cmos_sensor_twobyte(0X6028, 0X4000);     \
	write_cmos_sensor_twobyte(0X6214, 0X7970);     \
	write_cmos_sensor_twobyte(0X6218, 0X7150);     \
	write_cmos_sensor_twobyte(0X0344, 0X0000);     \
	write_cmos_sensor_twobyte(0X0346, 0X016E);     \
	write_cmos_sensor_twobyte(0X0348, 0X1FDF);     \
	write_cmos_sensor_twobyte(0X034A, 0X0A61);     \
	write_cmos_sensor_twobyte(0X034C, 0X0540);     \
	write_cmos_sensor_twobyte(0X034E, 0X02F4);     \
	write_cmos_sensor_twobyte(0X0408, 0X0008);     \
	write_cmos_sensor_twobyte(0X040A, 0X0004);     \
	write_cmos_sensor_twobyte(0X0900, 0X0223);     \
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
	write_cmos_sensor_twobyte(0X0342, 0X1E78);     \
	write_cmos_sensor_twobyte(0X0340, 0X1000);     \
	write_cmos_sensor_twobyte(0X021E, 0X0000);     \
	write_cmos_sensor_twobyte(0X3098, 0X0100);     \
	write_cmos_sensor_twobyte(0X309A, 0X0002);     \
	write_cmos_sensor_twobyte(0X30BC, 0X0031);     \
	write_cmos_sensor_twobyte(0X30A6, 0X0000);     \
	write_cmos_sensor_twobyte(0X30A8, 0X0000);     \
	write_cmos_sensor_twobyte(0X30AA, 0X0000);     \
	write_cmos_sensor_twobyte(0X30AC, 0X0000);     \
	write_cmos_sensor_twobyte(0X30A0, 0X0000);     \
	write_cmos_sensor_twobyte(0X30A4, 0X0000);     \
	write_cmos_sensor_twobyte(0X6A0C, 0XFFFF);     \
	write_cmos_sensor_twobyte(0XF41E, 0X2100);     \
	write_cmos_sensor_twobyte(0X6028, 0X2000);     \
	write_cmos_sensor_twobyte(0X602A, 0X0990);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0040);     \
	write_cmos_sensor_twobyte(0X602A, 0X0AF8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X000C);     \
	write_cmos_sensor_twobyte(0X602A, 0X27B8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0000);     \
	write_cmos_sensor_twobyte(0X602A, 0X2AA8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0000);     \
	write_cmos_sensor_twobyte(0X602A, 0X09AC);     \
	write_cmos_sensor_twobyte(0X6F12, 0X07FF);     \
	write_cmos_sensor_twobyte(0X602A, 0X09AA);     \
	write_cmos_sensor_twobyte(0X6F12, 0X07FF);     \
	write_cmos_sensor_twobyte(0X602A, 0X0B02);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0180);     \
	write_cmos_sensor_twobyte(0X602A, 0X1698);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0080);     \
	write_cmos_sensor_twobyte(0X602A, 0X16C6);     \
	write_cmos_sensor_twobyte(0X6F12, 0X122F);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0328);     \
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
	write_cmos_sensor_twobyte(0X602A, 0X1604);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0104);     \
	write_cmos_sensor_twobyte(0X602A, 0X14CA);     \
	write_cmos_sensor_twobyte(0X6F12, 0X027C);     \
	write_cmos_sensor_twobyte(0X602A, 0X0DAA);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0418);     \
	write_cmos_sensor_twobyte(0X602A, 0X0D9A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0418);     \
	write_cmos_sensor_twobyte(0X602A, 0X0D7A);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0418);     \
	write_cmos_sensor_twobyte(0X602A, 0X0C22);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0418);     \
	write_cmos_sensor_twobyte(0X602A, 0X0B16);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0100);     \
	write_cmos_sensor_twobyte(0X602A, 0X0CE8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0353);     \
	write_cmos_sensor_twobyte(0X602A, 0X0CD8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0355);     \
	write_cmos_sensor_twobyte(0X602A, 0X0C20);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0361);     \
	write_cmos_sensor_twobyte(0X602A, 0X0D78);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0361);     \
	write_cmos_sensor_twobyte(0X602A, 0X0D98);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0361);     \
	write_cmos_sensor_twobyte(0X602A, 0X1168);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0352);     \
	write_cmos_sensor_twobyte(0X602A, 0X1070);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0353);     \
	write_cmos_sensor_twobyte(0X602A, 0X1038);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0355);     \
	write_cmos_sensor_twobyte(0X602A, 0X1030);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0352);     \
	write_cmos_sensor_twobyte(0X602A, 0X0FF8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0357);     \
	write_cmos_sensor_twobyte(0X602A, 0X0F48);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0354);     \
	write_cmos_sensor_twobyte(0X602A, 0X0F40);     \
	write_cmos_sensor_twobyte(0X6F12, 0X033E);     \
	write_cmos_sensor_twobyte(0X602A, 0X0F38);     \
	write_cmos_sensor_twobyte(0X6F12, 0X033C);     \
	write_cmos_sensor_twobyte(0X602A, 0X0F30);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0326);     \
	write_cmos_sensor_twobyte(0X602A, 0X0F28);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0324);     \
	write_cmos_sensor_twobyte(0X602A, 0X0F20);     \
	write_cmos_sensor_twobyte(0X6F12, 0X030E);     \
	write_cmos_sensor_twobyte(0X602A, 0X0E88);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0353);     \
	write_cmos_sensor_twobyte(0X602A, 0X0DE8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0351);     \
	write_cmos_sensor_twobyte(0X602A, 0X0DC8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0355);     \
	write_cmos_sensor_twobyte(0X602A, 0X0DB8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0355);     \
	write_cmos_sensor_twobyte(0X602A, 0X0DA8);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0361);     \
	write_cmos_sensor_twobyte(0X602A, 0X15EC);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0006);     \
	write_cmos_sensor_twobyte(0X602A, 0X0AEC);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0207);     \
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
	write_cmos_sensor_twobyte(0X6F12, 0X00F0);     \
	write_cmos_sensor_twobyte(0X602A, 0X5160);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0100);     \
	write_cmos_sensor_twobyte(0X602A, 0X09F2);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0000);     \
	write_cmos_sensor_twobyte(0X6F12, 0X2000);     \
	write_cmos_sensor_twobyte(0X6F12, 0XF41E);     \
	write_cmos_sensor_twobyte(0X602A, 0X0B06);     \
	write_cmos_sensor_twobyte(0X6F12, 0X0800);     \
	write_cmos_sensor_twobyte(0X6028, 0X4000);     \
	write_cmos_sensor_twobyte(0X6214, 0X79F0);     \
	write_cmos_sensor_twobyte(0X6218, 0X79F0);     \
} while (0)

/*****************************************************************************
 *
 * Description:
 * ------------
 *     mode 2 cpature with WDR setting
 *
 ****************************************************************************/
#define _S5K2L7_MODE2_CAPTURE_WDR_V2_ do { \
} while (0)

#endif
