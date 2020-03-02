/*
 * Copyright (C) 2015 MediaTek Inc.
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

#ifndef __DISP_PM_QOS_H__
#define __DISP_PM_QOS_H__

#include "layering_rule.h"

#define HRT_BW_UNREQ 0xFFFF
#define HRT_BW_BYPASS 0x0

void disp_pm_qos_init(void);
void disp_pm_qos_deinit(void);
int disp_pm_qos_request_dvfs(enum HRT_LEVEL hrt);

int disp_pm_qos_update_bw(unsigned long long bandwidth);
int disp_pm_qos_set_default_bw(unsigned long long *bandwidth);
int disp_pm_qos_set_ovl_bw(unsigned long long in_fps,
			unsigned long long out_fps,
			unsigned long long *bandwidth);
int disp_pm_qos_set_rdma_bw(unsigned long long out_fps,
			unsigned long long *bandwidth);
int disp_pm_qos_set_default_hrt(void);
unsigned int get_has_hrt_bw(void);

#endif /* __DISP_PM_QOS_H__ */
