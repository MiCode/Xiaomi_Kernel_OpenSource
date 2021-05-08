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
#include "mali_kbase_cpu_mt6785.h"
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

static void *g_MFG_base;
static int g_curFreqID;
static int g_is_suspend;

static void __mtk_check_MFG_bus_idle(void)
{
	u32 val;

	/* MFG_QCHANNEL_CON (0x13fb_f0b4) bit [1:0] = 0x1 */
	writel(0x00000001, g_MFG_base + 0xb4);
	mali_pr_debug("@%s: 0x13fb_f0b4 val = 0x%x\n", __func__, readl(g_MFG_base + 0xb4));

	/* set register MFG_DEBUG_SEL (0x13fb_f170) bit [7:0] = 0x03 */
	writel(0x00000003, g_MFG_base + 0x170);
	mali_pr_debug("@%s: 0x13fb_f170 val = 0x%x\n", __func__, readl(g_MFG_base + 0x170));

	/* polling register MFG_DEBUG_TOP (0x13fb_f178) bit 2 = 0x1 */
	/* => 1 for bus idle, 0 for bus non-idle */
	do {
		val = readl(g_MFG_base + 0x178);
		mali_pr_debug("@%s: 0x13fb_f178 val = 0x%x\n", __func__, val);
	} while ((val & 0x4) != 0x4);
}

static void __mtk_set_MFG_HW_CG(void)
{
	u32 val;

	/* [F] & [G] adopt default setting because PD implementation issue */
	/* it should be free run */

	/* [K] MFG_GLOBAL_CON: 0x13fb_f0b0 bit [8] = 0x0 */
	/* [D] MFG_GLOBAL_CON: 0x13fb_f0b0 bit [10] = 0x0 */
	mali_pr_debug("@%s: 0x13fb_f0b0 = 0x%x\n", __func__, readl(g_MFG_base + 0xb0));
	val = readl(g_MFG_base + 0xb0);
	val &= ~(1UL << 8);
	val &= ~(1UL << 10);
	writel(val, g_MFG_base + 0xb0);
	mali_pr_debug("@%s: 0x13fb_f0b0 = 0x%x\n", __func__, readl(g_MFG_base + 0xb0));

	/* [H] MFG_ASYNC_CON: 0x13fb_f020 bit [23] = 0x1 */
	/* [I] MFG_ASYNC_CON: 0x13fb_f020 bit [25] = 0x1 */
	mali_pr_debug("@%s: 0x13fb_f020 = 0x%x\n", __func__, readl(g_MFG_base + 0x20));
	val = readl(g_MFG_base + 0x20);
	val |= (1UL << 23);
	val |= (1UL << 25);
	writel(val, g_MFG_base + 0x20);
	mali_pr_debug("@%s: 0x13fb_f020 = 0x%x\n", __func__, readl(g_MFG_base + 0x20));

	/* [J] MFG_ASYNC_CON_1: 0x13fb_f024 bit [0] = 0x1 */
	mali_pr_debug("@%s: 0x13fb_f024 = 0x%x\n", __func__, readl(g_MFG_base + 0x24));
	writel(readl(g_MFG_base + 0x24) | (1UL), g_MFG_base + 0x24);
	mali_pr_debug("@%s: 0x13fb_f024 = 0x%x\n", __func__, readl(g_MFG_base + 0x24));
}

static int pm_callback_power_on_nolock(struct kbase_device *kbdev)
{
#if MT_GPUFREQ_BRINGUP == 1
	mtk_set_vgpu_power_on_flag(MTK_VGPU_POWER_ON);
	return 1;
#endif

	if (mtk_get_vgpu_power_on_flag() == MTK_VGPU_POWER_ON)
		return 0;

	mali_pr_debug("@%s: power on ...\n", __func__);

#ifdef MT_GPUFREQ_SRAM_DEBUG
	aee_rr_rec_gpu_dvfs_status(
			0x1 | (aee_rr_curr_gpu_dvfs_status() & 0xF0));
#endif

	if (g_is_suspend == 1) {
		mali_pr_info("@%s: discard powering on since GPU is suspended\n", __func__);
		return 0;
	}

	/* power on BUCK / MTCMOS / SWCG(BG3D) */
	mt_gpufreq_power_control_enable(true, true, true);

#ifdef MT_GPUFREQ_SRAM_DEBUG
	aee_rr_rec_gpu_dvfs_status(
			0x2 | (aee_rr_curr_gpu_dvfs_status() & 0xF0));
#endif

	/* set HWCG(DCM) */
	__mtk_set_MFG_HW_CG();

#ifdef MT_GPUFREQ_SRAM_DEBUG
	aee_rr_rec_gpu_dvfs_status(
			0x3 | (aee_rr_curr_gpu_dvfs_status() & 0xF0));
#endif

	/* write 1 into 0x13fb_f130 bit 0 to enable timestamp register */
	/* timestamp will be used by clGetEventProfilingInfo*/
	writel(0x00000001, g_MFG_base + 0x130);

	/* set a flag to enable GPU DVFS */
	mtk_set_vgpu_power_on_flag(MTK_VGPU_POWER_ON);

#ifdef MT_GPUFREQ_SRAM_DEBUG
	aee_rr_rec_gpu_dvfs_status(
			0x4 | (aee_rr_curr_gpu_dvfs_status() & 0xF0));
#endif

	/* resume frequency */
	mtk_set_mt_gpufreq_target(g_curFreqID);

#ifdef MT_GPUFREQ_SRAM_DEBUG
	aee_rr_rec_gpu_dvfs_status(
			0x5 | (aee_rr_curr_gpu_dvfs_status() & 0xF0));
#endif

#ifdef ENABLE_COMMON_DVFS
	ged_dvfs_gpu_clock_switch_notify(1);
#endif

#ifdef MT_GPUFREQ_SRAM_DEBUG
	aee_rr_rec_gpu_dvfs_status(
			0x6 | (aee_rr_curr_gpu_dvfs_status() & 0xF0));
#endif

	return 1;
}

static void pm_callback_power_off_nolock(struct kbase_device *kbdev)
{
#if MT_GPUFREQ_BRINGUP == 1
	return ;
#endif

	if (mtk_get_vgpu_power_on_flag() == MTK_VGPU_POWER_OFF)
		return;

	mali_pr_debug("@%s: power off ...\n", __func__);

#ifdef MT_GPUFREQ_SRAM_DEBUG
	aee_rr_rec_gpu_dvfs_status(
			0x7 | (aee_rr_curr_gpu_dvfs_status() & 0xF0));
#endif

#ifdef ENABLE_COMMON_DVFS
	ged_dvfs_gpu_clock_switch_notify(0);
#endif

#ifdef MT_GPUFREQ_SRAM_DEBUG
	aee_rr_rec_gpu_dvfs_status(
			0x8 | (aee_rr_curr_gpu_dvfs_status() & 0xF0));
#endif

	/* set a flag to disable GPU DVFS */
	mtk_set_vgpu_power_on_flag(MTK_VGPU_POWER_OFF);

#ifdef MT_GPUFREQ_SRAM_DEBUG
	aee_rr_rec_gpu_dvfs_status(
			0x9 | (aee_rr_curr_gpu_dvfs_status() & 0xF0));
#endif

	/* suspend frequency */
	g_curFreqID = mtk_get_ged_dvfs_last_commit_idx();

#ifdef MT_GPUFREQ_SRAM_DEBUG
	aee_rr_rec_gpu_dvfs_status(
			0xA | (aee_rr_curr_gpu_dvfs_status() & 0xF0));
#endif

	/* check MFG bus if idle */
	__mtk_check_MFG_bus_idle();

#ifdef MT_GPUFREQ_SRAM_DEBUG
	aee_rr_rec_gpu_dvfs_status(
			0xB | (aee_rr_curr_gpu_dvfs_status() & 0xF0));
#endif

	/* power off SWCG(BG3D) / MTCMOS / BUCK */
	mt_gpufreq_power_control_disable(true, true, true);

#ifdef MT_GPUFREQ_SRAM_DEBUG
	aee_rr_rec_gpu_dvfs_status(
			0xC | (aee_rr_curr_gpu_dvfs_status() & 0xF0));
#endif
}

static int pm_callback_power_on(struct kbase_device *kbdev)
{
	int ret = 0;

	mutex_lock(&g_mfg_lock);
#ifdef CONFIG_MTK_GPU_SWPM_SUPPORT
	MTKGPUPower_model_resume();
#endif
	ret = pm_callback_power_on_nolock(kbdev);
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

#ifdef MT_GPUFREQ_SRAM_DEBUG
	aee_rr_rec_gpu_dvfs_vgpu(0x01);
#endif

	mutex_unlock(&g_mfg_lock);
}

static void pm_callback_power_resume(struct kbase_device *kbdev)
{
	mutex_lock(&g_mfg_lock);

	g_is_suspend = 0;
	mali_pr_info("@%s: gpu_resume\n", __func__);

#ifdef MT_GPUFREQ_SRAM_DEBUG
	aee_rr_rec_gpu_dvfs_vgpu(0x02);
#endif

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

/**
 * MTK internal ioremap function
 */
static void *__mtk_of_ioremap(const char *node_name, int idx)
{
	struct device_node *node;

	node = of_find_compatible_node(NULL, NULL, node_name);

	if (node)
		return of_iomap(node, idx);

	mali_pr_info("@%s: cannot find [%s] of_node\n", __func__, node_name);

	return NULL;
}

int kbase_platform_early_init(void)
{
	/* Nothing needed at this stage */
	return 0;
}

int mtk_platform_init(struct platform_device *pdev, struct kbase_device *kbdev)
{

	if (!pdev || !kbdev) {
		mali_pr_info("@%s: input parameter is NULL\n", __func__);
		return -1;
	}

	g_MFG_base = __mtk_of_ioremap("mediatek,mfgcfg", 0);
	if (g_MFG_base == NULL) {
		mali_pr_info("@%s: fail to remap MGFCFG register\n", __func__);
		return -1;
	}

	g_is_suspend = -1;

	mali_pr_info("@%s: initialize successfully\n", __func__);

	return 0;
}
