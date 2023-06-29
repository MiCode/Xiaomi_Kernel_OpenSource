/*
 * Copyright (C) 2017 MediaTek Inc.
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

#include "kd_imgsensor.h"


#include "imgsensor_hw.h"
#include "imgsensor_cfg_table.h"

/* Legacy design */
struct IMGSENSOR_HW_POWER_SEQ sensor_power_sequence[] = {
#if defined(S5KHM2SD_MAIN_SUNNY_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5KHM2SD_MAIN_SUNNY_MIPI_RAW,
		{
			{RST, Vol_Low, 0},
			{PDN, Vol_Low, 1},
			{PDN, Vol_High, 4},
			{DVDD, Vol_1100, 1},
			{AVDD, Vol_2800, 1},
			{DOVDD, Vol_1800, 1},
			{RST, Vol_High, 2},
			{SensorMCLK, Vol_High, 10},
		},
	},
#endif

#if defined(S5KHM2SD_MAIN_OFILM_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5KHM2SD_MAIN_OFILM_MIPI_RAW,
		{
			{RST, Vol_Low, 0},
			{PDN, Vol_Low, 1},
			{PDN, Vol_High, 4},
			{DVDD, Vol_1100, 1},
			{AVDD, Vol_2800, 1},
			{DOVDD, Vol_1800, 1},
			{RST, Vol_High, 2},
			{SensorMCLK, Vol_High, 10},
		},
	},
#endif

#if defined(HI1634Q_FRONT_OFILM_MIPI_RAW)
	{
		SENSOR_DRVNAME_HI1634Q_FRONT_OFILM_MIPI_RAW,
		{
			{RST, Vol_Low, 1},
			{DOVDD, Vol_1800, 1},
			{AVDD, Vol_2800, 1},
			{DVDD, Vol_1200, 1},
			{SensorMCLK, Vol_High, 1},
			{RST, Vol_High, 2},
		},
	},
#endif

#if defined(HI1634Q_FRONT_QTECH_MIPI_RAW)
	{
		SENSOR_DRVNAME_HI1634Q_FRONT_QTECH_MIPI_RAW,
		{
			{RST, Vol_Low, 1},
			{DOVDD, Vol_1800, 1},
			{AVDD, Vol_2800, 1},
			{DVDD, Vol_1200, 1},
			{SensorMCLK, Vol_High, 1},
			{RST, Vol_High, 2},
		},
	},
#endif

#if defined(OV8856_ULTRA_AAC_MIPI_RAW)
	{
		SENSOR_DRVNAME_OV8856_ULTRA_AAC_MIPI_RAW,
		{
			{RST, Vol_Low, 1},
			{SensorMCLK, Vol_High, 0},
			{DOVDD, Vol_1800, 1},
			{AVDD, Vol_2800, 1},
			{DVDD, Vol_1200, 1},
			{RST, Vol_High, 1},
		},
	},
#endif

#if defined(IMX355_ULTRA_SUNNY_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX355_ULTRA_SUNNY_MIPI_RAW,
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

#if defined(OV02B1B_DEPTH_SUNNY_MIPI_RAW)
    {
		SENSOR_DRVNAME_OV02B1B_DEPTH_SUNNY_MIPI_RAW,
		{
			{RST, Vol_Low, 1},
			{SensorMCLK, Vol_Low, 1},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 9},
			{SensorMCLK, Vol_High, 5},
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
			{SensorMCLK, Vol_High, 5},
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

