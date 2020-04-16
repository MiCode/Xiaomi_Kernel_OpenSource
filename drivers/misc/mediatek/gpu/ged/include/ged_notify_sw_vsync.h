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

#ifndef __GED_NOTIFY_SW_VSYNC_H__
#define __GED_NOTIFY_SW_VSYNC_H__

#include "ged_type.h"

extern unsigned int gpu_block;
extern unsigned int gpu_idle;
extern unsigned int gpu_av_loading;

GED_ERROR ged_notify_sw_vsync(GED_VSYNC_TYPE eType, GED_DVFS_UM_QUERY_PACK *psQueryData);

GED_ERROR ged_notify_sw_vsync_system_init(void);

void ged_notify_sw_vsync_system_exit(void);
#ifdef GED_ENABLE_FB_DVFS
void ged_set_backup_timer_timeout(u64 time_out);
void ged_cancel_backup_timer(void);
#endif

void ged_sodi_start(void);
void ged_sodi_stop(void);

#if defined(CONFIG_MACH_MT8167) || defined(CONFIG_MACH_MT8173) ||\
defined(CONFIG_MACH_MT6739) || defined(CONFIG_MACH_MT6761) ||\
defined(CONFIG_MACH_MT6765)
extern void MTKFWDump(void);
#endif

#endif
