// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/mutex.h>

#include <mt-plat/mtk_gpu_utility.h>
#include <mtk_gpufreq.h>

unsigned int (*mtk_get_gpu_memory_usage_fp)(void) = NULL;
static int gpu_pmu_flag;
EXPORT_SYMBOL(mtk_get_gpu_memory_usage_fp);

bool mtk_get_gpu_memory_usage(unsigned int *pMemUsage)
{
	if (mtk_get_gpu_memory_usage_fp != NULL) {
		if (pMemUsage) {
			*pMemUsage = mtk_get_gpu_memory_usage_fp();
			return true;
		}
	}
	return false;
}
EXPORT_SYMBOL(mtk_get_gpu_memory_usage);

unsigned int (*mtk_get_gpu_loading_fp)(void) = NULL;
EXPORT_SYMBOL(mtk_get_gpu_loading_fp);

bool mtk_get_gpu_loading(unsigned int *pLoading)
{
	if (mtk_get_gpu_loading_fp != NULL) {
		if (pLoading) {
			*pLoading = mtk_get_gpu_loading_fp();
			return true;
		}
	}
	return false;
}
EXPORT_SYMBOL(mtk_get_gpu_loading);

unsigned int (*mtk_get_gpu_block_fp)(void) = NULL;
EXPORT_SYMBOL(mtk_get_gpu_block_fp);

bool mtk_get_gpu_block(unsigned int *pBlock)
{
	if (mtk_get_gpu_block_fp != NULL) {
		if (pBlock) {
			*pBlock = mtk_get_gpu_block_fp();
			return true;
		}
	}
	return false;
}
EXPORT_SYMBOL(mtk_get_gpu_block);

unsigned int (*mtk_get_gpu_idle_fp)(void) = NULL;
EXPORT_SYMBOL(mtk_get_gpu_idle_fp);

bool mtk_get_gpu_idle(unsigned int *pIdle)
{
	if (mtk_get_gpu_idle_fp != NULL) {
		if (pIdle) {
			*pIdle = mtk_get_gpu_idle_fp();
			return true;
		}
	}
	return false;
}
EXPORT_SYMBOL(mtk_get_gpu_idle);

void (*mtk_set_bottom_gpu_freq_fp)(unsigned int) = NULL;
EXPORT_SYMBOL(mtk_set_bottom_gpu_freq_fp);

bool mtk_set_bottom_gpu_freq(unsigned int ui32FreqLevel)
{
	if (mtk_set_bottom_gpu_freq_fp != NULL) {
		mtk_set_bottom_gpu_freq_fp(ui32FreqLevel);
		return true;
	}
	return false;
}
EXPORT_SYMBOL(mtk_set_bottom_gpu_freq);

/* -------------------------------------------------------------------------- */
unsigned int (*mtk_get_bottom_gpu_freq_fp)(void) = NULL;
EXPORT_SYMBOL(mtk_get_bottom_gpu_freq_fp);

bool mtk_get_bottom_gpu_freq(unsigned int *pui32FreqLevel)
{
	if ((mtk_get_bottom_gpu_freq_fp != NULL) && (pui32FreqLevel)) {
		*pui32FreqLevel = mtk_get_bottom_gpu_freq_fp();
		return true;
	}
	return false;
}
EXPORT_SYMBOL(mtk_get_bottom_gpu_freq);
/* -------------------------------------------------------------------------- */
unsigned int (*mtk_custom_get_gpu_freq_level_count_fp)(void) = NULL;
EXPORT_SYMBOL(mtk_custom_get_gpu_freq_level_count_fp);

bool mtk_custom_get_gpu_freq_level_count(unsigned int *pui32FreqLevelCount)
{
	if (mtk_custom_get_gpu_freq_level_count_fp != NULL) {
		if (pui32FreqLevelCount) {
			*pui32FreqLevelCount =
				mtk_custom_get_gpu_freq_level_count_fp();
			return true;
		}
	}
	return false;
}
EXPORT_SYMBOL(mtk_custom_get_gpu_freq_level_count);

/* -------------------------------------------------------------------------- */

void (*mtk_custom_boost_gpu_freq_fp)(unsigned int ui32FreqLevel) = NULL;
EXPORT_SYMBOL(mtk_custom_boost_gpu_freq_fp);

bool mtk_custom_boost_gpu_freq(unsigned int ui32FreqLevel)
{
	if (mtk_custom_boost_gpu_freq_fp != NULL) {
		mtk_custom_boost_gpu_freq_fp(ui32FreqLevel);
		return true;
	}
	return false;
}
EXPORT_SYMBOL(mtk_custom_boost_gpu_freq);

/* -------------------------------------------------------------------------- */

void (*mtk_custom_upbound_gpu_freq_fp)(unsigned int ui32FreqLevel) = NULL;
EXPORT_SYMBOL(mtk_custom_upbound_gpu_freq_fp);

bool mtk_custom_upbound_gpu_freq(unsigned int ui32FreqLevel)
{
	if (mtk_custom_upbound_gpu_freq_fp != NULL) {
		mtk_custom_upbound_gpu_freq_fp(ui32FreqLevel);
		return true;
	}
	return false;
}
EXPORT_SYMBOL(mtk_custom_upbound_gpu_freq);

/* -------------------------------------------------------------------------- */

bool (*mtk_dump_gpu_memory_usage_fp)(void) = NULL;
EXPORT_SYMBOL(mtk_dump_gpu_memory_usage_fp);

bool mtk_dump_gpu_memory_usage(void)
{
	if (mtk_dump_gpu_memory_usage_fp != NULL) {
		mtk_dump_gpu_memory_usage_fp();
		return true;
	}
	return false;
}
EXPORT_SYMBOL(mtk_dump_gpu_memory_usage);

/* -------------------------------------------------------------------------- */

unsigned long (*mtk_get_gpu_bottom_freq_fp)(void) = NULL;
EXPORT_SYMBOL(mtk_get_gpu_bottom_freq_fp);

bool mtk_get_gpu_bottom_freq(unsigned long *pulFreq)
{
	if (mtk_get_gpu_bottom_freq_fp != NULL) {
		if (pulFreq) {
			*pulFreq = mtk_get_gpu_bottom_freq_fp();
			return true;
		}
	}
	return false;
}
EXPORT_SYMBOL(mtk_get_gpu_bottom_freq);

/* -------------------------------------------------------------------------- */

#if defined(CONFIG_MTK_GPUFREQ_V2)
int (*mtk_get_gpu_limit_index_fp)(enum gpufreq_target target,
	enum gpuppm_limit_type limit) = NULL;
EXPORT_SYMBOL(mtk_get_gpu_limit_index_fp);

bool mtk_get_gpu_floor_index(int *piIndex)
{
	if (mtk_get_gpu_limit_index_fp != NULL) {
		if (piIndex) {
			*piIndex = mtk_get_gpu_limit_index_fp(
				TARGET_DEFAULT, GPUPPM_FLOOR);
			return true;
		}
	}
	return false;
}
EXPORT_SYMBOL(mtk_get_gpu_floor_index);

bool mtk_get_gpu_ceiling_index(int *piIndex)
{
	if (mtk_get_gpu_limit_index_fp != NULL) {
		if (piIndex) {
			*piIndex = mtk_get_gpu_limit_index_fp(
				TARGET_DEFAULT, GPUPPM_CEILING);
			return true;
		}
	}
	return false;
}
EXPORT_SYMBOL(mtk_get_gpu_ceiling_index);

/* -------------------------------------------------------------------------- */

unsigned int (*mtk_get_gpu_limiter_fp)(enum gpufreq_target target,
	enum gpuppm_limit_type limit) = NULL;
EXPORT_SYMBOL(mtk_get_gpu_limiter_fp);

bool mtk_get_gpu_floor_limiter(unsigned int *puiLimiter)
{
	if (mtk_get_gpu_limiter_fp != NULL) {
		if (puiLimiter) {
			*puiLimiter = mtk_get_gpu_limiter_fp(
				TARGET_DEFAULT, GPUPPM_FLOOR);
			return true;
		}
	}
	return false;
}
EXPORT_SYMBOL(mtk_get_gpu_floor_limiter);

bool mtk_get_gpu_ceiling_limiter(unsigned int *puiLimiter)
{
	if (mtk_get_gpu_limiter_fp != NULL) {
		if (puiLimiter) {
			*puiLimiter = mtk_get_gpu_limiter_fp(
				TARGET_DEFAULT, GPUPPM_CEILING);
			return true;
		}
	}
	return false;
}
EXPORT_SYMBOL(mtk_get_gpu_ceiling_limiter);

/* -------------------------------------------------------------------------- */

unsigned int (*mtk_get_gpu_cur_freq_fp)(enum gpufreq_target target) = NULL;
EXPORT_SYMBOL(mtk_get_gpu_cur_freq_fp);

bool mtk_get_gpu_cur_freq(unsigned int *puiFreq)
{
	if (mtk_get_gpu_cur_freq_fp != NULL) {
		if (puiFreq) {
			*puiFreq = mtk_get_gpu_cur_freq_fp(TARGET_DEFAULT);
			return true;
		}
	}
	return false;
}
EXPORT_SYMBOL(mtk_get_gpu_cur_freq);

/* -------------------------------------------------------------------------- */

int (*mtk_get_gpu_cur_oppidx_fp)(enum gpufreq_target target) = NULL;
EXPORT_SYMBOL(mtk_get_gpu_cur_oppidx_fp);

bool mtk_get_gpu_cur_oppidx(int *piIndex)
{
	if (mtk_get_gpu_cur_oppidx_fp != NULL) {
		if (piIndex) {
			*piIndex = mtk_get_gpu_cur_oppidx_fp(TARGET_DEFAULT);
			return true;
		}
	}
	return false;
}
EXPORT_SYMBOL(mtk_get_gpu_cur_oppidx);

/* -------------------------------------------------------------------------- */

#else /* CONFIG_MTK_GPUFREQ_V2 */

bool mtk_get_gpu_floor_index(int *piIndex)
{
	return false;
}
EXPORT_SYMBOL(mtk_get_gpu_floor_index);

bool mtk_get_gpu_ceiling_index(int *piIndex)
{
	return false;
}
EXPORT_SYMBOL(mtk_get_gpu_ceiling_index);

bool mtk_get_gpu_floor_limiter(unsigned int *puiLimiter)
{
	return false;
}
EXPORT_SYMBOL(mtk_get_gpu_floor_limiter);

bool mtk_get_gpu_ceiling_limiter(unsigned int *puiLimiter)
{
	return false;
}
EXPORT_SYMBOL(mtk_get_gpu_ceiling_limiter);

bool mtk_get_gpu_cur_freq(unsigned int *puiFreq)
{
	return false;
}
EXPORT_SYMBOL(mtk_get_gpu_cur_freq);

bool mtk_get_gpu_cur_oppidx(int *piIndex)
{
	return false;
}
EXPORT_SYMBOL(mtk_get_gpu_cur_oppidx);
#endif /* CONFIG_MTK_GPUFREQ_V2 */

/* -------------------------------------------------------------------------- */

int (*mtk_get_gpu_pmu_init_fp)(struct GPU_PMU *pmus, int pmuSize, int *retSize);
EXPORT_SYMBOL(mtk_get_gpu_pmu_init_fp);

bool mtk_get_gpu_pmu_init(struct GPU_PMU *pmus, int pmu_size, int *ret_size)
{
	if (mtk_get_gpu_pmu_init_fp != NULL)
		return mtk_get_gpu_pmu_init_fp(pmus, pmu_size, ret_size) == 0;
	return false;
}
EXPORT_SYMBOL(mtk_get_gpu_pmu_init);

/* -------------------------------------------------------------------------- */

int (*mtk_get_gpu_pmu_swapnreset_fp)(struct GPU_PMU *pmus, int pmu_size);
EXPORT_SYMBOL(mtk_get_gpu_pmu_swapnreset_fp);

bool mtk_get_gpu_pmu_swapnreset(struct GPU_PMU *pmus, int pmu_size)
{
	if (mtk_get_gpu_pmu_swapnreset_fp != NULL) {
		gpu_pmu_flag = 1;
		return mtk_get_gpu_pmu_swapnreset_fp(pmus, pmu_size) == 0;
	}
	return false;
}
EXPORT_SYMBOL(mtk_get_gpu_pmu_swapnreset);

/* -------------------------------------------------------------------------- */

struct sGpuPowerChangeEntry {
	void (*callback)(int power_on);
	char name[128];
	struct list_head sList;
};

static struct {
	struct mutex lock;
	struct list_head listen;
} g_power_change = {
	.lock     = __MUTEX_INITIALIZER(g_power_change.lock),
	.listen   = LIST_HEAD_INIT(g_power_change.listen),
};

bool mtk_register_gpu_power_change(const char *name,
	void (*callback)(int power_on))
{
	struct sGpuPowerChangeEntry *entry = NULL;

	entry = kmalloc(sizeof(struct sGpuPowerChangeEntry), GFP_KERNEL);
	if (entry == NULL)
		return false;

	entry->callback = callback;
	strncpy(entry->name, name, sizeof(entry->name) - 1);
	entry->name[sizeof(entry->name) - 1] = 0;
	INIT_LIST_HEAD(&entry->sList);

	mutex_lock(&g_power_change.lock);

	list_add(&entry->sList, &g_power_change.listen);

	mutex_unlock(&g_power_change.lock);

	return true;
}
EXPORT_SYMBOL(mtk_register_gpu_power_change);

bool mtk_unregister_gpu_power_change(const char *name)
{
	struct list_head *pos, *head;
	struct sGpuPowerChangeEntry *entry = NULL;

	mutex_lock(&g_power_change.lock);

	head = &g_power_change.listen;
	list_for_each(pos, head) {
		entry = list_entry(pos, struct sGpuPowerChangeEntry, sList);
		if (strncmp(entry->name, name, sizeof(entry->name) - 1) == 0)
			break;
		entry = NULL;
	}

	if (entry) {
		list_del(&entry->sList);
		kfree(entry);
	}

	mutex_unlock(&g_power_change.lock);

	return true;
}
EXPORT_SYMBOL(mtk_unregister_gpu_power_change);

void mtk_notify_gpu_power_change(int power_on)
{
	struct list_head *pos, *head;
	struct sGpuPowerChangeEntry *entry = NULL;
	if (!gpu_pmu_flag) {
		mutex_lock(&g_power_change.lock);

		head = &g_power_change.listen;
		list_for_each(pos, head) {
			entry = list_entry(pos,
				struct sGpuPowerChangeEntry,
				sList);
			entry->callback(power_on);
		}

		mutex_unlock(&g_power_change.lock);
	}
}
EXPORT_SYMBOL(mtk_notify_gpu_power_change);

int (*mtk_get_gpu_pmu_deinit_fp)(void);
EXPORT_SYMBOL(mtk_get_gpu_pmu_deinit_fp);

bool mtk_get_gpu_pmu_deinit(void)
{
	if (mtk_get_gpu_pmu_deinit_fp != NULL)
		return mtk_get_gpu_pmu_deinit_fp() == 0;
	return false;
}
EXPORT_SYMBOL(mtk_get_gpu_pmu_deinit);

int (*mtk_get_gpu_pmu_swapnreset_stop_fp)(void);
EXPORT_SYMBOL(mtk_get_gpu_pmu_swapnreset_stop_fp);

bool mtk_get_gpu_pmu_swapnreset_stop(void)
{
	if (mtk_get_gpu_pmu_swapnreset_stop_fp != NULL) {
		gpu_pmu_flag = 0;
		return mtk_get_gpu_pmu_swapnreset_stop_fp() == 0;
	}
	return false;
}
EXPORT_SYMBOL(mtk_get_gpu_pmu_swapnreset_stop);

/* ------------------------------------------------------------------------ */
void (*mtk_dvfs_margin_value_fp)(int i32MarginValue) = NULL;
EXPORT_SYMBOL(mtk_dvfs_margin_value_fp);

bool mtk_dvfs_margin_value(int i32MarginValue)
{
	if (mtk_dvfs_margin_value_fp != NULL) {
		mtk_dvfs_margin_value_fp(i32MarginValue);
		return true;
	}
	return false;
}
EXPORT_SYMBOL(mtk_dvfs_margin_value);

int (*mtk_get_dvfs_margin_value_fp)(void) = NULL;
EXPORT_SYMBOL(mtk_get_dvfs_margin_value_fp);

bool mtk_get_dvfs_margin_value(int *pi32MarginValue)
{
	if ((mtk_get_dvfs_margin_value_fp != NULL) &&
		(pi32MarginValue != NULL)) {

		*pi32MarginValue = mtk_get_dvfs_margin_value_fp();
		return true;
	}
	return false;
}
EXPORT_SYMBOL(mtk_get_dvfs_margin_value);

/* -------------------------------------------------------------------------*/
void (*mtk_loading_base_dvfs_step_fp)(int i32StepValue) = NULL;
EXPORT_SYMBOL(mtk_loading_base_dvfs_step_fp);

bool mtk_loading_base_dvfs_step(int i32StepValue)
{
	if (mtk_loading_base_dvfs_step_fp != NULL) {
		mtk_loading_base_dvfs_step_fp(i32StepValue);
		return true;
	}
	return false;
}
EXPORT_SYMBOL(mtk_loading_base_dvfs_step);

int (*mtk_get_loading_base_dvfs_step_fp)(void) = NULL;
EXPORT_SYMBOL(mtk_get_loading_base_dvfs_step_fp);

bool mtk_get_loading_base_dvfs_step(int *pi32StepValue)
{
	if ((mtk_get_loading_base_dvfs_step_fp != NULL) &&
		(pi32StepValue != NULL)) {
		*pi32StepValue = mtk_get_loading_base_dvfs_step_fp();
		return true;
	}
	return false;
}
EXPORT_SYMBOL(mtk_get_loading_base_dvfs_step);
/* ------------------------------------------------------------------------ */
void (*mtk_timer_base_dvfs_margin_fp)(int i32MarginValue) = NULL;
EXPORT_SYMBOL(mtk_timer_base_dvfs_margin_fp);

bool mtk_timer_base_dvfs_margin(int i32MarginValue)
{
	if (mtk_timer_base_dvfs_margin_fp != NULL) {
		mtk_timer_base_dvfs_margin_fp(i32MarginValue);
		return true;
	}
	return false;
}
EXPORT_SYMBOL(mtk_timer_base_dvfs_margin);

int (*mtk_get_timer_base_dvfs_margin_fp)(void) = NULL;
EXPORT_SYMBOL(mtk_get_timer_base_dvfs_margin_fp);

bool mtk_get_timer_base_dvfs_margin(int *pi32MarginValue)
{
	if ((mtk_get_timer_base_dvfs_margin_fp != NULL) &&
		(pi32MarginValue != NULL)) {

		*pi32MarginValue = mtk_get_timer_base_dvfs_margin_fp();
		return true;
	}
	return false;
}
EXPORT_SYMBOL(mtk_get_timer_base_dvfs_margin);
/* ------------------------------------------------------------------------ */
void (*mtk_dvfs_loading_mode_fp)(int i32LoadingMode) = NULL;
EXPORT_SYMBOL(mtk_dvfs_loading_mode_fp);

bool mtk_dvfs_loading_mode(int i32LoadingMode)
{
	if (mtk_dvfs_loading_mode_fp != NULL) {
		mtk_dvfs_loading_mode_fp(i32LoadingMode);
		return true;
	}
	return false;
}
EXPORT_SYMBOL(mtk_dvfs_loading_mode);

int (*mtk_get_dvfs_loading_mode_fp)(void) = NULL;
EXPORT_SYMBOL(mtk_get_dvfs_loading_mode_fp);

bool mtk_get_dvfs_loading_mode(unsigned int *pui32LoadingMode)
{
	if ((mtk_get_dvfs_loading_mode_fp != NULL) &&
		(pui32LoadingMode != NULL)) {

		*pui32LoadingMode = mtk_get_dvfs_loading_mode_fp();
		return true;
	}
	return false;
}
EXPORT_SYMBOL(mtk_get_dvfs_loading_mode);
/* ------------------------------------------------------------------------ */
void (*mtk_dvfs_workload_mode_fp)(int i32WorkloadMode) = NULL;
EXPORT_SYMBOL(mtk_dvfs_workload_mode_fp);

bool mtk_dvfs_workload_mode(int i32WorkloadMode)
{
	if (mtk_dvfs_workload_mode_fp != NULL) {
		mtk_dvfs_workload_mode_fp(i32WorkloadMode);
		return true;
	}
	return false;
}
EXPORT_SYMBOL(mtk_dvfs_workload_mode);

int (*mtk_get_dvfs_workload_mode_fp)(void) = NULL;
EXPORT_SYMBOL(mtk_get_dvfs_workload_mode_fp);

bool mtk_get_dvfs_workload_mode(unsigned int *pui32WorkloadMode)
{
	if ((mtk_get_dvfs_workload_mode_fp != NULL) &&
		(pui32WorkloadMode != NULL)) {

		*pui32WorkloadMode = mtk_get_dvfs_workload_mode_fp();
		return true;
	}
	return false;
}
EXPORT_SYMBOL(mtk_get_dvfs_workload_mode);
/* ------------------------------------------------------------------------ */
void (*mtk_set_fastdvfs_mode_fp)(unsigned int u32Mode) = NULL;
EXPORT_SYMBOL(mtk_set_fastdvfs_mode_fp);

bool mtk_set_fastdvfs_mode(unsigned int u32Mode)
{
	if (mtk_set_fastdvfs_mode_fp != NULL) {
		mtk_set_fastdvfs_mode_fp(u32Mode);
		return true;
	}
	return false;
}
EXPORT_SYMBOL(mtk_set_fastdvfs_mode);

unsigned int (*mtk_get_fastdvfs_mode_fp)(void) = NULL;
EXPORT_SYMBOL(mtk_get_fastdvfs_mode_fp);

bool mtk_get_fastdvfs_mode(unsigned int *pui32Mode)
{
	if (pui32Mode == NULL)
		return false;

	if ((mtk_get_fastdvfs_mode_fp != NULL) &&
		(pui32Mode != NULL)) {

		*pui32Mode = mtk_get_fastdvfs_mode_fp();
		return true;
	}

	*pui32Mode = 0;
	return false;
}
EXPORT_SYMBOL(mtk_get_fastdvfs_mode);

/* -----------------------------gpu pmu fp--------------------------------- */
void (*mtk_ltr_gpu_pmu_start_fp)(unsigned int interval_ns) = NULL;
EXPORT_SYMBOL(mtk_ltr_gpu_pmu_start_fp);

bool mtk_ltr_gpu_pmu_start(unsigned int interval_ns)
{
	if (mtk_ltr_gpu_pmu_start_fp != NULL) {
		mtk_ltr_gpu_pmu_start_fp(interval_ns);
		return true;
	}
	return false;

}
EXPORT_SYMBOL(mtk_ltr_gpu_pmu_start);

void (*mtk_swpm_gpu_pm_start_fp)(void) = NULL;
EXPORT_SYMBOL(mtk_swpm_gpu_pm_start_fp);

bool mtk_swpm_gpu_pm_start(void)
{
	if (mtk_swpm_gpu_pm_start_fp != NULL) {
		mtk_swpm_gpu_pm_start_fp();
		return true;
	}
	return false;

}
EXPORT_SYMBOL(mtk_swpm_gpu_pm_start);

void (*mtk_ltr_gpu_pmu_stop_fp)(void) = NULL;
EXPORT_SYMBOL(mtk_ltr_gpu_pmu_stop_fp);

bool mtk_ltr_gpu_pmu_stop(void)
{
	if (mtk_ltr_gpu_pmu_stop_fp != NULL) {
		mtk_ltr_gpu_pmu_stop_fp();
		return true;
	}
	return false;

}
EXPORT_SYMBOL(mtk_ltr_gpu_pmu_stop);

/* -----------------------------set gpu idle time--------------------------------- */

void (*mtk_set_gpu_idle_fp)(unsigned int val) = NULL;
EXPORT_SYMBOL(mtk_set_gpu_idle_fp);

bool mtk_set_gpu_idle(unsigned int val)
{
	if (mtk_set_gpu_idle_fp != NULL) {
		mtk_set_gpu_idle_fp(val);
		return true;
	}
	return false;

}
EXPORT_SYMBOL(mtk_set_gpu_idle);


/* ----------------------gpufreq change notify fp-------------------------- */
void (*mtk_notify_gpu_freq_change_fp)(u32 clk_idx, u32 gpufreq) = NULL;
EXPORT_SYMBOL(mtk_notify_gpu_freq_change_fp);

bool mtk_notify_gpu_freq_change(u32 clk_idx, u32 gpufreq)
{
	if (mtk_notify_gpu_freq_change_fp != NULL) {
		mtk_notify_gpu_freq_change_fp(clk_idx, gpufreq);
		return true;
	}
	return false;
}
EXPORT_SYMBOL(mtk_notify_gpu_freq_change);

/* ------------------------------------------------------------------------ */

/* ----------------------gpu fence debug fp-------------------------- */
void (*mtk_gpu_fence_debug_dump_fp)(int fd, int pid, int type, int timeouts) = NULL;
EXPORT_SYMBOL(mtk_gpu_fence_debug_dump_fp);

void mtk_gpu_fence_debug_dump(int fd, int pid, int type, int timeouts)
{
	if (mtk_gpu_fence_debug_dump_fp != NULL)
		mtk_gpu_fence_debug_dump_fp(fd, pid, type, timeouts);
}
EXPORT_SYMBOL(mtk_gpu_fence_debug_dump);

static int mtk_gpu_hal_init(void)
{
	/*Do Nothing*/
	return 0;
}

static void mtk_gpu_hal_exit(void)
{
	/*Do Nothing*/
	;
}

arch_initcall(mtk_gpu_hal_init);
module_exit(mtk_gpu_hal_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek GPU HAL");
MODULE_AUTHOR("MediaTek Inc.");

