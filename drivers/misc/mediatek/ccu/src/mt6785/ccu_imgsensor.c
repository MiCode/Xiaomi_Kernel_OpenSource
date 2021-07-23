/*
 * Copyright (C) 2016 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
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

#include <linux/types.h>
#include "ccu_cmn.h"
#include "ccu_imgsensor_if.h"
#include "ccu_imgsensor.h"
#include "kd_camera_feature.h"/*for IMGSENSOR_SENSOR_IDX*/

/*>>>>> Information get from imgsensor driver*/
static int32_t g_ccu_sensor_current_fps[IMGSENSOR_SENSOR_IDX_MAX_NUM] = {-1};
#define SENSOR_NAME_MAX_LEN 50
static struct ccu_sensor_info g_ccu_sensor_info_main  = {-1, 0, NULL};
static char g_ccu_sensor_name_main[SENSOR_NAME_MAX_LEN];
static struct ccu_sensor_info g_ccu_sensor_info_main2  = {-1, 0, NULL};
static char g_ccu_sensor_name_main2[SENSOR_NAME_MAX_LEN];
static struct ccu_sensor_info g_ccu_sensor_info_main3  = {-1, 0, NULL};
static char g_ccu_sensor_name_main3[SENSOR_NAME_MAX_LEN];
static struct ccu_sensor_info g_ccu_sensor_info_sub  = {-1, 0, NULL};
static char g_ccu_sensor_name_sub[SENSOR_NAME_MAX_LEN];
static struct ccu_sensor_info g_ccu_sensor_info_sub2  = {-1, 0, NULL};
static char g_ccu_sensor_name_sub2[SENSOR_NAME_MAX_LEN];

/*<<<<< Information get from imgsensor driver*/

void ccu_set_current_fps(int32_t sensorType, int32_t current_fps)
{
	g_ccu_sensor_current_fps[sensorType] = current_fps;
	LOG_DBG_MUST("ccu catch current fps : type(%d), fps(%d)\n",
		sensorType, current_fps);
}

void ccu_get_current_fps(int32_t *current_fps_list)
{
	int i;

	for (i = IMGSENSOR_SENSOR_IDX_MIN_NUM;
		i < IMGSENSOR_SENSOR_IDX_MAX_NUM ; i++)
		current_fps_list[i] = g_ccu_sensor_current_fps[i];
}

void ccu_set_sensor_info(int32_t sensorType, struct ccu_sensor_info *info)
{
	if (sensorType == IMGSENSOR_SENSOR_IDX_NONE) {
		/*Non-sensor*/
		LOG_ERR("No sensor been detected.\n");
	} else if (sensorType == IMGSENSOR_SENSOR_IDX_MAIN) {
		/*Main*/
		g_ccu_sensor_info_main.slave_addr  = info->slave_addr;
		if (info->sensor_name_string != NULL) {
			memcpy(g_ccu_sensor_name_main, info->sensor_name_string,
				strlen(info->sensor_name_string)+1);
			g_ccu_sensor_info_main.sensor_name_string =
			g_ccu_sensor_name_main;
		}
		LOG_DBG_MUST("ccu catch Main sensor i2c slave address : 0x%x\n",
			info->slave_addr);
		LOG_DBG_MUST("ccu catch Main sensor name : %s\n",
			g_ccu_sensor_info_main.sensor_name_string);
	} else if (sensorType == IMGSENSOR_SENSOR_IDX_SUB) {
		/*Sub*/
		g_ccu_sensor_info_sub.slave_addr  = info->slave_addr;
		if (info->sensor_name_string != NULL) {
			memcpy(g_ccu_sensor_name_sub, info->sensor_name_string,
				strlen(info->sensor_name_string)+1);
			g_ccu_sensor_info_sub.sensor_name_string =
			g_ccu_sensor_name_sub;
		}
		LOG_DBG_MUST("ccu catch Sub sensor i2c slave address : 0x%x\n",
			info->slave_addr);
		LOG_DBG_MUST("ccu catch Sub sensor name : %s\n",
			g_ccu_sensor_info_sub.sensor_name_string);
	} else if (sensorType == IMGSENSOR_SENSOR_IDX_MAIN2) {
		/*Main2*/
		g_ccu_sensor_info_main2.slave_addr  = info->slave_addr;
		if (info->sensor_name_string != NULL) {
			memcpy(g_ccu_sensor_name_main2,
				info->sensor_name_string,
				strlen(info->sensor_name_string)+1);
			g_ccu_sensor_info_main2.sensor_name_string =
			g_ccu_sensor_name_main2;
		}
		LOG_DBG_MUST("ccu catch Main2 sensor i2c slave addr : 0x%x\n",
			info->slave_addr);
		LOG_DBG_MUST("ccu catch Main2 sensor name : %s\n",
			g_ccu_sensor_info_main2.sensor_name_string);
	} else if (sensorType == IMGSENSOR_SENSOR_IDX_MAIN3) {
		/*Main2*/
		g_ccu_sensor_info_main3.slave_addr  = info->slave_addr;
		if (info->sensor_name_string != NULL) {
			memcpy(g_ccu_sensor_name_main3,
				info->sensor_name_string,
				strlen(info->sensor_name_string)+1);
			g_ccu_sensor_info_main3.sensor_name_string =
			g_ccu_sensor_name_main3;
		}
		LOG_DBG_MUST("ccu catch Main3 sensor i2c slave addr : 0x%x\n",
			info->slave_addr);
		LOG_DBG_MUST("ccu catch Main3 sensor name : %s\n",
			g_ccu_sensor_info_main3.sensor_name_string);
	} else if (sensorType == IMGSENSOR_SENSOR_IDX_SUB2) {
		/*Main2*/
		g_ccu_sensor_info_sub2.slave_addr  = info->slave_addr;
		if (info->sensor_name_string != NULL) {
			memcpy(g_ccu_sensor_name_sub2,
				info->sensor_name_string,
				strlen(info->sensor_name_string)+1);
			g_ccu_sensor_info_sub2.sensor_name_string =
			g_ccu_sensor_name_sub2;
		}
		LOG_DBG_MUST("ccu catch sub2 sensor i2c slave addr : 0x%x\n",
			info->slave_addr);
		LOG_DBG_MUST("ccu catch sub2 sensor name : %s\n",
			g_ccu_sensor_info_sub2.sensor_name_string);
	} else {
		LOG_DBG_MUST("ccu catch sensor i2c slave address fail!\n");
	}
}

void ccu_get_sensor_i2c_slave_addr(int32_t *sensorI2cSlaveAddr)
{
	sensorI2cSlaveAddr[0] =
	g_ccu_sensor_info_main.slave_addr;
	sensorI2cSlaveAddr[1] =
	g_ccu_sensor_info_sub.slave_addr;
	sensorI2cSlaveAddr[2] =
	g_ccu_sensor_info_main2.slave_addr;
	sensorI2cSlaveAddr[3] =
	g_ccu_sensor_info_main3.slave_addr;
	sensorI2cSlaveAddr[4] =
	g_ccu_sensor_info_sub2.slave_addr;
}

void ccu_get_sensor_name(char **sensor_name)
{
	sensor_name[0] =
	g_ccu_sensor_info_main.sensor_name_string;
	sensor_name[1] =
	g_ccu_sensor_info_sub.sensor_name_string;
	sensor_name[2] =
	g_ccu_sensor_info_main2.sensor_name_string;
	sensor_name[3] =
	g_ccu_sensor_info_main3.sensor_name_string;
	sensor_name[4] =
	g_ccu_sensor_info_sub2.sensor_name_string;
}
