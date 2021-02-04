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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/mutex.h>

#include <mt-plat/mtk_gpu_utility.h>
#if 0
#include "ged_monitor_3D_fence.h"
#endif
#include "ged_gpu_tuner.h"

unsigned int (*mtk_get_gpu_memory_usage_fp)(void) = NULL;
EXPORT_SYMBOL(mtk_get_gpu_memory_usage_fp);

bool mtk_get_gpu_memory_usage(unsigned int* pMemUsage)
{
    if (NULL != mtk_get_gpu_memory_usage_fp)
    {
        if (pMemUsage)
        {
            *pMemUsage = mtk_get_gpu_memory_usage_fp();
            return true;
        }
    }
    return false;
}
EXPORT_SYMBOL(mtk_get_gpu_memory_usage);

unsigned int (*mtk_get_gpu_page_cache_fp)(void) = NULL;
EXPORT_SYMBOL(mtk_get_gpu_page_cache_fp);

bool mtk_get_gpu_page_cache(unsigned int* pPageCache)
{
    if (NULL != mtk_get_gpu_page_cache_fp)
    {
        if (pPageCache)
        {
            *pPageCache = mtk_get_gpu_page_cache_fp();
            return true;
        }
    }
    return false;
}
EXPORT_SYMBOL(mtk_get_gpu_page_cache);

unsigned int (*mtk_get_gpu_loading_fp)(void) = NULL;
EXPORT_SYMBOL(mtk_get_gpu_loading_fp);

bool mtk_get_gpu_loading(unsigned int* pLoading)
{
    if (NULL != mtk_get_gpu_loading_fp)
    {
        if (pLoading)
        {
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

bool mtk_get_gpu_block(unsigned int* pBlock)
{
    if (NULL != mtk_get_gpu_block_fp)
    {
        if (pBlock)
        {
            *pBlock = mtk_get_gpu_block_fp();
            return true;
        }
    }
    return false;
}
EXPORT_SYMBOL(mtk_get_gpu_block);

unsigned int (*mtk_get_gpu_idle_fp)(void) = NULL;
EXPORT_SYMBOL(mtk_get_gpu_idle_fp);

bool mtk_get_gpu_idle(unsigned int* pIdle)
{
    if (NULL != mtk_get_gpu_idle_fp)
    {
        if (pIdle)
        {
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
	if (NULL != mtk_get_gpu_freq_fp) {
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

bool mtk_get_gpu_GP_loading(unsigned int* pLoading)
{
    if (NULL != mtk_get_gpu_GP_loading_fp)
    {
        if (pLoading)
        {
            *pLoading = mtk_get_gpu_GP_loading_fp();
            return true;
        }
    }
    return false;
}
EXPORT_SYMBOL(mtk_get_gpu_GP_loading);

unsigned int (*mtk_get_gpu_PP_loading_fp)(void) = NULL;
EXPORT_SYMBOL(mtk_get_gpu_PP_loading_fp);

bool mtk_get_gpu_PP_loading(unsigned int* pLoading)
{
    if (NULL != mtk_get_gpu_PP_loading_fp)
    {
        if (pLoading)
        {
            *pLoading = mtk_get_gpu_PP_loading_fp();
            return true;
        }
    }
    return false;
}
EXPORT_SYMBOL(mtk_get_gpu_PP_loading);

unsigned int (*mtk_get_gpu_power_loading_fp)(void) = NULL;
EXPORT_SYMBOL(mtk_get_gpu_power_loading_fp);

bool mtk_get_gpu_power_loading(unsigned int* pLoading)
{
    if (NULL != mtk_get_gpu_power_loading_fp)
    {
        if (pLoading)
        {
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
    if (NULL != mtk_enable_gpu_dvfs_timer_fp)
    {
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
    if (NULL != mtk_boost_gpu_freq_fp)
    {
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
    if (NULL != mtk_set_bottom_gpu_freq_fp)
    {
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
    if ((NULL != mtk_get_bottom_gpu_freq_fp) && (pui32FreqLevel))
    {
        *pui32FreqLevel = mtk_get_bottom_gpu_freq_fp();
        return true;
    }
    return false;
}
EXPORT_SYMBOL(mtk_get_bottom_gpu_freq);
//-----------------------------------------------------------------------------
unsigned int (*mtk_custom_get_gpu_freq_level_count_fp)(void) = NULL;
EXPORT_SYMBOL(mtk_custom_get_gpu_freq_level_count_fp);

bool mtk_custom_get_gpu_freq_level_count(unsigned int* pui32FreqLevelCount)
{
    if (NULL != mtk_custom_get_gpu_freq_level_count_fp)
    {
        if (pui32FreqLevelCount)
        {
            *pui32FreqLevelCount = mtk_custom_get_gpu_freq_level_count_fp();
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
    if (NULL != mtk_custom_boost_gpu_freq_fp)
    {
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
    if (NULL != mtk_custom_upbound_gpu_freq_fp)
    {
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
    if ((NULL != mtk_get_custom_boost_gpu_freq_fp) && (NULL != pui32FreqLevel))
    {
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
    if ((NULL != mtk_get_custom_upbound_gpu_freq_fp) && (NULL != pui32FreqLevel))
    {
        *pui32FreqLevel = mtk_get_custom_upbound_gpu_freq_fp();
        return true;
    }
    return false;
}
EXPORT_SYMBOL(mtk_get_custom_upbound_gpu_freq);

//-----------------------------------------------------------------------------
void (*mtk_do_gpu_dvfs_fp)(unsigned long t, long phase, unsigned long ul3DFenceDoneTime) = NULL;
EXPORT_SYMBOL(mtk_do_gpu_dvfs_fp);

bool mtk_do_gpu_dvfs(unsigned long t, long phase, unsigned long ul3DFenceDoneTime)
{
    if (NULL != mtk_do_gpu_dvfs_fp)
    {
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
    if (NULL != mtk_gpu_sodi_entry_fp)
    {
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
    if (NULL != mtk_gpu_sodi_exit_fp)
    {
        mtk_gpu_sodi_exit_fp();
        return true;
    }
    return false;
}
EXPORT_SYMBOL(mtk_gpu_sodi_exit);


//-----------------------------------------------------------------------------

unsigned int (*mtk_get_sw_vsync_phase_fp)(void) = NULL;
EXPORT_SYMBOL(mtk_get_sw_vsync_phase_fp);

bool mtk_get_sw_vsync_phase(long* plPhase)
{
    if (NULL != mtk_get_sw_vsync_phase_fp)
    {
        if (plPhase)
        {
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

bool mtk_get_sw_vsync_time(unsigned long* pulTime)
{
    if (NULL != mtk_get_sw_vsync_time_fp)
    {
        if (pulTime)
        {
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

bool mtk_get_gpu_fence_done(unsigned long* pulTime)
{
    if (NULL != mtk_get_gpu_fence_done_fp)
    {
        if (pulTime)
        {
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
    if (NULL != mtk_gpu_dvfs_set_mode_fp)
    {
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
    if (NULL != mtk_dump_gpu_memory_usage_fp)
    {
        mtk_dump_gpu_memory_usage_fp();
        return true;
    }
    return false;
}
EXPORT_SYMBOL(mtk_dump_gpu_memory_usage);


//-----------------------------------------------------------------------------
int (*mtk_get_gpu_power_state_fp)(void) =NULL;
EXPORT_SYMBOL(mtk_get_gpu_power_state_fp);

int mtk_get_gpu_power_state(void)
{
    if (NULL != mtk_get_gpu_power_state_fp)
    {
        return mtk_get_gpu_power_state_fp();
    }
    return -1;
}
EXPORT_SYMBOL(mtk_get_gpu_power_state);

//-----------------------------------------------------------------------------
void (*mtk_gpu_dvfs_clock_switch_fp)(bool bSwitch) =NULL;
EXPORT_SYMBOL(mtk_gpu_dvfs_clock_switch_fp);

bool mtk_gpu_dvfs_clock_switch(bool bSwitch)
{
    if (NULL != mtk_gpu_dvfs_clock_switch_fp)
    {
        mtk_gpu_dvfs_clock_switch_fp(bSwitch);
        return true;
    }
    return false;
}
EXPORT_SYMBOL(mtk_gpu_dvfs_clock_switch);

//-----------------------------------------------------------------------------
void (*mtk_get_gpu_dvfs_from_fp)(MTK_GPU_DVFS_TYPE* peType, unsigned long *pulFreq) = NULL;
EXPORT_SYMBOL(mtk_get_gpu_dvfs_from_fp);

bool mtk_get_gpu_dvfs_from(MTK_GPU_DVFS_TYPE* peType, unsigned long *pulFreq)
{
    if (NULL != mtk_get_gpu_dvfs_from_fp)
    {
        if (peType && pulFreq)
        {
            mtk_get_gpu_dvfs_from_fp(peType, pulFreq);
            return true;
    }
}
    return false;
}
EXPORT_SYMBOL(mtk_get_gpu_dvfs_from);

//-----------------------------------------------------------------------------
bool mtk_get_3D_fences_count(int* pi32Count)
{
    if (pi32Count)
    {
        //*pi32Count = ged_monitor_3D_fence_get_count();
         return true;
    }
    return false;
}
EXPORT_SYMBOL(mtk_get_3D_fences_count);

//-----------------------------------------------------------------------------
unsigned long (*mtk_get_vsync_based_target_freq_fp)(void) = NULL;
EXPORT_SYMBOL(mtk_get_vsync_based_target_freq_fp);

bool mtk_get_vsync_based_target_freq(unsigned long *pulFreq)
{
    if (NULL != mtk_get_vsync_based_target_freq_fp)
    {
        if (pulFreq)
        {
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

bool mtk_get_gpu_sub_loading(unsigned int* pLoading)
{
    if (NULL != mtk_get_gpu_sub_loading_fp)
    {
        if (pLoading)
        {
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
    if (NULL != mtk_get_gpu_bottom_freq_fp)
    {
        if (pulFreq)
        {
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

bool mtk_get_gpu_custom_boost_freq(unsigned long* pulFreq)
{
    if (NULL != mtk_get_gpu_custom_boost_freq_fp)
    {
        if (pulFreq)
        {
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

bool mtk_get_gpu_custom_upbound_freq(unsigned long* pulFreq)
{
    if (NULL != mtk_get_gpu_custom_upbound_freq_fp)
    {
        if (pulFreq)
        {
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

bool mtk_get_vsync_offset_event_status(unsigned int* pui32EventStatus)
{
    if (NULL != mtk_get_vsync_offset_event_status_fp)
    {
        if (pui32EventStatus)
        {
            *pui32EventStatus = mtk_get_vsync_offset_event_status_fp();
            return true;
        }
    }
    return false;
}
EXPORT_SYMBOL(mtk_get_vsync_offset_event_status);

//-----------------------------------------------------------------------------

unsigned int (*mtk_get_vsync_offset_debug_status_fp)(void) = NULL;
EXPORT_SYMBOL(mtk_get_vsync_offset_debug_status_fp);

bool mtk_get_vsync_offset_debug_status(unsigned int* pui32DebugStatus)
{
    if (NULL != mtk_get_vsync_offset_debug_status_fp)
    {
        if (pui32DebugStatus)
        {
            *pui32DebugStatus = mtk_get_vsync_offset_debug_status_fp();
            return true;
        }
    }
    return false;
}
EXPORT_SYMBOL(mtk_get_vsync_offset_debug_status);

/* ----------------------------------------------------------------------------- */
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

/**
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
	if (NULL != mtk_enable_gpu_perf_monitor_fp)
	{
		return mtk_enable_gpu_perf_monitor_fp(enable);
	}

	return false;
}
EXPORT_SYMBOL(mtk_enable_gpu_perf_monitor);

/* ----------------------------------------------------------------------------- */

int (*mtk_get_gpu_pmu_init_fp)(GPU_PMU *pmus, int pmu_size, int *ret_size);
EXPORT_SYMBOL(mtk_get_gpu_pmu_init_fp);

bool mtk_get_gpu_pmu_init(GPU_PMU *pmus, int pmu_size, int *ret_size)
{
	if (mtk_get_gpu_pmu_init_fp != NULL)
		return mtk_get_gpu_pmu_init_fp(pmus, pmu_size, ret_size) == 0;
	return false;
}
EXPORT_SYMBOL(mtk_get_gpu_pmu_init);

/* ----------------------------------------------------------------------------- */

int (*mtk_get_gpu_pmu_deinit_fp)(void);
EXPORT_SYMBOL(mtk_get_gpu_pmu_deinit_fp);

bool mtk_get_gpu_pmu_deinit(void)
{
	if (mtk_get_gpu_pmu_deinit_fp != NULL)
		return mtk_get_gpu_pmu_deinit_fp() == 0;
	return false;
}
EXPORT_SYMBOL(mtk_get_gpu_pmu_deinit);

/* ------------------------------------------------------------------------- */

int (*mtk_get_gpu_pmu_swapnreset_fp)(GPU_PMU *pmus, int pmu_size);
EXPORT_SYMBOL(mtk_get_gpu_pmu_swapnreset_fp);

bool mtk_get_gpu_pmu_swapnreset(GPU_PMU *pmus, int pmu_size)
{
	if (mtk_get_gpu_pmu_swapnreset_fp != NULL)
		return mtk_get_gpu_pmu_swapnreset_fp(pmus, pmu_size) == 0;
	return false;
}
EXPORT_SYMBOL(mtk_get_gpu_pmu_swapnreset);

/* ----------------------------------------------------------------------------- */

int (*mtk_get_gpu_pmu_swapnreset_stop_fp)(void);
EXPORT_SYMBOL(mtk_get_gpu_pmu_swapnreset_stop_fp);

bool mtk_get_gpu_pmu_swapnreset_stop(void)
{
	if (mtk_get_gpu_pmu_swapnreset_stop_fp != NULL)
		return mtk_get_gpu_pmu_swapnreset_stop_fp() == 0;
	return false;
}
EXPORT_SYMBOL(mtk_get_gpu_pmu_swapnreset_stop);

/* ------------------------------------------------------------------------- */

typedef struct {
	 gpu_power_change_notify_fp callback;
	 char name[128];
	 struct list_head sList;
} gpu_power_change_entry_t;

static struct {
	struct mutex lock;
	struct list_head listen;
} g_power_change = {
	.lock     = __MUTEX_INITIALIZER(g_power_change.lock),
	.listen   = LIST_HEAD_INIT(g_power_change.listen),
};

bool mtk_register_gpu_power_change(const char *name, gpu_power_change_notify_fp callback)
{
	gpu_power_change_entry_t *entry = NULL;

	entry = kmalloc(sizeof(gpu_power_change_entry_t), GFP_KERNEL);
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
	gpu_power_change_entry_t *entry = NULL;

	mutex_lock(&g_power_change.lock);

	head = &g_power_change.listen;
	list_for_each(pos, head) {
		entry = list_entry(pos, gpu_power_change_entry_t, sList);
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
	gpu_power_change_entry_t *entry = NULL;

	mutex_lock(&g_power_change.lock);

	head = &g_power_change.listen;
	list_for_each(pos, head) {
		entry = list_entry(pos, gpu_power_change_entry_t, sList);
		entry->callback(power_on);
	}

	mutex_unlock(&g_power_change.lock);
}
EXPORT_SYMBOL(mtk_notify_gpu_power_change);

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
