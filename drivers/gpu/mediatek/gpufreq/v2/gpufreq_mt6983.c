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
#include <linux/err.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/printk.h>
#include <linux/pm_runtime.h>

#include <gpufreq_v2.h>
#include <gpufreq_debug.h>
#include <gpuppm.h>
#include <gpufreq_common.h>
#include <gpufreq_mt6983.h>

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
#if IS_ENABLED(CONFIG_MTK_FREQ_HOPPING)
#include <mtk_freqhopping_drv.h>
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
static void __gpufreq_dump_bringup_status(void);
static void __gpufreq_measure_power(enum gpufreq_target target);
static void __iomem *__gpufreq_of_ioremap(const char *node_name, int idx);
static void __gpufreq_set_regulator_mode(enum gpufreq_target target, unsigned int mode);
static int __gpufreq_pause_dvfs(unsigned int keep_freq, unsigned int keep_volt);
static void __gpufreq_resume_dvfs(void);
static void __gpufreq_interpolate_volt(enum gpufreq_target target);
static void __gpufreq_apply_aging(enum gpufreq_target target, unsigned int apply_aging);
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
static int __gpufreq_switch_clksrc(enum gpufreq_clk_src clksrc);
static unsigned int __gpufreq_calculate_pcw(unsigned int freq, enum gpufreq_postdiv posdiv_power);
static unsigned int __gpufreq_settle_time_vstack(unsigned int direction, int deltaV);
static void __gpufreq_delsel_control(unsigned int direction);
static void __gpufreq_dvfs_timing_control(unsigned int direction);
/* get function */
static unsigned int __gpufreq_get_fmeter_fgpu(void);
static unsigned int __gpufreq_get_fmeter_fstack(void);
static unsigned int __gpufreq_get_real_fgpu(void);
static unsigned int __gpufreq_get_real_fstack(void);
static unsigned int __gpufreq_get_real_vgpu(void);
static unsigned int __gpufreq_get_real_vstack(void);
static unsigned int __gpufreq_get_real_vsram(void);
static void __gpufreq_get_hw_constraint_fgpu(
	unsigned int fstack, int *oppidx_gpu, unsigned int *fgpu);
static void __gpufreq_get_hw_constraint_fstack(
	unsigned int fgpu, int *oppidx_stack, unsigned int *fstack);
static unsigned int __gpufreq_get_hw_constraint_vstack(
	unsigned int volt_old, unsigned int volt_new);
static enum gpufreq_postdiv __gpufreq_get_real_posdiv_gpu(void);
static enum gpufreq_postdiv __gpufreq_get_real_posdiv_stack(void);
static enum gpufreq_postdiv __gpufreq_get_posdiv_by_fgpu(unsigned int freq);
static enum gpufreq_postdiv __gpufreq_get_posdiv_by_fstack(unsigned int freq);
/* aging sensor function */
static void __gpufreq_asensor_read_register(u32 *lvt, u32 *ulvt, u32 *ulvtll);
static unsigned int __gpufreq_asensor_read_efuse(u32 *a_t0_lvt_rt, u32 *a_t0_ulvt_rt,
	u32 *a_t0_ulvtll_rt, u32 *a_shift_error, u32 *efuse_error);
static unsigned int __gpufreq_get_aging_table_idx(u32 a_t0_lvt_rt, u32 a_t0_ulvt_rt,
	u32 a_t0_ulvtll_rt, u32 a_shift_error, u32 efuse_error, u32 a_tn_lvt_cnt,
	u32 a_tn_ulvt_cnt, u32 a_tn_ulvtll_cnt, unsigned int is_efuse_read_success);
static struct gpufreq_asensor_info __gpufreq_get_asensor_info(void);
/* power control function */
static int __gpufreq_cg_control(enum gpufreq_power_state power);
static int __gpufreq_mtcmos_control(enum gpufreq_power_state power);
static int __gpufreq_buck_control(enum gpufreq_power_state power);
static void __gpufreq_aoc_control(enum gpufreq_power_state power);
static void __gpufreq_hw_apm_control(void);
static void __gpufreq_hw_dcm_control(void);
static void __gpufreq_acp_control(void);
static void __gpufreq_gpm_control(void);
/* init function */
static void __gpufreq_init_shader_present(void);
static void __gpufreq_segment_adjustment(struct platform_device *pdev);
static void __gpufreq_avs_adjustment(void);
static void __gpufreq_aging_adjustment(void);
static int __gpufreq_init_opp_idx(void);
static int __gpufreq_init_opp_table(struct platform_device *pdev);
static int __gpufreq_init_segment_id(struct platform_device *pdev);
static int __gpufreq_init_clk(struct platform_device *pdev);
static int __gpufreq_init_pmic(struct platform_device *pdev);
static int __gpufreq_init_platform_info(struct platform_device *pdev);
static int __gpufreq_pdrv_probe(struct platform_device *pdev);
static int __gpufreq_mtcmos_pdrv_probe(struct platform_device *pdev);
static int __gpufreq_mtcmos_pdrv_remove(struct platform_device *pdev);
static int __gpufreq_mfg2_probe(struct platform_device *pdev);
static int __gpufreq_mfg3_probe(struct platform_device *pdev);
static int __gpufreq_mfg4_probe(struct platform_device *pdev);
static int __gpufreq_mfg5_probe(struct platform_device *pdev);
static int __gpufreq_mfg6_probe(struct platform_device *pdev);
static int __gpufreq_mfg_remove(struct platform_device *pdev);

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
	.remove = NULL,
	.driver = {
		.name = "gpufreq",
		.owner = THIS_MODULE,
		.of_match_table = g_gpufreq_of_match,
	},
};

static const struct gpufreq_mfg_fp mfg2_fp = {
	.probe = __gpufreq_mfg2_probe,
	.remove = __gpufreq_mfg_remove,
};
static const struct gpufreq_mfg_fp mfg3_fp = {
	.probe = __gpufreq_mfg3_probe,
	.remove = __gpufreq_mfg_remove,
};
static const struct gpufreq_mfg_fp mfg4_fp = {
	.probe = __gpufreq_mfg4_probe,
	.remove = __gpufreq_mfg_remove,
};
static const struct gpufreq_mfg_fp mfg5_fp = {
	.probe = __gpufreq_mfg5_probe,
	.remove = __gpufreq_mfg_remove,
};
static const struct gpufreq_mfg_fp mfg6_fp = {
	.probe = __gpufreq_mfg6_probe,
	.remove = __gpufreq_mfg_remove,
};
static const struct of_device_id g_gpufreq_mtcmos_of_match[] = {
	{
		.compatible = "mediatek,mt6893-mfg2",
		.data = &mfg2_fp,
	}, {
		.compatible = "mediatek,mt6893-mfg3",
		.data = &mfg3_fp,
	}, {
		.compatible = "mediatek,mt6893-mfg4",
		.data = &mfg4_fp,
	}, {
		.compatible = "mediatek,mt6893-mfg5",
		.data = &mfg5_fp,
	}, {
		.compatible = "mediatek,mt6893-mfg6",
		.data = &mfg6_fp,
	}, {
		/* sentinel */
	}
};
static struct platform_driver g_gpufreq_mtcmos_pdrv = {
	.probe = __gpufreq_mtcmos_pdrv_probe,
	.remove = __gpufreq_mtcmos_pdrv_remove,
	.driver = {
		.name = "gpufreq-mtcmos",
		.of_match_table = g_gpufreq_mtcmos_of_match,
	},
};

static void __iomem *g_mfg_pll_base;
static void __iomem *g_mfg_scpll_base;
static void __iomem *g_mfg_rpc_base;
static void __iomem *g_g3d_base;
static void __iomem *g_sleep;
static void __iomem *g_infracfg_base;
static void __iomem *g_infra_bpi_bsi_slv0;
static void __iomem *g_infra_peri_debug1;
static void __iomem *g_infra_peri_debug2;
static void __iomem *g_infra_peri_debug3;
static void __iomem *g_infra_peri_debug4;
static void __iomem *g_avs_efuse_base;
static void __iomem *g_mfg_cpe_control_base;
static void __iomem *g_mfg_cpe_sensor_base;
static void __iomem *g_cpe_0p65v_rt_blow_base;
static struct gpufreq_pmic_info *g_pmic;
static struct gpufreq_clk_info *g_clk;
static struct gpufreq_mtcmos_info *g_mtcmos;
static struct gpufreq_status g_gpu;
static struct gpufreq_status g_stack;
static struct gpufreq_asensor_info g_asensor_info;
static unsigned int g_shader_present;
static unsigned int g_stress_test_enable;
static unsigned int g_aging_enable;
static unsigned int g_aging_load;
static unsigned int g_gpm_enable;
static unsigned int g_gpueb_support;
static enum gpufreq_dvfs_state g_dvfs_state;
static DEFINE_MUTEX(gpufreq_lock);

static struct gpufreq_platform_fp platform_fp = {
	.bringup = __gpufreq_bringup,
	.power_ctrl_enable = __gpufreq_power_ctrl_enable,
	.get_dvfs_state = __gpufreq_get_dvfs_state,
	.get_shader_present = __gpufreq_get_shader_present,
	.get_cur_fgpu = __gpufreq_get_cur_fgpu,
	.get_max_fgpu = __gpufreq_get_max_fgpu,
	.get_min_fgpu = __gpufreq_get_min_fgpu,
	.get_cur_vgpu = __gpufreq_get_cur_vgpu,
	.get_max_vgpu = __gpufreq_get_max_vgpu,
	.get_min_vgpu = __gpufreq_get_min_vgpu,
	.get_cur_vsram_gpu = __gpufreq_get_cur_vsram_gpu,
	.get_cur_pgpu = __gpufreq_get_cur_pgpu,
	.get_max_pgpu = __gpufreq_get_max_pgpu,
	.get_min_pgpu = __gpufreq_get_min_pgpu,
	.get_cur_idx_gpu = __gpufreq_get_cur_idx_gpu,
	.get_max_idx_gpu = __gpufreq_get_max_idx_gpu,
	.get_min_idx_gpu = __gpufreq_get_min_idx_gpu,
	.get_opp_num_gpu = __gpufreq_get_opp_num_gpu,
	.get_signed_opp_num_gpu = __gpufreq_get_signed_opp_num_gpu,
	.get_working_table_gpu = __gpufreq_get_working_table_gpu,
	.get_signed_table_gpu = __gpufreq_get_signed_table_gpu,
	.get_debug_opp_info_gpu = __gpufreq_get_debug_opp_info_gpu,
	.get_fgpu_by_idx = __gpufreq_get_fgpu_by_idx,
	.get_vgpu_by_idx = __gpufreq_get_vgpu_by_idx,
	.get_pgpu_by_idx = __gpufreq_get_pgpu_by_idx,
	.get_idx_by_fgpu = __gpufreq_get_idx_by_fgpu,
	.get_idx_by_vgpu = __gpufreq_get_idx_by_vgpu,
	.get_idx_by_pgpu = __gpufreq_get_idx_by_pgpu,
	.get_vsram_by_vgpu = __gpufreq_get_vsram_by_vgpu,
	.get_vsram_by_vstack = __gpufreq_get_vsram_by_vstack,
	.get_lkg_pgpu = __gpufreq_get_lkg_pgpu,
	.get_dyn_pgpu = __gpufreq_get_dyn_pgpu,
	.power_control = __gpufreq_power_control,
	.generic_commit_gpu = __gpufreq_generic_commit_gpu,
	.fix_target_oppidx_gpu = __gpufreq_fix_target_oppidx_gpu,
	.fix_custom_freq_volt_gpu = __gpufreq_fix_custom_freq_volt_gpu,
	.get_cur_fstack = __gpufreq_get_cur_fstack,
	.get_max_fstack = __gpufreq_get_max_fstack,
	.get_min_fstack = __gpufreq_get_min_fstack,
	.get_cur_vstack = __gpufreq_get_cur_vstack,
	.get_max_vstack = __gpufreq_get_max_vstack,
	.get_min_vstack = __gpufreq_get_min_vstack,
	.get_cur_vsram_stack = __gpufreq_get_cur_vsram_stack,
	.get_cur_pstack = __gpufreq_get_cur_pstack,
	.get_max_pstack = __gpufreq_get_max_pstack,
	.get_min_pstack = __gpufreq_get_min_pstack,
	.get_cur_idx_stack = __gpufreq_get_cur_idx_stack,
	.get_max_idx_stack = __gpufreq_get_max_idx_stack,
	.get_min_idx_stack = __gpufreq_get_min_idx_stack,
	.get_opp_num_stack = __gpufreq_get_opp_num_stack,
	.get_signed_opp_num_stack = __gpufreq_get_signed_opp_num_stack,
	.get_working_table_stack = __gpufreq_get_working_table_stack,
	.get_signed_table_stack = __gpufreq_get_signed_table_stack,
	.get_debug_opp_info_stack = __gpufreq_get_debug_opp_info_stack,
	.get_fstack_by_idx = __gpufreq_get_fstack_by_idx,
	.get_vstack_by_idx = __gpufreq_get_vstack_by_idx,
	.get_pstack_by_idx = __gpufreq_get_pstack_by_idx,
	.get_idx_by_fstack = __gpufreq_get_idx_by_fstack,
	.get_idx_by_vstack = __gpufreq_get_idx_by_vstack,
	.get_idx_by_pstack = __gpufreq_get_idx_by_pstack,
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

/* API: get max Freq of GPU */
unsigned int __gpufreq_get_max_fgpu(void)
{
	return g_gpu.working_table[g_gpu.max_oppidx].freq;
}

/* API: get max Freq of STACK */
unsigned int __gpufreq_get_max_fstack(void)
{
	return g_stack.working_table[g_stack.max_oppidx].freq;
}

/* API: get min Freq of GPU */
unsigned int __gpufreq_get_min_fgpu(void)
{
	return g_gpu.working_table[g_gpu.min_oppidx].freq;
}

/* API: get min Freq of STACK */
unsigned int __gpufreq_get_min_fstack(void)
{
	return g_stack.working_table[g_stack.min_oppidx].freq;
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

/* API: get max Volt of GPU */
unsigned int __gpufreq_get_max_vgpu(void)
{
	return g_gpu.working_table[g_gpu.max_oppidx].volt;
}

/* API: get max Volt of STACK */
unsigned int __gpufreq_get_max_vstack(void)
{
	return g_stack.working_table[g_stack.max_oppidx].volt;
}

/* API: get min Volt of GPU */
unsigned int __gpufreq_get_min_vgpu(void)
{
	return g_gpu.working_table[g_gpu.min_oppidx].volt;
}

/* API: get min Volt of STACK */
unsigned int __gpufreq_get_min_vstack(void)
{
	return g_stack.working_table[g_stack.min_oppidx].volt;
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

/* API: get the segment OPP index of GPU with the highest performance */
int __gpufreq_get_max_idx_gpu(void)
{
	return g_gpu.max_oppidx;
}

/* API: get the segment OPP index of STACK with the highest performance */
int __gpufreq_get_max_idx_stack(void)
{
	return g_stack.max_oppidx;
}

/* API: get the segment OPP index of GPU with the lowest performance */
int __gpufreq_get_min_idx_gpu(void)
{
	return g_gpu.min_oppidx;
}

/* API: get the segment OPP index of STACK with the lowest performance */
int __gpufreq_get_min_idx_stack(void)
{
	return g_stack.min_oppidx;
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
	int ret = GPUFREQ_SUCCESS;

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
	opp_info.gpm_enable = g_gpm_enable;
	mutex_unlock(&gpufreq_lock);

	/* power on at here to read Reg and avoid increasing power count */
	ret = __gpufreq_power_control(POWER_ON);
	if (unlikely(ret < 0)) {
		GPUFREQ_LOGE("fail to control power state: %d (%d)", POWER_ON, ret);
		goto done;
	}

	mutex_lock(&gpufreq_lock);
	opp_info.fmeter_freq = __gpufreq_get_fmeter_fgpu();
	opp_info.con1_freq = __gpufreq_get_real_fgpu();
	opp_info.regulator_volt = __gpufreq_get_real_vgpu();
	opp_info.regulator_vsram = __gpufreq_get_real_vsram();
	mutex_unlock(&gpufreq_lock);

	ret = __gpufreq_power_control(POWER_OFF);
	if (unlikely(ret < 0)) {
		GPUFREQ_LOGE("fail to control power state: %d (%d)", POWER_OFF, ret);
		goto done;
	}

done:
	return opp_info;
}

/* API: get debug info of STACK for Proc show */
struct gpufreq_debug_opp_info __gpufreq_get_debug_opp_info_stack(void)
{
	struct gpufreq_debug_opp_info opp_info = {};
	int ret = GPUFREQ_SUCCESS;

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
	opp_info.gpm_enable = g_gpm_enable;
	mutex_unlock(&gpufreq_lock);

	/* power on at here to read Reg and avoid increasing power count */
	ret = __gpufreq_power_control(POWER_ON);
	if (unlikely(ret < 0)) {
		GPUFREQ_LOGE("fail to control power state: %d (%d)", POWER_ON, ret);
		goto done;
	}

	mutex_lock(&gpufreq_lock);
	opp_info.fmeter_freq = __gpufreq_get_fmeter_fstack();
	opp_info.con1_freq = __gpufreq_get_real_fstack();
	opp_info.regulator_volt = __gpufreq_get_real_vstack();
	opp_info.regulator_vsram = __gpufreq_get_real_vsram();
	mutex_unlock(&gpufreq_lock);

	ret = __gpufreq_power_control(POWER_OFF);
	if (unlikely(ret < 0)) {
		GPUFREQ_LOGE("fail to control power state: %d (%d)", POWER_OFF, ret);
		goto done;
	}

done:
	return opp_info;
}

/* API: get asensor info for Proc show */
static struct gpufreq_asensor_info __gpufreq_get_asensor_info(void)
{
	struct gpufreq_asensor_info asensor_info = {};

#if GPUFREQ_ASENSOR_ENABLE
	mutex_lock(&gpufreq_lock);
	asensor_info.aging_table_idx_choosed = g_asensor_info.aging_table_idx_choosed;
	asensor_info.aging_table_idx_most_agrresive = g_asensor_info.aging_table_idx_most_agrresive;
	asensor_info.efuse_val1_addr = g_asensor_info.efuse_val1_addr;
	asensor_info.efuse_val1 = g_asensor_info.efuse_val1;
	asensor_info.efuse_val2_addr = g_asensor_info.efuse_val2_addr;
	asensor_info.efuse_val2 = g_asensor_info.efuse_val2;
	asensor_info.efuse_val3_addr = g_asensor_info.efuse_val3_addr;
	asensor_info.efuse_val3 = g_asensor_info.efuse_val3;
	asensor_info.a_t0_lvt_rt = g_asensor_info.a_t0_lvt_rt;
	asensor_info.a_t0_ulvt_rt = g_asensor_info.a_t0_ulvt_rt;
	asensor_info.a_t0_ulvtll_rt = g_asensor_info.a_t0_ulvtll_rt;
	asensor_info.a_tn_lvt_cnt = g_asensor_info.a_tn_lvt_cnt;
	asensor_info.a_tn_ulvt_cnt = g_asensor_info.a_tn_ulvt_cnt;
	asensor_info.a_tn_ulvtll_cnt = g_asensor_info.a_tn_ulvtll_cnt;
	asensor_info.tj1 = g_asensor_info.tj1;
	asensor_info.tj2 = g_asensor_info.tj2;
	asensor_info.adiff1 = g_asensor_info.adiff1;
	asensor_info.adiff2 = g_asensor_info.adiff2;
	asensor_info.adiff3 = g_asensor_info.adiff3;
	asensor_info.leakage_power = g_asensor_info.leakage_power;
	mutex_unlock(&gpufreq_lock);
#endif
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

/* API: get Volt of GPU via OPP index */
unsigned int __gpufreq_get_vgpu_by_idx(int oppidx)
{
	if (oppidx >= 0 && oppidx < g_gpu.opp_num)
		return g_gpu.working_table[oppidx].volt;
	else
		return 0;
}

/* API: get Volt of STACK via OPP index */
unsigned int __gpufreq_get_vstack_by_idx(int oppidx)
{
	if (oppidx >= 0 && oppidx < g_stack.opp_num)
		return g_stack.working_table[oppidx].volt;
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
	int idx = 0;

	/* find the smallest index that satisfy given freq */
	for (idx = g_gpu.min_oppidx; idx >= 0; idx--) {
		if (g_gpu.working_table[idx].freq >= freq)
			break;
	}

	/* found */
	if (idx >= 0)
		return idx;
	/* not found */
	else
		return 0;
}

/* API: get working OPP index of STACK via Freq */
int __gpufreq_get_idx_by_fstack(unsigned int freq)
{
	int idx = 0;

	/* find the smallest index that satisfy given freq */
	for (idx = g_stack.min_oppidx; idx >= 0; idx--) {
		if (g_stack.working_table[idx].freq >= freq)
			break;
	}

	/* found */
	if (idx >= 0)
		return idx;
	/* not found */
	else
		return 0;
}

/* API: get working OPP index of GPU via Volt */
int __gpufreq_get_idx_by_vgpu(unsigned int volt)
{
	int idx = 0;

	/* find the smallest index that satisfy given volt */
	for (idx = g_gpu.min_oppidx; idx >= 0; idx--) {
		if (g_gpu.working_table[idx].volt >= volt)
			break;
	}

	/* found */
	if (idx >= 0)
		return idx;
	/* not found */
	else
		return 0;
}

/* API: get working OPP index of STACK via Volt */
int __gpufreq_get_idx_by_vstack(unsigned int volt)
{
	int idx = 0;

	/* find the smallest index that satisfy given volt */
	for (idx = g_stack.min_oppidx; idx >= 0; idx--) {
		if (g_stack.working_table[idx].volt >= volt)
			break;
	}

	/* found */
	if (idx >= 0)
		return idx;
	/* not found */
	else
		return 0;
}

/* API: get working OPP index of GPU via Power */
int __gpufreq_get_idx_by_pgpu(unsigned int power)
{
	int idx = 0;

	/* find the smallest index that satisfy given power */
	for (idx = g_gpu.min_oppidx; idx >= 0; idx--) {
		if (g_gpu.working_table[idx].power >= power)
			break;
	}

	/* found */
	if (idx >= 0)
		return idx;
	/* not found */
	else
		return 0;
}

/* API: get working OPP index of STACK via Power */
int __gpufreq_get_idx_by_pstack(unsigned int power)
{
	int idx = 0;

	/* find the smallest index that satisfy given power */
	for (idx = g_stack.min_oppidx; idx >= 0; idx--) {
		if (g_stack.working_table[idx].power >= power)
			break;
	}

	/* found */
	if (idx >= 0)
		return idx;
	/* not found */
	else
		return 0;
}

/* API: get Volt of SRAM via volt of GPU */
unsigned int __gpufreq_get_vsram_by_vgpu(unsigned int volt)
{
	GPUFREQ_UNREFERENCED(volt);

	return VSRAM_LEVEL_0;
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

	if (power == POWER_ON) {
		__gpufreq_footprint_vgpu(GPUFREQ_VGPU_STEP_1);

		/* control Buck */
		ret = __gpufreq_buck_control(POWER_ON);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to control Buck: On (%d)", ret);
			ret = GPUFREQ_EINVAL;
			goto done_unlock;
		}
		__gpufreq_footprint_vgpu(GPUFREQ_VGPU_STEP_2);

		/* control AOC after MFG_0 on */
		__gpufreq_aoc_control(power);
		__gpufreq_footprint_vgpu(GPUFREQ_VGPU_STEP_3);

		/* control MTCMOS */
		ret = __gpufreq_mtcmos_control(POWER_ON);
		if (unlikely(ret < 0)) {
			GPUFREQ_LOGE("fail to control MTCMOS: On (%d)", ret);
			ret = GPUFREQ_EINVAL;
			goto done_unlock;
		}
		__gpufreq_footprint_vgpu(GPUFREQ_VGPU_STEP_4);

		/* control ACP after MFG_1 on */
		__gpufreq_acp_control();
		__gpufreq_footprint_vgpu(GPUFREQ_VGPU_STEP_5);

		/* control CG */
		ret = __gpufreq_cg_control(POWER_ON);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to control CG: On (%d)", ret);
			ret = GPUFREQ_EINVAL;
			goto done_unlock;
		}
		__gpufreq_footprint_vgpu(GPUFREQ_VGPU_STEP_6);

		/* control HWDCM */
		__gpufreq_hw_dcm_control();
		__gpufreq_footprint_vgpu(GPUFREQ_VGPU_STEP_7);

		/* control GPM 1.0 */
		if (g_gpm_enable)
			__gpufreq_gpm_control();
		__gpufreq_footprint_vgpu(GPUFREQ_VGPU_STEP_8);

		if (g_stack.power_count == 1)
			g_dvfs_state &= ~DVFS_POWEROFF;
	} else {
		__gpufreq_footprint_vgpu(GPUFREQ_VGPU_STEP_9);

		if (g_stack.power_count == 0)
			g_dvfs_state |= DVFS_POWEROFF;

		/* control CG */
		ret = __gpufreq_cg_control(POWER_OFF);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to control CG: Off (%d)", ret);
			ret = GPUFREQ_EINVAL;
			goto done_unlock;
		}
		__gpufreq_footprint_vgpu(GPUFREQ_VGPU_STEP_A);

		/* control MTCMOS */
		ret = __gpufreq_mtcmos_control(POWER_OFF);
		if (unlikely(ret < 0)) {
			GPUFREQ_LOGE("fail to control MTCMOS: Off (%d)", ret);
			ret = GPUFREQ_EINVAL;
			goto done_unlock;
		}
		__gpufreq_footprint_vgpu(GPUFREQ_VGPU_STEP_B);

		/* control AOC before MFG_0 off */
		__gpufreq_aoc_control(power);
		__gpufreq_footprint_vgpu(GPUFREQ_VGPU_STEP_C);

		/* control Buck */
		ret = __gpufreq_buck_control(POWER_OFF);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to control Buck: Off (%d)", ret);
			ret = GPUFREQ_EINVAL;
			goto done_unlock;
		}
		__gpufreq_footprint_vgpu(GPUFREQ_VGPU_STEP_D);
	}

	/* return power count if successfully control power */
	ret = g_stack.power_count;

done_unlock:
	mutex_unlock(&gpufreq_lock);

	GPUFREQ_TRACE_END();

	return ret;
}

#if defined(GPUFREQ_TODO_SRAMRC_POWER_CONTROL)
/* API: contorl SRAMRC when power on/off */
int __gpufreq_sramrc_control(enum gpufreq_power_state power)
{
	int ret = GPUFREQ_SUCCESS;
	int safe_oppidx = 0;
	unsigned int safe_vstack = 0;

	GPUFREQ_TRACE_START("power=%d", power);

	/* power on */
	if (power == POWER_ON) {

		/* get Vsafe and update limit table when power on */
		safe_vstack = sramrc_get_gpu_bound_level(*need_ack);
		safe_oppidx = __gpufreq_get_idx_by_vstack(safe_vstack);
		gpuppm_set_limit_stack(LIMIT_SRAMRC, GPUPPM_DEFAULT_IDX, safe_oppidx);

		/* set Vsafe */
		ret = gpufreq_commit(TARGET_DEFAULT, safe_oppidx);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to commit STACK OPP index: %d (%d)",
				safe_oppidx, ret);
		}

		/* notify SRAMRC that GPU online */
		sramrc_mask_passive_gpu();
	/* power off */
	} else {
		/* notify SRAMRC that GPU offline */
		sramrc_unmask_passive_gpu();
	}

done:
	GPUFREQ_TRACE_END();

	return ret;
}
#endif

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
		GPUFREQ_LOGI("unavailable DVFS state (0x%x)", g_dvfs_state);
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
	__gpufreq_get_hw_constraint_fstack(cur_fgpu, &target_oppidx_stack, &target_fstack);
	target_vstack = working_stack[target_oppidx_stack].volt;
	target_vsram_stack = working_stack[target_oppidx_stack].vsram;

	GPUFREQ_LOGD("begin to commit GPU OPP index: (%d->%d), STACK OPP index: (%d->%d)",
		cur_oppidx_gpu, target_oppidx_gpu, cur_oppidx_stack, target_oppidx_stack);

	/* todo: GED log buffer (gpufreq_pr_logbuf) */

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
		GPUFREQ_LOGI("unavailable DVFS state (0x%x)", g_dvfs_state);
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
	__gpufreq_get_hw_constraint_fgpu(cur_fstack, &target_oppidx_gpu, &target_fgpu);
	target_vgpu = working_gpu[target_oppidx_gpu].volt;
	target_vsram_gpu = working_gpu[target_oppidx_gpu].vsram;

	GPUFREQ_LOGD("begin to commit STACK OPP index: (%d->%d), GPU OPP index: (%d->%d)",
		cur_oppidx_stack, target_oppidx_stack, cur_oppidx_gpu, target_oppidx_gpu);

	/* todo: GED log buffer (gpufreq_pr_logbuf) */

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
	unsigned int min_oppidx = g_stack.min_oppidx;
	int ret = GPUFREQ_SUCCESS;

	ret = __gpufreq_power_control(POWER_ON);
	if (unlikely(ret < 0)) {
		GPUFREQ_LOGE("fail to control power state: %d (%d)", POWER_ON, ret);
		goto done;
	}

	if (oppidx == -1) {
		ret = __gpufreq_generic_commit_stack(min_oppidx, DVFS_DEBUG_KEEP);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to commit STACK OPP index: %d (%d)",
				min_oppidx, ret);
		}
		__gpufreq_set_dvfs_state(false, DVFS_DEBUG_KEEP);
	} else if (oppidx >= 0 && oppidx < opp_num) {
		__gpufreq_set_dvfs_state(true, DVFS_DEBUG_KEEP);
		ret = __gpufreq_generic_commit_stack(oppidx, DVFS_DEBUG_KEEP);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to commit STACK OPP index: %d (%d)",
				oppidx, ret);
			__gpufreq_set_dvfs_state(false, DVFS_DEBUG_KEEP);
		}
	} else {
		GPUFREQ_LOGE("invalid fixed OPP index: %d", oppidx);
		ret = GPUFREQ_EINVAL;
	}

	ret = __gpufreq_power_control(POWER_OFF);
	if (unlikely(ret < 0)) {
		GPUFREQ_LOGE("fail to control power state: %d (%d)", POWER_OFF, ret);
		goto done;
	}

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
	struct gpufreq_opp_info *working_table = g_stack.working_table;
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
	max_freq = working_table[max_oppidx].freq;
	min_freq = working_table[min_oppidx].freq;
	max_volt = working_table[max_oppidx].volt;
	min_volt = working_table[min_oppidx].volt;

	if (freq == 0 && volt == 0) {
		ret = __gpufreq_generic_commit_stack(min_oppidx, DVFS_DEBUG_KEEP);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to commit STACK OPP index: %d (%d)",
				min_oppidx, ret);
		}
		__gpufreq_set_dvfs_state(false, DVFS_DEBUG_KEEP);
	} else if (freq > max_freq || freq < min_freq) {
		GPUFREQ_LOGE("invalid fixed Freq: %d\n", freq);
		ret = GPUFREQ_EINVAL;
	} else if (volt > max_volt || volt < min_volt) {
		GPUFREQ_LOGE("invalid fixed Volt: %d\n", volt);
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

	ret = __gpufreq_power_control(POWER_OFF);
	if (unlikely(ret < 0)) {
		GPUFREQ_LOGE("fail to control power state: %d (%d)", POWER_OFF, ret);
		goto done;
	}

done:
	return ret;
}

void __gpufreq_set_timestamp(void)
{
	/* write 1 into 0x13fb_f130 bit 0 to enable timestamp register */
	/* timestamp will be used by clGetEventProfilingInfo */
	writel(0x00000001, g_g3d_base + 0x130);
}

void __gpufreq_check_bus_idle(void)
{
	u32 val = 0;

	/* MFG_QCHANNEL_CON 0x13FBF0B4 [1:0] MFG_ACTIVE_SEL = 0x1 */
	writel(0x00000001, g_g3d_base + 0xB4);

	/* MFG_DEBUG_SEL 0x13FBF170 [7:0] MFG_DEBUG_TOP_SEL = 0x3 */
	writel(0x00000003, g_g3d_base + 0x170);

	/*
	 * polling MFG_DEBUG_TOP 0x13FBF178 [0]
	 * 0x1: bus idle
	 * 0x0: bus non-idle
	 */
	do {
		val = readl(g_g3d_base + 0x178);
	} while ((val & 0x1) != 0x1);
}

void __gpufreq_dump_infra_status(void)
{
	GPUFREQ_LOGI("== [GPUFREQ INFRA STATUS] ==");
	GPUFREQ_LOGI("GPU[%d] Freq: %d, Vgpu: %d, Vsram: %d",
		g_gpu.cur_oppidx, g_gpu.cur_freq,
		g_gpu.cur_volt, g_gpu.cur_vsram);
	GPUFREQ_LOGI("STACK[%d] Freq: %d, Vgpu: %d, Vsram: %d",
		g_stack.cur_oppidx, g_stack.cur_freq,
		g_stack.cur_volt, g_stack.cur_vsram);

	/* 0x1020E000 */
	if (g_infracfg_base) {
		GPUFREQ_LOGI("infra status (0x%x): 0x%08x",
			0x1020E810, readl(g_infracfg_base + 0x810));

		GPUFREQ_LOGI("infra status (0x%x): 0x%08x",
			0x1020E814, readl(g_infracfg_base + 0x814));
	}

	/* 0x1021E000 */
	if (g_infra_bpi_bsi_slv0) {
		GPUFREQ_LOGI("infra status (0x%x): 0x%08x",
			0x1021E230, readl(g_infra_bpi_bsi_slv0 + 0x230));

		GPUFREQ_LOGI("infra status (0x%x): 0x%08x",
			0x1021E234, readl(g_infra_bpi_bsi_slv0 + 0x234));
	}

	/* 0x10023000 */
	if (g_infra_peri_debug1) {
		GPUFREQ_LOGI("infra status (0x%x): 0x%08x",
			0x10023000, readl(g_infra_peri_debug1 + 0x000));

		GPUFREQ_LOGI("infra status (0x%x): 0x%08x",
			0x10023440, readl(g_infra_peri_debug1 + 0x440));

		GPUFREQ_LOGI("infra status (0x%x): 0x%08x",
			0x10023444, readl(g_infra_peri_debug1 + 0x444));
	}

	/* 0x10025000 */
	if (g_infra_peri_debug2) {
		GPUFREQ_LOGI("infra status (0x%x): 0x%08x",
			0x10025000, readl(g_infra_peri_debug2 + 0x000));

		GPUFREQ_LOGI("infra status (0x%x): 0x%08x",
			0x1002542C, readl(g_infra_peri_debug2 + 0x42C));
	}

	/* 0x1002B000 */
	if (g_infra_peri_debug3) {
		GPUFREQ_LOGI("infra status (0x%x): 0x%08x",
			0x1002B000, readl(g_infra_peri_debug3 + 0x000));
	}

	/* 0x1002E000 */
	if (g_infra_peri_debug4) {
		GPUFREQ_LOGI("infra status (0x%x): 0x%08x",
			0x1002E000, readl(g_infra_peri_debug4 + 0x000));
	}

	/* 0x10006000 */
	if (g_sleep) {
		GPUFREQ_LOGI("pwr status (0x%x): 0x%08x 0x%08x 0x%08x 0x%08x",
			0x10006000 + 0x308, readl(g_sleep + 0x308),
			readl(g_sleep + 0x30C), readl(g_sleep + 0x310),
			readl(g_sleep + 0x314));

		GPUFREQ_LOGI("pwr status (0x%x) :0x%08x 0x%08x 0x%08x",
			0x10006000 + 0x318, readl(g_sleep + 0x318),
			readl(g_sleep + 0x31C), readl(g_sleep + 0x320));

		GPUFREQ_LOGI("pwr status (0x%x): 0x%08x",
			0x10006000 + 0x16C, readl(g_sleep + 0x16C));

		GPUFREQ_LOGI("pwr status (0x%x): 0x%08x",
			0x10006000 + 0x170, readl(g_sleep + 0x170));
	}
}

/* API: get working OPP index of STACK limited by BATTERY_OC via given level */
int __gpufreq_get_batt_oc_idx(int batt_oc_level)
{
#if (GPUFREQ_BATT_OC_ENABLE && IS_ENABLED(CONFIG_MTK_BATTERY_OC_POWER_THROTTLING))
	if (batt_oc_level == BATTERY_OC_LEVEL_1)
		return GPUFREQ_BATT_OC_IDX - g_stack.segment_upbound;
	else
		return GPUPPM_RESET_IDX;
#else
	GPUFREQ_UNREFERENCED(batt_oc_level);

	return GPUPPM_KEEP_IDX;
#endif /* GPUFREQ_BATT_OC_ENABLE */
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
#endif /* GPUFREQ_BATT_PERCENT_ENABLE */
}

/* API: get working OPP index of STACK limited by LOW_BATTERY via given level */
int __gpufreq_get_low_batt_idx(int low_batt_level)
{
#if (GPUFREQ_LOW_BATT_ENABLE && IS_ENABLED(CONFIG_MTK_LOW_BATTERY_POWER_THROTTLING))
	if (low_batt_level == LOW_BATTERY_LEVEL_2)
		return GPUFREQ_LOW_BATT_IDX - g_stack.segment_upbound;
	else
		return GPUPPM_RESET_IDX;
#else
	GPUFREQ_UNREFERENCED(low_batt_level);

	return GPUPPM_KEEP_IDX;
#endif /* GPUFREQ_LOW_BATT_ENABLE */
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
		__gpufreq_apply_aging(TARGET_STACK, mode);
		g_aging_enable = mode;

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
		GPUFREQ_LOGI("unavailable DVFS state (0x%x)", g_dvfs_state);
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
	__gpufreq_get_hw_constraint_fgpu(cur_fstack, &target_oppidx_gpu, &target_fgpu);
	target_vgpu = working_gpu[target_oppidx_gpu].volt;
	target_vsram_gpu = working_gpu[target_oppidx_gpu].vsram;

	GPUFREQ_LOGD("begin to commit STACK Freq: (%d->%d), Volt: (%d->%d), " \
		"GPU Freq: (%d->%d), Volt: (%d->%d)",
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

done_unlock:
	mutex_unlock(&gpufreq_lock);

	GPUFREQ_TRACE_END();

	return ret;
}

/*
 * API: set AUTO_MODE or PWM_MODE to VGPU
 * REGULATOR_MODE_FAST: PWM mode
 * REGULATOR_MODE_NORMAL: Auto mode
 */
static void __gpufreq_set_regulator_mode(enum gpufreq_target target, unsigned int mode)
{
	int ret = GPUFREQ_SUCCESS;

	if (target == TARGET_STACK) {
		if (regulator_is_enabled(g_pmic->reg_vstack)) {
			ret = regulator_set_mode(g_pmic->reg_vstack, mode);
			if (unlikely(ret))
				GPUFREQ_LOGE("fail to set STACK regulator mode: %d (%d)",
					mode, ret);
			else
				GPUFREQ_LOGD("set STACK regulator mode: %d, (%d: PWM, %d: AUTO)",
					mode, REGULATOR_MODE_NORMAL,
					REGULATOR_MODE_FAST);
		}
	} else {
		if (regulator_is_enabled(g_pmic->reg_vgpu)) {
			ret = regulator_set_mode(g_pmic->reg_vgpu, mode);
			if (unlikely(ret))
				GPUFREQ_LOGE("fail to set GPU regulator mode: %d (%d)",
					mode, ret);
			else
				GPUFREQ_LOGD("set GPU regulator mode: %d, (%d: PWM, %d: AUTO)",
					mode, REGULATOR_MODE_NORMAL,
					REGULATOR_MODE_FAST);
		}
	}
}

static int __gpufreq_switch_clksrc(enum gpufreq_clk_src clksrc)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("clksrc=%d", clksrc);

	ret = clk_prepare_enable(g_clk->clk_mux);
	if (unlikely(ret)) {
		__gpufreq_abort(GPUFREQ_FREQ_EXCEPTION,
			"fail to enable clk_mux(TOP_MUX_MFG) (%d)", ret);
		goto done;
	}

	if (clksrc == CLOCK_MAIN) {
		ret = clk_set_parent(g_clk->clk_mux, g_clk->clk_main_parent);
		if (unlikely(ret)) {
			__gpufreq_abort(GPUFREQ_FREQ_EXCEPTION,
				"fail to switch to CLOCK_MAIN (%d)", ret);
			goto done;
		}
	} else if (clksrc == CLOCK_SUB) {
		ret = clk_set_parent(g_clk->clk_mux, g_clk->clk_sub_parent);
		if (unlikely(ret)) {
			__gpufreq_abort(GPUFREQ_FREQ_EXCEPTION,
				"fail to switch to CLOCK_SUB (%d)", ret);
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
static unsigned int __gpufreq_calculate_pcw(unsigned int freq, enum gpufreq_postdiv postdiv)
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
		pcw = (((freq / TO_MHZ_HEAD * (1 << postdiv)) << DDS_SHIFT)
			/ MFGPLL_FIN + ROUNDING_VALUE) / TO_MHZ_TAIL;
	} else {
		GPUFREQ_LOGE("out of range Freq: %d (EINVAL)", freq);
	}

	return pcw;
}

static enum gpufreq_postdiv __gpufreq_get_real_posdiv_gpu(void)
{
	unsigned int mfgpll = 0;
	enum gpufreq_postdiv postdiv = POSDIV_POWER_1;

	mfgpll = readl(MFGPLL_GPU_CON1);

	postdiv = (mfgpll & (0x7 << POSDIV_SHIFT)) >> POSDIV_SHIFT;

	return postdiv;
}

static enum gpufreq_postdiv __gpufreq_get_real_posdiv_stack(void)
{
	unsigned int mfgpll = 0;
	enum gpufreq_postdiv postdiv = POSDIV_POWER_1;

	mfgpll = readl(MFGPLL_STACK_CON1);

	postdiv = (mfgpll & (0x7 << POSDIV_SHIFT)) >> POSDIV_SHIFT;

	return postdiv;
}

static enum gpufreq_postdiv __gpufreq_get_posdiv_by_fgpu(unsigned int freq)
{
	struct gpufreq_opp_info *signed_table = g_gpu.signed_table;
	int i = 0;

	for (i = 0; i < g_gpu.signed_opp_num; i++) {
		if (signed_table[i].freq <= freq)
			return signed_table[i].postdiv;
	}

	GPUFREQ_LOGE("fail to find post divder of Freq: %d", freq);

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

static enum gpufreq_postdiv __gpufreq_get_posdiv_by_fstack(unsigned int freq)
{
	struct gpufreq_opp_info *signed_table = g_stack.signed_table;
	int i = 0;

	for (i = 0; i < g_stack.signed_opp_num; i++) {
		if (signed_table[i].freq <= freq)
			return signed_table[i].postdiv;
	}

	GPUFREQ_LOGE("fail to find post divder of Freq: %d", freq);

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
	enum gpufreq_postdiv cur_posdiv = POSDIV_POWER_1;
	enum gpufreq_postdiv target_posdiv = POSDIV_POWER_1;
	unsigned int pcw = 0;
	unsigned int pll = 0;
	unsigned int parking = false;
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("freq_old=%d, freq_new=%d", freq_old, freq_new);

	GPUFREQ_LOGD("begin to scale Fgpu: (%d->%d)", freq_old, freq_new);

	cur_posdiv = __gpufreq_get_real_posdiv_gpu();
	target_posdiv = __gpufreq_get_posdiv_by_fgpu(freq_new);
	pcw = __gpufreq_calculate_pcw(freq_new, target_posdiv);
	if (!pcw) {
		GPUFREQ_LOGE("invalid pcw: %d", pcw);
		goto done;
	}
	pll = (0x80000000) | (target_posdiv << POSDIV_SHIFT) | pcw;

#if IS_ENABLED(CONFIG_MTK_FREQ_HOPPING)
	if (target_posdiv != cur_posdiv)
		parking = true;
	else
		parking = false;
#else
	/* force parking if FHCTL isn't ready */
	parking = true;
#endif /* CONFIG_MTK_FREQ_HOPPING */

	if (parking) {
		ret = __gpufreq_switch_clksrc(CLOCK_SUB);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to switch sub clock source (%d)", ret);
			goto done;
		}
		/*
		 * MFGPLL_GPU_CON1[31:31] = MFGPLL_SDM_PCW_CHG
		 * MFGPLL_GPU_CON1[26:24] = MFGPLL_POSDIV
		 * MFGPLL_GPU_CON1[21:0]  = MFGPLL_SDM_PCW (pcw)
		 */
		writel(pll, MFGPLL_GPU_CON1);
		/* PLL spec */
		udelay(20);

		ret = __gpufreq_switch_clksrc(CLOCK_MAIN);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to switch main clock source (%d)", ret);
			goto done;
		}
	} else {
#if IS_ENABLED(CONFIG_MTK_FREQ_HOPPING)
		ret = mt_dfs_general_pll(MFGPLL_FH_PLL, pcw);
		if (unlikely(ret)) {
			__gpufreq_abort(GPUFREQ_FREQ_EXCEPTION,
				"fail to hopping pcw: 0x%x (%d)", pcw, ret);
			goto done;
		}
#endif /* CONFIG_MTK_FREQ_HOPPING */
	}

	g_gpu.cur_freq = __gpufreq_get_real_fgpu();
	if (unlikely(g_gpu.cur_freq != freq_new))
		__gpufreq_abort(GPUFREQ_FREQ_EXCEPTION,
			"inconsistent scaled Fgpu, cur_freq: %d, target_freq: %d",
			g_gpu.cur_freq, freq_new);

	GPUFREQ_LOGD("Fgpu: %d, pcw: 0x%x, pll: 0x%08x, parking: %d",
		g_gpu.cur_freq, pcw, pll, parking);

	/* todo: GED log buffer (gpufreq_pr_logbuf) */

done:
	GPUFREQ_TRACE_END();

	return ret;
}

/* API: scale Freq of STACK via CON1 Reg or FHCTL */
static int __gpufreq_freq_scale_stack(unsigned int freq_old, unsigned int freq_new)
{
	enum gpufreq_postdiv cur_posdiv = POSDIV_POWER_1;
	enum gpufreq_postdiv target_posdiv = POSDIV_POWER_1;
	unsigned int pcw = 0;
	unsigned int pll = 0;
	unsigned int parking = false;
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("freq_old=%d, freq_new=%d", freq_old, freq_new);

	GPUFREQ_LOGD("begin to scale Fstack: (%d->%d)", freq_old, freq_new);

	cur_posdiv = __gpufreq_get_real_posdiv_stack();
	target_posdiv = __gpufreq_get_posdiv_by_fstack(freq_new);
	pcw = __gpufreq_calculate_pcw(freq_new, target_posdiv);
	if (!pcw) {
		GPUFREQ_LOGE("invalid pcw: %d", pcw);
		goto done;
	}
	pll = (0x80000000) | (target_posdiv << POSDIV_SHIFT) | pcw;

#if IS_ENABLED(CONFIG_MTK_FREQ_HOPPING)
	if (target_posdiv != cur_posdiv)
		parking = true;
	else
		parking = false;
#else
	/* force parking if FHCTL isn't ready */
	parking = true;
#endif /* CONFIG_MTK_FREQ_HOPPING */

	if (parking) {
		ret = __gpufreq_switch_clksrc(CLOCK_SUB);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to switch sub clock source (%d)", ret);
			goto done;
		}
		/*
		 * MFGPLL_STACK_CON1[31:31] = MFGPLL_SDM_PCW_CHG
		 * MFGPLL_STACK_CON1[26:24] = MFGPLL_POSDIV
		 * MFGPLL_STACK_CON1[21:0]  = MFGPLL_SDM_PCW (pcw)
		 */
		writel(pll, MFGPLL_STACK_CON1);
		/* PLL spec */
		udelay(20);

		ret = __gpufreq_switch_clksrc(CLOCK_MAIN);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to switch main clock source (%d)", ret);
			goto done;
		}
	} else {
#if IS_ENABLED(CONFIG_MTK_FREQ_HOPPING)
		ret = mt_dfs_general_pll(MFGPLL_FH_PLL, pcw);
		if (unlikely(ret)) {
			__gpufreq_abort(GPUFREQ_FREQ_EXCEPTION,
				"fail to hopping pcw: 0x%x (%d)", pcw, ret);
			goto done;
		}
#endif /* CONFIG_MTK_FREQ_HOPPING */
	}

	g_stack.cur_freq = __gpufreq_get_real_fstack();
	if (unlikely(g_stack.cur_freq != freq_new))
		__gpufreq_abort(GPUFREQ_FREQ_EXCEPTION,
			"inconsistent scaled Fstack, cur_freq: %d, target_freq: %d",
			g_stack.cur_freq, freq_new);

	GPUFREQ_LOGD("Fstack: %d, PCW: 0x%x, PLL: 0x%08x, parking: %d",
		g_stack.cur_freq, pcw, pll, parking);

	/* todo: GED log buffer (gpufreq_pr_logbuf) */

done:
	GPUFREQ_TRACE_END();

	return ret;
}

static unsigned int __gpufreq_settle_time_vstack(unsigned int direction, int deltaV)
{
	/* [MT6315][VGPU]
	 * DVFS Rising : delta(V) / 12.5mV + 4us + 5us
	 * DVFS Falling: delta(V) / 5mV + 4us + 5us
	 */
	unsigned int t_settle = 0;

	if (direction) {
		/* rising 12.5mv/us*/
		t_settle = deltaV / (125 * 10) + 9;
	} else {
		/* falling 5mv/us*/
		t_settle = deltaV / (5 * 100) + 9;
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
	ret = regulator_set_voltage(g_pmic->reg_vgpu, volt_new * 10, INT_MAX);
	if (unlikely(ret)) {
		__gpufreq_abort(GPUFREQ_FREQ_EXCEPTION, "fail to set regulator VGPU (%d)", ret);
		goto done;
	}

	g_gpu.cur_volt = __gpufreq_get_real_vgpu();
	g_gpu.cur_vsram = vsram_new;

	/* todo: GED log buffer (gpufreq_pr_logbuf) */

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
	unsigned volt_park = 0;
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("volt_old=%d, volt_new=%d, vsram_old=%d, vsram_new=%d",
		volt_old, volt_new, vsram_old, vsram_new);

	GPUFREQ_LOGD("begin to scale Vstack: (%d->%d), Vsram_stack: (%d->%d)",
		volt_old, volt_new, vsram_old, vsram_new);

	/* volt scaling up */
	if (volt_new > volt_old) {
		while (volt_new != volt_old) {
			t_settle_volt =  __gpufreq_settle_time_vstack(true, (volt_new - volt_old));
			/* find parking volt and control registers because of HW constraint */
			volt_park = __gpufreq_get_hw_constraint_vstack(volt_old, volt_new);

			ret = regulator_set_voltage(g_pmic->reg_vstack,
				volt_park * 10, VSTACK_MAX_VOLT * 10 + 125);
			if (unlikely(ret)) {
				__gpufreq_abort(GPUFREQ_FREQ_EXCEPTION,
					"fail to set regulator VSTACK (%d)", ret);
				goto done;
			}
			udelay(t_settle_volt);

			volt_old = volt_park;
		}
	/* volt scaling down */
	} else if (volt_new < volt_old) {
		while (volt_new != volt_old) {
			t_settle_volt = __gpufreq_settle_time_vstack(false, (volt_old - volt_new));
			/* find parking volt and control registers because of HW constraint */
			volt_park = __gpufreq_get_hw_constraint_vstack(volt_old, volt_new);

			ret = regulator_set_voltage(g_pmic->reg_vstack,
				volt_park * 10, VSTACK_MAX_VOLT * 10 + 125);
			if (unlikely(ret)) {
				__gpufreq_abort(GPUFREQ_FREQ_EXCEPTION,
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
		__gpufreq_abort(GPUFREQ_FREQ_EXCEPTION,
			"inconsistent scaled Vstack, cur_volt: %d, target_volt: %d",
			g_stack.cur_volt, volt_new);

	/* todo: GED log buffer (gpufreq_pr_logbuf) */

	GPUFREQ_LOGD("Vstack: %d, Vsram: %d, udelay: %d",
		g_stack.cur_volt, g_stack.cur_vsram, t_settle_volt);

done:
	GPUFREQ_TRACE_END();

	return ret;
}

/*
 * API: dump power/clk status when bring-up
 */
static void __gpufreq_dump_bringup_status(void)
{
	/* only dump when bringup */
	if (!__gpufreq_bringup())
		return;

	/* 0x13FA0000 */
	g_mfg_pll_base = __gpufreq_of_ioremap("mediatek,mfg_pll", 0);
	if (!g_mfg_pll_base) {
		GPUFREQ_LOGE("fail to ioremap mfg_pll (ENOENT)");
		goto done;
	}

	/* 0x13FA0C00 */
	g_mfg_scpll_base = __gpufreq_of_ioremap("mediatek,mfg_scpll", 0);
	if (!g_mfg_scpll_base) {
		GPUFREQ_LOGE("fail to ioremap mfg_pll (ENOENT)");
		goto done;
	}

	/* 0x10006000 */
	g_sleep = __gpufreq_of_ioremap("mediatek,sleep", 0);
	if (!g_sleep) {
		GPUFREQ_LOGE("fail to ioremap SLEEP (ENOENT)");
		goto done;
	}

	/*
	 * [SPM] pwr_status    : 0x1000616C
	 * [SPM] pwr_status_2nd: 0x10006170
	 * Power ON: 0001 1111 1100 (0x1FC)
	 * [2]: MFG0, [3]: MFG1, [4]: MFG2, [5]: MFG3, [6]: MFG4, [7]: MFG5, [8]: MFG6
	 */
	GPUFREQ_LOGI("[MFG0-6] PWR_STATUS: 0x%08x, PWR_STATUS_2ND: 0x%08x",
		readl(g_sleep + 0x16C) & 0x1FC,
		readl(g_sleep + 0x170) & 0x1FC);
	GPUFREQ_LOGI("[MFGPLL] FMETER: %d CON1: %d",
		__gpufreq_get_fmeter_fgpu(), __gpufreq_get_real_fgpu());

done:
	return;
}

static unsigned int __gpufreq_get_fmeter_fgpu(void)
{
	/* todo: fmeter not ready */
	// return mt_get_abist_freq(FM_MGPLL_CK);
	return 0;
}

static unsigned int __gpufreq_get_fmeter_fstack(void)
{
	/* todo: fmeter not ready */
	// return mt_get_abist_freq(FM_MGPLL_CK);
	return 0;
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

	mfgpll = readl(MFGPLL_GPU_CON1);

	pcw = mfgpll & (0x3FFFFF);

	posdiv_power = (mfgpll & (0x7 << POSDIV_SHIFT)) >> POSDIV_SHIFT;

	freq = (((pcw * TO_MHZ_TAIL + ROUNDING_VALUE) * MFGPLL_FIN) >> DDS_SHIFT) /
		(1 << posdiv_power) * TO_MHZ_HEAD;

	return freq;
}

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

	mfgpll = readl(MFGPLL_STACK_CON1);

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

	if (regulator_is_enabled(g_pmic->reg_vgpu)) {
		/* regulator_get_voltage prints volt with uV */
		volt = regulator_get_voltage(g_pmic->reg_vgpu) / 10;
	}

	return volt;
}

/* API: get real current Vstack from regulator (mV * 100) */
static unsigned int __gpufreq_get_real_vstack(void)
{
	unsigned int volt = 0;

	if (regulator_is_enabled(g_pmic->reg_vstack)) {
		/* regulator_get_voltage prints volt with uV */
		volt = regulator_get_voltage(g_pmic->reg_vstack) / 10;
	}

	return volt;
}

/* API: get real current Vsram from regulator (mV * 100) */
static unsigned int __gpufreq_get_real_vsram(void)
{
	unsigned int volt = 0;

	if (regulator_is_enabled(g_pmic->reg_vsram)) {
		/* regulator_get_voltage prints volt with uV */
		volt = regulator_get_voltage(g_pmic->reg_vsram) / 10;
	}

	return volt;
}

/* DVFS HW Timing issue */
static void __gpufreq_dvfs_timing_control(unsigned int direction)
{
	u32 val = 0;

#if defined(GPUFREQ_TODO_HW_TIMING)
	/* volt scale up */
	if (direction) {

	/* volt scale down */
	} else {

	}
#endif
}

/* DELSEL ULV SRAM access */
static void __gpufreq_delsel_control(unsigned int direction)
{
	u32 val = 0;

	/* volt scale up */
	if (direction) {
		/* MFG_SRAM_FUL_SEL_ULV 0x13FBF080 [0] FUL_SEL_ULV = 1'b0 */
		val = readl(g_g3d_base + 0x80);
		val &= ~(1UL << 0);
		writel(val, g_g3d_base + 0x80);
	/* volt scale down */
	} else {
		/* MFG_SRAM_FUL_SEL_ULV 0x13FBF080 [9] FUL_SEL_ULV = 1'b1 */
		val = readl(g_g3d_base + 0x80);
		val |= (1UL << 0);
		writel(val, g_g3d_base + 0x80);
	}
}

/* API: find freq_gpu that satify fgpu >= fstack * 1.1 */
static void __gpufreq_get_hw_constraint_fgpu(unsigned int fstack,
	int *oppidx_gpu, unsigned int *fgpu)
{
	struct gpufreq_opp_info *working_table = g_gpu.working_table;
	int target_oppidx = 0;
	unsigned int target_freq = 0;
	int min_oppidx = g_gpu.min_oppidx;
	int i = 0;

	for (i = min_oppidx; i >= 0; i--) {
		target_freq = working_table[i].freq * GPUFREQ_BASE_COEF;
		if (target_freq >= fstack * GPUFREQ_CONSTRAINT_COEF)
			break;
	}

	if (unlikely(i < 0))
		__gpufreq_abort(GPUFREQ_FREQ_EXCEPTION,
			"fail to find HW constraint Fgpu (Fstack x1.1: %d)",
			(int)(fstack * GPUFREQ_CONSTRAINT_COEF / GPUFREQ_BASE_COEF));

	*oppidx_gpu = i;
	*fgpu = working_table[i].freq;
}

/* API: find freq_stack that satify fgpu >= fstack * 1.1 */
static void __gpufreq_get_hw_constraint_fstack(unsigned int fgpu,
	int *oppidx_stack, unsigned int *fstack)
{
	struct gpufreq_opp_info *working_table = g_stack.working_table;
	int target_oppidx = 0;
	unsigned int target_freq = 0;
	int opp_num = g_stack.opp_num;
	int i = 0;

	for (i = 0; i < opp_num; i++) {
		target_freq = working_table[i].freq * GPUFREQ_CONSTRAINT_COEF;
		if (fgpu * GPUFREQ_BASE_COEF >= target_freq)
			break;
	}

	if (unlikely(i >= opp_num))
		__gpufreq_abort(GPUFREQ_FREQ_EXCEPTION,
			"fail to find HW constraint Fstack (Fgpu /1.1: %d)",
			(int)(fgpu * GPUFREQ_BASE_COEF / GPUFREQ_CONSTRAINT_COEF));

	*oppidx_stack = i;
	*fstack = working_table[i].freq;
}

/* API: find parking volt and control registers because of platform HW constraint */
static unsigned int __gpufreq_get_hw_constraint_vstack(unsigned int volt_old, unsigned int volt_new)
{
	unsigned int volt_park = 0;

	/* volt scaling up */
	if (volt_new > volt_old) {
		/* additionally control register when cross special volt */
		if (volt_old == DVFS_TIMING_PARK_VOLT)
			__gpufreq_dvfs_timing_control(true);
		else if (volt_old == DELSEL_ULV_PARK_VOLT)
			__gpufreq_delsel_control(true);

		/* check smaller parking volt first while scaling up */
		if (volt_new > DELSEL_ULV_PARK_VOLT && volt_old < DELSEL_ULV_PARK_VOLT)
			volt_park = DELSEL_ULV_PARK_VOLT;
		else if (volt_new > DVFS_TIMING_PARK_VOLT && volt_old < DVFS_TIMING_PARK_VOLT)
			volt_park = DVFS_TIMING_PARK_VOLT;
		else
			volt_park = volt_new;
	/* volt scaling down */
	} else if (volt_new < volt_old) {
		/* additionally control register when cross special volt */
		if (volt_old == DVFS_TIMING_PARK_VOLT)
			__gpufreq_dvfs_timing_control(false);
		else if (volt_old == DVFS_TIMING_PARK_VOLT)
			__gpufreq_delsel_control(false);

		/* check larger parking volt first while scaling down */
		if (volt_new < DVFS_TIMING_PARK_VOLT && volt_old > DVFS_TIMING_PARK_VOLT)
			volt_park = DVFS_TIMING_PARK_VOLT;
		else if (volt_new < DELSEL_ULV_PARK_VOLT && volt_old > DELSEL_ULV_PARK_VOLT)
			volt_park = DELSEL_ULV_PARK_VOLT;
		else
			volt_park = volt_new;
	} else {
		volt_park = volt_new;
	}

	return volt_park;
}

static void __gpufreq_hw_dcm_control(void)
{
	u32 val = 0;

	/* MFG_DCM_CON_0 0x13FBF010 [6:0] BG3D_DBC_CNT = 7'b0111111 */
	/* MFG_DCM_CON_0 0x13FBF010 [15]  BG3D_DCM_EN = 1'b1 */
	val = readl(g_g3d_base + 0x10);
	val |= (1UL << 0);
	val |= (1UL << 1);
	val |= (1UL << 2);
	val |= (1UL << 3);
	val |= (1UL << 4);
	val |= (1UL << 5);
	val &= ~(1UL << 6);
	val |= (1UL << 15);
	writel(val, g_g3d_base + 0x10);

	/* MFG_ASYNC_CON 0x13FBF020 [23] MEM0_SLV_CG_ENABLE = 1'b1 */
	/* MFG_ASYNC_CON 0x13FBF020 [25] MEM1_SLV_CG_ENABLE = 1'b1 */
	val = readl(g_g3d_base + 0x20);
	val |= (1UL << 23);
	val |= (1UL << 25);
	writel(val, g_g3d_base + 0x20);

	/* MFG_GLOBAL_CON 0x13FBF0B0 [8]  GPU_SOCIF_MST_FREE_RUN = 1'b0 */
	/* MFG_GLOBAL_CON 0x13FBF0B0 [10] GPU_CLK_FREE_RUN = 1'b0 */
	/* MFG_GLOBAL_CON 0x13FBF0B0 [21] dvfs_hint_cg_en = 1'b0 */
	val = readl(g_g3d_base + 0xB0);
	val &= ~(1UL << 8);
	val &= ~(1UL << 10);
	val &= ~(1UL << 21);
	writel(val, g_g3d_base + 0xB0);

	/* MFG_RPC_AO_CLK_CFG 0x13F91034 [0] CG_FAXI_CK_SOC_IN_FREE_RUN = 1'b0 */
	val = readl(g_mfg_rpc_base + 0x1034);
	val &= ~(1UL << 0);
	writel(val, g_mfg_rpc_base + 0x1034);
}

static int __gpufreq_cg_control(enum gpufreq_power_state power)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("power=%d", power);

	if (power == POWER_ON) {
		ret = clk_prepare_enable(g_clk->subsys_mfg_cg);
		if (unlikely(ret)) {
			__gpufreq_abort(GPUFREQ_FREQ_EXCEPTION,
				"fail to enable subsys_mfg_cg (%d)", ret);
			goto done;
		}
		g_gpu.cg_count++;
		g_stack.cg_count++;
	} else {
		clk_disable_unprepare(g_clk->subsys_mfg_cg);
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

#if defined(GPUFREQ_TODO_AOC_DTS)
	/* power on: AOCISO -> AOCLHENB */
	if (power == POWER_ON) {
		/* AOCISO_Vgpustack 0x1C001F30 [9] = 1'b0 */
		val = readl(0x1C001 + 0xF30);
		val &= ~(1UL << 9);
		writel(val, 0x1C001 + 0xF30);

		/* AOCLHENB_Vgpustack 0x1C001F30 [10] = 1'b0 */
		val = readl(0x1C001 + 0xF30);
		val &= ~(1UL << 10);
		writel(val, 0x1C001 + 0xF30);
	/* power off: AOCLHENB -> AOCISO */
	} else {
		/* AOCLHENB_Vgpustack 0x1C001F30 [10] = 1'b1 */
		val = readl(0x1C001 + 0xF30);
		val |= (1UL << 10);
		writel(val, 0x1C001 + 0xF30);

		/* AOCISO_Vgpustack 0x1C001F30 [9] = 1'b1 */
		val = readl(0x1C001 + 0xF30);
		val |= (1UL << 9);
		writel(val, 0x1C001 + 0xF30);
	}
#endif
}

/* HWAPM: GPU FW Control GPU shader cores itself */
static void __gpufreq_hw_apm_control(void)
{
	u32 val = 0;

	/* MFG_ACTIVE_POWER_CON_CG_06 0x13FBF100 [0] rg_cg_active_pwrctl_en = 1'b1 */
	val = readl(g_g3d_base + 0x100);
	val |= (1UL << 0);
	writel(val, g_g3d_base + 0x100);

	/* MFG_ACTIVE_POWER_CON_ST0_06 0x13FBF120 [0] rg_st0_active_pwrctl_en = 1'b1 */
	val = readl(g_g3d_base + 0x120);
	val |= (1UL << 0);
	writel(val, g_g3d_base + 0x120);

	/* MFG_ACTIVE_POWER_CON_ST1_06 0x13FBF140 [0] rg_st1_active_pwrctl_en = 1'b1 */
	val = readl(g_g3d_base + 0x140);
	val |= (1UL << 0);
	writel(val, g_g3d_base + 0x140);

	/* MFG_ACTIVE_POWER_CON_ST2_06 0x13FBF118 [0] rg_st2_active_pwrctl_en = 1'b1 */
	val = readl(g_g3d_base + 0x118);
	val |= (1UL << 0);
	writel(val, g_g3d_base + 0x118);

	/* MFG_ACTIVE_POWER_CON_ST4_06 0x13FBF0C0 [0] rg_st4_active_pwrctl_en = 1'b1 */
	val = readl(g_g3d_base + 0xC0);
	val |= (1UL << 0);
	writel(val, g_g3d_base + 0xC0);

	/* MFG_ACTIVE_POWER_CON_ST5_06 0x13FBF098 [0] rg_st5_active_pwrctl_en = 1'b1 */
	val = readl(g_g3d_base + 0x98);
	val |= (1UL << 0);
	writel(val, g_g3d_base + 0x98);

	/* MFG_ACTIVE_POWER_CON_ST6_06 0x13FBF1C0 [0] rg_st6_active_pwrctl_en = 1'b1 */
	val = readl(g_g3d_base + 0x1C0);
	val |= (1UL << 0);
	writel(val, g_g3d_base + 0x1C0);

	/* MFG_ACTIVE_POWER_CON_00 0x13FBF400 [0] rg_sc0_active_pwrctl_en = 1'b1 */
	val = readl(g_g3d_base + 0x400);
	val |= (1UL << 0);
	writel(val, g_g3d_base + 0x400);

	/* MFG_ACTIVE_POWER_CON_06 0x13FBF418 [0] rg_sc1_active_pwrctl_en = 1'b1 */
	val = readl(g_g3d_base + 0x418);
	val |= (1UL << 0);
	writel(val, g_g3d_base + 0x418);

	/* MFG_ACTIVE_POWER_CON_12 0x13FBF430 [0] rg_sc2_active_pwrctl_en = 1'b1 */
	val = readl(g_g3d_base + 0x430);
	val |= (1UL << 0);
	writel(val, g_g3d_base + 0x430);

	/* MFG_ACTIVE_POWER_CON_18 0x13FBF448 [0] rg_sc3_active_pwrctl_en = 1'b1 */
	val = readl(g_g3d_base + 0x448);
	val |= (1UL << 0);
	writel(val, g_g3d_base + 0x448);

	/* MFG_ACTIVE_POWER_CON_24 0x13FBF460 [0] rg_sc4_active_pwrctl_en = 1'b1 */
	val = readl(g_g3d_base + 0x460);
	val |= (1UL << 0);
	writel(val, g_g3d_base + 0x460);

	/* MFG_ACTIVE_POWER_CON_30 0x13FBF478 [0] rg_sc5_active_pwrctl_en = 1'b1 */
	val = readl(g_g3d_base + 0x478);
	val |= (1UL << 0);
	writel(val, g_g3d_base + 0x478);

	/* MFG_ACTIVE_POWER_CON_36 0x13FBF490 [0] rg_sc6_active_pwrctl_en = 1'b1 */
	val = readl(g_g3d_base + 0x490);
	val |= (1UL << 0);
	writel(val, g_g3d_base + 0x490);

	/* MFG_ACTIVE_POWER_CON_42 0x13FBF4A8 [0] rg_sc7_active_pwrctl_en = 1'b1 */
	val = readl(g_g3d_base + 0x4A8);
	val |= (1UL << 0);
	writel(val, g_g3d_base + 0x4A8);

	/* MFG_ACTIVE_POWER_CON_48 0x13FBF4C0 [0] rg_sc8_active_pwrctl_en = 1'b1 */
	val = readl(g_g3d_base + 0x4C0);
	val |= (1UL << 0);
	writel(val, g_g3d_base + 0x4C0);

	/* MFG_ACTIVE_POWER_CON_54 0x13FBF4D8 [0] rg_sc9_active_pwrctl_en = 1'b1 */
	val = readl(g_g3d_base + 0x4D8);
	val |= (1UL << 0);
	writel(val, g_g3d_base + 0x4D8);
}

/* ACP: GPU can access CPU cache directly */
static void __gpufreq_acp_control(void)
{
	u32 val = 0;

	/* MFG_AXCOHERENCE 0x13FBF168 [0] M0_coherence_enable = 1'b1 */
	/* MFG_AXCOHERENCE 0x13FBF168 [1] M1_coherence_enable = 1'b1 */
	/* MFG_AXCOHERENCE 0x13FBF168 [2] M2_coherence_enable = 1'b1 */
	/* MFG_AXCOHERENCE 0x13FBF168 [3] M3_coherence_enable = 1'b1 */
	val = readl(g_g3d_base + 0x168);
	val |= (1UL << 0);
	val |= (1UL << 1);
	val |= (1UL << 2);
	val |= (1UL << 3);
	writel(val, g_g3d_base + 0x168);

	/* MFG_1TO2AXI_CON_00 0x13FBF8E0 [23:0] mfg_axi1to2_R_dispatch_mode = 0x855 */
	writel(0x855, g_g3d_base + 0x8E0);

	/* MFG_1TO2AXI_CON_02 0x13FBF8E8 [23:0] mfg_axi1to2_R_dispatch_mode = 0x855 */
	writel(0x855, g_g3d_base + 0x8E8);

	/* MFG_1TO2AXI_CON_04 0x13FBF910 [23:0] mfg_axi1to2_R_dispatch_mode = 0x855 */
	writel(0x855, g_g3d_base + 0x910);

	/* MFG_1TO2AXI_CON_06 0x13FBF918 [23:0] mfg_axi1to2_R_dispatch_mode = 0x855 */
	writel(0x855, g_g3d_base + 0x918);

	/* MFG_OUT_1TO2AXI_CON_00 0x13FBF900 [23:0] mfg_axi1to2_R_dispatch_mode = 0x55 */
	writel(0x55, g_g3d_base + 0x900);

	/* MFG_OUT_1TO2AXI_CON_02 0x13FBF908 [23:0] mfg_axi1to2_R_dispatch_mode = 0x55 */
	writel(0x55, g_g3d_base + 0x908);

	/* MFG_OUT_1TO2AXI_CON_04 0x13FBF920 [23:0] mfg_axi1to2_R_dispatch_mode = 0x55 */
	writel(0x55, g_g3d_base + 0x920);

	/* MFG_OUT_1TO2AXI_CON_06 0x13FBF928 [23:0] mfg_axi1to2_R_dispatch_mode = 0x55 */
	writel(0x55, g_g3d_base + 0x928);
}

/* GPM1.0: di/dt reduction by slowing down speed of frequency scaling up or down */
static void __gpufreq_gpm_control(void)
{
	/* MFG_I2M_PROTECTOR_CFG_00 0x13FBFF60 = 0x20700316 */
	writel(0x20700316, g_g3d_base + 0xF60);

	/* MFG_I2M_PROTECTOR_CFG_01 0x13FBFF64 = 0x1800000C */
	writel(0x1800000C, g_g3d_base + 0xF64);

	/* MFG_I2M_PROTECTOR_CFG_02 0x13FBFF68 = 0x01010802 */
	writel(0x01011002, g_g3d_base + 0xF68);

	/* MFG_I2M_PROTECTOR_CFG_03 0x13FBFFA8 = 0x01010802 */
	writel(0x00030FF1, g_g3d_base + 0xFA8);

	/* wait 1us */
	udelay(1);

	/* MFG_I2M_PROTECTOR_CFG_00 0x13FBFF60 = 0x20700317 */
	writel(0x20700317, g_g3d_base + 0xF60);
}

static int __gpufreq_mtcmos_control(enum gpufreq_power_state power)
{
	int ret = GPUFREQ_SUCCESS;
	u32 val = 0;

	GPUFREQ_TRACE_START("power=%d", power);

	if (power == POWER_ON) {
		/* MFG_1 on */
		ret = pm_runtime_get_sync(&g_mtcmos->mfg1_pdev->dev);
		if (unlikely(ret < 0)) {
			__gpufreq_abort(GPUFREQ_FREQ_EXCEPTION,
				"fail to enable mtcmos_mfg1 (%d)", ret);
			goto done;
		}
		g_gpu.mtcmos_count++;
		g_stack.mtcmos_count++;

#if GPUFREQ_HWAPM_ENABLE
#if GPUFREQ_CHECK_MTCMOS_PWR_STATUS
		/* check MTCMOS_0-1 power status */
		if (g_sleep) {
			val = readl(g_sleep + 0x16C) & 0x1FC;
			if (unlikely(val != 0xC)) {
				__gpufreq_abort(GPUFREQ_FREQ_EXCEPTION,
					"incorrect MTCMOS power on status: 0x%08x", val);
				ret = GPUFREQ_EINVAL;
				goto done;
			}
		}
#endif /* GPUFREQ_CHECK_MTCMOS_PWR_STATUS */
		/*
		 * Set HW APM register to enable PDC mode 2 when power on
		 * let GPU DDK control MTCMOS itself
		 */
		__gpufreq_hw_apm_control();
#else
		if (g_shader_present & MFG2_SHADER_STACK0) {
			ret = pm_runtime_get_sync(&g_mtcmos->mfg2_pdev->dev);
			if (unlikely(ret < 0)) {
				__gpufreq_abort(GPUFREQ_FREQ_EXCEPTION,
					"fail to enable mtcmos_mfg2 (%d)", ret);
				goto done;
			}
		}
		if (g_shader_present & MFG3_SHADER_STACK1) {
			ret = pm_runtime_get_sync(&g_mtcmos->mfg3_pdev->dev);
			if (unlikely(ret < 0)) {
				__gpufreq_abort(GPUFREQ_FREQ_EXCEPTION,
					"fail to enable mtcmos_mfg3 (%d)", ret);
				goto done;
			}
		}
		if (g_shader_present & MFG4_SHADER_STACK2) {
			ret = pm_runtime_get_sync(&g_mtcmos->mfg4_pdev->dev);
			if (unlikely(ret < 0)) {
				__gpufreq_abort(GPUFREQ_FREQ_EXCEPTION,
					"fail to enable mtcmos_mfg4 (%d)", ret);
				goto done;
			}
		}
		if (g_shader_present & MFG5_SHADER_STACK5) {
			ret = pm_runtime_get_sync(&g_mtcmos->mfg5_pdev->dev);
			if (unlikely(ret < 0)) {
				__gpufreq_abort(GPUFREQ_FREQ_EXCEPTION,
					"fail to enable mtcmos_mfg5 (%d)", ret);
				goto done;
			}
		}
		if (g_shader_present & MFG6_SHADER_STACK6) {
			ret = pm_runtime_get_sync(&g_mtcmos->mfg6_pdev->dev);
			if (unlikely(ret < 0)) {
				__gpufreq_abort(GPUFREQ_FREQ_EXCEPTION,
					"fail to enable mtcmos_mfg6 (%d)", ret);
				goto done;
			}
		}

#if GPUFREQ_CHECK_MTCMOS_PWR_STATUS
		/* check MTCMOS_0-6 power status */
		if (g_sleep) {
			val = readl(g_sleep + 0x16C) & 0x1FC;
			if (unlikely(val != 0x1FC)) {
				__gpufreq_abort(GPUFREQ_FREQ_EXCEPTION,
					"incorrect MTCMOS power on status: 0x%08x", val);
				ret = GPUFREQ_EINVAL;
				goto done;
			}
		}
#endif /* GPUFREQ_CHECK_MTCMOS_PWR_STATUS */
#endif /* GPUFREQ_HWAPM_ENABLE */
	} else {
#if GPUFREQ_HWAPM_ENABLE
		/* No need to disable HW APM when power off */
#else
		if (g_shader_present & MFG6_SHADER_STACK6)
			pm_runtime_put_sync(&g_mtcmos->mfg6_pdev->dev);
		if (g_shader_present & MFG5_SHADER_STACK5)
			pm_runtime_put_sync(&g_mtcmos->mfg5_pdev->dev);
		if (g_shader_present & MFG4_SHADER_STACK2)
			pm_runtime_put_sync(&g_mtcmos->mfg4_pdev->dev);
		if (g_shader_present & MFG3_SHADER_STACK1)
			pm_runtime_put_sync(&g_mtcmos->mfg3_pdev->dev);
		if (g_shader_present & MFG2_SHADER_STACK0)
			pm_runtime_put_sync(&g_mtcmos->mfg2_pdev->dev);
#endif /* GPUFREQ_HWAPM_ENABLE */

		/* MFG_1 off */
		pm_runtime_put_sync(&g_mtcmos->mfg1_pdev->dev);

		g_gpu.mtcmos_count--;
		g_stack.mtcmos_count--;

#if GPUFREQ_CHECK_MTCMOS_PWR_STATUS
		/* check MTCMOS_1-6 power status, MTCMOS_0 is Off in preloader */
		if (g_sleep && !g_stack.mtcmos_count) {
			val = readl(g_sleep + 0x16C) & 0x1FC;
			if (unlikely(val != 0x4)) {
				__gpufreq_abort(GPUFREQ_FREQ_EXCEPTION,
					"incorrect MTCMOS power off status: 0x%08x", val);
				ret = GPUFREQ_EINVAL;
				goto done;
			}
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
		ret = regulator_set_voltage(g_pmic->reg_vgpu, g_gpu.cur_volt * 10, INT_MAX);
		if (unlikely(ret)) {
			__gpufreq_abort(GPUFREQ_FREQ_EXCEPTION, "fail to enable VGPU (%d)", ret);
			goto done;
		}
		g_gpu.buck_count++;
#endif /* GPUFREQ_VCORE_DVFS_ENABLE */

		ret = regulator_enable(g_pmic->reg_vstack);
		if (unlikely(ret)) {
			__gpufreq_abort(GPUFREQ_FREQ_EXCEPTION, "fail to enable VSTACK (%d)", ret);
			goto done;
		}
		g_stack.buck_count++;
	/* power off */
	} else {
		ret = regulator_disable(g_pmic->reg_vstack);
		if (unlikely(ret)) {
			__gpufreq_abort(GPUFREQ_FREQ_EXCEPTION, "fail to disable VSTACK (%d)", ret);
			goto done;
		}
		g_stack.buck_count--;

#if GPUFREQ_VCORE_DVFS_ENABLE
		/* vote level_0 to DVFSRC as GPU power off */
		ret = regulator_set_voltage(g_pmic->reg_vgpu, VCORE_LEVEL_0 * 10, INT_MAX);
		if (unlikely(ret)) {
			__gpufreq_abort(GPUFREQ_FREQ_EXCEPTION, "fail to disable VGPU (%d)", ret);
			goto done;
		}
		g_gpu.buck_count--;
#endif /* GPUFREQ_VCORE_DVFS_ENABLE */
	}

done:
	GPUFREQ_TRACE_END();

	return ret;
}

/* API: init first OPP idx by init freq set in preloader */
static int __gpufreq_init_opp_idx(void)
{
	struct gpufreq_opp_info *working_table = g_stack.working_table;
	unsigned int cur_fgpu = 0, cur_fstack = 0;
	int cur_oppidx_gpu = -1, cur_oppidx_stack = -1;
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START();

	/* get GPU OPP idx */
	cur_fgpu = g_gpu.cur_freq;
	cur_oppidx_gpu = __gpufreq_get_idx_by_fgpu(cur_fgpu);
	g_gpu.cur_oppidx = cur_oppidx_gpu;

	/* get STACK OPP idx */
	cur_fstack = g_stack.cur_freq;
	if (__gpufreq_custom_init_enable()) {
		cur_oppidx_stack = GPUFREQ_CUST_INIT_OPPIDX;
		GPUFREQ_LOGI("custom init STACK OPP index: %d, Freq: %d",
			cur_oppidx_stack, working_table[cur_oppidx_stack].freq);
	} else {
		cur_oppidx_stack = __gpufreq_get_idx_by_fstack(cur_fstack);
	}
	g_stack.cur_oppidx = cur_oppidx_stack;

	/* init first OPP index */
	if (!__gpufreq_dvfs_enable()) {
		g_dvfs_state = DVFS_DISABLE;
		GPUFREQ_LOGI("DVFS state: 0x%x, disable DVFS", g_dvfs_state);

		if (__gpufreq_custom_init_enable()) {
			ret = __gpufreq_generic_commit_stack(cur_oppidx_stack, DVFS_DISABLE);
			if (unlikely(ret)) {
				GPUFREQ_LOGE("fail to commit STACK OPP index: %d (%d)",
					cur_oppidx_stack, ret);
				goto done;
			}
		}
	} else {
		g_dvfs_state = DVFS_FREE;
		GPUFREQ_LOGI("DVFS state: 0x%x, init STACK OPP index: %d",
			g_dvfs_state, cur_oppidx_stack);

		ret = __gpufreq_generic_commit_stack(cur_oppidx_stack, DVFS_FREE);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to commit STACK OPP index: %d (%d)",
				cur_oppidx_stack, ret);
			goto done;
		}
	}

done:
	GPUFREQ_TRACE_END();

	return ret;
}

/* API: calculate power of every OPP in working table */
static void __gpufreq_measure_power(enum gpufreq_target target)
{
	unsigned int freq = 0, volt = 0;
	unsigned int p_total = 0, p_dynamic = 0, p_leakage = 0;
	int i = 0;
	struct gpufreq_opp_info *working_table = NULL;
	int opp_num = 0;

	GPUFREQ_TRACE_START("target=%d", target);

	if (target == TARGET_STACK) {
		working_table = g_stack.working_table;
		opp_num = g_stack.opp_num;
	} else {
		working_table = g_gpu.working_table;
		opp_num = g_gpu.opp_num;
	}

	for (i = 0; i < opp_num; i++) {
		freq = working_table[i].freq;
		volt = working_table[i].volt;

		if (target == TARGET_STACK) {
			p_leakage = __gpufreq_get_lkg_pstack(volt);
			p_dynamic = __gpufreq_get_dyn_pstack(freq, volt);
		} else {
			p_leakage = __gpufreq_get_lkg_pgpu(volt);
			p_dynamic = __gpufreq_get_dyn_pgpu(freq, volt);
		}
		p_total = p_dynamic + p_leakage;

		working_table[i].power = p_total;

		GPUFREQ_LOGD("%s[%02d] power: %d (dynamic: %d, leakage: %d)",
			(target == TARGET_STACK) ? "STACK" : "GPU",
			i, p_total, p_dynamic, p_leakage);
	}

	GPUFREQ_TRACE_END();
}

/* API: resume dvfs to free run */
static void __gpufreq_resume_dvfs(void)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START();

	__gpufreq_set_regulator_mode(TARGET_STACK, REGULATOR_MODE_NORMAL); /* NORMAL */

	ret = __gpufreq_power_control(POWER_OFF);
	if (unlikely(ret < 0)) {
		GPUFREQ_LOGE("fail to control power state: %d (%d)", POWER_OFF, ret);
	}

	__gpufreq_set_dvfs_state(false, DVFS_AGING_KEEP);

	GPUFREQ_LOGD("dvfs state: 0x%x", g_dvfs_state);

	GPUFREQ_TRACE_END();
}

/* API: pause dvfs to given freq and volt */
static int __gpufreq_pause_dvfs(unsigned int keep_freq, unsigned int keep_volt)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("keep_freq=%d, keep_volt=%d", keep_freq, keep_volt);

	__gpufreq_set_dvfs_state(true, DVFS_AGING_KEEP);

	ret = __gpufreq_power_control(POWER_ON);
	if (unlikely(ret < 0)) {
		GPUFREQ_LOGE("fail to control power state: %d (%d)", POWER_ON, ret);
		__gpufreq_set_dvfs_state(false, DVFS_AGING_KEEP);
		goto done;
	}

	ret = __gpufreq_custom_commit_stack(keep_freq, keep_volt, DVFS_AGING_KEEP);
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to commit STACK Freq: %d, Volt: %d (%d)",
			keep_freq, keep_volt, ret);
		__gpufreq_set_dvfs_state(false, DVFS_AGING_KEEP);
		goto done;
	}

	__gpufreq_set_regulator_mode(TARGET_STACK, REGULATOR_MODE_FAST); /* PWM */

	GPUFREQ_LOGD("DVFS state: 0x%x, keep Freq: %d, keep Volt: %d",
		g_dvfs_state, keep_freq, keep_volt);

done:
	GPUFREQ_TRACE_END();

	return ret;
}

/*
 * API: interpolate OPP of none AVS idx.
 * step = (large - small) / range
 * vnew = large - step * j
 */
static void __gpufreq_interpolate_volt(enum gpufreq_target target)
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
	struct gpufreq_opp_info *signed_table = NULL;

	GPUFREQ_TRACE_START("target=%d", target);

	if (target == TARGET_STACK)
		signed_table = g_stack.signed_table;
	else
		signed_table = g_gpu.signed_table;

	avs_num = AVS_ADJ_NUM;

	mutex_lock(&gpufreq_lock);

	for (i = 1; i < avs_num; i++) {
		front_idx = g_avs_adj[i - 1].oppidx;
		rear_idx = g_avs_adj[i].oppidx;
		range = rear_idx - front_idx;

		/* freq division to amplify slope */
		large_volt = signed_table[front_idx].volt;
		large_freq = signed_table[front_idx].freq / 1000;

		small_volt = signed_table[rear_idx].volt;
		small_freq = signed_table[rear_idx].freq / 1000;

		/* slope = volt / freq */
		slope = (large_volt - small_volt) / (large_freq - small_freq);

		if (unlikely(slope < 0))
			__gpufreq_abort(GPUFREQ_FREQ_EXCEPTION,
				"invalid slope when interpolate OPP Volt: %d", slope);

		GPUFREQ_LOGD("%s[%02d*] Freq: %d, Volt: %d, slope: %d",
			(target == TARGET_STACK) ? "STACK" : "GPU",
			rear_idx, small_freq*1000, small_volt, slope);

		/* start from small v and f, and use (+) instead of (-) */
		for (j = 1; j < range; j++) {
			inner_idx = rear_idx - j;
			inner_freq = signed_table[inner_idx].freq / 1000;
			inner_volt = small_volt + slope * (inner_freq - small_freq);
			inner_volt = VOLT_NORMALIZATION(inner_volt);

			/* compare interpolated volt with volt of previous OPP idx */
			previous_volt = signed_table[inner_idx + 1].volt;
			if (inner_volt < previous_volt)
				__gpufreq_abort(GPUFREQ_FREQ_EXCEPTION,
					"invalid interpolated [%02d*] Volt: %d < [%02d*] Volt: %d",
					inner_idx, inner_volt, inner_idx + 1, previous_volt);

			signed_table[inner_idx].volt = inner_volt;
			signed_table[inner_idx].vsram = __gpufreq_get_vsram_by_vgpu(inner_volt);

			GPUFREQ_LOGD("%s[%02d*] Freq: %d, Volt: %d, vsram: %d,",
				(target == TARGET_STACK) ? "STACK" : "GPU",
				inner_idx, inner_freq*1000, inner_volt,
				signed_table[inner_idx].vsram);
		}
		GPUFREQ_LOGD("%s[%02d*] Freq: %d, Volt: %d",
			(target == TARGET_STACK) ? "STACK" : "GPU",
			front_idx, large_freq*1000, large_volt);
	}

	mutex_unlock(&gpufreq_lock);

	GPUFREQ_TRACE_END();
}

/* API: apply aging volt diff to working table */
static void __gpufreq_apply_aging(enum gpufreq_target target, unsigned int apply_aging)
{
	int i = 0;
	struct gpufreq_opp_info *working_table = NULL;
	int opp_num = 0;

	GPUFREQ_TRACE_START("apply_aging=%d, target=%d", apply_aging, target);

	if (target == TARGET_STACK) {
		working_table = g_stack.working_table;
		opp_num = g_stack.opp_num;
	} else {
		working_table = g_gpu.working_table;
		opp_num = g_gpu.opp_num;
	}

	mutex_lock(&gpufreq_lock);

	for (i = 0; i < opp_num; i++) {
		if (apply_aging)
			working_table[i].volt -= working_table[i].vaging;
		else
			working_table[i].volt += working_table[i].vaging;

		working_table[i].vsram = __gpufreq_get_vsram_by_vgpu(working_table[i].volt);

		GPUFREQ_LOGD("apply Vaging: %d, %s[%02d] Volt: %d, Vsram: %d",
			(target == TARGET_STACK) ? "STACK" : "GPU",
			apply_aging, i, working_table[i].volt, working_table[i].vsram);
	}

	mutex_unlock(&gpufreq_lock);

	GPUFREQ_TRACE_END();
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

/* API: get Aging sensor data from EFUSE, return if success*/
static unsigned int __gpufreq_asensor_read_efuse(u32 *a_t0_lvt_rt, u32 *a_t0_ulvt_rt,
	u32 *a_t0_ulvtll_rt, u32 *a_shift_error, u32 *efuse_error)
{
#if GPUFREQ_ASENSOR_ENABLE
	u32 efuse_val1 = 0, efuse_val2 = 0, efuse_val3 = 0;

	/*
	 * A_T0_ULVTLL_RT  : 0x11EE0AA8 [9:0]
	 * A_T0_LVT_RT     : 0x11EE0AA8 [19:10]
	 * A_T0_ULVT_RT    : 0x11EE0AA8 [29:20]
	 * A_shift_error   : 0x11EE0AA8 [31]
	 * efuse_error     : 0x11EE0B6C [31]
	 */
	efuse_val1 = readl(g_cpe_0p65v_rt_blow_base + 0xAA8);
	efuse_val2 = readl(g_cpe_0p65v_rt_blow_base + 0xAAC);
	efuse_val3 = readl(g_cpe_0p65v_rt_blow_base + 0xB6C);
	if (efuse_val1 == 0 || efuse_val3 == 0)
		return false;

	*a_t0_ulvtll_rt = (efuse_val1  & 0x000003FF) + 1500;
	*a_t0_lvt_rt    = ((efuse_val1 & 0x000FFC00) >> 10) + 1200;
	*a_t0_ulvt_rt   = ((efuse_val1 & 0x3FF00000) >> 20) + 1800;
	*a_shift_error  = efuse_val1 >> 31;
	*efuse_error    = efuse_val3 >> 31;

	g_asensor_info.efuse_val1 = efuse_val1;
	g_asensor_info.efuse_val2 = efuse_val2;
	g_asensor_info.efuse_val3 = efuse_val3;
	g_asensor_info.efuse_val1_addr = 0x11EE0AA8;
	g_asensor_info.efuse_val2_addr = 0x11EE0AAC;
	g_asensor_info.efuse_val3_addr = 0x11EE0B6C;
	g_asensor_info.a_t0_lvt_rt = *a_t0_lvt_rt;
	g_asensor_info.a_t0_ulvt_rt = *a_t0_ulvt_rt;

	GPUFREQ_LOGD("efuse_val1=0x%08x, efuse_val2=0x%08x, efuse_val3=0x%08x, \
		*a_t0_lvt_rt=%08x, *a_t0_ulvt_rt=%08x, *a_t0_ulvtll_rt=%08x, *a_shift_error=%08x, \
		*efuse_error=%08x", efuse_val1, efuse_val2, efuse_val3, *a_t0_lvt_rt,
		*a_t0_ulvt_rt, *a_t0_ulvtll_rt, *a_shift_error, *efuse_error);

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

static void __gpufreq_asensor_read_register(u32 *a_tn_lvt_cnt, u32 *a_tn_ulvt_cnt, u32 *a_tn_ulvtll_cnt)
{
#if GPUFREQ_ASENSOR_ENABLE
	u32 aging_data0 = 0, aging_data1 =0;

	/*
	 * Enable sensor bclk cg
	 * MFG_SENSOR_BCLK_CG 0x13fb_ff98 = 0x0000_0001
	 */
	writel(0x1, g_g3d_base + 0xf98);

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

	*a_tn_ulvtll_cnt = (aging_data0 & 0xFFFF);
	*a_tn_lvt_cnt    = (aging_data0 & 0xFFFF0000) >> 16;
	*a_tn_ulvt_cnt   = (aging_data1 & 0xFFFF);

	g_asensor_info.a_tn_lvt_cnt = *a_tn_lvt_cnt;
	g_asensor_info.a_tn_ulvt_cnt = *a_tn_ulvt_cnt;
	g_asensor_info.a_tn_ulvtll_cnt = *a_tn_ulvtll_cnt;

	GPUFREQ_LOGD("aging_data0 = 0x%08x, aging_data1 = 0x%08x, *a_tn_lvt_cnt=0x%08x, \
		*a_tn_ulvt_cnt=0x%08x, *a_tn_ulvtll_cnt=0x%08x", aging_data0, aging_data1,
		*a_tn_lvt_cnt, *a_tn_ulvt_cnt);
#else
	GPUFREQ_UNREFERENCED(a_tn_lvt_cnt);
	GPUFREQ_UNREFERENCED(a_tn_ulvt_cnt);
	GPUFREQ_UNREFERENCED(a_tn_ulvtll_cnt);
#endif /* GPUFREQ_ASENSOR_ENABLE */
}

static unsigned int __gpufreq_get_aging_table_idx(u32 a_t0_lvt_rt, u32 a_t0_ulvt_rt,
	u32 a_t0_ulvtll_rt, u32 a_shift_error, u32 efuse_error, u32 a_tn_lvt_cnt, u32 a_tn_ulvt_cnt,
	u32 a_tn_ulvtll_cnt, unsigned int is_efuse_read_success)
{
	unsigned int aging_table_idx = GPUFREQ_AGING_MOST_AGRRESIVE_IDX;

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

	GPUFREQ_LOGD("tj1=%d, tj2=%d, tj=%d, adiff1=%d, adiff2=%d, adiff3=%d, adiff=%d, \
		leakage_power=%d, is_efuse_read_success=%d", tj1, tj2, tj, adiff1, adiff2,
		adiff, leakage_power, is_efuse_read_success);

	if (g_aging_load)
		aging_table_idx = GPUFREQ_AGING_MOST_AGRRESIVE_IDX;
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
		GPUFREQ_LOGW("non of the condition is true for aging_table_idx\n");
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
	aging_table_idx = MAX(aging_table_idx, GPUFREQ_AGING_MOST_AGRRESIVE_IDX);

	return aging_table_idx;
}

static void __gpufreq_aging_adjustment(void)
{
	int adj_num = 0;
	int i;
	u32 a_tn_lvt_cnt = 0, a_tn_ulvt_cnt = 0, a_tn_ulvtll_cnt = 0;
	u32 a_t0_lvt_rt = 0, a_t0_ulvt_rt = 0, a_t0_ulvtll_rt = 0;
	u32 a_shift_error = 0, efuse_error = 0;
	unsigned int is_efuse_read_success = false;
	unsigned int aging_table_idx = GPUFREQ_AGING_MOST_AGRRESIVE_IDX;
	struct gpufreq_adj_info *aging_adj = NULL;

	if (__gpufreq_dvfs_enable()) {
		/* park the volt to 0.65V for A sensor working */
		__gpufreq_pause_dvfs(GPUFREQ_AGING_KEEP_FREQ, GPUFREQ_AGING_KEEP_VOLT);

		/* get aging efuse data */
		is_efuse_read_success = __gpufreq_asensor_read_efuse(&a_t0_lvt_rt, &a_t0_ulvt_rt,
								&a_t0_ulvtll_rt, &a_shift_error, &efuse_error);

		/* get aging sensor data */
		__gpufreq_asensor_read_register(&a_tn_lvt_cnt, &a_tn_ulvt_cnt, &a_tn_ulvtll_cnt);

		/* resume DVFS */
		__gpufreq_resume_dvfs();

		/* compute aging_table_idx with aging efuse data and aging sensor data */
		aging_table_idx = __gpufreq_get_aging_table_idx(a_t0_lvt_rt, a_t0_ulvt_rt, a_t0_ulvtll_rt,
							a_shift_error, efuse_error, a_tn_lvt_cnt, a_tn_ulvt_cnt, a_tn_ulvtll_cnt,
							is_efuse_read_success);
	}


	g_asensor_info.aging_table_idx_most_agrresive = GPUFREQ_AGING_MOST_AGRRESIVE_IDX;
	g_asensor_info.aging_table_idx_choosed = aging_table_idx;

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

static void __gpufreq_avs_adjustment(void)
{
#if GPUFREQ_AVS_ENABLE
	u32 val = 0, temp_volt = 0, temp_freq = 0;
	int i = 0;
	int adj_num = AVS_ADJ_NUM;

	/*
	 * Read AVS efuse
	 *
	 * Freq (MHz) | Signedoff Volt (V) | Efuse name | Efuse address
	 * ============================================================
	 * 848        | 0.75               | PTPOD21    | 0x11EE_05D4
	 * 797        | 0.7                | PTPOD22    | 0x11EE_05D8
	 * 586        | 0.65               | PTPOD23    | 0x11EE_05DC
	 * 316        | 0.55               | PTPOD24    | 0x11EE_05E0
	 * 214        | 0.5                | PTPOD25    | 0x11EE_05E4
	 *
	 * the binning result of lowest voltage will be 0.45V from 214MHz/0.5V.
	 */

	for (i = 0; i < adj_num; i++) {
		val = readl(g_avs_efuse_base + (i * 0x4));

		/* Check Freq in efuse */
		temp_freq |= (val & 0x00100000) >> 10; // Get freq[10]  from efuse[20]
		temp_freq |= (val & 0x00000C00) >> 2;  // Get freq[9:8] from efuse[11:10]
		temp_freq |= (val & 0x00000003) << 6;  // Get freq[7:6] from efuse[1:0]
		temp_freq |= (val & 0x000000C0) >> 2;  // Get freq[5:4] from efuse[7:6]
		temp_freq |= (val & 0x000C0000) >> 16; // Get freq[3:2] from efuse[19:18]
		temp_freq |= (val & 0x00003000) >> 12; // Get freq[1:0] from efuse[13:12]

		if ((temp_freq * 1000) != g_gpu.signed_table[g_avs_adj[i].oppidx].freq)
			GPUFREQ_LOGW("OPP[%02d]: AVS efuse[%d].freq(%d) != signed-off.freq(%d)",
				g_avs_adj[i].oppidx, i, temp_freq,
				g_gpu.signed_table[g_avs_adj[i].oppidx].freq);

		/* Compute volt result of AVS */
		temp_volt |= (val & 0x0003C000) >> 14; // Get volt[3:0] from efuse[17:14]
		temp_volt |= (val & 0x00000030);       // Get volt[5:4] from efuse[5:4]
		temp_volt |= (val & 0x0000000C) << 4;  // Get volt[7:6] from efuse[3:2]
		/* Volt is stored in efuse with 6.25mv unit */
		g_avs_adj[i].volt = temp_volt * 625;
	}

	/* apply AVS to signed table */
	__gpufreq_apply_adjust(TARGET_STACK, g_avs_adj, adj_num);

	/* interpolate volt of non-sign-off OPP */
	__gpufreq_interpolate_volt(TARGET_STACK);
#endif /* GPUFREQ_AVS_ENABLE */
}

static void __gpufreq_segment_adjustment(struct platform_device *pdev)
{
	struct gpufreq_adj_info *segment_adj;
	int adj_num = 0;
	unsigned int efuse_id = 0x0;

	switch (efuse_id) {
	case 0x2:
		segment_adj = g_segment_adj_stack_1;
		adj_num = SEGMENT_ADJ_STACK_1_NUM;
		__gpufreq_apply_adjust(TARGET_STACK, segment_adj, adj_num);
		break;
	default:
		GPUFREQ_LOGW("unknown efuse id: 0x%x", efuse_id);
		break;
	}

	GPUFREQ_LOGI("efuse_id: 0x%x, adj_num: %d", efuse_id, adj_num);
}

static void __gpufreq_init_shader_present(void)
{
	unsigned int segment_id = 0;

	segment_id = g_gpu.segment_id;

	switch (segment_id) {
	case MT6981_SEGMENT:
	case MT6983_SEGMENT:
		g_shader_present = GPU_SHADER_PRESENT_9;
		break;
	default:
		g_shader_present = GPU_SHADER_PRESENT_9;
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
	g_gpu.cur_volt = __gpufreq_get_real_vgpu();
	g_gpu.cur_vsram = __gpufreq_get_real_vsram();

	g_stack.cur_freq = __gpufreq_get_real_fstack();
	g_stack.cur_volt = __gpufreq_get_real_vstack();
	g_stack.cur_vsram = __gpufreq_get_real_vsram();

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
		g_gpu.working_table[i].postdiv = g_gpu.signed_table[j].postdiv;
		g_gpu.working_table[i].vaging = g_gpu.signed_table[j].vaging;
		g_gpu.working_table[i].power = g_gpu.signed_table[j].power;

		GPUFREQ_LOGD("GPU[%02d] Freq: %d, Volt: %d, Vsram: %d, Vaging: %d",
			i, g_gpu.working_table[i].freq, g_gpu.working_table[i].volt,
			g_gpu.working_table[i].vsram, g_gpu.working_table[i].vaging);
	}

	/* set power info to working table */
	__gpufreq_measure_power(TARGET_GPU);

	/* init STACK OPP table */
	/* init OPP segment range */
	segment_id = g_stack.segment_id;
	if (segment_id == MT6983_SEGMENT)
		g_stack.segment_upbound = 0;
	else if (segment_id == MT6981_SEGMENT)
		g_stack.segment_upbound = 8;
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
		g_stack.working_table[i].postdiv = g_stack.signed_table[j].postdiv;
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
		__gpufreq_apply_aging(TARGET_STACK, true);

	/* set power info to working table */
	__gpufreq_measure_power(TARGET_STACK);

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
	unsigned int segment_id = 0;
	int ret = GPUFREQ_SUCCESS;

	switch (efuse_id) {
	default:
		segment_id = MT6983_SEGMENT;
		GPUFREQ_LOGW("unknown efuse id: 0x%x", efuse_id);
		break;
	}

	GPUFREQ_LOGI("efuse_id: 0x%x, segment_id: %d", efuse_id, segment_id);

	g_stack.segment_id = segment_id;

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
		__gpufreq_abort(GPUFREQ_FREQ_EXCEPTION,
			"fail to get clk_mux (%ld)", PTR_ERR(g_clk->clk_mux));
		ret = PTR_ERR(g_clk->clk_mux);
		goto done;
	}

	g_clk->clk_main_parent = devm_clk_get(&pdev->dev, "clk_main_parent");
	if (IS_ERR(g_clk->clk_main_parent)) {
		__gpufreq_abort(GPUFREQ_FREQ_EXCEPTION,
			"fail to get clk_main_parent (%ld)", PTR_ERR(g_clk->clk_main_parent));
		ret = PTR_ERR(g_clk->clk_main_parent);
		goto done;
	}

	g_clk->clk_sub_parent = devm_clk_get(&pdev->dev, "clk_sub_parent");
	if (IS_ERR(g_clk->clk_sub_parent)) {
		__gpufreq_abort(GPUFREQ_FREQ_EXCEPTION,
			"fail to get clk_sub_parent (%ld)", PTR_ERR(g_clk->clk_sub_parent));
		ret = PTR_ERR(g_clk->clk_sub_parent);
		goto done;
	}

	g_clk->subsys_mfg_cg = devm_clk_get(&pdev->dev, "subsys_mfg_cg");
	if (IS_ERR(g_clk->subsys_mfg_cg)) {
		__gpufreq_abort(GPUFREQ_FREQ_EXCEPTION,
			"fail to get subsys_mfg_cg (%ld)", PTR_ERR(g_clk->subsys_mfg_cg));
		ret = PTR_ERR(g_clk->subsys_mfg_cg);
		goto done;
	}

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

	/* VGPU is co-buck with VCORE, so use DVFSRC to control VGPU */
	g_pmic->reg_vgpu = regulator_get_optional(&pdev->dev, "_vgpu");
	if (IS_ERR(g_pmic->reg_vgpu)) {
		__gpufreq_abort(GPUFREQ_FREQ_EXCEPTION,
			"fail to get VGPU (%ld)", PTR_ERR(g_pmic->reg_vgpu));
		ret = PTR_ERR(g_pmic->reg_vgpu);
		goto done;
	}

	g_pmic->reg_vstack = regulator_get_optional(&pdev->dev, "_vstack");
	if (IS_ERR(g_pmic->reg_vstack)) {
		__gpufreq_abort(GPUFREQ_FREQ_EXCEPTION,
			"fail to get VSATCK (%ld)", PTR_ERR(g_pmic->reg_vstack));
		ret = PTR_ERR(g_pmic->reg_vstack);
		goto done;
	}

	/* VSRAM is co-buck and controlled by SRAMRC, but use regulator to get Volt */
	g_pmic->reg_vsram = regulator_get_optional(&pdev->dev, "_vsram");
	if (IS_ERR(g_pmic->reg_vsram)) {
		__gpufreq_abort(GPUFREQ_FREQ_EXCEPTION,
			"fail to get VSRAM (%ld)", PTR_ERR(g_pmic->reg_vsram));
		ret = PTR_ERR(g_pmic->reg_vsram);
		goto done;
	}

done:
	GPUFREQ_TRACE_END();

	return ret;
}

/* API: init reg base address and flavor config of the platform */
static int __gpufreq_init_platform_info(struct platform_device *pdev)
{
	struct device_node *of_gpufreq = pdev->dev.of_node;
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("pdev=0x%x", pdev);

	if (!of_gpufreq) {
		ret = GPUFREQ_ENOENT;
		__gpufreq_abort(GPUFREQ_FREQ_EXCEPTION, "fail to find gpufreq of_node (ENOENT)");
		goto done;
	}

	/* ignore return error and use default value if property doesn't exist */
	of_property_read_u32(of_gpufreq, "aging-load", &g_aging_load);

	/* return error should be handled */
	/* 0x13FA0000 */
	g_mfg_pll_base = __gpufreq_of_ioremap("mediatek,mfg_pll", 0);
	if (!g_mfg_pll_base) {
		GPUFREQ_LOGE("fail to ioremap mfg_pll (ENOENT)");
		ret = GPUFREQ_ENOENT;
		goto done;
	}

	/* 0x13FA0C00 */
	g_mfg_scpll_base = __gpufreq_of_ioremap("mediatek,mfg_scpll", 0);
	if (!g_mfg_scpll_base) {
		GPUFREQ_LOGE("fail to ioremap mfg_pll (ENOENT)");
		ret = GPUFREQ_ENOENT;
		goto done;
	}

	/* 0x13F90000 */
	g_mfg_rpc_base = __gpufreq_of_ioremap("mediatek,mfg_rpc", 0);
	if (!g_mfg_rpc_base) {
		GPUFREQ_LOGE("fail to ioremap mfg_rpc (ENOENT)");
		ret = GPUFREQ_ENOENT;
		goto done;
	}

	/* 0x13FBF000 */
	g_g3d_base = __gpufreq_of_ioremap("mediatek,g3d_config", 0);
	if (!g_g3d_base) {
		GPUFREQ_LOGE("fail to ioremap g3d_config (ENOENT)");
		ret = GPUFREQ_ENOENT;
		goto done;
	}

	/* 0x10006000 */
	g_sleep = __gpufreq_of_ioremap("mediatek,sleep", 0);
	if (!g_sleep) {
		__gpufreq_abort(GPUFREQ_FREQ_EXCEPTION, "fail to ioremap sleep (ENOENT)");
		ret = GPUFREQ_ENOENT;
		goto done;
	}

	g_infracfg_base = of_iomap(of_gpufreq, 0);
	if (!g_infracfg_base) {
		__gpufreq_abort(GPUFREQ_FREQ_EXCEPTION, "fail to ioremap infracfg (ENOENT)");
		ret = GPUFREQ_ENOENT;
		goto done;
	}

	g_infra_bpi_bsi_slv0 = of_iomap(of_gpufreq, 1);
	if (!g_infra_bpi_bsi_slv0) {
		__gpufreq_abort(GPUFREQ_FREQ_EXCEPTION, "fail to ioremap bpi_bsi_slv0 (ENOENT)");
		ret = GPUFREQ_ENOENT;
		goto done;
	}

	g_infra_peri_debug1 = of_iomap(of_gpufreq, 2);
	if (!g_infra_peri_debug1) {
		__gpufreq_abort(GPUFREQ_FREQ_EXCEPTION,
			"fail to ioremap devapc_ao_infra_peri_debug1 (ENOENT)");
		ret = GPUFREQ_ENOENT;
		goto done;
	}

	g_infra_peri_debug2 = of_iomap(of_gpufreq, 3);
	if (!g_infra_peri_debug2) {
		__gpufreq_abort(GPUFREQ_FREQ_EXCEPTION,
			"fail to ioremap devapc_ao_infra_peri_debug2 (ENOENT)");
		ret = GPUFREQ_ENOENT;
		goto done;
	}

	g_infra_peri_debug3 = of_iomap(of_gpufreq, 4);
	if (!g_infra_peri_debug3) {
		__gpufreq_abort(GPUFREQ_FREQ_EXCEPTION,
			"fail to ioremap devapc_ao_infra_peri_debug3 (ENOENT)");
		ret = GPUFREQ_ENOENT;
		goto done;
	}

	g_infra_peri_debug4 = of_iomap(of_gpufreq, 5);
	if (!g_infra_peri_debug4) {
		__gpufreq_abort(GPUFREQ_FREQ_EXCEPTION,
			"fail to ioremap devapc_ao_infra_peri_debug4 (ENOENT)");
		ret = GPUFREQ_ENOENT;
		goto done;
	}

#if GPUFREQ_AVS_ENABLE
	/*
	 * todo:
     * 1. Add reg of 0x11EE_05D4 for ioremap - g_avs_efuse_base
	 * 2. Correct the idx of __gpufreq_of_ioremap
	 */
	g_avs_efuse_base = of_iomap(of_gpufreq, 6);
	if (!g_avs_efuse_base) {
		__gpufreq_abort(GPUFREQ_FREQ_EXCEPTION,
			"fail to ioremap avs_efuse (ENOENT)");
		ret = GPUFREQ_ENOENT;
		goto done;
	}
#endif

#if GPUFREQ_ASENSOR_ENABLE
	/*
	 * todo:
	 * 1. Add reg = <0 0x13FB9C00 0 0x100> for ioremap
	 * 2. Correct the idx of __gpufreq_of_ioremap
	 */
	g_mfg_cpe_control_base = of_iomap(of_gpufreq, 7);
	if (!g_mfg_cpe_control_base) {
		__gpufreq_abort(GPUFREQ_FREQ_EXCEPTION,
			"fail to ioremap mfg_cpe_control (ENOENT)");
		ret = GPUFREQ_ENOENT;
		goto done;
	}
	/*
	 * todo:
	 * 1. Add reg = <0 0x13FB6000 0 0x1000> for ioremap
	 * 2. Correct the idx of __gpufreq_of_ioremap
	 */
	g_mfg_cpe_sensor_base = of_iomap(of_gpufreq, 8);
	if (!g_mfg_cpe_sensor_base) {
		__gpufreq_abort(GPUFREQ_FREQ_EXCEPTION,
			"fail to ioremap cpe_sensor (ENOENT)");
		ret = GPUFREQ_ENOENT;
		goto done;
	}
	/*
	 * todo:
	 * 1. Add reg = <0 0x11EE0000 0 0x1000> for ioremap
	 * 2. Correct the idx of __gpufreq_of_ioremap
	 */
	g_cpe_0p65v_rt_blow_base = of_iomap(of_gpufreq, 9);
	if (!g_cpe_0p65v_rt_blow_base) {
		__gpufreq_abort(GPUFREQ_FREQ_EXCEPTION,
			"fail to ioremap cpe_0p65v_rt_blow (ENOENT)");
		ret = GPUFREQ_ENOENT;
		goto done;
	}
#endif

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
		__gpufreq_dump_bringup_status();
		goto done;
	}

	/* init reg base address and flavor config of the platform in both AP and EB mode */
	ret = __gpufreq_init_platform_info(pdev);
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to init platform info (%d)", ret);
		goto done;
	}

	/* probe only init register base in EB mode */
	if (g_gpueb_support) {
		GPUFREQ_LOGI("skip gpufreq platform driver probe when in EB mode");
		goto done;
	}

	/* init pmic regulator */
	ret = __gpufreq_init_pmic(pdev);
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to init pmic (%d)", ret);
		goto done;
	}

	/* init clock source */
	ret = __gpufreq_init_clk(pdev);
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to init clk (%d)", ret);
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

	/* init OPP table */
	ret = __gpufreq_init_opp_table(pdev);
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to init OPP table (%d)", ret);
		goto done;
	}

	/* default enable GPM 1.0, before power control */
	g_gpm_enable = true;

	/* power on and never off if disable power control */
	if (!__gpufreq_power_ctrl_enable()) {
		GPUFREQ_LOGI("power control always on");
		ret = __gpufreq_power_control(POWER_ON);
		if (unlikely(ret < 0)) {
			GPUFREQ_LOGE("fail to control power state: %d (%d)", POWER_ON, ret);
			goto done;
		}
	}

	/* init first OPP index by current freq and volt */
	ret = __gpufreq_init_opp_idx();
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to init OPP index (%d)", ret);
		goto done;
	}

	GPUFREQ_LOGI("gpufreq platform driver probe done");

done:
	return ret;
}

static int __gpufreq_mfg2_probe(struct platform_device *pdev)
{
	if (!pdev->dev.pm_domain) {
		GPUFREQ_LOGE("fail to get mfg2 pm_domain");
		return -EPROBE_DEFER;
	}

	g_mtcmos->mfg2_pdev = pdev;
	pm_runtime_enable(&pdev->dev);
	dev_pm_syscore_device(&pdev->dev, true);

	return GPUFREQ_SUCCESS;
}

static int __gpufreq_mfg3_probe(struct platform_device *pdev)
{
	if (!pdev->dev.pm_domain) {
		GPUFREQ_LOGE("fail to get mfg3 pm_domain");
		return -EPROBE_DEFER;
	}

	g_mtcmos->mfg3_pdev = pdev;
	pm_runtime_enable(&pdev->dev);
	dev_pm_syscore_device(&pdev->dev, true);

	return GPUFREQ_SUCCESS;
}

static int __gpufreq_mfg4_probe(struct platform_device *pdev)
{
	if (!pdev->dev.pm_domain) {
		GPUFREQ_LOGE("fail to get mfg4 pm_domain");
		return -EPROBE_DEFER;
	}

	g_mtcmos->mfg4_pdev = pdev;
	pm_runtime_enable(&pdev->dev);
	dev_pm_syscore_device(&pdev->dev, true);

	return GPUFREQ_SUCCESS;
}

static int __gpufreq_mfg5_probe(struct platform_device *pdev)
{
	if (!pdev->dev.pm_domain) {
		GPUFREQ_LOGE("fail to get mfg5 pm_domain");
		return -EPROBE_DEFER;
	}

	g_mtcmos->mfg5_pdev = pdev;
	pm_runtime_enable(&pdev->dev);
	dev_pm_syscore_device(&pdev->dev, true);

	return GPUFREQ_SUCCESS;
}

static int __gpufreq_mfg6_probe(struct platform_device *pdev)
{
	if (!pdev->dev.pm_domain) {
		GPUFREQ_LOGE("fail to get mfg6 pm_domain");
		return -EPROBE_DEFER;
	}

	g_mtcmos->mfg6_pdev = pdev;
	pm_runtime_enable(&pdev->dev);
	dev_pm_syscore_device(&pdev->dev, true);

	return GPUFREQ_SUCCESS;
}

static int __gpufreq_mfg_remove(struct platform_device *pdev)
{
	pm_runtime_disable(&pdev->dev);

	return GPUFREQ_SUCCESS;
}

static int __gpufreq_mtcmos_pdrv_probe(struct platform_device *pdev)
{
	const struct gpufreq_mfg_fp *mfg_fp;
	int ret = GPUFREQ_SUCCESS;

	/* keep probe successful but do nothing */
	if (__gpufreq_bringup() || g_gpueb_support) {
		GPUFREQ_LOGI("skip gpufreq mtcmos probe when bringup or in EB mode");
		goto done;
	}

	if (!g_mtcmos) {
		g_mtcmos = kzalloc(sizeof(struct gpufreq_mtcmos_info), GFP_KERNEL);
		if (!g_mtcmos) {
			GPUFREQ_LOGE("fail to alloc gpufreq_mtcmos_info (ENOMEM)");
			ret = GPUFREQ_ENOMEM;
			goto done;
		}
	}

	mfg_fp = of_device_get_match_data(&pdev->dev);
	if (!mfg_fp) {
		GPUFREQ_LOGE("fail to get mtcmos match data");
		ret = GPUFREQ_EINVAL;
		goto done;
	}

	ret = mfg_fp->probe(pdev);
	if (unlikely(ret))
		GPUFREQ_LOGE("fail to probe mtcmos device");

done:
	return ret;
}

static int __gpufreq_mtcmos_pdrv_remove(struct platform_device *pdev)
{
	const struct gpufreq_mfg_fp *mfg_fp;
	int ret = GPUFREQ_SUCCESS;

	/* skip remove because do nothing in probe */
	if (__gpufreq_bringup() || g_gpueb_support) {
		GPUFREQ_LOGI("skip gpufreq mtcmos remove when bringup or in EB mode");
		goto done;
	}

	mfg_fp = of_device_get_match_data(&pdev->dev);
	if (!mfg_fp) {
		GPUFREQ_LOGE("fail to get mtcmos match data");
		ret = GPUFREQ_EINVAL;
		goto done;
	}

	ret = mfg_fp->remove(pdev);
	if (unlikely(ret))
		GPUFREQ_LOGE("fail to remove mtcmos device");

done:
	return ret;
}

/* API: register gpufreq platform driver */
static int __init __gpufreq_init(void)
{
	struct device_node *gpueb;
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_LOGI("start to init gpufreq platform driver");

	gpueb = of_find_compatible_node(NULL, NULL, "mediatek,gpueb");
	if (!gpueb) {
		GPUFREQ_LOGE("fail to find gpueb node");
		ret = GPUFREQ_ENOENT;
		goto done;
	}

	ret = of_property_read_u32(gpueb, "gpueb-support", &g_gpueb_support);
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to read gpueb-support (%d)", ret);
		goto done;
	}

	/* register gpufreq mtcmos driver */
	ret = platform_driver_register(&g_gpufreq_mtcmos_pdrv);
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to register gpufreq mtcmos driver\n");
		goto done;
	}

	/* register gpufreq platform driver */
	ret = platform_driver_register(&g_gpufreq_pdrv);
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to register gpufreq platform driver (%d)",
			ret);
		goto done;
	}

	if (__gpufreq_bringup()) {
		GPUFREQ_LOGI("skip the rest of gpufreq platform init when bringup");
		goto done;
	}

	/* register gpufreq platform function to wrapper in both AP and EB mode */
	gpufreq_register_gpufreq_fp(&platform_fp);
	gpufreq_debug_register_gpufreq_fp(&platform_fp);

	/* init gpu ppm */
	ret = gpuppm_init(g_gpueb_support);
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to init gpuppm (%d)", ret);
		goto done;
	}

	__gpufreq_footprint_vgpu_reset();
	__gpufreq_footprint_oppidx_reset();
	__gpufreq_footprint_power_count_reset();

	GPUFREQ_LOGI("gpufreq platform driver init done");

done:
	return ret;
}

/* API: unregister gpufreq driver */
static void __exit __gpufreq_exit(void)
{
	kfree(g_gpu.working_table);
	kfree(g_stack.working_table);
	kfree(g_clk);
	kfree(g_pmic);
	kfree(g_mtcmos);

	platform_driver_unregister(&g_gpufreq_pdrv);
	platform_driver_unregister(&g_gpufreq_mtcmos_pdrv);
}

module_init(__gpufreq_init);
module_exit(__gpufreq_exit);

MODULE_DEVICE_TABLE(of, g_gpufreq_of_match);
MODULE_DEVICE_TABLE(of, g_gpufreq_mtcmos_of_match);
MODULE_DESCRIPTION("MediaTek GPU-DVFS platform driver");
MODULE_LICENSE("GPL");
