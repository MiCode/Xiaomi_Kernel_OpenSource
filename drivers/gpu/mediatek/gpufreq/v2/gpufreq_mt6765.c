// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */
/**
 * @file    mtk_gpufreq_core.c
 * @brief   GPU-DVFS Driver Platform Implementation
 */
/**
 * ===============================================
 * Include
 * ===============================================
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/ioport.h>
#include <linux/err.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/printk.h>
#include <linux/pm_runtime.h>
#include <linux/pm_domain.h>
#include <linux/random.h>

#include <gpufreq_v2.h>
#include <gpufreq_mt6765.h>
#include <gpufreq_debug.h>
#include <gpuppm.h>
#include <gpufreq_common.h>
#include <mtk_gpu_utility.h>
#include <gpu_misc.h>
#include "clkchk-mt6765.h"


#if IS_ENABLED(CONFIG_MTK_DEVINFO)
#include <linux/nvmem-consumer.h>
#endif

#if IS_ENABLED(CONFIG_MTK_BATTERY_OC_POWER_THROTTLING)
#include <mtk_battery_oc_throttling.h>
#endif
#if IS_ENABLED(CONFIG_MTK_BATTERY_PERCENT_THROTTLING)
#include <mtk_bp_thl.h>
#endif
#if IS_ENABLED(CONFIG_MTK_LOW_BATTERY_POWER_THROTTLING)
#include <mtk_low_battery_throttling.h>
#endif
#if IS_ENABLED(CONFIG_MTK_STATIC_POWER_LEGACY)
#include <leakage_table_v2/mtk_static_power.h>
#endif
#if IS_ENABLED(CONFIG_COMMON_CLK_MTK_FREQ_HOPPING)
#include <clk-mtk.h>
#endif
//TODO:GKI check api
#if IS_ENABLED(CONFIG_COMMON_CLK_MT6765)
#include <clk-fmeter.h>
#include <clk-mt6765-fmeter.h>
#endif

#if IS_ENABLED(CONFIG_MTK_DEVINFO)
#include <linux/nvmem-consumer.h>
#endif

#if IS_ENABLED(CONFIG_MTK_PBM)
#include "mtk_pbm.h"
#endif

/**
 * ===============================================
 * Local Function Declaration
 * ===============================================
 */
/* misc function */
static unsigned int __gpufreq_custom_init_enable(void);
static unsigned int __gpufreq_dvfs_enable(void);
static void __gpufreq_set_dvfs_state(unsigned int set, unsigned int state);
static void __gpufreq_dump_bringup_status(struct platform_device *pdev);
static void __gpufreq_measure_power(void);
static void __iomem *__gpufreq_of_ioremap(const char *node_name, int idx);
static int __gpufreq_pause_dvfs(unsigned int oppidx);
static void __gpufreq_resume_dvfs(void);
static void __gpufreq_apply_aging(unsigned int apply_aging);
static void __gpufreq_apply_adjust(struct gpufreq_adj_info *adj_table, int adj_num);
/* dvfs function */
static int __gpufreq_custom_commit_gpu(unsigned int target_freq,
	unsigned int target_volt, enum gpufreq_dvfs_state key);
static int __gpufreq_freq_scale_gpu(unsigned int freq_old, unsigned int freq_new);
static int __gpufreq_volt_scale_gpu(
	unsigned int vgpu_old, unsigned int vgpu_new,
	unsigned int vsram_old, unsigned int vsram_new);
static int __gpufreq_switch_clksrc(enum gpufreq_clk_src clksrc);
static unsigned int __gpufreq_calculate_pcw(unsigned int freq, enum gpufreq_posdiv posdiv_power);
/* get function */
static unsigned int __gpufreq_get_fmeter_fgpu(void);
static unsigned int __gpufreq_get_real_fgpu(void);
static unsigned int __gpufreq_get_real_vgpu(void);
static unsigned int __gpufreq_get_real_vsram(void);
static enum gpufreq_posdiv __gpufreq_get_real_posdiv_gpu(void);
static enum gpufreq_posdiv __gpufreq_get_posdiv_by_fgpu(unsigned int freq);
/* power control function */
static int __gpufreq_clock_control(enum gpufreq_power_state power);
static int __gpufreq_mtcmos_control(enum gpufreq_power_state power);
static int __gpufreq_buck_control(enum gpufreq_power_state power);
/* init function */
static void __gpufreq_init_shader_present(void);
static void __gpufreq_segment_adjustment(struct platform_device *pdev);
static void __gpufreq_custom_adjustment(void);
static void __gpufreq_avs_adjustment(void);
static void __gpufreq_aging_adjustment(void);
static int __gpufreq_init_opp_idx(void);
static int __gpufreq_init_opp_table(struct platform_device *pdev);
static int __gpufreq_init_segment_id(struct platform_device *pdev);
static int __gpufreq_init_mtcmos(struct platform_device *pdev);
static int __gpufreq_init_clk(struct platform_device *pdev);
static int __gpufreq_init_pmic(struct platform_device *pdev);
static int __gpufreq_init_platform_info(struct platform_device *pdev);
static int __gpufreq_pdrv_probe(struct platform_device *pdev);
static int __gpufreq_pdrv_remove(struct platform_device *pdev);

/*low power*/
static void __gpufreq_kick_pbm(int enable);
/*external function*/
#if IS_ENABLED(CONFIG_MTK_PBM)
extern void kicker_pbm_by_gpu(bool status, unsigned int loading, int voltage);
#endif
//thermal
static void __gpufreq_update_power_table(void);

/**
 * ===============================================
 * Local Variable Definition
 * ===============================================
 */
static const struct of_device_id g_gpufreq_of_match[] = {
	{ .compatible = "mediatek,gpufreq" },
	{ /* sentinel */ }
};
static struct platform_driver g_gpufreq_pdrv = {
	.probe = __gpufreq_pdrv_probe,
	.remove = __gpufreq_pdrv_remove,
	.driver = {
		.name = "gpufreq",
		.owner = THIS_MODULE,
		.of_match_table = g_gpufreq_of_match,
	},
};
static struct mt_gpufreq_power_table_info *g_power_table;
static int (*mt_gpufreq_wrap_fp)(void);

static struct g_pmic_info *g_pmic;
static struct g_clk_info *g_clk;
static void __iomem *g_MFG_base;
static void __iomem *g_apmixed_base;
static unsigned int g_aging_load;
static unsigned int g_mcl50_load;
static unsigned int g_gpueb_support;
static unsigned int g_aging_enable;
static unsigned int g_shader_present;
static unsigned int g_stress_test_enable;
static struct gpufreq_status g_gpu;
static bool g_EnableHWAPM_state;
static struct gpufreq_mtcmos_info *g_mtcmos;
static enum gpufreq_dvfs_state g_dvfs_state;


static DEFINE_MUTEX(gpufreq_lock);


static struct gpufreq_platform_fp platform_ap_fp = {
	.bringup = __gpufreq_bringup,
	.power_ctrl_enable = __gpufreq_power_ctrl_enable,
	.get_power_state = __gpufreq_get_power_state,
	.get_dvfs_state = __gpufreq_get_dvfs_state,
	.get_cur_fgpu = __gpufreq_get_cur_fgpu,
	.get_cur_vgpu = __gpufreq_get_cur_vgpu,
	.get_cur_vsram_gpu = __gpufreq_get_cur_vsram_gpu,
	.get_cur_pgpu = __gpufreq_get_cur_pgpu,
	.get_max_pgpu = __gpufreq_get_max_pgpu,
	.get_min_pgpu = __gpufreq_get_min_pgpu,
	.get_cur_idx_gpu = __gpufreq_get_cur_idx_gpu,
	.get_opp_num_gpu = __gpufreq_get_opp_num_gpu,
	.get_signed_opp_num_gpu = __gpufreq_get_signed_opp_num_gpu,
	.get_working_table_gpu = __gpufreq_get_working_table_gpu,
	.get_signed_table_gpu = __gpufreq_get_signed_table_gpu,
	.get_debug_opp_info_gpu = __gpufreq_get_debug_opp_info_gpu,
	.get_fgpu_by_idx = __gpufreq_get_fgpu_by_idx,
	.get_pgpu_by_idx = __gpufreq_get_pgpu_by_idx,
	.get_idx_by_fgpu = __gpufreq_get_idx_by_fgpu,
	.get_lkg_pgpu = __gpufreq_get_lkg_pgpu,
	.get_dyn_pgpu = __gpufreq_get_dyn_pgpu,
	.power_control = __gpufreq_power_control,
	.generic_commit_gpu = __gpufreq_generic_commit_gpu,
	.fix_target_oppidx_gpu = __gpufreq_fix_target_oppidx_gpu,
	.fix_custom_freq_volt_gpu = __gpufreq_fix_custom_freq_volt_gpu,
	.set_timestamp = __gpufreq_set_timestamp,
	.check_bus_idle = __gpufreq_check_bus_idle,
	.dump_infra_status = __gpufreq_dump_infra_status,
	.set_stress_test = __gpufreq_set_stress_test,
	.set_aging_mode = __gpufreq_set_aging_mode,
	.update_power_table = __gpufreq_update_power_table,
	.get_idx_by_pgpu = __gpufreq_get_idx_by_pgpu,
};
static struct gpufreq_platform_fp platform_eb_fp = {
};
/**
 * ===============================================
 * External Function Definition
 * ===============================================
 */
/* API: get BRINGUP status */
unsigned int __gpufreq_bringup(void)
{
	return GPUFREQ_BRINGUP;
}
/* API: get POWER_CTRL status */
unsigned int __gpufreq_power_ctrl_enable(void)
{
	return GPUFREQ_POWER_CTRL_ENABLE;
}
/* API: get power state (on/off) */
unsigned int __gpufreq_get_power_state(void)
{
	if (g_gpu.power_count > 0)
		return POWER_ON;
	else
		return POWER_OFF;
}
/* API: get DVFS state (free/disable/keep) */
unsigned int __gpufreq_get_dvfs_state(void)
{
	return g_dvfs_state;
}
/* API: get current Freq of GPU */
unsigned int __gpufreq_get_cur_fgpu(void)
{
	return g_gpu.cur_freq;
}
/* API: get current Freq of STACK */
unsigned int __gpufreq_get_cur_fstack(void)
{
	return 0;
}
/* API: get current Volt of GPU */
unsigned int __gpufreq_get_cur_vgpu(void)
{
	return g_gpu.buck_count ? g_gpu.cur_volt : 0;
}
/* API: get current Volt of STACK */
unsigned int __gpufreq_get_cur_vstack(void)
{
	return 0;
}
/* API: get current Vsram of GPU */
unsigned int __gpufreq_get_cur_vsram_gpu(void)
{
	return g_gpu.cur_vsram;
}
/* API: get current Vsram of STACK */
unsigned int __gpufreq_get_cur_vsram_stack(void)
{
	return 0;
}
/* API: get current Power of GPU */
unsigned int __gpufreq_get_cur_pgpu(void)
{
	return g_gpu.working_table[g_gpu.cur_oppidx].power;
}
/* API: get current Power of STACK */
unsigned int __gpufreq_get_cur_pstack(void)
{
	return 0;
}
/* API: get max Power of GPU */
unsigned int __gpufreq_get_max_pgpu(void)
{
	return g_gpu.working_table[g_gpu.max_oppidx].power;
}
/* API: get max Power of STACK */
unsigned int __gpufreq_get_max_pstack(void)
{
	return 0;
}
/* API: get min Power of GPU */
unsigned int __gpufreq_get_min_pgpu(void)
{
	return g_gpu.working_table[g_gpu.min_oppidx].power;
}
/* API: get min Power of STACK */
unsigned int __gpufreq_get_min_pstack(void)
{
	return 0;
}
/* API: get current working OPP index of GPU */
int __gpufreq_get_cur_idx_gpu(void)
{
	return g_gpu.cur_oppidx;
}
/* API: get current working OPP index of STACK */
int __gpufreq_get_cur_idx_stack(void)
{
	return -1;
}
/* API: get number of working OPP of GPU */
int __gpufreq_get_opp_num_gpu(void)
{
	return g_gpu.opp_num;
}
/* API: get number of working OPP of STACK */
int __gpufreq_get_opp_num_stack(void)
{
	return 0;
}
/* API: get number of signed OPP of GPU */
int __gpufreq_get_signed_opp_num_gpu(void)
{
	return g_gpu.signed_opp_num;
}
/* API: get number of signed OPP of STACK */
int __gpufreq_get_signed_opp_num_stack(void)
{
	return 0;
}
/* API: get poiner of working OPP table of GPU */
const struct gpufreq_opp_info *__gpufreq_get_working_table_gpu(void)
{
	return g_gpu.working_table;
}
/* API: get poiner of signed OPP table of GPU */
const struct gpufreq_opp_info *__gpufreq_get_signed_table_gpu(void)
{
	return g_gpu.signed_table;
}
/* API: get debug info of GPU for Proc show */
struct gpufreq_debug_opp_info __gpufreq_get_debug_opp_info_gpu(void)
{
	struct gpufreq_debug_opp_info opp_info = {};

	mutex_lock(&gpufreq_lock);
	opp_info.cur_oppidx = g_gpu.cur_oppidx;
	opp_info.cur_freq = g_gpu.cur_freq;
	opp_info.cur_volt = g_gpu.cur_volt;
	opp_info.cur_vsram = g_gpu.cur_vsram;
	opp_info.power_count = g_gpu.power_count;
	opp_info.cg_count = g_gpu.cg_count;
	opp_info.mtcmos_count = g_gpu.mtcmos_count;
	opp_info.buck_count = g_gpu.buck_count;
	opp_info.segment_id = g_gpu.segment_id;
	opp_info.opp_num = g_gpu.opp_num;
	opp_info.signed_opp_num = g_gpu.signed_opp_num;
	opp_info.dvfs_state = g_dvfs_state;
	opp_info.shader_present = g_shader_present;
	opp_info.aging_enable = g_aging_enable;
	if (__gpufreq_get_power_state()) {
		opp_info.fmeter_freq = __gpufreq_get_fmeter_fgpu();
		opp_info.con1_freq = __gpufreq_get_real_fgpu();
		opp_info.regulator_volt = __gpufreq_get_real_vgpu();
		opp_info.regulator_vsram = __gpufreq_get_real_vsram();
	} else {
		opp_info.fmeter_freq = 0;
		opp_info.con1_freq = 0;
		opp_info.regulator_volt = 0;
		opp_info.regulator_vsram = 0;
	}
	mutex_unlock(&gpufreq_lock);
	return opp_info;
}
/* API: get debug info of STACK for Proc show */
struct gpufreq_debug_opp_info __gpufreq_get_debug_opp_info_stack(void)
{
	struct gpufreq_debug_opp_info opp_info = {};
	return opp_info;
}
//Export symbol [START]

/* API: mt_gpufreq_get_power_table */
struct mt_gpufreq_power_table_info *mt_gpufreq_get_power_table(void)
{
	return g_power_table;
}
EXPORT_SYMBOL(mt_gpufreq_get_power_table);

/* API: mt_gpufreq_get_power_table_num */
unsigned int mt_gpufreq_get_power_table_num(void)
{
	return g_gpu.signed_opp_num;
}
EXPORT_SYMBOL(mt_gpufreq_get_power_table_num);

/* API: mt_gpufreq_set_hwapm_state */
void mt_gpufreq_set_hwapm_state(bool bEnableHWAPM)
{
	g_EnableHWAPM_state = bEnableHWAPM;
	GPUFREQ_LOGD("@%s: g_EnableHWAPM_state updated = %d\n", __func__, bEnableHWAPM);
}
EXPORT_SYMBOL(mt_gpufreq_set_hwapm_state);

/* API: mt_gpufreq_get_dvfs_table_num */
unsigned int mt_gpufreq_get_dvfs_table_num(void)
{
	return  g_gpu.segment_lowbound - g_gpu.segment_upbound + 1;
}
EXPORT_SYMBOL(mt_gpufreq_get_dvfs_table_num);

/* API: mt_gpufreq_get_freq_by_idx */
unsigned int mt_gpufreq_get_freq_by_idx(unsigned int idx)
{
	return __gpufreq_get_fgpu_by_idx(idx);
}
EXPORT_SYMBOL(mt_gpufreq_get_freq_by_idx);
/* API: get Volt of GPU via OPP index */
unsigned int __gpufreq_get_vgpu_by_idx(int oppidx)
{
	if (oppidx >= 0 && oppidx < g_gpu.opp_num)
		return g_gpu.working_table[oppidx].volt;
	else
		return 0;
}
/* API: mt_gpufreq_get_volt_by_idx */
unsigned int mt_gpufreq_get_volt_by_idx(unsigned int idx)
{
	return __gpufreq_get_vgpu_by_idx(idx);
}
EXPORT_SYMBOL(mt_gpufreq_get_volt_by_idx);

/* API: mt_gpufreq_get_seg_max_opp_index */
unsigned int mt_gpufreq_get_seg_max_opp_index(void)
{
	return g_gpu.segment_upbound;
}
EXPORT_SYMBOL(mt_gpufreq_get_seg_max_opp_index);

/* API: mt_gpufreq_get_cur_freq_index */
unsigned int mt_gpufreq_get_cur_freq_index(void)
{
	GPUFREQ_LOGD("@%s:current OPP table conditional index is %d\n", __func__, g_gpu.cur_oppidx);
	return g_gpu.cur_oppidx;
}
EXPORT_SYMBOL(mt_gpufreq_get_cur_freq_index);

/* API: mt_gpufreq_get_cur_freq */
unsigned int mt_gpufreq_get_cur_freq(void)
{
	GPUFREQ_LOGD("@%s: current frequency is %d\n", __func__, g_gpu.cur_freq);
	return g_gpu.cur_freq;
}
EXPORT_SYMBOL(mt_gpufreq_get_cur_freq);
//EXPORT_SYMBOL(MTKPowerStatus);

/* API: mt_gpufreq_get_cur_volt */
unsigned int mt_gpufreq_get_cur_volt(void)
{
	return g_gpu.buck_count ? g_gpu.cur_volt : 0;
}
EXPORT_SYMBOL(mt_gpufreq_get_cur_volt);

/* API: mt_gpufreq_power_limit_notify_registerCB */
void mt_gpufreq_power_limit_notify_registerCB(gpufreq_power_limit_notify pCB)
{
  /* legacy */
}
EXPORT_SYMBOL(mt_gpufreq_power_limit_notify_registerCB);

/*
 * API : set gpu wrap function pointer
 */

void mt_gpufreq_set_gpu_wrap_fp(int (*gpu_wrap_fp)(void))
{
	mt_gpufreq_wrap_fp = gpu_wrap_fp;
}
EXPORT_SYMBOL(mt_gpufreq_set_gpu_wrap_fp);
/*
 * API : enable DVFS for PTPOD initializing
 */
void mt_gpufreq_enable_by_ptpod(void)
{
	__gpufreq_resume_dvfs();

	__gpufreq_power_control(POWER_OFF);

#if defined(CONFIG_ARM64) && \
	defined(CONFIG_BUILD_ARM64_DTB_OVERLAY_IMAGE_NAMES)
	GPUFREQ_LOGI("flavor name: %s\n",
		CONFIG_BUILD_ARM64_DTB_OVERLAY_IMAGE_NAMES);
	if ((strstr(CONFIG_BUILD_ARM64_DTB_OVERLAY_IMAGE_NAMES,
		"k65v1_64_aging") != NULL)) {
		GPUFREQ_LOGI("AGING flavor !!!\n");
		g_aging_enable = 1;
	}
#endif

	GPUFREQ_LOGD("DVFS is enabled by ptpod\n");
}
EXPORT_SYMBOL(mt_gpufreq_enable_by_ptpod);

/*
 * API : disable DVFS for PTPOD initializing
 */
void mt_gpufreq_disable_by_ptpod(void)
{
	int i = 0;
	int target_idx = 0;

	__gpufreq_power_control(POWER_ON);

	/* Fix GPU @ 0.8V */
	for (i = 0; i < g_gpu.opp_num; i++) {
		if (g_gpu.working_table[i].volt <= GPU_DVFS_PTPOD_DISABLE_VOLT) {
			target_idx = i;
			break;
		}
	}
	__gpufreq_pause_dvfs(target_idx);

	GPUFREQ_LOGD("DVFS is disabled by ptpod, g_DVFS_off_by_ptpod_idx: %d\n",
		target_idx);
}
EXPORT_SYMBOL(mt_gpufreq_disable_by_ptpod);
//Export symbol [END]

/* power calculation for power table */
static void __gpufreq_calculate_power(unsigned int idx, unsigned int freq,
			unsigned int volt, unsigned int temp)
{
	unsigned int p_total = 0;
	unsigned int p_dynamic = 0;
	unsigned int ref_freq = 0;
	unsigned int ref_volt = 0;
	int p_leakage = 0;

	p_dynamic = __gpufreq_get_dyn_pgpu(freq, volt);

#if IS_ENABLED(CONFIG_MTK_STATIC_POWER_LEGACY)
	p_leakage = mt_spower_get_leakage(MTK_SPOWER_GPU, (volt / 100), temp);
	if (g_gpu.buck_count == 0 || p_leakage < 0)
		p_leakage = 0;
#else
	p_leakage = 71;
#endif /* ifdef MT_GPUFREQ_STATIC_PWR_READY2USE */

	p_total = p_dynamic + p_leakage;

	GPUFREQ_LOGD("idx = %d, p_dynamic = %d, p_leakage = %d, p_total = %d, temp = %d\n",
			idx, p_dynamic, p_leakage, p_total, temp);

	g_power_table[idx].gpufreq_power = p_total;
	g_gpu.working_table[idx].power = p_total;

}

/*
 * OPP power table initialization
 */
static void __gpufreq_setup_opp_power_table(int num)
{
	int i = 0;
	int temp = 0;

	g_power_table = kzalloc((num) * sizeof(struct mt_gpufreq_power_table_info), GFP_KERNEL);

	if (g_power_table == NULL)
		return;

#if IS_ENABLED(CONFIG_MTK_LEGACY_THERMAL)
	if (mt_gpufreq_wrap_fp)
		temp = mt_gpufreq_wrap_fp() / 1000;
	else
		temp = 40;
#else
	temp = 40;
#endif /* ifdef CONFIG_MTK_LEGACY_THERMAL */

	GPUFREQ_LOGD("@%s: temp = %d\n", __func__, temp);

	if ((temp < -20) || (temp > 125)) {
		GPUFREQ_LOGD("temp < -20 or temp > 125!\n");
		temp = 65;
	}

	for (i = 0; i < num; i++) {
		g_power_table[i].gpufreq_khz = g_gpu.signed_table[i].freq;
		g_power_table[i].gpufreq_volt = g_gpu.signed_table[i].volt;

		__gpufreq_calculate_power(i, g_power_table[i].gpufreq_khz,
				g_power_table[i].gpufreq_volt, temp);

		GPUFREQ_LOGD("[%d], freq_khz = %u, volt = %u, power = %u\n",
				i, g_power_table[i].gpufreq_khz,
				g_power_table[i].gpufreq_volt,
				g_power_table[i].gpufreq_power);
	}
}


/* update OPP power table */
static void __gpufreq_update_power_table(void)
{
	int i;
	int temp = 0;
	unsigned int freq = 0;
	unsigned int volt = 0;

#if IS_ENABLED(CONFIG_MTK_LEGACY_THERMAL)
	if (mt_gpufreq_wrap_fp)
		temp = mt_gpufreq_wrap_fp() / 1000;
	else
		temp = 40;
#else
	temp = 40;
#endif /* ifdef CONFIG_THERMAL */

	GPUFREQ_LOGD("temp = %d\n", temp);

	mutex_lock(&gpufreq_lock);

	if ((temp >= -20) && (temp <= 125)) {
		for (i = 0; i < g_gpu.signed_opp_num; i++) {
			freq = g_power_table[i].gpufreq_khz;
			volt = g_power_table[i].gpufreq_volt;

			__gpufreq_calculate_power(i, freq, volt, temp);

			GPUFREQ_LOGD("[%d] freq_khz = %d, volt = %d, power = %d\n",
				i, g_power_table[i].gpufreq_khz,
				g_power_table[i].gpufreq_volt,
				g_power_table[i].gpufreq_power);
		}
	} else {
		GPUFREQ_LOGE("temp < -20 or temp > 125, NOT update power table!\n");
	}

	mutex_unlock(&gpufreq_lock);

}

/* API: get Freq of GPU via OPP index */
unsigned int __gpufreq_get_fgpu_by_idx(int oppidx)
{
	if (oppidx >= 0 && oppidx < g_gpu.opp_num)
		return g_gpu.working_table[oppidx].freq;
	else
		return 0;
}
/* API: get Power of GPU via OPP index */
unsigned int __gpufreq_get_pgpu_by_idx(int oppidx)
{
	if (oppidx >= 0 && oppidx < g_gpu.opp_num)
		return g_gpu.working_table[oppidx].power;
	else
		return 0;
}
/* API: get working OPP index of GPU via Freq */
int __gpufreq_get_idx_by_fgpu(unsigned int freq)
{
	int oppidx = -1;
	int i = 0;
	/* find the smallest index that satisfy given freq */
	for (i = g_gpu.min_oppidx; i >= g_gpu.max_oppidx; i--) {
		if (g_gpu.working_table[i].freq >= freq)
			break;
	}
	/* use max_oppidx if not found */
	oppidx = (i > g_gpu.max_oppidx) ? i : g_gpu.max_oppidx;
	return oppidx;
}
/* API: get working OPP index of STACK via Freq */
int __gpufreq_get_idx_by_fstack(unsigned int freq)
{
	GPUFREQ_UNREFERENCED(freq);
	return 0;
}
/* API: get working OPP index of GPU via Volt */
int __gpufreq_get_idx_by_vgpu(unsigned int volt)
{
	int oppidx = -1;
	int i = 0;
	/* find the smallest index that satisfy given volt */
	for (i = g_gpu.min_oppidx; i >= g_gpu.max_oppidx; i--) {
		if (g_gpu.working_table[i].volt >= volt)
			break;
	}
	/* use max_oppidx if not found */
	oppidx = (i > g_gpu.max_oppidx) ? i : g_gpu.max_oppidx;
	return oppidx;
}
/* API: get working OPP index of STACK via Volt */
int __gpufreq_get_idx_by_vstack(unsigned int volt)
{
	GPUFREQ_UNREFERENCED(volt);
	return -1;
}
/* API: get working OPP index of GPU via Power */
int __gpufreq_get_idx_by_pgpu(unsigned int power)
{
	int oppidx = -1;
	int i = 0;
	/* find the smallest index that satisfy given power */
	for (i = g_gpu.min_oppidx; i >= g_gpu.max_oppidx; i--) {
		if (g_gpu.working_table[i].power >= power)
			break;
	}
	/* use max_oppidx if not found */
	oppidx = (i > g_gpu.max_oppidx) ? i : g_gpu.max_oppidx;
	return oppidx;
}
/* API: get working OPP index of STACK via Power */
int __gpufreq_get_idx_by_pstack(unsigned int power)
{
	GPUFREQ_UNREFERENCED(power);
	return -1;
}
/* API: get Volt of SRAM via volt of GPU */
unsigned int __gpufreq_get_vsram_by_vgpu(unsigned int vgpu)
{
	//All opp table segments based on segment id has same vsram :Legacy
	return 87500;
}
/* API: get leakage Power of GPU */
unsigned int __gpufreq_get_lkg_pgpu(unsigned int volt)
{
	GPUFREQ_UNREFERENCED(volt);
	return GPU_LEAKAGE_POWER;
}
/* API: get dynamic Power of GPU */
unsigned int __gpufreq_get_dyn_pgpu(unsigned int freq, unsigned int volt)
{
	unsigned int p_dynamic = GPU_ACT_REF_POWER;
	unsigned int ref_freq = GPU_ACT_REF_FREQ;
	unsigned int ref_volt = GPU_ACT_REF_VOLT;

	p_dynamic = p_dynamic *
		((freq * 100) / ref_freq) *
		((volt * 100) / ref_volt) *
		((volt * 100) / ref_volt) /
		(100 * 100 * 100);
	return p_dynamic;
}

/*
 * kick Power Budget Manager(PBM) when OPP changed
 */
static void __gpufreq_kick_pbm(int enable)
{
#if IS_ENABLED(CONFIG_MTK_PBM)
	unsigned int power;
	unsigned int cur_freq;
	unsigned int cur_volt;
	unsigned int found = 0;
	int tmp_idx = -1;
	int i;

	cur_freq = g_gpu.cur_freq;
	cur_volt = g_gpu.cur_volt;

	if (enable) {
		for (i = 0; i <  g_gpu.opp_num; i++) {
			if (g_power_table[i].gpufreq_khz == cur_freq) {
				/* record idx since current voltage
				 * may not in DVFS table
				 */
				tmp_idx = i;

				if (g_power_table[i].gpufreq_volt == cur_volt) {
					power = g_power_table[i].gpufreq_power;
					found = 1;
					kicker_pbm_by_gpu(true,
						power, cur_volt / 100);
					GPUFREQ_LOGD(
						"@%s: request GPU power = %d,",
						__func__, power);
					GPUFREQ_LOGD(
				" cur_volt = %d uV, cur_freq = %d KHz\n",
					cur_volt * 10, cur_freq);
					return;
				}
			}
		}

		if (!found) {
			GPUFREQ_LOGD("@%s: tmp_idx = %d\n",
				__func__, tmp_idx);
			if (tmp_idx != -1 && tmp_idx < g_gpu.opp_num) {
				/* freq to find corresponding power budget */
				power = g_power_table[tmp_idx].gpufreq_power;
				kicker_pbm_by_gpu(true, power, cur_volt / 100);
				GPUFREQ_LOGD("@%s: request GPU power = %d,",
				__func__, power);
				GPUFREQ_LOGD(
				" cur_volt = %d uV, cur_freq = %d KHz\n",
				cur_volt * 10, cur_freq);
			} else {
				GPUFREQ_LOGD(
				"@%s: Cannot found request power in power table",
				__func__);
				GPUFREQ_LOGD(
				", cur_freq = %d KHz, cur_volt = %d uV\n",
				cur_freq, cur_volt * 10);
			}
		}
	} else {
		kicker_pbm_by_gpu(false, 0, cur_volt / 100);
	}
#endif /* CONFIG_MTK_PBM */
}

/*
 * API: control power state of whole MFG system
 * return power_count if success
 * return GPUFREQ_EINVAL if failure
 */
int __gpufreq_power_control(enum gpufreq_power_state power)
{
	int ret = 0;

	GPUFREQ_TRACE_START("power=%d", power);
	mutex_lock(&gpufreq_lock);
	GPUFREQ_LOGD("switch power: %s (Power: %d, Buck: %d, MTCMOS: %d, CG: %d)",
		power ? "On" : "Off",
		g_gpu.power_count, g_gpu.buck_count,
		g_gpu.mtcmos_count, g_gpu.cg_count);
	if (power == POWER_ON) {
		g_gpu.power_count++;
	} else {
		g_gpu.power_count--;
		__gpufreq_check_pending_exception();
	}
	__gpufreq_footprint_power_count(g_gpu.power_count);
	if (power == POWER_ON && g_gpu.power_count == 1) {
		__gpufreq_footprint_power_step(GPUFREQ_POWER_STEP_01);
		/* control Buck */
		ret = __gpufreq_buck_control(POWER_ON);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to control Buck: On (%d)", ret);
			ret = GPUFREQ_EINVAL;
			goto done_unlock;
		}
		__gpufreq_footprint_power_step(GPUFREQ_POWER_STEP_02);
		/* control clock */
		ret = __gpufreq_clock_control(POWER_ON);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to control CLOCK: On (%d)", ret);
			ret = GPUFREQ_EINVAL;
			goto done_unlock;
		}
		__gpufreq_footprint_power_step(GPUFREQ_POWER_STEP_03);
		/* control MTCMOS */
		ret = __gpufreq_mtcmos_control(POWER_ON);
		if (unlikely(ret < 0)) {
			GPUFREQ_LOGE("fail to control MTCMOS: On (%d)", ret);
			ret = GPUFREQ_EINVAL;
			goto done_unlock;
		}
		__gpufreq_footprint_power_step(GPUFREQ_POWER_STEP_04);

		/* free DVFS when power on */
		g_dvfs_state &= ~DVFS_POWEROFF;
		__gpufreq_kick_pbm(1);
	} else if (power == POWER_OFF && g_gpu.power_count == 0) {
		__gpufreq_footprint_power_step(GPUFREQ_POWER_STEP_05);
		/* freeze DVFS when power off */
		g_dvfs_state |= DVFS_POWEROFF;
		__gpufreq_footprint_power_step(GPUFREQ_POWER_STEP_06);
		/* control MTCMOS */
		ret = __gpufreq_mtcmos_control(POWER_OFF);
		if (unlikely(ret < 0)) {
			GPUFREQ_LOGE("fail to control MTCMOS: Off (%d)", ret);
			ret = GPUFREQ_EINVAL;
			goto done_unlock;
		}
		__gpufreq_footprint_power_step(GPUFREQ_POWER_STEP_07);
		/* control clock */
		ret = __gpufreq_clock_control(POWER_OFF);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to control CLOCK: Off (%d)", ret);
			ret = GPUFREQ_EINVAL;
			goto done_unlock;
		}
		__gpufreq_footprint_power_step(GPUFREQ_POWER_STEP_08);
		/* control Buck */
		ret = __gpufreq_buck_control(POWER_OFF);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to control Buck: Off (%d)", ret);
			ret = GPUFREQ_EINVAL;
			goto done_unlock;
		}
		__gpufreq_footprint_power_step(GPUFREQ_POWER_STEP_09);
		__gpufreq_kick_pbm(0);
	}
	/* return power count if successfully control power */
	ret = g_gpu.power_count;
done_unlock:
	GPUFREQ_LOGD("PWR_STATUS DONE");
	mutex_unlock(&gpufreq_lock);
	GPUFREQ_TRACE_END();
	return ret;
}
/*
 * API: commit DVFS to GPU by given OPP index
 * this is the main entrance of generic DVFS
 */
int __gpufreq_generic_commit_gpu(int target_oppidx, enum gpufreq_dvfs_state key)
{
	struct gpufreq_opp_info *opp_table = g_gpu.working_table;
	int opp_num = g_gpu.opp_num;
	int cur_oppidx = 0;
	unsigned int cur_freq = 0, target_freq = 0;
	unsigned int cur_volt = 0, target_volt = 0;
	unsigned int cur_vsram = 0, target_vsram = 0;
	int sb_idx = 0;
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("target_oppidx=%d, key=%d",
		target_oppidx, key);
	GPUFREQ_LOGD("opp_num(g_gpu.opp_num) = %d target_oppidx = %d",
		g_gpu.opp_num, target_oppidx);
	/* validate 0 <= target_oppidx < opp_num */
	if (target_oppidx < 0 || target_oppidx >= opp_num) {
		GPUFREQ_LOGE("invalid target GPU OPP index: %d (OPP_NUM: %d)",
			target_oppidx, opp_num);
		ret = GPUFREQ_EINVAL;
		goto done;
	}
	mutex_lock(&gpufreq_lock);
	/* check dvfs state */
	if (g_dvfs_state & ~key) {
		GPUFREQ_LOGD("unavailable dvfs state (0x%x)", g_dvfs_state);
		ret = GPUFREQ_SUCCESS;
		goto done_unlock;
	}
	/* randomly replace target index */
	if (g_stress_test_enable) {
		get_random_bytes(&target_oppidx, sizeof(target_oppidx));
		target_oppidx = target_oppidx < 0 ?
			(target_oppidx*-1) % opp_num : target_oppidx % opp_num;
	}
	cur_oppidx = g_gpu.cur_oppidx;
	cur_freq = __gpufreq_get_real_fgpu();
	cur_volt = __gpufreq_get_real_vgpu();
	cur_vsram = __gpufreq_get_real_vsram();
	target_freq = opp_table[target_oppidx].freq;
	target_volt = opp_table[target_oppidx].volt;
	target_vsram = opp_table[target_oppidx].vsram;
	GPUFREQ_LOGD("begin to commit GPU OPP index: (%d->%d)",
		cur_oppidx, target_oppidx);
	/* todo: GED log buffer (gpufreq_pr_logbuf) */
	if (target_freq > cur_freq) {
		/* voltage scaling */
		ret = __gpufreq_volt_scale_gpu(
		cur_volt, target_volt, cur_vsram, target_vsram);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to scale Vcore: (%d->%d), Vsram_gpu: (%d->%d)",
				cur_volt, target_volt, cur_vsram, target_vsram);
			goto done_unlock;
		}

		/* frequency scaling */
		ret = __gpufreq_freq_scale_gpu(cur_freq, target_freq);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to scale Freq: (%d->%d)",
				cur_freq, target_freq);
			goto done_unlock;
		}
	} else {
		/* frequency scaling */
		ret = __gpufreq_freq_scale_gpu(cur_freq, target_freq);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to scale Freq: (%d->%d)",
				cur_freq, target_freq);
			goto done_unlock;
		}

		/* voltage scaling */
		ret = __gpufreq_volt_scale_gpu(
		cur_volt, target_volt, cur_vsram, target_vsram);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to scale Vcore: (%d->%d), Vsram_gpu: (%d->%d)",
				cur_volt, target_volt, cur_vsram, target_vsram);
			goto done_unlock;
		}
	}
	g_gpu.cur_oppidx = target_oppidx;
	__gpufreq_footprint_oppidx(target_oppidx);

	__gpufreq_kick_pbm(1);
done_unlock:
	mutex_unlock(&gpufreq_lock);
done:
	GPUFREQ_TRACE_END();
	return ret;
}
/*
 * API: commit DVFS to STACK by given OPP index
 * this is the main entrance of generic DVFS
 */
int __gpufreq_generic_commit_stack(int target_oppidx, enum gpufreq_dvfs_state key)
{
	GPUFREQ_UNREFERENCED(target_oppidx);
	GPUFREQ_UNREFERENCED(key);
	return GPUFREQ_EINVAL;
}
/* API: fix OPP of GPU via given OPP index */
int __gpufreq_fix_target_oppidx_gpu(int oppidx)
{
	int opp_num = g_gpu.opp_num;
	int ret = GPUFREQ_SUCCESS;

	ret = __gpufreq_power_control(POWER_ON);
	if (unlikely(ret < 0)) {
		GPUFREQ_LOGE("fail to control power state: %d (%d)", POWER_ON, ret);
		goto done;
	}
	if (oppidx == -1) {
		__gpufreq_set_dvfs_state(false, DVFS_DEBUG_KEEP);
		ret = GPUFREQ_SUCCESS;
	} else if (oppidx >= 0 && oppidx < opp_num) {
		__gpufreq_set_dvfs_state(true, DVFS_DEBUG_KEEP);
		ret = __gpufreq_generic_commit_gpu(oppidx, DVFS_DEBUG_KEEP);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to commit GPU OPP index: %d (%d)", oppidx, ret);
			__gpufreq_set_dvfs_state(false, DVFS_DEBUG_KEEP);
		}
	} else {
		GPUFREQ_LOGE("invalid fixed OPP index: %d", oppidx);
		ret = GPUFREQ_EINVAL;
	}
	__gpufreq_power_control(POWER_OFF);
done:
	return ret;
}
/* API: fix Freq and Volt of GPU via given Freq and Volt */
int __gpufreq_fix_custom_freq_volt_gpu(unsigned int freq, unsigned int volt)
{
	int ret = GPUFREQ_SUCCESS;

	ret = __gpufreq_power_control(POWER_ON);
	if (unlikely(ret < 0)) {
		GPUFREQ_LOGE("fail to control power state: %d (%d)", POWER_ON, ret);
		goto done;
	}
	if (freq == 0 && volt == 0) {
		mutex_lock(&gpufreq_lock);
		g_dvfs_state &= ~DVFS_DEBUG_KEEP;
		mutex_unlock(&gpufreq_lock);
		ret = GPUFREQ_SUCCESS;
	} else if (freq > POSDIV_2_MAX_FREQ || freq < POSDIV_8_MIN_FREQ) {
		GPUFREQ_LOGE("invalid fixed freq: %d\n", freq);
		ret = GPUFREQ_EINVAL;
	} else if (volt > VGPU_MAX_VOLT || volt < VGPU_MIN_VOLT) {
		GPUFREQ_LOGE("invalid fixed volt: %d\n", volt);
		ret = GPUFREQ_EINVAL;
	} else {
		mutex_lock(&gpufreq_lock);
		g_dvfs_state |= DVFS_DEBUG_KEEP;
		mutex_unlock(&gpufreq_lock);
		ret = __gpufreq_custom_commit_gpu(freq, volt, DVFS_DEBUG_KEEP);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to custom commit GPU freq: %d, volt: %d (%d)",
				freq, volt, ret);
			mutex_lock(&gpufreq_lock);
			g_dvfs_state &= ~DVFS_DEBUG_KEEP;
			mutex_unlock(&gpufreq_lock);
		}
	}
	__gpufreq_power_control(POWER_OFF);
done:
	return ret;
}
void __gpufreq_set_timestamp(void)
{
	/* Write 1 into 0x13000130 bit 0 to enable timestamp register (TIMESTAMP).*/
	/* TIMESTAMP will be used by clGetEventProfilingInfo.*/
	writel(0x00000003, g_MFG_base + 0x130);
}
void __gpufreq_check_bus_idle(void)
{
	u32 val;
	/* MFG_QCHANNEL_CON (0x130000b4) bit [1:0] = 0x1 */
	writel(0x00000001, g_MFG_base + 0xb4);
	GPUFREQ_LOGD("@%s: 0x130000b4 val = 0x%x\n", __func__, readl(g_MFG_base + 0xb4));
	/* set register MFG_DEBUG_SEL (0x13000170) bit [7:0] = 0x03 */
	writel(0x00000003, g_MFG_base + 0x170);
	GPUFREQ_LOGD("@%s: 0x13000170 val = 0x%x\n", __func__, readl(g_MFG_base + 0x170));
	/* polling register MFG_DEBUG_TOP (0x13000178) bit 2 = 0x1 */
	/* => 1 for GPU (BUS) idle, 0 for GPU (BUS) non-idle */
	/* do not care about 0x13000174 */
	do {
		val = readl(g_MFG_base + 0x178);
		GPUFREQ_LOGD("@%s: 0x13000178 val = 0x%x\n", __func__, val);
	} while ((val & 0x4) != 0x4);
}
void __gpufreq_dump_infra_status(void)
{

	u32 val = 0;

	GPUFREQ_LOGI("== [GPUFREQ INFRA STATUS] ==");
	GPUFREQ_LOGI("mfgpll=%d, GPU[%d] Freq: %d, Vgpu: %d, Vsram: %d",
		mt_get_abist_freq(AD_MFGPLL_CK), g_gpu.cur_oppidx, g_gpu.cur_freq,
		g_gpu.cur_volt, g_gpu.cur_vsram);
}
/* API: get working OPP index of GPU limited by BATTERY_OC via given level */
int __gpufreq_get_batt_oc_idx(int batt_oc_level)
{
#if (GPUFREQ_BATT_OC_ENABLE && IS_ENABLED(CONFIG_MTK_BATTERY_OC_POWER_THROTTLING))
	if (batt_oc_level == BATTERY_OC_LEVEL_1)
		return __gpufreq_get_idx_by_fgpu(GPUFREQ_BATT_OC_FREQ);
	else
		return GPUPPM_RESET_IDX;
#else
	GPUFREQ_UNREFERENCED(batt_oc_level);
	return GPUPPM_KEEP_IDX;
#endif /* GPUFREQ_BATT_OC_ENABLE && CONFIG_MTK_BATTERY_OC_POWER_THROTTLING */
}
/* API: get working OPP index of GPU limited by BATTERY_PERCENT via given level */
int __gpufreq_get_batt_percent_idx(int batt_percent_level)
{
#if (GPUFREQ_BATT_PERCENT_ENABLE && IS_ENABLED(CONFIG_MTK_BATTERY_PERCENT_THROTTLING))
	if (batt_percent_level == BATTERY_PERCENT_LEVEL_1)
		return GPUFREQ_BATT_PERCENT_IDX - g_gpu.segment_upbound;
	else
		return GPUPPM_RESET_IDX;
#else
	GPUFREQ_UNREFERENCED(batt_percent_level);
	return GPUPPM_KEEP_IDX;
#endif /* GPUFREQ_BATT_PERCENT_ENABLE && CONFIG_MTK_BATTERY_PERCENT_THROTTLING */
}
/* API: get working OPP index of GPU limited by LOW_BATTERY via given level */
int __gpufreq_get_low_batt_idx(int low_batt_level)
{
#if (GPUFREQ_LOW_BATT_ENABLE && IS_ENABLED(CONFIG_MTK_LOW_BATTERY_POWER_THROTTLING))
	if (low_batt_level == LOW_BATTERY_LEVEL_2)
		return __gpufreq_get_idx_by_fgpu(GPUFREQ_LOW_BATT_FREQ);
	else
		return GPUPPM_RESET_IDX;
#else
	GPUFREQ_UNREFERENCED(low_batt_level);
	return GPUPPM_KEEP_IDX;
#endif /* GPUFREQ_LOW_BATT_ENABLE && CONFIG_MTK_LOW_BATTERY_POWER_THROTTLING */
}
/* API: enable/disable random OPP index substitution to do stress test */
void __gpufreq_set_stress_test(unsigned int mode)
{
	g_stress_test_enable = mode;
}
/* API: apply/restore Vaging to working table of GPU */
int __gpufreq_set_aging_mode(unsigned int mode)
{
	/* prevent from repeatedly applying aging */
	if (g_aging_enable ^ mode) {
		__gpufreq_apply_aging(mode);
		g_aging_enable = mode;
		/* set power info to working table */
		__gpufreq_measure_power();
		return GPUFREQ_SUCCESS;
	} else {
		return GPUFREQ_EINVAL;
	}
}
/**
 * ===============================================
 * Internal Function Definition
 * ===============================================
 */
static unsigned int __gpufreq_custom_init_enable(void)
{
	return GPUFREQ_CUST_INIT_ENABLE;
}
static unsigned int __gpufreq_dvfs_enable(void)
{
	return GPUFREQ_DVFS_ENABLE;
}
/* API: set/reset DVFS state with lock */
static void __gpufreq_set_dvfs_state(unsigned int set, unsigned int state)
{
	mutex_lock(&gpufreq_lock);
	if (set)
		g_dvfs_state |= state;
	else
		g_dvfs_state &= ~state;
	mutex_unlock(&gpufreq_lock);
}
/*
 * API: commit DVFS to GPU by given freq and volt
 * this is debug function and use it with caution
 */
static int __gpufreq_custom_commit_gpu(unsigned int target_freq,
	unsigned int target_volt, enum gpufreq_dvfs_state key)
{
	unsigned int cur_freq = 0, cur_volt = 0;
	unsigned int cur_vsram = 0, target_vsram = 0;
	unsigned int sb_volt = 0, sb_vsram = 0;
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("target_freq=%d, target_volt=%d, key=%d",
		target_freq, target_volt, key);
	mutex_lock(&gpufreq_lock);
	/* check dvfs state */
	if (g_dvfs_state & ~key) {
		GPUFREQ_LOGI("unavailable dvfs state (0x%x)", g_dvfs_state);
		ret = GPUFREQ_SUCCESS;
		goto done_unlock;
	}
	cur_freq = g_gpu.cur_freq;
	cur_volt = g_gpu.cur_volt;
	cur_vsram = g_gpu.cur_vsram;
	target_vsram = __gpufreq_get_vsram_by_vgpu(target_volt);
	GPUFREQ_LOGD("begin to custom commit freq: (%d->%d), volt: (%d->%d)",
		cur_freq, target_freq, cur_volt, target_volt);
	if (target_freq == cur_freq) {
		/* voltage scaling */
		ret = __gpufreq_volt_scale_gpu(
			cur_volt, target_volt, cur_vsram, target_vsram);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to scale Vgpu: (%d->%d), Vsram_gpu: (%d->%d)",
				cur_volt, target_volt, cur_vsram, target_vsram);
			goto done_unlock;
		}
	} else if (target_freq > cur_freq) {
		/* voltage scaling */
		while (target_volt != cur_volt) {
			if ((target_vsram - cur_volt) > MAX_BUCK_DIFF) {
				sb_volt = cur_volt + MAX_BUCK_DIFF;
				sb_vsram = __gpufreq_get_vsram_by_vgpu(sb_volt);
			} else {
				sb_volt = target_volt;
				sb_vsram = target_vsram;
			}
			ret = __gpufreq_volt_scale_gpu(
				cur_volt, sb_volt, cur_vsram, sb_vsram);
			if (unlikely(ret)) {
				GPUFREQ_LOGE("fail to scale Vgpu: (%d->%d), Vsram_gpu: (%d->%d)",
					cur_volt, sb_volt, cur_vsram, sb_vsram);
				goto done_unlock;
			}
			cur_volt = sb_volt;
			cur_vsram = sb_vsram;
		}
		/* frequency scaling */
		ret = __gpufreq_freq_scale_gpu(cur_freq, target_freq);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to scale Fgpu: (%d->%d)",
				cur_freq, target_freq);
			goto done_unlock;
		}
	} else {
		/* frequency scaling */
		ret = __gpufreq_freq_scale_gpu(cur_freq, target_freq);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to scale Fgpu: (%d->%d)",
				cur_freq, target_freq);
			goto done_unlock;
		}
		/* voltage scaling */
		while (target_volt != cur_volt) {
			if ((cur_vsram - target_volt) > MAX_BUCK_DIFF) {
				sb_volt = cur_volt - MAX_BUCK_DIFF;
				sb_vsram = __gpufreq_get_vsram_by_vgpu(sb_volt);
			} else {
				sb_volt = target_volt;
				sb_vsram = target_vsram;
			}
			ret = __gpufreq_volt_scale_gpu(
				cur_volt, sb_volt, cur_vsram, sb_vsram);
			if (unlikely(ret)) {
				GPUFREQ_LOGE("fail to scale Vgpu: (%d->%d), Vsram_gpu: (%d->%d)",
					cur_volt, sb_volt, cur_vsram, sb_vsram);
				goto done_unlock;
			}
			cur_volt = sb_volt;
			cur_vsram = sb_vsram;
		}
	}
	g_gpu.cur_oppidx = __gpufreq_get_idx_by_fgpu(target_freq);
	__gpufreq_footprint_oppidx(g_gpu.cur_oppidx);
done_unlock:
	mutex_unlock(&gpufreq_lock);
	GPUFREQ_TRACE_END();
	return ret;
}
static int __gpufreq_switch_clksrc(enum gpufreq_clk_src clksrc)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("clksrc=%d", clksrc);
	ret = clk_prepare_enable(g_clk->clk_mux);
	if (unlikely(ret)) {
		__gpufreq_abort(GPUFREQ_CCF_EXCEPTION,
			"fail to enable clk_mux(TOP_MUX_MFG) (%d)", ret);
		goto done;
	}
	if (clksrc == CLOCK_MAIN) {
		ret = clk_set_parent(g_clk->clk_mux, g_clk->clk_main_parent);
		if (unlikely(ret)) {
			__gpufreq_abort(GPUFREQ_CCF_EXCEPTION,
				"fail to switch GPU to CLOCK_MAIN (%d)", ret);
		goto done;
		}
	} else if (clksrc == CLOCK_SUB) {
		ret = clk_set_parent(g_clk->clk_mux, g_clk->clk_sub_parent);
		if (unlikely(ret)) {
			__gpufreq_abort(GPUFREQ_CCF_EXCEPTION,
				"fail to switch GPU to CLOCK_SUB (%d)", ret);
			goto done;
		}
	} else {
		GPUFREQ_LOGE("invalid clock source: %d (EINVAL)", clksrc);
		goto done;
	}
	clk_disable_unprepare(g_clk->clk_mux);
done:
	GPUFREQ_TRACE_END();
	return ret;
}
/*
 * API: calculate pcw for setting CON1
 * Fin is 26 MHz
 * VCO Frequency = Fin * N_INFO
 * MFGPLL output Frequency = VCO Frequency / POSDIV
 * N_INFO = MFGPLL output Frequency * POSDIV / FIN
 * N_INFO[21:14] = FLOOR(N_INFO, 8)
 */
static unsigned int __gpufreq_calculate_pcw(unsigned int freq, enum gpufreq_posdiv posdiv)
{
	/* [MT6765]
	 *    VCO range: 1.5GHz - 3.8GHz by divider 1/2/4/8/16,
	 *    PLL range: 125MHz - 3.8GHz,
	 *    | VCO MAX | VCO MIN | POSTDIV | PLL OUT MAX | PLL OUT MIN |
	 *    |  3800   |  1500   |    1    |   3800MHz   |  1500MHz    | (X)
	 *    |  3800   |  1500   |    2    |   1900MHz   |   750MHz    | (X)
	 *    |  3800   |  1500   |    4    |   950MHz    |   375MHz    | (O)
	 *    |  3800   |  1500   |    8    |   475MHz    |   187.5MHz  | (O)
	 *    |  3800   |  2000   |   16    |   237.5MHz  |   125MHz    | (X)
	 */
	unsigned int pcw = 0;
	/* FOllowing Legacy design */
	if ((freq >= POSDIV_16_MIN_FREQ) && (freq <= POSDIV_4_MAX_FREQ)) {
		pcw = (((freq / TO_MHZ_HEAD * (1 << posdiv)) << DDS_SHIFT)
			/ MFGPLL_FIN + ROUNDING_VALUE) / TO_MHZ_TAIL;
	} else {
		GPUFREQ_LOGE("out of range Freq: %d (EINVAL)", freq);
	}
	return pcw;
}
static enum gpufreq_posdiv __gpufreq_get_real_posdiv_gpu(void)
{
	unsigned int mfgpll = 0;
	enum gpufreq_posdiv posdiv = POSDIV_POWER_1;

	mfgpll = readl(MFGPLL_CON1);
	posdiv = (mfgpll & (0x7 << POSDIV_SHIFT)) >> POSDIV_SHIFT;
	return posdiv;
}
static enum gpufreq_posdiv __gpufreq_get_posdiv_by_fgpu(unsigned int freq)
{
	/*
	 *    VCO range: 1.5GHz - 3.8GHz by divider 1/2/4/8/16,
	 *    PLL range: 125MHz - 3.8GHz,
	 *    | VCO MAX | VCO MIN | POSTDIV | PLL OUT MAX | PLL OUT MIN |
	 *    |  3800   |  1500   |    1    |   3800MHz   |  1500MHz    | (X)
	 *    |  3800   |  1500   |    2    |   1900MHz   |   750MHz    | (X)
	 *    |  3800   |  1500   |    4    |   950MHz    |   375MHz    | (O)
	 *    |  3800   |  1500   |    8    |   475MHz    |   187.5MHz  | (O)
	 *    |  3800   |  2000   |   16    |   237.5MHz  |   125MHz    | (X)
	 */
	if (freq < POSDIV_4_MIN_FREQ)
		return POSDIV_POWER_8;
	else
		return POSDIV_POWER_4;
}
/* API: scale Freq of GPU via CON1 Reg or FHCTL */
static int __gpufreq_freq_scale_gpu(unsigned int freq_old, unsigned int freq_new)
{
	enum gpufreq_posdiv cur_posdiv = POSDIV_POWER_1;
	enum gpufreq_posdiv target_posdiv = POSDIV_POWER_1;
	unsigned int pcw = 0;
	unsigned int pll = 0;
	unsigned int parking = false;
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("freq_old=%d, freq_new=%d", freq_old, freq_new);
	GPUFREQ_LOGI("begin to scale Fgpu: (%d->%d)", freq_old, freq_new);
	/*
	 * MFGPLL_CON1[31:31]: MFGPLL_SDM_PCW_CHG
	 * MFGPLL_CON1[26:24]: MFGPLL_POSDIV
	 * MFGPLL_CON1[21:0] : MFGPLL_SDM_PCW (DDS)
	 */
	cur_posdiv = __gpufreq_get_real_posdiv_gpu();
	target_posdiv = __gpufreq_get_posdiv_by_fgpu(freq_new);
	/* compute PCW based on target Freq */
	pcw = __gpufreq_calculate_pcw(freq_new, target_posdiv);
	if (!pcw) {
		GPUFREQ_LOGE("invalid PCW: 0x%x", pcw);
		goto done;
	}
	pll = (0x80000000) | (target_posdiv << POSDIV_SHIFT) | pcw;
//TODO: GKI check FHCTL option
#if (GPUFREQ_FHCTL_ENABLE && IS_ENABLED(CONFIG_MTK_FREQ_HOPPING))
	if (target_posdiv != cur_posdiv)
		parking = true;
	else
		parking = false;
#else
	/* force parking if FHCTL isn't ready */
	parking = true;
	GPUFREQ_LOGI("Fgpu: %d, PCW: 0x%x, CON1: 0x%08x", g_gpu.cur_freq, pcw, pll);
#endif
	if (parking) {
		/* mfgpll_ck to syspll_d3 */
		__gpufreq_switch_clksrc(CLOCK_SUB);
		/* dds = MFGPLL_CON1[21:0], POST_DIVIDER = MFGPLL_CON1[24:26] */
		writel(pll, MFGPLL_CON1);
		udelay(20);
		/* syspll_d3 to mfgpll_ck */
		__gpufreq_switch_clksrc(CLOCK_MAIN);
	} else {
#if (GPUFREQ_FHCTL_ENABLE && IS_ENABLED(CONFIG_MTK_FREQ_HOPPING))
// TODO: GKI check fhctl api
		mt_dfs_general_pll(3, dds);
#endif
	}
	g_gpu.cur_freq = __gpufreq_get_real_fgpu();
	if (unlikely(g_gpu.cur_freq != freq_new))
		__gpufreq_abort(GPUFREQ_GPU_EXCEPTION,
			"inconsistent scaled Fgpu, cur_freq: %d, target_freq: %d",
			g_gpu.cur_freq, freq_new);
	GPUFREQ_LOGD("Fgpu: %d, PCW: 0x%x, CON1: 0x%08x", g_gpu.cur_freq, pcw, pll);
	/* because return value is different across the APIs */
	ret = GPUFREQ_SUCCESS;
	/* notify gpu freq change to DDK */
	mtk_notify_gpu_freq_change(0, freq_new);
done:
	GPUFREQ_TRACE_END();
	return ret;
}

/* API: scale vgpu and vsram via PMIC */
static int __gpufreq_volt_scale_gpu(
	unsigned int vgpu_old, unsigned int vgpu_new,
	unsigned int vsram_old, unsigned int vsram_new)
{
	int ret = GPUFREQ_SUCCESS;
	//vgpu_new = 80000;
	//GPUFREQ_LOGI("force VGPU to 0.8V");
	GPUFREQ_TRACE_START("vgpu_old=%d, vgpu_new=%d, vsram_old=%d, vsram_new=%d",
		vgpu_old, vgpu_new, vsram_old, vsram_new);
	GPUFREQ_LOGD("begin to scale Vgpu: (%d->%d), Vsram_gpu: (%d->%d)",
		vgpu_old, vgpu_new, vsram_old, vsram_new);
		ret = regulator_set_voltage(
				g_pmic->mtk_pm_vgpu,
				vgpu_new * 10,
				VGPU_MAX_VOLT * 10 + 125);
		if (unlikely(ret)) {
			__gpufreq_abort(GPUFREQ_PMIC_EXCEPTION, "fail to set VSRAM_G (%d)", ret);
			goto done;
		}
	//TODO: Vcore slew Rate calculation : hardcode
	//Max switch 80000 ~ 65000 = 15000
	//steps = 15000/625 = 24
	//sfchg rate rising/ falling = 2
	//delay (steps+1)*sfchg + 52
	udelay(250);
	g_gpu.cur_volt = __gpufreq_get_real_vgpu();
	if (unlikely(g_gpu.cur_volt < vgpu_new))
		__gpufreq_abort(GPUFREQ_GPU_EXCEPTION,
			"inconsistent scaled Vgpu, cur_volt: %d, target_volt: %d",
			g_gpu.cur_volt, vgpu_new);
	/*Legacy has fixed Vsram = 87500. so not updating the vsram */
	/* todo: GED log buffer (gpufreq_pr_logbuf) */
	GPUFREQ_LOGD("Updated Vgpu: %d, Vsram: %d",
		g_gpu.cur_volt, g_gpu.cur_vsram);
done:
	GPUFREQ_TRACE_END();
	return ret;
}
/*
 * API: dump power/clk status when bring-up
 */
static void __gpufreq_dump_bringup_status(struct platform_device *pdev)
{

	GPUFREQ_LOGI("[TOP] FMETER: %d, CON1: %d",
		__gpufreq_get_fmeter_fgpu(), __gpufreq_get_real_fgpu());
}
static unsigned int __gpufreq_get_fmeter_fgpu(void)
{
#if IS_ENABLED(CONFIG_COMMON_CLK_MT6765)
	return mt_get_abist_freq(25);
#else
	return 0;
#endif
}
/*
 * API: get real current frequency from CON1 (khz)
 * Freq = ((PLL_CON1[21:0] * 26M) / 2^14) / 2^PLL_CON1[26:24]
 */
static unsigned int __gpufreq_get_real_fgpu(void)
{
	unsigned int mfgpll = 0;
	unsigned int posdiv_power = 0;
	unsigned int freq = 0;
	unsigned int pcw = 0;

	GPUFREQ_LOGE("%s: MFGPLL_CON1 = 0x%x", __func__, MFGPLL_CON1);
	mfgpll = readl(MFGPLL_CON1);
	pcw = mfgpll & (0x3FFFFF);
	posdiv_power = (mfgpll & (0x7 << POSDIV_SHIFT)) >> POSDIV_SHIFT;
	freq = (((pcw * TO_MHZ_TAIL + ROUNDING_VALUE) * MFGPLL_FIN) >> DDS_SHIFT) /
		(1 << posdiv_power) * TO_MHZ_HEAD;
	GPUFREQ_LOGD("@%s: freq = %d KHz", __func__, freq);
	return freq;
}
/* API: get real current Vgpu from regulator (mV * 100) */
static unsigned int __gpufreq_get_real_vgpu(void)
{
	unsigned int volt = 0;

	if (regulator_is_enabled(g_pmic->mtk_pm_vgpu))
		/* regulator_get_voltage return volt with uV */
		volt = regulator_get_voltage(g_pmic->reg_vcore) / 10;
	GPUFREQ_LOGD("@%s: voltage = %d mV", __func__, volt);
	return volt;
}
/* API: get real current Vsram from regulator (mV * 100) */
static unsigned int __gpufreq_get_real_vsram(void)
{
	GPUFREQ_LOGD("@%s: return fixed vsram 87500\n", __func__);
	return 87500;
}
static int __gpufreq_clock_control(enum gpufreq_power_state power)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("power=%d", power);
	if (power == POWER_ON) {
		ret = clk_prepare_enable(g_clk->clk_mux);
		if (unlikely(ret)) {
			__gpufreq_abort(GPUFREQ_CCF_EXCEPTION,
				"fail to enable clk_mux (%d)", ret);
			goto done;
		}
		__gpufreq_switch_clksrc(CLOCK_MAIN);
		g_gpu.cg_count++;
	} else {
		__gpufreq_switch_clksrc(CLOCK_SUB);
		clk_disable_unprepare(g_clk->clk_mux);
		g_gpu.cg_count--;
	}
done:
	GPUFREQ_TRACE_END();
	return ret;
}
static int __gpufreq_mtcmos_control(enum gpufreq_power_state power)
{
	int ret = GPUFREQ_SUCCESS;
	u32 val = 0;

	GPUFREQ_TRACE_START("power=%d", power);
	if (power == POWER_ON) {
#ifdef MT_GPUFREQ_SRAM_DEBUG
		aee_rr_rec_gpu_dvfs_status(0x70 | (aee_rr_curr_gpu_dvfs_status() & 0x0F));
#endif
		/* MFG1 on by CCF */
		ret = pm_runtime_get_sync(g_mtcmos->pd_mfg_async);
		if (unlikely(ret < 0)) {
			__gpufreq_abort(GPUFREQ_CCF_EXCEPTION,
				"fail to enable pd_mfg_async (%d)", ret);
			goto done;
		}
		ret = pm_runtime_get_sync(g_mtcmos->pd_mfg);
		if (unlikely(ret < 0)) {
			__gpufreq_abort(GPUFREQ_CCF_EXCEPTION,
				"fail to enable pd_mfg (%d)", ret);
			goto done;
		}
		if (!g_EnableHWAPM_state) {
			ret = pm_runtime_get_sync(g_mtcmos->pd_mfg_core0);
			if (unlikely(ret < 0)) {
				GPUFREQ_LOGI("@%s: dump clk/pd reg after enabling pd\n", __func__);
				print_subsys_reg_mt6765(0);//dump mnuxes
				print_subsys_reg_mt6765(3);//dump pd
				__gpufreq_abort(GPUFREQ_CCF_EXCEPTION,
					"fail to enable pd_mfg_core0 (%d)", ret);
				goto done;
			}
		}
#ifdef MT_GPUFREQ_SRAM_DEBUG
		aee_rr_rec_gpu_dvfs_status(0x80 | (aee_rr_curr_gpu_dvfs_status() & 0x0F));
#endif
		GPUFREQ_LOGD("@%s: enable MTCMOS done\n", __func__);
		g_gpu.mtcmos_count++;
	} else {
#ifdef MT_GPUFREQ_SRAM_DEBUG
		aee_rr_rec_gpu_dvfs_status(0x90 | (aee_rr_curr_gpu_dvfs_status() & 0x0F));
#endif
		if (!g_EnableHWAPM_state) {
			ret = pm_runtime_put_sync(g_mtcmos->pd_mfg_core0);
			if (unlikely(ret < 0)) {
				GPUFREQ_LOGI("@%s: dump clk/pd reg after disabling pd\n", __func__);
				print_subsys_reg_mt6765(0);//dump muxes
				print_subsys_reg_mt6765(3);//dump pd
				__gpufreq_abort(GPUFREQ_CCF_EXCEPTION,
					"fail to disable pd_mfg_core0 (%d)", ret);
				goto done;
			}
		}
		ret = pm_runtime_put_sync(g_mtcmos->pd_mfg);
		if (unlikely(ret < 0)) {
			__gpufreq_abort(GPUFREQ_CCF_EXCEPTION,
				"fail to disable pd_mfg (%d)", ret);
			goto done;
		}
		ret = pm_runtime_put_sync(g_mtcmos->pd_mfg_async);
		if (unlikely(ret < 0)) {
			__gpufreq_abort(GPUFREQ_CCF_EXCEPTION,
				"fail to enable pd_mfg_async (%d)", ret);
			goto done;
		}
#ifdef MT_GPUFREQ_SRAM_DEBUG
		aee_rr_rec_gpu_dvfs_status(0xA0 | (aee_rr_curr_gpu_dvfs_status() & 0x0F));
#endif
		GPUFREQ_LOGD("@%s: disable MTCMOS done\n", __func__);
		g_gpu.mtcmos_count--;
#if GPUFREQ_CHECK_MTCMOS_PWR_STATUS
		//TODO:GKI check mtcmos power
#endif /* GPUFREQ_CHECK_MTCMOS_PWR_STATUS */
	}
done:
	GPUFREQ_TRACE_END();
	return ret;
}
static int __gpufreq_buck_control(enum gpufreq_power_state power)
{
	int ret = GPUFREQ_SUCCESS;
	unsigned int volt = __gpufreq_get_real_vgpu();

	GPUFREQ_LOGD("@%s: volt = %d ", __func__, volt);
	GPUFREQ_TRACE_START("power=%d", power);
	/* power on */
	if (power == POWER_ON) {
		ret = regulator_enable(g_pmic->mtk_pm_vgpu);
		if (unlikely(ret)) {
			__gpufreq_abort(GPUFREQ_PMIC_EXCEPTION, "fail to enable VCORE (%d)",
			ret);
			goto done;
		}
		g_gpu.buck_count++;
	/* power off */
	} else {
		ret = regulator_disable(g_pmic->mtk_pm_vgpu);
		if (unlikely(ret)) {
			__gpufreq_abort(GPUFREQ_PMIC_EXCEPTION, "fail to disable VCORE (%d)", ret);
			goto done;
		}
		g_gpu.buck_count--;
	}
done:
	GPUFREQ_TRACE_END();
	return ret;
}

/* API: init first OPP idx by init freq set in preloader */
static int __gpufreq_init_opp_idx(void)
{
	struct gpufreq_opp_info *working_table = g_gpu.working_table;
	unsigned int cur_freq = 0;
	int oppidx = 0;
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START();
	/* get current GPU OPP idx by freq set in preloader */
	cur_freq = __gpufreq_get_real_fgpu();
	GPUFREQ_LOGI("preloader init freq: %d", cur_freq);
	/* decide first OPP idx by custom setting */
	if (__gpufreq_custom_init_enable()) {
		oppidx = GPUFREQ_CUST_INIT_OPPIDX;
		GPUFREQ_LOGI("custom init GPU OPP index: %d, Freq: %d",
			oppidx, working_table[oppidx].freq);
	/* decide first OPP idx by SRAMRC setting */
	} else {
		/* Restrict freq to legal opp idx */
		if (cur_freq >= working_table[0].freq) {
			oppidx = 0;
		} else if (cur_freq <= working_table[g_gpu.min_oppidx].freq) {
			oppidx = g_gpu.min_oppidx;
			/* Mapping freq to the first smaller opp idx */
		} else {
			//Legecy design to set opp 0
			oppidx = 0;
		}
	}
	GPUFREQ_LOGI("init_opp_idx : GPU OPP index: %d, Freq: %d",
			oppidx, working_table[oppidx].freq);
	g_gpu.cur_oppidx = oppidx;
	g_gpu.cur_freq = cur_freq;
	g_gpu.cur_volt = __gpufreq_get_real_vgpu();
	g_gpu.cur_vsram = __gpufreq_get_real_vsram();
	/* init first OPP index */
	if (!__gpufreq_dvfs_enable()) {
		g_dvfs_state = DVFS_DISABLE;
		GPUFREQ_LOGI("DVFS state: 0x%x, disable DVFS", g_dvfs_state);
		/* set OPP once if DVFS is disabled but custom init is enabled */
		if (__gpufreq_custom_init_enable()) {
			ret = __gpufreq_generic_commit_gpu(oppidx, DVFS_DISABLE);
			if (unlikely(ret)) {
				GPUFREQ_LOGE("fail to commit GPU OPP index: %d (%d)",
					oppidx, ret);
				goto done;
			}
		}
	} else {
		g_dvfs_state = DVFS_FREE;
		GPUFREQ_LOGI("DVFS state: 0x%x, enable DVFS", g_dvfs_state);
		ret = __gpufreq_generic_commit_gpu(oppidx, DVFS_FREE);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to commit GPU OPP index: %d (%d)",
				oppidx, ret);
			goto done;
		}
	}
done:
	GPUFREQ_TRACE_END();
	return ret;
}
/* API: calculate power of every OPP in working table */
static void __gpufreq_measure_power(void)
{
	unsigned int freq = 0, volt = 0;
	unsigned int p_total = 0, p_dynamic = 0, p_leakage = 0;
	int i = 0;
	struct gpufreq_opp_info *working_table = g_gpu.working_table;
	int opp_num = g_gpu.opp_num;

	for (i = 0; i < opp_num; i++) {
		freq = working_table[i].freq;
		volt = working_table[i].volt;
		p_leakage = __gpufreq_get_lkg_pgpu(volt);
		p_dynamic = __gpufreq_get_dyn_pgpu(freq, volt);
		p_total = p_dynamic + p_leakage;
		working_table[i].power = p_total;
		GPUFREQ_LOGD("GPU[%02d] power: %d (dynamic: %d, leakage: %d)",
			i, p_total, p_dynamic, p_leakage);
	}
}
/* API: resume dvfs to free run */
static void __gpufreq_resume_dvfs(void)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START();

	__gpufreq_power_control(POWER_OFF);

	if (unlikely(ret < 0))
		GPUFREQ_LOGE("fail to control power state: %d (%d)", POWER_OFF, ret);

	__gpufreq_set_dvfs_state(false, DVFS_DISABLE);

	GPUFREQ_LOGI("resume DVFS, state: 0x%x", g_dvfs_state);

	GPUFREQ_TRACE_END();
}
/* API: fix Freq and Volt of GPU via given Freq and Volt */
int __gpufreq_fix_freq_volt_gpu(unsigned int freq, unsigned int volt)
{
	int ret = GPUFREQ_SUCCESS;

	ret = __gpufreq_power_control(POWER_ON);
	if (unlikely(ret < 0)) {
		GPUFREQ_LOGE("fail to control power state: %d (%d)", POWER_ON, ret);
		goto done;
	}

	if (freq == 0 && volt == 0) {
		ret = GPUFREQ_SUCCESS;
	} else if (freq > POSDIV_2_MAX_FREQ || freq < POSDIV_8_MIN_FREQ) {
		GPUFREQ_LOGE("invalid fixed freq: %d\n", freq);
		ret = GPUFREQ_EINVAL;
	} else if (volt > VGPU_MAX_VOLT || volt < VGPU_MIN_VOLT) {
		GPUFREQ_LOGE("invalid fixed volt: %d\n", volt);
		ret = GPUFREQ_EINVAL;
	} else {
		ret = __gpufreq_custom_commit_gpu(freq, volt, DVFS_DISABLE);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to custom commit GPU freq: %d, volt: %d (%d)",
				freq, volt, ret);
		}
	}

	__gpufreq_power_control(POWER_OFF);

done:
	return ret;
}

/* API: pause dvfs to given freq and volt */
static int __gpufreq_pause_dvfs(unsigned int oppidx)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START();

	__gpufreq_set_dvfs_state(true, DVFS_DISABLE);

	ret = __gpufreq_power_control(POWER_ON);
	if (unlikely(ret < 0)) {
		GPUFREQ_LOGE("fail to control power state: %d (%d)", POWER_ON, ret);
		__gpufreq_set_dvfs_state(false, DVFS_AGING_KEEP);
		goto done_unlock;
	}

	//if can not find DISABLE_VOLT in working table.
	if (g_gpu.working_table[oppidx].volt < GPU_DVFS_PTPOD_DISABLE_VOLT) {
		GPUFREQ_LOGI("Cann't find PTPOD_DISABLE_VOLT in working table, use it directly.");
		ret = __gpufreq_fix_freq_volt_gpu(
			g_gpu.working_table[oppidx].freq,
			GPU_DVFS_PTPOD_DISABLE_VOLT);
	} else {
		//DISABLE_VOLT is in the working table.
		ret = __gpufreq_generic_commit_gpu(oppidx, DVFS_DISABLE);
	}

	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to commit GPU OPP index: %d (%d)",
			oppidx, ret);
		goto done_unlock;
	}

	GPUFREQ_LOGI("pause DVFS at OPP index(%d), state: 0x%x",
		oppidx, g_dvfs_state);

done_unlock:
	GPUFREQ_TRACE_END();
	return ret;
}

/* API: apply aging volt diff to working table */
static void __gpufreq_apply_aging(unsigned int apply_aging)
{
	int i = 0;
	struct gpufreq_opp_info *working_table = g_gpu.working_table;
	int opp_num = g_gpu.opp_num;

	mutex_lock(&gpufreq_lock);
	for (i = 0; i < opp_num; i++) {
		if (apply_aging)
			working_table[i].volt -= working_table[i].vaging;
		else
			working_table[i].volt += working_table[i].vaging;
		working_table[i].vsram = __gpufreq_get_vsram_by_vgpu(working_table[i].volt);
		GPUFREQ_LOGD("apply Vaging: %d, GPU[%02d] Volt: %d, Vsram: %d",
			apply_aging, i, working_table[i].volt, working_table[i].vsram);
	}
	mutex_unlock(&gpufreq_lock);
}
/* API: apply given adjustment table to signed table */
static void __gpufreq_apply_adjust(struct gpufreq_adj_info *adj_table, int adj_num)
{
	int i = 0;
	int oppidx = 0;
	struct gpufreq_opp_info *signed_table = g_gpu.signed_table;
	int opp_num = g_gpu.signed_opp_num;

	GPUFREQ_TRACE_START("adj_table=0x%x, adj_num=%d",
		adj_table, adj_num);
	if (!adj_table) {
		GPUFREQ_LOGE("null adjustment table (EINVAL)");
		goto done;
	}
	mutex_lock(&gpufreq_lock);
	for (i = 0; i < adj_num; i++) {
		oppidx = adj_table[i].oppidx;
		if (oppidx >= 0 && oppidx < opp_num) {
			signed_table[oppidx].freq = adj_table[i].freq ?
				adj_table[i].freq : signed_table[oppidx].freq;
			signed_table[oppidx].volt = adj_table[i].volt ?
				adj_table[i].volt : signed_table[oppidx].volt;
			signed_table[oppidx].vsram = adj_table[i].vsram ?
				adj_table[i].vsram : signed_table[oppidx].vsram;
			signed_table[oppidx].vaging = adj_table[i].vaging ?
				adj_table[i].vaging : signed_table[oppidx].vaging;
		} else {
			GPUFREQ_LOGE("invalid adj_table[%d].oppidx: %d", i, adj_table[i].oppidx);
		}
		GPUFREQ_LOGD("%s[%02d*] Freq: %d, Volt: %d, Vsram: %d, Vaging: %d",
			oppidx, signed_table[oppidx].freq,
			signed_table[oppidx].volt,
			signed_table[oppidx].vsram,
			signed_table[oppidx].vaging);
	}
	mutex_unlock(&gpufreq_lock);
done:
	GPUFREQ_TRACE_END();
}
static void __gpufreq_aging_adjustment(void)
{

}
static void __gpufreq_custom_adjustment(void)
{

}
static void __gpufreq_segment_adjustment(struct platform_device *pdev)
{

}
static void __gpufreq_init_shader_present(void)
{

}
/*
 * 1. init working OPP range
 * 2. init working OPP table = default + adjustment
 * 3. init springboard table
 */
static int __gpufreq_init_opp_table(struct platform_device *pdev)
{
	unsigned int segment_id = 0;
	int i = 0, j = 0, table_size = 0;
	int ret = GPUFREQ_SUCCESS;
	/* init working OPP range */
	segment_id = g_gpu.segment_id;
	/* setup segment max/min opp_idx */
	if (segment_id == MT6762M_SEGMENT) {
		g_gpu.signed_table = g_default_gpu_segment1;
		table_size = ARRAY_SIZE(g_default_gpu_segment1);
	} else if (segment_id == MT6762_SEGMENT) {
		g_gpu.signed_table = g_default_gpu_segment2;
		table_size = ARRAY_SIZE(g_default_gpu_segment2);
	} else if (segment_id == MT6765_SEGMENT) {
		g_gpu.signed_table = g_default_gpu_segment3;
		table_size = ARRAY_SIZE(g_default_gpu_segment3);
	} else if (segment_id == MT6765T_SEGMENT) {
		g_gpu.signed_table = g_default_gpu_segment4;
		table_size = ARRAY_SIZE(g_default_gpu_segment4);
	} else if (segment_id == MT6762D_SEGMENT) {
		g_gpu.signed_table = g_default_gpu_segment5;
		table_size = ARRAY_SIZE(g_default_gpu_segment5);
	} else { /*efuse =0 ; MT6765_SEGMENT*/
		g_gpu.signed_table = g_default_gpu_segment3;
		table_size = ARRAY_SIZE(g_default_gpu_segment3);
	}
	g_gpu.segment_upbound = 0;
	g_gpu.segment_lowbound = table_size - 1;
	g_gpu.signed_opp_num = table_size;
	g_gpu.max_oppidx = 0;
	g_gpu.min_oppidx = g_gpu.segment_lowbound - g_gpu.segment_upbound;
	g_gpu.opp_num = g_gpu.min_oppidx + 1;
	GPUFREQ_LOGD("number of signed GPU OPP: %d, upper and lower bound: [%d, %d]",
		g_gpu.signed_opp_num, g_gpu.segment_upbound, g_gpu.segment_lowbound);
	GPUFREQ_LOGI("number of working GPU OPP: %d, max and min OPP index: [%d, %d]",
		g_gpu.opp_num, g_gpu.max_oppidx, g_gpu.min_oppidx);
	//g_gpu.signed_table = g_default_gpu;
	/* apply segment adjustment to GPU signed table */
	__gpufreq_segment_adjustment(pdev);
	/* after these, signed table is settled down */
	g_gpu.working_table = kcalloc(g_gpu.opp_num, sizeof(struct gpufreq_opp_info), GFP_KERNEL);
	if (!g_gpu.working_table) {
		GPUFREQ_LOGE("fail to alloc gpufreq_opp_info (ENOMEM)");
		ret = GPUFREQ_ENOMEM;
		goto done;
	}
	for (i = 0; i < g_gpu.opp_num; i++) {
		j = i + g_gpu.segment_upbound;
		g_gpu.working_table[i].freq = g_gpu.signed_table[j].freq;
		g_gpu.working_table[i].volt = g_gpu.signed_table[j].volt;
		g_gpu.working_table[i].vsram = g_gpu.signed_table[j].vsram;
		g_gpu.working_table[i].posdiv = g_gpu.signed_table[j].posdiv;
		g_gpu.working_table[i].vaging = g_gpu.signed_table[j].vaging;
		g_gpu.working_table[i].power = g_gpu.signed_table[j].power;
		GPUFREQ_LOGD("GPU[%02d] Freq: %d, Volt: %d, Vsram: %d, Vaging: %d",
			i, g_gpu.working_table[i].freq, g_gpu.working_table[i].volt,
			g_gpu.working_table[i].vsram, g_gpu.working_table[i].vaging);
	}
	/* set power info to working table */
	__gpufreq_measure_power();
	__gpufreq_setup_opp_power_table(g_gpu.opp_num);
done:
	return ret;
}
static void __iomem *__gpufreq_of_ioremap(const char *node_name, int idx)
{
	struct device_node *node;
	void __iomem *base;

	node = of_find_compatible_node(NULL, NULL, node_name);
	if (node)
		base = of_iomap(node, idx);
	else
		base = NULL;
	return base;
}
static int __gpufreq_init_segment_id(struct platform_device *pdev)
{
	unsigned int efuse_id = 0;
	unsigned int segment_id = 0;
#if IS_ENABLED(CONFIG_MTK_DEVINFO)
	struct nvmem_cell *efuse_cell;
	unsigned int *efuse_buf;
	size_t efuse_len;
#endif
#if IS_ENABLED(CONFIG_MTK_DEVINFO)
	efuse_cell = nvmem_cell_get(&pdev->dev, "efuse_segment_cell");
	if (IS_ERR(efuse_cell)) {
		GPUFREQ_LOGE("@%s: cannot get efuse_segment_cell\n", __func__);
		PTR_ERR(efuse_cell);
	}
	efuse_buf = (unsigned int *)nvmem_cell_read(efuse_cell, &efuse_len);
	nvmem_cell_put(efuse_cell);
	if (IS_ERR(efuse_buf)) {
		GPUFREQ_LOGE("@%s: cannot get efuse_buf of efuse segment\n", __func__);
	    PTR_ERR(efuse_buf);
	}
	efuse_id = (*efuse_buf & 0xFF);
	kfree(efuse_buf);
#else
	efuse_id = 0x0;
#endif /* CONFIG_MTK_DEVINFO */
	if (efuse_id == 0x8 || efuse_id == 0xf) {
		/* 6762M */
		segment_id = MT6762M_SEGMENT;
	} else if (efuse_id == 0x1 || efuse_id == 0x7
		|| efuse_id == 0x9) {
		/* 6762 */
		segment_id = MT6762_SEGMENT;
	} else if (efuse_id == 0x2 || efuse_id == 0x5) {
		/* SpeedBin */
		segment_id = MT6765T_SEGMENT;
	} else if (efuse_id == 0x20) {
		/* 6762D */
		segment_id = MT6762D_SEGMENT;
	} else {
		/* Other Version, set default segment */
		segment_id = MT6765_SEGMENT;
	}
	GPUFREQ_LOGD("@%s: efuse_id = 0x%08X, segment_id = %d\n", __func__, efuse_id, segment_id);
	g_gpu.segment_id = segment_id;
	return 0;
}
static int __gpufreq_init_mtcmos(struct platform_device *pdev)
{
	int ret = GPUFREQ_SUCCESS;
	struct device *dev = &pdev->dev;

	GPUFREQ_TRACE_START("pdev=0x%x", pdev);
	g_mtcmos = kzalloc(sizeof(struct gpufreq_mtcmos_info), GFP_KERNEL);
	if (!g_mtcmos) {
		GPUFREQ_LOGE("fail to alloc gpufreq_mtcmos_info (ENOMEM)");
		ret = GPUFREQ_ENOMEM;
		goto done;
	}
	g_mtcmos->pd_mfg = dev_pm_domain_attach_by_name(dev, "pd_mfg");
	if (IS_ERR_OR_NULL(g_mtcmos->pd_mfg)) {
		ret = g_mtcmos->pd_mfg ? PTR_ERR(g_mtcmos->pd_mfg) : GPUFREQ_ENODEV;
		__gpufreq_abort(GPUFREQ_CCF_EXCEPTION, "fail to get mfg0_dev (%ld)", ret);
		goto done;
	}
	dev_pm_syscore_device(g_mtcmos->pd_mfg, true);
	g_mtcmos->pd_mfg_async = dev_pm_domain_attach_by_name(dev, "pd_mfg_async");
	if (IS_ERR_OR_NULL(g_mtcmos->pd_mfg_async)) {
		ret = g_mtcmos->pd_mfg_async ? PTR_ERR(g_mtcmos->pd_mfg_async) : GPUFREQ_ENODEV;
		__gpufreq_abort(GPUFREQ_CCF_EXCEPTION, "fail to get mfg1_dev (%ld)", ret);
		goto done;
	}
	dev_pm_syscore_device(g_mtcmos->pd_mfg_async, true);
	g_mtcmos->pd_mfg_core0 = dev_pm_domain_attach_by_name(dev, "pd_mfg_core0");
	if (IS_ERR_OR_NULL(g_mtcmos->pd_mfg_core0)) {
		ret = g_mtcmos->pd_mfg_core0 ? PTR_ERR(g_mtcmos->pd_mfg_core0) : GPUFREQ_ENODEV;
		__gpufreq_abort(GPUFREQ_CCF_EXCEPTION, "fail to get mfg2_dev (%ld)", ret);
		goto done;
	}
	dev_pm_syscore_device(g_mtcmos->pd_mfg_core0, true);
	GPUFREQ_LOGI("@%s: pd_mfg is at 0x%p, pd_mfg_async is at 0x%p, \t"
			"pd_mfg_core0 is at 0x%p\n",
			__func__, g_mtcmos->pd_mfg, g_mtcmos->pd_mfg_async, g_mtcmos->pd_mfg_core0);
done:
	GPUFREQ_TRACE_END();
	return ret;
}
static int __gpufreq_init_clk(struct platform_device *pdev)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("pdev=0x%x", pdev);
	g_clk = kzalloc(sizeof(struct g_clk_info), GFP_KERNEL);
	if (g_clk == NULL) {
		GPUFREQ_LOGE("@%s: cannot allocate g_clk\n", __func__);
		GPUFREQ_TRACE_END();
		return -ENOMEM;
	}
	g_clk->clk_mux = devm_clk_get(&pdev->dev, "clk_mux");
	if (IS_ERR(g_clk->clk_mux)) {
		GPUFREQ_LOGE("@%s: cannot get clk_mux\n", __func__);
		GPUFREQ_TRACE_END();
		return PTR_ERR(g_clk->clk_mux);
	}
	g_clk->clk_main_parent = devm_clk_get(&pdev->dev, "clk_main_parent");
	if (IS_ERR(g_clk->clk_main_parent)) {
		GPUFREQ_LOGE("@%s: cannot get clk_main_parent\n", __func__);
		GPUFREQ_TRACE_END();
		return PTR_ERR(g_clk->clk_main_parent);
	}
	g_clk->clk_sub_parent = devm_clk_get(&pdev->dev, "clk_sub_parent");
	if (IS_ERR(g_clk->clk_sub_parent)) {
		GPUFREQ_LOGE("@%s: cannot get clk_sub_parent\n", __func__);
		GPUFREQ_TRACE_END();
		return PTR_ERR(g_clk->clk_sub_parent);
	}
	GPUFREQ_LOGI("@%s: clk_mux is at 0x%p, clk_main_parent is at 0x%p, \t"
			"clk_sub_parent is at 0x%p\n",
			__func__, g_clk->clk_mux, g_clk->clk_main_parent, g_clk->clk_sub_parent);
	GPUFREQ_TRACE_END();
	return ret;
}
static int __gpufreq_init_pmic(struct platform_device *pdev)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("pdev=0x%x", pdev);
	g_pmic = kzalloc(sizeof(struct g_pmic_info), GFP_KERNEL);
	if (g_pmic == NULL) {
		GPUFREQ_LOGE("@%s: cannot allocate g_pmic\n", __func__);
		return -ENOMEM;
	}
	g_pmic->mtk_pm_vgpu = devm_regulator_get(&pdev->dev, "dvfsrc-vcore");
	if (IS_ERR(g_pmic->mtk_pm_vgpu)) {
		GPUFREQ_LOGE("@%s: cannot get dvfsrc-vcore\n", __func__);
		return PTR_ERR(g_pmic->mtk_pm_vgpu);
	}
	//g_pmic->reg_vcore = devm_regulator_get_optional(&pdev->dev, "vcore");
	g_pmic->reg_vcore = regulator_get(&pdev->dev, "vcore");
	if (IS_ERR(g_pmic->reg_vcore)) {
		GPUFREQ_LOGE("@%s: cannot get VCORE\n", __func__);
		return PTR_ERR(g_pmic->reg_vcore);
	}
	regulator_set_voltage(g_pmic->mtk_pm_vgpu, VGPU_MAX_VOLT * 10, VGPU_MAX_VOLT * 10 + 125);
	GPUFREQ_LOGE("@%s: set VCORE to %d\n", __func__, VGPU_MAX_VOLT * 10);
	if (regulator_enable(g_pmic->mtk_pm_vgpu))
		GPUFREQ_LOGE("@%s: enable VCORE failed\n", __func__);
	else
		GPUFREQ_LOGE("@%s: enable VCORE success and vore volt = %d\n",
		       __func__, __gpufreq_get_real_vgpu());
	return ret;
}
/* API: init reg base address and flavor config of the platform */
static int __gpufreq_init_platform_info(struct platform_device *pdev)
{
	struct device *gpufreq_dev = &pdev->dev;
	struct device_node *of_wrapper = NULL;
	struct resource *res = NULL;
	int ret = GPUFREQ_ENOENT;

	GPUFREQ_TRACE_START("pdev=0x%x", pdev);
	if (unlikely(!gpufreq_dev)) {
		GPUFREQ_LOGE("fail to find gpufreq device (ENOENT)");
		goto done;
	}
	of_wrapper = of_find_compatible_node(NULL, NULL, "mediatek,gpufreq_wrapper");
	if (unlikely(!of_wrapper)) {
		GPUFREQ_LOGE("fail to find gpufreq_wrapper of_node");
		goto done;
	}
	/* ignore return error and use default value if property doesn't exist */
	of_property_read_u32(gpufreq_dev->of_node, "aging-load", &g_aging_load);
	of_property_read_u32(gpufreq_dev->of_node, "mcl50-load", &g_mcl50_load);
	of_property_read_u32(gpufreq_dev->of_node, "enable-aging", &g_aging_enable);
	of_property_read_u32(of_wrapper, "gpueb-support", &g_gpueb_support);
	/* 13000000 */
	g_MFG_base = __gpufreq_of_ioremap("mediatek,mfgcfg", 0);
	if (unlikely(!g_MFG_base)) {
		GPUFREQ_LOGE("fail to get resource mfgcfg");
		goto done;
	}
	g_apmixed_base = __gpufreq_of_ioremap("mediatek,mt6765-apmixedsys", 0);
	if (unlikely(!g_apmixed_base)) {
		GPUFREQ_LOGE("fail to ioremap APMIXED");
		goto done;
	}
	/* 0x1000C000 */
	ret = GPUFREQ_SUCCESS;
done:
	GPUFREQ_TRACE_END();
	return ret;
}
/* API: gpufreq driver probe */
static int __gpufreq_pdrv_probe(struct platform_device *pdev)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_LOGI("start to probe gpufreq platform driver");
	/* keep probe successful but do nothing when bringup */
	if (__gpufreq_bringup()) {
		GPUFREQ_LOGI("skip gpufreq platform driver probe when bringup");
		//__gpufreq_dump_bringup_status(pdev);
		goto done;
	}
	/* init reg base address and flavor config of the platform in both AP and EB mode */
	ret = __gpufreq_init_platform_info(pdev);
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to init platform info (%d)", ret);
		goto done;
	}
	/* init pmic regulator */
	ret = __gpufreq_init_pmic(pdev);
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to init pmic (%d)", ret);
		goto done;
	}
	/* skip most of probe in EB mode */
	if (g_gpueb_support) {
		GPUFREQ_LOGI("gpufreq platform probe only init reg_base/dfd/pmic/fp in EB mode");
		goto register_fp;
	}
	/* init clock source */
	ret = __gpufreq_init_clk(pdev);
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to init clk (%d)", ret);
		goto done;
	}
	/* init mtcmos power domain */
	ret = __gpufreq_init_mtcmos(pdev);
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to init mtcmos (%d)", ret);
		goto done;
	}
	/* init segment id */
	ret = __gpufreq_init_segment_id(pdev);
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to init segment id (%d)", ret);
		goto done;
	}
	/*before power ON , set the hwapm state first*/
	mt_gpufreq_set_hwapm_state(false);
	/* init OPP table */
	ret = __gpufreq_init_opp_table(pdev);
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to init OPP table (%d)", ret);
		goto done;
	}
	/* power on to init first OPP index */
	ret = __gpufreq_power_control(POWER_ON);
	if (unlikely(ret < 0)) {
		GPUFREQ_LOGE("fail to control power state: %d (%d)", POWER_ON, ret);
		goto done;
	}
	if (g_aging_enable)
		__gpufreq_apply_aging(true);
	/* init first OPP index by current freq and volt */
	ret = __gpufreq_init_opp_idx();
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to init OPP index (%d)", ret);
		goto done;
	}
	/* power off after init first OPP index */
	if (__gpufreq_power_ctrl_enable())
		__gpufreq_power_control(POWER_OFF);
	else
		/* never power off if power control is disabled */
		GPUFREQ_LOGI("power control always on");
	/* init AEE debug */
	__gpufreq_footprint_power_step_reset();
	__gpufreq_footprint_oppidx_reset();
	__gpufreq_footprint_power_count_reset();
register_fp:
	/*
	 * GPUFREQ PLATFORM INIT DONE
	 * register differnet platform fp to wrapper depending on AP or EB mode
	 */
	if (g_gpueb_support)
		gpufreq_register_gpufreq_fp(&platform_eb_fp);
	else
		gpufreq_register_gpufreq_fp(&platform_ap_fp);
	/* init gpu ppm */
	ret = gpuppm_init(TARGET_GPU, g_gpueb_support, GPUFREQ_SAFE_VLOGIC);
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to init gpuppm (%d)", ret);
		goto done;
	}
	GPUFREQ_LOGI("gpufreq platform driver probe done");
done:
	GPUFREQ_LOGI("gpufreq platform driver probe done");
	return ret;
}
/* API: gpufreq driver remove */
static int __gpufreq_pdrv_remove(struct platform_device *pdev)
{
	dev_pm_domain_detach(g_mtcmos->pd_mfg, true);
	dev_pm_domain_detach(g_mtcmos->pd_mfg_async, true);
	dev_pm_domain_detach(g_mtcmos->pd_mfg_core0, true);

	kfree(g_gpu.working_table);
	kfree(g_clk);
	kfree(g_pmic);
	kfree(g_mtcmos);
	kfree(g_power_table);
	return GPUFREQ_SUCCESS;
}
/* API: register gpufreq platform driver */
static int __init __gpufreq_init(void)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_LOGI("start to init gpufreq platform driver");
	/* register gpufreq platform driver */
	ret = platform_driver_register(&g_gpufreq_pdrv);
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to register gpufreq platform driver (%d)", ret);
		goto done;
	}
	GPUFREQ_LOGI("gpufreq platform driver init done");
done:
	return ret;
}
/* API: unregister gpufreq driver */
static void __exit __gpufreq_exit(void)
{
	platform_driver_unregister(&g_gpufreq_pdrv);
}
#if IS_BUILTIN(CONFIG_MTK_GPU_MT6765_SUPPORT)
rootfs_initcall(__gpufreq_init);
#else
module_init(__gpufreq_init);
#endif
module_exit(__gpufreq_exit);
MODULE_DEVICE_TABLE(of, g_gpufreq_of_match);
MODULE_DESCRIPTION("MediaTek GPU-DVFS platform driver");
MODULE_LICENSE("GPL");
