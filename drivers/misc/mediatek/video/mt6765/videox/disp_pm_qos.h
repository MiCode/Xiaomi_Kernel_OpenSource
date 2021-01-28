/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
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
int prim_disp_request_hrt_bw(int overlap_num,
			enum DDP_SCENARIO_ENUM scenario, const char *caller,
			unsigned int active_cfg);

#endif /* __DISP_PM_QOS_H__ */
