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

#include <linux/platform_device.h>
#include <linux/version.h>
#include <linux/pm.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#endif

#ifdef CONFIG_PM_RUNTIME
#include <linux/pm_runtime.h>
#endif
#include <asm/io.h>
#include <linux/mali/mali_utgard.h>
#include "mali_kernel_common.h"
#include <linux/dma-mapping.h>
/*#include <mach/mt_irq.h>*/

#include "mali_kernel_common.h"
#include "arm_core_scaling.h"
#include "platform_pmm.h"
#include "mali_pm.h"
#include "mali_osk.h"
/*#include "mt_reg_base.h"*/

static void mali_platform_device_release(struct device *device);
static int mali_pm_suspend(struct device *device);
static int mali_pm_resume(struct device *device);

static const struct of_device_id efuse_of_match[] = {
	{ .compatible = "mediatek,EFUSEC", },
	{},
};

static void __iomem *efuse_base = 0;
static int           core_count = 0;

static struct mali_gpu_device_data mali_gpu_data =
{
    // System memory
    .shared_mem_size = 512 * 1024 * 1024, /* 1GB */
    // Framebuffer physical address, only for validation usage
    .fb_start = 0x80000000,
    .fb_size  = 0x80000000,
    // DVFS
    .control_interval   = 8, /* ms */
    .utilization_callback   = mali_pmm_utilization_handler, /*<utilization function>,*/
};


static struct resource mali_gpu_resources_mp1[] =
{
    MALI_GPU_RESOURCES_MALI400_MP1(
                    IO_VIRT_TO_PHYS(0xF3010000),
                    176, //MT_MFG_IRQ_GP_ID,
                    177, //MT_MFG_IRQ_GPMMU_ID ,
                    178, //MT_MFG_IRQ_PP0_ID,
                    179  //MT_MFG_IRQ_PPMMU0_ID
                )
};

static struct resource mali_gpu_resources_mp2[] =
{
    MALI_GPU_RESOURCES_MALI400_MP2(
                    IO_VIRT_TO_PHYS(0xF3010000),
                    176, //MT_MFG_IRQ_GP_ID,
                    177, //MT_MFG_IRQ_GPMMU_ID ,
                    178, //MT_MFG_IRQ_PP0_ID,
                    179, //MT_MFG_IRQ_PPMMU0_ID,
                    180, //MT_MFG_IRQ_PP1_ID,
                    181  //MT_MFG_IRQ_PPMMU1_ID
                )
};

static struct dev_pm_ops mali_gpu_device_type_pm_ops =
{
    .suspend = mali_pm_suspend,
    .resume  = mali_pm_resume, 
    .freeze  = mali_pm_suspend, 
    .thaw    = mali_pm_resume,   
    .restore = mali_pm_resume,
    
#ifdef CONFIG_PM_RUNTIME
    .runtime_suspend = mali_runtime_suspend,
    .runtime_resume  = mali_runtime_resume,
    .runtime_idle   = mali_runtime_idle,
#endif
};

static struct device_type mali_gpu_device_device_type =
{
    .pm = &mali_gpu_device_type_pm_ops,
};

static struct platform_device mali_gpu_device_mp1 =
{
   .name = MALI_GPU_NAME_UTGARD,
   .id = 0,
   .num_resources = ARRAY_SIZE(mali_gpu_resources_mp1),
   .resource = (struct resource *)&mali_gpu_resources_mp1,
   .dev.platform_data = &mali_gpu_data,
   .dev.release = mali_platform_device_release,
   .dev.coherent_dma_mask = DMA_BIT_MASK(32),    
    /// Ideally .dev.pm_domain should be used instead, as this is the new framework designed
    /// to control the power of devices.     
   .dev.type = &mali_gpu_device_device_type /// We should probably use the pm_domain instead of type on newer kernels
};

static struct platform_device mali_gpu_device_mp2 =
{
   .name = MALI_GPU_NAME_UTGARD,
   .id = 0,
   .num_resources = ARRAY_SIZE(mali_gpu_resources_mp2),
   .resource = (struct resource *)&mali_gpu_resources_mp2,
   .dev.platform_data = &mali_gpu_data,
   .dev.release = mali_platform_device_release,
   .dev.coherent_dma_mask = DMA_BIT_MASK(32),    
    /// Ideally .dev.pm_domain should be used instead, as this is the new framework designed
    /// to control the power of devices.     
   .dev.type = &mali_gpu_device_device_type /// We should probably use the pm_domain instead of type on newer kernels
};


extern unsigned int get_max_DRAM_size (void);

static int mali_pmm_reg_read(void *addr)
{
    return ((int)*((volatile unsigned int*)addr));
}

/*static void mali_pmm_reg_write(void *addr, unsigned int value)
{
    (*((volatile unsigned int*)addr) = value);
}*/

int mali_platform_device_register(void)
{
    int err = -1;
    int value;

#ifdef CONFIG_OF
	struct device_node *np_efuse;

	np_efuse = of_find_compatible_node(NULL, NULL, efuse_of_match[0].compatible);
	
	if(!efuse_base)
	{	
		efuse_base = of_iomap(np_efuse, 0);
	}
#endif

    MALI_DEBUG_PRINT(1, ("%s\n", __FUNCTION__));
    
    mali_gpu_data.shared_mem_size = get_max_DRAM_size();

    if (efuse_base)
    {
        value = mali_pmm_reg_read(efuse_base + 0x040);
        if (value & 0x00020000)
        {
            MALI_DEBUG_ASSERT(0);
            return err;
        }
        else if (value & 0x00040000)
        {
            err = platform_device_register(&mali_gpu_device_mp1);
            core_count = 1;
        }
        else
        {
            err = platform_device_register(&mali_gpu_device_mp2);
            core_count = 2;
        }
    }
    else
    {
        err = platform_device_register(&mali_gpu_device_mp2);
        core_count = 2;
    }
	
    mali_platform_power_mode_change(MALI_POWER_MODE_ON);
	
    if (0 == err) 
    {         
        mali_pmm_init();
        
#ifdef CONFIG_PM_RUNTIME
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37))
        if (coue_count == 2)
        {
            pm_runtime_set_autosuspend_delay(&(mali_gpu_device_mp2.dev), 1000);
            pm_runtime_use_autosuspend(&(mali_gpu_device_mp2.dev));
        }
        else
        {
            pm_runtime_set_autosuspend_delay(&(mali_gpu_device_mp1.dev), 1000);
            pm_runtime_use_autosuspend(&(mali_gpu_device_mp1.dev));
        }
#endif
        if (coue_count == 2)
        {
            pm_runtime_enable(&(mali_gpu_device_mp2.dev));
        }
        else
        {
            pm_runtime_enable(&(mali_gpu_device_mp1.dev));
        }
#endif
        
        return 0;
    }

    MALI_DEBUG_PRINT(1, ("%s err=%d\n",__FUNCTION__, err));

    if (2 == core_count)
    {
        platform_device_unregister(&mali_gpu_device_mp2);
    }
    else
    {
        platform_device_unregister(&mali_gpu_device_mp1);
    }

    return err;
}

void mali_platform_device_unregister(void)
{
    MALI_DEBUG_PRINT(1, ("%s\n", __FUNCTION__));    
    
    mali_pmm_deinit();
 
    if (2 == core_count)
    {
        platform_device_unregister(&mali_gpu_device_mp2);
    }
    else
    {
        platform_device_unregister(&mali_gpu_device_mp1);
    }
}

static void mali_platform_device_release(struct device *device)
{
    MALI_DEBUG_PRINT(1, ("%s\n", __FUNCTION__));
}

static int mali_pm_suspend(struct device *device)
{
    int ret = 0;

    MALI_DEBUG_PRINT(1, ("Mali PM:%s\n", __FUNCTION__));

    if (NULL != device->driver &&
        NULL != device->driver->pm &&
        NULL != device->driver->pm->suspend)
    {
        /* Need to notify Mali driver about this event */
        ret = device->driver->pm->suspend(device);
    }

    //_mali_osk_pm_delete_callback_timer();
	mali_platform_power_mode_change(MALI_POWER_MODE_LIGHT_SLEEP);

    return ret;
}

static int mali_pm_resume(struct device *device)
{
    int ret = 0;

    MALI_DEBUG_PRINT(1, ("Mali PM: %s\n", __FUNCTION__));

	mali_platform_power_mode_change(MALI_POWER_MODE_ON);

    if (NULL != device->driver &&
        NULL != device->driver->pm &&
        NULL != device->driver->pm->resume)
    {
        /* Need to notify Mali driver about this event */
        ret = device->driver->pm->resume(device);
    }

    return ret;
}

#if 0//because not used
static int mali_pm_freeze(struct device *device)
{
    int ret = 0;
    
    MALI_DEBUG_PRINT(1, ("Mali PM: %s\n", __FUNCTION__));
    
    if (NULL != device->driver &&
        NULL != device->driver->pm &&
        NULL != device->driver->pm->freeze)
    {
        /* Need to notify Mali driver about this event */
        ret = device->driver->pm->freeze(device);
    }

    return ret;
}

static int mali_pm_thaw(struct device *device)
{
    int ret = 0;

    MALI_DEBUG_PRINT(1, ("Mali PM: %s\n", __FUNCTION__));

    if (NULL != device->driver &&
        NULL != device->driver->pm &&
        NULL != device->driver->pm->thaw)
    {
        /* Need to notify Mali driver about this event */
        ret = device->driver->pm->thaw(device);
    }

    return ret;
}
#endif

#ifdef CONFIG_PM_RUNTIME
static int mali_runtime_suspend(struct device *device)
{
    int ret = 0;

    MALI_DEBUG_PRINT(4, ("mali_runtime_suspend() called\n"));

    if (NULL != device->driver &&
        NULL != device->driver->pm &&
        NULL != device->driver->pm->runtime_suspend)
    {
        /* Need to notify Mali driver about this event */
        ret = device->driver->pm->runtime_suspend(device);
    }

    mali_platform_power_mode_change(MALI_POWER_MODE_LIGHT_SLEEP);

    return ret;
}

static int mali_runtime_resume(struct device *device)
{
    int ret = 0;

    MALI_DEBUG_PRINT(4, ("mali_runtime_resume() called\n"));

    mali_platform_power_mode_change(MALI_POWER_MODE_ON);

    if (NULL != device->driver &&
        NULL != device->driver->pm &&
        NULL != device->driver->pm->runtime_resume)
    {
        /* Need to notify Mali driver about this event */
        ret = device->driver->pm->runtime_resume(device);
    }

    return ret;
}

static int mali_runtime_idle(struct device *device)
{
    MALI_DEBUG_PRINT(4, ("mali_runtime_idle() called\n"));

    if (NULL != device->driver &&
        NULL != device->driver->pm &&
        NULL != device->driver->pm->runtime_idle)
    {
        /* Need to notify Mali driver about this event */
        int ret = device->driver->pm->runtime_idle(device);
        if (0 != ret)
        {
            return ret;
        }
    }

    pm_runtime_suspend(device);

    return 0;
}
#endif /// CONFIG_PM_RUNTIME
