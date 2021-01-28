// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/mutex.h>

#include <mt-plat/mtk_gpu_utility.h>

#ifdef CONFIG_MTK_GED_SUPPORT
#include "ged_gpu_tuner.h"
#endif

unsigned int (*mtk_get_gpu_memory_usage_fp)(void) = NULL;
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

unsigned int (*mtk_get_gpu_page_cache_fp)(void) = NULL;
EXPORT_SYMBOL(mtk_get_gpu_page_cache_fp);

bool mtk_get_gpu_page_cache(unsigned int *pPageCache)
{
	if (mtk_get_gpu_page_cache_fp != NULL) {
		if (pPageCache) {
			*pPageCache = mtk_get_gpu_page_cache_fp();
			return true;
		}
	}
	return false;
}
EXPORT_SYMBOL(mtk_get_gpu_page_cache);

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

unsigned int (*mtk_get_gpu_loading2_fp)(int) = NULL;
EXPORT_SYMBOL(mtk_get_gpu_loading2_fp);

bool mtk_get_gpu_loading2(unsigned int *pLoading, int reset)
{
	if (mtk_get_gpu_loading2_fp != NULL) {
		if (pLoading) {
			*pLoading = mtk_get_gpu_loading2_fp(reset);
			return true;
		}
	}
	return false;
}
EXPORT_SYMBOL(mtk_get_gpu_loading2);

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

unsigned int (*mtk_get_gpu_freq_fp)(void) = NULL;
EXPORT_SYMBOL(mtk_get_gpu_freq_fp);

bool mtk_get_gpu_freq(unsigned int *pFreq)
{
	if (mtk_get_gpu_freq_fp != NULL) {
		if (pFreq) {
			*pFreq = mtk_get_gpu_freq_fp();
			return true;
		}
	}
	return false;
}
EXPORT_SYMBOL(mtk_get_gpu_freq);

unsigned int (*mtk_get_gpu_GP_loading_fp)(void) = NULL;
EXPORT_SYMBOL(mtk_get_gpu_GP_loading_fp);

bool mtk_get_gpu_GP_loading(unsigned int *pLoading)
{
	if (mtk_get_gpu_GP_loading_fp != NULL) {
		if (pLoading) {
			*pLoading = mtk_get_gpu_GP_loading_fp();
			return true;
		}
	}
	return false;
}
EXPORT_SYMBOL(mtk_get_gpu_GP_loading);

unsigned int (*mtk_get_gpu_PP_loading_fp)(void) = NULL;
EXPORT_SYMBOL(mtk_get_gpu_PP_loading_fp);

bool mtk_get_gpu_PP_loading(unsigned int *pLoading)
{
	if (mtk_get_gpu_PP_loading_fp != NULL) {
		if (pLoading) {
			*pLoading = mtk_get_gpu_PP_loading_fp();
			return true;
		}
	}
	return false;
}
EXPORT_SYMBOL(mtk_get_gpu_PP_loading);

unsigned int (*mtk_get_gpu_power_loading_fp)(void) = NULL;
EXPORT_SYMBOL(mtk_get_gpu_power_loading_fp);

bool mtk_get_gpu_power_loading(unsigned int *pLoading)
{
	if (mtk_get_gpu_power_loading_fp != NULL) {
		if (pLoading) {
			*pLoading = mtk_get_gpu_power_loading_fp();
			return true;
		}
	}
	return false;
}
EXPORT_SYMBOL(mtk_get_gpu_power_loading);

void (*mtk_enable_gpu_dvfs_timer_fp)(bool bEnable) = NULL;
EXPORT_SYMBOL(mtk_enable_gpu_dvfs_timer_fp);

bool mtk_enable_gpu_dvfs_timer(bool bEnable)
{
	if (mtk_enable_gpu_dvfs_timer_fp != NULL) {
		mtk_enable_gpu_dvfs_timer_fp(bEnable);
		return true;
	}
	return false;
}
EXPORT_SYMBOL(mtk_enable_gpu_dvfs_timer);


void (*mtk_boost_gpu_freq_fp)(void) = NULL;
EXPORT_SYMBOL(mtk_boost_gpu_freq_fp);

bool mtk_boost_gpu_freq(void)
{
	if (mtk_boost_gpu_freq_fp != NULL) {
		mtk_boost_gpu_freq_fp();
		return true;
	}
	return false;
}
EXPORT_SYMBOL(mtk_boost_gpu_freq);

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

//-----------------------------------------------------------------------------
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
//-----------------------------------------------------------------------------
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

//-----------------------------------------------------------------------------

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

//-----------------------------------------------------------------------------

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

//-----------------------------------------------------------------------------

unsigned int (*mtk_get_custom_boost_gpu_freq_fp)(void) = NULL;
EXPORT_SYMBOL(mtk_get_custom_boost_gpu_freq_fp);

bool mtk_get_custom_boost_gpu_freq(unsigned int *pui32FreqLevel)
{
	if ((mtk_get_custom_boost_gpu_freq_fp != NULL)
		&& (pui32FreqLevel != NULL)) {
		*pui32FreqLevel = mtk_get_custom_boost_gpu_freq_fp();
		return true;
	}
	return false;
}
EXPORT_SYMBOL(mtk_get_custom_boost_gpu_freq);

//-----------------------------------------------------------------------------

unsigned int (*mtk_get_custom_upbound_gpu_freq_fp)(void) = NULL;
EXPORT_SYMBOL(mtk_get_custom_upbound_gpu_freq_fp);

bool mtk_get_custom_upbound_gpu_freq(unsigned int *pui32FreqLevel)
{
	if ((mtk_get_custom_upbound_gpu_freq_fp) != NULL
		&& (pui32FreqLevel != NULL)) {
		*pui32FreqLevel = mtk_get_custom_upbound_gpu_freq_fp();
		return true;
	}
	return false;
}
EXPORT_SYMBOL(mtk_get_custom_upbound_gpu_freq);

//-----------------------------------------------------------------------------
void (*mtk_do_gpu_dvfs_fp)(unsigned long t, long p, unsigned long ulFDT) = NULL;
EXPORT_SYMBOL(mtk_do_gpu_dvfs_fp);

bool mtk_do_gpu_dvfs(unsigned long t, long phase,
	unsigned long ul3DFenceDoneTime)
{
	if (mtk_do_gpu_dvfs_fp != NULL) {
		mtk_do_gpu_dvfs_fp(t, phase, ul3DFenceDoneTime);
		return true;
	}
	return false;
}
EXPORT_SYMBOL(mtk_do_gpu_dvfs);

//-----------------------------------------------------------------------------

void  (*mtk_gpu_sodi_entry_fp)(void) = NULL;
EXPORT_SYMBOL(mtk_gpu_sodi_entry_fp);

bool mtk_gpu_sodi_entry(void)
{
	if (mtk_gpu_sodi_entry_fp != NULL) {
		mtk_gpu_sodi_entry_fp();
		return true;
	}
	return false;
}
EXPORT_SYMBOL(mtk_gpu_sodi_entry);

//-----------------------------------------------------------------------------

void  (*mtk_gpu_sodi_exit_fp)(void) = NULL;
EXPORT_SYMBOL(mtk_gpu_sodi_exit_fp);

bool mtk_gpu_sodi_exit(void)
{
	if (mtk_gpu_sodi_exit_fp != NULL) {
		mtk_gpu_sodi_exit_fp();
		return true;
	}
	return false;
}
EXPORT_SYMBOL(mtk_gpu_sodi_exit);


//-----------------------------------------------------------------------------

unsigned int (*mtk_get_sw_vsync_phase_fp)(void) = NULL;
EXPORT_SYMBOL(mtk_get_sw_vsync_phase_fp);

bool mtk_get_sw_vsync_phase(long *plPhase)
{
	if (mtk_get_sw_vsync_phase_fp != NULL) {
		if (plPhase) {
			*plPhase = mtk_get_sw_vsync_phase_fp();
			return true;
		}
	}
	return false;
}
EXPORT_SYMBOL(mtk_get_sw_vsync_phase);

//-----------------------------------------------------------------------------

unsigned int (*mtk_get_sw_vsync_time_fp)(void) = NULL;
EXPORT_SYMBOL(mtk_get_sw_vsync_time_fp);

bool mtk_get_sw_vsync_time(unsigned long *pulTime)
{
	if (mtk_get_sw_vsync_time_fp != NULL) {
		if (pulTime) {
			*pulTime = mtk_get_sw_vsync_time_fp();
			return true;
		}
	}
	return false;
}
EXPORT_SYMBOL(mtk_get_sw_vsync_time);

//-----------------------------------------------------------------------------

unsigned int (*mtk_get_gpu_fence_done_fp)(void) = NULL;
EXPORT_SYMBOL(mtk_get_gpu_fence_done_fp);

bool mtk_get_gpu_fence_done(unsigned long *pulTime)
{
	if (mtk_get_gpu_fence_done_fp != NULL) {
		if (pulTime) {
			*pulTime = mtk_get_gpu_fence_done_fp();
			return true;
		}
	}
	return false;
}
EXPORT_SYMBOL(mtk_get_gpu_fence_done);

//-----------------------------------------------------------------------------
void (*mtk_gpu_dvfs_set_mode_fp)(int eMode) = NULL;
EXPORT_SYMBOL(mtk_gpu_dvfs_set_mode_fp);

bool mtk_gpu_dvfs_set_mode(int eMode)
{
	if (mtk_gpu_dvfs_set_mode_fp != NULL) {
		mtk_gpu_dvfs_set_mode_fp(eMode);
		return true;
	}
	return false;
}
EXPORT_SYMBOL(mtk_gpu_dvfs_set_mode);

//-----------------------------------------------------------------------------
void (*mtk_dump_gpu_memory_usage_fp)(void) = NULL;
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


//-----------------------------------------------------------------------------
int (*mtk_get_gpu_power_state_fp)(void) = NULL;
EXPORT_SYMBOL(mtk_get_gpu_power_state_fp);

int mtk_get_gpu_power_state(void)
{
	if (mtk_get_gpu_power_state_fp != NULL)
		return mtk_get_gpu_power_state_fp();
	return -1;
}
EXPORT_SYMBOL(mtk_get_gpu_power_state);

//-----------------------------------------------------------------------------
void (*mtk_gpu_dvfs_clock_switch_fp)(bool bSwitch) = NULL;
EXPORT_SYMBOL(mtk_gpu_dvfs_clock_switch_fp);

bool mtk_gpu_dvfs_clock_switch(bool bSwitch)
{
	if (mtk_gpu_dvfs_clock_switch_fp != NULL) {
		mtk_gpu_dvfs_clock_switch_fp(bSwitch);
		return true;
	}
	return false;
}
EXPORT_SYMBOL(mtk_gpu_dvfs_clock_switch);

//-----------------------------------------------------------------------------
void (*mtk_GetGpuDVFSfromFp)(enum MTK_GPU_DVFS_TYPE *p, unsigned long *q) = 0;
EXPORT_SYMBOL(mtk_GetGpuDVFSfromFp);

bool mtk_get_gpu_dvfs_from(enum MTK_GPU_DVFS_TYPE *peType,
	unsigned long *pulFreq)
{
	if (mtk_GetGpuDVFSfromFp != NULL) {
		if (peType && pulFreq) {
			mtk_GetGpuDVFSfromFp(peType, pulFreq);
			return true;
		}
	}
	return false;
}
EXPORT_SYMBOL(mtk_get_gpu_dvfs_from);

//-----------------------------------------------------------------------------
bool mtk_get_3D_fences_count(int *pi32Count)
{
	if (pi32Count)
		return true;
	return false;
}
EXPORT_SYMBOL(mtk_get_3D_fences_count);

//-----------------------------------------------------------------------------
unsigned long (*mtk_get_vsync_based_target_freq_fp)(void) = NULL;
EXPORT_SYMBOL(mtk_get_vsync_based_target_freq_fp);

bool mtk_get_vsync_based_target_freq(unsigned long *pulFreq)
{
	if (mtk_get_vsync_based_target_freq_fp != NULL) {
		if (pulFreq) {
			*pulFreq = mtk_get_vsync_based_target_freq_fp();
			return true;
		}
	}
	return false;
}
EXPORT_SYMBOL(mtk_get_vsync_based_target_freq);

//-----------------------------------------------------------------------------

unsigned int (*mtk_get_gpu_sub_loading_fp)(void) = NULL;
EXPORT_SYMBOL(mtk_get_gpu_sub_loading_fp);

bool mtk_get_gpu_sub_loading(unsigned int *pLoading)
{
	if (mtk_get_gpu_sub_loading_fp != NULL) {
		if (pLoading) {
			*pLoading = mtk_get_gpu_sub_loading_fp();
			return true;
		}
	}
	return false;
}
EXPORT_SYMBOL(mtk_get_gpu_sub_loading);

//-----------------------------------------------------------------------------

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

//-----------------------------------------------------------------------------

unsigned long (*mtk_get_gpu_custom_boost_freq_fp)(void) = NULL;
EXPORT_SYMBOL(mtk_get_gpu_custom_boost_freq_fp);

bool mtk_get_gpu_custom_boost_freq(unsigned long *pulFreq)
{
	if (mtk_get_gpu_custom_boost_freq_fp != NULL) {
		if (pulFreq) {
			*pulFreq = mtk_get_gpu_custom_boost_freq_fp();
			return true;
		}
	}
	return false;
}
EXPORT_SYMBOL(mtk_get_gpu_custom_boost_freq);

//-----------------------------------------------------------------------------

unsigned long (*mtk_get_gpu_custom_upbound_freq_fp)(void) = NULL;
EXPORT_SYMBOL(mtk_get_gpu_custom_upbound_freq_fp);

bool mtk_get_gpu_custom_upbound_freq(unsigned long *pulFreq)
{
	if (mtk_get_gpu_custom_upbound_freq_fp != NULL) {
		if (pulFreq) {
			*pulFreq = mtk_get_gpu_custom_upbound_freq_fp();
			return true;
		}
	}
	return false;
}
EXPORT_SYMBOL(mtk_get_gpu_custom_upbound_freq);

//-----------------------------------------------------------------------------

unsigned int (*mtk_get_vsync_offset_event_status_fp)(void) = NULL;
EXPORT_SYMBOL(mtk_get_vsync_offset_event_status_fp);

bool mtk_get_vsync_offset_event_status(unsigned int *pui32EventStatus)
{
	if (mtk_get_vsync_offset_event_status_fp != NULL) {
		if (pui32EventStatus) {
			*pui32EventStatus =
				mtk_get_vsync_offset_event_status_fp();
			return true;
		}
	}
	return false;
}
EXPORT_SYMBOL(mtk_get_vsync_offset_event_status);

//-----------------------------------------------------------------------------

unsigned int (*mtk_get_vsync_offset_debug_status_fp)(void) = NULL;
EXPORT_SYMBOL(mtk_get_vsync_offset_debug_status_fp);

bool mtk_get_vsync_offset_debug_status(unsigned int *pui32DebugStatus)
{
	if (mtk_get_vsync_offset_debug_status_fp != NULL) {
		if (pui32DebugStatus) {
			*pui32DebugStatus =
				mtk_get_vsync_offset_debug_status_fp();
			return true;
		}
	}
	return false;
}
EXPORT_SYMBOL(mtk_get_vsync_offset_debug_status);

/* -------------------------------------------------------------------------- */
/*
 *	Get policy given targfet GPU freq in KHz
 */
unsigned int (*mtk_get_gpu_dvfs_cal_freq_fp)(void) = NULL;
EXPORT_SYMBOL(mtk_get_gpu_dvfs_cal_freq_fp);

bool mtk_get_gpu_dvfs_cal_freq(unsigned long *pulGpu_tar_freq)
{
	if (mtk_get_vsync_offset_debug_status_fp != NULL) {
		if (pulGpu_tar_freq) {
			*pulGpu_tar_freq = mtk_get_gpu_dvfs_cal_freq_fp();
			return true;
		}
	}
	return false;
}
EXPORT_SYMBOL(mtk_get_gpu_dvfs_cal_freq);


//-----------------------------------------------------------------------------

/*
 * Enable MFG performance monitor
 *
 * @brief
 * Enable MFG performance monitor for MET usage
 * Default MFG performance monitor is off
 * Each platform needs to implement corresponding function
 *
 * @param[in] enable: true for enable, false for disable
 * return 0 if change successfully or fail for other return values
 */

bool (*mtk_enable_gpu_perf_monitor_fp)(bool enable) = NULL;
EXPORT_SYMBOL(mtk_enable_gpu_perf_monitor_fp);

bool mtk_enable_gpu_perf_monitor(bool enable)
{
	if (mtk_enable_gpu_perf_monitor_fp != NULL)
		return mtk_enable_gpu_perf_monitor_fp(enable);

	return false;
}
EXPORT_SYMBOL(mtk_enable_gpu_perf_monitor);

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
	if (mtk_get_gpu_pmu_swapnreset_fp != NULL)
		return mtk_get_gpu_pmu_swapnreset_fp(pmus, pmu_size) == 0;
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

	mutex_lock(&g_power_change.lock);

	head = &g_power_change.listen;
	list_for_each(pos, head) {
		entry = list_entry(pos, struct sGpuPowerChangeEntry, sList);
		entry->callback(power_on);
	}

	mutex_unlock(&g_power_change.lock);
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
	if (mtk_get_gpu_pmu_swapnreset_stop_fp != NULL)
		return mtk_get_gpu_pmu_swapnreset_stop_fp() == 0;
	return false;
}
EXPORT_SYMBOL(mtk_get_gpu_pmu_swapnreset_stop);

#ifdef CONFIG_MTK_GED_SUPPORT

bool mtk_gpu_tuner_hint_set(char *packagename, enum GPU_TUNER_FEATURE eFeature)
{
	return ged_gpu_tuner_hint_set(packagename, eFeature);
}
EXPORT_SYMBOL(mtk_gpu_tuner_hint_set);

bool mtk_gpu_tuner_hint_restore(char *packagename,
	enum GPU_TUNER_FEATURE eFeature)
{
	return ged_gpu_tuner_hint_restore(packagename, eFeature);
}
EXPORT_SYMBOL(mtk_gpu_tuner_hint_restore);

bool mtk_gpu_tuner_get_stauts_by_packagename(char *packagename, int *feature)
{
	struct GED_GPU_TUNER_ITEM item;
	GED_ERROR err = ged_gpu_get_stauts_by_packagename(packagename, &item);

	if (err == GED_OK)
		*feature = item.status.feature;

	return err;
}
EXPORT_SYMBOL(mtk_gpu_tuner_get_stauts_by_packagename);

#endif

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

module_init(mtk_gpu_hal_init);
module_exit(mtk_gpu_hal_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek GPU HAL");
MODULE_AUTHOR("MediaTek Inc.");

