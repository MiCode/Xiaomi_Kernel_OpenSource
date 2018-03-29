/*
* Copyright (C) 2011-2014 MediaTek Inc.
* 
* This program is free software: you can redistribute it and/or modify it under the terms of the 
* GNU General Public License version 2 as published by the Free Software Foundation.
* 
* This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; 
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with this program.
* If not, see <http://www.gnu.org/licenses/>.
*/

#include <linux/version.h>
#include <linux/mali/mali_utgard.h>
#include "mali_kernel_common.h"
#include "mali_osk.h"
#include "platform_pmm.h"
#include "mt_gpufreq.h"
/*#include "mach/mt_spm.h"*/
/*#include "mach/mt_wdt.h"*/
#include "mach/mt_clkmgr.h"
#include "mali_control_timer.h"

#include <asm/atomic.h>

#if defined(CONFIG_MALI400_PROFILING)
#include "mali_osk_profiling.h"
#endif

extern unsigned int (*mtk_get_gpu_loading_fp)(void);
extern void (*mtk_boost_gpu_freq_fp)(void);
extern void (*mtk_custom_boost_gpu_freq_fp)(unsigned int ui32FreqLevel);
extern void (*mtk_set_bottom_gpu_freq_fp)(unsigned int ui32FreqLevel);
extern unsigned int (*mtk_custom_get_gpu_freq_level_count_fp)(void);
extern void (*mtk_gpu_sodi_entry_fp)(void);
extern void (*mtk_gpu_sodi_exit_fp)(void);
//extern void mali_utilization_suspend(void);

#include <linux/kernel.h>

static DEFINE_SPINLOCK(mali_pwr_lock);

static struct workqueue_struct *mali_dvfs_queue = NULL;
static struct work_struct      mali_dvfs_work;
static unsigned int            mali_utilization = 0;

// Mediatek: for GPU DVFS control
atomic_t g_current_frequency_id;
atomic_t g_boost_frequency_id;
atomic_t g_perf_hal_frequency_id;
atomic_t g_ged_hal_frequency_id;
atomic_t g_current_deferred_count;
atomic_t g_input_boost_duration;
atomic_t g_input_boost_enabled;
atomic_t g_dvfs_threshold_min;
atomic_t g_dvfs_threshold_max;
atomic_t g_dvfs_deferred_count;
atomic_t g_is_power_enabled;

int mtk_get_current_frequency_id(void)
{
    return atomic_read(&g_current_frequency_id);
}

void mtk_set_current_frequency_id(int id)
{
    atomic_set(&g_current_frequency_id, id);
}

int mtk_get_boost_frequency_id(void)
{
    return atomic_read(&g_boost_frequency_id);
}

void mtk_set_boost_frequency_id(int id)
{
    atomic_set(&g_boost_frequency_id, id);
}

int mtk_get_perf_hal_frequency_id(void)
{
    return atomic_read(&g_perf_hal_frequency_id);
}

void mtk_set_perf_hal_frequency_id(int id)
{
    atomic_set(&g_perf_hal_frequency_id, id);
}

int mtk_get_ged_hal_frequency_id(void)
{
    return atomic_read(&g_ged_hal_frequency_id);
}

void mtk_set_ged_hal_frequency_id(int id)
{
    atomic_set(&g_ged_hal_frequency_id, id);
}

int mtk_get_current_deferred_count(void)
{
    return atomic_read(&g_current_deferred_count);
}

void mtk_set_current_deferred_count(int count)
{
    atomic_set(&g_current_deferred_count, count);
}

int mtk_get_input_boost_duration(void)
{
    return atomic_read(&g_input_boost_duration);
}

void mtk_set_input_boost_duration(int duration)
{
    atomic_set(&g_input_boost_duration, duration); 
}

int mtk_get_input_boost_enabled(void)
{
    return atomic_read(&g_input_boost_enabled);
}

void mtk_set_input_boost_enabled(int enabled)
{
    atomic_set(&g_input_boost_enabled, enabled);
}

int mtk_get_dvfs_threshold_min(void)
{
    return atomic_read(&g_dvfs_threshold_min);
}

void mtk_set_dvfs_threshold_min(int min)
{
    if (0 == min)
    {
        min = 1;
    }
    atomic_set(&g_dvfs_threshold_min, min);
}

int mtk_get_dvfs_threshold_max(void)
{
    return atomic_read(&g_dvfs_threshold_max);
}

void mtk_set_dvfs_threshold_max(int max)
{
    if (0 == max)
    {
        max = 1;
    }
    atomic_set(&g_dvfs_threshold_max, max);
}

int mtk_get_dvfs_deferred_count(void)
{
    return atomic_read(&g_dvfs_deferred_count);
}

void mtk_set_dvfs_deferred_count(int count)
{
    atomic_set(&g_dvfs_deferred_count, count);
}


static void mali_dvfs_handler(struct work_struct *work)
{
    /*unsigned long flags;*/
    int           enabled;
    int           duration;
    int           boostID;
    int           perfID;
    int           gedID;
    int           halID;
    int           targetID;

    if (0 == atomic_read(&g_is_power_enabled))
    {
        MALI_DEBUG_PRINT(4, ("GPU clock is switching down\n"));
        return;
    }

    // Get related settings
    enabled  = mtk_get_input_boost_enabled();
    duration = mtk_get_input_boost_duration();
    boostID  = mtk_get_boost_frequency_id();
    perfID   = mtk_get_perf_hal_frequency_id();
    gedID    = mtk_get_ged_hal_frequency_id();
    targetID = mtk_get_current_frequency_id();

    if ((enabled != 0) && (duration > 0))
    {
        targetID = boostID;
    }
    else
    {
        if (targetID < mt_gpufreq_get_thermal_limit_index())
        {
            targetID = mt_gpufreq_get_thermal_limit_index();
        }

        /* Calculate higher boost frequency (e.g. lower index id) */
        if(perfID < gedID)
        {
            halID = perfID;
        }
        else
        {
            halID = gedID;
        }
    
        if(targetID > halID)
        {
            MALI_DEBUG_PRINT(4, ("Use GPU boost frequency %d as target!\n", halID));
            targetID = halID;
        }
    }

    if (targetID != mt_gpufreq_get_cur_freq_index())
    {
        mt_gpufreq_target(targetID);
    }
}


void mali_dispatch_dvfs_work(void)
{
#if MALI_LICENSE_IS_GPL
    // Adjust GPU frequency
    if (mali_dvfs_queue)
    {
        queue_work(mali_dvfs_queue, &mali_dvfs_work);
    }
    else
#endif // MALI_LICENSE_IS_GPL
    {
        mali_dvfs_handler(NULL);
    }
}


void mali_cancel_dvfs_work(void)
{
#if MALI_LICENSE_IS_GPL
    if (mali_dvfs_queue)
    {
        cancel_work_sync(&mali_dvfs_work);
        flush_workqueue(mali_dvfs_queue);
    }
#endif  // MALI_LICENSE_IS_GPL
}


void mtk_gpu_input_boost_callback(unsigned int boostID)
{
    unsigned int  currentID;

    if(mtk_get_input_boost_enabled() == 0)
    {
        // Boost is disabled, so return directly.
        return;
    }

    MALI_DEBUG_PRINT(4, ("[MALI] mtk_gpu_input_boost_callback() set to freq id=%d\n", boostID));

    currentID = mt_gpufreq_get_cur_freq_index();

    if ((1 == atomic_read(&g_is_power_enabled)) && (boostID < currentID))
    {
        mtk_set_boost_frequency_id(boostID);
        mtk_set_input_boost_duration(3);
        mtk_set_current_deferred_count(0);
        mt_gpufreq_target(boostID);
    }
}


void mtk_gpu_power_limit_callback(unsigned int limitID)
{
    unsigned int  currentID;

    MALI_DEBUG_PRINT(4, ("[MALI] mtk_gpu_power_limit_callback() set to freq id=%d\n", limitID));

    currentID = mt_gpufreq_get_cur_freq_index();

    if ((1 == atomic_read(&g_is_power_enabled)) && (currentID < limitID))
    {
        mtk_set_current_frequency_id(limitID);
        mtk_set_input_boost_duration(0);
        mtk_set_current_deferred_count(0);
        mt_gpufreq_target(limitID);
    }
}

static void mtk_touch_boost_gpu_freq(void)
{
    mtk_gpu_input_boost_callback(0);
}


/* MTK custom boost. 0 is the lowest frequency index. The function is used for performance service currently.*/
static void mtk_perf_hal_callback(unsigned int level)
{
    unsigned int total;
    unsigned int targetID;

    total = mt_gpufreq_get_dvfs_table_num();
    if (level >= total)
    {
        level = total - 1;
    }
    
    targetID = total - level - 1;

    mtk_set_perf_hal_frequency_id(targetID);

    MALI_DEBUG_PRINT(4, ("[MALI] mtk_perf_hal_callback() level=%d, boost ID=%d", level, targetID));

    if (targetID != mt_gpufreq_get_cur_freq_index())
    {
        mt_gpufreq_target(targetID);
    }
}

/* MTK set boost. 0 is the lowest frequency index. The function is used for GED boost currently.*/
static void mtk_ged_hal_callback(unsigned int level)
{
    unsigned int total;
    unsigned int targetID;

    total = mt_gpufreq_get_dvfs_table_num();
    if (level >= total)
    {
        level = total - 1;
    }

    targetID = total - level - 1;

    mtk_set_ged_hal_frequency_id(targetID);

    MALI_DEBUG_PRINT(4, ("[MALI] mtk_ged_hal_callback() level=%d, boost ID=%d", level, targetID));

    if (targetID != mt_gpufreq_get_cur_freq_index())
    {
        mt_gpufreq_target(targetID);
    }
}

static unsigned int mtk_get_freq_level_count(void)
{
    return mt_gpufreq_get_dvfs_table_num();
}


static unsigned int mtk_get_mali_utilization(void)
{
    return (mali_utilization * 100) / 256;
}


void mali_set_mali_SODI_begin(void)
{
    mali_control_timer_suspend(MALI_TRUE);
}


void mali_set_mali_SODI_exit(void)
{
    mtk_set_current_frequency_id(0);
    mtk_set_input_boost_duration(0);
    mtk_set_current_deferred_count(0);
}


void mali_pmm_init(void)
{
    MALI_DEBUG_PRINT(4, ("%s\n", __FUNCTION__));

#if MALI_LICENSE_IS_GPL
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36))
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,18,0)    
    mali_dvfs_queue = alloc_workqueue("mali_dvfs", WQ_NON_REENTRANT | WQ_UNBOUND | WQ_HIGHPRI, 0);
#else
    mali_dvfs_queue = alloc_workqueue("mali_dvfs", WQ_UNBOUND | WQ_HIGHPRI, 0);
#endif
#else
    mali_dvfs_queue = create_workqueue("mali_dvfs");
#endif

    INIT_WORK(&mali_dvfs_work, mali_dvfs_handler);
#endif // MALI_LICENSE_IS_GPL

    atomic_set(&g_current_frequency_id, 0);
    atomic_set(&g_boost_frequency_id, 0);
    atomic_set(&g_perf_hal_frequency_id, 4);
    atomic_set(&g_ged_hal_frequency_id, 4); 
    atomic_set(&g_current_deferred_count, 0);
    atomic_set(&g_input_boost_enabled, 1);
    atomic_set(&g_input_boost_duration, 0);
    atomic_set(&g_dvfs_threshold_min, 32);
    atomic_set(&g_dvfs_threshold_max, 45);
    atomic_set(&g_dvfs_deferred_count, 0);
    atomic_set(&g_is_power_enabled, 0);

	/* MTK Register input boost and power limit call back function */
	mt_gpufreq_input_boost_notify_registerCB(mtk_gpu_input_boost_callback);
	mt_gpufreq_power_limit_notify_registerCB(mtk_gpu_power_limit_callback);

	/* Register gpu boost function to MTK HAL */
	mtk_boost_gpu_freq_fp                  = mtk_touch_boost_gpu_freq;
	mtk_custom_boost_gpu_freq_fp           = mtk_perf_hal_callback; /* used for for performance service boost */
    mtk_set_bottom_gpu_freq_fp             = mtk_ged_hal_callback; /* used for GED boost */
	mtk_custom_get_gpu_freq_level_count_fp = mtk_get_freq_level_count;
    mtk_get_gpu_loading_fp                 = mtk_get_mali_utilization;
    mtk_gpu_sodi_entry_fp                  = mali_set_mali_SODI_begin;
    mtk_gpu_sodi_exit_fp                   = mali_set_mali_SODI_exit;
}


void mali_pmm_deinit(void)
{
    MALI_DEBUG_PRINT(4, ("%s\n", __FUNCTION__));

#if MALI_LICENSE_IS_GPL
    if (mali_dvfs_queue)
    {
        mali_cancel_dvfs_work();

        destroy_workqueue(mali_dvfs_queue);
        mali_dvfs_queue = NULL;
    }
#endif // MALI_LICENSE_IS_GPL
}


static int mapIndexToFrequency(int index)
{
    int value;

    switch(index)
    {
        case 0:
            value = 500500;
            break;
        case 1:
            value = 416000;
            break;
        case 2:
            value = 300300;
            break;
        case 3:
            value = 214500;
            break;
        case 4:
            value = 107250;
            break;
        default:
            value = 0;
            break;
    }

    return value;
}


static int mapFrequencyToIndex(int frequency)
{
    int value;

    if (frequency > 416000)
    {
        value = 0;  // 500500
    }
    else if (frequency > 300300)
    {
        value = 1;  // 416000
    }
    else if (frequency > 214500)
    {
        value = 2;  // 300300
    }
    else if (frequency > 107250)
    {
        value = 3;  // 214500
    }
    else
    {
        value = 4;  // 107250
    }

    return value;
}


/* this function will be called periodically with sampling period 200ms~1000ms */
void mali_pmm_utilization_handler(struct mali_gpu_utilization_data *data)
{    
    int              utilization;
    mali_dvfs_action action;
    int              frequency;
    int              duration;
    int              currentID;
    int              targetID;
    int              deferred;

    mali_utilization = data->utilization_gpu;
    
    if (0 == atomic_read(&g_is_power_enabled))
        
    {
        MALI_DEBUG_PRINT(4, ("GPU clock is in off state\n"));
        return;
    }

    utilization = (mali_utilization * 100) / 256;

    MALI_DEBUG_PRINT(4, ("%s GPU utilization=%d\n", __FUNCTION__, utilization));

    if (utilization <= mtk_get_dvfs_threshold_min())
    {
        action = MALI_DVFS_CLOCK_DOWN;
    }
    else if (utilization >= mtk_get_dvfs_threshold_max())
    {
        action = MALI_DVFS_CLOCK_UP;
    }
    else
    {
        MALI_DEBUG_PRINT(4, ("No need to adjust GPU frequency!\n"));
        return;
    }

    // Get current frequency id    
    currentID = mt_gpufreq_get_cur_freq_index();

    // Get current deferred count
    deferred  = mtk_get_current_deferred_count();

    switch(action)
    {
        case MALI_DVFS_CLOCK_UP:
            frequency = mapIndexToFrequency(currentID) * utilization / mtk_get_dvfs_threshold_min();
            targetID  = mapFrequencyToIndex(frequency);
            deferred += 1;
            break;
        case MALI_DVFS_CLOCK_DOWN:
            frequency = mapIndexToFrequency(currentID) * utilization / mtk_get_dvfs_threshold_max();
            targetID  = mapFrequencyToIndex(frequency);
            deferred += 1;
            break;
        default:
            MALI_DEBUG_PRINT(4, ("Unknown GPU DVFS operation!\n"));
            return;
    }

    // Thermal power limit
    if (targetID < mt_gpufreq_get_thermal_limit_index())
    {
        targetID = mt_gpufreq_get_thermal_limit_index();
    }

    duration = mtk_get_input_boost_duration();
    if((duration > 0) && (mtk_get_input_boost_enabled() != 0))
    {
        MALI_DEBUG_PRINT(4, ("Still in the boost duration!\n"));
        
        mtk_set_input_boost_duration(duration - 1);
        
        if (targetID >= mtk_get_boost_frequency_id())
        {
            mtk_set_current_deferred_count(deferred);
            return;
        }
    }
    else if (deferred < mtk_get_dvfs_deferred_count())
    {
        MALI_DEBUG_PRINT(4, ("Defer DVFS frequency operation!\n"));
        mtk_set_current_deferred_count(deferred);             
        return;
    }

    if(currentID == targetID)
    {
        MALI_DEBUG_PRINT(4, ("Target GPU frequency is the same!\n"));
        return;
    }

    if (targetID < 0)
    {
        targetID = 0;
    }
    else if (targetID >= mt_gpufreq_get_dvfs_table_num())
    {
        targetID = mt_gpufreq_get_dvfs_table_num() - 1;
    }

    mtk_set_current_frequency_id(targetID);
    
    if (targetID < mtk_get_boost_frequency_id())
    {
        mtk_set_boost_frequency_id(targetID);
    }

    mtk_set_current_deferred_count(0);

#if MALI_LICENSE_IS_GPL
    if (mali_dvfs_queue && (1 == atomic_read(&g_is_power_enabled)))
    {
        queue_work(mali_dvfs_queue, &mali_dvfs_work);
    }
#endif // MALI_LICENSE_IS_GPL
}

void mali_pmm_acquire_protect(void)
{
    // Handled by MTCMOS owner
}

void mali_pmm_release_protect(void)
{
    // Handled by MTCMOS owner
}

void mali_platform_power_mode_change(mali_power_mode power_mode)
{
    unsigned long flags;

    switch (power_mode)
    {
        case MALI_POWER_MODE_ON:
            MALI_DEBUG_PRINT(4, ("[+] MFG enable_clock \n"));

            spin_lock_irqsave(&mali_pwr_lock, flags);

            if (!clock_is_on(MT_CG_BG3D))
            {
                enable_clock(MT_CG_BG3D, "MFG");
            }

            spin_unlock_irqrestore(&mali_pwr_lock, flags);

            atomic_set(&g_is_power_enabled, 1);
            mali_dispatch_dvfs_work();

            MALI_DEBUG_PRINT(4, ("[-] MFG enable_clock \n"));

#if defined(CONFIG_MALI400_PROFILING)
            _mali_osk_profiling_add_event(MALI_PROFILING_EVENT_TYPE_SINGLE |
                                              MALI_PROFILING_EVENT_CHANNEL_GPU |
                                              MALI_PROFILING_EVENT_REASON_SINGLE_GPU_FREQ_VOLT_CHANGE, 500, 1200 / 1000, 0, 0, 0);
#endif
            break;
        case MALI_POWER_MODE_LIGHT_SLEEP:
        case MALI_POWER_MODE_DEEP_SLEEP:
            MALI_DEBUG_PRINT(4, ("[+] MFG disable_clock \n"));

            atomic_set(&g_is_power_enabled, 0);
            mali_cancel_dvfs_work();

            spin_lock_irqsave(&mali_pwr_lock, flags);

            if (clock_is_on(MT_CG_BG3D))
            {   
                disable_clock(MT_CG_BG3D, "MFG");
                mtk_set_input_boost_duration(0);
            }

            spin_unlock_irqrestore(&mali_pwr_lock, flags);

            MALI_DEBUG_PRINT(4, ("[-] MFG disable_clock \n"));

#if defined(CONFIG_MALI400_PROFILING)
            _mali_osk_profiling_add_event(MALI_PROFILING_EVENT_TYPE_SINGLE |
                                          MALI_PROFILING_EVENT_CHANNEL_GPU |
                                          MALI_PROFILING_EVENT_REASON_SINGLE_GPU_FREQ_VOLT_CHANGE, 0, 0, 0, 0, 0);
#endif
            break;
    }
}
