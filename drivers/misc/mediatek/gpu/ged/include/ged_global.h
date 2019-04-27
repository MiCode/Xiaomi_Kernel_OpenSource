/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __GED_GLOBAL_H__
#define __GED_GLOBAL_H__

#include "ged_dvfs.h"

extern GED_LOG_BUF_HANDLE ghLogBuf_DVFS;
extern GED_LOG_BUF_HANDLE ghLogBuf_ged_srv;
extern void (*mtk_boost_gpu_freq_fp)(void);
extern void (*mtk_set_bottom_gpu_freq_fp)(unsigned int);
extern unsigned int (*mtk_get_bottom_gpu_freq_fp)(void);
extern unsigned int (*mtk_custom_get_gpu_freq_level_count_fp)(void);
extern void (*mtk_custom_boost_gpu_freq_fp)(unsigned int ui32FreqLevel);
extern void (*mtk_custom_upbound_gpu_freq_fp)(unsigned int ui32FreqLevel);
extern unsigned int (*mtk_get_custom_boost_gpu_freq_fp)(void);
extern unsigned int (*mtk_get_custom_upbound_gpu_freq_fp)(void);
extern unsigned int (*mtk_get_gpu_loading_fp)(void);
extern unsigned int (*mtk_get_gpu_loading2_fp)(int);
extern unsigned int (*mtk_get_gpu_block_fp)(void);
extern unsigned int (*mtk_get_gpu_idle_fp)(void);
extern void (*mtk_do_gpu_dvfs_fp)(unsigned long t, long phase, unsigned long ul3DFenceDoneTime);
extern void (*mtk_gpu_dvfs_set_mode_fp)(int eMode);

extern unsigned int (*mtk_get_gpu_sub_loading_fp)(void);
extern unsigned long (*mtk_get_vsync_based_target_freq_fp)(void);
extern void (*mtk_get_gpu_dvfs_from_fp)(MTK_GPU_DVFS_TYPE *peType, unsigned long *pulFreq);

extern unsigned long (*mtk_get_gpu_bottom_freq_fp)(void);
extern unsigned long (*mtk_get_gpu_custom_boost_freq_fp)(void);
extern unsigned long (*mtk_get_gpu_custom_upbound_freq_fp)(void);

extern void ged_monitor_3D_fence_set_enable(GED_BOOL bEnable);

extern unsigned int g_ui32EventStatus;
extern unsigned int g_ui32EventDebugStatus;

extern void (*ged_dvfs_cal_gpu_utilization_fp)(unsigned int *pui32Loading, unsigned int *pui32Block, unsigned int *pui32Idle);
extern void (*ged_dvfs_gpu_freq_commit_fp)(unsigned long ui32NewFreqID, GED_DVFS_COMMIT_TYPE eCommitType, int *pbCommited);
extern bool ged_gpu_power_on_notified;
extern bool ged_gpu_power_off_notified;

extern bool mtk_get_bottom_gpu_freq(unsigned int *pui32FreqLevel);
extern GED_LOG_BUF_HANDLE ghLogBuf_GED;

extern void (*mtk_gpu_sodi_entry_fp)(void);
extern void (*mtk_gpu_sodi_exit_fp)(void);

extern unsigned int g_gpu_timer_based_emu;
extern unsigned long g_ulCalResetTS_us; // calculate loading reset time stamp
extern unsigned long g_ulPreCalResetTS_us; // previous calculate loading reset time stamp
extern unsigned long g_ulWorkingPeriod_us; // last frame half, t0

#endif
