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

#ifndef __MTK_GPU_UTILITY_H__
#define __MTK_GPU_UTILITY_H__

#include <linux/types.h>
#include <linux/preempt.h>
#include <linux/trace_events.h>

  #define MTK_GPU_DVFS_TYPE_LIST {\
MTK_GPU_DVFS_TYPE_ITEM(NONE) \
MTK_GPU_DVFS_TYPE_ITEM(SMARTBOOST) \
MTK_GPU_DVFS_TYPE_ITEM(VSYNCBASED) \
MTK_GPU_DVFS_TYPE_ITEM(FALLBACK) \
MTK_GPU_DVFS_TYPE_ITEM(TIMERBASED) \
MTK_GPU_DVFS_TYPE_ITEM(FASTDVFS) \
MTK_GPU_DVFS_TYPE_ITEM(TOUCHBOOST) \
MTK_GPU_DVFS_TYPE_ITEM(THERMAL) \
MTK_GPU_DVFS_TYPE_ITEM(CUSTOMIZATION)}

typedef enum MTK_GPU_DVFS_TYPE_TAG

#define MTK_GPU_DVFS_TYPE_ITEM(type) MTK_GPU_DVFS_TYPE_##type,
MTK_GPU_DVFS_TYPE_LIST
#undef MTK_GPU_DVFS_TYPE_ITEM
MTK_GPU_DVFS_TYPE;

/* gpu info*/
struct gpu_info {
	u32 freq;
	u32 loading;
	u32 dvfs_type;
	u32 thermal_limit_freq;
};

#ifdef __cplusplus
extern "C"
{
#endif

/* returning false indicated no implement */

/* unit: x bytes */
bool mtk_get_gpu_memory_usage(unsigned int *pMemUsage);
bool mtk_get_gpu_page_cache(unsigned int *pPageCache);

/* unit: 0~100 % */
bool mtk_get_gpu_loading(unsigned int *pLoading);
bool mtk_get_gpu_loading2(unsigned int *pLoading, int reset);
bool mtk_get_gpu_block(unsigned int *pBlock);
bool mtk_get_gpu_idle(unsigned int *pIlde);
bool mtk_get_gpu_freq(unsigned int *pFreq);

bool mtk_get_gpu_GP_loading(unsigned int *pLoading);
bool mtk_get_gpu_PP_loading(unsigned int *pLoading);
bool mtk_get_gpu_power_loading(unsigned int *pLoading);

bool mtk_enable_gpu_dvfs_timer(bool bEnable);
bool mtk_boost_gpu_freq(void);
bool mtk_set_bottom_gpu_freq(unsigned int ui32FreqLevel);

/* ui32FreqLevel: 0=>lowest freq, count-1=>highest freq */
bool mtk_custom_get_gpu_freq_level_count(unsigned int *pui32FreqLevelCount);
bool mtk_custom_boost_gpu_freq(unsigned int ui32FreqLevel);
bool mtk_custom_upbound_gpu_freq(unsigned int ui32FreqLevel);
bool mtk_get_custom_boost_gpu_freq(unsigned int *pui32FreqLevel);
bool mtk_get_custom_upbound_gpu_freq(unsigned int *pui32FreqLevel);

bool mtk_dump_gpu_memory_usage(void);

bool mtk_get_gpu_dvfs_from(MTK_GPU_DVFS_TYPE *peType, unsigned long *pulFreq);
bool mtk_get_3D_fences_count(int *pi32Count);
bool mtk_get_vsync_based_target_freq(unsigned long *pulFreq);
bool mtk_get_gpu_sub_loading(unsigned int *pLoading);
bool mtk_get_gpu_bottom_freq(unsigned long *pulFreq);
bool mtk_get_gpu_custom_boost_freq(unsigned long *pulFreq);
bool mtk_get_gpu_custom_upbound_freq(unsigned long *pulFreq);
bool mtk_get_vsync_offset_event_status(unsigned int *pui32EventStatus);
bool mtk_get_vsync_offset_debug_status(unsigned int *pui32DebugStatus);

/* MET */
bool mtk_enable_gpu_perf_monitor(bool enable);

/* GPU PMU should be implemented by GPU IP-dep code */

typedef struct {
	int id;
	const char *name;
	unsigned int value;
	int overflow;
} GPU_PMU;

bool mtk_get_gpu_pmu_init(GPU_PMU *pmus, int pmu_size, int *ret_size);
bool mtk_get_gpu_pmu_swapnreset(GPU_PMU *pmus, int pmu_size);

typedef void (*gpu_power_change_notify_fp)(int power_on);

bool mtk_register_gpu_power_change(const char *name,
gpu_power_change_notify_fp callback);
bool mtk_unregister_gpu_power_change(const char *name);

/* GPU POWER NOTIFY should be called by GPU only */
void mtk_notify_gpu_power_change(int power_on);

/* ftrace */
bool mtk_gpu_systrace_c(pid_t pid, int val, const char *fmt, ...);

/*  gpu  info */
bool mtk_get_gpuinfo(struct gpu_info *info);

#ifdef __cplusplus
}
#endif

#endif
