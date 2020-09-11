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
 *   ov5645_mipi_yuv_Sensor.h
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

/* SENSOR FULL SIZE */
#ifndef __SENSOR_H
#define __SENSOR_H


enum OV5645MIPI_SENSOR_MODE {
	SENSOR_MODE_INIT = 0,
	SENSOR_MODE_PREVIEW,
	SENSOR_MODE_CAPTURE
};

enum OV5645MIPI_OP_TYPE {
	OV5645MIPI_MODE_NONE,
	OV5645MIPI_MODE_PREVIEW,
	OV5645MIPI_MODE_CAPTURE,
	OV5645MIPI_MODE_QCIF_VIDEO,
	OV5645MIPI_MODE_CIF_VIDEO,
	OV5645MIPI_MODE_QVGA_VIDEO
};

struct OV5645Status {
	UINT16 iSensorVersion;
	UINT16 iNightMode;
	UINT16 iWB;
	UINT16 iEffect;
	UINT16 iEV;
	UINT16 iBanding;
	UINT16 iMirror;
	UINT16 iFrameRate;
};

enum AE_SECTION_INDEX {
	AE_SECTION_INDEX_BEGIN = 0,
	AE_SECTION_INDEX_1 = AE_SECTION_INDEX_BEGIN,
	AE_SECTION_INDEX_2,
	AE_SECTION_INDEX_3,
	AE_SECTION_INDEX_4,
	AE_SECTION_INDEX_5,
	AE_SECTION_INDEX_6,
	AE_SECTION_INDEX_7,
	AE_SECTION_INDEX_8,
	AE_SECTION_INDEX_9,
	AE_SECTION_INDEX_10,
	AE_SECTION_INDEX_11,
	AE_SECTION_INDEX_12,
	AE_SECTION_INDEX_13,
	AE_SECTION_INDEX_14,
	AE_SECTION_INDEX_15,
	AE_SECTION_INDEX_16,
	AE_SECTION_INDEX_MAX
};

enum AE_VERTICAL_HORIZONTAL_BLOCKS {
	AE_VERTICAL_BLOCKS = 4,
	AE_VERTICAL_BLOCKS_MAX,
	AE_HORIZONTAL_BLOCKS = 4,
	AE_HORIZONTAL_BLOCKS_MAX
};

#define OV5645MIPI_ID_REG                          (0x300A)
#define OV5645MIPI_INFO_REG                        (0x300B)

/* sensor size */
#define OV5645MIPI_IMAGE_SENSOR_SVGA_WIDTH          (1280)	/*  */
#define OV5645MIPI_IMAGE_SENSOR_SVGA_HEIGHT         (960)	/*  */
#define OV5645MIPI_IMAGE_SENSOR_QSXGA_WITDH         (2560)	/* (2592-16) */
#define OV5645MIPI_IMAGE_SENSOR_QSXGA_HEIGHT        (1920)	/* (1944-12) */
#define OV5645MIPI_IMAGE_SENSOR_VIDEO_WITDH         (1280)	/*  */
#define OV5645MIPI_IMAGE_SENSOR_VIDEO_HEIGHT        (960)	/*  */

#define OV5645MIPI_IMAGE_SENSOR_720P_WIDTH          (1280)
#define OV5645MIPI_IMAGE_SENSOR_720P_HEIGHT         (720)


/* Sesnor Pixel/Line Numbers in One Period */
/* (1896)  Default preview line length HTS */
#define OV5645MIPI_PV_PERIOD_PIXEL_NUMS		(1896)
/* (984)   Default preview frame length  VTS */
#define OV5645MIPI_PV_PERIOD_LINE_NUMS		(984)
/* Default full size line length */
#define OV5645MIPI_FULL_PERIOD_PIXEL_NUMS		(2844)
/* Default full size frame length */
#define OV5645MIPI_FULL_PERIOD_LINE_NUMS		(1968)

/* Sensor Exposure Line Limitation */
#define OV5645MIPI_PV_EXPOSURE_LIMITATION	(984-4)
#define OV5645MIPI_FULL_EXPOSURE_LIMITATION	(1968-4)

/* Config the ISP grab start x & start y, Config the ISP grab width & height */
#define OV5645MIPI_PV_GRAB_START_X				(0)
#define OV5645MIPI_PV_GRAB_START_Y			(1)
#define OV5645MIPI_FULL_GRAB_START_X			(0)
#define OV5645MIPI_FULL_GRAB_START_Y			(1)

/*50Hz,60Hz*/
#define OV5645MIPI_NUM_50HZ                        (50 * 2)
#define OV5645MIPI_NUM_60HZ                        (60 * 2)

/* FRAME RATE UNIT */
#define OV5645MIPI_FRAME_RATE_UNIT                 (10)

/* MAX CAMERA FRAME RATE */
#define OV5645MIPI_MAX_CAMERA_FPS  (OV5645MIPI_FRAME_RATE_UNIT * 30)

#define OV5645_PREVIEW_MODE             0
#define OV5645_VIDEO_MODE               1
#define OV5645_PREVIEW_FULLSIZE_MODE    2


/* SENSOR READ/WRITE ID */
#define OV5645MIPI_WRITE_ID		0x78
#define OV5645MIPI_READ_ID		0x79

UINT32 OV5645MIPIopen(void);
UINT32 OV5645MIPIGetResolution(
		MSDK_SENSOR_RESOLUTION_INFO_STRUCT * pSensorResolution);
UINT32 OV5645MIPIGetInfo(MSDK_SCENARIO_ID_ENUM ScenarioId,
		MSDK_SENSOR_INFO_STRUCT *pSensorInfo,
			 MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData);
UINT32 OV5645MIPIControl(MSDK_SCENARIO_ID_ENUM ScenarioId,
			 MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow,
			 MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData);
UINT32 OV5645MIPIFeatureControl(MSDK_SENSOR_FEATURE_ENUM FeatureId,
		UINT8 *pFeaturePara,
		UINT32 *pFeatureParaLen);
UINT32 OV5645MIPIClose(void);
UINT32 OV5645MIPI_YUV_SensorInit(PSENSOR_FUNCTION_STRUCT pfFunc);

extern int iReadReg(u16 a_u2Addr, u8 *a_puBuff, u16 i2cId);
extern int iWriteReg(u16 a_u2Addr, u32 a_u4Data, u32 a_u4Bytes, u16 i2cId);
extern void OV5645MIPI_set_scene_mode(UINT16 para);
extern enum OV5645MIPI_OP_TYPE OV5645MIPI_g_iOV5645MIPI_Mode;

#endif				/* __SENSOR_H */
