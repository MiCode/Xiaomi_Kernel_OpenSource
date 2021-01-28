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

#include <linux/atomic.h>
#include "imgsensor_common.h"
#include "imgsensor_platform.h"

#ifndef __IMGSENSOR_CFG_TABLE_H__
#define __IMGSENSOR_CFG_TABLE_H__

#define IMGSENSOR_DEV_NAME "kd_camera_hw"


#define IMGSENSOR_HW_POWER_INFO_MAX	12
#define IMGSENSOR_HW_SENSOR_MAX_NUM	12

enum IMGSENSOR_HW_PIN {
	IMGSENSOR_HW_PIN_NONE = 0,
	IMGSENSOR_HW_PIN_PDN,
	IMGSENSOR_HW_PIN_RST,
	IMGSENSOR_HW_PIN_AVDD,
	IMGSENSOR_HW_PIN_DVDD,
	IMGSENSOR_HW_PIN_DOVDD,
#ifdef MIPI_SWITCH
	IMGSENSOR_HW_PIN_MIPI_SWITCH_EN,
	IMGSENSOR_HW_PIN_MIPI_SWITCH_SEL,
#endif
	IMGSENSOR_HW_PIN_MCLK,
	IMGSENSOR_HW_PIN_MAX_NUM,
	IMGSENSOR_HW_PIN_UNDEF = -1
};

enum IMGSENSOR_HW_PIN_STATE {
	IMGSENSOR_HW_PIN_STATE_LEVEL_0,
	IMGSENSOR_HW_PIN_STATE_LEVEL_1000,
	IMGSENSOR_HW_PIN_STATE_LEVEL_1100,
	IMGSENSOR_HW_PIN_STATE_LEVEL_1200,
	IMGSENSOR_HW_PIN_STATE_LEVEL_1210,
	IMGSENSOR_HW_PIN_STATE_LEVEL_1220,
	IMGSENSOR_HW_PIN_STATE_LEVEL_1500,
	IMGSENSOR_HW_PIN_STATE_LEVEL_1800,
	IMGSENSOR_HW_PIN_STATE_LEVEL_2500,
	IMGSENSOR_HW_PIN_STATE_LEVEL_2800,
	IMGSENSOR_HW_PIN_STATE_LEVEL_2900,
	IMGSENSOR_HW_PIN_STATE_LEVEL_HIGH,

	IMGSENSOR_HW_PIN_STATE_NONE = -1
};

/*Legacy design*/
/* PowerType */
#define SensorMCLK  IMGSENSOR_HW_PIN_MCLK
#define PDN         IMGSENSOR_HW_PIN_PDN
#define RST         IMGSENSOR_HW_PIN_RST
#define AVDD        IMGSENSOR_HW_PIN_AVDD
#define DVDD        IMGSENSOR_HW_PIN_DVDD
#define DOVDD       IMGSENSOR_HW_PIN_DOVDD
#define VDD_None    IMGSENSOR_HW_PIN_NONE

	/* For backward compatible */
#define AFVDD       IMGSENSOR_HW_PIN_UNDEF


/* Voltage */
#define Vol_Low   IMGSENSOR_HW_PIN_STATE_LEVEL_0
#define Vol_High  IMGSENSOR_HW_PIN_STATE_LEVEL_HIGH
#define Vol_1000  IMGSENSOR_HW_PIN_STATE_LEVEL_1000
#define Vol_1100  IMGSENSOR_HW_PIN_STATE_LEVEL_1100
#define Vol_1200  IMGSENSOR_HW_PIN_STATE_LEVEL_1200
#define Vol_1210  IMGSENSOR_HW_PIN_STATE_LEVEL_1210
#define Vol_1220  IMGSENSOR_HW_PIN_STATE_LEVEL_1220
#define Vol_1500  IMGSENSOR_HW_PIN_STATE_LEVEL_1500
#define Vol_1800  IMGSENSOR_HW_PIN_STATE_LEVEL_1800
#define Vol_2500  IMGSENSOR_HW_PIN_STATE_LEVEL_2500
#define Vol_2800  IMGSENSOR_HW_PIN_STATE_LEVEL_2800
#define Vol_2900  IMGSENSOR_HW_PIN_STATE_LEVEL_2900

enum IMGSENSOR_HW_ID {
	IMGSENSOR_HW_ID_MCLK,
	IMGSENSOR_HW_ID_REGULATOR,
	IMGSENSOR_HW_ID_GPIO,

	IMGSENSOR_HW_ID_MAX_NUM,
	IMGSENSOR_HW_ID_NONE = -1
};

#define IMGSENSOR_I2C_DRV_NAME_0  "kd_camera_hw"
#define IMGSENSOR_I2C_DRV_NAME_1  "kd_camera_hw_bus2"
#define IMGSENSOR_I2C_DRV_NAME_2  "kd_camera_hw_bus3"
#define IMGSENSOR_I2C_DRV_NAME_3  "kd_camera_hw_bus4"
#define IMGSENSOR_I2C_DRV_NAME_4  "kd_camera_hw_bus5"
#define IMGSENSOR_I2C_DRV_NAME_5  "kd_camera_hw_bus6"
#define IMGSENSOR_I2C_DRV_NAME_6  "kd_camera_hw_bus7"
#define IMGSENSOR_I2C_DRV_NAME_7  "kd_camera_hw_bus8"
#define IMGSENSOR_I2C_DRV_NAME_8  "kd_camera_hw_trigger"

#define IMGSENSOR_I2C_OF_DRV_NAME_0 "mediatek,camera_main"
#define IMGSENSOR_I2C_OF_DRV_NAME_1 "mediatek,camera_sub"
#define IMGSENSOR_I2C_OF_DRV_NAME_2 "mediatek,camera_main_two"
#define IMGSENSOR_I2C_OF_DRV_NAME_3 "mediatek,camera_sub_two"
#define IMGSENSOR_I2C_OF_DRV_NAME_4 "mediatek,camera_main_three"
#define IMGSENSOR_I2C_OF_DRV_NAME_5 "mediatek,camera_sub_three"
#define IMGSENSOR_I2C_OF_DRV_NAME_6 "mediatek,camera_main_four"
#define IMGSENSOR_I2C_OF_DRV_NAME_7 "mediatek,camera_sub_four"

enum IMGSENSOR_I2C_DEV {
	IMGSENSOR_I2C_DEV_0,
	IMGSENSOR_I2C_DEV_1,
	IMGSENSOR_I2C_DEV_2,
	IMGSENSOR_I2C_DEV_3,
	IMGSENSOR_I2C_DEV_4,
	IMGSENSOR_I2C_DEV_5,
	IMGSENSOR_I2C_DEV_6,
	IMGSENSOR_I2C_DEV_7,
	IMGSENSOR_I2C_DEV_MAX_NUM,
};

#endif

