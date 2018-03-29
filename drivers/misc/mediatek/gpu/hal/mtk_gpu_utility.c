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
#include <mt-plat/mtk_gpu_utility.h>
#include "ged_monitor_3D_fence.h"

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
        *pi32Count = ged_monitor_3D_fence_get_count();
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

