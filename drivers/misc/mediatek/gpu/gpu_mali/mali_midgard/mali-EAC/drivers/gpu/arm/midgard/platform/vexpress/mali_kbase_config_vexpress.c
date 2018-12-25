/*
 *
 * (C) COPYRIGHT ARM Limited. All rights reserved.
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

/* MTK clock modified */
#include "mt_gpufreq.h"
#include "upmu_common.h"
#include "upmu_sw.h"
#include "upmu_hw.h"
#include "mali_kbase_pm.h"

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

/* Versatile Express (VE) configuration defaults shared between config_attributes[]
 * and config_attributes_hw_issue_8408[]. Settings are not shared for
 * JS_HARD_STOP_TICKS_SS and JS_RESET_TICKS_SS.
 */
#define KBASE_VE_JS_SCHEDULING_TICK_NS_DEBUG    15000000u      /* 15ms, an agressive tick for testing purposes. This will reduce performance significantly */
#define KBASE_VE_JS_SOFT_STOP_TICKS_DEBUG       1	/* between 15ms and 30ms before soft-stop a job */
#define KBASE_VE_JS_SOFT_STOP_TICKS_CL_DEBUG    1	/* between 15ms and 30ms before soft-stop a CL job */
#define KBASE_VE_JS_HARD_STOP_TICKS_SS_DEBUG    333	/* 5s before hard-stop */
#define KBASE_VE_JS_HARD_STOP_TICKS_SS_8401_DEBUG 2000	/* 30s before hard-stop, for a certain GLES2 test at 128x128 (bound by combined vertex+tiler job) - for issue 8401 */
#define KBASE_VE_JS_HARD_STOP_TICKS_CL_DEBUG    166	/* 2.5s before hard-stop */
#define KBASE_VE_JS_HARD_STOP_TICKS_NSS_DEBUG   100000	/* 1500s (25mins) before NSS hard-stop */
#define KBASE_VE_JS_RESET_TICKS_SS_DEBUG        500	/* 45s before resetting GPU, for a certain GLES2 test at 128x128 (bound by combined vertex+tiler job) */
#define KBASE_VE_JS_RESET_TICKS_SS_8401_DEBUG   3000	/* 7.5s before resetting GPU - for issue 8401 */
#define KBASE_VE_JS_RESET_TICKS_CL_DEBUG        500	/* 45s before resetting GPU */
#define KBASE_VE_JS_RESET_TICKS_NSS_DEBUG       100166	/* 1502s before resetting GPU */

#define KBASE_VE_JS_SCHEDULING_TICK_NS          1250000000u	/* 1.25s */
#define KBASE_VE_JS_SOFT_STOP_TICKS             2	/* 2.5s before soft-stop a job */
#define KBASE_VE_JS_SOFT_STOP_TICKS_CL          1	/* 1.25s before soft-stop a CL job */
#define KBASE_VE_JS_HARD_STOP_TICKS_SS          4	/* 5s before hard-stop */
#define KBASE_VE_JS_HARD_STOP_TICKS_SS_8401     24	/* 30s before hard-stop, for a certain GLES2 test at 128x128 (bound by combined vertex+tiler job) - for issue 8401 */
#define KBASE_VE_JS_HARD_STOP_TICKS_CL          2	/* 2.5s before hard-stop */
#define KBASE_VE_JS_HARD_STOP_TICKS_NSS         1200	/* 1500s before NSS hard-stop */
#define KBASE_VE_JS_RESET_TICKS_SS              6	/* 7.5s before resetting GPU */
#define KBASE_VE_JS_RESET_TICKS_SS_8401         36	/* 45s before resetting GPU, for a certain GLES2 test at 128x128 (bound by combined vertex+tiler job) - for issue 8401 */
#define KBASE_VE_JS_RESET_TICKS_CL              3	/* 7.5s before resetting GPU */
#define KBASE_VE_JS_RESET_TICKS_NSS             1201	/* 1502s before resetting GPU */

#define KBASE_VE_JS_RESET_TIMEOUT_MS            3000	/* 3s before cancelling stuck jobs */
#define KBASE_VE_JS_CTX_TIMESLICE_NS            1000000	/* 1ms - an agressive timeslice for testing purposes (causes lots of scheduling out for >4 ctxs) */
#define KBASE_VE_POWER_MANAGEMENT_CALLBACKS     ((uintptr_t)&pm_callbacks)
#define KBASE_VE_CPU_SPEED_FUNC                 ((uintptr_t)&kbase_get_vexpress_cpu_clock_speed)

#define HARD_RESET_AT_POWER_OFF 1

#ifndef CONFIG_OF
static kbase_io_resources io_resources = {
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

static int pm_callback_power_on(struct kbase_device *kbdev)
{
	int touch_boost_flag, touch_boost_id;
    unsigned int current_gpu_freq_idx;
#ifndef CONFIG_MTK_CLKMGR
	int ret;
#endif

	unsigned int code = mt_get_chip_hw_code();

	mt_gpufreq_voltage_enable_set(1);
    
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
		pr_debug("[MALI] power off delay error!\n");
	}
      
	/// 2. Polling the MFG_DEBUG_REG for checking GPU IDLE before MTCMOS power off (0.1ms)
	MFG_WRITE32(0x3, MFG_DEBUG_CTRL_REG);

	do {
		/// 0x13000184[2]
		/// 1'b1: bus idle
		/// 1'b0: bus busy  
		if (MFG_READ32(MFG_DEBUG_STAT_REG) & MFG_BUS_IDLE_BIT)
		{
			/// pr_debug("[MALI]MFG BUS already IDLE! Ready to power off, %d\n", polling_count);
			break;
		}
	} while (polling_count--);

	if (polling_count <=0)
	{
		pr_debug("[MALI]!!!!MFG(GPU) subsys is still BUSY!!!!!, polling_count=%d\n", polling_count);
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
#endif

	///  Polling the MFG_DEBUG_REG for checking GPU IDLE before MTCMOS power off (0.1ms)
	MFG_WRITE32(0x3, MFG_DEBUG_CTRL_REG);

	do {
		/// 0x13000184[2]
		/// 1'b1: bus idle
		/// 1'b0: bus busy  
		if (MFG_READ32(MFG_DEBUG_STAT_REG) & MFG_BUS_IDLE_BIT)
		{
			/// pr_debug("[MALI]MFG BUS already IDLE! Ready to power off, %d\n", polling_count);
			break;
		}
	} while (polling_count--);

	if (polling_count <=0)
	{
		pr_debug("[MALI]!!!!MFG(GPU) subsys is still BUSY!!!!!, polling_count=%d\n", polling_count);
	}

	g_power_status = 0; // the power status is "power off".
	
    g_power_off_gpu_freq_idx = mt_gpufreq_get_cur_freq_index(); // record current freq. index.
    //pr_debug("MALI:  GPU power off freq idx : %d\n",g_power_off_gpu_freq_idx );
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

}

static struct kbase_pm_callback_conf pm_callbacks = {
	.power_on_callback = pm_callback_power_on,
	.power_off_callback = pm_callback_power_off,
	.power_suspend_callback  = NULL,
	.power_resume_callback = NULL
};

/* Please keep table config_attributes in sync with config_attributes_hw_issue_8408 */
static struct kbase_attribute config_attributes[] = {
#ifdef CONFIG_MALI_DEBUG
/* Use more aggressive scheduling timeouts in debug builds for testing purposes */
	{
	 KBASE_CONFIG_ATTR_JS_SCHEDULING_TICK_NS,
	 KBASE_VE_JS_SCHEDULING_TICK_NS_DEBUG},

	{
	 KBASE_CONFIG_ATTR_JS_SOFT_STOP_TICKS,
	 KBASE_VE_JS_SOFT_STOP_TICKS_DEBUG},

	{
	 KBASE_CONFIG_ATTR_JS_SOFT_STOP_TICKS_CL,
	 KBASE_VE_JS_SOFT_STOP_TICKS_CL_DEBUG},

	{
	 KBASE_CONFIG_ATTR_JS_HARD_STOP_TICKS_SS,
	 KBASE_VE_JS_HARD_STOP_TICKS_SS_DEBUG},

	{
	 KBASE_CONFIG_ATTR_JS_HARD_STOP_TICKS_CL,
	 KBASE_VE_JS_HARD_STOP_TICKS_CL_DEBUG},

	{
	 KBASE_CONFIG_ATTR_JS_HARD_STOP_TICKS_NSS,
	 KBASE_VE_JS_HARD_STOP_TICKS_NSS_DEBUG},

	{
	 KBASE_CONFIG_ATTR_JS_RESET_TICKS_SS,
	 KBASE_VE_JS_RESET_TICKS_SS_DEBUG},

	{
	 KBASE_CONFIG_ATTR_JS_RESET_TICKS_CL,
	 KBASE_VE_JS_RESET_TICKS_CL_DEBUG},

	{
	 KBASE_CONFIG_ATTR_JS_RESET_TICKS_NSS,
	 KBASE_VE_JS_RESET_TICKS_NSS_DEBUG},
#else				/* CONFIG_MALI_DEBUG */
/* In release builds same as the defaults but scaled for 5MHz FPGA */
	{
	 KBASE_CONFIG_ATTR_JS_SCHEDULING_TICK_NS,
	 KBASE_VE_JS_SCHEDULING_TICK_NS},

	{
	 KBASE_CONFIG_ATTR_JS_SOFT_STOP_TICKS,
	 KBASE_VE_JS_SOFT_STOP_TICKS},

	{
	 KBASE_CONFIG_ATTR_JS_SOFT_STOP_TICKS_CL,
	 KBASE_VE_JS_SOFT_STOP_TICKS_CL},

	{
	 KBASE_CONFIG_ATTR_JS_HARD_STOP_TICKS_SS,
	 KBASE_VE_JS_HARD_STOP_TICKS_SS},

	{
	 KBASE_CONFIG_ATTR_JS_HARD_STOP_TICKS_CL,
	 KBASE_VE_JS_HARD_STOP_TICKS_CL},

	{
	 KBASE_CONFIG_ATTR_JS_HARD_STOP_TICKS_NSS,
	 KBASE_VE_JS_HARD_STOP_TICKS_NSS},

	{
	 KBASE_CONFIG_ATTR_JS_RESET_TICKS_SS,
	 KBASE_VE_JS_RESET_TICKS_SS},

	{
	 KBASE_CONFIG_ATTR_JS_RESET_TICKS_CL,
	 KBASE_VE_JS_RESET_TICKS_CL},

	{
	 KBASE_CONFIG_ATTR_JS_RESET_TICKS_NSS,
	 KBASE_VE_JS_RESET_TICKS_NSS},
#endif				/* CONFIG_MALI_DEBUG */
	{
	 KBASE_CONFIG_ATTR_JS_RESET_TIMEOUT_MS,
	 KBASE_VE_JS_RESET_TIMEOUT_MS},

	{
	 KBASE_CONFIG_ATTR_JS_CTX_TIMESLICE_NS,
	 KBASE_VE_JS_CTX_TIMESLICE_NS},

	{
	 KBASE_CONFIG_ATTR_POWER_MANAGEMENT_CALLBACKS,
	 KBASE_VE_POWER_MANAGEMENT_CALLBACKS},

	{
	 KBASE_CONFIG_ATTR_CPU_SPEED_FUNC,
	 KBASE_VE_CPU_SPEED_FUNC},

	{
	 KBASE_CONFIG_ATTR_END,
	 0}
};

/* as config_attributes array above except with different settings for
 * JS_HARD_STOP_TICKS_SS, JS_RESET_TICKS_SS that
 * are needed for BASE_HW_ISSUE_8408.
 */
struct kbase_attribute config_attributes_hw_issue_8408[] = {
#ifdef CONFIG_MALI_DEBUG
/* Use more aggressive scheduling timeouts in debug builds for testing purposes */
	{
	 KBASE_CONFIG_ATTR_JS_SCHEDULING_TICK_NS,
	 KBASE_VE_JS_SCHEDULING_TICK_NS_DEBUG},

	{
	 KBASE_CONFIG_ATTR_JS_SOFT_STOP_TICKS,
	 KBASE_VE_JS_SOFT_STOP_TICKS_DEBUG},

	{
	 KBASE_CONFIG_ATTR_JS_HARD_STOP_TICKS_SS,
	 KBASE_VE_JS_HARD_STOP_TICKS_SS_8401_DEBUG},

	{
	 KBASE_CONFIG_ATTR_JS_HARD_STOP_TICKS_NSS,
	 KBASE_VE_JS_HARD_STOP_TICKS_NSS_DEBUG},

	{
	 KBASE_CONFIG_ATTR_JS_RESET_TICKS_SS,
	 KBASE_VE_JS_RESET_TICKS_SS_8401_DEBUG},

	{
	 KBASE_CONFIG_ATTR_JS_RESET_TICKS_NSS,
	 KBASE_VE_JS_RESET_TICKS_NSS_DEBUG},
#else				/* CONFIG_MALI_DEBUG */
/* In release builds same as the defaults but scaled for 5MHz FPGA */
	{
	 KBASE_CONFIG_ATTR_JS_SCHEDULING_TICK_NS,
	 KBASE_VE_JS_SCHEDULING_TICK_NS},

	{
	 KBASE_CONFIG_ATTR_JS_SOFT_STOP_TICKS,
	 KBASE_VE_JS_SOFT_STOP_TICKS},

	{
	 KBASE_CONFIG_ATTR_JS_HARD_STOP_TICKS_SS,
	 KBASE_VE_JS_HARD_STOP_TICKS_SS_8401},

	{
	 KBASE_CONFIG_ATTR_JS_HARD_STOP_TICKS_NSS,
	 KBASE_VE_JS_HARD_STOP_TICKS_NSS},

	{
	 KBASE_CONFIG_ATTR_JS_RESET_TICKS_SS,
	 KBASE_VE_JS_RESET_TICKS_SS_8401},

	{
	 KBASE_CONFIG_ATTR_JS_RESET_TICKS_NSS,
	 KBASE_VE_JS_RESET_TICKS_NSS},
#endif				/* CONFIG_MALI_DEBUG */
	{
	 KBASE_CONFIG_ATTR_JS_RESET_TIMEOUT_MS,
	 KBASE_VE_JS_RESET_TIMEOUT_MS},

	{
	 KBASE_CONFIG_ATTR_JS_CTX_TIMESLICE_NS,
	 KBASE_VE_JS_CTX_TIMESLICE_NS},

	{
	 KBASE_CONFIG_ATTR_POWER_MANAGEMENT_CALLBACKS,
	 KBASE_VE_POWER_MANAGEMENT_CALLBACKS},

	{
	 KBASE_CONFIG_ATTR_CPU_SPEED_FUNC,
	 KBASE_VE_CPU_SPEED_FUNC},

	{
	 KBASE_CONFIG_ATTR_END,
	 0}
};

static struct kbase_platform_config versatile_platform_config = {
	.attributes = config_attributes,
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
