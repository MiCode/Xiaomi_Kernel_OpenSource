/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#ifndef __MTK_GPU_UTILITY_H__
#define __MTK_GPU_UTILITY_H__

#include <linux/types.h>

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

enum MTK_GPU_DVFS_TYPE
#define MTK_GPU_DVFS_TYPE_ITEM(type) MTK_GPU_DVFS_TYPE_##type,
MTK_GPU_DVFS_TYPE_LIST
#undef MTK_GPU_DVFS_TYPE_ITEM
;



#define GT_MAKE_BIT(start_bit, index) ((index##u) << (start_bit))
enum GPU_TUNER_FEATURE {
	MTK_GPU_TUNER_ANISOTROPIC_DISABLE = GT_MAKE_BIT(0, 1),
	MTK_GPU_TUNER_TRILINEAR_DISABLE = GT_MAKE_BIT(1, 1),
};


#ifdef __cplusplus
extern "C"
{
#endif

/* returning false indicated no implement */

/* unit: x bytes */
bool mtk_get_gpu_memory_usage(unsigned int *pMemUsage);

/* unit: 0~100 % */
bool mtk_get_gpu_loading(unsigned int *pLoading);
bool mtk_get_gpu_block(unsigned int *pBlock);
bool mtk_get_gpu_idle(unsigned int *pIlde);
bool mtk_set_bottom_gpu_freq(unsigned int ui32FreqLevel);

/* ui32FreqLevel: 0=>lowest freq, count-1=>highest freq */
bool mtk_custom_get_gpu_freq_level_count(unsigned int *pui32FreqLevelCount);
bool mtk_custom_boost_gpu_freq(unsigned int ui32FreqLevel);
bool mtk_custom_upbound_gpu_freq(unsigned int ui32FreqLevel);

bool mtk_dump_gpu_memory_usage(void);

bool mtk_get_gpu_bottom_freq(unsigned long *pulFreq);

/* unit: OPP index [0 : OPP_NUM-1] */
bool mtk_get_gpu_floor_index(int *piIndex);
bool mtk_get_gpu_ceiling_index(int *piIndex);
/* unit: limiter [0 : LIMIT_NUM-1] */
bool mtk_get_gpu_floor_limiter(unsigned int *puiLimiter);
bool mtk_get_gpu_ceiling_limiter(unsigned int *puiLimiter);
/* unit: frequency (MHz) */
bool mtk_get_gpu_cur_freq(unsigned int *puiFreq);
/* unit: OPP index [0 : OPP_NUM-1] */
bool mtk_get_gpu_cur_oppidx(int *piIndex);

bool mtk_dvfs_margin_value(int i32MarginValue);
bool mtk_get_dvfs_margin_value(int *pi32MarginValue);
bool mtk_loading_base_dvfs_step(int i32StepValue);
bool mtk_get_loading_base_dvfs_step(int *pi32StepValue);
bool mtk_timer_base_dvfs_margin(int i32MarginValue);
bool mtk_get_timer_base_dvfs_margin(int *pi32MaginValue);
bool mtk_dvfs_loading_mode(int i32LoadingMode);
bool mtk_get_dvfs_loading_mode(unsigned int *pui32LoadingMode);
bool mtk_dvfs_workload_mode(int i32WorkloadMode);
bool mtk_get_dvfs_workload_mode(unsigned int *pui32WorkloadMode);
bool mtk_set_fastdvfs_mode(unsigned int u32Mode);
bool mtk_get_fastdvfs_mode(unsigned int *pui32Mode);
bool mtk_set_gpu_idle(unsigned int val);


/* GPU PMU should be implemented by GPU IP-dep code */
struct GPU_PMU {
	int id;
	const char *name;
	unsigned int value;
	int overflow;
};
bool mtk_get_gpu_pmu_init(struct GPU_PMU *pmus, int pmu_size, int *ret_size);
bool mtk_get_gpu_pmu_swapnreset(struct GPU_PMU *pmus, int pmu_size);
bool mtk_get_gpu_pmu_deinit(void);
bool mtk_get_gpu_pmu_swapnreset_stop(void);


bool mtk_register_gpu_power_change(const char *name,
	void (*callback)(int power_on));
bool mtk_unregister_gpu_power_change(const char *name);

/* GPU POWER NOTIFY should be called by GPU only */
void mtk_notify_gpu_power_change(int power_on);

bool mtk_notify_gpu_freq_change(u32 clk_idx, u32 gpufreq);

void mtk_gpu_fence_debug_dump(int fd, int pid, int type, int timeouts);
#ifdef __cplusplus
}
#endif

#endif
