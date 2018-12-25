/*
 *
 * (C) COPYRIGHT 2011-2015 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * A copy of the licence is included with the program, and can also be obtained
 * from Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 */





#include <linux/ioport.h>
#include <mali_kbase.h>
#include <mali_kbase_defs.h>
#include <mali_kbase_config.h>
#include "mali_kbase_cpu_vexpress.h"
#include "mali_kbase_config_platform.h"

/* MTK clock modified */
#include "mt_gpufreq.h"
#include "upmu_common.h"
#include "mach/upmu_sw.h"
#include "mach/upmu_hw.h"
#include "mali_kbase_pm.h"
#include <backend/gpu/mali_kbase_pm_internal.h>

#include "mt_chip.h"

#ifdef CONFIG_MTK_CLKMGR
#include "mt_clkmgr.h"
#endif /* CONFIG_MTK_CLKMGR */

#ifndef CONFIG_MTK_CLKMGR
#include <linux/clk.h>
#endif /* !CONFIG_MTK_CLKMGR */

#ifdef CONFIG_OF
extern void __iomem  *clk_mfgcfg_base_addr;
#endif

#include "ged_dvfs.h"

#define HARD_RESET_AT_POWER_OFF 1

#ifndef CONFIG_OF
static struct kbase_io_resources io_resources = {
	.job_irq_number = 68,
	.mmu_irq_number = 69,
	.gpu_irq_number = 70,
	.io_memory_region = {
	.start = 0xFC010000,
	.end = 0xFC010000 + (4096 * 4) - 1
	}
};
#endif /* CONFIG_OF */

unsigned int g_power_off_gpu_freq_idx;
unsigned int g_power_status;
extern unsigned int g_type_T;

static int pm_callback_power_on(struct kbase_device *kbdev)
{
#ifdef CONFIG_MALI_MIDGARD_DVFS
	int touch_boost_flag, touch_boost_id;
#endif /* CONFIG_MALI_MIDGARD_DVFS */

	unsigned int current_gpu_freq_idx;

#ifndef CONFIG_MTK_CLKMGR
	int ret;
#endif

	unsigned int code;
	code = mt_get_chip_hw_code();

	mt_gpufreq_voltage_enable_set(1);
#ifdef ENABLE_COMMON_DVFS
    ged_dvfs_gpu_clock_switch_notify(1);
#endif
	
	
	
	
	
#ifdef CONFIG_MALI_MIDGARD_DVFS

#endif /* CONFIG_MALI_MIDGARD_DVFS */

if (0x321 == code) {
		// do something for Denali-1(6735)
#ifdef CONFIG_MTK_CLKMGR
		enable_clock( MT_CG_DISP0_SMI_COMMON, "GPU");
		enable_clock( MT_CG_MFG_BG3D, "GPU");
#else
		ret = clk_prepare_enable(kbdev->clk_display_scp);
		if (ret)
		{
			pr_debug("MALI: clk_prepare_enable failed when enabling display MTCMOS");
		}
		
		ret = clk_prepare_enable(kbdev->clk_smi_common);
		if (ret)
		{
			pr_debug("MALI: clk_prepare_enable failed when enabling display smi_common clock");
		}
		
		ret = clk_prepare_enable(kbdev->clk_mfg_scp);
		if (ret)
		{
			pr_debug("MALI: clk_prepare_enable failed when enabling mfg MTCMOS");
		}
		
		ret = clk_prepare_enable(kbdev->clk_mfg);
		if (ret)
		{
			pr_debug("MALI: clk_prepare_enable failed when enabling mfg clock");
		}
#endif
	} else if (0x335 == code) {
		// do something for Denali-2(6735M)
#ifdef CONFIG_MTK_CLKMGR
		enable_clock( MT_CG_DISP0_SMI_COMMON, "GPU");
		enable_clock( MT_CG_MFG_BG3D, "GPU");
#endif /* CONFIG_MTK_CLKMGR */
	} else if (0x337 == code) {
		// do something for Denali-3(6753)
#ifdef CONFIG_MTK_CLKMGR
		enable_clock( MT_CG_DISP0_SMI_COMMON, "GPU");
		enable_clock( MT_CG_MFG_BG3D, "GPU");
#endif /* CONFIG_MTK_CLKMGR */
	} else {
		// unknown chip ID, error !!
#ifdef CONFIG_MTK_CLKMGR
		enable_clock( MT_CG_DISP0_SMI_COMMON, "GPU");
		enable_clock( MT_CG_MFG_BG3D, "GPU");
#endif /* CONFIG_MTK_CLKMGR */
	}

	g_power_status = 1; // the power status is "power on".
	mt_gpufreq_target(g_power_off_gpu_freq_idx);
	current_gpu_freq_idx = mt_gpufreq_get_cur_freq_index();
	if( current_gpu_freq_idx > g_power_off_gpu_freq_idx)
		pr_debug("MALI: GPU freq. can't switch to idx=%d\n", g_power_off_gpu_freq_idx );

    mtk_get_touch_boost_flag( &touch_boost_flag, &touch_boost_id);
    if(touch_boost_flag > 0)
    {
        mt_gpufreq_target(touch_boost_id);
        mtk_clear_touch_boost_flag();
    }
	
	/* Nothing is needed on VExpress, but we may have destroyed GPU state (if the below HARD_RESET code is active) */
	return 1;
}

/*-----------------------------------------------------------------------------
		Macro
-----------------------------------------------------------------------------*/
#define DELAY_LOOP_COUNT    100000
#define MFG_DEBUG_SEL        0x3
#define MFG_BUS_IDLE_BIT    (1 << 2)
														
#define MFG_DEBUG_CTRL_REG  (clk_mfgcfg_base_addr + 0x180)
#define MFG_DEBUG_STAT_REG  (clk_mfgcfg_base_addr + 0x184)

#define MFG_WRITE32(value, addr) writel(value, addr)
#define MFG_READ32(addr)         readl(addr)

static void pm_callback_power_off(struct kbase_device *kbdev)
{
	unsigned int uiCurrentFreqCount;

	volatile int polling_count = 100000;
	volatile int i = 0;
	unsigned int code;

	/// 1. Delay 0.01ms before power off   
	for (i=0; i < DELAY_LOOP_COUNT;i++);
	if (DELAY_LOOP_COUNT != i)
	{   
		pr_warn("[MALI] power off delay error!\n");
	}
			
	/// 2. Polling the MFG_DEBUG_REG for checking GPU IDLE before MTCMOS power off (0.1ms)
	MFG_WRITE32(0x3, MFG_DEBUG_CTRL_REG);

	do {
		/// 0x13000184[2]
		/// 1'b1: bus idle
		/// 1'b0: bus busy  
		if (MFG_READ32(MFG_DEBUG_STAT_REG) & MFG_BUS_IDLE_BIT)
		{
			/// printk("[MALI]MFG BUS already IDLE! Ready to power off, %d\n", polling_count);
			break;
		}
	} while (polling_count--);

	if (polling_count <=0)
	{
		pr_warn("[MALI]!!!!MFG(GPU) subsys is still BUSY!!!!!, polling_count=%d\n", polling_count);
	}
	
#if HARD_RESET_AT_POWER_OFF
	/* Cause a GPU hard reset to test whether we have actually idled the GPU
	 * and that we properly reconfigure the GPU on power up.
	 * Usually this would be dangerous, but if the GPU is working correctly it should
	 * be completely safe as the GPU should not be active at this point.
	 * However this is disabled normally because it will most likely interfere with
	 * bus logging etc.
	 */
	//KBASE_TRACE_ADD(kbdev, CORE_GPU_HARD_RESET, NULL, NULL, 0u, 0);
	kbase_os_reg_write(kbdev, GPU_CONTROL_REG(GPU_COMMAND), GPU_COMMAND_HARD_RESET);
	///  Polling the MFG_DEBUG_REG for checking GPU IDLE before MTCMOS power off (0.1ms)
	MFG_WRITE32(0x3, MFG_DEBUG_CTRL_REG);

	do {
		/// 0x13000184[2]
		/// 1'b1: bus idle
		/// 1'b0: bus busy  
		if (MFG_READ32(MFG_DEBUG_STAT_REG) & MFG_BUS_IDLE_BIT)
		{
			/// printk("[MALI]MFG BUS already IDLE! Ready to power off, %d\n", polling_count);
			break;
		}
	} while (polling_count--);

	if (polling_count <=0)
	{
		printk("[MALI]!!!!MFG(GPU) subsys is still BUSY!!!!!, polling_count=%d\n", polling_count);
	}

	g_power_status = 0; // the power status is "power off".
	
	g_power_off_gpu_freq_idx = mt_gpufreq_get_cur_freq_index(); // record current freq. index.
	//printk("MALI:  GPU power off freq idx : %d\n",g_power_off_gpu_freq_idx );
#if 1
	uiCurrentFreqCount = mt_gpufreq_get_dvfs_table_num();       // get freq. table size 
	mt_gpufreq_target(uiCurrentFreqCount-1);                    // set gpu to lowest freq.
#endif

	code = mt_get_chip_hw_code();
	
	/* MTK clock modified */
	if (0x321 == code) {
		// do something for Denali-1(6735)
#ifdef CONFIG_MTK_CLKMGR
		disable_clock( MT_CG_MFG_BG3D, "GPU");
		disable_clock( MT_CG_DISP0_SMI_COMMON, "GPU");
#else	 
		clk_disable_unprepare(kbdev->clk_mfg);
		clk_disable_unprepare(kbdev->clk_mfg_scp);
		clk_disable_unprepare(kbdev->clk_smi_common);
		clk_disable_unprepare(kbdev->clk_display_scp);
#endif
	} else if (0x335 == code) {
		// do something for Denali-2(6735M)
#ifdef CONFIG_MTK_CLKMGR
		disable_clock( MT_CG_MFG_BG3D, "GPU");
		disable_clock( MT_CG_DISP0_SMI_COMMON, "GPU");
#endif /* CONFIG_MTK_CLKMGR */
	} else if (0x337 == code) {
		// do something for Denali-3(6753)
#ifdef CONFIG_MTK_CLKMGR
		disable_clock( MT_CG_MFG_BG3D, "GPU");
		disable_clock( MT_CG_DISP0_SMI_COMMON, "GPU");
#endif /* CONFIG_MTK_CLKMGR */
	} else {
		// unknown chip ID, error !!
#ifdef CONFIG_MTK_CLKMGR
		disable_clock( MT_CG_MFG_BG3D, "GPU");
		disable_clock( MT_CG_DISP0_SMI_COMMON, "GPU");
#endif /* CONFIG_MTK_CLKMGR */
	}

	mt_gpufreq_voltage_enable_set(0);
#ifdef ENABLE_COMMON_DVFS
    ged_dvfs_gpu_clock_switch_notify(0);
#endif
#endif
}

struct kbase_pm_callback_conf pm_callbacks = {
	.power_on_callback = pm_callback_power_on,
	.power_off_callback = pm_callback_power_off,
	.power_suspend_callback  = NULL,
	.power_resume_callback = NULL
};

static struct kbase_platform_config versatile_platform_config = {
#ifndef CONFIG_OF
	.io_resources = &io_resources
#endif
};

struct kbase_platform_config *kbase_get_platform_config(void)
{
	return &versatile_platform_config;
}


int kbase_platform_early_init(void)
{
	/* Nothing needed at this stage */
	return 0;
}
