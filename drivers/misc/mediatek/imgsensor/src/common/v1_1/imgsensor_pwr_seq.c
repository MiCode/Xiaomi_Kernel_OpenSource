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
#if defined(S5KJN1SUNNY_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5KJN1SUNNY_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 3},
			{RST, Vol_Low, 1},
			{DOVDD, Vol_1800, 1},
			{DVDD, Vol_1050, 1},
			{AVDD, Vol_2800, 5},
			{RST, Vol_High, 5},
		},
	},
#endif
#if defined(OV50C40OFILM_MIPI_RAW)
	{
		SENSOR_DRVNAME_OV50C40OFILM_MIPI_RAW,
		{
                        {SensorMCLK, Vol_High, 0},
			{RST, Vol_Low, 1},
			{AVDD, Vol_2800, 1},
			{DOVDD, Vol_1800, 1},
			{DVDD, Vol_1200, 1},
			{RST, Vol_High, 5},
		},
	},
#endif
#if defined(OV16A1QOFILM_MIPI_RAW)
       {
               SENSOR_DRVNAME_OV16A1QOFILM_MIPI_RAW,
               {
                       {RST, Vol_Low, 2},
                       {DOVDD, Vol_1800, 0},
                       {DVDD, Vol_1200, 0},
                       {AVDD, Vol_2800, 2},
                       {SensorMCLK, Vol_High, 2},
                       {RST, Vol_High, 5},
               },
       },
#endif
#if defined(OV16A1QQTECH_MIPI_RAW)
       {
               SENSOR_DRVNAME_OV16A1QQTECH_MIPI_RAW,
               {
                       {RST, Vol_Low, 2},
                       {DOVDD, Vol_1800, 0},
                       {DVDD, Vol_1200, 0},
                       {AVDD, Vol_2800, 2},
                       {SensorMCLK, Vol_High, 2},
                       {RST, Vol_High, 5},
               },
       },
#endif
#if defined(OV02B10AAC_MIPI_RAW)
	{
		SENSOR_DRVNAME_OV02B10AAC_MIPI_RAW,
		{
			{IMGSENSOR_HW_PIN_MIPI_SWITCH_SEL, Vol_High, 1},
			{RST, Vol_Low, 1},
			{DOVDD, Vol_1800, 1},
			{SensorMCLK, Vol_High, 0},
			{AVDD, Vol_2800, 9},
			{RST, Vol_High, 1}
		},
	},
#endif
#if defined(GC02M1OFILM_MIPI_RAW)
	{
		SENSOR_DRVNAME_GC02M1OFILM_MIPI_RAW,
        {
			{IMGSENSOR_HW_PIN_MIPI_SWITCH_SEL, Vol_High, 1},
			{RST, Vol_Low, 1},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 1},
			{SensorMCLK, Vol_High, 1},
			{RST, Vol_High, 1}
		},
	},
#endif
#if defined(IMX355OFILM_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX355OFILM_MIPI_RAW,
		{
			{RST, Vol_Low, 1},
			{DOVDD, Vol_1800, 0},
			{DVDD, Vol_1200, 0},
			{AVDD, Vol_2700, 0},
			{SensorMCLK, Vol_High, 1},
			{RST, Vol_High, 1}
		},
	},
#endif
#if defined(IMX355SUNNY_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX355SUNNY_MIPI_RAW,
		{
			{RST, Vol_Low, 1},
			{DOVDD, Vol_1800, 0},
			{DVDD, Vol_1200, 0},
			{AVDD, Vol_2700, 0},
			{SensorMCLK, Vol_High, 1},
			{RST, Vol_High, 1}
		},
	},
#endif
	/* add new sensor before this line */
	{NULL,},
};
