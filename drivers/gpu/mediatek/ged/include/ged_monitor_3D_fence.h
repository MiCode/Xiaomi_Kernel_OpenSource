/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __GED_MONITOR_3D_FENCE_H__
#define __GED_MONITOR_3D_FENCE_H__

#include "ged_type.h"

GED_ERROR ged_monitor_3D_fence_add(int fence_fd);
void ged_monitor_3D_fence_notify(void);
unsigned long ged_monitor_3D_fence_done_time(void);
int ged_monitor_3D_fence_get_count(void);

#endif
