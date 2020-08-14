/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _CCU_IMGSENSOR_IF_H_
#define _CCU_IMGSENSOR_IF_H_

struct ccu_sensor_info {
	int32_t slave_addr;
	int32_t i2c_id;
	char *sensor_name_string;
};


void ccu_set_current_fps(int32_t sensorType, int32_t current_fps);
void ccu_set_sensor_info(int32_t sensorType, struct ccu_sensor_info *info);

#endif
