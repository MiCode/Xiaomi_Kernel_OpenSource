/* Copyright (c) 2010, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __MSM_PM_QOS_H__
#define __MSM_PM_QOS_H__

#include <linux/pm_qos_params.h>

struct pm_qos_object;

int msm_pm_qos_add(struct pm_qos_object *class, char *request_name,
	s32 value, void **request_data);
int msm_pm_qos_update(struct pm_qos_object *class, char *request_name,
	s32 value, void **request_data);
int msm_pm_qos_remove(struct pm_qos_object *class, char *request_name,
	s32 value, void **request_data);

#endif /* __MSM_PM_QOS_H__ */

