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
 *   sensor.h
 *
 * Project:
 * --------
 *   DUMA
 *
 * Description:
 * ------------
 *   Header file of Sensor driver
 *
 *
 * Author:
 * -------
 *   PC Huang (MTK02204)
 *
 *============================================================================
 *             HISTORY
 * Below this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *------------------------------------------------------------------------------
 * $Revision:$
 * $Modtime:$
 * $Log:$
 *
 * 07 11 2011 jun.pei
 * [ALPS00059464] hi704 sensor check in
 * .
 *
 *
 *
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/
/* SENSOR FULL SIZE */
#ifndef __SENSOR_H
#define __SENSOR_H

/*
	These defines are used to synchronize initial seetings
	and following operations in driver flow.
	Naming Rule:
		#define ADDR_0xPGAD values
		PG: Page number
		AD: Register address
	P.S. Add inorder for duplicated define
*/

/* ++++ IQ Setting Synchronized Define ++++ */

	/* Page 0 */
#define ADDR_0x0010 (0x00)
#define ADDR_0x0011 (0x90)

#define ADDR_0x0020 (0x00)
#define ADDR_0x0021 (0x04)

#define ADDR_0x0040 (0x00)
#define ADDR_0x0041 (0x70)
#define ADDR_0x0042 (0x00)
#define ADDR_0x0043 (0x15)

	/* Page 10 */
#define ADDR_0x1011 (0x43)	/*(0x03)        {0x11, 0x43} */
#define ADDR_0x1012 (0x30)
#define ADDR_0x1013 (0x00)
#define ADDR_0x1040 (0x00)
#define ADDR_0x1044 (0x80)
#define ADDR_0x1045 (0x80)

#define ADDR_0x1047 (0x7f)

	/* Page 11 */
#define ADDR_0x1110 (0x21)	/*(0x25)        {0x10, 0x21} */

	/* Page 13 */
#define ADDR_0x1320 (0x03)	/*(0x07)        {0x20, 0x03} */
#define ADDR_0x1321 (0x01)	/*(0x07)        {0x21, 0x01} */

	/* Page 20 */
#define ADDR_0x2083 (0x00)
#define ADDR_0x2084 (0xc3)	/*(0xbe)        {0x84, 0xc3} */
#define ADDR_0x2085 (0x00)	/*(0x6e)        {0x85, 0x00} */
#define ADDR_0x2086 (0x00)
#define ADDR_0x2087 (0xc0)	/*(0xfa)        {0x87, 0xc0} */

#define ADDR_0x208b (0x3a)	/*(0x3f)        {0x8b, 0x3a} */
#define ADDR_0x208c (0x80)	/*(0x7a)        {0x8c, 0x80} */
#define ADDR_0x208d (0x30)	/*(0x34)        {0x8d, 0x30} */
#define ADDR_0x208e (0xc0)	/*(0xbc)        {0x8e, 0xc0} */

#define ADDR_0x209c (0x09)	/*(0x0b)        {0x9c, 0x09} */
#define ADDR_0x209d (0xc0)	/*(0xb8)        {0x9d, 0xc0} */
#define ADDR_0x209e (0x00)
#define ADDR_0x209f (0xc0)	/*(0xfa)        {0x9f, 0xc0} */

	/* Page 22 */
#define ADDR_0x2210 (0xfb)
#define ADDR_0x2211 (0x26)
#define ADDR_0x2280 (0x1b)
#define ADDR_0x2282 (0x50)
#define ADDR_0x2283 (0x55)
#define ADDR_0x2284 (0x1b)
#define ADDR_0x2285 (0x52)
#define ADDR_0x2286 (0x20)

/* ---- IQ Setting Synchronized Define ---- */

	/* follow is define by jun */
	/* SENSOR READ/WRITE ID */

#define HI704_IMAGE_SENSOR_QVGA_WIDTH       (320)
#define HI704_IMAGE_SENSOR_QVGA_HEIGHT      (240)
#define HI704_IMAGE_SENSOR_VGA_WIDTH        (640)
#define HI704_IMAGE_SENSOR_VGA_HEIGHT       (480)
#define HI704_IMAGE_SENSOR_SXGA_WIDTH       (1280)
#define HI704_IMAGE_SENSOR_SXGA_HEIGHT      (1024)

#define HI704_IMAGE_SENSOR_FULL_WIDTH	   HI704_IMAGE_SENSOR_VGA_WIDTH
#define HI704_IMAGE_SENSOR_FULL_HEIGHT	   HI704_IMAGE_SENSOR_VGA_HEIGHT

#define HI704_IMAGE_SENSOR_PV_WIDTH   HI704_IMAGE_SENSOR_VGA_WIDTH
#define HI704_IMAGE_SENSOR_PV_HEIGHT  HI704_IMAGE_SENSOR_VGA_HEIGHT

/* SENSOR PIXEL/LINE NUMBERS IN ONE PERIOD */
#define HI704_VGA_DEFAULT_PIXEL_NUMS		   (656)
#define HI704_VGA_DEFAULT_LINE_NUMS		   (500)

#define HI704_QVGA_DEFAULT_PIXEL_NUMS		   (656)
#define HI704_QVGA_DEFAULT_LINE_NUMS		   (254)

/* MAX/MIN FRAME RATE (FRAMES PER SEC.) */
#define HI704_MIN_FRAMERATE_5					(50)
#define HI704_MIN_FRAMERATE_7_5				(75)
#define HI704_MIN_FRAMERATE_10					(100)
#define HI704_MIN_FRAMERATE_15                  (150)

/* Video Fixed Framerate */
#define HI704_VIDEO_FIX_FRAMERATE_5			(50)
#define HI704_VIDEO_FIX_FRAMERATE_7_5			(75)
#define HI704_VIDEO_FIX_FRAMERATE_10			(100)
#define HI704_VIDEO_FIX_FRAMERATE_15			(150)
#define HI704_VIDEO_FIX_FRAMERATE_20			(200)
#define HI704_VIDEO_FIX_FRAMERATE_25			(250)
#define HI704_VIDEO_FIX_FRAMERATE_30			(300)


#define HI704_WRITE_ID		0x60
#define HI704_READ_ID		0x61

	/* #define HI704_SCCB_SLAVE_ADDR 0x60 */

typedef struct _SENSOR_INIT_INFO {
	kal_uint8 address;
	kal_uint8 data;
} HI704_SENSOR_INIT_INFO;
typedef enum __VIDEO_MODE__ {
	HI704_VIDEO_NORMAL = 0,
	HI704_VIDEO_MPEG4,
	HI704_VIDEO_MAX
} HI704_VIDEO_MODE;

struct HI704_sensor_STRUCT {
	kal_bool first_init;
	kal_bool pv_mode;	/* True: Preview Mode; False: Capture Mode */
	kal_bool night_mode;	/* True: Night Mode; False: Auto Mode */
	kal_bool MPEG4_Video_mode;	/* Video Mode: MJPEG or MPEG4 */
	kal_uint8 mirror;
	kal_uint32 pv_pclk;	/* Preview Pclk */
	kal_uint32 cp_pclk;	/* Capture Pclk */
	kal_uint16 pv_dummy_pixels;	/* Dummy Pixels */
	kal_uint16 pv_dummy_lines;	/* Dummy Lines */
	kal_uint16 cp_dummy_pixels;	/* Dummy Pixels */
	kal_uint16 cp_dummy_lines;	/* Dummy Lines */
	kal_uint16 fix_framerate;	/* Fixed Framerate */
	kal_uint32 wb;
	kal_uint32 exposure;
	kal_uint32 effect;
	kal_uint32 banding;
	kal_uint16 pv_line_length;
	kal_uint16 pv_frame_height;
	kal_uint16 cp_line_length;
	kal_uint16 cp_frame_height;
	kal_uint16 video_current_frame_rate;
	kal_uint16 output_format;
};

/* Extern Functions / Variables */
extern int iReadRegI2C(u8 *a_pSendData, u16 a_sizeSendData, u8 *a_pRecvData, u16 a_sizeRecvData,
		       u16 i2cId);
extern int iWriteRegI2C(u8 *a_pSendData, u16 a_sizeSendData, u16 i2cId);
extern int sub_sensor_init_setting_switch;

/* export functions */
UINT32 HI704Open(void);
UINT32 HI704GetResolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *pSensorResolution);
UINT32 HI704GetInfo(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_INFO_STRUCT *pSensorInfo,
		    MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData);
UINT32 HI704Control(MSDK_SCENARIO_ID_ENUM ScenarioId,
		    MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow,
		    MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData);
UINT32 HI704FeatureControl(MSDK_SENSOR_FEATURE_ENUM FeatureId, UINT8 *pFeaturePara,
			   UINT32 *pFeatureParaLen);
UINT32 HI704Close(void);
extern int sub_sensor_init_setting_switch;	/* Add for build pass */

#endif				/* __SENSOR_H */
