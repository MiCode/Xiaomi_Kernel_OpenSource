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
#include "mali_kbase_cpu_mt6771.h"
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

enum g_bucks_state_enum {
	BUCKS_OFF = 0,
	BUCKS_ON,
};

static void *g_MFG_base;
static void *g_DBGAPB_base;
static void *g_INFRA_AO_base;
static void *g_TOPCKGEN_base;
static int g_curFreqID;
static enum g_bucks_state_enum g_bucks_state;
static bool g_queue_work_state;
static unsigned long g_idle_notifier_state;
static struct notifier_block gpu_idle_notifier;
static struct work_struct g_disable_bucks_work;
static struct workqueue_struct *g_bucks_workqueue;
static spinlock_t g_mfg_spinlock;

/**
 * For GPU idle check
 */
static void __mtk_check_MFG_idle(void)
{
	u32 val;

	/* MFG_QCHANNEL_CON (0x130000b4) bit [1:0] = 0x1 */
	writel(0x00000001, g_MFG_base + 0xb4);
	/* mali_pr_debug("@%s: 0x130000b4 val = 0x%x\n", __func__, readl(g_MFG_base + 0xb4)); */

	/* set register MFG_DEBUG_SEL (0x13000180) bit [7:0] = 0x03 */
	writel(0x00000003, g_MFG_base + 0x180);
	/* mali_pr_debug("@%s: 0x13000180 val = 0x%x\n", __func__, readl(g_MFG_base + 0x180)); */

	/* polling register MFG_DEBUG_TOP (0x13000188) bit 2 = 0x1 */
	/* => 1 for GPU (BUS) idle, 0 for GPU (BUS) non-idle */
	/* do not care about 0x13000184 */
	do {
		val = readl(g_MFG_base + 0x184);
		val = readl(g_MFG_base + 0x188);
		mali_pr_debug("@%s: 0x13000188 val = 0x%x\n", __func__, val);
	} while ((val & 0x4) != 0x4);
}

/**
 * check LCD state
 */
static unsigned int __mtk_check_LCD_state(void)
{
	unsigned long state = ged_query_info(GED_EVENT_STATUS);

	return (state & GED_EVENT_LCD) ? 1 : 0;
}

static int pm_callback_power_on_nolock(struct kbase_device *kbdev)
{
	unsigned long flags;

	if (mtk_get_vgpu_power_on_flag() == MTK_VGPU_POWER_ON)
		return 0;

	mali_pr_debug("@%s: power on ..., g_bucks_state = %d\n", __func__, g_bucks_state);

#ifdef MT_GPUFREQ_SRAM_DEBUG
	aee_rr_rec_gpu_dvfs_status(0x1 | (aee_rr_curr_gpu_dvfs_status() & 0xF0));
#endif

	if (g_bucks_state == BUCKS_OFF) {
		/* Turn on VGPU / VSRAM_GPU Buck */
		mt_gpufreq_voltage_enable_set(1);
		g_bucks_state = BUCKS_ON;
		spin_lock_irqsave(&g_mfg_spinlock, flags);
		g_queue_work_state = false;
		spin_unlock_irqrestore(&g_mfg_spinlock, flags);
		mali_pr_debug("@%s: enable VGPU / VSRAM_GPU bucks, g_bucks_state = %d\n", __func__, g_bucks_state);
	}

	/* Turn on GPU MTCMOS */
	mt_gpufreq_enable_MTCMOS();

#ifdef MT_GPUFREQ_SRAM_DEBUG
	aee_rr_rec_gpu_dvfs_status(0x2 | (aee_rr_curr_gpu_dvfs_status() & 0xF0));
#endif

	/* enable Clock Gating */
	mt_gpufreq_enable_CG();

#ifdef MT_GPUFREQ_SRAM_DEBUG
	aee_rr_rec_gpu_dvfs_status(0x3 | (aee_rr_curr_gpu_dvfs_status() & 0xF0));
#endif

	/* Write 1 into 0x13000130 bit 0 to enable timestamp register (TIMESTAMP).*/
	/* TIMESTAMP will be used by clGetEventProfilingInfo.*/
	writel(0x00000001, g_MFG_base + 0x130);

#ifdef MT_GPUFREQ_SRAM_DEBUG
	aee_rr_rec_gpu_dvfs_status(0x4 | (aee_rr_curr_gpu_dvfs_status() & 0xF0));
#endif

	/* Resume frequency before power off */
	mtk_set_vgpu_power_on_flag(MTK_VGPU_POWER_ON);
	mtk_set_mt_gpufreq_target(g_curFreqID);

#ifdef MT_GPUFREQ_SRAM_DEBUG
	aee_rr_rec_gpu_dvfs_status(0x5 | (aee_rr_curr_gpu_dvfs_status() & 0xF0));
#endif

#ifdef ENABLE_COMMON_DVFS
	ged_dvfs_gpu_clock_switch_notify(1);
#endif /* ENABLE_COMMON_DVFS */

	return 1;
}

static void pm_callback_power_off_nolock(struct kbase_device *kbdev)
{
	if (mtk_get_vgpu_power_on_flag() == MTK_VGPU_POWER_OFF)
		return;

	mali_pr_debug("@%s: power off ..., g_bucks_state = %d\n", __func__, g_bucks_state);

#ifdef MT_GPUFREQ_SRAM_DEBUG
	aee_rr_rec_gpu_dvfs_status(0x6 | (aee_rr_curr_gpu_dvfs_status() & 0xF0));
#endif

#ifdef ENABLE_COMMON_DVFS
	ged_dvfs_gpu_clock_switch_notify(0);
#endif /* ENABLE_COMMON_DVFS */

#ifdef MT_GPUFREQ_SRAM_DEBUG
	aee_rr_rec_gpu_dvfs_status(0x7 | (aee_rr_curr_gpu_dvfs_status() & 0xF0));
#endif

	mtk_set_vgpu_power_on_flag(MTK_VGPU_POWER_OFF);
	g_curFreqID = mtk_get_ged_dvfs_last_commit_idx();

#ifdef MT_GPUFREQ_SRAM_DEBUG
	aee_rr_rec_gpu_dvfs_status(0x8 | (aee_rr_curr_gpu_dvfs_status() & 0xF0));
#endif

	__mtk_check_MFG_idle();

#ifdef MT_GPUFREQ_SRAM_DEBUG
	aee_rr_rec_gpu_dvfs_status(0x9 | (aee_rr_curr_gpu_dvfs_status() & 0xF0));
#endif

	/* disable Clock Gating */
	mt_gpufreq_disable_CG();

#ifdef MT_GPUFREQ_SRAM_DEBUG
	aee_rr_rec_gpu_dvfs_status(0xA | (aee_rr_curr_gpu_dvfs_status() & 0xF0));
#endif

	/* Turn off GPU MTCMOS */
	mt_gpufreq_disable_MTCMOS();
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
	mutex_lock(&g_mfg_lock);

	mali_pr_debug("@%s: power suspend ..., g_bucks_state = %d\n", __func__, g_bucks_state);

#ifdef MT_GPUFREQ_SRAM_DEBUG
	aee_rr_rec_gpu_dvfs_status(0xB | (aee_rr_curr_gpu_dvfs_status() & 0xF0));
#endif

	pm_callback_power_off_nolock(kbdev);

	if (mtk_get_vgpu_power_on_flag() == MTK_VGPU_POWER_OFF
			&& __mtk_check_LCD_state() == 0
			&& g_bucks_state == BUCKS_ON) {
		/* Turn off VGPU / VSRAM_GPU Buck */
		mt_gpufreq_voltage_enable_set(0);
		g_bucks_state = BUCKS_OFF;
		mali_pr_debug("@%s: disable VGPU / VSRAM_GPU bucks, g_bucks_state = %d\n", __func__, g_bucks_state);
	}

	mutex_unlock(&g_mfg_lock);
}

void pm_callback_power_resume(struct kbase_device *kbdev)
{
	/* do-nothing */
}

static int gpu_idle_notifier_callback(struct notifier_block *nfb, unsigned long id, void *arg)
{
	unsigned long flags;

	switch (id) {
	case NOTIFY_DPIDLE_ENTER:
	case NOTIFY_SOIDLE_ENTER:
		spin_lock_irqsave(&g_mfg_spinlock, flags);
		if (mtk_get_vgpu_power_on_flag() == MTK_VGPU_POWER_OFF
				&& __mtk_check_LCD_state() == 0
				&& g_bucks_state == BUCKS_ON
				&& g_queue_work_state == false) {
			queue_work(g_bucks_workqueue, &g_disable_bucks_work);
			g_queue_work_state = true;
			g_idle_notifier_state = id;
			mali_pr_debug("@%s: queue_work [DPIDLE | SOIDLE], g_idle_notifier_state = %lu, g_bucks_state = %d\n",
					__func__, g_idle_notifier_state, g_bucks_state);
		}
		spin_unlock_irqrestore(&g_mfg_spinlock, flags);
		break;
	case NOTIFY_SOIDLE3_ENTER:
		/* don't care about LCD on / off status */
		spin_lock_irqsave(&g_mfg_spinlock, flags);
		if (mtk_get_vgpu_power_on_flag() == MTK_VGPU_POWER_OFF
				&& g_bucks_state == BUCKS_ON
				&& g_queue_work_state == false) {
			queue_work(g_bucks_workqueue, &g_disable_bucks_work);
			g_queue_work_state = true;
			g_idle_notifier_state = id;
			mali_pr_debug("@%s: queue_work [SOIDLE3], g_idle_notifier_state = %lu, g_bucks_state = %d\n",
					__func__, g_idle_notifier_state, g_bucks_state);
		}
		spin_unlock_irqrestore(&g_mfg_spinlock, flags);
		break;
	case NOTIFY_DPIDLE_LEAVE:
	case NOTIFY_SOIDLE_LEAVE:
	case NOTIFY_SOIDLE3_LEAVE:
		/* do-nothing */
	default:
		/* do-nothing */
		break;
	}

	return NOTIFY_OK;
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

static void __disable_bucks_work_cb(struct work_struct *work)
{
	unsigned long flags;

	mutex_lock(&g_mfg_lock);

	if (mtk_get_vgpu_power_on_flag() == MTK_VGPU_POWER_OFF
			&& g_bucks_state == BUCKS_ON) {
		if ((g_idle_notifier_state != NOTIFY_SOIDLE3_ENTER && __mtk_check_LCD_state() == 0)
				|| g_idle_notifier_state == NOTIFY_SOIDLE3_ENTER) {
			/* Turn off VGPU / VSRAM_GPU Buck */
			mt_gpufreq_voltage_enable_set(0);
			g_bucks_state = BUCKS_OFF;
			mali_pr_debug("@%s: disable VGPU / VSRAM_GPU bucks, g_idle_notifier_state = %lu, g_bucks_state = %d\n",
					__func__, g_idle_notifier_state, g_bucks_state);
		}
	} else {
		/* skip this work, so give chance to the next queue_work */
		spin_lock_irqsave(&g_mfg_spinlock, flags);
		g_queue_work_state = false;
		spin_unlock_irqrestore(&g_mfg_spinlock, flags);
		mali_pr_debug("@%s: skip this work, g_bucks_state = %d\n", __func__, g_bucks_state);
	}

	mutex_unlock(&g_mfg_lock);
}

int mtk_platform_init(struct platform_device *pdev, struct kbase_device *kbdev)
{

	if (!pdev || !kbdev) {
		mali_pr_info("@%s: input parameter is NULL\n", __func__);
		return -1;
	}

	g_bucks_state = BUCKS_ON;
	g_queue_work_state = false;

	g_bucks_workqueue = create_workqueue("gpu_bucks");
	if (!g_bucks_workqueue)
		mali_pr_info("@%s: Failed to create g_bucks_workqueue\n", __func__);

	INIT_WORK(&g_disable_bucks_work, __disable_bucks_work_cb);

	spin_lock_init(&g_mfg_spinlock);

	gpu_idle_notifier.notifier_call = gpu_idle_notifier_callback;
	mtk_idle_notifier_register(&gpu_idle_notifier);

	g_MFG_base = _mtk_of_ioremap("mediatek,mfgcfg", 0);
	if (g_MFG_base == NULL) {
		mali_pr_info("@%s: fail to remap MGFCFG register\n", __func__);
		return -1;
	}

	g_INFRA_AO_base = _mtk_of_ioremap("mediatek,mfgcfg", 1);
	if (g_INFRA_AO_base == NULL) {
		mali_pr_info("@%s: fail to remap INFRA_AO register\n", __func__);
		return -1;
	}

	g_DBGAPB_base = _mtk_of_ioremap("mediatek,mfgcfg", 4);
	if (g_DBGAPB_base == NULL) {
		mali_pr_info("@%s: fail to remap DBGAPB register\n", __func__);
		return -1;
	}

	g_TOPCKGEN_base = _mtk_of_ioremap("mediatek,topckgen", 0);
	if (g_TOPCKGEN_base == NULL) {
		mali_pr_info("@%s: fail to remap TOPCKGEN register\n", __func__);
		return -1;
	}

	mali_pr_info("@%s: initialize successfully\n", __func__);

	return 0;
}
