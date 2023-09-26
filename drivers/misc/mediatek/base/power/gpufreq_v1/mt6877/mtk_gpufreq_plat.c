/*
 * Copyright (C) 2020 MediaTek Inc.
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
#include <linux/seq_file.h>

#include "mtk_gpufreq.h"
#include "mtk_gpufreq_internal.h"
#include "mtk_gpufreq_common.h"

#include "clk-mtk.h"
#include "clk-gate.h"
#include "clk-fmeter.h"
#include "clk-mt6877.h"
#include "dt-bindings/clock/mt6877-clk.h"

#include "mtk_pmic_wrap.h"
#include "mtk_devinfo.h"
#include "upmu_common.h"
#include "upmu_sw.h"
#include "upmu_hw.h"
#ifdef CONFIG_THERMAL
#include "mtk_thermal.h"
#endif
#ifdef CONFIG_MTK_FREQ_HOPPING
#include "mtk_freqhopping.h"
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
#ifdef CONFIG_MTK_RAM_CONSOLE
	aee_rr_rec_gpu_dvfs_vgpu(step |
				(aee_rr_curr_gpu_dvfs_vgpu() & 0xF0));
#endif
}

static inline void gpu_dvfs_vgpu_reset_footprint(void)
{
#ifdef CONFIG_MTK_RAM_CONSOLE
	aee_rr_rec_gpu_dvfs_vgpu(0);
#endif
}

static inline void gpu_dvfs_oppidx_footprint(unsigned int idx)
{
#ifdef CONFIG_MTK_RAM_CONSOLE
	aee_rr_rec_gpu_dvfs_oppidx(idx);
#endif
}

static inline void gpu_dvfs_oppidx_reset_footprint(void)
{
#ifdef CONFIG_MTK_RAM_CONSOLE
	aee_rr_rec_gpu_dvfs_oppidx(0xFF);
#endif
}

static inline void gpu_dvfs_power_count_footprint(int count)
{
#ifdef CONFIG_MTK_RAM_CONSOLE
	aee_rr_rec_gpu_dvfs_power_count(count);
#endif
}

static inline void gpu_dvfs_power_count_reset_footprint(void)
{
#ifdef CONFIG_MTK_RAM_CONSOLE
	aee_rr_rec_gpu_dvfs_power_count(0);
#endif
}

/**
 * ===============================================
 * SECTION : Local functions declaration
 * ===============================================
 */
static int __mt_gpufreq_pdrv_probe(struct platform_device *pdev);
static int __mt_gpufreq_mfgpll_probe(struct platform_device *pdev);
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
static void mt_gpufreq_apply_aging(bool apply);
static void mt_gpufreq_cal_sb_opp_index(void);

static unsigned int __calculate_vgpu_settletime(bool mode, int deltaV);
static unsigned int __calculate_vsram_settletime(bool mode, int deltaV);

static void __mt_gpufreq_switch_to_clksrc(enum g_clock_source_enum clksrc);

static void __mt_gpufreq_enable_by_asensor(void);
static void __mt_gpufreq_disable_by_asensor(void);

/**
 * ===============================================
 * SECTION : Local variables definition
 * ===============================================
 */

static struct mt_gpufreq_power_table_info *g_power_table;
static struct g_pmic_info *g_pmic;
static struct g_clk_info *g_clk;

static const struct of_device_id g_mfgpll_of_match[] = {
	{ .compatible = "mediatek,mt6877-gpu_pll_ctrl" },
	{ /* sentinel */ }
};
static struct platform_driver g_gpufreq_mfgpll_pdrv = {
	.probe = __mt_gpufreq_mfgpll_probe,
	.remove = NULL,
	.driver = {
		.name = "gpufreq_mfgpll",
		.owner = THIS_MODULE,
		.of_match_table = g_mfgpll_of_match,
	},
};

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

static bool g_DVFS_is_paused_by_asensor;
static bool g_cg_on;
static bool g_mtcmos_on;
static bool g_buck_on;
static bool g_fixed_freq_volt_state;
static bool g_probe_done;
static int g_power_count;
static unsigned int g_opp_stress_test_state;
static unsigned int g_max_opp_idx_num;
static unsigned int g_segment_max_opp_idx;
static unsigned int g_segment_min_opp_idx;
static unsigned int g_aging_enable = 1;
static int g_aging_table_id = -1;
static struct g_asensor_info g_asensor;
static unsigned int g_aging_most_agrresive;
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
#if MT_GPUFREQ_SHADER_PWR_CTL_WA
static spinlock_t mt_gpufreq_clksrc_parking_lock;
#endif
static void __iomem *g_gpu_pll_ctrl;
static void __iomem *g_topckgen_base;
static void __iomem *g_mfg_base;
static void __iomem *g_infracfg_base;
static void __iomem *g_toprgu_base;
static void __iomem *g_infra_peri_debug1;
static void __iomem *g_infra_peri_debug2;
static void __iomem *g_infra_peri_debug3;
static void __iomem *g_infra_peri_debug4;
static void __iomem *g_infra_peri_debug5;
static void __iomem *g_infracfg_ao;
static void __iomem *g_dbgtop;
static void __iomem *g_sleep;
static void __iomem *g_toprgu;
#if MT_GPUFREQ_ASENSOR_ENABLE
static void __iomem *g_cpe_controller_sensor;
static void __iomem *g_sensor_network_0_base;
static void __iomem *g_cpe_controller_mcu_base;
#endif

static struct pwr_status g_mfg_clk_pwr_status =
	GATE_PWR_STAT(PWR_STATUS_OFS, PWR_STATUS_2ND_OFS,
		INV_OFS, 0x3F, 0x3F);
static const struct mtk_pll_data g_mfg_plls[] = {
	PLL_PWR(CLK_MFG_AO_MFGPLL1, "mfg_ao_mfgpll1",
		MFGPLL1_CON0_OFS/*base*/,
		MFGPLL1_CON0_OFS, BIT(0)/*en*/,
		MFGPLL1_CON3_OFS/*pwr*/, 0/*flags*/,
		MFGPLL1_CON1_OFS, 24/*pd*/,
		MFGPLL1_CON1_OFS, 0, 22/*pcw*/,
		&g_mfg_clk_pwr_status),
	PLL_PWR(CLK_MFG_AO_MFGPLL4, "mfg_ao_mfgpll4",
		MFGPLL4_CON0_OFS/*base*/,
		MFGPLL4_CON0_OFS, BIT(0)/*en*/,
		MFGPLL4_CON3_OFS/*pwr*/, 0/*flags*/,
		MFGPLL4_CON1_OFS, 24/*pd*/,
		MFGPLL4_CON1_OFS, 0, 22/*pcw*/,
		&g_mfg_clk_pwr_status),
};

unsigned int mt_gpufreq_get_shader_present(void)
{
	static int shader_present = -1;
	unsigned int segment_id = 0;

	if (shader_present != -1)
		return shader_present;

	segment_id = __mt_gpufreq_get_segment_id();

	switch (segment_id) {
	case MT6877_SEGMENT:
		shader_present = MT_GPU_SHADER_PRESENT_4;
		break;
	case MT6877T_SEGMENT:
		shader_present = MT_GPU_SHADER_PRESENT_4;
		break;
	default:
		shader_present = MT_GPU_SHADER_PRESENT_4;
		gpufreq_pr_info("@%s: invalid segment_id(%d)\n",
				__func__, segment_id);
	}
	gpufreq_pr_info("@%s: segment_id=%d shader_present=%d\n",
			__func__, segment_id, shader_present);

	return shader_present;
}

void mt_gpufreq_wdt_reset(void)
{
	unsigned int val;

	gpufreq_pr_info("%s\n", __func__);

	if (g_toprgu_base) {
		writel(0x88000004, g_toprgu_base + 0x18);
		val = readl(g_toprgu_base + 0x18);

		gpufreq_pr_info("@%s: gpu info 0x%x:0x%08x\n",
				__func__, 0x10007018, val);
		udelay(1);

		writel(0x88000000, g_toprgu_base + 0x18);
		val = readl(g_toprgu_base + 0x18);

		gpufreq_pr_info("@%s: gpu info 0x%x:0x%08x\n",
				__func__, 0x10007018, val);
		udelay(1);
	} else {
		gpufreq_pr_info("@%s: no g_toprgu_base\n", __func__);
	}
}

void mt_gpufreq_dump_infra_status(void)
{
	int i;
	unsigned int start, offset, val;

	gpufreq_pr_info("[GPU_DFD] ====\n");
	gpufreq_pr_info("[GPU_DFD] mfgpll=%d freq=%d vgpu=%d vsram_gpu=%d\n",
			mt_get_subsys_freq(FM_MFGPLL1),
			g_cur_opp_freq,
			g_cur_opp_vgpu,
			g_cur_opp_vsram_gpu);

	gpufreq_pr_info("[GPU_DFD] g_aging_table_id=%d, g_aging_most_agrresive = %d\n",
			g_aging_table_id,
			g_aging_most_agrresive);

	for (i = g_segment_max_opp_idx; i <= g_segment_min_opp_idx; i++) {
		gpufreq_pr_info("[%02d] freq = %d, vgpu = %d, vsram = %d, posdiv = %d, aging = %d\n",
			i - g_segment_max_opp_idx,
			g_opp_table[i].gpufreq_khz,
			g_opp_table[i].gpufreq_vgpu,
			g_opp_table[i].gpufreq_vsram,
			1 << g_opp_table[i].gpufreq_post_divider,
			g_opp_table[i].gpufreq_aging_margin);
	}

	// 0x10000000
	if (g_topckgen_base) {
		gpufreq_pr_info("[GPU_DFD] mux =0x%08X\n",
			readl(g_topckgen_base + 0x120));
	}

	// 0x1020E000
	if (g_infracfg_base) {
		gpufreq_pr_info("[GPU_DFD] infra info 0x%x:0x%08x\n",
				0x1020E810,
				readl(g_infracfg_base + 0x810));

		gpufreq_pr_info("[GPU_DFD] infra info 0x%x:0x%08x\n",
				0x1020E814,
				readl(g_infracfg_base + 0x814));
	}

	// 0x10023000
	if (g_infra_peri_debug1) {
		gpufreq_pr_info("[GPU_DFD] infra info 0x%x:0x%08x\n",
				0x10023000,
				readl(g_infra_peri_debug1 + 0x000));

		if (g_dfd_force_dump == 1 || g_dfd_force_dump == 3) {
			start = 0x400;
			for (offset = start; offset <= 0x434; offset += 4) {
				val = readl(g_infra_peri_debug1 + offset);
				gpufreq_pr_info("[GPU_DFD] infra info 0x%x:0x%08x\n",
						0x10023000 + offset, val);
			}
		}
	}

	// 0x1002B000
	if (g_infra_peri_debug2) {
		gpufreq_pr_info("[GPU_DFD] infra info 0x%x:0x%08x\n",
				0x1002B000,
				readl(g_infra_peri_debug2 + 0x000));

		if (g_dfd_force_dump == 1 || g_dfd_force_dump == 3) {
			start = 0x408;
			for (offset = start; offset <= 0x438; offset += 4) {
				val = readl(g_infra_peri_debug2 + offset);
				gpufreq_pr_info("[GPU_DFD] infra info 0x%x:0x%08x\n",
						0x1002B000 + offset, val);
			}
		}
	}

	// 0x1002E000
	if (g_infra_peri_debug3) {
		gpufreq_pr_info("[GPU_DFD] infra info 0x%x:0x%08x\n",
				0x1002E000,
				readl(g_infra_peri_debug3 + 0x000));

		if (g_dfd_force_dump == 1 || g_dfd_force_dump == 3) {
			start = 0x408;
			for (offset = start; offset <= 0x418; offset += 4) {
				val = readl(g_infra_peri_debug3 + offset);
				gpufreq_pr_info("[GPU_DFD] infra info 0x%x:0x%08x\n",
						0x1002E000 + offset, val);
			}
		}
	}

	// 0x10040000
	if (g_infra_peri_debug4) {
		gpufreq_pr_info("[GPU_DFD] infra info 0x%x:0x%08x\n",
				0x10040000,
				readl(g_infra_peri_debug4 + 0x000));

		if (g_dfd_force_dump == 1 || g_dfd_force_dump == 3) {
			start = 0x408;
			for (offset = start; offset <= 0x420; offset += 4) {
				val = readl(g_infra_peri_debug4 + offset);
				gpufreq_pr_info("[GPU_DFD] infra info 0x%x:0x%08x\n",
						0x10040000 + offset, val);
			}
		}
	}

	// 0x10042000
	if (g_infra_peri_debug5) {
		gpufreq_pr_info("[GPU_DFD] infra info 0x%x:0x%08x\n",
				0x10042000,
				readl(g_infra_peri_debug5 + 0x000));

		if (g_dfd_force_dump == 1 || g_dfd_force_dump == 3) {
			start = 0x408;
			for (offset = start; offset <= 0x44c; offset += 4) {
				val = readl(g_infra_peri_debug5 + offset);
				gpufreq_pr_info("[GPU_DFD] infra info 0x%x:0x%08x\n",
						0x10042000 + offset, val);
			}
		}
	}

	// 0x10006000
	if (g_sleep) {
		gpufreq_pr_info("[GPU_DFD] pwr info 0x%X:0x%08X %08X %08X %08X\n",
				0x10006000 + 0xE80,
				readl(g_sleep + 0xE80),
				readl(g_sleep + 0xE84),
				readl(g_sleep + 0xE88),
				readl(g_sleep + 0xE8C));

		gpufreq_pr_info("[GPU_DFD] pwr info 0x%X:0x%08X %08X %08X %08X\n",
				0x10006000 + 0xE90,
				readl(g_sleep + 0xE90),
				readl(g_sleep + 0xE94),
				readl(g_sleep + 0xE98),
				readl(g_sleep + 0xE9C));

		gpufreq_pr_info("[GPU_DFD] pwr info 0x%X:0x%08X 0x%08X\n",
				0x10006000 + 0xEF8,
				readl(g_sleep + 0xEF8),
				readl(g_sleep + 0xEFC));
	}

	// 0x13FBF000
	if (g_mfg_base && g_cg_on) {
		if (g_dfd_force_dump == 1 || g_dfd_force_dump == 3) {
			start = 0x000;
			for (offset = start; offset <= 0xFFC; offset += 4) {
				gpufreq_pr_info("[GPU_DFD] mfg info 0x%x:0x%08x\n",
						0x13FBF000 + offset,
						readl(g_mfg_base + offset));
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

	if (kicker != KIR_ASENSOR) {
		if (limit_idx > g_segment_min_opp_idx ||
					limit_idx < g_segment_max_opp_idx) {
			ret |= (1 << 1);
			gpufreq_pr_info("@%s: out of segment opp range, %d (%d)\n",
					__func__, limit_idx,
					g_segment_min_opp_idx);
		}
		if (g_DVFS_is_paused_by_asensor)
			ret |= (1 << 2);
	}

	/* if /proc/gpufreq/gpufreq_fixed_freq_volt fix freq and volt */
	if (g_fixed_freq_volt_state)
		ret |= (1 << 3);

	/* the same freq && volt */
	if (g_cur_opp_freq == g_opp_table[limit_idx].gpufreq_khz &&
			g_cur_opp_vgpu == g_opp_table[limit_idx].gpufreq_vgpu)
		ret |= (1 << 4);

	if (g_power_count == 0)
		ret |= (1 << 5);

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

	gpufreq_pr_debug("@%s: kicker=%d t_upper_idx=%d t_lower_idx=%d\n",
			__func__, kicker, t_upper_idx, t_lower_idx);

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

	gpufreq_pr_debug("@%s: kicker=%d t_upper_enable=%d t_lower_enable=%d\n",
			__func__, kicker, t_upper_enable, t_lower_enable);

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

#if MT_GPUFREQ_SHADER_PWR_CTL_WA
void mt_gpufreq_clock_parking_lock(unsigned long *pFlags)
{
	spin_lock_irqsave(&mt_gpufreq_clksrc_parking_lock, *pFlags);
}
void mt_gpufreq_clock_parking_unlock(unsigned long *pFlags)
{
	spin_unlock_irqrestore(&mt_gpufreq_clksrc_parking_lock, *pFlags);
}
unsigned int mt_gpufreq_clock_parking(int clksrc)
{
	/*
	 * This function will be called under the Interrupt-Handler,
	 * so can't implement any mutex-lock behaviors
	 * (that will result the sleep/schedule operations).
	 */

	if (clksrc == CLOCK_MAIN) {
		/* Switch to <&topckgen_clk CLK_TOP_MFGPLL1> */
		/* 0. set reg bit [18] */
		writel(0x00040000, g_topckgen_base + 0x124);

		/* Switch to <&clk26m>: 26MHz */
		/* 1. clr reg bit [19] */
		writel(0x00080000, g_topckgen_base + 0x128);

	} else if (clksrc == CLOCK_SUB) {
		/* Switch to <&topckgen_clk CLK_TOP_MFGPLL4> */
		/* 0. set reg bit [19] */
		writel(0x00080000, g_topckgen_base + 0x124);

		/* Switch to <&topckgen_clk CLK_TOP_MFG_INTERNAL1> */
		/* 1. clr reg bit [18] */
		writel(0x00040000, g_topckgen_base + 0x128);

	} else if (clksrc == CLOCK_SUB2) {
		/* Switch to <&clk26m>: 26MHz */
		/* 0. clr reg bit [19] */
		writel(0x00080000, g_topckgen_base + 0x128);

		/* Switch to <&topckgen_clk CLK_TOP_MFG_INTERNAL1> */
		/* 1. clr reg bit [18] */
		writel(0x00040000, g_topckgen_base + 0x128);
	}

	gpufreq_pr_logbuf("%s, clksrc: %d ... [MUX] = 0x%08X\n",
		__func__, clksrc, readl(g_topckgen_base + 0x120));

	return 0;
}
#endif

void mt_gpufreq_set_timestamp(void)
{
	gpufreq_pr_debug("@%s\n", __func__);

	/* timestamp will be used by clGetEventProfilingInfo
	 * 0x13fb_f130
	 * [0] : write 1 to enable timestamp register
	 * [1] : 0: timer from internal module
	 *     : 1: timer from soc
	 */
	writel(0x00000003, g_mfg_base + 0x130);
}

void mt_gpufreq_set_gpm(void)
{
#if MT_GPUFREQ_GPM_ENABLE
	gpufreq_pr_debug("@%s\n", __func__);

	/* MFG_I2M_PROTECTOR_CFG_00 (0x13fb_ff60) = 0x20700316 */
	writel(0x20700316, g_mfg_base + 0xf60);

	/* MFG_I2M_PROTECTOR_CFG_01 (0x13fb_ff64) = 0x1000000C */
	writel(0x1000000C, g_mfg_base + 0xf64);

	/* MFG_I2M_PROTECTOR_CFG_02 (0x13fb_ff68) = 0x01010802 */
	writel(0x01010802, g_mfg_base + 0xf68);

	/* Wait 1us */
	udelay(1);

	/* MFG_I2M_PROTECTOR_CFG_00 (0x13fb_ff60) = 0x20700317 */
	writel(0x20700317, g_mfg_base + 0xf60);
#endif
}

static void __mt_gpufreq_get_asensor_val(u32 *val1, u32 *val2)
{
#if MT_GPUFREQ_ASENSOR_ENABLE
	__mt_gpufreq_disable_by_asensor();

	gpufreq_pr_debug("@%s\n", __func__);

	if (g_mfg_base) {
		/* enable CPE pclk cg  (0x13fb_ff80) = 0x1  */
		writel(0x1, g_mfg_base + 0xf80);

		/* enable sensor bclk cg (0x13fb_ff98) 0x1 */
		writel(0x1, g_mfg_base + 0xf98);
	}

	if (g_cpe_controller_mcu_base) {
		/* CPE_CTRL_MCU_REG_CPEMONCTL (window setting) */
		/* 0x13fb_9c00  = 0x09010103 */
		writel(0x0901031f, g_cpe_controller_mcu_base + 0x0);

		/* CPE_CTRL_MCU_REG_CEPEN (Enable CPE) */
		/* 0x13fb9c04 = 0xff */
		writel(0xff, g_cpe_controller_mcu_base + 0x4);
	}

	/* Wait 50us */
	udelay(50);

	if (g_cpe_controller_sensor) {
		/* Read 0x13FB7000 [31:16] & 0x13FB7004 [15:0] */
		*val1 = readl(g_cpe_controller_sensor + 0x0);
		*val2 = readl(g_cpe_controller_sensor + 0x4);

		gpufreq_pr_info("@%s: val1 = 0x%08x, val2 = 0x%08x\n",
				__func__, *val1, *val2);

		*val1 = (*val1 & 0xFFFF0000) >> 16;
		*val2 = (*val2 & 0xFFFF);
	}

	__mt_gpufreq_enable_by_asensor();
#endif
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

	/* [K] MFG_ASYNC_CON 0x13FB_F020 [22] MEM0_MST_CG_ENABLE = 0x1 */
	/* [B] MFG_ASYNC_CON 0x13FB_F020 [23] MEM0_SLV_CG_ENABLE = 0x1 */
	/* [K] MFG_ASYNC_CON 0x13FB_F020 [24] MEM1_MST_CG_ENABLE = 0x1 */
	/* [B] MFG_ASYNC_CON 0x13FB_F020 [25] MEM1_SLV_CG_ENABLE = 0x1 */
	val = readl(g_mfg_base + 0x20);
	val |= (1UL << 22);
	val |= (1UL << 23);
	val |= (1UL << 24);
	val |= (1UL << 25);
	writel(val, g_mfg_base + 0x20);

	/* [A] MFG_GLOBAL_CON 0x13FB_F0B0 [8] MFG_SOC_IN_AXI_FREE_RUN = 0x0 */
	/* [C] MFG_GLOBAL_CON 0x13FB_F0B0 [20] RG_FAXI_CK_SOC_IN_FREE_RUN = 0x0 */
	/* [I] MFG_GLOBAL_CON 0x13FB_F0B0 [21] DVFS_HINT_CG_EN = 0x0 */
	/* [L] MFG_GLOBAL_CON 0x13FB_F0B0 [10] GPU_CLK_FREE_RUN = 0x0 */
	val = readl(g_mfg_base + 0xB0);
	val &= ~(1UL << 8);
	val &= ~(1UL << 20);
	val &= ~(1UL << 21);
	val &= ~(1UL << 10);
	writel(val, g_mfg_base + 0xB0);

	/* [G] MFG_DCM_CON 0x13FB_F010 [15] BG3D_DCM_ENABLE = 0x1 */
	/* [G] MFG_DCM_CON 0x13FB_F010 [6:0] BG3D_DCM_DBC_CNT = 0x0111111 */
	val = readl(g_mfg_base + 0x10);
	val |= (1UL << 15);
	val &= ~(1UL << 6);
	val |= (1UL << 5);
	val |= (1UL << 4);
	val |= (1UL << 3);
	val |= (1UL << 2);
	val |= (1UL << 1);
	val |= (1UL << 0);
	writel(val, g_mfg_base + 0x10);
}

static void mt_gpufreq_cg_control(enum mt_power_state power)
{
	gpufreq_pr_debug("@%s: power=%d", __func__, power);

	if (power == POWER_ON) {
		if (clk_prepare_enable(g_clk->subsys_bg3d))
			gpufreq_pr_info("@%s: failed when enable subsys-bg3d\n",
					__func__);
		g_cg_on++;
		mt_gpufreq_external_cg_control();
	} else {
		clk_disable_unprepare(g_clk->subsys_bg3d);
		g_cg_on--;
	}
}

static void mt_gpufreq_mtcmos_control(enum mt_power_state power)
{
	unsigned int shader_present = 0;

	gpufreq_pr_debug("@%s: power=%d\n", __func__, power);

	shader_present = mt_gpufreq_get_shader_present();

	if (power == POWER_ON) {
		if (clk_prepare_enable(g_clk->mtcmos_mfg0))
			gpufreq_pr_info("@%s: failed when enable mtcmos_mfg0\n",
					__func__);

		if (clk_prepare_enable(g_clk->mtcmos_mfg1))
			gpufreq_pr_info("@%s: failed when enable mtcmos_mfg1\n",
					__func__);

		if (shader_present & MFG2_SHADER_STACK0)
			if (clk_prepare_enable(g_clk->mtcmos_mfg2))
				gpufreq_pr_info("@%s: failed when enable mtcmos_mfg2\n",
						__func__);

		if (shader_present & MFG3_SHADER_STACK2)
			if (clk_prepare_enable(g_clk->mtcmos_mfg3))
				gpufreq_pr_info("@%s: failed when enable mtcmos_mfg3\n",
						__func__);

		if (shader_present & MFG4_SHADER_STACK4)
			if (clk_prepare_enable(g_clk->mtcmos_mfg4))
				gpufreq_pr_info("@%s: failed when enable mtcmos_mfg4\n",
						__func__);

		if (shader_present & MFG5_SHADER_STACK6)
			if (clk_prepare_enable(g_clk->mtcmos_mfg5))
				gpufreq_pr_info("@%s: failed when enable mtcmos_mfg5\n",
						__func__);
		g_mtcmos_on++;
	} else {
		if (shader_present & MFG5_SHADER_STACK6)
			clk_disable_unprepare(g_clk->mtcmos_mfg5);

		if (shader_present & MFG4_SHADER_STACK4)
			clk_disable_unprepare(g_clk->mtcmos_mfg4);

		if (shader_present & MFG3_SHADER_STACK2)
			clk_disable_unprepare(g_clk->mtcmos_mfg3);

		if (shader_present & MFG2_SHADER_STACK0)
			clk_disable_unprepare(g_clk->mtcmos_mfg2);

		clk_disable_unprepare(g_clk->mtcmos_mfg1);
		clk_disable_unprepare(g_clk->mtcmos_mfg0);
		g_mtcmos_on--;
	}
}

static void mt_gpufreq_buck_control(enum mt_power_state power)
{
	gpufreq_pr_debug("@%s: power=%d", __func__, power);

	if (power == POWER_ON) {
		if (regulator_enable(g_pmic->reg_vsram_gpu)) {
			gpufreq_pr_info("@%s: fail tp enable VSRAM_GPU\n",
					__func__);
			return;
		}
		if (regulator_enable(g_pmic->reg_vgpu)) {
			gpufreq_pr_info("@%s: fail to enable VGPU\n",
					__func__);
			return;
		}
		g_buck_on++;
	} else {
		if (regulator_disable(g_pmic->reg_vgpu)) {
			gpufreq_pr_info("@%s: fail to disable VGPU\n",
					__func__);
			return;
		}
		if (regulator_disable(g_pmic->reg_vsram_gpu)) {
			gpufreq_pr_info("@%s: fail to disable VSRAM_GPU\n",
					__func__);
			return;
		}
		g_buck_on--;
	}
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
		gpufreq_pr_info("[GPU_DFD] software_trigger failed, %0x:%08x\n",
				0x10001600, val);
	else if (g_dfd_force_dump)
		gpufreq_pr_info("[GPU_DFD] software_trigger state, %0x:%08x\n",
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
	//0x1000D060
	gpu_assert(!(status & 0x80000), GPU_DFD_PROBE_TRIGGERED,
		"gpu dfd is triggered at probe\n"
		"dfd status 0x%x, WDT_STA 0x%x, WDT_REQ_MODE 0x%x\n"
		"pwr info 0x%x, 0x1000D060 0x%x\n",
		status, readl(g_toprgu + 0x00C), readl(g_toprgu + 0x030),
		readl(g_sleep + 0xEF8), readl(g_dbgtop + 0x060));

#endif
}

static int __mt_gpufreq_is_dfd_triggered(void)
{
#if MT_GPUFREQ_DFD_ENABLE
	unsigned int status = readl(g_infracfg_ao + 0x600);

#if MT_GPUFREQ_DFD_DEBUG
	gpufreq_pr_info("[GPU_DFD] dfd status 0x%x\n", __func__, status);
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
		gpufreq_pr_info("[GPU_DFD] mtk_dbgtop_mfg_pwr_on(%d) fail=0x%0x ret=%d retry_remain=%d\n",
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

		writel(0x000138A3, g_mfg_base + 0xA04);
		writel(0x0001829F, g_mfg_base + 0xA08);
		writel(0x00100027, g_mfg_base + 0xA0C);
		writel(0x00000000, g_mfg_base + 0xA10);
		writel(0x00000000, g_mfg_base + 0xA14);
		writel(0x00000000, g_mfg_base + 0xA18);
		writel(0x00000000, g_mfg_base + 0xA1C);
		writel(0x00000000, g_mfg_base + 0xA20);
		writel(0x00000000, g_mfg_base + 0xA24);
		writel(0x00000000, g_mfg_base + 0xA28);
		writel(0x00000000, g_mfg_base + 0xA2C);
		// [8] enable
		writel(0x0F001100, g_mfg_base + 0xA00);

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

	gpufreq_pr_logbuf("@%s: power=%d g_power_count=%d (cg=%d, mtcmos=%d, buck=%d)\n",
			__func__, power, g_power_count, cg, mtcmos, buck);

	if (__mt_gpufreq_is_dfd_triggered()) {
#if MT_GPUFREQ_DFD_DEBUG
		unsigned int dfd_status = readl(g_infracfg_ao + 0x600);
		unsigned int rgu_pwr = readl(g_dbgtop + 0x060);

		gpufreq_pr_info("[GPU_DFD] power %d, dfd_status 0x%x, rgu_pwr 0x%x\n",
				__func__, power, dfd_status, rgu_pwr);

		gpufreq_pr_info("[GPU_DFD] power %d, pwr info 0x%X:0x%08X\n",
				power,
				0x10006000 + 0xEF8,
				readl(g_sleep + 0xEF8));
#endif
		if (g_probe_done) {
			gpufreq_pr_info("@%s: power=%d g_power_count=%d, skip by dfd_trigger\n",
					__func__, power, g_power_count);
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

		/*
		 * MFG_INTERNAL2_SEL is CLOCK_SUB by default,
		 * switch to CLOCK_MAIN every time power on
		 */
		if (g_power_count == 1)
			__mt_gpufreq_switch_to_clksrc(CLOCK_MAIN);

#if MT_GPUFREQ_DFD_ENABLE
		__mt_gpufreq_config_dfd(true);
#endif

		gpu_dvfs_vgpu_footprint(GPU_DVFS_VGPU_STEP_4);
#ifdef CONFIG_MTK_GPU_SUPPORT
		mtk_notify_gpu_power_change(1);
#endif
	} else {
#if MT_GPUFREQ_DFD_ENABLE
		__mt_gpufreq_config_dfd(false);
#endif
#ifdef CONFIG_MTK_GPU_SUPPORT
		mtk_notify_gpu_power_change(0);
#endif

		if (g_power_count == 0)
			__mt_gpufreq_switch_to_clksrc(CLOCK_SUB);

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

static void __mt_gpufreq_enable_by_asensor(void)
{
	__mt_gpufreq_vgpu_set_mode(REGULATOR_MODE_NORMAL); /* NORMAL */

	mt_gpufreq_power_control(POWER_OFF, CG_OFF,
					MTCMOS_OFF, BUCK_OFF);

	mt_gpufreq_update_limit_idx(KIR_ASENSOR,
		LIMIT_IDX_DEFAULT,
		LIMIT_IDX_DEFAULT);

	g_DVFS_is_paused_by_asensor = false;

	gpufreq_pr_debug("@%s: g_DVFS_is_paused_by_asensor=%d\n",
			__func__, g_DVFS_is_paused_by_asensor);
}

static void __mt_gpufreq_disable_by_asensor(void)
{
	struct opp_table_info *opp_table = g_opp_table;
	unsigned int i = 0;
	unsigned int target_idx = 0;

	for (i = 0; i < NUM_OF_OPP_IDX; i++) {
		if (opp_table[i].gpufreq_vgpu <= ASENSOR_DISABLE_VOLT) {
			target_idx = i;
			break;
		}
	}

	mt_gpufreq_update_limit_idx(KIR_ASENSOR, target_idx, target_idx);

	g_DVFS_is_paused_by_asensor = true;

	gpufreq_pr_debug("@%s: g_DVFS_is_paused_by_asensor=%d target_idx=%d(%d)\n",
			__func__, g_DVFS_is_paused_by_asensor, target_idx,
			opp_table[target_idx].gpufreq_vgpu);

	mt_gpufreq_power_control(POWER_ON, CG_ON, MTCMOS_ON, BUCK_ON);

	mt_gpufreq_target(target_idx, KIR_ASENSOR);

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

		gpufreq_pr_debug("@%s: PTPOD opp_table[%02d] vgpu=%d vsram=%d\n",
				__func__, i,
				g_opp_table[i].gpufreq_vgpu,
				g_opp_table[i].gpufreq_vsram);
	}

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

static void __mt_gpufreq_update_table_by_asensor(void)
{
	int aging_table_id;
	u32 efuse_val1, efuse_val2;
	u32 a_t0_lvt_rt, a_t0_ulvt_rt;
	u32 a_tn_lvt_cnt, a_tn_ulvt_cnt;
	int tj, tj1, tj2;
	bool error_bit = false;
	int adiff, adiff1, adiff2;
	int i;
	int leak_power;

	if (g_aging_table_id != -1)
		return;

	/*
	 * 1. Get efuse val : 0x11F10A90(181) & 0x11F10A94(182)
	 * A_T0_LVT_RT   : 0x11F10A90 [9:0]
	 * A_T0_ULVT_RT  : 0x11F10A90 [19:10]
	 * A_shift_error : 0x11F10A90 [31]
	 * efuse_error   : 0x11F10A94 [31]
	 */

	efuse_val1 = get_devinfo_with_index(181);
	efuse_val2 = get_devinfo_with_index(182);

	a_t0_lvt_rt = (efuse_val1 & 0x3FF);
	a_t0_ulvt_rt = ((efuse_val1 & 0xFFC00) >> 10);

	error_bit = efuse_val1 >> 31;

	/*
	 * 2. Get a sensor counter val
	 * SN trigger A_Tn_LVT counter value  : 0x13FB7000 [31:16]
	 * SN trigger A_Tn_ULVT counter value : 0x13FB7004 [15:0]
	 */

	__mt_gpufreq_get_asensor_val(&a_tn_lvt_cnt, &a_tn_ulvt_cnt);

	/* 3. Get thermal zone temp */
#ifdef CONFIG_THERMAL
	tj1 = get_immediate_tslvts3_0_wrap() / 1000;
	tj2 = get_immediate_tslvts3_1_wrap() / 1000;
#else
	tj1 = 40;
	tj2 = 40;
#endif

	tj = (((MAX(tj1, tj2) - 25) * 5) / 40) + 1;

	/* 4. Calculate adiff (aging diff)
	 * (EfuseRead + (((MAX(tj1, tj2) - 25) * 5) / 40) + 1) - SNRead
	 */

	adiff1 = a_t0_lvt_rt + tj - a_tn_lvt_cnt;
	adiff2 = a_t0_ulvt_rt + tj - a_tn_ulvt_cnt;
	adiff = MAX(adiff1, adiff2);

	/* apply volt=0.8V, temp=30C to get leakage information.
	 * For leakage < 16 (mW), DISABLE aging reduction.
	 */
	leak_power = mt_spower_get_leakage(MTK_SPOWER_GPU, (80000 / 100), 30);

	/* 5. Deside aging_table_id */
	if (efuse_val1 == 0 || efuse_val2 == 0)
		aging_table_id = 3;
	else if (MIN(tj1, tj2) < 25)
		aging_table_id = 3;
	else if (error_bit)
		aging_table_id = 3;
	else if (leak_power < 16)
		aging_table_id = 3;
	else if (adiff < MT_GPUFREQ_AGING_GAP0)
		aging_table_id = 3;
	else if ((adiff >= MT_GPUFREQ_AGING_GAP0)
		&& (adiff < MT_GPUFREQ_AGING_GAP1))
		aging_table_id = 0;
	else if ((adiff >= MT_GPUFREQ_AGING_GAP1)
		&& (adiff < MT_GPUFREQ_AGING_GAP2))
		aging_table_id = 1;
	else if ((adiff >= MT_GPUFREQ_AGING_GAP2)
		&& (adiff < MT_GPUFREQ_AGING_GAP3))
		aging_table_id = 2;
	else if (adiff >= MT_GPUFREQ_AGING_GAP3)
		aging_table_id = 3;
	else {
		gpufreq_pr_debug("@%s: non of the condition is true\n",
				__func__);
		aging_table_id = 3;
	}

	/* 6. Check the MostAgrresive setting */
	aging_table_id = MAX(aging_table_id, g_aging_most_agrresive);

	/* 7. Use worst case if aging load */
#if defined(AGING_LOAD)
	gpufreq_pr_info("@%s: AGING load\n", __func__);
	aging_table_id = 0;
#endif

	g_aging_table_id = aging_table_id;
	g_asensor.efuse_val1 = efuse_val1;
	g_asensor.efuse_val2 = efuse_val2;
	g_asensor.a_t0_lvt_rt = a_t0_lvt_rt;
	g_asensor.a_t0_ulvt_rt = a_t0_ulvt_rt;
	g_asensor.a_tn_lvt_cnt = a_tn_lvt_cnt;
	g_asensor.a_tn_ulvt_cnt = a_tn_ulvt_cnt;
	g_asensor.tj1 = tj1;
	g_asensor.tj2 = tj2;
	g_asensor.adiff1 = adiff1;
	g_asensor.adiff2 = adiff2;
	g_asensor.leak_power = leak_power;

	/* 8. Update aginge value */
	for (i = 0; i < NUM_OF_OPP_IDX; i++)
		g_opp_table[i].gpufreq_aging_margin =
		g_aging_table[aging_table_id][i];

	gpufreq_pr_info("@%s: efuse_val1 = 0x%08x, efuse_val2 = 0x%08x, error_bit = %d, leak_power = %d, a_t0_lvt_rt = %d, a_t0_ulvt_rt = %d, a_tn_lvt_cnt = %d, a_tn_ulvt_cnt = %d\n",
				__func__,
				efuse_val1, efuse_val2, error_bit, leak_power,
				a_t0_lvt_rt, a_t0_ulvt_rt,
				a_tn_lvt_cnt, a_tn_ulvt_cnt);

	gpufreq_pr_info("@%s: tj = %d, tj1 = %d, tj2 = %d, adiff = %d, adiff1 = %d, adiff2 = %d, aging_table_id = %d, g_aging_most_agrresive = %d\n",
				__func__,
				tj, tj1, tj2,
				adiff, adiff1, adiff2,
				aging_table_id, g_aging_most_agrresive);
}

static void mt_gpufreq_apply_aging(bool apply)
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

		gpufreq_pr_debug("@%s: apply=%d, [%02d] vgpu=%d, vsram_gpu=%d\n",
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

	__mt_gpufreq_update_table_by_asensor();

	mutex_lock(&mt_gpufreq_lock);

	gpufreq_pr_debug("@%s: PTPOD update OPP table to given voltage\n",
			__func__);

	for (i = 0; i < array_size; i++) {
		target_idx = mt_gpufreq_get_ori_opp_idx(i);
		g_opp_table[target_idx].gpufreq_vgpu = pmic_volt[i];
		g_opp_table[target_idx].gpufreq_vsram =
			__mt_gpufreq_get_vsram_gpu_by_vgpu(pmic_volt[i]);
	}
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
	if (mt_gpufreq_bringup())
		return false;

	if (IS_ERR(g_pmic->reg_vgpu) || IS_ERR(g_pmic->reg_vsram_gpu)) {
		gpufreq_pr_info("@%s: VGPU(%lu)/VSRAM_GPU(%ld) was not initialized\n",
				__func__,
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

/* API : provide gpu lkg for swpm */
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

	gpufreq_pr_debug("@%s: limited_power=%d limited_idx=%d\n",
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

	gpufreq_pr_debug("@%s: limited_freq=%d limited_idx=%d\n",
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

	gpufreq_pr_debug("@%s: battery_oc_level=%d batt_oc_limited_idx=%d\n",
			__func__,
			battery_oc_level,
			batt_oc_limited_idx);
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

	gpufreq_pr_debug("@%s: battery_percent_level=%d batt_percent_limited_idx=%d\n",
			__func__,
			battery_percent_level,
			batt_percent_limited_idx);
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

	gpufreq_pr_debug("@%s: low_battery_level=%d low_batt_limited_idx=%d\n",
			__func__, low_battery_level, low_batt_limited_idx);
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

	gpufreq_pr_debug("@%s: limited_power=%d thermal_limited_idx=%d\n",
			__func__, limited_power, thermal_limited_idx);

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

	gpufreq_pr_debug("@%s: limited_power=%d pbm_limited_idx=%d\n",
			__func__, limited_power, pbm_limited_idx);

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
	unsigned int efuse_id;
	static int segment_id = -1;

	if (segment_id != -1)
		return segment_id;

	efuse_id = (get_devinfo_with_index(7) & 0xFF);

	switch (efuse_id) {
	case 0x01:
		segment_id = MT6877_SEGMENT;    /* 5G-5 */
		break;
	case 0x02:
		segment_id = MT6877T_SEGMENT;    /* 5G-5+ */
		break;
	default:
		segment_id = MT6877_SEGMENT;
		gpufreq_pr_info("@%s: invalid efuse_id(0x%x)\n",
				__func__, efuse_id);
	}

	gpufreq_pr_info("@%s: efuse_id=0x%x segment_id=%d\n",
			__func__, efuse_id, segment_id);

	return segment_id;
}

static struct opp_table_info *__mt_gpufreq_get_segment_table(void)
{
	unsigned int efuse_id;

	efuse_id = ((get_devinfo_with_index(183) >> 14) & 0x3);
	switch (efuse_id) {
	case 0x0: // EFUSE 0x11F105F0[15:14] = 2’b00
		return g_opp_table_segment_1;
	case 0x1: // EFUSE 0x11F105F0[15:14] = 2’b01
		return g_opp_table_segment_3;
	case 0x2: // EFUSE 0x11F105F0[15:14] = 2’b10
		return g_opp_table_segment_2;
	default:
		gpufreq_pr_debug("@%s: invalid efuse_id(0x%x)\n",
				__func__, efuse_id);
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
	seq_puts(m, "[0] disable\n");
	seq_puts(m, "[1] force dump + debug log\n");
	seq_puts(m, "[2] dump\n");
	seq_puts(m, "[3] debug log\n");
	seq_printf(m, "==> g_dfd_force_dump = %d\n", g_dfd_force_dump);
	mt_gpufreq_dump_infra_status();
#else
	seq_puts(m, "DFD is not support !\n");
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

	mt_gpufreq_power_control(POWER_ON, CG_ON, MTCMOS_ON, BUCK_ON);
	seq_printf(m, "(real) freq: %d, freq: %d, vgpu: %d, vsram_gpu: %d\n",
			mt_get_subsys_freq(FM_MFGPLL1),
			__mt_gpufreq_get_cur_freq(),
			__mt_gpufreq_get_cur_vgpu(),
			__mt_gpufreq_get_cur_vsram_gpu());
	mt_gpufreq_power_control(POWER_OFF, CG_OFF, MTCMOS_OFF, BUCK_OFF);

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

static int mt_gpufreq_aging_enable_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "g_aging_enable = %d\n\n", g_aging_enable);
	seq_printf(m, "g_aging_table_id = %d, g_aging_most_agrresive = %d\n",
		g_aging_table_id, g_aging_most_agrresive);
	seq_printf(m, "efuse_val1 = 0x%08x, efuse_val2 = 0x%08x\n"
					"a_t0_lvt_rt = %d, a_t0_ulvt_rt = %d\n"
					"a_tn_lvt_cnt = %d, a_tn_ulvt_cnt = %d\n"
					"tj1 = %d, tj2 = %d\n"
					"adiff1 = %d, adiff2 = %d, leak_power = %d\n",
					g_asensor.efuse_val1,
					g_asensor.efuse_val2,
					g_asensor.a_t0_lvt_rt,
					g_asensor.a_t0_ulvt_rt,
					g_asensor.a_tn_lvt_cnt,
					g_asensor.a_tn_ulvt_cnt,
					g_asensor.tj1,
					g_asensor.tj2,
					g_asensor.adiff1,
					g_asensor.adiff2,
					g_asensor.leak_power);

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
				mt_gpufreq_power_control(POWER_ON, CG_ON, MTCMOS_ON, BUCK_ON);
					mt_gpufreq_target(i, KIR_PROC);
				mt_gpufreq_power_control(POWER_OFF, CG_OFF, MTCMOS_OFF, BUCK_OFF);
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
			gpufreq_pr_debug("@%s: fixed_volt(%d) > VGPU_MAX_VOLT(%d)\n",
					__func__,
					fixed_volt,
					VGPU_MAX_VOLT);
			goto out;
		} else if (fixed_volt < VGPU_MIN_VOLT && fixed_volt > 0) {
			gpufreq_pr_debug("@%s: fixed_volt(%d) < VGPU_MIN_VOLT(%d)\n",
					__func__,
					fixed_volt,
					VGPU_MIN_VOLT);
			goto out;
		} else if (fixed_freq > POSDIV_2_MAX_FREQ) {
			gpufreq_pr_debug("@%s: fixed_freq(%d) > POSDIV_2_MAX_FREQ(%d)\n",
					__func__,
					fixed_freq,
					POSDIV_2_MAX_FREQ);
			goto out;
		} else if (fixed_freq < POSDIV_4_MIN_FREQ && fixed_freq > 0) {
			gpufreq_pr_debug("@%s: fixed_freq(%d) < POSDIV_4_MIN_FREQ(%d)\n",
					__func__,
					fixed_freq,
					POSDIV_4_MIN_FREQ);
			goto out;
		}

		mt_gpufreq_power_control(POWER_ON, CG_ON, MTCMOS_ON, BUCK_ON);
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
		mt_gpufreq_power_control(POWER_OFF, CG_OFF, MTCMOS_OFF, BUCK_OFF);
	}

out:
	return (ret < 0) ? ret : count;
}

/*
 * PROCFS : initialization
 */
PROC_FOPS_RW(gpufreq_opp_stress_test);
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
		gpufreq_pr_info("@%s: fail to create /proc/gpufreq !!!\n",
				__func__);
		return -ENOMEM;
	}

	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!proc_create(entries[i].name, 0660, dir, entries[i].fops))
			gpufreq_pr_info("@%s: create /proc/gpufreq/%s failed\n",
					__func__, entries[i].name);
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
	gpufreq_pr_debug("@%s: min_vsram_idx=%d\n", __func__, min_vsram_idx);

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
		gpufreq_pr_debug("@%s: g_opp_sb_idx_up[%d]=%d\n",
				__func__, i, g_opp_sb_idx_up[i]);
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
		gpufreq_pr_debug("@%s: g_opp_sb_idx_down[%d]=%d\n",
				__func__, i, g_opp_sb_idx_down[i]);
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
			"@%s: Clock switch failed, %d -> %d (target: %d)\n",
			__func__, freq_old,
			g_cur_opp_freq, freq_new);

	} else {
		__mt_gpufreq_clock_switch(freq_new);
		g_cur_opp_freq = __mt_gpufreq_get_cur_freq();

		gpu_assert(g_cur_opp_freq == freq_new,
			GPU_FREQ_EXCEPTION,
			"@%s: Clock switch failed, %d -> %d (target: %d)\n",
			__func__, freq_old,
			g_cur_opp_freq, freq_new);

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
		"end idx: %d -> %d, clk: %d, freq: %d, vgpu: %d, vsram_gpu: %d\n",
		idx_old, idx_new,
		mt_get_subsys_freq(FM_MFGPLL1),
		__mt_gpufreq_get_cur_freq(),
		__mt_gpufreq_get_cur_vgpu(),
		__mt_gpufreq_get_cur_vsram_gpu());

	__mt_gpufreq_kick_pbm(1);
}

/*
 * pcw calculation for clock switching
 * Fin is 26 MHz
 * VCO Frequency = Fin * N_INFO
 * MFGPLL output Frequency = VCO Frequency / POSDIV
 * N_INFO = MFGPLL output Frequency * POSDIV / FIN
 * N_INFO[21:14] = FLOOR(N_INFO, 8)
 */
static unsigned int __mt_gpufreq_calculate_pcw(
					unsigned int freq_khz,
					enum g_posdiv_power_enum posdiv_power)
{
	/*
	 * MFGPLL VCO range: 1.5GHz - 3.8GHz by divider 1/2/4/8/16,
	 * MFGPLL range: 125MHz - 3.8GHz,
	 * | VCO MAX | VCO MIN | POSDIV | PLL OUT MAX | PLL OUT MIN |
	 * |  3800   |  1500   |    1   |   3800MHz   |   1500MHz   |
	 * |  3800   |  1500   |    2   |   1900MHz   |    750MHz   |
	 * |  3800   |  1500   |    4   |    950MHz   |    375MHz   |
	 * |  3800   |  1500   |    8   |    475MHz   |  187.5MHz   |
	 * |  3800   |  2000   |   16   |  237.5MHz   |    125MHz   |
	 */
	unsigned int pcw = 0;

	/* only use posdiv 2 or 4 */
	if ((freq_khz >= POSDIV_4_MIN_FREQ) &&
		(freq_khz <= POSDIV_2_MAX_FREQ)) {
		pcw = (((freq_khz / TO_MHZ_HEAD * (1 << posdiv_power))
			<< DDS_SHIFT) /
			MFGPLL_FIN + ROUNDING_VALUE) / TO_MHZ_TAIL;
	} else {
		gpufreq_pr_info("@%s: out of range, freq_khz=%d\n",
				__func__, freq_khz);
	}

	return pcw;
}

static void __mt_gpufreq_switch_to_clksrc(enum g_clock_source_enum clksrc)
{
	int ret;

	ret = clk_prepare_enable(g_clk->clk_mux);
	if (ret)
		gpufreq_pr_info("@%s: enable clk_mux(MFG_INTERNAL2_SEL) failed: %d\n",
				__func__, ret);

	if (clksrc == CLOCK_MAIN) {
		ret = clk_set_parent(g_clk->clk_mux, g_clk->clk_main_parent);
		if (ret)
			gpufreq_pr_info("@%s: switch to main clock source failed: %d\n",
					__func__, ret);

	} else if (clksrc == CLOCK_SUB) {
		ret = clk_set_parent(g_clk->clk_mux, g_clk->clk_sub_parent);
		if (ret)
			gpufreq_pr_info("@%s: switch to sub clock source failed: %d\n",
					__func__, ret);

	} else {
		gpufreq_pr_info("@%s: clock source index is not valid, clksrc: %d\n",
				__func__, clksrc);
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

	gpufreq_pr_info("@%s: freq %d find no post divider\n", __func__, freq);
	return (freq > POSDIV_4_MAX_FREQ) ? POSDIV_POWER_2 : POSDIV_POWER_4;
}

static enum g_posdiv_power_enum __mt_gpufreq_get_curr_posdiv_power(void)
{
	unsigned long mfgpll;
	enum g_posdiv_power_enum posdiv_power;

	mfgpll = DRV_Reg32(MFGPLL1_CON1);

	posdiv_power = (mfgpll & (0x7 << POSDIV_SHIFT)) >> POSDIV_SHIFT;

	gpufreq_pr_logbuf(
		"%s : mfgpll: 0x%08x, posdiv_power: %d\n",
		__func__, mfgpll, posdiv_power);

	return posdiv_power;
}

static void __mt_gpufreq_clock_switch(unsigned int freq_new)
{
	enum g_posdiv_power_enum posdiv_power;
	enum g_posdiv_power_enum curr_posdiv_power;
	unsigned int pll, curr_freq, mfgpll, pcw;
	bool parking = false;
	int hopping = -1;

#if MT_GPUFREQ_SHADER_PWR_CTL_WA
	int ret;
	unsigned long flags;
#endif

	curr_freq = __mt_gpufreq_get_cur_freq();
	curr_posdiv_power = __mt_gpufreq_get_curr_posdiv_power();
	posdiv_power = __mt_gpufreq_get_posdiv_power(freq_new);
	pcw = __mt_gpufreq_calculate_pcw(freq_new, posdiv_power);
	/*
	 * MFGPLL1_CON1[31:31] = MFGPLL_SDM_PCW_CHG
	 * MFGPLL1_CON1[26:24] = MFGPLL_POSDIV
	 * MFGPLL1_CON1[21:0]  = MFGPLL_SDM_PCW (dds)
	 */
	pll = (0x80000000) | (posdiv_power << POSDIV_SHIFT) | pcw;

#ifndef CONFIG_MTK_FREQ_HOPPING
	/* force parking if FHCTL not ready */
	parking = true;
#else
	if (posdiv_power != curr_posdiv_power)
		parking = true;
	else
		parking = false;
#endif

	if (parking) {
#ifndef GLITCH_FREE
#if MT_GPUFREQ_SHADER_PWR_CTL_WA
		ret = clk_prepare_enable(g_clk->clk_mux);
		if (ret)
			gpufreq_pr_info("enable clk_mux(TOP_MUX_MFG) failed:%d\n", ret);

		ret = clk_prepare_enable(g_clk->clk_pll4);
		if (ret)
			gpufreq_pr_info("enable clk_pll4 failed:%d\n", ret);

		mt_gpufreq_clock_parking_lock(&flags);
		mt_gpufreq_clock_parking(CLOCK_SUB);
#else
		__mt_gpufreq_switch_to_clksrc(CLOCK_SUB);
#endif

		/*
		 * MFGPLL1_CON1[31:31] = MFGPLL_SDM_PCW_CHG
		 * MFGPLL1_CON1[26:24] = MFGPLL_POSDIV
		 * MFGPLL1_CON1[21:0]  = MFGPLL_SDM_PCW (dds)
		 */
		DRV_WriteReg32(MFGPLL1_CON1, pll);

		/* PLL spec */
		udelay(20);
#if MT_GPUFREQ_SHADER_PWR_CTL_WA
		mt_gpufreq_clock_parking(CLOCK_MAIN);
		mt_gpufreq_clock_parking_unlock(&flags);

		clk_disable_unprepare(g_clk->clk_pll4);
		clk_disable_unprepare(g_clk->clk_mux);
#else
		__mt_gpufreq_switch_to_clksrc(CLOCK_MAIN);
#endif
#else
		/*
		 * MFGPLL1_CON0[0] = RG_MFGPLL_EN
		 * MFGPLL1_CON0[4] = RG_MFGPLL_GLITCH_FREE_EN
		 */
		if (freq_new > curr_freq) {
			/* 1. change PCW (by hopping) */
			hopping = mt_dfs_general_pll(FH_GPU_PLL0, pcw);
			if (hopping != 0)
				gpufreq_pr_info("@%s: hopping failing: %d\n",
						__func__, hopping);
			/* 2. change POSDIV (by MFGPLL1_CON1) */
			pll = (DRV_Reg32(MFGPLL1_CON1) & 0xF8FFFFFF) |
				(posdiv_power << POSDIV_SHIFT);
			DRV_WriteReg32(MFGPLL1_CON1, pll);
			/* 3. wait 20us for MFGPLL Stable */
			udelay(20);
		} else {
			/* 1. change POSDIV (by MFGPLL1_CON1) */
			pll = (DRV_Reg32(MFGPLL1_CON1) & 0xF8FFFFFF) |
				(posdiv_power << POSDIV_SHIFT);
			DRV_WriteReg32(MFGPLL1_CON1, pll);
			/* 2. wait 20us for MFGPLL Stable */
			udelay(20);
			/* 3. change PCW (by hopping) */
			hopping = mt_dfs_general_pll(FH_GPU_PLL0, pcw);
			if (hopping != 0)
				gpufreq_pr_info("@%s: hopping failing: %d\n",
						__func__, hopping);
		}
#endif
	} else {
#ifdef CONFIG_MTK_FREQ_HOPPING
		/* change PCW (by hopping) */
		hopping = mt_dfs_general_pll(FH_GPU_PLL0, pcw);
		if (hopping != 0)
			gpufreq_pr_info("@%s: hopping failing: %d\n",
					__func__, hopping);
#endif
	}

	gpufreq_pr_logbuf(
		"posdiv: %d, curr_posdiv: %d, pcw: 0x%x, pll: 0x%08x, pll_con0: 0x%08x, pll_con1: 0x%08x, parking: %d, clk_cfg_20: 0x%08x\n",
		(1 << posdiv_power), (1 << curr_posdiv_power), pcw, pll, DRV_Reg32(MFGPLL1_CON0),
		DRV_Reg32(MFGPLL1_CON1), parking, readl(g_topckgen_base + 0x120));
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
		gpufreq_pr_debug("@%s: set AUTO_MODE(%d) or PWM_MODE(%d) mode: %d\n",
				__func__, REGULATOR_MODE_NORMAL,
				REGULATOR_MODE_FAST, mode);
	else
		gpufreq_pr_info("@%s: failed to configure mode, ret=%d mode=%d\n",
				__func__, ret, mode);
}

/*
 * set fixed frequency for PROCFS: fixed_freq_volt
 */
static void __mt_gpufreq_set_fixed_freq(int fixed_freq)
{
	int get_cur_freq = 0;

	gpufreq_pr_debug("@%s: before, g_fixed_freq=%d g_fixed_vgpu=%d\n",
			__func__, g_fixed_freq, g_fixed_vgpu);

	g_fixed_freq = fixed_freq;
	g_fixed_vgpu = g_cur_opp_vgpu;

	__mt_gpufreq_clock_switch(g_fixed_freq);

	get_cur_freq = __mt_gpufreq_get_cur_freq();
	gpufreq_pr_logbuf("before: %d, after: %d, target: %d, vgpu = %d\n",
	g_cur_opp_freq, get_cur_freq, g_fixed_freq, g_fixed_vgpu);

	g_cur_opp_freq = get_cur_freq;
}

/*
 * set fixed voltage for PROCFS: fixed_freq_volt
 */
static void __mt_gpufreq_set_fixed_vgpu(int fixed_vgpu)
{
	gpufreq_pr_debug("@%s: before, g_fixed_freq=%d g_fixed_vgpu=%d\n",
			__func__, g_fixed_freq, g_fixed_vgpu);

	g_fixed_freq = g_cur_opp_freq;
	g_fixed_vgpu = fixed_vgpu;

	gpufreq_pr_debug("@%s: now, g_fixed_freq=%d g_fixed_vgpu=%d\n",
			__func__, g_fixed_freq, g_fixed_vgpu);

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

	gpufreq_pr_debug("@%s: idx=%d dynamic=%d leakage=%d total=%d temp=%d\n",
			__func__, idx, p_dynamic, p_leakage, p_total, temp);

	g_power_table[idx].gpufreq_power = p_total;
}

static unsigned int __calculate_vgpu_settletime(bool mode, int deltaV)
{
	unsigned int settleTime;
	/* [MT6359P][VGPU]
	 * DVS Rising : delta(mV) / 10(mV/us) + 4(us) + 5(us)
	 * DVS Falling: delta(mV) /  5(mV/us) + 4(us) + 5(us)
	 */

	if (mode) {
		/* rising 10 mV/us */
		settleTime = deltaV / (10 * 100) + 9;
	} else {
		/* falling 10 mV/us */
		settleTime = deltaV / (5 * 100) + 9;
	}
	return settleTime; /* us */
}

static unsigned int __calculate_vsram_settletime(bool mode, int deltaV)
{
	unsigned int settleTime;
	/* [MT6359P][VSRAM_GPU]
	 * DVS Rising : delta(mV) / 12.5(mV/us) + 3(us) + 5(us)
	 * DVS Falling: delta(mV) /    5(mV/us) + 3(us) + 5(us)
	 */

	if (mode) {
		/* rising 10 mV/us */
		settleTime = deltaV / (125 * 10) + 8;
	} else {
		/* falling 5 mV/us */
		settleTime = deltaV / (5 * 100) + 8;
	}
	return settleTime; /* us */
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
	unsigned long pcw;

	mfgpll = DRV_Reg32(MFGPLL1_CON1);

	pcw = mfgpll & (0x3FFFFF);

	posdiv_power = (mfgpll & (0x7 << POSDIV_SHIFT)) >> POSDIV_SHIFT;

	freq_khz = (((pcw * TO_MHZ_TAIL + ROUNDING_VALUE) * MFGPLL_FIN) >>
			DDS_SHIFT) / (1 << posdiv_power) * TO_MHZ_HEAD;

	return freq_khz;
}

/*
 * get current vsram voltage (mV * 100)
 */
static unsigned int __mt_gpufreq_get_cur_vsram_gpu(void)
{
	unsigned int volt = 0;

	if (regulator_is_enabled(g_pmic->reg_vsram_gpu)) {
		/* regulator_get_voltage prints volt with uV */
		volt = regulator_get_voltage(g_pmic->reg_vsram_gpu) / 10;
	}

	return volt;
}

/*
 * get current voltage (mV * 100)
 */
static unsigned int __mt_gpufreq_get_cur_vgpu(void)
{
	unsigned int volt = 0;

	if (regulator_is_enabled(g_pmic->reg_vgpu)) {
		/* regulator_get_voltage prints volt with uV */
		volt = regulator_get_voltage(g_pmic->reg_vgpu) / 10;
	}

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

	gpufreq_pr_debug("@%s: vgpu=%d vsram_gpu=%d\n",
			__func__, vgpu, vsram_gpu);

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

	gpufreq_pr_debug("@%s: temp=%d\n", __func__, temp);

	mutex_lock(&mt_gpufreq_lock);

	if ((temp >= -20) && (temp <= 125)) {
		for (i = 0; i < g_max_opp_idx_num; i++) {

			freq = g_power_table[i].gpufreq_khz;
			volt = g_power_table[i].gpufreq_vgpu;

			__mt_gpufreq_calculate_power(i, freq, volt, temp);

			gpufreq_pr_debug("@%s: [%02d] freq=%d vgpu=%d power=%d\n",
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
#if MT_GPUFREQ_KICKER_PBM_READY
	unsigned int cur_power = 0;
	unsigned int cur_volt = 0;

	cur_volt = __mt_gpufreq_get_cur_vgpu();

	if (enable == BUCK_ON) {
		cur_power = g_power_table[g_cur_opp_idx].gpufreq_power;

		kicker_pbm_by_gpu(true, cur_power, cur_volt / 100);
	} else
		kicker_pbm_by_gpu(false, 0, cur_volt / 100);
#endif
}

static void __mt_gpufreq_init_table(void)
{
	struct opp_table_info *opp_table = __mt_gpufreq_get_segment_table();
	unsigned int segment_id = __mt_gpufreq_get_segment_id();
	unsigned int i = 0;

	/* determine max_opp/num/segment_table... by segment  */
	if (segment_id == MT6877_SEGMENT)
		g_segment_max_opp_idx = 4;
	else if (segment_id == MT6877T_SEGMENT)
		g_segment_max_opp_idx = 0;
	else
		g_segment_max_opp_idx = 4;

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

		gpufreq_pr_debug("@%s: [%02u] freq=%u vgpu=%u vsram=%u aging=%u\n",
				__func__, i,
				opp_table[i].gpufreq_khz,
				opp_table[i].gpufreq_vgpu,
				opp_table[i].gpufreq_vsram,
				opp_table[i].gpufreq_aging_margin);
	}

	g_max_opp_idx_num = NUM_OF_OPP_IDX;
	g_max_upper_limited_idx = g_segment_max_opp_idx;
	g_min_lower_limited_idx = g_segment_min_opp_idx;

	gpufreq_pr_debug("@%s: g_segment_max_opp_idx=%u g_max_opp_idx_num=%u g_segment_min_opp_idx=%u\n",
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

	gpufreq_pr_debug("@%s: temp=%d\n", __func__, temp);

	if ((temp < -20) || (temp > 125)) {
		gpufreq_pr_debug("@%s: temp < -20 or temp > 125!\n", __func__);
		temp = 65;
	}

	for (i = 0; i < num; i++) {
		g_power_table[i].gpufreq_khz = g_opp_table[i].gpufreq_khz;
		g_power_table[i].gpufreq_vgpu = g_opp_table[i].gpufreq_vgpu;

		__mt_gpufreq_calculate_power(i, g_power_table[i].gpufreq_khz,
				g_power_table[i].gpufreq_vgpu, temp);

		gpufreq_pr_debug("@%s: [%02d] freq_khz=%u vgpu=%u power=%u\n",
				__func__, i,
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
	gpufreq_pr_info("@%s: Preloader default freq is %d\n", __func__, freq);

	if (mt_gpufreq_get_cust_init_en()) {
		freq = MT_GPUFREQ_CUST_INIT_OPP;
		gpufreq_pr_info("@%s: CUST request freq to %d\n",
				__func__, freq);
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
		gpufreq_pr_info("@%s: Disable DVFS and CUST INIT !!!\n",
				__func__);
	} else {
		gpufreq_pr_info("@%s: Enable DVFS or CUST INIT !!!\n",
				__func__);
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
	int ret = 0;
	struct fm_pwr_sta mfg_fm_pwr_status = {
		.ofs = PWR_STATUS_OFS,
		.msk = 0x3F,
	};
	struct fm_subsys mfg_fm[] = {
		{
		.id = FM_MFGPLL1,
		.name = "fm_mfgpll1",
		.base = 0,
		.con0 = PLL4H_FQMTR_CON0_OFS,
		.con1 = PLL4H_FQMTR_CON1_OFS,
		.pwr_sta = mfg_fm_pwr_status,
		}
	};

	/* MFGPLL1 & MFGPLL4 are from GPU_PLL_CTRL */
	// 0x13fa0000
	g_gpu_pll_ctrl =
			__mt_gpufreq_of_ioremap(
				"mediatek,mt6877-gpu_pll_ctrl", 0);
	if (!g_gpu_pll_ctrl) {
		gpufreq_pr_info("@%s: ioremap failed at GPU_PLL_CTRL\n", __func__);
		return -ENOENT;
	}

	mfg_fm[0].base = g_gpu_pll_ctrl;
	ret = mt_subsys_freq_register(&mfg_fm[0], ARRAY_SIZE(mfg_fm));
	if (ret) {
		gpufreq_pr_info("@%s: failed to register fmeter\n", __func__);
		return ret;
	}

	// 0x10000000
	g_topckgen_base =
			__mt_gpufreq_of_ioremap("mediatek,topckgen", 0);
	if (!g_topckgen_base) {
		gpufreq_pr_info("@%s: ioremap failed at topckgen\n", __func__);
		return -ENOENT;
	}

	g_mfg_base = __mt_gpufreq_of_ioremap("mediatek,mfgcfg", 0);
	if (!g_mfg_base) {
		gpufreq_pr_info("@%s: ioremap failed at mfgcfg\n", __func__);
		return -ENOENT;
	}

#if MT_GPUFREQ_ASENSOR_ENABLE
	// 0x13FB7000
	g_cpe_controller_sensor =
		__mt_gpufreq_of_ioremap("mediatek,cpe_controller_sensor", 0);
	if (!g_cpe_controller_sensor) {
		gpufreq_pr_info("@%s: ioremap failed at cpe_controller_sensor\n",
			__func__);
		return -ENOENT;
	}

	// 0x13FB9C00
	g_cpe_controller_mcu_base =
		__mt_gpufreq_of_ioremap("mediatek,cpe_controller_mcu", 0);
	if (!g_cpe_controller_mcu_base) {
		gpufreq_pr_info("@%s: ioremap failed at cpe_controller_mcu\n",
			__func__);
		return -ENOENT;
	}

	// 0x13FCF000
	g_sensor_network_0_base =
		__mt_gpufreq_of_ioremap("mediatek,sensor_network_0", 0);
	if (!g_sensor_network_0_base) {
		gpufreq_pr_info("@%s: ioremap failed at sensor_network_0\n",
			__func__);
		return -ENOENT;
	}
#endif

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

	g_clk->clk_pll4 = devm_clk_get(&pdev->dev, "clk_pll4");
	if (IS_ERR(g_clk->clk_pll4)) {
		gpufreq_pr_info("@%s: cannot get clk_pll4\n", __func__);
		return PTR_ERR(g_clk->clk_pll4);
	}

	g_clk->subsys_bg3d = devm_clk_get(&pdev->dev, "subsys_bg3d");
	if (IS_ERR(g_clk->subsys_bg3d)) {
		gpufreq_pr_info("@%s: cannot get subsys_bg3d\n", __func__);
		return PTR_ERR(g_clk->subsys_bg3d);
	}

	g_clk->mtcmos_mfg0 = devm_clk_get(&pdev->dev, "mtcmos_mfg0");
	if (IS_ERR(g_clk->mtcmos_mfg0)) {
		gpufreq_pr_info("@%s: cannot get mtcmos_mfg0\n", __func__);
		return PTR_ERR(g_clk->mtcmos_mfg0);
	}

	g_clk->mtcmos_mfg1 = devm_clk_get(&pdev->dev, "mtcmos_mfg1");
	if (IS_ERR(g_clk->mtcmos_mfg1)) {
		gpufreq_pr_info("@%s: cannot get mtcmos_mfg1\n", __func__);
		return PTR_ERR(g_clk->mtcmos_mfg1);
	}

	g_clk->mtcmos_mfg2 = devm_clk_get(&pdev->dev, "mtcmos_mfg2");
	if (IS_ERR(g_clk->mtcmos_mfg2)) {
		gpufreq_pr_info("@%s: cannot get mtcmos_mfg2\n", __func__);
		return PTR_ERR(g_clk->mtcmos_mfg2);
	}

	g_clk->mtcmos_mfg3 = devm_clk_get(&pdev->dev, "mtcmos_mfg3");
	if (IS_ERR(g_clk->mtcmos_mfg3)) {
		gpufreq_pr_info("@%s: cannot get mtcmos_mfg3\n", __func__);
		return PTR_ERR(g_clk->mtcmos_mfg3);
	}

	g_clk->mtcmos_mfg4 = devm_clk_get(&pdev->dev, "mtcmos_mfg4");
	if (IS_ERR(g_clk->mtcmos_mfg4)) {
		gpufreq_pr_info("@%s: cannot get mtcmos_mfg4\n", __func__);
		return PTR_ERR(g_clk->mtcmos_mfg4);
	}

	g_clk->mtcmos_mfg5 = devm_clk_get(&pdev->dev, "mtcmos_mfg5");
	if (IS_ERR(g_clk->mtcmos_mfg5)) {
		gpufreq_pr_info("@%s: cannot get mtcmos_mfg5\n", __func__);
		return PTR_ERR(g_clk->mtcmos_mfg5);
	}

#if MT_GPUFREQ_SHADER_PWR_CTL_WA
	spin_lock_init(&mt_gpufreq_clksrc_parking_lock);
#endif

	// 0x1020E000
	g_infracfg_base = __mt_gpufreq_of_ioremap("mediatek,infracfg", 0);
	if (!g_infracfg_base) {
		gpufreq_pr_info("@%s: ioremap failed at infracfg\n", __func__);
		return -ENOENT;
	}

	// 0x10006000
	g_sleep = __mt_gpufreq_of_ioremap("mediatek,sleep", 0);
	if (!g_sleep) {
		gpufreq_pr_info("@%s: ioremap failed at sleep\n", __func__);
		return -ENOENT;
	}

	// 0x10001000
	g_infracfg_ao = __mt_gpufreq_of_ioremap("mediatek,infracfg_ao", 0);
	if (!g_infracfg_ao) {
		gpufreq_pr_info("@%s: ioremap failed at infracfg_ao\n",
				__func__);
		return -ENOENT;
	}

	// 0x1000d000
	g_dbgtop = __mt_gpufreq_of_ioremap("mediatek,dbgtop", 0);
	if (!g_dbgtop) {
		gpufreq_pr_info("@%s: ioremap failed at dbgtop\n", __func__);
		return -ENOENT;
	}

	// 0x10007000
	g_toprgu = __mt_gpufreq_of_ioremap("mediatek,toprgu", 0);
	if (!g_toprgu) {
		gpufreq_pr_info("@%s: ioremap failed at toprgu\n", __func__);
		return -ENOENT;
	}

	return 0;
}

static void __mt_gpufreq_init_acp(void)
{
	unsigned int val;

	/* disable acp */
	val = readl(g_infracfg_ao + 0x290) | (0x1 << 9);
	writel(val, g_infracfg_ao + 0x290);
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

	gpufreq_pr_info("[GPU_DFD] dfd trigger simulate complete!\n");
}
#endif

static void __mt_gpufreq_gpu_hard_reset(void)
{
	/*
	 * // [2] = mfg_rst, reset mfg
	 * Write register WDT_SWSYSRST (@ 0x1000_7208) = 0x88000004;
	 * Wait 10us;
	 * // [2] = mfg_rst, release reset
	 * Write register WDT_SWSYSRST (@ 0x1000_7208) = 0x88000000;
	 * Wait 10us;
	 */
	writel(0x88000004, g_toprgu + 0x208);
	udelay(10);
	writel(0x88000000, g_toprgu + 0x208);
	udelay(10);
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

static void __mt_gpufreq_dump_bringup_status(void)
{
	if (!mt_gpufreq_bringup())
		return;

	// 0x10000000
	g_topckgen_base =
			__mt_gpufreq_of_ioremap("mediatek,topckgen", 0);
	if (!g_topckgen_base) {
		gpufreq_pr_info("@%s: ioremap failed at topckgen\n", __func__);
		return;
	}

	// 0x13fa0000
	g_gpu_pll_ctrl =
			__mt_gpufreq_of_ioremap(
				"mediatek,mt6877-gpu_pll_ctrl", 0);
	if (!g_gpu_pll_ctrl) {
		gpufreq_pr_info("@%s: ioremap failed at GPU_PLL_CTRL\n", __func__);
		return;
	}

	// 0x10006000
	g_sleep = __mt_gpufreq_of_ioremap("mediatek,sleep", 0);
	if (!g_sleep) {
		gpufreq_pr_info("@%s: ioremap failed at sleep\n", __func__);
		return;
	}

	// [SPM] pwr_status: pwr_ack (@0x1000_6EF8)
	// [SPM] pwr_status_2nd: pwr_ack_2nd (@x1000_6EFC)
	gpufreq_pr_info("@%s: [PWR_ACK] PWR_STATUS=0x%08X, PWR_STATUS_2ND=0x%08X\n",
			__func__,
			readl(g_sleep + PWR_STATUS_OFS),
			readl(g_sleep + PWR_STATUS_2ND_OFS));
	gpufreq_pr_info("@%s: [MUX] =0x%08X\n",
			__func__,
			readl(g_topckgen_base + 0x120));
	gpufreq_pr_info("@%s: [MFGPLL] FMETER=%d FREQ=%d\n",
			__func__,
			mt_get_subsys_freq(FM_MFGPLL1),
			__mt_gpufreq_get_cur_freq());
}

/*
 * gpufreq driver probe
 */
static int __mt_gpufreq_pdrv_probe(struct platform_device *pdev)
{
	struct device_node *node;
	int ret;

	gpufreq_pr_info("@%s: driver init is started\n", __func__);

	node = of_find_matching_node(NULL, g_gpufreq_of_match);
	if (!node)
		gpufreq_pr_info("@%s: find GPU node failed\n", __func__);

#if MT_GPUFREQ_DFD_ENABLE
	if (mtk_dbgtop_mfg_pwr_en(1)) {
		gpufreq_pr_info("[GPU_DFD] wait dbgtop ready\n");
		return -EPROBE_DEFER;
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

	__mt_gpufreq_init_acp();

	/* init opp table */
	__mt_gpufreq_init_table();

#if MT_GPUFREQ_DFD_ENABLE
	__mt_gpufreq_dbgtop_pwr_on(true);
#endif

	mt_gpufreq_power_control(POWER_ON, CG_ON, MTCMOS_ON, BUCK_ON);

	/* init Vgpu/Vsram_gpu by bootup freq index */
	__mt_gpufreq_init_volt_by_freq();

	if (mt_gpufreq_power_ctl_en())
		mt_gpufreq_power_control(POWER_OFF, CG_OFF, MTCMOS_OFF, BUCK_OFF);
	else
		gpufreq_pr_info("@%s: Power Control Always On !!!\n", __func__);

	__mt_gpufreq_init_power();

#if MT_GPUFREQ_DFD_DEBUG
	// for debug only. simulate gpu dfd trigger state
	//__mt_gpufreq_gpu_dfd_trigger_simulate();
#endif

	g_probe_done = true;
	gpufreq_pr_info("@%s: driver init is finished\n", __func__);

	return 0;
}

static int __mt_gpufreq_mfgpll_probe(struct platform_device *pdev)
{
	int ret = 0;

	gpufreq_pr_info("@%s: driver init is started\n", __func__);

	ret = clk_mt6877_pll_registration(GPU_PLL_CTRL, g_mfg_plls,
		pdev, ARRAY_SIZE(g_mfg_plls));
	if (ret) {
		gpufreq_pr_info("@%s: failed to register pll\n", __func__);
		return ret;
	}

	gpufreq_pr_info("@%s: driver init is finished\n", __func__);

	return ret;
}

/*
 * register the gpufreq driver
 */
static int __init __mt_gpufreq_init(void)
{
	int ret = 0;

	if (mt_gpufreq_bringup()) {
#if MT_GPUFREQ_SHADER_PWR_CTL_WA
		spin_lock_init(&mt_gpufreq_clksrc_parking_lock);
#endif
		__mt_gpufreq_dump_bringup_status();
		gpufreq_pr_info("@%s: skip driver init when bringup\n",
				__func__);
		return 0;
	}

	gpufreq_pr_debug("@%s: start to initialize gpufreq driver\n",
			__func__);

#ifdef CONFIG_PROC_FS
	if (__mt_gpufreq_create_procfs())
		goto out;
#endif

	/* register platform driver */
	ret = platform_driver_register(&g_gpufreq_pdrv);
	if (ret)
		gpufreq_pr_info("@%s: fail to register gpufreq driver\n",
			__func__);

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

/*
 * register the gpufreq mfgpll driver
 */
static int __init __mt_gpufreq_mfgpll_init(void)
{
	int ret = 0;

	gpufreq_pr_debug("@%s: start to initialize mfgpll driver\n",
			__func__);

	/* register platform driver */
	ret = platform_driver_register(&g_gpufreq_mfgpll_pdrv);
	if (ret)
		gpufreq_pr_info("@%s: fail to register mfgpll driver\n",
			__func__);

	return ret;
}

arch_initcall_sync(__mt_gpufreq_mfgpll_init);
module_init(__mt_gpufreq_init);
module_exit(__mt_gpufreq_exit);

MODULE_DEVICE_TABLE(of, g_mfgpll_of_match);
MODULE_DEVICE_TABLE(of, g_gpufreq_of_match);
MODULE_DESCRIPTION("MediaTek GPU-DVFS-PLAT driver");
MODULE_LICENSE("GPL");
