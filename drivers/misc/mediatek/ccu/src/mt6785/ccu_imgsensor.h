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

#ifndef _CCU_IMGSENSOR_H_
#define _CCU_IMGSENSOR_H_
#include "ccu_i2c.h"
void ccu_get_current_fps(int32_t *current_fps_list);

void ccu_get_sensor_i2c_info(struct ccu_i2c_info *sensor_info);

void ccu_get_sensor_name(char **sensor_name);

#endif
