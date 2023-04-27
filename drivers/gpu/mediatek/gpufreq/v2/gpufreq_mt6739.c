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
#include <gpufreq_mt6739.h>
#include <gpufreq_debug.h>
#include <gpuppm.h>
#include <gpufreq_common.h>
#include <mtk_gpu_utility.h>
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
#if IS_ENABLED(CONFIG_COMMON_CLK_MT6739)
#include <clk-fmeter.h>
#include <clk-mt6739-fmeter.h>
#endif

#if IS_ENABLED(CONFIG_MTK_DEVINFO)
#include <linux/nvmem-consumer.h>
#endif

#include <mtk_vcorefs_manager.h>

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
static int __gpufreq_pause_dvfs(void);
static void __gpufreq_resume_dvfs(void);
static void __gpufreq_interpolate_volt(void);
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
#if 0 //No need on mt6739
static unsigned int __gpufreq_settle_time_vgpu(unsigned int mode, int deltaV);
static unsigned int __gpufreq_settle_time_vsram(unsigned int mode, int deltaV);
#endif
/* get function */
static unsigned int __gpufreq_get_real_fgpu(void);
static unsigned int __gpufreq_get_real_vgpu(void);
static unsigned int __gpufreq_get_real_vsram(void);
static enum gpufreq_posdiv __gpufreq_get_real_posdiv_gpu(void);
static enum gpufreq_posdiv __gpufreq_get_posdiv_by_fgpu(unsigned int freq);
/* power control function */
static void __gpufreq_external_cg_control(void);
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

//thermal
static struct mt_gpufreq_power_table_info *g_power_table;
static int (*mt_gpufreq_wrap_fp)(void);

//TODO:GKI
#if 0
static void __iomem *g_mfg_pll_base;
static void __iomem *g_mfg_rpc_base;
static void __iomem *g_sleep;
static void __iomem *g_nth_emicfg_base;
static void __iomem *g_infracfg_base;
static void __iomem *g_infra_peri_debug3;
static void __iomem *g_infra_peri_debug4;
static void __iomem *g_infracfg_ao_base;
static void __iomem *g_infra_ao_debug_ctrl;
static void __iomem *g_infra_ao1_debug_ctrl;
static void __iomem *g_fmem_ao_debug_ctrl;
static void __iomem *g_efuse_base;
static void __iomem *g_mfg_cpe_control_base;
static void __iomem *g_mfg_cpe_sensor_base;
static void __iomem *g_mali_base;
static void __iomem *g_infra_ao_mem_base;
static struct gpufreq_pmic_info *g_pmic;
static struct gpufreq_clk_info *g_clk;
static struct gpufreq_asensor_info g_asensor_info;
static unsigned int g_avs_enable;
#endif
static bool g_volt_enable_state;
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
/* API: get poiner of working OPP table of STACK */
const struct gpufreq_opp_info *__gpufreq_get_working_table_stack(void)
{
	return NULL;
}
/* API: get poiner of signed OPP table of GPU */
const struct gpufreq_opp_info *__gpufreq_get_signed_table_gpu(void)
{
	return g_gpu.signed_table;
}
/* API: get poiner of signed OPP table of STACK */
const struct gpufreq_opp_info *__gpufreq_get_signed_table_stack(void)
{
	return NULL;
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
	// TOGO:GKI
	//opp_info.avs_enable = g_avs_enable;
	if (__gpufreq_get_power_state()) {
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

/*
 * API : set gpu wrap function pointer
 */

void mt_gpufreq_set_gpu_wrap_fp(int (*gpu_wrap_fp)(void))
{
	mt_gpufreq_wrap_fp = gpu_wrap_fp;
}
EXPORT_SYMBOL(mt_gpufreq_set_gpu_wrap_fp);

/*
 * API : get current segment max opp index
 */
unsigned int mt_gpufreq_get_seg_max_opp_index(void)
{
	return g_gpu.segment_upbound;
}
EXPORT_SYMBOL(mt_gpufreq_get_seg_max_opp_index);

/* API : get OPP table index number */
/* need to sub g_segment_max_opp_idx to map to real idx */
unsigned int mt_gpufreq_get_dvfs_table_num(void)
{
	return  g_gpu.segment_lowbound - g_gpu.segment_upbound + 1;
}
EXPORT_SYMBOL(mt_gpufreq_get_dvfs_table_num);

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
	if (p_leakage < 0)
		p_leakage = 0;
#else
	p_leakage = GPU_LEAKAGE_POWER;
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
#endif /* ifdef CONFIG_MTK_LEGACY_THERMAL */

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

struct mt_gpufreq_power_table_info *mt_gpufreq_get_power_table(void)
{
	return g_power_table;
}
EXPORT_SYMBOL(mt_gpufreq_get_power_table);

unsigned int mt_gpufreq_get_power_table_num(void)
{
	return g_gpu.signed_opp_num;
}
EXPORT_SYMBOL(mt_gpufreq_get_power_table_num);


/* API: get Freq of GPU via OPP index */
unsigned int __gpufreq_get_fgpu_by_idx(int oppidx)
{
	if (oppidx >= 0 && oppidx < g_gpu.opp_num)
		return g_gpu.working_table[oppidx].freq;
	else
		return 0;
}
/* API: get Freq of STACK via OPP index */
unsigned int __gpufreq_get_fstack_by_idx(int oppidx)
{
	GPUFREQ_UNREFERENCED(oppidx);
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
/* API: get Power of STACK via OPP index */
unsigned int __gpufreq_get_pstack_by_idx(int oppidx)
{
	GPUFREQ_UNREFERENCED(oppidx);
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
//TODO:GKI
/* API: get leakage Power of GPU */
unsigned int __gpufreq_get_lkg_pgpu(unsigned int volt)
{
	GPUFREQ_UNREFERENCED(volt);
	return GPU_LEAKAGE_POWER;
}
//TODO:GKI
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
//TODO:GKI
/* API: get leakage Power of STACK */
unsigned int __gpufreq_get_lkg_pstack(unsigned int volt)
{
	GPUFREQ_UNREFERENCED(volt);
	return 0;
}
/* API: get dynamic Power of STACK */
unsigned int __gpufreq_get_dyn_pstack(unsigned int freq, unsigned int volt)
{
	GPUFREQ_UNREFERENCED(freq);
	GPUFREQ_UNREFERENCED(volt);
	return 0;
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
		//TODO:GKI  check  __mt_gpufreq_kick_pbm
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to control Buck: On (%d)", ret);
			ret = GPUFREQ_EINVAL;
			goto done_unlock;
		}
		__gpufreq_footprint_power_step(GPUFREQ_POWER_STEP_02);
		/* control MTCMOS */
		ret = __gpufreq_mtcmos_control(POWER_ON);
		if (unlikely(ret < 0)) {
			GPUFREQ_LOGE("fail to control MTCMOS: On (%d)", ret);
			ret = GPUFREQ_EINVAL;
			goto done_unlock;
		}
		__gpufreq_footprint_power_step(GPUFREQ_POWER_STEP_03);
		/* control clock */
		ret = __gpufreq_clock_control(POWER_ON);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to control CLOCK: On (%d)", ret);
			ret = GPUFREQ_EINVAL;
			goto done_unlock;
		}
		__gpufreq_footprint_power_step(GPUFREQ_POWER_STEP_04);
		/* free DVFS when power on */
		g_dvfs_state &= ~DVFS_POWEROFF;
	} else if (power == POWER_OFF && g_gpu.power_count == 0) {
		__gpufreq_footprint_power_step(GPUFREQ_POWER_STEP_05);
		/* freeze DVFS when power off */
		g_dvfs_state |= DVFS_POWEROFF;
		__gpufreq_footprint_power_step(GPUFREQ_POWER_STEP_06);
		/* control clock */
		ret = __gpufreq_clock_control(POWER_OFF);
		//TODO:GKI  check  __mt_gpufreq_kick_pbm
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to control CLOCK: Off (%d)", ret);
			ret = GPUFREQ_EINVAL;
			goto done_unlock;
		}
		__gpufreq_footprint_power_step(GPUFREQ_POWER_STEP_07);
		/* control MTCMOS */
		ret = __gpufreq_mtcmos_control(POWER_OFF);
		if (unlikely(ret < 0)) {
			GPUFREQ_LOGE("fail to control MTCMOS: Off (%d)", ret);
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
	}
	/* return power count if successfully control power */
	ret = g_gpu.power_count;
done_unlock:
// TODO:GKI
	// GPUFREQ_LOGD("PWR_STATUS (0x%x): 0x%08x",
	// (0x10006000 + 0x16C), readl(g_sleep + 0x16C));
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
	int ret = GPUFREQ_SUCCESS;
	GPUFREQ_TRACE_START("target_oppidx=%d, key=%d",
		target_oppidx, key);
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
	cur_freq = g_gpu.cur_freq;
	cur_volt = g_gpu.cur_volt;
	cur_vsram = g_gpu.cur_vsram;
	target_freq = opp_table[target_oppidx].freq;
	target_volt = opp_table[target_oppidx].volt;
	target_vsram = opp_table[target_oppidx].vsram;
	GPUFREQ_LOGD("begin to commit GPU OPP index: (%d->%d)",
		cur_oppidx, target_oppidx);
	/* todo: GED log buffer (gpufreq_pr_logbuf) */
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
		if (target_volt != cur_volt) {
			ret = __gpufreq_volt_scale_gpu(
				cur_volt, target_volt, cur_vsram, target_vsram);
			if (unlikely(ret)) {
				GPUFREQ_LOGE("fail to scale Vgpu: (%d->%d), Vsram_gpu: (%d->%d)",
					cur_volt, target_volt,
					cur_vsram, target_vsram);
				goto done_unlock;
			}
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
		if (target_volt != cur_volt) {
			ret = __gpufreq_volt_scale_gpu(
				cur_volt, target_volt,
				cur_vsram, target_vsram);
			if (unlikely(ret)) {
				GPUFREQ_LOGE("fail to scale Vgpu: (%d->%d), Vsram_gpu: (%d->%d)",
					cur_volt, target_volt,
					cur_vsram, target_vsram);
				goto done_unlock;
			}
		}
	}
	g_gpu.cur_oppidx = target_oppidx;
	__gpufreq_footprint_oppidx(target_oppidx);
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
/* API: fix OPP of STACK via given OPP index */
int __gpufreq_fix_target_oppidx_stack(int oppidx)
{
	GPUFREQ_UNREFERENCED(oppidx);
	return GPUFREQ_EINVAL;
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
/* API: fix Freq and Volt of STACK via given Freq and Volt */
int __gpufreq_fix_custom_freq_volt_stack(unsigned int freq, unsigned int volt)
{
	GPUFREQ_UNREFERENCED(freq);
	GPUFREQ_UNREFERENCED(volt);
	return GPUFREQ_EINVAL;
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
//TODO:gki
#if 0
	u32 val = 0;
	GPUFREQ_LOGI("== [GPUFREQ INFRA STATUS] ==");
	GPUFREQ_LOGI("mfgpll=%d, GPU[%d] Freq: %d, Vgpu: %d, Vsram: %d",
		mt_get_abist_freq(FM_MGPLL_CK), g_gpu.cur_oppidx, g_gpu.cur_freq,
		g_gpu.cur_volt, g_gpu.cur_vsram);
	/*0x1020E000 */
	if (g_infracfg_base) {
		/* g_infracfg_base */
		GPUFREQ_LOGI("%-7s (0x%x): 0x%08x, (0x%x): 0x%08x",
			"[EMI]",
			(0x1020E000 + 0x810), readl(g_infracfg_base + 0x810),
			(0x1020E000 + 0x814), readl(g_infracfg_base + 0x814));
	}
	/* 0x10001000, 0x10023000 */
	if (g_infracfg_ao_base && g_infra_ao_debug_ctrl) {
		/* MD_MFGSYS_PROTECT_EN_STA_0 */
		/* MD_MFGSYS_PROTECT_RDY_STA_0 */
		/* INFRA_AO_BUS_U_DEBUG_CTRL_AO_INFRA_AO_CTRL0 */
		GPUFREQ_LOGI("%-7s (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x",
			"[INFRA]",
			(0x10001000 + 0xCA0), readl(g_infracfg_ao_base + 0xCA0),
			(0x10001000 + 0xCAC), readl(g_infracfg_ao_base + 0xCAC),
			(0x10023000 + 0x000), readl(g_infra_ao_debug_ctrl + 0x000));
	}
	/* 0x1002B000, 0x1002E000 */
	if (g_infra_ao1_debug_ctrl && g_infra_peri_debug3) {
		/* INFRA_QAXI_AO_BUS_SUB1_U_DEBUG_CTRL_AO_INFRA_AO1_CTRL0 */
		/*GPU_DFD*/
		GPUFREQ_LOGI("%-7s (0x%x): 0x%08x, (0x%x): 0x%08x",
			"[INFRA]",
			(0x1002B000 + 0x000), readl(g_infra_ao1_debug_ctrl + 0x000),
			(0x1002E000 + 0x000), readl(g_infra_peri_debug3 + 0x000));
	}
	/* 0x10040000, 0x10042000 */
	if (g_infra_peri_debug4 && g_fmem_ao_debug_ctrl) {
		/* GPU_DFD */
		/* NTH_EMI_AO_DEBUG_CTRL_EMI_AO_BUS_U_DEBUG_CTRL_AO_EMI_AO_CTRL0 */
		GPUFREQ_LOGI("%-7s (0x%x): 0x%08x, (0x%x): 0x%08x",
			"[INFRA]",
			(0x10040000 + 0x000), readl(g_infra_peri_debug4 + 0x000),
			(0x10042000 + 0x000), readl(g_fmem_ao_debug_ctrl + 0x000));
	}
	/* 0x10006000 */
	if (g_sleep) {
		GPUFREQ_LOGI("[GPU_DFD] pwr info 0x%x:0x%08x %08x %08x %08x\n",
				(0x10006000 + 0x308),
				readl(g_sleep + 0x308),
				readl(g_sleep + 0x30C),
				readl(g_sleep + 0x310),
				readl(g_sleep + 0x314));
		GPUFREQ_LOGI("[GPU_DFD] pwr info 0x%x:0x%08x %08x %08x\n",
				(0x10006000 + 0x318),
				readl(g_sleep + 0x318),
				readl(g_sleep + 0x31C),
				readl(g_sleep + 0x320));
		GPUFREQ_LOGI("[GPU_DFD] pwr info 0x%x:0x%08x\n",
				(0x10006000 + 0x16C),
				readl(g_sleep + 0x16C));
		GPUFREQ_LOGI("[GPU_DFD] pwr info 0x%x:0x%08x\n",
				(0x10006000 + 0x170),
				readl(g_sleep + 0x170));
	}
#endif
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
#if 0
/* API: get max number of shader cores */
unsigned int __gpufreq_get_core_num(void)
{
	return SHADER_CORE_NUM;
}
#endif
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
			//TODO: GKI check sb
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
	/* [MT6739]
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
	// TOD:GKI
#if 0
	struct gpufreq_opp_info *signed_table = g_gpu.signed_table;
	int i = 0;
	for (i = 0; i < g_gpu.signed_opp_num; i++) {
		if (signed_table[i].freq <= freq)
			return signed_table[i].posdiv;
	}
	GPUFREQ_LOGE("fail to find post-divider of Freq: %d", freq);
#endif
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
#if 0
static unsigned int __gpufreq_settle_time_vgpu(unsigned int mode, int deltaV)
{
	unsigned int t_settle = 0, steps = 0;
	steps = (deltaV / DELAY_FACTOR) + 1;
	t_settle = steps * g_vgpu_sfchg_frate + 52;
	return t_settle; /* us */
}
static unsigned int __gpufreq_settle_time_vsram(unsigned int mode, int deltaV)
{
	unsigned int t_settle = 0, steps = 0;
	steps = (deltaV / DELAY_FACTOR) + 1;
	t_settle = steps * g_vsram_sfchg_frate + 52;
	return t_settle; /* us */
}
#endif
/* API: scale vgpu and vsram via PMIC */
static int __gpufreq_volt_scale_gpu(
	unsigned int vgpu_old, unsigned int vgpu_new,
	unsigned int vsram_old, unsigned int vsram_new)
{
	int ret = GPUFREQ_SUCCESS;
	unsigned int volt = g_gpu.cur_volt;
	GPUFREQ_TRACE_START("vgpu_old=%d, vgpu_new=%d, vsram_old=%d, vsram_new=%d",
		vgpu_old, vgpu_new, vsram_old, vsram_new);
	GPUFREQ_LOGD("begin to scale Vgpu: (%d->%d), Vsram_gpu: (%d->%d)",
		vgpu_old, vgpu_new, vsram_old, vsram_new);
	if (vgpu_new != vgpu_old) {
		switch (vgpu_new) {
		case GPU_DVFS_VOLT0:
			ret = vcorefs_request_dvfs_opp(KIR_GPU, OPP_0);
			break;
		case GPU_DVFS_VOLT1:
			ret = vcorefs_request_dvfs_opp(KIR_GPU, OPP_1);
			break;
		case GPU_DVFS_VOLT2:
			ret = vcorefs_request_dvfs_opp(KIR_GPU, OPP_2);
			break;
		case GPU_DVFS_VOLT3:
			ret = vcorefs_request_dvfs_opp(KIR_GPU, OPP_3);
			break;
		default:
			GPUFREQ_LOGD("@%s: ummapped voltage! volt %d\n", __func__, vgpu_new);
			break;
		}
		volt = __gpufreq_get_real_vgpu();
		if (ret == -1)
			GPUFREQ_LOGE("@%s: vcorefs_request_dvfs_opp fail! volt %d\n",
			__func__, volt);
		else {
			g_gpu.cur_volt = vgpu_new;
			g_gpu.cur_vsram = vsram_new;
		}
	}
	GPUFREQ_TRACE_END();
	return ret;
}
/*
 * API: dump power/clk status when bring-up
 */
static void __gpufreq_dump_bringup_status(struct platform_device *pdev)
{
//TODO:GKI
#if 0
	struct device *gpufreq_dev = &pdev->dev;
	struct resource *res = NULL;
	if (unlikely(!gpufreq_dev)) {
		GPUFREQ_LOGE("fail to find gpufreq device (ENOENT)");
		goto done;
	}
	/* 0x13FA0000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_pll");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource MFG_PLL");
		goto done;
	}
	g_mfg_pll_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_mfg_pll_base)) {
		GPUFREQ_LOGE("fail to ioremap MFG_PLL: 0x%llx", res->start);
		goto done;
	}
	/* 0x13FBF000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_top_config");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource MFG_TOP_CONFIG");
		goto done;
	}
	g_mfg_top_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_mfg_top_base)) {
		GPUFREQ_LOGE("fail to ioremap MFG_TOP_CONFIG: 0x%llx", res->start);
		goto done;
	}
	/* 0x10006000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "sleep");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource SLEEP");
		goto done;
	}
	g_sleep = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_sleep)) {
		GPUFREQ_LOGE("fail to ioremap SLEEP: 0x%llx", res->start);
		goto done;
	}
	/* 0x13000000 */
	g_mali_base = __gpufreq_of_ioremap("mediatek,mali", 0);
	if (unlikely(!g_mali_base)) {
		GPUFREQ_LOGE("fail to ioremap MALI");
		goto done;
	}
	/* 0x13F90000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_rpc");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource MFG_RPC");
		goto done;
	}
	g_mfg_rpc_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_mfg_rpc_base)) {
		GPUFREQ_LOGE("fail to ioremap MFG_RPC: 0x%llx", res->start);
		goto done;
	}
	/*
	 * [SPM] pwr_status: pwr_ack (@0x1000_616C)
	 * [SPM] pwr_status_2nd: pwr_ack_2nd (@x1000_6170)
	 * [2]: MFG0, [3]: MFG1, [4]: MFG2, [5]: MFG3
	 */
	GPUFREQ_LOGI("[GPU] MALI: 0x%08x, MFG_TOP_CONFIG: 0x%08x",
		readl(g_mali_base), readl(g_mfg_top_base));
	GPUFREQ_LOGI("[MUX] MFG_RPC_AO_CLK_CFG: 0x%08x",
		readl(g_mfg_rpc_base + CLK_MUX_OFS));
	GPUFREQ_LOGI("@%s: [PWR_ACK] MFG0~MFG3=0x%08X(0x%08X)\n",
		__func__,
		readl(g_sleep + 0x16C) & 0x0000003C,
		readl(g_sleep + 0x170) & 0x0000003C);
done:
	return;
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
	GPUFREQ_LOGD("%s: MFGPLL_CON1 = 0x%x", __func__, MFGPLL_CON1);
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
	if (g_volt_enable_state)
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
static void __gpufreq_external_cg_control(void)
{
	u32 val;
	/* MFG_GLOBAL_CON: 0x1300_00b0 bit [8] = 0x0 */
	/* MFG_GLOBAL_CON: 0x1300_00b0 bit [10] = 0x0 */
	GPUFREQ_LOGD("@%s: 0x1300_00b0 = 0x%x\n", __func__, readl(g_MFG_base + 0xb0));
	val = readl(g_MFG_base + 0xb0);
	val &= ~(1UL << 8);
	val &= ~(1UL << 10);
	writel(val, g_MFG_base + 0xb0);
	GPUFREQ_LOGD("@%s: 0x1300_00b0 = 0x%x\n", __func__, readl(g_MFG_base + 0xb0));
	/* MFG_ASYNC_CON: 0x1300_0020 bit [25:22] = 0xF */
	GPUFREQ_LOGD("@%s: 0x1300_0020 = 0x%x\n", __func__, readl(g_MFG_base + 0x20));
	writel(readl(g_MFG_base + 0x20) | (0xF << 22), g_MFG_base + 0x20);
	GPUFREQ_LOGD("@%s: 0x1300_0020 = 0x%x\n", __func__, readl(g_MFG_base + 0x20));
	/* MFG_ASYNC_CON_1: 0x1300_0024 bit [0] = 0x1 */
	GPUFREQ_LOGD("@%s: 0x1300_0024 = 0x%x\n", __func__, readl(g_MFG_base + 0x24));
	writel(readl(g_MFG_base + 0x24) | (1UL), g_MFG_base + 0x24);
	GPUFREQ_LOGD("@%s: 0x1300_0024 = 0x%x\n", __func__, readl(g_MFG_base + 0x24));
}
static int __gpufreq_clock_control(enum gpufreq_power_state power)
{
	int ret = GPUFREQ_SUCCESS;
#if 0
	GPUFREQ_TRACE_START("power=%d", power);
	if (power == POWER_ON) {
		ret = clk_prepare_enable(g_clk->subsys_mfg_cg);
		if (unlikely(ret)) {
			__gpufreq_abort(GPUFREQ_CCF_EXCEPTION,
				"fail to enable subsys_mfg_cg (%d)", ret);
			goto done;
		}
		__gpufreq_external_cg_control();
		g_gpu.cg_count++;
	} else {
		clk_disable_unprepare(g_clk->subsys_mfg_cg);
		g_gpu.cg_count--;
	}
done:
	GPUFREQ_TRACE_END();
#endif
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
		/* enable PLL and set clk_mux to mfgpll */
		if (clk_prepare_enable(g_clk->clk_mux))
			GPUFREQ_LOGE("@%s: failed when enable top-clk\n", __func__);
		__gpufreq_switch_clksrc(CLOCK_MAIN);
		/* MFG1 on by CCF */
		ret = pm_runtime_get_sync(g_mtcmos->pd_mfg);
		if (unlikely(ret < 0)) {
			__gpufreq_abort(GPUFREQ_CCF_EXCEPTION,
				"fail to enable pd_mfg (%d)", ret);
			goto done;
		}
		ret = pm_runtime_get_sync(g_mtcmos->pd_mfg_core0);
		if (unlikely(ret < 0)) {
			__gpufreq_abort(GPUFREQ_CCF_EXCEPTION,
				"fail to enable pd_mfg_core0 (%d)", ret);
			goto done;
		}
#if GPUFREQ_CHECK_MTCMOS_PWR_STATUS
	//TODO:GKI check mtcmos power
#endif /* GPUFREQ_CHECK_MTCMOS_PWR_STATUS */
#ifdef MT_GPUFREQ_SRAM_DEBUG
	aee_rr_rec_gpu_dvfs_status(0x80 | (aee_rr_curr_gpu_dvfs_status() & 0x0F));
#endif
	GPUFREQ_LOGD("@%s: enable MTCMOS done\n", __func__);
		g_gpu.mtcmos_count++;
	} else {
	#ifdef MT_GPUFREQ_SRAM_DEBUG
		aee_rr_rec_gpu_dvfs_status(0x90 | (aee_rr_curr_gpu_dvfs_status() & 0x0F));
	#endif
		ret = pm_runtime_put_sync(g_mtcmos->pd_mfg_core0);
		if (unlikely(ret < 0)) {
			__gpufreq_abort(GPUFREQ_CCF_EXCEPTION,
				"fail to disable pd_mfg_core0 (%d)", ret);
			goto done;
		}
		ret = pm_runtime_put_sync(g_mtcmos->pd_mfg);
		if (unlikely(ret < 0)) {
			__gpufreq_abort(GPUFREQ_CCF_EXCEPTION,
				"fail to disable pd_mfg (%d)", ret);
			goto done;
		}
		/* set clk_mux to 26M and turn-off gpupll reference */
		__gpufreq_switch_clksrc(CLOCK_SUB);
		clk_disable_unprepare(g_clk->clk_mux);
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
	unsigned int volt = g_gpu.cur_volt;

	GPUFREQ_LOGD("@%s: current volt = %d ", __func__, volt);
	GPUFREQ_TRACE_START("power=%d", power);
	/* power on */
	if (power == POWER_ON) {
		g_volt_enable_state = true;
		g_gpu.buck_count++;
	/* power off */
	} else {
		ret = vcorefs_request_dvfs_opp(KIR_GPU, OPP_UNREQ);
		GPUFREQ_LOGD("@%s:power off vcorefs_request_dvfs_opp ret %d\n", __func__, ret);
		g_volt_enable_state = false;
		g_gpu.buck_count--;
	}
	if (ret == -1) {
		GPUFREQ_LOGE("@%s: vcorefs_request_dvfs_opp fail! volt %d\n", __func__, volt);
	} else {
		g_gpu.cur_volt = __gpufreq_get_real_vgpu();
		g_gpu.cur_vsram = __gpufreq_get_real_vsram();
	}
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
			for (oppidx = 1; oppidx < g_gpu.opp_num; oppidx++) {
				if (cur_freq >= working_table[oppidx].freq)
					break;
			}
		}
	}
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
	__gpufreq_set_dvfs_state(false, DVFS_AGING_KEEP);
	GPUFREQ_LOGI("resume DVFS, state: 0x%x", g_dvfs_state);
	GPUFREQ_TRACE_END();
}
/* API: pause dvfs to given freq and volt */
static int __gpufreq_pause_dvfs(void)
{
	int ret = GPUFREQ_SUCCESS;
//TODO:GKI
	return ret;
}
/*
 * API: interpolate OPP of none AVS idx.
 * step = (large - small) / range
 * vnew = large - step * j
 */
static void __gpufreq_interpolate_volt(void)
{
	// TODO: GKI check PTPOD
#if 0
	int avs_num = 0;
	int front_idx = 0, rear_idx = 0, inner_idx = 0;
	unsigned int large_volt = 0, small_volt = 0;
	unsigned int large_freq = 0, small_freq = 0;
	unsigned int inner_volt = 0, inner_freq = 0;
	unsigned int previous_volt = 0;
	int range = 0;
	int slope = 0;
	int i = 0, j = 0;
	struct gpufreq_opp_info *signed_table = g_gpu.signed_table;
	avs_num = AVS_ADJ_NUM;
	mutex_lock(&gpufreq_lock);
	for (i = 1; i < avs_num; i++) {
		front_idx = g_avs_adj[i - 1].oppidx;
		rear_idx = g_avs_adj[i].oppidx;
		range = rear_idx - front_idx;
		/* freq division to amplify slope */
		large_volt = signed_table[front_idx].volt * 100;
		large_freq = signed_table[front_idx].freq / 1000;
		small_volt = signed_table[rear_idx].volt * 100;
		small_freq = signed_table[rear_idx].freq / 1000;
		/* slope = volt / freq */
		slope = (large_volt - small_volt) / (large_freq - small_freq);
		if (unlikely(slope < 0))
			__gpufreq_abort(GPUFREQ_GPU_EXCEPTION,
				"invalid slope when interpolate OPP Volt: %d", slope);
		GPUFREQ_LOGD("GPU[%02d*] Freq: %d, Volt: %d, slope: %d",
			rear_idx, small_freq*1000, small_volt, slope);
		/* start from small v and f, and use (+) instead of (-) */
		for (j = 1; j < range; j++) {
			inner_idx = rear_idx - j;
			inner_freq = signed_table[inner_idx].freq / 1000;
			inner_volt = (small_volt + slope * (inner_freq - small_freq)) / 100;
			inner_volt = VOLT_NORMALIZATION(inner_volt);
			/* compare interpolated volt with volt of previous OPP idx */
			previous_volt = signed_table[inner_idx + 1].volt;
			if (inner_volt < previous_volt)
				__gpufreq_abort(GPUFREQ_GPU_EXCEPTION,
					"invalid interpolated [%02d*] Volt: %d < [%02d*] Volt: %d",
					inner_idx, inner_volt, inner_idx + 1, previous_volt);
			signed_table[inner_idx].volt = inner_volt;
			signed_table[inner_idx].vsram = __gpufreq_get_vsram_by_vgpu(inner_volt);
			GPUFREQ_LOGD("GPU[%02d*] Freq: %d, Volt: %d, vsram: %d,",
				inner_idx, inner_freq*1000, inner_volt,
				signed_table[inner_idx].vsram);
		}
		GPUFREQ_LOGD("GPU[%02d*] Freq: %d, Volt: %d",
			front_idx, large_freq*1000, large_volt);
	}
	mutex_unlock(&gpufreq_lock);
#endif
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
		adj_table, adj_num, target);
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
	//TODO: GKI
#if 0
	struct gpufreq_adj_info *aging_adj = NULL;
	int adj_num = 0;
	int i;
	unsigned int aging_table_idx = GPUFREQ_AGING_MOST_AGRRESIVE;
	adj_num = g_gpu.signed_opp_num;
	/* prepare aging adj */
	aging_adj = kcalloc(adj_num, sizeof(struct gpufreq_adj_info), GFP_KERNEL);
	if (!aging_adj) {
		GPUFREQ_LOGE("fail to alloc gpufreq_adj_info (ENOMEM)");
		return;
	}
	for (i = 0; i < adj_num; i++) {
		aging_adj[i].oppidx = i;
		aging_adj[i].vaging = g_aging_table[aging_table_idx][i];
	}
	/* apply aging to signed table */
	__gpufreq_apply_adjust(aging_adj, adj_num);
	kfree(aging_adj);
#endif
}
static void __gpufreq_custom_adjustment(void)
{
	struct gpufreq_adj_info *custom_adj;
	int adj_num = 0;
// TODO:GKI
#if 0
	if (g_mcl50_load) {
		custom_adj = g_mcl50_adj;
		adj_num = MCL50_ADJ_NUM;
		__gpufreq_apply_adjust(custom_adj, adj_num);
		GPUFREQ_LOGI("MCL50 flavor load");
	}
#endif
}
static void __gpufreq_segment_adjustment(struct platform_device *pdev)
{
	//TODO:GKI
}
static void __gpufreq_init_shader_present(void)
{
	#if 0
	unsigned int segment_id = 0;
	segment_id = g_gpu.segment_id;
	switch (segment_id) {
	case MT6739_SEGMENT:
		g_shader_present = GPU_SHADER_PRESENT_2;
		break;
	default:
		g_shader_present = GPU_SHADER_PRESENT_2;
		GPUFREQ_LOGI("invalid segment id: %d", segment_id);
	}
	GPUFREQ_LOGD("segment_id: %d, shader_present: %d", segment_id, g_shader_present);
	#endif
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
	if (segment_id == MT6739_SEGMENT) {
		g_gpu.signed_table = g_default_gpu_segment1;
		table_size = ARRAY_SIZE(g_default_gpu_segment1);
	} else if (segment_id == MT6739TW_SEGMENT) {
		g_gpu.signed_table = g_default_gpu_segment2;
		table_size = ARRAY_SIZE(g_default_gpu_segment2);
	} else if (segment_id == MT6739WD_SEGMENT) {
		g_gpu.signed_table = g_default_gpu_segment3;
		table_size = ARRAY_SIZE(g_default_gpu_segment3);
	} else { /*efuse =0 ; MT6739_SEGMENT*/
		g_gpu.signed_table = g_default_gpu_segment1;
		table_size = ARRAY_SIZE(g_default_gpu_segment1);
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
	/* init springboard table */
	g_gpu.sb_table = kcalloc(g_gpu.opp_num,
		sizeof(struct gpufreq_sb_info), GFP_KERNEL);
	if (!g_gpu.sb_table) {
		GPUFREQ_LOGE("fail to alloc springboard table (ENOMEM)");
		ret = GPUFREQ_ENOMEM;
		goto done;
	}
	__gpufreq_setup_opp_power_table(g_gpu.signed_opp_num);
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
	unsigned int turbo_id = 0;
	unsigned int segment_id = 0;

	struct nvmem_cell *efuse_cell;
	struct nvmem_cell *turbo_cell;
	unsigned int *efuse_buf;
	unsigned int *turbo_buf;
	size_t efuse_len;
	size_t turbo_len;
	// for get segment id
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
	// for get turbo id
	turbo_cell = nvmem_cell_get(&pdev->dev, "efuse_turbo_cell");
	if (IS_ERR(turbo_cell)) {
		GPUFREQ_LOGE("@%s: cannot get efuse_turbo_cell\n", __func__);
		PTR_ERR(turbo_cell);
	}
	turbo_buf = (unsigned int *)nvmem_cell_read(turbo_cell, &turbo_len);
	nvmem_cell_put(turbo_cell);
	if (IS_ERR(turbo_buf)) {
		GPUFREQ_LOGE("@%s: cannot get turbo_buf of turbo segment\n", __func__);
		PTR_ERR(turbo_buf);
	}
	turbo_id = (*turbo_buf & 0xFF);
	kfree(turbo_buf);

	if (((turbo_id & EFUSE_FAB_INFO_TURBO_MASK) == EFUSE_FAB_INFO_TURBO_MASK)
		&& (efuse_id == 0x80 || efuse_id == 0x88
		|| efuse_id == 0x90 || efuse_id == 0x08
		|| efuse_id == 0x00 || efuse_id == 0x40
		|| efuse_id == 0x48 || efuse_id == 0xC8
		|| efuse_id == 0xC0 || efuse_id == 0xD0)) {
		/* 6739D */
		segment_id = MT6739TW_SEGMENT;
	} else if (efuse_id == 0xB8 || efuse_id == 0x38) {
		/* Other Version, set default segment */
		segment_id = MT6739WD_SEGMENT;
	} else {
		segment_id = MT6739_SEGMENT;
	}
	GPUFREQ_LOGI("@%s: efuse_id = 0x%08X, turbo_id = 0x%08X, segment_id = %d\n",
		__func__, efuse_id, turbo_id, segment_id);
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
	g_mtcmos->pd_mfg_core0 = dev_pm_domain_attach_by_name(dev, "pd_mfg_core0");
	if (IS_ERR_OR_NULL(g_mtcmos->pd_mfg_core0)) {
		ret = g_mtcmos->pd_mfg_core0 ? PTR_ERR(g_mtcmos->pd_mfg_core0) : GPUFREQ_ENODEV;
		__gpufreq_abort(GPUFREQ_CCF_EXCEPTION, "fail to get mfg2_dev (%ld)", ret);
		goto done;
	}
	dev_pm_syscore_device(g_mtcmos->pd_mfg_core0, true);
	GPUFREQ_LOGI("@%s: pd_mfg is at 0x%p, pd_mfg_core0 is at 0x%p\n",
			__func__, g_mtcmos->pd_mfg, g_mtcmos->pd_mfg_core0);
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
/*
 * VGPU slew rate calculation
 * false : falling rate
 * true : rising rate
 */
static unsigned int __calculate_vgpu_sfchg_rate(bool isRising)
{
	unsigned int sfchg_rate_vgpu;
	/* [MT6358] RG_BUCK_VGPU_SFCHG_RRATE and RG_BUCK_VGPU_SFCHG_FRATE
	 * Rising soft change rate
	 * Ref clock = 26MHz (0.038us)
	 * Step = ( code + 1 ) * 0.038 us
	 */
	if (isRising) {
		/* sfchg_rate_reg is 19, (19+1)*0.038 = 0.76us */
		sfchg_rate_vgpu = 1;
	} else {
		/* sfchg_rate_reg is 39, (39+1)*0.038 = 1.52us */
		sfchg_rate_vgpu = 2;
	}
	GPUFREQ_LOGD("@%s: isRising = %d, sfchg_rate_vgpu = %d\n",
			__func__, isRising, sfchg_rate_vgpu);
	return sfchg_rate_vgpu;
}
/*
 * VSRAM slew rate calculation
 * false : falling rate
 * true : rising rate
 */
static unsigned int __calculate_vsram_sfchg_rate(bool isRising)
{
	unsigned int sfchg_rate_vsram;
	/* [MT6358] RG_LDO_VSRAM_GPU_SFCHG_RRATE and RG_LDO_VSRAM_GPU_SFCHG_FRATE
	 *    7'd4 : 0.19us
	 *    7'd8 : 0.34us
	 *    7'd11 : 0.46us
	 *    7'd17 : 0.69us
	 *    7'd23 : 0.92us
	 *    7'd25 : 1us
	 */
	/* sfchg_rate_reg is 7 for rising, (7+1)*0.038 = 0.304us */
	/* sfchg_rate_reg is 15 for falling, (15+1)*0.038 = 0.608us */
	sfchg_rate_vsram = 1;
	GPUFREQ_LOGD("@%s: isRising = %d, sfchg_rate_vsram = %d\n",
			__func__, isRising, sfchg_rate_vsram);
	return sfchg_rate_vsram;
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
	g_pmic->reg_vcore = devm_regulator_get_optional(&pdev->dev, "vcore");
	if (IS_ERR(g_pmic->reg_vcore)) {
		GPUFREQ_LOGE("@%s: cannot get VCORE\n", __func__);
		g_volt_enable_state = false;
		return PTR_ERR(g_pmic->reg_vcore);
	}
	g_volt_enable_state = true;
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
	g_apmixed_base = __gpufreq_of_ioremap("mediatek,apmixedsys", 0);
	if (unlikely(!g_apmixed_base)) {
		GPUFREQ_LOGE("fail to ioremap APMIXED");
		goto done;
	}
	/* 0x1000C000 */
	// TODO:GKI
#if 0
	/* 0x13FA0000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_pll");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource MFG_PLL");
		goto done;
	}
	g_mfg_pll_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_mfg_pll_base)) {
		GPUFREQ_LOGE("fail to ioremap MFG_PLL: 0x%llx", res->start);
		goto done;
	}
	/* 0x13FBF000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_top_config");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource MFG_TOP_CONFIG");
		goto done;
	}
	g_mfg_top_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_mfg_top_base)) {
		GPUFREQ_LOGE("fail to ioremap MFG_TOP_CONFIG: 0x%llx", res->start);
		goto done;
	}
	/* 0x13F90000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_rpc");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource MFG_RPC");
		goto done;
	}
	g_mfg_rpc_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_mfg_rpc_base)) {
		GPUFREQ_LOGE("fail to ioremap MFG_RPC: 0x%llx", res->start);
		goto done;
	}
	/* 0x10006000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "sleep");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource SLEEP");
		goto done;
	}
	g_sleep = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_sleep)) {
		GPUFREQ_LOGE("fail to ioremap SLEEP: 0x%llx", res->start);
		goto done;
	}
	/* 0x1020E000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "infracfg");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource infracfg");
		goto done;
	}
	g_infracfg_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_infracfg_base)) {
		GPUFREQ_LOGE("fail to ioremap infracfg: 0x%llx", res->start);
		goto done;
	}
	/* 0x10001000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "infracfg_ao");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource INFRACFG_AO");
		goto done;
	}
	g_infracfg_ao_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_infracfg_ao_base)) {
		GPUFREQ_LOGE("fail to ioremap INFRACFG_AO: 0x%llx", res->start);
		goto done;
	}
	/* 0x10023000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "infra_ao_debug_ctrl");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource INFRA_AO_DEBUG_CTRL");
		goto done;
	}
	g_infra_ao_debug_ctrl = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_infra_ao_debug_ctrl)) {
		GPUFREQ_LOGE("fail to ioremap INFRA_AO_DEBUG_CTRL: 0x%llx", res->start);
		goto done;
	}
	/* 0x1002B000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "infra_ao1_debug_ctrl");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource INFRA_AO1_DEBUG_CTRL");
		goto done;
	}
	g_infra_ao1_debug_ctrl = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_infra_ao1_debug_ctrl)) {
		GPUFREQ_LOGE("fail to ioremap INFRA_AO1_DEBUG_CTRL: 0x%llx", res->start);
		goto done;
	}
	/* 0x10042000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "fmem_ao_debug_ctrl");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource fmem_ao_debug_ctrl");
		goto done;
	}
	g_fmem_ao_debug_ctrl = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_fmem_ao_debug_ctrl)) {
		GPUFREQ_LOGE("fail to ioremap fmem_ao_debug_ctrl: 0x%llx", res->start);
		goto done;
	}
#endif
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
//TODO:GKI
#if 0
	/* init shader present */
	__gpufreq_init_shader_present();
#endif
	/* power on to init first OPP index */
	ret = __gpufreq_power_control(POWER_ON);
	if (unlikely(ret < 0)) {
		GPUFREQ_LOGE("fail to control power state: %d (%d)", POWER_ON, ret);
		goto done;
	}
//TODO: GKI
#if 0
	if (g_aging_enable)
		__gpufreq_apply_aging(true);
#endif
	/* init OPP table */
	ret = __gpufreq_init_opp_table(pdev);
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to init OPP table (%d)", ret);
		goto done;
	}
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

	kfree(g_gpu.working_table);
	kfree(g_gpu.sb_table);
	kfree(g_clk);
	kfree(g_pmic);
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
module_init(__gpufreq_init);
module_exit(__gpufreq_exit);
MODULE_DEVICE_TABLE(of, g_gpufreq_of_match);
MODULE_DESCRIPTION("MediaTek GPU-DVFS platform driver");
MODULE_LICENSE("GPL");
