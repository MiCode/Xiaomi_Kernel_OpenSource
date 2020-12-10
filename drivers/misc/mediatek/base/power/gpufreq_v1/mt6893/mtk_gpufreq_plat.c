/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

/**
 * @file    mtk_gpufreq_plat.c
 * @brief   Driver for GPU-DVFS
 */

/**
 * ===============================================
 * SECTION : Include files
 * ===============================================
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/uaccess.h>
#include <linux/random.h>

#include "mtk_gpufreq.h"
#include "mtk_gpufreq_internal.h"
#include "mtk_gpufreq_common.h"

#include "clk-fmeter.h"

#include "mtk_pmic_wrap.h"
#include "mtk_devinfo.h"
#include "upmu_common.h"
#include "upmu_sw.h"
#include "upmu_hw.h"
#ifdef CONFIG_THERMAL
#include "mtk_thermal.h"
#endif
#ifdef CONFIG_MTK_FREQ_HOPPING
#include "mtk_freqhopping_drv.h"
#endif
#if MT_GPUFREQ_KICKER_PBM_READY
#include "mtk_pbm.h"
#endif
#if MT_GPUFREQ_STATIC_PWR_READY2USE
#include "mtk_static_power.h"
#endif
#ifdef CONFIG_MTK_GPU_SUPPORT
#include "ged_log.h"
#include "ged_base.h"
#endif
#include "mtk_gpu_utility.h"

#ifdef CONFIG_MTK_GPU_SUPPORT
/* adb pull "/d/ged/logbufs/gfreq" */
extern GED_LOG_BUF_HANDLE gpufreq_ged_log;
#endif

#if MT_GPUFREQ_DFD_ENABLE
#include "dbgtop.h"
#endif

enum gpu_dvfs_vgpu_step {
	GPU_DVFS_VGPU_STEP_1 = 0x1,
	GPU_DVFS_VGPU_STEP_2 = 0x2,
	GPU_DVFS_VGPU_STEP_3 = 0x3,
	GPU_DVFS_VGPU_STEP_4 = 0x4,
	GPU_DVFS_VGPU_STEP_5 = 0x5,
	GPU_DVFS_VGPU_STEP_6 = 0x6,
	GPU_DVFS_VGPU_STEP_7 = 0x7,
	GPU_DVFS_VGPU_STEP_8 = 0x8,
	GPU_DVFS_VGPU_STEP_9 = 0x9,
	GPU_DVFS_VGPU_STEP_A = 0xA,
	GPU_DVFS_VGPU_STEP_B = 0xB,
	GPU_DVFS_VGPU_STEP_C = 0xC,
	GPU_DVFS_VGPU_STEP_D = 0xD,
	GPU_DVFS_VGPU_STEP_E = 0xE,
	GPU_DVFS_VGPU_STEP_F = 0xF,
};

static inline void gpu_dvfs_vgpu_footprint(enum gpu_dvfs_vgpu_step step)
{
	aee_rr_rec_gpu_dvfs_vgpu(step |
				(aee_rr_curr_gpu_dvfs_vgpu() & 0xF0));
}

static inline void gpu_dvfs_vgpu_reset_footprint(void)
{
	aee_rr_rec_gpu_dvfs_vgpu(0);
}

static inline void gpu_dvfs_oppidx_footprint(unsigned int idx)
{
	aee_rr_rec_gpu_dvfs_oppidx(idx);
}

static inline void gpu_dvfs_oppidx_reset_footprint(void)
{
	aee_rr_rec_gpu_dvfs_oppidx(0xFF);
}

static inline void gpu_dvfs_power_count_footprint(int count)
{
	aee_rr_rec_gpu_dvfs_power_count(count);
}

static inline void gpu_dvfs_power_count_reset_footprint(void)
{
	aee_rr_rec_gpu_dvfs_power_count(0);
}

/**
 * ===============================================
 * SECTION : Local functions declaration
 * ===============================================
 */
static int __mt_gpufreq_pdrv_probe(struct platform_device *pdev);
static void __mt_gpufreq_set(
		unsigned int idx_old,
		unsigned int idx_new,
		unsigned int freq_old,
		unsigned int freq_new,
		unsigned int vgpu_old,
		unsigned int vgpu_new,
		unsigned int vsram_gpu_old,
		unsigned int vsram_gpu_new);
static void __mt_gpufreq_set_fixed_vgpu(int fixed_vgpu);
static void __mt_gpufreq_set_fixed_freq(int fixed_freq);
static void __mt_gpufreq_vgpu_set_mode(unsigned int mode);
static unsigned int __mt_gpufreq_get_cur_vgpu(void);
static unsigned int __mt_gpufreq_get_cur_freq(void);
static unsigned int __mt_gpufreq_get_cur_vsram_gpu(void);
static unsigned int __mt_gpufreq_get_segment_id(void);
static struct opp_table_info *__mt_gpufreq_get_segment_table(void);
static int __mt_gpufreq_get_opp_idx_by_vgpu(unsigned int vgpu);
static unsigned int __mt_gpufreq_get_vsram_gpu_by_vgpu(unsigned int vgpu);
static void __mt_gpufreq_kick_pbm(int enable);
static void __mt_gpufreq_clock_switch(unsigned int freq_new);
static void __mt_gpufreq_volt_switch(
		unsigned int vgpu_old, unsigned int vgpu_new,
		unsigned int vsram_gpu_old, unsigned int vsram_gpu_new);
static void __mt_gpufreq_volt_switch_without_vsram_gpu(
		unsigned int vgpu_old, unsigned int vgpu_new);

#if MT_GPUFREQ_DYNAMIC_POWER_TABLE_UPDATE
static void __mt_update_gpufreqs_power_table(void);
#endif

static void __mt_gpufreq_setup_opp_power_table(int num);
static void mt_gpufreq_cal_sb_opp_index(void);

static unsigned int __calculate_vgpu_settletime(bool mode, int deltaV);
static unsigned int __calculate_vsram_settletime(bool mode, int deltaV);

/**
 * ===============================================
 * SECTION : Local variables definition
 * ===============================================
 */

static struct mt_gpufreq_power_table_info *g_power_table;
static struct g_pmic_info *g_pmic;
static struct g_clk_info *g_clk;


static const struct of_device_id g_gpufreq_of_match[] = {
	{ .compatible = "mediatek,gpufreq" },
	{ /* sentinel */ }
};
static struct platform_driver g_gpufreq_pdrv = {
	.probe = __mt_gpufreq_pdrv_probe,
	.remove = NULL,
	.driver = {
		.name = "gpufreq",
		.owner = THIS_MODULE,
		.of_match_table = g_gpufreq_of_match,
	},
};

static bool g_DVFS_is_paused_by_ptpod;
static bool g_cg_on;
static bool g_mtcmos_on;
static bool g_buck_on;
static bool g_fixed_freq_volt_state;
static bool g_probe_done;
static int g_power_count;
static unsigned int g_opp_stress_test_state;
static int g_opp_power_test_state;
static unsigned int g_max_opp_idx_num;
static unsigned int g_segment_max_opp_idx;
static unsigned int g_segment_min_opp_idx;
static unsigned int g_aging_enable;
static unsigned int g_cur_opp_freq;
static unsigned int g_cur_opp_vgpu;
static unsigned int g_cur_opp_vsram_gpu;
static unsigned int g_cur_opp_idx;
static unsigned int g_fixed_freq;
static unsigned int g_fixed_vgpu;
static unsigned int g_max_upper_limited_idx;
static unsigned int g_min_lower_limited_idx;
static unsigned int g_upper_kicker;
static unsigned int g_lower_kicker;
static unsigned int g_lkg_pwr;
/* g_dfd_force_dump
 * 0: disable
 * 1: force dump + log
 * 2: force dump
 * 3: log
 */
static unsigned int g_dfd_force_dump;
static int g_opp_sb_idx_up[NUM_OF_OPP_IDX] = { 0 };
static int g_opp_sb_idx_down[NUM_OF_OPP_IDX] = { 0 };

static DEFINE_MUTEX(mt_gpufreq_lock);
static DEFINE_MUTEX(mt_gpufreq_power_lock);
static DEFINE_MUTEX(mt_gpufreq_limit_table_lock);

static void __iomem *g_apmixed_base;
static void __iomem *g_mfg_base;
static void __iomem *g_infracfg_base;
static void __iomem *g_infra_bpi_bsi_slv0;
static void __iomem *g_infra_peri_debug1;
static void __iomem *g_infra_peri_debug2;
static void __iomem *g_infra_peri_debug3;
static void __iomem *g_infra_peri_debug4;
static void __iomem *g_infracfg_ao;
static void __iomem *g_dbgtop;
static void __iomem *g_sleep;
static void __iomem *g_toprgu;

u64 mt_gpufreq_get_shader_present(void)
{
	static u64 shader_present;
	u32 segment_id = 0;

	if (shader_present)
		return shader_present;

	segment_id = __mt_gpufreq_get_segment_id();

	switch (segment_id) {
	case MT6891_SEGMENT:
	case MT6893_SEGMENT:
		shader_present = MT_GPU_SHADER_PRESENT_9;
		break;
	default:
		shader_present = MT_GPU_SHADER_PRESENT_9;
	}

	return shader_present;
}

void mt_gpufreq_dump_infra_status(void)
{
	unsigned int start, offset;

	gpufreq_pr_info("====\n");
	gpufreq_pr_info(
		"clk: %d, freq: %d, vgpu: %d, vsram_gpu: %d\n",
		mt_get_ckgen_freq(hf_fmfg_ck),
		g_cur_opp_freq,
		g_cur_opp_vgpu,
		g_cur_opp_vsram_gpu);

	// 0x1020E
	if (g_infracfg_base) {
		gpufreq_pr_info("infra info 0x%x:0x%08x\n",
			0x1020E810,
			readl(g_infracfg_base + 0x810));

		gpufreq_pr_info("infra info 0x%x:0x%08x\n",
			0x1020E814,
			readl(g_infracfg_base + 0x814));
	}

	// 0x1021E
	if (g_infra_bpi_bsi_slv0) {
		gpufreq_pr_info("infra info 0x%x:0x%08x\n",
			0x1021E230,
			readl(g_infra_bpi_bsi_slv0 + 0x230));

		gpufreq_pr_info("infra info 0x%x:0x%08x\n",
			0x1021E234,
			readl(g_infra_bpi_bsi_slv0 + 0x234));
	}

	// 0x10023000
	if (g_infra_peri_debug1) {
		gpufreq_pr_info("infra info 0x%x:0x%08x\n",
			0x10023000,
			readl(g_infra_peri_debug1 + 0x000));

		gpufreq_pr_info("infra info 0x%x:0x%08x\n",
			0x10023440,
			readl(g_infra_peri_debug1 + 0x440));

		gpufreq_pr_info("infra info 0x%x:0x%08x\n",
			0x10023444,
			readl(g_infra_peri_debug1 + 0x444));
	}

	// 0x10025000
	if (g_infra_peri_debug2) {
		gpufreq_pr_info("infra info 0x%x:0x%08x\n",
			0x10025000,
			readl(g_infra_peri_debug2 + 0x000));

		gpufreq_pr_info("infra info 0x%x:0x%08x\n",
			0x1002542C,
			readl(g_infra_peri_debug2 + 0x42C));
	}

	// 0x1002B000
	if (g_infra_peri_debug3) {
		gpufreq_pr_info("infra info 0x%x:0x%08x\n",
			0x1002B000,
			readl(g_infra_peri_debug3 + 0x000));
	}

	// 0x1002E000
	if (g_infra_peri_debug4) {
		gpufreq_pr_info("infra info 0x%x:0x%08x\n",
			0x1002E000,
			readl(g_infra_peri_debug4 + 0x000));
	}

	// 0x10006000
	if (g_sleep) {
		gpufreq_pr_info("pwr info 0x%x:0x%08x %08x %08x %08x\n",
			0x10006000 + 0x308,
			readl(g_sleep + 0x308),
			readl(g_sleep + 0x30C),
			readl(g_sleep + 0x310),
			readl(g_sleep + 0x314));

		gpufreq_pr_info("pwr info 0x%x:0x%08x %08x %08x\n",
			0x10006000 + 0x318,
			readl(g_sleep + 0x318),
			readl(g_sleep + 0x31C),
			readl(g_sleep + 0x320));

		gpufreq_pr_info("pwr info 0x%x:0x%08x\n",
			0x10006000 + 0x16C,
			readl(g_sleep + 0x16C));

		gpufreq_pr_info("pwr info 0x%x:0x%08x\n",
			0x10006000 + 0x170,
			readl(g_sleep + 0x170));
	}

	if (g_dfd_force_dump == 1 || g_dfd_force_dump == 3) {
		gpufreq_pr_info("====\n");

		if (g_cg_on) {
			start = 0x000;
			for (offset = start; offset <= 0xFFC; offset += 4) {
				gpufreq_pr_info("mfg info 0x%x:0x%08x\n",
					0x13FBF000 + offset,
					readl(g_mfg_base + offset));
			}
		}

		// 0x10023000
		if (g_infra_peri_debug1) {
			gpufreq_pr_info("====\n");
			start = 0x408;
			for (offset = start; offset <= 0x4E4; offset += 4) {
				gpufreq_pr_info("infra info 0x%x:0x%08x\n",
					0x10023000 + offset,
					readl(g_infra_peri_debug1 + offset));
			}
		}

		// 0x10025000
		if (g_infra_peri_debug2) {
			gpufreq_pr_info("====\n");
			start = 0x408;
			for (offset = start; offset <= 0x43C; offset += 4) {
				gpufreq_pr_info("infra info 0x%x:0x%08x\n",
					0x10025000 + offset,
					readl(g_infra_peri_debug2 + offset));
			}
		}

		// 0x1002B000
		if (g_infra_peri_debug3) {
			gpufreq_pr_info("====\n");
			start = 0x408;
			for (offset = start; offset <= 0x494; offset += 4) {
				gpufreq_pr_info("infra info 0x%x:0x%08x\n",
					0x1002B000 + offset,
					readl(g_infra_peri_debug3 + offset));
			}
		}

		// 0x1002E000
		if (g_infra_peri_debug4) {
			gpufreq_pr_info("====\n");
			start = 0x408;
			for (offset = start; offset <= 0x434; offset += 4) {
				gpufreq_pr_info("infra info 0x%x:0x%08x\n",
					0x1002E000 + offset,
					readl(g_infra_peri_debug4 + offset));
			}
		}
	}
}

int mt_gpufreq_is_dfd_force_dump(void)
{
	return g_dfd_force_dump;
}

static unsigned int mt_gpufreq_return_by_condition(
			unsigned int limit_idx, enum mt_gpufreq_kicker kicker)
{
	unsigned int ret = 0;

	/* GPU DVFS disabled */
	if (!mt_gpufreq_get_dvfs_en())
		ret |= (1 << 0);

	if (kicker != KIR_PTPOD) {
		if (limit_idx > g_segment_min_opp_idx ||
					limit_idx < g_segment_max_opp_idx) {
			ret |= (1 << 1);
			gpufreq_pr_info("out of segment opp range, %d (%d)\n",
					limit_idx, g_segment_min_opp_idx);
		}
		if (g_DVFS_is_paused_by_ptpod)
			ret |= (1 << 2);
	}

	/* if /proc/gpufreq/gpufreq_fixed_freq_volt fix freq and volt */
	if (g_fixed_freq_volt_state)
		ret |= (1 << 3);

	/* the same freq && volt */
	if (g_cur_opp_freq == g_opp_table[limit_idx].gpufreq_khz &&
			g_cur_opp_vgpu == g_opp_table[limit_idx].gpufreq_vgpu)
		ret |= (1 << 4);

	gpufreq_pr_logbuf("return_by_condition: 0x%x\n", ret);

	return ret;
}

static void mt_gpufreq_update_limit_idx(unsigned int kicker,
		unsigned int t_upper_idx, unsigned int t_lower_idx)
{
	unsigned int i;
	unsigned int upper_kicker, lower_kicker;
	unsigned int upper_prio, lower_prio;
	unsigned int upper_limit_idx, lower_limit_idx;

	mutex_lock(&mt_gpufreq_limit_table_lock);

	if (limit_table[kicker].upper_idx == t_upper_idx &&
	   limit_table[kicker].lower_idx == t_lower_idx) {
		mutex_unlock(&mt_gpufreq_limit_table_lock);
		return;
	}

	limit_table[kicker].upper_idx = t_upper_idx;
	limit_table[kicker].lower_idx = t_lower_idx;

	gpufreq_pr_debug("update_limited_idx limit_kicker: %d, t_upper_idx: %d, t_lower_idx = %d\n",
		kicker, t_upper_idx, t_lower_idx);

	upper_kicker = NUM_OF_KIR;
	lower_kicker = NUM_OF_KIR;

	upper_prio = GPUFREQ_LIMIT_PRIO_NONE;
	lower_prio = GPUFREQ_LIMIT_PRIO_NONE;

	upper_limit_idx = g_segment_max_opp_idx;
	lower_limit_idx = g_segment_min_opp_idx;

	for (i = 0; i < NUM_OF_KIR; i++) {
		/* check upper limit */
		/* choose limit idx not default and limit is enable */
		if (limit_table[i].upper_idx != LIMIT_IDX_DEFAULT &&
			limit_table[i].upper_enable == LIMIT_ENABLE) {
			/* choose limit idx of higher priority */
			if (limit_table[i].prio > upper_prio) {
				upper_kicker = i;
				upper_limit_idx = limit_table[i].upper_idx;
				upper_prio = limit_table[i].prio;
			}
			/* choose big limit idx if proiority is the same */
			else if ((limit_table[i].upper_idx > upper_limit_idx) &&
				(limit_table[i].prio == upper_prio)) {
				upper_kicker = i;
				upper_limit_idx = limit_table[i].upper_idx;
				upper_prio = limit_table[i].prio;
			}
		}

		/* check lower limit */
		/* choose limit idx not default and limit is enable */
		if (limit_table[i].lower_idx != LIMIT_IDX_DEFAULT &&
			limit_table[i].lower_enable == LIMIT_ENABLE) {
			/* choose limit idx of higher priority */
			if (limit_table[i].prio > lower_prio) {
				lower_kicker = i;
				lower_limit_idx = limit_table[i].lower_idx;
				lower_prio = limit_table[i].prio;
			}
			/* choose small limit idx if proiority is the same */
			else if ((limit_table[i].lower_idx < lower_limit_idx) &&
				(limit_table[i].prio == lower_prio)) {
				lower_kicker = i;
				lower_limit_idx = limit_table[i].lower_idx;
				lower_prio = limit_table[i].prio;
			}
		}
	}

	mutex_unlock(&mt_gpufreq_limit_table_lock);

	g_upper_kicker = upper_kicker;
	g_lower_kicker = lower_kicker;

	if (upper_limit_idx > lower_limit_idx) {
		if (upper_prio >= lower_prio)
			lower_limit_idx = g_segment_min_opp_idx;
		else
			upper_limit_idx = g_segment_max_opp_idx;
	}

	g_max_upper_limited_idx = upper_limit_idx;
	g_min_lower_limited_idx = lower_limit_idx;
}

static void mt_gpufreq_update_limit_enable(unsigned int kicker,
		unsigned int t_upper_enable, unsigned int t_lower_enable)
{
	mutex_lock(&mt_gpufreq_limit_table_lock);

	if (limit_table[kicker].upper_enable == t_upper_enable &&
	   limit_table[kicker].lower_enable == t_lower_enable) {
		mutex_unlock(&mt_gpufreq_limit_table_lock);
		return;
	}

	limit_table[kicker].upper_enable = t_upper_enable;
	limit_table[kicker].lower_enable = t_lower_enable;

	gpufreq_pr_debug("update_limited_enable limit_kicker: %d, t_upper_enable: %d, t_lower_enable = %d\n",
		kicker, t_upper_enable, t_lower_enable);

	mutex_unlock(&mt_gpufreq_limit_table_lock);
}

static unsigned int mt_gpufreq_limit_idx_by_condition(unsigned int target_idx)
{
	unsigned int limit_idx;

	limit_idx = target_idx;

	/* generate random segment OPP index for stress test */
	if (g_opp_stress_test_state == 1) {
		get_random_bytes(&target_idx, sizeof(target_idx));
		limit_idx = target_idx %
			(g_segment_min_opp_idx - g_segment_max_opp_idx + 1) +
			g_segment_max_opp_idx;
		mt_gpufreq_update_limit_idx(KIR_STRESS, limit_idx, limit_idx);
	}

	if (limit_idx < g_max_upper_limited_idx)
		limit_idx = g_max_upper_limited_idx;

	if (limit_idx > g_min_lower_limited_idx)
		limit_idx = g_min_lower_limited_idx;

	gpufreq_pr_logbuf(
		"limit_idx: %d, g_upper_kicker: %d, g_max_upper_limited_idx: %d, g_lower_kicker: %d, g_min_lower_limited_idx: %d\n",
		limit_idx,
		g_upper_kicker, g_max_upper_limited_idx,
		g_lower_kicker, g_min_lower_limited_idx);

	return limit_idx;
}

unsigned int mt_gpufreq_target(unsigned int request_idx,
					enum mt_gpufreq_kicker kicker)
{
	unsigned int target_idx, limit_idx;
	unsigned int return_condition;

	mutex_lock(&mt_gpufreq_lock);

	if (kicker == KIR_POLICY)
		target_idx = request_idx + g_segment_max_opp_idx;
	else
		target_idx = request_idx;

	gpufreq_pr_logbuf("kicker: %d, target_idx: %d (%d, %d)\n",
		kicker, target_idx, request_idx, g_segment_max_opp_idx);

	limit_idx = mt_gpufreq_limit_idx_by_condition(target_idx);

	return_condition = mt_gpufreq_return_by_condition(limit_idx, kicker);

	if (return_condition) {
		mutex_unlock(&mt_gpufreq_lock);
		return 0;
	}

	__mt_gpufreq_set(g_cur_opp_idx, limit_idx,
		g_cur_opp_freq, g_opp_table[limit_idx].gpufreq_khz,
		g_cur_opp_vgpu, g_opp_table[limit_idx].gpufreq_vgpu,
		g_cur_opp_vsram_gpu, g_opp_table[limit_idx].gpufreq_vsram);

	mutex_unlock(&mt_gpufreq_lock);
	return 0;
}

void mt_gpufreq_set_timestamp(void)
{
	gpufreq_pr_debug("@%s\n", __func__);

	/* write 1 into 0x13fb_f130 bit 0 to enable timestamp register */
	/* timestamp will be used by clGetEventProfilingInfo*/
	writel(0x00000001, g_mfg_base + 0x130);
}

void mt_gpufreq_check_bus_idle(void)
{
	u32 val;

	gpufreq_pr_debug("@%s\n", __func__);

	/* MFG_QCHANNEL_CON (0x13fb_f0b4) bit [1:0] = 0x1 */
	writel(0x00000001, g_mfg_base + 0xb4);

	/* set register MFG_DEBUG_SEL (0x13fb_f170) bit [7:0] = 0x03 */
	writel(0x00000003, g_mfg_base + 0x170);

	/* polling register MFG_DEBUG_TOP (0x13fb_f178) bit 2 = 0x1 */
	/* => 1 for bus idle, 0 for bus non-idle */
	do {
		val = readl(g_mfg_base + 0x178);
	} while ((val & 0x4) != 0x4);
}

static void mt_gpufreq_external_cg_control(void)
{
	u32 val;

	gpufreq_pr_debug("@%s\n", __func__);

	/* [F] MFG_ASYNC_CON 0x13FB_F020 [22] MEM0_MST_CG_ENABLE = 0x1 */
	/* [J] MFG_ASYNC_CON 0x13FB_F020 [23] MEM0_SLV_CG_ENABLE = 0x1 */
	/* [G] MFG_ASYNC_CON 0x13FB_F020 [24] MEM1_MST_CG_ENABLE = 0x1 */
	/* [K] MFG_ASYNC_CON 0x13FB_F020 [25] MEM1_SLV_CG_ENABLE = 0x1 */
	val = readl(g_mfg_base + 0x20);
	val |= (1UL << 22);
	val |= (1UL << 23);
	val |= (1UL << 24);
	val |= (1UL << 25);
	writel(val, g_mfg_base + 0x20);

	/* [H] MFG_ASYNC_CON_3 0x13FB_F02C [12] MEM0_1_MST_CG_ENABLE = 0x1 */
	/* [L] MFG_ASYNC_CON_3 0x13FB_F02C [13] MEM0_1_SLV_CG_ENABLE = 0x1 */
	/* [I] MFG_ASYNC_CON_3 0x13FB_F02C [14] MEM1_1_MST_CG_ENABLE = 0x1 */
	/* [M] MFG_ASYNC_CON_3 0x13FB_F02C [15] MEM1_1_SLV_CG_ENABLE = 0x1 */
	val = readl(g_mfg_base + 0x2C);
	val |= (1UL << 12);
	val |= (1UL << 13);
	val |= (1UL << 14);
	val |= (1UL << 15);
	writel(val, g_mfg_base + 0x2C);

	/* [D] MFG_GLOBAL_CON 0x13FB_F0B0 [10] GPU_CLK_FREE_RUN = 0x0 */
	/* [D] MFG_GLOBAL_CON 0x13FB_F0B0 [9] MFG_SOC_OUT_AXI_FREE_RUN = 0x0 */
	val = readl(g_mfg_base + 0xB0);
	val &= ~(1UL << 10);
	val &= ~(1UL << 9);
	writel(val, g_mfg_base + 0xB0);

	/* [D] MFG_QCHANNEL_CON 0x13FB_F0B4 [4] QCHANNEL_ENABLE = 0x1 */
	val = readl(g_mfg_base + 0xB4);
	val |= (1UL << 4);
	writel(val, g_mfg_base + 0xB4);

	/* [E] MFG_GLOBAL_CON 0x13FB_F0B0 [19] PWR_CG_FREE_RUN = 0x0 */
	/* [P] MFG_GLOBAL_CON 0x13FB_F0B0 [8] MFG_SOC_IN_AXI_FREE_RUN = 0x0 */
	val = readl(g_mfg_base + 0xB0);
	val &= ~(1UL << 19);
	val &= ~(1UL << 8);
	writel(val, g_mfg_base + 0xB0);

	/*[O] MFG_ASYNC_CON_1 0x13FB_F024 [0] FAXI_CK_SOC_IN_EN_ENABLE = 0x1*/
	val = readl(g_mfg_base + 0x24);
	val |= (1UL << 0);
	writel(val, g_mfg_base + 0x24);
}

static void mt_gpufreq_cg_control(enum mt_power_state power)
{
	gpufreq_pr_debug("@%s: power = %d", __func__, power);

	if (power == POWER_ON) {
		if (clk_prepare_enable(g_clk->subsys_mfg_cg))
			gpufreq_pr_info("failed when enable subsys-mfg-cg\n");

		mt_gpufreq_external_cg_control();
	} else {
		clk_disable_unprepare(g_clk->subsys_mfg_cg);
	}

	g_cg_on = power;
}

static void mt_gpufreq_mtcmos_control(enum mt_power_state power)
{
	u64 shader_present = 0;
	gpufreq_pr_debug("@%s: power = %d\n", __func__, power);

	shader_present = mt_gpufreq_get_shader_present();

	if (power == POWER_ON) {
		if (clk_prepare_enable(g_clk->mtcmos_mfg_async))
			gpufreq_pr_info("failed when enable mtcmos_mfg_async\n");

		if (clk_prepare_enable(g_clk->mtcmos_mfg))
			gpufreq_pr_info("failed when enable mtcmos_mfg\n");

		if (shader_present & MT_GPU_CORE_MASK_0)
			if (clk_prepare_enable(g_clk->mtcmos_mfg_core0))
				gpufreq_pr_info("failed when enable mtcmos_mfg_core0\n");

		if (shader_present & MT_GPU_CORE_MASK_1_2)
			if (clk_prepare_enable(g_clk->mtcmos_mfg_core1_2))
				gpufreq_pr_info("failed when enable mtcmos_mfg_core1_2\n");

		if (shader_present & MT_GPU_CORE_MASK_3_4)
			if (clk_prepare_enable(g_clk->mtcmos_mfg_core3_4))
				gpufreq_pr_info("failed when enable mtcmos_mfg_core3_4\n");

		if (shader_present & MT_GPU_CORE_MASK_5_6)
			if (clk_prepare_enable(g_clk->mtcmos_mfg_core5_6))
				gpufreq_pr_info("failed when enable mtcmos_mfg_core5_6\n");

		if (shader_present & MT_GPU_CORE_MASK_7_8)
			if (clk_prepare_enable(g_clk->mtcmos_mfg_core7_8))
				gpufreq_pr_info("failed when enable mtcmos_mfg_core7_8\n");
	} else {
		if (shader_present & MT_GPU_CORE_MASK_7_8)
			clk_disable_unprepare(g_clk->mtcmos_mfg_core7_8);

		if (shader_present & MT_GPU_CORE_MASK_5_6)
			clk_disable_unprepare(g_clk->mtcmos_mfg_core5_6);

		if (shader_present & MT_GPU_CORE_MASK_3_4)
			clk_disable_unprepare(g_clk->mtcmos_mfg_core3_4);

		if (shader_present & MT_GPU_CORE_MASK_1_2)
			clk_disable_unprepare(g_clk->mtcmos_mfg_core1_2);

		if (shader_present & MT_GPU_CORE_MASK_0)
			clk_disable_unprepare(g_clk->mtcmos_mfg_core0);

		clk_disable_unprepare(g_clk->mtcmos_mfg);
		clk_disable_unprepare(g_clk->mtcmos_mfg_async);
	}

	g_mtcmos_on = power;
}

static void mt_gpufreq_buck_control(enum mt_power_state power)
{
	gpufreq_pr_debug("@%s: power = %d", __func__, power);

	if (power == POWER_ON) {
		if (regulator_enable(g_pmic->reg_vsram_gpu)) {
			gpufreq_pr_info("enable VSRAM_GPU failed\n");
			return;
		}
		if (regulator_enable(g_pmic->reg_vgpu)) {
			gpufreq_pr_info("enable VGPU failed\n");
			return;
		}
	} else {
		if (regulator_disable(g_pmic->reg_vgpu)) {
			gpufreq_pr_info("disable VGPU failed\n");
			return;
		}
		if (regulator_disable(g_pmic->reg_vsram_gpu)) {
			gpufreq_pr_info("disable VSRAM_GPU failed\n");
			return;
		}
	}

	g_buck_on = power;
	__mt_gpufreq_kick_pbm(power);
}

void mt_gpufreq_software_trigger_dfd(void)
{
#if MT_GPUFREQ_DFD_ENABLE
	unsigned int val;

	val = readl(g_dbgtop + 0x040) | (0x95 << 24) | 0x10;
	writel(val, g_dbgtop + 0x040);

	val = readl(g_dbgtop + 0x044) | (0x95 << 24) | 0x20000;
	writel(val, g_dbgtop + 0x044);

	writel(0x0F011100, g_mfg_base + 0xA00);

	val = readl(g_infracfg_ao + 0x600);
	if (!(val & 0x80000))
		gpufreq_pr_info("software_trigger failed: %0x:%08x\n",
			0x10001600, val);
	else if (g_dfd_force_dump)
		gpufreq_pr_info("software_trigger state: %0x:%08x\n",
			0x10001600, val);
#endif
}

/*
 * general kernelAPI db when dfd is triggerd in probe function
 * we need dump debug register information
 */
static void __mt_gpufreq_dfd_debug_exception(void)
{
#if MT_GPUFREQ_DFD_ENABLE
	unsigned int status = readl(g_infracfg_ao + 0x600);

	//0x1000700C WDT_STA
	//0x10007030 WDT_REQ_MODE
	gpu_assert(!(status & 0x80000), GPU_DFD_PROBE_TRIGGERED,
		"[GPU_DFD] gpu dfd is triggered at probe\n"
		"[GPU_DFD] dfd status 0x%x, WDT_STA 0x%x, WDT_REQ_MODE 0x%x\n",
		status, readl(g_toprgu + 0x00C), readl(g_toprgu + 0x030));

#endif // MT_GPUFREQ_DFD_ENABLE
}

static int __mt_gpufreq_is_dfd_triggered(void)
{
#if MT_GPUFREQ_DFD_ENABLE
	unsigned int status = readl(g_infracfg_ao + 0x600);

#if MT_GPUFREQ_DFD_DEBUG
	gpufreq_pr_debug("[GPU_DFD] @%s: dfd status 0x%x\n",
			__func__, status);
#endif

	return (status & 0x80000);
#else
	return 0;
#endif
}

static int __mt_gpufreq_is_dfd_completed(void)
{
#if MT_GPUFREQ_DFD_ENABLE
	unsigned int status = readl(g_infracfg_ao + 0x600);

#if MT_GPUFREQ_DFD_DEBUG
	gpufreq_pr_debug("[GPU_DFD] @%s: dfd status 0x%x\n",
			__func__, status);
#endif

	return (status & 0x40000);
#else
	return 0;
#endif
}

static void __mt_gpufreq_dbgtop_pwr_on(bool enable)
{
#if MT_GPUFREQ_DFD_ENABLE
	unsigned int rgu_pwr;
	int ret;
	int retry = 10;

	while (retry) {
		ret = mtk_dbgtop_mfg_pwr_on(enable);
		udelay(80);
		rgu_pwr = readl(g_dbgtop + 0x060);

		if ((enable && (rgu_pwr & 0x1)) ||
		    (!enable && !(rgu_pwr & 0x1)))
			break;

		retry--;
		gpufreq_pr_info("[GPU_DFD] mtk_dbgtop_mfg_pwr_on(%d) fail:0x%0x ret:%d retry_remain:%d\n",
			enable, rgu_pwr, ret, retry);
	}
#endif
}

static void __mt_gpufreq_config_dfd(bool enable)
{
#if MT_GPUFREQ_DFD_ENABLE
	if (enable) {
		// debug monitor
		if (mt_gpufreq_is_dfd_force_dump())
			writel(0xFFFFFFFF, g_mfg_base + 0x8F8);

		writel(0x00012EC9, g_mfg_base + 0xA04);
		writel(0x0001813A, g_mfg_base + 0xA08);
		writel(0x00210862, g_mfg_base + 0xA0C);
		writel(0x00000000, g_mfg_base + 0xA10);
		writel(0x00000000, g_mfg_base + 0xA14);
		writel(0x00000000, g_mfg_base + 0xA18);
		writel(0x00000000, g_mfg_base + 0xA1C);
		writel(0x00000000, g_mfg_base + 0xA20);
		writel(0x00000000, g_mfg_base + 0xA24);
		writel(0x00000000, g_mfg_base + 0xA28);
		writel(0x00000000, g_mfg_base + 0xA2C);
		// [8] enable
		writel(0x0F101100, g_mfg_base + 0xA00);

		mtk_dbgtop_dfd_timeout(0x3E8, 0); // 500 ms

	} else {
		writel(0x00000000, g_mfg_base + 0xA00);
		writel(0x00000000, g_mfg_base + 0xA04);
		writel(0x00000000, g_mfg_base + 0xA08);
		writel(0x00000000, g_mfg_base + 0xA0C);
		writel(0x00000000, g_mfg_base + 0xA10);
		writel(0x00000000, g_mfg_base + 0xA14);
		writel(0x00000000, g_mfg_base + 0xA18);
		writel(0x00000000, g_mfg_base + 0xA1C);
		writel(0x00000000, g_mfg_base + 0xA20);
		writel(0x00000000, g_mfg_base + 0xA24);
		writel(0x00000000, g_mfg_base + 0xA28);
		writel(0x00000000, g_mfg_base + 0xA2C);
		writel(0x00000000, g_mfg_base + 0x8F8);
	}
#endif
}

void mt_gpufreq_power_control(enum mt_power_state power, enum mt_cg_state cg,
			enum mt_mtcmos_state mtcmos, enum mt_buck_state buck)
{
	mutex_lock(&mt_gpufreq_lock);

	gpufreq_pr_debug("power=%d g_power_count=%d (todo cg: %d, mtcmos: %d, buck: %d)\n",
		power, g_power_count, cg, mtcmos, buck);

	if (__mt_gpufreq_is_dfd_triggered()) {
#if MT_GPUFREQ_DFD_DEBUG
		unsigned int dfd_status = readl(g_infracfg_ao + 0x600);
		unsigned int rgu_pwr = readl(g_dbgtop + 0x060);

		gpufreq_pr_info("[GPU_DFD]@%s: power %d, dfd_status 0x%x, rgu_pwr 0x%x\n",
			__func__, power, dfd_status, rgu_pwr);

		gpufreq_pr_info("[GPU_DFD] power %d, pwr info 0x%x:0x%08x\n",
			power,
			0x10006000 + 0x16C,
			readl(g_sleep + 0x16C));
#endif
		if (g_probe_done) {
			gpufreq_pr_info("power=%d g_power_count=%d, skip by dfd_trigger\n",
				power, g_power_count);
			return;
		}
	}

	if (power == POWER_ON)
		g_power_count++;
	else {
		check_pending_info();

		g_power_count--;
		gpu_assert(g_power_count >= 0, GPU_FREQ_EXCEPTION,
			"power=%d g_power_count=%d (todo cg: %d, mtcmos: %d, buck: %d)\n",
			power, g_power_count, cg, mtcmos, buck);
	}
	gpu_dvfs_power_count_footprint(g_power_count);

	if (power == POWER_ON) {
		gpu_dvfs_vgpu_footprint(GPU_DVFS_VGPU_STEP_1);

		if (buck == BUCK_ON)
			mt_gpufreq_buck_control(power);

		gpu_dvfs_vgpu_footprint(GPU_DVFS_VGPU_STEP_2);

		if (mtcmos == MTCMOS_ON)
			mt_gpufreq_mtcmos_control(power);

		gpu_dvfs_vgpu_footprint(GPU_DVFS_VGPU_STEP_3);

		if (cg == CG_ON)
			mt_gpufreq_cg_control(power);

#if MT_GPUFREQ_DFD_ENABLE
		__mt_gpufreq_config_dfd(true);
#endif

		gpu_dvfs_vgpu_footprint(GPU_DVFS_VGPU_STEP_4);
		mtk_notify_gpu_power_change(1);
	} else {
#if MT_GPUFREQ_DFD_ENABLE
		__mt_gpufreq_config_dfd(false);
#endif
		mtk_notify_gpu_power_change(0);

		gpu_dvfs_vgpu_footprint(GPU_DVFS_VGPU_STEP_5);

		if (cg == CG_OFF)
			mt_gpufreq_cg_control(power);

		gpu_dvfs_vgpu_footprint(GPU_DVFS_VGPU_STEP_6);

		if (mtcmos == MTCMOS_OFF)
			mt_gpufreq_mtcmos_control(power);

		gpu_dvfs_vgpu_footprint(GPU_DVFS_VGPU_STEP_7);

		if (buck == BUCK_OFF)
			mt_gpufreq_buck_control(power);

		gpu_dvfs_vgpu_footprint(GPU_DVFS_VGPU_STEP_8);
	}

	mutex_unlock(&mt_gpufreq_lock);
}

void mt_gpufreq_enable_by_ptpod(void)
{
	__mt_gpufreq_vgpu_set_mode(REGULATOR_MODE_NORMAL); /* NORMAL */

	mt_gpufreq_power_control(POWER_OFF, CG_OFF,
					MTCMOS_OFF, BUCK_OFF);

	mt_gpufreq_update_limit_idx(KIR_PTPOD,
		LIMIT_IDX_DEFAULT,
		LIMIT_IDX_DEFAULT);

	g_DVFS_is_paused_by_ptpod = false;

	gpufreq_pr_debug("@%s: PTPOD paused DVFS = %d,\n", __func__,
			g_DVFS_is_paused_by_ptpod);
}

void mt_gpufreq_disable_by_ptpod(void)
{
	struct opp_table_info *opp_table = g_opp_table;
	unsigned int i = 0;
	unsigned int target_idx = 0;

	for (i = 0; i < NUM_OF_OPP_IDX; i++) {
		if (opp_table[i].gpufreq_vgpu <= PTPOD_DISABLE_VOLT) {
			target_idx = i;
			break;
		}
	}

	mt_gpufreq_update_limit_idx(KIR_PTPOD, target_idx, target_idx);

	g_DVFS_is_paused_by_ptpod = true;

	gpufreq_pr_debug("@%s: PTPOD paused DVFS = %d, target_idx = %d(%d)\n",
			__func__, g_DVFS_is_paused_by_ptpod, target_idx,
			opp_table[target_idx].gpufreq_vgpu);

	mt_gpufreq_power_control(POWER_ON, CG_ON, MTCMOS_ON, BUCK_ON);

	mt_gpufreq_target(target_idx, KIR_PTPOD);

	__mt_gpufreq_vgpu_set_mode(REGULATOR_MODE_FAST); /* PWM */
}

/*
 * API : update OPP and switch back to default voltage setting
 */
void mt_gpufreq_restore_default_volt(void)
{
	struct opp_table_info *opp_table = __mt_gpufreq_get_segment_table();
	int i;

	mutex_lock(&mt_gpufreq_lock);

	gpufreq_pr_debug("@%s: PTPOD restore OPP table to default voltage\n",
		__func__);

	for (i = 0; i < NUM_OF_OPP_IDX; i++) {
		g_opp_table[i].gpufreq_vgpu =
				opp_table[i].gpufreq_vgpu;
		g_opp_table[i].gpufreq_vsram =
				opp_table[i].gpufreq_vsram;

		gpufreq_pr_debug("@%s: PTPOD opp_table[%d] vgpu = %d, vsram = %d\n",
			__func__, i,
			g_opp_table[i].gpufreq_vgpu,
			g_opp_table[i].gpufreq_vsram);
	}

	mt_gpufreq_cal_sb_opp_index();

	/* update volt if powered */
	if (g_buck_on && !g_fixed_freq_volt_state) {
		__mt_gpufreq_volt_switch_without_vsram_gpu(
			g_cur_opp_vgpu,
			g_opp_table[g_cur_opp_idx].gpufreq_vgpu);

		g_cur_opp_vgpu = g_opp_table[g_cur_opp_idx].gpufreq_vgpu;
		g_cur_opp_vsram_gpu = g_opp_table[g_cur_opp_idx].gpufreq_vsram;
	}

	mutex_unlock(&mt_gpufreq_lock);
}

/*
 * interpolation none PTPOP.
 *
 * step = (large - small) / range
 * vnew = large - step * j
 */
void mt_gpufreq_update_volt_interpolation(void)
{
	int i, j, largeOppIndex, smallOppIndex, range, freq, vnew;
	int large_vgpu, small_vgpu, large_freq, small_freq, slope;
	unsigned int ptpod_opp_idx_num;
	struct opp_table_info *opp_table = __mt_gpufreq_get_segment_table();
	int lower_bound;

	ptpod_opp_idx_num = ARRAY_SIZE(g_ptpod_opp_idx_table_segment);

	for (i = 1; i < ptpod_opp_idx_num; i++) {
		largeOppIndex = mt_gpufreq_get_ori_opp_idx(i - 1);
		smallOppIndex = mt_gpufreq_get_ori_opp_idx(i);
		range = smallOppIndex - largeOppIndex;

		large_vgpu = g_opp_table[largeOppIndex].gpufreq_vgpu;
		large_freq = g_opp_table[largeOppIndex].gpufreq_khz / 1000;

		small_vgpu = g_opp_table[smallOppIndex].gpufreq_vgpu;
		small_freq = g_opp_table[smallOppIndex].gpufreq_khz / 1000;

		slope = (large_vgpu - small_vgpu) / (large_freq - small_freq);

		if (slope < 0) {
			dump_stack();

			gpu_assert(slope >= 0, GPU_OPP_PTPOD_SLOPE,
				"i %d, slope %d, largeOppIndex %d, smallOppIndex %d\n"
				"large_vgpu %d, large_freq %d\n"
				"small_vgpu %d, small_freq %d\n",
				i, slope, largeOppIndex, smallOppIndex,
				large_vgpu, large_freq,
				small_vgpu, small_freq);
		}

		for (j = 1; j < range; j++) {
			freq = g_opp_table[largeOppIndex + j].gpufreq_khz
				/ 1000;
			vnew = small_vgpu + slope * (freq - small_freq);
			vnew = VOLT_NORMALIZATION(vnew);
			gpu_assert(vnew >= small_vgpu && vnew <= large_vgpu,
				GPU_FREQ_EXCEPTION,
				"j %d, vnew %d, small_vgpu %d, large_vgpu %d\n",
				j, vnew, small_vgpu, large_vgpu);

			g_opp_table[largeOppIndex + j].gpufreq_vgpu = vnew;
			g_opp_table[largeOppIndex + j].gpufreq_vsram =
				__mt_gpufreq_get_vsram_gpu_by_vgpu(vnew);
		}
	}
}

/* post process ptpod voltage
 * the voltage of 886MHz (OPP0)
 * EFUSE 0x11C105D8[23:21]
 *    b'011 = 0x3 = 0.8V
 *    b'100 = 0x4 = 0.7625V
 *    b'101 = 0x5 = 0.725V
 *    b'110 = 0x6 = 0.7V
 *    others = 0.8V
 */
void mt_gpufreq_update_ptpod_by_efuse(void)
{
	unsigned int efuse_id;

	/* [PTPOD22 (0x11C105D8) 72] [23:21]*/
	efuse_id = ((get_devinfo_with_index(72) >> 21) & 0x7);

	switch (efuse_id) {
	case 0x3:
		g_opp_table[0].gpufreq_vgpu = 80000;
		break;
	case 0x4:
		g_opp_table[0].gpufreq_vgpu = 76250;
		break;
	case 0x5:
		g_opp_table[0].gpufreq_vgpu = 72500;
		break;
	case 0x6:
		g_opp_table[0].gpufreq_vgpu = 70000;
		break;
	default:
		g_opp_table[0].gpufreq_vgpu = 80000;
		break;
	}

}

void mt_gpufreq_apply_aging(bool apply)
{
	int i;

	for (i = 0; i < g_max_opp_idx_num; i++) {
		if (apply) {
			g_opp_table[i].gpufreq_vgpu -=
					g_opp_table[i].gpufreq_aging_margin;
		} else {
			g_opp_table[i].gpufreq_vgpu +=
					g_opp_table[i].gpufreq_aging_margin;
		}

		g_opp_table[i].gpufreq_vsram =
				__mt_gpufreq_get_vsram_gpu_by_vgpu(
				g_opp_table[i].gpufreq_vgpu);

		gpufreq_pr_debug("@%s: apply: %d: [%d] vgpu: %d, vsram_gpu: %d\n",
					__func__, apply, i,
					g_opp_table[i].gpufreq_vgpu,
					g_opp_table[i].gpufreq_vsram);
	}

	mt_gpufreq_cal_sb_opp_index();
}

/*
 * API : update OPP and set voltage
 * because PTPOD modified voltage table by PMIC wrapper
 */
unsigned int mt_gpufreq_update_volt(
		unsigned int pmic_volt[], unsigned int array_size)
{
	int i;
	int target_idx;

	mutex_lock(&mt_gpufreq_lock);

	gpufreq_pr_debug("@%s: PTPOD update OPP table to given voltage\n",
		__func__);

	for (i = 0; i < array_size; i++) {
		target_idx = mt_gpufreq_get_ori_opp_idx(i);
		g_opp_table[target_idx].gpufreq_vgpu = pmic_volt[i];
		g_opp_table[target_idx].gpufreq_vsram =
			__mt_gpufreq_get_vsram_gpu_by_vgpu(pmic_volt[i]);
	}

	// post process ptpod voltage
	mt_gpufreq_update_ptpod_by_efuse();

	// update none PTP
	mt_gpufreq_update_volt_interpolation();

	if (g_aging_enable)
		mt_gpufreq_apply_aging(true);
	else
		mt_gpufreq_cal_sb_opp_index();

	/* update volt if powered */
	if (g_buck_on && !g_fixed_freq_volt_state) {
		__mt_gpufreq_volt_switch_without_vsram_gpu(
				g_cur_opp_vgpu,
				g_opp_table[g_cur_opp_idx].gpufreq_vgpu);
		g_cur_opp_vgpu = g_opp_table[g_cur_opp_idx].gpufreq_vgpu;
		g_cur_opp_vsram_gpu = g_opp_table[g_cur_opp_idx].gpufreq_vsram;
	}

	mutex_unlock(&mt_gpufreq_lock);

	return 0;
}

unsigned int mt_gpufreq_bringup(void)
{
	return MT_GPUFREQ_BRINGUP;
}

unsigned int mt_gpufreq_get_dvfs_en(void)
{
	return MT_GPUFREQ_DVFS_ENABLE;
}

unsigned int mt_gpufreq_not_ready(void)
{
	if (IS_ERR(g_pmic->reg_vgpu) || IS_ERR(g_pmic->reg_vsram_gpu)) {
		gpufreq_pr_info("VGPU: %lu, VSRAM_GPU: %ld not initialized\n",
			PTR_ERR(g_pmic->reg_vgpu),
			PTR_ERR(g_pmic->reg_vsram_gpu));
		return true;
	} else {
		return false;
	}
}

unsigned int mt_gpufreq_power_ctl_en(void)
{
	return MT_GPUFREQ_POWER_CTL_ENABLE;
}

unsigned int mt_gpufreq_get_cust_init_en(void)
{
	return MT_GPUFREQ_CUST_CONFIG;
}

/* API : get OPP table index number */
/* need to sub g_segment_max_opp_idx to map to real idx */
unsigned int mt_gpufreq_get_dvfs_table_num(void)
{
	/* prevent get wrong index */
	if (mt_gpufreq_not_ready())
		return -1;

	return g_segment_min_opp_idx - g_segment_max_opp_idx + 1;
}

/* API : get real OPP table index number */
unsigned int mt_gpufreq_get_real_dvfs_table_num(void)
{
	return g_max_opp_idx_num;
}

/* API : get frequency via OPP table index */
unsigned int mt_gpufreq_get_freq_by_idx(unsigned int idx)
{
	idx += g_segment_max_opp_idx;
	if (idx < g_max_opp_idx_num)
		return g_opp_table[idx].gpufreq_khz;
	else
		return 0;
}

/* API : get frequency via OPP table real index */
unsigned int mt_gpufreq_get_freq_by_real_idx(unsigned int idx)
{
	if (idx < g_max_opp_idx_num)
		return g_opp_table[idx].gpufreq_khz;
	else
		return 0;
}

/* API : get vgpu via OPP table index */
unsigned int mt_gpufreq_get_volt_by_idx(unsigned int idx)
{
	idx += g_segment_max_opp_idx;
	if (idx < g_max_opp_idx_num)
		return g_opp_table[idx].gpufreq_vgpu;
	else
		return 0;
}

/* API : get vgpu via OPP table real index */
unsigned int mt_gpufreq_get_volt_by_real_idx(unsigned int idx)
{
	if (idx < g_max_opp_idx_num)
		return g_opp_table[idx].gpufreq_vgpu;
	else
		return 0;
}

/* API : get vsram via OPP table index */
unsigned int mt_gpufreq_get_vsram_by_idx(unsigned int idx)
{
	idx += g_segment_max_opp_idx;
	if (idx < g_max_opp_idx_num)
		return g_opp_table[idx].gpufreq_vsram;
	else
		return 0;
}

/* API : get vsram via OPP table index */
unsigned int mt_gpufreq_get_vsram_by_real_idx(unsigned int idx)
{
	if (idx < g_max_opp_idx_num)
		return g_opp_table[idx].gpufreq_vsram;
	else
		return 0;
}

/* API: get opp idx in original opp tables */
/* This is usually for ptpod use */
unsigned int mt_gpufreq_get_ori_opp_idx(unsigned int idx)
{
	unsigned int ptpod_opp_idx_num;

	ptpod_opp_idx_num = ARRAY_SIZE(g_ptpod_opp_idx_table_segment);

	if (idx < ptpod_opp_idx_num && idx >= 0)
		return g_ptpod_opp_idx_table_segment[idx];
	else
		return idx;

}

/* API: pass GPU power table to EARA-QoS */
struct mt_gpufreq_power_table_info *pass_gpu_table_to_eara(void)
{
	return g_power_table;
}

/* API : get max power on power table */
unsigned int mt_gpufreq_get_max_power(void)
{
	return (!g_power_table) ?
			0 :
			g_power_table[g_segment_max_opp_idx].gpufreq_power;
}

/* API : get min power on power table */
unsigned int mt_gpufreq_get_min_power(void)
{
	return (!g_power_table) ?
			0 :
			g_power_table[g_segment_min_opp_idx].gpufreq_power;
}

/* API : get idx on opp table */
int mt_gpufreq_get_opp_idx_by_freq(unsigned int freq)
{
	int i = g_segment_min_opp_idx;

	while (i >= g_segment_max_opp_idx) {
		if (g_opp_table[i--].gpufreq_khz >= freq)
			goto EXIT;
	}

EXIT:
	return (i+1-g_segment_max_opp_idx);
}

/* API : get power on power table */
unsigned int mt_gpufreq_get_power_by_idx(int idx)
{
	if (!g_power_table)
		return 0;

	idx += g_segment_max_opp_idx;
	if (idx <= g_segment_min_opp_idx)
		return g_power_table[idx].gpufreq_power;
	else
		return 0;
}

/* API : get static leakage power */
unsigned int mt_gpufreq_get_leakage_mw(void)
{
	int temp = 0;
#if MT_GPUFREQ_STATIC_PWR_READY2USE
	unsigned int cur_vcore = __mt_gpufreq_get_cur_vgpu() / 100;
	int leak_power;
#endif

#ifdef CONFIG_THERMAL
	temp = get_immediate_gpu_wrap() / 1000;
#else
	temp = 40;
#endif

#if MT_GPUFREQ_STATIC_PWR_READY2USE
	leak_power = mt_spower_get_leakage(MTK_SPOWER_GPU, cur_vcore, temp);
	if (g_buck_on && leak_power > 0) {
		g_lkg_pwr = leak_power;
		return leak_power;
	} else {
		return 0;
	}
#else
	return 130;
#endif
}

//API : provide gpu lkg for swpm
unsigned int mt_gpufreq_get_leakage_no_lock(void)
{
	return g_lkg_pwr;
}

/*
 * API : get current segment max opp index
 */
unsigned int mt_gpufreq_get_seg_max_opp_index(void)
{
	return g_segment_max_opp_idx;
}

/*
 * API : get current Thermal/Power/PBM limited OPP table index
 */
unsigned int mt_gpufreq_get_thermal_limit_index(void)
{
	return g_max_upper_limited_idx - g_segment_max_opp_idx;
}

/*
 * API : get current Thermal/Power/PBM limited OPP table frequency
 */
unsigned int mt_gpufreq_get_thermal_limit_freq(void)
{
	return g_opp_table[g_max_upper_limited_idx].gpufreq_khz;
}
EXPORT_SYMBOL(mt_gpufreq_get_thermal_limit_freq);

/*
 * API : get current OPP table conditional index
 */
unsigned int mt_gpufreq_get_cur_freq_index(void)
{
	return (g_cur_opp_idx < g_segment_max_opp_idx) ?
			0 : g_cur_opp_idx - g_segment_max_opp_idx;
}

/*
 * API : get current OPP table frequency
 */
unsigned int mt_gpufreq_get_cur_freq(void)
{
	return g_cur_opp_freq;
}
EXPORT_SYMBOL(mt_gpufreq_get_cur_freq);

unsigned int mt_gpufreq_get_limit_user(unsigned int limit_user)
{
	if (limit_user == 0)
		return g_lower_kicker;

	if (limit_user == 1)
		return g_upper_kicker;

	return NUM_OF_KIR;
}
EXPORT_SYMBOL(mt_gpufreq_get_limit_user);

/*
 * API : get current voltage
 */
unsigned int mt_gpufreq_get_cur_volt(void)
{
	return (g_buck_on) ? g_cur_opp_vgpu : 0;
}
EXPORT_SYMBOL(mt_gpufreq_get_cur_volt);

/* API : get Thermal/Power/PBM limited OPP table index */
int mt_gpufreq_get_cur_ceiling_idx(void)
{
	return (int)mt_gpufreq_get_thermal_limit_index();
}

static unsigned int mt_gpufreq_get_limited_idx_by_power(
		unsigned int limited_power)
{
	int i;
	unsigned int limited_idx = g_segment_min_opp_idx;

	for (i = g_segment_max_opp_idx; i <= g_segment_min_opp_idx; i++) {
		if (g_power_table[i].gpufreq_power <= limited_power) {
			limited_idx = i;
			break;
		}
	}

	gpufreq_pr_debug("@%s: limited_power = %d, limited_idx = %d\n",
		__func__, limited_power, limited_idx);

	return limited_idx;
}

static unsigned int mt_gpufreq_get_limited_idx_by_freq(
		unsigned int limited_freq)
{
	int i;
	unsigned int limited_idx = g_segment_min_opp_idx;

	for (i = g_segment_max_opp_idx; i <= g_segment_min_opp_idx; i++) {
		if (g_opp_table[i].gpufreq_khz <= limited_freq) {
			limited_idx = i;
			break;
		}
	}

	gpufreq_pr_debug("@%s: limited_freq = %d, limited_idx = %d\n",
		__func__, limited_freq, limited_idx);

	return limited_idx;
}

#if MT_GPUFREQ_BATT_OC_PROTECT
void mt_gpufreq_batt_oc_callback(BATTERY_OC_LEVEL battery_oc_level)
{
	unsigned int batt_oc_limited_idx = LIMIT_IDX_DEFAULT;

	if (battery_oc_level == BATTERY_OC_LEVEL_1) {
		batt_oc_limited_idx =
			mt_gpufreq_get_limited_idx_by_freq(
			MT_GPUFREQ_BATT_OC_LIMIT_FREQ);

		mt_gpufreq_update_limit_idx(KIR_BATT_OC,
			batt_oc_limited_idx,
			LIMIT_IDX_DEFAULT);

		if (g_cur_opp_freq >
			g_opp_table[batt_oc_limited_idx].gpufreq_khz)
			mt_gpufreq_target(batt_oc_limited_idx, KIR_BATT_OC);
	} else {
		mt_gpufreq_update_limit_idx(KIR_BATT_OC,
			LIMIT_IDX_DEFAULT,
			LIMIT_IDX_DEFAULT);
	}

	gpufreq_pr_debug("battery_oc_level = %d, batt_oc_limited_idx = %d\n",
					battery_oc_level, batt_oc_limited_idx);
}
#endif

#if MT_GPUFREQ_BATT_PERCENT_PROTECT
void mt_gpufreq_batt_percent_callback(
		BATTERY_PERCENT_LEVEL battery_percent_level)
{
	unsigned int batt_percent_limited_idx = LIMIT_IDX_DEFAULT;

	if (battery_percent_level == BATTERY_PERCENT_LEVEL_1) {
		batt_percent_limited_idx =
			mt_gpufreq_get_limited_idx_by_freq(
			MT_GPUFREQ_BATT_PERCENT_LIMIT_FREQ);

		mt_gpufreq_update_limit_idx(KIR_BATT_PERCENT,
			batt_percent_limited_idx,
			LIMIT_IDX_DEFAULT);

		if (g_cur_opp_freq >
			g_opp_table[batt_percent_limited_idx].gpufreq_khz)
			mt_gpufreq_target(
				batt_percent_limited_idx,
				KIR_BATT_PERCENT);
	} else {
		mt_gpufreq_update_limit_idx(KIR_BATT_PERCENT,
				LIMIT_IDX_DEFAULT,
				LIMIT_IDX_DEFAULT);
	}

	gpufreq_pr_debug("battery_percent_level = %d, batt_percent_limited_idx = %d\n",
		battery_percent_level, batt_percent_limited_idx);
}
#endif

#if MT_GPUFREQ_LOW_BATT_VOLT_PROTECT
void mt_gpufreq_low_batt_callback(LOW_BATTERY_LEVEL low_battery_level)
{
	unsigned int low_batt_limited_idx = LIMIT_IDX_DEFAULT;

	if (low_battery_level == LOW_BATTERY_LEVEL_2) {
		low_batt_limited_idx =
			mt_gpufreq_get_limited_idx_by_freq(
			MT_GPUFREQ_LOW_BATT_VOLT_LIMIT_FREQ);

		mt_gpufreq_update_limit_idx(KIR_BATT_LOW,
			low_batt_limited_idx,
			LIMIT_IDX_DEFAULT);

		if (g_cur_opp_freq >
			g_opp_table[low_batt_limited_idx].gpufreq_khz)
			mt_gpufreq_target(low_batt_limited_idx, KIR_BATT_LOW);
	} else {
		mt_gpufreq_update_limit_idx(KIR_BATT_LOW,
			LIMIT_IDX_DEFAULT,
			LIMIT_IDX_DEFAULT);
	}

	gpufreq_pr_debug("low_battery_level = %d, low_batt_limited_idx = %d\n",
		low_battery_level, low_batt_limited_idx);
}
#endif

/*
 * API : set limited OPP table index for Thermal protection
 */
void mt_gpufreq_thermal_protect(unsigned int limited_power)
{
	unsigned int thermal_limited_idx = LIMIT_IDX_DEFAULT;

	mutex_lock(&mt_gpufreq_power_lock);

#if MT_GPUFREQ_DYNAMIC_POWER_TABLE_UPDATE
	__mt_update_gpufreqs_power_table();
#endif

	if (limited_power == 0) {
		mt_gpufreq_update_limit_idx(KIR_THERMAL,
			LIMIT_IDX_DEFAULT,
			LIMIT_IDX_DEFAULT);
	} else {
		thermal_limited_idx =
			mt_gpufreq_get_limited_idx_by_power(limited_power);

		mt_gpufreq_update_limit_idx(KIR_THERMAL,
			thermal_limited_idx,
			LIMIT_IDX_DEFAULT);

		if (g_cur_opp_freq >
			g_opp_table[thermal_limited_idx].gpufreq_khz)
			mt_gpufreq_target(thermal_limited_idx, KIR_THERMAL);
	}

	gpufreq_pr_debug("limited_power = %d, thermal_limited_idx = %d\n",
			limited_power, thermal_limited_idx);

	mutex_unlock(&mt_gpufreq_power_lock);
}

/* API : set limited OPP table index by PBM */
void mt_gpufreq_set_power_limit_by_pbm(unsigned int limited_power)
{
	unsigned int pbm_limited_idx = LIMIT_IDX_DEFAULT;

	mutex_lock(&mt_gpufreq_power_lock);

	if (limited_power == 0) {
		mt_gpufreq_update_limit_idx(KIR_PBM,
			LIMIT_IDX_DEFAULT,
			LIMIT_IDX_DEFAULT);
	} else {
		pbm_limited_idx =
			mt_gpufreq_get_limited_idx_by_power(limited_power);

		mt_gpufreq_update_limit_idx(KIR_PBM,
			pbm_limited_idx,
			LIMIT_IDX_DEFAULT);

		if (g_cur_opp_freq >
			g_opp_table[pbm_limited_idx].gpufreq_khz)
			mt_gpufreq_target(pbm_limited_idx, KIR_PBM);
	}

	gpufreq_pr_debug("limited_power = %d, pbm_limited_idx = %d\n",
			limited_power, pbm_limited_idx);

	mutex_unlock(&mt_gpufreq_power_lock);
}

/*
 * API : set GPU loading for SSPM
 */
void mt_gpufreq_set_loading(unsigned int gpu_loading)
{
	/* legacy */
}

/*
 * API : register GPU power limited notifiction callback
 */
void mt_gpufreq_power_limit_notify_registerCB(gpufreq_power_limit_notify pCB)
{
	/* legacy */
}

static unsigned int __mt_gpufreq_get_segment_id(void)
{
	static unsigned int segment_id = -1;
	unsigned int efuse_id;

	if (segment_id != -1)
		return segment_id;

	/* spare[7:0] */
	efuse_id = (get_devinfo_with_index(30) & 0xFF);

	switch (efuse_id) {
	case 0x10:
		segment_id = MT6891_SEGMENT;
		break;
	case 0x40:
		segment_id = MT6893_SEGMENT;
		break;
	default:
		segment_id = MT6893_SEGMENT;
		gpufreq_pr_info("invalid efuse id: 0x%x\n", efuse_id);
	}

	gpufreq_pr_info("@%s: efuse_id: 0x%x, segment_id: %d\n",
						__func__, efuse_id, segment_id);

	return segment_id;
}

static struct opp_table_info *__mt_gpufreq_get_segment_table(void)
{
	unsigned int efuse_id;

	/* [FAB_INFO4 (0x11C107B0) 134] [2:0]*/
	efuse_id = ((get_devinfo_with_index(134) >> 0) & 0x3);

	switch (efuse_id) {
	case 0x2:
		return g_opp_table_segment_2;
	default:
		return g_opp_table_segment_1;
	}
}

/**
 * ===============================================
 * SECTION : PROCFS interface for debugging
 * ===============================================
 */
static int mt_gpufreq_dfd_test_proc_show(struct seq_file *m, void *v)
{
#if MT_GPUFREQ_DFD_DEBUG
	/* for UT only!!
	 * do not enable this debug node on MP release
	 * MTBF may trigger this BUG_ON
	 */

	//if (g_cg_on) {
	//	seq_printf(m, "dfd trigger!\n");
	//	BUG_ON(1);
	//	mt_gpufreq_software_trigger_dfd();
	//} else {
	//	seq_printf(m, "gpu if power off\n");
	//}
#endif
	return 0;
}

static int mt_gpufreq_dfd_force_dump_proc_show(struct seq_file *m, void *v)
{
#if MT_GPUFREQ_DFD_ENABLE
	seq_puts(m, "0: disable\n");
	seq_puts(m, "1: force dump + debug log\n");
	seq_puts(m, "2: dump\n");
	seq_puts(m, "3: debug log\n");
	seq_printf(m, "g_dfd_force_dump %d\n", g_dfd_force_dump);
	mt_gpufreq_dump_infra_status();
#else
	seq_puts(m, "dfd not support\n");
#endif
	return 0;
}

static ssize_t mt_gpufreq_dfd_force_dump_proc_write(
		struct file *file, const char __user *buffer,
		size_t count, loff_t *data)
{
	int ret = -EFAULT;
#if MT_GPUFREQ_DFD_ENABLE
	char buf[64];
	unsigned int len = 0;
	unsigned int value = 0;

	len = (count < (sizeof(buf) - 1)) ? count : (sizeof(buf) - 1);

	if (copy_from_user(buf, buffer, len))
		goto out;

	buf[len] = '\0';

	if (!kstrtouint(buf, 10, &value)) {
		ret = 0;
		g_dfd_force_dump = value;
	}

out:
#endif
	return (ret < 0) ? ret : count;
}

#ifdef CONFIG_PROC_FS
static int mt_gpufreq_opp_dump_proc_show(struct seq_file *m, void *v)
{
	int i;

	for (i = g_segment_max_opp_idx; i <= g_segment_min_opp_idx; i++) {
		seq_printf(m, "[%02d] ",
				i - g_segment_max_opp_idx);
		seq_printf(m, "freq = %d, ",
				g_opp_table[i].gpufreq_khz);
		seq_printf(m, "vgpu = %d, ",
				g_opp_table[i].gpufreq_vgpu);
		seq_printf(m, "vsram = %d, ",
				g_opp_table[i].gpufreq_vsram);
		seq_printf(m, "posdiv = %d, ",
				(1 << g_opp_table[i].gpufreq_post_divider));
		seq_printf(m, "gpu_power = %d, ",
				g_power_table[i].gpufreq_power);
		seq_printf(m, "aging = %d\n",
				g_opp_table[i].gpufreq_aging_margin);
	}

	return 0;
}

static int mt_gpufreq_sb_idx_proc_show(struct seq_file *m, void *v)
{
	int i, max_opp_idx;

	max_opp_idx = g_segment_max_opp_idx;

	for (i = max_opp_idx; i <= g_segment_min_opp_idx; i++) {
		seq_printf(m,
				"[%02d] ", i - max_opp_idx);
		seq_printf(m, "g_opp_sb_idx_up = %d, ",
				g_opp_sb_idx_up[i] - max_opp_idx >= 0 ?
				g_opp_sb_idx_up[i] - max_opp_idx : 0);
		seq_printf(m, "g_opp_sb_idx_down = %d\n",
				g_opp_sb_idx_down[i] - max_opp_idx >= 0 ?
				g_opp_sb_idx_down[i] - max_opp_idx : 0);
	}

	return 0;
}

static int mt_gpufreq_var_dump_proc_show(struct seq_file *m, void *v)
{
	int i;
	unsigned int gpu_loading = 0;

#ifdef CONFIG_MTK_GPU_SUPPORT
	mtk_get_gpu_loading(&gpu_loading);
#endif

	seq_printf(m, "idx: %d, freq: %d, vgpu: %d, vsram_gpu: %d\n",
			g_cur_opp_idx - g_segment_max_opp_idx,
			g_cur_opp_freq,
			g_cur_opp_vgpu,
			g_cur_opp_vsram_gpu);
	seq_printf(m, "(real) freq: %d, freq: %d, vgpu: %d, vsram_gpu: %d\n",
			mt_get_ckgen_freq(hf_fmfg_ck),
			__mt_gpufreq_get_cur_freq(),
			__mt_gpufreq_get_cur_vgpu(),
			__mt_gpufreq_get_cur_vsram_gpu());
	seq_printf(m, "segment_id = %d\n", __mt_gpufreq_get_segment_id());
	seq_printf(m, "g_power_count = %d, g_cg_on = %d, g_mtcmos_on = %d, g_buck_on = %d\n",
			g_power_count, g_cg_on, g_mtcmos_on, g_buck_on);
	seq_printf(m, "g_opp_stress_test_state = %d\n",
			g_opp_stress_test_state);
	seq_printf(m, "g_max_upper_limited_idx = %d\n",
			g_max_upper_limited_idx - g_segment_max_opp_idx);
	seq_printf(m, "g_min_lower_limited_idx = %d\n",
			g_min_lower_limited_idx - g_segment_max_opp_idx);
	seq_printf(m, "gpu_loading = %d\n",
			gpu_loading);

	return 0;
}

/*
 * PROCFS : show current opp stress test state
 */
static int mt_gpufreq_opp_stress_test_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "g_opp_stress_test_state: %d\n", g_opp_stress_test_state);
	return 0;
}

/*
 * PROCFS : opp stress test message setting
 * 0 : disable
 * 1 : enable for segment OPP table
 * 2 : enable for real OPP table
 */
static ssize_t mt_gpufreq_opp_stress_test_proc_write(
		struct file *file, const char __user *buffer,
		size_t count, loff_t *data)
{
	char buf[64];
	unsigned int len = 0;
	unsigned int value = 0;
	int ret = -EFAULT;

	len = (count < (sizeof(buf) - 1)) ? count : (sizeof(buf) - 1);

	if (copy_from_user(buf, buffer, len))
		goto out;

	buf[len] = '\0';

	if (!kstrtouint(buf, 10, &value)) {
		if (!value || !(value-1)) {
			ret = 0;
			g_opp_stress_test_state = value;
				if (g_opp_stress_test_state == 0) {
					mt_gpufreq_update_limit_idx(
						KIR_STRESS,
						LIMIT_IDX_DEFAULT,
						LIMIT_IDX_DEFAULT);
			}
		}
	}

out:
	return (ret < 0) ? ret : count;
}

static int mt_gpufreq_power_stress_test_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "g_opp_power_test_state: %d\n", g_opp_power_test_state);
	return 0;
}

/*
 * power stress test
 * the times of power on/off
 */
static ssize_t mt_gpufreq_power_stress_test_proc_write(
		struct file *file, const char __user *buffer,
		size_t count, loff_t *data)
{
	char buf[64];
	unsigned int len = 0;
	unsigned int value = 0;
	unsigned int delay = 0;
	unsigned int rand;
	int ret = -EFAULT;

	len = (count < (sizeof(buf) - 1)) ? count : (sizeof(buf) - 1);

	if (copy_from_user(buf, buffer, len))
		goto out;

	buf[len] = '\0';

	if (!kstrtouint(buf, 10, &value)) {
		ret = 0;
		g_opp_power_test_state = value;
	}

	while (g_opp_power_test_state > 0) {
		g_opp_power_test_state--;

		mt_gpufreq_power_control(POWER_ON, CG_ON, MTCMOS_ON, BUCK_ON);
		// delay 5 ~ 10ms
		get_random_bytes(&rand, sizeof(rand));
		delay = (rand % 6) + 5;
		mdelay(delay);
		mt_gpufreq_power_control(
			POWER_OFF, CG_OFF, MTCMOS_OFF, BUCK_OFF);
		// delay 5 ~ 10ms
		get_random_bytes(&rand, sizeof(rand));
		delay = (rand % 6) + 5;
		mdelay(delay);
	}

	g_opp_power_test_state = 0;
out:
	return (ret < 0) ? ret : count;
}

static int mt_gpufreq_aging_enable_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "g_aging_enable = %d\n", g_aging_enable);
	return 0;
}

static ssize_t mt_gpufreq_aging_enable_proc_write(
		struct file *file, const char __user *buffer,
		size_t count, loff_t *data)
{
	char buf[64];
	unsigned int len = 0;
	unsigned int value = 0;
	int ret = -EFAULT;

	len = (count < (sizeof(buf) - 1)) ? count : (sizeof(buf) - 1);

	if (copy_from_user(buf, buffer, len))
		goto out;

	buf[len] = '\0';

	mutex_lock(&mt_gpufreq_lock);
	if (!kstrtouint(buf, 10, &value)) {
		if (!value || !(value-1)) {
			ret = 0;
			if (g_aging_enable ^ value) {
				g_aging_enable = value;
				mt_gpufreq_apply_aging(value);
			}
		}
	}
	mutex_unlock(&mt_gpufreq_lock);

out:
	return (ret < 0) ? ret : count;
}

static int mt_gpufreq_limit_table_proc_show(struct seq_file *m, void *v)
{
	int i;

	seq_puts(m, "echo [id][up_enable][low_enable] > /proc/gpufreq/gpufreq_limit_table\n");
	seq_puts(m, "ex: echo 3 0 0 > /proc/gpufreq/gpufreq_limit_table\n");
	seq_puts(m, "means disable THERMAL upper_limit_idx & lower_limit_idx\n\n");

	seq_printf(m, "%15s %5s %10s %10s %10s %10s %10s\n",
		"[name]", "[id]", "[prio]",
		"[up_idx]", "[up_enable]",
		"[low_idx]", "[low_enable]");

	mutex_lock(&mt_gpufreq_limit_table_lock);
	for (i = 0; i < NUM_OF_KIR; i++) {
		seq_printf(m, "%15s %5d %10d %10d %10d %10d %10d\n",
		limit_table[i].name,
		i,
		limit_table[i].prio,
		limit_table[i].upper_idx == LIMIT_IDX_DEFAULT
		? LIMIT_IDX_DEFAULT
		: limit_table[i].upper_idx - g_segment_max_opp_idx,
		limit_table[i].upper_enable,
		limit_table[i].lower_idx == LIMIT_IDX_DEFAULT
		? LIMIT_IDX_DEFAULT
		: limit_table[i].lower_idx - g_segment_max_opp_idx,
		limit_table[i].lower_enable
		);
	}
	mutex_unlock(&mt_gpufreq_limit_table_lock);

	return 0;
}

static ssize_t mt_gpufreq_limit_table_proc_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *data)
{
	char buf[64];
	unsigned int len = 0;
	int ret = -EFAULT;
	unsigned int kicker;
	int upper_en, lower_en;

	len = (count < (sizeof(buf) - 1)) ? count : (sizeof(buf) - 1);

	if (copy_from_user(buf, buffer, len))
		goto out;

	buf[len] = '\0';

	if (sscanf(buf, "%d %d %d",
		&kicker, &upper_en, &lower_en) == 3) {
		if (kicker >= NUM_OF_KIR)
			goto out;
		if (upper_en != LIMIT_DISABLE &&
			upper_en != LIMIT_ENABLE)
			goto out;
		if (lower_en != LIMIT_DISABLE &&
			lower_en != LIMIT_ENABLE)
			goto out;

		ret = 0;
		mt_gpufreq_update_limit_enable(
			kicker, upper_en, lower_en);
	}

out:
	return (ret < 0) ? ret : count;
}

/*
 * PROCFS : show current keeping OPP frequency state
 */
static int mt_gpufreq_opp_freq_proc_show(struct seq_file *m, void *v)
{
	unsigned int keep_opp_freq_idx;

	mutex_lock(&mt_gpufreq_limit_table_lock);
	keep_opp_freq_idx = limit_table[KIR_PROC].upper_idx;
	mutex_unlock(&mt_gpufreq_limit_table_lock);

	if (keep_opp_freq_idx != LIMIT_IDX_DEFAULT) {
		seq_puts(m, "[GPU-DVFS] fixed OPP is enabled\n");
		seq_printf(m, "[%d] ",
				keep_opp_freq_idx - g_segment_max_opp_idx);
		seq_printf(m, "freq = %d, ",
				g_opp_table[keep_opp_freq_idx].gpufreq_khz);
		seq_printf(m, "vgpu = %d, ",
				g_opp_table[keep_opp_freq_idx].gpufreq_vgpu);
		seq_printf(m, "vsram = %d\n",
				g_opp_table[keep_opp_freq_idx].gpufreq_vsram);
	} else
		seq_puts(m, "[GPU-DVFS] fixed OPP is disabled\n");

	return 0;
}

/*
 * PROCFS : keeping OPP frequency setting
 * 0 : free run
 * 1 : keep OPP frequency
 */
static ssize_t mt_gpufreq_opp_freq_proc_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *data)
{
	char buf[64];
	unsigned int len = 0;
	unsigned int value = 0;
	unsigned int i = 0;
	int ret = -EFAULT;

	len = (count < (sizeof(buf) - 1)) ? count : (sizeof(buf) - 1);

	if (copy_from_user(buf, buffer, len))
		goto out;

	buf[len] = '\0';

	if (kstrtouint(buf, 10, &value) == 0) {
		if (value == 0) {
			mt_gpufreq_update_limit_idx(KIR_PROC,
				LIMIT_IDX_DEFAULT,
				LIMIT_IDX_DEFAULT);
		} else {
			for (i = g_segment_max_opp_idx;
				i <= g_segment_min_opp_idx;
				i++) {
				if (value == g_opp_table[i].gpufreq_khz) {
					ret = 0;
					mt_gpufreq_update_limit_idx(
						KIR_PROC, i, i);
					mt_gpufreq_target(i, KIR_PROC);
					break;
				}
			}
		}
	}

out:
	return (ret < 0) ? ret : count;
}

/*
 * PROCFS : show current fixed freq & volt state
 */
static int mt_gpufreq_fixed_freq_volt_proc_show(struct seq_file *m, void *v)
{
	if (g_fixed_freq_volt_state) {
		seq_puts(m, "[GPU-DVFS] fixed freq & volt is enabled\n");
		seq_printf(m, "g_fixed_freq = %d\n", g_fixed_freq);
		seq_printf(m, "g_fixed_vgpu = %d\n", g_fixed_vgpu);
	} else
		seq_puts(m, "[GPU-DVFS] fixed freq & volt is disabled\n");

	return 0;
}

/*
 * PROCFS : fixed freq & volt state setting
 */
static ssize_t mt_gpufreq_fixed_freq_volt_proc_write(
		struct file *file, const char __user *buffer,
		size_t count, loff_t *data)
{
	char buf[64];
	unsigned int len = 0;
	int ret = -EFAULT;
	unsigned int fixed_freq = 0;
	unsigned int fixed_volt = 0;

	len = (count < (sizeof(buf) - 1)) ? count : (sizeof(buf) - 1);

	if (copy_from_user(buf, buffer, len))
		goto out;

	buf[len] = '\0';

	if (sscanf(buf, "%d %d", &fixed_freq, &fixed_volt) == 2) {
		ret = 0;
		if (fixed_volt > VGPU_MAX_VOLT) {
			gpufreq_pr_debug("fixed volt(%d) > VGPU_MAX_VOLT(%d)\n",
					fixed_volt, VGPU_MAX_VOLT);
			goto out;
		} else if (fixed_volt < VGPU_MIN_VOLT && fixed_volt > 0) {
			gpufreq_pr_debug("fixed volt(%d) < VGPU_MIN_VOLT(%d)\n",
					fixed_volt, VGPU_MIN_VOLT);
			goto out;
		} else if (fixed_freq > POSDIV_4_MAX_FREQ) {
			gpufreq_pr_debug("fixed freq(%d) > POSDIV_4_MAX_FREQ(%d)\n",
					fixed_freq, POSDIV_4_MAX_FREQ);
			goto out;
		} else if (fixed_freq < POSDIV_8_MIN_FREQ && fixed_freq > 0) {
			gpufreq_pr_debug("fixed freq(%d) < POSDIV_8_MIN_FREQ(%d)\n",
					fixed_freq, POSDIV_8_MIN_FREQ);
			goto out;
		}

		mutex_lock(&mt_gpufreq_lock);
		if ((fixed_freq == 0) && (fixed_volt == 0)) {
			fixed_freq =
				g_opp_table[g_segment_min_opp_idx].gpufreq_khz;
			fixed_volt =
				g_opp_table[g_segment_min_opp_idx].gpufreq_vgpu;
			g_cur_opp_freq = __mt_gpufreq_get_cur_freq();
			if (fixed_freq > g_cur_opp_freq) {
				__mt_gpufreq_set_fixed_vgpu(fixed_volt);
				__mt_gpufreq_set_fixed_freq(fixed_freq);
			} else {
				__mt_gpufreq_set_fixed_freq(fixed_freq);
				__mt_gpufreq_set_fixed_vgpu(fixed_volt);
			}
			g_fixed_freq = 0;
			g_fixed_vgpu = 0;
			g_fixed_freq_volt_state = false;
		} else {
			g_cur_opp_freq = __mt_gpufreq_get_cur_freq();
			fixed_volt = VOLT_NORMALIZATION(fixed_volt);
			if (fixed_freq > g_cur_opp_freq) {
				__mt_gpufreq_set_fixed_vgpu(fixed_volt);
				__mt_gpufreq_set_fixed_freq(fixed_freq);
			} else {
				__mt_gpufreq_set_fixed_freq(fixed_freq);
				__mt_gpufreq_set_fixed_vgpu(fixed_volt);
			}
			g_fixed_freq_volt_state = true;
		}
		mutex_unlock(&mt_gpufreq_lock);
	}

out:
	return (ret < 0) ? ret : count;
}

/*
 * PROCFS : initialization
 */
PROC_FOPS_RW(gpufreq_opp_stress_test);
PROC_FOPS_RW(gpufreq_power_stress_test);
PROC_FOPS_RO(gpufreq_opp_dump);
PROC_FOPS_RW(gpufreq_opp_freq);
PROC_FOPS_RO(gpufreq_var_dump);
PROC_FOPS_RW(gpufreq_fixed_freq_volt);
PROC_FOPS_RO(gpufreq_sb_idx);
PROC_FOPS_RW(gpufreq_aging_enable);
PROC_FOPS_RW(gpufreq_limit_table);
PROC_FOPS_RO(gpufreq_dfd_test);
PROC_FOPS_RW(gpufreq_dfd_force_dump);

static int __mt_gpufreq_create_procfs(void)
{
	struct proc_dir_entry *dir = NULL;
	int i;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	const struct pentry entries[] = {
		PROC_ENTRY(gpufreq_opp_stress_test),
		PROC_ENTRY(gpufreq_power_stress_test),
		PROC_ENTRY(gpufreq_opp_dump),
		PROC_ENTRY(gpufreq_opp_freq),
		PROC_ENTRY(gpufreq_var_dump),
		PROC_ENTRY(gpufreq_fixed_freq_volt),
		PROC_ENTRY(gpufreq_sb_idx),
		PROC_ENTRY(gpufreq_aging_enable),
		PROC_ENTRY(gpufreq_limit_table),
		PROC_ENTRY(gpufreq_dfd_test),
		PROC_ENTRY(gpufreq_dfd_force_dump),
	};

	dir = proc_mkdir("gpufreq", NULL);
	if (!dir) {
		gpufreq_pr_info("fail to create /proc/gpufreq\n");
		return -ENOMEM;
	}

	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!proc_create(entries[i].name, 0660, dir, entries[i].fops))
			gpufreq_pr_info("create /proc/gpufreq/%s failed\n",
					entries[i].name);
	}

	return 0;
}
#endif

/*
 * calculate springboard opp index to avoid buck variation,
 * the voltage between VGPU and VSRAM_GPU must be in 0mV ~ 350mV
 * that is, 0mV <= VSRAM_GPU - VGPU <= 350mV
 * (variation: VGPU / VSRAM_GPU {-6.25% / max(+8%, +53mV)}
 */
static void mt_gpufreq_cal_sb_opp_index(void)
{
	int i, j, diff;
	int min_vsram_idx = g_max_opp_idx_num - 1;

	for (i = 0; i < g_max_opp_idx_num; i++) {
		if (g_opp_table[i].gpufreq_vsram ==
			g_opp_table[g_max_opp_idx_num - 1].gpufreq_vsram) {
			min_vsram_idx = i;
			break;
		}
	}
	gpufreq_pr_debug("@%s: min_vsram_idx: %d\n", __func__,
			min_vsram_idx);

	/* build up table */
	for (i = 0; i < g_max_opp_idx_num; i++) {
		g_opp_sb_idx_up[i] = min_vsram_idx;
		for (j = 0; j <= min_vsram_idx; j++) {
			diff = g_opp_table[i].gpufreq_vgpu + BUCK_DIFF_MAX;
			if (g_opp_table[j].gpufreq_vsram <= diff) {
				g_opp_sb_idx_up[i] = j;
				break;
			}
		}
		gpufreq_pr_debug("@%s: g_opp_sb_idx_up[%d]: %d\n", __func__,
				i, g_opp_sb_idx_up[i]);
	}

	/* build down table */
	for (i = 0; i < g_max_opp_idx_num; i++) {
		if (i >= min_vsram_idx)
			g_opp_sb_idx_down[i] = g_max_opp_idx_num - 1;
		else {
			for (j = g_max_opp_idx_num - 1; j >= 0; j--) {
				diff =
				g_opp_table[i].gpufreq_vsram - BUCK_DIFF_MAX;
				if (g_opp_table[j].gpufreq_vgpu >= diff) {
					g_opp_sb_idx_down[i] = j;
					break;
				}
			}
		}
		gpufreq_pr_debug("@%s: g_opp_sb_idx_down[%d]: %d\n", __func__,
				i, g_opp_sb_idx_down[i]);
	}
}

/*
 * frequency ramp up/down handler
 * - frequency ramp up need to wait voltage settle
 * - frequency ramp down do not need to wait voltage settle
 */
static void __mt_gpufreq_set(
		unsigned int idx_old, unsigned int idx_new,
		unsigned int freq_old, unsigned int freq_new,
		unsigned int vgpu_old, unsigned int vgpu_new,
		unsigned int vsram_gpu_old, unsigned int vsram_gpu_new)
{
	unsigned int sb_idx = 0;

	gpufreq_pr_logbuf(
		"begin idx: %d -> %d, freq: %d -> %d, vgpu: %d -> %d, vsram_gpu: %d -> %d\n",
		idx_old, idx_new,
		freq_old, freq_new,
		vgpu_old, vgpu_new,
		vsram_gpu_old, vsram_gpu_new);

	if (freq_new == freq_old) {
		__mt_gpufreq_volt_switch(
				vgpu_old, vgpu_new,
				vsram_gpu_old, vsram_gpu_new);

	} else if (freq_new > freq_old) {
		while (g_cur_opp_vgpu != vgpu_new) {
			sb_idx = g_opp_sb_idx_up[g_cur_opp_idx] < idx_new ?
				idx_new : g_opp_sb_idx_up[g_cur_opp_idx];

			__mt_gpufreq_volt_switch(
			g_cur_opp_vgpu, g_opp_table[sb_idx].gpufreq_vgpu,
			g_cur_opp_vsram_gpu,
			g_opp_table[sb_idx].gpufreq_vsram);

			g_cur_opp_idx = sb_idx;
			g_cur_opp_vgpu = g_opp_table[sb_idx].gpufreq_vgpu;
			g_cur_opp_vsram_gpu = g_opp_table[sb_idx].gpufreq_vsram;
		}

		__mt_gpufreq_clock_switch(freq_new);
		g_cur_opp_freq = __mt_gpufreq_get_cur_freq();

		gpu_assert(g_cur_opp_freq == freq_new,
			GPU_FREQ_EXCEPTION,
			"Clock switch failing: %d -> %d (target: %d)\n",
			freq_old, g_cur_opp_freq, freq_new);
	} else {
		__mt_gpufreq_clock_switch(freq_new);
		g_cur_opp_freq = __mt_gpufreq_get_cur_freq();

		gpu_assert(g_cur_opp_freq == freq_new,
			GPU_FREQ_EXCEPTION,
			"Clock switch failing: %d -> %d (target: %d)\n",
			freq_old, g_cur_opp_freq, freq_new);

		while (g_cur_opp_vgpu != vgpu_new) {
			sb_idx = g_opp_sb_idx_down[g_cur_opp_idx] > idx_new ?
				idx_new : g_opp_sb_idx_down[g_cur_opp_idx];

			__mt_gpufreq_volt_switch(
			g_cur_opp_vgpu, g_opp_table[sb_idx].gpufreq_vgpu,
			g_cur_opp_vsram_gpu,
			g_opp_table[sb_idx].gpufreq_vsram);

			g_cur_opp_idx = sb_idx;
			g_cur_opp_vgpu = g_opp_table[sb_idx].gpufreq_vgpu;
			g_cur_opp_vsram_gpu = g_opp_table[sb_idx].gpufreq_vsram;
		}
	}

	/* update "g_cur_opp_idx" when "Vgpu old" and "Vgpu new" is the same */
	g_cur_opp_idx = idx_new;
	g_cur_opp_freq = __mt_gpufreq_get_cur_freq();
	g_cur_opp_vgpu = __mt_gpufreq_get_cur_vgpu();
	g_cur_opp_vsram_gpu = __mt_gpufreq_get_cur_vsram_gpu();

	gpu_dvfs_oppidx_footprint(idx_new);

	gpufreq_pr_logbuf(
		"done idx: %d -> %d, clk: %d, freq: %d, vgpu: %d, vsram_gpu: %d\n",
		idx_old, idx_new,
		mt_get_ckgen_freq(hf_fmfg_ck),
		__mt_gpufreq_get_cur_freq(),
		__mt_gpufreq_get_cur_vgpu(),
		__mt_gpufreq_get_cur_vsram_gpu());

	__mt_gpufreq_kick_pbm(1);
}

/*
 * dds calculation for clock switching
 * Fin is 26 MHz
 * VCO Frequency = Fin * N_INFO
 * MFGPLL output Frequency = VCO Frequency / POSDIV
 * N_INFO = MFGPLL output Frequency * POSDIV / FIN
 * N_INFO[21:14] = FLOOR(N_INFO, 8)
 */
static unsigned int __mt_gpufreq_calculate_dds(
					unsigned int freq_khz,
					enum g_posdiv_power_enum posdiv_power)
{
	unsigned int dds = 0;

	/* only use posdiv 4 or 8 */
	if ((freq_khz >= POSDIV_8_MIN_FREQ) &&
					(freq_khz <= POSDIV_4_MAX_FREQ)) {
		dds = (((freq_khz / TO_MHZ_HEAD * (1 << posdiv_power))
			<< DDS_SHIFT) /
			MFGPLL_FIN + ROUNDING_VALUE) / TO_MHZ_TAIL;
	} else {
		gpufreq_pr_info("out of range, freq_khz: %d\n", freq_khz);
	}

	return dds;
}

static void __mt_gpufreq_switch_to_clksrc(enum g_clock_source_enum clksrc)
{
	int ret;

	ret = clk_prepare_enable(g_clk->clk_mux);
	if (ret)
		gpufreq_pr_info("enable clk_mux(TOP_MUX_MFG) failed:%d\n",
				ret);

	if (clksrc == CLOCK_MAIN) {
		ret = clk_set_parent(g_clk->clk_mux, g_clk->clk_main_parent);
		if (ret)
			gpufreq_pr_info("switch to main clock source failed: %d\n",
					ret);

	} else if (clksrc == CLOCK_SUB) {
		ret = clk_set_parent(g_clk->clk_mux, g_clk->clk_sub_parent);
		if (ret)
			gpufreq_pr_info("switch to sub clock source failed: %d\n",
					ret);

	} else {
		gpufreq_pr_info("clock source index is not valid, clksrc: %d\n",
				clksrc);
	}

	clk_disable_unprepare(g_clk->clk_mux);
}

static enum g_posdiv_power_enum __mt_gpufreq_get_posdiv_power(unsigned int freq)
{
	int i;

	for (i = 0; i < NUM_OF_OPP_IDX; i++) {
		if (g_opp_table[i].gpufreq_khz <= freq)
			return g_opp_table[i].gpufreq_post_divider;
	}

	gpufreq_pr_info("freq %d find no post divider\n", freq);
	return POSDIV_POWER_4;
}

static enum g_posdiv_power_enum __mt_gpufreq_get_curr_posdiv_power(void)
{
	unsigned long mfgpll;
	enum g_posdiv_power_enum real_posdiv_power;

	mfgpll = DRV_Reg32(MFGPLL_CON1);

	real_posdiv_power = (mfgpll & (0x7 << POSDIV_SHIFT)) >> POSDIV_SHIFT;

	return real_posdiv_power;
}

static void __mt_gpufreq_clock_switch(unsigned int freq_new)
{
	enum g_posdiv_power_enum posdiv_power;
	enum g_posdiv_power_enum real_posdiv_power;
	unsigned int dds, pll;
	bool parking = false;

	/*
	 * MFGPLL_CON1[26:24] is POST DIVIDER
	 * 3'b000 : /1  (POSDIV_POWER_1)
	 * 3'b001 : /2  (POSDIV_POWER_2)
	 * 3'b010 : /4  (POSDIV_POWER_4)
	 * 3'b011 : /8  (POSDIV_POWER_8)
	 * 3'b100 : /16 (POSDIV_POWER_16)
	 */
	real_posdiv_power = __mt_gpufreq_get_curr_posdiv_power();
	posdiv_power = __mt_gpufreq_get_posdiv_power(freq_new);
	dds = __mt_gpufreq_calculate_dds(freq_new, posdiv_power);
	pll = (0x80000000) | (posdiv_power << POSDIV_SHIFT) | dds;

#if (!defined(CONFIG_MTK_FREQ_HOPPING)) || !MT_GPUFREQ_DVFS_HOPPING_ENABLE
	/* force parking if FHCTL not ready */
	parking = true;
#else
	if (posdiv_power != real_posdiv_power)
		parking = true;
	else
		parking = false;
#endif

	if (parking) {
		/* mfgpll_ck to univpll_d3(416MHz) */
		__mt_gpufreq_switch_to_clksrc(CLOCK_SUB);

		/*
		 * MFGPLL_CON1[31:31] = MFGPLL_SDM_PCW_CHG
		 * MFGPLL_CON1[26:24] = MFGPLL_POSDIV
		 * MFGPLL_CON1[21:0]  = MFGPLL_SDM_PCW (dds)
		 */
		DRV_WriteReg32(MFGPLL_CON1, pll);

		/* PLL spec */
		udelay(20);

		/* univpll_d3(416MHz) to mfgpll_ck */
		__mt_gpufreq_switch_to_clksrc(CLOCK_MAIN);
	} else {
#ifdef CONFIG_MTK_FREQ_HOPPING
		mt_dfs_general_pll(MFGPLL_FH_PLL, dds);
#endif
	}

	gpufreq_pr_logbuf(
	"posdiv: %d, real_posdiv: %d, dds: 0x%x, pll: 0x%08x, parking: %d\n",
	(1 << posdiv_power), (1 << real_posdiv_power), dds, pll, parking);
}

/*
 * switch voltage and vsram via PMIC
 */
static void __mt_gpufreq_volt_switch_without_vsram_gpu(
		unsigned int vgpu_old, unsigned int vgpu_new)
{
	unsigned int vsram_gpu_new, vsram_gpu_old;

	vgpu_new = VOLT_NORMALIZATION(vgpu_new);

	vsram_gpu_new = __mt_gpufreq_get_vsram_gpu_by_vgpu(vgpu_new);
	vsram_gpu_old = __mt_gpufreq_get_vsram_gpu_by_vgpu(vgpu_old);

	__mt_gpufreq_volt_switch(
			vgpu_old,
			vgpu_new,
			vsram_gpu_old,
			vsram_gpu_new);
}

/*
 * switch voltage and vsram via PMIC
 */
static void __mt_gpufreq_volt_switch(
		unsigned int vgpu_old, unsigned int vgpu_new,
		unsigned int vsram_gpu_old, unsigned int vsram_gpu_new)
{
	unsigned int vgpu_settle_time, vsram_settle_time, final_settle_time;

	if (vgpu_new > vgpu_old) {
		/* rising */
		vgpu_settle_time = __calculate_vgpu_settletime(
			true, (vgpu_new - vgpu_old));
		vsram_settle_time = __calculate_vsram_settletime(
			true, (vsram_gpu_new - vsram_gpu_old));

		regulator_set_voltage(
				g_pmic->reg_vsram_gpu,
				vsram_gpu_new * 10,
				VSRAM_GPU_MAX_VOLT * 10 + 125);
		regulator_set_voltage(
				g_pmic->reg_vgpu,
				vgpu_new * 10,
				VGPU_MAX_VOLT * 10 + 125);

	} else if (vgpu_new < vgpu_old) {
		/* falling */
		vgpu_settle_time = __calculate_vgpu_settletime(
			false, (vgpu_old - vgpu_new));
		vsram_settle_time = __calculate_vsram_settletime(
			false, (vsram_gpu_old - vsram_gpu_new));

		regulator_set_voltage(
				g_pmic->reg_vgpu,
				vgpu_new * 10,
				VGPU_MAX_VOLT * 10 + 125);
		regulator_set_voltage(
				g_pmic->reg_vsram_gpu,
				vsram_gpu_new * 10,
				VSRAM_GPU_MAX_VOLT * 10 + 125);
	} else {
		/* voltage no change */
		return;
	}

	final_settle_time = (vgpu_settle_time > vsram_settle_time) ?
		vgpu_settle_time : vsram_settle_time;
	udelay(final_settle_time);

	gpufreq_pr_logbuf("Vgpu: %d, Vsram_gpu: %d, udelay: %d\n",
		__mt_gpufreq_get_cur_vgpu(), __mt_gpufreq_get_cur_vsram_gpu(),
		final_settle_time);
}

/*
 * set AUTO_MODE or PWM_MODE to VGPU
 * REGULATOR_MODE_FAST: PWM Mode
 * REGULATOR_MODE_NORMAL: Auto Mode
 */
static void __mt_gpufreq_vgpu_set_mode(unsigned int mode)
{
	int ret;

	ret = regulator_set_mode(g_pmic->reg_vgpu, mode);

	if (ret == 0)
		gpufreq_pr_debug("set AUTO_MODE(%d) or PWM_MODE(%d) mode: %d\n",
			REGULATOR_MODE_NORMAL, REGULATOR_MODE_FAST, mode);
	else
		gpufreq_pr_info("failed to configure mode, ret = %d, mode = %d\n",
				ret, mode);
}

/*
 * set fixed frequency for PROCFS: fixed_freq_volt
 */
static void __mt_gpufreq_set_fixed_freq(int fixed_freq)
{
	gpufreq_pr_debug("before, g_fixed_freq = %d, g_fixed_vgpu = %d\n",
			g_fixed_freq, g_fixed_vgpu);

	g_fixed_freq = fixed_freq;
	g_fixed_vgpu = g_cur_opp_vgpu;

	gpufreq_pr_debug("@%s: now, g_fixed_freq = %d, g_fixed_vgpu = %d\n",
			__func__,
			g_fixed_freq, g_fixed_vgpu);

	__mt_gpufreq_clock_switch(g_fixed_freq);

	g_cur_opp_freq = g_fixed_freq;
}

/*
 * set fixed voltage for PROCFS: fixed_freq_volt
 */
static void __mt_gpufreq_set_fixed_vgpu(int fixed_vgpu)
{
	gpufreq_pr_debug("@%s: before, g_fixed_freq = %d, g_fixed_vgpu = %d\n",
			__func__,
			g_fixed_freq, g_fixed_vgpu);

	g_fixed_freq = g_cur_opp_freq;
	g_fixed_vgpu = fixed_vgpu;

	gpufreq_pr_debug("@%s: now, g_fixed_freq = %d, g_fixed_vgpu = %d\n",
			__func__,
			g_fixed_freq, g_fixed_vgpu);

	__mt_gpufreq_volt_switch_without_vsram_gpu(
			g_cur_opp_vgpu,
			g_fixed_vgpu);

	g_cur_opp_vgpu = g_fixed_vgpu;
	g_cur_opp_vsram_gpu =
			__mt_gpufreq_get_vsram_gpu_by_vgpu(g_fixed_vgpu);
}

/* power calculation for power table */
static void __mt_gpufreq_calculate_power(
		unsigned int idx, unsigned int freq,
		unsigned int volt, unsigned int temp)
{
	unsigned int p_total = 0;
	unsigned int p_dynamic = 0;
	unsigned int ref_freq = 0;
	unsigned int ref_vgpu = 0;
	int p_leakage = 0;

	p_dynamic = GPU_ACT_REF_POWER;
	ref_freq = GPU_ACT_REF_FREQ;
	ref_vgpu = GPU_ACT_REF_VOLT;

	p_dynamic = p_dynamic *
			((freq * 100) / ref_freq) *
			((volt * 100) / ref_vgpu) *
			((volt * 100) / ref_vgpu) /
			(100 * 100 * 100);
#if MT_GPUFREQ_STATIC_PWR_READY2USE
	p_leakage = mt_spower_get_leakage(MTK_SPOWER_GPU, (volt / 100), temp);
	if (!g_buck_on || p_leakage < 0)
		p_leakage = 0;
#else
	p_leakage = 71;
#endif

	p_total = p_dynamic + p_leakage;

	gpufreq_pr_debug("idx: %d, dynamic: %d, leakage: %d, total: %d, temp: %d\n",
			idx, p_dynamic, p_leakage, p_total, temp);

	g_power_table[idx].gpufreq_power = p_total;
}

/*
 * VGPU settle time calculation
 * mode(false) : falling
 * mode(true) : rising
 * deltaV : voltage diff(100mv)
 * return : settle time(us)
 */
static unsigned int __calculate_vgpu_settletime(bool mode, int deltaV)
{
	unsigned int settleTime;
	/* [MT6315][VGPU]
	 * DVFS Rising : delta(V) / 12.5mV + 4us + 5us
	 * DVFS Falling: delta(V) / 5mV + 4us + 5us
	 */

	if (mode) {
		/* rising 12.5mv/us*/
		settleTime = deltaV / (125 * 10) + 9;
	} else {
		/* falling 5mv/us*/
		settleTime = deltaV / (5 * 100) + 9;
	}
	return settleTime;
}

/*
 * VSRAM_GPU settle time calculation
 * mode(false) : falling
 * mode(true) : rising
 * deltaV : voltage diff(100mv)
 * return : settle time(us)
 */
static unsigned int __calculate_vsram_settletime(bool mode, int deltaV)
{
	unsigned int settleTime;
	/* [MT6359][VSRAM_GPU]
	 * DVFS Rising : delta(V) / 12.5mV + 3us + 5us
	 * DVFS Falling: delta(V) / 5mV + 3us + 5us
	 */

	if (mode) {
		/* rising 12.5mv/us*/
		settleTime = deltaV / (125 * 10) + 8;
	} else {
		/* falling 5mv/us*/
		settleTime = deltaV / (5 * 100) + 8;
	}
	return settleTime;
}

/*
 * get current frequency (KHZ)
 * Freq = ((PLL_CON1[21:0] * 26M) / 2^14) / 2^PLL_CON1[26:24]
 */
static unsigned int __mt_gpufreq_get_cur_freq(void)
{
	unsigned long mfgpll = 0;
	unsigned int posdiv_power = 0;
	unsigned int freq_khz = 0;
	unsigned long dds;

	mfgpll = DRV_Reg32(MFGPLL_CON1);

	dds = mfgpll & (0x3FFFFF);

	posdiv_power = (mfgpll & (0x7 << POSDIV_SHIFT)) >> POSDIV_SHIFT;

	freq_khz = (((dds * TO_MHZ_TAIL + ROUNDING_VALUE) * MFGPLL_FIN) >>
			DDS_SHIFT) / (1 << posdiv_power) * TO_MHZ_HEAD;

	return freq_khz;
}

/*
 * get current vsram voltage (mV * 100)
 */
static unsigned int __mt_gpufreq_get_cur_vsram_gpu(void)
{
	unsigned int volt = 0;

	/* regulator_get_voltage prints volt with uV */
	volt = regulator_get_voltage(g_pmic->reg_vsram_gpu) / 10;

	return volt;
}

/*
 * get current voltage (mV * 100)
 */
static unsigned int __mt_gpufreq_get_cur_vgpu(void)
{
	unsigned int volt = 0;

	/* regulator_get_voltage prints volt with uV */
	volt = regulator_get_voltage(g_pmic->reg_vgpu) / 10;

	return volt;
}

/*
 * get OPP table index by voltage (mV * 100)
 */
static int __mt_gpufreq_get_opp_idx_by_vgpu(unsigned int vgpu)
{
	int i = g_max_opp_idx_num - 1;

	while (i >= 0) {
		if (g_opp_table[i--].gpufreq_vgpu >= vgpu)
			goto out;
	}

out:
	return i + 1;
}

/*
 * calculate vsram_gpu via given vgpu
 * PTPOD only change vgpu, so we need change vsram by vgpu.
 */
static unsigned int __mt_gpufreq_get_vsram_gpu_by_vgpu(unsigned int vgpu)
{
	unsigned int vsram_gpu;

	if (vgpu > FIXED_VSRAM_VOLT_THSRESHOLD)
		vsram_gpu = vgpu + FIXED_VSRAM_VOLT_DIFF;
	else
		vsram_gpu = FIXED_VSRAM_VOLT;

	gpufreq_pr_debug("@%s: vgpu = %d, vsram_gpu = %d\n", __func__,
			vgpu, vsram_gpu);

	return vsram_gpu;
}

#if MT_GPUFREQ_DYNAMIC_POWER_TABLE_UPDATE
/* update OPP power table */
static void __mt_update_gpufreqs_power_table(void)
{
	int i;
	int temp = 0;
	unsigned int freq = 0;
	unsigned int volt = 0;

#ifdef CONFIG_THERMAL
	temp = get_immediate_gpu_wrap() / 1000;
#else
	temp = 40;
#endif

	gpufreq_pr_debug("@%s: temp = %d\n", __func__, temp);

	mutex_lock(&mt_gpufreq_lock);

	if ((temp >= -20) && (temp <= 125)) {
		for (i = 0; i < g_max_opp_idx_num; i++) {

			freq = g_power_table[i].gpufreq_khz;
			volt = g_power_table[i].gpufreq_vgpu;

			__mt_gpufreq_calculate_power(i, freq, volt, temp);

			gpufreq_pr_debug(
				"@%s: [%d] freq: %d, vgpu: %d, power: %d\n",
				__func__, i,
				g_power_table[i].gpufreq_khz,
				g_power_table[i].gpufreq_vgpu,
				g_power_table[i].gpufreq_power);
		}
	} else {
		gpufreq_pr_info("@%s: temp < -20 or temp > 125\n", __func__);
	}

	mutex_unlock(&mt_gpufreq_lock);
}
#endif

/*
 * kick Power Budget Manager(PBM) when OPP changed
 */
static void __mt_gpufreq_kick_pbm(int enable)
{
	unsigned int power;
	unsigned int cur_freq;
	unsigned int cur_vgpu;
	unsigned int found = 0;
	int tmp_idx = -1;
	int i;

	cur_freq = __mt_gpufreq_get_cur_freq();
	cur_vgpu = __mt_gpufreq_get_cur_vgpu();

	if (enable) {
		for (i = 0; i < g_max_opp_idx_num; i++) {
			if (g_power_table[i].gpufreq_khz == cur_freq) {
				/*
				 * record idx since
				 * current voltage may
				 * not in DVFS table
				 */
				tmp_idx = i;

				if (g_power_table[i].gpufreq_vgpu == cur_vgpu) {
					power = g_power_table[i].gpufreq_power;
					found = 1;
#if MT_GPUFREQ_KICKER_PBM_READY
					kicker_pbm_by_gpu(true,
							power, cur_vgpu / 100);
#endif
					return;
				}
			}
		}

		if (!found) {
			gpufreq_pr_debug("tmp_idx = %d\n", tmp_idx);
			if (tmp_idx != -1 && tmp_idx < g_max_opp_idx_num) {
				/*
				 * use freq to find
				 * corresponding power budget
				 */
				power = g_power_table[tmp_idx].gpufreq_power;
#if MT_GPUFREQ_KICKER_PBM_READY
				kicker_pbm_by_gpu(true, power, cur_vgpu / 100);
#endif
			}
		}
	} else {
#if MT_GPUFREQ_KICKER_PBM_READY
		kicker_pbm_by_gpu(false, 0, cur_vgpu / 100);
#endif
	}
}

static void __mt_gpufreq_init_table(void)
{
	struct opp_table_info *opp_table = __mt_gpufreq_get_segment_table();
	unsigned int segment_id = __mt_gpufreq_get_segment_id();
	unsigned int i = 0;

	// no segment limit
	g_segment_max_opp_idx = 0;

	/* determine max_opp/min_opp... by segment */
	if (segment_id == MT6891_SEGMENT)
		g_segment_max_opp_idx = 8; //836MHz
	else if (segment_id == MT6893_SEGMENT)
		g_segment_max_opp_idx = 0; // 886MHz

	g_segment_min_opp_idx = NUM_OF_OPP_IDX - 1;

	g_opp_table = kzalloc((NUM_OF_OPP_IDX)*sizeof(*opp_table), GFP_KERNEL);

	if (g_opp_table == NULL)
		return;

	for (i = 0; i < NUM_OF_OPP_IDX; i++) {
		g_opp_table[i].gpufreq_khz = opp_table[i].gpufreq_khz;
		g_opp_table[i].gpufreq_vgpu = opp_table[i].gpufreq_vgpu;
		g_opp_table[i].gpufreq_vsram = opp_table[i].gpufreq_vsram;
		g_opp_table[i].gpufreq_post_divider =
					opp_table[i].gpufreq_post_divider;
		g_opp_table[i].gpufreq_aging_margin =
					opp_table[i].gpufreq_aging_margin;

		gpufreq_pr_debug("@%s: [%u] freq: %u, vgpu: %u, vsram: %u, aging: %u\n",
				__func__,
				i,
				opp_table[i].gpufreq_khz,
				opp_table[i].gpufreq_vgpu,
				opp_table[i].gpufreq_vsram,
				opp_table[i].gpufreq_aging_margin);
	}

	g_max_opp_idx_num = NUM_OF_OPP_IDX;
	g_max_upper_limited_idx = g_segment_max_opp_idx;
	g_min_lower_limited_idx = g_segment_min_opp_idx;

	gpufreq_pr_debug("@%s: g_segment_max_opp_idx = %u, g_max_opp_idx_num = %u, g_segment_min_opp_idx = %u\n",
			__func__,
			g_segment_max_opp_idx,
			g_max_opp_idx_num,
			g_segment_min_opp_idx);

	mutex_lock(&mt_gpufreq_lock);
	mt_gpufreq_cal_sb_opp_index();
	mutex_unlock(&mt_gpufreq_lock);
	__mt_gpufreq_setup_opp_power_table(NUM_OF_OPP_IDX);
}

/*
 * OPP power table initialization
 */
static void __mt_gpufreq_setup_opp_power_table(int num)
{
	int i = 0;
	int temp = 0;

	g_power_table = kzalloc(
			(num) * sizeof(struct mt_gpufreq_power_table_info),
			GFP_KERNEL);

	if (g_power_table == NULL)
		return;

#ifdef CONFIG_THERMAL
	temp = get_immediate_gpu_wrap() / 1000;
#else
	temp = 40;
#endif

	gpufreq_pr_debug(
			"@%s: temp = %d\n",
			__func__,
			temp);

	if ((temp < -20) || (temp > 125)) {
		gpufreq_pr_debug("@%s: temp < -20 or temp > 125!\n", __func__);
		temp = 65;
	}

	for (i = 0; i < num; i++) {
		g_power_table[i].gpufreq_khz = g_opp_table[i].gpufreq_khz;
		g_power_table[i].gpufreq_vgpu = g_opp_table[i].gpufreq_vgpu;

		__mt_gpufreq_calculate_power(i, g_power_table[i].gpufreq_khz,
				g_power_table[i].gpufreq_vgpu, temp);

		gpufreq_pr_debug("@%s: [%d], freq_khz = %u, vgpu = %u, power = %u\n",
				__func__,
				i,
				g_power_table[i].gpufreq_khz,
				g_power_table[i].gpufreq_vgpu,
				g_power_table[i].gpufreq_power);
	}

#ifdef CONFIG_THERMAL
	mtk_gpufreq_register(g_power_table, num);
#endif
}

/*
 * I/O remap
 */
static void *__mt_gpufreq_of_ioremap(const char *node_name, int idx)
{
	struct device_node *node;

	node = of_find_compatible_node(NULL, NULL, node_name);

	if (node)
		return of_iomap(node, idx);

	return NULL;
}

static void __mt_gpufreq_init_volt_by_freq(void)
{
	struct opp_table_info *opp_table = g_opp_table;
	unsigned int freq, idx;

	freq = __mt_gpufreq_get_cur_freq();
	gpufreq_pr_info("Preloader init freq is %d\n", freq);

	if (mt_gpufreq_get_cust_init_en()) {
		freq = MT_GPUFREQ_CUST_INIT_OPP;
		gpufreq_pr_info("CUST request freq to %d\n", freq);
	}

	/*
	 *  freq need to check lower/upper bound
	 *  because get real mfg will not correct
	 */
	if (freq >= opp_table[0].gpufreq_khz) {
		/* get Maximum opp */
		idx = 0;
	} else if (freq <= opp_table[NUM_OF_OPP_IDX - 1].gpufreq_khz) {
		/* get Minimum opp */
		idx = NUM_OF_OPP_IDX - 1;
	} else {
		for (idx = 1; idx < NUM_OF_OPP_IDX; idx++) {
			if (opp_table[idx].gpufreq_khz <= freq) {
				/* find the idx with closest freq */
				if ((freq - opp_table[idx].gpufreq_khz) >
					opp_table[idx-1].gpufreq_khz - freq)
					idx -= 1;
				break;
			}
		}
	}

	g_cur_opp_idx = idx;
	g_cur_opp_freq = __mt_gpufreq_get_cur_freq();
	g_cur_opp_vgpu = __mt_gpufreq_get_cur_vgpu();
	g_cur_opp_vsram_gpu = __mt_gpufreq_get_cur_vsram_gpu();

	if (!mt_gpufreq_get_dvfs_en() && !mt_gpufreq_get_cust_init_en()) {
		gpufreq_pr_info("GPU DVFS and CUST INIT Disabled !!!\n");
	} else {
		mutex_lock(&mt_gpufreq_lock);
		__mt_gpufreq_set(g_cur_opp_idx, idx,
			g_cur_opp_freq, opp_table[idx].gpufreq_khz,
			g_cur_opp_vgpu, opp_table[idx].gpufreq_vgpu,
			g_cur_opp_vsram_gpu, opp_table[idx].gpufreq_vsram);
		mutex_unlock(&mt_gpufreq_lock);
	}
}

static int __mt_gpufreq_init_pmic(struct platform_device *pdev)
{
	if (g_pmic == NULL)
		g_pmic = kzalloc(sizeof(struct g_pmic_info), GFP_KERNEL);
	if (g_pmic == NULL)
		return -ENOMEM;

	g_pmic->reg_vgpu =
			regulator_get_optional(&pdev->dev, "_vgpu");
	if (IS_ERR(g_pmic->reg_vgpu)) {
		gpufreq_pr_info("@%s: cannot get VGPU, %ld\n",
			__func__, PTR_ERR(g_pmic->reg_vgpu));
		return PTR_ERR(g_pmic->reg_vgpu);
	}

	g_pmic->reg_vsram_gpu =
			regulator_get_optional(&pdev->dev, "_vsram_gpu");
	if (IS_ERR(g_pmic->reg_vsram_gpu)) {
		gpufreq_pr_info("@%s: cannot get VSRAM_GPU, %ld\n",
			__func__, PTR_ERR(g_pmic->reg_vsram_gpu));
		return PTR_ERR(g_pmic->reg_vsram_gpu);
	}

	return 0;
}

static int __mt_gpufreq_init_clk(struct platform_device *pdev)
{
	/* MFGPLL is from APMIXED and its parent clock is from XTAL(26MHz); */
	g_apmixed_base = __mt_gpufreq_of_ioremap("mediatek,apmixed", 0);
	if (!g_apmixed_base) {
		gpufreq_pr_info("@%s: ioremap failed at APMIXED", __func__);
		return -ENOENT;
	}

	g_mfg_base = __mt_gpufreq_of_ioremap("mediatek,g3d_config", 0);
	if (!g_mfg_base) {
		gpufreq_pr_info("@%s: ioremap failed at g3d_config",
			__func__);
		return -ENOENT;
	}

	if (g_clk == NULL)
		g_clk = kzalloc(sizeof(struct g_clk_info), GFP_KERNEL);
	if (g_clk == NULL)
		return -ENOMEM;

	g_clk->clk_mux = devm_clk_get(&pdev->dev, "clk_mux");
	if (IS_ERR(g_clk->clk_mux)) {
		gpufreq_pr_info("@%s: cannot get clk_mux\n", __func__);
		return PTR_ERR(g_clk->clk_mux);
	}

	g_clk->clk_main_parent = devm_clk_get(&pdev->dev, "clk_main_parent");
	if (IS_ERR(g_clk->clk_main_parent)) {
		gpufreq_pr_info("@%s: cannot get clk_main_parent\n", __func__);
		return PTR_ERR(g_clk->clk_main_parent);
	}

	g_clk->clk_sub_parent = devm_clk_get(&pdev->dev, "clk_sub_parent");
	if (IS_ERR(g_clk->clk_sub_parent)) {
		gpufreq_pr_info("@%s: cannot get clk_sub_parent\n", __func__);
		return PTR_ERR(g_clk->clk_sub_parent);
	}

	g_clk->subsys_mfg_cg = devm_clk_get(&pdev->dev, "subsys_mfg_cg");
	if (IS_ERR(g_clk->subsys_mfg_cg)) {
		gpufreq_pr_info("@%s: cannot get subsys_mfg_cg\n", __func__);
		return PTR_ERR(g_clk->subsys_mfg_cg);
	}

	g_clk->mtcmos_mfg_async = devm_clk_get(&pdev->dev, "mtcmos_mfg_async");
	if (IS_ERR(g_clk->mtcmos_mfg_async)) {
		gpufreq_pr_info("@%s: cannot get mtcmos_mfg_async\n", __func__);
		return PTR_ERR(g_clk->mtcmos_mfg_async);
	}

	g_clk->mtcmos_mfg = devm_clk_get(&pdev->dev, "mtcmos_mfg");
	if (IS_ERR(g_clk->mtcmos_mfg)) {
		gpufreq_pr_info("@%s: cannot get mtcmos_mfg\n", __func__);
		return PTR_ERR(g_clk->mtcmos_mfg);
	}

	g_clk->mtcmos_mfg_core0 = devm_clk_get(&pdev->dev, "mtcmos_mfg_core0");
	if (IS_ERR(g_clk->mtcmos_mfg_core0)) {
		gpufreq_pr_info("@%s: cannot get mtcmos_mfg_core0\n", __func__);
		return PTR_ERR(g_clk->mtcmos_mfg_core0);
	}

	g_clk->mtcmos_mfg_core1_2 = devm_clk_get(&pdev->dev,
		"mtcmos_mfg_core1_2");
	if (IS_ERR(g_clk->mtcmos_mfg_core1_2)) {
		gpufreq_pr_info("@%s: cannot get mtcmos_mfg_core1_2\n",
			__func__);
		return PTR_ERR(g_clk->mtcmos_mfg_core1_2);
	}

	g_clk->mtcmos_mfg_core3_4 = devm_clk_get(&pdev->dev,
		"mtcmos_mfg_core3_4");
	if (IS_ERR(g_clk->mtcmos_mfg_core3_4)) {
		gpufreq_pr_info("@%s: cannot get mtcmos_mfg_core3_4\n",
			__func__);
		return PTR_ERR(g_clk->mtcmos_mfg_core3_4);
	}

	g_clk->mtcmos_mfg_core5_6 = devm_clk_get(&pdev->dev,
		"mtcmos_mfg_core5_6");
	if (IS_ERR(g_clk->mtcmos_mfg_core5_6)) {
		gpufreq_pr_info("@%s: cannot get mtcmos_mfg_core5_6\n",
			__func__);
		return PTR_ERR(g_clk->mtcmos_mfg_core5_6);
	}

	g_clk->mtcmos_mfg_core7_8 = devm_clk_get(&pdev->dev,
		"mtcmos_mfg_core7_8");
	if (IS_ERR(g_clk->mtcmos_mfg_core7_8)) {
		gpufreq_pr_info("@%s: cannot get mtcmos_mfg_core7_8\n",
			__func__);
		return PTR_ERR(g_clk->mtcmos_mfg_core7_8);
	}

	g_infracfg_base = __mt_gpufreq_of_ioremap("mediatek,infracfg", 0);
	if (!g_infracfg_base) {
		gpufreq_pr_info("@%s: ioremap failed at infracfg",
			__func__);
		return -ENOENT;
	}

	g_infra_bpi_bsi_slv0 = __mt_gpufreq_of_ioremap(
		"mediatek,bpi_bsi_slv0", 0);
	if (!g_infra_bpi_bsi_slv0) {
		gpufreq_pr_info("@%s: ioremap failed at bpi_bsi_slv0",
			__func__);
		return -ENOENT;
	}

	g_infra_peri_debug1 = __mt_gpufreq_of_ioremap(
		"mediatek,devapc_ao_infra_peri_debug1", 0);
	if (!g_infra_peri_debug1) {
		gpufreq_pr_info("@%s: ioremap failed at devapc_ao_infra_peri_debug1",
			__func__);
		return -ENOENT;
	}

	g_infra_peri_debug2 = __mt_gpufreq_of_ioremap(
		"mediatek,devapc_ao_infra_peri_debug2", 0);
	if (!g_infra_peri_debug2) {
		gpufreq_pr_info("@%s: ioremap failed at devapc_ao_infra_peri_debug2",
			__func__);
		return -ENOENT;
	}

	g_infra_peri_debug3 = __mt_gpufreq_of_ioremap(
		"mediatek,devapc_ao_infra_peri_debug3", 0);
	if (!g_infra_peri_debug3) {
		gpufreq_pr_info("@%s: ioremap failed at devapc_ao_infra_peri_debug3",
			__func__);
		return -ENOENT;
	}

	g_infra_peri_debug4 = __mt_gpufreq_of_ioremap(
		"mediatek,devapc_ao_infra_peri_debug4", 0);
	if (!g_infra_peri_debug4) {
		gpufreq_pr_info("@%s: ioremap failed at devapc_ao_infra_peri_debug4",
			__func__);
		return -ENOENT;
	}

	g_infracfg_ao = __mt_gpufreq_of_ioremap("mediatek,infracfg_ao", 0);
	if (!g_infracfg_ao) {
		gpufreq_pr_info("@%s: ioremap failed at infracfg_ao",
			__func__);
		return -ENOENT;
	}

	g_dbgtop = __mt_gpufreq_of_ioremap("mediatek,dbgtop", 0);
	if (!g_dbgtop) {
		gpufreq_pr_info("@%s: ioremap failed at dbgtop",
			__func__);
		return -ENOENT;
	}

	g_sleep = __mt_gpufreq_of_ioremap("mediatek,sleep", 0);
	if (!g_sleep) {
		gpufreq_pr_info("@%s: ioremap failed at sleep",
			__func__);
		return -ENOENT;
	}

	g_toprgu = __mt_gpufreq_of_ioremap("mediatek,toprgu", 0);
	if (!g_toprgu) {
		gpufreq_pr_info("@%s: ioremap failed at toprgu",
			__func__);
		return -ENOENT;
	}

	return 0;
}

static void __mt_gpufreq_init_power(void)
{
#if MT_GPUFREQ_STATIC_PWR_READY2USE
	/* Initial leackage power usage */
	mt_spower_init();
#endif

#if MT_GPUFREQ_LOW_BATT_VOLT_PROTECT
	register_low_battery_notify(
			&mt_gpufreq_low_batt_callback,
			LOW_BATTERY_PRIO_GPU);
#endif

#if MT_GPUFREQ_BATT_PERCENT_PROTECT
	register_battery_percent_notify(
			&mt_gpufreq_batt_percent_callback,
			BATTERY_PERCENT_PRIO_GPU);
#endif

#if MT_GPUFREQ_BATT_OC_PROTECT
	register_battery_oc_notify(
			&mt_gpufreq_batt_oc_callback,
			BATTERY_OC_PRIO_GPU);
#endif
}

#if MT_GPUFREQ_DFD_DEBUG
/*
 * software trigger gpu dfd and power off MTCMOS
 * this function can simulate gpu dfd trigger scenario
 *
 * to let this function work, you need modify
 * /kernel-4.14/drivers/clk/mediatek/clk-mt6885-pg.c
 * + #define IGNORE_MTCMOS_CHECK
 */
static void __mt_gpufreq_gpu_dfd_trigger_simulate(void)
{
	gpufreq_pr_info("[GPU_DFD] power on MTCMOS, prepare gpu DFD\n");
	mt_gpufreq_mtcmos_control(POWER_ON);

	gpufreq_pr_info("[GPU_DFD] dfd software trigger\n");
	__mt_gpufreq_config_dfd(true);
	mt_gpufreq_software_trigger_dfd();

	gpufreq_pr_info("[GPU_DFD] wait dfd complete\n");
	while (!__mt_gpufreq_is_dfd_completed())
		gpufreq_pr_info("[GPU_DFD] waiting...");

	gpufreq_pr_info("[GPU_DFD] wait dfd complete done!\n");

	gpufreq_pr_info("[GPU_DFD] power off MTCMOS\n");
	mt_gpufreq_mtcmos_control(POWER_OFF);

	gpufreq_pr_info("[GPU_DFD] gpu dfd trigger simulate complete!\n");
}
#endif

static void __mt_gpufreq_gpu_hard_reset(void)
{
	/*
	 * // [2] = mfg_rst, reset mfg
	 * Write register WDT_SWSYSRST (@ 0x1000_7018) = 0x88000004;
	 * Wait 10us;
	 * // [2] = mfg_rst, release reset
	 * Write register WDT_SWSYSRST (@ 0x1000_7018) = 0x88000000;
	 */
	writel(0x88000004, g_toprgu + 0x018);
	udelay(10);
	writel(0x88000000, g_toprgu + 0x018);
}

/*
 * clear gpu dfd if it is triggerd
 * this is a workaround to prevent dev apc violation
 */
static void __mt_gpufreq_gpu_dfd_clear(void)
{
	/* 0. gpu hard reset*/
#if MT_GPUFREQ_DFD_DEBUG
	gpufreq_pr_info("[GPU_DFD] step0: gpu hard reset\n");
#endif
	__mt_gpufreq_gpu_hard_reset();

	/* 1. do fake power on */
#if MT_GPUFREQ_DFD_DEBUG
	gpufreq_pr_info("[GPU_DFD] step1: do fake power on\n");
#endif
	mt_gpufreq_power_control(POWER_ON, CG_ON, MTCMOS_ON, BUCK_ON);

	/* 2. unlock sspm
	 * Write register MFG_DFD_CON_0 (@ 0x13FB_FA00) = 0x0f000011
	 */
#if MT_GPUFREQ_DFD_DEBUG
	gpufreq_pr_info("[GPU_DFD] step2: unlock sspm\n");
#endif
	writel(0x0F000011, g_mfg_base + 0xA00);

	/* 3. clear wdt_mfg_pwr_on, let power control back to SPM */
#if MT_GPUFREQ_DFD_DEBUG
	gpufreq_pr_info("[GPU_DFD] step3: clear wdt_mfg_pwr_on\n");
#endif
	//writel(0x77 << 24, g_dbgtop + 0x060);
	__mt_gpufreq_dbgtop_pwr_on(false);

	/* 4. clear gpu dfd setting */
	/* 5. disable drm power */
#if MT_GPUFREQ_DFD_DEBUG
	gpufreq_pr_info("[GPU_DFD] step4&5: clear gpu dfd setting, disable drm power\n");
#endif
	__mt_gpufreq_config_dfd(false);

	/* 6. power off gpu */
#if MT_GPUFREQ_DFD_DEBUG
	gpufreq_pr_info("[GPU_DFD] step6: power off gpu\n");
#endif
	mt_gpufreq_power_control(POWER_OFF, CG_OFF, MTCMOS_OFF, BUCK_OFF);

#if MT_GPUFREQ_DFD_DEBUG
	gpufreq_pr_info("[GPU_DFD] step7: check dfd status: triggered %d\n",
		__mt_gpufreq_is_dfd_triggered());
#endif

#if MT_GPUFREQ_DFD_DEBUG
	gpufreq_pr_info("[GPU_DFD] step8: enable wdt_mfg_pwr_on\n");
#endif
	__mt_gpufreq_dbgtop_pwr_on(true);
}

/*
 * gpufreq driver probe
 */
static int __mt_gpufreq_pdrv_probe(struct platform_device *pdev)
{
	struct device_node *node;
	int ret;

	gpufreq_pr_info("@%s start\n", __func__);

	node = of_find_matching_node(NULL, g_gpufreq_of_match);
	if (!node)
		gpufreq_pr_info("@%s: find GPU node failed\n", __func__);

#if MT_GPUFREQ_DFD_ENABLE
	if (mtk_dbgtop_mfg_pwr_en(1)) {
		gpufreq_pr_info("[GPU_DFD] wait dbgtop ready\n");
		return EPROBE_DEFER;
	}
#endif

	/* init pmic regulator */
	ret = __mt_gpufreq_init_pmic(pdev);
	if (ret)
		return ret;

	/* init clock source and mtcmos */
	ret = __mt_gpufreq_init_clk(pdev);
	if (ret)
		return ret;

	/* init opp table */
	__mt_gpufreq_init_table();

#if MT_GPUFREQ_DFD_ENABLE
	__mt_gpufreq_dbgtop_pwr_on(true);
#endif

	if (!mt_gpufreq_power_ctl_en()) {
		gpufreq_pr_info("@%s: Power Control Always On !!!\n", __func__);
		mt_gpufreq_power_control(POWER_ON, CG_ON, MTCMOS_ON, BUCK_ON);
	}

	/* init Vgpu/Vsram_gpu by bootup freq index */
	__mt_gpufreq_init_volt_by_freq();

	__mt_gpufreq_init_power();

#if defined(AGING_LOAD)
	gpufreq_pr_info("@%s: AGING load\n", __func__);
	g_aging_enable = 1;
#endif

#if MT_GPUFREQ_DFD_DEBUG
	// for debug only. simulate gpu dfd trigger state
	//__mt_gpufreq_gpu_dfd_trigger_simulate();
#endif

	g_probe_done = true;
	gpufreq_pr_info("@%s: GPU driver init done\n", __func__);

	return 0;
}

/*
 * register the gpufreq driver
 */
static int __init __mt_gpufreq_init(void)
{
	int ret = 0;

	if (mt_gpufreq_bringup()) {
		gpufreq_pr_info("skip driver init: clock ID: %d\n", hf_fmfg_ck);
		return 0;
	}

	gpufreq_pr_debug("start to initialize gpufreq driver\n");

#ifdef CONFIG_PROC_FS
	if (__mt_gpufreq_create_procfs())
		goto out;
#endif

	/* register platform driver */
	ret = platform_driver_register(&g_gpufreq_pdrv);
	if (ret)
		gpufreq_pr_info("fail to register gpufreq driver\n");

out:
	gpu_dvfs_vgpu_reset_footprint();
	gpu_dvfs_oppidx_reset_footprint();
	gpu_dvfs_power_count_reset_footprint();

	return ret;
}

/*
 * unregister the gpufreq driver
 */
static void __exit __mt_gpufreq_exit(void)
{
	platform_driver_unregister(&g_gpufreq_pdrv);
}

module_init(__mt_gpufreq_init);
module_exit(__mt_gpufreq_exit);

MODULE_DEVICE_TABLE(of, g_gpufreq_of_match);
MODULE_DESCRIPTION("MediaTek GPU-DVFS-PLAT driver");
MODULE_LICENSE("GPL");
