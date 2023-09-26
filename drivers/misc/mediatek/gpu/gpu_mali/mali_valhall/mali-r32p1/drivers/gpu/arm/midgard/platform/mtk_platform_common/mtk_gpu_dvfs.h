/*
 * Copyright (C) 2021 MediaTek Inc.
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

#ifndef __MTK_GPU_DVFS_H__
#define __MTK_GPU_DVFS_H__

#include <ged_dvfs.h>

#if IS_ENABLED(CONFIG_MALI_MIDGARD_DVFS) && IS_ENABLED(CONFIG_MTK_GPU_COMMON_DVFS)
#if IS_ENABLED(GED_ENABLE_DVFS_LOADING_MODE)
void mtk_common_cal_gpu_utilization_ex(unsigned int *pui32Loading,
                                       unsigned int *pui32Block,
                                       unsigned int *pui32Idle,
                                       void *Util_Ex);
#else
void mtk_common_cal_gpu_utilization(unsigned int *pui32Loading,
                                    unsigned int *pui32Block,
                                    unsigned int *pui32Idle);
#endif
void mtk_common_ged_dvfs_commit(unsigned long ui32NewFreqID,
                                GED_DVFS_COMMIT_TYPE eCommitType,
                                int *pbCommited);
int mtk_common_get_util_active(void);
int mtk_common_get_util_3d(void);
int mtk_common_get_util_ta(void);
int mtk_common_get_util_compute(void);
#endif

#if IS_ENABLED(CONFIG_MALI_MIDGARD_DVFS) && IS_ENABLED(CONFIG_MTK_GPU_COMMON_DVFS)
#if IS_ENABLED(GED_ENABLE_DVFS_LOADING_MODE)
extern void (*ged_dvfs_cal_gpu_utilization_ex_fp)(unsigned int *pui32Loading,
             unsigned int *pui32Block, unsigned int *pui32Idle, void *Util_Ex);
#else
extern void (*ged_dvfs_cal_gpu_utilization_fp)(unsigned int *pui32Loading,
             unsigned int *pui32Block, unsigned int *pui32Idle);
#endif
extern void (*ged_dvfs_gpu_freq_commit_fp)(unsigned long ui32NewFreqID,
             GED_DVFS_COMMIT_TYPE eCommitType, int *pbCommited);
#endif

#endif /* __MTK_GPU_DVFS_H__ */
