/*
 *
 * (C) COPYRIGHT 2011-2017 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can access it online at
 * http://www.gnu.org/licenses/gpl-2.0.html.
 *
 * SPDX-License-Identifier: GPL-2.0
 *
 */

#include <linux/ioport.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <mali_kbase.h>
#include <mali_kbase_defs.h>
#include <mali_kbase_config.h>
#include "mali_kbase_cpu_mt6877.h"
#include "mali_kbase_config_platform.h"
#include "platform/mtk_platform_common.h"
#include "ged_dvfs.h"
#include "mtk_gpufreq.h"
#include "mtk_idle.h"
#ifdef CONFIG_MTK_GPU_SWPM_SUPPORT
#include <mtk_gpu_power_sspm_ipi.h>
#endif

#define MALI_TAG				"[GPU/MALI]"
#define mali_pr_info(fmt, args...)		pr_info(MALI_TAG"[INFO]"fmt, ##args)
#define mali_pr_debug(fmt, args...)		pr_debug(MALI_TAG"[DEBUG]"fmt, ##args)

DEFINE_MUTEX(g_mfg_lock);

//FIXME
static int g_curFreqID;
static int g_is_suspend;

enum gpu_dvfs_status_step {
	GPU_DVFS_STATUS_STEP_1 = 0x1,
	GPU_DVFS_STATUS_STEP_2 = 0x2,
	GPU_DVFS_STATUS_STEP_3 = 0x3,
	GPU_DVFS_STATUS_STEP_4 = 0x4,
	GPU_DVFS_STATUS_STEP_5 = 0x5,
	GPU_DVFS_STATUS_STEP_6 = 0x6,
	GPU_DVFS_STATUS_STEP_7 = 0x7,
	GPU_DVFS_STATUS_STEP_8 = 0x8,
	GPU_DVFS_STATUS_STEP_9 = 0x9,
	GPU_DVFS_STATUS_STEP_A = 0xA,
	GPU_DVFS_STATUS_STEP_B = 0xB,
	GPU_DVFS_STATUS_STEP_C = 0xC,
	GPU_DVFS_STATUS_STEP_D = 0xD,
	GPU_DVFS_STATUS_STEP_E = 0xE,
	GPU_DVFS_STATUS_STEP_F = 0xF,
};

static inline void gpu_dvfs_status_footprint(enum gpu_dvfs_status_step step)
{
#ifdef CONFIG_MTK_RAM_CONSOLE
	aee_rr_rec_gpu_dvfs_status(step |
				(aee_rr_curr_gpu_dvfs_status() & 0xF0));
#endif
}

static inline void gpu_dvfs_status_reset_footprint(void)
{
#ifdef CONFIG_MTK_RAM_CONSOLE
	aee_rr_rec_gpu_dvfs_status(0);
#endif
}

static int pm_callback_power_on_nolock(struct kbase_device *kbdev)
{
	if (mt_gpufreq_bringup()) {
		mtk_set_vgpu_power_on_flag(MTK_VGPU_POWER_ON);
		return 1;
	}

	if (!mt_gpufreq_power_ctl_en()) {
		mtk_set_vgpu_power_on_flag(MTK_VGPU_POWER_ON);
		ged_dvfs_gpu_clock_switch_notify(1);
		return 1;
	}

	if (mtk_get_vgpu_power_on_flag() == MTK_VGPU_POWER_ON)
		return 0;

	mali_pr_debug("@%s: power on ...\n", __func__);

	gpu_dvfs_status_footprint(GPU_DVFS_STATUS_STEP_1);

	if (g_is_suspend == 1) {
		mali_pr_info("@%s: discard powering on since GPU is suspended\n", __func__);
		return 0;
	}

	/* on,off/ SWCG(BG3D)/ MTCMOS/ BUCK */
	mt_gpufreq_power_control(POWER_ON, CG_ON, MTCMOS_ON, BUCK_ON);

	gpu_dvfs_status_footprint(GPU_DVFS_STATUS_STEP_2);

	mt_gpufreq_set_timestamp();

	mt_gpufreq_set_gpm();

	/* set a flag to enable GPU DVFS */
	mtk_set_vgpu_power_on_flag(MTK_VGPU_POWER_ON);

	gpu_dvfs_status_footprint(GPU_DVFS_STATUS_STEP_3);

	/* resume frequency */
	mtk_set_mt_gpufreq_target(g_curFreqID);

	gpu_dvfs_status_footprint(GPU_DVFS_STATUS_STEP_4);

#ifdef ENABLE_COMMON_DVFS
	ged_dvfs_gpu_clock_switch_notify(1);
#endif

	gpu_dvfs_status_footprint(GPU_DVFS_STATUS_STEP_5);

	return 1;
}

static void pm_callback_power_off_nolock(struct kbase_device *kbdev)
{
	if (mt_gpufreq_bringup())
		return;

	if (!mt_gpufreq_power_ctl_en())
		return;

	if (mtk_get_vgpu_power_on_flag() == MTK_VGPU_POWER_OFF)
		return;

	mali_pr_debug("@%s: power off ...\n", __func__);

	gpu_dvfs_status_footprint(GPU_DVFS_STATUS_STEP_6);

#ifdef ENABLE_COMMON_DVFS
	ged_dvfs_gpu_clock_switch_notify(0);
#endif

	gpu_dvfs_status_footprint(GPU_DVFS_STATUS_STEP_7);

	/* set a flag to disable GPU DVFS */
	mtk_set_vgpu_power_on_flag(MTK_VGPU_POWER_OFF);

	gpu_dvfs_status_footprint(GPU_DVFS_STATUS_STEP_8);

	/* suspend frequency */
	g_curFreqID = mtk_get_ged_dvfs_last_commit_idx();

	gpu_dvfs_status_footprint(GPU_DVFS_STATUS_STEP_9);

	/* check MFG bus if idle */
	mt_gpufreq_check_bus_idle();

	gpu_dvfs_status_footprint(GPU_DVFS_STATUS_STEP_A);

	/* on,off/ SWCG(BG3D)/ MTCMOS/ BUCK */
	mt_gpufreq_power_control(POWER_OFF, CG_OFF, MTCMOS_OFF, BUCK_OFF);

	gpu_dvfs_status_footprint(GPU_DVFS_STATUS_STEP_B);
}

static int pm_callback_power_on(struct kbase_device *kbdev)
{
	int ret = 0;

	mutex_lock(&g_mfg_lock);

	ret = pm_callback_power_on_nolock(kbdev);
#ifdef CONFIG_MTK_GPU_SWPM_SUPPORT
	MTKGPUPower_model_resume();
#endif
	mutex_unlock(&g_mfg_lock);

	return ret;
}

static void pm_callback_power_off(struct kbase_device *kbdev)
{
	mutex_lock(&g_mfg_lock);
#ifdef CONFIG_MTK_GPU_SWPM_SUPPORT
	MTKGPUPower_model_suspend();
#endif
	pm_callback_power_off_nolock(kbdev);
	mutex_unlock(&g_mfg_lock);
}

static void pm_callback_power_suspend(struct kbase_device *kbdev)
{
	mutex_lock(&g_mfg_lock);

	if (mtk_get_vgpu_power_on_flag() == MTK_VGPU_POWER_ON) {
		pm_callback_power_off_nolock(kbdev);
		mali_pr_info("@%s: force powering off GPU\n", __func__);
	}
	g_is_suspend = 1;
	mali_pr_info("@%s: gpu_suspend\n", __func__);

	gpu_dvfs_status_footprint(GPU_DVFS_STATUS_STEP_E);

	mutex_unlock(&g_mfg_lock);
}

static void pm_callback_power_resume(struct kbase_device *kbdev)
{
	mutex_lock(&g_mfg_lock);

	g_is_suspend = 0;
	mali_pr_info("@%s: gpu_resume\n", __func__);

	gpu_dvfs_status_footprint(GPU_DVFS_STATUS_STEP_F);

	mutex_unlock(&g_mfg_lock);
}

struct kbase_pm_callback_conf pm_callbacks = {
	.power_on_callback = pm_callback_power_on,
	.power_off_callback = pm_callback_power_off,
	.power_suspend_callback  = pm_callback_power_suspend,
	.power_resume_callback = pm_callback_power_resume,
};

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
#endif

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

int mtk_platform_init(struct kbase_device *kbdev)
{

	if (!kbdev) {
		mali_pr_info("@%s: input parameter is NULL\n", __func__);
		return -1;
	}

	gpu_dvfs_status_reset_footprint();

	//FIXME
	g_is_suspend = -1;

	mali_pr_info("@%s: initialize successfully\n", __func__);

	return 0;
}
