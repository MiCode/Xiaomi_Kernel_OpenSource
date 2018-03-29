/**
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2010-2016 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file mali_osk_pm.c
 * Implementation of the callback functions from common power management
 */

#include <linux/sched.h>

#include "mali_kernel_linux.h"
#ifdef CONFIG_PM_RUNTIME
#include <linux/pm_runtime.h>
#endif /* CONFIG_PM_RUNTIME */
#include <linux/platform_device.h>
#include <linux/version.h>
#include "mali_osk.h"
#include "mali_kernel_common.h"
#include "mali_kernel_linux.h"

// MTK: for SODI ================================================================= //
#include "mali_pm.h"
#include "platform_pmm.h"

static _mali_osk_atomic_t mtk_mali_suspend_called;
static _mali_osk_atomic_t mtk_mali_pm_ref_count;
static _mali_osk_mutex_t* mtk_pm_lock;
static struct work_struct mtk_mali_pm_wq_work_handle, mtk_mali_pm_wq_work_handle2;
#if MALI_LICENSE_IS_GPL
static struct workqueue_struct *mtk_mali_pm_wq = NULL;
static struct workqueue_struct *mtk_mali_pm_wq2 = NULL;
#endif

static void MTK_mali_bottom_half_pm_suspend ( struct work_struct *work )
{    
    _mali_osk_mutex_wait(mtk_pm_lock);
    if((_mali_osk_atomic_read(&mtk_mali_pm_ref_count) == 0) &&
       (_mali_osk_atomic_read(&mtk_mali_suspend_called) == 0))
    {
        if (MALI_TRUE == mali_pm_runtime_suspend())
        {
            _mali_osk_atomic_inc(&mtk_mali_suspend_called);
            mali_platform_power_mode_change(MALI_POWER_MODE_DEEP_SLEEP);
        }
    }
    _mali_osk_mutex_signal(mtk_pm_lock);
}

static void MTK_mali_bottom_half_pm_resume ( struct work_struct *work )
{   
    _mali_osk_mutex_wait(mtk_pm_lock);
    mali_platform_power_mode_change(MALI_POWER_MODE_ON);
    if(_mali_osk_atomic_read(&mtk_mali_suspend_called))
    {	      		
        mali_pm_runtime_resume();
        _mali_osk_atomic_dec(&mtk_mali_suspend_called);
    }
    _mali_osk_mutex_signal(mtk_pm_lock);
}

void MTK_mali_osk_pm_dev_enable(void)
{
    _mali_osk_atomic_init(&mtk_mali_pm_ref_count, 0);
    _mali_osk_atomic_init(&mtk_mali_suspend_called, 0);
    mtk_pm_lock = _mali_osk_mutex_init(_MALI_OSK_LOCKFLAG_ORDERED, 0);

    mtk_mali_pm_wq = alloc_workqueue("mtk_mali_pm", WQ_UNBOUND, 0);
    mtk_mali_pm_wq2 = alloc_workqueue("mtk_mali_pm2", WQ_UNBOUND, 0);

    if(NULL == mtk_mali_pm_wq || NULL == mtk_mali_pm_wq2)
    {
        MALI_PRINT_ERROR(("Unable to create Mali pm workqueue\n"));
    }

    INIT_WORK(&mtk_mali_pm_wq_work_handle, MTK_mali_bottom_half_pm_suspend );
    INIT_WORK(&mtk_mali_pm_wq_work_handle2, MTK_mali_bottom_half_pm_resume);
}

void MTK_mali_osk_pm_dev_disable(void)
{
    if (mtk_mali_pm_wq)
    {
        flush_workqueue(mtk_mali_pm_wq);
        destroy_workqueue(mtk_mali_pm_wq);
        mtk_mali_pm_wq = NULL;
    }
    
    if (mtk_mali_pm_wq2)
    {
        flush_workqueue(mtk_mali_pm_wq2);
        destroy_workqueue(mtk_mali_pm_wq2);
        mtk_mali_pm_wq2 = NULL;
    }

	_mali_osk_atomic_term(&mtk_mali_pm_ref_count);
	_mali_osk_atomic_term(&mtk_mali_suspend_called);
	_mali_osk_mutex_term(mtk_pm_lock);
}
// MTK =========================================================================== //


/* Can NOT run in atomic context */
_mali_osk_errcode_t _mali_osk_pm_dev_ref_get_sync(void)
{
#ifdef CONFIG_PM_RUNTIME
    int err;
    MALI_DEBUG_ASSERT_POINTER(mali_platform_device);
    err = pm_runtime_get_sync(&(mali_platform_device->dev));
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 37))
    pm_runtime_mark_last_busy(&(mali_platform_device->dev));
#endif
    if (0 > err) {
        MALI_PRINT_ERROR(("Mali OSK PM: pm_runtime_get_sync() returned error code %d\n", err));
        return _MALI_OSK_ERR_FAULT;
    }
#else
    _mali_osk_mutex_wait(mtk_pm_lock);
    mali_platform_power_mode_change(MALI_POWER_MODE_ON);
    if(_mali_osk_atomic_read(&mtk_mali_suspend_called))
    {	      		
        mali_pm_runtime_resume();
        _mali_osk_atomic_dec(&mtk_mali_suspend_called);
    }
    _mali_osk_atomic_inc(&mtk_mali_pm_ref_count);
    _mali_osk_mutex_signal(mtk_pm_lock);
#endif
    return _MALI_OSK_ERR_OK;
}

/* Can run in atomic context */
_mali_osk_errcode_t _mali_osk_pm_dev_ref_get_async(void)
{
#ifdef CONFIG_PM_RUNTIME
    int err;
    MALI_DEBUG_ASSERT_POINTER(mali_platform_device);
    err = pm_runtime_get(&(mali_platform_device->dev));
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 37))
    pm_runtime_mark_last_busy(&(mali_platform_device->dev));
#endif
    if (0 > err && -EINPROGRESS != err) {
        MALI_PRINT_ERROR(("Mali OSK PM: pm_runtime_get() returned error code %d\n", err));
        return _MALI_OSK_ERR_FAULT;
    }
#else
    _mali_osk_atomic_inc(&mtk_mali_pm_ref_count);
    queue_work(mtk_mali_pm_wq2, &mtk_mali_pm_wq_work_handle2);
#endif
    return _MALI_OSK_ERR_OK;
}


/* Can run in atomic context */
void _mali_osk_pm_dev_ref_put(void)
{
#ifdef CONFIG_PM_RUNTIME
	MALI_DEBUG_ASSERT_POINTER(mali_platform_device);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 37))
	pm_runtime_mark_last_busy(&(mali_platform_device->dev));
	pm_runtime_put_autosuspend(&(mali_platform_device->dev));
#else
	pm_runtime_put(&(mali_platform_device->dev));
#endif
#else
    if(_mali_osk_atomic_dec_return(&mtk_mali_pm_ref_count) == 0)
    {
        if (mtk_mali_pm_wq)
        {
            queue_work(mtk_mali_pm_wq, &mtk_mali_pm_wq_work_handle);
        }
        else
        {
            MTK_mali_bottom_half_pm_suspend(NULL);
        }
    }
#endif
}

void _mali_osk_pm_dev_barrier(void)
{
#ifdef CONFIG_PM_RUNTIME
	pm_runtime_barrier(&(mali_platform_device->dev));
#endif
}
