// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
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
#include "mali_kbase_cpu_mt6768.h"
#include "mali_kbase_config_platform.h"
#include "platform/mtk_platform_common.h"
#include "ged_dvfs.h"
#include "mtk_gpufreq.h"
#include <mtk_gpu_log.h>
#include "mtk_idle.h"

#define MALI_TAG						"[GPU/MALI]"
#define mali_pr_info(fmt, args...)		pr_info(MALI_TAG"[INFO]"fmt, ##args)
#define mali_pr_debug(fmt, args...)		pr_debug(MALI_TAG"[DEBUG]"fmt, ##args)

DEFINE_MUTEX(g_mfg_lock);

static void *g_MFG_base;
static int g_curFreqID;

/**
 * For GPU idle check
 */
static void __mtk_check_MFG_idle(void)
{
	u32 val;

	/* MFG_QCHANNEL_CON (0x130000b4) bit [1:0] = 0x1 */
	writel(0x00000001, g_MFG_base + 0xb4);
	mali_pr_debug("@%s: 0x130000b4 val = 0x%x\n", __func__, readl(g_MFG_base + 0xb4));

	/* set register MFG_DEBUG_SEL (0x13000170) bit [7:0] = 0x03 */
	writel(0x00000003, g_MFG_base + 0x170);
	mali_pr_debug("@%s: 0x13000170 val = 0x%x\n", __func__, readl(g_MFG_base + 0x170));

	/* polling register MFG_DEBUG_TOP (0x13000178) bit 2 = 0x1 */
	/* => 1 for GPU (BUS) idle, 0 for GPU (BUS) non-idle */
	/* do not care about 0x13000174 */
	do {
		val = readl(g_MFG_base + 0x178);
		mali_pr_debug("@%s: 0x13000178 val = 0x%x\n", __func__, val);
	} while ((val & 0x4) != 0x4);
}

static void __mtk_enable_MFG_internal_CG(void)
{
	u32 val;

	/* MFG_GLOBAL_CON: 0x1300_00b0 bit [8] = 0x0 */
	/* MFG_GLOBAL_CON: 0x1300_00b0 bit [10] = 0x0 */
	mali_pr_debug("@%s: 0x1300_00b0 = 0x%x\n", __func__, readl(g_MFG_base + 0xb0));
	val = readl(g_MFG_base + 0xb0);
	val &= ~(1UL << 8);
	val &= ~(1UL << 10);
	writel(val, g_MFG_base + 0xb0);
	mali_pr_debug("@%s: 0x1300_00b0 = 0x%x\n", __func__, readl(g_MFG_base + 0xb0));

	/* MFG_ASYNC_CON: 0x1300_0020 bit [25:22] = 0xF */
	mali_pr_debug("@%s: 0x1300_0020 = 0x%x\n", __func__, readl(g_MFG_base + 0x20));
	writel(readl(g_MFG_base + 0x20) | (0xF << 22), g_MFG_base + 0x20);
	mali_pr_debug("@%s: 0x1300_0020 = 0x%x\n", __func__, readl(g_MFG_base + 0x20));

	/* MFG_ASYNC_CON_1: 0x1300_0024 bit [0] = 0x1 */
	mali_pr_debug("@%s: 0x1300_0024 = 0x%x\n", __func__, readl(g_MFG_base + 0x24));
	writel(readl(g_MFG_base + 0x24) | (1UL), g_MFG_base + 0x24);
	mali_pr_debug("@%s: 0x1300_0024 = 0x%x\n", __func__, readl(g_MFG_base + 0x24));
}

static inline void gpu_dvfs_status_reset_footprint(void)
{
#ifdef MT_GPUFREQ_SRAM_DEBUG
	aee_rr_rec_gpu_dvfs_status(0);
#endif
}

static int pm_callback_power_on_nolock(struct kbase_device *kbdev)
{
	if (mtk_common_pm_is_mfg_active())
		return 0;

	mali_pr_debug("@%s: power on ...\n", __func__);

#ifdef MT_GPUFREQ_SRAM_DEBUG
	aee_rr_rec_gpu_dvfs_status(0x1 | (aee_rr_curr_gpu_dvfs_status() & 0xF0));
#endif

	/* Turn on VGPU / VSRAM_GPU Buck */
	mt_gpufreq_voltage_enable_set(1);

#ifdef MT_GPUFREQ_SRAM_DEBUG
	aee_rr_rec_gpu_dvfs_status(0x2 | (aee_rr_curr_gpu_dvfs_status() & 0xF0));
#endif

	/* Turn on GPU MTCMOS */
	mt_gpufreq_enable_MTCMOS();

#ifdef MT_GPUFREQ_SRAM_DEBUG
	aee_rr_rec_gpu_dvfs_status(0x3 | (aee_rr_curr_gpu_dvfs_status() & 0xF0));
#endif

	/* enable Clock Gating */
	mt_gpufreq_enable_CG();

#ifdef MT_GPUFREQ_SRAM_DEBUG
	aee_rr_rec_gpu_dvfs_status(0x4 | (aee_rr_curr_gpu_dvfs_status() & 0xF0));
#endif

	__mtk_enable_MFG_internal_CG();

#ifdef MT_GPUFREQ_SRAM_DEBUG
	aee_rr_rec_gpu_dvfs_status(0x5 | (aee_rr_curr_gpu_dvfs_status() & 0xF0));
 #endif

	/* Write 1 into 0x13000130 bit 0 to enable timestamp register (TIMESTAMP).*/
	/* TIMESTAMP will be used by clGetEventProfilingInfo.*/
	writel(0x00000003, g_MFG_base + 0x130);

#ifdef MT_GPUFREQ_SRAM_DEBUG
	aee_rr_rec_gpu_dvfs_status(0x6 | (aee_rr_curr_gpu_dvfs_status() & 0xF0));
#endif

	/* Resume frequency before power off */
	mtk_common_pm_mfg_active();
	mtk_common_gpufreq_commit(g_curFreqID);

#ifdef MT_GPUFREQ_SRAM_DEBUG
	aee_rr_rec_gpu_dvfs_status(0x7 | (aee_rr_curr_gpu_dvfs_status() & 0xF0));
#endif

#if IS_ENABLED(CONFIG_MALI_MIDGARD_DVFS) && IS_ENABLED(CONFIG_MTK_GPU_COMMON_DVFS)
	ged_dvfs_gpu_clock_switch_notify(1);
#endif /* ENABLE_COMMON_DVFS */

	return 1;
}

static void pm_callback_power_off_nolock(struct kbase_device *kbdev)
{
	if (!mtk_common_pm_is_mfg_active())
		return;

	mali_pr_debug("@%s: power off ...\n", __func__);

#ifdef MT_GPUFREQ_SRAM_DEBUG
	aee_rr_rec_gpu_dvfs_status(0x8 | (aee_rr_curr_gpu_dvfs_status() & 0xF0));
#endif

#if IS_ENABLED(CONFIG_MALI_MIDGARD_DVFS) && IS_ENABLED(CONFIG_MTK_GPU_COMMON_DVFS)
	ged_dvfs_gpu_clock_switch_notify(0);
#endif /* ENABLE_COMMON_DVFS */

#ifdef MT_GPUFREQ_SRAM_DEBUG
	aee_rr_rec_gpu_dvfs_status(0x9 | (aee_rr_curr_gpu_dvfs_status() & 0xF0));
#endif

	mtk_common_pm_mfg_idle();
	g_curFreqID = mtk_common_ged_dvfs_get_last_commit_idx();

#ifdef MT_GPUFREQ_SRAM_DEBUG
	aee_rr_rec_gpu_dvfs_status(0xA | (aee_rr_curr_gpu_dvfs_status() & 0xF0));
#endif

	__mtk_check_MFG_idle();

#ifdef MT_GPUFREQ_SRAM_DEBUG
	aee_rr_rec_gpu_dvfs_status(0xB | (aee_rr_curr_gpu_dvfs_status() & 0xF0));
#endif

	/* disable Clock Gating */
	mt_gpufreq_disable_CG();

#ifdef MT_GPUFREQ_SRAM_DEBUG
	aee_rr_rec_gpu_dvfs_status(0xC | (aee_rr_curr_gpu_dvfs_status() & 0xF0));
#endif

	/* Turn off GPU MTCMOS */
	mt_gpufreq_disable_MTCMOS();

#ifdef MT_GPUFREQ_SRAM_DEBUG
	aee_rr_rec_gpu_dvfs_status(0xD | (aee_rr_curr_gpu_dvfs_status() & 0xF0));
#endif

	/* Turn off VGPU / VSRAM_GPU Buck */
	mt_gpufreq_voltage_enable_set(0);

#ifdef MT_GPUFREQ_SRAM_DEBUG
	aee_rr_rec_gpu_dvfs_status(0xE | (aee_rr_curr_gpu_dvfs_status() & 0xF0));
#endif
}

static int pm_callback_power_on(struct kbase_device *kbdev)
{
	int ret = 0;

	mutex_lock(&g_mfg_lock);
	ret = pm_callback_power_on_nolock(kbdev);
	mutex_unlock(&g_mfg_lock);

	return ret;
}

static void pm_callback_power_off(struct kbase_device *kbdev)
{
	mutex_lock(&g_mfg_lock);
	pm_callback_power_off_nolock(kbdev);
	mutex_unlock(&g_mfg_lock);
}

void pm_callback_power_suspend(struct kbase_device *kbdev)
{
	/* do-nothing */
}

void pm_callback_power_resume(struct kbase_device *kbdev)
{
	/* do-nothing */
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
#endif /* CONFIG_OF */

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
 * MTK internal io map function
 *
 */
static void *_mtk_of_ioremap(const char *node_name, int idx)
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


int mtk_platform_device_init(struct kbase_device *kbdev)
{

	if (!kbdev) {
		mali_pr_info("@%s: kbdev is NULL\n", __func__);
		return -1;
	}

	gpu_dvfs_status_reset_footprint();

	g_MFG_base = _mtk_of_ioremap("mediatek,mfgcfg", 0);
	if (g_MFG_base == NULL) {
		mali_pr_info("@%s: fail to remap MGFCFG register\n", __func__);
		return -1;
	}

	mali_pr_info("@%s: initialize successfully\n", __func__);
	return 0;
}

int mtk_platform_init(struct platform_device *pdev, struct kbase_device *kbdev)
{

	if (!pdev || !kbdev) {
		mali_pr_info("@%s: input parameter is NULL\n", __func__);
		return -1;
	}

	g_MFG_base = _mtk_of_ioremap("mediatek,mfgcfg", 0);
	if (g_MFG_base == NULL) {
		mali_pr_info("@%s: fail to remap MGFCFG register\n", __func__);
		return -1;
	}

	mali_pr_info("@%s: initialize successfully\n", __func__);

	return 0;
}


void mtk_platform_device_term(struct kbase_device *kbdev) { }
