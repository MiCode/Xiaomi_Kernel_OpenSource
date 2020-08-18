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
 *   ov5645_mipi_yuv_Sensor.c
 *
 * Project:
 * --------
 *     ALPS
 *
 * Description:
 * ------------
 *     Source code of Sensor driver
 *
 *
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/atomic.h>
#include <linux/io.h>

#include "kd_camera_hw.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
#include "kd_camera_feature.h"
#include "ov5645_mipi_yuv_Sensor.h"

#define PFX         "OV5645_MIPI_YUV"
/* #define OV5645MIPIYUV_DEBUG */
#ifdef OV5645MIPIYUV_DEBUG
#define LOG_INF(format, args...)   \
		pr_debug(PFX "[%s] " format, __func__, ##args)
#else
#define LOG_INF(format, args...)
#endif

static DEFINE_SPINLOCK(ov5645mipi_drv_lock);
static MSDK_SCENARIO_ID_ENUM CurrentScenarioId =
						MSDK_SCENARIO_ID_CAMERA_PREVIEW;

#define OV5645MIPI_write_cmos_sensor(addr, para)   \
	iWriteReg((u16) addr, (u32) para, 1, OV5645MIPI_WRITE_ID)
#define mDELAY(ms)  mdelay(ms)
#define SETTLE_DELAY 85

kal_uint8 OV5645MIPI_sensor_socket = DUAL_CAMERA_NONE_SENSOR;

enum PREVIEW_VIEW_SIZE {
	PRV_W = 1280,
	PRV_H = 960
};

kal_uint16 OV5645MIPIYUV_read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;

	iReadReg((u16) addr, (u8 *) &get_byte, OV5645MIPI_WRITE_ID);
	return get_byte;
}

static struct {
	/* kal_uint8   Banding; */
	kal_bool NightMode;
	kal_bool VideoMode;
	kal_uint16 Fps;
	kal_uint16 ShutterStep;
	kal_uint8 IsPVmode;
	kal_uint32 PreviewDummyPixels;
	kal_uint32 PreviewDummyLines;
	kal_uint32 CaptureDummyPixels;
	kal_uint32 CaptureDummyLines;
	kal_uint32 PreviewPclk;
	kal_uint32 CapturePclk;
	kal_uint32 PreviewShutter;
	kal_uint32 PreviewExtraShutter;
	kal_uint32 SensorGain;
	enum OV5645MIPI_SENSOR_MODE SensorMode;
} OV5645MIPISensor;

struct OV5645Status OV5645CurrentStatus;

/* Global Valuable */
static kal_uint32 zoom_factor;
static kal_uint8 OV5645MIPI_Banding_setting = AE_FLICKER_MODE_50HZ;
static kal_bool OV5645MIPI_AWB_ENABLE = KAL_TRUE;
static kal_bool OV5645MIPI_AE_ENABLE = KAL_TRUE;

MSDK_SENSOR_CONFIG_STRUCT OV5645MIPISensorConfigData;
#define OV5645_TEST_PATTERN_CHECKSUM (0x7ba87eae)
kal_bool run_test_potten;

static BOOL OV5645MIPI_set_param_wb(UINT16 para);
static UINT32 line_coordinate[AE_VERTICAL_BLOCKS_MAX] = { 0 };

/* line[0]=0  line[1]=160  line[2]=320 line[3]=480 line[4]=640 */
static UINT32 row_coordinate[AE_HORIZONTAL_BLOCKS_MAX] = { 0 };

/* line[0]=0  line[1]=120  line[2]=240 line[3]=360 line[4]=480 */
static BOOL AE_1_ARRAY[AE_SECTION_INDEX_MAX] = { FALSE };

static BOOL AE_2_ARRAY[AE_HORIZONTAL_BLOCKS][AE_VERTICAL_BLOCKS] = {
	{FALSE}, {FALSE}, {FALSE}, {FALSE}
};				/* how to .... */

/* =====================touch AE begin==========================// */
void writeAEReg(void)
{
	UINT8 temp;
	/* write 1280X960 */
	OV5645MIPI_write_cmos_sensor(0x501d, 0x10);
	OV5645MIPI_write_cmos_sensor(0x5680, 0x00);
	OV5645MIPI_write_cmos_sensor(0x5681, 0x00);
	OV5645MIPI_write_cmos_sensor(0x5682, 0x00);
	OV5645MIPI_write_cmos_sensor(0x5683, 0x00);
	OV5645MIPI_write_cmos_sensor(0x5684, 0x05);	/* width=256 */
	OV5645MIPI_write_cmos_sensor(0x5685, 0x00);
	OV5645MIPI_write_cmos_sensor(0x5686, 0x03);	/* heght=256 */
	OV5645MIPI_write_cmos_sensor(0x5687, 0xc0);
	temp = 0x11;
	if (AE_1_ARRAY[AE_SECTION_INDEX_1] == TRUE)
		temp = temp | 0x0F;
	if (AE_1_ARRAY[AE_SECTION_INDEX_2] == TRUE)
		temp = temp | 0xF0;
	/* write 0x5688 */
	OV5645MIPI_write_cmos_sensor(0x5688, temp);

	temp = 0x11;
	if (AE_1_ARRAY[AE_SECTION_INDEX_3] == TRUE)
		temp = temp | 0x0F;
	if (AE_1_ARRAY[AE_SECTION_INDEX_4] == TRUE)
		temp = temp | 0xF0;
	/* write 0x5689 */
	OV5645MIPI_write_cmos_sensor(0x5689, temp);

	temp = 0x11;
	if (AE_1_ARRAY[AE_SECTION_INDEX_5] == TRUE)
		temp = temp | 0x0F;
	if (AE_1_ARRAY[AE_SECTION_INDEX_6] == TRUE)
		temp = temp | 0xF0;
	/* write 0x568A */
	OV5645MIPI_write_cmos_sensor(0x568A, temp);

	temp = 0x11;
	if (AE_1_ARRAY[AE_SECTION_INDEX_7] == TRUE)
		temp = temp | 0x0F;
	if (AE_1_ARRAY[AE_SECTION_INDEX_8] == TRUE)
		temp = temp | 0xF0;
	/* write 0x568B */
	OV5645MIPI_write_cmos_sensor(0x568B, temp);

	temp = 0x11;
	if (AE_1_ARRAY[AE_SECTION_INDEX_9] == TRUE)
		temp = temp | 0x0F;
	if (AE_1_ARRAY[AE_SECTION_INDEX_10] == TRUE)
		temp = temp | 0xF0;
	/* write 0x568C */
	OV5645MIPI_write_cmos_sensor(0x568C, temp);

	temp = 0x11;
	if (AE_1_ARRAY[AE_SECTION_INDEX_11] == TRUE)
		temp = temp | 0x0F;
	if (AE_1_ARRAY[AE_SECTION_INDEX_12] == TRUE)
		temp = temp | 0xF0;
	/* write 0x568D */
	OV5645MIPI_write_cmos_sensor(0x568D, temp);

	temp = 0x11;
	if (AE_1_ARRAY[AE_SECTION_INDEX_13] == TRUE)
		temp = temp | 0x0F;
	if (AE_1_ARRAY[AE_SECTION_INDEX_14] == TRUE)
		temp = temp | 0xF0;
	/* write 0x568E */
	OV5645MIPI_write_cmos_sensor(0x568E, temp);

	temp = 0x11;
	if (AE_1_ARRAY[AE_SECTION_INDEX_15] == TRUE)
		temp = temp | 0x0F;
	if (AE_1_ARRAY[AE_SECTION_INDEX_16] == TRUE)
		temp = temp | 0xF0;
	/* write 0x568F */
	OV5645MIPI_write_cmos_sensor(0x568F, temp);
}


void printAE_1_ARRAY(void)
{
	UINT32 i;

	for (i = 0; i < AE_SECTION_INDEX_MAX; i++)
		LOG_INF("AE_1_ARRAY[%2d]=%d\n", i, AE_1_ARRAY[i]);
}

void printAE_2_ARRAY(void)
{
	UINT32 i, j;

	LOG_INF("\t\t");
	for (i = 0; i < AE_VERTICAL_BLOCKS; i++)
		LOG_INF("      line[%2d]", i);
	LOG_INF("\n");
	for (j = 0; j < AE_HORIZONTAL_BLOCKS; j++) {
		LOG_INF("\trow[%2d]", j);
		for (i = 0; i < AE_VERTICAL_BLOCKS; i++)
			// SENSORDB("AE_2_ARRAY[%2d][%2d]=%d\n",
			// j,i,AE_2_ARRAY[j][i]);
			LOG_INF("  %7d", AE_2_ARRAY[j][i]);
		LOG_INF("\n");
	}
}

void clearAE_2_ARRAY(void)
{
	UINT32 i, j;

	for (j = 0; j < AE_HORIZONTAL_BLOCKS; j++) {
		for (i = 0; i < AE_VERTICAL_BLOCKS; i++)
			AE_2_ARRAY[j][i] = FALSE;
	}
}

void mapAE_2_ARRAY_To_AE_1_ARRAY(void)
{
	UINT32 i, j;

	for (j = 0; j < AE_HORIZONTAL_BLOCKS; j++) {
		for (i = 0; i < AE_VERTICAL_BLOCKS; i++)
			AE_1_ARRAY[j * AE_VERTICAL_BLOCKS + i] =
			AE_2_ARRAY[j][i];
	}
}

void mapMiddlewaresizePointToPreviewsizePoint(UINT32 mx,
	UINT32 my, UINT32 mw, UINT32 mh,
	UINT32 *pvx, UINT32 *pvy, UINT32 pvw, UINT32 pvh)
{
	*pvx = pvw * mx / mw;
	*pvy = pvh * my / mh;
	LOG_INF("mapping middlware x[%d],y[%d],
	[%d X %d]\n\t\tto x[%d], \y[%d],[%d X %d]\n ",
	mx, my, mw, mh, *pvx, *pvy, pvw, pvh);
}


void calcLine(void)
{				/* line[5] */
	UINT32 i;
	UINT32 step = PRV_W / AE_VERTICAL_BLOCKS;

	for (i = 0; i <= AE_VERTICAL_BLOCKS; i++) {
		*(&line_coordinate[0] + i) = step * i;
		LOG_INF("line[%d]=%d\t", i, *(&line_coordinate[0] + i));
	}
	LOG_INF("\n");
}

void calcRow(void)
{				/* row[5] */
	UINT32 i;
	UINT32 step = PRV_H / AE_HORIZONTAL_BLOCKS;

	for (i = 0; i <= AE_HORIZONTAL_BLOCKS; i++) {
		*(&row_coordinate[0] + i) = step * i;
		LOG_INF("row[%d]=%d\t", i, *(&row_coordinate[0] + i));
	}
	LOG_INF("\n");
}

void calcPointsAELineRowCoordinate(UINT32 x, UINT32 y,
	UINT32 *linenum, UINT32 *rownum)
{
	UINT32 i;

	i = 1;
	while (i <= AE_VERTICAL_BLOCKS) {
		if (x < line_coordinate[i]) {
			*linenum = i;
			break;
		}
		*linenum = i++;
	}

	i = 1;
	while (i <= AE_HORIZONTAL_BLOCKS) {
		if (y < row_coordinate[i]) {
			*rownum = i;
			break;
		}
		*rownum = i++;
	}
	LOG_INF("PV point [%d, %d] to section line coordinate[%d] row[%d]\n",
		x, y, *linenum, *rownum);
}



MINT32 clampSection(UINT32 x, UINT32 min, UINT32 max)
{
	if (x > max)
		return max;
	if (x < min)
		return min;
	return x;
}

void mapCoordinate(UINT32 linenum, UINT32 rownum,
	UINT32 *sectionlinenum, UINT32 *sectionrownum)
{
	*sectionlinenum = clampSection(linenum - 1, 0, AE_VERTICAL_BLOCKS - 1);
	*sectionrownum = clampSection(rownum - 1, 0, AE_HORIZONTAL_BLOCKS - 1);
	LOG_INF("%s from[%d][%d] to[%d][%d]\n", __func__,
	linenum, rownum, *sectionlinenum, *sectionrownum);
}

void mapRectToAE_2_ARRAY(UINT32 x0, UINT32 y0, UINT32 x1, UINT32 y1)
{
	UINT32 i, j;

	LOG_INF("([%d][%d]),([%d][%d])\n", x0, y0, x1, y1);
	clearAE_2_ARRAY();
	x0 = clampSection(x0, 0, AE_VERTICAL_BLOCKS - 1);
	y0 = clampSection(y0, 0, AE_HORIZONTAL_BLOCKS - 1);
	x1 = clampSection(x1, 0, AE_VERTICAL_BLOCKS - 1);
	y1 = clampSection(y1, 0, AE_HORIZONTAL_BLOCKS - 1);

	for (j = y0; j <= y1; j++) {
		for (i = x0; i <= x1; i++)
			AE_2_ARRAY[j][i] = TRUE;
	}
}

void resetPVAE_2_ARRAY(void)
{
	mapRectToAE_2_ARRAY(1, 1, 2, 2);
}

/* update ae window */
/* @input zone[] addr */
void OV5645_FOCUS_Set_AE_Window(UINT32 zone_addr)
{
	UINT32 FD_XS;
	UINT32 FD_YS;
	UINT32 x0, y0, x1, y1;
	UINT32 pvx0, pvy0, pvx1, pvy1;
	UINT32 linenum, rownum;
	UINT32 rightbottomlinenum, rightbottomrownum;
	UINT32 leftuplinenum, leftuprownum;
	UINT32 *zone = (UINT32 *) zone_addr;

	x0 = *zone;
	y0 = *(zone + 1);
	x1 = *(zone + 2);
	y1 = *(zone + 3);
	FD_XS = *(zone + 4);
	FD_YS = *(zone + 5);

	LOG_INF("AE x0=%d,y0=%d,x1=%d,y1=%d,FD_XS=%d,FD_YS=%d\n",
			   x0, y0, x1, y1, FD_XS, FD_YS);

	/* print_sensor_ae_section(); */
	/* print_AE_section(); */

	/* 1.transfer points to preview size */
	/* UINT32 pvx0, pvy0, pvx1, pvy1; */
	mapMiddlewaresizePointToPreviewsizePoint(x0, y0, FD_XS, FD_YS,
	&pvx0, &pvy0, PRV_W, PRV_H);
	mapMiddlewaresizePointToPreviewsizePoint(x1, y1, FD_XS, FD_YS,
	&pvx1, &pvy1, PRV_W, PRV_H);

	/* 2.sensor AE line and row coordinate */
	calcLine();
	calcRow();

	/* 3.calc left up point to section */
	/* UINT32 linenum, rownum; */
	calcPointsAELineRowCoordinate(pvx0, pvy0, &linenum, &rownum);
	/* UINT32 leftuplinenum,leftuprownum; */
	mapCoordinate(linenum, rownum, &leftuplinenum, &leftuprownum);
	// SENSORDB("leftuplinenum=%d,leftuprownum=%d\n",
	// leftuplinenum,leftuprownum);

	/* 4.calc right bottom point to section */
	calcPointsAELineRowCoordinate(pvx1, pvy1, &linenum, &rownum);
	/* UINT32 rightbottomlinenum,rightbottomrownum; */
	mapCoordinate(linenum, rownum, &rightbottomlinenum, &rightbottomrownum);
	// SENSORDB("rightbottomlinenum=%d,rightbottomrownum=%d\n",
	//  rightbottomlinenum,rightbottomrownum);

	/* 5.update global section array */
	mapRectToAE_2_ARRAY(leftuplinenum, leftuprownum,
			rightbottomlinenum, rightbottomrownum);
	/* print_AE_section(); */

	/* 6.write to reg */
	mapAE_2_ARRAY_To_AE_1_ARRAY();
	/* printAE_1_ARRAY(); */
	printAE_2_ARRAY();
	writeAEReg();
	LOG_INF("exit %s function\n", __func__);
}

/* =====================touch AE end==========================// */
/*************************************************************************
 * FUNCTION
 *	OV5645MIPI_set_dummy
 *
 * DESCRIPTION
 *	This function set the dummy pixels(Horizontal Blanking) &
 *			dummy lines(Vertical Blanking), it can be
 *	used to adjust the frame rate or gain more time for back-end process.
 *
 *	IMPORTANT NOTICE: the base shutter need re-calculate for some sensor,
 *				or else flicker may occur.
 *
 * PARAMETERS
 *	1. kal_uint32 : Dummy Pixels (Horizontal Blanking)
 *	2. kal_uint32 : Dummy Lines (Vertical Blanking)
 *
 * RETURNS
 *	None
 *
 *************************************************************************/
static void OV5645MIPIinitalvariable(void)
{
	spin_lock(&ov5645mipi_drv_lock);
	OV5645MIPISensor.VideoMode = KAL_FALSE;
	OV5645MIPISensor.NightMode = KAL_FALSE;
	OV5645MIPISensor.Fps = 100;
	OV5645MIPISensor.ShutterStep = 0xde;
	OV5645MIPISensor.CaptureDummyPixels = 0;
	OV5645MIPISensor.CaptureDummyLines = 0;
	OV5645MIPISensor.PreviewDummyPixels = 0;
	OV5645MIPISensor.PreviewDummyLines = 0;
	OV5645MIPISensor.SensorMode = SENSOR_MODE_INIT;
	OV5645MIPISensor.IsPVmode = KAL_TRUE;
	OV5645MIPISensor.PreviewPclk = 560;
	OV5645MIPISensor.CapturePclk = 840;
	OV5645MIPISensor.PreviewShutter = 0x0375;	/* 0375 */
	OV5645MIPISensor.PreviewExtraShutter = 0x00;
	OV5645MIPISensor.SensorGain = 0x10;

	OV5645CurrentStatus.iNightMode = 0xFFFF;
	OV5645CurrentStatus.iWB = AWB_MODE_AUTO;
	OV5645CurrentStatus.iEffect = MEFFECT_OFF;
	OV5645CurrentStatus.iBanding = AE_FLICKER_MODE_50HZ;
	OV5645CurrentStatus.iEV = AE_EV_COMP_00;
	OV5645CurrentStatus.iMirror = IMAGE_NORMAL;
	OV5645CurrentStatus.iFrameRate = 0;	/* No Fix FrameRate */

	spin_unlock(&ov5645mipi_drv_lock);
}

static void OV5645MIPISetDummy(kal_uint32 dummy_pixels,
					kal_uint32 dummy_lines)
{
	LOG_INF("enter %s function:\n ", __func__);
	if (OV5645MIPISensor.IsPVmode) {
		dummy_pixels = dummy_pixels + OV5645MIPI_PV_PERIOD_PIXEL_NUMS;
		OV5645MIPI_write_cmos_sensor(0x380D, (dummy_pixels & 0xFF));
		OV5645MIPI_write_cmos_sensor(0x380C,
		((dummy_pixels & 0xFF00) >> 8));

		dummy_lines = dummy_lines + OV5645MIPI_PV_PERIOD_LINE_NUMS;
		OV5645MIPI_write_cmos_sensor(0x380F, (dummy_lines & 0xFF));
		OV5645MIPI_write_cmos_sensor(0x380E,
		((dummy_lines & 0xFF00) >> 8));
	} else {
		dummy_pixels = dummy_pixels + OV5645MIPI_FULL_PERIOD_PIXEL_NUMS;
		OV5645MIPI_write_cmos_sensor(0x380D, (dummy_pixels & 0xFF));
		OV5645MIPI_write_cmos_sensor(0x380C,
		((dummy_pixels & 0xFF00) >> 8));

		dummy_lines = dummy_lines + OV5645MIPI_FULL_PERIOD_LINE_NUMS;
		OV5645MIPI_write_cmos_sensor(0x380F, (dummy_lines & 0xFF));
		OV5645MIPI_write_cmos_sensor(0x380E,
		((dummy_lines & 0xFF00) >> 8));
	}
	LOG_INF("exit %s function:\n ", __func__);
}				/* OV5645MIPI_set_dummy */

/*************************************************************************
 * FUNCTION
 *	OV5645MIPIWriteShutter
 *
 * DESCRIPTION
 *	This function used to write the shutter.
 *
 * PARAMETERS
 *	1. kal_uint32 : The shutter want to apply to sensor.
 *
 * RETURNS
 *	None
 *
 *************************************************************************/
static void OV5645MIPIWriteShutter(kal_uint32 shutter)
{
	kal_uint32 extra_exposure_lines = 0;

	LOG_INF("enter %s function:\n ", __func__);
	if (shutter < 1)
		shutter = 1;

	if (OV5645MIPISensor.IsPVmode) {
		if (shutter <= OV5645MIPI_PV_EXPOSURE_LIMITATION)
			extra_exposure_lines = 0;
		else
			extra_exposure_lines =
		shutter - OV5645MIPI_PV_EXPOSURE_LIMITATION;

	} else {
		if (shutter <= OV5645MIPI_FULL_EXPOSURE_LIMITATION)
			extra_exposure_lines = 0;
		else
			extra_exposure_lines =
		shutter - OV5645MIPI_FULL_EXPOSURE_LIMITATION;

	}
	/* AEC PK EXPOSURE */
	shutter *= 16;
	/* AEC[7:0] */
	OV5645MIPI_write_cmos_sensor(0x3502, (shutter & 0x00FF));
	/* AEC[15:8] */
	OV5645MIPI_write_cmos_sensor(0x3501, ((shutter & 0x0FF00) >> 8));
	OV5645MIPI_write_cmos_sensor(0x3500, ((shutter & 0xFF0000) >> 16));
	/* set extra exposure line [aec add vts] */
	/* EXVTS[b7~b0] */
	OV5645MIPI_write_cmos_sensor(0x350D, extra_exposure_lines & 0xFF);
	/* EXVTS[b15~b8] */
	OV5645MIPI_write_cmos_sensor(0x350C,
	(extra_exposure_lines & 0xFF00) >> 8);
	LOG_INF("exit %s function:\n ", __func__);
}				/* OV5645MIPI_write_shutter */

/*************************************************************************
 * FUNCTION
 *	OV5645MIPIExpWriteShutter
 *
 * DESCRIPTION
 *	This function used to write the shutter.
 *
 * PARAMETERS
 *	1. kal_uint32 : The shutter want to apply to sensor.
 *
 * RETURNS
 *	None
 *
 *************************************************************************/
static void OV5645MIPIWriteExpShutter(kal_uint32 shutter)
{
	shutter *= 16;
	LOG_INF("enter %s function:\n ", __func__);
	OV5645MIPI_write_cmos_sensor(0x3502, (shutter & 0x00FF));
	/* AEC[7:0] */
	OV5645MIPI_write_cmos_sensor(0x3501, ((shutter & 0x0FF00) >> 8));
	/* AEC[15:8] */
	OV5645MIPI_write_cmos_sensor(0x3500, ((shutter & 0xFF0000) >> 16));
	LOG_INF("exit %s function:\n ", __func__);
}				/* OV5645MIPI_write_shutter */

/*************************************************************************
 * FUNCTION
 *	OV5645MIPIExtraWriteShutter
 *
 * DESCRIPTION
 *	This function used to write the shutter.
 *
 * PARAMETERS
 *	1. kal_uint32 : The shutter want to apply to sensor.
 *
 * RETURNS
 *	None
 *
 *************************************************************************/
static void OV5645MIPIWriteExtraShutter(kal_uint32 shutter)
{
	LOG_INF("enter %s function:\n ", __func__);
	/* EXVTS[b7~b0] */
	OV5645MIPI_write_cmos_sensor(0x350D, shutter & 0xFF);
	/* EXVTS[b15~b8] */
	OV5645MIPI_write_cmos_sensor(0x350C, (shutter & 0xFF00) >> 8);
	LOG_INF("exit %s function:\n ", __func__);
}				/* OV5645MIPI_write_shutter */

/*************************************************************************
 * FUNCTION
 *	OV5645MIPIWriteSensorGain
 *
 * DESCRIPTION
 *	This function used to write the sensor gain.
 *
 * PARAMETERS
 *	1. kal_uint32 : The sensor gain want to apply to sensor.
 *
 * RETURNS
 *	None
 *
 *************************************************************************/

/*************************************************************************
 * FUNCTION
 *	OV5645MIPIReadShutter
 *
 * DESCRIPTION
 *	This function read current shutter for calculate the exposure.
 *
 * PARAMETERS
 *	None
 *
 * RETURNS
 *	kal_uint16 : The current shutter value.
 *
 *************************************************************************/
static kal_uint32 OV5645MIPIReadShutter(void)
{
	kal_uint16 temp_reg1, temp_reg2, temp_reg3;

	LOG_INF("enter %s function:\n ", __func__);
	/* AEC[b19~b16] */
	temp_reg1 = OV5645MIPIYUV_read_cmos_sensor(0x3500);
	/* AEC[b15~b8] */
	temp_reg2 = OV5645MIPIYUV_read_cmos_sensor(0x3501);
	/* AEC[b7~b0] */
	temp_reg3 = OV5645MIPIYUV_read_cmos_sensor(0x3502);

	spin_lock(&ov5645mipi_drv_lock);
	OV5645MIPISensor.PreviewShutter = (temp_reg1 << 12) |
				(temp_reg2 << 4) | (temp_reg3 >> 4);
	spin_unlock(&ov5645mipi_drv_lock);
	LOG_INF("exit %s function:\n ", __func__);
	return OV5645MIPISensor.PreviewShutter;
}				/* OV5645MIPI_read_shutter */

/*************************************************************************
 * FUNCTION
 *	OV5645MIPIReadExtraShutter
 *
 * DESCRIPTION
 *	This function read current shutter for calculate the exposure.
 *
 * PARAMETERS
 *	None
 *
 * RETURNS
 *	kal_uint16 : The current shutter value.
 *
 *************************************************************************/
static kal_uint32 OV5645MIPIReadExtraShutter(void)
{
	kal_uint16 temp_reg1, temp_reg2;

	LOG_INF("enter %s function:\n ", __func__);
	/* AEC[b15~b8] */
	temp_reg1 = OV5645MIPIYUV_read_cmos_sensor(0x350c);
	/* AEC[b7~b0] */
	temp_reg2 = OV5645MIPIYUV_read_cmos_sensor(0x350d);
	spin_lock(&ov5645mipi_drv_lock);
	OV5645MIPISensor.PreviewExtraShutter = ((temp_reg1 << 8) | temp_reg2);
	spin_unlock(&ov5645mipi_drv_lock);
	LOG_INF("exit %s function:\n ", __func__);
	return OV5645MIPISensor.PreviewExtraShutter;
}				/* OV5645MIPI_read_shutter */

/*************************************************************************
 * FUNCTION
 *	OV5645MIPIReadSensorGain
 *
 * DESCRIPTION
 *	This function read current sensor gain for calculate the exposure.
 *
 * PARAMETERS
 *	None
 *
 * RETURNS
 *	kal_uint16 : The current sensor gain value.
 *
 *************************************************************************/
static kal_uint32 OV5645MIPIReadSensorGain(void)
{
	kal_uint32 sensor_gain = 0;

	LOG_INF("enter %s function:\n ", __func__);
	sensor_gain = (OV5645MIPIYUV_read_cmos_sensor(0x350B) & 0xFF);
	LOG_INF("exit %s function:\n ", __func__);
	return sensor_gain;
}				/* OV5645MIPIReadSensorGain */

/*************************************************************************
 * FUNCTION
 *	OV5645MIPI_set_AE_mode
 *
 * DESCRIPTION
 *	This function OV5645MIPI_set_AE_mode.
 *
 * PARAMETERS
 *	none
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static void OV5645MIPI_set_AE_mode(kal_bool AE_enable)
{
	kal_uint8 AeTemp;

	LOG_INF("enter %s function:\n ", __func__);
	AeTemp = OV5645MIPIYUV_read_cmos_sensor(0x3503);
	if (AE_enable == KAL_TRUE) {
		/* turn on AEC/AGC */
		OV5645MIPI_write_cmos_sensor(0x3503, (AeTemp & (~0x07)));
	} else {
		/* turn off AEC/AGC */
		OV5645MIPI_write_cmos_sensor(0x3503, (AeTemp | 0x07));
	}
	LOG_INF("exit %s function:\n ", __func__);
}

/*************************************************************************
 * FUNCTION
 *	OV5645MIPI_set_AWB_mode
 *
 * DESCRIPTION
 *	This function OV5645MIPI_set_AWB_mode.
 *
 * PARAMETERS
 *	none
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static void OV5645MIPI_set_AWB_mode(kal_bool AWB_enable)
{
	kal_uint8 AwbTemp;

	LOG_INF("enter %s function:\n ", __func__);
	AwbTemp = OV5645MIPIYUV_read_cmos_sensor(0x3406);

	if (AWB_enable == KAL_TRUE)
		OV5645MIPI_write_cmos_sensor(0x3406, AwbTemp & 0xFE);
	else
		OV5645MIPI_write_cmos_sensor(0x3406, AwbTemp | 0x01);

	LOG_INF("exit %s function:\n ", __func__);
}


/*************************************************************************
 * FUNCTION
 *	OV5645MIPI_night_mode
 *
 * DESCRIPTION
 *	This function night mode of OV5645MIPI.
 *
 * PARAMETERS
 *	none
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
void OV5645MIPI_night_mode(kal_bool enable)
{
	kal_uint16 night = OV5645MIPIYUV_read_cmos_sensor(0x3A00);

	if (enable) {
		/* camera night mode */
		/* 30fps-5fps */
		OV5645MIPI_write_cmos_sensor(0x3A00, night | 0x04);
		OV5645MIPI_write_cmos_sensor(0x3a02, 0x11);
		OV5645MIPI_write_cmos_sensor(0x3a03, 0x4c);
		OV5645MIPI_write_cmos_sensor(0x3a14, 0x10);
		OV5645MIPI_write_cmos_sensor(0x3a15, 0x22);
		OV5645MIPI_write_cmos_sensor(0x3a19, 0xc8);
	} else {		/* camera normal mode */
		/* 30fps-10fps */
		OV5645MIPI_write_cmos_sensor(0x3A00, night | 0x04);
		OV5645MIPI_write_cmos_sensor(0x3a02, 0x09);
		OV5645MIPI_write_cmos_sensor(0x3a03, 0x9c);
		OV5645MIPI_write_cmos_sensor(0x3a14, 0x09);
		OV5645MIPI_write_cmos_sensor(0x3a15, 0x38);
		OV5645MIPI_write_cmos_sensor(0x3a19, 0xc8);
	}
	spin_lock(&ov5645mipi_drv_lock);
	OV5645MIPISensor.NightMode = enable;
	spin_unlock(&ov5645mipi_drv_lock);
	LOG_INF("exit %s function:\n ", __func__);
}				/* OV5645MIPI_night_mode */

/*************************************************************************
 * FUNCTION
 *	OV5645MIPI_GetSensorID
 *
 * DESCRIPTION
 *	This function get the sensor ID
 *
 * PARAMETERS
 *	None
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 OV5645MIPI_GetSensorID(kal_uint32 *sensorID)
{
	signed char i;

	LOG_INF("enter %s function:\n ", __func__);

	OV5645MIPI_write_cmos_sensor(0x3008, 0x82);	/* Reset sensor */
	mDELAY(10);
	for (i = 0; i < 3; i++) {
		*sensorID =
		    (OV5645MIPIYUV_read_cmos_sensor(0x300A) << 8) |
		    OV5645MIPIYUV_read_cmos_sensor(0x300B);
		if (*sensorID != OV5645MIPI_SENSOR_ID) {
			LOG_INF("Read Sensor ID Fail = 0x%04x\n", *sensorID);
			*sensorID = 0xFFFFFFFF;
			return ERROR_SENSOR_CONNECT_FAIL;
		}
	}
	LOG_INF("exit %s function:\n", __func__);
	return ERROR_NONE;
}

/*************************************************************************
 * FUNCTION
 *    OV5645MIPIInitialSetting
 *
 * DESCRIPTION
 *    This function initialize the registers of CMOS sensor.
 *
 * PARAMETERS
 *    None
 *
 * RETURNS
 *    None
 *
 * LOCAL AFFECTED
 *
 *************************************************************************/
static void OV5645MIPIInitialSetting(void)
{
	/* ;OV5645MIPI 1280x960,30fps */
	/* 56Mhz, 224Mbps/Lane, 2 Lane */
	LOG_INF("enter %s function:\n ", __func__);
	/* ; PLL clock selection */
	OV5645MIPI_write_cmos_sensor(0x3103, 0x11);
	/* ; software reset */
	OV5645MIPI_write_cmos_sensor(0x3008, 0x82);
	mDELAY(20);		/* ; delay 2ms */
	/* ; software power down */
	OV5645MIPI_write_cmos_sensor(0x3008, 0x42);
	/* ; clock from PLL */
	OV5645MIPI_write_cmos_sensor(0x3103, 0x03);
	/* ; AGC manual, AEC manual */
	OV5645MIPI_write_cmos_sensor(0x3503, 0x07);
	OV5645MIPI_write_cmos_sensor(0x3000, 0x30);
	OV5645MIPI_write_cmos_sensor(0x3004, 0xef);
	/* ; system reset */
	OV5645MIPI_write_cmos_sensor(0x3002, 0x1c);
	/* ; clock enable */
	OV5645MIPI_write_cmos_sensor(0x3006, 0xc3);
	/* ; MIPI 2 lane */
	OV5645MIPI_write_cmos_sensor(0x300e, 0x45);
	/* ; Frex, CSK input, Vsync output */
	OV5645MIPI_write_cmos_sensor(0x3017, 0x40);
	/* ; GPIO input */
	OV5645MIPI_write_cmos_sensor(0x3018, 0x00);
	/* ; GPIO input */
	OV5645MIPI_write_cmos_sensor(0x302c, 0x42);
	OV5645MIPI_write_cmos_sensor(0x302e, 0x0b);	/*  */
	OV5645MIPI_write_cmos_sensor(0x3031, 0x00);	/*  */
	OV5645MIPI_write_cmos_sensor(0x3611, 0x06);	/*  */
	OV5645MIPI_write_cmos_sensor(0x3612, 0xab);	/*  */
	OV5645MIPI_write_cmos_sensor(0x3614, 0x50);	/*  */
	OV5645MIPI_write_cmos_sensor(0x3618, 0x00);	/*  */
	/* ; PLL, MIPI 8-bit mode */
	OV5645MIPI_write_cmos_sensor(0x3034, 0x18);
	OV5645MIPI_write_cmos_sensor(0x3035, 0x21);	/* ; PLL */
	OV5645MIPI_write_cmos_sensor(0x3036, 0x70);	/* ; PLL */
	OV5645MIPI_write_cmos_sensor(0x3037, 0x13);	/* ; PLL */
	OV5645MIPI_write_cmos_sensor(0x3108, 0x01);	/* ; PLL */
	OV5645MIPI_write_cmos_sensor(0x3824, 0x01);	/* ; PLL */
	OV5645MIPI_write_cmos_sensor(0x460c, 0x20);	/* ; PLL */
	/* ; exposure = 0x100 */
	OV5645MIPI_write_cmos_sensor(0x3500, 0x00);
	/* ; exposure */
	OV5645MIPI_write_cmos_sensor(0x3501, 0x01);
	/* ; exposure */
	OV5645MIPI_write_cmos_sensor(0x3502, 0x00);
	/* ; gain = 0x3f */
	OV5645MIPI_write_cmos_sensor(0x350a, 0x00);
	/* ; gain */
	OV5645MIPI_write_cmos_sensor(0x350b, 0x3f);
	OV5645MIPI_write_cmos_sensor(0x3600, 0x09);	/*  */
	OV5645MIPI_write_cmos_sensor(0x3601, 0x43);	/*  */
	OV5645MIPI_write_cmos_sensor(0x3620, 0x33);	/*  */
	OV5645MIPI_write_cmos_sensor(0x3621, 0xe0);	/*  */
	OV5645MIPI_write_cmos_sensor(0x3622, 0x01);	/*  */
	OV5645MIPI_write_cmos_sensor(0x3630, 0x2d);	/*  */
	OV5645MIPI_write_cmos_sensor(0x3631, 0x00);	/*  */
	OV5645MIPI_write_cmos_sensor(0x3632, 0x32);	/*  */
	OV5645MIPI_write_cmos_sensor(0x3633, 0x52);	/*  */
	OV5645MIPI_write_cmos_sensor(0x3634, 0x70);	/*  */
	OV5645MIPI_write_cmos_sensor(0x3635, 0x13);	/*  */
	OV5645MIPI_write_cmos_sensor(0x3636, 0x03);	/*  */
	OV5645MIPI_write_cmos_sensor(0x3702, 0x6e);	/*  */
	OV5645MIPI_write_cmos_sensor(0x3703, 0x52);	/*  */
	OV5645MIPI_write_cmos_sensor(0x3704, 0xa0);	/*  */
	OV5645MIPI_write_cmos_sensor(0x3705, 0x33);	/*  */
	OV5645MIPI_write_cmos_sensor(0x3708, 0x66);	/*  */
	OV5645MIPI_write_cmos_sensor(0x3709, 0x12);	/*  */
	OV5645MIPI_write_cmos_sensor(0x370b, 0x61);	/*  */
	OV5645MIPI_write_cmos_sensor(0x370c, 0xc3);	/*  */
	OV5645MIPI_write_cmos_sensor(0x370f, 0x10);	/*  */
	OV5645MIPI_write_cmos_sensor(0x3715, 0x08);	/*  */
	OV5645MIPI_write_cmos_sensor(0x3717, 0x01);	/*  */
	OV5645MIPI_write_cmos_sensor(0x371b, 0x20);	/*  */
	OV5645MIPI_write_cmos_sensor(0x3731, 0x22);	/*  */
	OV5645MIPI_write_cmos_sensor(0x3739, 0x70);	/*  */
	OV5645MIPI_write_cmos_sensor(0x3901, 0x0a);	/*  */
	OV5645MIPI_write_cmos_sensor(0x3905, 0x02);	/*  */
	OV5645MIPI_write_cmos_sensor(0x3906, 0x10);	/*  */
	OV5645MIPI_write_cmos_sensor(0x3719, 0x86);	/*  */
	OV5645MIPI_write_cmos_sensor(0x3800, 0x00);	/* ; HS = 0 */
	OV5645MIPI_write_cmos_sensor(0x3801, 0x00);	/* ; HS */
	OV5645MIPI_write_cmos_sensor(0x3802, 0x00);	/* ; VS = 250 */
	OV5645MIPI_write_cmos_sensor(0x3803, 0x06);	/* ; VS */
	OV5645MIPI_write_cmos_sensor(0x3804, 0x0a);	/* ; HW = 2623 */
	OV5645MIPI_write_cmos_sensor(0x3805, 0x3f);	/* ; HW */
	OV5645MIPI_write_cmos_sensor(0x3806, 0x07);	/* ; VH = 1705 */
	OV5645MIPI_write_cmos_sensor(0x3807, 0x9d);	/* ; VH */
	OV5645MIPI_write_cmos_sensor(0x3808, 0x05);	/* ; DVPHO = 1280 */
	OV5645MIPI_write_cmos_sensor(0x3809, 0x00);	/* ; DVPHO */
	OV5645MIPI_write_cmos_sensor(0x380a, 0x03);	/* ; DVPHO */
	OV5645MIPI_write_cmos_sensor(0x380b, 0xc0);	/* ; DVPVO */
	OV5645MIPI_write_cmos_sensor(0x380c, 0x07);	/* ; HTS = 2160 */
	OV5645MIPI_write_cmos_sensor(0x380d, 0x68);	/* ; HTS */
	OV5645MIPI_write_cmos_sensor(0x380e, 0x03);	/* ; VTS = 740 */
	OV5645MIPI_write_cmos_sensor(0x380f, 0xd8);	/* ; VTS */
	OV5645MIPI_write_cmos_sensor(0x3810, 0x00);	/* ; H OFF = 16 */
	OV5645MIPI_write_cmos_sensor(0x3811, 0x10);	/* ; H OFF */
	OV5645MIPI_write_cmos_sensor(0x3812, 0x00);	/* ; V OFF = 4 */
	OV5645MIPI_write_cmos_sensor(0x3813, 0x06);	/* ; V OFF */
	OV5645MIPI_write_cmos_sensor(0x3814, 0x31);	/* ; X INC */
	OV5645MIPI_write_cmos_sensor(0x3815, 0x31);	/* ; Y INC */
	/* ; flip off, V bin on */
	OV5645MIPI_write_cmos_sensor(0x3820, 0x41);
	/* ; mirror on, H bin on */
	OV5645MIPI_write_cmos_sensor(0x3821, 0x07);
	OV5645MIPI_write_cmos_sensor(0x3826, 0x03);	/*  */
	OV5645MIPI_write_cmos_sensor(0x3828, 0x08);	/*  */
	OV5645MIPI_write_cmos_sensor(0x3a02, 0x03);	/* ; max exp 60 = 740 */
	OV5645MIPI_write_cmos_sensor(0x3a03, 0xd8);	/* ; max exp 60 */
	OV5645MIPI_write_cmos_sensor(0x3a08, 0x01);	/* ; B50 = 222 */
	OV5645MIPI_write_cmos_sensor(0x3a09, 0x27);	/* ; B50 */
	OV5645MIPI_write_cmos_sensor(0x3a0a, 0x00);	/* ; B60 = 185 */
	OV5645MIPI_write_cmos_sensor(0x3a0b, 0xf6);	/* ; B60 */
	OV5645MIPI_write_cmos_sensor(0x3a0e, 0x03);	/* ; max 50 */
	OV5645MIPI_write_cmos_sensor(0x3a0d, 0x04);	/* ; max 60 */
	OV5645MIPI_write_cmos_sensor(0x3a14, 0x03);	/* ; max exp 50 = 740 */
	OV5645MIPI_write_cmos_sensor(0x3a15, 0xd8);	/* ; max exp 50 */
	/* ; gain ceiling = 15.5x */
	OV5645MIPI_write_cmos_sensor(0x3a18, 0x00);
	/* ; gain ceiling 0x60 */
	OV5645MIPI_write_cmos_sensor(0x3a19, 0xf8);
	/* ; enable band insert, ken, */
	OV5645MIPI_write_cmos_sensor(0x3a05, 0x30);
	/* ;manual banding mode */
	OV5645MIPI_write_cmos_sensor(0x3c01, 0xb4);
	/* ;50 Banding mode */
	OV5645MIPI_write_cmos_sensor(0x3c00, 0x04);
	OV5645MIPI_write_cmos_sensor(0x3c04, 0x28);	/*  */
	OV5645MIPI_write_cmos_sensor(0x3c05, 0x98);	/*  */
	OV5645MIPI_write_cmos_sensor(0x3c07, 0x07);	/*  */
	OV5645MIPI_write_cmos_sensor(0x3c08, 0x01);	/*  */
	OV5645MIPI_write_cmos_sensor(0x3c09, 0xc2);	/*  */
	OV5645MIPI_write_cmos_sensor(0x3c0a, 0x9c);	/*  */
	OV5645MIPI_write_cmos_sensor(0x3c0b, 0x40);	/*  */
	OV5645MIPI_write_cmos_sensor(0x4001, 0x02);	/* ; BLC start line */
	OV5645MIPI_write_cmos_sensor(0x4004, 0x02);	/* ; BLC line number */
	/* ; BLC update triggered by gain change */
	OV5645MIPI_write_cmos_sensor(0x4005, 0x18);
	OV5645MIPI_write_cmos_sensor(0x4300, 0x30);	/* ; YUV 422, YUYV */
	OV5645MIPI_write_cmos_sensor(0x4514, 0x00);	/*  */
	OV5645MIPI_write_cmos_sensor(0x4520, 0xb0);	/*  */
	OV5645MIPI_write_cmos_sensor(0x460b, 0x37);	/*  */
	OV5645MIPI_write_cmos_sensor(0x4818, 0x01);	/*  */
	OV5645MIPI_write_cmos_sensor(0x481d, 0xf0);	/*  */
	OV5645MIPI_write_cmos_sensor(0x481f, 0x50);	/*  */
	OV5645MIPI_write_cmos_sensor(0x4823, 0x70);	/*  */
	OV5645MIPI_write_cmos_sensor(0x4831, 0x14);	/*  */
	OV5645MIPI_write_cmos_sensor(0x4837, 0x11);	/*  */
	/* ; Lenc on, raw gamma on, BPC on, WPC on */
	OV5645MIPI_write_cmos_sensor(0x5000, 0xa7);
	/* ; SDE on, scale off, UV adjust off */
	OV5645MIPI_write_cmos_sensor(0x5001, 0xa3);
	OV5645MIPI_write_cmos_sensor(0x5002, 0x80);	/*  */
	OV5645MIPI_write_cmos_sensor(0x501d, 0x00);	/*  */
	/* ; select ISP YUV 422 */
	OV5645MIPI_write_cmos_sensor(0x501f, 0x00);
	OV5645MIPI_write_cmos_sensor(0x503d, 0x00);	/*  */
	OV5645MIPI_write_cmos_sensor(0x505c, 0x30);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5181, 0x59);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5183, 0x00);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5191, 0xf0);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5192, 0x03);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5684, 0x10);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5685, 0xa0);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5686, 0x0c);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5687, 0x78);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5a00, 0x08);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5a21, 0x00);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5a24, 0x00);	/*  */
	/* ; wake up from software standby */
	OV5645MIPI_write_cmos_sensor(0x3008, 0x02);
	/* ; AGC on, AEC on */
	OV5645MIPI_write_cmos_sensor(0x3503, 0x00);
	OV5645MIPI_write_cmos_sensor(0x5180, 0xff);	/* ;awb */
	OV5645MIPI_write_cmos_sensor(0x5181, 0xf3);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5182, 0x0);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5183, 0x14);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5184, 0x25);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5185, 0x24);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5186, 0xe);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5187, 0x10);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5188, 0xb);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5189, 0x74);	/*  */
	OV5645MIPI_write_cmos_sensor(0x518a, 0x54);	/*  */
	OV5645MIPI_write_cmos_sensor(0x518b, 0xeb);	/*  */
	OV5645MIPI_write_cmos_sensor(0x518c, 0xa8);	/*  */
	OV5645MIPI_write_cmos_sensor(0x518d, 0x36);	/*  */
	OV5645MIPI_write_cmos_sensor(0x518e, 0x2d);	/*  */
	OV5645MIPI_write_cmos_sensor(0x518f, 0x51);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5190, 0x40);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5191, 0xf8);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5192, 0x4);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5193, 0x70);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5194, 0xf0);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5195, 0xf0);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5196, 0x3);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5197, 0x1);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5198, 0x5);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5199, 0xe5);	/*  */
	OV5645MIPI_write_cmos_sensor(0x519a, 0x4);	/*  */
	OV5645MIPI_write_cmos_sensor(0x519b, 0x0);	/*  */
	OV5645MIPI_write_cmos_sensor(0x519c, 0x4);	/*  */
	OV5645MIPI_write_cmos_sensor(0x519d, 0x8f);	/*  */
	OV5645MIPI_write_cmos_sensor(0x519e, 0x38);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5381, 0x1e);	/* ccm */
	OV5645MIPI_write_cmos_sensor(0x5382, 0x5b);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5383, 0x08);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5384, 0x0a);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5385, 0x7e);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5386, 0x88);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5387, 0x7c);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5388, 0x6c);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5389, 0x10);	/*  */
	OV5645MIPI_write_cmos_sensor(0x538a, 0x01);	/*  */
	OV5645MIPI_write_cmos_sensor(0x538b, 0x98);	/*  */
	/* ; sharpen MT th1 */
	OV5645MIPI_write_cmos_sensor(0x5300, 0x08);
	/* ; sharpen MT th2 */
	OV5645MIPI_write_cmos_sensor(0x5301, 0x30);
	/* ; sharpen MT off1 */
	OV5645MIPI_write_cmos_sensor(0x5302, 0x18);
	/* ; sharpen MT off2 */
	OV5645MIPI_write_cmos_sensor(0x5303, 0x08);
	/* ; DNS th1 */
	OV5645MIPI_write_cmos_sensor(0x5304, 0x08);
	/* ; DNS th2 */
	OV5645MIPI_write_cmos_sensor(0x5305, 0x30);
	/* ; DNS off1 */
	OV5645MIPI_write_cmos_sensor(0x5306, 0x08);
	/* ; DNS off2 */
	OV5645MIPI_write_cmos_sensor(0x5307, 0x16);
	/* ; sharpen TH th1 */
	OV5645MIPI_write_cmos_sensor(0x5309, 0x08);
	/* ; sharpen TH th2 */
	OV5645MIPI_write_cmos_sensor(0x530a, 0x30);
	/* ; sharpen TH th2 */
	OV5645MIPI_write_cmos_sensor(0x530b, 0x04);
	/* ; sharpen TH off2 */
	OV5645MIPI_write_cmos_sensor(0x530c, 0x06);
	/* ; bias on */
	OV5645MIPI_write_cmos_sensor(0x5480, 0x01);
	/* ; Y yst 00 */
	OV5645MIPI_write_cmos_sensor(0x5481, 0x08);
	OV5645MIPI_write_cmos_sensor(0x5482, 0x14);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5483, 0x28);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5484, 0x51);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5485, 0x65);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5486, 0x71);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5487, 0x7d);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5488, 0x87);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5489, 0x91);	/*  */
	OV5645MIPI_write_cmos_sensor(0x548a, 0x9a);	/*  */
	OV5645MIPI_write_cmos_sensor(0x548b, 0xaa);	/*  */
	OV5645MIPI_write_cmos_sensor(0x548c, 0xb8);	/*  */
	OV5645MIPI_write_cmos_sensor(0x548d, 0xcd);	/*  */
	OV5645MIPI_write_cmos_sensor(0x548e, 0xdd);	/*  */
	OV5645MIPI_write_cmos_sensor(0x548f, 0xea);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5490, 0x1d);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5588, 0x01);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5580, 0x06);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5583, 0x40);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5584, 0x38);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5589, 0x18);	/*  */
	OV5645MIPI_write_cmos_sensor(0x558a, 0x00);	/*  */
	OV5645MIPI_write_cmos_sensor(0x558b, 0x3c);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5800, 0x3F);	/* lsc */
	OV5645MIPI_write_cmos_sensor(0x5801, 0x1D);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5802, 0x19);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5803, 0x18);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5804, 0x1E);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5805, 0x3F);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5806, 0x12);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5807, 0x0D);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5808, 0x09);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5809, 0x09);	/*  */
	OV5645MIPI_write_cmos_sensor(0x580a, 0x0D);	/*  */
	OV5645MIPI_write_cmos_sensor(0x580b, 0x11);	/*  */
	OV5645MIPI_write_cmos_sensor(0x580c, 0x0E);	/*  */
	OV5645MIPI_write_cmos_sensor(0x580d, 0x04);	/*  */
	OV5645MIPI_write_cmos_sensor(0x580e, 0x01);	/*  */
	OV5645MIPI_write_cmos_sensor(0x580f, 0x01);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5810, 0x05);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5811, 0x0D);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5812, 0x0B);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5813, 0x04);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5814, 0x00);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5815, 0x00);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5816, 0x04);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5817, 0x0B);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5818, 0x12);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5819, 0x0A);	/*  */
	OV5645MIPI_write_cmos_sensor(0x581a, 0x06);	/*  */
	OV5645MIPI_write_cmos_sensor(0x581b, 0x06);	/*  */
	OV5645MIPI_write_cmos_sensor(0x581c, 0x0A);	/*  */
	OV5645MIPI_write_cmos_sensor(0x581d, 0x10);	/*  */
	OV5645MIPI_write_cmos_sensor(0x581e, 0x2F);	/*  */
	OV5645MIPI_write_cmos_sensor(0x581f, 0x17);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5820, 0x13);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5821, 0x13);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5822, 0x18);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5823, 0x33);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5824, 0x84);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5825, 0x27);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5826, 0x29);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5827, 0x27);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5828, 0x56);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5829, 0x47);	/*  */
	OV5645MIPI_write_cmos_sensor(0x582a, 0x45);	/*  */
	OV5645MIPI_write_cmos_sensor(0x582b, 0x54);	/*  */
	OV5645MIPI_write_cmos_sensor(0x582c, 0x55);	/*  */
	OV5645MIPI_write_cmos_sensor(0x582d, 0x27);	/*  */
	OV5645MIPI_write_cmos_sensor(0x582e, 0x46);	/*  */
	OV5645MIPI_write_cmos_sensor(0x582f, 0x62);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5830, 0x60);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5831, 0x62);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5832, 0x26);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5833, 0x37);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5834, 0x55);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5835, 0x63);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5836, 0x45);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5837, 0x17);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5838, 0x56);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5839, 0x2A);	/*  */
	OV5645MIPI_write_cmos_sensor(0x583a, 0x0A);	/*  */
	OV5645MIPI_write_cmos_sensor(0x583b, 0x18);	/*  */
	OV5645MIPI_write_cmos_sensor(0x583c, 0x36);	/*  */
	OV5645MIPI_write_cmos_sensor(0x583d, 0xAE);	/*  */
	OV5645MIPI_write_cmos_sensor(0x583e, 0x10);	/*  */
	OV5645MIPI_write_cmos_sensor(0x583f, 0x08);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5840, 0x00);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5025, 0x00);	/*  */
	/* ; ae mode */
	OV5645MIPI_write_cmos_sensor(0x3a00, 0x38);
	/* ; AEC in H */
	OV5645MIPI_write_cmos_sensor(0x3a0f, 0x30);
	/* ; AEC in L */
	OV5645MIPI_write_cmos_sensor(0x3a10, 0x28);
	/* ; AEC out H */
	OV5645MIPI_write_cmos_sensor(0x3a1b, 0x30);
	/* ; AEC out L */
	OV5645MIPI_write_cmos_sensor(0x3a1e, 0x26);
	/* ; control zone H */
	OV5645MIPI_write_cmos_sensor(0x3a11, 0x60);
	/* ; control zone L */
	OV5645MIPI_write_cmos_sensor(0x3a1f, 0x14);
	LOG_INF("exit %s function:\n ", __func__);

}

/*****************************************************************
 * FUNCTION
 *    OV5645MIPIPreviewSetting
 *
 * DESCRIPTION
 *    This function config Preview setting related registers of CMOS sensor.
 *
 * PARAMETERS
 *    None
 *
 * RETURNS
 *    None
 *
 * LOCAL AFFECTED
 *
 *************************************************************************/
static void OV5645MIPIPreviewSetting_SVGA(void)
{
	/* ;OV5645MIPI 1280x960,30fps */
	/* 56Mhz, 224Mbps/Lane, 2Lane. */
	LOG_INF("enter %s function:\n ", __func__);
	/* ;stad */
	OV5645MIPI_write_cmos_sensor(0x3008, 0x42);
	/* ; MIPI 2 lane */
	OV5645MIPI_write_cmos_sensor(0x300e, 0x45);
	/* PLL, MIPI 8-bit mode */
	OV5645MIPI_write_cmos_sensor(0x3034, 0x18);
	/* PLL */
	OV5645MIPI_write_cmos_sensor(0x3035, 0x21);
	/* PLL */
	OV5645MIPI_write_cmos_sensor(0x3036, 0x70);
	/* PLL */
	OV5645MIPI_write_cmos_sensor(0x3037, 0x13);
	/* PLL */
	OV5645MIPI_write_cmos_sensor(0x3108, 0x01);
	/* PLL */
	OV5645MIPI_write_cmos_sensor(0x3824, 0x01);
	/* PLL */
	OV5645MIPI_write_cmos_sensor(0x460c, 0x20);
	OV5645MIPI_write_cmos_sensor(0x3618, 0x00);	/*  */
	OV5645MIPI_write_cmos_sensor(0x3600, 0x09);	/*  */
	OV5645MIPI_write_cmos_sensor(0x3601, 0x43);	/*  */
	OV5645MIPI_write_cmos_sensor(0x3708, 0x66);	/*  */
	OV5645MIPI_write_cmos_sensor(0x3709, 0x12);	/*  */
	OV5645MIPI_write_cmos_sensor(0x370c, 0xc3);	/*  */
	/* HS = 0 */
	OV5645MIPI_write_cmos_sensor(0x3800, 0x00);
	/* HS */
	OV5645MIPI_write_cmos_sensor(0x3801, 0x00);
	/* VS = 250 */
	OV5645MIPI_write_cmos_sensor(0x3802, 0x00);
	/* VS */
	OV5645MIPI_write_cmos_sensor(0x3803, 0x06);
	/* HW = 2623 */
	OV5645MIPI_write_cmos_sensor(0x3804, 0x0a);
	/* ; HW */
	OV5645MIPI_write_cmos_sensor(0x3805, 0x3f);
	/* ; VH = */
	OV5645MIPI_write_cmos_sensor(0x3806, 0x07);
	/* ; VH */
	OV5645MIPI_write_cmos_sensor(0x3807, 0x9d);
	/* ; DVPHO = 1280 */
	OV5645MIPI_write_cmos_sensor(0x3808, 0x05);
	/* ; DVPHO */
	OV5645MIPI_write_cmos_sensor(0x3809, 0x00);
	/* ; DVPVO = 960 */
	OV5645MIPI_write_cmos_sensor(0x380a, 0x03);
	/* ; DVPVO */
	OV5645MIPI_write_cmos_sensor(0x380b, 0xc0);
	/* ; HTS = 2160 */
	OV5645MIPI_write_cmos_sensor(0x380c, 0x07);
	/* ; HTS */
	OV5645MIPI_write_cmos_sensor(0x380d, 0x68);
	/* ; VTS = 740 */
	OV5645MIPI_write_cmos_sensor(0x380e, 0x03);
	/* ; VTS */
	OV5645MIPI_write_cmos_sensor(0x380f, 0xd8);
	/* H OFF = 16 */
	OV5645MIPI_write_cmos_sensor(0x3810, 0x00);
	/* H OFF */
	OV5645MIPI_write_cmos_sensor(0x3811, 0x10);
	/* V OFF = 4 */
	OV5645MIPI_write_cmos_sensor(0x3812, 0x00);
	/* ; V OFF */
	OV5645MIPI_write_cmos_sensor(0x3813, 0x06);
	/* ; X INC */
	OV5645MIPI_write_cmos_sensor(0x3814, 0x31);
	/* ; Y INC */
	OV5645MIPI_write_cmos_sensor(0x3815, 0x31);
	/* ; flip off, V bin on */
	OV5645MIPI_write_cmos_sensor(0x3820, 0x41);
	/* ; mirror on, H bin on */
	OV5645MIPI_write_cmos_sensor(0x3821, 0x07);
	/* ; max exp 60 = 740 */
	OV5645MIPI_write_cmos_sensor(0x3a02, 0x03);
	/* ; max exp 60 */
	OV5645MIPI_write_cmos_sensor(0x3a03, 0xd8);
	/* ; B50 = 222 */
	OV5645MIPI_write_cmos_sensor(0x3a08, 0x01);
	/* ; B50 */
	OV5645MIPI_write_cmos_sensor(0x3a09, 0x27);
	/* ; B60 = 185 */
	OV5645MIPI_write_cmos_sensor(0x3a0a, 0x00);
	/* ; B60 */
	OV5645MIPI_write_cmos_sensor(0x3a0b, 0xf6);
	/* ; max 50 */
	OV5645MIPI_write_cmos_sensor(0x3a0e, 0x03);
	/* ; max 60 */
	OV5645MIPI_write_cmos_sensor(0x3a0d, 0x04);
	/* ; max exp 50 = 740 */
	OV5645MIPI_write_cmos_sensor(0x3a14, 0x03);
	/* ; max exp 50 */
	OV5645MIPI_write_cmos_sensor(0x3a15, 0xd8);
	/* ; 50/60 auto detect */
	OV5645MIPI_write_cmos_sensor(0x3c07, 0x07);
	/* ; 50/60 auto detect */
	OV5645MIPI_write_cmos_sensor(0x3c08, 0x01);
	/* ; 50/60 auto detect */
	OV5645MIPI_write_cmos_sensor(0x3c09, 0xc2);
	/* ; BLC line number */
	OV5645MIPI_write_cmos_sensor(0x4004, 0x02);
	/* ; BLC triggered by gain change */
	OV5645MIPI_write_cmos_sensor(0x4005, 0x18);
	/* MIPI global timing 16 */
	OV5645MIPI_write_cmos_sensor(0x4837, 0x11);
	OV5645MIPI_write_cmos_sensor(0x503d, 0x00);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5000, 0xa7);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5001, 0x83);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5002, 0x80);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5003, 0x08);	/*  */
	OV5645MIPI_write_cmos_sensor(0x3032, 0x00);	/*  */
	OV5645MIPI_write_cmos_sensor(0x4000, 0x89);	/*  */
	/* ; ae mode */
	OV5645MIPI_write_cmos_sensor(0x3a00, 0x38);
	/* ;stad */
	OV5645MIPI_write_cmos_sensor(0x3008, 0x02);
	OV5645MIPIWriteExpShutter(OV5645MIPISensor.PreviewShutter);
	OV5645MIPIWriteExtraShutter(OV5645MIPISensor.PreviewExtraShutter);
	spin_lock(&ov5645mipi_drv_lock);
	OV5645MIPISensor.SensorMode = SENSOR_MODE_PREVIEW;
	OV5645MIPISensor.IsPVmode = KAL_TRUE;
	OV5645MIPISensor.PreviewPclk = 560;
	spin_unlock(&ov5645mipi_drv_lock);
	LOG_INF("exit %s function:\n ", __func__);
}

/*************************************************************************
 * FUNCTION
 *     OV5645MIPIFullSizeCaptureSetting
 *
 * DESCRIPTION
 *    This function config full size capture setting
 *					related registers of CMOS sensor.
 *
 * PARAMETERS
 *    None
 *
 * RETURNS
 *    None
 *
 * LOCAL AFFECTED
 *
 *************************************************************************/
static void OV5645MIPIFullSizeCaptureSetting(void)
{
	/* OV5645MIPI 2592x1944,10fps */
	/* 84Mhz, 336Mbps/Lane, 2Lane.15 */
	LOG_INF("enter %s function:\n ", __func__);
	/* ; MIPI 2 lane */
	OV5645MIPI_write_cmos_sensor(0x300e, 0x45);
	/* PLL, MIPI 8-bit mode */
	OV5645MIPI_write_cmos_sensor(0x3034, 0x18);
	/* PLL */
	OV5645MIPI_write_cmos_sensor(0x3035, 0x11);
	/* PLL */
	OV5645MIPI_write_cmos_sensor(0x3036, 0x54);
	/* PLL */
	OV5645MIPI_write_cmos_sensor(0x3037, 0x13);
	/* PLL */
	OV5645MIPI_write_cmos_sensor(0x3108, 0x01);
	/* PLL */
	OV5645MIPI_write_cmos_sensor(0x3824, 0x01);
	/* PLL */
	OV5645MIPI_write_cmos_sensor(0x460c, 0x20);
	OV5645MIPI_write_cmos_sensor(0x3618, 0x04);	/*  */
	OV5645MIPI_write_cmos_sensor(0x3600, 0x08);	/*  */
	OV5645MIPI_write_cmos_sensor(0x3601, 0x33);	/*  */
	OV5645MIPI_write_cmos_sensor(0x3708, 0x63);	/*  */
	OV5645MIPI_write_cmos_sensor(0x3709, 0x12);	/*  */
	OV5645MIPI_write_cmos_sensor(0x370c, 0xc0);	/*  */
	/* HS = 0 */
	OV5645MIPI_write_cmos_sensor(0x3800, 0x00);
	/* HS */
	OV5645MIPI_write_cmos_sensor(0x3801, 0x00);
	/* VS = 0 */
	OV5645MIPI_write_cmos_sensor(0x3802, 0x00);
	/* VS */
	OV5645MIPI_write_cmos_sensor(0x3803, 0x00);
	/* HW = 2623 */
	OV5645MIPI_write_cmos_sensor(0x3804, 0x0a);
	/* ; HW */
	OV5645MIPI_write_cmos_sensor(0x3805, 0x3f);
	/* ; VH = 1705 */
	OV5645MIPI_write_cmos_sensor(0x3806, 0x07);
	/* ; VH */
	OV5645MIPI_write_cmos_sensor(0x3807, 0x9f);
	/* ; DVPHO = 2560 */
	OV5645MIPI_write_cmos_sensor(0x3808, 0x0a);
	/* ; DVPHO */
	OV5645MIPI_write_cmos_sensor(0x3809, 0x20);
	/* ; DVPVO = 1440 */
	OV5645MIPI_write_cmos_sensor(0x380a, 0x07);
	/* ; DVPVO */
	OV5645MIPI_write_cmos_sensor(0x380b, 0x98);
	/* ; HTS = 2984 */
	OV5645MIPI_write_cmos_sensor(0x380c, 0x0b);
	/* ; HTS */
	OV5645MIPI_write_cmos_sensor(0x380d, 0x1c);
	/* ; VTS = 1464 */
	OV5645MIPI_write_cmos_sensor(0x380e, 0x07);
	/* ; VTS */
	OV5645MIPI_write_cmos_sensor(0x380f, 0xb0);
	/* H OFF = 16 */
	OV5645MIPI_write_cmos_sensor(0x3810, 0x00);
	/* H OFF */
	OV5645MIPI_write_cmos_sensor(0x3811, 0x10);
	/* V OFF = 4 */
	OV5645MIPI_write_cmos_sensor(0x3812, 0x00);
	/* ; V OFF */
	OV5645MIPI_write_cmos_sensor(0x3813, 0x06);
	/* ; X INC */
	OV5645MIPI_write_cmos_sensor(0x3814, 0x11);
	/* ; Y INC */
	OV5645MIPI_write_cmos_sensor(0x3815, 0x11);
	/* ; flip off, V bin off */
	OV5645MIPI_write_cmos_sensor(0x3820, 0x40);
	/* ; mirror on, H bin off */
	OV5645MIPI_write_cmos_sensor(0x3821, 0x06);
	/* ; BLC line number */
	OV5645MIPI_write_cmos_sensor(0x4004, 0x06);
	/* ; BLC triggered every frame */
	OV5645MIPI_write_cmos_sensor(0x4005, 0x1a);
	/* ; MIPI global timing */
	OV5645MIPI_write_cmos_sensor(0x4837, 0x16);
	OV5645MIPI_write_cmos_sensor(0x503d, 0x00);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5000, 0xa7);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5001, 0x83);	/*  */
	OV5645MIPI_write_cmos_sensor(0x5002, 0x81);
	OV5645MIPI_write_cmos_sensor(0x5003, 0x08);
	OV5645MIPI_write_cmos_sensor(0x3032, 0x00);
	OV5645MIPI_write_cmos_sensor(0x4000, 0x89);
	spin_lock(&ov5645mipi_drv_lock);
	OV5645MIPISensor.IsPVmode = KAL_FALSE;
	OV5645MIPISensor.CapturePclk = 840;
	OV5645MIPISensor.SensorMode = SENSOR_MODE_CAPTURE;
	spin_unlock(&ov5645mipi_drv_lock);
	LOG_INF("exit %s function:\n ", __func__);
}

/*************************************************************************
 * FUNCTION
 *    OV5645MIPISetHVMirror
 *
 * DESCRIPTION
 *    This function set sensor Mirror
 *
 * PARAMETERS
 *    Mirror
 *
 * RETURNS
 *    None
 *
 * LOCAL AFFECTED
 *
 *************************************************************************/
static void OV5645MIPISetHVMirror(kal_uint8 Mirror, kal_uint8 Mode)
{
	kal_uint8 mirror = 0, flip = 0;

	LOG_INF("enter %s %d %d:\n ", __func__, Mode, Mirror);
	flip = OV5645MIPIYUV_read_cmos_sensor(0x3820);
	mirror = OV5645MIPIYUV_read_cmos_sensor(0x3821);

	if (Mode == SENSOR_MODE_PREVIEW) {
		switch (Mirror) {
		case IMAGE_H_MIRROR:
			OV5645MIPI_write_cmos_sensor(0x3820, flip & 0xf9);
			OV5645MIPI_write_cmos_sensor(0x3821, mirror & 0xf9);
			OV5645MIPI_write_cmos_sensor(0x4514, 0x00);
			break;
		case IMAGE_NORMAL:
			OV5645MIPI_write_cmos_sensor(0x3820, flip & 0xf9);
			OV5645MIPI_write_cmos_sensor(0x3821, mirror | 0x06);
			OV5645MIPI_write_cmos_sensor(0x4514, 0x00);
			break;
		case IMAGE_V_MIRROR:
			OV5645MIPI_write_cmos_sensor(0x3820, flip | 0x06);
			OV5645MIPI_write_cmos_sensor(0x3821, mirror & 0xf9);
			OV5645MIPI_write_cmos_sensor(0x4514, 0x00);
			break;
		case IMAGE_HV_MIRROR:
			OV5645MIPI_write_cmos_sensor(0x3820, flip | 0x06);
			OV5645MIPI_write_cmos_sensor(0x3821, mirror | 0x06);
			OV5645MIPI_write_cmos_sensor(0x4514, 0x00);
			break;
		default:
			ASSERT(0);
		}
	} else if (Mode == SENSOR_MODE_CAPTURE) {
		switch (Mirror) {
		case IMAGE_H_MIRROR:
			OV5645MIPI_write_cmos_sensor(0x3820, flip & 0xf9);
			OV5645MIPI_write_cmos_sensor(0x3821, mirror & 0xf9);
			OV5645MIPI_write_cmos_sensor(0x4514, 0x00);
			break;
		case IMAGE_NORMAL:
			OV5645MIPI_write_cmos_sensor(0x3820, flip & 0xf9);
			OV5645MIPI_write_cmos_sensor(0x3821, mirror | 0x06);
			OV5645MIPI_write_cmos_sensor(0x4514, 0x00);
			break;
		case IMAGE_V_MIRROR:
			OV5645MIPI_write_cmos_sensor(0x3820, flip | 0x06);
			OV5645MIPI_write_cmos_sensor(0x3821, mirror & 0xf9);
			OV5645MIPI_write_cmos_sensor(0x4514, 0xaa);
			break;
		case IMAGE_HV_MIRROR:
			OV5645MIPI_write_cmos_sensor(0x3820, flip | 0x06);
			OV5645MIPI_write_cmos_sensor(0x3821, mirror | 0x06);
			OV5645MIPI_write_cmos_sensor(0x4514, 0xbb);
			break;
		default:
			ASSERT(0);
		}
	}
	LOG_INF("exit %s function:\n ", __func__);
}

void OV5645MIPI_Standby(void)
{
	OV5645MIPI_write_cmos_sensor(0x3008, 0x42);
}

void OV5645MIPI_Wakeup(void)
{
	OV5645MIPI_write_cmos_sensor(0x3008, 0x02);
}

/*************************************************************************
 * FUNCTION
 *   OV5645_FOCUS_OVT_AFC_Init
 * DESCRIPTION
 *   This function is to load micro code for AF function
 * PARAMETERS
 *   None
 * RETURNS
 *   None
 * GLOBALS AFFECTED
 *************************************************************************/
static void OV5645_FOCUS_OVT_AFC_Init(void)
{
	OV5645MIPI_write_cmos_sensor(0x3000, 0x20);
	OV5645MIPI_write_cmos_sensor(0x8000, 0x02);
	OV5645MIPI_write_cmos_sensor(0x8001, 0x15);
	OV5645MIPI_write_cmos_sensor(0x8002, 0x5c);
	OV5645MIPI_write_cmos_sensor(0x8003, 0x02);
	OV5645MIPI_write_cmos_sensor(0x8004, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8005, 0x01);
	OV5645MIPI_write_cmos_sensor(0x8006, 0xc2);
	OV5645MIPI_write_cmos_sensor(0x8007, 0x01);
	OV5645MIPI_write_cmos_sensor(0x8008, 0x22);
	OV5645MIPI_write_cmos_sensor(0x8009, 0x00);
	OV5645MIPI_write_cmos_sensor(0x800a, 0x00);
	OV5645MIPI_write_cmos_sensor(0x800b, 0x02);
	OV5645MIPI_write_cmos_sensor(0x800c, 0x15);
	OV5645MIPI_write_cmos_sensor(0x800d, 0x18);
	OV5645MIPI_write_cmos_sensor(0x800e, 0x78);
	OV5645MIPI_write_cmos_sensor(0x800f, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x8010, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8011, 0x18);
	OV5645MIPI_write_cmos_sensor(0x8012, 0xf6);
	OV5645MIPI_write_cmos_sensor(0x8013, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x8014, 0x30);
	OV5645MIPI_write_cmos_sensor(0x8015, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x8016, 0x13);
	OV5645MIPI_write_cmos_sensor(0x8017, 0xfc);
	OV5645MIPI_write_cmos_sensor(0x8018, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x8019, 0x31);
	OV5645MIPI_write_cmos_sensor(0x801a, 0x13);
	OV5645MIPI_write_cmos_sensor(0x801b, 0xfd);
	OV5645MIPI_write_cmos_sensor(0x801c, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x801d, 0x2e);
	OV5645MIPI_write_cmos_sensor(0x801e, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x801f, 0x13);
	OV5645MIPI_write_cmos_sensor(0x8020, 0xfe);
	OV5645MIPI_write_cmos_sensor(0x8021, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x8022, 0x2f);
	OV5645MIPI_write_cmos_sensor(0x8023, 0x13);
	OV5645MIPI_write_cmos_sensor(0x8024, 0x2d);
	OV5645MIPI_write_cmos_sensor(0x8025, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8026, 0x9b);
	OV5645MIPI_write_cmos_sensor(0x8027, 0xf6);
	OV5645MIPI_write_cmos_sensor(0x8028, 0xee);
	OV5645MIPI_write_cmos_sensor(0x8029, 0x3c);
	OV5645MIPI_write_cmos_sensor(0x802a, 0x18);
	OV5645MIPI_write_cmos_sensor(0x802b, 0xf6);
	OV5645MIPI_write_cmos_sensor(0x802c, 0x78);
	OV5645MIPI_write_cmos_sensor(0x802d, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x802e, 0xa6);
	OV5645MIPI_write_cmos_sensor(0x802f, 0x4f);
	OV5645MIPI_write_cmos_sensor(0x8030, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x8031, 0x1e);
	OV5645MIPI_write_cmos_sensor(0x8032, 0x70);
	OV5645MIPI_write_cmos_sensor(0x8033, 0x6b);
	OV5645MIPI_write_cmos_sensor(0x8034, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8035, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8036, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x8037, 0x25);
	OV5645MIPI_write_cmos_sensor(0x8038, 0xff);
	OV5645MIPI_write_cmos_sensor(0x8039, 0x33);
	OV5645MIPI_write_cmos_sensor(0x803a, 0x95);
	OV5645MIPI_write_cmos_sensor(0x803b, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x803c, 0xfe);
	OV5645MIPI_write_cmos_sensor(0x803d, 0x74);
	OV5645MIPI_write_cmos_sensor(0x803e, 0x9d);
	OV5645MIPI_write_cmos_sensor(0x803f, 0x2f);
	OV5645MIPI_write_cmos_sensor(0x8040, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x8041, 0x82);
	OV5645MIPI_write_cmos_sensor(0x8042, 0x74);
	OV5645MIPI_write_cmos_sensor(0x8043, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x8044, 0x3e);
	OV5645MIPI_write_cmos_sensor(0x8045, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x8046, 0x83);
	OV5645MIPI_write_cmos_sensor(0x8047, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x8048, 0x93);
	OV5645MIPI_write_cmos_sensor(0x8049, 0x78);
	OV5645MIPI_write_cmos_sensor(0x804a, 0xc1);
	OV5645MIPI_write_cmos_sensor(0x804b, 0xf6);
	OV5645MIPI_write_cmos_sensor(0x804c, 0x75);
	OV5645MIPI_write_cmos_sensor(0x804d, 0x4e);
	OV5645MIPI_write_cmos_sensor(0x804e, 0x02);
	OV5645MIPI_write_cmos_sensor(0x804f, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8050, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x8051, 0x31);
	OV5645MIPI_write_cmos_sensor(0x8052, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8053, 0x56);
	OV5645MIPI_write_cmos_sensor(0x8054, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8055, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x8056, 0x2c);
	OV5645MIPI_write_cmos_sensor(0x8057, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8058, 0x96);
	OV5645MIPI_write_cmos_sensor(0x8059, 0x12);
	OV5645MIPI_write_cmos_sensor(0x805a, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x805b, 0x2c);
	OV5645MIPI_write_cmos_sensor(0x805c, 0x12);
	OV5645MIPI_write_cmos_sensor(0x805d, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x805e, 0x91);
	OV5645MIPI_write_cmos_sensor(0x805f, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8060, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x8061, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8062, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8063, 0x9e);
	OV5645MIPI_write_cmos_sensor(0x8064, 0xf6);
	OV5645MIPI_write_cmos_sensor(0x8065, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8066, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x8067, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8068, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8069, 0xbe);
	OV5645MIPI_write_cmos_sensor(0x806a, 0xf6);
	OV5645MIPI_write_cmos_sensor(0x806b, 0x78);
	OV5645MIPI_write_cmos_sensor(0x806c, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x806d, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x806e, 0x78);
	OV5645MIPI_write_cmos_sensor(0x806f, 0xbf);
	OV5645MIPI_write_cmos_sensor(0x8070, 0xf6);
	OV5645MIPI_write_cmos_sensor(0x8071, 0x08);
	OV5645MIPI_write_cmos_sensor(0x8072, 0x76);
	OV5645MIPI_write_cmos_sensor(0x8073, 0x02);
	OV5645MIPI_write_cmos_sensor(0x8074, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8075, 0xc5);
	OV5645MIPI_write_cmos_sensor(0x8076, 0x76);
	OV5645MIPI_write_cmos_sensor(0x8077, 0x01);
	OV5645MIPI_write_cmos_sensor(0x8078, 0x08);
	OV5645MIPI_write_cmos_sensor(0x8079, 0x76);
	OV5645MIPI_write_cmos_sensor(0x807a, 0x01);
	OV5645MIPI_write_cmos_sensor(0x807b, 0x08);
	OV5645MIPI_write_cmos_sensor(0x807c, 0x76);
	OV5645MIPI_write_cmos_sensor(0x807d, 0x01);
	OV5645MIPI_write_cmos_sensor(0x807e, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x807f, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8080, 0xc9);
	OV5645MIPI_write_cmos_sensor(0x8081, 0xf6);
	OV5645MIPI_write_cmos_sensor(0x8082, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8083, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8084, 0xcb);
	OV5645MIPI_write_cmos_sensor(0x8085, 0xf6);
	OV5645MIPI_write_cmos_sensor(0x8086, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8087, 0xc9);
	OV5645MIPI_write_cmos_sensor(0x8088, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8089, 0x78);
	OV5645MIPI_write_cmos_sensor(0x808a, 0xcc);
	OV5645MIPI_write_cmos_sensor(0x808b, 0xf6);
	OV5645MIPI_write_cmos_sensor(0x808c, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x808d, 0x08);
	OV5645MIPI_write_cmos_sensor(0x808e, 0xf6);
	OV5645MIPI_write_cmos_sensor(0x808f, 0x08);
	OV5645MIPI_write_cmos_sensor(0x8090, 0xf6);
	OV5645MIPI_write_cmos_sensor(0x8091, 0x08);
	OV5645MIPI_write_cmos_sensor(0x8092, 0x76);
	OV5645MIPI_write_cmos_sensor(0x8093, 0x40);
	OV5645MIPI_write_cmos_sensor(0x8094, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8095, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x8096, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8097, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8098, 0xd0);
	OV5645MIPI_write_cmos_sensor(0x8099, 0xf6);
	OV5645MIPI_write_cmos_sensor(0x809a, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x809b, 0x08);
	OV5645MIPI_write_cmos_sensor(0x809c, 0xf6);
	OV5645MIPI_write_cmos_sensor(0x809d, 0xc2);
	OV5645MIPI_write_cmos_sensor(0x809e, 0x38);
	OV5645MIPI_write_cmos_sensor(0x809f, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x80a0, 0x1e);
	OV5645MIPI_write_cmos_sensor(0x80a1, 0x64);
	OV5645MIPI_write_cmos_sensor(0x80a2, 0x06);
	OV5645MIPI_write_cmos_sensor(0x80a3, 0x70);
	OV5645MIPI_write_cmos_sensor(0x80a4, 0x2e);
	OV5645MIPI_write_cmos_sensor(0x80a5, 0xd3);
	OV5645MIPI_write_cmos_sensor(0x80a6, 0x78);
	OV5645MIPI_write_cmos_sensor(0x80a7, 0xc0);
	OV5645MIPI_write_cmos_sensor(0x80a8, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x80a9, 0x64);
	OV5645MIPI_write_cmos_sensor(0x80aa, 0x80);
	OV5645MIPI_write_cmos_sensor(0x80ab, 0x94);
	OV5645MIPI_write_cmos_sensor(0x80ac, 0x80);
	OV5645MIPI_write_cmos_sensor(0x80ad, 0x40);
	OV5645MIPI_write_cmos_sensor(0x80ae, 0x02);
	OV5645MIPI_write_cmos_sensor(0x80af, 0x16);
	OV5645MIPI_write_cmos_sensor(0x80b0, 0x22);
	OV5645MIPI_write_cmos_sensor(0x80b1, 0xa2);
	OV5645MIPI_write_cmos_sensor(0x80b2, 0x38);
	OV5645MIPI_write_cmos_sensor(0x80b3, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x80b4, 0x33);
	OV5645MIPI_write_cmos_sensor(0x80b5, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x80b6, 0x41);
	OV5645MIPI_write_cmos_sensor(0x80b7, 0x90);
	OV5645MIPI_write_cmos_sensor(0x80b8, 0x30);
	OV5645MIPI_write_cmos_sensor(0x80b9, 0x28);
	OV5645MIPI_write_cmos_sensor(0x80ba, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x80bb, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x80bc, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x80bd, 0x1e);
	OV5645MIPI_write_cmos_sensor(0x80be, 0xc2);
	OV5645MIPI_write_cmos_sensor(0x80bf, 0x01);
	OV5645MIPI_write_cmos_sensor(0x80c0, 0x75);
	OV5645MIPI_write_cmos_sensor(0x80c1, 0x1d);
	OV5645MIPI_write_cmos_sensor(0x80c2, 0x10);
	OV5645MIPI_write_cmos_sensor(0x80c3, 0xd2);
	OV5645MIPI_write_cmos_sensor(0x80c4, 0x36);
	OV5645MIPI_write_cmos_sensor(0x80c5, 0x78);
	OV5645MIPI_write_cmos_sensor(0x80c6, 0x52);
	OV5645MIPI_write_cmos_sensor(0x80c7, 0xa6);
	OV5645MIPI_write_cmos_sensor(0x80c8, 0x2e);
	OV5645MIPI_write_cmos_sensor(0x80c9, 0x08);
	OV5645MIPI_write_cmos_sensor(0x80ca, 0xa6);
	OV5645MIPI_write_cmos_sensor(0x80cb, 0x2f);
	OV5645MIPI_write_cmos_sensor(0x80cc, 0x08);
	OV5645MIPI_write_cmos_sensor(0x80cd, 0xa6);
	OV5645MIPI_write_cmos_sensor(0x80ce, 0x30);
	OV5645MIPI_write_cmos_sensor(0x80cf, 0x08);
	OV5645MIPI_write_cmos_sensor(0x80d0, 0xa6);
	OV5645MIPI_write_cmos_sensor(0x80d1, 0x31);
	OV5645MIPI_write_cmos_sensor(0x80d2, 0x22);
	OV5645MIPI_write_cmos_sensor(0x80d3, 0x79);
	OV5645MIPI_write_cmos_sensor(0x80d4, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x80d5, 0xe7);
	OV5645MIPI_write_cmos_sensor(0x80d6, 0x78);
	OV5645MIPI_write_cmos_sensor(0x80d7, 0xc1);
	OV5645MIPI_write_cmos_sensor(0x80d8, 0x26);
	OV5645MIPI_write_cmos_sensor(0x80d9, 0x78);
	OV5645MIPI_write_cmos_sensor(0x80da, 0xc4);
	OV5645MIPI_write_cmos_sensor(0x80db, 0xf6);
	OV5645MIPI_write_cmos_sensor(0x80dc, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x80dd, 0x78);
	OV5645MIPI_write_cmos_sensor(0x80de, 0xce);
	OV5645MIPI_write_cmos_sensor(0x80df, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x80e0, 0x64);
	OV5645MIPI_write_cmos_sensor(0x80e1, 0x80);
	OV5645MIPI_write_cmos_sensor(0x80e2, 0xf8);
	OV5645MIPI_write_cmos_sensor(0x80e3, 0x09);
	OV5645MIPI_write_cmos_sensor(0x80e4, 0xe7);
	OV5645MIPI_write_cmos_sensor(0x80e5, 0x64);
	OV5645MIPI_write_cmos_sensor(0x80e6, 0x80);
	OV5645MIPI_write_cmos_sensor(0x80e7, 0x98);
	OV5645MIPI_write_cmos_sensor(0x80e8, 0x50);
	OV5645MIPI_write_cmos_sensor(0x80e9, 0x06);
	OV5645MIPI_write_cmos_sensor(0x80ea, 0x78);
	OV5645MIPI_write_cmos_sensor(0x80eb, 0xce);
	OV5645MIPI_write_cmos_sensor(0x80ec, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x80ed, 0x78);
	OV5645MIPI_write_cmos_sensor(0x80ee, 0xc4);
	OV5645MIPI_write_cmos_sensor(0x80ef, 0xf6);
	OV5645MIPI_write_cmos_sensor(0x80f0, 0xd3);
	OV5645MIPI_write_cmos_sensor(0x80f1, 0x78);
	OV5645MIPI_write_cmos_sensor(0x80f2, 0xcf);
	OV5645MIPI_write_cmos_sensor(0x80f3, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x80f4, 0x64);
	OV5645MIPI_write_cmos_sensor(0x80f5, 0x80);
	OV5645MIPI_write_cmos_sensor(0x80f6, 0xf8);
	OV5645MIPI_write_cmos_sensor(0x80f7, 0x79);
	OV5645MIPI_write_cmos_sensor(0x80f8, 0xc4);
	OV5645MIPI_write_cmos_sensor(0x80f9, 0xe7);
	OV5645MIPI_write_cmos_sensor(0x80fa, 0x64);
	OV5645MIPI_write_cmos_sensor(0x80fb, 0x80);
	OV5645MIPI_write_cmos_sensor(0x80fc, 0x98);
	OV5645MIPI_write_cmos_sensor(0x80fd, 0x40);
	OV5645MIPI_write_cmos_sensor(0x80fe, 0x06);
	OV5645MIPI_write_cmos_sensor(0x80ff, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8100, 0xcf);
	OV5645MIPI_write_cmos_sensor(0x8101, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8102, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8103, 0xc4);
	OV5645MIPI_write_cmos_sensor(0x8104, 0xf6);
	OV5645MIPI_write_cmos_sensor(0x8105, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8106, 0xc4);
	OV5645MIPI_write_cmos_sensor(0x8107, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8108, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x8109, 0x4f);
	OV5645MIPI_write_cmos_sensor(0x810a, 0x12);
	OV5645MIPI_write_cmos_sensor(0x810b, 0x12);
	OV5645MIPI_write_cmos_sensor(0x810c, 0xaf);
	OV5645MIPI_write_cmos_sensor(0x810d, 0x78);
	OV5645MIPI_write_cmos_sensor(0x810e, 0xc1);
	OV5645MIPI_write_cmos_sensor(0x810f, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8110, 0xff);
	OV5645MIPI_write_cmos_sensor(0x8111, 0x33);
	OV5645MIPI_write_cmos_sensor(0x8112, 0x95);
	OV5645MIPI_write_cmos_sensor(0x8113, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x8114, 0xfe);
	OV5645MIPI_write_cmos_sensor(0x8115, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8116, 0x15);
	OV5645MIPI_write_cmos_sensor(0x8117, 0x2e);
	OV5645MIPI_write_cmos_sensor(0x8118, 0x8f);
	OV5645MIPI_write_cmos_sensor(0x8119, 0x0a);
	OV5645MIPI_write_cmos_sensor(0x811a, 0xd3);
	OV5645MIPI_write_cmos_sensor(0x811b, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x811c, 0x0a);
	OV5645MIPI_write_cmos_sensor(0x811d, 0x64);
	OV5645MIPI_write_cmos_sensor(0x811e, 0x80);
	OV5645MIPI_write_cmos_sensor(0x811f, 0x94);
	OV5645MIPI_write_cmos_sensor(0x8120, 0x86);
	OV5645MIPI_write_cmos_sensor(0x8121, 0x40);
	OV5645MIPI_write_cmos_sensor(0x8122, 0x05);
	OV5645MIPI_write_cmos_sensor(0x8123, 0x75);
	OV5645MIPI_write_cmos_sensor(0x8124, 0x1e);
	OV5645MIPI_write_cmos_sensor(0x8125, 0x01);
	OV5645MIPI_write_cmos_sensor(0x8126, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8127, 0x1f);
	OV5645MIPI_write_cmos_sensor(0x8128, 0xd3);
	OV5645MIPI_write_cmos_sensor(0x8129, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x812a, 0x0a);
	OV5645MIPI_write_cmos_sensor(0x812b, 0x64);
	OV5645MIPI_write_cmos_sensor(0x812c, 0x80);
	OV5645MIPI_write_cmos_sensor(0x812d, 0x94);
	OV5645MIPI_write_cmos_sensor(0x812e, 0x83);
	OV5645MIPI_write_cmos_sensor(0x812f, 0x40);
	OV5645MIPI_write_cmos_sensor(0x8130, 0x05);
	OV5645MIPI_write_cmos_sensor(0x8131, 0x75);
	OV5645MIPI_write_cmos_sensor(0x8132, 0x1e);
	OV5645MIPI_write_cmos_sensor(0x8133, 0x02);
	OV5645MIPI_write_cmos_sensor(0x8134, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8135, 0x11);
	OV5645MIPI_write_cmos_sensor(0x8136, 0xd3);
	OV5645MIPI_write_cmos_sensor(0x8137, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x8138, 0x0a);
	OV5645MIPI_write_cmos_sensor(0x8139, 0x64);
	OV5645MIPI_write_cmos_sensor(0x813a, 0x80);
	OV5645MIPI_write_cmos_sensor(0x813b, 0x94);
	OV5645MIPI_write_cmos_sensor(0x813c, 0x81);
	OV5645MIPI_write_cmos_sensor(0x813d, 0x40);
	OV5645MIPI_write_cmos_sensor(0x813e, 0x05);
	OV5645MIPI_write_cmos_sensor(0x813f, 0x75);
	OV5645MIPI_write_cmos_sensor(0x8140, 0x1e);
	OV5645MIPI_write_cmos_sensor(0x8141, 0x03);
	OV5645MIPI_write_cmos_sensor(0x8142, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8143, 0x03);
	OV5645MIPI_write_cmos_sensor(0x8144, 0x75);
	OV5645MIPI_write_cmos_sensor(0x8145, 0x1e);
	OV5645MIPI_write_cmos_sensor(0x8146, 0x04);
	OV5645MIPI_write_cmos_sensor(0x8147, 0xd3);
	OV5645MIPI_write_cmos_sensor(0x8148, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8149, 0xc0);
	OV5645MIPI_write_cmos_sensor(0x814a, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x814b, 0x64);
	OV5645MIPI_write_cmos_sensor(0x814c, 0x80);
	OV5645MIPI_write_cmos_sensor(0x814d, 0x94);
	OV5645MIPI_write_cmos_sensor(0x814e, 0x80);
	OV5645MIPI_write_cmos_sensor(0x814f, 0x40);
	OV5645MIPI_write_cmos_sensor(0x8150, 0x02);
	OV5645MIPI_write_cmos_sensor(0x8151, 0x16);
	OV5645MIPI_write_cmos_sensor(0x8152, 0x22);
	OV5645MIPI_write_cmos_sensor(0x8153, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8154, 0xc6);
	OV5645MIPI_write_cmos_sensor(0x8155, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8156, 0x18);
	OV5645MIPI_write_cmos_sensor(0x8157, 0xf6);
	OV5645MIPI_write_cmos_sensor(0x8158, 0x08);
	OV5645MIPI_write_cmos_sensor(0x8159, 0x06);
	OV5645MIPI_write_cmos_sensor(0x815a, 0x78);
	OV5645MIPI_write_cmos_sensor(0x815b, 0xc2);
	OV5645MIPI_write_cmos_sensor(0x815c, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x815d, 0xff);
	OV5645MIPI_write_cmos_sensor(0x815e, 0x12);
	OV5645MIPI_write_cmos_sensor(0x815f, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x8160, 0x99);
	OV5645MIPI_write_cmos_sensor(0x8161, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8162, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x8163, 0x2f);
	OV5645MIPI_write_cmos_sensor(0x8164, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8165, 0xc5);
	OV5645MIPI_write_cmos_sensor(0x8166, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8167, 0x25);
	OV5645MIPI_write_cmos_sensor(0x8168, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x8169, 0x24);
	OV5645MIPI_write_cmos_sensor(0x816a, 0x56);
	OV5645MIPI_write_cmos_sensor(0x816b, 0xf8);
	OV5645MIPI_write_cmos_sensor(0x816c, 0xa6);
	OV5645MIPI_write_cmos_sensor(0x816d, 0x06);
	OV5645MIPI_write_cmos_sensor(0x816e, 0x08);
	OV5645MIPI_write_cmos_sensor(0x816f, 0xa6);
	OV5645MIPI_write_cmos_sensor(0x8170, 0x07);
	OV5645MIPI_write_cmos_sensor(0x8171, 0x79);
	OV5645MIPI_write_cmos_sensor(0x8172, 0xc5);
	OV5645MIPI_write_cmos_sensor(0x8173, 0xe7);
	OV5645MIPI_write_cmos_sensor(0x8174, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8175, 0xc7);
	OV5645MIPI_write_cmos_sensor(0x8176, 0x66);
	OV5645MIPI_write_cmos_sensor(0x8177, 0x70);
	OV5645MIPI_write_cmos_sensor(0x8178, 0x05);
	OV5645MIPI_write_cmos_sensor(0x8179, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x817a, 0x78);
	OV5645MIPI_write_cmos_sensor(0x817b, 0xc9);
	OV5645MIPI_write_cmos_sensor(0x817c, 0xf6);
	OV5645MIPI_write_cmos_sensor(0x817d, 0x22);
	OV5645MIPI_write_cmos_sensor(0x817e, 0x78);
	OV5645MIPI_write_cmos_sensor(0x817f, 0xc5);
	OV5645MIPI_write_cmos_sensor(0x8180, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8181, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8182, 0x99);
	OV5645MIPI_write_cmos_sensor(0x8183, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8184, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x8185, 0xfa);
	OV5645MIPI_write_cmos_sensor(0x8186, 0x40);
	OV5645MIPI_write_cmos_sensor(0x8187, 0x0d);
	OV5645MIPI_write_cmos_sensor(0x8188, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8189, 0xc5);
	OV5645MIPI_write_cmos_sensor(0x818a, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x818b, 0x12);
	OV5645MIPI_write_cmos_sensor(0x818c, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x818d, 0xdf);
	OV5645MIPI_write_cmos_sensor(0x818e, 0xfe);
	OV5645MIPI_write_cmos_sensor(0x818f, 0x08);
	OV5645MIPI_write_cmos_sensor(0x8190, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8191, 0xff);
	OV5645MIPI_write_cmos_sensor(0x8192, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8193, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x8194, 0x91);
	OV5645MIPI_write_cmos_sensor(0x8195, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8196, 0xc5);
	OV5645MIPI_write_cmos_sensor(0x8197, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8198, 0x25);
	OV5645MIPI_write_cmos_sensor(0x8199, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x819a, 0x24);
	OV5645MIPI_write_cmos_sensor(0x819b, 0x57);
	OV5645MIPI_write_cmos_sensor(0x819c, 0xf9);
	OV5645MIPI_write_cmos_sensor(0x819d, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x819e, 0xe7);
	OV5645MIPI_write_cmos_sensor(0x819f, 0x78);
	OV5645MIPI_write_cmos_sensor(0x81a0, 0x97);
	OV5645MIPI_write_cmos_sensor(0x81a1, 0x96);
	OV5645MIPI_write_cmos_sensor(0x81a2, 0x19);
	OV5645MIPI_write_cmos_sensor(0x81a3, 0xe7);
	OV5645MIPI_write_cmos_sensor(0x81a4, 0x18);
	OV5645MIPI_write_cmos_sensor(0x81a5, 0x96);
	OV5645MIPI_write_cmos_sensor(0x81a6, 0x50);
	OV5645MIPI_write_cmos_sensor(0x81a7, 0x11);
	OV5645MIPI_write_cmos_sensor(0x81a8, 0x78);
	OV5645MIPI_write_cmos_sensor(0x81a9, 0xc5);
	OV5645MIPI_write_cmos_sensor(0x81aa, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x81ab, 0x12);
	OV5645MIPI_write_cmos_sensor(0x81ac, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x81ad, 0xdf);
	OV5645MIPI_write_cmos_sensor(0x81ae, 0xfe);
	OV5645MIPI_write_cmos_sensor(0x81af, 0x08);
	OV5645MIPI_write_cmos_sensor(0x81b0, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x81b1, 0xff);
	OV5645MIPI_write_cmos_sensor(0x81b2, 0x78);
	OV5645MIPI_write_cmos_sensor(0x81b3, 0x96);
	OV5645MIPI_write_cmos_sensor(0x81b4, 0xa6);
	OV5645MIPI_write_cmos_sensor(0x81b5, 0x06);
	OV5645MIPI_write_cmos_sensor(0x81b6, 0x08);
	OV5645MIPI_write_cmos_sensor(0x81b7, 0xa6);
	OV5645MIPI_write_cmos_sensor(0x81b8, 0x07);
	OV5645MIPI_write_cmos_sensor(0x81b9, 0x78);
	OV5645MIPI_write_cmos_sensor(0x81ba, 0xc5);
	OV5645MIPI_write_cmos_sensor(0x81bb, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x81bc, 0x25);
	OV5645MIPI_write_cmos_sensor(0x81bd, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x81be, 0x24);
	OV5645MIPI_write_cmos_sensor(0x81bf, 0x57);
	OV5645MIPI_write_cmos_sensor(0x81c0, 0xf9);
	OV5645MIPI_write_cmos_sensor(0x81c1, 0x78);
	OV5645MIPI_write_cmos_sensor(0x81c2, 0xc9);
	OV5645MIPI_write_cmos_sensor(0x81c3, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x81c4, 0x12);
	OV5645MIPI_write_cmos_sensor(0x81c5, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x81c6, 0xdf);
	OV5645MIPI_write_cmos_sensor(0x81c7, 0xfe);
	OV5645MIPI_write_cmos_sensor(0x81c8, 0x08);
	OV5645MIPI_write_cmos_sensor(0x81c9, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x81ca, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x81cb, 0x97);
	OV5645MIPI_write_cmos_sensor(0x81cc, 0xee);
	OV5645MIPI_write_cmos_sensor(0x81cd, 0x19);
	OV5645MIPI_write_cmos_sensor(0x81ce, 0x97);
	OV5645MIPI_write_cmos_sensor(0x81cf, 0x50);
	OV5645MIPI_write_cmos_sensor(0x81d0, 0x06);
	OV5645MIPI_write_cmos_sensor(0x81d1, 0x78);
	OV5645MIPI_write_cmos_sensor(0x81d2, 0xc5);
	OV5645MIPI_write_cmos_sensor(0x81d3, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x81d4, 0x78);
	OV5645MIPI_write_cmos_sensor(0x81d5, 0xc9);
	OV5645MIPI_write_cmos_sensor(0x81d6, 0xf6);
	OV5645MIPI_write_cmos_sensor(0x81d7, 0x78);
	OV5645MIPI_write_cmos_sensor(0x81d8, 0xc5);
	OV5645MIPI_write_cmos_sensor(0x81d9, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x81da, 0x24);
	OV5645MIPI_write_cmos_sensor(0x81db, 0x9e);
	OV5645MIPI_write_cmos_sensor(0x81dc, 0x78);
	OV5645MIPI_write_cmos_sensor(0x81dd, 0xbe);
	OV5645MIPI_write_cmos_sensor(0x81de, 0x12);
	OV5645MIPI_write_cmos_sensor(0x81df, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x81e0, 0x09);
	OV5645MIPI_write_cmos_sensor(0x81e1, 0x40);
	OV5645MIPI_write_cmos_sensor(0x81e2, 0x07);
	OV5645MIPI_write_cmos_sensor(0x81e3, 0x12);
	OV5645MIPI_write_cmos_sensor(0x81e4, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x81e5, 0x99);
	OV5645MIPI_write_cmos_sensor(0x81e6, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x81e7, 0x78);
	OV5645MIPI_write_cmos_sensor(0x81e8, 0xbe);
	OV5645MIPI_write_cmos_sensor(0x81e9, 0xf6);
	OV5645MIPI_write_cmos_sensor(0x81ea, 0x78);
	OV5645MIPI_write_cmos_sensor(0x81eb, 0xc5);
	OV5645MIPI_write_cmos_sensor(0x81ec, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x81ed, 0x24);
	OV5645MIPI_write_cmos_sensor(0x81ee, 0x9e);
	OV5645MIPI_write_cmos_sensor(0x81ef, 0x78);
	OV5645MIPI_write_cmos_sensor(0x81f0, 0xbf);
	OV5645MIPI_write_cmos_sensor(0x81f1, 0x12);
	OV5645MIPI_write_cmos_sensor(0x81f2, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x81f3, 0xe8);
	OV5645MIPI_write_cmos_sensor(0x81f4, 0x50);
	OV5645MIPI_write_cmos_sensor(0x81f5, 0x07);
	OV5645MIPI_write_cmos_sensor(0x81f6, 0x12);
	OV5645MIPI_write_cmos_sensor(0x81f7, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x81f8, 0x99);
	OV5645MIPI_write_cmos_sensor(0x81f9, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x81fa, 0x78);
	OV5645MIPI_write_cmos_sensor(0x81fb, 0xbf);
	OV5645MIPI_write_cmos_sensor(0x81fc, 0xf6);
	OV5645MIPI_write_cmos_sensor(0x81fd, 0x78);
	OV5645MIPI_write_cmos_sensor(0x81fe, 0xc5);
	OV5645MIPI_write_cmos_sensor(0x81ff, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8200, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8201, 0xc8);
	OV5645MIPI_write_cmos_sensor(0x8202, 0xf6);
	OV5645MIPI_write_cmos_sensor(0x8203, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8204, 0x10);
	OV5645MIPI_write_cmos_sensor(0x8205, 0x7f);
	OV5645MIPI_write_cmos_sensor(0x8206, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8207, 0x0c);
	OV5645MIPI_write_cmos_sensor(0x8208, 0x86);
	OV5645MIPI_write_cmos_sensor(0x8209, 0x12);
	OV5645MIPI_write_cmos_sensor(0x820a, 0x14);
	OV5645MIPI_write_cmos_sensor(0x820b, 0xdd);
	OV5645MIPI_write_cmos_sensor(0x820c, 0x78);
	OV5645MIPI_write_cmos_sensor(0x820d, 0xcd);
	OV5645MIPI_write_cmos_sensor(0x820e, 0xa6);
	OV5645MIPI_write_cmos_sensor(0x820f, 0x07);
	OV5645MIPI_write_cmos_sensor(0x8210, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8211, 0x24);
	OV5645MIPI_write_cmos_sensor(0x8212, 0x02);
	OV5645MIPI_write_cmos_sensor(0x8213, 0x70);
	OV5645MIPI_write_cmos_sensor(0x8214, 0x03);
	OV5645MIPI_write_cmos_sensor(0x8215, 0x02);
	OV5645MIPI_write_cmos_sensor(0x8216, 0x02);
	OV5645MIPI_write_cmos_sensor(0x8217, 0x9e);
	OV5645MIPI_write_cmos_sensor(0x8218, 0x14);
	OV5645MIPI_write_cmos_sensor(0x8219, 0x70);
	OV5645MIPI_write_cmos_sensor(0x821a, 0x03);
	OV5645MIPI_write_cmos_sensor(0x821b, 0x02);
	OV5645MIPI_write_cmos_sensor(0x821c, 0x02);
	OV5645MIPI_write_cmos_sensor(0x821d, 0x9e);
	OV5645MIPI_write_cmos_sensor(0x821e, 0x24);
	OV5645MIPI_write_cmos_sensor(0x821f, 0xfe);
	OV5645MIPI_write_cmos_sensor(0x8220, 0x60);
	OV5645MIPI_write_cmos_sensor(0x8221, 0x03);
	OV5645MIPI_write_cmos_sensor(0x8222, 0x02);
	OV5645MIPI_write_cmos_sensor(0x8223, 0x03);
	OV5645MIPI_write_cmos_sensor(0x8224, 0xb1);
	OV5645MIPI_write_cmos_sensor(0x8225, 0xd2);
	OV5645MIPI_write_cmos_sensor(0x8226, 0x38);
	OV5645MIPI_write_cmos_sensor(0x8227, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8228, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x8229, 0x16);
	OV5645MIPI_write_cmos_sensor(0x822a, 0x40);
	OV5645MIPI_write_cmos_sensor(0x822b, 0x16);
	OV5645MIPI_write_cmos_sensor(0x822c, 0x78);
	OV5645MIPI_write_cmos_sensor(0x822d, 0xc9);
	OV5645MIPI_write_cmos_sensor(0x822e, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x822f, 0x24);
	OV5645MIPI_write_cmos_sensor(0x8230, 0x9d);
	OV5645MIPI_write_cmos_sensor(0x8231, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8232, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x8233, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8234, 0x50);
	OV5645MIPI_write_cmos_sensor(0x8235, 0x20);
	OV5645MIPI_write_cmos_sensor(0x8236, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8237, 0xc9);
	OV5645MIPI_write_cmos_sensor(0x8238, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8239, 0x24);
	OV5645MIPI_write_cmos_sensor(0x823a, 0x9d);
	OV5645MIPI_write_cmos_sensor(0x823b, 0xf8);
	OV5645MIPI_write_cmos_sensor(0x823c, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x823d, 0x78);
	OV5645MIPI_write_cmos_sensor(0x823e, 0xce);
	OV5645MIPI_write_cmos_sensor(0x823f, 0xf6);
	OV5645MIPI_write_cmos_sensor(0x8240, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8241, 0x14);
	OV5645MIPI_write_cmos_sensor(0x8242, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8243, 0xc9);
	OV5645MIPI_write_cmos_sensor(0x8244, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8245, 0x24);
	OV5645MIPI_write_cmos_sensor(0x8246, 0x9d);
	OV5645MIPI_write_cmos_sensor(0x8247, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8248, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x8249, 0x07);
	OV5645MIPI_write_cmos_sensor(0x824a, 0x40);
	OV5645MIPI_write_cmos_sensor(0x824b, 0x0a);
	OV5645MIPI_write_cmos_sensor(0x824c, 0x78);
	OV5645MIPI_write_cmos_sensor(0x824d, 0xc9);
	OV5645MIPI_write_cmos_sensor(0x824e, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x824f, 0x24);
	OV5645MIPI_write_cmos_sensor(0x8250, 0x9d);
	OV5645MIPI_write_cmos_sensor(0x8251, 0xf8);
	OV5645MIPI_write_cmos_sensor(0x8252, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8253, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8254, 0xcf);
	OV5645MIPI_write_cmos_sensor(0x8255, 0xf6);
	OV5645MIPI_write_cmos_sensor(0x8256, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8257, 0xca);
	OV5645MIPI_write_cmos_sensor(0x8258, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8259, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x825a, 0x52);
	OV5645MIPI_write_cmos_sensor(0x825b, 0x79);
	OV5645MIPI_write_cmos_sensor(0x825c, 0xc2);
	OV5645MIPI_write_cmos_sensor(0x825d, 0xe7);
	OV5645MIPI_write_cmos_sensor(0x825e, 0x78);
	OV5645MIPI_write_cmos_sensor(0x825f, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x8260, 0x66);
	OV5645MIPI_write_cmos_sensor(0x8261, 0x60);
	OV5645MIPI_write_cmos_sensor(0x8262, 0x03);
	OV5645MIPI_write_cmos_sensor(0x8263, 0x02);
	OV5645MIPI_write_cmos_sensor(0x8264, 0x04);
	OV5645MIPI_write_cmos_sensor(0x8265, 0x9f);
	OV5645MIPI_write_cmos_sensor(0x8266, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8267, 0xd1);
	OV5645MIPI_write_cmos_sensor(0x8268, 0x06);
	OV5645MIPI_write_cmos_sensor(0x8269, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x826a, 0x1e);
	OV5645MIPI_write_cmos_sensor(0x826b, 0xb4);
	OV5645MIPI_write_cmos_sensor(0x826c, 0x01);
	OV5645MIPI_write_cmos_sensor(0x826d, 0x07);
	OV5645MIPI_write_cmos_sensor(0x826e, 0x12);
	OV5645MIPI_write_cmos_sensor(0x826f, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x8270, 0x1f);
	OV5645MIPI_write_cmos_sensor(0x8271, 0xf6);
	OV5645MIPI_write_cmos_sensor(0x8272, 0x09);
	OV5645MIPI_write_cmos_sensor(0x8273, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8274, 0x1e);
	OV5645MIPI_write_cmos_sensor(0x8275, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x8276, 0x1e);
	OV5645MIPI_write_cmos_sensor(0x8277, 0xb4);
	OV5645MIPI_write_cmos_sensor(0x8278, 0x02);
	OV5645MIPI_write_cmos_sensor(0x8279, 0x08);
	OV5645MIPI_write_cmos_sensor(0x827a, 0x12);
	OV5645MIPI_write_cmos_sensor(0x827b, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x827c, 0x1f);
	OV5645MIPI_write_cmos_sensor(0x827d, 0xf6);
	OV5645MIPI_write_cmos_sensor(0x827e, 0x79);
	OV5645MIPI_write_cmos_sensor(0x827f, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x8280, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8281, 0x11);
	OV5645MIPI_write_cmos_sensor(0x8282, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x8283, 0x1e);
	OV5645MIPI_write_cmos_sensor(0x8284, 0xb4);
	OV5645MIPI_write_cmos_sensor(0x8285, 0x03);
	OV5645MIPI_write_cmos_sensor(0x8286, 0x14);
	OV5645MIPI_write_cmos_sensor(0x8287, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8288, 0xc1);
	OV5645MIPI_write_cmos_sensor(0x8289, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x828a, 0xf4);
	OV5645MIPI_write_cmos_sensor(0x828b, 0x04);
	OV5645MIPI_write_cmos_sensor(0x828c, 0xff);
	OV5645MIPI_write_cmos_sensor(0x828d, 0xa2);
	OV5645MIPI_write_cmos_sensor(0x828e, 0xe7);
	OV5645MIPI_write_cmos_sensor(0x828f, 0x13);
	OV5645MIPI_write_cmos_sensor(0x8290, 0xf6);
	OV5645MIPI_write_cmos_sensor(0x8291, 0x79);
	OV5645MIPI_write_cmos_sensor(0x8292, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x8293, 0xe7);
	OV5645MIPI_write_cmos_sensor(0x8294, 0x26);
	OV5645MIPI_write_cmos_sensor(0x8295, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8296, 0xc4);
	OV5645MIPI_write_cmos_sensor(0x8297, 0xf6);
	OV5645MIPI_write_cmos_sensor(0x8298, 0x02);
	OV5645MIPI_write_cmos_sensor(0x8299, 0x04);
	OV5645MIPI_write_cmos_sensor(0x829a, 0x8a);
	OV5645MIPI_write_cmos_sensor(0x829b, 0x02);
	OV5645MIPI_write_cmos_sensor(0x829c, 0x04);
	OV5645MIPI_write_cmos_sensor(0x829d, 0x1c);
	OV5645MIPI_write_cmos_sensor(0x829e, 0xd2);
	OV5645MIPI_write_cmos_sensor(0x829f, 0x38);
	OV5645MIPI_write_cmos_sensor(0x82a0, 0x78);
	OV5645MIPI_write_cmos_sensor(0x82a1, 0xd1);
	OV5645MIPI_write_cmos_sensor(0x82a2, 0x06);
	OV5645MIPI_write_cmos_sensor(0x82a3, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x82a4, 0x12);
	OV5645MIPI_write_cmos_sensor(0x82a5, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x82a6, 0x17);
	OV5645MIPI_write_cmos_sensor(0x82a7, 0x50);
	OV5645MIPI_write_cmos_sensor(0x82a8, 0x25);
	OV5645MIPI_write_cmos_sensor(0x82a9, 0x79);
	OV5645MIPI_write_cmos_sensor(0x82aa, 0xc9);
	OV5645MIPI_write_cmos_sensor(0x82ab, 0xe7);
	OV5645MIPI_write_cmos_sensor(0x82ac, 0x78);
	OV5645MIPI_write_cmos_sensor(0x82ad, 0xc5);
	OV5645MIPI_write_cmos_sensor(0x82ae, 0x66);
	OV5645MIPI_write_cmos_sensor(0x82af, 0x78);
	OV5645MIPI_write_cmos_sensor(0x82b0, 0xc9);
	OV5645MIPI_write_cmos_sensor(0x82b1, 0x60);
	OV5645MIPI_write_cmos_sensor(0x82b2, 0x05);
	OV5645MIPI_write_cmos_sensor(0x82b3, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x82b4, 0x04);
	OV5645MIPI_write_cmos_sensor(0x82b5, 0xff);
	OV5645MIPI_write_cmos_sensor(0x82b6, 0x80);
	OV5645MIPI_write_cmos_sensor(0x82b7, 0x02);
	OV5645MIPI_write_cmos_sensor(0x82b8, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x82b9, 0xff);
	OV5645MIPI_write_cmos_sensor(0x82ba, 0x8f);
	OV5645MIPI_write_cmos_sensor(0x82bb, 0x0a);
	OV5645MIPI_write_cmos_sensor(0x82bc, 0x74);
	OV5645MIPI_write_cmos_sensor(0x82bd, 0x9e);
	OV5645MIPI_write_cmos_sensor(0x82be, 0x2f);
	OV5645MIPI_write_cmos_sensor(0x82bf, 0x12);
	OV5645MIPI_write_cmos_sensor(0x82c0, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x82c1, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x82c2, 0x50);
	OV5645MIPI_write_cmos_sensor(0x82c3, 0x2d);
	OV5645MIPI_write_cmos_sensor(0x82c4, 0x74);
	OV5645MIPI_write_cmos_sensor(0x82c5, 0x9e);
	OV5645MIPI_write_cmos_sensor(0x82c6, 0x2f);
	OV5645MIPI_write_cmos_sensor(0x82c7, 0xf8);
	OV5645MIPI_write_cmos_sensor(0x82c8, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x82c9, 0x78);
	OV5645MIPI_write_cmos_sensor(0x82ca, 0xce);
	OV5645MIPI_write_cmos_sensor(0x82cb, 0xf6);
	OV5645MIPI_write_cmos_sensor(0x82cc, 0x80);
	OV5645MIPI_write_cmos_sensor(0x82cd, 0x23);
	OV5645MIPI_write_cmos_sensor(0x82ce, 0x79);
	OV5645MIPI_write_cmos_sensor(0x82cf, 0xc9);
	OV5645MIPI_write_cmos_sensor(0x82d0, 0xe7);
	OV5645MIPI_write_cmos_sensor(0x82d1, 0x78);
	OV5645MIPI_write_cmos_sensor(0x82d2, 0xc5);
	OV5645MIPI_write_cmos_sensor(0x82d3, 0x66);
	OV5645MIPI_write_cmos_sensor(0x82d4, 0x78);
	OV5645MIPI_write_cmos_sensor(0x82d5, 0xc9);
	OV5645MIPI_write_cmos_sensor(0x82d6, 0x60);
	OV5645MIPI_write_cmos_sensor(0x82d7, 0x05);
	OV5645MIPI_write_cmos_sensor(0x82d8, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x82d9, 0x04);
	OV5645MIPI_write_cmos_sensor(0x82da, 0xff);
	OV5645MIPI_write_cmos_sensor(0x82db, 0x80);
	OV5645MIPI_write_cmos_sensor(0x82dc, 0x02);
	OV5645MIPI_write_cmos_sensor(0x82dd, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x82de, 0xff);
	OV5645MIPI_write_cmos_sensor(0x82df, 0x8f);
	OV5645MIPI_write_cmos_sensor(0x82e0, 0x0a);
	OV5645MIPI_write_cmos_sensor(0x82e1, 0x74);
	OV5645MIPI_write_cmos_sensor(0x82e2, 0x9e);
	OV5645MIPI_write_cmos_sensor(0x82e3, 0x2f);
	OV5645MIPI_write_cmos_sensor(0x82e4, 0x12);
	OV5645MIPI_write_cmos_sensor(0x82e5, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x82e6, 0x07);
	OV5645MIPI_write_cmos_sensor(0x82e7, 0x40);
	OV5645MIPI_write_cmos_sensor(0x82e8, 0x08);
	OV5645MIPI_write_cmos_sensor(0x82e9, 0x74);
	OV5645MIPI_write_cmos_sensor(0x82ea, 0x9e);
	OV5645MIPI_write_cmos_sensor(0x82eb, 0x2f);
	OV5645MIPI_write_cmos_sensor(0x82ec, 0xf8);
	OV5645MIPI_write_cmos_sensor(0x82ed, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x82ee, 0x78);
	OV5645MIPI_write_cmos_sensor(0x82ef, 0xcf);
	OV5645MIPI_write_cmos_sensor(0x82f0, 0xf6);
	OV5645MIPI_write_cmos_sensor(0x82f1, 0x12);
	OV5645MIPI_write_cmos_sensor(0x82f2, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x82f3, 0x50);
	OV5645MIPI_write_cmos_sensor(0x82f4, 0x78);
	OV5645MIPI_write_cmos_sensor(0x82f5, 0xc1);
	OV5645MIPI_write_cmos_sensor(0x82f6, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x82f7, 0xff);
	OV5645MIPI_write_cmos_sensor(0x82f8, 0x33);
	OV5645MIPI_write_cmos_sensor(0x82f9, 0x95);
	OV5645MIPI_write_cmos_sensor(0x82fa, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x82fb, 0xfe);
	OV5645MIPI_write_cmos_sensor(0x82fc, 0xef);
	OV5645MIPI_write_cmos_sensor(0x82fd, 0x78);
	OV5645MIPI_write_cmos_sensor(0x82fe, 0x02);
	OV5645MIPI_write_cmos_sensor(0x82ff, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x8300, 0x33);
	OV5645MIPI_write_cmos_sensor(0x8301, 0xce);
	OV5645MIPI_write_cmos_sensor(0x8302, 0x33);
	OV5645MIPI_write_cmos_sensor(0x8303, 0xce);
	OV5645MIPI_write_cmos_sensor(0x8304, 0xd8);
	OV5645MIPI_write_cmos_sensor(0x8305, 0xf9);
	OV5645MIPI_write_cmos_sensor(0x8306, 0xff);
	OV5645MIPI_write_cmos_sensor(0x8307, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8308, 0x15);
	OV5645MIPI_write_cmos_sensor(0x8309, 0x2e);
	OV5645MIPI_write_cmos_sensor(0x830a, 0x78);
	OV5645MIPI_write_cmos_sensor(0x830b, 0xce);
	OV5645MIPI_write_cmos_sensor(0x830c, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x830d, 0xfd);
	OV5645MIPI_write_cmos_sensor(0x830e, 0x33);
	OV5645MIPI_write_cmos_sensor(0x830f, 0x95);
	OV5645MIPI_write_cmos_sensor(0x8310, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x8311, 0xfc);
	OV5645MIPI_write_cmos_sensor(0x8312, 0x08);
	OV5645MIPI_write_cmos_sensor(0x8313, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8314, 0xfb);
	OV5645MIPI_write_cmos_sensor(0x8315, 0x33);
	OV5645MIPI_write_cmos_sensor(0x8316, 0x95);
	OV5645MIPI_write_cmos_sensor(0x8317, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x8318, 0xfa);
	OV5645MIPI_write_cmos_sensor(0x8319, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x831a, 0xeb);
	OV5645MIPI_write_cmos_sensor(0x831b, 0x9d);
	OV5645MIPI_write_cmos_sensor(0x831c, 0xfd);
	OV5645MIPI_write_cmos_sensor(0x831d, 0xea);
	OV5645MIPI_write_cmos_sensor(0x831e, 0x9c);
	OV5645MIPI_write_cmos_sensor(0x831f, 0xfc);
	OV5645MIPI_write_cmos_sensor(0x8320, 0xd3);
	OV5645MIPI_write_cmos_sensor(0x8321, 0xed);
	OV5645MIPI_write_cmos_sensor(0x8322, 0x9f);
	OV5645MIPI_write_cmos_sensor(0x8323, 0xee);
	OV5645MIPI_write_cmos_sensor(0x8324, 0x64);
	OV5645MIPI_write_cmos_sensor(0x8325, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8326, 0xf8);
	OV5645MIPI_write_cmos_sensor(0x8327, 0xec);
	OV5645MIPI_write_cmos_sensor(0x8328, 0x64);
	OV5645MIPI_write_cmos_sensor(0x8329, 0x80);
	OV5645MIPI_write_cmos_sensor(0x832a, 0x98);
	OV5645MIPI_write_cmos_sensor(0x832b, 0x40);
	OV5645MIPI_write_cmos_sensor(0x832c, 0x02);
	OV5645MIPI_write_cmos_sensor(0x832d, 0x80);
	OV5645MIPI_write_cmos_sensor(0x832e, 0x01);
	OV5645MIPI_write_cmos_sensor(0x832f, 0xd3);
	OV5645MIPI_write_cmos_sensor(0x8330, 0x92);
	OV5645MIPI_write_cmos_sensor(0x8331, 0x3a);
	OV5645MIPI_write_cmos_sensor(0x8332, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x8333, 0x1e);
	OV5645MIPI_write_cmos_sensor(0x8334, 0x64);
	OV5645MIPI_write_cmos_sensor(0x8335, 0x01);
	OV5645MIPI_write_cmos_sensor(0x8336, 0x70);
	OV5645MIPI_write_cmos_sensor(0x8337, 0x21);
	OV5645MIPI_write_cmos_sensor(0x8338, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8339, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x833a, 0x72);
	OV5645MIPI_write_cmos_sensor(0x833b, 0x30);
	OV5645MIPI_write_cmos_sensor(0x833c, 0x3a);
	OV5645MIPI_write_cmos_sensor(0x833d, 0x05);
	OV5645MIPI_write_cmos_sensor(0x833e, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x833f, 0xa2);
	OV5645MIPI_write_cmos_sensor(0x8340, 0xe7);
	OV5645MIPI_write_cmos_sensor(0x8341, 0x13);
	OV5645MIPI_write_cmos_sensor(0x8342, 0xf6);
	OV5645MIPI_write_cmos_sensor(0x8343, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8344, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x8345, 0x16);
	OV5645MIPI_write_cmos_sensor(0x8346, 0x40);
	OV5645MIPI_write_cmos_sensor(0x8347, 0x06);
	OV5645MIPI_write_cmos_sensor(0x8348, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8349, 0xce);
	OV5645MIPI_write_cmos_sensor(0x834a, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x834b, 0xff);
	OV5645MIPI_write_cmos_sensor(0x834c, 0x80);
	OV5645MIPI_write_cmos_sensor(0x834d, 0x04);
	OV5645MIPI_write_cmos_sensor(0x834e, 0x78);
	OV5645MIPI_write_cmos_sensor(0x834f, 0xcf);
	OV5645MIPI_write_cmos_sensor(0x8350, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8351, 0xff);
	OV5645MIPI_write_cmos_sensor(0x8352, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8353, 0xc4);
	OV5645MIPI_write_cmos_sensor(0x8354, 0xa6);
	OV5645MIPI_write_cmos_sensor(0x8355, 0x07);
	OV5645MIPI_write_cmos_sensor(0x8356, 0x02);
	OV5645MIPI_write_cmos_sensor(0x8357, 0x04);
	OV5645MIPI_write_cmos_sensor(0x8358, 0x8a);
	OV5645MIPI_write_cmos_sensor(0x8359, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x835a, 0x1e);
	OV5645MIPI_write_cmos_sensor(0x835b, 0x64);
	OV5645MIPI_write_cmos_sensor(0x835c, 0x02);
	OV5645MIPI_write_cmos_sensor(0x835d, 0x70);
	OV5645MIPI_write_cmos_sensor(0x835e, 0x21);
	OV5645MIPI_write_cmos_sensor(0x835f, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8360, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x8361, 0x72);
	OV5645MIPI_write_cmos_sensor(0x8362, 0x30);
	OV5645MIPI_write_cmos_sensor(0x8363, 0x3a);
	OV5645MIPI_write_cmos_sensor(0x8364, 0x05);
	OV5645MIPI_write_cmos_sensor(0x8365, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8366, 0xa2);
	OV5645MIPI_write_cmos_sensor(0x8367, 0xe7);
	OV5645MIPI_write_cmos_sensor(0x8368, 0x13);
	OV5645MIPI_write_cmos_sensor(0x8369, 0xf6);
	OV5645MIPI_write_cmos_sensor(0x836a, 0x12);
	OV5645MIPI_write_cmos_sensor(0x836b, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x836c, 0x16);
	OV5645MIPI_write_cmos_sensor(0x836d, 0x40);
	OV5645MIPI_write_cmos_sensor(0x836e, 0x06);
	OV5645MIPI_write_cmos_sensor(0x836f, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8370, 0xce);
	OV5645MIPI_write_cmos_sensor(0x8371, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8372, 0xff);
	OV5645MIPI_write_cmos_sensor(0x8373, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8374, 0x04);
	OV5645MIPI_write_cmos_sensor(0x8375, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8376, 0xcf);
	OV5645MIPI_write_cmos_sensor(0x8377, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8378, 0xff);
	OV5645MIPI_write_cmos_sensor(0x8379, 0x78);
	OV5645MIPI_write_cmos_sensor(0x837a, 0xc4);
	OV5645MIPI_write_cmos_sensor(0x837b, 0xa6);
	OV5645MIPI_write_cmos_sensor(0x837c, 0x07);
	OV5645MIPI_write_cmos_sensor(0x837d, 0x02);
	OV5645MIPI_write_cmos_sensor(0x837e, 0x04);
	OV5645MIPI_write_cmos_sensor(0x837f, 0x8a);
	OV5645MIPI_write_cmos_sensor(0x8380, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x8381, 0x1e);
	OV5645MIPI_write_cmos_sensor(0x8382, 0x64);
	OV5645MIPI_write_cmos_sensor(0x8383, 0x03);
	OV5645MIPI_write_cmos_sensor(0x8384, 0x70);
	OV5645MIPI_write_cmos_sensor(0x8385, 0x21);
	OV5645MIPI_write_cmos_sensor(0x8386, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8387, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x8388, 0x72);
	OV5645MIPI_write_cmos_sensor(0x8389, 0x30);
	OV5645MIPI_write_cmos_sensor(0x838a, 0x3a);
	OV5645MIPI_write_cmos_sensor(0x838b, 0x05);
	OV5645MIPI_write_cmos_sensor(0x838c, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x838d, 0xa2);
	OV5645MIPI_write_cmos_sensor(0x838e, 0xe7);
	OV5645MIPI_write_cmos_sensor(0x838f, 0x13);
	OV5645MIPI_write_cmos_sensor(0x8390, 0xf6);
	OV5645MIPI_write_cmos_sensor(0x8391, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8392, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x8393, 0x16);
	OV5645MIPI_write_cmos_sensor(0x8394, 0x40);
	OV5645MIPI_write_cmos_sensor(0x8395, 0x06);
	OV5645MIPI_write_cmos_sensor(0x8396, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8397, 0xce);
	OV5645MIPI_write_cmos_sensor(0x8398, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8399, 0xff);
	OV5645MIPI_write_cmos_sensor(0x839a, 0x80);
	OV5645MIPI_write_cmos_sensor(0x839b, 0x04);
	OV5645MIPI_write_cmos_sensor(0x839c, 0x78);
	OV5645MIPI_write_cmos_sensor(0x839d, 0xcf);
	OV5645MIPI_write_cmos_sensor(0x839e, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x839f, 0xff);
	OV5645MIPI_write_cmos_sensor(0x83a0, 0x78);
	OV5645MIPI_write_cmos_sensor(0x83a1, 0xc4);
	OV5645MIPI_write_cmos_sensor(0x83a2, 0xa6);
	OV5645MIPI_write_cmos_sensor(0x83a3, 0x07);
	OV5645MIPI_write_cmos_sensor(0x83a4, 0x02);
	OV5645MIPI_write_cmos_sensor(0x83a5, 0x04);
	OV5645MIPI_write_cmos_sensor(0x83a6, 0x8a);
	OV5645MIPI_write_cmos_sensor(0x83a7, 0x78);
	OV5645MIPI_write_cmos_sensor(0x83a8, 0xc0);
	OV5645MIPI_write_cmos_sensor(0x83a9, 0x76);
	OV5645MIPI_write_cmos_sensor(0x83aa, 0x01);
	OV5645MIPI_write_cmos_sensor(0x83ab, 0x12);
	OV5645MIPI_write_cmos_sensor(0x83ac, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x83ad, 0xa0);
	OV5645MIPI_write_cmos_sensor(0x83ae, 0x02);
	OV5645MIPI_write_cmos_sensor(0x83af, 0x04);
	OV5645MIPI_write_cmos_sensor(0x83b0, 0x9c);
	OV5645MIPI_write_cmos_sensor(0x83b1, 0x79);
	OV5645MIPI_write_cmos_sensor(0x83b2, 0xc2);
	OV5645MIPI_write_cmos_sensor(0x83b3, 0xe7);
	OV5645MIPI_write_cmos_sensor(0x83b4, 0x78);
	OV5645MIPI_write_cmos_sensor(0x83b5, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x83b6, 0x66);
	OV5645MIPI_write_cmos_sensor(0x83b7, 0x60);
	OV5645MIPI_write_cmos_sensor(0x83b8, 0x03);
	OV5645MIPI_write_cmos_sensor(0x83b9, 0x02);
	OV5645MIPI_write_cmos_sensor(0x83ba, 0x04);
	OV5645MIPI_write_cmos_sensor(0x83bb, 0x9f);
	OV5645MIPI_write_cmos_sensor(0x83bc, 0x78);
	OV5645MIPI_write_cmos_sensor(0x83bd, 0xd1);
	OV5645MIPI_write_cmos_sensor(0x83be, 0x06);
	OV5645MIPI_write_cmos_sensor(0x83bf, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x83c0, 0x12);
	OV5645MIPI_write_cmos_sensor(0x83c1, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x83c2, 0x17);
	OV5645MIPI_write_cmos_sensor(0x83c3, 0x50);
	OV5645MIPI_write_cmos_sensor(0x83c4, 0x16);
	OV5645MIPI_write_cmos_sensor(0x83c5, 0x78);
	OV5645MIPI_write_cmos_sensor(0x83c6, 0xc9);
	OV5645MIPI_write_cmos_sensor(0x83c7, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x83c8, 0x24);
	OV5645MIPI_write_cmos_sensor(0x83c9, 0x9f);
	OV5645MIPI_write_cmos_sensor(0x83ca, 0x12);
	OV5645MIPI_write_cmos_sensor(0x83cb, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x83cc, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x83cd, 0x50);
	OV5645MIPI_write_cmos_sensor(0x83ce, 0x20);
	OV5645MIPI_write_cmos_sensor(0x83cf, 0x78);
	OV5645MIPI_write_cmos_sensor(0x83d0, 0xc9);
	OV5645MIPI_write_cmos_sensor(0x83d1, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x83d2, 0x24);
	OV5645MIPI_write_cmos_sensor(0x83d3, 0x9f);
	OV5645MIPI_write_cmos_sensor(0x83d4, 0xf8);
	OV5645MIPI_write_cmos_sensor(0x83d5, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x83d6, 0x78);
	OV5645MIPI_write_cmos_sensor(0x83d7, 0xce);
	OV5645MIPI_write_cmos_sensor(0x83d8, 0xf6);
	OV5645MIPI_write_cmos_sensor(0x83d9, 0x80);
	OV5645MIPI_write_cmos_sensor(0x83da, 0x14);
	OV5645MIPI_write_cmos_sensor(0x83db, 0x78);
	OV5645MIPI_write_cmos_sensor(0x83dc, 0xc9);
	OV5645MIPI_write_cmos_sensor(0x83dd, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x83de, 0x24);
	OV5645MIPI_write_cmos_sensor(0x83df, 0x9f);
	OV5645MIPI_write_cmos_sensor(0x83e0, 0x12);
	OV5645MIPI_write_cmos_sensor(0x83e1, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x83e2, 0x07);
	OV5645MIPI_write_cmos_sensor(0x83e3, 0x40);
	OV5645MIPI_write_cmos_sensor(0x83e4, 0x0a);
	OV5645MIPI_write_cmos_sensor(0x83e5, 0x78);
	OV5645MIPI_write_cmos_sensor(0x83e6, 0xc9);
	OV5645MIPI_write_cmos_sensor(0x83e7, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x83e8, 0x24);
	OV5645MIPI_write_cmos_sensor(0x83e9, 0x9f);
	OV5645MIPI_write_cmos_sensor(0x83ea, 0xf8);
	OV5645MIPI_write_cmos_sensor(0x83eb, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x83ec, 0x78);
	OV5645MIPI_write_cmos_sensor(0x83ed, 0xcf);
	OV5645MIPI_write_cmos_sensor(0x83ee, 0xf6);
	OV5645MIPI_write_cmos_sensor(0x83ef, 0x12);
	OV5645MIPI_write_cmos_sensor(0x83f0, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x83f1, 0x50);
	OV5645MIPI_write_cmos_sensor(0x83f2, 0x20);
	OV5645MIPI_write_cmos_sensor(0x83f3, 0x38);
	OV5645MIPI_write_cmos_sensor(0x83f4, 0x2e);
	OV5645MIPI_write_cmos_sensor(0x83f5, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x83f6, 0x08);
	OV5645MIPI_write_cmos_sensor(0x83f7, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x83f8, 0x64);
	OV5645MIPI_write_cmos_sensor(0x83f9, 0x80);
	OV5645MIPI_write_cmos_sensor(0x83fa, 0x94);
	OV5645MIPI_write_cmos_sensor(0x83fb, 0x82);
	OV5645MIPI_write_cmos_sensor(0x83fc, 0x50);
	OV5645MIPI_write_cmos_sensor(0x83fd, 0x1b);
	OV5645MIPI_write_cmos_sensor(0x83fe, 0x12);
	OV5645MIPI_write_cmos_sensor(0x83ff, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x8400, 0x72);
	OV5645MIPI_write_cmos_sensor(0x8401, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8402, 0x64);
	OV5645MIPI_write_cmos_sensor(0x8403, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8404, 0x94);
	OV5645MIPI_write_cmos_sensor(0x8405, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8406, 0x40);
	OV5645MIPI_write_cmos_sensor(0x8407, 0x06);
	OV5645MIPI_write_cmos_sensor(0x8408, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8409, 0xce);
	OV5645MIPI_write_cmos_sensor(0x840a, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x840b, 0xff);
	OV5645MIPI_write_cmos_sensor(0x840c, 0x80);
	OV5645MIPI_write_cmos_sensor(0x840d, 0x04);
	OV5645MIPI_write_cmos_sensor(0x840e, 0x78);
	OV5645MIPI_write_cmos_sensor(0x840f, 0xcf);
	OV5645MIPI_write_cmos_sensor(0x8410, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8411, 0xff);
	OV5645MIPI_write_cmos_sensor(0x8412, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8413, 0xc4);
	OV5645MIPI_write_cmos_sensor(0x8414, 0xa6);
	OV5645MIPI_write_cmos_sensor(0x8415, 0x07);
	OV5645MIPI_write_cmos_sensor(0x8416, 0x02);
	OV5645MIPI_write_cmos_sensor(0x8417, 0x04);
	OV5645MIPI_write_cmos_sensor(0x8418, 0x8a);
	OV5645MIPI_write_cmos_sensor(0x8419, 0x12);
	OV5645MIPI_write_cmos_sensor(0x841a, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x841b, 0xa0);
	OV5645MIPI_write_cmos_sensor(0x841c, 0x78);
	OV5645MIPI_write_cmos_sensor(0x841d, 0xc0);
	OV5645MIPI_write_cmos_sensor(0x841e, 0x76);
	OV5645MIPI_write_cmos_sensor(0x841f, 0x01);
	OV5645MIPI_write_cmos_sensor(0x8420, 0x02);
	OV5645MIPI_write_cmos_sensor(0x8421, 0x04);
	OV5645MIPI_write_cmos_sensor(0x8422, 0x9c);
	OV5645MIPI_write_cmos_sensor(0x8423, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x8424, 0x1e);
	OV5645MIPI_write_cmos_sensor(0x8425, 0x64);
	OV5645MIPI_write_cmos_sensor(0x8426, 0x01);
	OV5645MIPI_write_cmos_sensor(0x8427, 0x70);
	OV5645MIPI_write_cmos_sensor(0x8428, 0x1d);
	OV5645MIPI_write_cmos_sensor(0x8429, 0x78);
	OV5645MIPI_write_cmos_sensor(0x842a, 0xc1);
	OV5645MIPI_write_cmos_sensor(0x842b, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x842c, 0xf4);
	OV5645MIPI_write_cmos_sensor(0x842d, 0x04);
	OV5645MIPI_write_cmos_sensor(0x842e, 0x12);
	OV5645MIPI_write_cmos_sensor(0x842f, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x8430, 0x25);
	OV5645MIPI_write_cmos_sensor(0x8431, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8432, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x8433, 0x81);
	OV5645MIPI_write_cmos_sensor(0x8434, 0x40);
	OV5645MIPI_write_cmos_sensor(0x8435, 0x06);
	OV5645MIPI_write_cmos_sensor(0x8436, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8437, 0xce);
	OV5645MIPI_write_cmos_sensor(0x8438, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8439, 0xff);
	OV5645MIPI_write_cmos_sensor(0x843a, 0x80);
	OV5645MIPI_write_cmos_sensor(0x843b, 0x04);
	OV5645MIPI_write_cmos_sensor(0x843c, 0x78);
	OV5645MIPI_write_cmos_sensor(0x843d, 0xcf);
	OV5645MIPI_write_cmos_sensor(0x843e, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x843f, 0xff);
	OV5645MIPI_write_cmos_sensor(0x8440, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8441, 0xc4);
	OV5645MIPI_write_cmos_sensor(0x8442, 0xa6);
	OV5645MIPI_write_cmos_sensor(0x8443, 0x07);
	OV5645MIPI_write_cmos_sensor(0x8444, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8445, 0x44);
	OV5645MIPI_write_cmos_sensor(0x8446, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x8447, 0x1e);
	OV5645MIPI_write_cmos_sensor(0x8448, 0x64);
	OV5645MIPI_write_cmos_sensor(0x8449, 0x02);
	OV5645MIPI_write_cmos_sensor(0x844a, 0x70);
	OV5645MIPI_write_cmos_sensor(0x844b, 0x1d);
	OV5645MIPI_write_cmos_sensor(0x844c, 0x78);
	OV5645MIPI_write_cmos_sensor(0x844d, 0xc1);
	OV5645MIPI_write_cmos_sensor(0x844e, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x844f, 0xf4);
	OV5645MIPI_write_cmos_sensor(0x8450, 0x04);
	OV5645MIPI_write_cmos_sensor(0x8451, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8452, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x8453, 0x25);
	OV5645MIPI_write_cmos_sensor(0x8454, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8455, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x8456, 0x81);
	OV5645MIPI_write_cmos_sensor(0x8457, 0x40);
	OV5645MIPI_write_cmos_sensor(0x8458, 0x06);
	OV5645MIPI_write_cmos_sensor(0x8459, 0x78);
	OV5645MIPI_write_cmos_sensor(0x845a, 0xce);
	OV5645MIPI_write_cmos_sensor(0x845b, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x845c, 0xff);
	OV5645MIPI_write_cmos_sensor(0x845d, 0x80);
	OV5645MIPI_write_cmos_sensor(0x845e, 0x04);
	OV5645MIPI_write_cmos_sensor(0x845f, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8460, 0xcf);
	OV5645MIPI_write_cmos_sensor(0x8461, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8462, 0xff);
	OV5645MIPI_write_cmos_sensor(0x8463, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8464, 0xc4);
	OV5645MIPI_write_cmos_sensor(0x8465, 0xa6);
	OV5645MIPI_write_cmos_sensor(0x8466, 0x07);
	OV5645MIPI_write_cmos_sensor(0x8467, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8468, 0x21);
	OV5645MIPI_write_cmos_sensor(0x8469, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x846a, 0x1e);
	OV5645MIPI_write_cmos_sensor(0x846b, 0x64);
	OV5645MIPI_write_cmos_sensor(0x846c, 0x03);
	OV5645MIPI_write_cmos_sensor(0x846d, 0x70);
	OV5645MIPI_write_cmos_sensor(0x846e, 0x26);
	OV5645MIPI_write_cmos_sensor(0x846f, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8470, 0xc1);
	OV5645MIPI_write_cmos_sensor(0x8471, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8472, 0xf4);
	OV5645MIPI_write_cmos_sensor(0x8473, 0x04);
	OV5645MIPI_write_cmos_sensor(0x8474, 0xa2);
	OV5645MIPI_write_cmos_sensor(0x8475, 0xe7);
	OV5645MIPI_write_cmos_sensor(0x8476, 0x13);
	OV5645MIPI_write_cmos_sensor(0x8477, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8478, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x8479, 0x81);
	OV5645MIPI_write_cmos_sensor(0x847a, 0x40);
	OV5645MIPI_write_cmos_sensor(0x847b, 0x06);
	OV5645MIPI_write_cmos_sensor(0x847c, 0x78);
	OV5645MIPI_write_cmos_sensor(0x847d, 0xce);
	OV5645MIPI_write_cmos_sensor(0x847e, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x847f, 0xff);
	OV5645MIPI_write_cmos_sensor(0x8480, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8481, 0x04);
	OV5645MIPI_write_cmos_sensor(0x8482, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8483, 0xcf);
	OV5645MIPI_write_cmos_sensor(0x8484, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8485, 0xff);
	OV5645MIPI_write_cmos_sensor(0x8486, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8487, 0xc4);
	OV5645MIPI_write_cmos_sensor(0x8488, 0xa6);
	OV5645MIPI_write_cmos_sensor(0x8489, 0x07);
	OV5645MIPI_write_cmos_sensor(0x848a, 0x78);
	OV5645MIPI_write_cmos_sensor(0x848b, 0xc6);
	OV5645MIPI_write_cmos_sensor(0x848c, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x848d, 0x08);
	OV5645MIPI_write_cmos_sensor(0x848e, 0xf6);
	OV5645MIPI_write_cmos_sensor(0x848f, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8490, 0xc0);
	OV5645MIPI_write_cmos_sensor(0x8491, 0x76);
	OV5645MIPI_write_cmos_sensor(0x8492, 0x01);
	OV5645MIPI_write_cmos_sensor(0x8493, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8494, 0x0a);
	OV5645MIPI_write_cmos_sensor(0x8495, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8496, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x8497, 0xa0);
	OV5645MIPI_write_cmos_sensor(0x8498, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8499, 0xc0);
	OV5645MIPI_write_cmos_sensor(0x849a, 0x76);
	OV5645MIPI_write_cmos_sensor(0x849b, 0x01);
	OV5645MIPI_write_cmos_sensor(0x849c, 0x75);
	OV5645MIPI_write_cmos_sensor(0x849d, 0x1e);
	OV5645MIPI_write_cmos_sensor(0x849e, 0x06);
	OV5645MIPI_write_cmos_sensor(0x849f, 0x78);
	OV5645MIPI_write_cmos_sensor(0x84a0, 0xc4);
	OV5645MIPI_write_cmos_sensor(0x84a1, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x84a2, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x84a3, 0x4f);
	OV5645MIPI_write_cmos_sensor(0x84a4, 0x12);
	OV5645MIPI_write_cmos_sensor(0x84a5, 0x12);
	OV5645MIPI_write_cmos_sensor(0x84a6, 0xaf);
	OV5645MIPI_write_cmos_sensor(0x84a7, 0x22);
	OV5645MIPI_write_cmos_sensor(0x84a8, 0x30);
	OV5645MIPI_write_cmos_sensor(0x84a9, 0x01);
	OV5645MIPI_write_cmos_sensor(0x84aa, 0x03);
	OV5645MIPI_write_cmos_sensor(0x84ab, 0x02);
	OV5645MIPI_write_cmos_sensor(0x84ac, 0x08);
	OV5645MIPI_write_cmos_sensor(0x84ad, 0x5f);
	OV5645MIPI_write_cmos_sensor(0x84ae, 0x30);
	OV5645MIPI_write_cmos_sensor(0x84af, 0x02);
	OV5645MIPI_write_cmos_sensor(0x84b0, 0x03);
	OV5645MIPI_write_cmos_sensor(0x84b1, 0x02);
	OV5645MIPI_write_cmos_sensor(0x84b2, 0x08);
	OV5645MIPI_write_cmos_sensor(0x84b3, 0x5f);
	OV5645MIPI_write_cmos_sensor(0x84b4, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x84b5, 0x1e);
	OV5645MIPI_write_cmos_sensor(0x84b6, 0x60);
	OV5645MIPI_write_cmos_sensor(0x84b7, 0x03);
	OV5645MIPI_write_cmos_sensor(0x84b8, 0x02);
	OV5645MIPI_write_cmos_sensor(0x84b9, 0x05);
	OV5645MIPI_write_cmos_sensor(0x84ba, 0x3b);
	OV5645MIPI_write_cmos_sensor(0x84bb, 0x75);
	OV5645MIPI_write_cmos_sensor(0x84bc, 0x1d);
	OV5645MIPI_write_cmos_sensor(0x84bd, 0x20);
	OV5645MIPI_write_cmos_sensor(0x84be, 0xd2);
	OV5645MIPI_write_cmos_sensor(0x84bf, 0x36);
	OV5645MIPI_write_cmos_sensor(0x84c0, 0xd3);
	OV5645MIPI_write_cmos_sensor(0x84c1, 0x78);
	OV5645MIPI_write_cmos_sensor(0x84c2, 0x53);
	OV5645MIPI_write_cmos_sensor(0x84c3, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x84c4, 0x94);
	OV5645MIPI_write_cmos_sensor(0x84c5, 0x00);
	OV5645MIPI_write_cmos_sensor(0x84c6, 0x18);
	OV5645MIPI_write_cmos_sensor(0x84c7, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x84c8, 0x94);
	OV5645MIPI_write_cmos_sensor(0x84c9, 0x00);
	OV5645MIPI_write_cmos_sensor(0x84ca, 0x40);
	OV5645MIPI_write_cmos_sensor(0x84cb, 0x07);
	OV5645MIPI_write_cmos_sensor(0x84cc, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x84cd, 0xfe);
	OV5645MIPI_write_cmos_sensor(0x84ce, 0x08);
	OV5645MIPI_write_cmos_sensor(0x84cf, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x84d0, 0xff);
	OV5645MIPI_write_cmos_sensor(0x84d1, 0x80);
	OV5645MIPI_write_cmos_sensor(0x84d2, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x84d3, 0x90);
	OV5645MIPI_write_cmos_sensor(0x84d4, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x84d5, 0x8d);
	OV5645MIPI_write_cmos_sensor(0x84d6, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x84d7, 0x93);
	OV5645MIPI_write_cmos_sensor(0x84d8, 0x25);
	OV5645MIPI_write_cmos_sensor(0x84d9, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x84da, 0x25);
	OV5645MIPI_write_cmos_sensor(0x84db, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x84dc, 0x24);
	OV5645MIPI_write_cmos_sensor(0x84dd, 0x2a);
	OV5645MIPI_write_cmos_sensor(0x84de, 0x12);
	OV5645MIPI_write_cmos_sensor(0x84df, 0x11);
	OV5645MIPI_write_cmos_sensor(0x84e0, 0x8e);
	OV5645MIPI_write_cmos_sensor(0x84e1, 0x78);
	OV5645MIPI_write_cmos_sensor(0x84e2, 0x52);
	OV5645MIPI_write_cmos_sensor(0x84e3, 0xa6);
	OV5645MIPI_write_cmos_sensor(0x84e4, 0x06);
	OV5645MIPI_write_cmos_sensor(0x84e5, 0x08);
	OV5645MIPI_write_cmos_sensor(0x84e6, 0xa6);
	OV5645MIPI_write_cmos_sensor(0x84e7, 0x07);
	OV5645MIPI_write_cmos_sensor(0x84e8, 0xd3);
	OV5645MIPI_write_cmos_sensor(0x84e9, 0x78);
	OV5645MIPI_write_cmos_sensor(0x84ea, 0x55);
	OV5645MIPI_write_cmos_sensor(0x84eb, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x84ec, 0x94);
	OV5645MIPI_write_cmos_sensor(0x84ed, 0x00);
	OV5645MIPI_write_cmos_sensor(0x84ee, 0x18);
	OV5645MIPI_write_cmos_sensor(0x84ef, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x84f0, 0x94);
	OV5645MIPI_write_cmos_sensor(0x84f1, 0x00);
	OV5645MIPI_write_cmos_sensor(0x84f2, 0x40);
	OV5645MIPI_write_cmos_sensor(0x84f3, 0x07);
	OV5645MIPI_write_cmos_sensor(0x84f4, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x84f5, 0xfe);
	OV5645MIPI_write_cmos_sensor(0x84f6, 0x08);
	OV5645MIPI_write_cmos_sensor(0x84f7, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x84f8, 0xff);
	OV5645MIPI_write_cmos_sensor(0x84f9, 0x80);
	OV5645MIPI_write_cmos_sensor(0x84fa, 0x08);
	OV5645MIPI_write_cmos_sensor(0x84fb, 0x90);
	OV5645MIPI_write_cmos_sensor(0x84fc, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x84fd, 0x8d);
	OV5645MIPI_write_cmos_sensor(0x84fe, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x84ff, 0x93);
	OV5645MIPI_write_cmos_sensor(0x8500, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8501, 0x11);
	OV5645MIPI_write_cmos_sensor(0x8502, 0x88);
	OV5645MIPI_write_cmos_sensor(0x8503, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8504, 0x54);
	OV5645MIPI_write_cmos_sensor(0x8505, 0xa6);
	OV5645MIPI_write_cmos_sensor(0x8506, 0x06);
	OV5645MIPI_write_cmos_sensor(0x8507, 0x08);
	OV5645MIPI_write_cmos_sensor(0x8508, 0xa6);
	OV5645MIPI_write_cmos_sensor(0x8509, 0x07);
	OV5645MIPI_write_cmos_sensor(0x850a, 0x12);
	OV5645MIPI_write_cmos_sensor(0x850b, 0x11);
	OV5645MIPI_write_cmos_sensor(0x850c, 0xd8);
	OV5645MIPI_write_cmos_sensor(0x850d, 0x12);
	OV5645MIPI_write_cmos_sensor(0x850e, 0x11);
	OV5645MIPI_write_cmos_sensor(0x850f, 0xa9);
	OV5645MIPI_write_cmos_sensor(0x8510, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8511, 0x5a);
	OV5645MIPI_write_cmos_sensor(0x8512, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8513, 0x11);
	OV5645MIPI_write_cmos_sensor(0x8514, 0x82);
	OV5645MIPI_write_cmos_sensor(0x8515, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8516, 0x5c);
	OV5645MIPI_write_cmos_sensor(0x8517, 0xa6);
	OV5645MIPI_write_cmos_sensor(0x8518, 0x06);
	OV5645MIPI_write_cmos_sensor(0x8519, 0x08);
	OV5645MIPI_write_cmos_sensor(0x851a, 0xa6);
	OV5645MIPI_write_cmos_sensor(0x851b, 0x07);
	OV5645MIPI_write_cmos_sensor(0x851c, 0x12);
	OV5645MIPI_write_cmos_sensor(0x851d, 0x11);
	OV5645MIPI_write_cmos_sensor(0x851e, 0xd8);
	OV5645MIPI_write_cmos_sensor(0x851f, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8520, 0xad);
	OV5645MIPI_write_cmos_sensor(0x8521, 0xa6);
	OV5645MIPI_write_cmos_sensor(0x8522, 0x33);
	OV5645MIPI_write_cmos_sensor(0x8523, 0x08);
	OV5645MIPI_write_cmos_sensor(0x8524, 0xa6);
	OV5645MIPI_write_cmos_sensor(0x8525, 0x33);
	OV5645MIPI_write_cmos_sensor(0x8526, 0x08);
	OV5645MIPI_write_cmos_sensor(0x8527, 0xa6);
	OV5645MIPI_write_cmos_sensor(0x8528, 0x35);
	OV5645MIPI_write_cmos_sensor(0x8529, 0x78);
	OV5645MIPI_write_cmos_sensor(0x852a, 0xb3);
	OV5645MIPI_write_cmos_sensor(0x852b, 0xa6);
	OV5645MIPI_write_cmos_sensor(0x852c, 0x33);
	OV5645MIPI_write_cmos_sensor(0x852d, 0x08);
	OV5645MIPI_write_cmos_sensor(0x852e, 0xa6);
	OV5645MIPI_write_cmos_sensor(0x852f, 0x33);
	OV5645MIPI_write_cmos_sensor(0x8530, 0x08);
	OV5645MIPI_write_cmos_sensor(0x8531, 0xa6);
	OV5645MIPI_write_cmos_sensor(0x8532, 0x35);
	OV5645MIPI_write_cmos_sensor(0x8533, 0x75);
	OV5645MIPI_write_cmos_sensor(0x8534, 0x1e);
	OV5645MIPI_write_cmos_sensor(0x8535, 0x01);
	OV5645MIPI_write_cmos_sensor(0x8536, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8537, 0xaa);
	OV5645MIPI_write_cmos_sensor(0x8538, 0x76);
	OV5645MIPI_write_cmos_sensor(0x8539, 0x01);
	OV5645MIPI_write_cmos_sensor(0x853a, 0x22);
	OV5645MIPI_write_cmos_sensor(0x853b, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x853c, 0x1e);
	OV5645MIPI_write_cmos_sensor(0x853d, 0xb4);
	OV5645MIPI_write_cmos_sensor(0x853e, 0x05);
	OV5645MIPI_write_cmos_sensor(0x853f, 0x10);
	OV5645MIPI_write_cmos_sensor(0x8540, 0xd2);
	OV5645MIPI_write_cmos_sensor(0x8541, 0x01);
	OV5645MIPI_write_cmos_sensor(0x8542, 0xc2);
	OV5645MIPI_write_cmos_sensor(0x8543, 0x02);
	OV5645MIPI_write_cmos_sensor(0x8544, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x8545, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x8546, 0x1e);
	OV5645MIPI_write_cmos_sensor(0x8547, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x8548, 0x1d);
	OV5645MIPI_write_cmos_sensor(0x8549, 0xd2);
	OV5645MIPI_write_cmos_sensor(0x854a, 0x36);
	OV5645MIPI_write_cmos_sensor(0x854b, 0xd2);
	OV5645MIPI_write_cmos_sensor(0x854c, 0x34);
	OV5645MIPI_write_cmos_sensor(0x854d, 0xd2);
	OV5645MIPI_write_cmos_sensor(0x854e, 0x37);
	OV5645MIPI_write_cmos_sensor(0x854f, 0x22);
	OV5645MIPI_write_cmos_sensor(0x8550, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8551, 0x11);
	OV5645MIPI_write_cmos_sensor(0x8552, 0xc7);
	OV5645MIPI_write_cmos_sensor(0x8553, 0x24);
	OV5645MIPI_write_cmos_sensor(0x8554, 0xb3);
	OV5645MIPI_write_cmos_sensor(0x8555, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8556, 0x11);
	OV5645MIPI_write_cmos_sensor(0x8557, 0xc4);
	OV5645MIPI_write_cmos_sensor(0x8558, 0x24);
	OV5645MIPI_write_cmos_sensor(0x8559, 0xb4);
	OV5645MIPI_write_cmos_sensor(0x855a, 0x12);
	OV5645MIPI_write_cmos_sensor(0x855b, 0x11);
	OV5645MIPI_write_cmos_sensor(0x855c, 0xc4);
	OV5645MIPI_write_cmos_sensor(0x855d, 0x24);
	OV5645MIPI_write_cmos_sensor(0x855e, 0xb5);
	OV5645MIPI_write_cmos_sensor(0x855f, 0xf8);
	OV5645MIPI_write_cmos_sensor(0x8560, 0xa6);
	OV5645MIPI_write_cmos_sensor(0x8561, 0x35);
	OV5645MIPI_write_cmos_sensor(0x8562, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8563, 0x11);
	OV5645MIPI_write_cmos_sensor(0x8564, 0xa9);
	OV5645MIPI_write_cmos_sensor(0x8565, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8566, 0x11);
	OV5645MIPI_write_cmos_sensor(0x8567, 0xbc);
	OV5645MIPI_write_cmos_sensor(0x8568, 0x24);
	OV5645MIPI_write_cmos_sensor(0x8569, 0x5a);
	OV5645MIPI_write_cmos_sensor(0x856a, 0xf8);
	OV5645MIPI_write_cmos_sensor(0x856b, 0x12);
	OV5645MIPI_write_cmos_sensor(0x856c, 0x11);
	OV5645MIPI_write_cmos_sensor(0x856d, 0x82);
	OV5645MIPI_write_cmos_sensor(0x856e, 0x12);
	OV5645MIPI_write_cmos_sensor(0x856f, 0x11);
	OV5645MIPI_write_cmos_sensor(0x8570, 0xbc);
	OV5645MIPI_write_cmos_sensor(0x8571, 0x24);
	OV5645MIPI_write_cmos_sensor(0x8572, 0x5c);
	OV5645MIPI_write_cmos_sensor(0x8573, 0xf8);
	OV5645MIPI_write_cmos_sensor(0x8574, 0xa6);
	OV5645MIPI_write_cmos_sensor(0x8575, 0x06);
	OV5645MIPI_write_cmos_sensor(0x8576, 0x08);
	OV5645MIPI_write_cmos_sensor(0x8577, 0xa6);
	OV5645MIPI_write_cmos_sensor(0x8578, 0x07);
	OV5645MIPI_write_cmos_sensor(0x8579, 0x12);
	OV5645MIPI_write_cmos_sensor(0x857a, 0x11);
	OV5645MIPI_write_cmos_sensor(0x857b, 0xbc);
	OV5645MIPI_write_cmos_sensor(0x857c, 0x24);
	OV5645MIPI_write_cmos_sensor(0x857d, 0x5e);
	OV5645MIPI_write_cmos_sensor(0x857e, 0xf8);
	OV5645MIPI_write_cmos_sensor(0x857f, 0xa6);
	OV5645MIPI_write_cmos_sensor(0x8580, 0x2a);
	OV5645MIPI_write_cmos_sensor(0x8581, 0x08);
	OV5645MIPI_write_cmos_sensor(0x8582, 0xa6);
	OV5645MIPI_write_cmos_sensor(0x8583, 0x2b);
	OV5645MIPI_write_cmos_sensor(0x8584, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8585, 0x11);
	OV5645MIPI_write_cmos_sensor(0x8586, 0xbc);
	OV5645MIPI_write_cmos_sensor(0x8587, 0x24);
	OV5645MIPI_write_cmos_sensor(0x8588, 0x60);
	OV5645MIPI_write_cmos_sensor(0x8589, 0xf8);
	OV5645MIPI_write_cmos_sensor(0x858a, 0xa6);
	OV5645MIPI_write_cmos_sensor(0x858b, 0x2c);
	OV5645MIPI_write_cmos_sensor(0x858c, 0x08);
	OV5645MIPI_write_cmos_sensor(0x858d, 0xa6);
	OV5645MIPI_write_cmos_sensor(0x858e, 0x2d);
	OV5645MIPI_write_cmos_sensor(0x858f, 0x90);
	OV5645MIPI_write_cmos_sensor(0x8590, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x8591, 0x99);
	OV5645MIPI_write_cmos_sensor(0x8592, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x8593, 0x93);
	OV5645MIPI_write_cmos_sensor(0x8594, 0x24);
	OV5645MIPI_write_cmos_sensor(0x8595, 0xff);
	OV5645MIPI_write_cmos_sensor(0x8596, 0xff);
	OV5645MIPI_write_cmos_sensor(0x8597, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x8598, 0x34);
	OV5645MIPI_write_cmos_sensor(0x8599, 0xff);
	OV5645MIPI_write_cmos_sensor(0x859a, 0xfe);
	OV5645MIPI_write_cmos_sensor(0x859b, 0x78);
	OV5645MIPI_write_cmos_sensor(0x859c, 0xaa);
	OV5645MIPI_write_cmos_sensor(0x859d, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x859e, 0x24);
	OV5645MIPI_write_cmos_sensor(0x859f, 0x01);
	OV5645MIPI_write_cmos_sensor(0x85a0, 0xfd);
	OV5645MIPI_write_cmos_sensor(0x85a1, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x85a2, 0x33);
	OV5645MIPI_write_cmos_sensor(0x85a3, 0xfc);
	OV5645MIPI_write_cmos_sensor(0x85a4, 0xd3);
	OV5645MIPI_write_cmos_sensor(0x85a5, 0xed);
	OV5645MIPI_write_cmos_sensor(0x85a6, 0x9f);
	OV5645MIPI_write_cmos_sensor(0x85a7, 0xee);
	OV5645MIPI_write_cmos_sensor(0x85a8, 0x64);
	OV5645MIPI_write_cmos_sensor(0x85a9, 0x80);
	OV5645MIPI_write_cmos_sensor(0x85aa, 0xf8);
	OV5645MIPI_write_cmos_sensor(0x85ab, 0xec);
	OV5645MIPI_write_cmos_sensor(0x85ac, 0x64);
	OV5645MIPI_write_cmos_sensor(0x85ad, 0x80);
	OV5645MIPI_write_cmos_sensor(0x85ae, 0x98);
	OV5645MIPI_write_cmos_sensor(0x85af, 0x40);
	OV5645MIPI_write_cmos_sensor(0x85b0, 0x04);
	OV5645MIPI_write_cmos_sensor(0x85b1, 0x7f);
	OV5645MIPI_write_cmos_sensor(0x85b2, 0x00);
	OV5645MIPI_write_cmos_sensor(0x85b3, 0x80);
	OV5645MIPI_write_cmos_sensor(0x85b4, 0x05);
	OV5645MIPI_write_cmos_sensor(0x85b5, 0x78);
	OV5645MIPI_write_cmos_sensor(0x85b6, 0xaa);
	OV5645MIPI_write_cmos_sensor(0x85b7, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x85b8, 0x04);
	OV5645MIPI_write_cmos_sensor(0x85b9, 0xff);
	OV5645MIPI_write_cmos_sensor(0x85ba, 0x78);
	OV5645MIPI_write_cmos_sensor(0x85bb, 0xaa);
	OV5645MIPI_write_cmos_sensor(0x85bc, 0xa6);
	OV5645MIPI_write_cmos_sensor(0x85bd, 0x07);
	OV5645MIPI_write_cmos_sensor(0x85be, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x85bf, 0x1e);
	OV5645MIPI_write_cmos_sensor(0x85c0, 0xb4);
	OV5645MIPI_write_cmos_sensor(0x85c1, 0x01);
	OV5645MIPI_write_cmos_sensor(0x85c2, 0x07);
	OV5645MIPI_write_cmos_sensor(0x85c3, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x85c4, 0x70);
	OV5645MIPI_write_cmos_sensor(0x85c5, 0x04);
	OV5645MIPI_write_cmos_sensor(0x85c6, 0x75);
	OV5645MIPI_write_cmos_sensor(0x85c7, 0x1e);
	OV5645MIPI_write_cmos_sensor(0x85c8, 0x02);
	OV5645MIPI_write_cmos_sensor(0x85c9, 0x22);
	OV5645MIPI_write_cmos_sensor(0x85ca, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x85cb, 0x78);
	OV5645MIPI_write_cmos_sensor(0x85cc, 0xab);
	OV5645MIPI_write_cmos_sensor(0x85cd, 0xf6);
	OV5645MIPI_write_cmos_sensor(0x85ce, 0x08);
	OV5645MIPI_write_cmos_sensor(0x85cf, 0xf6);
	OV5645MIPI_write_cmos_sensor(0x85d0, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x85d1, 0x0b);
	OV5645MIPI_write_cmos_sensor(0x85d2, 0x12);
	OV5645MIPI_write_cmos_sensor(0x85d3, 0x11);
	OV5645MIPI_write_cmos_sensor(0x85d4, 0xcf);
	OV5645MIPI_write_cmos_sensor(0x85d5, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x85d6, 0x14);
	OV5645MIPI_write_cmos_sensor(0x85d7, 0x08);
	OV5645MIPI_write_cmos_sensor(0x85d8, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x85d9, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x85da, 0x15);
	OV5645MIPI_write_cmos_sensor(0x85db, 0x12);
	OV5645MIPI_write_cmos_sensor(0x85dc, 0x11);
	OV5645MIPI_write_cmos_sensor(0x85dd, 0xcf);
	OV5645MIPI_write_cmos_sensor(0x85de, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x85df, 0x16);
	OV5645MIPI_write_cmos_sensor(0x85e0, 0x08);
	OV5645MIPI_write_cmos_sensor(0x85e1, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x85e2, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x85e3, 0x17);
	OV5645MIPI_write_cmos_sensor(0x85e4, 0x12);
	OV5645MIPI_write_cmos_sensor(0x85e5, 0x11);
	OV5645MIPI_write_cmos_sensor(0x85e6, 0xcf);
	OV5645MIPI_write_cmos_sensor(0x85e7, 0xfe);
	OV5645MIPI_write_cmos_sensor(0x85e8, 0x08);
	OV5645MIPI_write_cmos_sensor(0x85e9, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x85ea, 0xff);
	OV5645MIPI_write_cmos_sensor(0x85eb, 0x12);
	OV5645MIPI_write_cmos_sensor(0x85ec, 0x11);
	OV5645MIPI_write_cmos_sensor(0x85ed, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x85ee, 0x75);
	OV5645MIPI_write_cmos_sensor(0x85ef, 0x0a);
	OV5645MIPI_write_cmos_sensor(0x85f0, 0x01);
	OV5645MIPI_write_cmos_sensor(0x85f1, 0x90);
	OV5645MIPI_write_cmos_sensor(0x85f2, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x85f3, 0x99);
	OV5645MIPI_write_cmos_sensor(0x85f4, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x85f5, 0x93);
	OV5645MIPI_write_cmos_sensor(0x85f6, 0xfb);
	OV5645MIPI_write_cmos_sensor(0x85f7, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x85f8, 0x0a);
	OV5645MIPI_write_cmos_sensor(0x85f9, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x85fa, 0x9b);
	OV5645MIPI_write_cmos_sensor(0x85fb, 0x50);
	OV5645MIPI_write_cmos_sensor(0x85fc, 0x67);
	OV5645MIPI_write_cmos_sensor(0x85fd, 0x12);
	OV5645MIPI_write_cmos_sensor(0x85fe, 0x11);
	OV5645MIPI_write_cmos_sensor(0x85ff, 0x73);
	OV5645MIPI_write_cmos_sensor(0x8600, 0xf8);
	OV5645MIPI_write_cmos_sensor(0x8601, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8602, 0xfe);
	OV5645MIPI_write_cmos_sensor(0x8603, 0x08);
	OV5645MIPI_write_cmos_sensor(0x8604, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8605, 0xff);
	OV5645MIPI_write_cmos_sensor(0x8606, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x8607, 0xfc);
	OV5645MIPI_write_cmos_sensor(0x8608, 0xfd);
	OV5645MIPI_write_cmos_sensor(0x8609, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x860a, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x860b, 0x2f);
	OV5645MIPI_write_cmos_sensor(0x860c, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x860d, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x860e, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x860f, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x8610, 0x3e);
	OV5645MIPI_write_cmos_sensor(0x8611, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x8612, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x8613, 0xed);
	OV5645MIPI_write_cmos_sensor(0x8614, 0x35);
	OV5645MIPI_write_cmos_sensor(0x8615, 0x0d);
	OV5645MIPI_write_cmos_sensor(0x8616, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x8617, 0x0d);
	OV5645MIPI_write_cmos_sensor(0x8618, 0xec);
	OV5645MIPI_write_cmos_sensor(0x8619, 0x35);
	OV5645MIPI_write_cmos_sensor(0x861a, 0x0c);
	OV5645MIPI_write_cmos_sensor(0x861b, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x861c, 0x0c);
	OV5645MIPI_write_cmos_sensor(0x861d, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x861e, 0x0a);
	OV5645MIPI_write_cmos_sensor(0x861f, 0x75);
	OV5645MIPI_write_cmos_sensor(0x8620, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x8621, 0x08);
	OV5645MIPI_write_cmos_sensor(0x8622, 0xa4);
	OV5645MIPI_write_cmos_sensor(0x8623, 0x24);
	OV5645MIPI_write_cmos_sensor(0x8624, 0x5b);
	OV5645MIPI_write_cmos_sensor(0x8625, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8626, 0x11);
	OV5645MIPI_write_cmos_sensor(0x8627, 0x7b);
	OV5645MIPI_write_cmos_sensor(0x8628, 0xf9);
	OV5645MIPI_write_cmos_sensor(0x8629, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x862a, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x862b, 0x15);
	OV5645MIPI_write_cmos_sensor(0x862c, 0x97);
	OV5645MIPI_write_cmos_sensor(0x862d, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x862e, 0x14);
	OV5645MIPI_write_cmos_sensor(0x862f, 0x19);
	OV5645MIPI_write_cmos_sensor(0x8630, 0x97);
	OV5645MIPI_write_cmos_sensor(0x8631, 0x50);
	OV5645MIPI_write_cmos_sensor(0x8632, 0x0b);
	OV5645MIPI_write_cmos_sensor(0x8633, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8634, 0x11);
	OV5645MIPI_write_cmos_sensor(0x8635, 0x73);
	OV5645MIPI_write_cmos_sensor(0x8636, 0xf8);
	OV5645MIPI_write_cmos_sensor(0x8637, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8638, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x8639, 0x14);
	OV5645MIPI_write_cmos_sensor(0x863a, 0x08);
	OV5645MIPI_write_cmos_sensor(0x863b, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x863c, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x863d, 0x15);
	OV5645MIPI_write_cmos_sensor(0x863e, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x863f, 0x0a);
	OV5645MIPI_write_cmos_sensor(0x8640, 0x75);
	OV5645MIPI_write_cmos_sensor(0x8641, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x8642, 0x08);
	OV5645MIPI_write_cmos_sensor(0x8643, 0xa4);
	OV5645MIPI_write_cmos_sensor(0x8644, 0x24);
	OV5645MIPI_write_cmos_sensor(0x8645, 0x5b);
	OV5645MIPI_write_cmos_sensor(0x8646, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8647, 0x11);
	OV5645MIPI_write_cmos_sensor(0x8648, 0x7b);
	OV5645MIPI_write_cmos_sensor(0x8649, 0xf9);
	OV5645MIPI_write_cmos_sensor(0x864a, 0xd3);
	OV5645MIPI_write_cmos_sensor(0x864b, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x864c, 0x17);
	OV5645MIPI_write_cmos_sensor(0x864d, 0x97);
	OV5645MIPI_write_cmos_sensor(0x864e, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x864f, 0x16);
	OV5645MIPI_write_cmos_sensor(0x8650, 0x19);
	OV5645MIPI_write_cmos_sensor(0x8651, 0x97);
	OV5645MIPI_write_cmos_sensor(0x8652, 0x40);
	OV5645MIPI_write_cmos_sensor(0x8653, 0x0b);
	OV5645MIPI_write_cmos_sensor(0x8654, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8655, 0x11);
	OV5645MIPI_write_cmos_sensor(0x8656, 0x73);
	OV5645MIPI_write_cmos_sensor(0x8657, 0xf8);
	OV5645MIPI_write_cmos_sensor(0x8658, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8659, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x865a, 0x16);
	OV5645MIPI_write_cmos_sensor(0x865b, 0x08);
	OV5645MIPI_write_cmos_sensor(0x865c, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x865d, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x865e, 0x17);
	OV5645MIPI_write_cmos_sensor(0x865f, 0x05);
	OV5645MIPI_write_cmos_sensor(0x8660, 0x0a);
	OV5645MIPI_write_cmos_sensor(0x8661, 0x02);
	OV5645MIPI_write_cmos_sensor(0x8662, 0x05);
	OV5645MIPI_write_cmos_sensor(0x8663, 0xf1);
	OV5645MIPI_write_cmos_sensor(0x8664, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x8665, 0xfa);
	OV5645MIPI_write_cmos_sensor(0x8666, 0xf9);
	OV5645MIPI_write_cmos_sensor(0x8667, 0xf8);
	OV5645MIPI_write_cmos_sensor(0x8668, 0xaf);
	OV5645MIPI_write_cmos_sensor(0x8669, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x866a, 0xae);
	OV5645MIPI_write_cmos_sensor(0x866b, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x866c, 0xad);
	OV5645MIPI_write_cmos_sensor(0x866d, 0x0d);
	OV5645MIPI_write_cmos_sensor(0x866e, 0xac);
	OV5645MIPI_write_cmos_sensor(0x866f, 0x0c);
	OV5645MIPI_write_cmos_sensor(0x8670, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8671, 0x0b);
	OV5645MIPI_write_cmos_sensor(0x8672, 0x7b);
	OV5645MIPI_write_cmos_sensor(0x8673, 0x8e);
	OV5645MIPI_write_cmos_sensor(0x8674, 0x18);
	OV5645MIPI_write_cmos_sensor(0x8675, 0x8f);
	OV5645MIPI_write_cmos_sensor(0x8676, 0x19);
	OV5645MIPI_write_cmos_sensor(0x8677, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x8678, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x8679, 0x15);
	OV5645MIPI_write_cmos_sensor(0x867a, 0x95);
	OV5645MIPI_write_cmos_sensor(0x867b, 0x17);
	OV5645MIPI_write_cmos_sensor(0x867c, 0xff);
	OV5645MIPI_write_cmos_sensor(0x867d, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x867e, 0x14);
	OV5645MIPI_write_cmos_sensor(0x867f, 0x95);
	OV5645MIPI_write_cmos_sensor(0x8680, 0x16);
	OV5645MIPI_write_cmos_sensor(0x8681, 0xfe);
	OV5645MIPI_write_cmos_sensor(0x8682, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x8683, 0x0b);
	OV5645MIPI_write_cmos_sensor(0x8684, 0x25);
	OV5645MIPI_write_cmos_sensor(0x8685, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x8686, 0x24);
	OV5645MIPI_write_cmos_sensor(0x8687, 0x53);
	OV5645MIPI_write_cmos_sensor(0x8688, 0xf9);
	OV5645MIPI_write_cmos_sensor(0x8689, 0xd3);
	OV5645MIPI_write_cmos_sensor(0x868a, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x868b, 0x15);
	OV5645MIPI_write_cmos_sensor(0x868c, 0x97);
	OV5645MIPI_write_cmos_sensor(0x868d, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x868e, 0x14);
	OV5645MIPI_write_cmos_sensor(0x868f, 0x19);
	OV5645MIPI_write_cmos_sensor(0x8690, 0x97);
	OV5645MIPI_write_cmos_sensor(0x8691, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x8692, 0x0b);
	OV5645MIPI_write_cmos_sensor(0x8693, 0x40);
	OV5645MIPI_write_cmos_sensor(0x8694, 0x11);
	OV5645MIPI_write_cmos_sensor(0x8695, 0x25);
	OV5645MIPI_write_cmos_sensor(0x8696, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x8697, 0x24);
	OV5645MIPI_write_cmos_sensor(0x8698, 0x53);
	OV5645MIPI_write_cmos_sensor(0x8699, 0xf8);
	OV5645MIPI_write_cmos_sensor(0x869a, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x869b, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x869c, 0x15);
	OV5645MIPI_write_cmos_sensor(0x869d, 0x96);
	OV5645MIPI_write_cmos_sensor(0x869e, 0xfd);
	OV5645MIPI_write_cmos_sensor(0x869f, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x86a0, 0x14);
	OV5645MIPI_write_cmos_sensor(0x86a1, 0x18);
	OV5645MIPI_write_cmos_sensor(0x86a2, 0x96);
	OV5645MIPI_write_cmos_sensor(0x86a3, 0xfc);
	OV5645MIPI_write_cmos_sensor(0x86a4, 0x80);
	OV5645MIPI_write_cmos_sensor(0x86a5, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x86a6, 0x25);
	OV5645MIPI_write_cmos_sensor(0x86a7, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x86a8, 0x24);
	OV5645MIPI_write_cmos_sensor(0x86a9, 0x53);
	OV5645MIPI_write_cmos_sensor(0x86aa, 0xf8);
	OV5645MIPI_write_cmos_sensor(0x86ab, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x86ac, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x86ad, 0x95);
	OV5645MIPI_write_cmos_sensor(0x86ae, 0x15);
	OV5645MIPI_write_cmos_sensor(0x86af, 0xfd);
	OV5645MIPI_write_cmos_sensor(0x86b0, 0x18);
	OV5645MIPI_write_cmos_sensor(0x86b1, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x86b2, 0x95);
	OV5645MIPI_write_cmos_sensor(0x86b3, 0x14);
	OV5645MIPI_write_cmos_sensor(0x86b4, 0xfc);
	OV5645MIPI_write_cmos_sensor(0x86b5, 0x8c);
	OV5645MIPI_write_cmos_sensor(0x86b6, 0x1a);
	OV5645MIPI_write_cmos_sensor(0x86b7, 0x8d);
	OV5645MIPI_write_cmos_sensor(0x86b8, 0x1b);
	OV5645MIPI_write_cmos_sensor(0x86b9, 0x12);
	OV5645MIPI_write_cmos_sensor(0x86ba, 0x11);
	OV5645MIPI_write_cmos_sensor(0x86bb, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x86bc, 0x12);
	OV5645MIPI_write_cmos_sensor(0x86bd, 0x11);
	OV5645MIPI_write_cmos_sensor(0x86be, 0x6a);
	OV5645MIPI_write_cmos_sensor(0x86bf, 0x90);
	OV5645MIPI_write_cmos_sensor(0x86c0, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x86c1, 0x8e);
	OV5645MIPI_write_cmos_sensor(0x86c2, 0x12);
	OV5645MIPI_write_cmos_sensor(0x86c3, 0x11);
	OV5645MIPI_write_cmos_sensor(0x86c4, 0x95);
	OV5645MIPI_write_cmos_sensor(0x86c5, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x86c6, 0x85);
	OV5645MIPI_write_cmos_sensor(0x86c7, 0x15);
	OV5645MIPI_write_cmos_sensor(0x86c8, 0x13);
	OV5645MIPI_write_cmos_sensor(0x86c9, 0x85);
	OV5645MIPI_write_cmos_sensor(0x86ca, 0x14);
	OV5645MIPI_write_cmos_sensor(0x86cb, 0x12);
	OV5645MIPI_write_cmos_sensor(0x86cc, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x86cd, 0x11);
	OV5645MIPI_write_cmos_sensor(0x86ce, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x86cf, 0x10);
	OV5645MIPI_write_cmos_sensor(0x86d0, 0xaf);
	OV5645MIPI_write_cmos_sensor(0x86d1, 0x13);
	OV5645MIPI_write_cmos_sensor(0x86d2, 0xae);
	OV5645MIPI_write_cmos_sensor(0x86d3, 0x12);
	OV5645MIPI_write_cmos_sensor(0x86d4, 0x7b);
	OV5645MIPI_write_cmos_sensor(0x86d5, 0x04);
	OV5645MIPI_write_cmos_sensor(0x86d6, 0x12);
	OV5645MIPI_write_cmos_sensor(0x86d7, 0x11);
	OV5645MIPI_write_cmos_sensor(0x86d8, 0x58);
	OV5645MIPI_write_cmos_sensor(0x86d9, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x86da, 0x12);
	OV5645MIPI_write_cmos_sensor(0x86db, 0x0c);
	OV5645MIPI_write_cmos_sensor(0x86dc, 0x0d);
	OV5645MIPI_write_cmos_sensor(0x86dd, 0x50);
	OV5645MIPI_write_cmos_sensor(0x86de, 0x11);
	OV5645MIPI_write_cmos_sensor(0x86df, 0xaf);
	OV5645MIPI_write_cmos_sensor(0x86e0, 0x0b);
	OV5645MIPI_write_cmos_sensor(0x86e1, 0x74);
	OV5645MIPI_write_cmos_sensor(0x86e2, 0x01);
	OV5645MIPI_write_cmos_sensor(0x86e3, 0xa8);
	OV5645MIPI_write_cmos_sensor(0x86e4, 0x07);
	OV5645MIPI_write_cmos_sensor(0x86e5, 0x08);
	OV5645MIPI_write_cmos_sensor(0x86e6, 0x80);
	OV5645MIPI_write_cmos_sensor(0x86e7, 0x02);
	OV5645MIPI_write_cmos_sensor(0x86e8, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x86e9, 0x33);
	OV5645MIPI_write_cmos_sensor(0x86ea, 0xd8);
	OV5645MIPI_write_cmos_sensor(0x86eb, 0xfc);
	OV5645MIPI_write_cmos_sensor(0x86ec, 0x78);
	OV5645MIPI_write_cmos_sensor(0x86ed, 0xab);
	OV5645MIPI_write_cmos_sensor(0x86ee, 0x26);
	OV5645MIPI_write_cmos_sensor(0x86ef, 0xf6);
	OV5645MIPI_write_cmos_sensor(0x86f0, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x86f1, 0x85);
	OV5645MIPI_write_cmos_sensor(0x86f2, 0x1b);
	OV5645MIPI_write_cmos_sensor(0x86f3, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x86f4, 0x85);
	OV5645MIPI_write_cmos_sensor(0x86f5, 0x1a);
	OV5645MIPI_write_cmos_sensor(0x86f6, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x86f7, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x86f8, 0x0d);
	OV5645MIPI_write_cmos_sensor(0x86f9, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x86fa, 0x0c);
	OV5645MIPI_write_cmos_sensor(0x86fb, 0x12);
	OV5645MIPI_write_cmos_sensor(0x86fc, 0x11);
	OV5645MIPI_write_cmos_sensor(0x86fd, 0x6a);
	OV5645MIPI_write_cmos_sensor(0x86fe, 0x90);
	OV5645MIPI_write_cmos_sensor(0x86ff, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x8700, 0x92);
	OV5645MIPI_write_cmos_sensor(0x8701, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8702, 0x11);
	OV5645MIPI_write_cmos_sensor(0x8703, 0x95);
	OV5645MIPI_write_cmos_sensor(0x8704, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x8705, 0x0b);
	OV5645MIPI_write_cmos_sensor(0x8706, 0x25);
	OV5645MIPI_write_cmos_sensor(0x8707, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x8708, 0x24);
	OV5645MIPI_write_cmos_sensor(0x8709, 0x53);
	OV5645MIPI_write_cmos_sensor(0x870a, 0xf9);
	OV5645MIPI_write_cmos_sensor(0x870b, 0xd3);
	OV5645MIPI_write_cmos_sensor(0x870c, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x870d, 0x19);
	OV5645MIPI_write_cmos_sensor(0x870e, 0x97);
	OV5645MIPI_write_cmos_sensor(0x870f, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x8710, 0x18);
	OV5645MIPI_write_cmos_sensor(0x8711, 0x19);
	OV5645MIPI_write_cmos_sensor(0x8712, 0x97);
	OV5645MIPI_write_cmos_sensor(0x8713, 0x40);
	OV5645MIPI_write_cmos_sensor(0x8714, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x8715, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x8716, 0x0b);
	OV5645MIPI_write_cmos_sensor(0x8717, 0x25);
	OV5645MIPI_write_cmos_sensor(0x8718, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x8719, 0x24);
	OV5645MIPI_write_cmos_sensor(0x871a, 0x52);
	OV5645MIPI_write_cmos_sensor(0x871b, 0xf8);
	OV5645MIPI_write_cmos_sensor(0x871c, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x871d, 0xfe);
	OV5645MIPI_write_cmos_sensor(0x871e, 0x08);
	OV5645MIPI_write_cmos_sensor(0x871f, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8720, 0xff);
	OV5645MIPI_write_cmos_sensor(0x8721, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8722, 0x04);
	OV5645MIPI_write_cmos_sensor(0x8723, 0xae);
	OV5645MIPI_write_cmos_sensor(0x8724, 0x18);
	OV5645MIPI_write_cmos_sensor(0x8725, 0xaf);
	OV5645MIPI_write_cmos_sensor(0x8726, 0x19);
	OV5645MIPI_write_cmos_sensor(0x8727, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x8728, 0x8f);
	OV5645MIPI_write_cmos_sensor(0x8729, 0x13);
	OV5645MIPI_write_cmos_sensor(0x872a, 0x8e);
	OV5645MIPI_write_cmos_sensor(0x872b, 0x12);
	OV5645MIPI_write_cmos_sensor(0x872c, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x872d, 0x11);
	OV5645MIPI_write_cmos_sensor(0x872e, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x872f, 0x10);
	OV5645MIPI_write_cmos_sensor(0x8730, 0x7b);
	OV5645MIPI_write_cmos_sensor(0x8731, 0x10);
	OV5645MIPI_write_cmos_sensor(0x8732, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8733, 0x11);
	OV5645MIPI_write_cmos_sensor(0x8734, 0x58);
	OV5645MIPI_write_cmos_sensor(0x8735, 0xd3);
	OV5645MIPI_write_cmos_sensor(0x8736, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8737, 0x0c);
	OV5645MIPI_write_cmos_sensor(0x8738, 0x0d);
	OV5645MIPI_write_cmos_sensor(0x8739, 0x40);
	OV5645MIPI_write_cmos_sensor(0x873a, 0x11);
	OV5645MIPI_write_cmos_sensor(0x873b, 0xaf);
	OV5645MIPI_write_cmos_sensor(0x873c, 0x0b);
	OV5645MIPI_write_cmos_sensor(0x873d, 0x74);
	OV5645MIPI_write_cmos_sensor(0x873e, 0x01);
	OV5645MIPI_write_cmos_sensor(0x873f, 0xa8);
	OV5645MIPI_write_cmos_sensor(0x8740, 0x07);
	OV5645MIPI_write_cmos_sensor(0x8741, 0x08);
	OV5645MIPI_write_cmos_sensor(0x8742, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8743, 0x02);
	OV5645MIPI_write_cmos_sensor(0x8744, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x8745, 0x33);
	OV5645MIPI_write_cmos_sensor(0x8746, 0xd8);
	OV5645MIPI_write_cmos_sensor(0x8747, 0xfc);
	OV5645MIPI_write_cmos_sensor(0x8748, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8749, 0xac);
	OV5645MIPI_write_cmos_sensor(0x874a, 0x26);
	OV5645MIPI_write_cmos_sensor(0x874b, 0xf6);
	OV5645MIPI_write_cmos_sensor(0x874c, 0x05);
	OV5645MIPI_write_cmos_sensor(0x874d, 0x0b);
	OV5645MIPI_write_cmos_sensor(0x874e, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x874f, 0x0b);
	OV5645MIPI_write_cmos_sensor(0x8750, 0x64);
	OV5645MIPI_write_cmos_sensor(0x8751, 0x04);
	OV5645MIPI_write_cmos_sensor(0x8752, 0x60);
	OV5645MIPI_write_cmos_sensor(0x8753, 0x03);
	OV5645MIPI_write_cmos_sensor(0x8754, 0x02);
	OV5645MIPI_write_cmos_sensor(0x8755, 0x05);
	OV5645MIPI_write_cmos_sensor(0x8756, 0xd2);
	OV5645MIPI_write_cmos_sensor(0x8757, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x8758, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x8759, 0x0b);
	OV5645MIPI_write_cmos_sensor(0x875a, 0x12);
	OV5645MIPI_write_cmos_sensor(0x875b, 0x11);
	OV5645MIPI_write_cmos_sensor(0x875c, 0xfa);
	OV5645MIPI_write_cmos_sensor(0x875d, 0xfb);
	OV5645MIPI_write_cmos_sensor(0x875e, 0x12);
	OV5645MIPI_write_cmos_sensor(0x875f, 0x11);
	OV5645MIPI_write_cmos_sensor(0x8760, 0xfa);
	OV5645MIPI_write_cmos_sensor(0x8761, 0xfa);
	OV5645MIPI_write_cmos_sensor(0x8762, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8763, 0x11);
	OV5645MIPI_write_cmos_sensor(0x8764, 0xfa);
	OV5645MIPI_write_cmos_sensor(0x8765, 0x75);
	OV5645MIPI_write_cmos_sensor(0x8766, 0x18);
	OV5645MIPI_write_cmos_sensor(0x8767, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8768, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x8769, 0x19);
	OV5645MIPI_write_cmos_sensor(0x876a, 0x75);
	OV5645MIPI_write_cmos_sensor(0x876b, 0x0a);
	OV5645MIPI_write_cmos_sensor(0x876c, 0x01);
	OV5645MIPI_write_cmos_sensor(0x876d, 0x90);
	OV5645MIPI_write_cmos_sensor(0x876e, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x876f, 0x99);
	OV5645MIPI_write_cmos_sensor(0x8770, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x8771, 0x93);
	OV5645MIPI_write_cmos_sensor(0x8772, 0xff);
	OV5645MIPI_write_cmos_sensor(0x8773, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x8774, 0x0a);
	OV5645MIPI_write_cmos_sensor(0x8775, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x8776, 0x9f);
	OV5645MIPI_write_cmos_sensor(0x8777, 0x50);
	OV5645MIPI_write_cmos_sensor(0x8778, 0x2a);
	OV5645MIPI_write_cmos_sensor(0x8779, 0x12);
	OV5645MIPI_write_cmos_sensor(0x877a, 0x11);
	OV5645MIPI_write_cmos_sensor(0x877b, 0x4b);
	OV5645MIPI_write_cmos_sensor(0x877c, 0x25);
	OV5645MIPI_write_cmos_sensor(0x877d, 0x19);
	OV5645MIPI_write_cmos_sensor(0x877e, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x877f, 0x19);
	OV5645MIPI_write_cmos_sensor(0x8780, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x8781, 0x35);
	OV5645MIPI_write_cmos_sensor(0x8782, 0x18);
	OV5645MIPI_write_cmos_sensor(0x8783, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x8784, 0x18);
	OV5645MIPI_write_cmos_sensor(0x8785, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8786, 0x11);
	OV5645MIPI_write_cmos_sensor(0x8787, 0x4b);
	OV5645MIPI_write_cmos_sensor(0x8788, 0xfe);
	OV5645MIPI_write_cmos_sensor(0x8789, 0xeb);
	OV5645MIPI_write_cmos_sensor(0x878a, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x878b, 0x9e);
	OV5645MIPI_write_cmos_sensor(0x878c, 0x50);
	OV5645MIPI_write_cmos_sensor(0x878d, 0x04);
	OV5645MIPI_write_cmos_sensor(0x878e, 0x12);
	OV5645MIPI_write_cmos_sensor(0x878f, 0x11);
	OV5645MIPI_write_cmos_sensor(0x8790, 0x4b);
	OV5645MIPI_write_cmos_sensor(0x8791, 0xfb);
	OV5645MIPI_write_cmos_sensor(0x8792, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8793, 0x11);
	OV5645MIPI_write_cmos_sensor(0x8794, 0x4b);
	OV5645MIPI_write_cmos_sensor(0x8795, 0xfe);
	OV5645MIPI_write_cmos_sensor(0x8796, 0xea);
	OV5645MIPI_write_cmos_sensor(0x8797, 0xd3);
	OV5645MIPI_write_cmos_sensor(0x8798, 0x9e);
	OV5645MIPI_write_cmos_sensor(0x8799, 0x40);
	OV5645MIPI_write_cmos_sensor(0x879a, 0x04);
	OV5645MIPI_write_cmos_sensor(0x879b, 0x12);
	OV5645MIPI_write_cmos_sensor(0x879c, 0x11);
	OV5645MIPI_write_cmos_sensor(0x879d, 0x4b);
	OV5645MIPI_write_cmos_sensor(0x879e, 0xfa);
	OV5645MIPI_write_cmos_sensor(0x879f, 0x05);
	OV5645MIPI_write_cmos_sensor(0x87a0, 0x0a);
	OV5645MIPI_write_cmos_sensor(0x87a1, 0x80);
	OV5645MIPI_write_cmos_sensor(0x87a2, 0xca);
	OV5645MIPI_write_cmos_sensor(0x87a3, 0xef);
	OV5645MIPI_write_cmos_sensor(0x87a4, 0xfd);
	OV5645MIPI_write_cmos_sensor(0x87a5, 0x7c);
	OV5645MIPI_write_cmos_sensor(0x87a6, 0x00);
	OV5645MIPI_write_cmos_sensor(0x87a7, 0xae);
	OV5645MIPI_write_cmos_sensor(0x87a8, 0x18);
	OV5645MIPI_write_cmos_sensor(0x87a9, 0xaf);
	OV5645MIPI_write_cmos_sensor(0x87aa, 0x19);
	OV5645MIPI_write_cmos_sensor(0x87ab, 0x12);
	OV5645MIPI_write_cmos_sensor(0x87ac, 0x0a);
	OV5645MIPI_write_cmos_sensor(0x87ad, 0x9b);
	OV5645MIPI_write_cmos_sensor(0x87ae, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x87af, 0xeb);
	OV5645MIPI_write_cmos_sensor(0x87b0, 0x9a);
	OV5645MIPI_write_cmos_sensor(0x87b1, 0xfe);
	OV5645MIPI_write_cmos_sensor(0x87b2, 0x74);
	OV5645MIPI_write_cmos_sensor(0x87b3, 0xad);
	OV5645MIPI_write_cmos_sensor(0x87b4, 0x25);
	OV5645MIPI_write_cmos_sensor(0x87b5, 0x0b);
	OV5645MIPI_write_cmos_sensor(0x87b6, 0xf8);
	OV5645MIPI_write_cmos_sensor(0x87b7, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x87b8, 0xfd);
	OV5645MIPI_write_cmos_sensor(0x87b9, 0xef);
	OV5645MIPI_write_cmos_sensor(0x87ba, 0xd3);
	OV5645MIPI_write_cmos_sensor(0x87bb, 0x9d);
	OV5645MIPI_write_cmos_sensor(0x87bc, 0x74);
	OV5645MIPI_write_cmos_sensor(0x87bd, 0xad);
	OV5645MIPI_write_cmos_sensor(0x87be, 0x40);
	OV5645MIPI_write_cmos_sensor(0x87bf, 0x0b);
	OV5645MIPI_write_cmos_sensor(0x87c0, 0x25);
	OV5645MIPI_write_cmos_sensor(0x87c1, 0x0b);
	OV5645MIPI_write_cmos_sensor(0x87c2, 0xf8);
	OV5645MIPI_write_cmos_sensor(0x87c3, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x87c4, 0xfd);
	OV5645MIPI_write_cmos_sensor(0x87c5, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x87c6, 0xef);
	OV5645MIPI_write_cmos_sensor(0x87c7, 0x9d);
	OV5645MIPI_write_cmos_sensor(0x87c8, 0xff);
	OV5645MIPI_write_cmos_sensor(0x87c9, 0x80);
	OV5645MIPI_write_cmos_sensor(0x87ca, 0x07);
	OV5645MIPI_write_cmos_sensor(0x87cb, 0x25);
	OV5645MIPI_write_cmos_sensor(0x87cc, 0x0b);
	OV5645MIPI_write_cmos_sensor(0x87cd, 0xf8);
	OV5645MIPI_write_cmos_sensor(0x87ce, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x87cf, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x87d0, 0x9f);
	OV5645MIPI_write_cmos_sensor(0x87d1, 0xff);
	OV5645MIPI_write_cmos_sensor(0x87d2, 0x8f);
	OV5645MIPI_write_cmos_sensor(0x87d3, 0x1c);
	OV5645MIPI_write_cmos_sensor(0x87d4, 0x90);
	OV5645MIPI_write_cmos_sensor(0x87d5, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x87d6, 0x96);
	OV5645MIPI_write_cmos_sensor(0x87d7, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x87d8, 0x93);
	OV5645MIPI_write_cmos_sensor(0x87d9, 0xff);
	OV5645MIPI_write_cmos_sensor(0x87da, 0xee);
	OV5645MIPI_write_cmos_sensor(0x87db, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x87dc, 0x9f);
	OV5645MIPI_write_cmos_sensor(0x87dd, 0x50);
	OV5645MIPI_write_cmos_sensor(0x87de, 0x0d);
	OV5645MIPI_write_cmos_sensor(0x87df, 0x12);
	OV5645MIPI_write_cmos_sensor(0x87e0, 0x11);
	OV5645MIPI_write_cmos_sensor(0x87e1, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x87e2, 0x80);
	OV5645MIPI_write_cmos_sensor(0x87e3, 0x02);
	OV5645MIPI_write_cmos_sensor(0x87e4, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x87e5, 0x33);
	OV5645MIPI_write_cmos_sensor(0x87e6, 0xd8);
	OV5645MIPI_write_cmos_sensor(0x87e7, 0xfc);
	OV5645MIPI_write_cmos_sensor(0x87e8, 0x78);
	OV5645MIPI_write_cmos_sensor(0x87e9, 0xab);
	OV5645MIPI_write_cmos_sensor(0x87ea, 0x26);
	OV5645MIPI_write_cmos_sensor(0x87eb, 0xf6);
	OV5645MIPI_write_cmos_sensor(0x87ec, 0x90);
	OV5645MIPI_write_cmos_sensor(0x87ed, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x87ee, 0x97);
	OV5645MIPI_write_cmos_sensor(0x87ef, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x87f0, 0x93);
	OV5645MIPI_write_cmos_sensor(0x87f1, 0xff);
	OV5645MIPI_write_cmos_sensor(0x87f2, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x87f3, 0x1c);
	OV5645MIPI_write_cmos_sensor(0x87f4, 0xd3);
	OV5645MIPI_write_cmos_sensor(0x87f5, 0x9f);
	OV5645MIPI_write_cmos_sensor(0x87f6, 0x40);
	OV5645MIPI_write_cmos_sensor(0x87f7, 0x0d);
	OV5645MIPI_write_cmos_sensor(0x87f8, 0x12);
	OV5645MIPI_write_cmos_sensor(0x87f9, 0x11);
	OV5645MIPI_write_cmos_sensor(0x87fa, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x87fb, 0x80);
	OV5645MIPI_write_cmos_sensor(0x87fc, 0x02);
	OV5645MIPI_write_cmos_sensor(0x87fd, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x87fe, 0x33);
	OV5645MIPI_write_cmos_sensor(0x87ff, 0xd8);
	OV5645MIPI_write_cmos_sensor(0x8800, 0xfc);
	OV5645MIPI_write_cmos_sensor(0x8801, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8802, 0xac);
	OV5645MIPI_write_cmos_sensor(0x8803, 0x26);
	OV5645MIPI_write_cmos_sensor(0x8804, 0xf6);
	OV5645MIPI_write_cmos_sensor(0x8805, 0x74);
	OV5645MIPI_write_cmos_sensor(0x8806, 0xb0);
	OV5645MIPI_write_cmos_sensor(0x8807, 0x25);
	OV5645MIPI_write_cmos_sensor(0x8808, 0x0b);
	OV5645MIPI_write_cmos_sensor(0x8809, 0xf8);
	OV5645MIPI_write_cmos_sensor(0x880a, 0xa6);
	OV5645MIPI_write_cmos_sensor(0x880b, 0x1c);
	OV5645MIPI_write_cmos_sensor(0x880c, 0x05);
	OV5645MIPI_write_cmos_sensor(0x880d, 0x0b);
	OV5645MIPI_write_cmos_sensor(0x880e, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x880f, 0x0b);
	OV5645MIPI_write_cmos_sensor(0x8810, 0x64);
	OV5645MIPI_write_cmos_sensor(0x8811, 0x03);
	OV5645MIPI_write_cmos_sensor(0x8812, 0x60);
	OV5645MIPI_write_cmos_sensor(0x8813, 0x03);
	OV5645MIPI_write_cmos_sensor(0x8814, 0x02);
	OV5645MIPI_write_cmos_sensor(0x8815, 0x07);
	OV5645MIPI_write_cmos_sensor(0x8816, 0x5a);
	OV5645MIPI_write_cmos_sensor(0x8817, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8818, 0xb1);
	OV5645MIPI_write_cmos_sensor(0x8819, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x881a, 0xff);
	OV5645MIPI_write_cmos_sensor(0x881b, 0x18);
	OV5645MIPI_write_cmos_sensor(0x881c, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x881d, 0x2f);
	OV5645MIPI_write_cmos_sensor(0x881e, 0xff);
	OV5645MIPI_write_cmos_sensor(0x881f, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x8820, 0x33);
	OV5645MIPI_write_cmos_sensor(0x8821, 0xfe);
	OV5645MIPI_write_cmos_sensor(0x8822, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8823, 0xb2);
	OV5645MIPI_write_cmos_sensor(0x8824, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8825, 0x7c);
	OV5645MIPI_write_cmos_sensor(0x8826, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8827, 0x2f);
	OV5645MIPI_write_cmos_sensor(0x8828, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x8829, 0x1b);
	OV5645MIPI_write_cmos_sensor(0x882a, 0xec);
	OV5645MIPI_write_cmos_sensor(0x882b, 0x3e);
	OV5645MIPI_write_cmos_sensor(0x882c, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x882d, 0x1a);
	OV5645MIPI_write_cmos_sensor(0x882e, 0x90);
	OV5645MIPI_write_cmos_sensor(0x882f, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x8830, 0x98);
	OV5645MIPI_write_cmos_sensor(0x8831, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x8832, 0x93);
	OV5645MIPI_write_cmos_sensor(0x8833, 0xff);
	OV5645MIPI_write_cmos_sensor(0x8834, 0xd3);
	OV5645MIPI_write_cmos_sensor(0x8835, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x8836, 0x1b);
	OV5645MIPI_write_cmos_sensor(0x8837, 0x9f);
	OV5645MIPI_write_cmos_sensor(0x8838, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x8839, 0x1a);
	OV5645MIPI_write_cmos_sensor(0x883a, 0x94);
	OV5645MIPI_write_cmos_sensor(0x883b, 0x00);
	OV5645MIPI_write_cmos_sensor(0x883c, 0x40);
	OV5645MIPI_write_cmos_sensor(0x883d, 0x06);
	OV5645MIPI_write_cmos_sensor(0x883e, 0x78);
	OV5645MIPI_write_cmos_sensor(0x883f, 0xac);
	OV5645MIPI_write_cmos_sensor(0x8840, 0x74);
	OV5645MIPI_write_cmos_sensor(0x8841, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8842, 0x26);
	OV5645MIPI_write_cmos_sensor(0x8843, 0xf6);
	OV5645MIPI_write_cmos_sensor(0x8844, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8845, 0xac);
	OV5645MIPI_write_cmos_sensor(0x8846, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8847, 0x79);
	OV5645MIPI_write_cmos_sensor(0x8848, 0xab);
	OV5645MIPI_write_cmos_sensor(0x8849, 0x57);
	OV5645MIPI_write_cmos_sensor(0x884a, 0xf6);
	OV5645MIPI_write_cmos_sensor(0x884b, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x884c, 0x1e);
	OV5645MIPI_write_cmos_sensor(0x884d, 0xb4);
	OV5645MIPI_write_cmos_sensor(0x884e, 0x02);
	OV5645MIPI_write_cmos_sensor(0x884f, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x8850, 0x18);
	OV5645MIPI_write_cmos_sensor(0x8851, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8852, 0xb4);
	OV5645MIPI_write_cmos_sensor(0x8853, 0x7f);
	OV5645MIPI_write_cmos_sensor(0x8854, 0x0a);
	OV5645MIPI_write_cmos_sensor(0x8855, 0x08);
	OV5645MIPI_write_cmos_sensor(0x8856, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8857, 0xd3);
	OV5645MIPI_write_cmos_sensor(0x8858, 0x94);
	OV5645MIPI_write_cmos_sensor(0x8859, 0x00);
	OV5645MIPI_write_cmos_sensor(0x885a, 0x40);
	OV5645MIPI_write_cmos_sensor(0x885b, 0x03);
	OV5645MIPI_write_cmos_sensor(0x885c, 0x75);
	OV5645MIPI_write_cmos_sensor(0x885d, 0x1e);
	OV5645MIPI_write_cmos_sensor(0x885e, 0x05);
	OV5645MIPI_write_cmos_sensor(0x885f, 0x22);
	OV5645MIPI_write_cmos_sensor(0x8860, 0x90);
	OV5645MIPI_write_cmos_sensor(0x8861, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x8862, 0x89);
	OV5645MIPI_write_cmos_sensor(0x8863, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8864, 0x0c);
	OV5645MIPI_write_cmos_sensor(0x8865, 0x44);
	OV5645MIPI_write_cmos_sensor(0x8866, 0x8f);
	OV5645MIPI_write_cmos_sensor(0x8867, 0x4d);
	OV5645MIPI_write_cmos_sensor(0x8868, 0x8e);
	OV5645MIPI_write_cmos_sensor(0x8869, 0x4c);
	OV5645MIPI_write_cmos_sensor(0x886a, 0x8d);
	OV5645MIPI_write_cmos_sensor(0x886b, 0x4b);
	OV5645MIPI_write_cmos_sensor(0x886c, 0x8c);
	OV5645MIPI_write_cmos_sensor(0x886d, 0x4a);
	OV5645MIPI_write_cmos_sensor(0x886e, 0x90);
	OV5645MIPI_write_cmos_sensor(0x886f, 0x38);
	OV5645MIPI_write_cmos_sensor(0x8870, 0x04);
	OV5645MIPI_write_cmos_sensor(0x8871, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8872, 0x14);
	OV5645MIPI_write_cmos_sensor(0x8873, 0xa8);
	OV5645MIPI_write_cmos_sensor(0x8874, 0xfb);
	OV5645MIPI_write_cmos_sensor(0x8875, 0xaa);
	OV5645MIPI_write_cmos_sensor(0x8876, 0x06);
	OV5645MIPI_write_cmos_sensor(0x8877, 0x90);
	OV5645MIPI_write_cmos_sensor(0x8878, 0x38);
	OV5645MIPI_write_cmos_sensor(0x8879, 0x00);
	OV5645MIPI_write_cmos_sensor(0x887a, 0x12);
	OV5645MIPI_write_cmos_sensor(0x887b, 0x14);
	OV5645MIPI_write_cmos_sensor(0x887c, 0xa8);
	OV5645MIPI_write_cmos_sensor(0x887d, 0xff);
	OV5645MIPI_write_cmos_sensor(0x887e, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x887f, 0xeb);
	OV5645MIPI_write_cmos_sensor(0x8880, 0x9f);
	OV5645MIPI_write_cmos_sensor(0x8881, 0xfb);
	OV5645MIPI_write_cmos_sensor(0x8882, 0xea);
	OV5645MIPI_write_cmos_sensor(0x8883, 0x9e);
	OV5645MIPI_write_cmos_sensor(0x8884, 0xfa);
	OV5645MIPI_write_cmos_sensor(0x8885, 0x90);
	OV5645MIPI_write_cmos_sensor(0x8886, 0x38);
	OV5645MIPI_write_cmos_sensor(0x8887, 0x10);
	OV5645MIPI_write_cmos_sensor(0x8888, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x8889, 0xa3);
	OV5645MIPI_write_cmos_sensor(0x888a, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x888b, 0x75);
	OV5645MIPI_write_cmos_sensor(0x888c, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x888d, 0x02);
	OV5645MIPI_write_cmos_sensor(0x888e, 0xa4);
	OV5645MIPI_write_cmos_sensor(0x888f, 0xff);
	OV5645MIPI_write_cmos_sensor(0x8890, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x8891, 0xeb);
	OV5645MIPI_write_cmos_sensor(0x8892, 0x9f);
	OV5645MIPI_write_cmos_sensor(0x8893, 0xfb);
	OV5645MIPI_write_cmos_sensor(0x8894, 0xea);
	OV5645MIPI_write_cmos_sensor(0x8895, 0x95);
	OV5645MIPI_write_cmos_sensor(0x8896, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x8897, 0xfa);
	OV5645MIPI_write_cmos_sensor(0x8898, 0x90);
	OV5645MIPI_write_cmos_sensor(0x8899, 0x38);
	OV5645MIPI_write_cmos_sensor(0x889a, 0x06);
	OV5645MIPI_write_cmos_sensor(0x889b, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x889c, 0xfe);
	OV5645MIPI_write_cmos_sensor(0x889d, 0xa3);
	OV5645MIPI_write_cmos_sensor(0x889e, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x889f, 0xfd);
	OV5645MIPI_write_cmos_sensor(0x88a0, 0xee);
	OV5645MIPI_write_cmos_sensor(0x88a1, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x88a2, 0x0c);
	OV5645MIPI_write_cmos_sensor(0x88a3, 0xed);
	OV5645MIPI_write_cmos_sensor(0x88a4, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x88a5, 0x0d);
	OV5645MIPI_write_cmos_sensor(0x88a6, 0x90);
	OV5645MIPI_write_cmos_sensor(0x88a7, 0x38);
	OV5645MIPI_write_cmos_sensor(0x88a8, 0x02);
	OV5645MIPI_write_cmos_sensor(0x88a9, 0x12);
	OV5645MIPI_write_cmos_sensor(0x88aa, 0x14);
	OV5645MIPI_write_cmos_sensor(0x88ab, 0xa8);
	OV5645MIPI_write_cmos_sensor(0x88ac, 0xff);
	OV5645MIPI_write_cmos_sensor(0x88ad, 0x12);
	OV5645MIPI_write_cmos_sensor(0x88ae, 0x14);
	OV5645MIPI_write_cmos_sensor(0x88af, 0x91);
	OV5645MIPI_write_cmos_sensor(0x88b0, 0x90);
	OV5645MIPI_write_cmos_sensor(0x88b1, 0x38);
	OV5645MIPI_write_cmos_sensor(0x88b2, 0x12);
	OV5645MIPI_write_cmos_sensor(0x88b3, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x88b4, 0xa3);
	OV5645MIPI_write_cmos_sensor(0x88b5, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x88b6, 0x75);
	OV5645MIPI_write_cmos_sensor(0x88b7, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x88b8, 0x02);
	OV5645MIPI_write_cmos_sensor(0x88b9, 0xa4);
	OV5645MIPI_write_cmos_sensor(0x88ba, 0xff);
	OV5645MIPI_write_cmos_sensor(0x88bb, 0xae);
	OV5645MIPI_write_cmos_sensor(0x88bc, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x88bd, 0x12);
	OV5645MIPI_write_cmos_sensor(0x88be, 0x14);
	OV5645MIPI_write_cmos_sensor(0x88bf, 0x91);
	OV5645MIPI_write_cmos_sensor(0x88c0, 0xa3);
	OV5645MIPI_write_cmos_sensor(0x88c1, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x88c2, 0xb4);
	OV5645MIPI_write_cmos_sensor(0x88c3, 0x31);
	OV5645MIPI_write_cmos_sensor(0x88c4, 0x07);
	OV5645MIPI_write_cmos_sensor(0x88c5, 0xea);
	OV5645MIPI_write_cmos_sensor(0x88c6, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x88c7, 0x13);
	OV5645MIPI_write_cmos_sensor(0x88c8, 0xfa);
	OV5645MIPI_write_cmos_sensor(0x88c9, 0xeb);
	OV5645MIPI_write_cmos_sensor(0x88ca, 0x13);
	OV5645MIPI_write_cmos_sensor(0x88cb, 0xfb);
	OV5645MIPI_write_cmos_sensor(0x88cc, 0x90);
	OV5645MIPI_write_cmos_sensor(0x88cd, 0x38);
	OV5645MIPI_write_cmos_sensor(0x88ce, 0x14);
	OV5645MIPI_write_cmos_sensor(0x88cf, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x88d0, 0xb4);
	OV5645MIPI_write_cmos_sensor(0x88d1, 0x71);
	OV5645MIPI_write_cmos_sensor(0x88d2, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x88d3, 0xeb);
	OV5645MIPI_write_cmos_sensor(0x88d4, 0xae);
	OV5645MIPI_write_cmos_sensor(0x88d5, 0x02);
	OV5645MIPI_write_cmos_sensor(0x88d6, 0x78);
	OV5645MIPI_write_cmos_sensor(0x88d7, 0x02);
	OV5645MIPI_write_cmos_sensor(0x88d8, 0xce);
	OV5645MIPI_write_cmos_sensor(0x88d9, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x88da, 0x13);
	OV5645MIPI_write_cmos_sensor(0x88db, 0xce);
	OV5645MIPI_write_cmos_sensor(0x88dc, 0x13);
	OV5645MIPI_write_cmos_sensor(0x88dd, 0xd8);
	OV5645MIPI_write_cmos_sensor(0x88de, 0xf9);
	OV5645MIPI_write_cmos_sensor(0x88df, 0xfb);
	OV5645MIPI_write_cmos_sensor(0x88e0, 0xaa);
	OV5645MIPI_write_cmos_sensor(0x88e1, 0x06);
	OV5645MIPI_write_cmos_sensor(0x88e2, 0x90);
	OV5645MIPI_write_cmos_sensor(0x88e3, 0x38);
	OV5645MIPI_write_cmos_sensor(0x88e4, 0x15);
	OV5645MIPI_write_cmos_sensor(0x88e5, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x88e6, 0xb4);
	OV5645MIPI_write_cmos_sensor(0x88e7, 0x31);
	OV5645MIPI_write_cmos_sensor(0x88e8, 0x0b);
	OV5645MIPI_write_cmos_sensor(0x88e9, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x88ea, 0x0c);
	OV5645MIPI_write_cmos_sensor(0x88eb, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x88ec, 0x13);
	OV5645MIPI_write_cmos_sensor(0x88ed, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x88ee, 0x0c);
	OV5645MIPI_write_cmos_sensor(0x88ef, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x88f0, 0x0d);
	OV5645MIPI_write_cmos_sensor(0x88f1, 0x13);
	OV5645MIPI_write_cmos_sensor(0x88f2, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x88f3, 0x0d);
	OV5645MIPI_write_cmos_sensor(0x88f4, 0x90);
	OV5645MIPI_write_cmos_sensor(0x88f5, 0x38);
	OV5645MIPI_write_cmos_sensor(0x88f6, 0x15);
	OV5645MIPI_write_cmos_sensor(0x88f7, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x88f8, 0xb4);
	OV5645MIPI_write_cmos_sensor(0x88f9, 0x71);
	OV5645MIPI_write_cmos_sensor(0x88fa, 0x11);
	OV5645MIPI_write_cmos_sensor(0x88fb, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x88fc, 0x0d);
	OV5645MIPI_write_cmos_sensor(0x88fd, 0xae);
	OV5645MIPI_write_cmos_sensor(0x88fe, 0x0c);
	OV5645MIPI_write_cmos_sensor(0x88ff, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8900, 0x02);
	OV5645MIPI_write_cmos_sensor(0x8901, 0xce);
	OV5645MIPI_write_cmos_sensor(0x8902, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x8903, 0x13);
	OV5645MIPI_write_cmos_sensor(0x8904, 0xce);
	OV5645MIPI_write_cmos_sensor(0x8905, 0x13);
	OV5645MIPI_write_cmos_sensor(0x8906, 0xd8);
	OV5645MIPI_write_cmos_sensor(0x8907, 0xf9);
	OV5645MIPI_write_cmos_sensor(0x8908, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x8909, 0x0d);
	OV5645MIPI_write_cmos_sensor(0x890a, 0x8e);
	OV5645MIPI_write_cmos_sensor(0x890b, 0x0c);
	OV5645MIPI_write_cmos_sensor(0x890c, 0xea);
	OV5645MIPI_write_cmos_sensor(0x890d, 0xc4);
	OV5645MIPI_write_cmos_sensor(0x890e, 0xf8);
	OV5645MIPI_write_cmos_sensor(0x890f, 0x54);
	OV5645MIPI_write_cmos_sensor(0x8910, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x8911, 0xc8);
	OV5645MIPI_write_cmos_sensor(0x8912, 0x68);
	OV5645MIPI_write_cmos_sensor(0x8913, 0xfa);
	OV5645MIPI_write_cmos_sensor(0x8914, 0xeb);
	OV5645MIPI_write_cmos_sensor(0x8915, 0xc4);
	OV5645MIPI_write_cmos_sensor(0x8916, 0x54);
	OV5645MIPI_write_cmos_sensor(0x8917, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x8918, 0x48);
	OV5645MIPI_write_cmos_sensor(0x8919, 0xfb);
	OV5645MIPI_write_cmos_sensor(0x891a, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x891b, 0x0c);
	OV5645MIPI_write_cmos_sensor(0x891c, 0xc4);
	OV5645MIPI_write_cmos_sensor(0x891d, 0xf8);
	OV5645MIPI_write_cmos_sensor(0x891e, 0x54);
	OV5645MIPI_write_cmos_sensor(0x891f, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x8920, 0xc8);
	OV5645MIPI_write_cmos_sensor(0x8921, 0x68);
	OV5645MIPI_write_cmos_sensor(0x8922, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x8923, 0x0c);
	OV5645MIPI_write_cmos_sensor(0x8924, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x8925, 0x0d);
	OV5645MIPI_write_cmos_sensor(0x8926, 0xc4);
	OV5645MIPI_write_cmos_sensor(0x8927, 0x54);
	OV5645MIPI_write_cmos_sensor(0x8928, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x8929, 0x48);
	OV5645MIPI_write_cmos_sensor(0x892a, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x892b, 0x0d);
	OV5645MIPI_write_cmos_sensor(0x892c, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x892d, 0x41);
	OV5645MIPI_write_cmos_sensor(0x892e, 0x54);
	OV5645MIPI_write_cmos_sensor(0x892f, 0x10);
	OV5645MIPI_write_cmos_sensor(0x8930, 0xd3);
	OV5645MIPI_write_cmos_sensor(0x8931, 0x94);
	OV5645MIPI_write_cmos_sensor(0x8932, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8933, 0x40);
	OV5645MIPI_write_cmos_sensor(0x8934, 0x08);
	OV5645MIPI_write_cmos_sensor(0x8935, 0x85);
	OV5645MIPI_write_cmos_sensor(0x8936, 0x42);
	OV5645MIPI_write_cmos_sensor(0x8937, 0x4a);
	OV5645MIPI_write_cmos_sensor(0x8938, 0x85);
	OV5645MIPI_write_cmos_sensor(0x8939, 0x43);
	OV5645MIPI_write_cmos_sensor(0x893a, 0x4b);
	OV5645MIPI_write_cmos_sensor(0x893b, 0x80);
	OV5645MIPI_write_cmos_sensor(0x893c, 0x0b);
	OV5645MIPI_write_cmos_sensor(0x893d, 0x30);
	OV5645MIPI_write_cmos_sensor(0x893e, 0x39);
	OV5645MIPI_write_cmos_sensor(0x893f, 0x04);
	OV5645MIPI_write_cmos_sensor(0x8940, 0x7f);
	OV5645MIPI_write_cmos_sensor(0x8941, 0x16);
	OV5645MIPI_write_cmos_sensor(0x8942, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8943, 0x02);
	OV5645MIPI_write_cmos_sensor(0x8944, 0x7f);
	OV5645MIPI_write_cmos_sensor(0x8945, 0x1e);
	OV5645MIPI_write_cmos_sensor(0x8946, 0x8f);
	OV5645MIPI_write_cmos_sensor(0x8947, 0x4b);
	OV5645MIPI_write_cmos_sensor(0x8948, 0xaf);
	OV5645MIPI_write_cmos_sensor(0x8949, 0x4a);
	OV5645MIPI_write_cmos_sensor(0x894a, 0x12);
	OV5645MIPI_write_cmos_sensor(0x894b, 0x14);
	OV5645MIPI_write_cmos_sensor(0x894c, 0x76);
	OV5645MIPI_write_cmos_sensor(0x894d, 0xaf);
	OV5645MIPI_write_cmos_sensor(0x894e, 0x4b);
	OV5645MIPI_write_cmos_sensor(0x894f, 0x7e);
	OV5645MIPI_write_cmos_sensor(0x8950, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8951, 0xac);
	OV5645MIPI_write_cmos_sensor(0x8952, 0x0c);
	OV5645MIPI_write_cmos_sensor(0x8953, 0xad);
	OV5645MIPI_write_cmos_sensor(0x8954, 0x0d);
	OV5645MIPI_write_cmos_sensor(0x8955, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8956, 0x14);
	OV5645MIPI_write_cmos_sensor(0x8957, 0x84);
	OV5645MIPI_write_cmos_sensor(0x8958, 0xfd);
	OV5645MIPI_write_cmos_sensor(0x8959, 0x7c);
	OV5645MIPI_write_cmos_sensor(0x895a, 0x00);
	OV5645MIPI_write_cmos_sensor(0x895b, 0xae);
	OV5645MIPI_write_cmos_sensor(0x895c, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x895d, 0xaf);
	OV5645MIPI_write_cmos_sensor(0x895e, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x895f, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8960, 0x0a);
	OV5645MIPI_write_cmos_sensor(0x8961, 0x9b);
	OV5645MIPI_write_cmos_sensor(0x8962, 0x8f);
	OV5645MIPI_write_cmos_sensor(0x8963, 0x4a);
	OV5645MIPI_write_cmos_sensor(0x8964, 0xae);
	OV5645MIPI_write_cmos_sensor(0x8965, 0x10);
	OV5645MIPI_write_cmos_sensor(0x8966, 0xaf);
	OV5645MIPI_write_cmos_sensor(0x8967, 0x11);
	OV5645MIPI_write_cmos_sensor(0x8968, 0x7c);
	OV5645MIPI_write_cmos_sensor(0x8969, 0x00);
	OV5645MIPI_write_cmos_sensor(0x896a, 0x30);
	OV5645MIPI_write_cmos_sensor(0x896b, 0x39);
	OV5645MIPI_write_cmos_sensor(0x896c, 0x04);
	OV5645MIPI_write_cmos_sensor(0x896d, 0x7d);
	OV5645MIPI_write_cmos_sensor(0x896e, 0x2d);
	OV5645MIPI_write_cmos_sensor(0x896f, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8970, 0x02);
	OV5645MIPI_write_cmos_sensor(0x8971, 0x7d);
	OV5645MIPI_write_cmos_sensor(0x8972, 0x3c);
	OV5645MIPI_write_cmos_sensor(0x8973, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8974, 0x0a);
	OV5645MIPI_write_cmos_sensor(0x8975, 0x9b);
	OV5645MIPI_write_cmos_sensor(0x8976, 0x8f);
	OV5645MIPI_write_cmos_sensor(0x8977, 0x4b);
	OV5645MIPI_write_cmos_sensor(0x8978, 0x8b);
	OV5645MIPI_write_cmos_sensor(0x8979, 0x49);
	OV5645MIPI_write_cmos_sensor(0x897a, 0x85);
	OV5645MIPI_write_cmos_sensor(0x897b, 0x0d);
	OV5645MIPI_write_cmos_sensor(0x897c, 0x48);
	OV5645MIPI_write_cmos_sensor(0x897d, 0xaf);
	OV5645MIPI_write_cmos_sensor(0x897e, 0x4c);
	OV5645MIPI_write_cmos_sensor(0x897f, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8980, 0x14);
	OV5645MIPI_write_cmos_sensor(0x8981, 0x76);
	OV5645MIPI_write_cmos_sensor(0x8982, 0xaf);
	OV5645MIPI_write_cmos_sensor(0x8983, 0x4d);
	OV5645MIPI_write_cmos_sensor(0x8984, 0x7e);
	OV5645MIPI_write_cmos_sensor(0x8985, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8986, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8987, 0x14);
	OV5645MIPI_write_cmos_sensor(0x8988, 0x84);
	OV5645MIPI_write_cmos_sensor(0x8989, 0xfb);
	OV5645MIPI_write_cmos_sensor(0x898a, 0xae);
	OV5645MIPI_write_cmos_sensor(0x898b, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x898c, 0xaf);
	OV5645MIPI_write_cmos_sensor(0x898d, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x898e, 0xfd);
	OV5645MIPI_write_cmos_sensor(0x898f, 0x7c);
	OV5645MIPI_write_cmos_sensor(0x8990, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8991, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8992, 0x0a);
	OV5645MIPI_write_cmos_sensor(0x8993, 0x9b);
	OV5645MIPI_write_cmos_sensor(0x8994, 0x8f);
	OV5645MIPI_write_cmos_sensor(0x8995, 0x4c);
	OV5645MIPI_write_cmos_sensor(0x8996, 0xae);
	OV5645MIPI_write_cmos_sensor(0x8997, 0x10);
	OV5645MIPI_write_cmos_sensor(0x8998, 0xaf);
	OV5645MIPI_write_cmos_sensor(0x8999, 0x11);
	OV5645MIPI_write_cmos_sensor(0x899a, 0xad);
	OV5645MIPI_write_cmos_sensor(0x899b, 0x03);
	OV5645MIPI_write_cmos_sensor(0x899c, 0x7c);
	OV5645MIPI_write_cmos_sensor(0x899d, 0x00);
	OV5645MIPI_write_cmos_sensor(0x899e, 0x12);
	OV5645MIPI_write_cmos_sensor(0x899f, 0x0a);
	OV5645MIPI_write_cmos_sensor(0x89a0, 0x9b);
	OV5645MIPI_write_cmos_sensor(0x89a1, 0x8f);
	OV5645MIPI_write_cmos_sensor(0x89a2, 0x4d);
	OV5645MIPI_write_cmos_sensor(0x89a3, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x89a4, 0x4c);
	OV5645MIPI_write_cmos_sensor(0x89a5, 0x75);
	OV5645MIPI_write_cmos_sensor(0x89a6, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x89a7, 0x02);
	OV5645MIPI_write_cmos_sensor(0x89a8, 0xa4);
	OV5645MIPI_write_cmos_sensor(0x89a9, 0xad);
	OV5645MIPI_write_cmos_sensor(0x89aa, 0x49);
	OV5645MIPI_write_cmos_sensor(0x89ab, 0x7c);
	OV5645MIPI_write_cmos_sensor(0x89ac, 0x00);
	OV5645MIPI_write_cmos_sensor(0x89ad, 0xd3);
	OV5645MIPI_write_cmos_sensor(0x89ae, 0x9d);
	OV5645MIPI_write_cmos_sensor(0x89af, 0x74);
	OV5645MIPI_write_cmos_sensor(0x89b0, 0x80);
	OV5645MIPI_write_cmos_sensor(0x89b1, 0xf8);
	OV5645MIPI_write_cmos_sensor(0x89b2, 0x65);
	OV5645MIPI_write_cmos_sensor(0x89b3, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x89b4, 0x98);
	OV5645MIPI_write_cmos_sensor(0x89b5, 0x40);
	OV5645MIPI_write_cmos_sensor(0x89b6, 0x05);
	OV5645MIPI_write_cmos_sensor(0x89b7, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x89b8, 0x49);
	OV5645MIPI_write_cmos_sensor(0x89b9, 0x13);
	OV5645MIPI_write_cmos_sensor(0x89ba, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x89bb, 0x4c);
	OV5645MIPI_write_cmos_sensor(0x89bc, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x89bd, 0x4d);
	OV5645MIPI_write_cmos_sensor(0x89be, 0x75);
	OV5645MIPI_write_cmos_sensor(0x89bf, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x89c0, 0x02);
	OV5645MIPI_write_cmos_sensor(0x89c1, 0xa4);
	OV5645MIPI_write_cmos_sensor(0x89c2, 0xd3);
	OV5645MIPI_write_cmos_sensor(0x89c3, 0x95);
	OV5645MIPI_write_cmos_sensor(0x89c4, 0x48);
	OV5645MIPI_write_cmos_sensor(0x89c5, 0x74);
	OV5645MIPI_write_cmos_sensor(0x89c6, 0x80);
	OV5645MIPI_write_cmos_sensor(0x89c7, 0xf8);
	OV5645MIPI_write_cmos_sensor(0x89c8, 0x65);
	OV5645MIPI_write_cmos_sensor(0x89c9, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x89ca, 0x98);
	OV5645MIPI_write_cmos_sensor(0x89cb, 0x40);
	OV5645MIPI_write_cmos_sensor(0x89cc, 0x05);
	OV5645MIPI_write_cmos_sensor(0x89cd, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x89ce, 0x48);
	OV5645MIPI_write_cmos_sensor(0x89cf, 0x13);
	OV5645MIPI_write_cmos_sensor(0x89d0, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x89d1, 0x4d);
	OV5645MIPI_write_cmos_sensor(0x89d2, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x89d3, 0x4a);
	OV5645MIPI_write_cmos_sensor(0x89d4, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x89d5, 0x95);
	OV5645MIPI_write_cmos_sensor(0x89d6, 0x4c);
	OV5645MIPI_write_cmos_sensor(0x89d7, 0x50);
	OV5645MIPI_write_cmos_sensor(0x89d8, 0x03);
	OV5645MIPI_write_cmos_sensor(0x89d9, 0x85);
	OV5645MIPI_write_cmos_sensor(0x89da, 0x4c);
	OV5645MIPI_write_cmos_sensor(0x89db, 0x4a);
	OV5645MIPI_write_cmos_sensor(0x89dc, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x89dd, 0x4b);
	OV5645MIPI_write_cmos_sensor(0x89de, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x89df, 0x95);
	OV5645MIPI_write_cmos_sensor(0x89e0, 0x4d);
	OV5645MIPI_write_cmos_sensor(0x89e1, 0x50);
	OV5645MIPI_write_cmos_sensor(0x89e2, 0x03);
	OV5645MIPI_write_cmos_sensor(0x89e3, 0x85);
	OV5645MIPI_write_cmos_sensor(0x89e4, 0x4d);
	OV5645MIPI_write_cmos_sensor(0x89e5, 0x4b);
	OV5645MIPI_write_cmos_sensor(0x89e6, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x89e7, 0x4a);
	OV5645MIPI_write_cmos_sensor(0x89e8, 0x25);
	OV5645MIPI_write_cmos_sensor(0x89e9, 0x4c);
	OV5645MIPI_write_cmos_sensor(0x89ea, 0xff);
	OV5645MIPI_write_cmos_sensor(0x89eb, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x89ec, 0x33);
	OV5645MIPI_write_cmos_sensor(0x89ed, 0xfe);
	OV5645MIPI_write_cmos_sensor(0x89ee, 0xd3);
	OV5645MIPI_write_cmos_sensor(0x89ef, 0xef);
	OV5645MIPI_write_cmos_sensor(0x89f0, 0x9d);
	OV5645MIPI_write_cmos_sensor(0x89f1, 0xec);
	OV5645MIPI_write_cmos_sensor(0x89f2, 0x64);
	OV5645MIPI_write_cmos_sensor(0x89f3, 0x80);
	OV5645MIPI_write_cmos_sensor(0x89f4, 0xf8);
	OV5645MIPI_write_cmos_sensor(0x89f5, 0xee);
	OV5645MIPI_write_cmos_sensor(0x89f6, 0x64);
	OV5645MIPI_write_cmos_sensor(0x89f7, 0x80);
	OV5645MIPI_write_cmos_sensor(0x89f8, 0x98);
	OV5645MIPI_write_cmos_sensor(0x89f9, 0x40);
	OV5645MIPI_write_cmos_sensor(0x89fa, 0x06);
	OV5645MIPI_write_cmos_sensor(0x89fb, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x89fc, 0x49);
	OV5645MIPI_write_cmos_sensor(0x89fd, 0x95);
	OV5645MIPI_write_cmos_sensor(0x89fe, 0x4c);
	OV5645MIPI_write_cmos_sensor(0x89ff, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x8a00, 0x4a);
	OV5645MIPI_write_cmos_sensor(0x8a01, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x8a02, 0x4b);
	OV5645MIPI_write_cmos_sensor(0x8a03, 0x25);
	OV5645MIPI_write_cmos_sensor(0x8a04, 0x4d);
	OV5645MIPI_write_cmos_sensor(0x8a05, 0xff);
	OV5645MIPI_write_cmos_sensor(0x8a06, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x8a07, 0x33);
	OV5645MIPI_write_cmos_sensor(0x8a08, 0xfe);
	OV5645MIPI_write_cmos_sensor(0x8a09, 0xd3);
	OV5645MIPI_write_cmos_sensor(0x8a0a, 0xef);
	OV5645MIPI_write_cmos_sensor(0x8a0b, 0x95);
	OV5645MIPI_write_cmos_sensor(0x8a0c, 0x48);
	OV5645MIPI_write_cmos_sensor(0x8a0d, 0x74);
	OV5645MIPI_write_cmos_sensor(0x8a0e, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8a0f, 0xf8);
	OV5645MIPI_write_cmos_sensor(0x8a10, 0x6e);
	OV5645MIPI_write_cmos_sensor(0x8a11, 0x98);
	OV5645MIPI_write_cmos_sensor(0x8a12, 0x40);
	OV5645MIPI_write_cmos_sensor(0x8a13, 0x06);
	OV5645MIPI_write_cmos_sensor(0x8a14, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x8a15, 0x48);
	OV5645MIPI_write_cmos_sensor(0x8a16, 0x95);
	OV5645MIPI_write_cmos_sensor(0x8a17, 0x4d);
	OV5645MIPI_write_cmos_sensor(0x8a18, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x8a19, 0x4b);
	OV5645MIPI_write_cmos_sensor(0x8a1a, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x8a1b, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x8a1c, 0x4a);
	OV5645MIPI_write_cmos_sensor(0x8a1d, 0x95);
	OV5645MIPI_write_cmos_sensor(0x8a1e, 0x4c);
	OV5645MIPI_write_cmos_sensor(0x8a1f, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x8a20, 0x0a);
	OV5645MIPI_write_cmos_sensor(0x8a21, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x8a22, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x8a23, 0x4b);
	OV5645MIPI_write_cmos_sensor(0x8a24, 0x95);
	OV5645MIPI_write_cmos_sensor(0x8a25, 0x4d);
	OV5645MIPI_write_cmos_sensor(0x8a26, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x8a27, 0x0b);
	OV5645MIPI_write_cmos_sensor(0x8a28, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x8a29, 0x4a);
	OV5645MIPI_write_cmos_sensor(0x8a2a, 0x25);
	OV5645MIPI_write_cmos_sensor(0x8a2b, 0x4c);
	OV5645MIPI_write_cmos_sensor(0x8a2c, 0xf9);
	OV5645MIPI_write_cmos_sensor(0x8a2d, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x8a2e, 0x4b);
	OV5645MIPI_write_cmos_sensor(0x8a2f, 0x25);
	OV5645MIPI_write_cmos_sensor(0x8a30, 0x4d);
	OV5645MIPI_write_cmos_sensor(0x8a31, 0xfd);
	OV5645MIPI_write_cmos_sensor(0x8a32, 0x90);
	OV5645MIPI_write_cmos_sensor(0x8a33, 0x60);
	OV5645MIPI_write_cmos_sensor(0x8a34, 0x01);
	OV5645MIPI_write_cmos_sensor(0x8a35, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x8a36, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x8a37, 0xa3);
	OV5645MIPI_write_cmos_sensor(0x8a38, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x8a39, 0xa3);
	OV5645MIPI_write_cmos_sensor(0x8a3a, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x8a3b, 0x49);
	OV5645MIPI_write_cmos_sensor(0x8a3c, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x8a3d, 0xa3);
	OV5645MIPI_write_cmos_sensor(0x8a3e, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x8a3f, 0x48);
	OV5645MIPI_write_cmos_sensor(0x8a40, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x8a41, 0x7c);
	OV5645MIPI_write_cmos_sensor(0x8a42, 0x01);
	OV5645MIPI_write_cmos_sensor(0x8a43, 0xec);
	OV5645MIPI_write_cmos_sensor(0x8a44, 0x75);
	OV5645MIPI_write_cmos_sensor(0x8a45, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x8a46, 0x04);
	OV5645MIPI_write_cmos_sensor(0x8a47, 0xa4);
	OV5645MIPI_write_cmos_sensor(0x8a48, 0xff);
	OV5645MIPI_write_cmos_sensor(0x8a49, 0x24);
	OV5645MIPI_write_cmos_sensor(0x8a4a, 0x01);
	OV5645MIPI_write_cmos_sensor(0x8a4b, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8a4c, 0x14);
	OV5645MIPI_write_cmos_sensor(0x8a4d, 0x9d);
	OV5645MIPI_write_cmos_sensor(0x8a4e, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x8a4f, 0x0a);
	OV5645MIPI_write_cmos_sensor(0x8a50, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x8a51, 0xef);
	OV5645MIPI_write_cmos_sensor(0x8a52, 0x24);
	OV5645MIPI_write_cmos_sensor(0x8a53, 0x02);
	OV5645MIPI_write_cmos_sensor(0x8a54, 0xff);
	OV5645MIPI_write_cmos_sensor(0x8a55, 0xee);
	OV5645MIPI_write_cmos_sensor(0x8a56, 0x34);
	OV5645MIPI_write_cmos_sensor(0x8a57, 0x60);
	OV5645MIPI_write_cmos_sensor(0x8a58, 0x8f);
	OV5645MIPI_write_cmos_sensor(0x8a59, 0x82);
	OV5645MIPI_write_cmos_sensor(0x8a5a, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x8a5b, 0x83);
	OV5645MIPI_write_cmos_sensor(0x8a5c, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x8a5d, 0x0b);
	OV5645MIPI_write_cmos_sensor(0x8a5e, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x8a5f, 0xec);
	OV5645MIPI_write_cmos_sensor(0x8a60, 0x75);
	OV5645MIPI_write_cmos_sensor(0x8a61, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x8a62, 0x04);
	OV5645MIPI_write_cmos_sensor(0x8a63, 0xa4);
	OV5645MIPI_write_cmos_sensor(0x8a64, 0xff);
	OV5645MIPI_write_cmos_sensor(0x8a65, 0x24);
	OV5645MIPI_write_cmos_sensor(0x8a66, 0x03);
	OV5645MIPI_write_cmos_sensor(0x8a67, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8a68, 0x14);
	OV5645MIPI_write_cmos_sensor(0x8a69, 0x9d);
	OV5645MIPI_write_cmos_sensor(0x8a6a, 0xe9);
	OV5645MIPI_write_cmos_sensor(0x8a6b, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x8a6c, 0xef);
	OV5645MIPI_write_cmos_sensor(0x8a6d, 0x24);
	OV5645MIPI_write_cmos_sensor(0x8a6e, 0x04);
	OV5645MIPI_write_cmos_sensor(0x8a6f, 0xff);
	OV5645MIPI_write_cmos_sensor(0x8a70, 0xee);
	OV5645MIPI_write_cmos_sensor(0x8a71, 0x34);
	OV5645MIPI_write_cmos_sensor(0x8a72, 0x60);
	OV5645MIPI_write_cmos_sensor(0x8a73, 0x8f);
	OV5645MIPI_write_cmos_sensor(0x8a74, 0x82);
	OV5645MIPI_write_cmos_sensor(0x8a75, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x8a76, 0x83);
	OV5645MIPI_write_cmos_sensor(0x8a77, 0xed);
	OV5645MIPI_write_cmos_sensor(0x8a78, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x8a79, 0x0c);
	OV5645MIPI_write_cmos_sensor(0x8a7a, 0xbc);
	OV5645MIPI_write_cmos_sensor(0x8a7b, 0x05);
	OV5645MIPI_write_cmos_sensor(0x8a7c, 0xc6);
	OV5645MIPI_write_cmos_sensor(0x8a7d, 0x90);
	OV5645MIPI_write_cmos_sensor(0x8a7e, 0x30);
	OV5645MIPI_write_cmos_sensor(0x8a7f, 0x01);
	OV5645MIPI_write_cmos_sensor(0x8a80, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x8a81, 0x44);
	OV5645MIPI_write_cmos_sensor(0x8a82, 0x40);
	OV5645MIPI_write_cmos_sensor(0x8a83, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x8a84, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x8a85, 0x54);
	OV5645MIPI_write_cmos_sensor(0x8a86, 0xbf);
	OV5645MIPI_write_cmos_sensor(0x8a87, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x8a88, 0x22);
	OV5645MIPI_write_cmos_sensor(0x8a89, 0xef);
	OV5645MIPI_write_cmos_sensor(0x8a8a, 0x8d);
	OV5645MIPI_write_cmos_sensor(0x8a8b, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x8a8c, 0xa4);
	OV5645MIPI_write_cmos_sensor(0x8a8d, 0xa8);
	OV5645MIPI_write_cmos_sensor(0x8a8e, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x8a8f, 0xcf);
	OV5645MIPI_write_cmos_sensor(0x8a90, 0x8c);
	OV5645MIPI_write_cmos_sensor(0x8a91, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x8a92, 0xa4);
	OV5645MIPI_write_cmos_sensor(0x8a93, 0x28);
	OV5645MIPI_write_cmos_sensor(0x8a94, 0xce);
	OV5645MIPI_write_cmos_sensor(0x8a95, 0x8d);
	OV5645MIPI_write_cmos_sensor(0x8a96, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x8a97, 0xa4);
	OV5645MIPI_write_cmos_sensor(0x8a98, 0x2e);
	OV5645MIPI_write_cmos_sensor(0x8a99, 0xfe);
	OV5645MIPI_write_cmos_sensor(0x8a9a, 0x22);
	OV5645MIPI_write_cmos_sensor(0x8a9b, 0xbc);
	OV5645MIPI_write_cmos_sensor(0x8a9c, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8a9d, 0x0b);
	OV5645MIPI_write_cmos_sensor(0x8a9e, 0xbe);
	OV5645MIPI_write_cmos_sensor(0x8a9f, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8aa0, 0x29);
	OV5645MIPI_write_cmos_sensor(0x8aa1, 0xef);
	OV5645MIPI_write_cmos_sensor(0x8aa2, 0x8d);
	OV5645MIPI_write_cmos_sensor(0x8aa3, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x8aa4, 0x84);
	OV5645MIPI_write_cmos_sensor(0x8aa5, 0xff);
	OV5645MIPI_write_cmos_sensor(0x8aa6, 0xad);
	OV5645MIPI_write_cmos_sensor(0x8aa7, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x8aa8, 0x22);
	OV5645MIPI_write_cmos_sensor(0x8aa9, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x8aaa, 0xcc);
	OV5645MIPI_write_cmos_sensor(0x8aab, 0xf8);
	OV5645MIPI_write_cmos_sensor(0x8aac, 0x75);
	OV5645MIPI_write_cmos_sensor(0x8aad, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x8aae, 0x08);
	OV5645MIPI_write_cmos_sensor(0x8aaf, 0xef);
	OV5645MIPI_write_cmos_sensor(0x8ab0, 0x2f);
	OV5645MIPI_write_cmos_sensor(0x8ab1, 0xff);
	OV5645MIPI_write_cmos_sensor(0x8ab2, 0xee);
	OV5645MIPI_write_cmos_sensor(0x8ab3, 0x33);
	OV5645MIPI_write_cmos_sensor(0x8ab4, 0xfe);
	OV5645MIPI_write_cmos_sensor(0x8ab5, 0xec);
	OV5645MIPI_write_cmos_sensor(0x8ab6, 0x33);
	OV5645MIPI_write_cmos_sensor(0x8ab7, 0xfc);
	OV5645MIPI_write_cmos_sensor(0x8ab8, 0xee);
	OV5645MIPI_write_cmos_sensor(0x8ab9, 0x9d);
	OV5645MIPI_write_cmos_sensor(0x8aba, 0xec);
	OV5645MIPI_write_cmos_sensor(0x8abb, 0x98);
	OV5645MIPI_write_cmos_sensor(0x8abc, 0x40);
	OV5645MIPI_write_cmos_sensor(0x8abd, 0x05);
	OV5645MIPI_write_cmos_sensor(0x8abe, 0xfc);
	OV5645MIPI_write_cmos_sensor(0x8abf, 0xee);
	OV5645MIPI_write_cmos_sensor(0x8ac0, 0x9d);
	OV5645MIPI_write_cmos_sensor(0x8ac1, 0xfe);
	OV5645MIPI_write_cmos_sensor(0x8ac2, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x8ac3, 0xd5);
	OV5645MIPI_write_cmos_sensor(0x8ac4, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x8ac5, 0xe9);
	OV5645MIPI_write_cmos_sensor(0x8ac6, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x8ac7, 0xce);
	OV5645MIPI_write_cmos_sensor(0x8ac8, 0xfd);
	OV5645MIPI_write_cmos_sensor(0x8ac9, 0x22);
	OV5645MIPI_write_cmos_sensor(0x8aca, 0xed);
	OV5645MIPI_write_cmos_sensor(0x8acb, 0xf8);
	OV5645MIPI_write_cmos_sensor(0x8acc, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x8acd, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x8ace, 0xee);
	OV5645MIPI_write_cmos_sensor(0x8acf, 0x84);
	OV5645MIPI_write_cmos_sensor(0x8ad0, 0x20);
	OV5645MIPI_write_cmos_sensor(0x8ad1, 0xd2);
	OV5645MIPI_write_cmos_sensor(0x8ad2, 0x1c);
	OV5645MIPI_write_cmos_sensor(0x8ad3, 0xfe);
	OV5645MIPI_write_cmos_sensor(0x8ad4, 0xad);
	OV5645MIPI_write_cmos_sensor(0x8ad5, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x8ad6, 0x75);
	OV5645MIPI_write_cmos_sensor(0x8ad7, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x8ad8, 0x08);
	OV5645MIPI_write_cmos_sensor(0x8ad9, 0xef);
	OV5645MIPI_write_cmos_sensor(0x8ada, 0x2f);
	OV5645MIPI_write_cmos_sensor(0x8adb, 0xff);
	OV5645MIPI_write_cmos_sensor(0x8adc, 0xed);
	OV5645MIPI_write_cmos_sensor(0x8add, 0x33);
	OV5645MIPI_write_cmos_sensor(0x8ade, 0xfd);
	OV5645MIPI_write_cmos_sensor(0x8adf, 0x40);
	OV5645MIPI_write_cmos_sensor(0x8ae0, 0x07);
	OV5645MIPI_write_cmos_sensor(0x8ae1, 0x98);
	OV5645MIPI_write_cmos_sensor(0x8ae2, 0x50);
	OV5645MIPI_write_cmos_sensor(0x8ae3, 0x06);
	OV5645MIPI_write_cmos_sensor(0x8ae4, 0xd5);
	OV5645MIPI_write_cmos_sensor(0x8ae5, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x8ae6, 0xf2);
	OV5645MIPI_write_cmos_sensor(0x8ae7, 0x22);
	OV5645MIPI_write_cmos_sensor(0x8ae8, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x8ae9, 0x98);
	OV5645MIPI_write_cmos_sensor(0x8aea, 0xfd);
	OV5645MIPI_write_cmos_sensor(0x8aeb, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x8aec, 0xd5);
	OV5645MIPI_write_cmos_sensor(0x8aed, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x8aee, 0xea);
	OV5645MIPI_write_cmos_sensor(0x8aef, 0x22);
	OV5645MIPI_write_cmos_sensor(0x8af0, 0xe8);
	OV5645MIPI_write_cmos_sensor(0x8af1, 0x8f);
	OV5645MIPI_write_cmos_sensor(0x8af2, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x8af3, 0xa4);
	OV5645MIPI_write_cmos_sensor(0x8af4, 0xcc);
	OV5645MIPI_write_cmos_sensor(0x8af5, 0x8b);
	OV5645MIPI_write_cmos_sensor(0x8af6, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x8af7, 0xa4);
	OV5645MIPI_write_cmos_sensor(0x8af8, 0x2c);
	OV5645MIPI_write_cmos_sensor(0x8af9, 0xfc);
	OV5645MIPI_write_cmos_sensor(0x8afa, 0xe9);
	OV5645MIPI_write_cmos_sensor(0x8afb, 0x8e);
	OV5645MIPI_write_cmos_sensor(0x8afc, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x8afd, 0xa4);
	OV5645MIPI_write_cmos_sensor(0x8afe, 0x2c);
	OV5645MIPI_write_cmos_sensor(0x8aff, 0xfc);
	OV5645MIPI_write_cmos_sensor(0x8b00, 0x8a);
	OV5645MIPI_write_cmos_sensor(0x8b01, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x8b02, 0xed);
	OV5645MIPI_write_cmos_sensor(0x8b03, 0xa4);
	OV5645MIPI_write_cmos_sensor(0x8b04, 0x2c);
	OV5645MIPI_write_cmos_sensor(0x8b05, 0xfc);
	OV5645MIPI_write_cmos_sensor(0x8b06, 0xea);
	OV5645MIPI_write_cmos_sensor(0x8b07, 0x8e);
	OV5645MIPI_write_cmos_sensor(0x8b08, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x8b09, 0xa4);
	OV5645MIPI_write_cmos_sensor(0x8b0a, 0xcd);
	OV5645MIPI_write_cmos_sensor(0x8b0b, 0xa8);
	OV5645MIPI_write_cmos_sensor(0x8b0c, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x8b0d, 0x8b);
	OV5645MIPI_write_cmos_sensor(0x8b0e, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x8b0f, 0xa4);
	OV5645MIPI_write_cmos_sensor(0x8b10, 0x2d);
	OV5645MIPI_write_cmos_sensor(0x8b11, 0xcc);
	OV5645MIPI_write_cmos_sensor(0x8b12, 0x38);
	OV5645MIPI_write_cmos_sensor(0x8b13, 0x25);
	OV5645MIPI_write_cmos_sensor(0x8b14, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x8b15, 0xfd);
	OV5645MIPI_write_cmos_sensor(0x8b16, 0xe9);
	OV5645MIPI_write_cmos_sensor(0x8b17, 0x8f);
	OV5645MIPI_write_cmos_sensor(0x8b18, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x8b19, 0xa4);
	OV5645MIPI_write_cmos_sensor(0x8b1a, 0x2c);
	OV5645MIPI_write_cmos_sensor(0x8b1b, 0xcd);
	OV5645MIPI_write_cmos_sensor(0x8b1c, 0x35);
	OV5645MIPI_write_cmos_sensor(0x8b1d, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x8b1e, 0xfc);
	OV5645MIPI_write_cmos_sensor(0x8b1f, 0xeb);
	OV5645MIPI_write_cmos_sensor(0x8b20, 0x8e);
	OV5645MIPI_write_cmos_sensor(0x8b21, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x8b22, 0xa4);
	OV5645MIPI_write_cmos_sensor(0x8b23, 0xfe);
	OV5645MIPI_write_cmos_sensor(0x8b24, 0xa9);
	OV5645MIPI_write_cmos_sensor(0x8b25, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x8b26, 0xeb);
	OV5645MIPI_write_cmos_sensor(0x8b27, 0x8f);
	OV5645MIPI_write_cmos_sensor(0x8b28, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x8b29, 0xa4);
	OV5645MIPI_write_cmos_sensor(0x8b2a, 0xcf);
	OV5645MIPI_write_cmos_sensor(0x8b2b, 0xc5);
	OV5645MIPI_write_cmos_sensor(0x8b2c, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x8b2d, 0x2e);
	OV5645MIPI_write_cmos_sensor(0x8b2e, 0xcd);
	OV5645MIPI_write_cmos_sensor(0x8b2f, 0x39);
	OV5645MIPI_write_cmos_sensor(0x8b30, 0xfe);
	OV5645MIPI_write_cmos_sensor(0x8b31, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x8b32, 0x3c);
	OV5645MIPI_write_cmos_sensor(0x8b33, 0xfc);
	OV5645MIPI_write_cmos_sensor(0x8b34, 0xea);
	OV5645MIPI_write_cmos_sensor(0x8b35, 0xa4);
	OV5645MIPI_write_cmos_sensor(0x8b36, 0x2d);
	OV5645MIPI_write_cmos_sensor(0x8b37, 0xce);
	OV5645MIPI_write_cmos_sensor(0x8b38, 0x35);
	OV5645MIPI_write_cmos_sensor(0x8b39, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x8b3a, 0xfd);
	OV5645MIPI_write_cmos_sensor(0x8b3b, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x8b3c, 0x3c);
	OV5645MIPI_write_cmos_sensor(0x8b3d, 0xfc);
	OV5645MIPI_write_cmos_sensor(0x8b3e, 0x22);
	OV5645MIPI_write_cmos_sensor(0x8b3f, 0x75);
	OV5645MIPI_write_cmos_sensor(0x8b40, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x8b41, 0x08);
	OV5645MIPI_write_cmos_sensor(0x8b42, 0x75);
	OV5645MIPI_write_cmos_sensor(0x8b43, 0x82);
	OV5645MIPI_write_cmos_sensor(0x8b44, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8b45, 0xef);
	OV5645MIPI_write_cmos_sensor(0x8b46, 0x2f);
	OV5645MIPI_write_cmos_sensor(0x8b47, 0xff);
	OV5645MIPI_write_cmos_sensor(0x8b48, 0xee);
	OV5645MIPI_write_cmos_sensor(0x8b49, 0x33);
	OV5645MIPI_write_cmos_sensor(0x8b4a, 0xfe);
	OV5645MIPI_write_cmos_sensor(0x8b4b, 0xcd);
	OV5645MIPI_write_cmos_sensor(0x8b4c, 0x33);
	OV5645MIPI_write_cmos_sensor(0x8b4d, 0xcd);
	OV5645MIPI_write_cmos_sensor(0x8b4e, 0xcc);
	OV5645MIPI_write_cmos_sensor(0x8b4f, 0x33);
	OV5645MIPI_write_cmos_sensor(0x8b50, 0xcc);
	OV5645MIPI_write_cmos_sensor(0x8b51, 0xc5);
	OV5645MIPI_write_cmos_sensor(0x8b52, 0x82);
	OV5645MIPI_write_cmos_sensor(0x8b53, 0x33);
	OV5645MIPI_write_cmos_sensor(0x8b54, 0xc5);
	OV5645MIPI_write_cmos_sensor(0x8b55, 0x82);
	OV5645MIPI_write_cmos_sensor(0x8b56, 0x9b);
	OV5645MIPI_write_cmos_sensor(0x8b57, 0xed);
	OV5645MIPI_write_cmos_sensor(0x8b58, 0x9a);
	OV5645MIPI_write_cmos_sensor(0x8b59, 0xec);
	OV5645MIPI_write_cmos_sensor(0x8b5a, 0x99);
	OV5645MIPI_write_cmos_sensor(0x8b5b, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x8b5c, 0x82);
	OV5645MIPI_write_cmos_sensor(0x8b5d, 0x98);
	OV5645MIPI_write_cmos_sensor(0x8b5e, 0x40);
	OV5645MIPI_write_cmos_sensor(0x8b5f, 0x0c);
	OV5645MIPI_write_cmos_sensor(0x8b60, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x8b61, 0x82);
	OV5645MIPI_write_cmos_sensor(0x8b62, 0xee);
	OV5645MIPI_write_cmos_sensor(0x8b63, 0x9b);
	OV5645MIPI_write_cmos_sensor(0x8b64, 0xfe);
	OV5645MIPI_write_cmos_sensor(0x8b65, 0xed);
	OV5645MIPI_write_cmos_sensor(0x8b66, 0x9a);
	OV5645MIPI_write_cmos_sensor(0x8b67, 0xfd);
	OV5645MIPI_write_cmos_sensor(0x8b68, 0xec);
	OV5645MIPI_write_cmos_sensor(0x8b69, 0x99);
	OV5645MIPI_write_cmos_sensor(0x8b6a, 0xfc);
	OV5645MIPI_write_cmos_sensor(0x8b6b, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x8b6c, 0xd5);
	OV5645MIPI_write_cmos_sensor(0x8b6d, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x8b6e, 0xd6);
	OV5645MIPI_write_cmos_sensor(0x8b6f, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x8b70, 0xce);
	OV5645MIPI_write_cmos_sensor(0x8b71, 0xfb);
	OV5645MIPI_write_cmos_sensor(0x8b72, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x8b73, 0xcd);
	OV5645MIPI_write_cmos_sensor(0x8b74, 0xfa);
	OV5645MIPI_write_cmos_sensor(0x8b75, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x8b76, 0xcc);
	OV5645MIPI_write_cmos_sensor(0x8b77, 0xf9);
	OV5645MIPI_write_cmos_sensor(0x8b78, 0xa8);
	OV5645MIPI_write_cmos_sensor(0x8b79, 0x82);
	OV5645MIPI_write_cmos_sensor(0x8b7a, 0x22);
	OV5645MIPI_write_cmos_sensor(0x8b7b, 0xb8);
	OV5645MIPI_write_cmos_sensor(0x8b7c, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8b7d, 0xc1);
	OV5645MIPI_write_cmos_sensor(0x8b7e, 0xb9);
	OV5645MIPI_write_cmos_sensor(0x8b7f, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8b80, 0x59);
	OV5645MIPI_write_cmos_sensor(0x8b81, 0xba);
	OV5645MIPI_write_cmos_sensor(0x8b82, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8b83, 0x2d);
	OV5645MIPI_write_cmos_sensor(0x8b84, 0xec);
	OV5645MIPI_write_cmos_sensor(0x8b85, 0x8b);
	OV5645MIPI_write_cmos_sensor(0x8b86, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x8b87, 0x84);
	OV5645MIPI_write_cmos_sensor(0x8b88, 0xcf);
	OV5645MIPI_write_cmos_sensor(0x8b89, 0xce);
	OV5645MIPI_write_cmos_sensor(0x8b8a, 0xcd);
	OV5645MIPI_write_cmos_sensor(0x8b8b, 0xfc);
	OV5645MIPI_write_cmos_sensor(0x8b8c, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x8b8d, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x8b8e, 0xcb);
	OV5645MIPI_write_cmos_sensor(0x8b8f, 0xf9);
	OV5645MIPI_write_cmos_sensor(0x8b90, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8b91, 0x18);
	OV5645MIPI_write_cmos_sensor(0x8b92, 0xef);
	OV5645MIPI_write_cmos_sensor(0x8b93, 0x2f);
	OV5645MIPI_write_cmos_sensor(0x8b94, 0xff);
	OV5645MIPI_write_cmos_sensor(0x8b95, 0xee);
	OV5645MIPI_write_cmos_sensor(0x8b96, 0x33);
	OV5645MIPI_write_cmos_sensor(0x8b97, 0xfe);
	OV5645MIPI_write_cmos_sensor(0x8b98, 0xed);
	OV5645MIPI_write_cmos_sensor(0x8b99, 0x33);
	OV5645MIPI_write_cmos_sensor(0x8b9a, 0xfd);
	OV5645MIPI_write_cmos_sensor(0x8b9b, 0xec);
	OV5645MIPI_write_cmos_sensor(0x8b9c, 0x33);
	OV5645MIPI_write_cmos_sensor(0x8b9d, 0xfc);
	OV5645MIPI_write_cmos_sensor(0x8b9e, 0xeb);
	OV5645MIPI_write_cmos_sensor(0x8b9f, 0x33);
	OV5645MIPI_write_cmos_sensor(0x8ba0, 0xfb);
	OV5645MIPI_write_cmos_sensor(0x8ba1, 0x10);
	OV5645MIPI_write_cmos_sensor(0x8ba2, 0xd7);
	OV5645MIPI_write_cmos_sensor(0x8ba3, 0x03);
	OV5645MIPI_write_cmos_sensor(0x8ba4, 0x99);
	OV5645MIPI_write_cmos_sensor(0x8ba5, 0x40);
	OV5645MIPI_write_cmos_sensor(0x8ba6, 0x04);
	OV5645MIPI_write_cmos_sensor(0x8ba7, 0xeb);
	OV5645MIPI_write_cmos_sensor(0x8ba8, 0x99);
	OV5645MIPI_write_cmos_sensor(0x8ba9, 0xfb);
	OV5645MIPI_write_cmos_sensor(0x8baa, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x8bab, 0xd8);
	OV5645MIPI_write_cmos_sensor(0x8bac, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x8bad, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x8bae, 0xf9);
	OV5645MIPI_write_cmos_sensor(0x8baf, 0xfa);
	OV5645MIPI_write_cmos_sensor(0x8bb0, 0x22);
	OV5645MIPI_write_cmos_sensor(0x8bb1, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8bb2, 0x18);
	OV5645MIPI_write_cmos_sensor(0x8bb3, 0xef);
	OV5645MIPI_write_cmos_sensor(0x8bb4, 0x2f);
	OV5645MIPI_write_cmos_sensor(0x8bb5, 0xff);
	OV5645MIPI_write_cmos_sensor(0x8bb6, 0xee);
	OV5645MIPI_write_cmos_sensor(0x8bb7, 0x33);
	OV5645MIPI_write_cmos_sensor(0x8bb8, 0xfe);
	OV5645MIPI_write_cmos_sensor(0x8bb9, 0xed);
	OV5645MIPI_write_cmos_sensor(0x8bba, 0x33);
	OV5645MIPI_write_cmos_sensor(0x8bbb, 0xfd);
	OV5645MIPI_write_cmos_sensor(0x8bbc, 0xec);
	OV5645MIPI_write_cmos_sensor(0x8bbd, 0x33);
	OV5645MIPI_write_cmos_sensor(0x8bbe, 0xfc);
	OV5645MIPI_write_cmos_sensor(0x8bbf, 0xc9);
	OV5645MIPI_write_cmos_sensor(0x8bc0, 0x33);
	OV5645MIPI_write_cmos_sensor(0x8bc1, 0xc9);
	OV5645MIPI_write_cmos_sensor(0x8bc2, 0x10);
	OV5645MIPI_write_cmos_sensor(0x8bc3, 0xd7);
	OV5645MIPI_write_cmos_sensor(0x8bc4, 0x05);
	OV5645MIPI_write_cmos_sensor(0x8bc5, 0x9b);
	OV5645MIPI_write_cmos_sensor(0x8bc6, 0xe9);
	OV5645MIPI_write_cmos_sensor(0x8bc7, 0x9a);
	OV5645MIPI_write_cmos_sensor(0x8bc8, 0x40);
	OV5645MIPI_write_cmos_sensor(0x8bc9, 0x07);
	OV5645MIPI_write_cmos_sensor(0x8bca, 0xec);
	OV5645MIPI_write_cmos_sensor(0x8bcb, 0x9b);
	OV5645MIPI_write_cmos_sensor(0x8bcc, 0xfc);
	OV5645MIPI_write_cmos_sensor(0x8bcd, 0xe9);
	OV5645MIPI_write_cmos_sensor(0x8bce, 0x9a);
	OV5645MIPI_write_cmos_sensor(0x8bcf, 0xf9);
	OV5645MIPI_write_cmos_sensor(0x8bd0, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x8bd1, 0xd8);
	OV5645MIPI_write_cmos_sensor(0x8bd2, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x8bd3, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x8bd4, 0xc9);
	OV5645MIPI_write_cmos_sensor(0x8bd5, 0xfa);
	OV5645MIPI_write_cmos_sensor(0x8bd6, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x8bd7, 0xcc);
	OV5645MIPI_write_cmos_sensor(0x8bd8, 0xfb);
	OV5645MIPI_write_cmos_sensor(0x8bd9, 0x22);
	OV5645MIPI_write_cmos_sensor(0x8bda, 0x75);
	OV5645MIPI_write_cmos_sensor(0x8bdb, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x8bdc, 0x10);
	OV5645MIPI_write_cmos_sensor(0x8bdd, 0xef);
	OV5645MIPI_write_cmos_sensor(0x8bde, 0x2f);
	OV5645MIPI_write_cmos_sensor(0x8bdf, 0xff);
	OV5645MIPI_write_cmos_sensor(0x8be0, 0xee);
	OV5645MIPI_write_cmos_sensor(0x8be1, 0x33);
	OV5645MIPI_write_cmos_sensor(0x8be2, 0xfe);
	OV5645MIPI_write_cmos_sensor(0x8be3, 0xed);
	OV5645MIPI_write_cmos_sensor(0x8be4, 0x33);
	OV5645MIPI_write_cmos_sensor(0x8be5, 0xfd);
	OV5645MIPI_write_cmos_sensor(0x8be6, 0xcc);
	OV5645MIPI_write_cmos_sensor(0x8be7, 0x33);
	OV5645MIPI_write_cmos_sensor(0x8be8, 0xcc);
	OV5645MIPI_write_cmos_sensor(0x8be9, 0xc8);
	OV5645MIPI_write_cmos_sensor(0x8bea, 0x33);
	OV5645MIPI_write_cmos_sensor(0x8beb, 0xc8);
	OV5645MIPI_write_cmos_sensor(0x8bec, 0x10);
	OV5645MIPI_write_cmos_sensor(0x8bed, 0xd7);
	OV5645MIPI_write_cmos_sensor(0x8bee, 0x07);
	OV5645MIPI_write_cmos_sensor(0x8bef, 0x9b);
	OV5645MIPI_write_cmos_sensor(0x8bf0, 0xec);
	OV5645MIPI_write_cmos_sensor(0x8bf1, 0x9a);
	OV5645MIPI_write_cmos_sensor(0x8bf2, 0xe8);
	OV5645MIPI_write_cmos_sensor(0x8bf3, 0x99);
	OV5645MIPI_write_cmos_sensor(0x8bf4, 0x40);
	OV5645MIPI_write_cmos_sensor(0x8bf5, 0x0a);
	OV5645MIPI_write_cmos_sensor(0x8bf6, 0xed);
	OV5645MIPI_write_cmos_sensor(0x8bf7, 0x9b);
	OV5645MIPI_write_cmos_sensor(0x8bf8, 0xfd);
	OV5645MIPI_write_cmos_sensor(0x8bf9, 0xec);
	OV5645MIPI_write_cmos_sensor(0x8bfa, 0x9a);
	OV5645MIPI_write_cmos_sensor(0x8bfb, 0xfc);
	OV5645MIPI_write_cmos_sensor(0x8bfc, 0xe8);
	OV5645MIPI_write_cmos_sensor(0x8bfd, 0x99);
	OV5645MIPI_write_cmos_sensor(0x8bfe, 0xf8);
	OV5645MIPI_write_cmos_sensor(0x8bff, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x8c00, 0xd5);
	OV5645MIPI_write_cmos_sensor(0x8c01, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x8c02, 0xda);
	OV5645MIPI_write_cmos_sensor(0x8c03, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x8c04, 0xcd);
	OV5645MIPI_write_cmos_sensor(0x8c05, 0xfb);
	OV5645MIPI_write_cmos_sensor(0x8c06, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x8c07, 0xcc);
	OV5645MIPI_write_cmos_sensor(0x8c08, 0xfa);
	OV5645MIPI_write_cmos_sensor(0x8c09, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x8c0a, 0xc8);
	OV5645MIPI_write_cmos_sensor(0x8c0b, 0xf9);
	OV5645MIPI_write_cmos_sensor(0x8c0c, 0x22);
	OV5645MIPI_write_cmos_sensor(0x8c0d, 0xeb);
	OV5645MIPI_write_cmos_sensor(0x8c0e, 0x9f);
	OV5645MIPI_write_cmos_sensor(0x8c0f, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x8c10, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x8c11, 0xea);
	OV5645MIPI_write_cmos_sensor(0x8c12, 0x9e);
	OV5645MIPI_write_cmos_sensor(0x8c13, 0x42);
	OV5645MIPI_write_cmos_sensor(0x8c14, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x8c15, 0xe9);
	OV5645MIPI_write_cmos_sensor(0x8c16, 0x9d);
	OV5645MIPI_write_cmos_sensor(0x8c17, 0x42);
	OV5645MIPI_write_cmos_sensor(0x8c18, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x8c19, 0xe8);
	OV5645MIPI_write_cmos_sensor(0x8c1a, 0x9c);
	OV5645MIPI_write_cmos_sensor(0x8c1b, 0x45);
	OV5645MIPI_write_cmos_sensor(0x8c1c, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x8c1d, 0x22);
	OV5645MIPI_write_cmos_sensor(0x8c1e, 0xe8);
	OV5645MIPI_write_cmos_sensor(0x8c1f, 0x60);
	OV5645MIPI_write_cmos_sensor(0x8c20, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x8c21, 0xec);
	OV5645MIPI_write_cmos_sensor(0x8c22, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x8c23, 0x13);
	OV5645MIPI_write_cmos_sensor(0x8c24, 0xfc);
	OV5645MIPI_write_cmos_sensor(0x8c25, 0xed);
	OV5645MIPI_write_cmos_sensor(0x8c26, 0x13);
	OV5645MIPI_write_cmos_sensor(0x8c27, 0xfd);
	OV5645MIPI_write_cmos_sensor(0x8c28, 0xee);
	OV5645MIPI_write_cmos_sensor(0x8c29, 0x13);
	OV5645MIPI_write_cmos_sensor(0x8c2a, 0xfe);
	OV5645MIPI_write_cmos_sensor(0x8c2b, 0xef);
	OV5645MIPI_write_cmos_sensor(0x8c2c, 0x13);
	OV5645MIPI_write_cmos_sensor(0x8c2d, 0xff);
	OV5645MIPI_write_cmos_sensor(0x8c2e, 0xd8);
	OV5645MIPI_write_cmos_sensor(0x8c2f, 0xf1);
	OV5645MIPI_write_cmos_sensor(0x8c30, 0x22);
	OV5645MIPI_write_cmos_sensor(0x8c31, 0xe8);
	OV5645MIPI_write_cmos_sensor(0x8c32, 0x60);
	OV5645MIPI_write_cmos_sensor(0x8c33, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x8c34, 0xef);
	OV5645MIPI_write_cmos_sensor(0x8c35, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x8c36, 0x33);
	OV5645MIPI_write_cmos_sensor(0x8c37, 0xff);
	OV5645MIPI_write_cmos_sensor(0x8c38, 0xee);
	OV5645MIPI_write_cmos_sensor(0x8c39, 0x33);
	OV5645MIPI_write_cmos_sensor(0x8c3a, 0xfe);
	OV5645MIPI_write_cmos_sensor(0x8c3b, 0xed);
	OV5645MIPI_write_cmos_sensor(0x8c3c, 0x33);
	OV5645MIPI_write_cmos_sensor(0x8c3d, 0xfd);
	OV5645MIPI_write_cmos_sensor(0x8c3e, 0xec);
	OV5645MIPI_write_cmos_sensor(0x8c3f, 0x33);
	OV5645MIPI_write_cmos_sensor(0x8c40, 0xfc);
	OV5645MIPI_write_cmos_sensor(0x8c41, 0xd8);
	OV5645MIPI_write_cmos_sensor(0x8c42, 0xf1);
	OV5645MIPI_write_cmos_sensor(0x8c43, 0x22);
	OV5645MIPI_write_cmos_sensor(0x8c44, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x8c45, 0x93);
	OV5645MIPI_write_cmos_sensor(0x8c46, 0xfc);
	OV5645MIPI_write_cmos_sensor(0x8c47, 0x74);
	OV5645MIPI_write_cmos_sensor(0x8c48, 0x01);
	OV5645MIPI_write_cmos_sensor(0x8c49, 0x93);
	OV5645MIPI_write_cmos_sensor(0x8c4a, 0xfd);
	OV5645MIPI_write_cmos_sensor(0x8c4b, 0x74);
	OV5645MIPI_write_cmos_sensor(0x8c4c, 0x02);
	OV5645MIPI_write_cmos_sensor(0x8c4d, 0x93);
	OV5645MIPI_write_cmos_sensor(0x8c4e, 0xfe);
	OV5645MIPI_write_cmos_sensor(0x8c4f, 0x74);
	OV5645MIPI_write_cmos_sensor(0x8c50, 0x03);
	OV5645MIPI_write_cmos_sensor(0x8c51, 0x93);
	OV5645MIPI_write_cmos_sensor(0x8c52, 0xff);
	OV5645MIPI_write_cmos_sensor(0x8c53, 0x22);
	OV5645MIPI_write_cmos_sensor(0x8c54, 0xa4);
	OV5645MIPI_write_cmos_sensor(0x8c55, 0x25);
	OV5645MIPI_write_cmos_sensor(0x8c56, 0x82);
	OV5645MIPI_write_cmos_sensor(0x8c57, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x8c58, 0x82);
	OV5645MIPI_write_cmos_sensor(0x8c59, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x8c5a, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x8c5b, 0x35);
	OV5645MIPI_write_cmos_sensor(0x8c5c, 0x83);
	OV5645MIPI_write_cmos_sensor(0x8c5d, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x8c5e, 0x83);
	OV5645MIPI_write_cmos_sensor(0x8c5f, 0x22);
	OV5645MIPI_write_cmos_sensor(0x8c60, 0xd0);
	OV5645MIPI_write_cmos_sensor(0x8c61, 0x83);
	OV5645MIPI_write_cmos_sensor(0x8c62, 0xd0);
	OV5645MIPI_write_cmos_sensor(0x8c63, 0x82);
	OV5645MIPI_write_cmos_sensor(0x8c64, 0xf8);
	OV5645MIPI_write_cmos_sensor(0x8c65, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x8c66, 0x93);
	OV5645MIPI_write_cmos_sensor(0x8c67, 0x70);
	OV5645MIPI_write_cmos_sensor(0x8c68, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8c69, 0x74);
	OV5645MIPI_write_cmos_sensor(0x8c6a, 0x01);
	OV5645MIPI_write_cmos_sensor(0x8c6b, 0x93);
	OV5645MIPI_write_cmos_sensor(0x8c6c, 0x70);
	OV5645MIPI_write_cmos_sensor(0x8c6d, 0x0d);
	OV5645MIPI_write_cmos_sensor(0x8c6e, 0xa3);
	OV5645MIPI_write_cmos_sensor(0x8c6f, 0xa3);
	OV5645MIPI_write_cmos_sensor(0x8c70, 0x93);
	OV5645MIPI_write_cmos_sensor(0x8c71, 0xf8);
	OV5645MIPI_write_cmos_sensor(0x8c72, 0x74);
	OV5645MIPI_write_cmos_sensor(0x8c73, 0x01);
	OV5645MIPI_write_cmos_sensor(0x8c74, 0x93);
	OV5645MIPI_write_cmos_sensor(0x8c75, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x8c76, 0x82);
	OV5645MIPI_write_cmos_sensor(0x8c77, 0x88);
	OV5645MIPI_write_cmos_sensor(0x8c78, 0x83);
	OV5645MIPI_write_cmos_sensor(0x8c79, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x8c7a, 0x73);
	OV5645MIPI_write_cmos_sensor(0x8c7b, 0x74);
	OV5645MIPI_write_cmos_sensor(0x8c7c, 0x02);
	OV5645MIPI_write_cmos_sensor(0x8c7d, 0x93);
	OV5645MIPI_write_cmos_sensor(0x8c7e, 0x68);
	OV5645MIPI_write_cmos_sensor(0x8c7f, 0x60);
	OV5645MIPI_write_cmos_sensor(0x8c80, 0xef);
	OV5645MIPI_write_cmos_sensor(0x8c81, 0xa3);
	OV5645MIPI_write_cmos_sensor(0x8c82, 0xa3);
	OV5645MIPI_write_cmos_sensor(0x8c83, 0xa3);
	OV5645MIPI_write_cmos_sensor(0x8c84, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8c85, 0xdf);
	OV5645MIPI_write_cmos_sensor(0x8c86, 0x90);
	OV5645MIPI_write_cmos_sensor(0x8c87, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x8c88, 0x9a);
	OV5645MIPI_write_cmos_sensor(0x8c89, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8c8a, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x8c8b, 0x65);
	OV5645MIPI_write_cmos_sensor(0x8c8c, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8c8d, 0x98);
	OV5645MIPI_write_cmos_sensor(0x8c8e, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8c8f, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x8c90, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x8c91, 0x08);
	OV5645MIPI_write_cmos_sensor(0x8c92, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8c93, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x8c94, 0x10);
	OV5645MIPI_write_cmos_sensor(0x8c95, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x8c96, 0xfd);
	OV5645MIPI_write_cmos_sensor(0x8c97, 0xed);
	OV5645MIPI_write_cmos_sensor(0x8c98, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x8c99, 0x94);
	OV5645MIPI_write_cmos_sensor(0x8c9a, 0x08);
	OV5645MIPI_write_cmos_sensor(0x8c9b, 0x50);
	OV5645MIPI_write_cmos_sensor(0x8c9c, 0x18);
	OV5645MIPI_write_cmos_sensor(0x8c9d, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x8c9e, 0x10);
	OV5645MIPI_write_cmos_sensor(0x8c9f, 0x94);
	OV5645MIPI_write_cmos_sensor(0x8ca0, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8ca1, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x8ca2, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x8ca3, 0x94);
	OV5645MIPI_write_cmos_sensor(0x8ca4, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8ca5, 0x50);
	OV5645MIPI_write_cmos_sensor(0x8ca6, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x8ca7, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x8ca8, 0x10);
	OV5645MIPI_write_cmos_sensor(0x8ca9, 0x25);
	OV5645MIPI_write_cmos_sensor(0x8caa, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x8cab, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x8cac, 0x10);
	OV5645MIPI_write_cmos_sensor(0x8cad, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x8cae, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x8caf, 0x33);
	OV5645MIPI_write_cmos_sensor(0x8cb0, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x8cb1, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x8cb2, 0x1d);
	OV5645MIPI_write_cmos_sensor(0x8cb3, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8cb4, 0xe2);
	OV5645MIPI_write_cmos_sensor(0x8cb5, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x8cb6, 0x74);
	OV5645MIPI_write_cmos_sensor(0x8cb7, 0x07);
	OV5645MIPI_write_cmos_sensor(0x8cb8, 0x9d);
	OV5645MIPI_write_cmos_sensor(0x8cb9, 0xfd);
	OV5645MIPI_write_cmos_sensor(0x8cba, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x8cbb, 0x94);
	OV5645MIPI_write_cmos_sensor(0x8cbc, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8cbd, 0x50);
	OV5645MIPI_write_cmos_sensor(0x8cbe, 0x02);
	OV5645MIPI_write_cmos_sensor(0x8cbf, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x8cc0, 0xfd);
	OV5645MIPI_write_cmos_sensor(0x8cc1, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8cc2, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x8cc3, 0xca);
	OV5645MIPI_write_cmos_sensor(0x8cc4, 0xed);
	OV5645MIPI_write_cmos_sensor(0x8cc5, 0x90);
	OV5645MIPI_write_cmos_sensor(0x8cc6, 0x0d);
	OV5645MIPI_write_cmos_sensor(0x8cc7, 0xd5);
	OV5645MIPI_write_cmos_sensor(0x8cc8, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8cc9, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x8cca, 0x8a);
	OV5645MIPI_write_cmos_sensor(0x8ccb, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8ccc, 0x0a);
	OV5645MIPI_write_cmos_sensor(0x8ccd, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x8cce, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8ccf, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x8cd0, 0xc2);
	OV5645MIPI_write_cmos_sensor(0x8cd1, 0xc0);
	OV5645MIPI_write_cmos_sensor(0x8cd2, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8cd3, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8cd4, 0xd0);
	OV5645MIPI_write_cmos_sensor(0x8cd5, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8cd6, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8cd7, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x8cd8, 0x25);
	OV5645MIPI_write_cmos_sensor(0x8cd9, 0xff);
	OV5645MIPI_write_cmos_sensor(0x8cda, 0x33);
	OV5645MIPI_write_cmos_sensor(0x8cdb, 0x95);
	OV5645MIPI_write_cmos_sensor(0x8cdc, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x8cdd, 0xfe);
	OV5645MIPI_write_cmos_sensor(0x8cde, 0x74);
	OV5645MIPI_write_cmos_sensor(0x8cdf, 0xc4);
	OV5645MIPI_write_cmos_sensor(0x8ce0, 0x2f);
	OV5645MIPI_write_cmos_sensor(0x8ce1, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x8ce2, 0x82);
	OV5645MIPI_write_cmos_sensor(0x8ce3, 0x74);
	OV5645MIPI_write_cmos_sensor(0x8ce4, 0x0d);
	OV5645MIPI_write_cmos_sensor(0x8ce5, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8ce6, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x8ce7, 0x39);
	OV5645MIPI_write_cmos_sensor(0x8ce8, 0xd0);
	OV5645MIPI_write_cmos_sensor(0x8ce9, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8cea, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8ceb, 0x0a);
	OV5645MIPI_write_cmos_sensor(0x8cec, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x8ced, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8cee, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x8cef, 0xc2);
	OV5645MIPI_write_cmos_sensor(0x8cf0, 0xc0);
	OV5645MIPI_write_cmos_sensor(0x8cf1, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8cf2, 0xc0);
	OV5645MIPI_write_cmos_sensor(0x8cf3, 0x01);
	OV5645MIPI_write_cmos_sensor(0x8cf4, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x8cf5, 0x79);
	OV5645MIPI_write_cmos_sensor(0x8cf6, 0xbf);
	OV5645MIPI_write_cmos_sensor(0x8cf7, 0xe7);
	OV5645MIPI_write_cmos_sensor(0x8cf8, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8cf9, 0xbe);
	OV5645MIPI_write_cmos_sensor(0x8cfa, 0x96);
	OV5645MIPI_write_cmos_sensor(0x8cfb, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8cfc, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x8cfd, 0x25);
	OV5645MIPI_write_cmos_sensor(0x8cfe, 0xff);
	OV5645MIPI_write_cmos_sensor(0x8cff, 0x33);
	OV5645MIPI_write_cmos_sensor(0x8d00, 0x95);
	OV5645MIPI_write_cmos_sensor(0x8d01, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x8d02, 0xfe);
	OV5645MIPI_write_cmos_sensor(0x8d03, 0x74);
	OV5645MIPI_write_cmos_sensor(0x8d04, 0xb3);
	OV5645MIPI_write_cmos_sensor(0x8d05, 0x2f);
	OV5645MIPI_write_cmos_sensor(0x8d06, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x8d07, 0x82);
	OV5645MIPI_write_cmos_sensor(0x8d08, 0x74);
	OV5645MIPI_write_cmos_sensor(0x8d09, 0x0d);
	OV5645MIPI_write_cmos_sensor(0x8d0a, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8d0b, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x8d0c, 0x39);
	OV5645MIPI_write_cmos_sensor(0x8d0d, 0xd0);
	OV5645MIPI_write_cmos_sensor(0x8d0e, 0x01);
	OV5645MIPI_write_cmos_sensor(0x8d0f, 0xd0);
	OV5645MIPI_write_cmos_sensor(0x8d10, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8d11, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8d12, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x8d13, 0xb2);
	OV5645MIPI_write_cmos_sensor(0x8d14, 0x90);
	OV5645MIPI_write_cmos_sensor(0x8d15, 0x0d);
	OV5645MIPI_write_cmos_sensor(0x8d16, 0xee);
	OV5645MIPI_write_cmos_sensor(0x8d17, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8d18, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x8d19, 0x89);
	OV5645MIPI_write_cmos_sensor(0x8d1a, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8d1b, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x8d1c, 0xb2);
	OV5645MIPI_write_cmos_sensor(0x8d1d, 0xc0);
	OV5645MIPI_write_cmos_sensor(0x8d1e, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8d1f, 0xc0);
	OV5645MIPI_write_cmos_sensor(0x8d20, 0x01);
	OV5645MIPI_write_cmos_sensor(0x8d21, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x8d22, 0x79);
	OV5645MIPI_write_cmos_sensor(0x8d23, 0xcf);
	OV5645MIPI_write_cmos_sensor(0x8d24, 0xe7);
	OV5645MIPI_write_cmos_sensor(0x8d25, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8d26, 0xce);
	OV5645MIPI_write_cmos_sensor(0x8d27, 0x96);
	OV5645MIPI_write_cmos_sensor(0x8d28, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8d29, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x8d2a, 0x25);
	OV5645MIPI_write_cmos_sensor(0x8d2b, 0xff);
	OV5645MIPI_write_cmos_sensor(0x8d2c, 0x33);
	OV5645MIPI_write_cmos_sensor(0x8d2d, 0x95);
	OV5645MIPI_write_cmos_sensor(0x8d2e, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x8d2f, 0xfe);
	OV5645MIPI_write_cmos_sensor(0x8d30, 0x74);
	OV5645MIPI_write_cmos_sensor(0x8d31, 0xdd);
	OV5645MIPI_write_cmos_sensor(0x8d32, 0x2f);
	OV5645MIPI_write_cmos_sensor(0x8d33, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x8d34, 0x82);
	OV5645MIPI_write_cmos_sensor(0x8d35, 0x74);
	OV5645MIPI_write_cmos_sensor(0x8d36, 0x0d);
	OV5645MIPI_write_cmos_sensor(0x8d37, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8d38, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x8d39, 0x39);
	OV5645MIPI_write_cmos_sensor(0x8d3a, 0xd0);
	OV5645MIPI_write_cmos_sensor(0x8d3b, 0x01);
	OV5645MIPI_write_cmos_sensor(0x8d3c, 0xd0);
	OV5645MIPI_write_cmos_sensor(0x8d3d, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8d3e, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8d3f, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x8d40, 0x44);
	OV5645MIPI_write_cmos_sensor(0x8d41, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8d42, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x8d43, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8d44, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x8d45, 0xbf);
	OV5645MIPI_write_cmos_sensor(0x8d46, 0xc0);
	OV5645MIPI_write_cmos_sensor(0x8d47, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8d48, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8d49, 0x98);
	OV5645MIPI_write_cmos_sensor(0x8d4a, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8d4b, 0xfe);
	OV5645MIPI_write_cmos_sensor(0x8d4c, 0x08);
	OV5645MIPI_write_cmos_sensor(0x8d4d, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8d4e, 0xff);
	OV5645MIPI_write_cmos_sensor(0x8d4f, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x8d50, 0xfc);
	OV5645MIPI_write_cmos_sensor(0x8d51, 0xfd);
	OV5645MIPI_write_cmos_sensor(0x8d52, 0xd0);
	OV5645MIPI_write_cmos_sensor(0x8d53, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8d54, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8d55, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x8d56, 0x44);
	OV5645MIPI_write_cmos_sensor(0x8d57, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8d58, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x8d59, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8d5a, 0x0c);
	OV5645MIPI_write_cmos_sensor(0x8d5b, 0x1e);
	OV5645MIPI_write_cmos_sensor(0x8d5c, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8d5d, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x8d5e, 0x47);
	OV5645MIPI_write_cmos_sensor(0x8d5f, 0x90);
	OV5645MIPI_write_cmos_sensor(0x8d60, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x8d61, 0x9b);
	OV5645MIPI_write_cmos_sensor(0x8d62, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8d63, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x8d64, 0x89);
	OV5645MIPI_write_cmos_sensor(0x8d65, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8d66, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x8d67, 0xca);
	OV5645MIPI_write_cmos_sensor(0x8d68, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x8d69, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8d6a, 0x0c);
	OV5645MIPI_write_cmos_sensor(0x8d6b, 0x0d);
	OV5645MIPI_write_cmos_sensor(0x8d6c, 0x50);
	OV5645MIPI_write_cmos_sensor(0x8d6d, 0x06);
	OV5645MIPI_write_cmos_sensor(0x8d6e, 0x90);
	OV5645MIPI_write_cmos_sensor(0x8d6f, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x8d70, 0x9b);
	OV5645MIPI_write_cmos_sensor(0x8d71, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8d72, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x8d73, 0x65);
	OV5645MIPI_write_cmos_sensor(0x8d74, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8d75, 0xc9);
	OV5645MIPI_write_cmos_sensor(0x8d76, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8d77, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8d78, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x8d79, 0xdf);
	OV5645MIPI_write_cmos_sensor(0x8d7a, 0xfe);
	OV5645MIPI_write_cmos_sensor(0x8d7b, 0x08);
	OV5645MIPI_write_cmos_sensor(0x8d7c, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8d7d, 0xff);
	OV5645MIPI_write_cmos_sensor(0x8d7e, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x8d7f, 0xfc);
	OV5645MIPI_write_cmos_sensor(0x8d80, 0xfd);
	OV5645MIPI_write_cmos_sensor(0x8d81, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8d82, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x8d83, 0xca);
	OV5645MIPI_write_cmos_sensor(0x8d84, 0xd3);
	OV5645MIPI_write_cmos_sensor(0x8d85, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8d86, 0x0c);
	OV5645MIPI_write_cmos_sensor(0x8d87, 0x0d);
	OV5645MIPI_write_cmos_sensor(0x8d88, 0x40);
	OV5645MIPI_write_cmos_sensor(0x8d89, 0x07);
	OV5645MIPI_write_cmos_sensor(0x8d8a, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x8d8b, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x8d8c, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x8d8d, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x8d8e, 0x10);
	OV5645MIPI_write_cmos_sensor(0x8d8f, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8d90, 0x1a);
	OV5645MIPI_write_cmos_sensor(0x8d91, 0x85);
	OV5645MIPI_write_cmos_sensor(0x8d92, 0x0d);
	OV5645MIPI_write_cmos_sensor(0x8d93, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x8d94, 0x85);
	OV5645MIPI_write_cmos_sensor(0x8d95, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x8d96, 0x10);
	OV5645MIPI_write_cmos_sensor(0x8d97, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8d98, 0xc9);
	OV5645MIPI_write_cmos_sensor(0x8d99, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8d9a, 0x25);
	OV5645MIPI_write_cmos_sensor(0x8d9b, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x8d9c, 0x24);
	OV5645MIPI_write_cmos_sensor(0x8d9d, 0x57);
	OV5645MIPI_write_cmos_sensor(0x8d9e, 0xf8);
	OV5645MIPI_write_cmos_sensor(0x8d9f, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x8da0, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8da1, 0x95);
	OV5645MIPI_write_cmos_sensor(0x8da2, 0x10);
	OV5645MIPI_write_cmos_sensor(0x8da3, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x8da4, 0x10);
	OV5645MIPI_write_cmos_sensor(0x8da5, 0x18);
	OV5645MIPI_write_cmos_sensor(0x8da6, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8da7, 0x95);
	OV5645MIPI_write_cmos_sensor(0x8da8, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x8da9, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x8daa, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x8dab, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8dac, 0x9c);
	OV5645MIPI_write_cmos_sensor(0x8dad, 0xa6);
	OV5645MIPI_write_cmos_sensor(0x8dae, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x8daf, 0x08);
	OV5645MIPI_write_cmos_sensor(0x8db0, 0xa6);
	OV5645MIPI_write_cmos_sensor(0x8db1, 0x10);
	OV5645MIPI_write_cmos_sensor(0x8db2, 0x22);
	OV5645MIPI_write_cmos_sensor(0x8db3, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8db4, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8db5, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8db6, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8db7, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8db8, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8db9, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8dba, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8dbb, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8dbc, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8dbd, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8dbe, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8dbf, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8dc0, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8dc1, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8dc2, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8dc3, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8dc4, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8dc5, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8dc6, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8dc7, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8dc8, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8dc9, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8dca, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8dcb, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8dcc, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8dcd, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8dce, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8dcf, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8dd0, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8dd1, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8dd2, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8dd3, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8dd4, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8dd5, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8dd6, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8dd7, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8dd8, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8dd9, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8dda, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8ddb, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8ddc, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8ddd, 0x10);
	OV5645MIPI_write_cmos_sensor(0x8dde, 0x18);
	OV5645MIPI_write_cmos_sensor(0x8ddf, 0x20);
	OV5645MIPI_write_cmos_sensor(0x8de0, 0x28);
	OV5645MIPI_write_cmos_sensor(0x8de1, 0x30);
	OV5645MIPI_write_cmos_sensor(0x8de2, 0x38);
	OV5645MIPI_write_cmos_sensor(0x8de3, 0x40);
	OV5645MIPI_write_cmos_sensor(0x8de4, 0x48);
	OV5645MIPI_write_cmos_sensor(0x8de5, 0x50);
	OV5645MIPI_write_cmos_sensor(0x8de6, 0x58);
	OV5645MIPI_write_cmos_sensor(0x8de7, 0x60);
	OV5645MIPI_write_cmos_sensor(0x8de8, 0x68);
	OV5645MIPI_write_cmos_sensor(0x8de9, 0x70);
	OV5645MIPI_write_cmos_sensor(0x8dea, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8deb, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8dec, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8ded, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8dee, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8def, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8df0, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8df1, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8df2, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8df3, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8df4, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8df5, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8df6, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8df7, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8df8, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8df9, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8dfa, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8dfb, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8dfc, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8dfd, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8dfe, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8dff, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8e00, 0x13);
	OV5645MIPI_write_cmos_sensor(0x8e01, 0x04);
	OV5645MIPI_write_cmos_sensor(0x8e02, 0x26);
	OV5645MIPI_write_cmos_sensor(0x8e03, 0x15);
	OV5645MIPI_write_cmos_sensor(0x8e04, 0x01);
	OV5645MIPI_write_cmos_sensor(0x8e05, 0x36);
	OV5645MIPI_write_cmos_sensor(0x8e06, 0x4f);
	OV5645MIPI_write_cmos_sensor(0x8e07, 0x56);
	OV5645MIPI_write_cmos_sensor(0x8e08, 0x54);
	OV5645MIPI_write_cmos_sensor(0x8e09, 0x20);
	OV5645MIPI_write_cmos_sensor(0x8e0a, 0x20);
	OV5645MIPI_write_cmos_sensor(0x8e0b, 0x20);
	OV5645MIPI_write_cmos_sensor(0x8e0c, 0x20);
	OV5645MIPI_write_cmos_sensor(0x8e0d, 0x20);
	OV5645MIPI_write_cmos_sensor(0x8e0e, 0x43);
	OV5645MIPI_write_cmos_sensor(0x8e0f, 0x01);
	OV5645MIPI_write_cmos_sensor(0x8e10, 0x10);
	OV5645MIPI_write_cmos_sensor(0x8e11, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8e12, 0x56);
	OV5645MIPI_write_cmos_sensor(0x8e13, 0x45);
	OV5645MIPI_write_cmos_sensor(0x8e14, 0x1a);
	OV5645MIPI_write_cmos_sensor(0x8e15, 0x30);
	OV5645MIPI_write_cmos_sensor(0x8e16, 0x29);
	OV5645MIPI_write_cmos_sensor(0x8e17, 0x7e);
	OV5645MIPI_write_cmos_sensor(0x8e18, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8e19, 0x30);
	OV5645MIPI_write_cmos_sensor(0x8e1a, 0x04);
	OV5645MIPI_write_cmos_sensor(0x8e1b, 0x20);
	OV5645MIPI_write_cmos_sensor(0x8e1c, 0xdf);
	OV5645MIPI_write_cmos_sensor(0x8e1d, 0x30);
	OV5645MIPI_write_cmos_sensor(0x8e1e, 0x05);
	OV5645MIPI_write_cmos_sensor(0x8e1f, 0x40);
	OV5645MIPI_write_cmos_sensor(0x8e20, 0xbf);
	OV5645MIPI_write_cmos_sensor(0x8e21, 0x50);
	OV5645MIPI_write_cmos_sensor(0x8e22, 0x03);
	OV5645MIPI_write_cmos_sensor(0x8e23, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8e24, 0xfd);
	OV5645MIPI_write_cmos_sensor(0x8e25, 0x50);
	OV5645MIPI_write_cmos_sensor(0x8e26, 0x27);
	OV5645MIPI_write_cmos_sensor(0x8e27, 0x01);
	OV5645MIPI_write_cmos_sensor(0x8e28, 0xfe);
	OV5645MIPI_write_cmos_sensor(0x8e29, 0x60);
	OV5645MIPI_write_cmos_sensor(0x8e2a, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8e2b, 0x13);
	OV5645MIPI_write_cmos_sensor(0x8e2c, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8e2d, 0x36);
	OV5645MIPI_write_cmos_sensor(0x8e2e, 0x06);
	OV5645MIPI_write_cmos_sensor(0x8e2f, 0x07);
	OV5645MIPI_write_cmos_sensor(0x8e30, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8e31, 0x3f);
	OV5645MIPI_write_cmos_sensor(0x8e32, 0x05);
	OV5645MIPI_write_cmos_sensor(0x8e33, 0x30);
	OV5645MIPI_write_cmos_sensor(0x8e34, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8e35, 0x3f);
	OV5645MIPI_write_cmos_sensor(0x8e36, 0x06);
	OV5645MIPI_write_cmos_sensor(0x8e37, 0x22);
	OV5645MIPI_write_cmos_sensor(0x8e38, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8e39, 0x3f);
	OV5645MIPI_write_cmos_sensor(0x8e3a, 0x08);
	OV5645MIPI_write_cmos_sensor(0x8e3b, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8e3c, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8e3d, 0x3f);
	OV5645MIPI_write_cmos_sensor(0x8e3e, 0x09);
	OV5645MIPI_write_cmos_sensor(0x8e3f, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8e40, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8e41, 0x3f);
	OV5645MIPI_write_cmos_sensor(0x8e42, 0x0a);
	OV5645MIPI_write_cmos_sensor(0x8e43, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8e44, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8e45, 0x3f);
	OV5645MIPI_write_cmos_sensor(0x8e46, 0x0b);
	OV5645MIPI_write_cmos_sensor(0x8e47, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x8e48, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8e49, 0x3f);
	OV5645MIPI_write_cmos_sensor(0x8e4a, 0x01);
	OV5645MIPI_write_cmos_sensor(0x8e4b, 0x2a);
	OV5645MIPI_write_cmos_sensor(0x8e4c, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8e4d, 0x3f);
	OV5645MIPI_write_cmos_sensor(0x8e4e, 0x02);
	OV5645MIPI_write_cmos_sensor(0x8e4f, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8e50, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8e51, 0x30);
	OV5645MIPI_write_cmos_sensor(0x8e52, 0x01);
	OV5645MIPI_write_cmos_sensor(0x8e53, 0x40);
	OV5645MIPI_write_cmos_sensor(0x8e54, 0xbf);
	OV5645MIPI_write_cmos_sensor(0x8e55, 0x30);
	OV5645MIPI_write_cmos_sensor(0x8e56, 0x01);
	OV5645MIPI_write_cmos_sensor(0x8e57, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8e58, 0xbf);
	OV5645MIPI_write_cmos_sensor(0x8e59, 0x30);
	OV5645MIPI_write_cmos_sensor(0x8e5a, 0x29);
	OV5645MIPI_write_cmos_sensor(0x8e5b, 0x70);
	OV5645MIPI_write_cmos_sensor(0x8e5c, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8e5d, 0x3a);
	OV5645MIPI_write_cmos_sensor(0x8e5e, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8e5f, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8e60, 0xff);
	OV5645MIPI_write_cmos_sensor(0x8e61, 0x3a);
	OV5645MIPI_write_cmos_sensor(0x8e62, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8e63, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8e64, 0xff);
	OV5645MIPI_write_cmos_sensor(0x8e65, 0x36);
	OV5645MIPI_write_cmos_sensor(0x8e66, 0x03);
	OV5645MIPI_write_cmos_sensor(0x8e67, 0x36);
	OV5645MIPI_write_cmos_sensor(0x8e68, 0x02);
	OV5645MIPI_write_cmos_sensor(0x8e69, 0x41);
	OV5645MIPI_write_cmos_sensor(0x8e6a, 0x44);
	OV5645MIPI_write_cmos_sensor(0x8e6b, 0x58);
	OV5645MIPI_write_cmos_sensor(0x8e6c, 0x20);
	OV5645MIPI_write_cmos_sensor(0x8e6d, 0x18);
	OV5645MIPI_write_cmos_sensor(0x8e6e, 0x10);
	OV5645MIPI_write_cmos_sensor(0x8e6f, 0x0a);
	OV5645MIPI_write_cmos_sensor(0x8e70, 0x04);
	OV5645MIPI_write_cmos_sensor(0x8e71, 0x04);
	OV5645MIPI_write_cmos_sensor(0x8e72, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8e73, 0x03);
	OV5645MIPI_write_cmos_sensor(0x8e74, 0xff);
	OV5645MIPI_write_cmos_sensor(0x8e75, 0x64);
	OV5645MIPI_write_cmos_sensor(0x8e76, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8e77, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8e78, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8e79, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8e7a, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8e7b, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8e7c, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8e7d, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8e7e, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8e7f, 0x02);
	OV5645MIPI_write_cmos_sensor(0x8e80, 0x04);
	OV5645MIPI_write_cmos_sensor(0x8e81, 0x06);
	OV5645MIPI_write_cmos_sensor(0x8e82, 0x06);
	OV5645MIPI_write_cmos_sensor(0x8e83, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8e84, 0x02);
	OV5645MIPI_write_cmos_sensor(0x8e85, 0x65);
	OV5645MIPI_write_cmos_sensor(0x8e86, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8e87, 0x7a);
	OV5645MIPI_write_cmos_sensor(0x8e88, 0x50);
	OV5645MIPI_write_cmos_sensor(0x8e89, 0x28);
	OV5645MIPI_write_cmos_sensor(0x8e8a, 0x1e);
	OV5645MIPI_write_cmos_sensor(0x8e8b, 0x08);
	OV5645MIPI_write_cmos_sensor(0x8e8c, 0x08);
	OV5645MIPI_write_cmos_sensor(0x8e8d, 0x01);
	OV5645MIPI_write_cmos_sensor(0x8e8e, 0x1e);
	OV5645MIPI_write_cmos_sensor(0x8e8f, 0x1e);
	OV5645MIPI_write_cmos_sensor(0x8e90, 0x1e);
	OV5645MIPI_write_cmos_sensor(0x8e91, 0x1e);
	OV5645MIPI_write_cmos_sensor(0x8e92, 0x68);
	OV5645MIPI_write_cmos_sensor(0x8e93, 0x68);
	OV5645MIPI_write_cmos_sensor(0x8e94, 0x68);
	OV5645MIPI_write_cmos_sensor(0x8e95, 0x68);
	OV5645MIPI_write_cmos_sensor(0x8e96, 0x03);
	OV5645MIPI_write_cmos_sensor(0x8e97, 0x05);
	OV5645MIPI_write_cmos_sensor(0x8e98, 0x0a);
	OV5645MIPI_write_cmos_sensor(0x8e99, 0x08);
	OV5645MIPI_write_cmos_sensor(0x8e9a, 0x10);
	OV5645MIPI_write_cmos_sensor(0x8e9b, 0x01);
	OV5645MIPI_write_cmos_sensor(0x8e9c, 0x0a);
	OV5645MIPI_write_cmos_sensor(0x8e9d, 0x06);
	OV5645MIPI_write_cmos_sensor(0x8e9e, 0x06);
	OV5645MIPI_write_cmos_sensor(0x8e9f, 0x05);
	OV5645MIPI_write_cmos_sensor(0x8ea0, 0x05);
	OV5645MIPI_write_cmos_sensor(0x8ea1, 0x05);
	OV5645MIPI_write_cmos_sensor(0x8ea2, 0x05);
	OV5645MIPI_write_cmos_sensor(0x8ea3, 0x04);
	OV5645MIPI_write_cmos_sensor(0x8ea4, 0x04);
	OV5645MIPI_write_cmos_sensor(0x8ea5, 0x04);
	OV5645MIPI_write_cmos_sensor(0x8ea6, 0x04);
	OV5645MIPI_write_cmos_sensor(0x8ea7, 0x04);
	OV5645MIPI_write_cmos_sensor(0x8ea8, 0xfc);
	OV5645MIPI_write_cmos_sensor(0x8ea9, 0xfc);
	OV5645MIPI_write_cmos_sensor(0x8eaa, 0xfc);
	OV5645MIPI_write_cmos_sensor(0x8eab, 0xfc);
	OV5645MIPI_write_cmos_sensor(0x8eac, 0xfc);
	OV5645MIPI_write_cmos_sensor(0x8ead, 0xfc);
	OV5645MIPI_write_cmos_sensor(0x8eae, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8eaf, 0xa5);
	OV5645MIPI_write_cmos_sensor(0x8eb0, 0x5a);
	OV5645MIPI_write_cmos_sensor(0x8eb1, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8eb2, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8eb3, 0x0a);
	OV5645MIPI_write_cmos_sensor(0x8eb4, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x8eb5, 0x8f);
	OV5645MIPI_write_cmos_sensor(0x8eb6, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x8eb7, 0x8e);
	OV5645MIPI_write_cmos_sensor(0x8eb8, 0x0d);
	OV5645MIPI_write_cmos_sensor(0x8eb9, 0x8d);
	OV5645MIPI_write_cmos_sensor(0x8eba, 0x0c);
	OV5645MIPI_write_cmos_sensor(0x8ebb, 0x8c);
	OV5645MIPI_write_cmos_sensor(0x8ebc, 0x0b);
	OV5645MIPI_write_cmos_sensor(0x8ebd, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8ebe, 0x07);
	OV5645MIPI_write_cmos_sensor(0x8ebf, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8ec0, 0x0c);
	OV5645MIPI_write_cmos_sensor(0x8ec1, 0x1e);
	OV5645MIPI_write_cmos_sensor(0x8ec2, 0x8f);
	OV5645MIPI_write_cmos_sensor(0x8ec3, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x8ec4, 0x8e);
	OV5645MIPI_write_cmos_sensor(0x8ec5, 0x0d);
	OV5645MIPI_write_cmos_sensor(0x8ec6, 0x8d);
	OV5645MIPI_write_cmos_sensor(0x8ec7, 0x0c);
	OV5645MIPI_write_cmos_sensor(0x8ec8, 0x8c);
	OV5645MIPI_write_cmos_sensor(0x8ec9, 0x0b);
	OV5645MIPI_write_cmos_sensor(0x8eca, 0xab);
	OV5645MIPI_write_cmos_sensor(0x8ecb, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x8ecc, 0xaa);
	OV5645MIPI_write_cmos_sensor(0x8ecd, 0x0d);
	OV5645MIPI_write_cmos_sensor(0x8ece, 0xa9);
	OV5645MIPI_write_cmos_sensor(0x8ecf, 0x0c);
	OV5645MIPI_write_cmos_sensor(0x8ed0, 0xa8);
	OV5645MIPI_write_cmos_sensor(0x8ed1, 0x0b);
	OV5645MIPI_write_cmos_sensor(0x8ed2, 0x22);
	OV5645MIPI_write_cmos_sensor(0x8ed3, 0xef);
	OV5645MIPI_write_cmos_sensor(0x8ed4, 0x25);
	OV5645MIPI_write_cmos_sensor(0x8ed5, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x8ed6, 0x24);
	OV5645MIPI_write_cmos_sensor(0x8ed7, 0x56);
	OV5645MIPI_write_cmos_sensor(0x8ed8, 0xf8);
	OV5645MIPI_write_cmos_sensor(0x8ed9, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8eda, 0xfc);
	OV5645MIPI_write_cmos_sensor(0x8edb, 0x08);
	OV5645MIPI_write_cmos_sensor(0x8edc, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8edd, 0xfd);
	OV5645MIPI_write_cmos_sensor(0x8ede, 0xee);
	OV5645MIPI_write_cmos_sensor(0x8edf, 0x25);
	OV5645MIPI_write_cmos_sensor(0x8ee0, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x8ee1, 0x24);
	OV5645MIPI_write_cmos_sensor(0x8ee2, 0x56);
	OV5645MIPI_write_cmos_sensor(0x8ee3, 0xf8);
	OV5645MIPI_write_cmos_sensor(0x8ee4, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8ee5, 0x22);
	OV5645MIPI_write_cmos_sensor(0x8ee6, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8ee7, 0xce);
	OV5645MIPI_write_cmos_sensor(0x8ee8, 0xf9);
	OV5645MIPI_write_cmos_sensor(0x8ee9, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x8eea, 0xe7);
	OV5645MIPI_write_cmos_sensor(0x8eeb, 0x64);
	OV5645MIPI_write_cmos_sensor(0x8eec, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8eed, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x8eee, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x8eef, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8ef0, 0x64);
	OV5645MIPI_write_cmos_sensor(0x8ef1, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8ef2, 0x95);
	OV5645MIPI_write_cmos_sensor(0x8ef3, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x8ef4, 0x22);
	OV5645MIPI_write_cmos_sensor(0x8ef5, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8ef6, 0xcb);
	OV5645MIPI_write_cmos_sensor(0x8ef7, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8ef8, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8ef9, 0x9d);
	OV5645MIPI_write_cmos_sensor(0x8efa, 0x25);
	OV5645MIPI_write_cmos_sensor(0x8efb, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x8efc, 0x24);
	OV5645MIPI_write_cmos_sensor(0x8efd, 0x57);
	OV5645MIPI_write_cmos_sensor(0x8efe, 0xf9);
	OV5645MIPI_write_cmos_sensor(0x8eff, 0xd3);
	OV5645MIPI_write_cmos_sensor(0x8f00, 0xe7);
	OV5645MIPI_write_cmos_sensor(0x8f01, 0x96);
	OV5645MIPI_write_cmos_sensor(0x8f02, 0x19);
	OV5645MIPI_write_cmos_sensor(0x8f03, 0xe7);
	OV5645MIPI_write_cmos_sensor(0x8f04, 0x18);
	OV5645MIPI_write_cmos_sensor(0x8f05, 0x96);
	OV5645MIPI_write_cmos_sensor(0x8f06, 0x22);
	OV5645MIPI_write_cmos_sensor(0x8f07, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8f08, 0xcf);
	OV5645MIPI_write_cmos_sensor(0x8f09, 0xf9);
	OV5645MIPI_write_cmos_sensor(0x8f0a, 0xd3);
	OV5645MIPI_write_cmos_sensor(0x8f0b, 0xe7);
	OV5645MIPI_write_cmos_sensor(0x8f0c, 0x64);
	OV5645MIPI_write_cmos_sensor(0x8f0d, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8f0e, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x8f0f, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x8f10, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8f11, 0x64);
	OV5645MIPI_write_cmos_sensor(0x8f12, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8f13, 0x95);
	OV5645MIPI_write_cmos_sensor(0x8f14, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x8f15, 0x22);
	OV5645MIPI_write_cmos_sensor(0x8f16, 0xd3);
	OV5645MIPI_write_cmos_sensor(0x8f17, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8f18, 0xc1);
	OV5645MIPI_write_cmos_sensor(0x8f19, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8f1a, 0x64);
	OV5645MIPI_write_cmos_sensor(0x8f1b, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8f1c, 0x94);
	OV5645MIPI_write_cmos_sensor(0x8f1d, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8f1e, 0x22);
	OV5645MIPI_write_cmos_sensor(0x8f1f, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8f20, 0xc1);
	OV5645MIPI_write_cmos_sensor(0x8f21, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8f22, 0xf4);
	OV5645MIPI_write_cmos_sensor(0x8f23, 0x04);
	OV5645MIPI_write_cmos_sensor(0x8f24, 0xff);
	OV5645MIPI_write_cmos_sensor(0x8f25, 0xa2);
	OV5645MIPI_write_cmos_sensor(0x8f26, 0xe7);
	OV5645MIPI_write_cmos_sensor(0x8f27, 0x13);
	OV5645MIPI_write_cmos_sensor(0x8f28, 0xa2);
	OV5645MIPI_write_cmos_sensor(0x8f29, 0xe7);
	OV5645MIPI_write_cmos_sensor(0x8f2a, 0x13);
	OV5645MIPI_write_cmos_sensor(0x8f2b, 0x22);
	OV5645MIPI_write_cmos_sensor(0x8f2c, 0xa6);
	OV5645MIPI_write_cmos_sensor(0x8f2d, 0x06);
	OV5645MIPI_write_cmos_sensor(0x8f2e, 0x08);
	OV5645MIPI_write_cmos_sensor(0x8f2f, 0xa6);
	OV5645MIPI_write_cmos_sensor(0x8f30, 0x07);
	OV5645MIPI_write_cmos_sensor(0x8f31, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8f32, 0x9a);
	OV5645MIPI_write_cmos_sensor(0x8f33, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8f34, 0xfe);
	OV5645MIPI_write_cmos_sensor(0x8f35, 0x08);
	OV5645MIPI_write_cmos_sensor(0x8f36, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8f37, 0xff);
	OV5645MIPI_write_cmos_sensor(0x8f38, 0x22);
	OV5645MIPI_write_cmos_sensor(0x8f39, 0x3e);
	OV5645MIPI_write_cmos_sensor(0x8f3a, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x8f3b, 0x83);
	OV5645MIPI_write_cmos_sensor(0x8f3c, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x8f3d, 0x93);
	OV5645MIPI_write_cmos_sensor(0x8f3e, 0xff);
	OV5645MIPI_write_cmos_sensor(0x8f3f, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x8f40, 0xfc);
	OV5645MIPI_write_cmos_sensor(0x8f41, 0xfd);
	OV5645MIPI_write_cmos_sensor(0x8f42, 0xfe);
	OV5645MIPI_write_cmos_sensor(0x8f43, 0x22);
	OV5645MIPI_write_cmos_sensor(0x8f44, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8f45, 0x0a);
	OV5645MIPI_write_cmos_sensor(0x8f46, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x8f47, 0x8f);
	OV5645MIPI_write_cmos_sensor(0x8f48, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x8f49, 0x8e);
	OV5645MIPI_write_cmos_sensor(0x8f4a, 0x0d);
	OV5645MIPI_write_cmos_sensor(0x8f4b, 0x8d);
	OV5645MIPI_write_cmos_sensor(0x8f4c, 0x0c);
	OV5645MIPI_write_cmos_sensor(0x8f4d, 0x8c);
	OV5645MIPI_write_cmos_sensor(0x8f4e, 0x0b);
	OV5645MIPI_write_cmos_sensor(0x8f4f, 0x22);
	OV5645MIPI_write_cmos_sensor(0x8f50, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8f51, 0xc9);
	OV5645MIPI_write_cmos_sensor(0x8f52, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8f53, 0x24);
	OV5645MIPI_write_cmos_sensor(0x8f54, 0x9e);
	OV5645MIPI_write_cmos_sensor(0x8f55, 0xf8);
	OV5645MIPI_write_cmos_sensor(0x8f56, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8f57, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8f58, 0xd0);
	OV5645MIPI_write_cmos_sensor(0x8f59, 0xf6);
	OV5645MIPI_write_cmos_sensor(0x8f5a, 0x22);
	OV5645MIPI_write_cmos_sensor(0x8f5b, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x8f5c, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8f5d, 0x64);
	OV5645MIPI_write_cmos_sensor(0x8f5e, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8f5f, 0xf8);
	OV5645MIPI_write_cmos_sensor(0x8f60, 0xef);
	OV5645MIPI_write_cmos_sensor(0x8f61, 0x64);
	OV5645MIPI_write_cmos_sensor(0x8f62, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8f63, 0x98);
	OV5645MIPI_write_cmos_sensor(0x8f64, 0x22);
	OV5645MIPI_write_cmos_sensor(0x8f65, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x8f66, 0x93);
	OV5645MIPI_write_cmos_sensor(0x8f67, 0xff);
	OV5645MIPI_write_cmos_sensor(0x8f68, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x8f69, 0x8f);
	OV5645MIPI_write_cmos_sensor(0x8f6a, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x8f6b, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x8f6c, 0x0d);
	OV5645MIPI_write_cmos_sensor(0x8f6d, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x8f6e, 0x0c);
	OV5645MIPI_write_cmos_sensor(0x8f6f, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x8f70, 0x0b);
	OV5645MIPI_write_cmos_sensor(0x8f71, 0x22);
	OV5645MIPI_write_cmos_sensor(0x8f72, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8f73, 0xc1);
	OV5645MIPI_write_cmos_sensor(0x8f74, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8f75, 0xf4);
	OV5645MIPI_write_cmos_sensor(0x8f76, 0x04);
	OV5645MIPI_write_cmos_sensor(0x8f77, 0xf6);
	OV5645MIPI_write_cmos_sensor(0x8f78, 0x22);
	OV5645MIPI_write_cmos_sensor(0x8f79, 0xfa);
	OV5645MIPI_write_cmos_sensor(0x8f7a, 0x08);
	OV5645MIPI_write_cmos_sensor(0x8f7b, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8f7c, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x8f7d, 0x9d);
	OV5645MIPI_write_cmos_sensor(0x8f7e, 0xea);
	OV5645MIPI_write_cmos_sensor(0x8f7f, 0x9c);
	OV5645MIPI_write_cmos_sensor(0x8f80, 0x22);
	OV5645MIPI_write_cmos_sensor(0x8f81, 0xf6);
	OV5645MIPI_write_cmos_sensor(0x8f82, 0xd3);
	OV5645MIPI_write_cmos_sensor(0x8f83, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8f84, 0x64);
	OV5645MIPI_write_cmos_sensor(0x8f85, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8f86, 0x94);
	OV5645MIPI_write_cmos_sensor(0x8f87, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8f88, 0x22);
	OV5645MIPI_write_cmos_sensor(0x8f89, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x8f8a, 0x93);
	OV5645MIPI_write_cmos_sensor(0x8f8b, 0xff);
	OV5645MIPI_write_cmos_sensor(0x8f8c, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x8f8d, 0xfc);
	OV5645MIPI_write_cmos_sensor(0x8f8e, 0xfd);
	OV5645MIPI_write_cmos_sensor(0x8f8f, 0xfe);
	OV5645MIPI_write_cmos_sensor(0x8f90, 0x22);
	OV5645MIPI_write_cmos_sensor(0x8f91, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8f92, 0x98);
	OV5645MIPI_write_cmos_sensor(0x8f93, 0xa6);
	OV5645MIPI_write_cmos_sensor(0x8f94, 0x06);
	OV5645MIPI_write_cmos_sensor(0x8f95, 0x08);
	OV5645MIPI_write_cmos_sensor(0x8f96, 0xa6);
	OV5645MIPI_write_cmos_sensor(0x8f97, 0x07);
	OV5645MIPI_write_cmos_sensor(0x8f98, 0x22);
	OV5645MIPI_write_cmos_sensor(0x8f99, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8f9a, 0xc5);
	OV5645MIPI_write_cmos_sensor(0x8f9b, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8f9c, 0x24);
	OV5645MIPI_write_cmos_sensor(0x8f9d, 0x9e);
	OV5645MIPI_write_cmos_sensor(0x8f9e, 0xf8);
	OV5645MIPI_write_cmos_sensor(0x8f9f, 0x22);
	OV5645MIPI_write_cmos_sensor(0x8fa0, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8fa1, 0xd0);
	OV5645MIPI_write_cmos_sensor(0x8fa2, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x8fa3, 0x78);
	OV5645MIPI_write_cmos_sensor(0x8fa4, 0xc4);
	OV5645MIPI_write_cmos_sensor(0x8fa5, 0xf6);
	OV5645MIPI_write_cmos_sensor(0x8fa6, 0x22);
	OV5645MIPI_write_cmos_sensor(0x8fa7, 0x85);
	OV5645MIPI_write_cmos_sensor(0x8fa8, 0x28);
	OV5645MIPI_write_cmos_sensor(0x8fa9, 0x46);
	OV5645MIPI_write_cmos_sensor(0x8faa, 0x90);
	OV5645MIPI_write_cmos_sensor(0x8fab, 0x30);
	OV5645MIPI_write_cmos_sensor(0x8fac, 0x24);
	OV5645MIPI_write_cmos_sensor(0x8fad, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x8fae, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x8faf, 0x42);
	OV5645MIPI_write_cmos_sensor(0x8fb0, 0xa3);
	OV5645MIPI_write_cmos_sensor(0x8fb1, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x8fb2, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x8fb3, 0x43);
	OV5645MIPI_write_cmos_sensor(0x8fb4, 0xa3);
	OV5645MIPI_write_cmos_sensor(0x8fb5, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x8fb6, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x8fb7, 0x44);
	OV5645MIPI_write_cmos_sensor(0x8fb8, 0xa3);
	OV5645MIPI_write_cmos_sensor(0x8fb9, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x8fba, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x8fbb, 0x45);
	OV5645MIPI_write_cmos_sensor(0x8fbc, 0xa3);
	OV5645MIPI_write_cmos_sensor(0x8fbd, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x8fbe, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x8fbf, 0x41);
	OV5645MIPI_write_cmos_sensor(0x8fc0, 0xd2);
	OV5645MIPI_write_cmos_sensor(0x8fc1, 0x35);
	OV5645MIPI_write_cmos_sensor(0x8fc2, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x8fc3, 0x46);
	OV5645MIPI_write_cmos_sensor(0x8fc4, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8fc5, 0x0c);
	OV5645MIPI_write_cmos_sensor(0x8fc6, 0x60);
	OV5645MIPI_write_cmos_sensor(0x8fc7, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x8fc8, 0xf2);
	OV5645MIPI_write_cmos_sensor(0x8fc9, 0x03);
	OV5645MIPI_write_cmos_sensor(0x8fca, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x8fcb, 0xff);
	OV5645MIPI_write_cmos_sensor(0x8fcc, 0x04);
	OV5645MIPI_write_cmos_sensor(0x8fcd, 0x10);
	OV5645MIPI_write_cmos_sensor(0x8fce, 0x10);
	OV5645MIPI_write_cmos_sensor(0x8fcf, 0x05);
	OV5645MIPI_write_cmos_sensor(0x8fd0, 0x10);
	OV5645MIPI_write_cmos_sensor(0x8fd1, 0x13);
	OV5645MIPI_write_cmos_sensor(0x8fd2, 0x06);
	OV5645MIPI_write_cmos_sensor(0x8fd3, 0x10);
	OV5645MIPI_write_cmos_sensor(0x8fd4, 0x63);
	OV5645MIPI_write_cmos_sensor(0x8fd5, 0x07);
	OV5645MIPI_write_cmos_sensor(0x8fd6, 0x10);
	OV5645MIPI_write_cmos_sensor(0x8fd7, 0x1c);
	OV5645MIPI_write_cmos_sensor(0x8fd8, 0x08);
	OV5645MIPI_write_cmos_sensor(0x8fd9, 0x10);
	OV5645MIPI_write_cmos_sensor(0x8fda, 0x31);
	OV5645MIPI_write_cmos_sensor(0x8fdb, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8fdc, 0x10);
	OV5645MIPI_write_cmos_sensor(0x8fdd, 0x40);
	OV5645MIPI_write_cmos_sensor(0x8fde, 0x1a);
	OV5645MIPI_write_cmos_sensor(0x8fdf, 0x10);
	OV5645MIPI_write_cmos_sensor(0x8fe0, 0x4b);
	OV5645MIPI_write_cmos_sensor(0x8fe1, 0x1b);
	OV5645MIPI_write_cmos_sensor(0x8fe2, 0x10);
	OV5645MIPI_write_cmos_sensor(0x8fe3, 0x31);
	OV5645MIPI_write_cmos_sensor(0x8fe4, 0x80);
	OV5645MIPI_write_cmos_sensor(0x8fe5, 0x10);
	OV5645MIPI_write_cmos_sensor(0x8fe6, 0x2c);
	OV5645MIPI_write_cmos_sensor(0x8fe7, 0x81);
	OV5645MIPI_write_cmos_sensor(0x8fe8, 0x10);
	OV5645MIPI_write_cmos_sensor(0x8fe9, 0x63);
	OV5645MIPI_write_cmos_sensor(0x8fea, 0xdc);
	OV5645MIPI_write_cmos_sensor(0x8feb, 0x10);
	OV5645MIPI_write_cmos_sensor(0x8fec, 0x53);
	OV5645MIPI_write_cmos_sensor(0x8fed, 0xec);
	OV5645MIPI_write_cmos_sensor(0x8fee, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8fef, 0x00);
	OV5645MIPI_write_cmos_sensor(0x8ff0, 0x10);
	OV5645MIPI_write_cmos_sensor(0x8ff1, 0x7e);
	OV5645MIPI_write_cmos_sensor(0x8ff2, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8ff3, 0x15);
	OV5645MIPI_write_cmos_sensor(0x8ff4, 0x4e);
	OV5645MIPI_write_cmos_sensor(0x8ff5, 0xd2);
	OV5645MIPI_write_cmos_sensor(0x8ff6, 0x37);
	OV5645MIPI_write_cmos_sensor(0x8ff7, 0xd2);
	OV5645MIPI_write_cmos_sensor(0x8ff8, 0x01);
	OV5645MIPI_write_cmos_sensor(0x8ff9, 0xc2);
	OV5645MIPI_write_cmos_sensor(0x8ffa, 0x02);
	OV5645MIPI_write_cmos_sensor(0x8ffb, 0x12);
	OV5645MIPI_write_cmos_sensor(0x8ffc, 0x15);
	OV5645MIPI_write_cmos_sensor(0x8ffd, 0x53);
	OV5645MIPI_write_cmos_sensor(0x8ffe, 0x22);
	OV5645MIPI_write_cmos_sensor(0x8fff, 0xd2);
	OV5645MIPI_write_cmos_sensor(0x9000, 0x34);
	OV5645MIPI_write_cmos_sensor(0x9001, 0xd2);
	OV5645MIPI_write_cmos_sensor(0x9002, 0x37);
	OV5645MIPI_write_cmos_sensor(0x9003, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x9004, 0x42);
	OV5645MIPI_write_cmos_sensor(0x9005, 0xd3);
	OV5645MIPI_write_cmos_sensor(0x9006, 0x94);
	OV5645MIPI_write_cmos_sensor(0x9007, 0x00);
	OV5645MIPI_write_cmos_sensor(0x9008, 0x40);
	OV5645MIPI_write_cmos_sensor(0x9009, 0x03);
	OV5645MIPI_write_cmos_sensor(0x900a, 0x12);
	OV5645MIPI_write_cmos_sensor(0x900b, 0x15);
	OV5645MIPI_write_cmos_sensor(0x900c, 0x4e);
	OV5645MIPI_write_cmos_sensor(0x900d, 0xd2);
	OV5645MIPI_write_cmos_sensor(0x900e, 0x03);
	OV5645MIPI_write_cmos_sensor(0x900f, 0x22);
	OV5645MIPI_write_cmos_sensor(0x9010, 0xd2);
	OV5645MIPI_write_cmos_sensor(0x9011, 0x03);
	OV5645MIPI_write_cmos_sensor(0x9012, 0x22);
	OV5645MIPI_write_cmos_sensor(0x9013, 0xc2);
	OV5645MIPI_write_cmos_sensor(0x9014, 0x03);
	OV5645MIPI_write_cmos_sensor(0x9015, 0x20);
	OV5645MIPI_write_cmos_sensor(0x9016, 0x01);
	OV5645MIPI_write_cmos_sensor(0x9017, 0x66);
	OV5645MIPI_write_cmos_sensor(0x9018, 0x30);
	OV5645MIPI_write_cmos_sensor(0x9019, 0x02);
	OV5645MIPI_write_cmos_sensor(0x901a, 0x48);
	OV5645MIPI_write_cmos_sensor(0x901b, 0x22);
	OV5645MIPI_write_cmos_sensor(0x901c, 0xc2);
	OV5645MIPI_write_cmos_sensor(0x901d, 0x01);
	OV5645MIPI_write_cmos_sensor(0x901e, 0xc2);
	OV5645MIPI_write_cmos_sensor(0x901f, 0x02);
	OV5645MIPI_write_cmos_sensor(0x9020, 0xc2);
	OV5645MIPI_write_cmos_sensor(0x9021, 0x03);
	OV5645MIPI_write_cmos_sensor(0x9022, 0x12);
	OV5645MIPI_write_cmos_sensor(0x9023, 0x13);
	OV5645MIPI_write_cmos_sensor(0x9024, 0xc5);
	OV5645MIPI_write_cmos_sensor(0x9025, 0x75);
	OV5645MIPI_write_cmos_sensor(0x9026, 0x1d);
	OV5645MIPI_write_cmos_sensor(0x9027, 0x70);
	OV5645MIPI_write_cmos_sensor(0x9028, 0xd2);
	OV5645MIPI_write_cmos_sensor(0x9029, 0x36);
	OV5645MIPI_write_cmos_sensor(0x902a, 0x80);
	OV5645MIPI_write_cmos_sensor(0x902b, 0x37);
	OV5645MIPI_write_cmos_sensor(0x902c, 0x43);
	OV5645MIPI_write_cmos_sensor(0x902d, 0x41);
	OV5645MIPI_write_cmos_sensor(0x902e, 0x10);
	OV5645MIPI_write_cmos_sensor(0x902f, 0x80);
	OV5645MIPI_write_cmos_sensor(0x9030, 0x0a);
	OV5645MIPI_write_cmos_sensor(0x9031, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x9032, 0x41);
	OV5645MIPI_write_cmos_sensor(0x9033, 0x70);
	OV5645MIPI_write_cmos_sensor(0x9034, 0x03);
	OV5645MIPI_write_cmos_sensor(0x9035, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x9036, 0x80);
	OV5645MIPI_write_cmos_sensor(0x9037, 0x01);
	OV5645MIPI_write_cmos_sensor(0x9038, 0xd3);
	OV5645MIPI_write_cmos_sensor(0x9039, 0x92);
	OV5645MIPI_write_cmos_sensor(0x903a, 0x39);
	OV5645MIPI_write_cmos_sensor(0x903b, 0x12);
	OV5645MIPI_write_cmos_sensor(0x903c, 0x08);
	OV5645MIPI_write_cmos_sensor(0x903d, 0x60);
	OV5645MIPI_write_cmos_sensor(0x903e, 0x80);
	OV5645MIPI_write_cmos_sensor(0x903f, 0x23);
	OV5645MIPI_write_cmos_sensor(0x9040, 0x85);
	OV5645MIPI_write_cmos_sensor(0x9041, 0x45);
	OV5645MIPI_write_cmos_sensor(0x9042, 0x4e);
	OV5645MIPI_write_cmos_sensor(0x9043, 0x85);
	OV5645MIPI_write_cmos_sensor(0x9044, 0x41);
	OV5645MIPI_write_cmos_sensor(0x9045, 0x4f);
	OV5645MIPI_write_cmos_sensor(0x9046, 0x12);
	OV5645MIPI_write_cmos_sensor(0x9047, 0x12);
	OV5645MIPI_write_cmos_sensor(0x9048, 0xaf);
	OV5645MIPI_write_cmos_sensor(0x9049, 0x80);
	OV5645MIPI_write_cmos_sensor(0x904a, 0x18);
	OV5645MIPI_write_cmos_sensor(0x904b, 0x85);
	OV5645MIPI_write_cmos_sensor(0x904c, 0x4e);
	OV5645MIPI_write_cmos_sensor(0x904d, 0x45);
	OV5645MIPI_write_cmos_sensor(0x904e, 0x85);
	OV5645MIPI_write_cmos_sensor(0x904f, 0x4f);
	OV5645MIPI_write_cmos_sensor(0x9050, 0x41);
	OV5645MIPI_write_cmos_sensor(0x9051, 0x80);
	OV5645MIPI_write_cmos_sensor(0x9052, 0x10);
	OV5645MIPI_write_cmos_sensor(0x9053, 0xc2);
	OV5645MIPI_write_cmos_sensor(0x9054, 0xaf);
	OV5645MIPI_write_cmos_sensor(0x9055, 0x85);
	OV5645MIPI_write_cmos_sensor(0x9056, 0x2a);
	OV5645MIPI_write_cmos_sensor(0x9057, 0x42);
	OV5645MIPI_write_cmos_sensor(0x9058, 0x85);
	OV5645MIPI_write_cmos_sensor(0x9059, 0x2b);
	OV5645MIPI_write_cmos_sensor(0x905a, 0x43);
	OV5645MIPI_write_cmos_sensor(0x905b, 0x85);
	OV5645MIPI_write_cmos_sensor(0x905c, 0x2c);
	OV5645MIPI_write_cmos_sensor(0x905d, 0x44);
	OV5645MIPI_write_cmos_sensor(0x905e, 0x85);
	OV5645MIPI_write_cmos_sensor(0x905f, 0x2d);
	OV5645MIPI_write_cmos_sensor(0x9060, 0x45);
	OV5645MIPI_write_cmos_sensor(0x9061, 0xd2);
	OV5645MIPI_write_cmos_sensor(0x9062, 0xaf);
	OV5645MIPI_write_cmos_sensor(0x9063, 0x90);
	OV5645MIPI_write_cmos_sensor(0x9064, 0x30);
	OV5645MIPI_write_cmos_sensor(0x9065, 0x24);
	OV5645MIPI_write_cmos_sensor(0x9066, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x9067, 0x42);
	OV5645MIPI_write_cmos_sensor(0x9068, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x9069, 0xa3);
	OV5645MIPI_write_cmos_sensor(0x906a, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x906b, 0x43);
	OV5645MIPI_write_cmos_sensor(0x906c, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x906d, 0xa3);
	OV5645MIPI_write_cmos_sensor(0x906e, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x906f, 0x44);
	OV5645MIPI_write_cmos_sensor(0x9070, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x9071, 0xa3);
	OV5645MIPI_write_cmos_sensor(0x9072, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x9073, 0x45);
	OV5645MIPI_write_cmos_sensor(0x9074, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x9075, 0xa3);
	OV5645MIPI_write_cmos_sensor(0x9076, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x9077, 0x41);
	OV5645MIPI_write_cmos_sensor(0x9078, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x9079, 0x90);
	OV5645MIPI_write_cmos_sensor(0x907a, 0x30);
	OV5645MIPI_write_cmos_sensor(0x907b, 0x23);
	OV5645MIPI_write_cmos_sensor(0x907c, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x907d, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x907e, 0x22);
	OV5645MIPI_write_cmos_sensor(0x907f, 0x78);
	OV5645MIPI_write_cmos_sensor(0x9080, 0xc8);
	OV5645MIPI_write_cmos_sensor(0x9081, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x9082, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x9083, 0x0b);
	OV5645MIPI_write_cmos_sensor(0x9084, 0x18);
	OV5645MIPI_write_cmos_sensor(0x9085, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x9086, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x9087, 0x0c);
	OV5645MIPI_write_cmos_sensor(0x9088, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x9089, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x908a, 0x0d);
	OV5645MIPI_write_cmos_sensor(0x908b, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x908c, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x908d, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x908e, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x908f, 0xf9);
	OV5645MIPI_write_cmos_sensor(0x9090, 0x78);
	OV5645MIPI_write_cmos_sensor(0x9091, 0xc9);
	OV5645MIPI_write_cmos_sensor(0x9092, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x9093, 0x08);
	OV5645MIPI_write_cmos_sensor(0x9094, 0xf6);
	OV5645MIPI_write_cmos_sensor(0x9095, 0x78);
	OV5645MIPI_write_cmos_sensor(0x9096, 0xc7);
	OV5645MIPI_write_cmos_sensor(0x9097, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x9098, 0xff);
	OV5645MIPI_write_cmos_sensor(0x9099, 0x04);
	OV5645MIPI_write_cmos_sensor(0x909a, 0xfe);
	OV5645MIPI_write_cmos_sensor(0x909b, 0x78);
	OV5645MIPI_write_cmos_sensor(0x909c, 0xc8);
	OV5645MIPI_write_cmos_sensor(0x909d, 0x12);
	OV5645MIPI_write_cmos_sensor(0x909e, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x909f, 0x5b);
	OV5645MIPI_write_cmos_sensor(0x90a0, 0x50);
	OV5645MIPI_write_cmos_sensor(0x90a1, 0x54);
	OV5645MIPI_write_cmos_sensor(0x90a2, 0x12);
	OV5645MIPI_write_cmos_sensor(0x90a3, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x90a4, 0xd3);
	OV5645MIPI_write_cmos_sensor(0x90a5, 0xfa);
	OV5645MIPI_write_cmos_sensor(0x90a6, 0x08);
	OV5645MIPI_write_cmos_sensor(0x90a7, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x90a8, 0xd3);
	OV5645MIPI_write_cmos_sensor(0x90a9, 0x9d);
	OV5645MIPI_write_cmos_sensor(0x90aa, 0xea);
	OV5645MIPI_write_cmos_sensor(0x90ab, 0x9c);
	OV5645MIPI_write_cmos_sensor(0x90ac, 0x40);
	OV5645MIPI_write_cmos_sensor(0x90ad, 0x14);
	OV5645MIPI_write_cmos_sensor(0x90ae, 0x05);
	OV5645MIPI_write_cmos_sensor(0x90af, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x90b0, 0xd3);
	OV5645MIPI_write_cmos_sensor(0x90b1, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x90b2, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x90b3, 0x64);
	OV5645MIPI_write_cmos_sensor(0x90b4, 0x80);
	OV5645MIPI_write_cmos_sensor(0x90b5, 0xf8);
	OV5645MIPI_write_cmos_sensor(0x90b6, 0xe9);
	OV5645MIPI_write_cmos_sensor(0x90b7, 0x64);
	OV5645MIPI_write_cmos_sensor(0x90b8, 0x80);
	OV5645MIPI_write_cmos_sensor(0x90b9, 0x98);
	OV5645MIPI_write_cmos_sensor(0x90ba, 0x40);
	OV5645MIPI_write_cmos_sensor(0x90bb, 0x02);
	OV5645MIPI_write_cmos_sensor(0x90bc, 0x89);
	OV5645MIPI_write_cmos_sensor(0x90bd, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x90be, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x90bf, 0xf9);
	OV5645MIPI_write_cmos_sensor(0x90c0, 0x80);
	OV5645MIPI_write_cmos_sensor(0x90c1, 0x1b);
	OV5645MIPI_write_cmos_sensor(0x90c2, 0x12);
	OV5645MIPI_write_cmos_sensor(0x90c3, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x90c4, 0xd3);
	OV5645MIPI_write_cmos_sensor(0x90c5, 0x12);
	OV5645MIPI_write_cmos_sensor(0x90c6, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x90c7, 0x79);
	OV5645MIPI_write_cmos_sensor(0x90c8, 0x50);
	OV5645MIPI_write_cmos_sensor(0x90c9, 0x13);
	OV5645MIPI_write_cmos_sensor(0x90ca, 0x09);
	OV5645MIPI_write_cmos_sensor(0x90cb, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x90cc, 0x0d);
	OV5645MIPI_write_cmos_sensor(0x90cd, 0x64);
	OV5645MIPI_write_cmos_sensor(0x90ce, 0x80);
	OV5645MIPI_write_cmos_sensor(0x90cf, 0xf8);
	OV5645MIPI_write_cmos_sensor(0x90d0, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x90d1, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x90d2, 0x64);
	OV5645MIPI_write_cmos_sensor(0x90d3, 0x80);
	OV5645MIPI_write_cmos_sensor(0x90d4, 0x98);
	OV5645MIPI_write_cmos_sensor(0x90d5, 0x40);
	OV5645MIPI_write_cmos_sensor(0x90d6, 0x03);
	OV5645MIPI_write_cmos_sensor(0x90d7, 0x85);
	OV5645MIPI_write_cmos_sensor(0x90d8, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x90d9, 0x0d);
	OV5645MIPI_write_cmos_sensor(0x90da, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x90db, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x90dc, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x90dd, 0x78);
	OV5645MIPI_write_cmos_sensor(0x90de, 0xc9);
	OV5645MIPI_write_cmos_sensor(0x90df, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x90e0, 0x12);
	OV5645MIPI_write_cmos_sensor(0x90e1, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x90e2, 0xd4);
	OV5645MIPI_write_cmos_sensor(0x90e3, 0xfa);
	OV5645MIPI_write_cmos_sensor(0x90e4, 0x08);
	OV5645MIPI_write_cmos_sensor(0x90e5, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x90e6, 0xb5);
	OV5645MIPI_write_cmos_sensor(0x90e7, 0x05);
	OV5645MIPI_write_cmos_sensor(0x90e8, 0x08);
	OV5645MIPI_write_cmos_sensor(0x90e9, 0xea);
	OV5645MIPI_write_cmos_sensor(0x90ea, 0xb5);
	OV5645MIPI_write_cmos_sensor(0x90eb, 0x04);
	OV5645MIPI_write_cmos_sensor(0x90ec, 0x04);
	OV5645MIPI_write_cmos_sensor(0x90ed, 0x78);
	OV5645MIPI_write_cmos_sensor(0x90ee, 0xca);
	OV5645MIPI_write_cmos_sensor(0x90ef, 0xa6);
	OV5645MIPI_write_cmos_sensor(0x90f0, 0x06);
	OV5645MIPI_write_cmos_sensor(0x90f1, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x90f2, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x90f3, 0x02);
	OV5645MIPI_write_cmos_sensor(0x90f4, 0x10);
	OV5645MIPI_write_cmos_sensor(0x90f5, 0x9b);
	OV5645MIPI_write_cmos_sensor(0x90f6, 0x78);
	OV5645MIPI_write_cmos_sensor(0x90f7, 0xc7);
	OV5645MIPI_write_cmos_sensor(0x90f8, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x90f9, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x90fa, 0x0c);
	OV5645MIPI_write_cmos_sensor(0x90fb, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x90fc, 0x04);
	OV5645MIPI_write_cmos_sensor(0x90fd, 0xff);
	OV5645MIPI_write_cmos_sensor(0x90fe, 0x78);
	OV5645MIPI_write_cmos_sensor(0x90ff, 0xc9);
	OV5645MIPI_write_cmos_sensor(0x9100, 0x12);
	OV5645MIPI_write_cmos_sensor(0x9101, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x9102, 0x5b);
	OV5645MIPI_write_cmos_sensor(0x9103, 0x50);
	OV5645MIPI_write_cmos_sensor(0x9104, 0x17);
	OV5645MIPI_write_cmos_sensor(0x9105, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x9106, 0x0c);
	OV5645MIPI_write_cmos_sensor(0x9107, 0x12);
	OV5645MIPI_write_cmos_sensor(0x9108, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x9109, 0xdf);
	OV5645MIPI_write_cmos_sensor(0x910a, 0xfc);
	OV5645MIPI_write_cmos_sensor(0x910b, 0x08);
	OV5645MIPI_write_cmos_sensor(0x910c, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x910d, 0xfd);
	OV5645MIPI_write_cmos_sensor(0x910e, 0xef);
	OV5645MIPI_write_cmos_sensor(0x910f, 0x12);
	OV5645MIPI_write_cmos_sensor(0x9110, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x9111, 0xdf);
	OV5645MIPI_write_cmos_sensor(0x9112, 0x12);
	OV5645MIPI_write_cmos_sensor(0x9113, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x9114, 0x79);
	OV5645MIPI_write_cmos_sensor(0x9115, 0x50);
	OV5645MIPI_write_cmos_sensor(0x9116, 0x02);
	OV5645MIPI_write_cmos_sensor(0x9117, 0x8f);
	OV5645MIPI_write_cmos_sensor(0x9118, 0x0c);
	OV5645MIPI_write_cmos_sensor(0x9119, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x911a, 0x80);
	OV5645MIPI_write_cmos_sensor(0x911b, 0xe2);
	OV5645MIPI_write_cmos_sensor(0x911c, 0x78);
	OV5645MIPI_write_cmos_sensor(0x911d, 0xc8);
	OV5645MIPI_write_cmos_sensor(0x911e, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x911f, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x9120, 0x0b);
	OV5645MIPI_write_cmos_sensor(0x9121, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x9122, 0x14);
	OV5645MIPI_write_cmos_sensor(0x9123, 0xff);
	OV5645MIPI_write_cmos_sensor(0x9124, 0x78);
	OV5645MIPI_write_cmos_sensor(0x9125, 0xca);
	OV5645MIPI_write_cmos_sensor(0x9126, 0xd3);
	OV5645MIPI_write_cmos_sensor(0x9127, 0x12);
	OV5645MIPI_write_cmos_sensor(0x9128, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x9129, 0x5c);
	OV5645MIPI_write_cmos_sensor(0x912a, 0x40);
	OV5645MIPI_write_cmos_sensor(0x912b, 0x17);
	OV5645MIPI_write_cmos_sensor(0x912c, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x912d, 0x0b);
	OV5645MIPI_write_cmos_sensor(0x912e, 0x12);
	OV5645MIPI_write_cmos_sensor(0x912f, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x9130, 0xdf);
	OV5645MIPI_write_cmos_sensor(0x9131, 0xfc);
	OV5645MIPI_write_cmos_sensor(0x9132, 0x08);
	OV5645MIPI_write_cmos_sensor(0x9133, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x9134, 0xfd);
	OV5645MIPI_write_cmos_sensor(0x9135, 0xef);
	OV5645MIPI_write_cmos_sensor(0x9136, 0x12);
	OV5645MIPI_write_cmos_sensor(0x9137, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x9138, 0xdf);
	OV5645MIPI_write_cmos_sensor(0x9139, 0x12);
	OV5645MIPI_write_cmos_sensor(0x913a, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x913b, 0x79);
	OV5645MIPI_write_cmos_sensor(0x913c, 0x50);
	OV5645MIPI_write_cmos_sensor(0x913d, 0x02);
	OV5645MIPI_write_cmos_sensor(0x913e, 0x8f);
	OV5645MIPI_write_cmos_sensor(0x913f, 0x0b);
	OV5645MIPI_write_cmos_sensor(0x9140, 0x1f);
	OV5645MIPI_write_cmos_sensor(0x9141, 0x80);
	OV5645MIPI_write_cmos_sensor(0x9142, 0xe1);
	OV5645MIPI_write_cmos_sensor(0x9143, 0x78);
	OV5645MIPI_write_cmos_sensor(0x9144, 0xcb);
	OV5645MIPI_write_cmos_sensor(0x9145, 0xa6);
	OV5645MIPI_write_cmos_sensor(0x9146, 0x0c);
	OV5645MIPI_write_cmos_sensor(0x9147, 0x08);
	OV5645MIPI_write_cmos_sensor(0x9148, 0xa6);
	OV5645MIPI_write_cmos_sensor(0x9149, 0x0b);
	OV5645MIPI_write_cmos_sensor(0x914a, 0x22);
	OV5645MIPI_write_cmos_sensor(0x914b, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x914c, 0x0a);
	OV5645MIPI_write_cmos_sensor(0x914d, 0x75);
	OV5645MIPI_write_cmos_sensor(0x914e, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x914f, 0x03);
	OV5645MIPI_write_cmos_sensor(0x9150, 0xa4);
	OV5645MIPI_write_cmos_sensor(0x9151, 0x24);
	OV5645MIPI_write_cmos_sensor(0x9152, 0xb3);
	OV5645MIPI_write_cmos_sensor(0x9153, 0x25);
	OV5645MIPI_write_cmos_sensor(0x9154, 0x0b);
	OV5645MIPI_write_cmos_sensor(0x9155, 0xf8);
	OV5645MIPI_write_cmos_sensor(0x9156, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x9157, 0x22);
	OV5645MIPI_write_cmos_sensor(0x9158, 0xad);
	OV5645MIPI_write_cmos_sensor(0x9159, 0x11);
	OV5645MIPI_write_cmos_sensor(0x915a, 0xac);
	OV5645MIPI_write_cmos_sensor(0x915b, 0x10);
	OV5645MIPI_write_cmos_sensor(0x915c, 0xfa);
	OV5645MIPI_write_cmos_sensor(0x915d, 0xf9);
	OV5645MIPI_write_cmos_sensor(0x915e, 0xf8);
	OV5645MIPI_write_cmos_sensor(0x915f, 0x12);
	OV5645MIPI_write_cmos_sensor(0x9160, 0x0a);
	OV5645MIPI_write_cmos_sensor(0x9161, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x9162, 0x8f);
	OV5645MIPI_write_cmos_sensor(0x9163, 0x13);
	OV5645MIPI_write_cmos_sensor(0x9164, 0x8e);
	OV5645MIPI_write_cmos_sensor(0x9165, 0x12);
	OV5645MIPI_write_cmos_sensor(0x9166, 0x8d);
	OV5645MIPI_write_cmos_sensor(0x9167, 0x11);
	OV5645MIPI_write_cmos_sensor(0x9168, 0x8c);
	OV5645MIPI_write_cmos_sensor(0x9169, 0x10);
	OV5645MIPI_write_cmos_sensor(0x916a, 0xab);
	OV5645MIPI_write_cmos_sensor(0x916b, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x916c, 0xaa);
	OV5645MIPI_write_cmos_sensor(0x916d, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x916e, 0xa9);
	OV5645MIPI_write_cmos_sensor(0x916f, 0x0d);
	OV5645MIPI_write_cmos_sensor(0x9170, 0xa8);
	OV5645MIPI_write_cmos_sensor(0x9171, 0x0c);
	OV5645MIPI_write_cmos_sensor(0x9172, 0x22);
	OV5645MIPI_write_cmos_sensor(0x9173, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x9174, 0x0a);
	OV5645MIPI_write_cmos_sensor(0x9175, 0x75);
	OV5645MIPI_write_cmos_sensor(0x9176, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x9177, 0x08);
	OV5645MIPI_write_cmos_sensor(0x9178, 0xa4);
	OV5645MIPI_write_cmos_sensor(0x9179, 0x24);
	OV5645MIPI_write_cmos_sensor(0x917a, 0x5a);
	OV5645MIPI_write_cmos_sensor(0x917b, 0xf8);
	OV5645MIPI_write_cmos_sensor(0x917c, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x917d, 0x0b);
	OV5645MIPI_write_cmos_sensor(0x917e, 0x25);
	OV5645MIPI_write_cmos_sensor(0x917f, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x9180, 0x28);
	OV5645MIPI_write_cmos_sensor(0x9181, 0x22);
	OV5645MIPI_write_cmos_sensor(0x9182, 0xa6);
	OV5645MIPI_write_cmos_sensor(0x9183, 0x04);
	OV5645MIPI_write_cmos_sensor(0x9184, 0x08);
	OV5645MIPI_write_cmos_sensor(0x9185, 0xa6);
	OV5645MIPI_write_cmos_sensor(0x9186, 0x05);
	OV5645MIPI_write_cmos_sensor(0x9187, 0xef);
	OV5645MIPI_write_cmos_sensor(0x9188, 0x25);
	OV5645MIPI_write_cmos_sensor(0x9189, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x918a, 0x25);
	OV5645MIPI_write_cmos_sensor(0x918b, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x918c, 0x24);
	OV5645MIPI_write_cmos_sensor(0x918d, 0x2c);
	OV5645MIPI_write_cmos_sensor(0x918e, 0xf8);
	OV5645MIPI_write_cmos_sensor(0x918f, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x9190, 0xfe);
	OV5645MIPI_write_cmos_sensor(0x9191, 0x08);
	OV5645MIPI_write_cmos_sensor(0x9192, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x9193, 0xff);
	OV5645MIPI_write_cmos_sensor(0x9194, 0x22);
	OV5645MIPI_write_cmos_sensor(0x9195, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x9196, 0x0b);
	OV5645MIPI_write_cmos_sensor(0x9197, 0x93);
	OV5645MIPI_write_cmos_sensor(0x9198, 0xff);
	OV5645MIPI_write_cmos_sensor(0x9199, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x919a, 0xfc);
	OV5645MIPI_write_cmos_sensor(0x919b, 0xfd);
	OV5645MIPI_write_cmos_sensor(0x919c, 0xfe);
	OV5645MIPI_write_cmos_sensor(0x919d, 0x12);
	OV5645MIPI_write_cmos_sensor(0x919e, 0x0a);
	OV5645MIPI_write_cmos_sensor(0x919f, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x91a0, 0x8f);
	OV5645MIPI_write_cmos_sensor(0x91a1, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x91a2, 0x8e);
	OV5645MIPI_write_cmos_sensor(0x91a3, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x91a4, 0x8d);
	OV5645MIPI_write_cmos_sensor(0x91a5, 0x0d);
	OV5645MIPI_write_cmos_sensor(0x91a6, 0x8c);
	OV5645MIPI_write_cmos_sensor(0x91a7, 0x0c);
	OV5645MIPI_write_cmos_sensor(0x91a8, 0x22);
	OV5645MIPI_write_cmos_sensor(0x91a9, 0x90);
	OV5645MIPI_write_cmos_sensor(0x91aa, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x91ab, 0x8d);
	OV5645MIPI_write_cmos_sensor(0x91ac, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x91ad, 0x93);
	OV5645MIPI_write_cmos_sensor(0x91ae, 0xff);
	OV5645MIPI_write_cmos_sensor(0x91af, 0x25);
	OV5645MIPI_write_cmos_sensor(0x91b0, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x91b1, 0x25);
	OV5645MIPI_write_cmos_sensor(0x91b2, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x91b3, 0x24);
	OV5645MIPI_write_cmos_sensor(0x91b4, 0x2a);
	OV5645MIPI_write_cmos_sensor(0x91b5, 0xf8);
	OV5645MIPI_write_cmos_sensor(0x91b6, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x91b7, 0xfc);
	OV5645MIPI_write_cmos_sensor(0x91b8, 0x08);
	OV5645MIPI_write_cmos_sensor(0x91b9, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x91ba, 0xfd);
	OV5645MIPI_write_cmos_sensor(0x91bb, 0x22);
	OV5645MIPI_write_cmos_sensor(0x91bc, 0x78);
	OV5645MIPI_write_cmos_sensor(0x91bd, 0xaa);
	OV5645MIPI_write_cmos_sensor(0x91be, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x91bf, 0x75);
	OV5645MIPI_write_cmos_sensor(0x91c0, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x91c1, 0x08);
	OV5645MIPI_write_cmos_sensor(0x91c2, 0xa4);
	OV5645MIPI_write_cmos_sensor(0x91c3, 0x22);
	OV5645MIPI_write_cmos_sensor(0x91c4, 0xf8);
	OV5645MIPI_write_cmos_sensor(0x91c5, 0xa6);
	OV5645MIPI_write_cmos_sensor(0x91c6, 0x33);
	OV5645MIPI_write_cmos_sensor(0x91c7, 0x78);
	OV5645MIPI_write_cmos_sensor(0x91c8, 0xaa);
	OV5645MIPI_write_cmos_sensor(0x91c9, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x91ca, 0x75);
	OV5645MIPI_write_cmos_sensor(0x91cb, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x91cc, 0x03);
	OV5645MIPI_write_cmos_sensor(0x91cd, 0xa4);
	OV5645MIPI_write_cmos_sensor(0x91ce, 0x22);
	OV5645MIPI_write_cmos_sensor(0x91cf, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x91d0, 0x0b);
	OV5645MIPI_write_cmos_sensor(0x91d1, 0x25);
	OV5645MIPI_write_cmos_sensor(0x91d2, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x91d3, 0x24);
	OV5645MIPI_write_cmos_sensor(0x91d4, 0x5a);
	OV5645MIPI_write_cmos_sensor(0x91d5, 0xf8);
	OV5645MIPI_write_cmos_sensor(0x91d6, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x91d7, 0x22);
	OV5645MIPI_write_cmos_sensor(0x91d8, 0x08);
	OV5645MIPI_write_cmos_sensor(0x91d9, 0xa6);
	OV5645MIPI_write_cmos_sensor(0x91da, 0x2a);
	OV5645MIPI_write_cmos_sensor(0x91db, 0x08);
	OV5645MIPI_write_cmos_sensor(0x91dc, 0xa6);
	OV5645MIPI_write_cmos_sensor(0x91dd, 0x2b);
	OV5645MIPI_write_cmos_sensor(0x91de, 0x08);
	OV5645MIPI_write_cmos_sensor(0x91df, 0xa6);
	OV5645MIPI_write_cmos_sensor(0x91e0, 0x2c);
	OV5645MIPI_write_cmos_sensor(0x91e1, 0x08);
	OV5645MIPI_write_cmos_sensor(0x91e2, 0xa6);
	OV5645MIPI_write_cmos_sensor(0x91e3, 0x2d);
	OV5645MIPI_write_cmos_sensor(0x91e4, 0x22);
	OV5645MIPI_write_cmos_sensor(0x91e5, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x91e6, 0x0b);
	OV5645MIPI_write_cmos_sensor(0x91e7, 0x24);
	OV5645MIPI_write_cmos_sensor(0x91e8, 0x04);
	OV5645MIPI_write_cmos_sensor(0x91e9, 0xff);
	OV5645MIPI_write_cmos_sensor(0x91ea, 0x74);
	OV5645MIPI_write_cmos_sensor(0x91eb, 0x01);
	OV5645MIPI_write_cmos_sensor(0x91ec, 0xa8);
	OV5645MIPI_write_cmos_sensor(0x91ed, 0x07);
	OV5645MIPI_write_cmos_sensor(0x91ee, 0x08);
	OV5645MIPI_write_cmos_sensor(0x91ef, 0x22);
	OV5645MIPI_write_cmos_sensor(0x91f0, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x91f1, 0x8f);
	OV5645MIPI_write_cmos_sensor(0x91f2, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x91f3, 0x8e);
	OV5645MIPI_write_cmos_sensor(0x91f4, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x91f5, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x91f6, 0x0d);
	OV5645MIPI_write_cmos_sensor(0x91f7, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x91f8, 0x0c);
	OV5645MIPI_write_cmos_sensor(0x91f9, 0x22);
	OV5645MIPI_write_cmos_sensor(0x91fa, 0x74);
	OV5645MIPI_write_cmos_sensor(0x91fb, 0xb3);
	OV5645MIPI_write_cmos_sensor(0x91fc, 0x25);
	OV5645MIPI_write_cmos_sensor(0x91fd, 0x0b);
	OV5645MIPI_write_cmos_sensor(0x91fe, 0xf8);
	OV5645MIPI_write_cmos_sensor(0x91ff, 0xe6);
	OV5645MIPI_write_cmos_sensor(0x9200, 0x22);
	OV5645MIPI_write_cmos_sensor(0x9201, 0xc0);
	OV5645MIPI_write_cmos_sensor(0x9202, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x9203, 0xc0);
	OV5645MIPI_write_cmos_sensor(0x9204, 0x83);
	OV5645MIPI_write_cmos_sensor(0x9205, 0xc0);
	OV5645MIPI_write_cmos_sensor(0x9206, 0x82);
	OV5645MIPI_write_cmos_sensor(0x9207, 0xc0);
	OV5645MIPI_write_cmos_sensor(0x9208, 0xd0);
	OV5645MIPI_write_cmos_sensor(0x9209, 0x90);
	OV5645MIPI_write_cmos_sensor(0x920a, 0x3f);
	OV5645MIPI_write_cmos_sensor(0x920b, 0x0c);
	OV5645MIPI_write_cmos_sensor(0x920c, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x920d, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x920e, 0x08);
	OV5645MIPI_write_cmos_sensor(0x920f, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x9210, 0x08);
	OV5645MIPI_write_cmos_sensor(0x9211, 0x30);
	OV5645MIPI_write_cmos_sensor(0x9212, 0xe3);
	OV5645MIPI_write_cmos_sensor(0x9213, 0x60);
	OV5645MIPI_write_cmos_sensor(0x9214, 0x30);
	OV5645MIPI_write_cmos_sensor(0x9215, 0x37);
	OV5645MIPI_write_cmos_sensor(0x9216, 0x52);
	OV5645MIPI_write_cmos_sensor(0x9217, 0x90);
	OV5645MIPI_write_cmos_sensor(0x9218, 0x60);
	OV5645MIPI_write_cmos_sensor(0x9219, 0x19);
	OV5645MIPI_write_cmos_sensor(0x921a, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x921b, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x921c, 0x2a);
	OV5645MIPI_write_cmos_sensor(0x921d, 0xa3);
	OV5645MIPI_write_cmos_sensor(0x921e, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x921f, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x9220, 0x2b);
	OV5645MIPI_write_cmos_sensor(0x9221, 0x90);
	OV5645MIPI_write_cmos_sensor(0x9222, 0x60);
	OV5645MIPI_write_cmos_sensor(0x9223, 0x1d);
	OV5645MIPI_write_cmos_sensor(0x9224, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x9225, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x9226, 0x2c);
	OV5645MIPI_write_cmos_sensor(0x9227, 0xa3);
	OV5645MIPI_write_cmos_sensor(0x9228, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x9229, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x922a, 0x2d);
	OV5645MIPI_write_cmos_sensor(0x922b, 0x90);
	OV5645MIPI_write_cmos_sensor(0x922c, 0x60);
	OV5645MIPI_write_cmos_sensor(0x922d, 0x21);
	OV5645MIPI_write_cmos_sensor(0x922e, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x922f, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x9230, 0x2e);
	OV5645MIPI_write_cmos_sensor(0x9231, 0xa3);
	OV5645MIPI_write_cmos_sensor(0x9232, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x9233, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x9234, 0x2f);
	OV5645MIPI_write_cmos_sensor(0x9235, 0x90);
	OV5645MIPI_write_cmos_sensor(0x9236, 0x60);
	OV5645MIPI_write_cmos_sensor(0x9237, 0x25);
	OV5645MIPI_write_cmos_sensor(0x9238, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x9239, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x923a, 0x30);
	OV5645MIPI_write_cmos_sensor(0x923b, 0xa3);
	OV5645MIPI_write_cmos_sensor(0x923c, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x923d, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x923e, 0x31);
	OV5645MIPI_write_cmos_sensor(0x923f, 0x30);
	OV5645MIPI_write_cmos_sensor(0x9240, 0x01);
	OV5645MIPI_write_cmos_sensor(0x9241, 0x06);
	OV5645MIPI_write_cmos_sensor(0x9242, 0x30);
	OV5645MIPI_write_cmos_sensor(0x9243, 0x34);
	OV5645MIPI_write_cmos_sensor(0x9244, 0x03);
	OV5645MIPI_write_cmos_sensor(0x9245, 0xd3);
	OV5645MIPI_write_cmos_sensor(0x9246, 0x80);
	OV5645MIPI_write_cmos_sensor(0x9247, 0x01);
	OV5645MIPI_write_cmos_sensor(0x9248, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x9249, 0x92);
	OV5645MIPI_write_cmos_sensor(0x924a, 0x09);
	OV5645MIPI_write_cmos_sensor(0x924b, 0x30);
	OV5645MIPI_write_cmos_sensor(0x924c, 0x02);
	OV5645MIPI_write_cmos_sensor(0x924d, 0x06);
	OV5645MIPI_write_cmos_sensor(0x924e, 0x30);
	OV5645MIPI_write_cmos_sensor(0x924f, 0x34);
	OV5645MIPI_write_cmos_sensor(0x9250, 0x03);
	OV5645MIPI_write_cmos_sensor(0x9251, 0xd3);
	OV5645MIPI_write_cmos_sensor(0x9252, 0x80);
	OV5645MIPI_write_cmos_sensor(0x9253, 0x01);
	OV5645MIPI_write_cmos_sensor(0x9254, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x9255, 0x92);
	OV5645MIPI_write_cmos_sensor(0x9256, 0x0a);
	OV5645MIPI_write_cmos_sensor(0x9257, 0x30);
	OV5645MIPI_write_cmos_sensor(0x9258, 0x34);
	OV5645MIPI_write_cmos_sensor(0x9259, 0x0c);
	OV5645MIPI_write_cmos_sensor(0x925a, 0x30);
	OV5645MIPI_write_cmos_sensor(0x925b, 0x03);
	OV5645MIPI_write_cmos_sensor(0x925c, 0x09);
	OV5645MIPI_write_cmos_sensor(0x925d, 0x20);
	OV5645MIPI_write_cmos_sensor(0x925e, 0x02);
	OV5645MIPI_write_cmos_sensor(0x925f, 0x06);
	OV5645MIPI_write_cmos_sensor(0x9260, 0x20);
	OV5645MIPI_write_cmos_sensor(0x9261, 0x01);
	OV5645MIPI_write_cmos_sensor(0x9262, 0x03);
	OV5645MIPI_write_cmos_sensor(0x9263, 0xd3);
	OV5645MIPI_write_cmos_sensor(0x9264, 0x80);
	OV5645MIPI_write_cmos_sensor(0x9265, 0x01);
	OV5645MIPI_write_cmos_sensor(0x9266, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x9267, 0x92);
	OV5645MIPI_write_cmos_sensor(0x9268, 0x0b);
	OV5645MIPI_write_cmos_sensor(0x9269, 0x90);
	OV5645MIPI_write_cmos_sensor(0x926a, 0x30);
	OV5645MIPI_write_cmos_sensor(0x926b, 0x01);
	OV5645MIPI_write_cmos_sensor(0x926c, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x926d, 0x44);
	OV5645MIPI_write_cmos_sensor(0x926e, 0x40);
	OV5645MIPI_write_cmos_sensor(0x926f, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x9270, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x9271, 0x54);
	OV5645MIPI_write_cmos_sensor(0x9272, 0xbf);
	OV5645MIPI_write_cmos_sensor(0x9273, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x9274, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x9275, 0x08);
	OV5645MIPI_write_cmos_sensor(0x9276, 0x30);
	OV5645MIPI_write_cmos_sensor(0x9277, 0xe1);
	OV5645MIPI_write_cmos_sensor(0x9278, 0x14);
	OV5645MIPI_write_cmos_sensor(0x9279, 0x30);
	OV5645MIPI_write_cmos_sensor(0x927a, 0x35);
	OV5645MIPI_write_cmos_sensor(0x927b, 0x11);
	OV5645MIPI_write_cmos_sensor(0x927c, 0x90);
	OV5645MIPI_write_cmos_sensor(0x927d, 0x30);
	OV5645MIPI_write_cmos_sensor(0x927e, 0x22);
	OV5645MIPI_write_cmos_sensor(0x927f, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x9280, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x9281, 0x28);
	OV5645MIPI_write_cmos_sensor(0x9282, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x9283, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x9284, 0x30);
	OV5645MIPI_write_cmos_sensor(0x9285, 0x00);
	OV5645MIPI_write_cmos_sensor(0x9286, 0x03);
	OV5645MIPI_write_cmos_sensor(0x9287, 0xd3);
	OV5645MIPI_write_cmos_sensor(0x9288, 0x80);
	OV5645MIPI_write_cmos_sensor(0x9289, 0x01);
	OV5645MIPI_write_cmos_sensor(0x928a, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x928b, 0x92);
	OV5645MIPI_write_cmos_sensor(0x928c, 0x08);
	OV5645MIPI_write_cmos_sensor(0x928d, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x928e, 0x08);
	OV5645MIPI_write_cmos_sensor(0x928f, 0x30);
	OV5645MIPI_write_cmos_sensor(0x9290, 0xe2);
	OV5645MIPI_write_cmos_sensor(0x9291, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x9292, 0x90);
	OV5645MIPI_write_cmos_sensor(0x9293, 0x51);
	OV5645MIPI_write_cmos_sensor(0x9294, 0xa5);
	OV5645MIPI_write_cmos_sensor(0x9295, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x9296, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x9297, 0x33);
	OV5645MIPI_write_cmos_sensor(0x9298, 0xa3);
	OV5645MIPI_write_cmos_sensor(0x9299, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x929a, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x929b, 0x34);
	OV5645MIPI_write_cmos_sensor(0x929c, 0xa3);
	OV5645MIPI_write_cmos_sensor(0x929d, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x929e, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x929f, 0x35);
	OV5645MIPI_write_cmos_sensor(0x92a0, 0x90);
	OV5645MIPI_write_cmos_sensor(0x92a1, 0x3f);
	OV5645MIPI_write_cmos_sensor(0x92a2, 0x0c);
	OV5645MIPI_write_cmos_sensor(0x92a3, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x92a4, 0x08);
	OV5645MIPI_write_cmos_sensor(0x92a5, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x92a6, 0xd0);
	OV5645MIPI_write_cmos_sensor(0x92a7, 0xd0);
	OV5645MIPI_write_cmos_sensor(0x92a8, 0xd0);
	OV5645MIPI_write_cmos_sensor(0x92a9, 0x82);
	OV5645MIPI_write_cmos_sensor(0x92aa, 0xd0);
	OV5645MIPI_write_cmos_sensor(0x92ab, 0x83);
	OV5645MIPI_write_cmos_sensor(0x92ac, 0xd0);
	OV5645MIPI_write_cmos_sensor(0x92ad, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x92ae, 0x32);
	OV5645MIPI_write_cmos_sensor(0x92af, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x92b0, 0x4f);
	OV5645MIPI_write_cmos_sensor(0x92b1, 0xd3);
	OV5645MIPI_write_cmos_sensor(0x92b2, 0x94);
	OV5645MIPI_write_cmos_sensor(0x92b3, 0x40);
	OV5645MIPI_write_cmos_sensor(0x92b4, 0x40);
	OV5645MIPI_write_cmos_sensor(0x92b5, 0x04);
	OV5645MIPI_write_cmos_sensor(0x92b6, 0x7f);
	OV5645MIPI_write_cmos_sensor(0x92b7, 0x40);
	OV5645MIPI_write_cmos_sensor(0x92b8, 0x80);
	OV5645MIPI_write_cmos_sensor(0x92b9, 0x02);
	OV5645MIPI_write_cmos_sensor(0x92ba, 0xaf);
	OV5645MIPI_write_cmos_sensor(0x92bb, 0x4f);
	OV5645MIPI_write_cmos_sensor(0x92bc, 0x8f);
	OV5645MIPI_write_cmos_sensor(0x92bd, 0x4f);
	OV5645MIPI_write_cmos_sensor(0x92be, 0x90);
	OV5645MIPI_write_cmos_sensor(0x92bf, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x92c0, 0x86);
	OV5645MIPI_write_cmos_sensor(0x92c1, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x92c2, 0x93);
	OV5645MIPI_write_cmos_sensor(0x92c3, 0xfe);
	OV5645MIPI_write_cmos_sensor(0x92c4, 0x74);
	OV5645MIPI_write_cmos_sensor(0x92c5, 0x01);
	OV5645MIPI_write_cmos_sensor(0x92c6, 0x93);
	OV5645MIPI_write_cmos_sensor(0x92c7, 0xff);
	OV5645MIPI_write_cmos_sensor(0x92c8, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x92c9, 0x90);
	OV5645MIPI_write_cmos_sensor(0x92ca, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x92cb, 0x84);
	OV5645MIPI_write_cmos_sensor(0x92cc, 0x74);
	OV5645MIPI_write_cmos_sensor(0x92cd, 0x01);
	OV5645MIPI_write_cmos_sensor(0x92ce, 0x93);
	OV5645MIPI_write_cmos_sensor(0x92cf, 0x9f);
	OV5645MIPI_write_cmos_sensor(0x92d0, 0xff);
	OV5645MIPI_write_cmos_sensor(0x92d1, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x92d2, 0x93);
	OV5645MIPI_write_cmos_sensor(0x92d3, 0x9e);
	OV5645MIPI_write_cmos_sensor(0x92d4, 0xfe);
	OV5645MIPI_write_cmos_sensor(0x92d5, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x92d6, 0x8f);
	OV5645MIPI_write_cmos_sensor(0x92d7, 0x12);
	OV5645MIPI_write_cmos_sensor(0x92d8, 0x8e);
	OV5645MIPI_write_cmos_sensor(0x92d9, 0x11);
	OV5645MIPI_write_cmos_sensor(0x92da, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x92db, 0x10);
	OV5645MIPI_write_cmos_sensor(0x92dc, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x92dd, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x92de, 0xab);
	OV5645MIPI_write_cmos_sensor(0x92df, 0x12);
	OV5645MIPI_write_cmos_sensor(0x92e0, 0xaa);
	OV5645MIPI_write_cmos_sensor(0x92e1, 0x11);
	OV5645MIPI_write_cmos_sensor(0x92e2, 0xa9);
	OV5645MIPI_write_cmos_sensor(0x92e3, 0x10);
	OV5645MIPI_write_cmos_sensor(0x92e4, 0xa8);
	OV5645MIPI_write_cmos_sensor(0x92e5, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x92e6, 0xaf);
	OV5645MIPI_write_cmos_sensor(0x92e7, 0x4f);
	OV5645MIPI_write_cmos_sensor(0x92e8, 0xfc);
	OV5645MIPI_write_cmos_sensor(0x92e9, 0xfd);
	OV5645MIPI_write_cmos_sensor(0x92ea, 0xfe);
	OV5645MIPI_write_cmos_sensor(0x92eb, 0x12);
	OV5645MIPI_write_cmos_sensor(0x92ec, 0x0a);
	OV5645MIPI_write_cmos_sensor(0x92ed, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x92ee, 0x12);
	OV5645MIPI_write_cmos_sensor(0x92ef, 0x14);
	OV5645MIPI_write_cmos_sensor(0x92f0, 0xfd);
	OV5645MIPI_write_cmos_sensor(0x92f1, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x92f2, 0x7b);
	OV5645MIPI_write_cmos_sensor(0x92f3, 0x40);
	OV5645MIPI_write_cmos_sensor(0x92f4, 0xfa);
	OV5645MIPI_write_cmos_sensor(0x92f5, 0xf9);
	OV5645MIPI_write_cmos_sensor(0x92f6, 0xf8);
	OV5645MIPI_write_cmos_sensor(0x92f7, 0x12);
	OV5645MIPI_write_cmos_sensor(0x92f8, 0x0b);
	OV5645MIPI_write_cmos_sensor(0x92f9, 0x7b);
	OV5645MIPI_write_cmos_sensor(0x92fa, 0x12);
	OV5645MIPI_write_cmos_sensor(0x92fb, 0x14);
	OV5645MIPI_write_cmos_sensor(0x92fc, 0xfd);
	OV5645MIPI_write_cmos_sensor(0x92fd, 0x90);
	OV5645MIPI_write_cmos_sensor(0x92fe, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x92ff, 0x71);
	OV5645MIPI_write_cmos_sensor(0x9300, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x9301, 0x12);
	OV5645MIPI_write_cmos_sensor(0x9302, 0x15);
	OV5645MIPI_write_cmos_sensor(0x9303, 0x12);
	OV5645MIPI_write_cmos_sensor(0x9304, 0x12);
	OV5645MIPI_write_cmos_sensor(0x9305, 0x14);
	OV5645MIPI_write_cmos_sensor(0x9306, 0xfd);
	OV5645MIPI_write_cmos_sensor(0x9307, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x9308, 0x85);
	OV5645MIPI_write_cmos_sensor(0x9309, 0x4e);
	OV5645MIPI_write_cmos_sensor(0x930a, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x930b, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x930c, 0x0d);
	OV5645MIPI_write_cmos_sensor(0x930d, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x930e, 0x0c);
	OV5645MIPI_write_cmos_sensor(0x930f, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x9310, 0x0b);
	OV5645MIPI_write_cmos_sensor(0x9311, 0xaf);
	OV5645MIPI_write_cmos_sensor(0x9312, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x9313, 0xae);
	OV5645MIPI_write_cmos_sensor(0x9314, 0x0d);
	OV5645MIPI_write_cmos_sensor(0x9315, 0xad);
	OV5645MIPI_write_cmos_sensor(0x9316, 0x0c);
	OV5645MIPI_write_cmos_sensor(0x9317, 0xac);
	OV5645MIPI_write_cmos_sensor(0x9318, 0x0b);
	OV5645MIPI_write_cmos_sensor(0x9319, 0xa3);
	OV5645MIPI_write_cmos_sensor(0x931a, 0x12);
	OV5645MIPI_write_cmos_sensor(0x931b, 0x15);
	OV5645MIPI_write_cmos_sensor(0x931c, 0x12);
	OV5645MIPI_write_cmos_sensor(0x931d, 0x8f);
	OV5645MIPI_write_cmos_sensor(0x931e, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x931f, 0x8e);
	OV5645MIPI_write_cmos_sensor(0x9320, 0x0d);
	OV5645MIPI_write_cmos_sensor(0x9321, 0x8d);
	OV5645MIPI_write_cmos_sensor(0x9322, 0x0c);
	OV5645MIPI_write_cmos_sensor(0x9323, 0x8c);
	OV5645MIPI_write_cmos_sensor(0x9324, 0x0b);
	OV5645MIPI_write_cmos_sensor(0x9325, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x9326, 0x12);
	OV5645MIPI_write_cmos_sensor(0x9327, 0x45);
	OV5645MIPI_write_cmos_sensor(0x9328, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x9329, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x932a, 0x12);
	OV5645MIPI_write_cmos_sensor(0x932b, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x932c, 0x11);
	OV5645MIPI_write_cmos_sensor(0x932d, 0x45);
	OV5645MIPI_write_cmos_sensor(0x932e, 0x0d);
	OV5645MIPI_write_cmos_sensor(0x932f, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x9330, 0x11);
	OV5645MIPI_write_cmos_sensor(0x9331, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x9332, 0x10);
	OV5645MIPI_write_cmos_sensor(0x9333, 0x45);
	OV5645MIPI_write_cmos_sensor(0x9334, 0x0c);
	OV5645MIPI_write_cmos_sensor(0x9335, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x9336, 0x10);
	OV5645MIPI_write_cmos_sensor(0x9337, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x9338, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x9339, 0x45);
	OV5645MIPI_write_cmos_sensor(0x933a, 0x0b);
	OV5645MIPI_write_cmos_sensor(0x933b, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x933c, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x933d, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x933e, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x933f, 0x22);
	OV5645MIPI_write_cmos_sensor(0x9340, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x9341, 0x23);
	OV5645MIPI_write_cmos_sensor(0x9342, 0x85);
	OV5645MIPI_write_cmos_sensor(0x9343, 0x12);
	OV5645MIPI_write_cmos_sensor(0x9344, 0x40);
	OV5645MIPI_write_cmos_sensor(0x9345, 0x85);
	OV5645MIPI_write_cmos_sensor(0x9346, 0x11);
	OV5645MIPI_write_cmos_sensor(0x9347, 0x3f);
	OV5645MIPI_write_cmos_sensor(0x9348, 0x85);
	OV5645MIPI_write_cmos_sensor(0x9349, 0x10);
	OV5645MIPI_write_cmos_sensor(0x934a, 0x3e);
	OV5645MIPI_write_cmos_sensor(0x934b, 0x85);
	OV5645MIPI_write_cmos_sensor(0x934c, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x934d, 0x3d);
	OV5645MIPI_write_cmos_sensor(0x934e, 0x02);
	OV5645MIPI_write_cmos_sensor(0x934f, 0x14);
	OV5645MIPI_write_cmos_sensor(0x9350, 0xaf);
	OV5645MIPI_write_cmos_sensor(0x9351, 0x75);
	OV5645MIPI_write_cmos_sensor(0x9352, 0x89);
	OV5645MIPI_write_cmos_sensor(0x9353, 0x03);
	OV5645MIPI_write_cmos_sensor(0x9354, 0x75);
	OV5645MIPI_write_cmos_sensor(0x9355, 0xa8);
	OV5645MIPI_write_cmos_sensor(0x9356, 0x01);
	OV5645MIPI_write_cmos_sensor(0x9357, 0x75);
	OV5645MIPI_write_cmos_sensor(0x9358, 0xb8);
	OV5645MIPI_write_cmos_sensor(0x9359, 0x04);
	OV5645MIPI_write_cmos_sensor(0x935a, 0x75);
	OV5645MIPI_write_cmos_sensor(0x935b, 0x0a);
	OV5645MIPI_write_cmos_sensor(0x935c, 0xff);
	OV5645MIPI_write_cmos_sensor(0x935d, 0x75);
	OV5645MIPI_write_cmos_sensor(0x935e, 0x0b);
	OV5645MIPI_write_cmos_sensor(0x935f, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x9360, 0x75);
	OV5645MIPI_write_cmos_sensor(0x9361, 0x0c);
	OV5645MIPI_write_cmos_sensor(0x9362, 0x15);
	OV5645MIPI_write_cmos_sensor(0x9363, 0x75);
	OV5645MIPI_write_cmos_sensor(0x9364, 0x0d);
	OV5645MIPI_write_cmos_sensor(0x9365, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x9366, 0x12);
	OV5645MIPI_write_cmos_sensor(0x9367, 0x14);
	OV5645MIPI_write_cmos_sensor(0x9368, 0x36);
	OV5645MIPI_write_cmos_sensor(0x9369, 0x12);
	OV5645MIPI_write_cmos_sensor(0x936a, 0x08);
	OV5645MIPI_write_cmos_sensor(0x936b, 0x60);
	OV5645MIPI_write_cmos_sensor(0x936c, 0xc2);
	OV5645MIPI_write_cmos_sensor(0x936d, 0x39);
	OV5645MIPI_write_cmos_sensor(0x936e, 0x12);
	OV5645MIPI_write_cmos_sensor(0x936f, 0x00);
	OV5645MIPI_write_cmos_sensor(0x9370, 0x06);
	OV5645MIPI_write_cmos_sensor(0x9371, 0xd2);
	OV5645MIPI_write_cmos_sensor(0x9372, 0x00);
	OV5645MIPI_write_cmos_sensor(0x9373, 0xd2);
	OV5645MIPI_write_cmos_sensor(0x9374, 0x35);
	OV5645MIPI_write_cmos_sensor(0x9375, 0xd2);
	OV5645MIPI_write_cmos_sensor(0x9376, 0xaf);
	OV5645MIPI_write_cmos_sensor(0x9377, 0x75);
	OV5645MIPI_write_cmos_sensor(0x9378, 0x0a);
	OV5645MIPI_write_cmos_sensor(0x9379, 0xff);
	OV5645MIPI_write_cmos_sensor(0x937a, 0x75);
	OV5645MIPI_write_cmos_sensor(0x937b, 0x0b);
	OV5645MIPI_write_cmos_sensor(0x937c, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x937d, 0x75);
	OV5645MIPI_write_cmos_sensor(0x937e, 0x0c);
	OV5645MIPI_write_cmos_sensor(0x937f, 0x51);
	OV5645MIPI_write_cmos_sensor(0x9380, 0x75);
	OV5645MIPI_write_cmos_sensor(0x9381, 0x0d);
	OV5645MIPI_write_cmos_sensor(0x9382, 0x03);
	OV5645MIPI_write_cmos_sensor(0x9383, 0x12);
	OV5645MIPI_write_cmos_sensor(0x9384, 0x14);
	OV5645MIPI_write_cmos_sensor(0x9385, 0x36);
	OV5645MIPI_write_cmos_sensor(0x9386, 0x30);
	OV5645MIPI_write_cmos_sensor(0x9387, 0x08);
	OV5645MIPI_write_cmos_sensor(0x9388, 0x09);
	OV5645MIPI_write_cmos_sensor(0x9389, 0xc2);
	OV5645MIPI_write_cmos_sensor(0x938a, 0x35);
	OV5645MIPI_write_cmos_sensor(0x938b, 0x12);
	OV5645MIPI_write_cmos_sensor(0x938c, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x938d, 0xa7);
	OV5645MIPI_write_cmos_sensor(0x938e, 0xc2);
	OV5645MIPI_write_cmos_sensor(0x938f, 0x08);
	OV5645MIPI_write_cmos_sensor(0x9390, 0xd2);
	OV5645MIPI_write_cmos_sensor(0x9391, 0x35);
	OV5645MIPI_write_cmos_sensor(0x9392, 0x30);
	OV5645MIPI_write_cmos_sensor(0x9393, 0x0b);
	OV5645MIPI_write_cmos_sensor(0x9394, 0x09);
	OV5645MIPI_write_cmos_sensor(0x9395, 0xc2);
	OV5645MIPI_write_cmos_sensor(0x9396, 0x37);
	OV5645MIPI_write_cmos_sensor(0x9397, 0x12);
	OV5645MIPI_write_cmos_sensor(0x9398, 0x04);
	OV5645MIPI_write_cmos_sensor(0x9399, 0xa8);
	OV5645MIPI_write_cmos_sensor(0x939a, 0xc2);
	OV5645MIPI_write_cmos_sensor(0x939b, 0x0b);
	OV5645MIPI_write_cmos_sensor(0x939c, 0xd2);
	OV5645MIPI_write_cmos_sensor(0x939d, 0x37);
	OV5645MIPI_write_cmos_sensor(0x939e, 0x30);
	OV5645MIPI_write_cmos_sensor(0x939f, 0x09);
	OV5645MIPI_write_cmos_sensor(0x93a0, 0x09);
	OV5645MIPI_write_cmos_sensor(0x93a1, 0xc2);
	OV5645MIPI_write_cmos_sensor(0x93a2, 0x37);
	OV5645MIPI_write_cmos_sensor(0x93a3, 0x12);
	OV5645MIPI_write_cmos_sensor(0x93a4, 0x00);
	OV5645MIPI_write_cmos_sensor(0x93a5, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x93a6, 0xc2);
	OV5645MIPI_write_cmos_sensor(0x93a7, 0x09);
	OV5645MIPI_write_cmos_sensor(0x93a8, 0xd2);
	OV5645MIPI_write_cmos_sensor(0x93a9, 0x37);
	OV5645MIPI_write_cmos_sensor(0x93aa, 0x30);
	OV5645MIPI_write_cmos_sensor(0x93ab, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x93ac, 0x03);
	OV5645MIPI_write_cmos_sensor(0x93ad, 0x12);
	OV5645MIPI_write_cmos_sensor(0x93ae, 0x08);
	OV5645MIPI_write_cmos_sensor(0x93af, 0x60);
	OV5645MIPI_write_cmos_sensor(0x93b0, 0x30);
	OV5645MIPI_write_cmos_sensor(0x93b1, 0x36);
	OV5645MIPI_write_cmos_sensor(0x93b2, 0xd3);
	OV5645MIPI_write_cmos_sensor(0x93b3, 0x90);
	OV5645MIPI_write_cmos_sensor(0x93b4, 0x30);
	OV5645MIPI_write_cmos_sensor(0x93b5, 0x29);
	OV5645MIPI_write_cmos_sensor(0x93b6, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x93b7, 0x1d);
	OV5645MIPI_write_cmos_sensor(0x93b8, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x93b9, 0xb4);
	OV5645MIPI_write_cmos_sensor(0x93ba, 0x10);
	OV5645MIPI_write_cmos_sensor(0x93bb, 0x05);
	OV5645MIPI_write_cmos_sensor(0x93bc, 0x90);
	OV5645MIPI_write_cmos_sensor(0x93bd, 0x30);
	OV5645MIPI_write_cmos_sensor(0x93be, 0x23);
	OV5645MIPI_write_cmos_sensor(0x93bf, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x93c0, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x93c1, 0xc2);
	OV5645MIPI_write_cmos_sensor(0x93c2, 0x36);
	OV5645MIPI_write_cmos_sensor(0x93c3, 0x80);
	OV5645MIPI_write_cmos_sensor(0x93c4, 0xc1);
	OV5645MIPI_write_cmos_sensor(0x93c5, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x93c6, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x93c7, 0x4f);
	OV5645MIPI_write_cmos_sensor(0x93c8, 0x90);
	OV5645MIPI_write_cmos_sensor(0x93c9, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x93ca, 0x82);
	OV5645MIPI_write_cmos_sensor(0x93cb, 0x93);
	OV5645MIPI_write_cmos_sensor(0x93cc, 0xff);
	OV5645MIPI_write_cmos_sensor(0x93cd, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x93ce, 0x8f);
	OV5645MIPI_write_cmos_sensor(0x93cf, 0x0d);
	OV5645MIPI_write_cmos_sensor(0x93d0, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x93d1, 0x0c);
	OV5645MIPI_write_cmos_sensor(0x93d2, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x93d3, 0x0b);
	OV5645MIPI_write_cmos_sensor(0x93d4, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x93d5, 0x0a);
	OV5645MIPI_write_cmos_sensor(0x93d6, 0xaf);
	OV5645MIPI_write_cmos_sensor(0x93d7, 0x0d);
	OV5645MIPI_write_cmos_sensor(0x93d8, 0xae);
	OV5645MIPI_write_cmos_sensor(0x93d9, 0x0c);
	OV5645MIPI_write_cmos_sensor(0x93da, 0xad);
	OV5645MIPI_write_cmos_sensor(0x93db, 0x0b);
	OV5645MIPI_write_cmos_sensor(0x93dc, 0xac);
	OV5645MIPI_write_cmos_sensor(0x93dd, 0x0a);
	OV5645MIPI_write_cmos_sensor(0x93de, 0x90);
	OV5645MIPI_write_cmos_sensor(0x93df, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x93e0, 0x72);
	OV5645MIPI_write_cmos_sensor(0x93e1, 0x12);
	OV5645MIPI_write_cmos_sensor(0x93e2, 0x15);
	OV5645MIPI_write_cmos_sensor(0x93e3, 0x12);
	OV5645MIPI_write_cmos_sensor(0x93e4, 0x8f);
	OV5645MIPI_write_cmos_sensor(0x93e5, 0x0d);
	OV5645MIPI_write_cmos_sensor(0x93e6, 0x8e);
	OV5645MIPI_write_cmos_sensor(0x93e7, 0x0c);
	OV5645MIPI_write_cmos_sensor(0x93e8, 0x8d);
	OV5645MIPI_write_cmos_sensor(0x93e9, 0x0b);
	OV5645MIPI_write_cmos_sensor(0x93ea, 0x8c);
	OV5645MIPI_write_cmos_sensor(0x93eb, 0x0a);
	OV5645MIPI_write_cmos_sensor(0x93ec, 0x90);
	OV5645MIPI_write_cmos_sensor(0x93ed, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x93ee, 0x7a);
	OV5645MIPI_write_cmos_sensor(0x93ef, 0x12);
	OV5645MIPI_write_cmos_sensor(0x93f0, 0x0c);
	OV5645MIPI_write_cmos_sensor(0x93f1, 0x44);
	OV5645MIPI_write_cmos_sensor(0x93f2, 0xef);
	OV5645MIPI_write_cmos_sensor(0x93f3, 0x45);
	OV5645MIPI_write_cmos_sensor(0x93f4, 0x0d);
	OV5645MIPI_write_cmos_sensor(0x93f5, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x93f6, 0x0d);
	OV5645MIPI_write_cmos_sensor(0x93f7, 0xee);
	OV5645MIPI_write_cmos_sensor(0x93f8, 0x45);
	OV5645MIPI_write_cmos_sensor(0x93f9, 0x0c);
	OV5645MIPI_write_cmos_sensor(0x93fa, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x93fb, 0x0c);
	OV5645MIPI_write_cmos_sensor(0x93fc, 0xed);
	OV5645MIPI_write_cmos_sensor(0x93fd, 0x45);
	OV5645MIPI_write_cmos_sensor(0x93fe, 0x0b);
	OV5645MIPI_write_cmos_sensor(0x93ff, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x9400, 0x0b);
	OV5645MIPI_write_cmos_sensor(0x9401, 0xec);
	OV5645MIPI_write_cmos_sensor(0x9402, 0x45);
	OV5645MIPI_write_cmos_sensor(0x9403, 0x0a);
	OV5645MIPI_write_cmos_sensor(0x9404, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x9405, 0x0a);
	OV5645MIPI_write_cmos_sensor(0x9406, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x9407, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x9408, 0x22);
	OV5645MIPI_write_cmos_sensor(0x9409, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x940a, 0x23);
	OV5645MIPI_write_cmos_sensor(0x940b, 0x85);
	OV5645MIPI_write_cmos_sensor(0x940c, 0x0d);
	OV5645MIPI_write_cmos_sensor(0x940d, 0x40);
	OV5645MIPI_write_cmos_sensor(0x940e, 0x85);
	OV5645MIPI_write_cmos_sensor(0x940f, 0x0c);
	OV5645MIPI_write_cmos_sensor(0x9410, 0x3f);
	OV5645MIPI_write_cmos_sensor(0x9411, 0x85);
	OV5645MIPI_write_cmos_sensor(0x9412, 0x0b);
	OV5645MIPI_write_cmos_sensor(0x9413, 0x3e);
	OV5645MIPI_write_cmos_sensor(0x9414, 0x85);
	OV5645MIPI_write_cmos_sensor(0x9415, 0x0a);
	OV5645MIPI_write_cmos_sensor(0x9416, 0x3d);
	OV5645MIPI_write_cmos_sensor(0x9417, 0x12);
	OV5645MIPI_write_cmos_sensor(0x9418, 0x14);
	OV5645MIPI_write_cmos_sensor(0x9419, 0xaf);
	OV5645MIPI_write_cmos_sensor(0x941a, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x941b, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x941c, 0x22);
	OV5645MIPI_write_cmos_sensor(0x941d, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x941e, 0x23);
	OV5645MIPI_write_cmos_sensor(0x941f, 0x90);
	OV5645MIPI_write_cmos_sensor(0x9420, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x9421, 0x7a);
	OV5645MIPI_write_cmos_sensor(0x9422, 0x12);
	OV5645MIPI_write_cmos_sensor(0x9423, 0x15);
	OV5645MIPI_write_cmos_sensor(0x9424, 0x06);
	OV5645MIPI_write_cmos_sensor(0x9425, 0x12);
	OV5645MIPI_write_cmos_sensor(0x9426, 0x14);
	OV5645MIPI_write_cmos_sensor(0x9427, 0xaf);
	OV5645MIPI_write_cmos_sensor(0x9428, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x9429, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x942a, 0x22);
	OV5645MIPI_write_cmos_sensor(0x942b, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x942c, 0x23);
	OV5645MIPI_write_cmos_sensor(0x942d, 0x90);
	OV5645MIPI_write_cmos_sensor(0x942e, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x942f, 0x76);
	OV5645MIPI_write_cmos_sensor(0x9430, 0x12);
	OV5645MIPI_write_cmos_sensor(0x9431, 0x15);
	OV5645MIPI_write_cmos_sensor(0x9432, 0x06);
	OV5645MIPI_write_cmos_sensor(0x9433, 0x02);
	OV5645MIPI_write_cmos_sensor(0x9434, 0x14);
	OV5645MIPI_write_cmos_sensor(0x9435, 0xaf);
	OV5645MIPI_write_cmos_sensor(0x9436, 0xae);
	OV5645MIPI_write_cmos_sensor(0x9437, 0x0b);
	OV5645MIPI_write_cmos_sensor(0x9438, 0xaf);
	OV5645MIPI_write_cmos_sensor(0x9439, 0x0c);
	OV5645MIPI_write_cmos_sensor(0x943a, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x943b, 0xfd);
	OV5645MIPI_write_cmos_sensor(0x943c, 0xed);
	OV5645MIPI_write_cmos_sensor(0x943d, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x943e, 0x95);
	OV5645MIPI_write_cmos_sensor(0x943f, 0x0d);
	OV5645MIPI_write_cmos_sensor(0x9440, 0x50);
	OV5645MIPI_write_cmos_sensor(0x9441, 0x33);
	OV5645MIPI_write_cmos_sensor(0x9442, 0x12);
	OV5645MIPI_write_cmos_sensor(0x9443, 0x15);
	OV5645MIPI_write_cmos_sensor(0x9444, 0x68);
	OV5645MIPI_write_cmos_sensor(0x9445, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x9446, 0x93);
	OV5645MIPI_write_cmos_sensor(0x9447, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x9448, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x9449, 0x74);
	OV5645MIPI_write_cmos_sensor(0x944a, 0x01);
	OV5645MIPI_write_cmos_sensor(0x944b, 0x93);
	OV5645MIPI_write_cmos_sensor(0x944c, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x944d, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x944e, 0x45);
	OV5645MIPI_write_cmos_sensor(0x944f, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x9450, 0x60);
	OV5645MIPI_write_cmos_sensor(0x9451, 0x23);
	OV5645MIPI_write_cmos_sensor(0x9452, 0x85);
	OV5645MIPI_write_cmos_sensor(0x9453, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x9454, 0x82);
	OV5645MIPI_write_cmos_sensor(0x9455, 0x85);
	OV5645MIPI_write_cmos_sensor(0x9456, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x9457, 0x83);
	OV5645MIPI_write_cmos_sensor(0x9458, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x9459, 0xfc);
	OV5645MIPI_write_cmos_sensor(0x945a, 0x12);
	OV5645MIPI_write_cmos_sensor(0x945b, 0x15);
	OV5645MIPI_write_cmos_sensor(0x945c, 0x68);
	OV5645MIPI_write_cmos_sensor(0x945d, 0x74);
	OV5645MIPI_write_cmos_sensor(0x945e, 0x03);
	OV5645MIPI_write_cmos_sensor(0x945f, 0x93);
	OV5645MIPI_write_cmos_sensor(0x9460, 0x52);
	OV5645MIPI_write_cmos_sensor(0x9461, 0x04);
	OV5645MIPI_write_cmos_sensor(0x9462, 0x12);
	OV5645MIPI_write_cmos_sensor(0x9463, 0x15);
	OV5645MIPI_write_cmos_sensor(0x9464, 0x68);
	OV5645MIPI_write_cmos_sensor(0x9465, 0x74);
	OV5645MIPI_write_cmos_sensor(0x9466, 0x02);
	OV5645MIPI_write_cmos_sensor(0x9467, 0x93);
	OV5645MIPI_write_cmos_sensor(0x9468, 0x42);
	OV5645MIPI_write_cmos_sensor(0x9469, 0x04);
	OV5645MIPI_write_cmos_sensor(0x946a, 0x85);
	OV5645MIPI_write_cmos_sensor(0x946b, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x946c, 0x82);
	OV5645MIPI_write_cmos_sensor(0x946d, 0x85);
	OV5645MIPI_write_cmos_sensor(0x946e, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x946f, 0x83);
	OV5645MIPI_write_cmos_sensor(0x9470, 0xec);
	OV5645MIPI_write_cmos_sensor(0x9471, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x9472, 0x0d);
	OV5645MIPI_write_cmos_sensor(0x9473, 0x80);
	OV5645MIPI_write_cmos_sensor(0x9474, 0xc7);
	OV5645MIPI_write_cmos_sensor(0x9475, 0x22);
	OV5645MIPI_write_cmos_sensor(0x9476, 0x7e);
	OV5645MIPI_write_cmos_sensor(0x9477, 0x00);
	OV5645MIPI_write_cmos_sensor(0x9478, 0xad);
	OV5645MIPI_write_cmos_sensor(0x9479, 0x03);
	OV5645MIPI_write_cmos_sensor(0x947a, 0xac);
	OV5645MIPI_write_cmos_sensor(0x947b, 0x02);
	OV5645MIPI_write_cmos_sensor(0x947c, 0x12);
	OV5645MIPI_write_cmos_sensor(0x947d, 0x0a);
	OV5645MIPI_write_cmos_sensor(0x947e, 0x89);
	OV5645MIPI_write_cmos_sensor(0x947f, 0x8e);
	OV5645MIPI_write_cmos_sensor(0x9480, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x9481, 0x8f);
	OV5645MIPI_write_cmos_sensor(0x9482, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x9483, 0x22);
	OV5645MIPI_write_cmos_sensor(0x9484, 0x12);
	OV5645MIPI_write_cmos_sensor(0x9485, 0x0a);
	OV5645MIPI_write_cmos_sensor(0x9486, 0x89);
	OV5645MIPI_write_cmos_sensor(0x9487, 0x8e);
	OV5645MIPI_write_cmos_sensor(0x9488, 0x10);
	OV5645MIPI_write_cmos_sensor(0x9489, 0x8f);
	OV5645MIPI_write_cmos_sensor(0x948a, 0x11);
	OV5645MIPI_write_cmos_sensor(0x948b, 0x90);
	OV5645MIPI_write_cmos_sensor(0x948c, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x948d, 0x88);
	OV5645MIPI_write_cmos_sensor(0x948e, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x948f, 0x93);
	OV5645MIPI_write_cmos_sensor(0x9490, 0x22);
	OV5645MIPI_write_cmos_sensor(0x9491, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x9492, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x9493, 0x0d);
	OV5645MIPI_write_cmos_sensor(0x9494, 0x9f);
	OV5645MIPI_write_cmos_sensor(0x9495, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x9496, 0x0d);
	OV5645MIPI_write_cmos_sensor(0x9497, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x9498, 0x0c);
	OV5645MIPI_write_cmos_sensor(0x9499, 0x9e);
	OV5645MIPI_write_cmos_sensor(0x949a, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x949b, 0x0c);
	OV5645MIPI_write_cmos_sensor(0x949c, 0x22);
	OV5645MIPI_write_cmos_sensor(0x949d, 0xae);
	OV5645MIPI_write_cmos_sensor(0x949e, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x949f, 0xfb);
	OV5645MIPI_write_cmos_sensor(0x94a0, 0xee);
	OV5645MIPI_write_cmos_sensor(0x94a1, 0x34);
	OV5645MIPI_write_cmos_sensor(0x94a2, 0x60);
	OV5645MIPI_write_cmos_sensor(0x94a3, 0x8b);
	OV5645MIPI_write_cmos_sensor(0x94a4, 0x82);
	OV5645MIPI_write_cmos_sensor(0x94a5, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x94a6, 0x83);
	OV5645MIPI_write_cmos_sensor(0x94a7, 0x22);
	OV5645MIPI_write_cmos_sensor(0x94a8, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x94a9, 0xfe);
	OV5645MIPI_write_cmos_sensor(0x94aa, 0xa3);
	OV5645MIPI_write_cmos_sensor(0x94ab, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x94ac, 0xfd);
	OV5645MIPI_write_cmos_sensor(0x94ad, 0xed);
	OV5645MIPI_write_cmos_sensor(0x94ae, 0x22);
	OV5645MIPI_write_cmos_sensor(0x94af, 0xa2);
	OV5645MIPI_write_cmos_sensor(0x94b0, 0xaf);
	OV5645MIPI_write_cmos_sensor(0x94b1, 0x92);
	OV5645MIPI_write_cmos_sensor(0x94b2, 0x33);
	OV5645MIPI_write_cmos_sensor(0x94b3, 0xc2);
	OV5645MIPI_write_cmos_sensor(0x94b4, 0xaf);
	OV5645MIPI_write_cmos_sensor(0x94b5, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x94b6, 0x23);
	OV5645MIPI_write_cmos_sensor(0x94b7, 0x45);
	OV5645MIPI_write_cmos_sensor(0x94b8, 0x22);
	OV5645MIPI_write_cmos_sensor(0x94b9, 0x90);
	OV5645MIPI_write_cmos_sensor(0x94ba, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x94bb, 0x65);
	OV5645MIPI_write_cmos_sensor(0x94bc, 0x60);
	OV5645MIPI_write_cmos_sensor(0x94bd, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x94be, 0x12);
	OV5645MIPI_write_cmos_sensor(0x94bf, 0x15);
	OV5645MIPI_write_cmos_sensor(0x94c0, 0x43);
	OV5645MIPI_write_cmos_sensor(0x94c1, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x94c2, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x94c3, 0x3b);
	OV5645MIPI_write_cmos_sensor(0x94c4, 0x12);
	OV5645MIPI_write_cmos_sensor(0x94c5, 0x15);
	OV5645MIPI_write_cmos_sensor(0x94c6, 0x40);
	OV5645MIPI_write_cmos_sensor(0x94c7, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x94c8, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x94c9, 0x3c);
	OV5645MIPI_write_cmos_sensor(0x94ca, 0x80);
	OV5645MIPI_write_cmos_sensor(0x94cb, 0x0c);
	OV5645MIPI_write_cmos_sensor(0x94cc, 0x12);
	OV5645MIPI_write_cmos_sensor(0x94cd, 0x15);
	OV5645MIPI_write_cmos_sensor(0x94ce, 0x43);
	OV5645MIPI_write_cmos_sensor(0x94cf, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x94d0, 0x3f);
	OV5645MIPI_write_cmos_sensor(0x94d1, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x94d2, 0x12);
	OV5645MIPI_write_cmos_sensor(0x94d3, 0x15);
	OV5645MIPI_write_cmos_sensor(0x94d4, 0x40);
	OV5645MIPI_write_cmos_sensor(0x94d5, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x94d6, 0x40);
	OV5645MIPI_write_cmos_sensor(0x94d7, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x94d8, 0xa2);
	OV5645MIPI_write_cmos_sensor(0x94d9, 0x33);
	OV5645MIPI_write_cmos_sensor(0x94da, 0x92);
	OV5645MIPI_write_cmos_sensor(0x94db, 0xaf);
	OV5645MIPI_write_cmos_sensor(0x94dc, 0x22);
	OV5645MIPI_write_cmos_sensor(0x94dd, 0x78);
	OV5645MIPI_write_cmos_sensor(0x94de, 0xcc);
	OV5645MIPI_write_cmos_sensor(0x94df, 0x12);
	OV5645MIPI_write_cmos_sensor(0x94e0, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x94e1, 0xf7);
	OV5645MIPI_write_cmos_sensor(0x94e2, 0x40);
	OV5645MIPI_write_cmos_sensor(0x94e3, 0x0d);
	OV5645MIPI_write_cmos_sensor(0x94e4, 0x12);
	OV5645MIPI_write_cmos_sensor(0x94e5, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x94e6, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x94e7, 0x40);
	OV5645MIPI_write_cmos_sensor(0x94e8, 0x04);
	OV5645MIPI_write_cmos_sensor(0x94e9, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x94ea, 0xff);
	OV5645MIPI_write_cmos_sensor(0x94eb, 0x80);
	OV5645MIPI_write_cmos_sensor(0x94ec, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x94ed, 0x7f);
	OV5645MIPI_write_cmos_sensor(0x94ee, 0x01);
	OV5645MIPI_write_cmos_sensor(0x94ef, 0x80);
	OV5645MIPI_write_cmos_sensor(0x94f0, 0x0b);
	OV5645MIPI_write_cmos_sensor(0x94f1, 0x12);
	OV5645MIPI_write_cmos_sensor(0x94f2, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x94f3, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x94f4, 0x40);
	OV5645MIPI_write_cmos_sensor(0x94f5, 0x04);
	OV5645MIPI_write_cmos_sensor(0x94f6, 0x7f);
	OV5645MIPI_write_cmos_sensor(0x94f7, 0xff);
	OV5645MIPI_write_cmos_sensor(0x94f8, 0x80);
	OV5645MIPI_write_cmos_sensor(0x94f9, 0x02);
	OV5645MIPI_write_cmos_sensor(0x94fa, 0x7f);
	OV5645MIPI_write_cmos_sensor(0x94fb, 0xfe);
	OV5645MIPI_write_cmos_sensor(0x94fc, 0x22);
	OV5645MIPI_write_cmos_sensor(0x94fd, 0x8f);
	OV5645MIPI_write_cmos_sensor(0x94fe, 0x12);
	OV5645MIPI_write_cmos_sensor(0x94ff, 0x8e);
	OV5645MIPI_write_cmos_sensor(0x9500, 0x11);
	OV5645MIPI_write_cmos_sensor(0x9501, 0x8d);
	OV5645MIPI_write_cmos_sensor(0x9502, 0x10);
	OV5645MIPI_write_cmos_sensor(0x9503, 0x8c);
	OV5645MIPI_write_cmos_sensor(0x9504, 0x0f);
	OV5645MIPI_write_cmos_sensor(0x9505, 0x22);
	OV5645MIPI_write_cmos_sensor(0x9506, 0x12);
	OV5645MIPI_write_cmos_sensor(0x9507, 0x0c);
	OV5645MIPI_write_cmos_sensor(0x9508, 0x44);
	OV5645MIPI_write_cmos_sensor(0x9509, 0x8f);
	OV5645MIPI_write_cmos_sensor(0x950a, 0x40);
	OV5645MIPI_write_cmos_sensor(0x950b, 0x8e);
	OV5645MIPI_write_cmos_sensor(0x950c, 0x3f);
	OV5645MIPI_write_cmos_sensor(0x950d, 0x8d);
	OV5645MIPI_write_cmos_sensor(0x950e, 0x3e);
	OV5645MIPI_write_cmos_sensor(0x950f, 0x8c);
	OV5645MIPI_write_cmos_sensor(0x9510, 0x3d);
	OV5645MIPI_write_cmos_sensor(0x9511, 0x22);
	OV5645MIPI_write_cmos_sensor(0x9512, 0x93);
	OV5645MIPI_write_cmos_sensor(0x9513, 0xf9);
	OV5645MIPI_write_cmos_sensor(0x9514, 0xf8);
	OV5645MIPI_write_cmos_sensor(0x9515, 0x02);
	OV5645MIPI_write_cmos_sensor(0x9516, 0x0c);
	OV5645MIPI_write_cmos_sensor(0x9517, 0x31);
	OV5645MIPI_write_cmos_sensor(0x9518, 0xc0);
	OV5645MIPI_write_cmos_sensor(0x9519, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x951a, 0xc0);
	OV5645MIPI_write_cmos_sensor(0x951b, 0x83);
	OV5645MIPI_write_cmos_sensor(0x951c, 0xc0);
	OV5645MIPI_write_cmos_sensor(0x951d, 0x82);
	OV5645MIPI_write_cmos_sensor(0x951e, 0x90);
	OV5645MIPI_write_cmos_sensor(0x951f, 0x3f);
	OV5645MIPI_write_cmos_sensor(0x9520, 0x0d);
	OV5645MIPI_write_cmos_sensor(0x9521, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x9522, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x9523, 0x09);
	OV5645MIPI_write_cmos_sensor(0x9524, 0xe5);
	OV5645MIPI_write_cmos_sensor(0x9525, 0x09);
	OV5645MIPI_write_cmos_sensor(0x9526, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x9527, 0xd0);
	OV5645MIPI_write_cmos_sensor(0x9528, 0x82);
	OV5645MIPI_write_cmos_sensor(0x9529, 0xd0);
	OV5645MIPI_write_cmos_sensor(0x952a, 0x83);
	OV5645MIPI_write_cmos_sensor(0x952b, 0xd0);
	OV5645MIPI_write_cmos_sensor(0x952c, 0xe0);
	OV5645MIPI_write_cmos_sensor(0x952d, 0x32);
	OV5645MIPI_write_cmos_sensor(0x952e, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x952f, 0xee);
	OV5645MIPI_write_cmos_sensor(0x9530, 0x64);
	OV5645MIPI_write_cmos_sensor(0x9531, 0x80);
	OV5645MIPI_write_cmos_sensor(0x9532, 0x94);
	OV5645MIPI_write_cmos_sensor(0x9533, 0x80);
	OV5645MIPI_write_cmos_sensor(0x9534, 0x40);
	OV5645MIPI_write_cmos_sensor(0x9535, 0x02);
	OV5645MIPI_write_cmos_sensor(0x9536, 0x80);
	OV5645MIPI_write_cmos_sensor(0x9537, 0x07);
	OV5645MIPI_write_cmos_sensor(0x9538, 0xc3);
	OV5645MIPI_write_cmos_sensor(0x9539, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x953a, 0x9f);
	OV5645MIPI_write_cmos_sensor(0x953b, 0xff);
	OV5645MIPI_write_cmos_sensor(0x953c, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x953d, 0x9e);
	OV5645MIPI_write_cmos_sensor(0x953e, 0xfe);
	OV5645MIPI_write_cmos_sensor(0x953f, 0x22);
	OV5645MIPI_write_cmos_sensor(0x9540, 0x90);
	OV5645MIPI_write_cmos_sensor(0x9541, 0x0e);
	OV5645MIPI_write_cmos_sensor(0x9542, 0x67);
	OV5645MIPI_write_cmos_sensor(0x9543, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x9544, 0x93);
	OV5645MIPI_write_cmos_sensor(0x9545, 0xfe);
	OV5645MIPI_write_cmos_sensor(0x9546, 0x74);
	OV5645MIPI_write_cmos_sensor(0x9547, 0x01);
	OV5645MIPI_write_cmos_sensor(0x9548, 0x93);
	OV5645MIPI_write_cmos_sensor(0x9549, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x954a, 0x82);
	OV5645MIPI_write_cmos_sensor(0x954b, 0x8e);
	OV5645MIPI_write_cmos_sensor(0x954c, 0x83);
	OV5645MIPI_write_cmos_sensor(0x954d, 0x22);
	OV5645MIPI_write_cmos_sensor(0x954e, 0xd2);
	OV5645MIPI_write_cmos_sensor(0x954f, 0x01);
	OV5645MIPI_write_cmos_sensor(0x9550, 0xc2);
	OV5645MIPI_write_cmos_sensor(0x9551, 0x02);
	OV5645MIPI_write_cmos_sensor(0x9552, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x9553, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x9554, 0x1e);
	OV5645MIPI_write_cmos_sensor(0x9555, 0xf5);
	OV5645MIPI_write_cmos_sensor(0x9556, 0x1d);
	OV5645MIPI_write_cmos_sensor(0x9557, 0xd2);
	OV5645MIPI_write_cmos_sensor(0x9558, 0x36);
	OV5645MIPI_write_cmos_sensor(0x9559, 0xd2);
	OV5645MIPI_write_cmos_sensor(0x955a, 0x34);
	OV5645MIPI_write_cmos_sensor(0x955b, 0x22);
	OV5645MIPI_write_cmos_sensor(0x955c, 0x78);
	OV5645MIPI_write_cmos_sensor(0x955d, 0x7f);
	OV5645MIPI_write_cmos_sensor(0x955e, 0xe4);
	OV5645MIPI_write_cmos_sensor(0x955f, 0xf6);
	OV5645MIPI_write_cmos_sensor(0x9560, 0xd8);
	OV5645MIPI_write_cmos_sensor(0x9561, 0xfd);
	OV5645MIPI_write_cmos_sensor(0x9562, 0x75);
	OV5645MIPI_write_cmos_sensor(0x9563, 0x81);
	OV5645MIPI_write_cmos_sensor(0x9564, 0xd1);
	OV5645MIPI_write_cmos_sensor(0x9565, 0x02);
	OV5645MIPI_write_cmos_sensor(0x9566, 0x13);
	OV5645MIPI_write_cmos_sensor(0x9567, 0x51);
	OV5645MIPI_write_cmos_sensor(0x9568, 0x8f);
	OV5645MIPI_write_cmos_sensor(0x9569, 0x82);
	OV5645MIPI_write_cmos_sensor(0x956a, 0x8e);
	OV5645MIPI_write_cmos_sensor(0x956b, 0x83);
	OV5645MIPI_write_cmos_sensor(0x956c, 0x75);
	OV5645MIPI_write_cmos_sensor(0x956d, 0xf0);
	OV5645MIPI_write_cmos_sensor(0x956e, 0x04);
	OV5645MIPI_write_cmos_sensor(0x956f, 0xed);
	OV5645MIPI_write_cmos_sensor(0x9570, 0x02);
	OV5645MIPI_write_cmos_sensor(0x9571, 0x0c);
	OV5645MIPI_write_cmos_sensor(0x9572, 0x54);
	OV5645MIPI_write_cmos_sensor(0x3022, 0x00);
	OV5645MIPI_write_cmos_sensor(0x3023, 0x00);
	OV5645MIPI_write_cmos_sensor(0x3024, 0x00);
	OV5645MIPI_write_cmos_sensor(0x3025, 0x00);
	OV5645MIPI_write_cmos_sensor(0x3026, 0x00);
	OV5645MIPI_write_cmos_sensor(0x3027, 0x00);
	OV5645MIPI_write_cmos_sensor(0x3028, 0x00);
	OV5645MIPI_write_cmos_sensor(0x3029, 0x7F);
	OV5645MIPI_write_cmos_sensor(0x3000, 0x00);
}

/*************************************************************************
 * FUNCTION
 *   OV5640_FOCUS_OVT_AFC_Constant_Focus
 * DESCRIPTION
 *   GET af stauts
 * PARAMETERS
 *   None
 * RETURNS
 *   None
 * GLOBALS AFFECTED
 *************************************************************************/
static void OV5645_FOCUS_OVT_AFC_Constant_Focus(void)
{
	LOG_INF("enter %s function:\n ", __func__);
	OV5645MIPI_write_cmos_sensor(0x3023, 0x01);
	OV5645MIPI_write_cmos_sensor(0x3022, 0x04);
	LOG_INF("exit %s function:\n ", __func__);
}

/*************************************************************************
 * FUNCTION
 *   OV5640_FOCUS_OVT_AFC_Single_Focus
 * DESCRIPTION
 *   GET af stauts
 * PARAMETERS
 *   None
 * RETURNS
 *   None
 * GLOBALS AFFECTED
 *************************************************************************/
static void OV5645_FOCUS_OVT_AFC_Single_Focus(void)
{
	LOG_INF("enter %s function:\n ", __func__);
	OV5645MIPI_write_cmos_sensor(0x3023, 0x01);
	OV5645MIPI_write_cmos_sensor(0x3022, 0x81);
	mDELAY(50);
	OV5645MIPI_write_cmos_sensor(0x3022, 0x03);
	LOG_INF("exit %s function:\n ", __func__);
}

/*************************************************************************
 * FUNCTION
 *   OV5640_FOCUS_OVT_AFC_Pause_Focus
 * DESCRIPTION
 *   GET af stauts
 * PARAMETERS
 *   None
 * RETURNS
 *   None
 * GLOBALS AFFECTED
 *************************************************************************/

static void OV5645_FOCUS_Get_AF_Max_Num_Focus_Areas(
	UINT32 *pFeatureReturnPara32)
{
	*pFeatureReturnPara32 = 1;
	LOG_INF(" *pFeatureReturnPara32 = %d\n", *pFeatureReturnPara32);
}

static void OV5645_FOCUS_Get_AE_Max_Num_Metering_Areas(
	UINT32 *pFeatureReturnPara32)
{
	LOG_INF("enter %s function:\n ", __func__);
	*pFeatureReturnPara32 = 1;
	LOG_INF(" *pFeatureReturnPara32 = %d\n", *pFeatureReturnPara32);
	LOG_INF("exit %s function:\n ", __func__);
}

static void OV5645_FOCUS_OVT_AFC_Touch_AF(UINT32 x, UINT32 y)
{
	int x_view, y_view;
	int x_tmp, y_tmp;

	if (x < 1)
		x_view = 1;
	else if (x > 79)
		x_view = 79;
	else
		x_view = x;

	if (y < 1)
		y_view = 1;
	else if (y > 59)
		y_view = 59;
	else
		y_view = y;

	LOG_INF("AF x_view=%d,y_view=%d\n", x_view, y_view);
	OV5645MIPI_write_cmos_sensor(0x3024, x_view);
	OV5645MIPI_write_cmos_sensor(0x3025, y_view);
	x_tmp = OV5645MIPIYUV_read_cmos_sensor(0x3024);
	y_tmp = OV5645MIPIYUV_read_cmos_sensor(0x3025);
	LOG_INF("AF x_tmp1=%d,y_tmp1=%d\n", x_tmp, y_tmp);
	LOG_INF("exit %s function:\n ", __func__);
}

static void OV5645_FOCUS_Set_AF_Window(UINT32 zone_addr)
{
	UINT32 FD_XS;
	UINT32 FD_YS;
	UINT32 x0, y0, x1, y1;
	UINT32 pvx0, pvy0, pvx1, pvy1;
	UINT32 AF_pvx, AF_pvy;
	UINT32 *zone = (UINT32 *) zone_addr;

	x0 = *zone;
	y0 = *(zone + 1);
	x1 = *(zone + 2);
	y1 = *(zone + 3);
	FD_XS = *(zone + 4);
	FD_YS = *(zone + 5);

	LOG_INF("AE x0=%d,y0=%d,x1=%d,y1=%d,FD_XS=%d,FD_YS=%d\n",
			   x0, y0, x1, y1, FD_XS, FD_YS);
	mapMiddlewaresizePointToPreviewsizePoint(x0, y0, FD_XS, FD_YS,
		&pvx0, &pvy0, PRV_W, PRV_H);
	mapMiddlewaresizePointToPreviewsizePoint(x1, y1, FD_XS, FD_YS,
		&pvx1, &pvy1, PRV_W, PRV_H);
	LOG_INF("AF pvx0=%d,pvy0=%d\n", pvx0, pvy0);
	LOG_INF("AF pvx0=%d,pvy0=%d\n", pvx1, pvy1);
	AF_pvx = (pvx0 + pvx1) / 32;
	AF_pvy = (pvy0 + pvy1) / 32;
	LOG_INF("AF AF_pvx=%d,AF_pvy=%d\n", AF_pvx, AF_pvy);
	OV5645_FOCUS_OVT_AFC_Touch_AF(AF_pvx, AF_pvy);
	LOG_INF("exit %s function:\n ", __func__);
}

static void OV5645_FOCUS_Get_AF_Macro(UINT32 *pFeatureReturnPara32)
{
	*pFeatureReturnPara32 = 0;
}

static void OV5645_FOCUS_Get_AF_Inf(UINT32 *pFeatureReturnPara32)
{
	*pFeatureReturnPara32 = 0;
}

static void OV5645_FOCUS_Move_to(UINT32 a_u2MovePosition)
{				/* ??how many bits for ?? */
}

/*************************************************************************
 * FUNCTION
 *   OV5640_FOCUS_OVT_AFC_Get_AF_Status
 * DESCRIPTION
 *   GET af stauts
 * PARAMETERS
 *   None
 * RETURNS
 *   None
 * GLOBALS AFFECTED
 *************************************************************************/
static void OV5645_FOCUS_OVT_AFC_Get_AF_Status(UINT32 *pFeatureReturnPara32)
{
	UINT32 state_3028 = 0;
	UINT32 state_3029 = 0;
	*pFeatureReturnPara32 = SENSOR_AF_IDLE;
	state_3028 = OV5645MIPIYUV_read_cmos_sensor(0x3028);
	state_3029 = OV5645MIPIYUV_read_cmos_sensor(0x3029);
	mDELAY(1);
	LOG_INF("enter %s function:state_3028=%d,state_3029=%d\n", __func__,
	     state_3028, state_3029);
	if (state_3028 == 0) {
		*pFeatureReturnPara32 = SENSOR_AF_ERROR;
	} else if (state_3028 == 1) {
		switch (state_3029) {
		case 0x70:
			*pFeatureReturnPara32 = SENSOR_AF_IDLE;
			break;
		case 0x00:
			*pFeatureReturnPara32 = SENSOR_AF_FOCUSING;
			break;
		case 0x10:
			*pFeatureReturnPara32 = SENSOR_AF_FOCUSED;
			break;
		case 0x20:
			*pFeatureReturnPara32 = SENSOR_AF_FOCUSED;
			break;
		default:
			*pFeatureReturnPara32 = SENSOR_AF_SCENE_DETECTING;
			break;
		}
	}
	LOG_INF("exit %s function:state_3028=%d,state_3029=%d\n", __func__,
	     state_3028, state_3029);
}

/*************************************************************************
 * FUNCTION
 *   OV5640_FOCUS_OVT_AFC_Pause_Focus
 * DESCRIPTION
 *   GET af stauts
 * PARAMETERS
 *   None
 * RETURNS
 *   None
 * GLOBALS AFFECTED
 *************************************************************************/
static void OV5645_FOCUS_OVT_AFC_Cancel_Focus(void)
{
	OV5645MIPI_write_cmos_sensor(0x3023, 0x01);
	OV5645MIPI_write_cmos_sensor(0x3022, 0x08);
}



/*************************************************************************
 * FUNCTION
 *   OV5645WBcalibattion
 * DESCRIPTION
 *   color calibration
 * PARAMETERS
 *   None
 * RETURNS
 *   None
 * GLOBALS AFFECTED
 *************************************************************************/
static void OV5645WBcalibattion(kal_uint32 color_r_gain,
	kal_uint32 color_b_gain)
{
	kal_uint32 color_r_gain_w = 0;
	kal_uint32 color_b_gain_w = 0;

	kal_uint8 temp = OV5645MIPIYUV_read_cmos_sensor(0x350b);

	if (temp >= 0xb0) {
		color_r_gain_w = color_r_gain * 98 / 100;
		color_b_gain_w = color_b_gain * 99 / 100;
	} else if (temp >= 0x70) {
		color_r_gain_w = color_r_gain * 99 / 100;
		color_b_gain_w = color_b_gain * 100 / 100;
	} else if (temp >= 0x30) {
		color_r_gain_w = color_r_gain;
		color_b_gain_w = color_b_gain;
	} else {
		if (color_b_gain > 0x730) {
			color_r_gain_w = color_r_gain;
			color_b_gain_w = color_b_gain * 99 / 100;
		} else {
			color_r_gain_w = color_r_gain;
			color_b_gain_w = color_b_gain;
		}
	}
	OV5645MIPI_write_cmos_sensor(0x3400, (color_r_gain_w & 0xff00) >> 8);
	OV5645MIPI_write_cmos_sensor(0x3401, color_r_gain_w & 0xff);
	OV5645MIPI_write_cmos_sensor(0x3404, (color_b_gain_w & 0xff00) >> 8);
	OV5645MIPI_write_cmos_sensor(0x3405, color_b_gain_w & 0xff);
	LOG_INF("exit %s function:\n ", __func__);
}

/*************************************************************************
 * FUNCTION
 *	OV5645MIPIOpen
 *
 * DESCRIPTION
 *	This function initialize the registers of CMOS sensor
 *
 * PARAMETERS
 *	None
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
UINT32 OV5645MIPIOpen(void)
{
	signed int i;
	kal_uint16 sensor_id = 0;

	LOG_INF("enter %s function:\n ", __func__);
	OV5645MIPI_write_cmos_sensor(0x3103, 0x11);
	OV5645MIPI_write_cmos_sensor(0x3008, 0x82);
	mDELAY(10);
	for (i = 0; i < 3; i++) {
		sensor_id =
		    (OV5645MIPIYUV_read_cmos_sensor(0x300A) << 8) |
		    OV5645MIPIYUV_read_cmos_sensor(0x300B);
		if (sensor_id != OV5645MIPI_SENSOR_ID) {
			LOG_INF("Read Sensor ID Fail[open] = 0x%x\n",
					   sensor_id);
			return ERROR_SENSOR_CONNECT_FAIL;
		}
	}
	LOG_INF("Sensor Read ID OK\n");
	OV5645MIPIinitalvariable();
	OV5645MIPIInitialSetting();
	OV5645_FOCUS_OVT_AFC_Init();
	LOG_INF("exit %s function:\n ", __func__);
	return ERROR_NONE;
}				/* OV5645MIPIOpen() */

/*************************************************************************
 * FUNCTION
 *	OV5645MIPIClose
 *
 * DESCRIPTION
 *	This function is to turn off sensor module power.
 *
 * PARAMETERS
 *	None
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
UINT32 OV5645MIPIClose(void)
{
	/* CISModulePowerOn(FALSE); */
	return ERROR_NONE;
}				/* OV5645MIPIClose() */

/*************************************************************************
 * FUNCTION
 *	OV5645MIPIPreview
 *
 * DESCRIPTION
 *	This function start the sensor preview.
 *
 * PARAMETERS
 *	*image_window : address pointer of pixel numbers in one period of HSYNC
 *  *sensor_config_data : address pointer of line numbers
 *	in one period of VSYNC
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
UINT32 OV5645MIPIPreview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			 MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	// kal_uint8 iTemp, temp_AE_reg, temp_AWB_reg;
	// kal_uint16 iDummyPixels = 0, iDummyLines = 0,
	// iStartX = 1, iStartY = 1;
	LOG_INF("enter %s function:\n ", __func__);
	switch (CurrentScenarioId) {
	case MSDK_SCENARIO_ID_CAMERA_ZSD:
		OV5645MIPIFullSizeCaptureSetting();
		break;
	default:
		OV5645MIPIPreviewSetting_SVGA();
		break;
	}
	OV5645MIPI_set_AE_mode(KAL_TRUE);
	OV5645MIPI_set_AWB_mode(KAL_TRUE);
	mDELAY(150);
	OV5645MIPI_night_mode(OV5645MIPISensor.NightMode);
	OV5645MIPISetHVMirror(sensor_config_data->SensorImageMirror,
		SENSOR_MODE_PREVIEW);
	OV5645_FOCUS_OVT_AFC_Constant_Focus();
	LOG_INF("----Aaron------read register 0x3501=[%x]\n",
			   OV5645MIPIYUV_read_cmos_sensor(0x3501));
	LOG_INF("----Aaron------read register 0x3502=[%x]\n",
			   OV5645MIPIYUV_read_cmos_sensor(0x3502));
	LOG_INF("----Aaron------read register 0x350c=[%x]\n",
			   OV5645MIPIYUV_read_cmos_sensor(0x350c));
	LOG_INF("----Aaron------read register 0x350d=[%x]\n",
			   OV5645MIPIYUV_read_cmos_sensor(0x350d));
	LOG_INF("----Aaron------read register 0x350b=[%x]\n",
			   OV5645MIPIYUV_read_cmos_sensor(0x350b));
	LOG_INF("exit %s function:\n ", __func__);
	return ERROR_NONE;

}				/* OV5645MIPIPreview() */

UINT32 OV5645MIPICapture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			 MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	kal_uint32 shutter = 0;
	kal_uint32 extshutter = 0;
	kal_uint32 color_r_gain = 0;
	kal_uint32 color_b_gain = 0;
	kal_uint32 readgain = 0;

	LOG_INF("enter %s function:\n ", __func__);
	if (OV5645MIPISensor.SensorMode == SENSOR_MODE_PREVIEW) {
		shutter = OV5645MIPIReadShutter();
		extshutter = OV5645MIPIReadExtraShutter();
		readgain = OV5645MIPIReadSensorGain();
		spin_lock(&ov5645mipi_drv_lock);
		OV5645MIPISensor.PreviewShutter = shutter;
		OV5645MIPISensor.PreviewExtraShutter = extshutter;
		OV5645MIPISensor.SensorGain = readgain;
		spin_unlock(&ov5645mipi_drv_lock);
		/* OV5640_FOCUS_OVT_AFC_Pause_Focus(); */
		OV5645MIPI_set_AE_mode(KAL_FALSE);
		/* OV5645MIPI_set_AWB_mode(KAL_FALSE); */
		color_r_gain =
		    ((OV5645MIPIYUV_read_cmos_sensor(0x3401) & 0xFF) +
		     ((OV5645MIPIYUV_read_cmos_sensor(0x3400) & 0xFF) * 256));
		color_b_gain =
		    ((OV5645MIPIYUV_read_cmos_sensor(0x3405) & 0xFF) +
		     ((OV5645MIPIYUV_read_cmos_sensor(0x3404) & 0xFF) * 256));
		OV5645MIPIFullSizeCaptureSetting();
		OV5645WBcalibattion(color_r_gain, color_b_gain);
		shutter = shutter * 2;
		OV5645MIPISetHVMirror(sensor_config_data->SensorImageMirror,
			SENSOR_MODE_CAPTURE);
		/* OV5645MIPIWriteSensorGain(OV5645MIPISensor.SensorGain); */
		OV5645MIPIWriteShutter(shutter);
		mDELAY(200);
	}
	LOG_INF("exit %s function:\n ", __func__);
	return ERROR_NONE;
}				/* OV5645MIPICapture() */

UINT32 OV5645MIPIGetResolution(
	MSDK_SENSOR_RESOLUTION_INFO_STRUCT * pSensorResolution)
{
	LOG_INF("enter %s function:\n ", __func__);
	pSensorResolution->SensorPreviewWidth =
				OV5645MIPI_IMAGE_SENSOR_SVGA_WIDTH - 2;
	pSensorResolution->SensorPreviewHeight =
				OV5645MIPI_IMAGE_SENSOR_SVGA_HEIGHT - 2;
	pSensorResolution->SensorFullWidth =
				OV5645MIPI_IMAGE_SENSOR_QSXGA_WITDH - 2;
	pSensorResolution->SensorFullHeight =
				OV5645MIPI_IMAGE_SENSOR_QSXGA_HEIGHT - 2;
	pSensorResolution->SensorVideoWidth =
				OV5645MIPI_IMAGE_SENSOR_SVGA_WIDTH - 2;
	pSensorResolution->SensorVideoHeight =
				OV5645MIPI_IMAGE_SENSOR_SVGA_HEIGHT - 2;
	LOG_INF("exit %s function:\n ", __func__);
	return ERROR_NONE;
}				/* OV5645MIPIGetResolution() */

UINT32 OV5645MIPIGetInfo(MSDK_SCENARIO_ID_ENUM ScenarioId,
			MSDK_SENSOR_INFO_STRUCT *pSensorInfo,
			 MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{
	LOG_INF("enter %s function:\n ", __func__);
	switch (ScenarioId) {
	case MSDK_SCENARIO_ID_CAMERA_ZSD:
		pSensorInfo->SensorPreviewResolutionX =
				OV5645MIPI_IMAGE_SENSOR_QSXGA_WITDH;
		pSensorInfo->SensorPreviewResolutionY =
				OV5645MIPI_IMAGE_SENSOR_QSXGA_HEIGHT;
		break;
	default:
		pSensorInfo->SensorPreviewResolutionX =
				OV5645MIPI_IMAGE_SENSOR_SVGA_WIDTH;
		pSensorInfo->SensorPreviewResolutionY =
				OV5645MIPI_IMAGE_SENSOR_SVGA_HEIGHT;
		break;
	}
	pSensorInfo->SensorFullResolutionX =
			OV5645MIPI_IMAGE_SENSOR_QSXGA_WITDH;
	pSensorInfo->SensorFullResolutionY =
			OV5645MIPI_IMAGE_SENSOR_QSXGA_HEIGHT;
	pSensorInfo->SensorCameraPreviewFrameRate = 30;
	pSensorInfo->SensorVideoFrameRate = 30;
	pSensorInfo->SensorStillCaptureFrameRate = 10;
	pSensorInfo->SensorWebCamCaptureFrameRate = 10;
	pSensorInfo->SensorResetActiveHigh = FALSE;
	pSensorInfo->SensorResetDelayCount = 5;
	/* YVYU;//SENSOR_OUTPUT_FORMAT_UYVY; */
	pSensorInfo->SensorOutputDataFormat =
			SENSOR_OUTPUT_FORMAT_YUYV;
	pSensorInfo->SensorClockPolarity =
			SENSOR_CLOCK_POLARITY_LOW;
	pSensorInfo->SensorClockFallingPolarity =
			SENSOR_CLOCK_POLARITY_LOW;
	pSensorInfo->SensorHsyncPolarity =
			SENSOR_CLOCK_POLARITY_LOW;
	pSensorInfo->SensorVsyncPolarity =
			SENSOR_CLOCK_POLARITY_LOW;
	pSensorInfo->SensorInterruptDelayLines = 1;
	pSensorInfo->SensroInterfaceType =
			SENSOR_INTERFACE_TYPE_MIPI;
	pSensorInfo->SettleDelayMode = 1;
	pSensorInfo->CaptureDelayFrame = 2;
	pSensorInfo->PreviewDelayFrame = 2;
	pSensorInfo->VideoDelayFrame = 5;
	pSensorInfo->SensorMasterClockSwitch = 0;
	pSensorInfo->YUVAwbDelayFrame = 2;
	pSensorInfo->YUVEffectDelayFrame = 2;
	pSensorInfo->SensorDrivingCurrent =
			ISP_DRIVING_8MA;
	switch (ScenarioId) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		pSensorInfo->SensorClockFreq = 24;
		pSensorInfo->SensorClockDividCount = 3;
		pSensorInfo->SensorClockRisingCount = 0;
		pSensorInfo->SensorClockFallingCount = 2;
		pSensorInfo->SensorPixelClockCount = 3;
		pSensorInfo->SensorDataLatchCount = 2;
		pSensorInfo->SensorGrabStartX = OV5645MIPI_PV_GRAB_START_X;
		pSensorInfo->SensorGrabStartY = OV5645MIPI_PV_GRAB_START_Y;
		pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_2_LANE;
		pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
		pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			SETTLE_DELAY;
		pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
		pSensorInfo->SensorWidthSampling = 0;
		pSensorInfo->SensorHightSampling = 0;
		pSensorInfo->SensorPacketECCOrder = 1;
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
	case MSDK_SCENARIO_ID_CAMERA_ZSD:

		pSensorInfo->SensorClockFreq = 24;
		pSensorInfo->SensorClockDividCount = 3;
		pSensorInfo->SensorClockRisingCount = 0;
		pSensorInfo->SensorClockFallingCount = 2;
		pSensorInfo->SensorPixelClockCount = 3;
		pSensorInfo->SensorDataLatchCount = 2;
		pSensorInfo->SensorGrabStartX = OV5645MIPI_FULL_GRAB_START_X;
		pSensorInfo->SensorGrabStartY = OV5645MIPI_FULL_GRAB_START_Y;
		pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_2_LANE;
		pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
		pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			SETTLE_DELAY;
		pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
		pSensorInfo->SensorWidthSampling = 0;
		pSensorInfo->SensorHightSampling = 0;
		pSensorInfo->SensorPacketECCOrder = 1;
		break;
	default:
		pSensorInfo->SensorClockFreq = 24;
		pSensorInfo->SensorClockDividCount = 3;
		pSensorInfo->SensorClockRisingCount = 0;
		pSensorInfo->SensorClockFallingCount = 2;
		pSensorInfo->SensorPixelClockCount = 3;
		pSensorInfo->SensorDataLatchCount = 2;
		pSensorInfo->SensorGrabStartX = OV5645MIPI_PV_GRAB_START_X;
		pSensorInfo->SensorGrabStartY = OV5645MIPI_PV_GRAB_START_Y;
		pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_2_LANE;
		pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
		pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			SETTLE_DELAY;
		pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
		pSensorInfo->SensorWidthSampling = 0;
		pSensorInfo->SensorHightSampling = 0;
		pSensorInfo->SensorPacketECCOrder = 1;
		break;
	}
	memcpy(pSensorConfigData, &OV5645MIPISensorConfigData,
		sizeof(MSDK_SENSOR_CONFIG_STRUCT));
	LOG_INF("exit %s function:\n ", __func__);
	return ERROR_NONE;
}				/* OV5645MIPIGetInfo() */

UINT32 OV5645MIPIControl(MSDK_SCENARIO_ID_ENUM ScenarioId,
			 MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow,
			 MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{
	LOG_INF("enter %s function:\n ", __func__);
	spin_lock(&ov5645mipi_drv_lock);
	CurrentScenarioId = ScenarioId;
	spin_unlock(&ov5645mipi_drv_lock);
	switch (ScenarioId) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
	case MSDK_SCENARIO_ID_CAMERA_ZSD:
		OV5645MIPIPreview(pImageWindow, pSensorConfigData);
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		OV5645MIPICapture(pImageWindow, pSensorConfigData);
		break;
	default:
		return ERROR_INVALID_SCENARIO_ID;
	}
	LOG_INF("exit %s function:\n ", __func__);
	return ERROR_NONE;
}				/* OV5645MIPIControl() */

/* [TC] YUV sensor */

BOOL OV5645MIPI_set_param_wb(UINT16 para)
{
	LOG_INF("enter %s function:\n ", __func__);
	/* spin_lock(&ov5645mipi_drv_lock); */
	/* OV5645MIPISensor.awbMode = para; */
	/* spin_unlock(&ov5645mipi_drv_lock); */
	switch (para) {
	case AWB_MODE_OFF:
		spin_lock(&ov5645mipi_drv_lock);
		OV5645MIPI_AWB_ENABLE = KAL_FALSE;
		spin_unlock(&ov5645mipi_drv_lock);
		OV5645MIPI_set_AWB_mode(OV5645MIPI_AWB_ENABLE);
		break;
	case AWB_MODE_AUTO:
		spin_lock(&ov5645mipi_drv_lock);
		OV5645MIPI_AWB_ENABLE = KAL_TRUE;
		spin_unlock(&ov5645mipi_drv_lock);
		OV5645MIPI_set_AWB_mode(OV5645MIPI_AWB_ENABLE);
		break;
	case AWB_MODE_CLOUDY_DAYLIGHT:	/* cloudy */
		OV5645MIPI_write_cmos_sensor(0x3212, 0x03);
		OV5645MIPI_set_AWB_mode(KAL_FALSE);
		OV5645MIPI_write_cmos_sensor(0x3400, 0x06);
		OV5645MIPI_write_cmos_sensor(0x3401, 0x48);
		OV5645MIPI_write_cmos_sensor(0x3402, 0x04);
		OV5645MIPI_write_cmos_sensor(0x3403, 0x00);
		OV5645MIPI_write_cmos_sensor(0x3404, 0x04);
		OV5645MIPI_write_cmos_sensor(0x3405, 0xd3);
		OV5645MIPI_write_cmos_sensor(0x3212, 0x13);
		OV5645MIPI_write_cmos_sensor(0x3212, 0xa3);
		break;
	case AWB_MODE_DAYLIGHT:	/* sunny */
		OV5645MIPI_write_cmos_sensor(0x3212, 0x03);
		OV5645MIPI_set_AWB_mode(KAL_FALSE);
		OV5645MIPI_write_cmos_sensor(0x3400, 0x06);
		OV5645MIPI_write_cmos_sensor(0x3401, 0x10);
		OV5645MIPI_write_cmos_sensor(0x3402, 0x04);
		OV5645MIPI_write_cmos_sensor(0x3403, 0x00);
		OV5645MIPI_write_cmos_sensor(0x3404, 0x04);
		OV5645MIPI_write_cmos_sensor(0x3405, 0xf3);
		OV5645MIPI_write_cmos_sensor(0x3212, 0x13);
		OV5645MIPI_write_cmos_sensor(0x3212, 0xa3);
		break;
	case AWB_MODE_INCANDESCENT:	/* office */
		OV5645MIPI_write_cmos_sensor(0x3212, 0x03);
		OV5645MIPI_set_AWB_mode(KAL_FALSE);
		OV5645MIPI_write_cmos_sensor(0x3400, 0x04);
		OV5645MIPI_write_cmos_sensor(0x3401, 0xe0);
		OV5645MIPI_write_cmos_sensor(0x3402, 0x04);
		OV5645MIPI_write_cmos_sensor(0x3403, 0x00);
		OV5645MIPI_write_cmos_sensor(0x3404, 0x05);
		OV5645MIPI_write_cmos_sensor(0x3405, 0xa0);
		OV5645MIPI_write_cmos_sensor(0x3212, 0x13);
		OV5645MIPI_write_cmos_sensor(0x3212, 0xa3);
		break;
	case AWB_MODE_TUNGSTEN:
		OV5645MIPI_write_cmos_sensor(0x3212, 0x03);
		OV5645MIPI_set_AWB_mode(KAL_FALSE);
		OV5645MIPI_write_cmos_sensor(0x3400, 0x05);
		OV5645MIPI_write_cmos_sensor(0x3401, 0x48);
		OV5645MIPI_write_cmos_sensor(0x3402, 0x04);
		OV5645MIPI_write_cmos_sensor(0x3403, 0x00);
		OV5645MIPI_write_cmos_sensor(0x3404, 0x05);
		OV5645MIPI_write_cmos_sensor(0x3405, 0xe0);
		OV5645MIPI_write_cmos_sensor(0x3212, 0x13);
		OV5645MIPI_write_cmos_sensor(0x3212, 0xa3);
		break;
	case AWB_MODE_FLUORESCENT:
		OV5645MIPI_write_cmos_sensor(0x3212, 0x03);
		OV5645MIPI_set_AWB_mode(KAL_FALSE);
		OV5645MIPI_write_cmos_sensor(0x3400, 0x04);
		OV5645MIPI_write_cmos_sensor(0x3401, 0x00);
		OV5645MIPI_write_cmos_sensor(0x3402, 0x04);
		OV5645MIPI_write_cmos_sensor(0x3403, 0x00);
		OV5645MIPI_write_cmos_sensor(0x3404, 0x06);
		OV5645MIPI_write_cmos_sensor(0x3405, 0x50);
		OV5645MIPI_write_cmos_sensor(0x3212, 0x13);
		OV5645MIPI_write_cmos_sensor(0x3212, 0xa3);
		break;
	default:
		return FALSE;
	}

	spin_lock(&ov5645mipi_drv_lock);
	OV5645CurrentStatus.iWB = para;
	spin_unlock(&ov5645mipi_drv_lock);
	LOG_INF(" %s %d\n", __func__, para);

	LOG_INF("exit %s function:\n", __func__);
	return TRUE;
}				/* OV5645MIPI_set_param_wb */

BOOL OV5645MIPI_set_param_effect(UINT16 para)
{
	LOG_INF("enter %s function:\n", __func__);
	switch (para) {
	case MEFFECT_OFF:
		OV5645MIPI_write_cmos_sensor(0x3212, 0x03);
		OV5645MIPI_write_cmos_sensor(0x5580, 0x06);
		OV5645MIPI_write_cmos_sensor(0x5583, 0x40);
		OV5645MIPI_write_cmos_sensor(0x5584, 0x38);
		OV5645MIPI_write_cmos_sensor(0x3212, 0x13);
		OV5645MIPI_write_cmos_sensor(0x3212, 0xa3);
		break;
	case MEFFECT_SEPIA:
		OV5645MIPI_write_cmos_sensor(0x3212, 0x03);
		OV5645MIPI_write_cmos_sensor(0x5580, 0x1e);
		OV5645MIPI_write_cmos_sensor(0x5583, 0x40);
		OV5645MIPI_write_cmos_sensor(0x5584, 0xa0);
		OV5645MIPI_write_cmos_sensor(0x3212, 0x13);
		OV5645MIPI_write_cmos_sensor(0x3212, 0xa3);
		break;
	case MEFFECT_NEGATIVE:
		OV5645MIPI_write_cmos_sensor(0x3212, 0x03);
		OV5645MIPI_write_cmos_sensor(0x5580, 0x46);
		OV5645MIPI_write_cmos_sensor(0x5583, 0x40);
		OV5645MIPI_write_cmos_sensor(0x5584, 0x28);
		OV5645MIPI_write_cmos_sensor(0x3212, 0x13);
		OV5645MIPI_write_cmos_sensor(0x3212, 0xa3);
		break;
	case MEFFECT_SEPIAGREEN:
		OV5645MIPI_write_cmos_sensor(0x3212, 0x03);
		OV5645MIPI_write_cmos_sensor(0x5580, 0x1e);
		OV5645MIPI_write_cmos_sensor(0x5583, 0x60);
		OV5645MIPI_write_cmos_sensor(0x5584, 0x60);
		OV5645MIPI_write_cmos_sensor(0x3212, 0x13);
		OV5645MIPI_write_cmos_sensor(0x3212, 0xa3);
		break;
	case MEFFECT_SEPIABLUE:
		OV5645MIPI_write_cmos_sensor(0x3212, 0x03);
		OV5645MIPI_write_cmos_sensor(0x5580, 0x1e);
		OV5645MIPI_write_cmos_sensor(0x5583, 0xa0);
		OV5645MIPI_write_cmos_sensor(0x5584, 0x40);
		OV5645MIPI_write_cmos_sensor(0x3212, 0x13);
		OV5645MIPI_write_cmos_sensor(0x3212, 0xa3);
		break;
	case MEFFECT_MONO:	/* B&W */
		OV5645MIPI_write_cmos_sensor(0x3212, 0x03);
		OV5645MIPI_write_cmos_sensor(0x5580, 0x1e);
		OV5645MIPI_write_cmos_sensor(0x5583, 0x80);
		OV5645MIPI_write_cmos_sensor(0x5584, 0x80);
		OV5645MIPI_write_cmos_sensor(0x3212, 0x13);
		OV5645MIPI_write_cmos_sensor(0x3212, 0xa3);
		break;
	default:
		return KAL_FALSE;
	}
	LOG_INF("exit %s function:\n ", __func__);
	mDELAY(100);
	return KAL_FALSE;
}				/* OV5645MIPI_set_param_effect */

BOOL OV5645MIPI_set_param_banding(UINT16 para)
{
	LOG_INF("enter %s function:\n", __func__);
	switch (para) {
	case AE_FLICKER_MODE_50HZ:
		spin_lock(&ov5645mipi_drv_lock);
		OV5645MIPI_Banding_setting = AE_FLICKER_MODE_50HZ;
		spin_unlock(&ov5645mipi_drv_lock);
		OV5645MIPI_write_cmos_sensor(0x3c00, 0x04);
		OV5645MIPI_write_cmos_sensor(0x3c01, 0x80);
		break;
	case AE_FLICKER_MODE_60HZ:
		spin_lock(&ov5645mipi_drv_lock);
		OV5645MIPI_Banding_setting = AE_FLICKER_MODE_60HZ;
		spin_unlock(&ov5645mipi_drv_lock);
		OV5645MIPI_write_cmos_sensor(0x3c00, 0x00);
		OV5645MIPI_write_cmos_sensor(0x3c01, 0x80);
		break;
	default:
		return FALSE;
	}
	LOG_INF("exit %s function:\n", __func__);
	return TRUE;
}				/* OV5645MIPI_set_param_banding */

BOOL OV5645MIPI_set_param_exposure(UINT16 para)
{
	LOG_INF("enter %s function:\n", __func__);
	switch (para) {
	case AE_EV_COMP_30:
		OV5645MIPI_write_cmos_sensor(0x3212, 0x03);
		OV5645MIPI_write_cmos_sensor(0x5587, 0x50);
		OV5645MIPI_write_cmos_sensor(0x5588, 0x01);
		/* OV5645MIPI_write_cmos_sensor(0x5580, 0x80); */
		OV5645MIPI_write_cmos_sensor(0x3212, 0x13);
		OV5645MIPI_write_cmos_sensor(0x3212, 0xa3);
		break;
	case AE_EV_COMP_20:
		OV5645MIPI_write_cmos_sensor(0x3212, 0x03);
		OV5645MIPI_write_cmos_sensor(0x5587, 0x30);
		OV5645MIPI_write_cmos_sensor(0x5588, 0x01);
		/* OV5645MIPI_write_cmos_sensor(0x5580, 0x80); */
		OV5645MIPI_write_cmos_sensor(0x3212, 0x13);
		OV5645MIPI_write_cmos_sensor(0x3212, 0xa3);
		break;
	case AE_EV_COMP_10:
		OV5645MIPI_write_cmos_sensor(0x3212, 0x03);
		OV5645MIPI_write_cmos_sensor(0x5587, 0x10);
		OV5645MIPI_write_cmos_sensor(0x5588, 0x01);
		/* OV5645MIPI_write_cmos_sensor(0x5580, 0x80); */
		OV5645MIPI_write_cmos_sensor(0x3212, 0x13);
		OV5645MIPI_write_cmos_sensor(0x3212, 0xa3);
		break;
	case AE_EV_COMP_00:
		OV5645MIPI_write_cmos_sensor(0x3212, 0x03);
		OV5645MIPI_write_cmos_sensor(0x5587, 0x00);
		OV5645MIPI_write_cmos_sensor(0x5588, 0x01);
		/* OV5645MIPI_write_cmos_sensor(0x5580, 0x80); */
		OV5645MIPI_write_cmos_sensor(0x3212, 0x13);
		OV5645MIPI_write_cmos_sensor(0x3212, 0xa3);
		break;
	case AE_EV_COMP_n10:
		OV5645MIPI_write_cmos_sensor(0x3212, 0x03);
		OV5645MIPI_write_cmos_sensor(0x5587, 0x10);
		OV5645MIPI_write_cmos_sensor(0x5588, 0x09);
		/* OV5645MIPI_write_cmos_sensor(0x5580, 0x80); */
		OV5645MIPI_write_cmos_sensor(0x3212, 0x13);
		OV5645MIPI_write_cmos_sensor(0x3212, 0xa3);
		break;
	case AE_EV_COMP_n20:
		OV5645MIPI_write_cmos_sensor(0x3212, 0x03);
		OV5645MIPI_write_cmos_sensor(0x5587, 0x30);
		OV5645MIPI_write_cmos_sensor(0x5588, 0x09);
		/* OV5645MIPI_write_cmos_sensor(0x5580, 0x80); */
		OV5645MIPI_write_cmos_sensor(0x3212, 0x13);
		OV5645MIPI_write_cmos_sensor(0x3212, 0xa3);
		break;
	case AE_EV_COMP_n30:
		OV5645MIPI_write_cmos_sensor(0x3212, 0x03);
		OV5645MIPI_write_cmos_sensor(0x5587, 0x50);
		OV5645MIPI_write_cmos_sensor(0x5588, 0x09);
		/* OV5645MIPI_write_cmos_sensor(0x5580, 0x80); */
		OV5645MIPI_write_cmos_sensor(0x3212, 0x13);
		OV5645MIPI_write_cmos_sensor(0x3212, 0xa3);
		break;
	default:
		return FALSE;
	}
	LOG_INF("exit %s function:\n", __func__);
	return TRUE;
}				/* OV5645MIPI_set_param_exposure */

#if 0				/* afc */
BOOL OV5645MIPI_set_param_afmode(UINT16 para)
{
	switch (para) {
	case AF_MODE_AFS:
		OV5640_FOCUS_OVT_AFC_Single_Focus();
		break;
	case AF_MODE_AFC:
		OV5640_FOCUS_OVT_AFC_Constant_Focus();
		break;
	default:
		return FALSE;
	}
	return TRUE;
}				/* OV5645MIPI_set_param_banding */
#endif
UINT32 OV5645MIPIYUVSensorSetting(FEATURE_ID iCmd, UINT32 iPara)
{
	LOG_INF("enter %s function:\n", __func__);
	switch (iCmd) {
	case FID_SCENE_MODE:
		LOG_INF("Night Mode:%d\n", iPara);
		if (iPara == SCENE_MODE_OFF)
			OV5645MIPI_night_mode(KAL_FALSE);
		else if (iPara == SCENE_MODE_NIGHTSCENE)
			OV5645MIPI_night_mode(KAL_TRUE);
		break;
	case FID_AWB_MODE:
		OV5645MIPI_set_param_wb(iPara);
		break;
	case FID_COLOR_EFFECT:
		OV5645MIPI_set_param_effect(iPara);
		break;
	case FID_AE_EV:
		OV5645MIPI_set_param_exposure(iPara);
		break;
	case FID_AE_FLICKER:
		OV5645MIPI_set_param_banding(iPara);
		break;
	case FID_AE_SCENE_MODE:
		if (iPara == AE_MODE_OFF) {
			spin_lock(&ov5645mipi_drv_lock);
			OV5645MIPI_AE_ENABLE = KAL_FALSE;
			spin_unlock(&ov5645mipi_drv_lock);
		} else {
			spin_lock(&ov5645mipi_drv_lock);
			OV5645MIPI_AE_ENABLE = KAL_TRUE;
			spin_unlock(&ov5645mipi_drv_lock);
		}
		OV5645MIPI_set_AE_mode(OV5645MIPI_AE_ENABLE);
		break;
	case FID_ZOOM_FACTOR:
		LOG_INF("FID_ZOOM_FACTOR:%d\n", iPara);
		spin_lock(&ov5645mipi_drv_lock);
		zoom_factor = iPara;
		spin_unlock(&ov5645mipi_drv_lock);
		break;
	default:
		break;
	}
	LOG_INF("exit %s function:\n ", __func__);
	return TRUE;
}				/* OV5645MIPIYUVSensorSetting */

UINT32 OV5645MIPIYUVSetVideoMode(UINT16 u2FrameRate)
{
	LOG_INF("enter %s function:\n ", __func__);
	if (u2FrameRate == 30) {
		/* ;OV5645MIPI 1280x960,30fps */
		/* 56Mhz, 224Mbps/Lane, 2Lane. */
		LOG_INF("%s enter u2FrameRate == 30 setting  :\n ", __func__);
		/* fix30 */
		OV5645MIPI_write_cmos_sensor(0x3A00, 0x38);
		/* ; MIPI 2 lane */
		OV5645MIPI_write_cmos_sensor(0x300e, 0x45);
		/* PLL, MIPI 8-bit mode */
		OV5645MIPI_write_cmos_sensor(0x3034, 0x18);
		/* PLL */
		OV5645MIPI_write_cmos_sensor(0x3035, 0x21);
		/* PLL */
		OV5645MIPI_write_cmos_sensor(0x3036, 0x70);
		/* PLL */
		OV5645MIPI_write_cmos_sensor(0x3037, 0x13);
		/* PLL */
		OV5645MIPI_write_cmos_sensor(0x3108, 0x01);
		/* PLL */
		OV5645MIPI_write_cmos_sensor(0x3824, 0x01);
		/* PLL */
		OV5645MIPI_write_cmos_sensor(0x460c, 0x20);
		OV5645MIPI_write_cmos_sensor(0x3618, 0x00);	/*  */
		OV5645MIPI_write_cmos_sensor(0x3600, 0x09);	/*  */
		OV5645MIPI_write_cmos_sensor(0x3601, 0x43);	/*  */
		OV5645MIPI_write_cmos_sensor(0x3708, 0x66);	/*  */
		OV5645MIPI_write_cmos_sensor(0x3709, 0x12);	/*  */
		OV5645MIPI_write_cmos_sensor(0x370c, 0xc3);	/*  */
		/* HS = 0 */
		OV5645MIPI_write_cmos_sensor(0x3800, 0x00);
		/* HS */
		OV5645MIPI_write_cmos_sensor(0x3801, 0x00);
		/* VS = 250 */
		OV5645MIPI_write_cmos_sensor(0x3802, 0x00);
		/* VS */
		OV5645MIPI_write_cmos_sensor(0x3803, 0x06);
		/* HW = 2623 */
		OV5645MIPI_write_cmos_sensor(0x3804, 0x0a);
		/* ; HW */
		OV5645MIPI_write_cmos_sensor(0x3805, 0x3f);
		/* ; VH = */
		OV5645MIPI_write_cmos_sensor(0x3806, 0x07);
		/* ; VH */
		OV5645MIPI_write_cmos_sensor(0x3807, 0x9d);
		/* ; DVPHO = 1280 */
		OV5645MIPI_write_cmos_sensor(0x3808, 0x05);
		/* ; DVPHO */
		OV5645MIPI_write_cmos_sensor(0x3809, 0x00);
		/* ; DVPVO = 960 */
		OV5645MIPI_write_cmos_sensor(0x380a, 0x03);
		/* ; DVPVO */
		OV5645MIPI_write_cmos_sensor(0x380b, 0xc0);
		/* ; HTS = 2160 */
		OV5645MIPI_write_cmos_sensor(0x380c, 0x07);
		/* ; HTS */
		OV5645MIPI_write_cmos_sensor(0x380d, 0x68);
		/* ; VTS = 740 */
		OV5645MIPI_write_cmos_sensor(0x380e, 0x03);
		/* ; VTS */
		OV5645MIPI_write_cmos_sensor(0x380f, 0xd8);
		/* H OFF = 16 */
		OV5645MIPI_write_cmos_sensor(0x3810, 0x00);
		/* H OFF */
		OV5645MIPI_write_cmos_sensor(0x3811, 0x10);
		/* V OFF = 4 */
		OV5645MIPI_write_cmos_sensor(0x3812, 0x00);
		/* ; V OFF */
		OV5645MIPI_write_cmos_sensor(0x3813, 0x06);
		/* ; X INC */
		OV5645MIPI_write_cmos_sensor(0x3814, 0x31);
		/* ; Y INC */
		OV5645MIPI_write_cmos_sensor(0x3815, 0x31);
		/* ; flip off, V bin on */
		OV5645MIPI_write_cmos_sensor(0x3820, 0x41);
		/* ; mirror on, H bin on */
		OV5645MIPI_write_cmos_sensor(0x3821, 0x07);
		OV5645MIPI_write_cmos_sensor(0x4514, 0x00);
		/* ; ae mode */
		OV5645MIPI_write_cmos_sensor(0x3a00, 0x38);
		/* ; max exp 60 = 740 */
		OV5645MIPI_write_cmos_sensor(0x3a02, 0x03);
		/* ; max exp 60 */
		OV5645MIPI_write_cmos_sensor(0x3a03, 0xd8);
		/* ; B50 = 222 */
		OV5645MIPI_write_cmos_sensor(0x3a08, 0x01);
		/* ; B50 */
		OV5645MIPI_write_cmos_sensor(0x3a09, 0x27);
		/* ; B60 = 185 */
		OV5645MIPI_write_cmos_sensor(0x3a0a, 0x00);
		/* ; B60 */
		OV5645MIPI_write_cmos_sensor(0x3a0b, 0xf6);
		/* ; max 50 */
		OV5645MIPI_write_cmos_sensor(0x3a0e, 0x03);
		/* ; max 60 */
		OV5645MIPI_write_cmos_sensor(0x3a0d, 0x04);
		/* ; max exp 50 = 740 */
		OV5645MIPI_write_cmos_sensor(0x3a14, 0x03);
		/* ; max exp 50 */
		OV5645MIPI_write_cmos_sensor(0x3a15, 0xd8);
		/* ; 50/60 auto detect */
		OV5645MIPI_write_cmos_sensor(0x3c07, 0x08);
		/* ; 50/60 auto detect */
		OV5645MIPI_write_cmos_sensor(0x3c08, 0x00);
		/* ; 50/60 auto detect */
		OV5645MIPI_write_cmos_sensor(0x3c09, 0x1c);
		/* ; BLC line number */
		OV5645MIPI_write_cmos_sensor(0x4004, 0x02);
		/* ; BLC triggered by gain change */
		OV5645MIPI_write_cmos_sensor(0x4005, 0x18);
		/* MIPI global timing 16 */
		OV5645MIPI_write_cmos_sensor(0x4837, 0x11);
		OV5645MIPI_write_cmos_sensor(0x503d, 0x00);	/*  */
		OV5645MIPI_write_cmos_sensor(0x5000, 0xa7);	/*  */
		OV5645MIPI_write_cmos_sensor(0x5001, 0xa3);	/*  */
		OV5645MIPI_write_cmos_sensor(0x5002, 0x81);	/*  */
		OV5645MIPI_write_cmos_sensor(0x5003, 0x08);	/*  */
		OV5645MIPI_write_cmos_sensor(0x3032, 0x00);	/*  */
		OV5645MIPI_write_cmos_sensor(0x4000, 0x89);	/*  */
		OV5645MIPI_write_cmos_sensor(0x3000, 0x30);	/*  */
		OV5645MIPI_write_cmos_sensor(0x350c, 0x00);	/*  */
		OV5645MIPI_write_cmos_sensor(0x350d, 0x00);	/*  */
		LOG_INF("%s exit u2FrameRate == 30 setting:\n", __func__);
	} else if (u2FrameRate == 20) {
		/* fix30 */
		OV5645MIPI_write_cmos_sensor(0x3A00, 0x38);
		/* ; MIPI 2 lane */
		OV5645MIPI_write_cmos_sensor(0x300e, 0x45);
		/* PLL, MIPI 8-bit mode */
		OV5645MIPI_write_cmos_sensor(0x3034, 0x18);
		OV5645MIPI_write_cmos_sensor(0x3035, 0x21);	/* PLL */
		OV5645MIPI_write_cmos_sensor(0x3036, 0x4b);	/* PLL */
		OV5645MIPI_write_cmos_sensor(0x3037, 0x13);	/* PLL */
		OV5645MIPI_write_cmos_sensor(0x3108, 0x01);	/* PLL */
		OV5645MIPI_write_cmos_sensor(0x3824, 0x01);	/* PLL */
		OV5645MIPI_write_cmos_sensor(0x460c, 0x20);	/* PLL */
		OV5645MIPI_write_cmos_sensor(0x3618, 0x00);	/*  */
		OV5645MIPI_write_cmos_sensor(0x3600, 0x09);	/*  */
		OV5645MIPI_write_cmos_sensor(0x3601, 0x43);	/*  */
		OV5645MIPI_write_cmos_sensor(0x3708, 0x66);	/*  */
		OV5645MIPI_write_cmos_sensor(0x3709, 0x12);	/*  */
		OV5645MIPI_write_cmos_sensor(0x370c, 0xc3);	/*  */
		OV5645MIPI_write_cmos_sensor(0x3800, 0x00);	/* HS = 0 */
		OV5645MIPI_write_cmos_sensor(0x3801, 0x00);	/* HS */
		/* VS = 250 */
		OV5645MIPI_write_cmos_sensor(0x3802, 0x00);
		/* VS */
		OV5645MIPI_write_cmos_sensor(0x3803, 0x06);
		/* HW = 2623 */
		OV5645MIPI_write_cmos_sensor(0x3804, 0x0a);
		/* ; HW */
		OV5645MIPI_write_cmos_sensor(0x3805, 0x3f);
		/* ; VH = */
		OV5645MIPI_write_cmos_sensor(0x3806, 0x07);
		/* ; VH */
		OV5645MIPI_write_cmos_sensor(0x3807, 0x9d);
		/* ; DVPHO = 1280 */
		OV5645MIPI_write_cmos_sensor(0x3808, 0x05);
		/* ; DVPHO */
		OV5645MIPI_write_cmos_sensor(0x3809, 0x00);
		/* ; DVPVO = 960 */
		OV5645MIPI_write_cmos_sensor(0x380a, 0x03);
		/* ; DVPVO */
		OV5645MIPI_write_cmos_sensor(0x380b, 0xc0);
		/* ; HTS = 2160 */
		OV5645MIPI_write_cmos_sensor(0x380c, 0x07);
		/* ; HTS */
		OV5645MIPI_write_cmos_sensor(0x380d, 0x68);
		/* ; VTS = 740 */
		OV5645MIPI_write_cmos_sensor(0x380e, 0x03);
		/* ; VTS */
		OV5645MIPI_write_cmos_sensor(0x380f, 0xd8);
		/* H OFF = 16 */
		OV5645MIPI_write_cmos_sensor(0x3810, 0x00);
		/* H OFF */
		OV5645MIPI_write_cmos_sensor(0x3811, 0x10);
		/* V OFF = 4 */
		OV5645MIPI_write_cmos_sensor(0x3812, 0x00);
		/* ; V OFF */
		OV5645MIPI_write_cmos_sensor(0x3813, 0x06);
		/* ; X INC */
		OV5645MIPI_write_cmos_sensor(0x3814, 0x31);
		/* ; Y INC */
		OV5645MIPI_write_cmos_sensor(0x3815, 0x31);
		/* ; flip off, V bin on */
		OV5645MIPI_write_cmos_sensor(0x3820, 0x41);
		/* ; mirror on, H bin on */
		OV5645MIPI_write_cmos_sensor(0x3821, 0x07);
		OV5645MIPI_write_cmos_sensor(0x4514, 0x00);
		/* ; ae mode */
		OV5645MIPI_write_cmos_sensor(0x3a00, 0x38);
		/* ; max exp 60 = 740 */
		OV5645MIPI_write_cmos_sensor(0x3a02, 0x03);
		/* ; max exp 60 */
		OV5645MIPI_write_cmos_sensor(0x3a03, 0xd8);
		/* ; B50 = 222 */
		OV5645MIPI_write_cmos_sensor(0x3a08, 0x01);
		/* ; B50 */
		OV5645MIPI_write_cmos_sensor(0x3a09, 0x27);
		/* ; B60 = 185 */
		OV5645MIPI_write_cmos_sensor(0x3a0a, 0x00);
		/* ; B60 */
		OV5645MIPI_write_cmos_sensor(0x3a0b, 0xf6);
		/* ; max 50 */
		OV5645MIPI_write_cmos_sensor(0x3a0e, 0x03);
		/* ; max 60 */
		OV5645MIPI_write_cmos_sensor(0x3a0d, 0x04);
		/* ; max exp 50 = 740 */
		OV5645MIPI_write_cmos_sensor(0x3a14, 0x03);
		/* ; max exp 50 */
		OV5645MIPI_write_cmos_sensor(0x3a15, 0xd8);
		/* ; 50/60 auto detect */
		OV5645MIPI_write_cmos_sensor(0x3c07, 0x08);
		/* ; 50/60 auto detect */
		OV5645MIPI_write_cmos_sensor(0x3c08, 0x00);
		/* ; 50/60 auto detect */
		OV5645MIPI_write_cmos_sensor(0x3c09, 0x1c);
		/* ; BLC line number */
		OV5645MIPI_write_cmos_sensor(0x4004, 0x02);
		/* ; BLC triggered by gain change */
		OV5645MIPI_write_cmos_sensor(0x4005, 0x18);
		/* MIPI global timing 16 */
		OV5645MIPI_write_cmos_sensor(0x4837, 0x11);
		OV5645MIPI_write_cmos_sensor(0x503d, 0x00);	/*  */
		OV5645MIPI_write_cmos_sensor(0x5000, 0xa7);	/*  */
		OV5645MIPI_write_cmos_sensor(0x5001, 0xa3);	/*  */
		OV5645MIPI_write_cmos_sensor(0x5002, 0x81);	/*  */
		OV5645MIPI_write_cmos_sensor(0x5003, 0x08);	/*  */
		OV5645MIPI_write_cmos_sensor(0x3032, 0x00);	/*  */
		OV5645MIPI_write_cmos_sensor(0x4000, 0x89);	/*  */
		OV5645MIPI_write_cmos_sensor(0x3000, 0x30);	/*  */
		OV5645MIPI_write_cmos_sensor(0x350c, 0x00);	/*  */
		OV5645MIPI_write_cmos_sensor(0x350d, 0x00);	/*  */
		LOG_INF("%s exit u2FrameRate == 20 setting:\n", __func__);
	} else if (u2FrameRate == 15) {
		/* ;OV5645MIPI 1280x960,15fps */
		/* 28Mhz, 112Mbps/Lane, 2Lane. */
		LOG_INF("%s enter u2FrameRate == 15 setting  :\n ", __func__);
		OV5645MIPI_write_cmos_sensor(0x3A00, 0x38);	/* fix15 */
		/* ; MIPI 2 lane */
		OV5645MIPI_write_cmos_sensor(0x300e, 0x45);
		/* PLL, MIPI 8-bit mode */
		OV5645MIPI_write_cmos_sensor(0x3034, 0x18);
		/* PLL */
		OV5645MIPI_write_cmos_sensor(0x3035, 0x21);
		/* PLL */
		OV5645MIPI_write_cmos_sensor(0x3036, 0x38);
		/* PLL */
		OV5645MIPI_write_cmos_sensor(0x3037, 0x13);
		/* PLL */
		OV5645MIPI_write_cmos_sensor(0x3108, 0x01);
		/* PLL */
		OV5645MIPI_write_cmos_sensor(0x3824, 0x01);
		/* PLL */
		OV5645MIPI_write_cmos_sensor(0x460c, 0x20);
		OV5645MIPI_write_cmos_sensor(0x3618, 0x00);	/*  */
		OV5645MIPI_write_cmos_sensor(0x3600, 0x09);	/*  */
		OV5645MIPI_write_cmos_sensor(0x3601, 0x43);	/*  */
		OV5645MIPI_write_cmos_sensor(0x3708, 0x66);	/*  */
		OV5645MIPI_write_cmos_sensor(0x3709, 0x12);	/*  */
		OV5645MIPI_write_cmos_sensor(0x370c, 0xc3);	/*  */
		/* HS = 0 */
		OV5645MIPI_write_cmos_sensor(0x3800, 0x00);
		/* HS */
		OV5645MIPI_write_cmos_sensor(0x3801, 0x00);
		/* VS = 250 */
		OV5645MIPI_write_cmos_sensor(0x3802, 0x00);
		/* VS */
		OV5645MIPI_write_cmos_sensor(0x3803, 0x06);
		/* HW = 2623 */
		OV5645MIPI_write_cmos_sensor(0x3804, 0x0a);
		/* ; HW */
		OV5645MIPI_write_cmos_sensor(0x3805, 0x3f);
		/* ; VH = */
		OV5645MIPI_write_cmos_sensor(0x3806, 0x07);
		/* ; VH */
		OV5645MIPI_write_cmos_sensor(0x3807, 0x9d);
		/* ; DVPHO = 1280 */
		OV5645MIPI_write_cmos_sensor(0x3808, 0x05);
		/* ; DVPHO */
		OV5645MIPI_write_cmos_sensor(0x3809, 0x00);
		/* ; DVPVO = 960 */
		OV5645MIPI_write_cmos_sensor(0x380a, 0x03);
		/* ; DVPVO */
		OV5645MIPI_write_cmos_sensor(0x380b, 0xc0);
		/* ; HTS = 2160 */
		OV5645MIPI_write_cmos_sensor(0x380c, 0x07);
		/* ; HTS */
		OV5645MIPI_write_cmos_sensor(0x380d, 0x68);
		/* ; VTS = 740 */
		OV5645MIPI_write_cmos_sensor(0x380e, 0x03);
		/* ; VTS */
		OV5645MIPI_write_cmos_sensor(0x380f, 0xd8);
		/* H OFF = 16 */
		OV5645MIPI_write_cmos_sensor(0x3810, 0x00);
		/* H OFF */
		OV5645MIPI_write_cmos_sensor(0x3811, 0x10);
		/* V OFF = 4 */
		OV5645MIPI_write_cmos_sensor(0x3812, 0x00);
		/* ; V OFF */
		OV5645MIPI_write_cmos_sensor(0x3813, 0x06);
		/* ; X INC */
		OV5645MIPI_write_cmos_sensor(0x3814, 0x31);
		/* ; Y INC */
		OV5645MIPI_write_cmos_sensor(0x3815, 0x31);
		/* ; flip off, V bin on */
		OV5645MIPI_write_cmos_sensor(0x3820, 0x41);
		/* ; mirror on, H bin on */
		OV5645MIPI_write_cmos_sensor(0x3821, 0x07);
		OV5645MIPI_write_cmos_sensor(0x4514, 0x00);
		/* ; ae mode */
		OV5645MIPI_write_cmos_sensor(0x3a00, 0x38);
		/* ; max exp 60 = 740 */
		OV5645MIPI_write_cmos_sensor(0x3a02, 0x03);
		/* ; max exp 60 */
		OV5645MIPI_write_cmos_sensor(0x3a03, 0xd8);
		/* ; B50 = 222 */
		OV5645MIPI_write_cmos_sensor(0x3a08, 0x00);
		/* ; B50 */
		OV5645MIPI_write_cmos_sensor(0x3a09, 0x94);
		/* ; B60 = 185 */
		OV5645MIPI_write_cmos_sensor(0x3a0a, 0x00);
		/* ; B60 */
		OV5645MIPI_write_cmos_sensor(0x3a0b, 0x7b);
		/* ; max 50 */
		OV5645MIPI_write_cmos_sensor(0x3a0e, 0x06);
		/* ; max 60 */
		OV5645MIPI_write_cmos_sensor(0x3a0d, 0x07);
		/* ; max exp 50 = 740 */
		OV5645MIPI_write_cmos_sensor(0x3a14, 0x03);
		/* ; max exp 50 */
		OV5645MIPI_write_cmos_sensor(0x3a15, 0xd8);
		/* ; 50/60 auto detect */
		OV5645MIPI_write_cmos_sensor(0x3c07, 0x08);
		/* ; 50/60 auto detect */
		OV5645MIPI_write_cmos_sensor(0x3c08, 0x00);
		/* ; 50/60 auto detect */
		OV5645MIPI_write_cmos_sensor(0x3c09, 0x1c);
		/* ; BLC line number */
		OV5645MIPI_write_cmos_sensor(0x4004, 0x02);
		/* ; BLC triggered by gain change */
		OV5645MIPI_write_cmos_sensor(0x4005, 0x18);
		/* MIPI global timing 16 */
		OV5645MIPI_write_cmos_sensor(0x4837, 0x11);
		OV5645MIPI_write_cmos_sensor(0x503d, 0x00);	/*  */
		OV5645MIPI_write_cmos_sensor(0x5000, 0xa7);	/*  */
		OV5645MIPI_write_cmos_sensor(0x5001, 0xa3);	/*  */
		OV5645MIPI_write_cmos_sensor(0x5002, 0x81);	/*  */
		OV5645MIPI_write_cmos_sensor(0x5003, 0x08);	/*  */
		OV5645MIPI_write_cmos_sensor(0x3032, 0x00);	/*  */
		OV5645MIPI_write_cmos_sensor(0x4000, 0x89);	/*  */
		OV5645MIPI_write_cmos_sensor(0x3000, 0x30);	/*  */
		OV5645MIPI_write_cmos_sensor(0x350c, 0x00);	/*  */
		OV5645MIPI_write_cmos_sensor(0x350d, 0x00);	/*  */
		LOG_INF("%s exit u2FrameRate == 15 setting:\n", __func__);
	} else {
		LOG_INF("Wrong frame rate setting\n");
	}
	LOG_INF("exit %s function:\n ", __func__);
	return TRUE;
}

/**************************/
static void OV5645MIPIGetEvAwbRef(UINT32 pSensorAEAWBRefStruct)
{
#if 0
	PSENSOR_AE_AWB_REF_STRUCT Ref =
		(PSENSOR_AE_AWB_REF_STRUCT) pSensorAEAWBRefStruct;

	Ref->SensorAERef.AeRefLV05Shutter = 1503;
	Ref->SensorAERef.AeRefLV05Gain = 496 * 2;
	Ref->SensorAERef.AeRefLV13Shutter = 49;
	Ref->SensorAERef.AeRefLV13Gain = 64 * 2;
	Ref->SensorAwbGainRef.AwbRefD65Rgain = 188;
	Ref->SensorAwbGainRef.AwbRefD65Bgain = 128;
	Ref->SensorAwbGainRef.AwbRefCWFRgain = 160;
	Ref->SensorAwbGainRef.AwbRefCWFBgain = 164;
#endif
}

static void OV5645MIPIGetCurAeAwbInfo(UINT32 pSensorAEAWBCurStruct)
{
#if 0
	PSENSOR_AE_AWB_CUR_STRUCT Info =
		(PSENSOR_AE_AWB_CUR_STRUCT) pSensorAEAWBCurStruct;

	Info->SensorAECur.AeCurShutter = OV5645MIPIReadShutter();
	Info->SensorAECur.AeCurGain = OV5645MIPIReadSensorGain() * 2;
	Info->SensorAwbGainCur.AwbCurRgain =
			OV5645MIPIYUV_read_cmos_sensor(0x504c);
	Info->SensorAwbGainCur.AwbCurBgain =
			OV5645MIPIYUV_read_cmos_sensor(0x504e);
#endif
}

UINT32 OV5645MIPIMaxFramerateByScenario(
	MSDK_SCENARIO_ID_ENUM scenarioId, MUINT32 frameRate)
{
	kal_uint32 pclk;
	kal_int16 dummyLine;
	kal_uint16 lineLength, frameHeight;

	LOG_INF("%s: scenarioId = %d, frame rate = %d\n", __func__,
			   scenarioId, frameRate);
	LOG_INF("enter %s function:\n ", __func__);
	switch (scenarioId) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		pclk = 56000000;
		lineLength = OV5645MIPI_IMAGE_SENSOR_SVGA_WIDTH;
		frameHeight = (10 * pclk) / frameRate / lineLength;
		dummyLine = frameHeight - OV5645MIPI_IMAGE_SENSOR_SVGA_HEIGHT;
		if (dummyLine < 0)
			dummyLine = 0;
		spin_lock(&ov5645mipi_drv_lock);
		OV5645MIPISensor.SensorMode = SENSOR_MODE_PREVIEW;
		OV5645MIPISensor.PreviewDummyLines = dummyLine;
		spin_unlock(&ov5645mipi_drv_lock);
		OV5645MIPISetDummy(OV5645MIPISensor.PreviewDummyPixels,
				dummyLine);
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		pclk = 56000000;
		lineLength = OV5645MIPI_IMAGE_SENSOR_VIDEO_WITDH;
		frameHeight = (10 * pclk) / frameRate / lineLength;
		dummyLine = frameHeight - OV5645MIPI_IMAGE_SENSOR_VIDEO_HEIGHT;
		if (dummyLine < 0)
			dummyLine = 0;
		/* spin_lock(&ov5645mipi_drv_lock); */
		/* ov8825.sensorMode = SENSOR_MODE_VIDEO; */
		/* spin_unlock(&ov5645mipi_drv_lock); */
		OV5645MIPISetDummy(0, dummyLine);
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
	case MSDK_SCENARIO_ID_CAMERA_ZSD:
		pclk = 84000000;
		lineLength = OV5645MIPI_IMAGE_SENSOR_QSXGA_WITDH;
		frameHeight = (10 * pclk) / frameRate / lineLength;
		dummyLine = frameHeight - OV5645MIPI_IMAGE_SENSOR_QSXGA_HEIGHT;
		if (dummyLine < 0)
			dummyLine = 0;
		spin_lock(&ov5645mipi_drv_lock);
		OV5645MIPISensor.CaptureDummyLines = dummyLine;
		OV5645MIPISensor.SensorMode = SENSOR_MODE_CAPTURE;
		spin_unlock(&ov5645mipi_drv_lock);
		OV5645MIPISetDummy(OV5645MIPISensor.CaptureDummyPixels,
			dummyLine);
		break;
	case MSDK_SCENARIO_ID_CAMERA_3D_PREVIEW:	/* added */
		break;
	case MSDK_SCENARIO_ID_CAMERA_3D_VIDEO:
		break;
	case MSDK_SCENARIO_ID_CAMERA_3D_CAPTURE:	/* added */
		break;
	default:
		break;
	}
	LOG_INF("exit %s function:\n ", __func__);
	return ERROR_NONE;
}

UINT32 OV5645MIPIGetDefaultFramerateByScenario(
	MSDK_SCENARIO_ID_ENUM scenarioId,
	MUINT32 *pframeRate)
{
	LOG_INF("enter %s function:\n ", __func__);
	switch (scenarioId) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		*pframeRate = 300;
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
	case MSDK_SCENARIO_ID_CAMERA_ZSD:
		*pframeRate = 150;
		break;
	case MSDK_SCENARIO_ID_CAMERA_3D_PREVIEW:	/* added */
	case MSDK_SCENARIO_ID_CAMERA_3D_VIDEO:
	case MSDK_SCENARIO_ID_CAMERA_3D_CAPTURE:	/* added */
		*pframeRate = 300;
		break;
	default:
		break;
	}
	LOG_INF("exit %s function:\n ", __func__);
	return ERROR_NONE;
}

void OV5645MIPI_get_AEAWB_lock(UINT32 *pAElockRet32, UINT32 *pAWBlockRet32)
{
	LOG_INF("enter %s function:\n ", __func__);
	*pAElockRet32 = 1;
	*pAWBlockRet32 = 1;
	LOG_INF("%s,AE=%d,AWB=%d\n", __func__, *pAElockRet32,
			   *pAWBlockRet32);
	LOG_INF("exit %s function:\n ", __func__);
}

void OV5645_MIPI_GetExifInfo(UINT32 exifAddr)
{
	SENSOR_EXIF_INFO_STRUCT *pExifInfo =
		(SENSOR_EXIF_INFO_STRUCT *) exifAddr;

	pExifInfo->FNumber = 28;
	pExifInfo->AEISOSpeed = AE_ISO_100;
	pExifInfo->AWBMode = OV5645CurrentStatus.iWB;
	pExifInfo->CapExposureTime = 0;
	pExifInfo->FlashLightTimeus = 0;
	pExifInfo->RealISOValue = AE_ISO_100;
	LOG_INF("exit %s function:\n ", __func__);
}

/* mod for flash */
#define FLASH_BV_THRESHOLD 0x20
int flag_torch;
static void OV5645MIPI_FlashTriggerCheck(unsigned int *pFeatureReturnPara32)
{
	unsigned int NormBr;

	flag_torch = 1;
	NormBr = OV5645MIPIYUV_read_cmos_sensor(0x56A1);
	LOG_INF("################NormBr is %d\n", NormBr);
	if (NormBr > FLASH_BV_THRESHOLD) {
		*pFeatureReturnPara32 = FALSE;
		return;
	}
	*pFeatureReturnPara32 = TRUE;
}

/* --------end */
UINT32 OV5645MIPIFeatureControl(MSDK_SENSOR_FEATURE_ENUM FeatureId,
				UINT8 *pFeaturePara,
				UINT32 *pFeatureParaLen)
{
	UINT16 *pFeatureReturnPara16 = (UINT16 *) pFeaturePara;
	UINT16 *pFeatureData16 = (UINT16 *) pFeaturePara;
	UINT32 *pFeatureReturnPara32 = (UINT32 *) pFeaturePara;
	UINT32 *pFeatureData32 = (UINT32 *) pFeaturePara;
	MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData =
		(MSDK_SENSOR_CONFIG_STRUCT *) pFeaturePara;
	MSDK_SENSOR_REG_INFO_STRUCT *pSensorRegData =
		(MSDK_SENSOR_REG_INFO_STRUCT *) pFeaturePara;
	UINT32 Tony_Temp1 = 0;
	UINT32 Tony_Temp2 = 0;

	Tony_Temp1 = pFeaturePara[0];
	Tony_Temp2 = pFeaturePara[1];
	LOG_INF("feature id=%d\n", FeatureId);
	switch (FeatureId) {
	case SENSOR_FEATURE_GET_RESOLUTION:
		*pFeatureReturnPara16++ =
			OV5645MIPI_IMAGE_SENSOR_QSXGA_WITDH;
		*pFeatureReturnPara16 =
			OV5645MIPI_IMAGE_SENSOR_QSXGA_HEIGHT;
		*pFeatureParaLen = 4;
		break;
	case SENSOR_FEATURE_GET_PERIOD:
		switch (CurrentScenarioId) {
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
			*pFeatureReturnPara16++ =
			    OV5645MIPI_FULL_PERIOD_PIXEL_NUMS +
				OV5645MIPISensor.CaptureDummyPixels;
			*pFeatureReturnPara16 =
			    OV5645MIPI_FULL_PERIOD_LINE_NUMS +
				OV5645MIPISensor.CaptureDummyLines;
			*pFeatureParaLen = 4;
			break;
		default:
			*pFeatureReturnPara16++ =
			    OV5645MIPI_PV_PERIOD_PIXEL_NUMS +
				OV5645MIPISensor.PreviewDummyPixels;
			*pFeatureReturnPara16 =
			    OV5645MIPI_PV_PERIOD_LINE_NUMS +
				OV5645MIPISensor.PreviewDummyLines;
			*pFeatureParaLen = 4;
			break;
		}
	// LOG_INF("OV5645MIPIFeatureControl:
	//	SENSOR_FEATURE_GET_PERIOD\n");
	// *pFeatureReturnPara16++=OV5645MIPI_PV_PERIOD_PIXEL_NUMS +
	//				OV5645MIPISensor.PreviewDummyPixels;
	//  pFeatureReturnPara16=OV5645MIPI_PV_PERIOD_LINE_NUMS +
	//				OV5645MIPISensor.PreviewDummyLines;
	//	/* *pFeatureParaLen=4; */
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
		switch (CurrentScenarioId) {
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
			*pFeatureReturnPara32 =
				/* unit: Hz */
				OV5645MIPISensor.CapturePclk * 1000 * 100;
			*pFeatureParaLen = 4;
			break;
		default:
			*pFeatureReturnPara32 =
				/* unit: Hz */
				OV5645MIPISensor.PreviewPclk * 1000 * 100;
			*pFeatureParaLen = 4;
			break;
		}
		// LOG_INF("OV5645MIPIFeatureControl:
		//	SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ\n");
		//  *pFeatureReturnPara32 =
		//	OV5645MIPISensor.PreviewPclk * 1000 *100; //unit: Hz
		//  *pFeatureParaLen=4;
		break;
	case SENSOR_FEATURE_SET_ESHUTTER:
		// LOG_INF("OV5645MIPIFeatureControl:
		//	SENSOR_FEATURE_SET_ESHUTTER\n");
		break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
		// LOG_INF("OV5645MIPIFeatureControl:
		//	SENSOR_FEATURE_SET_NIGHTMODE\n");
		OV5645MIPI_night_mode((BOOL) *pFeatureData16);
		break;
	case SENSOR_FEATURE_SET_GAIN:
		// LOG_INF("OV5645MIPIFeatureControl:
		// SENSOR_FEATURE_SET_GAIN\n");
		break;
		/* mod for flash */
	case SENSOR_FEATURE_GET_TRIGGER_FLASHLIGHT_INFO:
		OV5645MIPI_FlashTriggerCheck(pFeatureData32);
		break;
		/* end */
	case SENSOR_FEATURE_SET_FLASHLIGHT:
		// LOG_INF("OV5645MIPIFeatureControl:
		//	SENSOR_FEATURE_SET_FLASHLIGHT\n");
		break;
	case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
		//  LOG_INF("OV5645MIPIFeatureControl:
		//	SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ\n");
		break;
	case SENSOR_FEATURE_SET_REGISTER:
		//  LOG_INF("OV5645MIPIFeatureControl:
		//	SENSOR_FEATURE_SET_REGISTER\n");
		OV5645MIPI_write_cmos_sensor(pSensorRegData->RegAddr,
					pSensorRegData->RegData);
		break;
	case SENSOR_FEATURE_GET_REGISTER:
		//  LOG_INF("OV5645MIPIFeatureControl:
		//	SENSOR_FEATURE_GET_REGISTER\n");
		pSensorRegData->RegData =
			OV5645MIPIYUV_read_cmos_sensor(
			pSensorRegData->RegAddr);
		break;
	case SENSOR_FEATURE_GET_CONFIG_PARA:
		//  LOG_INF("OV5645MIPIFeatureControl:
		//	SENSOR_FEATURE_GET_CONFIG_PARA\n");
		memcpy(pSensorConfigData, &OV5645MIPISensorConfigData,
		       sizeof(MSDK_SENSOR_CONFIG_STRUCT));
		*pFeatureParaLen = sizeof(MSDK_SENSOR_CONFIG_STRUCT);
		break;
	case SENSOR_FEATURE_SET_CCT_REGISTER:
	case SENSOR_FEATURE_GET_CCT_REGISTER:
	case SENSOR_FEATURE_SET_ENG_REGISTER:
	case SENSOR_FEATURE_GET_ENG_REGISTER:
	case SENSOR_FEATURE_GET_REGISTER_DEFAULT:
	case SENSOR_FEATURE_CAMERA_PARA_TO_SENSOR:
	case SENSOR_FEATURE_SENSOR_TO_CAMERA_PARA:
	case SENSOR_FEATURE_GET_GROUP_INFO:
	case SENSOR_FEATURE_GET_ITEM_INFO:
	case SENSOR_FEATURE_SET_ITEM_INFO:
	case SENSOR_FEATURE_GET_ENG_INFO:
		//  LOG_INF("OV5645MIPIFeatureControl:
		//	SENSOR_FEATURE_SET/get_CCT_xxxx ect\n");
		break;
	case SENSOR_FEATURE_GET_GROUP_COUNT:
		*pFeatureReturnPara32++ = 0;
		*pFeatureParaLen = 4;
		//  LOG_INF("OV5645MIPIFeatureControl:
		//	SENSOR_FEATURE_GET_GROUP_COUNT\n");
		break;
	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
		//  LOG_INF("OV5645MIPIFeatureControl:
		//	SENSOR_FEATURE_GET_LENS_DRIVER_ID\n");
		//  get the lens driver ID from EEPROM or just
		//	return LENS_DRIVER_ID_DO_NOT_CARE
		// if EEPROM does not exist in camera module.
		*pFeatureReturnPara32 = LENS_DRIVER_ID_DO_NOT_CARE;
		*pFeatureParaLen = 4;
		break;
	case SENSOR_FEATURE_CHECK_SENSOR_ID:
		//LOG_INF("%s:SENSOR_FEATURE_CHECK_SENSOR_ID\n");
		OV5645MIPI_GetSensorID(pFeatureData32);
		break;
	case SENSOR_FEATURE_SET_YUV_CMD:
		// LOG_INF("OV5645MIPIFeatureControl:
		//	SENSOR_FEATURE_SET_YUV_CMD\n");
		OV5645MIPIYUVSensorSetting((FEATURE_ID) *pFeatureData32,
					*(pFeatureData32 + 1));
		break;
	case SENSOR_FEATURE_SET_VIDEO_MODE:
		// LOG_INF("OV5645MIPIFeatureControl:
		//	SENSOR_FEATURE_SET_VIDEO_MODE\n");
		OV5645MIPIYUVSetVideoMode(*pFeatureData16);
		break;
	case SENSOR_FEATURE_GET_EV_AWB_REF:
		OV5645MIPIGetEvAwbRef(*pFeatureData32);
		break;
	case SENSOR_FEATURE_GET_SHUTTER_GAIN_AWB_GAIN:
		OV5645MIPIGetCurAeAwbInfo(*pFeatureData32);
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		OV5645MIPIMaxFramerateByScenario(
			(MSDK_SCENARIO_ID_ENUM) *pFeatureData32,
			 *(pFeatureData32 + 1));
		break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		OV5645MIPIGetDefaultFramerateByScenario(
			(MSDK_SCENARIO_ID_ENUM) *pFeatureData32,
			(MUINT32 *) (pFeatureData32 + 1));
		break;
	case SENSOR_FEATURE_GET_EXIF_INFO:
		/* SENSORDB("[4EC] F_GET_EXIF_INFO\n"); */
		OV5645_MIPI_GetExifInfo(*pFeatureData32);
		break;
#ifndef G130
	    /**********************below is AF control**********************/
	case SENSOR_FEATURE_INITIALIZE_AF:
		LOG_INF("SENSOR_FEATURE_INITIALIZE_AF\n");
		OV5645_FOCUS_OVT_AFC_Init();
		break;
	case SENSOR_FEATURE_MOVE_FOCUS_LENS:
		LOG_INF("SENSOR_FEATURE_MOVE_FOCUS_LENS\n");
		OV5645_FOCUS_Move_to(*pFeatureData16);
		break;
	case SENSOR_FEATURE_GET_AF_STATUS:
		LOG_INF("SENSOR_FEATURE_GET_AF_STATUS\n");
		OV5645_FOCUS_OVT_AFC_Get_AF_Status(pFeatureReturnPara32);
		*pFeatureParaLen = 4;
		break;
	case SENSOR_FEATURE_SINGLE_FOCUS_MODE:
		LOG_INF("SENSOR_FEATURE_SINGLE_FOCUS_MODE\n");
		OV5645_FOCUS_OVT_AFC_Single_Focus();
		break;
	case SENSOR_FEATURE_CONSTANT_AF:
		LOG_INF("SENSOR_FEATURE_CONSTANT_AF\n");
		OV5645_FOCUS_OVT_AFC_Constant_Focus();
		break;
	case SENSOR_FEATURE_CANCEL_AF:
		LOG_INF("SENSOR_FEATURE_CANCEL_AF\n");
		OV5645_FOCUS_OVT_AFC_Cancel_Focus();
		break;
	case SENSOR_FEATURE_GET_AF_INF:
		LOG_INF("SENSOR_FEATURE_GET_AF_INF\n");
		OV5645_FOCUS_Get_AF_Inf(pFeatureReturnPara32);
		*pFeatureParaLen = 4;
		break;
	case SENSOR_FEATURE_GET_AF_MACRO:
		LOG_INF("SENSOR_FEATURE_GET_AF_MACRO\n");
		OV5645_FOCUS_Get_AF_Macro(pFeatureReturnPara32);
		*pFeatureParaLen = 4;
		break;
	case SENSOR_FEATURE_SET_AF_WINDOW:
		LOG_INF("SENSOR_FEATURE_SET_AF_WINDOW\n");
		OV5645_FOCUS_Set_AF_Window(*pFeatureData32);
		break;
	case SENSOR_FEATURE_GET_AF_MAX_NUM_FOCUS_AREAS:
		LOG_INF("SENSOR_FEATURE_GET_AF_MAX_NUM_FOCUS_AREAS\n");
		OV5645_FOCUS_Get_AF_Max_Num_Focus_Areas(pFeatureReturnPara32);
		*pFeatureParaLen = 4;
		break;
	case SENSOR_FEATURE_GET_AE_AWB_LOCK_INFO:
		LOG_INF("SENSOR_FEATURE_GET_AF_STATUS\n");
		OV5645MIPI_get_AEAWB_lock(pFeatureData32, (pFeatureData32 + 1));
		break;
	case SENSOR_FEATURE_GET_AE_MAX_NUM_METERING_AREAS:
		LOG_INF("AE zone addr = 0x%x\n", *pFeatureData32);
		OV5645_FOCUS_Get_AE_Max_Num_Metering_Areas(
			pFeatureReturnPara32);
		*pFeatureParaLen = 4;
		break;
	case SENSOR_FEATURE_SET_AE_WINDOW:
		LOG_INF("AE zone addr = 0x%x\n", *pFeatureData32);
		OV5645_FOCUS_Set_AE_Window(*pFeatureData32);
		break;
#endif

	default:
		LOG_INF("%s:default\n", __func__);
		break;
	}
	LOG_INF("exit %s function:\n ", __func__);
	return ERROR_NONE;
}				/* OV5645MIPIFeatureControl() */

SENSOR_FUNCTION_STRUCT SensorFuncOV5645MIPI = {
	OV5645MIPIOpen,
	OV5645MIPIGetInfo,
	OV5645MIPIGetResolution,
	OV5645MIPIFeatureControl,
	OV5645MIPIControl,
	OV5645MIPIClose
};

UINT32 OV5645_MIPI_YUV_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &SensorFuncOV5645MIPI;
	return ERROR_NONE;
}				/* SensorInit() */
