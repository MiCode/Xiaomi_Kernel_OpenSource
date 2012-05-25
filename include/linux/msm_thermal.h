/*
 * Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#ifndef __MSM_THERMAL_H
#define __MSM_THERMAL_H

struct msm_thermal_data {
	uint32_t sensor_id;
	uint32_t poll_ms;
	uint32_t limit_temp;
	uint32_t temp_hysteresis;
	uint32_t limit_freq;
};

#ifdef CONFIG_THERMAL_MONITOR
extern int msm_thermal_init(struct msm_thermal_data *pdata);
#else
static inline int msm_thermal_init(struct msm_thermal_data *pdata)
{
	return -ENOSYS;
}
#endif

#endif /*__MSM_THERMAL_H*/
