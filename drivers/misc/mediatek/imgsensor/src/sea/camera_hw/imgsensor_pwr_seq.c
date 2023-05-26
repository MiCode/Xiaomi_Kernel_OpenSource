// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */


#include "kd_imgsensor.h"


#include "imgsensor_hw.h"
#include "imgsensor_cfg_table.h"

/* Legacy design */
struct IMGSENSOR_HW_POWER_SEQ sensor_power_sequence[] = {
#if defined(S5KHM2SD_MAIN_OFILM_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5KHM2SD_MAIN_OFILM_MIPI_RAW,
		{
			{RST, Vol_Low, 0},
			{PDN, Vol_Low, 1},
			{PDN, Vol_High, 2},
			{DVDD, Vol_1100, 0},
			{AVDD, Vol_2800, 0},
			{DOVDD, Vol_1800, 2},
			{RST, Vol_High, 2},
			{SensorMCLK, Vol_High, 10},
		},
	},
#endif
#if defined(S5KHM2SD_MAIN_SEMCO_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5KHM2SD_MAIN_SEMCO_MIPI_RAW,
		{
			{RST, Vol_Low, 0},
			{PDN, Vol_Low, 1},
			{PDN, Vol_High, 2},
			{DVDD, Vol_1100, 0},
			{AVDD, Vol_2800, 0},
			{DOVDD, Vol_1800, 2},
			{RST, Vol_High, 2},
			{SensorMCLK, Vol_High, 10},
		},
	},
#endif
#if defined(OV64B40_MAIN_SUNNY_MIPI_RAW)
	{
		SENSOR_DRVNAME_OV64B40_MAIN_SUNNY_MIPI_RAW,
		{
			{RST, Vol_Low, 0},
			{PDN, Vol_Low, 1},
			{PDN, Vol_High, 2},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1100, 0},
			//{AFVDD, Vol_2800, 2},
			{SensorMCLK, Vol_High, 1},
			{RST, Vol_High, 8},
		},
	},
#endif
#if defined(OV64B40_MAIN_AAC_MIPI_RAW)
	{
		SENSOR_DRVNAME_OV64B40_MAIN_AAC_MIPI_RAW,
		{
			{RST, Vol_Low, 0},
			{PDN, Vol_Low, 1},
			{PDN, Vol_High, 2},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1100, 0},
			//{AFVDD, Vol_2800, 2},
			{SensorMCLK, Vol_High, 1},
			{RST, Vol_High, 8},
		},
	},
#endif
#if defined(OV64B40_MAIN_OFILM_MIPI_RAW)
	{
		SENSOR_DRVNAME_OV64B40_MAIN_OFILM_MIPI_RAW,
		{
			{RST, Vol_Low, 0},
			{PDN, Vol_Low, 1},
			{PDN, Vol_High, 2},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1100, 0},
			//{AFVDD, Vol_2800, 2},
			{SensorMCLK, Vol_High, 1},
			{RST, Vol_High, 8},
		},
	},
#endif
#if defined(IMX471_FRONT_SUNNY_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX471_FRONT_SUNNY_MIPI_RAW,
		{
			{RST, Vol_Low, 1},
			{AVDD, Vol_2800, 1},
			{DVDD, Vol_1100, 1},
			{DOVDD, Vol_1800, 0},
			{SensorMCLK, Vol_High, 1},
			{RST, Vol_High, 1},
		},
	},
#endif
#if defined(IMX471_FRONT_OFILM_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX471_FRONT_OFILM_MIPI_RAW,
		{
			{RST, Vol_Low, 1},
			{AVDD, Vol_2800, 1},
			{DVDD, Vol_1100, 1},
			{DOVDD, Vol_1800, 0},
			{SensorMCLK, Vol_High, 1},
			{RST, Vol_High, 1},
		},
	},
#endif
#if defined(IMX355_ULTRA_OFILM_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX355_ULTRA_OFILM_MIPI_RAW,
		{
			{RST, Vol_Low, 0},
			{AVDD, Vol_2800, 1},
			{DOVDD, Vol_1800, 1},
			{DVDD, Vol_1200, 1},
			{SensorMCLK, Vol_High, 5},
			{RST, Vol_High, 5}
		},
	},
#endif
#if defined(S5K4H7_ULTRA_SUNNY_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K4H7_ULTRA_SUNNY_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 2},
			{RST, Vol_Low, 2},
			{AVDD, Vol_2800, 2},
			{DVDD, Vol_1200, 2},
			{DOVDD, Vol_1800, 2},
			{RST, Vol_High, 0},
		},
	},
#endif
#if defined(OV02B1B_DEPTH_SUNNY_MIPI_RAW)
	{
		SENSOR_DRVNAME_OV02B1B_DEPTH_SUNNY_MIPI_RAW,
		{
			{RST, Vol_Low, 1},
			{SensorMCLK, Vol_Low, 1},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 9},
			{SensorMCLK, Vol_High, 1},
			{RST, Vol_High, 5},
		},
	},
#endif
#if defined(OV02B1B_DEPTH_TRULY_MIPI_RAW)
	{
		SENSOR_DRVNAME_OV02B1B_DEPTH_TRULY_MIPI_RAW,
		{
			{RST, Vol_Low, 1},
			{SensorMCLK, Vol_Low, 1},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 9},
			{SensorMCLK, Vol_High, 1},
			{RST, Vol_High, 5},
		},
	},
#endif
#if defined(OV02B1B_DEPTH_SUNNY2_MIPI_RAW)
	{
		SENSOR_DRVNAME_OV02B1B_DEPTH_SUNNY2_MIPI_RAW,
		{
			{RST, Vol_Low, 1},
			{SensorMCLK, Vol_Low, 1},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 9},
			{SensorMCLK, Vol_High, 1},
			{RST, Vol_High, 5},
		},
	},
#endif
#if defined(GC02M1_MACRO_OFILM_MIPI_RAW)
	{
		SENSOR_DRVNAME_GC02M1_MACRO_OFILM_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 2},
			{RST, Vol_Low, 2},
			{DOVDD, Vol_1800, 2},
			{AVDD, Vol_2800, 2},
			{RST, Vol_High, 0},
		},
	},
#endif
#if defined(GC02M1_MACRO_AAC_MIPI_RAW)
	{
		SENSOR_DRVNAME_GC02M1_MACRO_AAC_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 2},
			{RST, Vol_Low, 2},
			{DOVDD, Vol_1800, 2},
			{AVDD, Vol_2800, 2},
			{RST, Vol_High, 0},
		},
	},
#endif

	/* add new sensor before this line */
	{NULL,},
};

