/* Copyright (c) 2013-2014, 2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef MSM_SENSOR_INIT_H
#define MSM_SENSOR_INIT_H

#include "msm_sensor.h"

struct msm_sensor_init_t {
	struct mutex imutex;
	struct msm_sd_subdev msm_sd;
	int module_init_status;
	wait_queue_head_t state_wait;
};

#endif
