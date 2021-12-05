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
#if IS_ENABLED(CONFIG_MTK_DEVINFO)
#include <linux/nvmem-consumer.h>
#endif

#include <gpufreq_v2.h>
#include <gpufreq_debug.h>
#include <gpuppm.h>
#include <gpufreq_common.h>
#include <gpufreq_mt6895.h>
//#include <gpudfd_mt6895.h>
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
#if IS_ENABLED(CONFIG_MTK_STATIC_POWER)
#include <leakage_table_v2/mtk_static_power.h>
#endif
#if IS_ENABLED(CONFIG_COMMON_CLK_MTK_FREQ_HOPPING)
#include <clk-mtk.h>
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
static int __gpufreq_pause_dvfs(void);
static void __gpufreq_resume_dvfs(void);
static void __gpufreq_interpolate_volt(void);
static void __gpufreq_apply_aging(unsigned int apply_aging);
static void __gpufreq_apply_adjust(enum gpufreq_target target,
	struct gpufreq_adj_info *adj_table, int adj_num);
/* dvfs function */
static int __gpufreq_generic_scale_gpu(
	unsigned int freq_old, unsigned int freq_new,
	unsigned int volt_old, unsigned int volt_new,
	unsigned int vsram_old, unsigned int vsram_new);
static int __gpufreq_generic_scale_stack(
	unsigned int freq_old, unsigned int freq_new,
	unsigned int volt_old, unsigned int volt_new,
	unsigned int vsram_old, unsigned int vsram_new);
static int __gpufreq_custom_commit_gpu(unsigned int target_freq,
	unsigned int target_volt, enum gpufreq_dvfs_state key);
static int __gpufreq_custom_commit_stack(unsigned int target_freq,
	unsigned int target_volt, enum gpufreq_dvfs_state key);
static int __gpufreq_freq_scale_gpu(unsigned int freq_old, unsigned int freq_new);
static int __gpufreq_freq_scale_stack(unsigned int freq_old, unsigned int freq_new);
static int __gpufreq_volt_scale_gpu(
	unsigned int vgpu_old, unsigned int vgpu_new,
	unsigned int vsram_old, unsigned int vsram_new);
static int __gpufreq_volt_scale_stack(
	unsigned int vgpu_old, unsigned int vgpu_new,
	unsigned int vsram_old, unsigned int vsram_new);
static int __gpufreq_switch_clksrc_gpu(enum gpufreq_clk_src clksrc);
static int __gpufreq_switch_clksrc_stack(enum gpufreq_clk_src clksrc);
static unsigned int __gpufreq_calculate_pcw(unsigned int freq, enum gpufreq_posdiv posdiv_power);
static unsigned int __gpufreq_settle_time_vstack(unsigned int direction, int deltaV);
static void __gpufreq_delsel_control(unsigned int direction);
static void __gpufreq_dvfs_timing_control(unsigned int direction);
/* get function */
static unsigned int __gpufreq_get_fmeter_fgpu(void);
static unsigned int __gpufreq_get_fmeter_fstack(void);
static unsigned int __gpufreq_get_fmeter_main_fgpu(void);
static unsigned int __gpufreq_get_fmeter_main_fstack(void);
static unsigned int __gpufreq_get_fmeter_sub_fgpu(void);
static unsigned int __gpufreq_get_fmeter_sub_fstack(void);
static unsigned int __gpufreq_get_real_fgpu(void);
static unsigned int __gpufreq_get_real_fstack(void);
static unsigned int __gpufreq_get_real_vgpu(void);
static unsigned int __gpufreq_get_real_vstack(void);
static unsigned int __gpufreq_get_real_vsram(void);
static void __gpufreq_get_hw_constraint_fgpu(
	unsigned int fstack, int *oppidx_gpu, unsigned int *fgpu);
static void __gpufreq_get_hw_constraint_fstack(
	unsigned int fgpu, int *oppidx_stack, unsigned int *fstack);
static unsigned int __gpufreq_hw_constraint_parking(unsigned int volt_old, unsigned int volt_new);
static void __gpufreq_update_hw_constraint_volt(void);
static enum gpufreq_posdiv __gpufreq_get_real_posdiv_gpu(void);
static enum gpufreq_posdiv __gpufreq_get_real_posdiv_stack(void);
static enum gpufreq_posdiv __gpufreq_get_posdiv_by_fgpu(unsigned int freq);
static enum gpufreq_posdiv __gpufreq_get_posdiv_by_fstack(unsigned int freq);
/* aging sensor function */
static void __gpufreq_asensor_read_register(u32 *a_tn_lvt_cnt,
	u32 *a_tn_ulvt_cnt, u32 *a_tn_ulvtll_cnt);
static unsigned int __gpufreq_asensor_read_efuse(u32 *a_t0_lvt_rt, u32 *a_t0_ulvt_rt,
	u32 *a_t0_ulvtll_rt, u32 *a_shift_error, u32 *efuse_error);
static unsigned int __gpufreq_get_aging_table_idx(u32 a_t0_lvt_rt, u32 a_t0_ulvt_rt,
	u32 a_t0_ulvtll_rt, u32 a_shift_error, u32 efuse_error, u32 a_tn_lvt_cnt,
	u32 a_tn_ulvt_cnt, u32 a_tn_ulvtll_cnt, unsigned int is_efuse_read_success);
/* power control function */
static int __gpufreq_clock_control(enum gpufreq_power_state power);
static int __gpufreq_mtcmos_control(enum gpufreq_power_state power);
static int __gpufreq_buck_control(enum gpufreq_power_state power);
static void __gpufreq_mfg_backup_restore(enum gpufreq_power_state power);
static void __gpufreq_aoc_control(enum gpufreq_power_state power);
static void __gpufreq_hw_dcm_control(void);
static void __gpufreq_acp_control(void);
static void __gpufreq_gpm_control(void);
static void __gpufreq_slc_control(void);

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

static void __iomem *g_mfg_pll_base;
static void __iomem *g_mfgsc_pll_base;
static void __iomem *g_mfg_rpc_base;
static void __iomem *g_mfg_top_base;
static void __iomem *g_sleep;
static void __iomem *g_nth_emicfg_base;
static void __iomem *g_sth_emicfg_base;
static void __iomem *g_nth_emicfg_ao_mem_base;
static void __iomem *g_sth_emicfg_ao_mem_base;
static void __iomem *g_infracfg_ao_base;
static void __iomem *g_infra_ao_debug_ctrl;
static void __iomem *g_infra_ao1_debug_ctrl;
static void __iomem *g_nth_emi_ao_debug_ctrl;
static void __iomem *g_efuse_base;
static void __iomem *g_mfg_cpe_control_base;
static void __iomem *g_mfg_cpe_sensor_base;
static void __iomem *g_topckgen_base;
static void __iomem *g_mali_base;
static struct gpufreq_pmic_info *g_pmic;
static struct gpufreq_clk_info *g_clk;
static struct gpufreq_mtcmos_info *g_mtcmos;
static struct gpufreq_status g_gpu;
static struct gpufreq_status g_stack;
static struct gpufreq_asensor_info g_asensor_info;
static unsigned int g_shader_present;
static unsigned int g_stress_test_enable;
static unsigned int g_aging_enable;
static unsigned int g_avs_enable;
static unsigned int g_aging_load;
static unsigned int g_mcl50_load;
static unsigned int g_gpm_enable;
static unsigned int g_gpueb_support;
static unsigned int g_dvfs_timing_park_volt;
static unsigned int g_dvfs_timing_park_reg;
static unsigned int g_desel_ulv_park_volt;
static unsigned int g_desel_ulv_park_reg;
static enum gpufreq_dvfs_state g_dvfs_state;
static DEFINE_MUTEX(gpufreq_lock);

static struct gpufreq_platform_fp platform_ap_fp = {
	.bringup = __gpufreq_bringup,
	.power_ctrl_enable = __gpufreq_power_ctrl_enable,
	.get_power_state = __gpufreq_get_power_state,
	.get_dvfs_state = __gpufreq_get_dvfs_state,
	.get_shader_present = __gpufreq_get_shader_present,
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
	.get_cur_fstack = __gpufreq_get_cur_fstack,
	.get_cur_vstack = __gpufreq_get_cur_vstack,
	.get_cur_vsram_stack = __gpufreq_get_cur_vsram_stack,
	.get_cur_pstack = __gpufreq_get_cur_pstack,
	.get_max_pstack = __gpufreq_get_max_pstack,
	.get_min_pstack = __gpufreq_get_min_pstack,
	.get_cur_idx_stack = __gpufreq_get_cur_idx_stack,
	.get_opp_num_stack = __gpufreq_get_opp_num_stack,
	.get_signed_opp_num_stack = __gpufreq_get_signed_opp_num_stack,
	.get_working_table_stack = __gpufreq_get_working_table_stack,
	.get_signed_table_stack = __gpufreq_get_signed_table_stack,
	.get_debug_opp_info_stack = __gpufreq_get_debug_opp_info_stack,
	.get_fstack_by_idx = __gpufreq_get_fstack_by_idx,
	.get_pstack_by_idx = __gpufreq_get_pstack_by_idx,
	.get_idx_by_fstack = __gpufreq_get_idx_by_fstack,
	.get_lkg_pstack = __gpufreq_get_lkg_pstack,
	.get_dyn_pstack = __gpufreq_get_dyn_pstack,
	.generic_commit_stack = __gpufreq_generic_commit_stack,
	.fix_target_oppidx_stack = __gpufreq_fix_target_oppidx_stack,
	.fix_custom_freq_volt_stack = __gpufreq_fix_custom_freq_volt_stack,
	.set_timestamp = __gpufreq_set_timestamp,
	.check_bus_idle = __gpufreq_check_bus_idle,
	.dump_infra_status = __gpufreq_dump_infra_status,
	.set_stress_test = __gpufreq_set_stress_test,
	.set_aging_mode = __gpufreq_set_aging_mode,
	.set_gpm_mode = __gpufreq_set_gpm_mode,
	.get_asensor_info = __gpufreq_get_asensor_info,
	.get_core_mask_table = __gpufreq_get_core_mask_table,
	.get_core_num = __gpufreq_get_core_num,
	.pdc_control = __gpufreq_pdc_control,
	.fake_spm_mtcmos_control = __gpufreq_fake_spm_mtcmos_control,
};

static struct gpufreq_platform_fp platform_eb_fp = {
	.bringup = __gpufreq_bringup,
	.set_timestamp = __gpufreq_set_timestamp,
	.check_bus_idle = __gpufreq_check_bus_idle,
	.dump_infra_status = __gpufreq_dump_infra_status,
	.get_dyn_pgpu = __gpufreq_get_dyn_pgpu,
	.get_dyn_pstack = __gpufreq_get_dyn_pstack,
	.get_core_mask_table = __gpufreq_get_core_mask_table,
	.get_core_num = __gpufreq_get_core_num,
	.pdc_control = __gpufreq_pdc_control,
	.fake_spm_mtcmos_control = __gpufreq_fake_spm_mtcmos_control,
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
	if (g_stack.power_count > 0)
		return POWER_ON;
	else
		return POWER_OFF;
}

/* API: get DVFS state (free/disable/keep) */
unsigned int __gpufreq_get_dvfs_state(void)
{
	return g_dvfs_state;
}

/* API: get GPU shader stack */
unsigned int __gpufreq_get_shader_present(void)
{
	return g_shader_present;
}

/* API: get current Freq of GPU */
unsigned int __gpufreq_get_cur_fgpu(void)
{
	return g_gpu.cur_freq;
}

/* API: get current Freq of STACK */
unsigned int __gpufreq_get_cur_fstack(void)
{
	return g_stack.cur_freq;
}

/* API: get current Volt of GPU */
unsigned int __gpufreq_get_cur_vgpu(void)
{
	return g_gpu.cur_volt;
}

/* API: get current Volt of STACK */
unsigned int __gpufreq_get_cur_vstack(void)
{
	return g_stack.buck_count ? g_stack.cur_volt : 0;
}

/* API: get current Vsram of GPU */
unsigned int __gpufreq_get_cur_vsram_gpu(void)
{
	return g_gpu.cur_vsram;
}

/* API: get current Vsram of STACK */
unsigned int __gpufreq_get_cur_vsram_stack(void)
{
	return g_stack.cur_vsram;
}

/* API: get current Power of GPU */
unsigned int __gpufreq_get_cur_pgpu(void)
{
	return g_gpu.working_table[g_gpu.cur_oppidx].power;
}

/* API: get current Power of STACK */
unsigned int __gpufreq_get_cur_pstack(void)
{
	return g_stack.working_table[g_stack.cur_oppidx].power;
}

/* API: get max Power of GPU */
unsigned int __gpufreq_get_max_pgpu(void)
{
	return g_gpu.working_table[g_gpu.max_oppidx].power;
}

/* API: get max Power of STACK */
unsigned int __gpufreq_get_max_pstack(void)
{
	return g_stack.working_table[g_stack.max_oppidx].power;
}

/* API: get min Power of GPU */
unsigned int __gpufreq_get_min_pgpu(void)
{
	return g_gpu.working_table[g_gpu.min_oppidx].power;
}

/* API: get min Power of STACK */
unsigned int __gpufreq_get_min_pstack(void)
{
	return g_stack.working_table[g_stack.min_oppidx].power;
}

/* API: get current working OPP index of GPU */
int __gpufreq_get_cur_idx_gpu(void)
{
	return g_gpu.cur_oppidx;
}

/* API: get current working OPP index of STACK */
int __gpufreq_get_cur_idx_stack(void)
{
	return g_stack.cur_oppidx;
}

/* API: get number of working OPP of GPU */
int __gpufreq_get_opp_num_gpu(void)
{
	return g_gpu.opp_num;
}

/* API: get number of working OPP of STACK */
int __gpufreq_get_opp_num_stack(void)
{
	return g_stack.opp_num;
}

/* API: get number of signed OPP of GPU */
int __gpufreq_get_signed_opp_num_gpu(void)
{
	return g_gpu.signed_opp_num;
}

/* API: get number of signed OPP of STACK */
int __gpufreq_get_signed_opp_num_stack(void)
{
	return g_stack.signed_opp_num;
}

/* API: get poiner of working OPP table of GPU */
const struct gpufreq_opp_info *__gpufreq_get_working_table_gpu(void)
{
	return g_gpu.working_table;
}

/* API: get poiner of working OPP table of STACK */
const struct gpufreq_opp_info *__gpufreq_get_working_table_stack(void)
{
	return g_stack.working_table;
}

/* API: get poiner of signed OPP table of GPU */
const struct gpufreq_opp_info *__gpufreq_get_signed_table_gpu(void)
{
	return g_gpu.signed_table;
}

/* API: get poiner of signed OPP table of STACK */
const struct gpufreq_opp_info *__gpufreq_get_signed_table_stack(void)
{
	return g_stack.signed_table;
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
	opp_info.avs_enable = g_avs_enable;
	opp_info.gpm_enable = g_gpm_enable;
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

	mutex_lock(&gpufreq_lock);
	opp_info.cur_oppidx = g_stack.cur_oppidx;
	opp_info.cur_freq = g_stack.cur_freq;
	opp_info.cur_volt = g_stack.cur_volt;
	opp_info.cur_vsram = g_stack.cur_vsram;
	opp_info.power_count = g_stack.power_count;
	opp_info.cg_count = g_stack.cg_count;
	opp_info.mtcmos_count = g_stack.mtcmos_count;
	opp_info.buck_count = g_stack.buck_count;
	opp_info.segment_id = g_stack.segment_id;
	opp_info.opp_num = g_stack.opp_num;
	opp_info.signed_opp_num = g_stack.signed_opp_num;
	opp_info.dvfs_state = g_dvfs_state;
	opp_info.shader_present = g_shader_present;
	opp_info.aging_enable = g_aging_enable;
	opp_info.avs_enable = g_avs_enable;
	opp_info.gpm_enable = g_gpm_enable;
	if (__gpufreq_get_power_state()) {
		opp_info.fmeter_freq = __gpufreq_get_fmeter_fstack();
		opp_info.con1_freq = __gpufreq_get_real_fstack();
		opp_info.regulator_volt = __gpufreq_get_real_vstack();
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

/* API: get asensor info for Proc show */
struct gpufreq_asensor_info __gpufreq_get_asensor_info(void)
{
	struct gpufreq_asensor_info asensor_info = {};

#if GPUFREQ_ASENSOR_ENABLE
	asensor_info = g_asensor_info;
#endif /* GPUFREQ_ASENSOR_ENABLE */

	return asensor_info;
}

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
	if (oppidx >= 0 && oppidx < g_stack.opp_num)
		return g_stack.working_table[oppidx].freq;
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

/* API: get Power of STACK via OPP index */
unsigned int __gpufreq_get_pstack_by_idx(int oppidx)
{
	if (oppidx >= 0 && oppidx < g_stack.opp_num)
		return g_stack.working_table[oppidx].power;
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
	int oppidx = -1;
	int i = 0;

	/* find the smallest index that satisfy given freq */
	for (i = g_stack.min_oppidx; i >= g_stack.max_oppidx; i--) {
		if (g_stack.working_table[i].freq >= freq)
			break;
	}
	/* use max_oppidx if not found */
	oppidx = (i > g_stack.max_oppidx) ? i : g_stack.max_oppidx;

	return oppidx;
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
	int oppidx = -1;
	int i = 0;

	/* find the smallest index that satisfy given volt */
	for (i = g_stack.min_oppidx; i >= g_stack.max_oppidx; i--) {
		if (g_stack.working_table[i].volt >= volt)
			break;
	}
	/* use max_oppidx if not found */
	oppidx = (i > g_stack.max_oppidx) ? i : g_stack.max_oppidx;

	return oppidx;
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
	int oppidx = -1;
	int i = 0;

	/* find the smallest index that satisfy given power */
	for (i = g_stack.min_oppidx; i >= g_stack.max_oppidx; i--) {
		if (g_stack.working_table[i].power >= power)
			break;
	}
	/* use max_oppidx if not found */
	oppidx = (i > g_stack.max_oppidx) ? i : g_stack.max_oppidx;

	return oppidx;
}

/* API: get Volt of SRAM via volt of STACK */
unsigned int __gpufreq_get_vsram_by_vstack(unsigned int volt)
{
	GPUFREQ_UNREFERENCED(volt);

	return VSRAM_LEVEL_0;
}

/* API: get leakage Power of GPU */
unsigned int __gpufreq_get_lkg_pgpu(unsigned int volt)
{
	GPUFREQ_UNREFERENCED(volt);

	return 0;
}

/* API: get dynamic Power of GPU */
unsigned int __gpufreq_get_dyn_pgpu(unsigned int freq, unsigned int volt)
{
	GPUFREQ_UNREFERENCED(freq);
	GPUFREQ_UNREFERENCED(volt);

	return 0;
}

/* API: get leakage Power of STACK */
unsigned int __gpufreq_get_lkg_pstack(unsigned int volt)
{
	GPUFREQ_UNREFERENCED(volt);

	return STACK_LEAKAGE_POWER;
}

/* API: get dynamic Power of STACK */
unsigned int __gpufreq_get_dyn_pstack(unsigned int freq, unsigned int volt)
{
	unsigned int p_dynamic = STACK_ACT_REF_POWER;
	unsigned int ref_freq = STACK_ACT_REF_FREQ;
	unsigned int ref_volt = STACK_ACT_REF_VOLT;

	p_dynamic = p_dynamic *
		((freq * 100) / ref_freq) *
		((volt * 100) / ref_volt) *
		((volt * 100) / ref_volt) /
		(100 * 100 * 100);

	return p_dynamic;
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
		g_stack.power_count, g_stack.buck_count,
		g_stack.mtcmos_count, g_stack.cg_count);

	if (power == POWER_ON) {
		g_gpu.power_count++;
		g_stack.power_count++;
	} else {
		g_gpu.power_count--;
		g_stack.power_count--;
		__gpufreq_check_pending_exception();
	}
	__gpufreq_footprint_power_count(g_stack.power_count);

	if (power == POWER_ON && g_stack.power_count == 1) {
		__gpufreq_footprint_power_step(GPUFREQ_POWER_STEP_01);

		/* control AOC after MFG_0 on */
		__gpufreq_aoc_control(POWER_ON);
		__gpufreq_footprint_power_step(GPUFREQ_POWER_STEP_02);

		/* control Buck */
		ret = __gpufreq_buck_control(POWER_ON);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to control Buck: On (%d)", ret);
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

		/* control clock */
		ret = __gpufreq_clock_control(POWER_ON);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to control CLOCK: On (%d)", ret);
			ret = GPUFREQ_EINVAL;
			goto done_unlock;
		}
		__gpufreq_footprint_power_step(GPUFREQ_POWER_STEP_05);

		/* restore MFG registers */
		__gpufreq_mfg_backup_restore(POWER_ON);
		__gpufreq_footprint_power_step(GPUFREQ_POWER_STEP_06);

		/* set PDCv2 register when power on, let GPU DDK control MTCMOS itself */
		__gpufreq_pdc_control(POWER_ON);
		__gpufreq_footprint_power_step(GPUFREQ_POWER_STEP_07);

		/* control ACP */
		__gpufreq_acp_control();
		__gpufreq_footprint_power_step(GPUFREQ_POWER_STEP_08);

		/* control HWDCM */
		__gpufreq_hw_dcm_control();
		__gpufreq_footprint_power_step(GPUFREQ_POWER_STEP_09);

		/* control GPM 1.0 */
		if (g_gpm_enable)
			__gpufreq_gpm_control();
		__gpufreq_footprint_power_step(GPUFREQ_POWER_STEP_0A);

		/* TODO: config SLC policy */
		//__gpufreq_slc_control();
		__gpufreq_footprint_power_step(GPUFREQ_POWER_STEP_0B);

		/* TODO: control DFD */
		//__gpudfd_config_dfd(true);
		__gpufreq_footprint_power_step(GPUFREQ_POWER_STEP_0C);

		/* free DVFS when power on */
		g_dvfs_state &= ~DVFS_POWEROFF;
	} else if (power == POWER_OFF && g_stack.power_count == 0) {
		__gpufreq_footprint_power_step(GPUFREQ_POWER_STEP_0D);

		/* freeze DVFS when power off */
		g_dvfs_state |= DVFS_POWEROFF;

		/* control DFD */
		//__gpudfd_config_dfd(false);
		__gpufreq_footprint_power_step(GPUFREQ_POWER_STEP_0E);

		/* backup MFG registers */
		__gpufreq_mfg_backup_restore(POWER_OFF);
		__gpufreq_footprint_power_step(GPUFREQ_POWER_STEP_0F);

		/* control clock */
		ret = __gpufreq_clock_control(POWER_OFF);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to control CLOCK: Off (%d)", ret);
			ret = GPUFREQ_EINVAL;
			goto done_unlock;
		}
		__gpufreq_footprint_power_step(GPUFREQ_POWER_STEP_10);

		/* control MTCMOS */
		ret = __gpufreq_mtcmos_control(POWER_OFF);
		if (unlikely(ret < 0)) {
			GPUFREQ_LOGE("fail to control MTCMOS: Off (%d)", ret);
			ret = GPUFREQ_EINVAL;
			goto done_unlock;
		}
		__gpufreq_footprint_power_step(GPUFREQ_POWER_STEP_11);

		/* control Buck */
		ret = __gpufreq_buck_control(POWER_OFF);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to control Buck: Off (%d)", ret);
			ret = GPUFREQ_EINVAL;
			goto done_unlock;
		}
		__gpufreq_footprint_power_step(GPUFREQ_POWER_STEP_12);

		/* control AOC before MFG_0 off */
		__gpufreq_aoc_control(POWER_OFF);
		__gpufreq_footprint_power_step(GPUFREQ_POWER_STEP_13);
	}

	/* return power count if successfully control power */
	ret = g_stack.power_count;

done_unlock:
	GPUFREQ_LOGD("PWR_STATUS (0x%x): 0x%08x",
		(0x1C001000 + PWR_STATUS_OFS), readl(g_sleep + PWR_STATUS_OFS));

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
	/* STACK */
	struct gpufreq_opp_info *working_stack = g_stack.working_table;
	int cur_oppidx_stack = 0, target_oppidx_stack = 0;
	unsigned int cur_fstack = 0, cur_vstack = 0, cur_vsram_stack = 0;
	unsigned int target_fstack = 0, target_vstack = 0, target_vsram_stack = 0;
	/* GPU */
	struct gpufreq_opp_info *working_gpu = g_gpu.working_table;
	int cur_oppidx_gpu = 0, target_oppidx_gpu = 0;
	int opp_num_gpu = g_gpu.opp_num;
	unsigned int cur_fgpu = 0, cur_vgpu = 0, cur_vsram_gpu = 0;
	unsigned int target_fgpu = 0, target_vgpu = 0, target_vsram_gpu = 0;
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("target_oppidx=%d, key=%d", target_oppidx, key);

	/* validate 0 <= target_oppidx < opp_num */
	if (target_oppidx < 0 || target_oppidx >= opp_num_gpu) {
		GPUFREQ_LOGE("invalid target GPU OPP index: %d (OPP_NUM: %d)",
			target_oppidx, opp_num_gpu);
		ret = GPUFREQ_EINVAL;
		goto done;
	}

	mutex_lock(&gpufreq_lock);

	/* check dvfs state */
	if (g_dvfs_state & ~key) {
		GPUFREQ_LOGD("unavailable DVFS state (0x%x)", g_dvfs_state);
		ret = GPUFREQ_SUCCESS;
		goto done_unlock;
	}

	/* prepare GPU setting */
	cur_oppidx_gpu = g_gpu.cur_oppidx;
	cur_fgpu = g_gpu.cur_freq;
	cur_vgpu = g_gpu.cur_volt;
	cur_vsram_gpu = g_gpu.cur_vsram;
	target_oppidx_gpu = target_oppidx;
	target_fgpu = working_gpu[target_oppidx].freq;
	target_vgpu = working_gpu[target_oppidx].volt;
	target_vsram_gpu = working_gpu[target_oppidx].vsram;

	/* prepare STACK setting */
	cur_oppidx_stack = g_stack.cur_oppidx;
	cur_fstack = g_stack.cur_freq;
	cur_vstack = g_stack.cur_volt;
	cur_vsram_stack = g_stack.cur_vsram;
	__gpufreq_get_hw_constraint_fstack(target_fgpu, &target_oppidx_stack, &target_fstack);
	target_vstack = working_stack[target_oppidx_stack].volt;
	target_vsram_stack = working_stack[target_oppidx_stack].vsram;

	GPUFREQ_LOGD("begin to commit GPU OPP index: (%d->%d), STACK OPP index: (%d->%d)",
		cur_oppidx_gpu, target_oppidx_gpu, cur_oppidx_stack, target_oppidx_stack);

	/* GPU volt scaling up: GPU -> STACK */
	if (target_vgpu > cur_vgpu) {
		ret = __gpufreq_generic_scale_gpu(cur_fgpu, target_fgpu,
			cur_vgpu, target_vgpu, cur_vsram_gpu, target_vsram_gpu);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to scale GPU: Idx(%d->%d), Freq(%d->%d), Volt(%d->%d), Vsram(%d->%d)",
				cur_oppidx_gpu, target_oppidx_gpu, cur_fgpu, target_fgpu,
				cur_vgpu, target_vgpu, cur_vsram_gpu, target_vsram_gpu);
			goto done_unlock;
		}
		ret = __gpufreq_generic_scale_stack(cur_fstack, target_fstack,
			cur_vstack, target_vstack, cur_vsram_stack, target_vsram_stack);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to scale STACK: Idx(%d->%d), Freq(%d->%d), Volt(%d->%d), Vsram(%d->%d)",
				cur_oppidx_stack, target_oppidx_stack, cur_fstack, target_fstack,
				cur_vstack, target_vstack, cur_vsram_stack, target_vsram_stack);
			goto done_unlock;
		}
	/* GPU volt scaling down: STACK -> GPU */
	} else if (target_vgpu < cur_vgpu) {
		ret = __gpufreq_generic_scale_stack(cur_fstack, target_fstack,
			cur_vstack, target_vstack, cur_vsram_stack, target_vsram_stack);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to scale STACK: Idx(%d->%d), Freq(%d->%d), Volt(%d->%d), Vsram(%d->%d)",
				cur_oppidx_stack, target_oppidx_stack, cur_fstack, target_fstack,
				cur_vstack, target_vstack, cur_vsram_stack, target_vsram_stack);
			goto done_unlock;
		}
		ret = __gpufreq_generic_scale_gpu(cur_fgpu, target_fgpu,
			cur_vgpu, target_vgpu, cur_vsram_gpu, target_vsram_gpu);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to scale GPU: Idx(%d->%d), Freq(%d->%d), Volt(%d->%d), Vsram(%d->%d)",
				cur_oppidx_gpu, target_oppidx_gpu, cur_fgpu, target_fgpu,
				cur_vgpu, target_vgpu, cur_vsram_gpu, target_vsram_gpu);
			goto done_unlock;
		}
	/* GPU volt keep: GPU only */
	} else {
		ret = __gpufreq_generic_scale_gpu(cur_fgpu, target_fgpu,
			cur_vgpu, target_vgpu, cur_vsram_gpu, target_vsram_gpu);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to scale GPU: Idx(%d->%d), Freq(%d->%d), Volt(%d->%d), Vsram(%d->%d)",
				cur_oppidx_gpu, target_oppidx_gpu, cur_fgpu, target_fgpu,
				cur_vgpu, target_vgpu, cur_vsram_gpu, target_vsram_gpu);
			goto done_unlock;
		}
	}

	g_gpu.cur_oppidx = target_oppidx_gpu;
	g_stack.cur_oppidx = target_oppidx_stack;

	__gpufreq_footprint_oppidx(target_oppidx_stack);

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
	/* STACK */
	struct gpufreq_opp_info *working_stack = g_stack.working_table;
	int cur_oppidx_stack = 0, target_oppidx_stack = 0;
	int opp_num_stack = g_stack.opp_num;
	unsigned int cur_fstack = 0, cur_vstack = 0, cur_vsram_stack = 0;
	unsigned int target_fstack = 0, target_vstack = 0, target_vsram_stack = 0;
	/* GPU */
	struct gpufreq_opp_info *working_gpu = g_gpu.working_table;
	int cur_oppidx_gpu = 0, target_oppidx_gpu = 0;
	unsigned int cur_fgpu = 0, cur_vgpu = 0, cur_vsram_gpu = 0;
	unsigned int target_fgpu = 0, target_vgpu = 0, target_vsram_gpu = 0;
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("target_oppidx=%d, key=%d", target_oppidx, key);

	/* validate 0 <= target_oppidx < opp_num */
	if (target_oppidx < 0 || target_oppidx >= opp_num_stack) {
		GPUFREQ_LOGE("invalid target STACK OPP index: %d (OPP_NUM: %d)",
			target_oppidx, opp_num_stack);
		ret = GPUFREQ_EINVAL;
		goto done;
	}

	mutex_lock(&gpufreq_lock);

	/* check dvfs state */
	if (g_dvfs_state & ~key) {
		GPUFREQ_LOGD("unavailable DVFS state (0x%x)", g_dvfs_state);
		ret = GPUFREQ_SUCCESS;
		goto done_unlock;
	}

	/* randomly replace target index */
	if (g_stress_test_enable) {
		get_random_bytes(&target_oppidx, sizeof(target_oppidx));
		target_oppidx = target_oppidx < 0 ?
			(target_oppidx*-1) % opp_num_stack : target_oppidx % opp_num_stack;
	}

	/* prepare STACK setting */
	cur_oppidx_stack = g_stack.cur_oppidx;
	cur_fstack = g_stack.cur_freq;
	cur_vstack = g_stack.cur_volt;
	cur_vsram_stack = g_stack.cur_vsram;
	target_oppidx_stack = target_oppidx;
	target_fstack = working_stack[target_oppidx].freq;
	target_vstack = working_stack[target_oppidx].volt;
	target_vsram_stack = working_stack[target_oppidx].vsram;

	/* prepare GPU setting */
	cur_oppidx_gpu = g_gpu.cur_oppidx;
	cur_fgpu = g_gpu.cur_freq;
	cur_vgpu = g_gpu.cur_volt;
	cur_vsram_gpu = g_gpu.cur_vsram;
	__gpufreq_get_hw_constraint_fgpu(target_fstack, &target_oppidx_gpu, &target_fgpu);
	target_vgpu = working_gpu[target_oppidx_gpu].volt;
	target_vsram_gpu = working_gpu[target_oppidx_gpu].vsram;

	GPUFREQ_LOGD("begin to commit STACK OPP index: (%d->%d), GPU OPP index: (%d->%d)",
		cur_oppidx_stack, target_oppidx_stack, cur_oppidx_gpu, target_oppidx_gpu);

	/* STACK volt scaling up: GPU -> STACK */
	if (target_vstack > cur_vstack) {
		ret = __gpufreq_generic_scale_gpu(cur_fgpu, target_fgpu,
			cur_vgpu, target_vgpu, cur_vsram_gpu, target_vsram_gpu);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to scale GPU: Idx(%d->%d), Freq(%d->%d), Volt(%d->%d), Vsram(%d->%d)",
				cur_oppidx_gpu, target_oppidx_gpu, cur_fgpu, target_fgpu,
				cur_vgpu, target_vgpu, cur_vsram_gpu, target_vsram_gpu);
			goto done_unlock;
		}
		ret = __gpufreq_generic_scale_stack(cur_fstack, target_fstack,
			cur_vstack, target_vstack, cur_vsram_stack, target_vsram_stack);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to scale STACK: Idx(%d->%d), Freq(%d->%d), Volt(%d->%d), Vsram(%d->%d)",
				cur_oppidx_stack, target_oppidx_stack, cur_fstack, target_fstack,
				cur_vstack, target_vstack, cur_vsram_stack, target_vsram_stack);
			goto done_unlock;
		}
	/* STACK volt scaling down: STACK -> GPU */
	} else if (target_vstack < cur_vstack) {
		ret = __gpufreq_generic_scale_stack(cur_fstack, target_fstack,
			cur_vstack, target_vstack, cur_vsram_stack, target_vsram_stack);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to scale STACK: Idx(%d->%d), Freq(%d->%d), Volt(%d->%d), Vsram(%d->%d)",
				cur_oppidx_stack, target_oppidx_stack, cur_fstack, target_fstack,
				cur_vstack, target_vstack, cur_vsram_stack, target_vsram_stack);
			goto done_unlock;
		}
		ret = __gpufreq_generic_scale_gpu(cur_fgpu, target_fgpu,
			cur_vgpu, target_vgpu, cur_vsram_gpu, target_vsram_gpu);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to scale GPU: Idx(%d->%d), Freq(%d->%d), Volt(%d->%d), Vsram(%d->%d)",
				cur_oppidx_gpu, target_oppidx_gpu, cur_fgpu, target_fgpu,
				cur_vgpu, target_vgpu, cur_vsram_gpu, target_vsram_gpu);
			goto done_unlock;
		}
	/* STACK volt keep: STACK only */
	} else {
		ret = __gpufreq_generic_scale_stack(cur_fstack, target_fstack,
			cur_vstack, target_vstack, cur_vsram_stack, target_vsram_stack);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to scale STACK: Idx(%d->%d), Freq(%d->%d), Volt(%d->%d), Vsram(%d->%d)",
				cur_oppidx_stack, target_oppidx_stack, cur_fstack, target_fstack,
				cur_vstack, target_vstack, cur_vsram_stack, target_vsram_stack);
			goto done_unlock;
		}
	}

	g_gpu.cur_oppidx = target_oppidx_gpu;
	g_stack.cur_oppidx = target_oppidx_stack;

	__gpufreq_footprint_oppidx(target_oppidx_stack);

done_unlock:
	mutex_unlock(&gpufreq_lock);

done:
	GPUFREQ_TRACE_END();

	return ret;
}

/* API: fix OPP of GPU via given OPP index */
int __gpufreq_fix_target_oppidx_gpu(int oppidx)
{
	GPUFREQ_UNREFERENCED(oppidx);

	return GPUFREQ_EINVAL;
}

/* API: fix OPP of STACK via given OPP index */
int __gpufreq_fix_target_oppidx_stack(int oppidx)
{
	int opp_num = g_stack.opp_num;
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

		ret = __gpufreq_generic_commit_stack(oppidx, DVFS_DEBUG_KEEP);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to commit STACK OPP index: %d (%d)", oppidx, ret);
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
	GPUFREQ_UNREFERENCED(freq);
	GPUFREQ_UNREFERENCED(volt);

	return GPUFREQ_EINVAL;
}

/* API: fix Freq and Volt of STACK via given Freq and Volt */
int __gpufreq_fix_custom_freq_volt_stack(unsigned int freq, unsigned int volt)
{
	struct gpufreq_opp_info *signed_table = g_stack.signed_table;
	unsigned int max_oppidx = g_stack.max_oppidx;
	unsigned int min_oppidx = g_stack.min_oppidx;
	unsigned int max_freq = 0, min_freq = 0;
	unsigned int max_volt = 0, min_volt = 0;
	int ret = GPUFREQ_SUCCESS;

	ret = __gpufreq_power_control(POWER_ON);
	if (unlikely(ret < 0)) {
		GPUFREQ_LOGE("fail to control power state: %d (%d)", POWER_ON, ret);
		goto done;
	}

	/*
	 * because of DVFS timing issues,
	 * we only support up/down to max/min freq/volt in OPP table
	 */
	max_freq = signed_table[max_oppidx].freq;
	min_freq = signed_table[min_oppidx].freq;
	max_volt = signed_table[max_oppidx].volt;
	min_volt = signed_table[min_oppidx].volt;

	if (freq == 0 && volt == 0) {
		__gpufreq_set_dvfs_state(false, DVFS_DEBUG_KEEP);
		ret = GPUFREQ_SUCCESS;
	} else if (freq > max_freq || freq < min_freq) {
		GPUFREQ_LOGE("invalid fixed Freq: %d", freq);
		ret = GPUFREQ_EINVAL;
	} else if (volt > max_volt || volt < min_volt) {
		GPUFREQ_LOGE("invalid fixed Volt: %d", volt);
		ret = GPUFREQ_EINVAL;
	} else {
		__gpufreq_set_dvfs_state(true, DVFS_DEBUG_KEEP);

		ret = __gpufreq_custom_commit_stack(freq, volt, DVFS_DEBUG_KEEP);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to commit STACK Freq: %d, Volt: %d (%d)",
				freq, volt, ret);
			__gpufreq_set_dvfs_state(false, DVFS_DEBUG_KEEP);
		}
	}

	__gpufreq_power_control(POWER_OFF);

done:
	return ret;
}

void __gpufreq_set_timestamp(void)
{
	/* MFG_TIMESTAMP 0x13FBF130 [0] enable timestamp = 1'b1 */
	/* MFG_TIMESTAMP 0x13FBF130 [1] timer from internal module = 1'b0 */
	/* MFG_TIMESTAMP 0x13FBF130 [1] timer from soc = 1'b0 */
	writel(0x00000003, g_mfg_top_base + 0x130);
}

void __gpufreq_check_bus_idle(void)
{
	u32 val = 0;

	/* MFG_QCHANNEL_CON 0x13FBF0B4 [0] MFG_ACTIVE_SEL = 1'b1 */
	val = readl(g_mfg_top_base + 0xB4);
	val |= (1UL << 0);
	writel(val, g_mfg_top_base + 0xB4);

	/* MFG_DEBUG_SEL 0x13FBF170 [1:0] MFG_DEBUG_TOP_SEL = 2'b11 */
	val = readl(g_mfg_top_base + 0x170);
	val |= (1UL << 0);
	val |= (1UL << 1);
	writel(val, g_mfg_top_base + 0x170);


	/*
	 * polling MFG_DEBUG_TOP 0x13FBF178 [0] MFG_DEBUG_TOP
	 * 0x0: bus idle
	 * 0x1: bus busy
	 */
	do {
		val = readl(g_mfg_top_base + 0x178);
	} while (val & 0x1);
}

void __gpufreq_dump_infra_status(void)
{
	u32 val = 0;

	GPUFREQ_LOGI("== [GPUFREQ INFRA STATUS] ==");
	if (g_gpueb_support) {
		GPUFREQ_LOGI("[Regulator] Vcore: %d, Vsram: %d, Vstack: %d",
			__gpufreq_get_real_vgpu(), __gpufreq_get_real_vsram(),
			__gpufreq_get_real_vstack());
		GPUFREQ_LOGI("[Clk] MFG_PLL: %d, MFG_SEL_0: 0x%x, MFGSC_PLL: %d, MFG_SEL_1: 0x%x",
			__gpufreq_get_real_fgpu(),
			readl(g_topckgen_base + 0x1F0) & MFG_SEL_0_MASK,
			__gpufreq_get_real_fstack(),
			readl(g_topckgen_base + 0x1F0) & MFG_SEL_1_MASK);
	} else {
		GPUFREQ_LOGI("GPU[%d] Freq: %d, Vgpu: %d, Vsram: %d",
			g_gpu.cur_oppidx, g_gpu.cur_freq,
			g_gpu.cur_volt, g_gpu.cur_vsram);
		GPUFREQ_LOGI("STACK[%d] Freq: %d, Vgpu: %d, Vsram: %d",
			g_stack.cur_oppidx, g_stack.cur_freq,
			g_stack.cur_volt, g_stack.cur_vsram);
	}

	/* 0x13FBF000, 0x13F90000 */
	if (g_mfg_top_base && g_mfg_rpc_base) {
		/* MFG_QCHANNEL_CON 0x13FBF0B4 [0] MFG_ACTIVE_SEL = 1'b1 */
		val = readl(g_mfg_top_base + 0xB4);
		val |= (1UL << 0);
		writel(val, g_mfg_top_base + 0xB4);
		/* MFG_DEBUG_SEL 0x13FBF170 [1:0] MFG_DEBUG_TOP_SEL = 2'b11 */
		val = readl(g_mfg_top_base + 0x170);
		val |= (1UL << 0);
		val |= (1UL << 1);
		writel(val, g_mfg_top_base + 0x170);

		/* MFG_DEBUG_SEL */
		/* MFG_DEBUG_TOP */
		/* MFG_GPU_EB_SPM_RPC_SLP_PROT_EN_STA */
		GPUFREQ_LOGI("%-7s (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x",
			"[MFG]",
			(0x13FBF000 + 0x170), readl(g_mfg_top_base + 0x170),
			(0x13FBF000 + 0x178), readl(g_mfg_top_base + 0x178),
			(0x13F90000 + 0x1048), readl(g_mfg_rpc_base + 0x1048));
	}

	/* 0x1021C000, 0x1021E000 */
	if (g_nth_emicfg_base && g_sth_emicfg_base) {
		/* NTH_EMICFG_REG_MFG_EMI1_GALS_SLV_DBG */
		/* NTH_EMICFG_REG_MFG_EMI0_GALS_SLV_DBG */
		/* STH_EMICFG_REG_MFG_EMI1_GALS_SLV_DBG */
		/* STH_EMICFG_REG_MFG_EMI0_GALS_SLV_DBG */
		GPUFREQ_LOGI("%-7s (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x",
			"[EMI]",
			(0x1021C000 + 0x82C), readl(g_nth_emicfg_base + 0x82C),
			(0x1021C000 + 0x830), readl(g_nth_emicfg_base + 0x830),
			(0x1021E000 + 0x82C), readl(g_sth_emicfg_base + 0x82C),
			(0x1021E000 + 0x830), readl(g_sth_emicfg_base + 0x830));
	}

	/* 0x10270000, 0x1030E000 */
	if (g_nth_emicfg_ao_mem_base && g_sth_emicfg_ao_mem_base) {
		/* NTH_EMICFG_AO_MEM_REG_M6M7_IDLE_BIT_EN_1 */
		/* NTH_EMICFG_AO_MEM_REG_M6M7_IDLE_BIT_EN_0 */
		/* STH_EMICFG_AO_MEM_REG_M6M7_IDLE_BIT_EN_1 */
		/* STH_EMICFG_AO_MEM_REG_M6M7_IDLE_BIT_EN_0 */
		GPUFREQ_LOGI("%-7s (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x",
			"[EMI]",
			(0x10270000 + 0x228), readl(g_nth_emicfg_ao_mem_base + 0x228),
			(0x10270000 + 0x22C), readl(g_nth_emicfg_ao_mem_base + 0x22C),
			(0x1030E000 + 0x228), readl(g_sth_emicfg_ao_mem_base + 0x228),
			(0x1030E000 + 0x22C), readl(g_sth_emicfg_ao_mem_base + 0x22C));
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

	/* 0x1002B000, 0x10042000 */
	if (g_infra_ao1_debug_ctrl && g_nth_emi_ao_debug_ctrl) {
		/* INFRA_QAXI_AO_BUS_SUB1_U_DEBUG_CTRL_AO_INFRA_AO1_CTRL0 */
		/* NTH_EMI_AO_DEBUG_CTRL_EMI_AO_BUS_U_DEBUG_CTRL_AO_EMI_AO_CTRL0 */
		GPUFREQ_LOGI("%-7s (0x%x): 0x%08x, (0x%x): 0x%08x",
			"[INFRA]",
			(0x1002B000 + 0x000), readl(g_infra_ao1_debug_ctrl + 0x000),
			(0x10042000 + 0x000), readl(g_nth_emi_ao_debug_ctrl + 0x000));
	}

	/* 0x1C001000 */
	if (g_sleep) {
		/* MFG0_PWR_CON - MFG3_PWR_CON */
		GPUFREQ_LOGI("%-7s (0x%x-%x): 0x%08x 0x%08x 0x%08x 0x%08x",
			"[SPM]", (0x1C001000 + 0xEB8), 0xEC4,
			readl(g_sleep + 0xEB8), readl(g_sleep + 0xEBC),
			readl(g_sleep + 0xEC0), readl(g_sleep + 0xEC4));
		/* MFG4_PWR_CON - MFG7_PWR_CON */
		GPUFREQ_LOGI("%-7s (0x%x-%x): 0x%08x 0x%08x 0x%08x 0x%08x",
			"[SPM]", (0x1C001000 + 0xEC8), 0xED4,
			readl(g_sleep + 0xEC8), readl(g_sleep + 0xECC),
			readl(g_sleep + 0xED0), readl(g_sleep + 0xED4));
		/* MFG8_PWR_CON - MFG12_PWR_CON */
		GPUFREQ_LOGI("%-7s (0x%x-%x): 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x",
			"[SPM]", (0x1C001000 + 0xED8), 0xEE8,
			readl(g_sleep + 0xED8), readl(g_sleep + 0xEDC),
			readl(g_sleep + 0xEE0), readl(g_sleep + 0xEE4),
			readl(g_sleep + 0xEE8));
		/* XPU_PWR_STATUS */
		/* XPU_PWR_STATUS_2ND */
		GPUFREQ_LOGI("%-7s (0x%x): 0x%08x, (0x%x): 0x%08x",
			"[SPM]",
			(0x1C001000 + PWR_STATUS_OFS), readl(g_sleep + PWR_STATUS_OFS),
			(0x1C001000 + PWR_STATUS_2ND_OFS), readl(g_sleep + PWR_STATUS_2ND_OFS));
	}
}

/* API: get working OPP index of STACK limited by BATTERY_OC via given level */
int __gpufreq_get_batt_oc_idx(int batt_oc_level)
{
#if (GPUFREQ_BATT_OC_ENABLE && IS_ENABLED(CONFIG_MTK_BATTERY_OC_POWER_THROTTLING))
	if (batt_oc_level == BATTERY_OC_LEVEL_1)
		return __gpufreq_get_idx_by_fstack(GPUFREQ_BATT_OC_FREQ);
	else
		return GPUPPM_RESET_IDX;
#else
	GPUFREQ_UNREFERENCED(batt_oc_level);

	return GPUPPM_KEEP_IDX;
#endif /* GPUFREQ_BATT_OC_ENABLE && CONFIG_MTK_BATTERY_OC_POWER_THROTTLING */
}

/* API: get working OPP index of STACK limited by BATTERY_PERCENT via given level */
int __gpufreq_get_batt_percent_idx(int batt_percent_level)
{
#if (GPUFREQ_BATT_PERCENT_ENABLE && IS_ENABLED(CONFIG_MTK_BATTERY_PERCENT_THROTTLING))
	if (batt_percent_level == BATTERY_PERCENT_LEVEL_1)
		return GPUFREQ_BATT_PERCENT_IDX - g_stack.segment_upbound;
	else
		return GPUPPM_RESET_IDX;
#else
	GPUFREQ_UNREFERENCED(batt_percent_level);

	return GPUPPM_KEEP_IDX;
#endif /* GPUFREQ_BATT_PERCENT_ENABLE && CONFIG_MTK_BATTERY_PERCENT_THROTTLING */
}

/* API: get working OPP index of STACK limited by LOW_BATTERY via given level */
int __gpufreq_get_low_batt_idx(int low_batt_level)
{
#if (GPUFREQ_LOW_BATT_ENABLE && IS_ENABLED(CONFIG_MTK_LOW_BATTERY_POWER_THROTTLING))
	if (low_batt_level == LOW_BATTERY_LEVEL_2)
		return __gpufreq_get_idx_by_fstack(GPUFREQ_LOW_BATT_FREQ);
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

/* API: apply/restore Vaging to working table of STACK */
int __gpufreq_set_aging_mode(unsigned int mode)
{
	/* prevent from repeatedly applying aging */
	if (g_aging_enable ^ mode) {
		__gpufreq_apply_aging(mode);
		g_aging_enable = mode;

		/* set power info to working table */
		__gpufreq_measure_power();

		/* update HW constraint parking volt */
		__gpufreq_update_hw_constraint_volt();

		return GPUFREQ_SUCCESS;
	} else {
		return GPUFREQ_EINVAL;
	}
}

/* API: enable/disable GPM 1.0 */
void __gpufreq_set_gpm_mode(unsigned int mode)
{
	g_gpm_enable = mode;
}

/* API: get core_mask table */
struct gpufreq_core_mask_info *__gpufreq_get_core_mask_table(void)
{
	return g_core_mask_table;
}

/* API: get max number of shader cores */
unsigned int __gpufreq_get_core_num(void)
{
	return SHADER_CORE_NUM;
}

//TODO
/* PDCv2: GPU IP automatically control GPU shader MTCMOS */
void __gpufreq_pdc_control(enum gpufreq_power_state power)
{
#if GPUFREQ_PDCv2_ENABLE
	u32 val = 0;

	if (power == POWER_ON) {
		/* MFG_ACTIVE_POWER_CON_00 0x13FBF400 [0] rg_sc0_active_pwrctl_en = 1'b1 */
		val = readl(g_mfg_top_base + 0x400);
		val |= (1UL << 0);
		writel(val, g_mfg_top_base + 0x400);

		/* MFG_ACTIVE_POWER_CON_06 0x13FBF418 [0] rg_sc1_active_pwrctl_en = 1'b1 */
		val = readl(g_mfg_top_base + 0x418);
		val |= (1UL << 0);
		writel(val, g_mfg_top_base + 0x418);

		/* MFG_ACTIVE_POWER_CON_12 0x13FBF430 [0] rg_sc2_active_pwrctl_en = 1'b1 */
		val = readl(g_mfg_top_base + 0x430);
		val |= (1UL << 0);
		writel(val, g_mfg_top_base + 0x430);

		/* MFG_ACTIVE_POWER_CON_18 0x13FBF448 [0] rg_sc3_active_pwrctl_en = 1'b1 */
		val = readl(g_mfg_top_base + 0x448);
		val |= (1UL << 0);
		writel(val, g_mfg_top_base + 0x448);

		/* MFG_ACTIVE_POWER_CON_24 0x13FBF460 [0] rg_sc4_active_pwrctl_en = 1'b1 */
		val = readl(g_mfg_top_base + 0x460);
		val |= (1UL << 0);
		writel(val, g_mfg_top_base + 0x460);

		/* MFG_ACTIVE_POWER_CON_30 0x13FBF478 [0] rg_sc5_active_pwrctl_en = 1'b1 */
		val = readl(g_mfg_top_base + 0x478);
		val |= (1UL << 0);
		writel(val, g_mfg_top_base + 0x478);

		/* MFG_ACTIVE_POWER_CON_36 0x13FBF490 [0] rg_sc6_active_pwrctl_en = 1'b1 */
		val = readl(g_mfg_top_base + 0x490);
		val |= (1UL << 0);
		writel(val, g_mfg_top_base + 0x490);

		/* MFG_ACTIVE_POWER_CON_42 0x13FBF4A8 [0] rg_sc7_active_pwrctl_en = 1'b1 */
		val = readl(g_mfg_top_base + 0x4A8);
		val |= (1UL << 0);
		writel(val, g_mfg_top_base + 0x4A8);

		/* MFG_ACTIVE_POWER_CON_48 0x13FBF4C0 [0] rg_sc8_active_pwrctl_en = 1'b1 */
		val = readl(g_mfg_top_base + 0x4C0);
		val |= (1UL << 0);
		writel(val, g_mfg_top_base + 0x4C0);

		/* MFG_ACTIVE_POWER_CON_54 0x13FBF4D8 [0] rg_sc9_active_pwrctl_en = 1'b1 */
		val = readl(g_mfg_top_base + 0x4D8);
		val |= (1UL << 0);
		writel(val, g_mfg_top_base + 0x4D8);

		/* MFG_ACTIVE_POWER_CON_CG_06 0x13FBF100 [0] rg_cg_active_pwrctl_en = 1'b1 */
		val = readl(g_mfg_top_base + 0x100);
		val |= (1UL << 0);
		writel(val, g_mfg_top_base + 0x100);

		/* MFG_ACTIVE_POWER_CON_ST0_06 0x13FBF120 [0] rg_st0_active_pwrctl_en = 1'b1 */
		val = readl(g_mfg_top_base + 0x120);
		val |= (1UL << 0);
		writel(val, g_mfg_top_base + 0x120);

		/* MFG_ACTIVE_POWER_CON_ST1_06 0x13FBF140 [0] rg_st1_active_pwrctl_en = 1'b1 */
		val = readl(g_mfg_top_base + 0x140);
		val |= (1UL << 0);
		writel(val, g_mfg_top_base + 0x140);

		/* MFG_ACTIVE_POWER_CON_ST2_06 0x13FBF118 [0] rg_st2_active_pwrctl_en = 1'b1 */
		val = readl(g_mfg_top_base + 0x118);
		val |= (1UL << 0);
		writel(val, g_mfg_top_base + 0x118);

		/* MFG_ACTIVE_POWER_CON_ST4_06 0x13FBF0C0 [0] rg_st4_active_pwrctl_en = 1'b1 */
		val = readl(g_mfg_top_base + 0xC0);
		val |= (1UL << 0);
		writel(val, g_mfg_top_base + 0xC0);

		/* MFG_ACTIVE_POWER_CON_ST5_06 0x13FBF098 [0] rg_st5_active_pwrctl_en = 1'b1 */
		val = readl(g_mfg_top_base + 0x98);
		val |= (1UL << 0);
		writel(val, g_mfg_top_base + 0x98);

		/* MFG_ACTIVE_POWER_CON_ST6_06 0x13FBF1C0 [0] rg_st6_active_pwrctl_en = 1'b1 */
		val = readl(g_mfg_top_base + 0x1C0);
		val |= (1UL << 0);
		writel(val, g_mfg_top_base + 0x1C0);

		/* MFG_ACTIVE_POWER_CON_01 0x13FBF404 [31] rg_sc0_active_pwrctl_rsv = 1'b1 */
		val = readl(g_mfg_top_base + 0x404);
		val |= (1UL << 31);
		writel(val, g_mfg_top_base + 0x404);

		/* MFG_ACTIVE_POWER_CON_07 0x13FBF41C [31] rg_sc1_active_pwrctl_rsv = 1'b1 */
		val = readl(g_mfg_top_base + 0x41C);
		val |= (1UL << 31);
		writel(val, g_mfg_top_base + 0x41C);

		/* MFG_ACTIVE_POWER_CON_13 0x13FBF434 [31] rg_sc2_active_pwrctl_rsv = 1'b1 */
		val = readl(g_mfg_top_base + 0x434);
		val |= (1UL << 31);
		writel(val, g_mfg_top_base + 0x434);

		/* MFG_ACTIVE_POWER_CON_19 0x13FBF44C [31] rg_sc3_active_pwrctl_rsv = 1'b1 */
		val = readl(g_mfg_top_base + 0x44C);
		val |= (1UL << 31);
		writel(val, g_mfg_top_base + 0x44C);

		/* MFG_ACTIVE_POWER_CON_25 0x13FBF464 [31] rg_sc4_active_pwrctl_rsv = 1'b1 */
		val = readl(g_mfg_top_base + 0x464);
		val |= (1UL << 31);
		writel(val, g_mfg_top_base + 0x464);

		/* MFG_ACTIVE_POWER_CON_31 0x13FBF47C [31] rg_sc5_active_pwrctl_rsv = 1'b1 */
		val = readl(g_mfg_top_base + 0x47C);
		val |= (1UL << 31);
		writel(val, g_mfg_top_base + 0x47C);

		/* MFG_ACTIVE_POWER_CON_37 0x13FBF494 [31] rg_sc6_active_pwrctl_rsv = 1'b1 */
		val = readl(g_mfg_top_base + 0x494);
		val |= (1UL << 31);
		writel(val, g_mfg_top_base + 0x494);

		/* MFG_ACTIVE_POWER_CON_43 0x13FBF4AC [31] rg_sc7_active_pwrctl_rsv = 1'b1 */
		val = readl(g_mfg_top_base + 0x4AC);
		val |= (1UL << 31);
		writel(val, g_mfg_top_base + 0x4AC);

		/* MFG_ACTIVE_POWER_CON_49 0x13FBF4C4 [31] rg_sc8_active_pwrctl_rsv = 1'b1 */
		val = readl(g_mfg_top_base + 0x4C4);
		val |= (1UL << 31);
		writel(val, g_mfg_top_base + 0x4C4);

		/* MFG_ACTIVE_POWER_CON_55 0x13FBF4DC [31] rg_sc9_active_pwrctl_rsv = 1'b1 */
		val = readl(g_mfg_top_base + 0x4DC);
		val |= (1UL << 31);
		writel(val, g_mfg_top_base + 0x4DC);
	} else {
		/* MFG_ACTIVE_POWER_CON_00 0x13FBF400 [0] rg_sc0_active_pwrctl_en = 1'b0 */
		val = readl(g_mfg_top_base + 0x400);
		val &= ~(1UL << 0);
		writel(val, g_mfg_top_base + 0x400);

		/* MFG_ACTIVE_POWER_CON_06 0x13FBF418 [0] rg_sc1_active_pwrctl_en = 1'b0 */
		val = readl(g_mfg_top_base + 0x418);
		val &= ~(1UL << 0);
		writel(val, g_mfg_top_base + 0x418);

		/* MFG_ACTIVE_POWER_CON_12 0x13FBF430 [0] rg_sc2_active_pwrctl_en = 1'b0 */
		val = readl(g_mfg_top_base + 0x430);
		val &= ~(1UL << 0);
		writel(val, g_mfg_top_base + 0x430);

		/* MFG_ACTIVE_POWER_CON_18 0x13FBF448 [0] rg_sc3_active_pwrctl_en = 1'b0 */
		val = readl(g_mfg_top_base + 0x448);
		val &= ~(1UL << 0);
		writel(val, g_mfg_top_base + 0x448);

		/* MFG_ACTIVE_POWER_CON_24 0x13FBF460 [0] rg_sc4_active_pwrctl_en = 1'b0 */
		val = readl(g_mfg_top_base + 0x460);
		val &= ~(1UL << 0);
		writel(val, g_mfg_top_base + 0x460);

		/* MFG_ACTIVE_POWER_CON_30 0x13FBF478 [0] rg_sc5_active_pwrctl_en = 1'b0 */
		val = readl(g_mfg_top_base + 0x478);
		val &= ~(1UL << 0);
		writel(val, g_mfg_top_base + 0x478);

		/* MFG_ACTIVE_POWER_CON_36 0x13FBF490 [0] rg_sc6_active_pwrctl_en = 1'b0 */
		val = readl(g_mfg_top_base + 0x490);
		val &= ~(1UL << 0);
		writel(val, g_mfg_top_base + 0x490);

		/* MFG_ACTIVE_POWER_CON_42 0x13FBF4A8 [0] rg_sc7_active_pwrctl_en = 1'b0 */
		val = readl(g_mfg_top_base + 0x4A8);
		val &= ~(1UL << 0);
		writel(val, g_mfg_top_base + 0x4A8);

		/* MFG_ACTIVE_POWER_CON_48 0x13FBF4C0 [0] rg_sc8_active_pwrctl_en = 1'b0 */
		val = readl(g_mfg_top_base + 0x4C0);
		val &= ~(1UL << 0);
		writel(val, g_mfg_top_base + 0x4C0);

		/* MFG_ACTIVE_POWER_CON_54 0x13FBF4D8 [0] rg_sc9_active_pwrctl_en = 1'b0 */
		val = readl(g_mfg_top_base + 0x4D8);
		val &= ~(1UL << 0);
		writel(val, g_mfg_top_base + 0x4D8);

		/* MFG_ACTIVE_POWER_CON_CG_06 0x13FBF100 [0] rg_cg_active_pwrctl_en = 1'b0 */
		val = readl(g_mfg_top_base + 0x100);
		val &= ~(1UL << 0);
		writel(val, g_mfg_top_base + 0x100);

		/* MFG_ACTIVE_POWER_CON_ST0_06 0x13FBF120 [0] rg_st0_active_pwrctl_en = 1'b0 */
		val = readl(g_mfg_top_base + 0x120);
		val &= ~(1UL << 0);
		writel(val, g_mfg_top_base + 0x120);

		/* MFG_ACTIVE_POWER_CON_ST1_06 0x13FBF140 [0] rg_st1_active_pwrctl_en = 1'b0 */
		val = readl(g_mfg_top_base + 0x140);
		val &= ~(1UL << 0);
		writel(val, g_mfg_top_base + 0x140);

		/* MFG_ACTIVE_POWER_CON_ST2_06 0x13FBF118 [0] rg_st2_active_pwrctl_en = 1'b0 */
		val = readl(g_mfg_top_base + 0x118);
		val &= ~(1UL << 0);
		writel(val, g_mfg_top_base + 0x118);

		/* MFG_ACTIVE_POWER_CON_ST4_06 0x13FBF0C0 [0] rg_st4_active_pwrctl_en = 1'b0 */
		val = readl(g_mfg_top_base + 0xC0);
		val &= ~(1UL << 0);
		writel(val, g_mfg_top_base + 0xC0);

		/* MFG_ACTIVE_POWER_CON_ST5_06 0x13FBF098 [0] rg_st5_active_pwrctl_en = 1'b0 */
		val = readl(g_mfg_top_base + 0x98);
		val &= ~(1UL << 0);
		writel(val, g_mfg_top_base + 0x98);

		/* MFG_ACTIVE_POWER_CON_ST6_06 0x13FBF1C0 [0] rg_st6_active_pwrctl_en = 1'b0 */
		val = readl(g_mfg_top_base + 0x1C0);
		val &= ~(1UL << 0);
		writel(val, g_mfg_top_base + 0x1C0);

		/* MFG_ACTIVE_POWER_CON_01 0x13FBF404 [31] rg_sc0_active_pwrctl_rsv = 1'b0 */
		val = readl(g_mfg_top_base + 0x404);
		val &= ~(1UL << 31);
		writel(val, g_mfg_top_base + 0x404);

		/* MFG_ACTIVE_POWER_CON_07 0x13FBF41C [31] rg_sc1_active_pwrctl_rsv = 1'b0 */
		val = readl(g_mfg_top_base + 0x41C);
		val &= ~(1UL << 31);
		writel(val, g_mfg_top_base + 0x41C);

		/* MFG_ACTIVE_POWER_CON_13 0x13FBF434 [31] rg_sc2_active_pwrctl_rsv = 1'b0 */
		val = readl(g_mfg_top_base + 0x434);
		val &= ~(1UL << 31);
		writel(val, g_mfg_top_base + 0x434);

		/* MFG_ACTIVE_POWER_CON_19 0x13FBF44C [31] rg_sc3_active_pwrctl_rsv = 1'b0 */
		val = readl(g_mfg_top_base + 0x44C);
		val &= ~(1UL << 31);
		writel(val, g_mfg_top_base + 0x44C);

		/* MFG_ACTIVE_POWER_CON_25 0x13FBF464 [31] rg_sc4_active_pwrctl_rsv = 1'b0 */
		val = readl(g_mfg_top_base + 0x464);
		val &= ~(1UL << 31);
		writel(val, g_mfg_top_base + 0x464);

		/* MFG_ACTIVE_POWER_CON_31 0x13FBF47C [31] rg_sc5_active_pwrctl_rsv = 1'b0 */
		val = readl(g_mfg_top_base + 0x47C);
		val &= ~(1UL << 31);
		writel(val, g_mfg_top_base + 0x47C);

		/* MFG_ACTIVE_POWER_CON_37 0x13FBF494 [31] rg_sc6_active_pwrctl_rsv = 1'b0 */
		val = readl(g_mfg_top_base + 0x494);
		val &= ~(1UL << 31);
		writel(val, g_mfg_top_base + 0x494);

		/* MFG_ACTIVE_POWER_CON_43 0x13FBF4AC [31] rg_sc7_active_pwrctl_rsv = 1'b0 */
		val = readl(g_mfg_top_base + 0x4AC);
		val &= ~(1UL << 31);
		writel(val, g_mfg_top_base + 0x4AC);

		/* MFG_ACTIVE_POWER_CON_49 0x13FBF4C4 [31] rg_sc8_active_pwrctl_rsv = 1'b0 */
		val = readl(g_mfg_top_base + 0x4C4);
		val &= ~(1UL << 31);
		writel(val, g_mfg_top_base + 0x4C4);

		/* MFG_ACTIVE_POWER_CON_55 0x13FBF4DC [31] rg_sc9_active_pwrctl_rsv = 1'b0 */
		val = readl(g_mfg_top_base + 0x4DC);
		val &= ~(1UL << 31);
		writel(val, g_mfg_top_base + 0x4DC);
	}
#else
	GPUFREQ_UNREFERENCED(power);
#endif /* GPUFREQ_PDCv2_ENABLE */
}

/* API: fake PWR_CON value to temporarily disable PDCv2 */
void __gpufreq_fake_spm_mtcmos_control(enum gpufreq_power_state power)
{
#if GPUFREQ_PDCv2_ENABLE
	if (power == POWER_ON) {
		/* fake power on value of SPM MFG2-12 */
		writel(0xC000000D, g_sleep + 0xEC0); /* MFG2  */
		writel(0xC000000D, g_sleep + 0xEC4); /* MFG3  */
		writel(0xC000000D, g_sleep + 0xEC8); /* MFG4  */
		writel(0xC000000D, g_sleep + 0xECC); /* MFG5  */
		writel(0xC000000D, g_sleep + 0xED0); /* MFG6  */
		writel(0xC000000D, g_sleep + 0xED4); /* MFG7  */
		writel(0xC000000D, g_sleep + 0xED8); /* MFG8  */
		writel(0xC000000D, g_sleep + 0xEDC); /* MFG9  */
		writel(0xC000000D, g_sleep + 0xEE0); /* MFG10 */
		writel(0xC000000D, g_sleep + 0xEE4); /* MFG11 */
		writel(0xC000000D, g_sleep + 0xEE8); /* MFG12 */
	} else {
		/* fake power off value of SPM MFG2-12 */
		writel(0x1112, g_sleep + 0xEE8); /* MFG12 */
		writel(0x1112, g_sleep + 0xEE4); /* MFG11 */
		writel(0x1112, g_sleep + 0xEE0); /* MFG10 */
		writel(0x1112, g_sleep + 0xEDC); /* MFG9  */
		writel(0x1112, g_sleep + 0xED8); /* MFG8  */
		writel(0x1112, g_sleep + 0xED4); /* MFG7  */
		writel(0x1112, g_sleep + 0xED0); /* MFG6  */
		writel(0x1112, g_sleep + 0xECC); /* MFG5  */
		writel(0x1112, g_sleep + 0xEC8); /* MFG4  */
		writel(0x1112, g_sleep + 0xEC4); /* MFG3  */
		writel(0x1112, g_sleep + 0xEC0); /* MFG2  */
	}
#else
	GPUFREQ_UNREFERENCED(power);
#endif /* GPUFREQ_PDCv2_ENABLE */
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

/* API: DVFS order control of GPU */
static int __gpufreq_generic_scale_gpu(
	unsigned int freq_old, unsigned int freq_new,
	unsigned int volt_old, unsigned int volt_new,
	unsigned int vsram_old, unsigned int vsram_new)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("freq_old=%d, freq_new=%d, volt_old=%d, volt_new=%d, vsram_old=%d, vsram_new=%d",
		freq_old, freq_new, volt_old, volt_new, vsram_old, vsram_new);

	/* scaling up: volt -> freq */
	if (freq_new > freq_old) {
		/* volt scaling */
		ret = __gpufreq_volt_scale_gpu(volt_old, volt_new, vsram_old, vsram_new);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to scale Vgpu: (%d->%d), Vsram_gpu: (%d->%d)",
				volt_old, volt_new, vsram_old, vsram_new);
			goto done;
		}
		/* freq scaling */
		ret = __gpufreq_freq_scale_gpu(freq_old, freq_new);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to scale Fgpu: (%d->%d)",
				freq_old, freq_new);
			goto done;
		}
	/* scaling down: freq -> volt */
	} else if (freq_new < freq_old) {
		/* freq scaling */
		ret = __gpufreq_freq_scale_gpu(freq_old, freq_new);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to scale Fgpu: (%d->%d)",
				freq_old, freq_new);
			goto done;
		}
		/* volt scaling */
		ret = __gpufreq_volt_scale_gpu(volt_old, volt_new, vsram_old, vsram_new);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to scale Vgpu: (%d->%d), Vsram_gpu: (%d->%d)",
				volt_old, volt_new, vsram_old, vsram_new);
			goto done;
		}
	/* keep: volt only */
	} else {
		/* volt scaling */
		ret = __gpufreq_volt_scale_gpu(volt_old, volt_new, vsram_old, vsram_new);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to scale Vgpu: (%d->%d), Vsram_gpu: (%d->%d)",
				volt_old, volt_new, vsram_old, vsram_new);
			goto done;
		}
	}

done:
	GPUFREQ_TRACE_END();

	return ret;
}

/* API: DVFS order control of STACK */
static int __gpufreq_generic_scale_stack(
	unsigned int freq_old, unsigned int freq_new,
	unsigned int volt_old, unsigned int volt_new,
	unsigned int vsram_old, unsigned int vsram_new)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("freq_old=%d, freq_new=%d, volt_old=%d, volt_new=%d, vsram_old=%d, vsram_new=%d",
		freq_old, freq_new, volt_old, volt_new, vsram_old, vsram_new);

	/* scaling up: volt -> freq */
	if (freq_new > freq_old) {
		/* volt scaling */
		ret = __gpufreq_volt_scale_stack(volt_old, volt_new, vsram_old, vsram_new);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to scale Vstack: (%d->%d), Vsram_stack: (%d->%d)",
				volt_old, volt_new, vsram_old, vsram_new);
			goto done;
		}
		/* freq scaling */
		ret = __gpufreq_freq_scale_stack(freq_old, freq_new);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to scale Fstack: (%d->%d)",
				freq_old, freq_new);
			goto done;
		}
	/* scaling down: freq -> volt */
	} else if (freq_new < freq_old) {
		/* freq scaling */
		ret = __gpufreq_freq_scale_stack(freq_old, freq_new);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to scale Fstack: (%d->%d)",
				freq_old, freq_new);
			goto done;
		}
		/* volt scaling */
		ret = __gpufreq_volt_scale_stack(volt_old, volt_new, vsram_old, vsram_new);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to scale Vstack: (%d->%d), Vsram_stack: (%d->%d)",
				volt_old, volt_new, vsram_old, vsram_new);
			goto done;
		}
	/* keep: volt only */
	} else {
		/* volt scaling */
		ret = __gpufreq_volt_scale_stack(volt_old, volt_new, vsram_old, vsram_new);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to scale Vstack: (%d->%d), Vsram_stack: (%d->%d)",
				volt_old, volt_new, vsram_old, vsram_new);
			goto done;
		}
	}

done:
	GPUFREQ_TRACE_END();

	return ret;
}

/*
 * API: commit DVFS to GPU by given freq and volt
 * this is debug function and use it with caution
 */
static int __gpufreq_custom_commit_gpu(unsigned int target_freq,
	unsigned int target_volt, enum gpufreq_dvfs_state key)
{
	GPUFREQ_UNREFERENCED(target_freq);
	GPUFREQ_UNREFERENCED(target_volt);
	GPUFREQ_UNREFERENCED(key);

	return GPUFREQ_EINVAL;
}

/*
 * API: commit DVFS to STACK by given freq and volt
 * this is debug function and use it with caution
 */
static int __gpufreq_custom_commit_stack(unsigned int target_freq,
	unsigned int target_volt, enum gpufreq_dvfs_state key)
{
	/* STACK */
	int cur_oppidx_stack = 0, target_oppidx_stack = 0;
	unsigned int cur_fstack = 0, cur_vstack = 0, cur_vsram_stack = 0;
	unsigned int target_fstack = 0, target_vstack = 0, target_vsram_stack = 0;
	/* GPU */
	struct gpufreq_opp_info *working_gpu = g_gpu.working_table;
	int cur_oppidx_gpu = 0, target_oppidx_gpu = 0;
	unsigned int cur_fgpu = 0, cur_vgpu = 0, cur_vsram_gpu = 0;
	unsigned int target_fgpu = 0, target_vgpu = 0, target_vsram_gpu = 0;
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("target_freq=%d, target_volt=%d, key=%d",
		target_freq, target_volt, key);

	mutex_lock(&gpufreq_lock);

	/* check dvfs state */
	if (g_dvfs_state & ~key) {
		GPUFREQ_LOGD("unavailable DVFS state (0x%x)", g_dvfs_state);
		ret = GPUFREQ_SUCCESS;
		goto done_unlock;
	}

	/* prepare STACK setting */
	cur_oppidx_stack = g_stack.cur_oppidx;
	cur_fstack = g_stack.cur_freq;
	cur_vstack = g_stack.cur_volt;
	cur_vsram_stack = g_stack.cur_vsram;
	target_oppidx_stack = __gpufreq_get_idx_by_fstack(target_freq);
	target_fstack = target_freq;
	target_vstack = target_volt;
	target_vsram_stack = __gpufreq_get_vsram_by_vstack(target_volt);

	/* prepare GPU setting */
	cur_oppidx_gpu = g_gpu.cur_oppidx;
	cur_fgpu = g_gpu.cur_freq;
	cur_vgpu = g_gpu.cur_volt;
	cur_vsram_gpu = g_gpu.cur_vsram;
	__gpufreq_get_hw_constraint_fgpu(target_fstack, &target_oppidx_gpu, &target_fgpu);
	target_vgpu = working_gpu[target_oppidx_gpu].volt;
	target_vsram_gpu = working_gpu[target_oppidx_gpu].vsram;

	GPUFREQ_LOGD("begin to commit STACK F(%d->%d), V(%d->%d), GPU F(%d->%d), V(%d->%d)",
		cur_fstack, target_fstack, cur_vstack, target_vstack,
		cur_fgpu, target_fgpu, cur_vgpu, target_vgpu);

	/* STACK volt scaling up: GPU -> STACK */
	if (target_vstack > cur_vstack) {
		ret = __gpufreq_generic_scale_gpu(cur_fgpu, target_fgpu,
			cur_vgpu, target_vgpu, cur_vsram_gpu, target_vsram_gpu);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to scale GPU: Idx(%d->%d), Freq(%d->%d), Volt(%d->%d), Vsram(%d->%d)",
				cur_oppidx_gpu, target_oppidx_gpu, cur_fgpu, target_fgpu,
				cur_vgpu, target_vgpu, cur_vsram_gpu, target_vsram_gpu);
			goto done_unlock;
		}
		ret = __gpufreq_generic_scale_stack(cur_fstack, target_fstack,
			cur_vstack, target_vstack, cur_vsram_stack, target_vsram_stack);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to scale STACK: Idx(%d->%d), Freq(%d->%d), Volt(%d->%d), Vsram(%d->%d)",
				cur_oppidx_stack, target_oppidx_stack, cur_fstack, target_fstack,
				cur_vstack, target_vstack, cur_vsram_stack, target_vsram_stack);
			goto done_unlock;
		}
	/* STACK volt scaling down: STACK -> GPU */
	} else if (target_vstack < cur_vstack) {
		ret = __gpufreq_generic_scale_stack(cur_fstack, target_fstack,
			cur_vstack, target_vstack, cur_vsram_stack, target_vsram_stack);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to scale STACK: Idx(%d->%d), Freq(%d->%d), Volt(%d->%d), Vsram(%d->%d)",
				cur_oppidx_stack, target_oppidx_stack, cur_fstack, target_fstack,
				cur_vstack, target_vstack, cur_vsram_stack, target_vsram_stack);
			goto done_unlock;
		}
		ret = __gpufreq_generic_scale_gpu(cur_fgpu, target_fgpu,
			cur_vgpu, target_vgpu, cur_vsram_gpu, target_vsram_gpu);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to scale GPU: Idx(%d->%d), Freq(%d->%d), Volt(%d->%d), Vsram(%d->%d)",
				cur_oppidx_gpu, target_oppidx_gpu, cur_fgpu, target_fgpu,
				cur_vgpu, target_vgpu, cur_vsram_gpu, target_vsram_gpu);
			goto done_unlock;
		}
	/* STACK volt keep: STACK only */
	} else {
		ret = __gpufreq_generic_scale_stack(cur_fstack, target_fstack,
			cur_vstack, target_vstack, cur_vsram_stack, target_vsram_stack);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to scale STACK: Idx(%d->%d), Freq(%d->%d), Volt(%d->%d), Vsram(%d->%d)",
				cur_oppidx_stack, target_oppidx_stack, cur_fstack, target_fstack,
				cur_vstack, target_vstack, cur_vsram_stack, target_vsram_stack);
			goto done_unlock;
		}
	}

	g_gpu.cur_oppidx = target_oppidx_gpu;
	g_stack.cur_oppidx = target_oppidx_stack;

	__gpufreq_footprint_oppidx(target_oppidx_stack);

done_unlock:
	mutex_unlock(&gpufreq_lock);

	GPUFREQ_TRACE_END();

	return ret;
}

static int __gpufreq_switch_clksrc_gpu(enum gpufreq_clk_src clksrc)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("clksrc=%d", clksrc);

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

done:
	GPUFREQ_TRACE_END();

	return ret;
}

static int __gpufreq_switch_clksrc_stack(enum gpufreq_clk_src clksrc)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("clksrc=%d", clksrc);

	if (clksrc == CLOCK_MAIN) {
		ret = clk_set_parent(g_clk->clk_sc_mux, g_clk->clk_sc_main_parent);
		if (unlikely(ret)) {
			__gpufreq_abort(GPUFREQ_CCF_EXCEPTION,
				"fail to switch STACK to CLOCK_MAIN (%d)", ret);
			goto done;
		}
	} else if (clksrc == CLOCK_SUB) {
		ret = clk_set_parent(g_clk->clk_sc_mux, g_clk->clk_sc_sub_parent);
		if (unlikely(ret)) {
			__gpufreq_abort(GPUFREQ_CCF_EXCEPTION,
				"fail to switch STACK to CLOCK_SUB (%d)", ret);
			goto done;
		}
	} else {
		GPUFREQ_LOGE("invalid clock source: %d (EINVAL)", clksrc);
		goto done;
	}

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

	if ((freq >= POSDIV_8_MIN_FREQ) && (freq <= POSDIV_4_MAX_FREQ)) {
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

	mfgpll = readl(MFG_PLL_CON1);

	posdiv = (mfgpll & (0x7 << POSDIV_SHIFT)) >> POSDIV_SHIFT;

	return posdiv;
}

static enum gpufreq_posdiv __gpufreq_get_real_posdiv_stack(void)
{
	unsigned int mfgpll = 0;
	enum gpufreq_posdiv posdiv = POSDIV_POWER_1;

	mfgpll = readl(MFGSC_PLL_CON1);

	posdiv = (mfgpll & (0x7 << POSDIV_SHIFT)) >> POSDIV_SHIFT;

	return posdiv;
}

static enum gpufreq_posdiv __gpufreq_get_posdiv_by_fgpu(unsigned int freq)
{
	struct gpufreq_opp_info *signed_table = g_gpu.signed_table;
	int i = 0;

	for (i = 0; i < g_gpu.signed_opp_num; i++) {
		if (signed_table[i].freq <= freq)
			return signed_table[i].posdiv;
	}

	GPUFREQ_LOGE("fail to find post-divider of Freq: %d", freq);

	if (freq > POSDIV_2_MAX_FREQ)
		return POSDIV_POWER_1;
	else if (freq > POSDIV_4_MAX_FREQ)
		return POSDIV_POWER_2;
	else if (freq > POSDIV_8_MAX_FREQ)
		return POSDIV_POWER_4;
	else if (freq > POSDIV_16_MAX_FREQ)
		return POSDIV_POWER_8;
	else
		return POSDIV_POWER_16;
}

static enum gpufreq_posdiv __gpufreq_get_posdiv_by_fstack(unsigned int freq)
{
	struct gpufreq_opp_info *signed_table = g_stack.signed_table;
	int i = 0;

	for (i = 0; i < g_stack.signed_opp_num; i++) {
		if (signed_table[i].freq <= freq)
			return signed_table[i].posdiv;
	}

	GPUFREQ_LOGE("fail to find post-divider of Freq: %d", freq);

	if (freq > POSDIV_2_MAX_FREQ)
		return POSDIV_POWER_1;
	else if (freq > POSDIV_4_MAX_FREQ)
		return POSDIV_POWER_2;
	else if (freq > POSDIV_8_MAX_FREQ)
		return POSDIV_POWER_4;
	else if (freq > POSDIV_16_MAX_FREQ)
		return POSDIV_POWER_8;
	else
		return POSDIV_POWER_16;
}

/* API: scale Freq of GPU via CON1 Reg or FHCTL */
static int __gpufreq_freq_scale_gpu(unsigned int freq_old, unsigned int freq_new)
{
	enum gpufreq_posdiv cur_posdiv = POSDIV_POWER_1;
	enum gpufreq_posdiv target_posdiv = POSDIV_POWER_1;
	unsigned int pcw = 0;
	unsigned int pll = 0;
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("freq_old=%d, freq_new=%d", freq_old, freq_new);

	GPUFREQ_LOGD("begin to scale Fgpu: (%d->%d)", freq_old, freq_new);

	/*
	 * MFG_PLL_CON1[31:31]: MFGPLL_SDM_PCW_CHG
	 * MFG_PLL_CON1[26:24]: MFGPLL_POSDIV
	 * MFG_PLL_CON1[21:0] : MFGPLL_SDM_PCW (DDS)
	 */
	cur_posdiv = __gpufreq_get_real_posdiv_gpu();
	target_posdiv = __gpufreq_get_posdiv_by_fgpu(freq_new);
	/* compute PCW based on target Freq */
	pcw = __gpufreq_calculate_pcw(freq_new, target_posdiv);
	if (!pcw) {
		GPUFREQ_LOGE("invalid PCW: 0x%x", pcw);
		goto done;
	}
//TODO
#if (GPUFREQ_FHCTL_ENABLE && IS_ENABLED(CONFIG_COMMON_CLK_MTK_FREQ_HOPPING))
	if (unlikely(!mtk_fh_set_rate)) {
		__gpufreq_abort(GPUFREQ_FHCTL_EXCEPTION, "null hopping fp");
		ret = GPUFREQ_ENOENT;
		goto done;
	}
	/* POSDIV remain the same */
	if (target_posdiv == cur_posdiv) {
		/* change PCW by hopping only */
		ret = mtk_fh_set_rate(MFG_PLL_NAME, pcw, target_posdiv);
		if (unlikely(!ret)) {
			__gpufreq_abort(GPUFREQ_FHCTL_EXCEPTION,
				"fail to hopping PCW: 0x%x (%d)", pcw, ret);
			ret = GPUFREQ_EINVAL;
			goto done;
		}
	/* freq scale up */
	} else if (freq_new > freq_old) {
		/* 1. change PCW by hopping */
		ret = mtk_fh_set_rate(MFG_PLL_NAME, pcw, target_posdiv);
		if (unlikely(!ret)) {
			__gpufreq_abort(GPUFREQ_FHCTL_EXCEPTION,
				"fail to hopping PCW: 0x%x (%d)", pcw, ret);
			ret = GPUFREQ_EINVAL;
			goto done;
		}
		/* 2. compute CON1 with target POSDIV */
		pll = (readl(MFG_PLL_CON1) & 0xF8FFFFFF) | (target_posdiv << POSDIV_SHIFT);
		/* 3. change POSDIV by writing CON1 */
		writel(pll, MFG_PLL_CON1);
		/* 4. wait until PLL stable */
		udelay(20);
	/* freq scale down */
	} else {
		/* 1. compute CON1 with target POSDIV */
		pll = (readl(MFG_PLL_CON1) & 0xF8FFFFFF) | (target_posdiv << POSDIV_SHIFT);
		/* 2. change POSDIV by writing CON1 */
		writel(pll, MFG_PLL_CON1);
		/* 3. wait until PLL stable */
		udelay(20);
		/* 4. change PCW by hopping */
		ret = mtk_fh_set_rate(MFG_PLL_NAME, pcw, target_posdiv);
		if (unlikely(!ret)) {
			__gpufreq_abort(GPUFREQ_FHCTL_EXCEPTION,
				"fail to hopping PCW: 0x%x (%d)", pcw, ret);
			ret = GPUFREQ_EINVAL;
			goto done;
		}
	}
#else
	/* 1. switch to parking clk source */
	ret = __gpufreq_switch_clksrc_gpu(CLOCK_SUB);
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to switch sub clock source (%d)", ret);
		goto done;
	}
	/* 2. compute CON1 with PCW and POSDIV */
	pll = (0x80000000) | (target_posdiv << POSDIV_SHIFT) | pcw;
	/* 3. change PCW and POSDIV by writing CON1 */
	writel(pll, MFG_PLL_CON1);
	/* 4. wait until PLL stable */
	udelay(20);
	/* 5. switch to main clk source */
	ret = __gpufreq_switch_clksrc_gpu(CLOCK_MAIN);
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to switch main clock source (%d)", ret);
		goto done;
	}
#endif /* GPUFREQ_FHCTL_ENABLE && CONFIG_COMMON_CLK_MTK_FREQ_HOPPING */

	g_gpu.cur_freq = __gpufreq_get_real_fgpu();
	if (unlikely(g_gpu.cur_freq != freq_new))
		__gpufreq_abort(GPUFREQ_GPU_EXCEPTION,
			"inconsistent scaled Fgpu, cur_freq: %d, target_freq: %d",
			g_gpu.cur_freq, freq_new);

	GPUFREQ_LOGD("Fgpu: %d, PCW: 0x%x, CON1: 0x%08x", g_gpu.cur_freq, pcw, pll);

	/* because return value is different across the APIs */
	ret = GPUFREQ_SUCCESS;

	/* notify gpu freq change to DDK  */
	mtk_notify_gpu_freq_change(0, freq_new);

done:
	GPUFREQ_TRACE_END();

	return ret;
}

/* API: scale Freq of STACK via CON1 Reg or FHCTL */
static int __gpufreq_freq_scale_stack(unsigned int freq_old, unsigned int freq_new)
{
	enum gpufreq_posdiv cur_posdiv = POSDIV_POWER_1;
	enum gpufreq_posdiv target_posdiv = POSDIV_POWER_1;
	unsigned int pcw = 0;
	unsigned int pll = 0;
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("freq_old=%d, freq_new=%d", freq_old, freq_new);

	GPUFREQ_LOGD("begin to scale Fstack: (%d->%d)", freq_old, freq_new);

	/*
	 * MFGSC_PLL_CON1[31:31]: MFGPLL_SDM_PCW_CHG
	 * MFGSC_PLL_CON1[26:24]: MFGPLL_POSDIV
	 * MFGSC_PLL_CON1[21:0] : MFGPLL_SDM_PCW (DDS)
	 */
	cur_posdiv = __gpufreq_get_real_posdiv_stack();
	target_posdiv = __gpufreq_get_posdiv_by_fstack(freq_new);
	/* compute PCW based on target Freq */
	pcw = __gpufreq_calculate_pcw(freq_new, target_posdiv);
	if (!pcw) {
		GPUFREQ_LOGE("invalid PCW: 0x%x", pcw);
		goto done;
	}
//TODO
#if (GPUFREQ_FHCTL_ENABLE && IS_ENABLED(CONFIG_COMMON_CLK_MTK_FREQ_HOPPING))
	if (unlikely(!mtk_fh_set_rate)) {
		__gpufreq_abort(GPUFREQ_FHCTL_EXCEPTION, "null hopping fp");
		ret = GPUFREQ_ENOENT;
		goto done;
	}
	/* POSDIV remain the same */
	if (target_posdiv == cur_posdiv) {
		/* change PCW by hopping only */
		ret = mtk_fh_set_rate(MFGSC_PLL_NAME, pcw, target_posdiv);
		if (unlikely(!ret)) {
			__gpufreq_abort(GPUFREQ_FHCTL_EXCEPTION,
				"fail to hopping PCW: 0x%x (%d)", pcw, ret);
			ret = GPUFREQ_EINVAL;
			goto done;
		}
	/* freq scale up */
	} else if (freq_new > freq_old) {
		/* 1. change PCW by hopping */
		ret = mtk_fh_set_rate(MFGSC_PLL_NAME, pcw, target_posdiv);
		if (unlikely(!ret)) {
			__gpufreq_abort(GPUFREQ_FHCTL_EXCEPTION,
				"fail to hopping PCW: 0x%x (%d)", pcw, ret);
			ret = GPUFREQ_EINVAL;
			goto done;
		}
		/* 2. compute CON1 with target POSDIV */
		pll = (readl(MFGSC_PLL_CON1) & 0xF8FFFFFF) | (target_posdiv << POSDIV_SHIFT);
		/* 3. change POSDIV by writing CON1 */
		writel(pll, MFGSC_PLL_CON1);
		/* 4. wait until PLL stable */
		udelay(20);
	/* freq scale down */
	} else {
		/* 1. compute CON1 with target POSDIV */
		pll = (readl(MFGSC_PLL_CON1) & 0xF8FFFFFF) | (target_posdiv << POSDIV_SHIFT);
		/* 2. change POSDIV by writing CON1 */
		writel(pll, MFGSC_PLL_CON1);
		/* 3. wait until PLL stable */
		udelay(20);
		/* 4. change PCW by hopping */
		ret = mtk_fh_set_rate(MFGSC_PLL_NAME, pcw, target_posdiv);
		if (unlikely(!ret)) {
			__gpufreq_abort(GPUFREQ_FHCTL_EXCEPTION,
				"fail to hopping PCW: 0x%x (%d)", pcw, ret);
			ret = GPUFREQ_EINVAL;
			goto done;
		}
	}
#else
	/* 1. switch to parking clk source */
	ret = __gpufreq_switch_clksrc_stack(CLOCK_SUB);
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to switch sub clock source (%d)", ret);
		goto done;
	}
	/* 2. compute CON1 with PCW and POSDIV */
	pll = (0x80000000) | (target_posdiv << POSDIV_SHIFT) | pcw;
	/* 3. change PCW and POSDIV by writing CON1 */
	writel(pll, MFGSC_PLL_CON1);
	/* 4. wait until PLL stable */
	udelay(20);
	/* 5. switch to main clk source */
	ret = __gpufreq_switch_clksrc_stack(CLOCK_MAIN);
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to switch main clock source (%d)", ret);
		goto done;
	}
#endif /* GPUFREQ_FHCTL_ENABLE && CONFIG_COMMON_CLK_MTK_FREQ_HOPPING */

	g_stack.cur_freq = __gpufreq_get_real_fstack();
	if (unlikely(g_stack.cur_freq != freq_new))
		__gpufreq_abort(GPUFREQ_GPU_EXCEPTION,
			"inconsistent scaled Fstack, cur_freq: %d, target_freq: %d",
			g_stack.cur_freq, freq_new);

	GPUFREQ_LOGD("Fstack: %d, PCW: 0x%x, CON1: 0x%08x", g_stack.cur_freq, pcw, pll);

	/* because return value is different across the APIs */
	ret = GPUFREQ_SUCCESS;

	/* notify stack freq change to DDK */
	mtk_notify_gpu_freq_change(1, freq_new);

done:
	GPUFREQ_TRACE_END();

	return ret;
}

static unsigned int __gpufreq_settle_time_vstack(unsigned int direction, int deltaV)
{
	/* [MT6368][VGPUSTACK]
	 * DVFS Rising : (deltaV / 12.5(mV)) + 3.85us + 2us
	 * DVFS Falling: (deltaV / 6.25(mV)) + 3.85us + 2us
	 * deltaV = mV x 100
	 */
	unsigned int t_settle = 0;

	if (direction) {
		/* rising: 12.5mV/us*/
		t_settle = (deltaV / 1250) + 4 + 2;
	} else {
		/* falling: 6.25mV/us*/
		t_settle = (deltaV / 625) + 4 + 2;
	}

	return t_settle; /* us */
}

/* API: scale Volt of GPU via DVFSRC */
static int __gpufreq_volt_scale_gpu(
	unsigned int volt_old, unsigned int volt_new,
	unsigned int vsram_old, unsigned int vsram_new)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("volt_old=%d, volt_new=%d, vsram_old=%d, vsram_new=%d",
		volt_old, volt_new, vsram_old, vsram_new);

	GPUFREQ_LOGD("begin to scale Vgpu: (%d->%d), Vsram_gpu: (%d->%d)",
		volt_old, volt_new, vsram_old, vsram_new);

#if GPUFREQ_VCORE_DVFS_ENABLE
	ret = regulator_set_voltage(g_pmic->reg_dvfsrc, volt_new * 10, INT_MAX);
	if (unlikely(ret)) {
		__gpufreq_abort(GPUFREQ_PMIC_EXCEPTION, "fail to set regulator VGPU (%d)", ret);
		goto done;
	}

	g_gpu.cur_volt = volt_new;
	g_gpu.cur_vsram = vsram_new;

	GPUFREQ_LOGD("Vgpu: %d, Vsram: %d", g_gpu.cur_volt, g_gpu.cur_vsram);

done:
#endif /* GPUFREQ_VCORE_DVFS_ENABLE */

	GPUFREQ_TRACE_END();

	return ret;
}

/* API: scale Volt of STACK via Regulator */
static int __gpufreq_volt_scale_stack(
	unsigned int volt_old, unsigned int volt_new,
	unsigned int vsram_old, unsigned int vsram_new)
{
	unsigned int t_settle_volt = 0;
	unsigned int volt_park = 0;
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("volt_old=%d, volt_new=%d, vsram_old=%d, vsram_new=%d",
		volt_old, volt_new, vsram_old, vsram_new);

	GPUFREQ_LOGD("begin to scale Vstack: (%d->%d), Vsram_stack: (%d->%d)",
		volt_old, volt_new, vsram_old, vsram_new);

	/* volt scaling up */
	if (volt_new > volt_old) {
		while (volt_new != volt_old) {
			/* find parking volt and control registers because of HW constraint */
			volt_park = __gpufreq_hw_constraint_parking(volt_old, volt_new);
			t_settle_volt =  __gpufreq_settle_time_vstack(true, (volt_park - volt_old));

			ret = regulator_set_voltage(g_pmic->reg_vstack,
				volt_park * 10, VSTACK_MAX_VOLT * 10 + 125);
			if (unlikely(ret)) {
				__gpufreq_abort(GPUFREQ_PMIC_EXCEPTION,
					"fail to set regulator VSTACK (%d)", ret);
				goto done;
			}
			udelay(t_settle_volt);

			volt_old = volt_park;
		}
	/* volt scaling down */
	} else if (volt_new < volt_old) {
		while (volt_new != volt_old) {
			/* find parking volt and control registers because of HW constraint */
			volt_park = __gpufreq_hw_constraint_parking(volt_old, volt_new);
			t_settle_volt = __gpufreq_settle_time_vstack(false, (volt_old - volt_park));

			ret = regulator_set_voltage(g_pmic->reg_vstack,
				volt_park * 10, VSTACK_MAX_VOLT * 10 + 125);
			if (unlikely(ret)) {
				__gpufreq_abort(GPUFREQ_PMIC_EXCEPTION,
					"fail to set regulator VSTACK (%d)", ret);
				goto done;
			}
			udelay(t_settle_volt);

			volt_old = volt_park;
		}
	/* keep volt */
	} else {
		ret = GPUFREQ_SUCCESS;
	}

	g_stack.cur_volt = __gpufreq_get_real_vstack();
	g_stack.cur_vsram = vsram_new;
	if (unlikely(g_stack.cur_volt != volt_new))
		__gpufreq_abort(GPUFREQ_GPU_EXCEPTION,
			"inconsistent scaled Vstack, cur_volt: %d, target_volt: %d",
			g_stack.cur_volt, volt_new);

	GPUFREQ_LOGD("Vstack: %d, Vsram: %d, udelay: %d",
		g_stack.cur_volt, g_stack.cur_vsram, t_settle_volt);

done:
	GPUFREQ_TRACE_END();

	return ret;
}

/*
 * API: dump power/clk status when bring-up
 */
static void __gpufreq_dump_bringup_status(struct platform_device *pdev)
{
	struct device *gpufreq_dev = &pdev->dev;
	struct resource *res = NULL;
	u32 mfg_sel_0 = 0, mfg_ref_sel = 0;
	u32 mfg_sel_1 = 0, mfgsc_ref_sel = 0;

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

	/* 0x13FA0C00 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfgsc_pll");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource MFGSC_PLL");
		goto done;
	}
	g_mfgsc_pll_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_mfgsc_pll_base)) {
		GPUFREQ_LOGE("fail to ioremap MFGSC_PLL: 0x%llx", res->start);
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

	/* 0x1C001000 */
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

	/* 0x10000000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "topckgen");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource TOPCKGEN");
		goto done;
	}
	g_topckgen_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_topckgen_base)) {
		GPUFREQ_LOGE("fail to ioremap TOPCKGEN: 0x%llx", res->start);
		goto done;
	}

	/* 0x13000000 */
	g_mali_base = __gpufreq_of_ioremap("mediatek,mali", 0);
	if (unlikely(!g_mali_base)) {
		GPUFREQ_LOGE("fail to ioremap MALI");
		goto done;
	}

	/* CLK_CFG_30 0x100001F0 [16] MFG_SEL_0 */
	mfg_sel_0 = readl(g_topckgen_base + 0x1F0) & MFG_SEL_0_MASK;
	/* CLK_CFG_30 0x100001F0 [17] MFG_SEL_1 */
	mfg_sel_1 = readl(g_topckgen_base + 0x1F0) & MFG_SEL_1_MASK;
	/* CLK_CFG_4 0x10000050 [25:24] MFG_REF_SEL */
	mfg_ref_sel = readl(g_topckgen_base + 0x50) & MFG_REF_SEL_MASK;
	/* CLK_CFG_5 0x10000060 [1:0] MFGSC_REF_SEL */
	mfgsc_ref_sel = readl(g_topckgen_base + 0x60) & MFGSC_REF_SEL_MASK;
	/*
	 * [SPM] pwr_status    : 0x10006F3C
	 * [SPM] pwr_status_2nd: 0x10006F40
	 * Power ON: 0011 1111 1111 1110 (0x3FFE)
	 * [13:1]: MFG0-12
	 */
	GPUFREQ_LOGI("[GPU]     MALI_ID:    0x%08x, MFG_TOP_CONFIG: 0x%08x",
		readl(g_mali_base), readl(g_mfg_top_base));
	GPUFREQ_LOGI("[MFG0-12] PWR_STATUS: 0x%08x, PWR_STATUS_2ND: 0x%08x",
		readl(g_sleep + PWR_STATUS_OFS) & MFG_0_12_PWR_MASK,
		readl(g_sleep + PWR_STATUS_2ND_OFS) & MFG_0_12_PWR_MASK);
	GPUFREQ_LOGI("[TOP]   CON1: %d, MFG_SEL_0: 0x%08x, MFG_REF_SEL:   0x%08x",
		__gpufreq_get_real_fgpu(), mfg_sel_0, mfg_ref_sel);
	GPUFREQ_LOGI("[STACK] CON1: %d, MFG_SEL_1: 0x%08x, MFGSC_REF_SEL: 0x%08x",
		__gpufreq_get_real_fstack(), mfg_sel_1, mfgsc_ref_sel);

done:
	return;
}

static unsigned int __gpufreq_get_fmeter_fgpu(void)
{
	u32 mux_src = 0;

	/* CLK_CFG_30 0x100001F0 [16] MFG_SEL_0 */
	mux_src = readl(g_topckgen_base + 0x1F0) & MFG_SEL_0_MASK;

	if (mux_src == MFG_SEL_0_MASK)
		return __gpufreq_get_fmeter_main_fgpu();
	else if (mux_src == 0x0)
		return __gpufreq_get_fmeter_sub_fgpu();
	else
		return 0;
}

static unsigned int __gpufreq_get_fmeter_fstack(void)
{
	u32 mux_src = 0;

	/* CLK_CFG_30 0x100001F0 [17] MFG_SEL_1 */
	mux_src = readl(g_topckgen_base + 0x1F0) & MFG_SEL_1_MASK;

	if (mux_src == MFG_SEL_1_MASK)
		return __gpufreq_get_fmeter_main_fstack();
	else if (mux_src == 0x0)
		return __gpufreq_get_fmeter_sub_fstack();
	else
		return 0;
}
//TODO FQMTR
/* MFGPLL fmeter control flow is from GPU DE */
static unsigned int __gpufreq_get_fmeter_main_fgpu(void)
{
	u32 val = 0, ckgen_load_cnt = 0, ckgen_k1 = 0;
	int i = 0;
	unsigned int freq = 0;

	writel(0x00FF0000, MFGPLL_FQMTR_CON1);
	val = readl(MFGPLL_FQMTR_CON0);
	writel((val & 0x00FFFFFF), MFGPLL_FQMTR_CON0);
	writel(0x00009000, MFGPLL_FQMTR_CON0);

	ckgen_load_cnt = readl(MFGPLL_FQMTR_CON1) >> 16;
	ckgen_k1 = readl(MFGPLL_FQMTR_CON0) >> 24;

	val = readl(MFGPLL_FQMTR_CON0);
	writel((val | 0x1010), MFGPLL_FQMTR_CON0);

	/* wait fmeter finish */
	while (readl(MFGPLL_FQMTR_CON0) & 0x10) {
		udelay(10);
		i++;
		if (i > 1000) {
			GPUFREQ_LOGE("wait MFGPLL Fmeter timeout");
			break;
		}
	}

	val = readl(MFGPLL_FQMTR_CON1) & 0xFFFF;
	/* KHz */
	freq = (val * 26000 * (ckgen_k1 + 1)) / (ckgen_load_cnt + 1);

	return freq;
}

/* MFGSCPLL fmeter control flow is from GPU DE */
static unsigned int __gpufreq_get_fmeter_main_fstack(void)
{
	u32 val = 0, ckgen_load_cnt = 0, ckgen_k1 = 0;
	int i = 0;
	unsigned int freq = 0;

	writel(0x00FF0000, MFGSCPLL_FQMTR_CON1);
	val = readl(MFGSCPLL_FQMTR_CON0);
	writel((val & 0x00FFFFFF), MFGSCPLL_FQMTR_CON0);
	writel(0x00009000, MFGSCPLL_FQMTR_CON0);

	ckgen_load_cnt = readl(MFGSCPLL_FQMTR_CON1) >> 16;
	ckgen_k1 = readl(MFGSCPLL_FQMTR_CON0) >> 24;

	val = readl(MFGSCPLL_FQMTR_CON0);
	writel((val | 0x1010), MFGSCPLL_FQMTR_CON0);

	/* wait fmeter finish */
	while (readl(MFGSCPLL_FQMTR_CON0) & 0x10) {
		udelay(10);
		i++;
		if (i > 1000) {
			GPUFREQ_LOGE("wait MFGSCPLL Fmeter timeout");
			break;
		}
	}

	val = readl(MFGSCPLL_FQMTR_CON1) & 0xFFFF;
	/* KHz */
	freq = (val * 26000 * (ckgen_k1 + 1)) / (ckgen_load_cnt + 1);

	return freq;
}

/* GPU parking source use CCF API directly */
static unsigned int __gpufreq_get_fmeter_sub_fgpu(void)
{
	unsigned int freq = 0;

	/* Hz */
	freq = clk_get_rate(g_clk->clk_sub_parent) / 1000;

	return freq;
}

/* STACK parking source use CCF API directly */
static unsigned int __gpufreq_get_fmeter_sub_fstack(void)
{
	unsigned int freq = 0;

	/* Hz */
	freq = clk_get_rate(g_clk->clk_sc_sub_parent) / 1000;

	return freq;
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

	mfgpll = readl(MFG_PLL_CON1);

	pcw = mfgpll & (0x3FFFFF);

	posdiv_power = (mfgpll & (0x7 << POSDIV_SHIFT)) >> POSDIV_SHIFT;

	freq = (((pcw * TO_MHZ_TAIL + ROUNDING_VALUE) * MFGPLL_FIN) >> DDS_SHIFT) /
		(1 << posdiv_power) * TO_MHZ_HEAD;

	return freq;
}

//TODO
/*
 * API: get real current frequency from CON1 (khz)
 * Freq = ((PLL_CON1[21:0] * 26M) / 2^14) / 2^PLL_CON1[26:24]
 */
static unsigned int __gpufreq_get_real_fstack(void)
{
	unsigned int mfgpll = 0;
	unsigned int posdiv_power = 0;
	unsigned int freq = 0;
	unsigned int pcw = 0;

	mfgpll = readl(MFGSC_PLL_CON1);

	pcw = mfgpll & (0x3FFFFF);

	posdiv_power = (mfgpll & (0x7 << POSDIV_SHIFT)) >> POSDIV_SHIFT;

	freq = (((pcw * TO_MHZ_TAIL + ROUNDING_VALUE) * MFGPLL_FIN) >> DDS_SHIFT) /
		(1 << posdiv_power) * TO_MHZ_HEAD;

	return freq;
}

/* API: get real current Vgpu from regulator (mV * 100) */
static unsigned int __gpufreq_get_real_vgpu(void)
{
	unsigned int volt = 0;

	if (regulator_is_enabled(g_pmic->reg_vcore))
		/* regulator_get_voltage return volt with uV */
		volt = regulator_get_voltage(g_pmic->reg_vcore) / 10;

	return volt;
}

/* API: get real current Vstack from regulator (mV * 100) */
static unsigned int __gpufreq_get_real_vstack(void)
{
	unsigned int volt = 0;

	if (regulator_is_enabled(g_pmic->reg_vstack))
		/* regulator_get_voltage return volt with uV */
		volt = regulator_get_voltage(g_pmic->reg_vstack) / 10;

	return volt;
}

/* API: get real current Vsram from regulator (mV * 100) */
static unsigned int __gpufreq_get_real_vsram(void)
{
	unsigned int volt = 0;

	if (regulator_is_enabled(g_pmic->reg_vsram))
		/* regulator_get_voltage return volt with uV */
		volt = regulator_get_voltage(g_pmic->reg_vsram) / 10;

	return volt;
}
//TODO
/* DVFS HW Timing issue */
static void __gpufreq_dvfs_timing_control(unsigned int direction)
{
	u32 val = 0;

	/*
	 * Volt > 0.65V: MFG_DUMMY_REG[31]=1'b0
	 * Volt = 0.65V: MFG_DUMMY_REG[31]=1'b0 or 1'b1
	 * Volt < 0.65V: MFG_DUMMY_REG[31]=1'b1
	 */

	/* volt scale up */
	if (direction) {
		/* MFG_DUMMY_REG 0x13FBF500 [31] eco_2_data_path = 1'b0 */
		val = readl(g_mfg_top_base + 0x500);
		val &= ~(1UL << 31);
		writel(val, g_mfg_top_base + 0x500);
		GPUFREQ_LOGD("MFG_DUMMY_REG 0x13FBF500 [31] eco_2_data_path = 1'b0");
	/* volt scale down */
	} else {
		/* MFG_DUMMY_REG 0x13FBF500 [31] eco_2_data_path = 1'b1 */
		val = readl(g_mfg_top_base + 0x500);
		val |= (1UL << 31);
		writel(val, g_mfg_top_base + 0x500);
		GPUFREQ_LOGD("MFG_DUMMY_REG 0x13FBF500 [31] eco_2_data_path = 1'b1");
	}
}

/* DELSEL ULV SRAM access */
static void __gpufreq_delsel_control(unsigned int direction)
{
	u32 val = 0;

	/*
	 * Volt > 0.55V: MFG_SRAM_FUL_SEL_ULV[0]=1'b0
	 * Volt = 0.55V: MFG_SRAM_FUL_SEL_ULV[0]=1'b0 or 1'b1
	 * Volt < 0.55V: MFG_SRAM_FUL_SEL_ULV[0]=1'b1
	 */

	/* volt scale up */
	if (direction) {
		/* MFG_SRAM_FUL_SEL_ULV 0x13FBF080 [0] FUL_SEL_ULV = 1'b0 */
		val = readl(g_mfg_top_base + 0x80);
		val &= ~(1UL << 0);
		writel(val, g_mfg_top_base + 0x80);
		GPUFREQ_LOGD("MFG_SRAM_FUL_SEL_ULV 0x13FBF080 [0] FUL_SEL_ULV = 1'b0");
	/* volt scale down */
	} else {
		/* MFG_SRAM_FUL_SEL_ULV 0x13FBF080 [0] FUL_SEL_ULV = 1'b1 */
		val = readl(g_mfg_top_base + 0x80);
		val |= (1UL << 0);
		writel(val, g_mfg_top_base + 0x80);
		GPUFREQ_LOGD("MFG_SRAM_FUL_SEL_ULV 0x13FBF080 [0] FUL_SEL_ULV = 1'b1");
	}
}

/* API: find Fgpu that satify Fgpu >= Fstack*1.1 by given Fstack */
static void __gpufreq_get_hw_constraint_fgpu(unsigned int fstack,
	int *oppidx_gpu, unsigned int *fgpu)
{
	struct gpufreq_opp_info *working_table = g_gpu.working_table;
	unsigned int target_freq = 0;
	int min_oppidx = g_gpu.min_oppidx;
	int i = 0;

	/*
	 * Fstack <= 800MHz: find Fgpu >= Fstack*1.1
	 * Fstack > 800Mhz: Fgpu = 880MHz (OPP[0])
	 */

	/* stop if found Fgpu >= Fstack*1.1 or i == 0 */
	for (i = min_oppidx; i > 0; i--) {
		target_freq = working_table[i].freq * GPUFREQ_BASE_COEF;
		if (target_freq >= fstack * GPUFREQ_CONSTRAINT_COEF)
			break;
	}

	if (unlikely(i < 0))
		__gpufreq_abort(GPUFREQ_GPU_EXCEPTION,
			"fail to find HW constraint Fgpu: %d (Fstack: %d x1.1)",
			(int)(fstack * GPUFREQ_CONSTRAINT_COEF / GPUFREQ_BASE_COEF), fstack);

	*oppidx_gpu = i;
	*fgpu = working_table[i].freq;

	GPUFREQ_LOGD("target HW constraint Fgpu: %d (Fstack: %d x1.1)", *fgpu, fstack);
}

/* API: find Fstack that satify Fgpu >= Fstack*1.1 by given Fgpu */
static void __gpufreq_get_hw_constraint_fstack(unsigned int fgpu,
	int *oppidx_stack, unsigned int *fstack)
{
	struct gpufreq_opp_info *working_table = g_stack.working_table;
	unsigned int target_freq = 0;
	int min_oppidx = g_stack.min_oppidx;
	int i = 0;

	/* stop if found Fgpu >= Fstack*1.1 or i == min_oppidx */
	for (i = 0; i < min_oppidx; i++) {
		target_freq = working_table[i].freq * GPUFREQ_CONSTRAINT_COEF;
		if (fgpu * GPUFREQ_BASE_COEF >= target_freq)
			break;
	}

	if (unlikely(i > min_oppidx))
		__gpufreq_abort(GPUFREQ_GPU_EXCEPTION,
			"fail to find HW constraint Fstack: %d (Fgpu: %d /1.1)",
			(int)(fgpu * GPUFREQ_BASE_COEF / GPUFREQ_CONSTRAINT_COEF), fgpu);

	*oppidx_stack = i;
	*fstack = working_table[i].freq;

	GPUFREQ_LOGD("target HW constraint Fstack: %d (Fgpu: %d /1.1)", *fstack, fgpu);
}

/* API: find parking volt and control registers because of platform HW constraint */
static unsigned int __gpufreq_hw_constraint_parking(unsigned int volt_old, unsigned int volt_new)
{
	unsigned int volt_park = 0;

	/* volt scaling up */
	if (volt_new > volt_old) {
		/* additionally control register when cross special volt */
		if (volt_old == g_dvfs_timing_park_volt)
			__gpufreq_dvfs_timing_control(true);
		else if (volt_old == g_desel_ulv_park_volt)
			__gpufreq_delsel_control(true);

		/* check smaller parking volt first while scaling up */
		if (volt_new > g_desel_ulv_park_volt && volt_old < g_desel_ulv_park_volt)
			volt_park = g_desel_ulv_park_volt;
		else if (volt_new > g_dvfs_timing_park_volt && volt_old < g_dvfs_timing_park_volt)
			volt_park = g_dvfs_timing_park_volt;
		else
			volt_park = volt_new;
	/* volt scaling down */
	} else if (volt_new < volt_old) {
		/* additionally control register when cross special volt */
		if (volt_old == g_dvfs_timing_park_volt)
			__gpufreq_dvfs_timing_control(false);
		else if (volt_old == g_desel_ulv_park_volt)
			__gpufreq_delsel_control(false);

		/* check larger parking volt first while scaling down */
		if (volt_new < g_dvfs_timing_park_volt && volt_old > g_dvfs_timing_park_volt)
			volt_park = g_dvfs_timing_park_volt;
		else if (volt_new < g_desel_ulv_park_volt && volt_old > g_desel_ulv_park_volt)
			volt_park = g_desel_ulv_park_volt;
		else
			volt_park = volt_new;
	} else {
		volt_park = volt_new;
	}

	GPUFREQ_LOGD("volt_old: %d, volt_new: %d, volt_park: %d",
		volt_old, volt_new, volt_park);

	return volt_park;
}

/* API: convert predefined Freq contraint to Volt */
static void __gpufreq_update_hw_constraint_volt(void)
{
	int dvfs_timing_oppidx = 0, desel_ulv_oppidx = 0;

	mutex_lock(&gpufreq_lock);

	dvfs_timing_oppidx = __gpufreq_get_idx_by_fstack(DVFS_TIMING_PARK_FREQ);
	desel_ulv_oppidx = __gpufreq_get_idx_by_fstack(DELSEL_ULV_PARK_FREQ);

	g_dvfs_timing_park_volt = g_stack.working_table[dvfs_timing_oppidx].volt;
	g_desel_ulv_park_volt = g_stack.working_table[desel_ulv_oppidx].volt;

	GPUFREQ_LOGI("DVFS_TIMING OPP[%d].volt: %d, DESEL_ULV OPP[%d].volt: %d",
		dvfs_timing_oppidx, g_dvfs_timing_park_volt,
		desel_ulv_oppidx, g_desel_ulv_park_volt);

	mutex_unlock(&gpufreq_lock);
};

static void __gpufreq_mfg_backup_restore(enum gpufreq_power_state power)
{
	/* restore */
	if (power == POWER_ON) {
		if (g_dvfs_timing_park_reg)
			/* MFG_DUMMY_REG 0x13FBF500 */
			writel(g_dvfs_timing_park_reg, g_mfg_top_base + 0x500);
		if (g_desel_ulv_park_reg)
			/* MFG_SRAM_FUL_SEL_ULV 0x13FBF080*/
			writel(g_desel_ulv_park_reg, g_mfg_top_base + 0x80);
	/* backup */
	} else {
		/* MFG_DUMMY_REG 0x13FBF500 */
		g_dvfs_timing_park_reg = readl(g_mfg_top_base + 0x500);
		/* MFG_SRAM_FUL_SEL_ULV 0x13FBF080*/
		g_desel_ulv_park_reg = readl(g_mfg_top_base + 0x80);
	}
}
//TODO
static void __gpufreq_hw_dcm_control(void)
{
	u32 val = 0;

	/* MFG_DCM_CON_0 0x13FBF010 [6:0] BG3D_DBC_CNT = 7'b0111111 */
	/* MFG_DCM_CON_0 0x13FBF010 [15]  BG3D_DCM_EN = 1'b1 */
	val = readl(g_mfg_top_base + 0x10);
	val |= (1UL << 0);
	val |= (1UL << 1);
	val |= (1UL << 2);
	val |= (1UL << 3);
	val |= (1UL << 4);
	val |= (1UL << 5);
	val &= ~(1UL << 6);
	val |= (1UL << 15);
	writel(val, g_mfg_top_base + 0x10);

	/* MFG_ASYNC_CON 0x13FBF020 [23] MEM0_SLV_CG_ENABLE = 1'b1 */
	/* MFG_ASYNC_CON 0x13FBF020 [25] MEM1_SLV_CG_ENABLE = 1'b1 */
	val = readl(g_mfg_top_base + 0x20);
	val |= (1UL << 23);
	val |= (1UL << 25);
	writel(val, g_mfg_top_base + 0x20);

	/* MFG_GLOBAL_CON 0x13FBF0B0 [8]  GPU_SOCIF_MST_FREE_RUN = 1'b0 */
	/* MFG_GLOBAL_CON 0x13FBF0B0 [10] GPU_CLK_FREE_RUN = 1'b0 */
	/* MFG_GLOBAL_CON 0x13FBF0B0 [13] mfg_m0_GALS_slpprot_idle_en = 1'b1 */
	/* MFG_GLOBAL_CON 0x13FBF0B0 [14] mfg_m1_GALS_slpprot_idle_en = 1'b1 */
	/* MFG_GLOBAL_CON 0x13FBF0B0 [17] mfg_m0_1_GALS_slpprot_idle_en = 1'b1 */
	/* MFG_GLOBAL_CON 0x13FBF0B0 [18] mfg_m1_1_GALS_slpprot_idle_en = 1'b1 */
	/* MFG_GLOBAL_CON 0x13FBF0B0 [21] dvfs_hint_cg_en = 1'b0 */
	val = readl(g_mfg_top_base + 0xB0);
	val &= ~(1UL << 8);
	val &= ~(1UL << 10);
	val |= (1UL << 13);
	val |= (1UL << 14);
	val |= (1UL << 17);
	val |= (1UL << 18);
	val &= ~(1UL << 21);
	writel(val, g_mfg_top_base + 0xB0);

	/* MFG_RPC_AO_CLK_CFG 0x13F91034 [0] CG_FAXI_CK_SOC_IN_FREE_RUN = 1'b0 */
	val = readl(g_mfg_rpc_base + 0x1034);
	val &= ~(1UL << 0);
	writel(val, g_mfg_rpc_base + 0x1034);
}

static int __gpufreq_clock_control(enum gpufreq_power_state power)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("power=%d", power);

	if (power == POWER_ON) {
#if GPUFREQ_CG_CONTROL_ENABLE
		/* enable CG */
		ret = clk_prepare_enable(g_clk->subsys_mfg_cg);
		if (unlikely(ret)) {
			__gpufreq_abort(GPUFREQ_CCF_EXCEPTION,
				"fail to enable subsys_mfg_cg (%d)", ret);
			goto done;
		}
#endif /* GPUFREQ_CG_CONTROL_ENABLE */
		/* enable GPU MUX and GPU PLL */
		ret = clk_prepare_enable(g_clk->clk_mux);
		if (unlikely(ret)) {
			__gpufreq_abort(GPUFREQ_CCF_EXCEPTION,
				"fail to enable clk_mux (%d)", ret);
			goto done;
		}
		/* enable STACK MUX and STACK MUX */
		ret = clk_prepare_enable(g_clk->clk_sc_mux);
		if (unlikely(ret)) {
			__gpufreq_abort(GPUFREQ_CCF_EXCEPTION,
				"fail to enable clk_sc_mux (%d)", ret);
			goto done;
		}

		/* switch GPU MUX to main_parent */
		ret = __gpufreq_switch_clksrc_gpu(CLOCK_MAIN);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to switch GPU to main clock source (%d)", ret);
			goto done;
		}
		/* switch STACK MUX to main_parent */
		ret = __gpufreq_switch_clksrc_stack(CLOCK_MAIN);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to switch STACK to main clock source (%d)", ret);
			goto done;
		}

		g_gpu.cg_count++;
		g_stack.cg_count++;
	} else {
		/* switch STACK MUX to sub_parent */
		ret = __gpufreq_switch_clksrc_stack(CLOCK_SUB);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to switch STACK to sub clock source (%d)", ret);
			goto done;
		}
		/* switch GPU MUX to sub_parent */
		ret = __gpufreq_switch_clksrc_gpu(CLOCK_SUB);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to switch GPU to sub clock source (%d)", ret);
			goto done;
		}

		/* disable STACK MUX and STACK PLL */
		clk_disable_unprepare(g_clk->clk_sc_mux);
		/* disable GPU MUX and GPU PLL */
		clk_disable_unprepare(g_clk->clk_mux);
#if GPUFREQ_CG_CONTROL_ENABLE
		/* disable CG */
		clk_disable_unprepare(g_clk->subsys_mfg_cg);
#endif /* GPUFREQ_CG_CONTROL_ENABLE */

		g_gpu.cg_count--;
		g_stack.cg_count--;
	}

done:
	GPUFREQ_TRACE_END();

	return ret;
}

/* AOC2.0: Vcore_AO can power off */
static void __gpufreq_aoc_control(enum gpufreq_power_state power)
{
	u32 val = 0;

	/* wait HW semaphore: SPM_SEMA_M4 0x1C0016AC [0] = 1'b1 */
	do {
		val = readl(g_sleep + 0x6AC);
		val |= (1UL << 0);
		writel(val, g_sleep + 0x6AC);
	} while ((readl(g_sleep + 0x6AC) & 0x1) != 0x1);

	/* power on: AOCISO -> AOCLHENB */
	if (power == POWER_ON) {
		/* SOC_BUCK_ISO_CON 0x1C001F30 [9] AOC_VGPU_SRAM_ISO_DIN = 1'b0 */
		val = readl(g_sleep + 0xF30);
		val &= ~(1UL << 9);
		writel(val, g_sleep + 0xF30);

		/* SOC_BUCK_ISO_CON 0x1C001F30 [10] AOC_VGPU_SRAM_LATCH_ENB = 1'b0 */
		val = readl(g_sleep + 0xF30);
		val &= ~(1UL << 10);
		writel(val, g_sleep + 0xF30);

		GPUFREQ_LOGD("AOC_VGPU_SRAM_ISO_DIN = 1'b0, AOC_VGPU_SRAM_LATCH_ENB = 1'b0");
	/* power off: AOCLHENB -> AOCISO */
	} else {
		/* SOC_BUCK_ISO_CON 0x1C001F30 [10] AOC_VGPU_SRAM_LATCH_ENB = 1'b1 */
		val = readl(g_sleep + 0xF30);
		val |= (1UL << 10);
		writel(val, g_sleep + 0xF30);

		/* SOC_BUCK_ISO_CON 0x1C001F30 [9] AOC_VGPU_SRAM_ISO_DIN = 1'b1 */
		val = readl(g_sleep + 0xF30);
		val |= (1UL << 9);
		writel(val, g_sleep + 0xF30);

		GPUFREQ_LOGD("AOC_VGPU_SRAM_ISO_DIN = 1'b1, AOC_VGPU_SRAM_LATCH_ENB = 1'b1");
	}

	/* signal HW semaphore: SPM_SEMA_M4 0x1C0016AC [0] = 1'b1 */
	val = readl(g_sleep + 0x6AC);
	val |= (1UL << 0);
	writel(val, g_sleep + 0x6AC);
}

/* ACP: GPU can access CPU cache directly */
static void __gpufreq_acp_control(void)
{
	u32 val = 0;

	/* MFG_AXCOHERENCE 0x13FBF168 [0] M0_coherence_enable = 1'b1 */
	/* MFG_AXCOHERENCE 0x13FBF168 [1] M1_coherence_enable = 1'b1 */
	/* MFG_AXCOHERENCE 0x13FBF168 [2] M2_coherence_enable = 1'b1 */
	/* MFG_AXCOHERENCE 0x13FBF168 [3] M3_coherence_enable = 1'b1 */
	val = readl(g_mfg_top_base + 0x168);
	val |= (1UL << 0);
	val |= (1UL << 1);
	val |= (1UL << 2);
	val |= (1UL << 3);
	writel(val, g_mfg_top_base + 0x168);

	/* MFG_1TO2AXI_CON_00 0x13FBF8E0 [11:0] mfg_axi1to2_R_dispatch_mode = 0x855 */
	val = readl(g_mfg_top_base + 0x8E0);
	val |= 0x855;
	writel(val, g_mfg_top_base + 0x8E0);

	/* MFG_1TO2AXI_CON_02 0x13FBF8E8 [11:0] mfg_axi1to2_R_dispatch_mode = 0x855 */
	val = readl(g_mfg_top_base + 0x8E8);
	val |= 0x855;
	writel(val, g_mfg_top_base + 0x8E8);

	/* MFG_1TO2AXI_CON_04 0x13FBF910 [11:0] mfg_axi1to2_R_dispatch_mode = 0x855 */
	val = readl(g_mfg_top_base + 0x910);
	val |= 0x855;
	writel(val, g_mfg_top_base + 0x910);

	/* MFG_1TO2AXI_CON_06 0x13FBF918 [11:0] mfg_axi1to2_R_dispatch_mode = 0x855 */
	val = readl(g_mfg_top_base + 0x918);
	val |= 0x855;
	writel(val, g_mfg_top_base + 0x918);

	/* MFG_OUT_1TO2AXI_CON_00 0x13FBF900 [11:0] mfg_axi1to2_R_dispatch_mode = 0x055 */
	val = readl(g_mfg_top_base + 0x900);
	val |= 0x055;
	writel(val, g_mfg_top_base + 0x900);

	/* MFG_OUT_1TO2AXI_CON_02 0x13FBF908 [11:0] mfg_axi1to2_R_dispatch_mode = 0x055 */
	val = readl(g_mfg_top_base + 0x908);
	val |= 0x055;
	writel(val, g_mfg_top_base + 0x908);

	/* MFG_OUT_1TO2AXI_CON_04 0x13FBF920 [11:0] mfg_axi1to2_R_dispatch_mode = 0x055 */
	val = readl(g_mfg_top_base + 0x920);
	val |= 0x055;
	writel(val, g_mfg_top_base + 0x920);

	/* MFG_OUT_1TO2AXI_CON_06 0x13FBF928 [11:0] mfg_axi1to2_R_dispatch_mode = 0x055 */
	val = readl(g_mfg_top_base + 0x928);
	val |= 0x055;
	writel(val, g_mfg_top_base + 0x928);
}

/* GPM1.0: di/dt reduction by slowing down speed of frequency scaling up or down */
static void __gpufreq_gpm_control(void)
{
	/* MFG_I2M_PROTECTOR_CFG_00 0x13FBFF60 = 0x20300316 */
	writel(0x20300316, g_mfg_top_base + 0xF60);

	/* MFG_I2M_PROTECTOR_CFG_01 0x13FBFF64 = 0x1800000C */
	writel(0x1800000C, g_mfg_top_base + 0xF64);

	/* MFG_I2M_PROTECTOR_CFG_02 0x13FBFF68 = 0x01010802 */
	writel(0x01010802, g_mfg_top_base + 0xF68);

	/* MFG_I2M_PROTECTOR_CFG_03 0x13FBFFA8 = 0x000227F3 */
	writel(0x000227F3, g_mfg_top_base + 0xFA8);

	/* wait 1us */
	udelay(1);

	/* MFG_I2M_PROTECTOR_CFG_00 0x13FBFF60 = 0x20300317 */
	writel(0x20300317, g_mfg_top_base + 0xF60);
}

static int __gpufreq_mtcmos_control(enum gpufreq_power_state power)
{
	int ret = GPUFREQ_SUCCESS;
	u32 val = 0;

	GPUFREQ_TRACE_START("power=%d", power);

	if (power == POWER_ON) {
		/* MFG1 on by CCF */
		ret = pm_runtime_get_sync(g_mtcmos->mfg1_dev);
		if (unlikely(ret < 0)) {
			__gpufreq_abort(GPUFREQ_CCF_EXCEPTION,
				"fail to enable mfg1_dev (%d)", ret);
			goto done;
		}
		g_gpu.mtcmos_count++;
		g_stack.mtcmos_count++;

#if GPUFREQ_CHECK_MTCMOS_PWR_STATUS
		/* check MFG0-1 power status */
		if (g_sleep) {
			val = readl(g_sleep + PWR_STATUS_OFS) & MFG_0_1_PWR_MASK;
			if (unlikely(val != MFG_0_1_PWR_MASK)) {
				__gpufreq_abort(GPUFREQ_GPU_EXCEPTION,
					"incorrect MFG0-1 power on status: 0x%08x", val);
				ret = GPUFREQ_EINVAL;
				goto done;
			}
		}
#endif /* GPUFREQ_CHECK_MTCMOS_PWR_STATUS */

#if !GPUFREQ_PDCv2_ENABLE
		/* manually control MFG2-12 if PDC is disabled */
		ret = pm_runtime_get_sync(g_mtcmos->mfg2_dev);
		if (unlikely(ret < 0)) {
			__gpufreq_abort(GPUFREQ_CCF_EXCEPTION,
				"fail to enable mfg2_dev (%d)", ret);
			goto done;
		}
		ret = pm_runtime_get_sync(g_mtcmos->mfg3_dev);
		if (unlikely(ret < 0)) {
			__gpufreq_abort(GPUFREQ_CCF_EXCEPTION,
				"fail to enable mfg3_dev (%d)", ret);
			goto done;
		}
		ret = pm_runtime_get_sync(g_mtcmos->mfg4_dev);
		if (unlikely(ret < 0)) {
			__gpufreq_abort(GPUFREQ_CCF_EXCEPTION,
				"fail to enable mfg4_dev (%d)", ret);
			goto done;
		}
		ret = pm_runtime_get_sync(g_mtcmos->mfg5_dev);
		if (unlikely(ret < 0)) {
			__gpufreq_abort(GPUFREQ_CCF_EXCEPTION,
				"fail to enable mfg5_dev (%d)", ret);
			goto done;
		}
		ret = pm_runtime_get_sync(g_mtcmos->mfg6_dev);
		if (unlikely(ret < 0)) {
			__gpufreq_abort(GPUFREQ_CCF_EXCEPTION,
				"fail to enable mfg6_dev (%d)", ret);
			goto done;
		}

		if (g_shader_present & MFG3_SHADER_STACK0) {
			ret = pm_runtime_get_sync(g_mtcmos->mfg7_dev);
			if (unlikely(ret < 0)) {
				__gpufreq_abort(GPUFREQ_CCF_EXCEPTION,
					"fail to enable mfg7_dev (%d)", ret);
				goto done;
			}
			ret = pm_runtime_get_sync(g_mtcmos->mfg9_dev);
			if (unlikely(ret < 0)) {
				__gpufreq_abort(GPUFREQ_CCF_EXCEPTION,
					"fail to enable mfg9_dev (%d)", ret);
				goto done;
			}
		}
		if (g_shader_present & MFG4_SHADER_STACK1) {
			ret = pm_runtime_get_sync(g_mtcmos->mfg8_dev);
			if (unlikely(ret < 0)) {
				__gpufreq_abort(GPUFREQ_CCF_EXCEPTION,
					"fail to enable mfg8_dev (%d)", ret);
				goto done;
			}
		}
		if (g_shader_present & MFG5_SHADER_STACK2) {
			ret = pm_runtime_get_sync(g_mtcmos->mfg10_dev);
			if (unlikely(ret < 0)) {
				__gpufreq_abort(GPUFREQ_CCF_EXCEPTION,
					"fail to enable mfg10_dev (%d)", ret);
				goto done;
			}
			ret = pm_runtime_get_sync(g_mtcmos->mfg12_dev);
			if (unlikely(ret < 0)) {
				__gpufreq_abort(GPUFREQ_CCF_EXCEPTION,
					"fail to enable mfg12_dev (%d)", ret);
				goto done;
			}
		}
		if (g_shader_present & MFG6_SHADER_STACK4) {
			ret = pm_runtime_get_sync(g_mtcmos->mfg11_dev);
			if (unlikely(ret < 0)) {
				__gpufreq_abort(GPUFREQ_CCF_EXCEPTION,
					"fail to enable mfg11_dev (%d)", ret);
				goto done;
			}
		}

#if GPUFREQ_CHECK_MTCMOS_PWR_STATUS
		/* CCF contorl, check MFG0-12 power status */
		if (g_sleep) {
			if (g_shader_present == GPU_SHADER_PRESENT_6) {
				val = readl(g_sleep + PWR_STATUS_OFS) & MFG_0_12_PWR_MASK;
				if (unlikely(val != MFG_0_12_PWR_MASK)) {
					__gpufreq_abort(GPUFREQ_GPU_EXCEPTION,
						"incorrect MFG0-12 power on status: 0x%08x", val);
					ret = GPUFREQ_EINVAL;
					goto done;
				}
			}
		}
#endif /* GPUFREQ_CHECK_MTCMOS_PWR_STATUS */
#endif /* GPUFREQ_PDCv2_ENABLE */
	} else {
#if !GPUFREQ_PDCv2_ENABLE
		/* manually control MFG2-12 if PDC is disabled */
		if (g_shader_present & MFG6_SHADER_STACK4) {
			ret = pm_runtime_put_sync(g_mtcmos->mfg11_dev);
			if (unlikely(ret < 0)) {
				__gpufreq_abort(GPUFREQ_CCF_EXCEPTION,
					"fail to disable mfg11_dev (%d)", ret);
				goto done;
			}
		}
		if (g_shader_present & MFG5_SHADER_STACK2) {
			ret = pm_runtime_put_sync(g_mtcmos->mfg12_dev);
			if (unlikely(ret < 0)) {
				__gpufreq_abort(GPUFREQ_CCF_EXCEPTION,
					"fail to disable mfg12_dev (%d)", ret);
				goto done;
			}
			ret = pm_runtime_put_sync(g_mtcmos->mfg10_dev);
			if (unlikely(ret < 0)) {
				__gpufreq_abort(GPUFREQ_CCF_EXCEPTION,
					"fail to disable mfg10_dev (%d)", ret);
				goto done;
			}
		}
		if (g_shader_present & MFG4_SHADER_STACK1) {
			ret = pm_runtime_put_sync(g_mtcmos->mfg8_dev);
			if (unlikely(ret < 0)) {
				__gpufreq_abort(GPUFREQ_CCF_EXCEPTION,
					"fail to disable mfg8_dev (%d)", ret);
				goto done;
			}
		}
		if (g_shader_present & MFG3_SHADER_STACK0) {
			ret = pm_runtime_put_sync(g_mtcmos->mfg9_dev);
			if (unlikely(ret < 0)) {
				__gpufreq_abort(GPUFREQ_CCF_EXCEPTION,
					"fail to disable mfg9_dev (%d)", ret);
				goto done;
			}
			ret = pm_runtime_put_sync(g_mtcmos->mfg7_dev);
			if (unlikely(ret < 0)) {
				__gpufreq_abort(GPUFREQ_CCF_EXCEPTION,
					"fail to disable mfg7_dev (%d)", ret);
				goto done;
			}
		}

		ret = pm_runtime_put_sync(g_mtcmos->mfg6_dev);
		if (unlikely(ret < 0)) {
			__gpufreq_abort(GPUFREQ_CCF_EXCEPTION,
				"fail to disable mfg6_dev (%d)", ret);
			goto done;
		}
		ret = pm_runtime_put_sync(g_mtcmos->mfg5_dev);
		if (unlikely(ret < 0)) {
			__gpufreq_abort(GPUFREQ_CCF_EXCEPTION,
				"fail to disable mfg5_dev (%d)", ret);
			goto done;
		}
		ret = pm_runtime_put_sync(g_mtcmos->mfg4_dev);
		if (unlikely(ret < 0)) {
			__gpufreq_abort(GPUFREQ_CCF_EXCEPTION,
				"fail to disable mfg4_dev (%d)", ret);
			goto done;
		}
		ret = pm_runtime_put_sync(g_mtcmos->mfg3_dev);
		if (unlikely(ret < 0)) {
			__gpufreq_abort(GPUFREQ_CCF_EXCEPTION,
				"fail to disable mfg3_dev (%d)", ret);
			goto done;
		}
		ret = pm_runtime_put_sync(g_mtcmos->mfg2_dev);
		if (unlikely(ret < 0)) {
			__gpufreq_abort(GPUFREQ_CCF_EXCEPTION,
				"fail to disable mfg2_dev (%d)", ret);
			goto done;
		}
#endif /* GPUFREQ_PDCv2_ENABLE */

		/* MFG1 off by CCF */
		ret = pm_runtime_put_sync(g_mtcmos->mfg1_dev);
		if (unlikely(ret < 0)) {
			__gpufreq_abort(GPUFREQ_CCF_EXCEPTION,
				"fail to disable mfg1_dev (%d)", ret);
			goto done;
		}
		g_gpu.mtcmos_count--;
		g_stack.mtcmos_count--;

#if GPUFREQ_CHECK_MTCMOS_PWR_STATUS
		/* no matter who control, check MFG1-12 power status, MFG0 is off in ATF */
		if (g_sleep && !g_stack.mtcmos_count) {
			val = readl(g_sleep + PWR_STATUS_OFS) & MFG_1_12_PWR_MASK;
			if (unlikely(val))
				/* only print error if pwr is incorrect when mtcmos off */
				GPUFREQ_LOGE("incorrect MFG1-12 power off status: 0x%08x", val);
		}
#endif /* GPUFREQ_CHECK_MTCMOS_PWR_STATUS */
	}

done:
	GPUFREQ_TRACE_END();

	return ret;
}

static int __gpufreq_buck_control(enum gpufreq_power_state power)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("power=%d", power);

	/* power on */
	if (power == POWER_ON) {
#if GPUFREQ_VCORE_DVFS_ENABLE
		/* vote current Vgpu to DVFSRC as GPU power on */
		ret = regulator_set_voltage(g_pmic->reg_dvfsrc, g_gpu.cur_volt * 10, INT_MAX);
		if (unlikely(ret)) {
			__gpufreq_abort(GPUFREQ_PMIC_EXCEPTION, "fail to enable VGPU (%d)", ret);
			goto done;
		}
		g_gpu.buck_count++;
#endif /* GPUFREQ_VCORE_DVFS_ENABLE */

		ret = regulator_enable(g_pmic->reg_vstack);
		if (unlikely(ret)) {
			__gpufreq_abort(GPUFREQ_PMIC_EXCEPTION, "fail to enable VSTACK (%d)", ret);
			goto done;
		}
		g_stack.buck_count++;
	/* power off */
	} else {
		ret = regulator_disable(g_pmic->reg_vstack);
		if (unlikely(ret)) {
			__gpufreq_abort(GPUFREQ_PMIC_EXCEPTION, "fail to disable VSTACK (%d)", ret);
			goto done;
		}
		g_stack.buck_count--;

#if GPUFREQ_VCORE_DVFS_ENABLE
		/* vote level_0 to DVFSRC as GPU power off */
		ret = regulator_set_voltage(g_pmic->reg_dvfsrc, VCORE_LEVEL_0 * 10, INT_MAX);
		if (unlikely(ret)) {
			__gpufreq_abort(GPUFREQ_PMIC_EXCEPTION, "fail to disable VGPU (%d)", ret);
			goto done;
		}
		g_gpu.buck_count--;
#endif /* GPUFREQ_VCORE_DVFS_ENABLE */
	}

done:
	GPUFREQ_TRACE_END();

	return ret;
}
//TODO
static void __gpufreq_slc_control(void)
{
	writel(0x00000000, g_mfg_top_base + 0x700); // group0: default

	writel(0xFFFFFFFF, g_mfg_top_base + 0x704); // group1
	writel(0xFFFFFFFF, g_mfg_top_base + 0x708);
	writel(0x0600A001, g_mfg_top_base + 0x70C);

	writel(0xFFFFFFFF, g_mfg_top_base + 0x710); // group2
	writel(0xFFFFFFFF, g_mfg_top_base + 0x714);
	writel(0x0600A002, g_mfg_top_base + 0x718);

	writel(0xFFFFFFFF, g_mfg_top_base + 0x71C); // group3
	writel(0xFFFFFFFF, g_mfg_top_base + 0x720);
	writel(0x0600A003, g_mfg_top_base + 0x724);

	writel(0xFFFFFFFF, g_mfg_top_base + 0x728); // group4
	writel(0xFFFFFFFF, g_mfg_top_base + 0x72C);
	writel(0x0600A004, g_mfg_top_base + 0x730);

	writel(0xFFFFFFFF, g_mfg_top_base + 0x734); // group5
	writel(0xFFFFFFFF, g_mfg_top_base + 0x738);
	writel(0x0600A005, g_mfg_top_base + 0x73C);

	writel(0xFFFFFFFF, g_mfg_top_base + 0x740); // group6 (OK)
	writel(0xFFFFFFFF, g_mfg_top_base + 0x744);
	writel(0x06002006, g_mfg_top_base + 0x748); // W-alloc, R cache, wo spec.
	//writel(0x0600A006, g_mfg_base + 0x748); // W-alloc, R cache, wz spec.
	//writel(0x0600E006, g_mfg_base + 0x748); // RW-alloc

	writel(0xFFFFFFFF, g_mfg_top_base + 0x74C); // group7
	writel(0xFFFFFFFF, g_mfg_top_base + 0x750);
	writel(0x0600A007, g_mfg_top_base + 0x754);

	writel(0xFFFFFFFF, g_mfg_top_base + 0x758); // group8
	writel(0xFFFFFFFF, g_mfg_top_base + 0x75C);
	writel(0x0600A008, g_mfg_top_base + 0x760);

	writel(0xFFFFFFFF, g_mfg_top_base + 0x764); // group9
	writel(0xFFFFFFFF, g_mfg_top_base + 0x768);
	writel(0x0600A009, g_mfg_top_base + 0x76C);

	writel(0xFFFFFFFF, g_mfg_top_base + 0x770); // group10
	writel(0xFFFFFFFF, g_mfg_top_base + 0x774);
	writel(0x0600A00a, g_mfg_top_base + 0x778);

	writel(0xFFFFFFFF, g_mfg_top_base + 0x77C); // group11
	writel(0xFFFFFFFF, g_mfg_top_base + 0x780);
	writel(0x0600A00b, g_mfg_top_base + 0x784);

	writel(0xFFFFFFFF, g_mfg_top_base + 0x788); // group12
	writel(0xFFFFFFFF, g_mfg_top_base + 0x78C);
	writel(0x0600A00c, g_mfg_top_base + 0x790);

	writel(0xFFFFFFFF, g_mfg_top_base + 0x794); // group13
	writel(0xFFFFFFFF, g_mfg_top_base + 0x798);
	writel(0x0600A00d, g_mfg_top_base + 0x79C);

	writel(0xFFFFFFFF, g_mfg_top_base + 0x7A0); // group14
	writel(0xFFFFFFFF, g_mfg_top_base + 0x7A4);
	writel(0x0600A00e, g_mfg_top_base + 0x7A8);

	writel(0xFFFFFFFF, g_mfg_top_base + 0x7AC); // group15
	writel(0xFFFFFFFF, g_mfg_top_base + 0x7B0);
	writel(0x0600A00f, g_mfg_top_base + 0x7B4);
}

/* API: init first OPP idx by init freq set in preloader */
static int __gpufreq_init_opp_idx(void)
{
	struct gpufreq_opp_info *working_table = g_stack.working_table;
	unsigned int cur_fgpu = 0, cur_fstack = 0;
	int target_oppidx_stack = -1;
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START();

	/* get current GPU OPP idx by freq set in preloader */
	cur_fgpu = g_gpu.cur_freq;
	g_gpu.cur_oppidx = __gpufreq_get_idx_by_fgpu(cur_fgpu);

	/* get current STACK OPP idx by freq set in preloader */
	cur_fstack = g_stack.cur_freq;
	g_stack.cur_oppidx = __gpufreq_get_idx_by_fstack(cur_fstack);

	/* decide first OPP idx by custom setting */
	if (__gpufreq_custom_init_enable()) {
		target_oppidx_stack = GPUFREQ_CUST_INIT_OPPIDX;
		GPUFREQ_LOGI("custom init STACK OPP index: %d, Freq: %d",
			target_oppidx_stack, working_table[target_oppidx_stack].freq);
	/* decide first OPP idx by SRAMRC setting */
	} else {
		target_oppidx_stack = __gpufreq_get_idx_by_vstack(GPUFREQ_SAFE_VLOGIC);
		GPUFREQ_LOGI("SRAMRC init STACK OPP index: %d, Volt: %d",
			target_oppidx_stack, working_table[target_oppidx_stack].volt);
	}

	/* init first OPP index */
	if (!__gpufreq_dvfs_enable()) {
		g_dvfs_state = DVFS_DISABLE;
		GPUFREQ_LOGI("DVFS state: 0x%x, disable DVFS", g_dvfs_state);

		/* set OPP once if DVFS is disabled but custom init is enabled */
		if (__gpufreq_custom_init_enable()) {
			ret = __gpufreq_generic_commit_stack(target_oppidx_stack, DVFS_DISABLE);
			if (unlikely(ret)) {
				GPUFREQ_LOGE("fail to commit STACK OPP index: %d (%d)",
					target_oppidx_stack, ret);
				goto done;
			}
		}
	} else {
		g_dvfs_state = DVFS_FREE;
		GPUFREQ_LOGI("DVFS state: 0x%x, enable DVFS", g_dvfs_state);

		ret = __gpufreq_generic_commit_stack(target_oppidx_stack, DVFS_FREE);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to commit STACK OPP index: %d (%d)",
				target_oppidx_stack, ret);
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
	struct gpufreq_opp_info *working_table = g_stack.working_table;
	int opp_num = g_stack.opp_num;

	for (i = 0; i < opp_num; i++) {
		freq = working_table[i].freq;
		volt = working_table[i].volt;

		p_leakage = __gpufreq_get_lkg_pstack(volt);
		p_dynamic = __gpufreq_get_dyn_pstack(freq, volt);

		p_total = p_dynamic + p_leakage;

		working_table[i].power = p_total;

		GPUFREQ_LOGD("STACK[%02d] power: %d (dynamic: %d, leakage: %d)",
			i, p_total, p_dynamic, p_leakage);
	}
}

/* API: resume dvfs to free run */
static void __gpufreq_resume_dvfs(void)
{
	GPUFREQ_TRACE_START();

	__gpufreq_power_control(POWER_OFF);

	__gpufreq_set_dvfs_state(false, DVFS_AGING_KEEP);

	GPUFREQ_LOGI("resume DVFS, state: 0x%x", g_dvfs_state);

	GPUFREQ_TRACE_END();
}

/* API: pause dvfs to given freq and volt */
static int __gpufreq_pause_dvfs(void)
{
	int ret = GPUFREQ_SUCCESS;
	/* STACK */
	unsigned int cur_fstack = 0, cur_vstack = 0, cur_vsram_stack = 0;
	unsigned int target_fstack = 0, target_vstack = 0, target_vsram_stack = 0;
	/* GPU */
	unsigned int cur_fgpu = 0, cur_vgpu = 0, cur_vsram_gpu = 0;
	unsigned int target_fgpu = 0, target_vgpu = 0, target_vsram_gpu = 0;

	GPUFREQ_TRACE_START();

	__gpufreq_set_dvfs_state(true, DVFS_AGING_KEEP);

	ret = __gpufreq_power_control(POWER_ON);
	if (unlikely(ret < 0)) {
		GPUFREQ_LOGE("fail to control power state: %d (%d)", POWER_ON, ret);
		__gpufreq_set_dvfs_state(false, DVFS_AGING_KEEP);
		goto done;
	}

	mutex_lock(&gpufreq_lock);

	/* prepare STACK setting */
	cur_fstack = g_stack.cur_freq;
	cur_vstack = g_stack.cur_volt;
	cur_vsram_stack = g_stack.cur_vsram;
	target_fstack = GPUFREQ_AGING_KEEP_FSTACK;
	target_vstack = GPUFREQ_AGING_KEEP_VSTACK;
	target_vsram_stack = VSRAM_LEVEL_0;

	/* prepare GPU setting */
	cur_fgpu = g_gpu.cur_freq;
	cur_vgpu = g_gpu.cur_volt;
	cur_vsram_gpu = g_gpu.cur_vsram;
	target_fgpu = GPUFREQ_AGING_KEEP_FGPU;
	target_vgpu = GPUFREQ_AGING_KEEP_VGPU;
	target_vsram_gpu = VSRAM_LEVEL_0;

	GPUFREQ_LOGD("begin to commit STACK F(%d->%d), V(%d->%d), GPU F(%d->%d), V(%d->%d)",
		cur_fstack, target_fstack, cur_vstack, target_vstack,
		cur_fgpu, target_fgpu, cur_vgpu, target_vgpu);

	ret = __gpufreq_generic_scale_gpu(cur_fgpu, target_fgpu,
		cur_vgpu, target_vgpu, cur_vsram_gpu, target_vsram_gpu);
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to scale GPU: Freq(%d->%d), Volt(%d->%d), Vsram(%d->%d)",
			cur_fgpu, target_fgpu, cur_vgpu, target_vgpu,
			cur_vsram_gpu, target_vsram_gpu);
		goto done_unlock;
	}

	ret = __gpufreq_generic_scale_stack(cur_fstack, target_fstack,
		cur_vstack, target_vstack, cur_vsram_stack, target_vsram_stack);
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to scale STACK: Freq(%d->%d), Volt(%d->%d), Vsram(%d->%d)",
			cur_fstack, target_fstack, cur_vstack, target_vstack,
			cur_vsram_stack, target_vsram_stack);
		goto done_unlock;
	}

	GPUFREQ_LOGI("pause DVFS at STACK(%d, %d), GPU(%d, %d), state: 0x%x",
		target_fstack, target_vstack, target_fgpu, target_vgpu, g_dvfs_state);

done_unlock:
	mutex_unlock(&gpufreq_lock);
done:
	GPUFREQ_TRACE_END();

	return ret;
}

/*
 * API: interpolate OPP of none AVS idx.
 * step = (large - small) / range
 * vnew = large - step * j
 */
static void __gpufreq_interpolate_volt(void)
{
	int avs_num = 0;
	int front_idx = 0, rear_idx = 0, inner_idx = 0;
	unsigned int large_volt = 0, small_volt = 0;
	unsigned int large_freq = 0, small_freq = 0;
	unsigned int inner_volt = 0, inner_freq = 0;
	unsigned int previous_volt = 0;
	int range = 0;
	int slope = 0;
	int i = 0, j = 0;
	struct gpufreq_opp_info *signed_table = g_stack.signed_table;

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

		GPUFREQ_LOGD("STACK[%02d*] Freq: %d, Volt: %d, slope: %d",
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
			signed_table[inner_idx].vsram = __gpufreq_get_vsram_by_vstack(inner_volt);

			GPUFREQ_LOGD("STACK[%02d*] Freq: %d, Volt: %d, Vsram: %d,",
				inner_idx, inner_freq*1000, inner_volt,
				signed_table[inner_idx].vsram);
		}
		GPUFREQ_LOGD("STACK[%02d*] Freq: %d, Volt: %d",
			front_idx, large_freq*1000, large_volt);
	}

	mutex_unlock(&gpufreq_lock);
}

/* API: apply aging volt diff to working table */
static void __gpufreq_apply_aging(unsigned int apply_aging)
{
	int i = 0;
	struct gpufreq_opp_info *working_table = g_stack.working_table;
	int opp_num = g_stack.opp_num;

	mutex_lock(&gpufreq_lock);

	for (i = 0; i < opp_num; i++) {
		if (apply_aging)
			working_table[i].volt -= working_table[i].vaging;
		else
			working_table[i].volt += working_table[i].vaging;

		working_table[i].vsram = __gpufreq_get_vsram_by_vstack(working_table[i].volt);

		GPUFREQ_LOGD("apply Vaging: %d, STACK[%02d] Volt: %d, Vsram: %d",
			apply_aging, i, working_table[i].volt, working_table[i].vsram);
	}

	mutex_unlock(&gpufreq_lock);
}

/* API: apply given adjustment table to signed table */
static void __gpufreq_apply_adjust(enum gpufreq_target target,
	struct gpufreq_adj_info *adj_table, int adj_num)
{
	int i = 0;
	int oppidx = 0;
	struct gpufreq_opp_info *signed_table = NULL;
	int opp_num = 0;

	GPUFREQ_TRACE_START("adj_table=0x%x, adj_num=%d, target=%d",
		adj_table, adj_num, target);

	if (!adj_table) {
		GPUFREQ_LOGE("null adjustment table (EINVAL)");
		goto done;
	}

	if (target == TARGET_STACK) {
		signed_table = g_stack.signed_table;
		opp_num = g_stack.signed_opp_num;
	} else {
		signed_table = g_gpu.signed_table;
		opp_num = g_gpu.signed_opp_num;
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
			(target == TARGET_STACK) ? "STACK" : "GPU",
			oppidx, signed_table[oppidx].freq,
			signed_table[oppidx].volt,
			signed_table[oppidx].vsram,
			signed_table[oppidx].vaging);
	}

	mutex_unlock(&gpufreq_lock);

done:
	GPUFREQ_TRACE_END();
}

//TODO
/* API: get Aging sensor data from EFUSE, return if success*/
static unsigned int __gpufreq_asensor_read_efuse(u32 *a_t0_lvt_rt, u32 *a_t0_ulvt_rt,
	u32 *a_t0_ulvtll_rt, u32 *a_shift_error, u32 *efuse_error)
{
#if GPUFREQ_ASENSOR_ENABLE
	u32 efuse_val1 = 0, efuse_val2 = 0, efuse_val3 = 0, efuse_val4 = 0;

	/* efuse_val1 address: 0x11EE0AA8 */
	efuse_val1 = readl(g_efuse_base + 0xAA8);
	/* efuse_val1 address: 0x11EE0AAC */
	efuse_val2 = readl(g_efuse_base + 0xAAC);
	/* efuse_val1 address: 0x11EE0B6C */
	efuse_val3 = readl(g_efuse_base + 0xB6C);
	/* efuse_val1 address: 0x11EE05D8 */
	efuse_val4 = readl(g_efuse_base + 0x5D8);
	if (efuse_val1 == 0)
		return false;

	GPUFREQ_LOGD("efuse_val1=0x%08x, efuse_val2=0x%08x, efuse_val3=0x%08x, efuse_val4=0x%08x",
		efuse_val1, efuse_val2, efuse_val3, efuse_val4);

	/* A_T0_ULVTLL_RT: 0x11EE0AA8 [9:0],   shift value: 0x5DC */
	*a_t0_ulvtll_rt = (efuse_val1  & 0x000003FF) + 0x5DC;
	/* A_T0_LVT_RT   : 0x11EE0AA8 [19:10], shift value: 0x4B0 */
	*a_t0_lvt_rt    = ((efuse_val1 & 0x000FFC00) >> 10) + 0x4B0;
	/* A_T0_ULVT_RT  : 0x11EE0AA8 [29:20], shift value: 0x708 */
	*a_t0_ulvt_rt   = ((efuse_val1 & 0x3FF00000) >> 20) + 0x708;
	/* A_shift_error : 0x11EE0AA8 [31] */
	*a_shift_error  = ((efuse_val1 & 0x80000000) >> 31);
	/* efuse_error   : A_shift_error | Reg(0x11EE0AA8) == 0 */
	*efuse_error    = *a_shift_error;

	g_asensor_info.efuse_val1 = efuse_val1;
	g_asensor_info.efuse_val2 = efuse_val2;
	g_asensor_info.efuse_val3 = efuse_val3;
	g_asensor_info.efuse_val3 = efuse_val4;
	g_asensor_info.efuse_val1_addr = 0x11EE0AA8;
	g_asensor_info.efuse_val2_addr = 0x11EE0AAC;
	g_asensor_info.efuse_val3_addr = 0x11EE0B6C;
	g_asensor_info.efuse_val4_addr = 0x11EE05D8;
	g_asensor_info.a_t0_lvt_rt = *a_t0_lvt_rt;
	g_asensor_info.a_t0_ulvt_rt = *a_t0_ulvt_rt;
	g_asensor_info.lvts5_0_y_temperature = ((efuse_val4 & 0xFF000000) >> 24);

	GPUFREQ_LOGD("a_t0_lvt_rt=%08x, a_t0_ulvt_rt=%08x, a_t0_ulvtll_rt=%08x, a_shift_error=%08x",
		*a_t0_lvt_rt, *a_t0_ulvt_rt, *a_t0_ulvtll_rt, *a_shift_error);

	return true;
#else
	GPUFREQ_UNREFERENCED(a_t0_lvt_rt);
	GPUFREQ_UNREFERENCED(a_t0_ulvt_rt);
	GPUFREQ_UNREFERENCED(a_t0_ulvtll_rt);
	GPUFREQ_UNREFERENCED(a_shift_error);
	GPUFREQ_UNREFERENCED(efuse_error);
	return false;
#endif /* GPUFREQ_ASENSOR_ENABLE */
}

//TODO
static void __gpufreq_asensor_read_register(u32 *a_tn_lvt_cnt,
	u32 *a_tn_ulvt_cnt, u32 *a_tn_ulvtll_cnt)
{
#if GPUFREQ_ASENSOR_ENABLE
	u32 aging_data0 = 0, aging_data1 = 0;

	/*
	 * Enable sensor bclk cg
	 * MFG_SENSOR_BCLK_CG 0x13fb_ff98 = 0x0000_0001
	 */
	writel(0x1, g_mfg_top_base + 0xf98);

	/*
	 * Config window setting
	 * CPE_CTRL_MCU_REG_CPEMONCTL 0x13fb_9c00 = 0x0901_031f
	 */
	writel(0x0901031f, g_mfg_cpe_control_base + 0x0);

	/*
	 * Enable CPE
	 * CPE_CTRL_MCU_REG_CEPEN  0x13fb9c04 = 0x0000_ffff
	 */
	writel(0x0000ffff, g_mfg_cpe_control_base + 0x4);

	/* wait 50us */
	udelay(50);

	/*
	 * Readout the data
	 * 0x13FB6008 [15:0]  for a_tn_ulvtll_cnt
	 * 0x13FB6008 [31:16] for a_tn_lvt_cnt
	 * 0x13FB600C [15:0]  for a_tn_ulvt_cnt
	 */
	aging_data0 = readl(g_mfg_cpe_sensor_base + 0x8);
	aging_data1 = readl(g_mfg_cpe_sensor_base + 0xC);

	GPUFREQ_LOGD("aging_data0=0x%08x, aging_data1=0x%08x", aging_data0, aging_data1);

	*a_tn_ulvtll_cnt = (aging_data0 & 0xFFFF);
	*a_tn_lvt_cnt    = (aging_data0 & 0xFFFF0000) >> 16;
	*a_tn_ulvt_cnt   = (aging_data1 & 0xFFFF);

	g_asensor_info.a_tn_lvt_cnt = *a_tn_lvt_cnt;
	g_asensor_info.a_tn_ulvt_cnt = *a_tn_ulvt_cnt;
	g_asensor_info.a_tn_ulvtll_cnt = *a_tn_ulvtll_cnt;

	GPUFREQ_LOGD("a_tn_lvt_cnt=0x%08x, a_tn_ulvt_cnt=0x%08x, a_tn_ulvtll_cnt=0x%08x",
		*a_tn_lvt_cnt, *a_tn_ulvt_cnt, *a_tn_ulvtll_cnt);
#else
	GPUFREQ_UNREFERENCED(a_tn_lvt_cnt);
	GPUFREQ_UNREFERENCED(a_tn_ulvt_cnt);
	GPUFREQ_UNREFERENCED(a_tn_ulvtll_cnt);
#endif /* GPUFREQ_ASENSOR_ENABLE */
}

static unsigned int __gpufreq_get_aging_table_idx(u32 a_t0_lvt_rt, u32 a_t0_ulvt_rt,
	u32 a_t0_ulvtll_rt, u32 a_shift_error, u32 efuse_error, u32 a_tn_lvt_cnt,
	u32 a_tn_ulvt_cnt, u32 a_tn_ulvtll_cnt, unsigned int is_efuse_read_success)
{
	unsigned int aging_table_idx = GPUFREQ_AGING_MOST_AGRRESIVE;

#if GPUFREQ_ASENSOR_ENABLE
	int tj = 0, tj1 = 0, tj2 = 0;
	int adiff = 0, adiff1 = 0, adiff2 = 0, adiff3 = 0;
	unsigned int leakage_power = 0;

	/*
	 * todo: Need to check the API for 2 tj sensor belong to GPU.
	 * Ex:
	 * tj1 = get_immediate_tslvts3_0_wrap() / 1000;
	 * tj2 = get_immediate_tslvts3_1_wrap() / 1000;
	 */

	tj1 = 40;
	tj2 = 40;

	tj = (((MAX(tj1, tj2) - 25) * 50) / 40) + 1;

	adiff1 = a_t0_lvt_rt + tj - a_tn_lvt_cnt;
	adiff2 = a_t0_ulvt_rt + tj - a_tn_ulvt_cnt;
	adiff3 = a_t0_ulvtll_rt + tj - a_tn_ulvtll_cnt;
	adiff = MAX(adiff1, adiff2);
	adiff = MAX(adiff, adiff3);

	/*
	 * todo: Check how to get leackage power
	 * apply volt=0.8V, temp=30C to get leakage information.
	 * For leakage < 16 (mW), DISABLE aging reduction.
	 */
	leakage_power = __gpufreq_get_lkg_pgpu(0);

	g_asensor_info.tj1 = tj1;
	g_asensor_info.tj2 = tj2;
	g_asensor_info.adiff1 = adiff1;
	g_asensor_info.adiff2 = adiff2;
	g_asensor_info.adiff3 = adiff3;
	g_asensor_info.leakage_power = leakage_power;

	GPUFREQ_LOGD("tj1=%d, tj2=%d, tj=%d, adiff1=%d, adiff2=%d, adiff3=%d",
		tj1, tj2, tj, adiff1, adiff2, adiff3);
	GPUFREQ_LOGD("adiff=%d, leakage_power=%d, is_efuse_read_success=%d",
		adiff, leakage_power, is_efuse_read_success);

	if (g_aging_load)
		aging_table_idx = GPUFREQ_AGING_MOST_AGRRESIVE;
	else if (!is_efuse_read_success)
		aging_table_idx = 3;
	else if (MIN(tj1, tj2) < 25)
		aging_table_idx = 3;
	else if (a_shift_error || efuse_error)
		aging_table_idx = 3;
	else if (leakage_power < 20)
		aging_table_idx = 3;
	else if (adiff < GPUFREQ_AGING_GAP_MIN)
		aging_table_idx = 3;
	else if ((adiff >= GPUFREQ_AGING_GAP_MIN)
		&& (adiff < GPUFREQ_AGING_GAP_1))
		aging_table_idx = 0;
	else if ((adiff >= GPUFREQ_AGING_GAP_1)
		&& (adiff < GPUFREQ_AGING_GAP_2))
		aging_table_idx = 1;
	else if ((adiff >= GPUFREQ_AGING_GAP_2)
		&& (adiff < GPUFREQ_AGING_GAP_3))
		aging_table_idx = 2;
	else if (adiff >= GPUFREQ_AGING_GAP_3)
		aging_table_idx = 3;
	else {
		GPUFREQ_LOGW("non of the condition is true for aging_table_idx");
		aging_table_idx = 3;
	}
#else
	GPUFREQ_UNREFERENCED(a_t0_lvt_rt);
	GPUFREQ_UNREFERENCED(a_t0_ulvt_rt);
	GPUFREQ_UNREFERENCED(a_t0_ulvtll_rt);
	GPUFREQ_UNREFERENCED(a_shift_error);
	GPUFREQ_UNREFERENCED(efuse_error);
	GPUFREQ_UNREFERENCED(a_tn_lvt_cnt);
	GPUFREQ_UNREFERENCED(a_tn_ulvt_cnt);
	GPUFREQ_UNREFERENCED(a_tn_ulvtll_cnt);
	GPUFREQ_UNREFERENCED(is_efuse_read_success);
#endif /* GPUFREQ_ASENSOR_ENABLE */

	/* check the MostAgrresive setting */
	aging_table_idx = MAX(aging_table_idx, GPUFREQ_AGING_MOST_AGRRESIVE);

	return aging_table_idx;
}

static void __gpufreq_aging_adjustment(void)
{
	struct gpufreq_adj_info *aging_adj = NULL;
	int adj_num = 0;
	int i;
	unsigned int aging_table_idx = GPUFREQ_AGING_MOST_AGRRESIVE;

#if GPUFREQ_ASENSOR_ENABLE
	u32 a_tn_lvt_cnt = 0, a_tn_ulvt_cnt = 0, a_tn_ulvtll_cnt = 0;
	u32 a_t0_lvt_rt = 0, a_t0_ulvt_rt = 0, a_t0_ulvtll_rt = 0;
	u32 a_shift_error = 0, efuse_error = 0;
	unsigned int is_efuse_read_success = false;

	if (__gpufreq_dvfs_enable()) {
		/* keep volt for A sensor working */
		if (__gpufreq_pause_dvfs()) {
			GPUFREQ_LOGE("fail to pause DVFS for Aging Sensor");
			return;
		}

		/* get aging efuse data */
		is_efuse_read_success = __gpufreq_asensor_read_efuse(
			&a_t0_lvt_rt, &a_t0_ulvt_rt, &a_t0_ulvtll_rt,
			&a_shift_error, &efuse_error);

		/* get aging sensor data */
		__gpufreq_asensor_read_register(&a_tn_lvt_cnt, &a_tn_ulvt_cnt, &a_tn_ulvtll_cnt);

		/* resume DVFS */
		__gpufreq_resume_dvfs();

		/* compute aging_table_idx with aging efuse data and aging sensor data */
		aging_table_idx = __gpufreq_get_aging_table_idx(
			a_t0_lvt_rt, a_t0_ulvt_rt, a_t0_ulvtll_rt, a_shift_error, efuse_error,
			a_tn_lvt_cnt, a_tn_ulvt_cnt, a_tn_ulvtll_cnt, is_efuse_read_success);
	}

	g_asensor_info.aging_table_idx_most_agrresive = GPUFREQ_AGING_MOST_AGRRESIVE;
	g_asensor_info.aging_table_idx_choosed = aging_table_idx;

	GPUFREQ_LOGI("Aging Sensor choose aging table id: %d", aging_table_idx);
#endif /* GPUFREQ_ASENSOR_ENABLE */

	adj_num = g_stack.signed_opp_num;

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
	__gpufreq_apply_adjust(TARGET_STACK, aging_adj, adj_num);

	kfree(aging_adj);
}
//TODO
static void __gpufreq_avs_adjustment(void)
{
#if GPUFREQ_AVS_ENABLE
	u32 val = 0;
	unsigned int temp_volt = 0, temp_freq = 0;
	int i = 0, oppidx = 0;
	int adj_num = AVS_ADJ_NUM;

	/*
	 * Read AVS efuse
	 *
	 * Freq (MHz) | Signedoff Volt (V) | Efuse name | Efuse address
	 * ============================================================
	 * 860        | 0.75               | PTPOD21    | 0x11EE_05D4
	 * 800        | 0.7                | PTPOD22    | 0x11EE_05D8
	 * 590        | 0.65               | PTPOD23    | 0x11EE_05DC
	 * 317        | 0.55               | PTPOD24    | 0x11EE_05E0
	 * 219        | 0.5                | PTPOD25    | 0x11EE_05E4
	 *
	 * the binning result of lowest voltage will be 0.45V from 214MHz/0.5V.
	 */

	for (i = 0; i < adj_num; i++) {
		oppidx = g_avs_adj[i].oppidx;
		val = readl(g_efuse_base + 0x5D4 + (i * 0x4));

		/* if efuse value is not set */
		if (!val)
			continue;

		/* compute Freq from efuse */
		temp_freq = 0;
		temp_freq |= (val & 0x00100000) >> 10; // Get freq[10]  from efuse[20]
		temp_freq |= (val & 0x00000C00) >> 2;  // Get freq[9:8] from efuse[11:10]
		temp_freq |= (val & 0x00000003) << 6;  // Get freq[7:6] from efuse[1:0]
		temp_freq |= (val & 0x000000C0) >> 2;  // Get freq[5:4] from efuse[7:6]
		temp_freq |= (val & 0x000C0000) >> 16; // Get freq[3:2] from efuse[19:18]
		temp_freq |= (val & 0x00003000) >> 12; // Get freq[1:0] from efuse[13:12]
		/* Freq is stored in efuse with MHz unit */
		temp_freq *= 1000;
		/* verify with signoff Freq */
		if (temp_freq != g_stack.signed_table[oppidx].freq) {
			__gpufreq_abort(GPUFREQ_GPU_EXCEPTION,
				"OPP[%02d*]: AVS efuse[%d].freq(%d) != signed-off.freq(%d)",
				oppidx, i, temp_freq, g_stack.signed_table[oppidx].freq);
			return;
		}

		/* compute Volt from efuse */
		temp_volt = 0;
		temp_volt |= (val & 0x0003C000) >> 14; // Get volt[3:0] from efuse[17:14]
		temp_volt |= (val & 0x00000030);       // Get volt[5:4] from efuse[5:4]
		temp_volt |= (val & 0x0000000C) << 4;  // Get volt[7:6] from efuse[3:2]
		/* Volt is stored in efuse with 6.25mV unit */
		temp_volt *= 625;
		/* clamp to signoff Volt */
		if (temp_volt > g_stack.signed_table[oppidx].volt) {
			GPUFREQ_LOGW("OPP[%02d*]: AVS efuse[%d].volt(%d) > signed-off.volt(%d)",
				oppidx, i, temp_volt, g_stack.signed_table[oppidx].volt);
			g_avs_adj[i].volt = g_stack.signed_table[oppidx].volt;
		} else
			g_avs_adj[i].volt = temp_volt;

		GPUFREQ_LOGI("OPP[%02d*]: AVS efuse[%d] freq(%d), volt(%d)",
			oppidx, i, temp_freq, temp_volt);
	}

	/* apply AVS to signed table */
	__gpufreq_apply_adjust(TARGET_STACK, g_avs_adj, adj_num);

	/* interpolate volt of non-sign-off OPP */
	__gpufreq_interpolate_volt();
#endif /* GPUFREQ_AVS_ENABLE */
}

static void __gpufreq_custom_adjustment(void)
{
	struct gpufreq_adj_info *custom_adj;
	int adj_num = 0;

	if (g_mcl50_load) {
		custom_adj = g_mcl50_adj;
		adj_num = MCL50_ADJ_NUM;
		__gpufreq_apply_adjust(TARGET_STACK, custom_adj, adj_num);
		GPUFREQ_LOGI("MCL50 flavor load");
	}
}

static void __gpufreq_segment_adjustment(struct platform_device *pdev)
{
	struct gpufreq_adj_info *segment_adj;
	int adj_num = 0;
	unsigned int efuse_id = 0x0;

	switch (efuse_id) {
	case 0x2:
		segment_adj = g_segment_adj;
		adj_num = SEGMENT_ADJ_NUM;
		__gpufreq_apply_adjust(TARGET_STACK, segment_adj, adj_num);
		break;
	default:
		GPUFREQ_LOGW("unknown efuse id: 0x%x", efuse_id);
		break;
	}

	GPUFREQ_LOGI("efuse_id: 0x%x, adj_num: %d", efuse_id, adj_num);
}

//TODO
static void __gpufreq_init_shader_present(void)
{
	unsigned int segment_id = 0;

	segment_id = g_stack.segment_id;

	switch (segment_id) {
	case MT6895_SEGMENT:
		g_shader_present = GPU_SHADER_PRESENT_6;
		break;
	default:
		g_shader_present = GPU_SHADER_PRESENT_6;
		GPUFREQ_LOGI("invalid segment id: %d", segment_id);
	}
	GPUFREQ_LOGD("segment_id: %d, shader_present: %d", segment_id, g_shader_present);
}

/*
 * 1. init OPP segment range
 * 2. init segment/working OPP table
 * 3. init power measurement
 * 4. init springboard table
 */
static int __gpufreq_init_opp_table(struct platform_device *pdev)
{
	unsigned int segment_id = 0;
	int i = 0, j = 0;
	int ret = GPUFREQ_SUCCESS;

	/* init current GPU/STACK freq and volt set by preloader */
	g_gpu.cur_freq = __gpufreq_get_real_fgpu();
	g_gpu.cur_volt = VCORE_LEVEL_0;
	g_gpu.cur_vsram = VSRAM_LEVEL_0;

	g_stack.cur_freq = __gpufreq_get_real_fstack();
	g_stack.cur_volt = __gpufreq_get_real_vstack();
	g_stack.cur_vsram = VSRAM_LEVEL_0;

	GPUFREQ_LOGI("preloader init [GPU] Freq: %d, Volt: %d [STACK] Freq: %d, Volt: %d, Vsram: %d",
		g_gpu.cur_freq, g_gpu.cur_volt,
		g_stack.cur_freq, g_stack.cur_volt, g_stack.cur_vsram);

	/* init GPU OPP table */
	/* GPU signed table == working table */
	g_gpu.segment_upbound = 0;
	g_gpu.segment_lowbound = SIGNED_OPP_GPU_NUM - 1;
	g_gpu.signed_opp_num = SIGNED_OPP_GPU_NUM;

	g_gpu.max_oppidx = g_gpu.segment_upbound;
	g_gpu.min_oppidx = g_gpu.segment_lowbound;
	g_gpu.opp_num = g_gpu.signed_opp_num;
	g_gpu.signed_table = g_default_gpu;

	GPUFREQ_LOGI("number of GPU OPP: %d, max and min OPP index: [%d, %d]",
		g_gpu.opp_num, g_gpu.max_oppidx, g_gpu.min_oppidx);

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


	/* init STACK OPP table */
	/* init OPP segment range */
	segment_id = g_stack.segment_id;
	if (segment_id == MT6895_SEGMENT)
		g_stack.segment_upbound = 12;
	else if (segment_id == MT6895T_SEGMENT)
		g_stack.segment_upbound = 1;
	else if (segment_id == MT6895TT_SEGMENT)
		g_stack.segment_upbound = 0;
	else
		g_stack.segment_upbound = 0;
	g_stack.segment_lowbound = SIGNED_OPP_STACK_NUM - 1;
	g_stack.signed_opp_num = SIGNED_OPP_STACK_NUM;

	g_stack.max_oppidx = 0;
	g_stack.min_oppidx = g_stack.segment_lowbound - g_stack.segment_upbound;
	g_stack.opp_num = g_stack.min_oppidx + 1;

	GPUFREQ_LOGD("number of signed STACK OPP: %d, upper and lower bound: [%d, %d]",
		g_stack.signed_opp_num, g_stack.segment_upbound, g_stack.segment_lowbound);
	GPUFREQ_LOGI("number of working STACK OPP: %d, max and min OPP index: [%d, %d]",
		g_stack.opp_num, g_stack.max_oppidx, g_stack.min_oppidx);

	g_stack.signed_table = g_default_stack;
	/* apply segment adjustment to STACK signed table */
	__gpufreq_segment_adjustment(pdev);
	/* apply AVS adjustment to STACK signed table */
	__gpufreq_avs_adjustment();
	/* apply aging adjustment to STACK signed table */
	__gpufreq_aging_adjustment();
	/* apply custom adjustment to STACK signed table */
	__gpufreq_custom_adjustment();

	/* after these, signed table is settled down */

	/* init working table, based on signed table */
	g_stack.working_table = kcalloc(g_stack.opp_num,
		sizeof(struct gpufreq_opp_info), GFP_KERNEL);
	if (!g_stack.working_table) {
		GPUFREQ_LOGE("fail to alloc gpufreq_opp_info (ENOMEM)");
		ret = GPUFREQ_ENOMEM;
		goto done;
	}

	for (i = 0; i < g_stack.opp_num; i++) {
		j = i + g_stack.segment_upbound;
		g_stack.working_table[i].freq = g_stack.signed_table[j].freq;
		g_stack.working_table[i].volt = g_stack.signed_table[j].volt;
		g_stack.working_table[i].vsram = g_stack.signed_table[j].vsram;
		g_stack.working_table[i].posdiv = g_stack.signed_table[j].posdiv;
		g_stack.working_table[i].vaging = g_stack.signed_table[j].vaging;
		g_stack.working_table[i].power = g_stack.signed_table[j].power;

		GPUFREQ_LOGD("STACK[%02d] Freq: %d, Volt: %d, Vsram: %d, Vaging: %d",
			i, g_stack.working_table[i].freq, g_stack.working_table[i].volt,
			g_stack.working_table[i].vsram, g_stack.working_table[i].vaging);
	}

#if GPUFREQ_ASENSOR_ENABLE
	/* apply aging volt to working table volt if Asensor works */
	g_aging_enable = true;
#else
	/* apply aging volt to working table volt depending on Aging load */
	g_aging_enable = g_aging_load;
#endif /* GPUFREQ_ASENSOR_ENABLE */
	if (g_aging_enable)
		__gpufreq_apply_aging(true);

	/* set power info to working table */
	__gpufreq_measure_power();

	/* update HW constraint parking volt */
	__gpufreq_update_hw_constraint_volt();

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
	unsigned int efuse_id = 0x0;
	unsigned int segment_id = MT6895_SEGMENT;
	int ret = GPUFREQ_SUCCESS;

#if IS_ENABLED(CONFIG_MTK_DEVINFO)
	struct nvmem_cell *efuse_cell;
	unsigned int *efuse_buf;
	size_t efuse_len;

	efuse_cell = nvmem_cell_get(&pdev->dev, "efuse_segment_cell");
	if (IS_ERR(efuse_cell)) {
		GPUFREQ_LOGE("fail to get efuse_segment_cell (%ld)", PTR_ERR(efuse_cell));
		ret = PTR_ERR(efuse_cell);
		goto done;
	}

	efuse_buf = (unsigned int *)nvmem_cell_read(efuse_cell, &efuse_len);
	nvmem_cell_put(efuse_cell);
	if (IS_ERR(efuse_buf)) {
		GPUFREQ_LOGE("fail to get efuse_buf (%ld)", PTR_ERR(efuse_buf));
		ret = PTR_ERR(efuse_buf);
		goto done;
	}

	efuse_id = (*efuse_buf & 0xFF);
	kfree(efuse_buf);
#else
	efuse_id = 0x0;
#endif /* CONFIG_MTK_DEVINFO */

	switch (efuse_id) {
	case 0x1:
		segment_id = MT6895_SEGMENT;
		break;
	case 0x2:
		segment_id = MT6895T_SEGMENT;
		break;
	case 0x3:
		segment_id = MT6895TT_SEGMENT;
		break;
	default:
		segment_id = MT6895_SEGMENT;
		GPUFREQ_LOGW("unknown efuse id: 0x%x", efuse_id);
		break;
	}

	GPUFREQ_LOGI("efuse_id: 0x%x, segment_id: %d", efuse_id, segment_id);

done:
	g_stack.segment_id = segment_id;

	return ret;

}

//TODO pd_mfg1 name
static int __gpufreq_init_mtcmos(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("pdev=0x%x", pdev);

	g_mtcmos = kzalloc(sizeof(struct gpufreq_mtcmos_info), GFP_KERNEL);
	if (!g_mtcmos) {
		GPUFREQ_LOGE("fail to alloc gpufreq_mtcmos_info (ENOMEM)");
		ret = GPUFREQ_ENOMEM;
		goto done;
	}

	g_mtcmos->mfg1_dev = dev_pm_domain_attach_by_name(dev, "pd_mfg1");
	if (IS_ERR_OR_NULL(g_mtcmos->mfg1_dev)) {
		ret = g_mtcmos->mfg1_dev ? PTR_ERR(g_mtcmos->mfg1_dev) : GPUFREQ_ENODEV;
		__gpufreq_abort(GPUFREQ_CCF_EXCEPTION, "fail to get mfg1_dev (%ld)", ret);
		goto done;
	}
	dev_pm_syscore_device(g_mtcmos->mfg1_dev, true);

#if !GPUFREQ_PDCv2_ENABLE
	g_mtcmos->mfg2_dev = dev_pm_domain_attach_by_name(dev, "pd_mfg2");
	if (IS_ERR_OR_NULL(g_mtcmos->mfg2_dev)) {
		ret = g_mtcmos->mfg2_dev ? PTR_ERR(g_mtcmos->mfg2_dev) : GPUFREQ_ENODEV;
		__gpufreq_abort(GPUFREQ_CCF_EXCEPTION, "fail to get mfg2_dev (%ld)", ret);
		goto done;
	}
	dev_pm_syscore_device(g_mtcmos->mfg2_dev, true);

	g_mtcmos->mfg3_dev = dev_pm_domain_attach_by_name(dev, "pd_mfg3");
	if (IS_ERR_OR_NULL(g_mtcmos->mfg3_dev)) {
		ret = g_mtcmos->mfg3_dev ? PTR_ERR(g_mtcmos->mfg3_dev) : GPUFREQ_ENODEV;
		__gpufreq_abort(GPUFREQ_CCF_EXCEPTION, "fail to get mfg3_dev (%ld)", ret);
		goto done;
	}
	dev_pm_syscore_device(g_mtcmos->mfg3_dev, true);

	g_mtcmos->mfg4_dev = dev_pm_domain_attach_by_name(dev, "pd_mfg4");
	if (IS_ERR_OR_NULL(g_mtcmos->mfg4_dev)) {
		ret = g_mtcmos->mfg4_dev ? PTR_ERR(g_mtcmos->mfg4_dev) : GPUFREQ_ENODEV;
		__gpufreq_abort(GPUFREQ_CCF_EXCEPTION, "fail to get mfg4_dev (%ld)", ret);
		goto done;
	}
	dev_pm_syscore_device(g_mtcmos->mfg4_dev, true);

	g_mtcmos->mfg5_dev = dev_pm_domain_attach_by_name(dev, "pd_mfg5");
	if (IS_ERR_OR_NULL(g_mtcmos->mfg5_dev)) {
		ret = g_mtcmos->mfg5_dev ? PTR_ERR(g_mtcmos->mfg5_dev) : GPUFREQ_ENODEV;
		__gpufreq_abort(GPUFREQ_CCF_EXCEPTION, "fail to get mfg5_dev (%ld)", ret);
		goto done;
	}
	dev_pm_syscore_device(g_mtcmos->mfg5_dev, true);

	g_mtcmos->mfg6_dev = dev_pm_domain_attach_by_name(dev, "pd_mfg6");
	if (IS_ERR_OR_NULL(g_mtcmos->mfg6_dev)) {
		ret = g_mtcmos->mfg6_dev ? PTR_ERR(g_mtcmos->mfg6_dev) : GPUFREQ_ENODEV;
		__gpufreq_abort(GPUFREQ_CCF_EXCEPTION, "fail to get mfg6_dev (%ld)", ret);
		goto done;
	}
	dev_pm_syscore_device(g_mtcmos->mfg6_dev, true);

	g_mtcmos->mfg7_dev = dev_pm_domain_attach_by_name(dev, "pd_mfg7");
	if (IS_ERR_OR_NULL(g_mtcmos->mfg7_dev)) {
		ret = g_mtcmos->mfg7_dev ? PTR_ERR(g_mtcmos->mfg7_dev) : GPUFREQ_ENODEV;
		__gpufreq_abort(GPUFREQ_CCF_EXCEPTION, "fail to get mfg7_dev (%ld)", ret);
		goto done;
	}
	dev_pm_syscore_device(g_mtcmos->mfg7_dev, true);

	g_mtcmos->mfg8_dev = dev_pm_domain_attach_by_name(dev, "pd_mfg8");
	if (IS_ERR_OR_NULL(g_mtcmos->mfg8_dev)) {
		ret = g_mtcmos->mfg8_dev ? PTR_ERR(g_mtcmos->mfg8_dev) : GPUFREQ_ENODEV;
		__gpufreq_abort(GPUFREQ_CCF_EXCEPTION, "fail to get mfg8_dev (%ld)", ret);
		goto done;
	}
	dev_pm_syscore_device(g_mtcmos->mfg8_dev, true);

	g_mtcmos->mfg9_dev = dev_pm_domain_attach_by_name(dev, "pd_mfg9");
	if (IS_ERR_OR_NULL(g_mtcmos->mfg9_dev)) {
		ret = g_mtcmos->mfg9_dev ? PTR_ERR(g_mtcmos->mfg9_dev) : GPUFREQ_ENODEV;
		__gpufreq_abort(GPUFREQ_CCF_EXCEPTION, "fail to get mfg9_dev (%ld)", ret);
		goto done;
	}
	dev_pm_syscore_device(g_mtcmos->mfg9_dev, true);

	g_mtcmos->mfg10_dev = dev_pm_domain_attach_by_name(dev, "pd_mfg10");
	if (IS_ERR_OR_NULL(g_mtcmos->mfg10_dev)) {
		ret = g_mtcmos->mfg10_dev ? PTR_ERR(g_mtcmos->mfg10_dev) : GPUFREQ_ENODEV;
		__gpufreq_abort(GPUFREQ_CCF_EXCEPTION, "fail to get mfg10_dev (%ld)", ret);
		goto done;
	}
	dev_pm_syscore_device(g_mtcmos->mfg10_dev, true);

	g_mtcmos->mfg11_dev = dev_pm_domain_attach_by_name(dev, "pd_mfg11");
	if (IS_ERR_OR_NULL(g_mtcmos->mfg11_dev)) {
		ret = g_mtcmos->mfg11_dev ? PTR_ERR(g_mtcmos->mfg11_dev) : GPUFREQ_ENODEV;
		__gpufreq_abort(GPUFREQ_CCF_EXCEPTION, "fail to get mfg11_dev (%ld)", ret);
		goto done;
	}
	dev_pm_syscore_device(g_mtcmos->mfg11_dev, true);

	g_mtcmos->mfg12_dev = dev_pm_domain_attach_by_name(dev, "pd_mfg12");
	if (IS_ERR_OR_NULL(g_mtcmos->mfg12_dev)) {
		ret = g_mtcmos->mfg12_dev ? PTR_ERR(g_mtcmos->mfg12_dev) : GPUFREQ_ENODEV;
		__gpufreq_abort(GPUFREQ_CCF_EXCEPTION, "fail to get mfg12_dev (%ld)", ret);
		goto done;
	}
	dev_pm_syscore_device(g_mtcmos->mfg12_dev, true);

#endif /* GPUFREQ_PDCv2_ENABLE */

done:
	GPUFREQ_TRACE_END();

	return ret;
}

static int __gpufreq_init_clk(struct platform_device *pdev)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("pdev=0x%x", pdev);

	g_clk = kzalloc(sizeof(struct gpufreq_clk_info), GFP_KERNEL);
	if (!g_clk) {
		GPUFREQ_LOGE("fail to alloc gpufreq_clk_info (ENOMEM)");
		ret = GPUFREQ_ENOMEM;
		goto done;
	}

	g_clk->clk_mux = devm_clk_get(&pdev->dev, "clk_mux");
	if (IS_ERR(g_clk->clk_mux)) {
		ret = PTR_ERR(g_clk->clk_mux);
		__gpufreq_abort(GPUFREQ_CCF_EXCEPTION, "fail to get clk_mux (%ld)", ret);
		goto done;
	}

	g_clk->clk_main_parent = devm_clk_get(&pdev->dev, "clk_main_parent");
	if (IS_ERR(g_clk->clk_main_parent)) {
		ret = PTR_ERR(g_clk->clk_main_parent);
		__gpufreq_abort(GPUFREQ_CCF_EXCEPTION, "fail to get clk_main_parent (%ld)", ret);
		goto done;
	}

	g_clk->clk_sub_parent = devm_clk_get(&pdev->dev, "clk_sub_parent");
	if (IS_ERR(g_clk->clk_sub_parent)) {
		ret = PTR_ERR(g_clk->clk_sub_parent);
		__gpufreq_abort(GPUFREQ_CCF_EXCEPTION, "fail to get clk_sub_parent (%ld)", ret);
		goto done;
	}

	g_clk->clk_sc_mux = devm_clk_get(&pdev->dev, "clk_sc_mux");
	if (IS_ERR(g_clk->clk_sc_mux)) {
		ret = PTR_ERR(g_clk->clk_sc_mux);
		__gpufreq_abort(GPUFREQ_CCF_EXCEPTION, "fail to get clk_sc_mux (%ld)", ret);
		goto done;
	}

	g_clk->clk_sc_main_parent = devm_clk_get(&pdev->dev, "clk_sc_main_parent");
	if (IS_ERR(g_clk->clk_sc_main_parent)) {
		ret = PTR_ERR(g_clk->clk_sc_main_parent);
		__gpufreq_abort(GPUFREQ_CCF_EXCEPTION,
			"fail to get clk_sc_main_parent (%ld)", ret);
		goto done;
	}

	g_clk->clk_sc_sub_parent = devm_clk_get(&pdev->dev, "clk_sc_sub_parent");
	if (IS_ERR(g_clk->clk_sc_sub_parent)) {
		ret = PTR_ERR(g_clk->clk_sc_sub_parent);
		__gpufreq_abort(GPUFREQ_CCF_EXCEPTION,
			"fail to get clk_sc_sub_parent (%ld)", ret);
		goto done;
	}

#if GPUFREQ_CG_CONTROL_ENABLE
	g_clk->subsys_mfg_cg = devm_clk_get(&pdev->dev, "subsys_mfg_cg");
	if (IS_ERR(g_clk->subsys_mfg_cg)) {
		ret = PTR_ERR(g_clk->subsys_mfg_cg);
		__gpufreq_abort(GPUFREQ_CCF_EXCEPTION, "fail to get subsys_mfg_cg (%ld)", ret);
		goto done;
	}
#endif /* GPUFREQ_CG_CONTROL_ENABLE */

done:
	GPUFREQ_TRACE_END();

	return ret;
}

static int __gpufreq_init_pmic(struct platform_device *pdev)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("pdev=0x%x", pdev);

	g_pmic = kzalloc(sizeof(struct gpufreq_pmic_info), GFP_KERNEL);
	if (!g_pmic) {
		GPUFREQ_LOGE("fail to alloc gpufreq_pmic_info (ENOMEM)");
		ret = GPUFREQ_ENOMEM;
		goto done;
	}

	/* VGPU is co-buck with VCORE, so use VCORE_BUCK to get Volt */
	g_pmic->reg_vcore = regulator_get_optional(&pdev->dev, "_vcore");
	if (IS_ERR(g_pmic->reg_vcore)) {
		ret = PTR_ERR(g_pmic->reg_vcore);
		__gpufreq_abort(GPUFREQ_PMIC_EXCEPTION, "fail to get VCORE (%ld)", ret);
		goto done;
	}

#if GPUFREQ_VCORE_DVFS_ENABLE
	/* VGPU is co-buck with VCORE, so use DVFSRC to set Volt */
	g_pmic->reg_dvfsrc = regulator_get_optional(&pdev->dev, "_dvfsrc");
	if (IS_ERR(g_pmic->reg_dvfsrc)) {
		ret = PTR_ERR(g_pmic->reg_dvfsrc);
		__gpufreq_abort(GPUFREQ_PMIC_EXCEPTION, "fail to get DVFSRC (%ld)", ret);
		goto done;
	}
#endif /* GPUFREQ_VCORE_DVFS_ENABLE */

	g_pmic->reg_vstack = regulator_get_optional(&pdev->dev, "_vstack");
	if (IS_ERR(g_pmic->reg_vstack)) {
		ret = PTR_ERR(g_pmic->reg_vstack);
		__gpufreq_abort(GPUFREQ_PMIC_EXCEPTION, "fail to get VSATCK (%ld)", ret);
		goto done;
	}

	/* VSRAM is co-buck and controlled by SRAMRC, but use regulator to get Volt */
	g_pmic->reg_vsram = regulator_get_optional(&pdev->dev, "_vsram");
	if (IS_ERR(g_pmic->reg_vsram)) {
		ret = PTR_ERR(g_pmic->reg_vsram);
		__gpufreq_abort(GPUFREQ_PMIC_EXCEPTION, "fail to get VSRAM (%ld)", ret);
		goto done;
	}

done:
	GPUFREQ_TRACE_END();

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
	of_property_read_u32(of_wrapper, "gpueb-support", &g_gpueb_support);

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

	/* 0x13FA0C00 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfgsc_pll");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource MFGSC_PLL");
		goto done;
	}
	g_mfgsc_pll_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_mfgsc_pll_base)) {
		GPUFREQ_LOGE("fail to ioremap MFGSC_PLL: 0x%llx", res->start);
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

	/* 0x1C001000 */
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

	/* 0x10000000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "topckgen");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource TOPCKGEN");
		goto done;
	}
	g_topckgen_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_topckgen_base)) {
		GPUFREQ_LOGE("fail to ioremap TOPCKGEN: 0x%llx", res->start);
		goto done;
	}

	/* 0x1021C000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "nth_emicfg_reg");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource NTH_EMICFG_REG");
		goto done;
	}
	g_nth_emicfg_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_nth_emicfg_base)) {
		GPUFREQ_LOGE("fail to ioremap NTH_EMICFG_REG: 0x%llx", res->start);
		goto done;
	}

	/* 0x1021E000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "sth_emicfg_reg");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource STH_EMICFG_REG");
		goto done;
	}
	g_sth_emicfg_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_sth_emicfg_base)) {
		GPUFREQ_LOGE("fail to ioremap STH_EMICFG_REG: 0x%llx", res->start);
		goto done;
	}

	/* 0x10270000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "nth_emicfg_ao_mem_reg");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource NTH_EMICFG_AO_MEM_REG");
		goto done;
	}
	g_nth_emicfg_ao_mem_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_nth_emicfg_ao_mem_base)) {
		GPUFREQ_LOGE("fail to ioremap NTH_EMICFG_AO_MEM_REG: 0x%llx", res->start);
		goto done;
	}

	/* 0x1030E000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "sth_emicfg_ao_mem_reg");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource STH_EMICFG_AO_MEM_REG");
		goto done;
	}
	g_sth_emicfg_ao_mem_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_sth_emicfg_ao_mem_base)) {
		GPUFREQ_LOGE("fail to ioremap STH_EMICFG_AO_MEM_REG: 0x%llx", res->start);
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
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "nth_emi_ao_debug_ctrl");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource NTH_EMI_AO_DEBUG_CTRL");
		goto done;
	}
	g_nth_emi_ao_debug_ctrl = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_nth_emi_ao_debug_ctrl)) {
		GPUFREQ_LOGE("fail to ioremap NTH_EMI_AO_DEBUG_CTRL: 0x%llx", res->start);
		goto done;
	}

#if GPUFREQ_AVS_ENABLE || GPUFREQ_ASENSOR_ENABLE
	/* 0x11EE0000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "efuse");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource EFUSE");
		goto done;
	}
	g_efuse_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_efuse_base)) {
		GPUFREQ_LOGE("fail to ioremap EFUSE: 0x%llx", res->start);
		goto done;
	}
#endif /* GPUFREQ_AVS_ENABLE || GPUFREQ_ASENSOR_ENABLE */

#if GPUFREQ_ASENSOR_ENABLE
	/* 0x13FB9C00 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_cpe_control");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource MFG_CPE_CONTROL");
		goto done;
	}
	g_mfg_cpe_control_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_mfg_cpe_control_base)) {
		GPUFREQ_LOGE("fail to ioremap MFG_CPE_CONTROL: 0x%llx", res->start);
		goto done;
	}

	/* 0x13FB6000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_cpe_sensor");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource MFG_CPE_SENSOR");
		goto done;
	}
	g_mfg_cpe_sensor_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_mfg_cpe_sensor_base)) {
		GPUFREQ_LOGE("fail to ioremap MFG_CPE_SENSOR: 0x%llx", res->start);
		goto done;
	}
#endif /* GPUFREQ_ASENSOR_ENABLE */

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
		__gpufreq_dump_bringup_status(pdev);
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

	/* init shader present */
	__gpufreq_init_shader_present();

	/* default enable GPM 1.0, before power control */
	g_gpm_enable = true;

	/* power on to init first OPP index */
	ret = __gpufreq_power_control(POWER_ON);
	if (unlikely(ret < 0)) {
		GPUFREQ_LOGE("fail to control power state: %d (%d)", POWER_ON, ret);
		goto done;
	}

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
	ret = gpuppm_init(TARGET_STACK, g_gpueb_support, GPUFREQ_SAFE_VLOGIC);
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to init gpuppm (%d)", ret);
		goto done;
	}

	/* init gpu dfd*/
	//ret = gpudfd_init(pdev);
	//if (unlikely(ret)) {
	//	GPUFREQ_LOGE("fail to init gpudfd (%d)", ret);
	//	goto done;
	//}

	GPUFREQ_LOGI("gpufreq platform driver probe done");

done:
	return ret;
}

/* API: gpufreq driver remove */
static int __gpufreq_pdrv_remove(struct platform_device *pdev)
{
#if !GPUFREQ_PDCv2_ENABLE
	dev_pm_domain_detach(g_mtcmos->mfg12_dev, true);
	dev_pm_domain_detach(g_mtcmos->mfg11_dev, true);
	dev_pm_domain_detach(g_mtcmos->mfg10_dev, true);
	dev_pm_domain_detach(g_mtcmos->mfg9_dev, true);
	dev_pm_domain_detach(g_mtcmos->mfg8_dev, true);
	dev_pm_domain_detach(g_mtcmos->mfg7_dev, true);
	dev_pm_domain_detach(g_mtcmos->mfg6_dev, true);
	dev_pm_domain_detach(g_mtcmos->mfg5_dev, true);
	dev_pm_domain_detach(g_mtcmos->mfg4_dev, true);
	dev_pm_domain_detach(g_mtcmos->mfg3_dev, true);
	dev_pm_domain_detach(g_mtcmos->mfg2_dev, true);
#endif /* GPUFREQ_PDCv2_ENABLE */
	dev_pm_domain_detach(g_mtcmos->mfg1_dev, true);

	kfree(g_gpu.working_table);
	kfree(g_stack.working_table);
	kfree(g_clk);
	kfree(g_pmic);
	kfree(g_mtcmos);

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
