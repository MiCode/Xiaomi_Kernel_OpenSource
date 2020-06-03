/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __GED_H__
#define __GED_H__

#include "ged_type.h"

#define GED_NOTIFICATION_TYPE                          int
#define GED_NOTIFICATION_TYPE_SW_VSYNC                 0
#define GED_NOTIFICATION_TYPE_HW_VSYNC_PRIMARY_DISPLAY 1

void ged_notification(GED_NOTIFICATION_TYPE eType);
int ged_set_target_fps(unsigned int target_fps, int mode);
unsigned int ged_get_cur_fps(void);
void ged_get_latest_perf_state(long long *t_cpu_remained,
				long long *t_gpu_remained,
				long *t_cpu_target,
				long *t_gpu_target);

#endif
