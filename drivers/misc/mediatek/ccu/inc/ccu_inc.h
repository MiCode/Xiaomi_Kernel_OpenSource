/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016 MediaTek Inc.
 */

#ifndef __CCU_INC_H__
#define __CCU_INC_H__

struct ccu_sensor_info {
	int32_t slave_addr;
	int32_t i2c_id;
	char *sensor_name_string;
};


void ccu_set_current_fps(int32_t current_fps);
void ccu_set_sensor_info(int32_t sensorType, struct ccu_sensor_info *info);
void ccu_get_timestamp(uint32_t *low, uint32_t *high);

#endif
