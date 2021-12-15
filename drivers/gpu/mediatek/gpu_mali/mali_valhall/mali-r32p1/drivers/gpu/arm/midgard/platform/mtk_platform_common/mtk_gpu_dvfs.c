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

#include <mali_kbase.h>
#include <platform/mtk_platform_common.h>
#include <backend/gpu/mali_kbase_pm_internal.h>
#include <backend/gpu/mali_kbase_pm_defs.h>
#include "mtk_gpu_dvfs.h"

#if IS_ENABLED(CONFIG_MALI_MIDGARD_DVFS) && IS_ENABLED(CONFIG_MTK_GPU_COMMON_DVFS)
static unsigned int current_util_active;
static unsigned int current_util_3d;
static unsigned int current_util_ta;
static unsigned int current_util_compute;
#endif

#if IS_ENABLED(CONFIG_MALI_MIDGARD_DVFS) && IS_ENABLED(CONFIG_MTK_GPU_COMMON_DVFS)
void mtk_common_ged_dvfs_commit(unsigned long ui32NewFreqID,
                                GED_DVFS_COMMIT_TYPE eCommitType,
                                int *pbCommited)
{
	int ret = mtk_common_gpufreq_commit(ui32NewFreqID);
	if (pbCommited) {
		*pbCommited = (ret == 0) ? true : false;
	}
}
#endif

#if IS_ENABLED(CONFIG_MALI_MIDGARD_DVFS) && IS_ENABLED(CONFIG_MTK_GPU_COMMON_DVFS)
int mtk_common_get_util_active(void)
{
	return current_util_active;
}

int mtk_common_get_util_3d(void)
{
	return current_util_3d;
}

int mtk_common_get_util_ta(void)
{
	return current_util_ta;
}

int mtk_common_get_util_compute(void)
{
	return current_util_compute;
}

#if IS_ENABLED(GED_ENABLE_DVFS_LOADING_MODE)
void mtk_common_cal_gpu_utilization_ex(unsigned int *pui32Loading,
                                       unsigned int *pui32Block,
                                       unsigned int *pui32Idle,
                                       void *Util_Ex)
#else
void mtk_common_cal_gpu_utilization(unsigned int *pui32Loading,
                                    unsigned int *pui32Block,
                                    unsigned int *pui32Idle)
#endif
{
	struct kbase_device *kbdev = (struct kbase_device *)mtk_common_get_kbdev();
	int utilisation, util_gl_share;
	int util_cl_share[2];
	int busy;
	struct kbasep_pm_metrics *diff;
#if IS_ENABLED(GED_ENABLE_DVFS_LOADING_MODE)
	struct GpuUtilization_Ex *util_ex = (struct GpuUtilization_Ex *) Util_Ex;
#endif

	KBASE_DEBUG_ASSERT(kbdev != NULL);

	diff = &kbdev->pm.backend.metrics.dvfs_diff;

	kbase_pm_get_dvfs_metrics(kbdev, &kbdev->pm.backend.metrics.dvfs_last, diff);

	utilisation = (100 * diff->time_busy) /
			max(diff->time_busy + diff->time_idle, 1u);

	busy = max(diff->busy_gl + diff->busy_cl[0] + diff->busy_cl[1], 1u);

	util_gl_share = (100 * diff->busy_gl) / busy;
	util_cl_share[0] = (100 * diff->busy_cl[0]) / busy;
	util_cl_share[1] = (100 * diff->busy_cl[1]) / busy;

#if IS_ENABLED(GED_ENABLE_DVFS_LOADING_MODE)
	util_ex->util_active = utilisation;
	util_ex->util_3d = (100 * diff->busy_gl_plus[0]) /
			max(diff->time_busy + diff->time_idle, 1u);
	util_ex->util_ta = (100 * (diff->busy_gl_plus[1]+diff->busy_gl_plus[2])) /
			max(diff->time_busy + diff->time_idle, 1u);
	util_ex->util_compute = (100 * (diff->busy_cl[0]+diff->busy_cl[1])) /
			max(diff->time_busy + diff->time_idle, 1u);
#endif

	if (pui32Loading)
		*pui32Loading = utilisation;

	if (pui32Idle)
		*pui32Idle = 100 - utilisation;

	if (utilisation < 0 || util_gl_share < 0 ||
	    util_cl_share[0] < 0 || util_cl_share[1] < 0) {
		utilisation = 0;
		util_gl_share = 0;
		util_cl_share[0] = 0;
		util_cl_share[1] = 0;
	} else {
		current_util_active = utilisation;
		current_util_3d = (100 * diff->busy_gl_plus[0]) /
				max(diff->time_busy + diff->time_idle, 1u);
		current_util_ta = (100 * (diff->busy_gl_plus[1]+diff->busy_gl_plus[2])) /
				max(diff->time_busy + diff->time_idle, 1u);
		current_util_compute = (100 * (diff->busy_cl[0]+diff->busy_cl[1])) /
				max(diff->time_busy + diff->time_idle, 1u);
	}
}
#endif
