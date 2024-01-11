// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include "kd_imgsensor.h"


#include "imgsensor_hw.h"
#include "imgsensor_cfg_table.h"

/* Legacy design */
struct IMGSENSOR_HW_POWER_SEQ sensor_power_sequence[] = {

#if defined(OV50D40_TRULY_MAIN_I_MIPI_RAW)
	{
		SENSOR_DRVNAME_OV50D40_TRULY_MAIN_I_MIPI_RAW,
		{
			{RST, Vol_Low, 0},
			{AVDD, Vol_2800, 1},
			{DOVDD, Vol_1800, 1},
			{DVDD, Vol_1200, 1},
			{AFVDD, Vol_2800, 1},
			{SensorMCLK, Vol_High, 1},
			{RST, Vol_High, 5}
		},
	},
#endif

#if defined(S5KJNS_SUNNY_MAIN_II_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5KJNS_SUNNY_MAIN_II_MIPI_RAW,
		{
			{RST, Vol_Low, 1},
			{DOVDD, Vol_1800, 1},
			{DVDD, Vol_1100, 1},
			{AVDD, Vol_2800, 1},
			{AFVDD, Vol_2800, 0},
			{RST, Vol_High, 1},
			{SensorMCLK, Vol_High, 14}
		},
	},
#endif

#if defined(S5KJNS_SUNNY_MAIN_III_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5KJNS_SUNNY_MAIN_III_MIPI_RAW,
		{
			{RST, Vol_Low, 1},
			{DOVDD, Vol_1800, 1},
			{DVDD, Vol_1100, 1},
			{AVDD, Vol_2800, 1},
			{AFVDD, Vol_2800, 0},
			{RST, Vol_High, 1},
			{SensorMCLK, Vol_High, 14}
		},
	},
#endif

#if defined(SC520_TRULY_FRONT_I_MIPI_RAW)
	{
		SENSOR_DRVNAME_SC520_TRULY_FRONT_I_MIPI_RAW,
		{
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 1},
			{AVDD, Vol_2800, 1},
			{DVDD, Vol_1200, 1},
			{RST, Vol_High, 5},
			{SensorMCLK, Vol_High, 5}
		},
	},
#endif

#if defined(GC05A2_QTECH_FRONT_II_MIPI_RAW)
	{
		SENSOR_DRVNAME_GC05A2_QTECH_FRONT_II_MIPI_RAW,
		{
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 1},
			{AVDD, Vol_2800, 1},
			{DVDD, Vol_1200, 1},
			{RST, Vol_High, 5},
			{SensorMCLK, Vol_High, 5}
		},
	},
#endif



	/* add new sensor before this line */
	{NULL,},
};

