/*
 * Copyright (C) 2016 MediaTek Inc.
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
static struct ccu_sensor_info
	g_ccu_sensor_info[IMGSENSOR_SENSOR_IDX_MAX_NUM] = {0};
static char g_ccu_sensor_name
	[IMGSENSOR_SENSOR_IDX_MAX_NUM][SENSOR_NAME_MAX_LEN];

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
		i < IMGSENSOR_SENSOR_IDX_MAX_NUM;
		i++)
		current_fps_list[i] = g_ccu_sensor_current_fps[i];
}

void ccu_set_sensor_info(int32_t sensorType, struct ccu_sensor_info *info)
{
	if (sensorType == IMGSENSOR_SENSOR_IDX_NONE) {
		/*Non-sensor*/
		LOG_ERR("No sensor been detected.\n");
	} else if ((sensorType >= IMGSENSOR_SENSOR_IDX_MIN_NUM) &&
		(sensorType < IMGSENSOR_SENSOR_IDX_MAX_NUM)) {
		g_ccu_sensor_info[sensorType].slave_addr  = info->slave_addr;
		g_ccu_sensor_info[sensorType].i2c_id  = info->i2c_id;
		if (info->sensor_name_string != NULL) {
			memcpy(g_ccu_sensor_name[sensorType],
			info->sensor_name_string,
			strlen(info->sensor_name_string)+1);
			g_ccu_sensor_info[sensorType].sensor_name_string =
				g_ccu_sensor_name[sensorType];
		}
		LOG_DBG_MUST("ccu catch sensor %d i2c slave address : 0x%x\n",
		sensorType, info->slave_addr);
		LOG_DBG_MUST("ccu catch sensor %d name : %s\n",
		sensorType, g_ccu_sensor_info[sensorType].sensor_name_string);
		LOG_DBG_MUST("ccu catch sensor %d i2c_id : %d\n",
		sensorType, g_ccu_sensor_info[sensorType].i2c_id);
	} else {
		LOG_DBG_MUST("ccu catch sensor i2c slave address fail!\n");
	}
}

void ccu_get_sensor_i2c_slave_addr(int32_t *sensorI2cSlaveAddr)
{
	sensorI2cSlaveAddr[0] =
		g_ccu_sensor_info[IMGSENSOR_SENSOR_IDX_MAIN].slave_addr;
	sensorI2cSlaveAddr[1] =
		g_ccu_sensor_info[IMGSENSOR_SENSOR_IDX_SUB].slave_addr;
	sensorI2cSlaveAddr[2] =
		g_ccu_sensor_info[IMGSENSOR_SENSOR_IDX_MAIN2].slave_addr;
}

void ccu_get_sensor_name(char **sensor_name)
{
	sensor_name[0] =
	g_ccu_sensor_info[IMGSENSOR_SENSOR_IDX_MAIN].sensor_name_string;
	sensor_name[1] =
	g_ccu_sensor_info[IMGSENSOR_SENSOR_IDX_SUB].sensor_name_string;
	sensor_name[2] =
	g_ccu_sensor_info[IMGSENSOR_SENSOR_IDX_MAIN2].sensor_name_string;
}
