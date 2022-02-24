/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

/**
 * @file	mtk_gpufreq_core
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
#include <linux/seq_file.h>

#include "mtk_gpufreq.h"
#include "mtk_gpufreq_core.h"

#include "mtk_pmic_wrap.h"
#include "mtk_devinfo.h"
#include "upmu_common.h"
#include "upmu_sw.h"
#include "upmu_hw.h"
#include "mtk_dramc.h"
#ifdef CONFIG_THERMAL
#include "mtk_thermal.h"
#endif
#ifdef CONFIG_MTK_FREQ_HOPPING
#include "mtk_freqhopping_drv.h"
#endif
#if MT_GPUFREQ_KICKER_PBM_READY == 1
#include "mtk_pbm.h"
#endif
#if MT_GPUFREQ_STATIC_PWR_READY2USE == 1
#include "mtk_static_power.h"
#include "mtk_static_power_mt6785.h"
#endif

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
static int __mt_gpufreq_get_opp_idx_by_vgpu(unsigned int vgpu);
static unsigned int __mt_gpufreq_get_vsram_gpu_by_vgpu(unsigned int vgpu);
static void __mt_gpufreq_update_intepolating_volt(
		unsigned int idx, unsigned int high,
		unsigned int low, int factor);
static unsigned int __mt_gpufreq_get_limited_freq_by_power(
		unsigned int limited_power);
static enum g_posdiv_power_enum __mt_gpufreq_get_posdiv_power(
		unsigned int freq);
static void __mt_gpufreq_switch_to_clksrc(enum g_clock_source_enum clksrc);
static void __mt_gpufreq_kick_pbm(int enable);
static void __mt_gpufreq_clock_switch(unsigned int freq_new);
static void __mt_gpufreq_volt_switch(
		unsigned int vgpu_old, unsigned int vgpu_new,
		unsigned int vsram_gpu_old, unsigned int vsram_gpu_new);
static void __mt_gpufreq_volt_switch_without_vsram_gpu(
		unsigned int vgpu_old, unsigned int vgpu_new);
static void __mt_gpufreq_update_aging(bool apply_aging_setting);

#if MT_GPUFREQ_BATT_OC_PROTECT == 1
static void __mt_gpufreq_batt_oc_protect(unsigned int limited_idx);
#endif

#if MT_GPUFREQ_BATT_PERCENT_PROTECT == 1
static void __mt_gpufreq_batt_percent_protect(unsigned int limited_index);
#endif

#if MT_GPUFREQ_LOW_BATT_VOLT_PROTECT == 1
static void __mt_gpufreq_low_batt_protect(unsigned int limited_index);
#endif

#if MT_GPUFREQ_DYNAMIC_POWER_TABLE_UPDATE == 1
static void __mt_update_gpufreqs_power_table(void);
#endif

static void __mt_gpufreq_update_max_limited_idx(void);
static unsigned int __mt_gpufreq_calculate_dds(
		unsigned int freq_khz,
		enum g_posdiv_power_enum posdiv_power);
static void __mt_gpufreq_setup_opp_power_table(int num);
static void __mt_gpufreq_cal_sb_opp_index(void);

/**
 * ===============================================
 * SECTION : Local variables definition
 * ===============================================
 */

static struct mt_gpufreq_power_table_info *g_power_table;
static struct g_opp_table_info *g_opp_table;
static struct g_opp_table_info *g_opp_table_default;
static struct g_pmic_info *g_pmic;
static struct g_clk_info *g_clk;

static unsigned int g_ptpod_opp_idx_num;
static unsigned int *g_ptpod_opp_idx_table;
static unsigned int g_ptpod_opp_idx_table_segment[] = {
	 0, 12, 15, 18,
	20, 22, 24, 26,
	28, 30, 32, 34,
	36, 38, 40, 42
};

static struct g_opp_table_info g_opp_table_segment[] = {
	GPUOP(SEG_GPU_DVFS_FREQ0,  SEG_GPU_DVFS_VOLT0,  SEG_GPU_DVFS_VSRAM0),
	GPUOP(SEG_GPU_DVFS_FREQ1,  SEG_GPU_DVFS_VOLT1,  SEG_GPU_DVFS_VSRAM1),
	GPUOP(SEG_GPU_DVFS_FREQ2,  SEG_GPU_DVFS_VOLT2,  SEG_GPU_DVFS_VSRAM2),
	GPUOP(SEG_GPU_DVFS_FREQ3,  SEG_GPU_DVFS_VOLT3,  SEG_GPU_DVFS_VSRAM3),
	GPUOP(SEG_GPU_DVFS_FREQ4,  SEG_GPU_DVFS_VOLT4,  SEG_GPU_DVFS_VSRAM4),
	GPUOP(SEG_GPU_DVFS_FREQ5,  SEG_GPU_DVFS_VOLT5,  SEG_GPU_DVFS_VSRAM5),
	GPUOP(SEG_GPU_DVFS_FREQ6,  SEG_GPU_DVFS_VOLT6,  SEG_GPU_DVFS_VSRAM6),
	GPUOP(SEG_GPU_DVFS_FREQ7,  SEG_GPU_DVFS_VOLT7,  SEG_GPU_DVFS_VSRAM7),
	GPUOP(SEG_GPU_DVFS_FREQ8,  SEG_GPU_DVFS_VOLT8,  SEG_GPU_DVFS_VSRAM8),
	GPUOP(SEG_GPU_DVFS_FREQ9,  SEG_GPU_DVFS_VOLT9,  SEG_GPU_DVFS_VSRAM9),
	GPUOP(SEG_GPU_DVFS_FREQ10, SEG_GPU_DVFS_VOLT10, SEG_GPU_DVFS_VSRAM10),
	GPUOP(SEG_GPU_DVFS_FREQ11, SEG_GPU_DVFS_VOLT11, SEG_GPU_DVFS_VSRAM11),
	GPUOP(SEG_GPU_DVFS_FREQ12, SEG_GPU_DVFS_VOLT12, SEG_GPU_DVFS_VSRAM12),
	GPUOP(SEG_GPU_DVFS_FREQ13, SEG_GPU_DVFS_VOLT13, SEG_GPU_DVFS_VSRAM13),
	GPUOP(SEG_GPU_DVFS_FREQ14, SEG_GPU_DVFS_VOLT14, SEG_GPU_DVFS_VSRAM14),
	GPUOP(SEG_GPU_DVFS_FREQ15, SEG_GPU_DVFS_VOLT15, SEG_GPU_DVFS_VSRAM15),
	GPUOP(SEG_GPU_DVFS_FREQ16, SEG_GPU_DVFS_VOLT16, SEG_GPU_DVFS_VSRAM16),
	GPUOP(SEG_GPU_DVFS_FREQ17, SEG_GPU_DVFS_VOLT17, SEG_GPU_DVFS_VSRAM17),
	GPUOP(SEG_GPU_DVFS_FREQ18, SEG_GPU_DVFS_VOLT18, SEG_GPU_DVFS_VSRAM18),
	GPUOP(SEG_GPU_DVFS_FREQ19, SEG_GPU_DVFS_VOLT19, SEG_GPU_DVFS_VSRAM19),
	GPUOP(SEG_GPU_DVFS_FREQ20, SEG_GPU_DVFS_VOLT20, SEG_GPU_DVFS_VSRAM20),
	GPUOP(SEG_GPU_DVFS_FREQ21, SEG_GPU_DVFS_VOLT21, SEG_GPU_DVFS_VSRAM20),
	GPUOP(SEG_GPU_DVFS_FREQ22, SEG_GPU_DVFS_VOLT22, SEG_GPU_DVFS_VSRAM20),
	GPUOP(SEG_GPU_DVFS_FREQ23, SEG_GPU_DVFS_VOLT23, SEG_GPU_DVFS_VSRAM20),
	GPUOP(SEG_GPU_DVFS_FREQ24, SEG_GPU_DVFS_VOLT24, SEG_GPU_DVFS_VSRAM20),
	GPUOP(SEG_GPU_DVFS_FREQ25, SEG_GPU_DVFS_VOLT25, SEG_GPU_DVFS_VSRAM20),
	GPUOP(SEG_GPU_DVFS_FREQ26, SEG_GPU_DVFS_VOLT26, SEG_GPU_DVFS_VSRAM20),
	GPUOP(SEG_GPU_DVFS_FREQ27, SEG_GPU_DVFS_VOLT27, SEG_GPU_DVFS_VSRAM20),
	GPUOP(SEG_GPU_DVFS_FREQ28, SEG_GPU_DVFS_VOLT28, SEG_GPU_DVFS_VSRAM20),
	GPUOP(SEG_GPU_DVFS_FREQ29, SEG_GPU_DVFS_VOLT29, SEG_GPU_DVFS_VSRAM20),
	GPUOP(SEG_GPU_DVFS_FREQ30, SEG_GPU_DVFS_VOLT30, SEG_GPU_DVFS_VSRAM20),
	GPUOP(SEG_GPU_DVFS_FREQ31, SEG_GPU_DVFS_VOLT31, SEG_GPU_DVFS_VSRAM20),
	GPUOP(SEG_GPU_DVFS_FREQ32, SEG_GPU_DVFS_VOLT32, SEG_GPU_DVFS_VSRAM20),
	GPUOP(SEG_GPU_DVFS_FREQ33, SEG_GPU_DVFS_VOLT33, SEG_GPU_DVFS_VSRAM20),
	GPUOP(SEG_GPU_DVFS_FREQ34, SEG_GPU_DVFS_VOLT34, SEG_GPU_DVFS_VSRAM20),
	GPUOP(SEG_GPU_DVFS_FREQ35, SEG_GPU_DVFS_VOLT35, SEG_GPU_DVFS_VSRAM20),
	GPUOP(SEG_GPU_DVFS_FREQ36, SEG_GPU_DVFS_VOLT36, SEG_GPU_DVFS_VSRAM20),
	GPUOP(SEG_GPU_DVFS_FREQ37, SEG_GPU_DVFS_VOLT37, SEG_GPU_DVFS_VSRAM20),
	GPUOP(SEG_GPU_DVFS_FREQ38, SEG_GPU_DVFS_VOLT38, SEG_GPU_DVFS_VSRAM20),
	GPUOP(SEG_GPU_DVFS_FREQ39, SEG_GPU_DVFS_VOLT39, SEG_GPU_DVFS_VSRAM20),
	GPUOP(SEG_GPU_DVFS_FREQ40, SEG_GPU_DVFS_VOLT40, SEG_GPU_DVFS_VSRAM20),
	GPUOP(SEG_GPU_DVFS_FREQ41, SEG_GPU_DVFS_VOLT41, SEG_GPU_DVFS_VSRAM20),
	GPUOP(SEG_GPU_DVFS_FREQ42, SEG_GPU_DVFS_VOLT42, SEG_GPU_DVFS_VSRAM20),
};

static const struct of_device_id g_gpufreq_of_match[] = {
	{ .compatible = "mediatek,mt6785-gpufreq" },
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
static bool g_keep_opp_freq_state;
static bool g_fixed_freq_volt_state;
static bool g_pbm_limited_ignore_state;
static bool g_thermal_protect_limited_ignore_state;
static unsigned int g_opp_stress_test_state;
static unsigned int g_efuse_id;
static unsigned int g_segment_id;
static unsigned int g_max_opp_idx_num;
static unsigned int g_segment_max_opp_idx;
static unsigned int g_segment_min_opp_idx;
static unsigned int g_enable_aging_test;
static unsigned int g_cur_opp_freq;
static unsigned int g_cur_opp_vgpu;
static unsigned int g_cur_opp_vsram_gpu;
static unsigned int g_cur_opp_idx;
static unsigned int g_keep_opp_freq_idx;
static unsigned int g_fixed_freq;
static unsigned int g_fixed_vgpu;
static unsigned int g_max_limited_idx;
static unsigned int g_pbm_limited_power;
static unsigned int g_thermal_protect_power;
static unsigned int g_vgpu_sfchg_rrate;
static unsigned int g_vgpu_sfchg_frate;
static unsigned int g_vsram_sfchg_rrate;
static unsigned int g_vsram_sfchg_frate;
static unsigned int g_DVFS_off_by_ptpod_idx;
static int g_opp_sb_idx_up[NUM_OF_OPP_IDX] = { 0 };
static int g_opp_sb_idx_down[NUM_OF_OPP_IDX] = { 0 };
#if MT_GPUFREQ_BATT_OC_PROTECT == 1
static bool g_batt_oc_limited_ignore_state;
static unsigned int g_batt_oc_level;
static unsigned int g_batt_oc_limited_idx;
static unsigned int g_batt_oc_limited_idx_lvl_0;
static unsigned int g_batt_oc_limited_idx_lvl_1;
#endif
#if MT_GPUFREQ_BATT_PERCENT_PROTECT == 1
static bool g_batt_percent_limited_ignore_state;
static unsigned int g_batt_percent_level;
static unsigned int g_batt_percent_limited_idx;
static unsigned int g_batt_percent_limited_idx_lvl_0;
static unsigned int g_batt_percent_limited_idx_lvl_1;
#endif
#if MT_GPUFREQ_LOW_BATT_VOLT_PROTECT == 1
static bool g_low_batt_limited_ignore_state;
static unsigned int g_low_battery_level;
static unsigned int g_low_batt_limited_idx;
static unsigned int g_low_batt_limited_idx_lvl_0;
static unsigned int g_low_batt_limited_idx_lvl_2;
#endif
static enum g_posdiv_power_enum g_posdiv_power;
static DEFINE_MUTEX(mt_gpufreq_lock);
static DEFINE_MUTEX(mt_gpufreq_power_lock);
static unsigned int g_limited_idx_array[NUMBER_OF_LIMITED_IDX] = { 0 };
static bool g_limited_ignore_array[NUMBER_OF_LIMITED_IDX] = { false };
static void __iomem *g_apmixed_base;

#include "ged_log.h"
#include "ged_base.h"

#if MT_GPUFREQ_GED_READY == 1
/* the debugging logs are at /d/ged/logbufs/gfreq */
extern GED_LOG_BUF_HANDLE gpufreq_ged_log;
#endif

/**
 * ===============================================
 * SECTION : API definition
 * ===============================================
 */

/*
 * API : handle frequency change request
 * @Input : is_real_idx
 *	false : pass by GED DVFS, need to map to internal real index
 *	true  : pass by GPUFREQ, already be real index
 */
unsigned int mt_gpufreq_target(unsigned int request_idx, bool is_real_idx)
{
	int i;
	unsigned int target_freq;
	unsigned int target_vgpu;
	unsigned int target_vsram_gpu;
	unsigned int target_idx;

	mutex_lock(&mt_gpufreq_lock);

	if (!is_real_idx)
		target_idx = request_idx + g_segment_max_opp_idx;
	else
		target_idx = request_idx;

	if (target_idx > (g_max_opp_idx_num - 1)) {
		gpufreq_pr_debug(
				"@%s: OPP index (%d) is out of range (skipped)\n",
				__func__,
				target_idx);
		mutex_unlock(&mt_gpufreq_lock);
		return -1;
	}

	if (target_idx > g_segment_min_opp_idx) {
		gpufreq_pr_debug(
				"@%s: OPP index (%d) is large than min_opp_idx (skipped)\n",
				__func__,
				target_idx);
		mutex_unlock(&mt_gpufreq_lock);
		return -1;
	}

	/* If /proc/gpufreq/gpufreq_fixed_freq_volt fix freq and volt */
	if (g_fixed_freq_volt_state) {
		gpufreq_pr_debug(
				"@%s: fixed_freq: %d, fixed_vgpu: %d (skipped)\n",
				__func__,
				g_fixed_freq,
				g_fixed_vgpu);
		mutex_unlock(&mt_gpufreq_lock);
		return 0;
	}

	/* look up for the target OPP table */
	target_freq = g_opp_table[target_idx].gpufreq_khz;
	target_vgpu = g_opp_table[target_idx].gpufreq_volt;
	target_vsram_gpu = g_opp_table[target_idx].gpufreq_vsram;

	gpufreq_pr_debug(
			"@%s: receive request_idx: %d, map to target_idx: %d, freq: %d, vgpu: %d, vsram_gpu: %d\n",
			__func__,
			request_idx,
			target_idx,
			target_freq,
			target_vgpu,
			target_vsram_gpu);

	/* generate random segment/real OPP index for stress test */
	if (g_opp_stress_test_state == 1) {
		get_random_bytes(&target_idx, sizeof(target_idx));
		target_idx = target_idx %
				(g_segment_min_opp_idx -
				g_segment_max_opp_idx + 1) +
				g_segment_max_opp_idx;
		gpufreq_pr_debug(
				"@%s: segment OPP stress test index: %d (from %d to %d)\n",
				__func__,
				target_idx,
				g_segment_max_opp_idx,
				g_segment_min_opp_idx);
	} else if (g_opp_stress_test_state == 2) {
		get_random_bytes(&target_idx, sizeof(target_idx));
		target_idx = target_idx % g_max_opp_idx_num;
		gpufreq_pr_debug(
				"@%s: real OPP stress test index: %d (from 0 to %d)\n",
				__func__,
				target_idx,
				g_max_opp_idx_num - 1);
	}

	/* OPP freq is limited by Thermal/Power/PBM */
	if (g_max_limited_idx != g_max_opp_idx_num) {
		if (target_freq > g_opp_table[g_max_limited_idx].gpufreq_khz) {
			gpufreq_pr_debug(
					"@%s: target_idx: %d is limited to g_max_limited_idx: %d by Thermal/Power/PBM\n",
					__func__,
					target_idx,
					g_max_limited_idx);
			target_idx = g_max_limited_idx;
		}
	}

	/* If /proc/gpufreq/gpufreq_opp_freq fix OPP freq */
	if (g_keep_opp_freq_state) {
		gpufreq_pr_debug(
				"@%s: target_idx: %d is limited to g_keep_opp_freq_idx: %d\n",
				__func__,
				target_idx,
				g_keep_opp_freq_idx);
		target_idx = g_keep_opp_freq_idx;
	}

	/* keep at max freq when PTPOD is initializing */
	if (g_DVFS_is_paused_by_ptpod) {
		gpufreq_pr_debug(
				"@%s: target_idx: %d is limited to g_DVFS_off_by_ptpod_idx: %d\n",
				__func__,
				target_idx,
				g_DVFS_off_by_ptpod_idx);
		target_idx = g_DVFS_off_by_ptpod_idx;
	}

	target_freq = g_opp_table[target_idx].gpufreq_khz;
	target_vgpu = g_opp_table[target_idx].gpufreq_volt;
	target_vsram_gpu = g_opp_table[target_idx].gpufreq_vsram;

	/* the sane freq && volt, skip it */
	if (g_cur_opp_freq == target_freq && g_cur_opp_vgpu == target_vgpu) {
		gpufreq_pr_debug(
				"@%s: Freq: %d -> %d / volt: %d ---> %d (skipped)\n",
				__func__,
				g_cur_opp_freq,
				target_freq,
				g_cur_opp_vgpu,
				target_vgpu);
		mutex_unlock(&mt_gpufreq_lock);
		return 0;
	}

	/* set to the target frequency and voltage */
	__mt_gpufreq_set(g_cur_opp_idx,
			target_idx,
			g_cur_opp_freq,
			target_freq,
			g_cur_opp_vgpu,
			target_vgpu,
			g_cur_opp_vsram_gpu,
			target_vsram_gpu);

#ifdef MT_GPUFREQ_SRAM_DEBUG
	aee_rr_rec_gpu_dvfs_oppidx(target_idx);
#endif

	g_cur_opp_idx = target_idx;

	mutex_unlock(&mt_gpufreq_lock);

	return 0;
}

/*
 * API : power control enable selections
 */
void mt_gpufreq_power_control_enable(bool cg, bool mtcmos, bool buck)
{
	mutex_lock(&mt_gpufreq_lock);

	/* check if power on BUCK */
	if (buck) {

#ifdef MT_GPUFREQ_SRAM_DEBUG
		aee_rr_rec_gpu_dvfs_status(
				0x10 | (aee_rr_curr_gpu_dvfs_status() & 0x0F));
#endif

		if (regulator_is_enabled(g_pmic->reg_vsram_gpu) == 0) {
			if (regulator_enable(g_pmic->reg_vsram_gpu)) {
				gpufreq_pr_err(
						"@%s: enable VSRAM_GPU failed\n",
						__func__);
				return;
			}
		}

#ifdef MT_GPUFREQ_SRAM_DEBUG
		aee_rr_rec_gpu_dvfs_status(
				0x20 | (aee_rr_curr_gpu_dvfs_status() & 0x0F));
#endif

		if (regulator_is_enabled(g_pmic->reg_vgpu) == 0) {
			if (regulator_enable(g_pmic->reg_vgpu)) {
				gpufreq_pr_err(
						"@%s: enable VGPU failed\n",
						__func__);
				return;
			}
		}
		g_buck_on = true;

#ifdef MT_GPUFREQ_SRAM_DEBUG
		aee_rr_rec_gpu_dvfs_status(
				0x30 | (aee_rr_curr_gpu_dvfs_status() & 0x0F));
#endif

		__mt_gpufreq_kick_pbm(1);

#ifdef MT_GPUFREQ_SRAM_DEBUG
		aee_rr_rec_gpu_dvfs_status(
				0x40 | (aee_rr_curr_gpu_dvfs_status() & 0x0F));
#endif

	}

	/* check if power on MTCMOS */
	if (mtcmos) {

#ifdef MT_GPUFREQ_SRAM_DEBUG
		aee_rr_rec_gpu_dvfs_status(
				0x50 | (aee_rr_curr_gpu_dvfs_status() & 0x0F));
#endif

		if (clk_prepare_enable(g_clk->mtcmos_mfg_async))
			gpufreq_pr_err(
					"@%s: failed when enable mtcmos_mfg_async\n",
					__func__);
		if (clk_prepare_enable(g_clk->mtcmos_mfg))
			gpufreq_pr_err(
					"@%s: failed when enable mtcmos_mfg\n",
					__func__);
		if (clk_prepare_enable(g_clk->mtcmos_mfg_core0))
			gpufreq_pr_err(
					"@%s: failed when enable mtcmos_mfg_core0\n",
					__func__);
		if (clk_prepare_enable(g_clk->mtcmos_mfg_core1))
			gpufreq_pr_err(
					"@%s: failed when enable mtcmos_mfg_core1\n",
					__func__);
		if (clk_prepare_enable(g_clk->mtcmos_mfg_core2))
			gpufreq_pr_err(
					"@%s: failed when enable mtcmos_mfg_core2\n",
					__func__);
		if (clk_prepare_enable(g_clk->mtcmos_mfg_core3))
			gpufreq_pr_err(
					"@%s: failed when enable mtcmos_mfg_core3\n",
					__func__);
		g_mtcmos_on = true;

#ifdef MT_GPUFREQ_SRAM_DEBUG
		aee_rr_rec_gpu_dvfs_status(
				0x60 | (aee_rr_curr_gpu_dvfs_status() & 0x0F));
#endif

	}

	/* check if enable SWCG(BG3D) */
	if (cg) {

#ifdef MT_GPUFREQ_SRAM_DEBUG
		aee_rr_rec_gpu_dvfs_status(
				0x70 | (aee_rr_curr_gpu_dvfs_status() & 0x0F));
#endif

		if (clk_prepare_enable(g_clk->subsys_mfg_cg))
			gpufreq_pr_err(
					"@%s: failed when enable subsys-mfg-cg\n",
					__func__);
		g_cg_on = true;

#ifdef MT_GPUFREQ_SRAM_DEBUG
		aee_rr_rec_gpu_dvfs_status(
				0x80 | (aee_rr_curr_gpu_dvfs_status() & 0x0F));
#endif

	}

	mutex_unlock(&mt_gpufreq_lock);
}

/*
 * API : power control disable selections
 */
void mt_gpufreq_power_control_disable(bool cg, bool mtcmos, bool buck)
{
	mutex_lock(&mt_gpufreq_lock);

	/* check if disable SWCG(BG3D) */
	if (cg) {

#ifdef MT_GPUFREQ_SRAM_DEBUG
		aee_rr_rec_gpu_dvfs_status(
				0x90 | (aee_rr_curr_gpu_dvfs_status() & 0x0F));
#endif

		clk_disable_unprepare(g_clk->subsys_mfg_cg);
		g_cg_on = false;

#ifdef MT_GPUFREQ_SRAM_DEBUG
		aee_rr_rec_gpu_dvfs_status(
				0xA0 | (aee_rr_curr_gpu_dvfs_status() & 0x0F));
#endif

	}

	/* Be powering off MTCMOS and BUCK when PTPOD is initializing */
	if (g_DVFS_is_paused_by_ptpod) {
		gpufreq_pr_info(
				"@%s: DVFS is paused by PTPOD\n",
				__func__);
	}

#ifdef MT_GPUFREQ_SRAM_DEBUG
		aee_rr_rec_gpu_dvfs_status(
				0xB0 | (aee_rr_curr_gpu_dvfs_status() & 0x0F));
#endif

	/* check if power off MTCMOS */
	if (mtcmos) {

#ifdef MT_GPUFREQ_SRAM_DEBUG
		aee_rr_rec_gpu_dvfs_status(
				0xC0 | (aee_rr_curr_gpu_dvfs_status() & 0x0F));
#endif

		clk_disable_unprepare(g_clk->mtcmos_mfg_core3);
		clk_disable_unprepare(g_clk->mtcmos_mfg_core2);
		clk_disable_unprepare(g_clk->mtcmos_mfg_core1);
		clk_disable_unprepare(g_clk->mtcmos_mfg_core0);
		clk_disable_unprepare(g_clk->mtcmos_mfg);
		clk_disable_unprepare(g_clk->mtcmos_mfg_async);
		g_mtcmos_on = false;

#ifdef MT_GPUFREQ_SRAM_DEBUG
		aee_rr_rec_gpu_dvfs_status(
				0xD0 | (aee_rr_curr_gpu_dvfs_status() & 0x0F));
#endif

	}

	/* check if power off BUCK */
	if (buck) {

#ifdef MT_GPUFREQ_SRAM_DEBUG
		aee_rr_rec_gpu_dvfs_status(
				0xE0 | (aee_rr_curr_gpu_dvfs_status() & 0x0F));
#endif

		if (regulator_is_enabled(g_pmic->reg_vgpu) > 0) {
			if (regulator_disable(g_pmic->reg_vgpu)) {
				gpufreq_pr_err(
						"@%s: disable VGPU failed\n",
						__func__);
				mutex_unlock(&mt_gpufreq_lock);
				return;
			}
		}

#ifdef MT_GPUFREQ_SRAM_DEBUG
		aee_rr_rec_gpu_dvfs_status(
				0xF0 | (aee_rr_curr_gpu_dvfs_status() & 0x0F));
#endif

		if (regulator_is_enabled(g_pmic->reg_vsram_gpu) > 0) {
			if (regulator_disable(g_pmic->reg_vsram_gpu)) {
				gpufreq_pr_err(
						"@%s: disable VSRAM_GPU failed\n",
						__func__);
				mutex_unlock(&mt_gpufreq_lock);
				return;
			}
		}
		g_buck_on = false;

#ifdef MT_GPUFREQ_SRAM_DEBUG
		aee_rr_rec_gpu_dvfs_status(
				0xD | (aee_rr_curr_gpu_dvfs_status() & 0xF0));
#endif

		__mt_gpufreq_kick_pbm(0);

#ifdef MT_GPUFREQ_SRAM_DEBUG
		aee_rr_rec_gpu_dvfs_status(
				0xE | (aee_rr_curr_gpu_dvfs_status() & 0xF0));
#endif

	}

	mutex_unlock(&mt_gpufreq_lock);
}

/*
 * API : enable DVFS for PTPOD initializing
 */
void mt_gpufreq_enable_by_ptpod(void)
{
	mutex_lock(&g_mfg_lock);

	/* set VGPU to leave PWM mode */
	__mt_gpufreq_vgpu_set_mode(REGULATOR_MODE_NORMAL);

	/* If SWCG(BG3D) is on, it means GPU is doing some jobs */
	if (!g_cg_on) {
		/* power off MTCMOS / BUCK */
		mt_gpufreq_power_control_disable(false, true, true);
	}

	/* enable DVFS */
	g_DVFS_is_paused_by_ptpod = false;

#if defined(CONFIG_ARM64) && \
	defined(CONFIG_BUILD_ARM64_DTB_OVERLAY_IMAGE_NAMES)
	gpufreq_pr_info("@%s: flavor name: %s\n",
			__func__,
			CONFIG_BUILD_ARM64_DTB_OVERLAY_IMAGE_NAMES);
	if ((strstr(CONFIG_BUILD_ARM64_DTB_OVERLAY_IMAGE_NAMES,
		"k85v1_64_aging") != NULL)) {
		gpufreq_pr_info(
				"@%s: AGING flavor !!!\n",
				__func__);
		g_enable_aging_test = 1;
	}
#endif

	gpufreq_pr_debug(
			"@%s: DVFS is enabled by ptpod\n",
			__func__);

	mutex_unlock(&g_mfg_lock);
}

/*
 * API : disable DVFS for PTPOD initializing
 */
void mt_gpufreq_disable_by_ptpod(void)
{
	int i = 0;
	int target_idx = g_segment_max_opp_idx;

	mutex_lock(&g_mfg_lock);

	/* disable DVFS & power control */
	g_DVFS_is_paused_by_ptpod = true;

	/* turn on BUCK / MTCMOS */
	mt_gpufreq_power_control_enable(false, true, true);

	/* fix VGPU @ 0.8V */
	for (i = 0; i < g_max_opp_idx_num; i++) {
		if (g_opp_table_default[i].gpufreq_volt <=
			GPU_DVFS_PTPOD_DISABLE_VOLT) {
			target_idx = i;
			break;
		}
	}
	g_DVFS_off_by_ptpod_idx = (unsigned int)target_idx;
	mt_gpufreq_target(target_idx, true);

	/* set VGPU to enter PWM mode */
	__mt_gpufreq_vgpu_set_mode(REGULATOR_MODE_FAST);

	gpufreq_pr_debug(
			"@%s: DVFS is disabled by ptpod, g_DVFS_off_by_ptpod_idx = %d\n",
			__func__,
			g_DVFS_off_by_ptpod_idx);

	mutex_unlock(&g_mfg_lock);
}

/*
 * API : update OPP and switch back to default voltage setting
 */
void mt_gpufreq_restore_default_volt(void)
{
	int i;

	mutex_lock(&mt_gpufreq_lock);

	gpufreq_pr_debug(
			"@%s: restore OPP table to default voltage\n",
			__func__);

	for (i = 0; i < g_max_opp_idx_num; i++) {
		g_opp_table[i].gpufreq_volt =
				g_opp_table_default[i].gpufreq_volt;
		g_opp_table[i].gpufreq_vsram =
				g_opp_table_default[i].gpufreq_vsram;
		gpufreq_pr_debug(
				"@%s: g_opp_table[%d].gpufreq_volt = %d, gpufreq_vsram = %d\n",
				__func__,
				i,
				g_opp_table[i].gpufreq_volt,
				g_opp_table[i].gpufreq_vsram);
	}

	__mt_gpufreq_cal_sb_opp_index();

	__mt_gpufreq_volt_switch_without_vsram_gpu(g_cur_opp_vgpu,
		g_opp_table[g_cur_opp_idx].gpufreq_volt);

	g_cur_opp_vgpu = g_opp_table[g_cur_opp_idx].gpufreq_volt;
	g_cur_opp_vsram_gpu = g_opp_table[g_cur_opp_idx].gpufreq_vsram;

	mutex_unlock(&mt_gpufreq_lock);
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

	gpufreq_pr_debug(
			"@%s: update OPP table to given voltage\n",
			__func__);

	for (i = 0; i < array_size; i++) {
		target_idx = mt_gpufreq_get_ori_opp_idx(i);
		if (target_idx == 12)
			continue;

		g_opp_table[target_idx].gpufreq_volt = pmic_volt[i];
		g_opp_table[target_idx].gpufreq_vsram =
		__mt_gpufreq_get_vsram_gpu_by_vgpu(pmic_volt[i]);

		if (i < array_size - 1) {
			/* interpolation for opps not for ptpod */
			int larger = pmic_volt[i];
			int smaller = pmic_volt[i + 1];
			if (target_idx == 0) {
				/* At opp 0, 14 opps need intepolation */
				smaller = pmic_volt[2];
				__mt_gpufreq_update_intepolating_volt(
				target_idx, larger, smaller, 15);
			} else if (target_idx == 15) {
				/* At opp 15, 2 opps need intepolation */
				__mt_gpufreq_update_intepolating_volt(
				target_idx, larger, smaller, 3);
			} else {
				__mt_gpufreq_update_intepolating_volt(
				target_idx, larger, smaller, 2);
			}
		}
	}

	if (g_enable_aging_test)
		__mt_gpufreq_update_aging(true);

	__mt_gpufreq_cal_sb_opp_index();

	/* update volt if powered */
	if (g_buck_on) {
		__mt_gpufreq_volt_switch_without_vsram_gpu(
				g_cur_opp_vgpu,
				g_opp_table[g_cur_opp_idx].gpufreq_volt);
		g_cur_opp_vgpu = g_opp_table[g_cur_opp_idx].gpufreq_volt;
		g_cur_opp_vsram_gpu = g_opp_table[g_cur_opp_idx].gpufreq_vsram;
	}

	mutex_unlock(&mt_gpufreq_lock);

	return 0;
}

/* API : get OPP table index number */
/* need to sub g_segment_max_opp_idx to map to real idx */
unsigned int mt_gpufreq_get_dvfs_table_num(void)
{
	return g_segment_min_opp_idx - g_segment_max_opp_idx + 1;
}

/* API : get real OPP table index number */
unsigned int mt_gpufreq_get_real_dvfs_table_num(void)
{
	return g_max_opp_idx_num;
}

/* API : get frequency via OPP table index */
/* need to add g_segment_max_opp_idx to map to real idx */
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

/* API : get voltage via OPP table index */
unsigned int mt_gpufreq_get_volt_by_idx(unsigned int idx)
{
	idx += g_segment_max_opp_idx;
	if (idx < g_max_opp_idx_num)
		return g_opp_table[idx].gpufreq_volt;
	else
		return 0;
}

/* API : get voltage via OPP table real index */
unsigned int mt_gpufreq_get_volt_by_real_idx(unsigned int idx)
{
	if (idx < g_max_opp_idx_num)
		return g_opp_table[idx].gpufreq_volt;
	else
		return 0;
}

/* API: get opp idx in original opp tables */
/* This is usually for ptpod use */
unsigned int mt_gpufreq_get_ori_opp_idx(unsigned int idx)
{
	if (idx < g_ptpod_opp_idx_num && idx >= 0)
		return g_ptpod_opp_idx_table[idx];
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

/* API : get static leakage power */
unsigned int mt_gpufreq_get_leakage_mw(void)
{
	int temp = 0;
#if MT_GPUFREQ_STATIC_PWR_READY2USE == 1
	unsigned int cur_vcore = __mt_gpufreq_get_cur_vgpu() / 100;
	int leak_power;
#endif

#ifdef CONFIG_THERMAL
	temp = get_immediate_gpu_wrap() / 1000;
#else
	temp = 40;
#endif

#if MT_GPUFREQ_STATIC_PWR_READY2USE == 1
	leak_power = mt_spower_get_leakage(MTK_SPOWER_GPU, cur_vcore, temp);
	if (g_buck_on && leak_power > 0)
		return leak_power;
	else
		return 0;
#else
	return 130;
#endif
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
	return g_max_limited_idx - g_segment_max_opp_idx;
}

/*
 * API : get current Thermal/Power/PBM limited OPP table frequency
 */
unsigned int mt_gpufreq_get_thermal_limit_freq(void)
{
	return g_opp_table[g_max_limited_idx].gpufreq_khz;
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

#if MT_GPUFREQ_BATT_OC_PROTECT == 1
/*
 * API : Over Currents(OC) Callback
 */
void mt_gpufreq_batt_oc_callback(BATTERY_OC_LEVEL battery_oc_level)
{
	if (g_batt_oc_limited_ignore_state) {
		gpufreq_pr_debug(
				"@%s: ignore Over Currents(OC) protection\n",
				__func__);
		return;
	}

	gpufreq_pr_debug(
			"@%s: battery_oc_level = %d\n",
			__func__,
			battery_oc_level);

	g_batt_oc_level = battery_oc_level;

	if (battery_oc_level == BATTERY_OC_LEVEL_1) {
		if (g_batt_oc_limited_idx !=
			g_batt_oc_limited_idx_lvl_1) {
			g_batt_oc_limited_idx =
					g_batt_oc_limited_idx_lvl_1;
			__mt_gpufreq_batt_oc_protect(
					g_batt_oc_limited_idx_lvl_1);
		}
	} else {
		if (g_batt_oc_limited_idx !=
			g_batt_oc_limited_idx_lvl_0) {
			g_batt_oc_limited_idx =
					g_batt_oc_limited_idx_lvl_0;
			__mt_gpufreq_batt_oc_protect(
					g_batt_oc_limited_idx_lvl_0);
		}
	}
}
#endif

#if MT_GPUFREQ_BATT_PERCENT_PROTECT == 1
/*
 * API : Battery Percentage Callback
 */
void mt_gpufreq_batt_percent_callback(
		BATTERY_PERCENT_LEVEL battery_percent_level)
{
	if (g_batt_percent_limited_ignore_state) {
		gpufreq_pr_debug(
				"@%s: ignore Battery Percentage protection\n",
				__func__);
		return;
	}

	gpufreq_pr_debug(
			"@%s: battery_percent_level = %d\n",
			__func__,
			battery_percent_level);

	g_batt_percent_level = battery_percent_level;

	/* BATTERY_PERCENT_LEVEL_1: <= 15%, BATTERY_PERCENT_LEVEL_0: >15% */
	if (battery_percent_level == BATTERY_PERCENT_LEVEL_1) {
		if (g_batt_percent_limited_idx !=
			g_batt_percent_limited_idx_lvl_1) {
			g_batt_percent_limited_idx =
					g_batt_percent_limited_idx_lvl_1;
			__mt_gpufreq_batt_percent_protect(
					g_batt_percent_limited_idx_lvl_1);
		}
	} else {
		if (g_batt_percent_limited_idx !=
			g_batt_percent_limited_idx_lvl_0) {
			g_batt_percent_limited_idx =
					g_batt_percent_limited_idx_lvl_0;
			__mt_gpufreq_batt_percent_protect(
					g_batt_percent_limited_idx_lvl_0);
		}
	}
}
#endif

#if MT_GPUFREQ_LOW_BATT_VOLT_PROTECT == 1
/*
 * API : Low Battery Volume Callback
 */
void mt_gpufreq_low_batt_callback(LOW_BATTERY_LEVEL low_battery_level)
{
	if (g_low_batt_limited_ignore_state) {
		gpufreq_pr_debug(
				"@%s: ignore Low Battery Volume protection\n",
				__func__);
		return;
	}

	gpufreq_pr_debug(
			"@%s: low_battery_level = %d\n",
			__func__,
			low_battery_level);

	g_low_battery_level = low_battery_level;

	/*
	 * 3.25V HW issue int and is_low_battery = 1
	 * 3.10V HW issue int and is_low_battery = 2
	 */
	if (low_battery_level == LOW_BATTERY_LEVEL_2) {
		if (g_low_batt_limited_idx !=
			g_low_batt_limited_idx_lvl_2) {
			g_low_batt_limited_idx = g_low_batt_limited_idx_lvl_2;
			__mt_gpufreq_low_batt_protect(
					g_low_batt_limited_idx_lvl_2);
		}
	} else {
		if (g_low_batt_limited_idx !=
			g_low_batt_limited_idx_lvl_0) {
			g_low_batt_limited_idx = g_low_batt_limited_idx_lvl_0;
			__mt_gpufreq_low_batt_protect(
					g_low_batt_limited_idx_lvl_0);
		}
	}
}
#endif

/*
 * API : set limited OPP table index for Thermal protection
 */
void mt_gpufreq_thermal_protect(unsigned int limited_power)
{
	int i = -1;
	unsigned int limited_freq;

	mutex_lock(&mt_gpufreq_power_lock);

	if (g_thermal_protect_limited_ignore_state) {
		gpufreq_pr_debug(
				"@%s: ignore Thermal protection\n",
				__func__);
		mutex_unlock(&mt_gpufreq_power_lock);
		return;
	}

	if (limited_power == g_thermal_protect_power) {
		gpufreq_pr_debug(
				"@%s: limited_power(%d mW) not changed, skip it\n",
				__func__,
				limited_power);
		mutex_unlock(&mt_gpufreq_power_lock);
		return;
	}

	g_thermal_protect_power = limited_power;

#if MT_GPUFREQ_DYNAMIC_POWER_TABLE_UPDATE == 1
	__mt_update_gpufreqs_power_table();
#endif

	if (limited_power == 0) {
		g_limited_idx_array[IDX_THERMAL_PROTECT_LIMITED] =
				g_segment_max_opp_idx;
		__mt_gpufreq_update_max_limited_idx();
	} else {
		limited_freq =
		__mt_gpufreq_get_limited_freq_by_power(limited_power);
		for (i = g_segment_max_opp_idx;
				i <= g_segment_min_opp_idx; i++) {
			if (g_opp_table[i].gpufreq_khz <= limited_freq) {
				g_limited_idx_array[
				IDX_THERMAL_PROTECT_LIMITED] = i;
				__mt_gpufreq_update_max_limited_idx();
				if (g_cur_opp_freq > g_opp_table[i].gpufreq_khz)
					mt_gpufreq_target(i, true);
				break;
			}
		}
	}

	gpufreq_pr_debug(
			"@%s: limited power index = %d, limited power = %d\n",
			__func__,
			i,
			limited_power);

	mutex_unlock(&mt_gpufreq_power_lock);
}

/* API : set limited OPP table index by PBM */
void mt_gpufreq_set_power_limit_by_pbm(unsigned int limited_power)
{
	int i = -1;
	unsigned int limited_freq;

	mutex_lock(&mt_gpufreq_power_lock);

	if (g_pbm_limited_ignore_state) {
		gpufreq_pr_debug(
				"@%s: ignore PBM Power limited\n",
				__func__);
		mutex_unlock(&mt_gpufreq_power_lock);
		return;
	}

	if (limited_power == g_pbm_limited_power) {
		gpufreq_pr_debug(
				"@%s: limited_power(%d mW) not changed, skip it\n",
				__func__,
				limited_power);
		mutex_unlock(&mt_gpufreq_power_lock);
		return;
	}

	g_pbm_limited_power = limited_power;

	if (limited_power == 0) {
		g_limited_idx_array[IDX_PBM_LIMITED] = g_segment_max_opp_idx;
		__mt_gpufreq_update_max_limited_idx();
	} else {
		limited_freq =
				__mt_gpufreq_get_limited_freq_by_power(
				limited_power);
		for (i = g_segment_max_opp_idx;
				i <= g_segment_min_opp_idx; i++) {
			if (g_opp_table[i].gpufreq_khz <= limited_freq) {
				g_limited_idx_array[IDX_PBM_LIMITED] = i;
				__mt_gpufreq_update_max_limited_idx();
				break;
			}
		}
	}

	gpufreq_pr_debug(
			"@%s: limited power index = %d, limited_power = %d\n",
			__func__,
			i,
			limited_power);

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

/**
 * ===============================================
 * SECTION : PROCFS interface for debugging
 * ===============================================
 */

#ifdef CONFIG_PROC_FS
/*
 * PROCFS : show OPP table
 */
static int mt_gpufreq_opp_dump_proc_show(struct seq_file *m, void *v)
{
	int i;

	for (i = g_segment_max_opp_idx; i <= g_segment_min_opp_idx; i++) {
		seq_printf(m,
				"[%02d] ",
				i - g_segment_max_opp_idx);
		seq_printf(m,
				"freq = %d, ",
				g_opp_table[i].gpufreq_khz);
		seq_printf(m,
				"volt = %d, ",
				g_opp_table[i].gpufreq_volt);
		seq_printf(m,
				"vsram = %d, ",
				g_opp_table[i].gpufreq_vsram);
		seq_printf(m,
				"gpufreq_power = %d\n",
				g_power_table[i].gpufreq_power);
	}

	return 0;
}

/*
 * PROCFS : show springboard table
 */
static int mt_gpufreq_sb_idx_proc_show(struct seq_file *m, void *v)
{
	int i, max_opp_idx;

	max_opp_idx = g_segment_max_opp_idx;

	for (i = max_opp_idx; i <= g_segment_min_opp_idx; i++) {
		seq_printf(m,
				"[%02d] ", i - max_opp_idx);
		seq_printf(m,
				"g_opp_sb_idx_up = %d, ",
				g_opp_sb_idx_up[i] - max_opp_idx >= 0 ?
				g_opp_sb_idx_up[i] - max_opp_idx : 0);
		seq_printf(m,
				"g_opp_sb_idx_down = %d\n",
				g_opp_sb_idx_down[i] - max_opp_idx >= 0 ?
				g_opp_sb_idx_down[i] - max_opp_idx : 0);
	}

	return 0;
}

/*
 * PROCFS : show important variables for debugging
 */
static int mt_gpufreq_var_dump_proc_show(struct seq_file *m, void *v)
{
	int i;
	unsigned int gpu_loading = 0;

	mtk_get_gpu_loading(&gpu_loading);

	seq_printf(m,
			"g_cur_opp_idx = %d, g_cur_opp_freq = %d, g_cur_opp_vgpu = %d, g_cur_opp_vsram_gpu = %d\n",
			g_cur_opp_idx - g_segment_max_opp_idx,
			g_cur_opp_freq,
			g_cur_opp_vgpu,
			g_cur_opp_vsram_gpu);
	seq_printf(m,
			"real clock freq = %d, MFGPLL freq = %d, real vgpu = %d, real vsram_gpu = %d\n",
			mt_get_ckgen_freq(15),
			__mt_gpufreq_get_cur_freq(),
			__mt_gpufreq_get_cur_vgpu(),
			__mt_gpufreq_get_cur_vsram_gpu());
	seq_printf(m,
			"g_segment_id = %d\n",
			g_segment_id);
	seq_printf(m,
			"g_cg_on = %d, g_mtcmos_on = %d, g_buck_on = %d\n",
			g_cg_on,
			g_mtcmos_on,
			g_buck_on);
	seq_printf(m,
			"g_opp_stress_test_state = %d\n",
			g_opp_stress_test_state);
	seq_printf(m,
			"g_max_limited_idx = %d\n",
			g_max_limited_idx - g_segment_max_opp_idx);
	seq_printf(m,
			"gpu_loading = %d\n",
			gpu_loading);

	for (i = 0; i < NUMBER_OF_LIMITED_IDX; i++)
		seq_printf(m,
				"g_limited_idx_array[%d] = %d\n",
				i,
				g_limited_idx_array[i] - g_segment_max_opp_idx);

	return 0;
}

/*
 * PROCFS : show current opp stress test state
 */
static int mt_gpufreq_opp_stress_test_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m,
			"g_opp_stress_test_state = %d\n",
			g_opp_stress_test_state);
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
		if (value >= 0 && value <= 2) {
			ret = 0;
			g_opp_stress_test_state = value;
		}
	}

out:
	return (ret < 0) ? ret : count;
}

/*
 * PROCFS : show current aging test state
 */
static int mt_gpufreq_aging_test_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m,
			"g_enable_aging_test = %d\n",
			g_enable_aging_test);
	return 0;
}

/*
 * PROCFS : aging test setting
 * 0 : disable
 * 1 : enable
 */
static ssize_t mt_gpufreq_aging_test_proc_write(
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
			if (g_enable_aging_test ^ value) {
				if (value) {
					g_enable_aging_test = 1;
					__mt_gpufreq_update_aging(true);
				} else {
					g_enable_aging_test = 0;
					__mt_gpufreq_update_aging(false);
				}
				__mt_gpufreq_cal_sb_opp_index();
			}
		}
	}
	mutex_unlock(&mt_gpufreq_lock);

out:
	return (ret < 0) ? ret : count;
}

/*
 * PROCFS : show Thermal/Power/PBM limited ignore state
 * 0 : consider
 * 1 : ignore
 */
static int mt_gpufreq_power_limited_proc_show(struct seq_file *m, void *v)
{
	seq_puts(m,
			"GPU-DVFS power limited state ....\n");
#if MT_GPUFREQ_BATT_OC_PROTECT == 1
	seq_printf(m,
			"g_batt_oc_limited_ignore_state = %d\n",
			g_batt_oc_limited_ignore_state);
#endif
#if MT_GPUFREQ_BATT_PERCENT_PROTECT == 1
	seq_printf(m,
			"g_batt_percent_limited_ignore_state = %d\n",
			g_batt_percent_limited_ignore_state);
#endif
#if MT_GPUFREQ_LOW_BATT_VOLT_PROTECT == 1
	seq_printf(m,
			"g_low_batt_limited_ignore_state = %d\n",
			g_low_batt_limited_ignore_state);
#endif
	seq_printf(m,
			"g_thermal_protect_limited_ignore_state = %d\n",
			g_thermal_protect_limited_ignore_state);
	seq_printf(m,
			"g_pbm_limited_ignore_state = %d\n",
			g_pbm_limited_ignore_state);
	return 0;
}

/*
 * PROCFS : ignore state or power value setting for Thermal/Power/PBM limit
 */
static ssize_t mt_gpufreq_power_limited_proc_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *data)
{
	char buf[64];
	unsigned int len = 0;
	int ret = -EFAULT;
	unsigned int i;
	unsigned int size;
	unsigned int value = 0;
	static const char * const array[] = {
#if MT_GPUFREQ_BATT_OC_PROTECT == 1
		"ignore_batt_oc",
#endif
#if MT_GPUFREQ_BATT_PERCENT_PROTECT == 1
		"ignore_batt_percent",
#endif
#if MT_GPUFREQ_LOW_BATT_VOLT_PROTECT == 1
		"ignore_low_batt",
#endif
		"ignore_thermal_protect",
		"ignore_pbm_limited",
		"pbm_limited_power",
		"thermal_protect_power",
	};

	len = (count < (sizeof(buf) - 1)) ? count : (sizeof(buf) - 1);

	if (copy_from_user(buf, buffer, len))
		goto out;

	buf[len] = '\0';

	size = ARRAY_SIZE(array);

	for (i = 0; i < size; i++) {
		if (strncmp(array[i], buf,
			MIN(strlen(array[i]), count)) == 0) {
			char cond_buf[64];

			snprintf(cond_buf,
					sizeof(cond_buf),
					"%s %%u",
					array[i]);
if (sscanf(buf, cond_buf, &value) == 1) {
	ret = 0;
	if (strncmp(array[i],
		"pbm_limited_power",
		strlen(array[i])) == 0)
		mt_gpufreq_set_power_limit_by_pbm(value);
	else if (strncmp(array[i],
		"thermal_protect_power",
		strlen(array[i])) == 0)
		mt_gpufreq_thermal_protect(value);
#if MT_GPUFREQ_BATT_OC_PROTECT == 1
	else if (strncmp(array[i],
		"ignore_batt_oc",
		strlen(array[i])) == 0) {
		if (!value || !(value-1)) {
			g_batt_oc_limited_ignore_state =
			(value) ? true : false;
			g_limited_ignore_array[
			IDX_BATT_OC_LIMITED] =
			g_batt_oc_limited_ignore_state;
		}
	}
#endif
#if MT_GPUFREQ_BATT_PERCENT_PROTECT == 1
	else if (strncmp(array[i],
		"ignore_batt_percent",
		strlen(array[i])) == 0) {
		if (!value || !(value-1)) {
			g_batt_percent_limited_ignore_state =
			(value) ? true : false;
			g_limited_ignore_array[
			IDX_BATT_PERCENT_LIMITED] =
			g_batt_percent_limited_ignore_state;
		}
	}
#endif
#if MT_GPUFREQ_LOW_BATT_VOLT_PROTECT == 1
	else if (strncmp(array[i],
		"ignore_low_batt",
		strlen(array[i])) == 0) {
		if (!value || !(value-1)) {
			g_low_batt_limited_ignore_state =
			(value) ? true : false;
			g_limited_ignore_array[
			IDX_LOW_BATT_LIMITED] =
			g_low_batt_limited_ignore_state;
		}
	}
#endif
	else if (strncmp(array[i],
		"ignore_thermal_protect",
		strlen(array[i])) == 0) {
		if (!value || !(value-1)) {
			g_thermal_protect_limited_ignore_state =
			(value) ? true : false;
			g_limited_ignore_array[
					IDX_THERMAL_PROTECT_LIMITED] =
					g_thermal_protect_limited_ignore_state;
		}
	} else if (strncmp(array[i],
		"ignore_pbm_limited",
		strlen(array[i])) == 0) {
		if (!value || !(value-1)) {
			g_pbm_limited_ignore_state =
			(value) ? true : false;
			g_limited_ignore_array[
			IDX_PBM_LIMITED] =
			g_pbm_limited_ignore_state;
		}
	}
	break;
}
		}
	}

out:
	return (ret < 0) ? ret : count;
}

/*
 * PROCFS : show current keeping OPP frequency state
 */
static int mt_gpufreq_opp_freq_proc_show(struct seq_file *m, void *v)
{
	if (g_keep_opp_freq_state) {
		seq_puts(m,
				"[GPU-DVFS] fixed OPP is enabled\n");
		seq_printf(m,
				"[%d] ",
				g_keep_opp_freq_idx - g_segment_max_opp_idx);
		seq_printf(m,
				"freq = %d, ",
				g_opp_table[g_keep_opp_freq_idx].gpufreq_khz);
		seq_printf(m,
				"volt = %d, ",
				g_opp_table[g_keep_opp_freq_idx].gpufreq_volt);
		seq_printf(m,
				"vsram = %d\n",
				g_opp_table[g_keep_opp_freq_idx].gpufreq_vsram);
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
			g_keep_opp_freq_state = false;
		} else {
			for (i = g_segment_max_opp_idx;
				i <= g_segment_min_opp_idx;
				i++) {
				if (value == g_opp_table[i].gpufreq_khz) {
					ret = 0;
					g_keep_opp_freq_idx = i;
					g_keep_opp_freq_state = true;
					mt_gpufreq_target(i, true);
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
		seq_puts(m,
				"[GPU-DVFS] fixed freq & volt is enabled\n");
		seq_printf(m,
				"g_fixed_freq = %d\n", g_fixed_freq);
		seq_printf(m,
				"g_fixed_vgpu = %d\n", g_fixed_vgpu);
	} else
		seq_puts(m,
			"[GPU-DVFS] fixed freq & volt is disabled\n");

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
			gpufreq_pr_debug(
					"@%s: fixed volt(%d) is greater than VGPU_MAX_VOLT(%d)\n",
					__func__,
					fixed_volt,
					VGPU_MAX_VOLT);
			goto out;
		} else if (fixed_volt < VGPU_MIN_VOLT && fixed_volt > 0) {
			gpufreq_pr_debug(
					"@%s: fixed volt(%d) is lesser than VGPU_MIN_VOLT(%d)\n",
					__func__,
					fixed_volt,
					VGPU_MIN_VOLT);
			goto out;
		} else if (fixed_freq > POSDIV_4_MAX_FREQ) {
			gpufreq_pr_debug(
					"@%s: fixed freq(%d) is greater than POSDIV_4_MAX_FREQ(%d)\n",
					__func__,
					fixed_freq,
					POSDIV_4_MAX_FREQ);
			goto out;
		} else if (fixed_freq < POSDIV_8_MIN_FREQ && fixed_freq > 0) {
			gpufreq_pr_debug(
					"@%s: fixed freq(%d) is lesser than POSDIV_8_MIN_FREQ(%d)\n",
					__func__,
					fixed_freq,
					POSDIV_8_MIN_FREQ);
			goto out;
		}

		mutex_lock(&mt_gpufreq_lock);
		if ((fixed_freq == 0) && (fixed_volt == 0)) {
			fixed_freq =
				g_opp_table[g_segment_min_opp_idx].gpufreq_khz;
			fixed_volt =
				g_opp_table[g_segment_min_opp_idx].gpufreq_volt;
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
PROC_FOPS_RW(gpufreq_power_limited);
PROC_FOPS_RO(gpufreq_opp_dump);
PROC_FOPS_RW(gpufreq_opp_freq);
PROC_FOPS_RO(gpufreq_var_dump);
PROC_FOPS_RW(gpufreq_fixed_freq_volt);
PROC_FOPS_RO(gpufreq_sb_idx);
PROC_FOPS_RW(gpufreq_aging_test);

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
		PROC_ENTRY(gpufreq_power_limited),
		PROC_ENTRY(gpufreq_opp_dump),
		PROC_ENTRY(gpufreq_opp_freq),
		PROC_ENTRY(gpufreq_var_dump),
		PROC_ENTRY(gpufreq_fixed_freq_volt),
		PROC_ENTRY(gpufreq_sb_idx),
		PROC_ENTRY(gpufreq_aging_test),
	};

	dir = proc_mkdir("gpufreq", NULL);
	if (!dir) {
		gpufreq_pr_err(
				"@%s: fail to create /proc/gpufreq\n",
				__func__);
		return -ENOMEM;
	}

	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!proc_create(
				entries[i].name,
				0664,
				dir,
				entries[i].fops))
			gpufreq_pr_err(
					"@%s: create /proc/gpufreq/%s failed\n",
					__func__,
					entries[i].name);
	}

	return 0;
}
#endif

/**
 * ===============================================
 * SECTION : Local functions definition
 * ===============================================
 */

/*
 * Update aging margin setting
 */
void __mt_gpufreq_update_aging(bool apply_aging_setting)
{
	int i;

	if (apply_aging_setting) {
		for (i = 0; i < g_max_opp_idx_num; i++) {
			if (i >= 0 && i <= 23)
				g_opp_table[i].gpufreq_volt -= 1875;
			else if (i >= 24 && i <= 33)
				g_opp_table[i].gpufreq_volt -= 1250;
			else if (i >= 34 && i <= 42)
				g_opp_table[i].gpufreq_volt -= 625;

			g_opp_table[i].gpufreq_vsram =
			__mt_gpufreq_get_vsram_gpu_by_vgpu(
			g_opp_table[i].gpufreq_volt);

			gpufreq_pr_debug(
				"@%s: update aging setting %d: g_opp_table[%d].gpufreq_volt = %d, gpufreq_vsram = %d\n",
				__func__,
				apply_aging_setting,
				i,
				g_opp_table[i].gpufreq_volt,
				g_opp_table[i].gpufreq_vsram);
		}
	} else {
		for (i = 0; i < g_max_opp_idx_num; i++) {
			if (i >= 0 && i <= 23)
				g_opp_table[i].gpufreq_volt += 1875;
			else if (i >= 24 && i <= 33)
				g_opp_table[i].gpufreq_volt += 1250;
			else if (i >= 34 && i <= 42)
				g_opp_table[i].gpufreq_volt += 625;

			g_opp_table[i].gpufreq_vsram =
			__mt_gpufreq_get_vsram_gpu_by_vgpu(
			g_opp_table[i].gpufreq_volt);

			gpufreq_pr_debug(
				"@%s: update aging setting %d: g_opp_table[%d].gpufreq_volt = %d, gpufreq_vsram = %d\n",
				__func__,
				apply_aging_setting,
				i,
				g_opp_table[i].gpufreq_volt,
				g_opp_table[i].gpufreq_vsram);
		}
	}
}

/*
 * calculate springboard opp index to avoid buck variation,
 * the voltage between VGPU and VSRAM_GPU must be in 0mV ~ 350mV
 * that is, 0mV <= VSRAM_GPU - VGPU <= 350mV
 * (variation: VGPU / VSRAM_GPU {-6.25% / max(+8%, +53mV)}
 */
static void __mt_gpufreq_cal_sb_opp_index(void)
{
	int i, j, diff;
	int min_vsram_idx = g_max_opp_idx_num - 1;

	/* find 0.850(V) index */
	for (i = 0; i < g_max_opp_idx_num; i++) {
		if (g_opp_table[i].gpufreq_vsram == SEG_GPU_DVFS_VSRAM20) {
			min_vsram_idx = i;
			break;
		}
	}
	gpufreq_pr_debug(
			"@%s: min_vsram_idx: %d\n",
			__func__,
			min_vsram_idx);

	/* build up table */
	for (i = 0; i < g_max_opp_idx_num; i++) {
		g_opp_sb_idx_up[i] = min_vsram_idx;
		for (j = 0; j <= min_vsram_idx; j++) {
			diff = g_opp_table[i].gpufreq_volt + BUCK_DIFF_MAX;
			if (g_opp_table[j].gpufreq_vsram <= diff) {
				g_opp_sb_idx_up[i] = j;
				break;
			}
		}
		gpufreq_pr_debug(
				"@%s: g_opp_sb_idx_up[%d]: %d\n",
				__func__,
				i,
				g_opp_sb_idx_up[i]);
	}

	/* build down table */
	for (i = 0; i < g_max_opp_idx_num; i++) {
		if (i >= min_vsram_idx)
			g_opp_sb_idx_down[i] = g_max_opp_idx_num - 1;
		else {
			for (j = g_max_opp_idx_num - 1; j >= 0; j--) {
				diff =
				g_opp_table[i].gpufreq_vsram - BUCK_DIFF_MAX;
				if (g_opp_table[j].gpufreq_volt >= diff) {
					g_opp_sb_idx_down[i] = j;
					break;
				}
			}
		}
		gpufreq_pr_debug(
				"@%s: g_opp_sb_idx_down[%d]: %d\n",
				__func__,
				i,
				g_opp_sb_idx_down[i]);
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
	unsigned int tmp_idx = idx_old;
	unsigned int tmp_vgpu = vgpu_old;
	unsigned int tmp_vsram_gpu = vsram_gpu_old;

	gpufreq_pr_debug(
			"@%s: begin idx: %d -> %d, freq: %d -> %d, vgpu: %d -> %d, vsram_gpu: %d -> %d\n",
			__func__,
			idx_old,
			idx_new,
			freq_old,
			freq_new,
			vgpu_old,
			vgpu_new,
			vsram_gpu_old,
			vsram_gpu_new);

#if MT_GPUFREQ_GED_READY == 1
	ged_log_buf_print2(gpufreq_ged_log, GED_LOG_ATTR_TIME,
			"begin idx: %d ---> %d, freq: %d ---> %d, vgpu: %d ---> %d, vsram_gpu: %d ---> %d\n",
			idx_old,
			idx_new,
			freq_old,
			freq_new,
			vgpu_old,
			vgpu_new,
			vsram_gpu_old,
			vsram_gpu_new);
#endif

	if (freq_new == freq_old) {
		__mt_gpufreq_volt_switch(
				vgpu_old,
				vgpu_new,
				vsram_gpu_old,
				vsram_gpu_new);

		gpufreq_pr_debug(
				"@%s: vgpu: %d ---> %d, vsram_gpu: %d ---> %d\n",
				__func__,
				vgpu_old,
				vgpu_new,
				vsram_gpu_old,
				vsram_gpu_new);
	} else if (freq_new > freq_old) {
		while (tmp_idx != idx_new) {
			sb_idx = g_opp_sb_idx_up[tmp_idx] < idx_new ?
					idx_new :
					g_opp_sb_idx_up[tmp_idx];

			__mt_gpufreq_volt_switch(
					tmp_vgpu,
					g_opp_table[sb_idx].gpufreq_volt,
					tmp_vsram_gpu,
					g_opp_table[sb_idx].gpufreq_vsram);

			gpufreq_pr_debug(
					"@%s: volt_switch idx: %d -> %d, vgpu: %d -> %d, vsram_gpu: %d -> %d\n",
					__func__,
					tmp_idx,
					sb_idx,
					tmp_vgpu,
					g_opp_table[sb_idx].gpufreq_volt,
					tmp_vsram_gpu,
					g_opp_table[sb_idx].gpufreq_vsram);

#if MT_GPUFREQ_GED_READY == 1
			ged_log_buf_print2(gpufreq_ged_log, GED_LOG_ATTR_TIME,
					"volt_switch idx: %d -> %d, vgpu: %d -> %d, vsram_gpu: %d -> %d\n",
					tmp_idx,
					sb_idx,
					tmp_vgpu,
					g_opp_table[sb_idx].gpufreq_volt,
					tmp_vsram_gpu,
					g_opp_table[sb_idx].gpufreq_vsram);
#endif

			tmp_idx = sb_idx;
			tmp_vgpu = g_opp_table[sb_idx].gpufreq_volt;
			tmp_vsram_gpu = g_opp_table[sb_idx].gpufreq_vsram;
		}
		__mt_gpufreq_clock_switch(freq_new);
	} else {
		__mt_gpufreq_clock_switch(freq_new);
		while (tmp_idx != idx_new) {
			sb_idx = g_opp_sb_idx_down[tmp_idx] > idx_new ?
					idx_new :
					g_opp_sb_idx_down[tmp_idx];

			__mt_gpufreq_volt_switch(
					tmp_vgpu,
					g_opp_table[sb_idx].gpufreq_volt,
					tmp_vsram_gpu,
					g_opp_table[sb_idx].gpufreq_vsram);

			gpufreq_pr_debug(
					"@%s: volt_switch idx: %d -> %d, vgpu: %d -> %d, vsram_gpu: %d -> %d\n",
					__func__,
					tmp_idx,
					sb_idx,
					tmp_vgpu,
					g_opp_table[sb_idx].gpufreq_volt,
					tmp_vsram_gpu,
					g_opp_table[sb_idx].gpufreq_vsram);

#if MT_GPUFREQ_GED_READY == 1
			ged_log_buf_print2(
					gpufreq_ged_log,
					GED_LOG_ATTR_TIME,
					"volt_switch idx: %d -> %d, vgpu: %d -> %d, vsram_gpu: %d -> %d\n",
					tmp_idx,
					sb_idx,
					tmp_vgpu,
					g_opp_table[sb_idx].gpufreq_volt,
					tmp_vsram_gpu,
					g_opp_table[sb_idx].gpufreq_vsram);
#endif

			tmp_idx = sb_idx;
			tmp_vgpu = g_opp_table[sb_idx].gpufreq_volt;
			tmp_vsram_gpu = g_opp_table[sb_idx].gpufreq_vsram;
		}
	}

	g_cur_opp_freq = freq_new;
	g_cur_opp_vgpu = vgpu_new;
	g_cur_opp_vsram_gpu = vsram_gpu_new;

	gpufreq_pr_debug(
			"@%s: done idx: %d -> %d, freq: %d -> %d(%d), vgpu: %d -> %d(%d), vsram_gpu: %d -> %d(%d)\n",
			__func__,
			idx_old,
			idx_new,
			freq_old,
			freq_new,
			mt_get_ckgen_freq(15),
			vgpu_old,
			vgpu_new,
			__mt_gpufreq_get_cur_vgpu(),
			vsram_gpu_old,
			vsram_gpu_new,
			__mt_gpufreq_get_cur_vsram_gpu());

#if MT_GPUFREQ_GED_READY == 1
	ged_log_buf_print2(
			gpufreq_ged_log,
			GED_LOG_ATTR_TIME,
			"done idx: %d -> %d, freq: %d -> %d(%d) (MFGPLL_CON1 = 0x%x), vgpu: %d -> %d(%d), vsram_gpu: %d -> %d(%d)\n\n",
			idx_old,
			idx_new,
			freq_old,
			freq_new,
			mt_get_ckgen_freq(15),
			DRV_Reg32(MFGPLL_CON1),
			vgpu_old,
			vgpu_new,
			__mt_gpufreq_get_cur_vgpu(),
			vsram_gpu_old,
			vsram_gpu_new,
			__mt_gpufreq_get_cur_vsram_gpu());
#endif

	__mt_gpufreq_kick_pbm(1);
}

/*
 * switch clock(frequency) via MFGPLL
 */
static void __mt_gpufreq_clock_switch(unsigned int freq_new)
{
	enum g_posdiv_power_enum posdiv_power;
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
	posdiv_power = __mt_gpufreq_get_posdiv_power(freq_new);
	dds = __mt_gpufreq_calculate_dds(freq_new, posdiv_power);
	pll = (0x80000000) | (posdiv_power << POSDIV_SHIFT) | dds;

#ifndef CONFIG_MTK_FREQ_HOPPING
	/* force parking if FHCTL not ready */
	parking = true;
#else
	if (posdiv_power != g_posdiv_power) {
		parking = true;
		g_posdiv_power = posdiv_power;
	} else {
		parking = false;
	}
#endif

	if (parking) {
		/* mfgpll_ck to mainpll_d5(218.4MHz) */
		__mt_gpufreq_switch_to_clksrc(CLOCK_SUB);
		/*
		 * MFGPLL_CON1[31:31] = MFGPLL_SDM_PCW_CHG
		 * MFGPLL_CON1[26:24] = MFGPLL_POSDIV
		 * MFGPLL_CON1[21:0]  = MFGPLL_SDM_PCW (dds)
		 */
		DRV_WriteReg32(MFGPLL_CON1, pll);
		udelay(20);
		/* mainpll_d5(218.4MHz) to mfgpll_ck */
		__mt_gpufreq_switch_to_clksrc(CLOCK_MAIN);
#if MT_GPUFREQ_GED_READY == 1
		ged_log_buf_print2(
			gpufreq_ged_log,
			GED_LOG_ATTR_TIME,
			"[MFGPLL_CON1] posdiv_power=%d dds=0x%x, set MFGPLL_CON1 to 0x%08x\n",
			posdiv_power,
			dds,
			pll);
#endif
	} else {
#ifdef CONFIG_MTK_FREQ_HOPPING
		/* FHCTL_HP_EN[4] = MFGPLL_HP_EN */
		mt_dfs_general_pll(MFGPLL_FH_PLL, dds);
#endif
#if MT_GPUFREQ_GED_READY == 1
		ged_log_buf_print2(
			gpufreq_ged_log,
			GED_LOG_ATTR_TIME,
			"[FHCTL_HP] posdiv_power=%d dds=0x%x, MFGPLL_CON1 should be 0x%08x\n",
			posdiv_power,
			dds,
			pll);
#endif
	}
}

/*
 * switch to target clock source
 */
static void __mt_gpufreq_switch_to_clksrc(enum g_clock_source_enum clksrc)
{
	int ret;

	ret = clk_prepare_enable(g_clk->clk_mux);
	if (ret)
		gpufreq_pr_debug(
				"@%s: enable clk_mux(TOP_MUX_MFG) failed, ret = %d\n",
				__func__,
				ret);
	else
		gpufreq_pr_debug(
				"@%s: enable clk_mux(TOP_MUX_MFG) done\n",
				__func__);

	if (clksrc == CLOCK_MAIN) {
		ret = clk_set_parent(g_clk->clk_mux, g_clk->clk_main_parent);
		if (ret)
			gpufreq_pr_debug(
					"@%s: switch to main clock source failed, ret = %d\n",
					__func__,
					ret);
		else
			gpufreq_pr_debug(
					"@%s: switch to main clock source done\n",
					__func__);
	} else if (clksrc == CLOCK_SUB) {
		ret = clk_set_parent(g_clk->clk_mux, g_clk->clk_sub_parent);
		if (ret)
			gpufreq_pr_debug(
					"@%s: switch to sub clock source failed, ret = %d\n",
					__func__,
					ret);
		else
			gpufreq_pr_debug(
					"@%s: switch to sub clock source done\n",
					__func__);
	} else {
		gpufreq_pr_debug(
				"@%s: clock source index is not valid, clksrc = %d\n",
				__func__,
				clksrc);
	}

	clk_disable_unprepare(g_clk->clk_mux);
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
	unsigned int vgpu_steps, vsram_steps, final_steps;
	unsigned int sfchg_rate;

	gpufreq_pr_debug(
			"@%s: vgpu_new = %d, vgpu_old = %d, vsram_gpu_new = %d, vsram_gpu_old = %d\n",
			__func__,
			vgpu_new,
			vgpu_old,
			vsram_gpu_new,
			vsram_gpu_old);

	if (g_fixed_freq_volt_state) {
		gpufreq_pr_debug(
			"@%s: skip DVS when both freq and volt are fixed\n",
			__func__);
		return;
	}

#if MT_GPUFREQ_SETTLE_TIME_PROFILE == 0
	if (vgpu_new > vgpu_old) {
		vgpu_steps = ((vgpu_new - vgpu_old) / PMIC_STEP) + 1;
		vsram_steps = ((vsram_gpu_new - vsram_gpu_old) / PMIC_STEP) + 1;

		regulator_set_voltage(
				g_pmic->reg_vsram_gpu,
				vsram_gpu_new * 10,
				VSRAM_GPU_MAX_VOLT * 10 + 125);
		regulator_set_voltage(
				g_pmic->reg_vgpu,
				vgpu_new * 10,
				VGPU_MAX_VOLT * 10 + 125);

		sfchg_rate = (g_vgpu_sfchg_rrate > g_vsram_sfchg_rrate) ?
				g_vgpu_sfchg_rrate : g_vsram_sfchg_rrate;
	} else if (vgpu_new < vgpu_old) {
		vgpu_steps = ((vgpu_old - vgpu_new) / PMIC_STEP) + 1;
		vsram_steps = ((vsram_gpu_old - vsram_gpu_new) / PMIC_STEP) + 1;

		regulator_set_voltage(
				g_pmic->reg_vgpu,
				vgpu_new * 10,
				VGPU_MAX_VOLT * 10 + 125);
		regulator_set_voltage(
				g_pmic->reg_vsram_gpu,
				vsram_gpu_new * 10,
				VSRAM_GPU_MAX_VOLT * 10 + 125);

		sfchg_rate = (g_vgpu_sfchg_frate > g_vsram_sfchg_frate) ?
				g_vgpu_sfchg_frate : g_vsram_sfchg_frate;
	} else {
		/* voltage no change */
		return;
	}

	final_steps = (vgpu_steps > vsram_steps) ? vgpu_steps : vsram_steps;
	udelay(final_steps * sfchg_rate + 52);
#else
	if (vgpu_new > vgpu_old) {
		vgpu_steps = ((vgpu_new - vgpu_old) / PMIC_STEP) + 1;
		vsram_steps = ((vsram_gpu_new - vsram_gpu_old) / PMIC_STEP) + 1;

		regulator_set_voltage(
				g_pmic->reg_vsram_gpu,
				vsram_gpu_new * 10,
				VSRAM_GPU_MAX_VOLT * 10 + 125);
		udelay(vsram_steps * g_vsram_sfchg_rrate + 52);

		regulator_set_voltage(
				g_pmic->reg_vgpu,
				vgpu_new * 10,
				VGPU_MAX_VOLT * 10 + 125);
		udelay(vgpu_steps * g_vgpu_sfchg_rrate + 52);

	} else if (vgpu_new < vgpu_old) {
		vgpu_steps = ((vgpu_old - vgpu_new) / PMIC_STEP) + 1;
		vsram_steps = ((vsram_gpu_old - vsram_gpu_new) / PMIC_STEP) + 1;

		regulator_set_voltage(
				g_pmic->reg_vgpu,
				vgpu_new * 10,
				VGPU_MAX_VOLT * 10 + 125);
		udelay(vgpu_steps * g_vgpu_sfchg_frate + 52);

		regulator_set_voltage(
				g_pmic->reg_vsram_gpu,
				vsram_gpu_new * 10,
				VSRAM_GPU_MAX_VOLT * 10 + 125);
		udelay(vsram_steps * g_vsram_sfchg_frate + 52);

	} else {
		/* voltage no change */
		return;
	}
#endif
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
		gpufreq_pr_debug(
				"@%s: set AUTO_MODE(%d) or PWM_MODE(%d) to PMIC(VGPU), mode = %d\n",
				__func__,
				REGULATOR_MODE_NORMAL,
				REGULATOR_MODE_FAST, mode);
	else
		gpufreq_pr_err(
				"@%s: failed to configure mode, ret = %d, mode = %d\n",
				__func__,
				ret,
				mode);
}

/*
 * set fixed frequency for PROCFS: fixed_freq_volt
 */
static void __mt_gpufreq_set_fixed_freq(int fixed_freq)
{
	gpufreq_pr_debug(
			"@%s: before, g_fixed_freq = %d, g_fixed_vgpu = %d\n",
			__func__,
			g_fixed_freq,
			g_fixed_vgpu);
	g_fixed_freq = fixed_freq;
	g_fixed_vgpu = g_cur_opp_vgpu;
	gpufreq_pr_debug(
			"@%s: now, g_fixed_freq = %d, g_fixed_vgpu = %d\n",
			__func__,
			g_fixed_freq,
			g_fixed_vgpu);

	__mt_gpufreq_clock_switch(g_fixed_freq);

	g_cur_opp_freq = g_fixed_freq;
}

/*
 * set fixed voltage for PROCFS: fixed_freq_volt
 */
static void __mt_gpufreq_set_fixed_vgpu(int fixed_vgpu)
{
	gpufreq_pr_debug(
			"@%s: before, g_fixed_freq = %d, g_fixed_vgpu = %d\n",
			__func__,
			g_fixed_freq,
			g_fixed_vgpu);
	g_fixed_freq = g_cur_opp_freq;
	g_fixed_vgpu = fixed_vgpu;
	gpufreq_pr_debug(
			"@%s: now, g_fixed_freq = %d, g_fixed_vgpu = %d\n",
			__func__,
			g_fixed_freq,
			g_fixed_vgpu);

	__mt_gpufreq_volt_switch_without_vsram_gpu(
			g_cur_opp_vgpu,
			g_fixed_vgpu);

	g_cur_opp_vgpu = g_fixed_vgpu;
	g_cur_opp_vsram_gpu =
			__mt_gpufreq_get_vsram_gpu_by_vgpu(g_fixed_vgpu);
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

	gpufreq_pr_debug(
			"@%s: request freq = %d, posdiv = %d\n",
			__func__,
			freq_khz,
			(1 << posdiv_power));

	/* only use posdiv 4 or 8 */
	if ((freq_khz >= POSDIV_8_MIN_FREQ) &&
		(freq_khz <= POSDIV_4_MAX_FREQ)) {
		dds = (((freq_khz / TO_MHZ_HEAD *
				(1 << posdiv_power)) << DDS_SHIFT) /
				MFGPLL_FIN + ROUNDING_VALUE) / TO_MHZ_TAIL;
	} else {
		gpufreq_pr_err(
				"@%s: out of range, freq_khz = %d\n",
				__func__,
				freq_khz);
	}

	return dds;
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
#if MT_GPUFREQ_STATIC_PWR_READY2USE == 1
	p_leakage = mt_spower_get_leakage(MTK_SPOWER_GPU, (volt / 100), temp);
	if (!g_buck_on || p_leakage < 0)
		p_leakage = 0;
#else
	p_leakage = 71;
#endif

	p_total = p_dynamic + p_leakage;

	gpufreq_pr_debug(
			"@%s: idx = %d, p_dynamic = %d, p_leakage = %d, p_total = %d, temp = %d\n",
			__func__,
			idx,
			p_dynamic,
			p_leakage,
			p_total,
			temp);

	g_power_table[idx].gpufreq_power = p_total;
}

/*
 * VGPU slew rate calculation
 * mode(false) : falling rate per step
 * mode(true) : rising rate per step
 */
static unsigned int __calculate_vgpu_sfchg_rate(bool mode)
{
	unsigned int sfchg_rate_vgpu;

	/* [MT6359][VGPU]
	 * resoulation : 6.25 mV/step
	 * dvfs ramp up spec (soft change) : slew rate 8 mV/us +- 20%
	 * dvfs ramp down spec (soft change) : slew rate 4 mV/us +- 20%
	 * rising rate per step : 6.25 (mV/step) / (9.6 ~ 6.4 (mV/us))
	 * falling rate per step : 6.25 (mV/step) / (4.8 ~ 3.2 (mV/us))
	 */

	if (mode) {
		/* rising rate per step = 0.651 ~ 0.977 (us/step) */
		sfchg_rate_vgpu = 1;
	} else {
		/* falling rate per step = 1.302 ~ 1.953 (us/step) */
		sfchg_rate_vgpu = 2;
	}

	return sfchg_rate_vgpu;
}

/*
 * VSRAM_GPU slew rate calculation
 * mode(false) : falling rate per step
 * mode(true) : rising rate per step
 */
static unsigned int __calculate_vsram_sfchg_rate(bool mode)
{
	unsigned int sfchg_rate_vsram;

	/* [MT6359][VSRAM_GPU] (belong to MT6359's VPU)
	 * resoulation : 6.25 mV/step
	 * dvfs ramp up spec (soft change) : slew rate 8 mV/us +- 20%
	 * dvfs ramp down spec (soft change) : slew rate 4 mV/us +- 20%
	 * rising rate per step : 6.25 (mV/step) / (9.6 ~ 6.4 (mV/us))
	 * falling rate per step : 6.25 (mV/step) / (4.8 ~ 3.2 (mV/us))
	 */

	if (mode) {
		/* rising rate per step = 0.651 ~ 0.977 (us/step) */
		sfchg_rate_vsram = 1;
	} else {
		/* falling rate per step = 1.302 ~ 1.953 (us/step) */
		sfchg_rate_vsram = 2;
	}

	return sfchg_rate_vsram;
}

/*
 * get post divider power value
 */
static enum g_posdiv_power_enum __mt_gpufreq_get_posdiv_power(
		unsigned int freq)
{
	/*
	 * MFGPLL VCO range: 1.5GHz - 3.8GHz by divider 1/2/4/8/16,
	 * MFGPLL range: 125MHz - 3.8GHz,
	 * | VCO MAX | VCO MIN | POSDIV | PLL OUT MAX | PLL OUT MIN |
	 * |  3800   |  1500   |    1   |   3800MHz   |   1500MHz   | (X)
	 * |  3800   |  1500   |    2   |   1900MHz   |   750MHz    | (X)
	 * |  3800   |  1500   |    4   |   950MHz    |   375MHz    | (O)
	 * |  3800   |  1500   |    8   |   475MHz    |   187.5MHz  | (O)
	 * |  3800   |  2000   |   16   |   237.5MHz  |   125MHz    | (X)
	 */
	enum g_posdiv_power_enum posdiv_power = POSDIV_POWER_4;

	/*
	 * only use posdiv 4 or 8
	 * sub-clksrc is MAINPLL_D5(218.4MHz), please make sure don't overclock
	 */
	if (freq < POSDIV_4_MIN_FREQ)
		posdiv_power = POSDIV_POWER_8;

	return posdiv_power;
}

/*
 * get current frequency (KHZ)
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

	gpufreq_pr_debug(
			"@%s: mfgpll = 0x%lx, freq = %d KHz, posdiv_power = %d\n",
			__func__,
			mfgpll,
			freq_khz,
			posdiv_power);

	return freq_khz;
}

/*
 * get current vsram voltage (mV * 100)
 */
static unsigned int __mt_gpufreq_get_cur_vsram_gpu(void)
{
	unsigned int volt = 0;

	if (g_buck_on) {
		/* regulator_get_voltage prints volt with uV */
		volt = regulator_get_voltage(g_pmic->reg_vsram_gpu) / 10;
	}

	gpufreq_pr_debug(
			"@%s: volt = %d\n",
			__func__,
			volt);

	return volt;
}

/*
 * get current voltage (mV * 100)
 */
static unsigned int __mt_gpufreq_get_cur_vgpu(void)
{
	unsigned int volt = 0;

	if (g_buck_on) {
		/* regulator_get_voltage prints volt with uV */
		volt = regulator_get_voltage(g_pmic->reg_vgpu) / 10;
	}

	gpufreq_pr_debug(
			"@%s: volt = %d\n",
			__func__,
			volt);

	return volt;
}

/*
 * get OPP table index by voltage (mV * 100)
 */
static int __mt_gpufreq_get_opp_idx_by_vgpu(unsigned int vgpu)
{
	int i = g_max_opp_idx_num - 1;

	while (i >= 0) {
		if (g_opp_table[i--].gpufreq_volt >= vgpu)
			goto out;
	}

out:
	return i + 1;
}

/*
 * calculate vsram_gpu via given vgpu
 */
static unsigned int __mt_gpufreq_get_vsram_gpu_by_vgpu(unsigned int vgpu)
{
	unsigned int vsram_gpu;

	if (vgpu > FIXED_VSRAM_VOLT_THSRESHOLD)
		vsram_gpu = vgpu + 10000;
	else
		vsram_gpu = FIXED_VSRAM_VOLT;

	gpufreq_pr_debug(
			"@%s: vgpu = %d, vsram_gpu = %d\n",
			__func__,
			vgpu,
			vsram_gpu);

	return vsram_gpu;
}

/*
 * update intepolating voltage via given vgpu
 */
static void __mt_gpufreq_update_intepolating_volt(
		unsigned int idx, unsigned int high,
		unsigned int low, int factor)
{
	int intepolating_volt, i;

	for (i = 1; i < factor; i++) {
		intepolating_volt =
		((high * (factor - i)) + low * (i)) / factor;
		g_opp_table[idx + i].gpufreq_volt =
		VOLT_NORMALIZATION(intepolating_volt);
		g_opp_table[idx + i].gpufreq_vsram =
		__mt_gpufreq_get_vsram_gpu_by_vgpu(
		g_opp_table[idx + i].gpufreq_volt);
	}
}

/*
 * get limited frequency by limited power (mW)
 */
static unsigned int __mt_gpufreq_get_limited_freq_by_power(
		unsigned int limited_power)
{
	int i;
	unsigned int limited_freq;

	limited_freq = g_power_table[g_segment_min_opp_idx].gpufreq_khz;

	for (i = g_segment_max_opp_idx; i <= g_segment_min_opp_idx; i++) {
		if (g_power_table[i].gpufreq_power <= limited_power) {
			limited_freq = g_power_table[i].gpufreq_khz;
			break;
		}
	}

	gpufreq_pr_debug(
			"@%s: limited_freq = %d\n",
			__func__,
			limited_freq);

	return limited_freq;
}

#if MT_GPUFREQ_DYNAMIC_POWER_TABLE_UPDATE == 1
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

	gpufreq_pr_debug(
			"@%s: temp = %d\n",
			__func__,
			temp);

	mutex_lock(&mt_gpufreq_lock);

	if ((temp >= -20) && (temp <= 125)) {
		for (i = 0; i < g_max_opp_idx_num; i++) {
			freq = g_power_table[i].gpufreq_khz;
			volt = g_power_table[i].gpufreq_volt;

			__mt_gpufreq_calculate_power(i, freq, volt, temp);

			gpufreq_pr_debug(
					"@%s: [%d] freq_khz = %d, volt = %d, power = %d\n",
					__func__,
					i,
					g_power_table[i].gpufreq_khz,
					g_power_table[i].gpufreq_volt,
					g_power_table[i].gpufreq_power);
		}
	} else {
		gpufreq_pr_err(
				"@%s: temp < -20 or temp > 125, not update power table!\n",
				__func__);
	}

	mutex_unlock(&mt_gpufreq_lock);
}
#endif

/* update OPP limited index for Thermal/Power/PBM protection */
static void __mt_gpufreq_update_max_limited_idx(void)
{
	int i = 0;
	unsigned int limited_idx = g_segment_max_opp_idx;

	/* Check lowest frequency index in all limitation */
	for (i = 0; i < NUMBER_OF_LIMITED_IDX; i++) {
		if (g_limited_idx_array[i] > limited_idx) {
			if (!g_limited_ignore_array[i])
				limited_idx = g_limited_idx_array[i];
		}
	}

	g_max_limited_idx = limited_idx;

	gpufreq_pr_debug(
			"@%s: g_max_limited_idx = %d\n",
			__func__,
			g_max_limited_idx);
}

#if MT_GPUFREQ_BATT_OC_PROTECT == 1
/*
 * limit OPP index for Over Currents (OC) protection
 */
static void __mt_gpufreq_batt_oc_protect(unsigned int limited_idx)
{
	mutex_lock(&mt_gpufreq_power_lock);

	gpufreq_pr_debug(
			"@%s: limited_idx = %d\n",
			__func__,
			limited_idx);

	g_limited_idx_array[IDX_BATT_OC_LIMITED] = limited_idx;
	__mt_gpufreq_update_max_limited_idx();

	mutex_unlock(&mt_gpufreq_power_lock);
}
#endif

#if MT_GPUFREQ_BATT_PERCENT_PROTECT == 1
/*
 * limit OPP index for Battery Percentage protection
 */
static void __mt_gpufreq_batt_percent_protect(unsigned int limited_index)
{
	mutex_lock(&mt_gpufreq_power_lock);

	gpufreq_pr_debug(
			"@%s: limited_index = %d\n",
			__func__,
			limited_index);

	g_limited_idx_array[IDX_BATT_PERCENT_LIMITED] = limited_index;
	__mt_gpufreq_update_max_limited_idx();

	mutex_unlock(&mt_gpufreq_power_lock);
}
#endif

#if MT_GPUFREQ_LOW_BATT_VOLT_PROTECT == 1
/*
 * limit OPP index for Low Battery Volume protection
 */
static void __mt_gpufreq_low_batt_protect(unsigned int limited_index)
{
	mutex_lock(&mt_gpufreq_power_lock);

	gpufreq_pr_debug(
			"@%s: limited_index = %d\n",
			__func__,
			limited_index);

	g_limited_idx_array[IDX_LOW_BATT_LIMITED] = limited_index;
	__mt_gpufreq_update_max_limited_idx();

	mutex_unlock(&mt_gpufreq_power_lock);
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

				if (g_power_table[i].gpufreq_volt == cur_vgpu) {
					power = g_power_table[i].gpufreq_power;
					found = 1;
#if MT_GPUFREQ_KICKER_PBM_READY == 1
					kicker_pbm_by_gpu(
							true,
							power,
							cur_vgpu / 100);
#endif
					gpufreq_pr_debug(
							"@%s: request power = %d, cur_vgpu = %d (uV), cur_freq = %d (KHz)\n",
							__func__,
							power,
							cur_vgpu * 10,
							cur_freq);
					return;
				}
			}
		}

		if (!found) {
			gpufreq_pr_debug(
					"@%s: tmp_idx = %d\n",
					__func__,
					tmp_idx);
			if (tmp_idx != -1 && tmp_idx < g_max_opp_idx_num) {
				/*
				 * use freq to find
				 * corresponding power budget
				 */
				power = g_power_table[tmp_idx].gpufreq_power;
#if MT_GPUFREQ_KICKER_PBM_READY == 1
				kicker_pbm_by_gpu(true, power, cur_vgpu / 100);
#endif
				gpufreq_pr_debug(
						"@%s: request power = %d, cur_vgpu = %d (uV), cur_freq = %d (KHz)\n",
						__func__,
						power,
						cur_vgpu * 10,
						cur_freq);
			} else {
				gpufreq_pr_debug(
						"@%s: cannot found request power! cur_vgpu = %d (uV), cur_freq = %d (KHz)\n",
						__func__,
						cur_vgpu * 10,
						cur_freq);
			}
		}
	} else {
#if MT_GPUFREQ_KICKER_PBM_READY == 1
		kicker_pbm_by_gpu(false, 0, cur_vgpu / 100);
#endif
	}
}

/*
 * (default) OPP table initialization
 */
static void __mt_gpufreq_setup_opp_table(
		struct g_opp_table_info *freqs,
		int num)
{
	unsigned int i = 0;

	g_opp_table = kzalloc((num) * sizeof(*freqs), GFP_KERNEL);
	g_opp_table_default = kzalloc((num) * sizeof(*freqs), GFP_KERNEL);

	if (g_opp_table == NULL || g_opp_table_default == NULL)
		return;

	for (i = 0; i < num; i++) {
		g_opp_table[i].gpufreq_khz = freqs[i].gpufreq_khz;
		g_opp_table[i].gpufreq_volt = freqs[i].gpufreq_volt;
		g_opp_table[i].gpufreq_vsram = freqs[i].gpufreq_vsram;

		g_opp_table_default[i].gpufreq_khz = freqs[i].gpufreq_khz;
		g_opp_table_default[i].gpufreq_volt = freqs[i].gpufreq_volt;
		g_opp_table_default[i].gpufreq_vsram = freqs[i].gpufreq_vsram;

		gpufreq_pr_debug(
				"@%s: idx = %u, freq_khz = %u, volt = %u, vsram = %u\n",
				__func__,
				i,
				freqs[i].gpufreq_khz,
				freqs[i].gpufreq_volt,
				freqs[i].gpufreq_vsram);
	}

	/* setup OPP table by device ID */
	if (g_segment_id == MT6785U_SEGMENT)
		g_segment_max_opp_idx = 0;
	else if (g_segment_id == MT6785T_SEGMENT)
		g_segment_max_opp_idx = 15;
	else if (g_segment_id == MT6785_SEGMENT)
		g_segment_max_opp_idx = 21;
	else if (g_segment_id == MT6783_SEGMENT)
		g_segment_max_opp_idx = 30;
	else
		g_segment_max_opp_idx = 15;

	g_segment_min_opp_idx = NUM_OF_OPP_IDX - 1;

	g_max_opp_idx_num = num;
	g_max_limited_idx = g_segment_max_opp_idx;
	g_DVFS_off_by_ptpod_idx = g_segment_max_opp_idx;

	g_ptpod_opp_idx_table = g_ptpod_opp_idx_table_segment;
	g_ptpod_opp_idx_num = ARRAY_SIZE(g_ptpod_opp_idx_table_segment);

	gpufreq_pr_debug(
			"@%s: g_segment_max_opp_idx = %u, g_max_opp_idx_num = %u, g_segment_min_opp_idx = %u\n",
			__func__,
			g_segment_max_opp_idx,
			g_max_opp_idx_num,
			g_segment_min_opp_idx);

	__mt_gpufreq_cal_sb_opp_index();
	__mt_gpufreq_setup_opp_power_table(num);
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
		gpufreq_pr_debug(
				"@%s: temp < -20 or temp > 125!\n",
				__func__);
		temp = 65;
	}

	for (i = 0; i < num; i++) {
		g_power_table[i].gpufreq_khz = g_opp_table[i].gpufreq_khz;
		g_power_table[i].gpufreq_volt = g_opp_table[i].gpufreq_volt;

		__mt_gpufreq_calculate_power(i, g_power_table[i].gpufreq_khz,
				g_power_table[i].gpufreq_volt, temp);

		gpufreq_pr_debug(
				"@%s: [%d], freq_khz = %u, volt = %u, power = %u\n",
				__func__,
				i,
				g_power_table[i].gpufreq_khz,
				g_power_table[i].gpufreq_volt,
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

/*
 * Set default OPP index at driver probe function
 */
static void __mt_gpufreq_set_initial(void)
{
	unsigned int cur_vgpu = 0;
	unsigned int cur_freq = 0;
	unsigned int cur_vsram_gpu = 0;
	unsigned int cur_preloader_idx = BOOTUP_OPP_IDX;

	/* set default OPP index */
	g_cur_opp_idx = g_segment_max_opp_idx;

	/* set POST DIVIDER initial value */
	g_posdiv_power =
			(DRV_Reg32(MFGPLL_CON1) & (0x7 << POSDIV_SHIFT))
			>> POSDIV_SHIFT;

	gpufreq_pr_info(
			"@%s: preloader opp index = %d(%d), initial opp index = %d, g_posdiv_power = %d\n",
			__func__,
			cur_preloader_idx,
			mt_get_ckgen_freq(15),
			g_cur_opp_idx,
			g_posdiv_power);

	cur_freq = __mt_gpufreq_get_cur_freq();
	cur_vgpu = __mt_gpufreq_get_cur_vgpu();
	cur_vsram_gpu = __mt_gpufreq_get_cur_vsram_gpu();

	mutex_lock(&mt_gpufreq_lock);

	__mt_gpufreq_set(cur_preloader_idx, g_cur_opp_idx,
			cur_freq,
			g_opp_table[g_cur_opp_idx].gpufreq_khz,
			cur_vgpu,
			g_opp_table[g_cur_opp_idx].gpufreq_volt,
			cur_vsram_gpu,
			g_opp_table[g_cur_opp_idx].gpufreq_vsram);

	g_cur_opp_freq = g_opp_table[g_cur_opp_idx].gpufreq_khz;
	g_cur_opp_vgpu = g_opp_table[g_cur_opp_idx].gpufreq_volt;
	g_cur_opp_vsram_gpu = g_opp_table[g_cur_opp_idx].gpufreq_vsram;

	mutex_unlock(&mt_gpufreq_lock);
}

static int __mt_gpufreq_init_pmic(struct platform_device *pdev)
{
	g_pmic = kzalloc(sizeof(struct g_pmic_info), GFP_KERNEL);

	if (g_pmic == NULL)
		return -ENOMEM;

	/* VGPU is MT6359's VGPU11 buck */
	g_pmic->reg_vgpu = regulator_get(&pdev->dev, "vgpu11");
	if (IS_ERR(g_pmic->reg_vgpu)) {
		gpufreq_pr_err(
				"@%s: cannot get VGPU\n",
				__func__);
		return PTR_ERR(g_pmic->reg_vgpu);
	}

	/* VSRAM_GPU is MT6359's VPU buck */
	g_pmic->reg_vsram_gpu = regulator_get(&pdev->dev, "vpu");
	if (IS_ERR(g_pmic->reg_vsram_gpu)) {
		gpufreq_pr_err(
				"@%s: cannot get VSRAM_GPU\n",
				__func__);
		return PTR_ERR(g_pmic->reg_vsram_gpu);
	}

	/* setup PMIC init value */
	g_vgpu_sfchg_rrate = __calculate_vgpu_sfchg_rate(true);
	g_vgpu_sfchg_frate = __calculate_vgpu_sfchg_rate(false);
	g_vsram_sfchg_rrate = __calculate_vsram_sfchg_rate(true);
	g_vsram_sfchg_frate = __calculate_vsram_sfchg_rate(false);

	/* set VSRAM_GPU */
	regulator_set_voltage(
			g_pmic->reg_vsram_gpu,
			VSRAM_GPU_MAX_VOLT * 10,
			VSRAM_GPU_MAX_VOLT * 10 + 125);
	/* set VGPU */
	regulator_set_voltage(
			g_pmic->reg_vgpu,
			VGPU_MAX_VOLT * 10,
			VGPU_MAX_VOLT * 10 + 125);

	/* enable bucks enforcement for correct reference counts */
	if (regulator_enable(g_pmic->reg_vsram_gpu))
		gpufreq_pr_err(
				"@%s: enable VSRAM_GPU failed\n",
				__func__);
	if (regulator_enable(g_pmic->reg_vgpu))
		gpufreq_pr_err(
				"@%s: enable VGPU failed\n",
				__func__);

	g_buck_on = true;

	gpufreq_pr_info(
			"@%s: VGPU sfchg raising rate: %d (us/step),\t"
			" VGPU sfchg falling rate: %d (us/step),\t"
			" VSRAM_GPU sfchg raising rate: %d (us/step),\t"
			" VSRAM_GPU sfchg falling rate: %d (us/step)\n",
			__func__,
			g_vgpu_sfchg_rrate,
			g_vgpu_sfchg_frate,
			g_vsram_sfchg_rrate,
			g_vsram_sfchg_frate);

	gpufreq_pr_info(
			"@%s: VGPU is enabled = %d (%d mV), VSRAM_GPU is enabled = %d (%d mV)\n",
			__func__,
			regulator_is_enabled(g_pmic->reg_vgpu),
			(regulator_get_voltage(g_pmic->reg_vgpu) / 1000),
			regulator_is_enabled(g_pmic->reg_vsram_gpu),
			(regulator_get_voltage(g_pmic->reg_vsram_gpu) / 1000));

	return 0;
}

static int __mt_gpufreq_init_clk(struct platform_device *pdev)
{
	/* MFGPLL is from APMIXED and its parent clock is from XTAL(26MHz); */
	g_apmixed_base = __mt_gpufreq_of_ioremap("mediatek,apmixed", 0);
	if (!g_apmixed_base) {
		gpufreq_pr_err(
				"@%s: ioremap failed at APMIXED",
				__func__);
		return -ENOENT;
	}

	g_clk = kzalloc(sizeof(struct g_clk_info), GFP_KERNEL);
	if (g_clk == NULL)
		return -ENOMEM;

	g_clk->clk_mux = devm_clk_get(&pdev->dev, "clk_mux");
	if (IS_ERR(g_clk->clk_mux)) {
		gpufreq_pr_err(
				"@%s: cannot get clk_mux\n",
				__func__);
		return PTR_ERR(g_clk->clk_mux);
	}

	g_clk->clk_main_parent = devm_clk_get(&pdev->dev, "clk_main_parent");
	if (IS_ERR(g_clk->clk_main_parent)) {
		gpufreq_pr_err(
				"@%s: cannot get clk_main_parent\n",
				__func__);
		return PTR_ERR(g_clk->clk_main_parent);
	}

	g_clk->clk_sub_parent = devm_clk_get(&pdev->dev, "clk_sub_parent");
	if (IS_ERR(g_clk->clk_sub_parent)) {
		gpufreq_pr_err(
				"@%s: cannot get clk_sub_parent\n",
				__func__);
		return PTR_ERR(g_clk->clk_sub_parent);
	}

	g_clk->subsys_mfg_cg = devm_clk_get(&pdev->dev, "subsys_mfg_cg");
	if (IS_ERR(g_clk->subsys_mfg_cg)) {
		gpufreq_pr_err(
				"@%s: cannot get subsys_mfg_cg\n",
				__func__);
		return PTR_ERR(g_clk->subsys_mfg_cg);
	}

	g_clk->mtcmos_mfg_async = devm_clk_get(&pdev->dev, "mtcmos_mfg_async");
	if (IS_ERR(g_clk->mtcmos_mfg_async)) {
		gpufreq_pr_err(
				"@%s: cannot get mtcmos_mfg_async\n",
				__func__);
		return PTR_ERR(g_clk->mtcmos_mfg_async);
	}

	g_clk->mtcmos_mfg = devm_clk_get(&pdev->dev, "mtcmos_mfg");
	if (IS_ERR(g_clk->mtcmos_mfg)) {
		gpufreq_pr_err(
				"@%s: cannot get mtcmos_mfg\n",
				__func__);
		return PTR_ERR(g_clk->mtcmos_mfg);
	}

	g_clk->mtcmos_mfg_core0 = devm_clk_get(&pdev->dev, "mtcmos_mfg_core0");
	if (IS_ERR(g_clk->mtcmos_mfg_core0)) {
		gpufreq_pr_err(
				"@%s: cannot get mtcmos_mfg_core0\n",
				__func__);
		return PTR_ERR(g_clk->mtcmos_mfg_core0);
	}

	g_clk->mtcmos_mfg_core1 = devm_clk_get(&pdev->dev, "mtcmos_mfg_core1");
	if (IS_ERR(g_clk->mtcmos_mfg_core1)) {
		gpufreq_pr_err(
				"@%s: cannot get mtcmos_mfg_core1\n",
				__func__);
		return PTR_ERR(g_clk->mtcmos_mfg_core1);
	}

	g_clk->mtcmos_mfg_core2 = devm_clk_get(&pdev->dev, "mtcmos_mfg_core2");
	if (IS_ERR(g_clk->mtcmos_mfg_core2)) {
		gpufreq_pr_err(
				"@%s: cannot get mtcmos_mfg_core2\n",
				__func__);
		return PTR_ERR(g_clk->mtcmos_mfg_core2);
	}

	g_clk->mtcmos_mfg_core3 = devm_clk_get(&pdev->dev, "mtcmos_mfg_core3");
	if (IS_ERR(g_clk->mtcmos_mfg_core3)) {
		gpufreq_pr_err(
				"@%s: cannot get mtcmos_mfg_core3\n",
				__func__);
		return PTR_ERR(g_clk->mtcmos_mfg_core3);
	}

	gpufreq_pr_info(
			"@%s: clk_mux is at 0x%p, clk_main_parent is at 0x%p,\t"
			" clk_sub_parent is at 0x%p, subsys_mfg_cg is at 0x%p,\t"
			" mtcmos_mfg_async is at 0x%p, mtcmos_mfg is at 0x%p,\t"
			" mtcmos_mfg_core0 is at 0x%p, mtcmos_mfg_core1 is at 0x%p,\t"
			" mtcmos_mfg_core2 is at 0x%p, mtcmos_mfg_core3 is at 0x%p\n",
			__func__,
			g_clk->clk_mux, g_clk->clk_main_parent,
			g_clk->clk_sub_parent, g_clk->subsys_mfg_cg,
			g_clk->mtcmos_mfg_async, g_clk->mtcmos_mfg,
			g_clk->mtcmos_mfg_core0, g_clk->mtcmos_mfg_core1,
			g_clk->mtcmos_mfg_core2, g_clk->mtcmos_mfg_core3);

	return 0;
}

static void __mt_gpufreq_init_efuse(struct platform_device *pdev)
{
#ifdef CONFIG_MTK_DEVINFO
    struct nvmem_cell *efuse_cell;
    unsigned int *efuse_buf;
    size_t efuse_len;
#endif

#ifdef CONFIG_MTK_DEVINFO
    efuse_cell = nvmem_cell_get(&pdev->dev, "efuse_segment_cell");
    if (IS_ERR(efuse_cell)) {
      PTR_ERR(efuse_cell);
    }

    efuse_buf = (unsigned int *)nvmem_cell_read(efuse_cell, &efuse_len);
    nvmem_cell_put(efuse_cell);
    if (IS_ERR(efuse_buf)) {
      PTR_ERR(efuse_buf);
    }

    g_efuse_id = (*efuse_buf & 0xFF);
    kfree(efuse_buf);

#else
    g_efuse_id = 0x0;
#endif /* CONFIG_MTK_DEVINFO */

	if (g_efuse_id == 0x80 ||
		g_efuse_id == 0x01 ||
		g_efuse_id == 0x40 ||
		g_efuse_id == 0x02) {
		g_segment_id = MT6783_SEGMENT;
	} else if (g_efuse_id == 0xC0 ||
		g_efuse_id == 0x03 ||
		g_efuse_id == 0x20 ||
		g_efuse_id == 0x04) {
		g_segment_id = MT6785_SEGMENT;
	} else if (g_efuse_id == 0xE0 ||
		g_efuse_id == 0x07) {
		g_segment_id = MT6785U_SEGMENT;
	} else {
		g_segment_id = MT6785T_SEGMENT;
	}

	gpufreq_pr_info(
			"@%s: g_efuse_id = 0x%08X, g_segment_id = %d\n",
			__func__,
			g_efuse_id,
			g_segment_id);
}

static void __mt_gpufreq_init_others(void)
{
	int i;

#if MT_GPUFREQ_STATIC_PWR_READY2USE == 1
	/* Initial leackage power usage */
	mt_spower_init();
#endif

#if MT_GPUFREQ_LOW_BATT_VOLT_PROTECT == 1
	g_low_batt_limited_idx_lvl_0 = g_segment_max_opp_idx;
	for (i = g_segment_max_opp_idx; i <= g_segment_min_opp_idx; i++) {
		if (g_opp_table[i].gpufreq_khz <=
			MT_GPUFREQ_LOW_BATT_VOLT_LIMIT_FREQ) {
			g_low_batt_limited_idx_lvl_2 = i;
			break;
		}
	}
	register_low_battery_notify(
			&mt_gpufreq_low_batt_callback,
			LOW_BATTERY_PRIO_GPU);
#endif

#if MT_GPUFREQ_BATT_PERCENT_PROTECT == 1
	g_batt_percent_limited_idx_lvl_0 = g_segment_max_opp_idx;
	g_batt_percent_limited_idx_lvl_1 = g_segment_max_opp_idx;
	for (i = g_segment_max_opp_idx; i <= g_segment_min_opp_idx; i++) {
		if (g_opp_table[i].gpufreq_khz ==
			MT_GPUFREQ_BATT_PERCENT_LIMIT_FREQ) {
			g_batt_percent_limited_idx_lvl_1 = i;
			break;
		}
	}
	register_battery_percent_notify(
			&mt_gpufreq_batt_percent_callback,
			BATTERY_PERCENT_PRIO_GPU);
#endif

#if MT_GPUFREQ_BATT_OC_PROTECT == 1
	g_batt_oc_limited_idx_lvl_0 = g_segment_max_opp_idx;
	for (i = g_segment_max_opp_idx; i <= g_segment_min_opp_idx; i++) {
		if (g_opp_table[i].gpufreq_khz <=
			MT_GPUFREQ_BATT_OC_LIMIT_FREQ) {
			g_batt_oc_limited_idx_lvl_1 = i;
			break;
		}
	}
	register_battery_oc_notify(
			&mt_gpufreq_batt_oc_callback,
			BATTERY_OC_PRIO_GPU);
#endif

	for (i = 0; i < NUMBER_OF_LIMITED_IDX; i++)
		g_limited_idx_array[i] = g_segment_max_opp_idx;
}

/*
 * gpufreq driver probe
 */
static int __mt_gpufreq_pdrv_probe(struct platform_device *pdev)
{
	struct device_node *node;
	int ret;

	g_opp_stress_test_state = 0;
	g_keep_opp_freq_state = false;

	node = of_find_matching_node(NULL, g_gpufreq_of_match);
	if (!node)
		gpufreq_pr_err(
				"@%s: find GPU node failed\n",
				__func__);
	/* alloc PMIC regulator */
	ret = __mt_gpufreq_init_pmic(pdev);
	if (ret)
		return ret;

	/* init clock source and mtcmos */
	ret = __mt_gpufreq_init_clk(pdev);
	if (ret)
		return ret;

	/* check efuse_id and set the corresponding segment_id */
    __mt_gpufreq_init_efuse(pdev);

	/* init opp table */
	__mt_gpufreq_setup_opp_table(
			g_opp_table_segment,
			ARRAY_SIZE(g_opp_table_segment));

	/* setup initial frequency */
	__mt_gpufreq_set_initial();

	gpufreq_pr_info(
			"@%s: freq = %d (KHz), vgpu = %d (uV), vsram_gpu = %d (uV)\n",
			__func__,
			mt_get_ckgen_freq(15),
			__mt_gpufreq_get_cur_vgpu() * 10,
			__mt_gpufreq_get_cur_vsram_gpu() * 10);

	gpufreq_pr_info(
			"@%s: g_cur_opp_freq = %d, g_cur_opp_vgpu = %d, g_cur_opp_vsram_gpu = %d\n",
			__func__,
			g_cur_opp_freq,
			g_cur_opp_vgpu,
			g_cur_opp_vsram_gpu);

	gpufreq_pr_info(
			"@%s: g_cur_opp_idx = %d\n",
			__func__,
			g_cur_opp_idx);

	__mt_gpufreq_init_others();

	return 0;
}

/*
 * register the gpufreq driver
 */
static int __init __mt_gpufreq_init(void)
{
	int ret = 0;

#if MT_GPUFREQ_BRINGUP == 1
	/* skip driver init in bring up stage */
	gpufreq_pr_info(
			"@%s: init freq = %d (KHz)\n",
			__func__,
			mt_get_ckgen_freq(15));
	return 0;
#endif

	gpufreq_pr_debug(
			"@%s: start to initialize gpufreq driver\n",
			__func__);

#ifdef CONFIG_PROC_FS
	if (__mt_gpufreq_create_procfs())
		goto out;
#endif
	pr_err("__mt_gpufreq_init 1 \n");
	/* register platform driver */
	ret = platform_driver_register(&g_gpufreq_pdrv);
	if (ret)
		gpufreq_pr_err(
				"@%s: fail to register gpufreq driver\n",
				__func__);

out:
#ifdef MT_GPUFREQ_SRAM_DEBUG
	aee_rr_rec_gpu_dvfs_vgpu(0xFF);
	aee_rr_rec_gpu_dvfs_oppidx(0xFF);
	aee_rr_rec_gpu_dvfs_status(0x0);
#endif
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
MODULE_DESCRIPTION("MediaTek GPU-DVFS driver");
MODULE_LICENSE("GPL");
