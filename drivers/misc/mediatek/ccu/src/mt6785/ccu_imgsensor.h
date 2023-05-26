/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef _CCU_IMGSENSOR_H_
#define _CCU_IMGSENSOR_H_
#include "ccu_i2c.h"
void ccu_get_current_fps(int32_t *current_fps_list);

void ccu_get_sensor_i2c_info(struct ccu_i2c_info *sensor_info);

void ccu_get_sensor_name(char **sensor_name);

#endif
