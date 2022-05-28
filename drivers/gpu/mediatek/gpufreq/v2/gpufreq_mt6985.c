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
#include <linux/timekeeping.h>

#include <gpufreq_v2.h>
#include <gpufreq_mssv.h>
#include <gpuppm.h>
#include <gpufreq_common.h>
#include <gpufreq_mt6985.h>
#include <gpufreq_reg_mt6985.h>
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
static void __gpufreq_set_margin_mode(unsigned int val);
static void __gpufreq_set_gpm_mode(unsigned int version, unsigned int val);
static void __gpufreq_apply_restore_margin(enum gpufreq_target target, unsigned int val);
static void __gpufreq_measure_power(void);
static void __iomem *__gpufreq_of_ioremap(const char *node_name, int idx);
static int __gpufreq_pause_dvfs(void);
static void __gpufreq_resume_dvfs(void);
static void __gpufreq_update_shared_status_opp_table(void);
static void __gpufreq_update_shared_status_adj_table(void);
static void __gpufreq_update_shared_status_init_reg(void);
static void __gpufreq_update_shared_status_power_reg(void);
static void __gpufreq_update_shared_status_active_idle_reg(void);
static void __gpufreq_update_shared_status_dvfs_reg(void);
#if GPUFREQ_MSSV_TEST_MODE
static void __gpufreq_mssv_set_stack_sel(unsigned int val);
static void __gpufreq_mssv_set_del_sel(unsigned int val);
#endif /* GPUFREQ_MSSV_TEST_MODE */
/* dvfs function */
static void __gpufreq_dvfs_sel_config(enum gpufreq_opp_direct direct, unsigned int volt);
static void __gpufreq_get_park_volt(enum gpufreq_opp_direct direct,
	unsigned int cur_vstack, unsigned int *vgpu_park, unsigned int *vstack_park);
static void __gpufreq_update_springboard(void);
static int __gpufreq_generic_scale(
	unsigned int fgpu_old, unsigned int fgpu_new,
	unsigned int vgpu_old, unsigned int vgpu_new,
	unsigned int fstack_old, unsigned int fstack_new,
	unsigned int vstack_old, unsigned int vstack_new,
	unsigned int vsram_old, unsigned int vsram_new);
static int __gpufreq_custom_commit_gpu(unsigned int target_freq,
	unsigned int target_volt, enum gpufreq_dvfs_state key);
static int __gpufreq_custom_commit_stack(unsigned int target_freq,
	unsigned int target_volt, enum gpufreq_dvfs_state key);
static int __gpufreq_freq_scale_gpu(unsigned int freq_old, unsigned int freq_new);
static int __gpufreq_freq_scale_stack(unsigned int freq_old, unsigned int freq_new);
static int __gpufreq_volt_scale_gpu(unsigned int volt_old, unsigned int volt_new);
static int __gpufreq_volt_scale_stack(unsigned int volt_old, unsigned int volt_new);
static int __gpufreq_volt_scale_sram(unsigned int volt_old, unsigned int volt_new);
static int __gpufreq_switch_clksrc(enum gpufreq_target target, enum gpufreq_clk_src clksrc);
static unsigned int __gpufreq_calculate_pcw(unsigned int freq, enum gpufreq_posdiv posdiv_power);
static unsigned int __gpufreq_settle_time_vgpu_vstack(enum gpufreq_opp_direct direct, int deltaV);
static unsigned int __gpufreq_settle_time_vsram(enum gpufreq_opp_direct direct, int deltaV);
/* get function */
static unsigned int __gpufreq_get_fmeter_freq(enum gpufreq_target target);
static unsigned int __gpufreq_get_fmeter_main_fgpu(void);
static unsigned int __gpufreq_get_fmeter_main_fstack(void);
static unsigned int __gpufreq_get_fmeter_sub_fgpu(void);
static unsigned int __gpufreq_get_fmeter_sub_fstack(void);
static unsigned int __gpufreq_get_real_fgpu(void);
static unsigned int __gpufreq_get_real_fstack(void);
static unsigned int __gpufreq_get_real_vgpu(void);
static unsigned int __gpufreq_get_real_vstack(void);
static unsigned int __gpufreq_get_real_vsram(void);
static unsigned int __gpufreq_get_vsram_by_vlogic(unsigned int volt);
static void __gpufreq_set_semaphore(enum gpufreq_sema_op op);
static enum gpufreq_posdiv __gpufreq_get_real_posdiv_gpu(void);
static enum gpufreq_posdiv __gpufreq_get_real_posdiv_stack(void);
static enum gpufreq_posdiv __gpufreq_get_posdiv_by_freq(unsigned int freq);
/* aging sensor function */
#if GPUFREQ_ASENSOR_ENABLE
static unsigned int __gpufreq_asensor_read_efuse(u32 *a_t0_efuse1,
	u32 *a_t0_efuse2, u32 *a_t0_efuse3, u32 *efuse_error);
static void __gpufreq_asensor_read_register(u32 *a_tn_sensor1,
	u32 *a_tn_sensor2, u32 *a_tn_sensor3);
static unsigned int __gpufreq_get_aging_table_idx(
	u32 a_t0_efuse1, u32 a_t0_efuse2, u32 a_t0_efuse3,
	u32 a_tn_sensor1, u32 a_tn_sensor2, u32 a_tn_sensor3,
	u32 efuse_error, unsigned int is_efuse_read_success);
#endif /* GPUFREQ_ASENSOR_ENABLE */
/* power control function */
#if GPUFREQ_SELF_CTRL_MTCMOS
static void __gpufreq_mfgx_rpc_control(enum gpufreq_power_state power, void __iomem *pwr_con);
static void __gpufreq_mfg1_rpc_control(enum gpufreq_power_state power);
#endif /* GPUFREQ_SELF_CTRL_MTCMOS */
static int __gpufreq_clock_control(enum gpufreq_power_state power);
static int __gpufreq_mtcmos_control(enum gpufreq_power_state power);
static int __gpufreq_buck_control(enum gpufreq_power_state power);
static void __gpufreq_aoc_config(enum gpufreq_power_state power);
static void __gpufreq_hwdcm_config(void);
static void __gpufreq_acp_config(void);
static void __gpufreq_gpm1_config(void);
static void __gpufreq_transaction_config(void);
static void __gpufreq_dfd_config(void);
static void __gpufreq_mfg_backup_restore(enum gpufreq_power_state power);
/* bringup function */
static unsigned int __gpufreq_bringup(void);
static void __gpufreq_dump_bringup_status(struct platform_device *pdev);
/* init function */
static void __gpufreq_interpolate_volt(enum gpufreq_target target);
static void __gpufreq_apply_adjustment(void);
static unsigned int __gpufreq_compute_avs_freq(u32 val);
static unsigned int __gpufreq_compute_avs_volt(u32 val);
static void __gpufreq_compute_avs(void);
static void __gpufreq_compute_aging(void);
static int __gpufreq_init_opp_idx(void);
static void __gpufreq_init_opp_table(void);
static void __gpufreq_init_shader_present(void);
static void __gpufreq_init_segment_id(void);
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

static void __iomem *g_mfg_top_base;
static void __iomem *g_mfg_pll_base;
static void __iomem *g_mfgsc_pll_base;
static void __iomem *g_mfg_rpc_base;
static void __iomem *g_sleep;
static void __iomem *g_topckgen_base;
static void __iomem *g_nth_emicfg_base;
static void __iomem *g_sth_emicfg_base;
static void __iomem *g_nth_emicfg_ao_mem_base;
static void __iomem *g_sth_emicfg_ao_mem_base;
static void __iomem *g_ifrbus_ao_base;
static void __iomem *g_infra_ao_debug_ctrl;
static void __iomem *g_infra_ao1_debug_ctrl;
static void __iomem *g_nth_emi_ao_debug_ctrl;
static void __iomem *g_sth_emi_ao_debug_ctrl;
static void __iomem *g_efuse_base;
static void __iomem *g_mfg_cpe_ctrl_mcu_base;
static void __iomem *g_mfg_cpe_sensor_base;
static void __iomem *g_mfg_secure_base;
static void __iomem *g_drm_debug_base;
static void __iomem *g_mali_base;
static struct gpufreq_pmic_info *g_pmic;
static struct gpufreq_clk_info *g_clk;
static struct gpufreq_mtcmos_info *g_mtcmos;
static struct gpufreq_status g_gpu;
static struct gpufreq_status g_stack;
static struct gpufreq_asensor_info g_asensor_info;
static struct gpufreq_volt_sb g_springboard[NUM_CONSTRAINT_IDX];
static unsigned int g_shader_present;
static unsigned int g_mcl50_load;
static unsigned int g_aging_table_idx;
static unsigned int g_asensor_enable;
static unsigned int g_aging_load;
static unsigned int g_aging_margin;
static unsigned int g_avs_enable;
static unsigned int g_avs_margin;
static unsigned int g_gpm1_mode;
static unsigned int g_gpm3_mode;
static unsigned int g_ptp_version;
static unsigned int g_dfd_mode;
static unsigned int g_gpueb_support;
static unsigned int g_gpufreq_ready;
static unsigned int g_stack_sel_reg;
static unsigned int g_del_sel_reg;
static enum gpufreq_dvfs_state g_dvfs_state;
static struct gpufreq_shared_status *g_shared_status;
static DEFINE_MUTEX(gpufreq_lock);

static struct gpufreq_platform_fp platform_ap_fp = {
	.power_ctrl_enable = __gpufreq_power_ctrl_enable,
	.active_idle_ctrl_enable = __gpufreq_active_idle_ctrl_enable,
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
	.get_fgpu_by_idx = __gpufreq_get_fgpu_by_idx,
	.get_pgpu_by_idx = __gpufreq_get_pgpu_by_idx,
	.get_idx_by_fgpu = __gpufreq_get_idx_by_fgpu,
	.get_lkg_pgpu = __gpufreq_get_lkg_pgpu,
	.get_dyn_pgpu = __gpufreq_get_dyn_pgpu,
	.power_control = __gpufreq_power_control,
	.active_idle_control = __gpufreq_active_idle_control,
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
	.set_mfgsys_config = __gpufreq_set_mfgsys_config,
	.get_core_mask_table = __gpufreq_get_core_mask_table,
	.get_core_num = __gpufreq_get_core_num,
	.pdca_config = __gpufreq_pdca_config,
	.fake_mtcmos_control = __gpufreq_fake_mtcmos_control,
	.update_debug_opp_info = __gpufreq_update_debug_opp_info,
	.set_shared_status = __gpufreq_set_shared_status,
	.mssv_commit = __gpufreq_mssv_commit,
};

static struct gpufreq_platform_fp platform_eb_fp = {
	.set_timestamp = __gpufreq_set_timestamp,
	.check_bus_idle = __gpufreq_check_bus_idle,
	.dump_infra_status = __gpufreq_dump_infra_status,
	.get_dyn_pgpu = __gpufreq_get_dyn_pgpu,
	.get_dyn_pstack = __gpufreq_get_dyn_pstack,
	.get_core_mask_table = __gpufreq_get_core_mask_table,
	.get_core_num = __gpufreq_get_core_num,
	.pdca_config = __gpufreq_pdca_config,
	.fake_mtcmos_control = __gpufreq_fake_mtcmos_control,
};

/**
 * ===============================================
 * External Function Definition
 * ===============================================
 */
/* API: get POWER_CTRL enable status */
unsigned int __gpufreq_power_ctrl_enable(void)
{
	return GPUFREQ_POWER_CTRL_ENABLE;
}

/* API: get ACTIVE_IDLE_CTRL status */
unsigned int __gpufreq_active_idle_ctrl_enable(void)
{
	return GPUFREQ_ACTIVE_IDLE_CTRL_ENABLE && GPUFREQ_POWER_CTRL_ENABLE;
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

/* API: get GPU shader core setting */
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

/* API: get pointer of working OPP table of GPU */
const struct gpufreq_opp_info *__gpufreq_get_working_table_gpu(void)
{
	return g_gpu.working_table;
}

/* API: get pointer of working OPP table of STACK */
const struct gpufreq_opp_info *__gpufreq_get_working_table_stack(void)
{
	return g_stack.working_table;
}

/* API: get pointer of signed OPP table of GPU */
const struct gpufreq_opp_info *__gpufreq_get_signed_table_gpu(void)
{
	return g_gpu.signed_table;
}

/* API: get pointer of signed OPP table of STACK */
const struct gpufreq_opp_info *__gpufreq_get_signed_table_stack(void)
{
	return g_stack.signed_table;
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

/* API: get leakage Power of GPU */
unsigned int __gpufreq_get_lkg_pgpu(unsigned int volt)
{
	GPUFREQ_UNREFERENCED(volt);

	return GPU_LKG_POWER;
}

/* API: get dynamic Power of GPU */
unsigned int __gpufreq_get_dyn_pgpu(unsigned int freq, unsigned int volt)
{
	unsigned int p_dynamic = GPU_DYN_REF_POWER;
	unsigned int ref_freq = GPU_DYN_REF_FREQ;
	unsigned int ref_volt = GPU_DYN_REF_VOLT;

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

	return STACK_LKG_POWER;
}

/* API: get dynamic Power of STACK */
unsigned int __gpufreq_get_dyn_pstack(unsigned int freq, unsigned int volt)
{
	unsigned int p_dynamic = STACK_DYN_REF_POWER;
	unsigned int ref_freq = STACK_DYN_REF_FREQ;
	unsigned int ref_volt = STACK_DYN_REF_VOLT;

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
	u64 power_time = 0;

	GPUFREQ_TRACE_START("power=%d", power);

	mutex_lock(&gpufreq_lock);

	GPUFREQ_LOGD("+ PWR_STATUS: 0x%08x", MFG_0_19_PWR_STATUS);
	GPUFREQ_LOGD("switch power: %s (Power: %d, Active: %d, Buck: %d, MTCMOS: %d, CG: %d)",
		power ? "On" : "Off", g_stack.power_count,
		g_stack.active_count, g_stack.buck_count,
		g_stack.mtcmos_count, g_stack.cg_count);

	if (power == POWER_ON) {
		g_gpu.power_count++;
		g_stack.power_count++;
	} else {
		g_gpu.power_count--;
		g_stack.power_count--;
	}
	__gpufreq_footprint_power_count(g_stack.power_count);

	if (power == POWER_ON && g_stack.power_count == 1) {
		__gpufreq_footprint_power_step(0x01);

		/* enable Buck */
		ret = __gpufreq_buck_control(POWER_ON);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to enable Buck (%d)", ret);
			ret = GPUFREQ_EINVAL;
			goto done_unlock;
		}
		__gpufreq_footprint_power_step(0x02);

		/* clear AOC ISO/LATCH after Buck on */
		__gpufreq_aoc_config(POWER_ON);
		__gpufreq_footprint_power_step(0x03);

		/* enable MTCMOS */
		ret = __gpufreq_mtcmos_control(POWER_ON);
		if (unlikely(ret < 0)) {
			GPUFREQ_LOGE("fail to enable MTCMOS (%d)", ret);
			ret = GPUFREQ_EINVAL;
			goto done_unlock;
		}
		__gpufreq_footprint_power_step(0x04);

		/* enable Clock */
		ret = __gpufreq_clock_control(POWER_ON);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to enable Clock (%d)", ret);
			ret = GPUFREQ_EINVAL;
			goto done_unlock;
		}
		__gpufreq_footprint_power_step(0x05);

		/* restore MFG registers */
		__gpufreq_mfg_backup_restore(POWER_ON);
		__gpufreq_footprint_power_step(0x06);

		/* set PDCA register when power on and let GPU DDK control MTCMOS */
		__gpufreq_pdca_config(POWER_ON);
		__gpufreq_footprint_power_step(0x07);

		/* config ACP */
		__gpufreq_acp_config();
		__gpufreq_footprint_power_step(0x08);

		/* config HWDCM */
		__gpufreq_hwdcm_config();
		__gpufreq_footprint_power_step(0x09);

		/* config GPM 1.0 */
		__gpufreq_gpm1_config();
		__gpufreq_footprint_power_step(0x0A);

		/* config AXI transaction */
		__gpufreq_transaction_config();
		__gpufreq_footprint_power_step(0x0B);

		/* config DFD */
		__gpufreq_dfd_config();
		__gpufreq_footprint_power_step(0x0C);

		/* free DVFS when power on */
		g_dvfs_state &= ~DVFS_POWEROFF;
		__gpufreq_footprint_power_step(0x0D);
	} else if (power == POWER_OFF && g_stack.power_count == 0) {
		__gpufreq_footprint_power_step(0x0E);

		/* freeze DVFS when power off */
		g_dvfs_state |= DVFS_POWEROFF;
		__gpufreq_footprint_power_step(0x0F);

		/* backup MFG registers */
		__gpufreq_mfg_backup_restore(POWER_OFF);
		__gpufreq_footprint_power_step(0x10);

		/* disable Clock */
		ret = __gpufreq_clock_control(POWER_OFF);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to disable Clock (%d)", ret);
			ret = GPUFREQ_EINVAL;
			goto done_unlock;
		}
		__gpufreq_footprint_power_step(0x11);

		/* disable MTCMOS */
		ret = __gpufreq_mtcmos_control(POWER_OFF);
		if (unlikely(ret < 0)) {
			GPUFREQ_LOGE("fail to disable MTCMOS (%d)", ret);
			ret = GPUFREQ_EINVAL;
			goto done_unlock;
		}
		__gpufreq_footprint_power_step(0x12);

		/* set AOC ISO/LATCH before Buck off */
		__gpufreq_aoc_config(POWER_OFF);
		__gpufreq_footprint_power_step(0x13);

		/* disable Buck */
		ret = __gpufreq_buck_control(POWER_OFF);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to disable Buck (%d)", ret);
			ret = GPUFREQ_EINVAL;
			goto done_unlock;
		}
		__gpufreq_footprint_power_step(0x14);
	}

	/* return power count if successfully control power */
	ret = g_stack.power_count;
	/* record time of successful power control */
	power_time = ktime_get_ns();

	/* update current status to shared memory */
	if (g_shared_status) {
		g_shared_status->dvfs_state = g_dvfs_state;
		g_shared_status->power_count = g_stack.power_count;
		g_shared_status->active_count = g_stack.active_count;
		g_shared_status->buck_count = g_stack.buck_count;
		g_shared_status->mtcmos_count = g_stack.mtcmos_count;
		g_shared_status->cg_count = g_stack.cg_count;
		g_shared_status->power_time_h = (power_time >> 32) & GENMASK(31, 0);
		g_shared_status->power_time_l = power_time & GENMASK(31, 0);
		if (power == POWER_ON)
			__gpufreq_update_shared_status_power_reg();
	}

	if (power == POWER_ON)
		__gpufreq_footprint_power_step(0x15);
	else if (power == POWER_OFF)
		__gpufreq_footprint_power_step(0x16);

done_unlock:
	GPUFREQ_LOGD("- PWR_STATUS: 0x%08x", MFG_0_19_PWR_STATUS);

	mutex_unlock(&gpufreq_lock);

	GPUFREQ_TRACE_END();

	return ret;
}

/*
 * API: control runtime active-idle state of GPU
 * return active_count if success
 * return GPUFREQ_EINVAL if failure
 */
int __gpufreq_active_idle_control(enum gpufreq_power_state power)
{
	int ret = 0;

	GPUFREQ_TRACE_START("power=%d", power);

	mutex_lock(&gpufreq_lock);

	GPUFREQ_LOGD("switch runtime state: %s (Active: %d)",
		power ? "Active" : "Idle", g_stack.active_count);

	/* active-idle control is only available at power-on */
	if (__gpufreq_get_power_state() == POWER_OFF)
		__gpufreq_abort("switch active-idle at power-off (%d)", g_stack.power_count);

	if (power == POWER_ON) {
		g_gpu.active_count++;
		g_stack.active_count++;
	} else {
		g_gpu.active_count--;
		g_stack.active_count--;
	}

	if (power == POWER_ON && g_stack.active_count == 1) {
		/* switch GPU MUX to PLL */
		ret = __gpufreq_switch_clksrc(TARGET_GPU, CLOCK_MAIN);
		if (ret)
			goto done;
		/* switch STACK MUX to PLL */
		ret = __gpufreq_switch_clksrc(TARGET_STACK, CLOCK_MAIN);
		if (ret)
			goto done;
		/* free DVFS when active */
		g_dvfs_state &= ~DVFS_IDLE;
	} else if (power == POWER_OFF && g_stack.active_count == 0) {
		/* freeze DVFS when idle */
		g_dvfs_state |= DVFS_IDLE;
		/* switch STACK MUX to REF_SEL */
		ret = __gpufreq_switch_clksrc(TARGET_STACK, CLOCK_SUB);
		if (ret)
			goto done;
		/* switch GPU MUX to REF_SEL */
		ret = __gpufreq_switch_clksrc(TARGET_GPU, CLOCK_SUB);
		if (ret)
			goto done;
	} else if (g_stack.active_count < 0)
		__gpufreq_abort("incorrect active count: %d", g_stack.active_count);

	/* return active count if successfully control runtime state */
	ret = g_stack.active_count;

	/* update current status to shared memory */
	if (g_shared_status) {
		g_shared_status->dvfs_state = g_dvfs_state;
		g_shared_status->active_count = g_stack.active_count;
		__gpufreq_update_shared_status_active_idle_reg();
	}

done:
	mutex_unlock(&gpufreq_lock);

	GPUFREQ_TRACE_END();

	return ret;
}

int __gpufreq_generic_commit_gpu(int target_oppidx, enum gpufreq_dvfs_state key)
{
	GPUFREQ_UNREFERENCED(target_oppidx);
	GPUFREQ_UNREFERENCED(key);

	return GPUFREQ_EINVAL;
}

/*
 * API: commit DVFS to STACK by given OPP index
 * this is the main entrance of generic DVFS
 */
int __gpufreq_generic_commit_stack(int target_oppidx, enum gpufreq_dvfs_state key)
{
	int ret = GPUFREQ_SUCCESS;
	int cur_oppidx = 0, opp_num = g_stack.opp_num;
	struct gpufreq_opp_info *working_gpu = g_gpu.working_table;
	unsigned int cur_fgpu = 0, cur_vgpu = 0, target_fgpu = 0, target_vgpu = 0;
	struct gpufreq_opp_info *working_stack = g_stack.working_table;
	unsigned int cur_fstack = 0, cur_vstack = 0, target_fstack = 0, target_vstack = 0;
	unsigned int cur_vsram = 0, target_vsram = 0;

	GPUFREQ_TRACE_START("target_oppidx=%d, key=%d", target_oppidx, key);

	/* validate 0 <= target_oppidx < opp_num */
	if (target_oppidx < 0 || target_oppidx >= opp_num) {
		GPUFREQ_LOGE("invalid target idx: %d (num: %d)", target_oppidx, opp_num);
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

	/* prepare OPP setting */
	cur_oppidx = g_stack.cur_oppidx;
	cur_fgpu = g_gpu.cur_freq;
	cur_vgpu = g_gpu.cur_volt;
	cur_fstack = g_stack.cur_freq;
	cur_vstack = g_stack.cur_volt;
	cur_vsram = g_stack.cur_vsram;

	target_fgpu = working_gpu[target_oppidx].freq;
	target_vgpu = working_gpu[target_oppidx].volt;
	target_fstack = working_stack[target_oppidx].freq;
	target_vstack = working_stack[target_oppidx].volt;
	target_vsram = MAX(working_gpu[target_oppidx].vsram, working_stack[target_oppidx].vsram);

	GPUFREQ_LOGD("begin to commit OPP idx: (%d->%d)", cur_oppidx, target_oppidx);

	ret = __gpufreq_generic_scale(cur_fgpu, target_fgpu, cur_vgpu, target_vgpu,
		cur_fstack, target_fstack, cur_vstack, target_vstack, cur_vsram, target_vsram);
	if (ret) {
		GPUFREQ_LOGE("fail to scale to GPU F(%d) V(%d), STACK F(%d) V(%d), VSRAM(%d)",
			target_fgpu, target_vgpu, target_fstack, target_vstack, target_vsram);
		goto done_unlock;
	}

	g_gpu.cur_oppidx = target_oppidx;
	g_stack.cur_oppidx = target_oppidx;
	__gpufreq_footprint_oppidx(target_oppidx);

	/* update current status to shared memory */
	if (g_shared_status) {
		g_shared_status->cur_oppidx_gpu = g_gpu.cur_oppidx;
		g_shared_status->cur_fgpu = g_gpu.cur_freq;
		g_shared_status->cur_vgpu = g_gpu.cur_volt;
		g_shared_status->cur_vsram_gpu = g_gpu.cur_vsram;
		g_shared_status->cur_power_gpu = g_gpu.working_table[g_gpu.cur_oppidx].power;
		g_shared_status->cur_oppidx_stack = g_stack.cur_oppidx;
		g_shared_status->cur_fstack = g_stack.cur_freq;
		g_shared_status->cur_vstack = g_stack.cur_volt;
		g_shared_status->cur_vsram_stack = g_stack.cur_vsram;
		g_shared_status->cur_power_stack = g_stack.working_table[g_stack.cur_oppidx].power;
		__gpufreq_update_shared_status_dvfs_reg();
	}

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
	if (unlikely(ret < 0))
		goto done;

	if (oppidx == -1) {
		__gpufreq_set_dvfs_state(false, DVFS_DEBUG_KEEP);
		ret = GPUFREQ_SUCCESS;
	} else if (oppidx >= 0 && oppidx < opp_num) {
		__gpufreq_set_dvfs_state(true, DVFS_DEBUG_KEEP);

		ret = __gpufreq_generic_commit_stack(oppidx, DVFS_DEBUG_KEEP);
		if (unlikely(ret))
			__gpufreq_set_dvfs_state(false, DVFS_DEBUG_KEEP);
	} else
		ret = GPUFREQ_EINVAL;

	__gpufreq_power_control(POWER_OFF);

done:
	if (unlikely(ret))
		GPUFREQ_LOGE("fail to commit idx: %d", oppidx);

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
	unsigned int max_freq = 0, min_freq = 0;
	unsigned int max_volt = 0, min_volt = 0;
	int ret = GPUFREQ_SUCCESS;

	ret = __gpufreq_power_control(POWER_ON);
	if (unlikely(ret < 0))
		goto done;

	max_freq = POSDIV_2_MAX_FREQ;
	max_volt = VSTACK_MAX_VOLT;
	min_freq = POSDIV_16_MIN_FREQ;
	min_volt = VSTACK_MIN_VOLT;

	if (freq == 0 && volt == 0) {
		__gpufreq_set_dvfs_state(false, DVFS_DEBUG_KEEP);
		ret = GPUFREQ_SUCCESS;
	} else if (freq > max_freq || freq < min_freq) {
		ret = GPUFREQ_EINVAL;
	} else if (volt > max_volt || volt < min_volt) {
		ret = GPUFREQ_EINVAL;
	} else {
		__gpufreq_set_dvfs_state(true, DVFS_DEBUG_KEEP);

		ret = __gpufreq_custom_commit_stack(freq, volt, DVFS_DEBUG_KEEP);
		if (unlikely(ret))
			__gpufreq_set_dvfs_state(false, DVFS_DEBUG_KEEP);
	}

	__gpufreq_power_control(POWER_OFF);

done:
	if (unlikely(ret))
		GPUFREQ_LOGE("fail to commit Freq: %d, Volt: %d", freq, volt);

	return ret;
}

void __gpufreq_set_timestamp(void)
{
	/* acquire sema before access MFG_TOP_CFG */
	__gpufreq_set_semaphore(SEMA_ACQUIRE);

	/* MFG_TIMESTAMP 0x13FBF130 [0] top_tsvalueb_en = 1'b1 */
	/* MFG_TIMESTAMP 0x13FBF130 [1] timer_sel = 1'b1 */
	writel(GENMASK(1, 0), MFG_TIMESTAMP);

	__gpufreq_set_semaphore(SEMA_RELEASE);
}

void __gpufreq_check_bus_idle(void)
{
	/* acquire sema before access MFG_TOP_CFG */
	__gpufreq_set_semaphore(SEMA_ACQUIRE);

	/* MFG_QCHANNEL_CON 0x13FBF0B4 [0] MFG_ACTIVE_SEL = 1'b1 */
	writel((readl_mfg(MFG_QCHANNEL_CON) | BIT(0)), MFG_QCHANNEL_CON);

	/* MFG_DEBUG_SEL 0x13FBF170 [1:0] MFG_DEBUG_TOP_SEL = 2'b11 */
	writel((readl_mfg(MFG_DEBUG_SEL) | GENMASK(1, 0)), MFG_DEBUG_SEL);

	/*
	 * polling MFG_DEBUG_TOP 0x13FBF178 [0] MFG_DEBUG_TOP
	 * 0x0: bus idle
	 * 0x1: bus busy
	 */
	do {} while (readl_mfg(MFG_DEBUG_TOP) & BIT(0));

	__gpufreq_set_semaphore(SEMA_RELEASE);
}

void __gpufreq_dump_infra_status(void)
{
	if (!g_gpufreq_ready)
		return;

	/* acquire sema before access MFG_TOP_CFG */
	__gpufreq_set_semaphore(SEMA_ACQUIRE);

	GPUFREQ_LOGI("== [GPUFREQ INFRA STATUS] ==");
	GPUFREQ_LOGI("[Regulator] Vgpu: %d, Vstack: %d, Vsram: %d",
		__gpufreq_get_real_vgpu(), __gpufreq_get_real_vstack(),
		__gpufreq_get_real_vsram());
	GPUFREQ_LOGI("[Clk] MFG_PLL: %d, MFG_SEL: 0x%x, MFGSC_PLL: %d, MFGSC_SEL: 0x%x",
		__gpufreq_get_real_fgpu(), readl(TOPCK_CLK_CFG_30) & MFG_INT0_SEL_MASK,
		__gpufreq_get_real_fstack(), readl(TOPCK_CLK_CFG_30) & MFGSC_INT1_SEL_MASK);

	/* MFG_QCHANNEL_CON 0x13FBF0B4 [0] MFG_ACTIVE_SEL = 1'b1 */
	writel((readl_mfg(MFG_QCHANNEL_CON) | BIT(0)), MFG_QCHANNEL_CON);
	/* MFG_DEBUG_SEL 0x13FBF170 [1:0] MFG_DEBUG_TOP_SEL = 2'b11 */
	writel((readl_mfg(MFG_DEBUG_SEL) | GENMASK(1, 0)), MFG_DEBUG_SEL);
	/* MFG_DEBUG_SEL */
	/* MFG_DEBUG_TOP */
	/* MFG_RPC_SLP_PROT_EN_STA */
	/* MFG_RPC_MFGIPS_PWR_CON */
	GPUFREQ_LOGI("%-11s (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x",
		"[MFG]",
		0x13FBF170, readl_mfg(MFG_DEBUG_SEL),
		0x13FBF178, readl_mfg(MFG_DEBUG_TOP),
		0x13F91048, readl(MFG_RPC_SLP_PROT_EN_STA),
		0x13F910FC, readl(MFG_RPC_MFGIPS_PWR_CON));

	/* NTH_MFG_EMI1_GALS_SLV_DBG */
	/* NTH_MFG_EMI0_GALS_SLV_DBG */
	/* STH_MFG_EMI1_GALS_SLV_DBG */
	/* STH_MFG_EMI0_GALS_SLV_DBG */
	GPUFREQ_LOGI("%-11s (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x",
		"[EMI]",
		0x1021C82C, readl(NTH_MFG_EMI1_GALS_SLV_DBG),
		0x1021C830, readl(NTH_MFG_EMI0_GALS_SLV_DBG),
		0x1021E82C, readl(STH_MFG_EMI1_GALS_SLV_DBG),
		0x1021E830, readl(STH_MFG_EMI0_GALS_SLV_DBG));

	/* NTH_M6M7_IDLE_BIT_EN_1 */
	/* NTH_M6M7_IDLE_BIT_EN_0 */
	/* STH_M6M7_IDLE_BIT_EN_1 */
	/* STH_M6M7_IDLE_BIT_EN_0 */
	GPUFREQ_LOGI("%11s (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x",
		"[EMI]",
		0x10270228, readl(NTH_M6M7_IDLE_BIT_EN_1),
		0x1027022C, readl(NTH_M6M7_IDLE_BIT_EN_0),
		0x1030E228, readl(STH_M6M7_IDLE_BIT_EN_1),
		0x1030E22C, readl(STH_M6M7_IDLE_BIT_EN_0));

	/* IFR_MFGSYS_PROT_EN_STA_0 */
	/* IFR_MFGSYS_PROT_RDY_STA_0 */
	/* IFR_EMISYS_PROTECT_EN_STA_0 */
	/* IFR_EMISYS_PROTECT_EN_STA_1 */
	GPUFREQ_LOGI("%-11s (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x",
		"[INFRA]",
		0x1002C1A0, readl(IFR_MFGSYS_PROT_EN_STA_0),
		0x1002C1AC, readl(IFR_MFGSYS_PROT_RDY_STA_0),
		0x1002C100, readl(IFR_EMISYS_PROTECT_EN_STA_0),
		0x1002C120, readl(IFR_EMISYS_PROTECT_EN_STA_1));

	/* NTH_EMI_AO_DEBUG_CTRL0 */
	/* STH_EMI_AO_DEBUG_CTRL0 */
	/* INFRA_AO_BUS0_U_DEBUG_CTRL0 */
	/* INFRA_AO1_BUS1_U_DEBUG_CTRL0 */
	GPUFREQ_LOGI("%-11s (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x",
		"[EMI_INFRA]",
		0x10042000, readl(NTH_EMI_AO_DEBUG_CTRL0),
		0x10028000, readl(STH_EMI_AO_DEBUG_CTRL0),
		0x10023000, readl(INFRA_AO_BUS0_U_DEBUG_CTRL0),
		0x1002B000, readl(INFRA_AO1_BUS1_U_DEBUG_CTRL0));

	GPUFREQ_LOGI("%-11s 0x%08x, 0x%08x, 0x%08x, 0x%08x, 0x%08x",
		"[MFG0-4]", readl(SPM_MFG0_PWR_CON),
		readl(MFG_RPC_MFG1_PWR_CON), readl(MFG_RPC_MFG2_PWR_CON),
		readl(MFG_RPC_MFG3_PWR_CON), readl(MFG_RPC_MFG4_PWR_CON));
	GPUFREQ_LOGI("%-11s 0x%08x, 0x%08x, 0x%08x, 0x%08x, 0x%08x",
		"[MFG5-9]", readl(MFG_RPC_MFG5_PWR_CON),
		readl(MFG_RPC_MFG6_PWR_CON), readl(MFG_RPC_MFG7_PWR_CON),
		readl(MFG_RPC_MFG8_PWR_CON), readl(MFG_RPC_MFG9_PWR_CON));
	GPUFREQ_LOGI("%-11s 0x%08x, 0x%08x, 0x%08x, 0x%08x, 0x%08x",
		"[MFG10-14]", readl(MFG_RPC_MFG10_PWR_CON),
		readl(MFG_RPC_MFG11_PWR_CON), readl(MFG_RPC_MFG12_PWR_CON),
		readl(MFG_RPC_MFG13_PWR_CON), readl(MFG_RPC_MFG14_PWR_CON));
	GPUFREQ_LOGI("%-11s 0x%08x, 0x%08x, 0x%08x, 0x%08x, 0x%08x",
		"[MFG15-19]", readl(MFG_RPC_MFG15_PWR_CON),
		readl(MFG_RPC_MFG16_PWR_CON), readl(MFG_RPC_MFG17_PWR_CON),
		readl(MFG_RPC_MFG18_PWR_CON), readl(MFG_RPC_MFG19_PWR_CON));

	__gpufreq_set_semaphore(SEMA_RELEASE);
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
		return GPUFREQ_BATT_PERCENT_IDX;
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

/* API: update debug info to shared memory */
void __gpufreq_update_debug_opp_info(void)
{
	if (!g_shared_status)
		return;

	mutex_lock(&gpufreq_lock);

	/* update current status to shared memory */
	if (__gpufreq_get_power_state()) {
		g_shared_status->cur_con1_fgpu = __gpufreq_get_real_fgpu();
		g_shared_status->cur_con1_fstack = __gpufreq_get_real_fstack();
		g_shared_status->cur_fmeter_fgpu = __gpufreq_get_fmeter_freq(TARGET_GPU);
		g_shared_status->cur_fmeter_fstack = __gpufreq_get_fmeter_freq(TARGET_STACK);
		g_shared_status->cur_regulator_vgpu = __gpufreq_get_real_vgpu();
		g_shared_status->cur_regulator_vstack = __gpufreq_get_real_vstack();
		g_shared_status->cur_regulator_vsram_gpu = __gpufreq_get_real_vsram();
		g_shared_status->cur_regulator_vsram_stack = __gpufreq_get_real_vsram();
	} else {
		g_shared_status->cur_con1_fgpu = 0;
		g_shared_status->cur_con1_fstack = 0;
		g_shared_status->cur_fmeter_fgpu = 0;
		g_shared_status->cur_fmeter_fstack = 0;
		g_shared_status->cur_regulator_vgpu = 0;
		g_shared_status->cur_regulator_vstack = 0;
		g_shared_status->cur_regulator_vsram_stack = 0;
	}
	g_shared_status->mfg_pwr_status = MFG_0_19_PWR_STATUS;

	mutex_unlock(&gpufreq_lock);
}

/* API: general interface to set MFGSYS config */
void __gpufreq_set_mfgsys_config(enum gpufreq_config_target target, enum gpufreq_config_value val)
{
	mutex_lock(&gpufreq_lock);

	switch (target) {
	case CONFIG_STRESS_TEST:
		gpuppm_set_stress_test(val);
		break;
	case CONFIG_MARGIN:
		__gpufreq_set_margin_mode(val);
		break;
	case CONFIG_GPM1:
		__gpufreq_set_gpm_mode(1, val);
		break;
	case CONFIG_DFD:
		g_dfd_mode = val;
		break;
	default:
		GPUFREQ_LOGE("invalid config target: %d", target);
		break;
	}

	mutex_unlock(&gpufreq_lock);
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

/* PDCA: GPU IP automatically control GPU shader MTCMOS */
void __gpufreq_pdca_config(enum gpufreq_power_state power)
{
#if GPUFREQ_PDCA_ENABLE
	/* acquire sema before access MFG_TOP_CFG */
	__gpufreq_set_semaphore(SEMA_ACQUIRE);

	if (power == POWER_ON) {
		/* MFG_ACTIVE_POWER_CON_CG 0x13FBF100 [0] rg_cg_active_pwrctl_en = 1'b1 */
		writel((readl_mfg(MFG_ACTIVE_POWER_CON_CG) | BIT(0)), MFG_ACTIVE_POWER_CON_CG);
		/* MFG_ACTIVE_POWER_CON_ST0 0x13FBF120 [0] rg_st0_active_pwrctl_en = 1'b1 */
		writel((readl_mfg(MFG_ACTIVE_POWER_CON_ST0) | BIT(0)), MFG_ACTIVE_POWER_CON_ST0);
		/* MFG_ACTIVE_POWER_CON_ST1 0x13FBF140 [0] rg_st1_active_pwrctl_en = 1'b1 */
		writel((readl_mfg(MFG_ACTIVE_POWER_CON_ST1) | BIT(0)), MFG_ACTIVE_POWER_CON_ST1);
		/* MFG_ACTIVE_POWER_CON_ST2 0x13FBF118 [0] rg_st2_active_pwrctl_en = 1'b1 */
		writel((readl_mfg(MFG_ACTIVE_POWER_CON_ST2) | BIT(0)), MFG_ACTIVE_POWER_CON_ST2);
		/* MFG_ACTIVE_POWER_CON_ST4 0x13FBF0C0 [0] rg_st4_active_pwrctl_en = 1'b1 */
		writel((readl_mfg(MFG_ACTIVE_POWER_CON_ST4) | BIT(0)), MFG_ACTIVE_POWER_CON_ST4);
		/* MFG_ACTIVE_POWER_CON_ST5 0x13FBF098 [0] rg_st5_active_pwrctl_en = 1'b1 */
		writel((readl_mfg(MFG_ACTIVE_POWER_CON_ST5) | BIT(0)), MFG_ACTIVE_POWER_CON_ST5);
		/* MFG_ACTIVE_POWER_CON_ST6 0x13FBF1C0 [0] rg_st6_active_pwrctl_en = 1'b1 */
		writel((readl_mfg(MFG_ACTIVE_POWER_CON_ST6) | BIT(0)), MFG_ACTIVE_POWER_CON_ST6);
		/* MFG_ACTIVE_POWER_CON_00 0x13FBF400 [0] rg_sc0_active_pwrctl_en = 1'b1 */
		writel((readl_mfg(MFG_ACTIVE_POWER_CON_00) | BIT(0)), MFG_ACTIVE_POWER_CON_00);
		/* MFG_ACTIVE_POWER_CON_01 0x13FBF404 [31] rg_sc0_active_pwrctl_rsv = 1'b1 */
		writel((readl_mfg(MFG_ACTIVE_POWER_CON_01) | BIT(31)), MFG_ACTIVE_POWER_CON_01);
		/* MFG_ACTIVE_POWER_CON_06 0x13FBF418 [0] rg_sc1_active_pwrctl_en = 1'b1 */
		writel((readl_mfg(MFG_ACTIVE_POWER_CON_06) | BIT(0)), MFG_ACTIVE_POWER_CON_06);
		/* MFG_ACTIVE_POWER_CON_07 0x13FBF41C [31] rg_sc1_active_pwrctl_rsv = 1'b1 */
		writel((readl_mfg(MFG_ACTIVE_POWER_CON_07) | BIT(31)), MFG_ACTIVE_POWER_CON_07);
		/* MFG_ACTIVE_POWER_CON_12 0x13FBF430 [0] rg_sc2_active_pwrctl_en = 1'b1 */
		writel((readl_mfg(MFG_ACTIVE_POWER_CON_12) | BIT(0)), MFG_ACTIVE_POWER_CON_12);
		/* MFG_ACTIVE_POWER_CON_13 0x13FBF434 [31] rg_sc2_active_pwrctl_rsv = 1'b1 */
		writel((readl_mfg(MFG_ACTIVE_POWER_CON_13) | BIT(31)), MFG_ACTIVE_POWER_CON_13);
		/* MFG_ACTIVE_POWER_CON_18 0x13FBF448 [0] rg_sc3_active_pwrctl_en = 1'b1 */
		writel((readl_mfg(MFG_ACTIVE_POWER_CON_18) | BIT(0)), MFG_ACTIVE_POWER_CON_18);
		/* MFG_ACTIVE_POWER_CON_19 0x13FBF44C [31] rg_sc3_active_pwrctl_rsv = 1'b1 */
		writel((readl_mfg(MFG_ACTIVE_POWER_CON_19) | BIT(31)), MFG_ACTIVE_POWER_CON_19);
		/* MFG_ACTIVE_POWER_CON_24 0x13FBF460 [0] rg_sc4_active_pwrctl_en = 1'b1 */
		writel((readl_mfg(MFG_ACTIVE_POWER_CON_24) | BIT(0)), MFG_ACTIVE_POWER_CON_24);
		/* MFG_ACTIVE_POWER_CON_25 0x13FBF464 [31] rg_sc4_active_pwrctl_rsv = 1'b1 */
		writel((readl_mfg(MFG_ACTIVE_POWER_CON_25) | BIT(31)), MFG_ACTIVE_POWER_CON_25);
		/* MFG_ACTIVE_POWER_CON_30 0x13FBF478 [0] rg_sc5_active_pwrctl_en = 1'b1 */
		writel((readl_mfg(MFG_ACTIVE_POWER_CON_30) | BIT(0)), MFG_ACTIVE_POWER_CON_30);
		/* MFG_ACTIVE_POWER_CON_31 0x13FBF47C [31] rg_sc5_active_pwrctl_rsv = 1'b1 */
		writel((readl_mfg(MFG_ACTIVE_POWER_CON_31) | BIT(31)), MFG_ACTIVE_POWER_CON_31);
		/* MFG_ACTIVE_POWER_CON_36 0x13FBF490 [0] rg_sc6_active_pwrctl_en = 1'b1 */
		writel((readl_mfg(MFG_ACTIVE_POWER_CON_36) | BIT(0)), MFG_ACTIVE_POWER_CON_36);
		/* MFG_ACTIVE_POWER_CON_37 0x13FBF494 [31] rg_sc6_active_pwrctl_rsv = 1'b1 */
		writel((readl_mfg(MFG_ACTIVE_POWER_CON_37) | BIT(31)), MFG_ACTIVE_POWER_CON_37);
		/* MFG_ACTIVE_POWER_CON_42 0x13FBF4A8 [0] rg_sc7_active_pwrctl_en = 1'b1 */
		writel((readl_mfg(MFG_ACTIVE_POWER_CON_42) | BIT(0)), MFG_ACTIVE_POWER_CON_42);
		/* MFG_ACTIVE_POWER_CON_43 0x13FBF4AC [31] rg_sc7_active_pwrctl_rsv = 1'b1 */
		writel((readl_mfg(MFG_ACTIVE_POWER_CON_43) | BIT(31)), MFG_ACTIVE_POWER_CON_43);
		/* MFG_ACTIVE_POWER_CON_48 0x13FBF4C0 [0] rg_sc8_active_pwrctl_en = 1'b1 */
		writel((readl_mfg(MFG_ACTIVE_POWER_CON_48) | BIT(0)), MFG_ACTIVE_POWER_CON_48);
		/* MFG_ACTIVE_POWER_CON_49 0x13FBF4C4 [31] rg_sc8_active_pwrctl_rsv = 1'b1 */
		writel((readl_mfg(MFG_ACTIVE_POWER_CON_49) | BIT(31)), MFG_ACTIVE_POWER_CON_49);
		/* MFG_ACTIVE_POWER_CON_54 0x13FBF4D8 [0] rg_sc9_active_pwrctl_en = 1'b1 */
		writel((readl_mfg(MFG_ACTIVE_POWER_CON_54) | BIT(0)), MFG_ACTIVE_POWER_CON_54);
		/* MFG_ACTIVE_POWER_CON_55 0x13FBF4DC [31] rg_sc9_active_pwrctl_rsv = 1'b1 */
		writel((readl_mfg(MFG_ACTIVE_POWER_CON_55) | BIT(31)), MFG_ACTIVE_POWER_CON_55);
		/* MFG_ACTIVE_POWER_CON_60 0x13FBF4F0 [0] rg_sc10_active_pwrctl_en = 1'b1 */
		writel((readl_mfg(MFG_ACTIVE_POWER_CON_60) | BIT(0)), MFG_ACTIVE_POWER_CON_60);
		/* MFG_ACTIVE_POWER_CON_61 0x13FBF4F4 [31] rg_sc10_active_pwrctl_rsv = 1'b1 */
		writel((readl_mfg(MFG_ACTIVE_POWER_CON_61) | BIT(31)), MFG_ACTIVE_POWER_CON_61);
	} else {
		/* MFG_ACTIVE_POWER_CON_CG 0x13FBF400 [0] rg_cg_active_pwrctl_en = 1'b0 */
		writel((readl_mfg(MFG_ACTIVE_POWER_CON_CG) & ~BIT(0)), MFG_ACTIVE_POWER_CON_CG);
		/* MFG_ACTIVE_POWER_CON_ST0 0x13FBF120 [0] rg_st0_active_pwrctl_en = 1'b0 */
		writel((readl_mfg(MFG_ACTIVE_POWER_CON_ST0) & ~BIT(0)), MFG_ACTIVE_POWER_CON_ST0);
		/* MFG_ACTIVE_POWER_CON_ST1 0x13FBF140 [0] rg_st1_active_pwrctl_en = 1'b0 */
		writel((readl_mfg(MFG_ACTIVE_POWER_CON_ST1) & ~BIT(0)), MFG_ACTIVE_POWER_CON_ST1);
		/* MFG_ACTIVE_POWER_CON_ST2 0x13FBF118 [0] rg_st2_active_pwrctl_en = 1'b0 */
		writel((readl_mfg(MFG_ACTIVE_POWER_CON_ST2) & ~BIT(0)), MFG_ACTIVE_POWER_CON_ST2);
		/* MFG_ACTIVE_POWER_CON_ST4 0x13FBF0C0 [0] rg_st4_active_pwrctl_en = 1'b0 */
		writel((readl_mfg(MFG_ACTIVE_POWER_CON_ST4) & ~BIT(0)), MFG_ACTIVE_POWER_CON_ST4);
		/* MFG_ACTIVE_POWER_CON_ST5 0x13FBF098 [0] rg_st5_active_pwrctl_en = 1'b0 */
		writel((readl_mfg(MFG_ACTIVE_POWER_CON_ST5) & ~BIT(0)), MFG_ACTIVE_POWER_CON_ST5);
		/* MFG_ACTIVE_POWER_CON_ST6 0x13FBF1C0 [0] rg_st6_active_pwrctl_en = 1'b0 */
		writel((readl_mfg(MFG_ACTIVE_POWER_CON_ST6) & ~BIT(0)), MFG_ACTIVE_POWER_CON_ST6);
		/* MFG_ACTIVE_POWER_CON_00 0x13FBF400 [0] rg_sc0_active_pwrctl_en = 1'b0*/
		writel((readl_mfg(MFG_ACTIVE_POWER_CON_00) & ~BIT(0)), MFG_ACTIVE_POWER_CON_00);
		/* MFG_ACTIVE_POWER_CON_01 0x13FBF404 [31] rg_sc0_active_pwrctl_rsv = 1'b0*/
		writel((readl_mfg(MFG_ACTIVE_POWER_CON_01) & ~BIT(31)), MFG_ACTIVE_POWER_CON_01);
		/* MFG_ACTIVE_POWER_CON_06 0x13FBF418 [0] rg_sc1_active_pwrctl_en = 1'b0*/
		writel((readl_mfg(MFG_ACTIVE_POWER_CON_06) & ~BIT(0)), MFG_ACTIVE_POWER_CON_06);
		/* MFG_ACTIVE_POWER_CON_07 0x13FBF41C [31] rg_sc1_active_pwrctl_rsv = 1'b0*/
		writel((readl_mfg(MFG_ACTIVE_POWER_CON_07) & ~BIT(31)), MFG_ACTIVE_POWER_CON_07);
		/* MFG_ACTIVE_POWER_CON_12 0x13FBF430 [0] rg_sc2_active_pwrctl_en = 1'b0*/
		writel((readl_mfg(MFG_ACTIVE_POWER_CON_12) & ~BIT(0)), MFG_ACTIVE_POWER_CON_12);
		/* MFG_ACTIVE_POWER_CON_13 0x13FBF434 [31] rg_sc2_active_pwrctl_rsv = 1'b0*/
		writel((readl_mfg(MFG_ACTIVE_POWER_CON_13) & ~BIT(31)), MFG_ACTIVE_POWER_CON_13);
		/* MFG_ACTIVE_POWER_CON_18 0x13FBF448 [0] rg_sc3_active_pwrctl_en = 1'b0*/
		writel((readl_mfg(MFG_ACTIVE_POWER_CON_18) & ~BIT(0)), MFG_ACTIVE_POWER_CON_18);
		/* MFG_ACTIVE_POWER_CON_19 0x13FBF44C [31] rg_sc3_active_pwrctl_rsv = 1'b0*/
		writel((readl_mfg(MFG_ACTIVE_POWER_CON_19) & ~BIT(31)), MFG_ACTIVE_POWER_CON_19);
		/* MFG_ACTIVE_POWER_CON_24 0x13FBF460 [0] rg_sc4_active_pwrctl_en = 1'b0*/
		writel((readl_mfg(MFG_ACTIVE_POWER_CON_24) & ~BIT(0)), MFG_ACTIVE_POWER_CON_24);
		/* MFG_ACTIVE_POWER_CON_25 0x13FBF464 [31] rg_sc4_active_pwrctl_rsv = 1'b0*/
		writel((readl_mfg(MFG_ACTIVE_POWER_CON_25) & ~BIT(31)), MFG_ACTIVE_POWER_CON_25);
		/* MFG_ACTIVE_POWER_CON_30 0x13FBF478 [0] rg_sc5_active_pwrctl_en = 1'b0*/
		writel((readl_mfg(MFG_ACTIVE_POWER_CON_30) & ~BIT(0)), MFG_ACTIVE_POWER_CON_30);
		/* MFG_ACTIVE_POWER_CON_31 0x13FBF47C [31] rg_sc5_active_pwrctl_rsv = 1'b0*/
		writel((readl_mfg(MFG_ACTIVE_POWER_CON_31) & ~BIT(31)), MFG_ACTIVE_POWER_CON_31);
		/* MFG_ACTIVE_POWER_CON_36 0x13FBF490 [0] rg_sc6_active_pwrctl_en = 1'b0*/
		writel((readl_mfg(MFG_ACTIVE_POWER_CON_36) & ~BIT(0)), MFG_ACTIVE_POWER_CON_36);
		/* MFG_ACTIVE_POWER_CON_37 0x13FBF494 [31] rg_sc6_active_pwrctl_rsv = 1'b0*/
		writel((readl_mfg(MFG_ACTIVE_POWER_CON_37) & ~BIT(31)), MFG_ACTIVE_POWER_CON_37);
		/* MFG_ACTIVE_POWER_CON_42 0x13FBF4A8 [0] rg_sc7_active_pwrctl_en = 1'b0*/
		writel((readl_mfg(MFG_ACTIVE_POWER_CON_42) & ~BIT(0)), MFG_ACTIVE_POWER_CON_42);
		/* MFG_ACTIVE_POWER_CON_43 0x13FBF4AC [31] rg_sc7_active_pwrctl_rsv = 1'b0*/
		writel((readl_mfg(MFG_ACTIVE_POWER_CON_43) & ~BIT(31)), MFG_ACTIVE_POWER_CON_43);
		/* MFG_ACTIVE_POWER_CON_48 0x13FBF4C0 [0] rg_sc8_active_pwrctl_en = 1'b0*/
		writel((readl_mfg(MFG_ACTIVE_POWER_CON_48) & ~BIT(0)), MFG_ACTIVE_POWER_CON_48);
		/* MFG_ACTIVE_POWER_CON_49 0x13FBF4C4 [31] rg_sc8_active_pwrctl_rsv = 1'b0*/
		writel((readl_mfg(MFG_ACTIVE_POWER_CON_49) & ~BIT(31)), MFG_ACTIVE_POWER_CON_49);
		/* MFG_ACTIVE_POWER_CON_54 0x13FBF4D8 [0] rg_sc9_active_pwrctl_en = 1'b0*/
		writel((readl_mfg(MFG_ACTIVE_POWER_CON_54) & ~BIT(0)), MFG_ACTIVE_POWER_CON_54);
		/* MFG_ACTIVE_POWER_CON_55 0x13FBF4DC [31] rg_sc9_active_pwrctl_rsv = 1'b0*/
		writel((readl_mfg(MFG_ACTIVE_POWER_CON_55) & ~BIT(31)), MFG_ACTIVE_POWER_CON_55);
		/* MFG_ACTIVE_POWER_CON_60 0x13FBF4F0 [0] rg_sc10_active_pwrctl_en = 1'b0*/
		writel((readl_mfg(MFG_ACTIVE_POWER_CON_60) & ~BIT(0)), MFG_ACTIVE_POWER_CON_60);
		/* MFG_ACTIVE_POWER_CON_61 0x13FBF4F4 [31] rg_sc10_active_pwrctl_rsv = 1'b0 */
		writel((readl_mfg(MFG_ACTIVE_POWER_CON_61) & ~BIT(31)), MFG_ACTIVE_POWER_CON_61);
	}

	__gpufreq_set_semaphore(SEMA_RELEASE);
#else
	GPUFREQ_UNREFERENCED(power);
#endif /* GPUFREQ_PDCA_ENABLE */
}

/* API: fake PWR_CON value to temporarily disable PDCA */
void __gpufreq_fake_mtcmos_control(enum gpufreq_power_state power)
{
#if GPUFREQ_PDCA_ENABLE
	if (power == POWER_ON) {
		/* fake power on value of SPM MFG2-19 */
		writel(0xC000000D, MFG_RPC_MFG2_PWR_CON);  /* MFG2  */
		writel(0xC000000D, MFG_RPC_MFG3_PWR_CON);  /* MFG3  */
		writel(0xC000000D, MFG_RPC_MFG4_PWR_CON);  /* MFG4  */
		writel(0xC000000D, MFG_RPC_MFG5_PWR_CON);  /* MFG5  */
		writel(0xC000000D, MFG_RPC_MFG6_PWR_CON);  /* MFG6  */
		writel(0xC000000D, MFG_RPC_MFG7_PWR_CON);  /* MFG7  */
		writel(0xC000000D, MFG_RPC_MFG8_PWR_CON);  /* MFG8  */
		writel(0xC000000D, MFG_RPC_MFG9_PWR_CON);  /* MFG9  */
		writel(0xC000000D, MFG_RPC_MFG10_PWR_CON); /* MFG10 */
		writel(0xC000000D, MFG_RPC_MFG11_PWR_CON); /* MFG11 */
		writel(0xC000000D, MFG_RPC_MFG12_PWR_CON); /* MFG12 */
		writel(0xC000000D, MFG_RPC_MFG13_PWR_CON); /* MFG13 */
		writel(0xC000000D, MFG_RPC_MFG14_PWR_CON); /* MFG14 */
		writel(0xC000000D, MFG_RPC_MFG15_PWR_CON); /* MFG15 */
		writel(0xC000000D, MFG_RPC_MFG16_PWR_CON); /* MFG16 */
		writel(0xC000000D, MFG_RPC_MFG17_PWR_CON); /* MFG17 */
		writel(0xC000000D, MFG_RPC_MFG18_PWR_CON); /* MFG18 */
		writel(0xC000000D, MFG_RPC_MFG19_PWR_CON); /* MFG19 */
	} else {
		/* fake power off value of SPM MFG2-19 */
		writel(0x1112, MFG_RPC_MFG19_PWR_CON); /* MFG19 */
		writel(0x1112, MFG_RPC_MFG18_PWR_CON); /* MFG18 */
		writel(0x1112, MFG_RPC_MFG17_PWR_CON); /* MFG17 */
		writel(0x1112, MFG_RPC_MFG16_PWR_CON); /* MFG16 */
		writel(0x1112, MFG_RPC_MFG15_PWR_CON); /* MFG15 */
		writel(0x1112, MFG_RPC_MFG14_PWR_CON); /* MFG14 */
		writel(0x1112, MFG_RPC_MFG13_PWR_CON); /* MFG13 */
		writel(0x1112, MFG_RPC_MFG12_PWR_CON); /* MFG12 */
		writel(0x1112, MFG_RPC_MFG11_PWR_CON); /* MFG11 */
		writel(0x1112, MFG_RPC_MFG10_PWR_CON); /* MFG10 */
		writel(0x1112, MFG_RPC_MFG9_PWR_CON);  /* MFG9  */
		writel(0x1112, MFG_RPC_MFG8_PWR_CON);  /* MFG8  */
		writel(0x1112, MFG_RPC_MFG7_PWR_CON);  /* MFG7  */
		writel(0x1112, MFG_RPC_MFG6_PWR_CON);  /* MFG6  */
		writel(0x1112, MFG_RPC_MFG5_PWR_CON);  /* MFG5  */
		writel(0x1112, MFG_RPC_MFG4_PWR_CON);  /* MFG4  */
		writel(0x1112, MFG_RPC_MFG3_PWR_CON);  /* MFG3  */
		writel(0x1112, MFG_RPC_MFG2_PWR_CON);  /* MFG2  */
	}
#else
	GPUFREQ_UNREFERENCED(power);
#endif /* GPUFREQ_PDCA_ENABLE */
}

/* API: init first time shared status */
void __gpufreq_set_shared_status(struct gpufreq_shared_status *shared_status)
{
	mutex_lock(&gpufreq_lock);

	if (shared_status)
		g_shared_status = shared_status;
	else
		__gpufreq_abort("null gpufreq shared status: 0x%llx", shared_status);

	/* update current status to shared memory */
	if (g_shared_status) {
		g_shared_status->cur_oppidx_gpu = g_gpu.cur_oppidx;
		g_shared_status->opp_num_gpu = g_gpu.opp_num;
		g_shared_status->signed_opp_num_gpu = g_gpu.signed_opp_num;
		g_shared_status->cur_fgpu = g_gpu.cur_freq;
		g_shared_status->cur_vgpu = g_gpu.cur_volt;
		g_shared_status->cur_vsram_gpu = g_gpu.cur_vsram;
		g_shared_status->cur_power_gpu = g_gpu.working_table[g_gpu.cur_oppidx].power;
		g_shared_status->max_power_gpu = g_gpu.working_table[g_gpu.max_oppidx].power;
		g_shared_status->min_power_gpu = g_gpu.working_table[g_gpu.min_oppidx].power;
		g_shared_status->cur_oppidx_stack = g_stack.cur_oppidx;
		g_shared_status->opp_num_stack = g_stack.opp_num;
		g_shared_status->signed_opp_num_stack = g_stack.signed_opp_num;
		g_shared_status->cur_fstack = g_stack.cur_freq;
		g_shared_status->cur_vstack = g_stack.cur_volt;
		g_shared_status->cur_vsram_stack = g_stack.cur_vsram;
		g_shared_status->cur_power_stack = g_stack.working_table[g_stack.cur_oppidx].power;
		g_shared_status->max_power_stack = g_stack.working_table[g_stack.max_oppidx].power;
		g_shared_status->min_power_stack = g_stack.working_table[g_stack.min_oppidx].power;
		g_shared_status->power_count = g_stack.power_count;
		g_shared_status->buck_count = g_stack.buck_count;
		g_shared_status->mtcmos_count = g_stack.mtcmos_count;
		g_shared_status->cg_count = g_stack.cg_count;
		g_shared_status->active_count = g_stack.active_count;
		g_shared_status->power_control = __gpufreq_power_ctrl_enable();
		g_shared_status->active_idle_control = __gpufreq_active_idle_ctrl_enable();
		g_shared_status->dvfs_state = g_dvfs_state;
		g_shared_status->shader_present = g_shader_present;
		g_shared_status->asensor_enable = g_asensor_enable;
		g_shared_status->aging_load = g_aging_load;
		g_shared_status->aging_margin = g_aging_margin;
		g_shared_status->avs_enable = g_avs_enable;
		g_shared_status->avs_margin = g_avs_margin;
		g_shared_status->ptp_version = g_ptp_version;
		g_shared_status->gpm1_mode = g_gpm1_mode;
		g_shared_status->gpm3_mode = g_gpm3_mode;
		g_shared_status->dual_buck = true;
		g_shared_status->segment_id = g_stack.segment_id;
		g_shared_status->test_mode = true;
#if GPUFREQ_MSSV_TEST_MODE
		g_shared_status->reg_stack_sel.addr = 0x13FBF500;
		g_shared_status->reg_del_sel.addr = 0x13FBF080;
#endif /* GPUFREQ_MSSV_TEST_MODE */
#if GPUFREQ_ASENSOR_ENABLE
		g_shared_status->asensor_info = g_asensor_info;
#endif /* GPUFREQ_ASENSOR_ENABLE */
		__gpufreq_update_shared_status_opp_table();
		__gpufreq_update_shared_status_adj_table();
		__gpufreq_update_shared_status_init_reg();
	}

	mutex_unlock(&gpufreq_lock);
}

/* API: MSSV test function */
int __gpufreq_mssv_commit(unsigned int target, unsigned int val)
{
#if GPUFREQ_MSSV_TEST_MODE
	int ret = GPUFREQ_SUCCESS;

	ret = __gpufreq_power_control(POWER_ON);
	if (unlikely(ret < 0))
		goto done;

	mutex_lock(&gpufreq_lock);

	/* acquire sema before access MFG_TOP_CFG */
	__gpufreq_set_semaphore(SEMA_ACQUIRE);

	switch (target) {
	case TARGET_MSSV_FGPU:
		if (val > POSDIV_2_MAX_FREQ || val < POSDIV_16_MIN_FREQ)
			ret = GPUFREQ_EINVAL;
		else
			ret = __gpufreq_freq_scale_gpu(g_gpu.cur_freq, val);
		break;
	case TARGET_MSSV_VGPU:
		if (val > VGPU_MAX_VOLT || val < VGPU_MIN_VOLT)
			ret = GPUFREQ_EINVAL;
		else
			ret = __gpufreq_volt_scale_gpu(g_gpu.cur_volt, val);
		break;
	case TARGET_MSSV_FSTACK:
		if (val > POSDIV_2_MAX_FREQ || val < POSDIV_16_MIN_FREQ)
			ret = GPUFREQ_EINVAL;
		else
			ret = __gpufreq_freq_scale_stack(g_stack.cur_freq, val);
		break;
	case TARGET_MSSV_VSTACK:
		if (val > VSTACK_MAX_VOLT || val < VSTACK_MIN_VOLT)
			ret = GPUFREQ_EINVAL;
		else
			ret = __gpufreq_volt_scale_stack(g_stack.cur_volt, val);
		break;
	case TARGET_MSSV_VSRAM:
		if (val > VSRAM_MAX_VOLT || val < VSRAM_MIN_VOLT)
			ret = GPUFREQ_EINVAL;
		else
			ret = __gpufreq_volt_scale_sram(g_stack.cur_vsram, val);
		break;
	case TARGET_MSSV_STACK_SEL:
		if (val == 1 || val == 0)
			__gpufreq_mssv_set_stack_sel(val);
		else
			ret = GPUFREQ_EINVAL;
		break;
	case TARGET_MSSV_DEL_SEL:
		if (val == 1 || val == 0)
			__gpufreq_mssv_set_del_sel(val);
		else
			ret = GPUFREQ_EINVAL;
		break;
	default:
		ret = GPUFREQ_EINVAL;
		break;
	}

	if (unlikely(ret))
		GPUFREQ_LOGE("invalid MSSV cmd, target: %d, val: %d", target, val);
	else {
		if (g_shared_status) {
			g_shared_status->cur_fgpu = g_gpu.cur_freq;
			g_shared_status->cur_vgpu = g_gpu.cur_volt;
			g_shared_status->cur_vsram_gpu = g_gpu.cur_vsram;
			g_shared_status->cur_fstack = g_stack.cur_freq;
			g_shared_status->cur_vstack = g_stack.cur_volt;
			g_shared_status->cur_vsram_stack = g_stack.cur_vsram;
			g_shared_status->reg_stack_sel.val = readl_mfg(MFG_DUMMY_REG);
			g_shared_status->reg_del_sel.val = readl_mfg(MFG_SRAM_FUL_SEL_ULV);
		}
	}

	__gpufreq_set_semaphore(SEMA_RELEASE);

	mutex_unlock(&gpufreq_lock);

	__gpufreq_power_control(POWER_OFF);

done:
	return ret;
#else
	GPUFREQ_UNREFERENCED(target);
	GPUFREQ_UNREFERENCED(val);

	return GPUFREQ_EINVAL;
#endif /* GPUFREQ_MSSV_TEST_MODE */
}

/**
 * ===============================================
 * Internal Function Definition
 * ===============================================
 */
static void __gpufreq_update_shared_status_opp_table(void)
{
	unsigned int copy_size = 0;

	if (!g_shared_status)
		return;

	/* GPU */
	/* working table */
	copy_size = sizeof(struct gpufreq_opp_info) * g_gpu.opp_num;
	memcpy(g_shared_status->working_table_gpu, g_gpu.working_table, copy_size);
	/* signed table */
	copy_size = sizeof(struct gpufreq_opp_info) * g_gpu.signed_opp_num;
	memcpy(g_shared_status->signed_table_gpu, g_gpu.signed_table, copy_size);

	/* STACK */
	/* working table */
	copy_size = sizeof(struct gpufreq_opp_info) * g_stack.opp_num;
	memcpy(g_shared_status->working_table_stack, g_stack.working_table, copy_size);
	/* signed table */
	copy_size = sizeof(struct gpufreq_opp_info) * g_stack.signed_opp_num;
	memcpy(g_shared_status->signed_table_stack, g_stack.signed_table, copy_size);
}

static void __gpufreq_update_shared_status_adj_table(void)
{
	unsigned int copy_size = 0;

	if (!g_shared_status)
		return;

	/* GPU */
	/* aging table */
	copy_size = sizeof(struct gpufreq_adj_info) * NUM_GPU_SIGNED_IDX;
	memcpy(g_shared_status->aging_table_gpu, g_gpu_aging_table[g_aging_table_idx], copy_size);
	/* avs table */
	copy_size = sizeof(struct gpufreq_adj_info) * NUM_GPU_SIGNED_IDX;
	memcpy(g_shared_status->avs_table_gpu, g_gpu_avs_table, copy_size);

	/* STACK */
	/* aging table */
	copy_size = sizeof(struct gpufreq_adj_info) * NUM_STACK_SIGNED_IDX;
	memcpy(g_shared_status->aging_table_stack,
		g_stack_aging_table[g_aging_table_idx], copy_size);
	/* avs table */
	copy_size = sizeof(struct gpufreq_adj_info) * NUM_STACK_SIGNED_IDX;
	memcpy(g_shared_status->avs_table_stack, g_stack_avs_table, copy_size);
}

static void __gpufreq_update_shared_status_init_reg(void)
{
#if GPUFREQ_SHARED_STATUS_REG
	unsigned int copy_size = 0;

	if (!g_shared_status)
		return;

	/* [WARNING] GPU is power off at this moment */
	g_reg_mfgsys[IDX_EFUSE_PTPOD21_SN].val = readl(EFUSE_PTPOD21_SN);
	g_reg_mfgsys[IDX_EFUSE_PTPOD22_AVS].val = readl(EFUSE_PTPOD22_AVS);
	g_reg_mfgsys[IDX_EFUSE_PTPOD23_AVS].val = readl(EFUSE_PTPOD23_AVS);
	g_reg_mfgsys[IDX_EFUSE_PTPOD24_AVS].val = readl(EFUSE_PTPOD24_AVS);
	g_reg_mfgsys[IDX_EFUSE_PTPOD25_AVS].val = readl(EFUSE_PTPOD25_AVS);
	g_reg_mfgsys[IDX_EFUSE_PTPOD26_AVS].val = readl(EFUSE_PTPOD26_AVS);
	g_reg_mfgsys[IDX_SPM_SPM2GPUPM_CON].val = readl(SPM_SPM2GPUPM_CON);

	copy_size = sizeof(struct gpufreq_reg_info) * NUM_MFGSYS_REG;
	memcpy(g_shared_status->reg_mfgsys, g_reg_mfgsys, copy_size);
#endif /* GPUFREQ_SHARED_STATUS_REG */
}

static void __gpufreq_update_shared_status_power_reg(void)
{
#if GPUFREQ_SHARED_STATUS_REG
	unsigned int copy_size = 0;

	if (!g_shared_status)
		return;

	/* acquire sema before access MFG_TOP_CFG */
	__gpufreq_set_semaphore(SEMA_ACQUIRE);

	g_reg_mfgsys[IDX_MFG_CG_CON].val = readl_mfg(MFG_CG_CON);
	g_reg_mfgsys[IDX_MFG_DCM_CON_0].val = readl_mfg(MFG_DCM_CON_0);
	g_reg_mfgsys[IDX_MFG_ASYNC_CON].val = readl_mfg(MFG_ASYNC_CON);
	g_reg_mfgsys[IDX_MFG_GLOBAL_CON].val = readl_mfg(MFG_GLOBAL_CON);
	g_reg_mfgsys[IDX_MFG_AXCOHERENCE_CON].val = readl_mfg(MFG_AXCOHERENCE_CON);
	g_reg_mfgsys[IDX_MFG_DUMMY_REG].val = readl_mfg(MFG_DUMMY_REG);
	g_reg_mfgsys[IDX_MFG_SRAM_FUL_SEL_ULV].val = readl_mfg(MFG_SRAM_FUL_SEL_ULV);
	g_reg_mfgsys[IDX_MFG_PLL_CON0].val = readl(MFG_PLL_CON0);
	g_reg_mfgsys[IDX_MFG_PLL_CON1].val = readl(MFG_PLL_CON1);
	g_reg_mfgsys[IDX_MFGSC_PLL_CON0].val = readl(MFGSC_PLL_CON0);
	g_reg_mfgsys[IDX_MFGSC_PLL_CON1].val = readl(MFGSC_PLL_CON1);
	g_reg_mfgsys[IDX_MFG_RPC_AO_CLK_CFG].val = readl(MFG_RPC_AO_CLK_CFG);
	g_reg_mfgsys[IDX_MFG_RPC_MFG1_PWR_CON].val = readl(MFG_RPC_MFG1_PWR_CON);
#if !GPUFREQ_PDCA_ENABLE
	g_reg_mfgsys[IDX_MFG_RPC_MFG2_PWR_CON].val = readl(MFG_RPC_MFG2_PWR_CON);
	g_reg_mfgsys[IDX_MFG_RPC_MFG3_PWR_CON].val = readl(MFG_RPC_MFG3_PWR_CON);
	g_reg_mfgsys[IDX_MFG_RPC_MFG4_PWR_CON].val = readl(MFG_RPC_MFG4_PWR_CON);
	g_reg_mfgsys[IDX_MFG_RPC_MFG5_PWR_CON].val = readl(MFG_RPC_MFG5_PWR_CON);
	g_reg_mfgsys[IDX_MFG_RPC_MFG6_PWR_CON].val = readl(MFG_RPC_MFG6_PWR_CON);
	g_reg_mfgsys[IDX_MFG_RPC_MFG7_PWR_CON].val = readl(MFG_RPC_MFG7_PWR_CON);
	g_reg_mfgsys[IDX_MFG_RPC_MFG8_PWR_CON].val = readl(MFG_RPC_MFG8_PWR_CON);
	g_reg_mfgsys[IDX_MFG_RPC_MFG9_PWR_CON].val = readl(MFG_RPC_MFG9_PWR_CON);
	g_reg_mfgsys[IDX_MFG_RPC_MFG10_PWR_CON].val = readl(MFG_RPC_MFG10_PWR_CON);
	g_reg_mfgsys[IDX_MFG_RPC_MFG11_PWR_CON].val = readl(MFG_RPC_MFG11_PWR_CON);
	g_reg_mfgsys[IDX_MFG_RPC_MFG12_PWR_CON].val = readl(MFG_RPC_MFG12_PWR_CON);
	g_reg_mfgsys[IDX_MFG_RPC_MFG13_PWR_CON].val = readl(MFG_RPC_MFG13_PWR_CON);
	g_reg_mfgsys[IDX_MFG_RPC_MFG14_PWR_CON].val = readl(MFG_RPC_MFG14_PWR_CON);
	g_reg_mfgsys[IDX_MFG_RPC_MFG15_PWR_CON].val = readl(MFG_RPC_MFG15_PWR_CON);
	g_reg_mfgsys[IDX_MFG_RPC_MFG16_PWR_CON].val = readl(MFG_RPC_MFG16_PWR_CON);
	g_reg_mfgsys[IDX_MFG_RPC_MFG17_PWR_CON].val = readl(MFG_RPC_MFG17_PWR_CON);
	g_reg_mfgsys[IDX_MFG_RPC_MFG18_PWR_CON].val = readl(MFG_RPC_MFG18_PWR_CON);
	g_reg_mfgsys[IDX_MFG_RPC_MFG19_PWR_CON].val = readl(MFG_RPC_MFG19_PWR_CON);
#endif /* GPUFREQ_PDCA_ENABLE */
	g_reg_mfgsys[IDX_MFG_RPC_SLP_PROT_EN_STA].val = readl(MFG_RPC_SLP_PROT_EN_STA);
	g_reg_mfgsys[IDX_SPM_MFG0_PWR_CON].val = readl(SPM_MFG0_PWR_CON);
	g_reg_mfgsys[IDX_SPM_SOC_BUCK_ISO_CON].val = readl(SPM_SOC_BUCK_ISO_CON);
	g_reg_mfgsys[IDX_TOPCK_CLK_CFG_3].val = readl(TOPCK_CLK_CFG_3);
	g_reg_mfgsys[IDX_TOPCK_CLK_CFG_30].val = readl(TOPCK_CLK_CFG_30);
	g_reg_mfgsys[IDX_NTH_MFG_EMI1_GALS_SLV_DBG].val = readl(NTH_MFG_EMI1_GALS_SLV_DBG);
	g_reg_mfgsys[IDX_NTH_MFG_EMI0_GALS_SLV_DBG].val = readl(NTH_MFG_EMI0_GALS_SLV_DBG);
	g_reg_mfgsys[IDX_STH_MFG_EMI1_GALS_SLV_DBG].val = readl(STH_MFG_EMI1_GALS_SLV_DBG);
	g_reg_mfgsys[IDX_STH_MFG_EMI0_GALS_SLV_DBG].val = readl(STH_MFG_EMI0_GALS_SLV_DBG);
	g_reg_mfgsys[IDX_NTH_M6M7_IDLE_BIT_EN_1].val = readl(NTH_M6M7_IDLE_BIT_EN_1);
	g_reg_mfgsys[IDX_NTH_M6M7_IDLE_BIT_EN_0].val = readl(NTH_M6M7_IDLE_BIT_EN_0);
	g_reg_mfgsys[IDX_STH_M6M7_IDLE_BIT_EN_1].val = readl(STH_M6M7_IDLE_BIT_EN_1);
	g_reg_mfgsys[IDX_STH_M6M7_IDLE_BIT_EN_0].val = readl(STH_M6M7_IDLE_BIT_EN_0);
	g_reg_mfgsys[IDX_IFR_MFGSYS_PROT_EN_STA_0].val = readl(IFR_MFGSYS_PROT_EN_STA_0);
	g_reg_mfgsys[IDX_IFR_MFGSYS_PROT_RDY_STA_0].val = readl(IFR_MFGSYS_PROT_RDY_STA_0);
	g_reg_mfgsys[IDX_IFR_EMISYS_PROTECT_EN_STA_0].val = readl(IFR_EMISYS_PROTECT_EN_STA_0);
	g_reg_mfgsys[IDX_IFR_EMISYS_PROTECT_EN_STA_1].val = readl(IFR_EMISYS_PROTECT_EN_STA_1);
	g_reg_mfgsys[IDX_NTH_EMI_AO_DEBUG_CTRL0].val = readl(NTH_EMI_AO_DEBUG_CTRL0);
	g_reg_mfgsys[IDX_STH_EMI_AO_DEBUG_CTRL0].val = readl(STH_EMI_AO_DEBUG_CTRL0);
	g_reg_mfgsys[IDX_INFRA_AO_BUS0_U_DEBUG_CTRL0].val = readl(INFRA_AO_BUS0_U_DEBUG_CTRL0);
	g_reg_mfgsys[IDX_INFRA_AO1_BUS1_U_DEBUG_CTRL0].val = readl(INFRA_AO1_BUS1_U_DEBUG_CTRL0);

	copy_size = sizeof(struct gpufreq_reg_info) * NUM_MFGSYS_REG;
	memcpy(g_shared_status->reg_mfgsys, g_reg_mfgsys, copy_size);

	__gpufreq_set_semaphore(SEMA_RELEASE);
#endif /* GPUFREQ_SHARED_STATUS_REG */
}

static void __gpufreq_update_shared_status_active_idle_reg(void)
{
#if GPUFREQ_SHARED_STATUS_REG
	unsigned int copy_size = 0;

	if (!g_shared_status)
		return;

	/* acquire sema before access MFG_TOP_CFG */
	__gpufreq_set_semaphore(SEMA_ACQUIRE);

	g_reg_mfgsys[IDX_MFG_PLL_CON0].val = readl(MFG_PLL_CON0);
	g_reg_mfgsys[IDX_MFG_PLL_CON1].val = readl(MFG_PLL_CON1);
	g_reg_mfgsys[IDX_MFGSC_PLL_CON0].val = readl(MFGSC_PLL_CON0);
	g_reg_mfgsys[IDX_MFGSC_PLL_CON1].val = readl(MFGSC_PLL_CON1);
	g_reg_mfgsys[IDX_TOPCK_CLK_CFG_3].val = readl(TOPCK_CLK_CFG_3);
	g_reg_mfgsys[IDX_TOPCK_CLK_CFG_30].val = readl(TOPCK_CLK_CFG_30);

	copy_size = sizeof(struct gpufreq_reg_info) * NUM_MFGSYS_REG;
	memcpy(g_shared_status->reg_mfgsys, g_reg_mfgsys, copy_size);

	__gpufreq_set_semaphore(SEMA_RELEASE);
#endif /* GPUFREQ_SHARED_STATUS_REG */
}

static void __gpufreq_update_shared_status_dvfs_reg(void)
{
#if GPUFREQ_SHARED_STATUS_REG
	unsigned int copy_size = 0;

	if (!g_shared_status)
		return;

	/* acquire sema before access MFG_TOP_CFG */
	__gpufreq_set_semaphore(SEMA_ACQUIRE);

	g_reg_mfgsys[IDX_MFG_DUMMY_REG].val = readl_mfg(MFG_DUMMY_REG);
	g_reg_mfgsys[IDX_MFG_SRAM_FUL_SEL_ULV].val = readl_mfg(MFG_SRAM_FUL_SEL_ULV);
	g_reg_mfgsys[IDX_MFG_PLL_CON0].val = readl(MFG_PLL_CON0);
	g_reg_mfgsys[IDX_MFG_PLL_CON1].val = readl(MFG_PLL_CON1);
	g_reg_mfgsys[IDX_MFGSC_PLL_CON0].val = readl(MFGSC_PLL_CON0);
	g_reg_mfgsys[IDX_MFGSC_PLL_CON1].val = readl(MFGSC_PLL_CON1);
	g_reg_mfgsys[IDX_TOPCK_CLK_CFG_3].val = readl(TOPCK_CLK_CFG_3);
	g_reg_mfgsys[IDX_TOPCK_CLK_CFG_30].val = readl(TOPCK_CLK_CFG_30);

	copy_size = sizeof(struct gpufreq_reg_info) * NUM_MFGSYS_REG;
	memcpy(g_shared_status->reg_mfgsys, g_reg_mfgsys, copy_size);

	__gpufreq_set_semaphore(SEMA_RELEASE);
#endif /* GPUFREQ_SHARED_STATUS_REG */
}

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

	/* update current status to shared memory */
	if (g_shared_status)
		g_shared_status->dvfs_state = g_dvfs_state;

	mutex_unlock(&gpufreq_lock);
}

/* API: apply/restore Vaging to working table of STACK */
static void __gpufreq_set_margin_mode(unsigned int val)
{
	/* update volt margin */
	__gpufreq_apply_restore_margin(TARGET_GPU, val);
	__gpufreq_apply_restore_margin(TARGET_STACK, val);

	/* update power info to working table */
	__gpufreq_measure_power();

	/* update DVFS constraint */
	__gpufreq_update_springboard();

	/* update current status to shared memory */
	if (g_shared_status)
		__gpufreq_update_shared_status_opp_table();
}

/* API: enable/disable GPM 1.0 */
static void __gpufreq_set_gpm_mode(unsigned int version, unsigned int val)
{
	if (version == 1)
		g_gpm1_mode = val;

	/* update current status to shared memory */
	if (g_shared_status)
		g_shared_status->gpm1_mode = g_gpm1_mode;
}

/* API: apply (enable) / restore (disable) margin */
static void __gpufreq_apply_restore_margin(enum gpufreq_target target, unsigned int val)
{
	struct gpufreq_opp_info *working_table = NULL;
	struct gpufreq_opp_info *signed_table = NULL;
	int working_opp_num = 0, signed_opp_num = 0, segment_upbound = 0, i = 0;

	if (target == TARGET_STACK) {
		working_table = g_stack.working_table;
		signed_table = g_stack.signed_table;
		working_opp_num = g_stack.opp_num;
		signed_opp_num = g_stack.signed_opp_num;
		segment_upbound = g_stack.segment_upbound;
	} else {
		working_table = g_gpu.working_table;
		signed_table = g_gpu.signed_table;
		working_opp_num = g_gpu.opp_num;
		signed_opp_num = g_gpu.signed_opp_num;
		segment_upbound = g_gpu.segment_upbound;
	}

	/* update margin to signed table */
	for (i = 0; i < signed_opp_num; i++) {
		if (val == FEAT_DISABLE)
			signed_table[i].volt += signed_table[i].margin;
		else if (val == FEAT_ENABLE)
			signed_table[i].volt -= signed_table[i].margin;
		signed_table[i].vsram = __gpufreq_get_vsram_by_vlogic(signed_table[i].volt);
	}

	for (i = 0; i < working_opp_num; i++) {
		working_table[i].volt = signed_table[segment_upbound + i].volt;
		working_table[i].vsram = signed_table[segment_upbound + i].vsram;

		GPUFREQ_LOGD("Margin mode: %d, %s[%d] Volt: %d, Vsram: %d",
			val, target == TARGET_STACK ? "STACK" : "GPU",
			i, working_table[i].volt, working_table[i].vsram);
	}
}

/* API: update DVFS constraint springboard */
static void __gpufreq_update_springboard(void)
{
	struct gpufreq_opp_info *signed_gpu = NULL;
	struct gpufreq_opp_info *signed_stack = NULL;
	int i = 0, constraint_num = 0, oppidx = 0;

	signed_gpu = g_gpu.signed_table;
	signed_stack = g_stack.signed_table;
	constraint_num = NUM_CONSTRAINT_IDX;

	/* update signed volt and sel to springboard */
	for (i = 0; i < constraint_num; i++) {
		if (g_constraint_idx[i] == CONSTRAINT_VSRAM_PARK)
			oppidx = __gpufreq_get_idx_by_vgpu(VSRAM_THRESH);
		else
			oppidx = g_constraint_idx[i];
		g_springboard[i].oppidx = oppidx;
		g_springboard[i].vgpu = signed_gpu[oppidx].volt;
		g_springboard[i].vstack = signed_stack[oppidx].volt;
	}

	/* update scale-up volt springboard back to front */
	for (i = constraint_num - 1; i > 0; i--) {
		g_springboard[i].vgpu_up = g_springboard[i - 1].vgpu;
		g_springboard[i].vstack_up = g_springboard[i - 1].vstack;
	}
	g_springboard[0].vgpu_up = VGPU_MAX_VOLT;
	g_springboard[0].vstack_up = VSTACK_MAX_VOLT;

	/* update scale-down volt springboard front to back */
	for (i = 0; i < constraint_num - 1; i++) {
		g_springboard[i].vgpu_down = g_springboard[i + 1].vgpu;
		g_springboard[i].vstack_down = g_springboard[i + 1].vstack;
	}
	g_springboard[constraint_num - 1].vgpu_down = VGPU_MIN_VOLT;
	g_springboard[constraint_num - 1].vstack_down = VSTACK_MIN_VOLT;

	for (i = 0; i < constraint_num; i++)
		GPUFREQ_LOGI(
			"[%02d] Vgpu: %6d (Up: %6d, Down: %6d), Vstack: %6d (Up: %6d, Down: %6d)",
			g_springboard[i].oppidx, g_springboard[i].vgpu,
			g_springboard[i].vgpu_up, g_springboard[i].vgpu_down,
			g_springboard[i].vstack, g_springboard[i].vstack_up,
			g_springboard[i].vstack_down);
};

#if GPUFREQ_MSSV_TEST_MODE
static void __gpufreq_mssv_set_stack_sel(unsigned int val)
{
	if (val == 1)
		writel((readl_mfg(MFG_DUMMY_REG) | BIT(31)), MFG_DUMMY_REG);
	else if (val == 0)
		writel((readl_mfg(MFG_DUMMY_REG) & ~BIT(31)), MFG_DUMMY_REG);
}

static void __gpufreq_mssv_set_del_sel(unsigned int val)
{
	if (val == 1)
		writel((readl_mfg(MFG_SRAM_FUL_SEL_ULV) | BIT(0)), MFG_SRAM_FUL_SEL_ULV);
	else if (val == 0)
		writel((readl_mfg(MFG_SRAM_FUL_SEL_ULV) & ~BIT(0)), MFG_SRAM_FUL_SEL_ULV);
}
#endif /* GPUFREQ_MSSV_TEST_MODE */

static void __gpufreq_dvfs_sel_config(enum gpufreq_opp_direct direct, unsigned int volt)
{
	unsigned int stack_sel_volt = 0, del_sel_volt = 0;

	stack_sel_volt = g_stack.signed_table[STACK_SEL_OPP].volt;
	del_sel_volt = g_stack.signed_table[SRAM_DEL_SEL_OPP].volt;

	/* acquire sema before access MFG_TOP_CFG */
	__gpufreq_set_semaphore(SEMA_ACQUIRE);

	/*
	 * Vstack @ 670MHz < Vstack                  : STACK_SEL = 0,      DEL_SEL = 0
	 * Vstack @ 670MHz = Vstack                  : STACK_SEL = 0 or 1, DEL_SEL = 0
	 * Vstack @ 390MHz < Vstack < Vstack @ 670MHz: STACK_SEL = 1,      DEL_SEL = 0
	 * Vstack @ 390MHz = Vstack                  : STACK_SEL = 1,      DEL_SEL = 0 or 1
	 * Vstack @ 390MHz < Vstack                  : STACK_SEL = 1,      DEL_SEL = 1
	 *
	 * STACK_SEL: MFG_DUMMY_REG 0x13FBF500 [31] eco_2_data_path
	 * DEL_SEL  : MFG_SRAM_FUL_SEL_ULV 0x13FBF080 [0] FUL_SEL_ULV
	 */
	/* high volt use SEL=0 */
	if (direct == SCALE_UP) {
		if (volt == stack_sel_volt)
			writel((readl_mfg(MFG_DUMMY_REG) & ~BIT(31)), MFG_DUMMY_REG);
		else if (volt == del_sel_volt)
			writel((readl_mfg(MFG_SRAM_FUL_SEL_ULV) & ~BIT(0)), MFG_SRAM_FUL_SEL_ULV);
	/* low volt use SEL=1 */
	} else if (direct == SCALE_DOWN) {
		if (volt == stack_sel_volt)
			writel((readl_mfg(MFG_DUMMY_REG) | BIT(31)), MFG_DUMMY_REG);
		else if (volt == del_sel_volt)
			writel((readl_mfg(MFG_SRAM_FUL_SEL_ULV) | BIT(0)), MFG_SRAM_FUL_SEL_ULV);
	/* Brisket FLL favor SEL=0 at critical volt */
	} else if (direct == SCALE_STAY) {
		if (volt == stack_sel_volt)
			writel((readl_mfg(MFG_DUMMY_REG) & ~BIT(31)), MFG_DUMMY_REG);
		else if (volt == del_sel_volt)
			writel((readl_mfg(MFG_SRAM_FUL_SEL_ULV) & ~BIT(0)), MFG_SRAM_FUL_SEL_ULV);
	}

	GPUFREQ_LOGD("Vstack: %d (%s) MFG_DUMMY_REG: 0x%08x, MFG_SRAM_FUL_SEL_ULV: 0x%08x",
		volt, direct == SCALE_DOWN ? "DOWN" : (direct == SCALE_UP ? "UP" : "STAY"),
		readl_mfg(MFG_DUMMY_REG), readl_mfg(MFG_SRAM_FUL_SEL_ULV));

	__gpufreq_set_semaphore(SEMA_RELEASE);
}

/* API: use Vstack to find constraint range as Vstack is unique in OPP */
static void __gpufreq_get_park_volt(enum gpufreq_opp_direct direct,
	unsigned int cur_vstack, unsigned int *vgpu_park, unsigned int *vstack_park)
{
	int i = 0, constraint_num = NUM_CONSTRAINT_IDX;

	/*
	 * [Springboard Guide]
	 * [Scale-up]  : find springboard volt that SMALLER than cur_volt
	 * [Scale-down]: find springboard volt that LARGER than cur_volt
	 *
	 * e.g. sb[2].vstack = 0.625V < cur_vstack = 0.65V < sb[1].vstack = 0.675V
	 * [Scale-up]   use sb[1].vstack_down
	 * [Scale-down] use sb[2].vstack_up
	 */
	if (direct == SCALE_UP) {
		/* find largest volt which is smaller than cur_volt */
		for (i = 0; i < constraint_num; i++)
			if (cur_vstack >= g_springboard[i].vstack)
				break;
		/* boundary check */
		i = i < constraint_num ? i : (constraint_num - 1);
		*vgpu_park = g_springboard[i].vgpu_up;
		*vstack_park = g_springboard[i].vstack_up;
	} else if (direct == SCALE_DOWN) {
		/* find smallest volt which is larger than cur_volt */
		for (i = constraint_num - 1; i >= 0; i--)
			if (cur_vstack <= g_springboard[i].vstack)
				break;
		/* boundary check */
		i = i < 0 ? 0 : i;
		*vgpu_park = g_springboard[i].vgpu_down;
		*vstack_park = g_springboard[i].vstack_down;
	}

	GPUFREQ_LOGD("Vstack: %d (%s) parking Vgpu: %d, Vstack: %d",
		cur_vstack, direct == SCALE_DOWN ? "DOWN" : (direct == SCALE_UP ? "UP" : "STAY"),
		*vgpu_park, *vstack_park);
}

/* API: main DVFS algorithm aligned with HW limitation */
static int __gpufreq_generic_scale(
	unsigned int fgpu_old, unsigned int fgpu_new,
	unsigned int vgpu_old, unsigned int vgpu_new,
	unsigned int fstack_old, unsigned int fstack_new,
	unsigned int vstack_old, unsigned int vstack_new,
	unsigned int vsram_old, unsigned int vsram_new)
{
	int ret = GPUFREQ_SUCCESS;
	unsigned int vgpu_park = 0, vstack_park = 0;
	unsigned int target_vgpu = 0, target_vstack = 0, target_vsram = 0;

	GPUFREQ_TRACE_START(
		"fgpu=(%d->%d), vgpu=(%d->%d), fstack=(%d->%d), vstack=(%d->%d), vsram=(%d->%d)",
		fgpu_old, fgpu_new, vgpu_old, vgpu_new,
		fstack_old, fstack_new, vstack_old, vstack_new,
		vsram_old, vsram_new);

	/*
	 * [WARNING]
	 * 1. Assume GPU & STACK OPP table are 1-1 mapping.
	 * 2. Assume Vgpu's OPP idx always align with Vstack's OPP idx.
	 */

	/* scale-up: Vstack -> (Vsram) -> Vgpu -> Freq */
	if (fstack_new > fstack_old) {
		/* Volt */
		while ((vstack_new != vstack_old) || (vgpu_new != vgpu_old)) {
			/* config DVFS SEL */
			__gpufreq_dvfs_sel_config(SCALE_UP, vstack_old);
			/* find reachable volt fitting DVFS constraint via Vstack */
			__gpufreq_get_park_volt(SCALE_UP, vstack_old, &vgpu_park, &vstack_park);
			target_vstack = vstack_park < vstack_new ? vstack_park : vstack_new;
			target_vgpu = vgpu_park < vgpu_new ? vgpu_park : vgpu_new;
			target_vsram =
				__gpufreq_get_vsram_by_vlogic(MAX(target_vgpu, target_vstack));

			/* VSTACK */
			ret = __gpufreq_volt_scale_stack(vstack_old, target_vstack);
			if (unlikely(ret))
				goto done;
			vstack_old = target_vstack;
			/* VSRAM */
			ret = __gpufreq_volt_scale_sram(vsram_old, target_vsram);
			if (unlikely(ret))
				goto done;
			vsram_old = target_vsram;
			/* VGPU */
			ret = __gpufreq_volt_scale_gpu(vgpu_old, target_vgpu);
			if (unlikely(ret))
				goto done;
			vgpu_old = target_vgpu;
		}
		/* additionally config SEL for Brisket FLL after scaling to target volt */
		__gpufreq_dvfs_sel_config(SCALE_STAY, vstack_old);

		/* Freq */
		ret = __gpufreq_freq_scale_stack(fstack_old, fstack_new);
		if (unlikely(ret))
			goto done;
		ret = __gpufreq_freq_scale_gpu(fgpu_old, fgpu_new);
		if (unlikely(ret))
			goto done;
	/* else: Freq -> Vgpu -> (Vsram) -> Vstack */
	} else {
		/* Freq */
		ret = __gpufreq_freq_scale_gpu(fgpu_old, fgpu_new);
		if (unlikely(ret))
			goto done;
		ret = __gpufreq_freq_scale_stack(fstack_old, fstack_new);
		if (unlikely(ret))
			goto done;

		/* Volt */
		while ((vstack_new != vstack_old) || (vgpu_new != vgpu_old)) {
			/* config DVFS SEL */
			__gpufreq_dvfs_sel_config(SCALE_DOWN, vstack_old);
			/* find reachable volt fitting DVFS constraint via Vstack */
			__gpufreq_get_park_volt(SCALE_DOWN, vstack_old, &vgpu_park, &vstack_park);
			target_vstack = vstack_park > vstack_new ? vstack_park : vstack_new;
			target_vgpu = vgpu_park > vgpu_new ? vgpu_park : vgpu_new;
			target_vsram =
				__gpufreq_get_vsram_by_vlogic(MAX(target_vgpu, target_vstack));

			/* VGPU */
			ret = __gpufreq_volt_scale_gpu(vgpu_old, target_vgpu);
			if (unlikely(ret))
				goto done;
			vgpu_old = target_vgpu;
			/* VSRAM */
			ret = __gpufreq_volt_scale_sram(vsram_old, target_vsram);
			if (unlikely(ret))
				goto done;
			vsram_old = target_vsram;
			/* VSTACK */
			ret = __gpufreq_volt_scale_stack(vstack_old, target_vstack);
			if (unlikely(ret))
				goto done;
			vstack_old = target_vstack;
		}
		/* additionally config SEL for Brisket FLL after scaling to target volt */
		__gpufreq_dvfs_sel_config(SCALE_STAY, vstack_old);
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
	int ret = GPUFREQ_SUCCESS;
	int cur_oppidx = 0,  target_oppidx = 0;
	struct gpufreq_opp_info *working_gpu = g_gpu.working_table;
	unsigned int cur_fgpu = 0, cur_vgpu = 0, target_fgpu = 0, target_vgpu = 0;
	unsigned int cur_fstack = 0, cur_vstack = 0, target_fstack = 0, target_vstack = 0;
	unsigned int cur_vsram = 0, target_vsram = 0;

	GPUFREQ_TRACE_START("target_freq=%d, target_volt=%d, key=%d",
		target_freq, target_volt, key);

	mutex_lock(&gpufreq_lock);

	/* check dvfs state */
	if (g_dvfs_state & ~key) {
		GPUFREQ_LOGD("unavailable DVFS state (0x%x)", g_dvfs_state);
		ret = GPUFREQ_SUCCESS;
		goto done_unlock;
	}

	/* prepare OPP setting */
	cur_oppidx = g_stack.cur_oppidx;
	cur_fgpu = g_gpu.cur_freq;
	cur_vgpu = g_gpu.cur_volt;
	cur_fstack = g_stack.cur_freq;
	cur_vstack = g_stack.cur_volt;
	cur_vsram = g_stack.cur_vsram;

	target_oppidx = __gpufreq_get_idx_by_vstack(target_volt);
	target_fgpu = working_gpu[target_oppidx].freq;
	target_vgpu = working_gpu[target_oppidx].volt;
	target_fstack = target_freq;
	target_vstack = target_volt;
	target_vsram =
		MAX(working_gpu[target_oppidx].vsram, __gpufreq_get_vsram_by_vlogic(target_vstack));

	GPUFREQ_LOGD(
		"begin to commit STACK F(%d->%d) V(%d->%d), GPU F(%d->%d) V(%d->%d), VSRAM(%d->%d)",
		cur_fstack, target_fstack, cur_vstack, target_vstack,
		cur_fgpu, target_fgpu, cur_vgpu, target_vgpu,
		cur_vsram, target_vsram);

	ret = __gpufreq_generic_scale(cur_fgpu, target_fgpu, cur_vgpu, target_vgpu,
		cur_fstack, target_fstack, cur_vstack, target_vstack, cur_vsram, target_vsram);
	if (ret) {
		GPUFREQ_LOGE("fail to scale to GPU F(%d) V(%d), STACK F(%d) V(%d), VSRAM(%d)",
			target_fgpu, target_vgpu, target_fstack, target_vstack, target_vsram);
		goto done_unlock;
	}

	g_gpu.cur_oppidx = target_oppidx;
	g_stack.cur_oppidx = target_oppidx;
	__gpufreq_footprint_oppidx(target_oppidx);

	/* update current status to shared memory */
	if (g_shared_status) {
		g_shared_status->cur_oppidx_gpu = g_gpu.cur_oppidx;
		g_shared_status->cur_fgpu = g_gpu.cur_freq;
		g_shared_status->cur_vgpu = g_gpu.cur_volt;
		g_shared_status->cur_vsram_gpu = g_gpu.cur_vsram;
		g_shared_status->cur_power_gpu = g_gpu.working_table[g_gpu.cur_oppidx].power;
		g_shared_status->cur_oppidx_stack = g_stack.cur_oppidx;
		g_shared_status->cur_fstack = g_stack.cur_freq;
		g_shared_status->cur_vstack = g_stack.cur_volt;
		g_shared_status->cur_vsram_stack = g_stack.cur_vsram;
		g_shared_status->cur_power_stack = g_stack.working_table[g_stack.cur_oppidx].power;
		__gpufreq_update_shared_status_dvfs_reg();
	}

done_unlock:
	mutex_unlock(&gpufreq_lock);

	GPUFREQ_TRACE_END();

	return ret;
}

static int __gpufreq_switch_clksrc(enum gpufreq_target target, enum gpufreq_clk_src clksrc)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("clksrc=%d", clksrc);

	if (target == TARGET_STACK) {
		if (clksrc == CLOCK_MAIN)
			ret = clk_set_parent(g_clk->clk_sc_mux, g_clk->clk_sc_main_parent);
		else if (clksrc == CLOCK_SUB)
			ret = clk_set_parent(g_clk->clk_sc_mux, g_clk->clk_sc_sub_parent);
	} else {
		if (clksrc == CLOCK_MAIN)
			ret = clk_set_parent(g_clk->clk_mux, g_clk->clk_main_parent);
		else if (clksrc == CLOCK_SUB)
			ret = clk_set_parent(g_clk->clk_mux, g_clk->clk_sub_parent);
	}

	if (unlikely(ret))
		__gpufreq_abort("fail to switch %s clk src: %d (%d)",
			target == TARGET_STACK ? "STACK" : "GPU",
			clksrc, ret);

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
	unsigned long long pcw = 0;

	if ((freq >= POSDIV_16_MIN_FREQ) && (freq <= POSDIV_2_MAX_FREQ))
		pcw = (((unsigned long long)freq * (1 << posdiv)) << DDS_SHIFT) / MFGPLL_FIN / 1000;
	else
		__gpufreq_abort("out of range Freq: %d", freq);

	GPUFREQ_LOGD("target freq: %d, posdiv: %d, pcw: 0x%llx", freq, posdiv, pcw);

	return (unsigned int)pcw;
}

static enum gpufreq_posdiv __gpufreq_get_real_posdiv_gpu(void)
{
	u32 con1 = 0;
	enum gpufreq_posdiv posdiv = POSDIV_POWER_1;

	con1 = readl(MFG_PLL_CON1);

	posdiv = (con1 & GENMASK(26, 24)) >> POSDIV_SHIFT;

	return posdiv;
}

static enum gpufreq_posdiv __gpufreq_get_real_posdiv_stack(void)
{
	u32 con1 = 0;
	enum gpufreq_posdiv posdiv = POSDIV_POWER_1;

	con1 = readl(MFGSC_PLL_CON1);

	posdiv = (con1 & GENMASK(26, 24)) >> POSDIV_SHIFT;

	return posdiv;
}

static enum gpufreq_posdiv __gpufreq_get_posdiv_by_freq(unsigned int freq)
{
	if (freq > POSDIV_2_MAX_FREQ)
		return POSDIV_POWER_1;
	else if (freq > POSDIV_4_MAX_FREQ)
		return POSDIV_POWER_2;
	else if (freq > POSDIV_4_MIN_FREQ)
		return POSDIV_POWER_4;
	else if (freq > POSDIV_8_MIN_FREQ)
		return POSDIV_POWER_8;
	else if (freq > POSDIV_16_MIN_FREQ)
		return POSDIV_POWER_16;
	else {
		__gpufreq_abort("invalid freq: %d", freq);
		return POSDIV_POWER_16;
	}
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

	if (freq_new == freq_old)
		goto done;

	/*
	 * MFG_PLL_CON1[31:31]: MFGPLL_SDM_PCW_CHG
	 * MFG_PLL_CON1[26:24]: MFGPLL_POSDIV
	 * MFG_PLL_CON1[21:0] : MFGPLL_SDM_PCW (DDS)
	 */
	cur_posdiv = __gpufreq_get_real_posdiv_gpu();
	target_posdiv = __gpufreq_get_posdiv_by_freq(freq_new);
	/* compute PCW based on target Freq */
	pcw = __gpufreq_calculate_pcw(freq_new, target_posdiv);
	if (unlikely(!pcw)) {
		__gpufreq_abort("invalid PCW: 0x%x", pcw);
		goto done;
	}

#if (GPUFREQ_FHCTL_ENABLE && IS_ENABLED(CONFIG_COMMON_CLK_MTK_FREQ_HOPPING))
	if (unlikely(!mtk_fh_set_rate)) {
		__gpufreq_abort("null hopping fp");
		ret = GPUFREQ_ENOENT;
		goto done;
	}
	/* POSDIV remain the same */
	if (target_posdiv == cur_posdiv) {
		/* change PCW by hopping only */
		ret = mtk_fh_set_rate(MFG_PLL_NAME, pcw, target_posdiv);
		if (unlikely(!ret)) {
			__gpufreq_abort("fail to hopping PCW: 0x%x (%d)", pcw, ret);
			ret = GPUFREQ_EINVAL;
			goto done;
		}
	/* freq scale up */
	} else if (freq_new > freq_old) {
		/* 1. change PCW by hopping */
		ret = mtk_fh_set_rate(MFG_PLL_NAME, pcw, target_posdiv);
		if (unlikely(!ret)) {
			__gpufreq_abort("fail to hopping PCW: 0x%x (%d)", pcw, ret);
			ret = GPUFREQ_EINVAL;
			goto done;
		}
		/* 2. compute CON1 with target POSDIV */
		pll = (readl(MFG_PLL_CON1) & ~GENMASK(26, 24)) | (target_posdiv << POSDIV_SHIFT);
		/* 3. change POSDIV by writing CON1 */
		writel(pll, MFG_PLL_CON1);
		/* 4. wait until PLL stable */
		udelay(20);
	/* freq scale down */
	} else {
		/* 1. compute CON1 with target POSDIV */
		pll = (readl(MFG_PLL_CON1) & ~GENMASK(26, 24)) | (target_posdiv << POSDIV_SHIFT);
		/* 2. change POSDIV by writing CON1 */
		writel(pll, MFG_PLL_CON1);
		/* 3. wait until PLL stable */
		udelay(20);
		/* 4. change PCW by hopping */
		ret = mtk_fh_set_rate(MFG_PLL_NAME, pcw, target_posdiv);
		if (unlikely(!ret)) {
			__gpufreq_abort("fail to hopping PCW: 0x%x (%d)", pcw, ret);
			ret = GPUFREQ_EINVAL;
			goto done;
		}
	}
#else
	/* 1. switch to parking clk source */
	ret = __gpufreq_switch_clksrc(TARGET_GPU, CLOCK_SUB);
	if (unlikely(ret))
		goto done;
	/* 2. compute CON1 with PCW and POSDIV */
	pll = BIT(31) | (target_posdiv << POSDIV_SHIFT) | pcw;
	/* 3. change PCW and POSDIV by writing CON1 */
	writel(pll, MFG_PLL_CON1);
	/* 4. wait until PLL stable */
	udelay(20);
	/* 5. switch to main clk source */
	ret = __gpufreq_switch_clksrc(TARGET_GPU, CLOCK_MAIN);
	if (unlikely(ret))
		goto done;
#endif /* GPUFREQ_FHCTL_ENABLE && CONFIG_COMMON_CLK_MTK_FREQ_HOPPING */

	g_gpu.cur_freq = __gpufreq_get_real_fgpu();
	if (unlikely(g_gpu.cur_freq != freq_new))
		__gpufreq_abort("inconsistent cur_freq: %d, target_freq: %d",
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

	if (freq_new == freq_old)
		goto done;

	/*
	 * MFGSC_PLL_CON1[31:31]: MFGPLL_SDM_PCW_CHG
	 * MFGSC_PLL_CON1[26:24]: MFGPLL_POSDIV
	 * MFGSC_PLL_CON1[21:0] : MFGPLL_SDM_PCW (DDS)
	 */
	cur_posdiv = __gpufreq_get_real_posdiv_stack();
	target_posdiv = __gpufreq_get_posdiv_by_freq(freq_new);
	/* compute PCW based on target Freq */
	pcw = __gpufreq_calculate_pcw(freq_new, target_posdiv);
	if (unlikely(!pcw)) {
		__gpufreq_abort("invalid PCW: 0x%x", pcw);
		goto done;
	}

#if (GPUFREQ_FHCTL_ENABLE && IS_ENABLED(CONFIG_COMMON_CLK_MTK_FREQ_HOPPING))
	if (unlikely(!mtk_fh_set_rate)) {
		__gpufreq_abort("null hopping fp");
		ret = GPUFREQ_ENOENT;
		goto done;
	}
	/* POSDIV remain the same */
	if (target_posdiv == cur_posdiv) {
		/* change PCW by hopping only */
		ret = mtk_fh_set_rate(MFGSC_PLL_NAME, pcw, target_posdiv);
		if (unlikely(!ret)) {
			__gpufreq_abort("fail to hopping PCW: 0x%x (%d)", pcw, ret);
			ret = GPUFREQ_EINVAL;
			goto done;
		}
	/* freq scale up */
	} else if (freq_new > freq_old) {
		/* 1. change PCW by hopping */
		ret = mtk_fh_set_rate(MFGSC_PLL_NAME, pcw, target_posdiv);
		if (unlikely(!ret)) {
			__gpufreq_abort("fail to hopping PCW: 0x%x (%d)", pcw, ret);
			ret = GPUFREQ_EINVAL;
			goto done;
		}
		/* 2. compute CON1 with target POSDIV */
		pll = (readl(MFGSC_PLL_CON1) & ~GENMASK(26, 24)) | (target_posdiv << POSDIV_SHIFT);
		/* 3. change POSDIV by writing CON1 */
		writel(pll, MFGSC_PLL_CON1);
		/* 4. wait until PLL stable */
		udelay(20);
	/* freq scale down */
	} else {
		/* 1. compute CON1 with target POSDIV */
		pll = (readl(MFGSC_PLL_CON1) & ~GENMASK(26, 24)) | (target_posdiv << POSDIV_SHIFT);
		/* 2. change POSDIV by writing CON1 */
		writel(pll, MFGSC_PLL_CON1);
		/* 3. wait until PLL stable */
		udelay(20);
		/* 4. change PCW by hopping */
		ret = mtk_fh_set_rate(MFGSC_PLL_NAME, pcw, target_posdiv);
		if (unlikely(!ret)) {
			__gpufreq_abort("fail to hopping PCW: 0x%x (%d)", pcw, ret);
			ret = GPUFREQ_EINVAL;
			goto done;
		}
	}
#else
	/* 1. switch to parking clk source */
	ret = __gpufreq_switch_clksrc(TARGET_STACK, CLOCK_SUB);
	if (unlikely(ret))
		goto done;
	/* 2. compute CON1 with PCW and POSDIV */
	pll = BIT(31) | (target_posdiv << POSDIV_SHIFT) | pcw;
	/* 3. change PCW and POSDIV by writing CON1 */
	writel(pll, MFGSC_PLL_CON1);
	/* 4. wait until PLL stable */
	udelay(20);
	/* 5. switch to main clk source */
	ret = __gpufreq_switch_clksrc(TARGET_STACK, CLOCK_MAIN);
	if (unlikely(ret))
		goto done;
#endif /* GPUFREQ_FHCTL_ENABLE && CONFIG_COMMON_CLK_MTK_FREQ_HOPPING */

	g_stack.cur_freq = __gpufreq_get_real_fstack();
	if (unlikely(g_stack.cur_freq != freq_new))
		__gpufreq_abort("inconsistent cur_freq: %d, target_freq: %d",
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

static unsigned int __gpufreq_settle_time_vgpu_vstack(enum gpufreq_opp_direct direct, int deltaV)
{
	/*
	 * [MT6373_VBUCK4][VGPU]
	 * [MT6373_VBUCK2][VGPUSTACK]
	 * DVFS Rising : (deltaV / 12.5(mV)) + 3.85us + 2us
	 * DVFS Falling: (deltaV / 12.5(mV)) + 3.85us + 2us
	 * deltaV = mV x 100
	 */
	unsigned int t_settle = 0;

	if (direct == SCALE_UP) {
		/* rising */
		t_settle = (deltaV / 1250) + 4 + 2;
	} else if (direct == SCALE_DOWN) {
		/* falling */
		t_settle = (deltaV / 1250) + 4 + 2;
	}

	return t_settle; /* us */
}

static unsigned int __gpufreq_settle_time_vsram(enum gpufreq_opp_direct direct, int deltaV)
{
	/*
	 * [MT6373_VSRAM_DIGRF_AIF][VSRAM]
	 * DVFS Rising : (deltaV / 12.5(mV)) + 5us
	 * DVFS Falling: (deltaV / 12.5(mV)) + 5us
	 * deltaV = mV x 100
	 */
	unsigned int t_settle = 0;

	if (direct == SCALE_UP) {
		/* rising */
		t_settle = (deltaV / 1250) + 5;
	} else if (direct == SCALE_DOWN) {
		/* falling */
		t_settle = (deltaV / 1250) + 5;
	}

	return t_settle; /* us */
}

/* API: scale Volt of GPU via Regulator */
static int __gpufreq_volt_scale_gpu(unsigned int volt_old, unsigned int volt_new)
{
	unsigned int t_settle = 0;
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("volt_old=%d, volt_new=%d", volt_old, volt_new);

	GPUFREQ_LOGD("begin to scale Vgpu: (%d->%d)", volt_old, volt_new);

	if (volt_new == volt_old)
		goto done;
	else if (volt_new > volt_old)
		t_settle = __gpufreq_settle_time_vgpu_vstack(SCALE_UP, (volt_new - volt_old));
	else if (volt_new < volt_old)
		t_settle = __gpufreq_settle_time_vgpu_vstack(SCALE_DOWN, (volt_old - volt_new));

	ret = regulator_set_voltage(g_pmic->reg_vgpu, volt_new * 10, VGPU_MAX_VOLT * 10 + 125);
	if (unlikely(ret)) {
		__gpufreq_abort("fail to set regulator volt: %d (%d)", volt_new, ret);
		goto done;
	}
	udelay(t_settle);

	g_gpu.cur_volt = __gpufreq_get_real_vgpu();
	if (unlikely(g_gpu.cur_volt != volt_new))
		__gpufreq_abort("inconsistent cur_volt: %d, target_volt: %d",
			g_gpu.cur_volt, volt_new);

	GPUFREQ_LOGD("Vgpu: %d, udelay: %d", g_gpu.cur_volt, t_settle);

done:
	GPUFREQ_TRACE_END();

	return ret;
}

/* API: scale Volt of STACK via Regulator */
static int __gpufreq_volt_scale_stack(unsigned int volt_old, unsigned int volt_new)
{
	unsigned int t_settle = 0;
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("volt_old=%d, volt_new=%d", volt_old, volt_new);

	GPUFREQ_LOGD("begin to scale Vstack: (%d->%d)", volt_old, volt_new);

	if (volt_new == volt_old)
		goto done;
	else if (volt_new > volt_old)
		t_settle = __gpufreq_settle_time_vgpu_vstack(SCALE_UP, (volt_new - volt_old));
	else if (volt_new < volt_old)
		t_settle = __gpufreq_settle_time_vgpu_vstack(SCALE_DOWN, (volt_old - volt_new));

	ret = regulator_set_voltage(g_pmic->reg_vstack, volt_new * 10, VSTACK_MAX_VOLT * 10 + 125);
	if (unlikely(ret)) {
		__gpufreq_abort("fail to set regulator volt: %d (%d)", volt_new, ret);
		goto done;
	}
	udelay(t_settle);

	g_stack.cur_volt = __gpufreq_get_real_vstack();
	if (unlikely(g_stack.cur_volt != volt_new))
		__gpufreq_abort("inconsistent cur_volt: %d, target_volt: %d",
			g_stack.cur_volt, volt_new);

	GPUFREQ_LOGD("Vstack: %d, udelay: %d", g_stack.cur_volt, t_settle);

done:
	GPUFREQ_TRACE_END();

	return ret;
}

/* API: scale Volt of STACK via Regulator */
static int __gpufreq_volt_scale_sram(unsigned int volt_old, unsigned int volt_new)
{
	unsigned int t_settle = 0;
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("volt_old=%d, volt_new=%d", volt_old, volt_new);

	GPUFREQ_LOGD("begin to scale Vsram: (%d->%d)", volt_old, volt_new);

	if (volt_new == volt_old)
		goto done;
	else if (volt_new > volt_old)
		t_settle = __gpufreq_settle_time_vsram(SCALE_UP, (volt_new - volt_old));
	else if (volt_new < volt_old)
		t_settle = __gpufreq_settle_time_vsram(SCALE_DOWN, (volt_old - volt_new));

	ret = regulator_set_voltage(g_pmic->reg_vsram, volt_new * 10, VSRAM_MAX_VOLT * 10 + 125);
	if (unlikely(ret)) {
		__gpufreq_abort("fail to set regulator volt: %d (%d)", volt_new, ret);
		goto done;
	}
	udelay(t_settle);

	g_gpu.cur_vsram = __gpufreq_get_real_vsram();
	g_stack.cur_vsram = g_gpu.cur_vsram;
	if (unlikely(g_stack.cur_vsram != volt_new))
		__gpufreq_abort("inconsistent cur_volt: %d, target_volt: %d",
			g_stack.cur_vsram, volt_new);

	GPUFREQ_LOGD("Vsram: %d, udelay: %d", g_stack.cur_vsram, t_settle);

done:
	GPUFREQ_TRACE_END();

	return ret;
}

static void __gpufreq_dump_bringup_status(struct platform_device *pdev)
{
	struct device *gpufreq_dev = &pdev->dev;
	struct resource *res = NULL;

	if (unlikely(!gpufreq_dev)) {
		GPUFREQ_LOGE("fail to find gpufreq device (ENOENT)");
		return;
	}

	/* 0x13000000 */
	g_mali_base = __gpufreq_of_ioremap("mediatek,mali", 0);
	if (unlikely(!g_mali_base)) {
		GPUFREQ_LOGE("fail to ioremap MALI");
		return;
	}

	/* 0x13FBF000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_top_config");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource MFG_TOP_CONFIG");
		return;
	}
	g_mfg_top_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_mfg_top_base)) {
		GPUFREQ_LOGE("fail to ioremap MFG_TOP_CONFIG: 0x%llx", res->start);
		return;
	}

	/* 0x13FA0000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_pll");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource MFG_PLL");
		return;
	}
	g_mfg_pll_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_mfg_pll_base)) {
		GPUFREQ_LOGE("fail to ioremap MFG_PLL: 0x%llx", res->start);
		return;
	}

	/* 0x13FA0C00 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfgsc_pll");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource MFGSC_PLL");
		return;
	}
	g_mfgsc_pll_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_mfgsc_pll_base)) {
		GPUFREQ_LOGE("fail to ioremap MFGSC_PLL: 0x%llx", res->start);
		return;
	}

	/* 0x13F90000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_rpc");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource MFG_RPC");
		return;
	}
	g_mfg_rpc_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_mfg_rpc_base)) {
		GPUFREQ_LOGE("fail to ioremap MFG_RPC: 0x%llx", res->start);
		return;
	}

	/* 0x1C001000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "sleep");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource SLEEP");
		return;
	}
	g_sleep = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_sleep)) {
		GPUFREQ_LOGE("fail to ioremap SLEEP: 0x%llx", res->start);
		return;
	}

	/* 0x10000000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "topckgen");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource TOPCKGEN");
		return;
	}
	g_topckgen_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_topckgen_base)) {
		GPUFREQ_LOGE("fail to ioremap TOPCKGEN: 0x%llx", res->start);
		return;
	}

	GPUFREQ_LOGI("[SPM] SPM_SPM2GPUPM_CON: 0x%08x", readl(SPM_SPM2GPUPM_CON));

	GPUFREQ_LOGI("[RPC] MFG_0_19_PWR_STATUS: 0x%08x, MFG_RPC_MFG1_PWR_CON: 0x%08x",
		MFG_0_19_PWR_STATUS,
		readl(MFG_RPC_MFG1_PWR_CON));

	GPUFREQ_LOGI("[TOP] CON0: 0x%08x, CON1: %d, FMETER: %d, SEL: 0x%08x, REF_SEL: 0x%08x",
		readl(MFG_PLL_CON0),
		__gpufreq_get_real_fgpu(),
		__gpufreq_get_fmeter_main_fgpu(),
		readl(TOPCK_CLK_CFG_30) & MFG_INT0_SEL_MASK,
		readl(TOPCK_CLK_CFG_3) & MFG_REF_SEL_MASK);

	GPUFREQ_LOGI("[STK] CON0: 0x%08x, CON1: %d, FMETER: %d, SEL: 0x%08x, REF_SEL: 0x%08x",
		readl(MFGSC_PLL_CON0),
		__gpufreq_get_real_fstack(),
		__gpufreq_get_fmeter_main_fstack(),
		readl(TOPCK_CLK_CFG_30) & MFGSC_INT1_SEL_MASK,
		readl(TOPCK_CLK_CFG_3) & MFGSC_REF_SEL_MASK);

	GPUFREQ_LOGI("[GPU] MALI_GPU_ID: 0x%08x", readl(MALI_GPU_ID));
}

static unsigned int __gpufreq_get_fmeter_freq(enum gpufreq_target target)
{
	u32 mux_src = 0;
	unsigned int freq = 0;

	if (target == TARGET_STACK) {
		/* CLK_CFG_30 0x100001F0 [17] mfg1_int1_sel */
		mux_src = readl(TOPCK_CLK_CFG_30) & MFGSC_INT1_SEL_MASK;

		if (mux_src == MFGSC_INT1_SEL_MASK)
			freq = __gpufreq_get_fmeter_main_fstack();
		else if (mux_src == 0x0)
			freq = __gpufreq_get_fmeter_sub_fstack();
		else
			freq = 0;
	} else {
		/* CLK_CFG_30 0x100001F0 [16] mfg_int0_sel */
		mux_src = readl(TOPCK_CLK_CFG_30) & MFG_INT0_SEL_MASK;

		if (mux_src == MFG_INT0_SEL_MASK)
			freq = __gpufreq_get_fmeter_main_fgpu();
		else if (mux_src == 0x0)
			freq = __gpufreq_get_fmeter_sub_fgpu();
		else
			freq = 0;
	}

	return freq;
}

static unsigned int __gpufreq_get_fmeter_main_fgpu(void)
{
	u32 val = 0, ckgen_load_cnt = 0, ckgen_k1 = 0;
	int i = 0;
	unsigned int freq = 0;

	writel(GENMASK(23, 16), MFG_PLL_FQMTR_CON1);
	val = readl(MFG_PLL_FQMTR_CON0);
	writel((val & GENMASK(23, 0)), MFG_PLL_FQMTR_CON0);
	writel((BIT(12) | BIT(15)), MFG_PLL_FQMTR_CON0);

	ckgen_load_cnt = readl(MFG_PLL_FQMTR_CON1) >> 16;
	ckgen_k1 = readl(MFG_PLL_FQMTR_CON0) >> 24;

	val = readl(MFG_PLL_FQMTR_CON0);
	writel((val | BIT(4) | BIT(12)), MFG_PLL_FQMTR_CON0);

	/* wait fmeter finish */
	while (readl(MFG_PLL_FQMTR_CON0) & BIT(4)) {
		udelay(10);
		i++;
		if (i > 1000) {
			GPUFREQ_LOGE("wait MFGPLL Fmeter timeout");
			break;
		}
	}

	val = readl(MFG_PLL_FQMTR_CON1) & GENMASK(15, 0);
	/* KHz */
	freq = (val * 26000 * (ckgen_k1 + 1)) / (ckgen_load_cnt + 1);

	return freq;
}

static unsigned int __gpufreq_get_fmeter_main_fstack(void)
{
	u32 val = 0, ckgen_load_cnt = 0, ckgen_k1 = 0;
	int i = 0;
	unsigned int freq = 0;

	writel(GENMASK(23, 16), MFGSC_PLL_FQMTR_CON1);
	val = readl(MFGSC_PLL_FQMTR_CON0);
	writel((val & GENMASK(23, 0)), MFGSC_PLL_FQMTR_CON0);
	writel((BIT(12) | BIT(15)), MFGSC_PLL_FQMTR_CON0);

	ckgen_load_cnt = readl(MFGSC_PLL_FQMTR_CON1) >> 16;
	ckgen_k1 = readl(MFGSC_PLL_FQMTR_CON0) >> 24;

	val = readl(MFGSC_PLL_FQMTR_CON0);
	writel((val | BIT(4) | BIT(12)), MFGSC_PLL_FQMTR_CON0);

	/* wait fmeter finish */
	while (readl(MFGSC_PLL_FQMTR_CON0) & BIT(4)) {
		udelay(10);
		i++;
		if (i > 1000) {
			GPUFREQ_LOGE("wait MFGSCPLL Fmeter timeout");
			break;
		}
	}

	val = readl(MFGSC_PLL_FQMTR_CON1) & GENMASK(15, 0);
	/* KHz */
	freq = (val * 26000 * (ckgen_k1 + 1)) / (ckgen_load_cnt + 1);

	return freq;
}

static unsigned int __gpufreq_get_fmeter_sub_fgpu(void)
{
	unsigned int freq = 0;

	/* parking clock use CCF API directly */
	freq = clk_get_rate(g_clk->clk_sub_parent) / 1000; /* Hz */

	return freq;
}

static unsigned int __gpufreq_get_fmeter_sub_fstack(void)
{
	unsigned int freq = 0;

	/* parking clock use CCF API directly */
	freq = clk_get_rate(g_clk->clk_sc_sub_parent) / 1000; /* Hz */

	return freq;
}

/*
 * API: get real current frequency from CON1 (khz)
 * Freq = ((PLL_CON1[21:0] * 26M) / 2^14) / 2^PLL_CON1[26:24]
 */
static unsigned int __gpufreq_get_real_fgpu(void)
{
	u32 con1 = 0;
	unsigned int posdiv = 0;
	unsigned long long freq = 0, pcw = 0;

	con1 = readl(MFG_PLL_CON1);

	pcw = con1 & GENMASK(21, 0);

	posdiv = (con1 & GENMASK(26, 24)) >> POSDIV_SHIFT;

	freq = (((pcw * 1000) * MFGPLL_FIN) >> DDS_SHIFT) / (1 << posdiv);

	return FREQ_ROUNDUP_TO_10((unsigned int)freq);
}

/*
 * API: get real current frequency from CON1 (khz)
 * Freq = ((PLL_CON1[21:0] * 26M) / 2^14) / 2^PLL_CON1[26:24]
 */
static unsigned int __gpufreq_get_real_fstack(void)
{
	u32 con1 = 0;
	unsigned int posdiv = 0;
	unsigned long long freq = 0, pcw = 0;

	con1 = readl(MFGSC_PLL_CON1);

	pcw = con1 & GENMASK(21, 0);

	posdiv = (con1 & GENMASK(26, 24)) >> POSDIV_SHIFT;

	freq = (((pcw * 1000) * MFGPLL_FIN) >> DDS_SHIFT) / (1 << posdiv);

	return FREQ_ROUNDUP_TO_10((unsigned int)freq);
}

/* API: get real current Vgpu from regulator (mV * 100) */
static unsigned int __gpufreq_get_real_vgpu(void)
{
	unsigned int volt = 0;

	if (regulator_is_enabled(g_pmic->reg_vgpu))
		/* regulator_get_voltage return volt with uV */
		volt = regulator_get_voltage(g_pmic->reg_vgpu) / 10;

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

static unsigned int __gpufreq_get_vsram_by_vlogic(unsigned int volt)
{
	unsigned int vsram = 0;

	if (volt <= VSRAM_THRESH)
		vsram = VSRAM_THRESH;
	else
		vsram = volt;

	return vsram;
}

/* API: use SPM HW semaphore to protect reading MFG_TOP_CFG */
static void __gpufreq_set_semaphore(enum gpufreq_sema_op op)
{
	int i = 0;

	/* acquire HW semaphore: SPM_SEMA_M3 0x1C0016A8 [3] = 1'b1 */
	if (op == SEMA_ACQUIRE) {
		do {
			/* 50ms timeout */
			if (unlikely(++i > 5000))
				goto fail;
			writel(BIT(3), SPM_SEMA_M3);
			udelay(10);
		} while ((readl(SPM_SEMA_M3) & BIT(3)) != BIT(3));
	/* signal HW semaphore: SPM_SEMA_M3 0x1C0016A8 [3] = 1'b1 */
	} else if (op == SEMA_RELEASE) {
		writel(BIT(3), SPM_SEMA_M3);
		udelay(10);
		if (readl(SPM_SEMA_M3) & BIT(3))
			goto fail;
	}

	return;

fail:
	__gpufreq_abort("fail to %s SPM_SEMA_M3, M3=(0x%08x), M4=(0x%08x)",
		op == SEMA_ACQUIRE ? "acquire" : "release",
		readl(SPM_SEMA_M3), readl(SPM_SEMA_M4));
}

/* AOC2.0: set AOC ISO/LATCH before SRAM power off to prevent leakage and SRAM shutdown */
static void __gpufreq_aoc_config(enum gpufreq_power_state power)
{
	/* power on: clear AOCISO -> clear AOCLHENB */
	if (power == POWER_ON) {
		/* SPM_SOC_BUCK_ISO_CON_CLR 0x1C001F80 [9] AOC_VGPU_SRAM_ISO_DIN */
		writel(BIT(9), SPM_SOC_BUCK_ISO_CON_CLR);
		/* SPM_SOC_BUCK_ISO_CON_CLR 0x1C001F80 [10] AOC_VGPU_SRAM_LATCH_ENB */
		writel(BIT(10), SPM_SOC_BUCK_ISO_CON_CLR);
		/* SPM_SOC_BUCK_ISO_CON_CLR 0x1C001F80 [17] AOC_VSTACK_SRAM_ISO_DIN */
		writel(BIT(17), SPM_SOC_BUCK_ISO_CON_CLR);
		/* SPM_SOC_BUCK_ISO_CON_CLR 0x1C001F80 [18] AOC_VSTACK_SRAM_LATCH_ENB */
		writel(BIT(18), SPM_SOC_BUCK_ISO_CON_CLR);
	/* power off: set AOCLHENB -> set AOCISO */
	} else {
		/* SPM_SOC_BUCK_ISO_CON_SET 0x1C001F7C [18] AOC_VSTACK_SRAM_LATCH_ENB */
		writel(BIT(18), SPM_SOC_BUCK_ISO_CON_SET);
		/* SPM_SOC_BUCK_ISO_CON_SET 0x1C001F7C [17] AOC_VSTACK_SRAM_ISO_DIN */
		writel(BIT(17), SPM_SOC_BUCK_ISO_CON_SET);
		/* SPM_SOC_BUCK_ISO_CON_SET 0x1C001F7C [10] AOC_VGPU_SRAM_LATCH_ENB */
		writel(BIT(10), SPM_SOC_BUCK_ISO_CON_SET);
		/* SPM_SOC_BUCK_ISO_CON_SET 0x1C001F7C [9] AOC_VGPU_SRAM_ISO_DIN */
		writel(BIT(9), SPM_SOC_BUCK_ISO_CON_SET);
	}

	GPUFREQ_LOGD("power: %d, SPM_SOC_BUCK_ISO_CON: 0x%08x",
		power, readl(SPM_SOC_BUCK_ISO_CON));
}

/* HWDCM: mask clock when GPU idle (dynamic clock mask) */
static void __gpufreq_hwdcm_config(void)
{
#if GPUFREQ_HWDCM_ENABLE
	u32 val = 0;

	/* acquire sema before access MFG_TOP_CFG */
	__gpufreq_set_semaphore(SEMA_ACQUIRE);

	/* MFG_DCM_CON_0 0x13FBF010 [6:0] BG3D_DBC_CNT = 7'b0111111 */
	/* MFG_DCM_CON_0 0x13FBF010 [15]  BG3D_DCM_EN = 1'b1 */
	val = (readl_mfg(MFG_DCM_CON_0) & ~BIT(6)) | GENMASK(5, 0) | BIT(15);
	writel(val, MFG_DCM_CON_0);

	/* MFG_ASYNC_CON 0x13FBF020 [23] MEM0_SLV_CG_ENABLE = 1'b1 */
	/* MFG_ASYNC_CON 0x13FBF020 [25] MEM1_SLV_CG_ENABLE = 1'b1 */
	val = readl_mfg(MFG_ASYNC_CON) | BIT(23) | BIT(25);
	writel(val, MFG_ASYNC_CON);

	/* MFG_GLOBAL_CON 0x13FBF0B0 [8]  GPU_SOCIF_MST_FREE_RUN = 1'b0 */
	/* MFG_GLOBAL_CON 0x13FBF0B0 [10] GPU_CLK_FREE_RUN = 1'b0 */
	/* MFG_GLOBAL_CON 0x13FBF0B0 [21] dvfs_hint_cg_en = 1'b0 */
	/* MFG_GLOBAL_CON 0x13FBF0B0 [24] stack_hd_bg3d_cg_free_run = 1'b0 */
	val = readl_mfg(MFG_GLOBAL_CON) & ~BIT(8) & ~BIT(10) & ~BIT(21) & ~BIT(24);
	writel(val, MFG_GLOBAL_CON);

	/* MFG_RPC_AO_CLK_CFG 0x13F91034 [0] CG_FAXI_CK_SOC_IN_FREE_RUN = 1'b0 */
	writel((readl(MFG_RPC_AO_CLK_CFG) & ~BIT(0)), MFG_RPC_AO_CLK_CFG);

	__gpufreq_set_semaphore(SEMA_RELEASE);
#endif /* GPUFREQ_HWDCM_ENABLE */
}

/* ACP: GPU can access CPU cache directly */
static void __gpufreq_acp_config(void)
{
#if GPUFREQ_ACP_ENABLE
	/* acquire sema before access MFG_TOP_CFG */
	__gpufreq_set_semaphore(SEMA_ACQUIRE);

	/* MFG_1TO2AXI_CON_00 0x13FBF8E0 [24:0] mfg_axi1to2_R_dispatch_mode = 0x855 */
	writel(0x00FFC855, MFG_1TO2AXI_CON_00);
	/* MFG_1TO2AXI_CON_02 0x13FBF8E8 [24:0] mfg_axi1to2_R_dispatch_mode = 0x855 */
	writel(0x00FFC855, MFG_1TO2AXI_CON_02);
	/* MFG_1TO2AXI_CON_04 0x13FBF910 [24:0] mfg_axi1to2_R_dispatch_mode = 0x855 */
	writel(0x00FFC855, MFG_1TO2AXI_CON_04);
	/* MFG_1TO2AXI_CON_06 0x13FBF918 [24:0] mfg_axi1to2_R_dispatch_mode = 0x855 */
	writel(0x00FFC855, MFG_1TO2AXI_CON_06);
	/* MFG_OUT_1TO2AXI_CON_00 0x13FBF900 [24:0] mfg_axi1to2_R_dispatch_mode = 0x055 */
	writel(0x00FFC055, MFG_OUT_1TO2AXI_CON_00);
	/* MFG_OUT_1TO2AXI_CON_02 0x13FBF908 [24:0] mfg_axi1to2_R_dispatch_mode = 0x055 */
	writel(0x00FFC055, MFG_OUT_1TO2AXI_CON_02);
	/* MFG_OUT_1TO2AXI_CON_04 0x13FBF920 [24:0] mfg_axi1to2_R_dispatch_mode = 0x055 */
	writel(0x00FFC055, MFG_OUT_1TO2AXI_CON_04);
	/* MFG_OUT_1TO2AXI_CON_06 0x13FBF928 [24:0] mfg_axi1to2_R_dispatch_mode = 0x055 */
	writel(0x00FFC055, MFG_OUT_1TO2AXI_CON_06);

	/* MFG_AXCOHERENCE_CON 0x13FBF168 [0] M0_coherence_enable = 1'b1 */
	/* MFG_AXCOHERENCE_CON 0x13FBF168 [1] M1_coherence_enable = 1'b1 */
	/* MFG_AXCOHERENCE_CON 0x13FBF168 [2] M2_coherence_enable = 1'b1 */
	/* MFG_AXCOHERENCE_CON 0x13FBF168 [3] M3_coherence_enable = 1'b1 */
	writel((readl_mfg(MFG_AXCOHERENCE_CON) | GENMASK(3, 0)), MFG_AXCOHERENCE_CON);

	/* MFG_SECURE_REG 0x13FBCFE0 [30] acp_mpu_enable = 1'b1 */
	/* MFG_SECURE_REG 0x13FBCFE0 [31] acp_mpu_rule3_disable = 1'b1 */
	writel((readl_mfg(MFG_SECURE_REG) | GENMASK(31, 30)), MFG_SECURE_REG);

	/* NTH_APU_ACP_GALS_SLV_CTRL  0x1021C600 [27:25] MFG_ACP_AR_MPAM_2_1_0 = 3'b111 */
	/* NTH_APU_EMI1_GALS_SLV_CTRL 0x1021C624 [27:25] MFG_ACP_AW_MPAM_2_1_0 = 3'b111 */
	writel((readl(NTH_APU_ACP_GALS_SLV_CTRL) | GENMASK(27, 25)), NTH_APU_ACP_GALS_SLV_CTRL);
	writel((readl(NTH_APU_EMI1_GALS_SLV_CTRL) | GENMASK(27, 25)), NTH_APU_EMI1_GALS_SLV_CTRL);

	__gpufreq_set_semaphore(SEMA_RELEASE);
#endif /* GPUFREQ_ACP_ENABLE */
}

/* GPM1.0: di/dt reduction by slowing down speed of frequency scaling up or down */
static void __gpufreq_gpm1_config(void)
{
#if GPUFREQ_GPM1_ENABLE
	/* acquire sema before access MFG_TOP_CFG */
	__gpufreq_set_semaphore(SEMA_ACQUIRE);

	if (g_gpm1_mode) {
		/* MFG_I2M_PROTECTOR_CFG_00 0x13FBFF60 = 0x20300316 */
		writel(0x20300316, MFG_I2M_PROTECTOR_CFG_00);

		/* MFG_I2M_PROTECTOR_CFG_01 0x13FBFF64 = 0x1800000C */
		writel(0x1800000C, MFG_I2M_PROTECTOR_CFG_01);

		/* MFG_I2M_PROTECTOR_CFG_02 0x13FBFF68 = 0x01010802 */
		writel(0x01010802, MFG_I2M_PROTECTOR_CFG_02);

		/* MFG_I2M_PROTECTOR_CFG_03 0x13FBFFA8 = 0x00030FF3 */
		writel(0x00030FF3, MFG_I2M_PROTECTOR_CFG_03);

		/* wait 1us */
		udelay(1);

		/* MFG_I2M_PROTECTOR_CFG_00 0x13FBFF60 = 0x20300317 */
		writel(0x20300317, MFG_I2M_PROTECTOR_CFG_00);
	}

	__gpufreq_set_semaphore(SEMA_RELEASE);
#endif /* GPUFREQ_GPM1_ENABLE */
}

/* Merge GPU transaction to maximize DRAM efficiency */
static void __gpufreq_transaction_config(void)
{
#if GPUFREQ_MERGER_ENABLE
	/* acquire sema before access MFG_TOP_CFG */
	__gpufreq_set_semaphore(SEMA_ACQUIRE);

	/* Merge AXI READ to window size 8T */
	writel(0x0808FF81, MFG_MERGE_R_CON_00);
	writel(0x0808FF81, MFG_MERGE_R_CON_02);
	writel(0x0808FF81, MFG_MERGE_R_CON_04);
	writel(0x0808FF81, MFG_MERGE_R_CON_06);

	/* Merge AXI WRITE to window size 64T */
	writel(0x4040FF81, MFG_MERGE_W_CON_00);
	writel(0x4040FF81, MFG_MERGE_W_CON_02);
	writel(0x4040FF81, MFG_MERGE_W_CON_04);
	writel(0x4040FF81, MFG_MERGE_W_CON_06);

	__gpufreq_set_semaphore(SEMA_RELEASE);
#endif /* GPUFREQ_MERGER_ENABLE */
}

static void __gpufreq_dfd_config(void)
{
#if GPUFREQ_DFD_ENABLE
	/* acquire sema before access MFG_TOP_CFG */
	__gpufreq_set_semaphore(SEMA_ACQUIRE);

	if (g_dfd_mode) {
		if (g_dfd_mode == DFD_FORCE_DUMP)
			writel(MFG_DEBUGMON_CON_00_ENABLE, MFG_DEBUGMON_CON_00);

		writel(MFG_DFD_CON_0_ENABLE, MFG_DFD_CON_0);
		writel(MFG_DFD_CON_1_ENABLE, MFG_DFD_CON_1);
		writel(MFG_DFD_CON_2_ENABLE, MFG_DFD_CON_2);
		writel(MFG_DFD_CON_3_ENABLE, MFG_DFD_CON_3);
		writel(MFG_DFD_CON_4_ENABLE, MFG_DFD_CON_4);
		writel(MFG_DFD_CON_5_ENABLE, MFG_DFD_CON_5);
		writel(MFG_DFD_CON_6_ENABLE, MFG_DFD_CON_6);
		writel(MFG_DFD_CON_7_ENABLE, MFG_DFD_CON_7);
		writel(MFG_DFD_CON_8_ENABLE, MFG_DFD_CON_8);
		writel(MFG_DFD_CON_9_ENABLE, MFG_DFD_CON_9);
		writel(MFG_DFD_CON_10_ENABLE, MFG_DFD_CON_10);
		writel(MFG_DFD_CON_11_ENABLE, MFG_DFD_CON_11);

		if ((readl(DRM_DEBUG_MFG_REG) & BIT(0)) != BIT(0)) {
			writel(0x77000000, DRM_DEBUG_MFG_REG);
			udelay(10);
			writel(0x77000001, DRM_DEBUG_MFG_REG);
		}
	}

	__gpufreq_set_semaphore(SEMA_RELEASE);
#endif /* GPUFREQ_DFD_ENABLE */
}

static void __gpufreq_mfg_backup_restore(enum gpufreq_power_state power)
{
	/* acquire sema before access MFG_TOP_CFG */
	__gpufreq_set_semaphore(SEMA_ACQUIRE);

	/* restore */
	if (power == POWER_ON) {
		if (g_stack_sel_reg)
			/* MFG_DUMMY_REG 0x13FBF500 [31] */
			writel((readl_mfg(MFG_DUMMY_REG) | BIT(31)), MFG_DUMMY_REG);
		if (g_del_sel_reg)
			/* MFG_SRAM_FUL_SEL_ULV 0x13FBF080 [0] */
			writel((readl_mfg(MFG_SRAM_FUL_SEL_ULV) | BIT(0)), MFG_SRAM_FUL_SEL_ULV);
	/* backup */
	} else {
		/* MFG_DUMMY_REG 0x13FBF500 [31] */
		g_stack_sel_reg = readl_mfg(MFG_DUMMY_REG) & BIT(31);
		/* MFG_SRAM_FUL_SEL_ULV 0x13FBF080 [0] */
		g_del_sel_reg = readl_mfg(MFG_SRAM_FUL_SEL_ULV) & BIT(0);
	}

	__gpufreq_set_semaphore(SEMA_RELEASE);
}

static int __gpufreq_clock_control(enum gpufreq_power_state power)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("power=%d", power);

	if (power == POWER_ON) {
		/* enable GPU MUX and GPU PLL */
		ret = clk_prepare_enable(g_clk->clk_mux);
		if (unlikely(ret)) {
			__gpufreq_abort("fail to enable clk_mux (%d)", ret);
			goto done;
		}
		/* switch GPU MUX to PLL */
		ret = __gpufreq_switch_clksrc(TARGET_GPU, CLOCK_MAIN);
		if (unlikely(ret))
			goto done;
		g_gpu.cg_count++;
		g_gpu.active_count++;

		/* enable STACK MUX and STACK MUX */
		ret = clk_prepare_enable(g_clk->clk_sc_mux);
		if (unlikely(ret)) {
			__gpufreq_abort("fail to enable clk_sc_mux (%d)", ret);
			goto done;
		}
		/* switch STACK MUX to PLL */
		ret = __gpufreq_switch_clksrc(TARGET_STACK, CLOCK_MAIN);
		if (unlikely(ret))
			goto done;
		g_stack.cg_count++;
		g_stack.active_count++;
	} else {
		/* switch STACK MUX to REF_SEL */
		ret = __gpufreq_switch_clksrc(TARGET_STACK, CLOCK_SUB);
		if (unlikely(ret))
			goto done;
		/* disable STACK MUX and STACK PLL */
		clk_disable_unprepare(g_clk->clk_sc_mux);
		g_stack.active_count--;
		g_stack.cg_count--;

		/* switch GPU MUX to REF_SEL */
		ret = __gpufreq_switch_clksrc(TARGET_GPU, CLOCK_SUB);
		if (unlikely(ret))
			goto done;
		/* disable GPU MUX and GPU PLL */
		clk_disable_unprepare(g_clk->clk_mux);
		g_gpu.active_count--;
		g_gpu.cg_count--;
	}

done:
	GPUFREQ_TRACE_END();

	return ret;
}

#if GPUFREQ_SELF_CTRL_MTCMOS
static void __gpufreq_mfgx_rpc_control(enum gpufreq_power_state power, void __iomem *pwr_con)
{
	int i = 0;

	if (power == POWER_ON) {
		/* MFGx_PWR_ON = 1'b1 */
		writel((readl(pwr_con) | BIT(2)), pwr_con);
		/* MFGx_PWR_ACK = 1'b1 */
		i = 0;
		while ((readl(pwr_con) & BIT(30)) != BIT(30)) {
			udelay(10);
			if (++i > 10) {
				__gpufreq_footprint_power_step(0xF0);
				break;
			}
		}
		/* MFGx_PWR_ON_2ND = 1'b1 */
		writel((readl(pwr_con) | BIT(3)), pwr_con);
		/* MFGx_PWR_ACK_2ND = 1'b1 */
		i = 0;
		while ((readl(pwr_con) & BIT(31)) != BIT(31)) {
			udelay(10);
			if (++i > 10) {
				__gpufreq_footprint_power_step(0xF1);
				break;
			}
		}
		/* MFGx_PWR_ACK = 1'b1 */
		/* MFGx_PWR_ACK_2ND = 1'b1 */
		i = 0;
		while ((readl(pwr_con) & GENMASK(31, 30)) != GENMASK(31, 30)) {
			udelay(10);
			if (++i > 1000) {
				__gpufreq_footprint_power_step(0xF2);
				goto timeout;
			}
		}
		/* MFGx_PWR_CLK_DIS = 1'b0 */
		writel((readl(pwr_con) & ~BIT(4)), pwr_con);
		/* MFGx_PWR_ISO = 1'b0 */
		writel((readl(pwr_con) & ~BIT(1)), pwr_con);
		/* MFGx_PWR_RST_B = 1'b1 */
		writel((readl(pwr_con) | BIT(0)), pwr_con);
		/* MFGx_PWR_SRAM_PDN = 1'b0 */
		writel((readl(pwr_con) & ~BIT(8)), pwr_con);
		/* MFGx_PWR_SRAM_PDN_ACK = 1'b0 */
		i = 0;
		while (readl(pwr_con) & BIT(12)) {
			udelay(10);
			if (++i > 1000) {
				__gpufreq_footprint_power_step(0xF3);
				goto timeout;
			}
		}
	} else {
		/* MFGx_PWR_SRAM_PDN = 1'b1 */
		writel((readl(pwr_con) | BIT(8)), pwr_con);
		/* MFGx_PWR_SRAM_PDN_ACK = 1'b1 */
		i = 0;
		while ((readl(pwr_con) & BIT(12)) != BIT(12)) {
			udelay(10);
			if (++i > 1000) {
				__gpufreq_footprint_power_step(0xF8);
				goto timeout;
			}
		}
		/* MFGx_PWR_ISO = 1'b1 */
		writel((readl(pwr_con) | BIT(1)), pwr_con);
		/* MFGx_PWR_CLK_DIS = 1'b1 */
		writel((readl(pwr_con) | BIT(4)), pwr_con);
		/* MFGx_PWR_RST_B = 1'b0 */
		writel((readl(pwr_con) & ~BIT(0)), pwr_con);
		/* MFGx_PWR_ON = 1'b0 */
		writel((readl(pwr_con) & ~BIT(2)), pwr_con);
		/* MFGx_PWR_ON_2ND = 1'b0 */
		writel((readl(pwr_con) & ~BIT(3)), pwr_con);
		/* MFGx_PWR_ACK = 1'b0 */
		/* MFGx_PWR_ACK_2ND = 1'b0 */
		i = 0;
		while (readl(pwr_con) & GENMASK(31, 30)) {
			udelay(10);
			if (++i > 1000) {
				__gpufreq_footprint_power_step(0xF9);
				goto timeout;
			}
		}
	}

	return;

timeout:
	GPUFREQ_LOGE("(0x13F91070)=0x%x, (0x13F910A0)=0x%x, (0x13F910A4)=0x%x, (0x13F910A8)=0x%x",
		readl(MFG_RPC_MFG1_PWR_CON), readl(MFG_RPC_MFG2_PWR_CON),
		readl(MFG_RPC_MFG3_PWR_CON), readl(MFG_RPC_MFG4_PWR_CON));
	GPUFREQ_LOGE("(0x13F910AC)=0x%x, (0x13F910B0)=0x%x, (0x13F910B4)=0x%x, (0x13F910B8)=0x%x",
		readl(MFG_RPC_MFG5_PWR_CON), readl(MFG_RPC_MFG6_PWR_CON),
		readl(MFG_RPC_MFG7_PWR_CON), readl(MFG_RPC_MFG8_PWR_CON));
	GPUFREQ_LOGE("(0x13F910BC)=0x%x, (0x13F910C0)=0x%x, (0x13F910C4)=0x%x, (0x13F910C8)=0x%x",
		readl(MFG_RPC_MFG9_PWR_CON), readl(MFG_RPC_MFG10_PWR_CON),
		readl(MFG_RPC_MFG11_PWR_CON), readl(MFG_RPC_MFG12_PWR_CON));
	GPUFREQ_LOGE("(0x13F910CC)=0x%x, (0x13F910D0)=0x%x, (0x13F910D4)=0x%x, (0x13F910D8)=0x%x",
		readl(MFG_RPC_MFG13_PWR_CON), readl(MFG_RPC_MFG14_PWR_CON),
		readl(MFG_RPC_MFG15_PWR_CON), readl(MFG_RPC_MFG16_PWR_CON));
	GPUFREQ_LOGE("(0x13F910DC)=0x%x, (0x13F910E0)=0x%x, (0x13F910E4)=0x%x",
		readl(MFG_RPC_MFG17_PWR_CON), readl(MFG_RPC_MFG18_PWR_CON),
		readl(MFG_RPC_MFG19_PWR_CON));
	__gpufreq_abort("timeout");
}

static void __gpufreq_mfg1_rpc_control(enum gpufreq_power_state power)
{
	int i = 0;

	if (power == POWER_ON) {
		/* MFG1_PWR_CON 0x13F91070 [2] MFG1_PWR_ON = 1'b1 */
		writel((readl(MFG_RPC_MFG1_PWR_CON) | BIT(2)), MFG_RPC_MFG1_PWR_CON);
		/* MFG1_PWR_CON 0x13F91070 [30] MFG1_PWR_ACK = 1'b1 */
		i = 0;
		while ((readl(MFG_RPC_MFG1_PWR_CON) & BIT(30)) != BIT(30)) {
			udelay(10);
			if (++i > 10) {
				__gpufreq_footprint_power_step(0xF0);
				break;
			}
		}
		/* MFG1_PWR_CON 0x13F91070 [3] MFG1_PWR_ON_2ND = 1'b1 */
		writel((readl(MFG_RPC_MFG1_PWR_CON) | BIT(3)), MFG_RPC_MFG1_PWR_CON);
		/* MFG1_PWR_CON 0x13F91070 [31] MFG1_PWR_ACK_2ND = 1'b1 */
		i = 0;
		while ((readl(MFG_RPC_MFG1_PWR_CON) & BIT(31)) != BIT(31)) {
			udelay(10);
			if (++i > 10) {
				__gpufreq_footprint_power_step(0xF1);
				break;
			}
		}
		/* MFG1_PWR_CON 0x13F91070 [30] MFG1_PWR_ACK = 1'b1 */
		/* MFG1_PWR_CON 0x13F91070 [31] MFG1_PWR_ACK_2ND = 1'b1 */
		i = 0;
		while ((readl(MFG_RPC_MFG1_PWR_CON) & GENMASK(31, 30)) != GENMASK(31, 30)) {
			udelay(10);
			if (++i > 1000) {
				__gpufreq_footprint_power_step(0xF2);
				goto timeout;
			}
		}
		/* MFG1_PWR_CON 0x13F91070 [4] MFG1_PWR_CLK_DIS = 1'b0 */
		writel((readl(MFG_RPC_MFG1_PWR_CON) & ~BIT(4)), MFG_RPC_MFG1_PWR_CON);
		/* MFG1_PWR_CON 0x13F91070 [1] MFG1_PWR_ISO = 1'b0 */
		writel((readl(MFG_RPC_MFG1_PWR_CON) & ~BIT(1)), MFG_RPC_MFG1_PWR_CON);
		/* MFG1_PWR_CON 0x13F91070 [0] MFG1_PWR_RST_B = 1'b1 */
		writel((readl(MFG_RPC_MFG1_PWR_CON) | BIT(0)), MFG_RPC_MFG1_PWR_CON);
		/* MFG1_PWR_CON 0x13F91070 [8] MFG1_PWR_SRAM_PDN = 1'b0 */
		writel((readl(MFG_RPC_MFG1_PWR_CON) & ~BIT(8)), MFG_RPC_MFG1_PWR_CON);
		/* MFG1_PWR_CON 0x13F91070 [12] MFG1_PWR_SRAM_PDN_ACK = 1'b0 */
		i = 0;
		while (readl(MFG_RPC_MFG1_PWR_CON) & BIT(12)) {
			udelay(10);
			if (++i > 1000) {
				__gpufreq_footprint_power_step(0xF3);
				goto timeout;
			}
		}
		/* IFR_EMISYS_PROTECT_EN_W1C_0 0x1002C108 [20:19] = 2'b11 */
		writel(GENMASK(20, 19), IFR_EMISYS_PROTECT_EN_W1C_0);
		/* IFR_EMISYS_PROTECT_EN_W1C_1 0x1002C128 [20:19] = 2'b11 */
		writel(GENMASK(20, 19), IFR_EMISYS_PROTECT_EN_W1C_1);
		/* MFG_RPC_SLP_PROT_EN_CLR 0x13F91044 [19:16] = 4'b1111 */
		writel(GENMASK(19, 16), MFG_RPC_SLP_PROT_EN_CLR);
		/* IFR_MFGSYS_PROT_EN_W1C_0 0x1002C1A8 [3:0] = 4'b1111 */
		writel(GENMASK(3, 0), IFR_MFGSYS_PROT_EN_W1C_0);
	} else {
		/* IFR_MFGSYS_PROT_EN_W1S_0 0x1002C1A8 [3:0] = 4'b1111 */
		writel(GENMASK(3, 0), IFR_MFGSYS_PROT_EN_W1S_0);
		/* IFR_MFGSYS_PROT_RDY_STA_0 0x1002C1AC [3:0] = 4'b1111 */
		i = 0;
		while ((readl(IFR_MFGSYS_PROT_RDY_STA_0) & GENMASK(3, 0)) != GENMASK(3, 0)) {
			udelay(10);
			if (++i > 1000) {
				__gpufreq_footprint_power_step(0xF4);
				goto timeout;
			}
		}
		/* MFG_RPC_SLP_PROT_EN_SET 0x13F91040 [19:16] = 4'b1111 */
		writel(GENMASK(19, 16), MFG_RPC_SLP_PROT_EN_SET);
		/* MFG_RPC_SLP_PROT_EN_STA 0x13F91048 [19:16] = 4'b1111 */
		i = 0;
		while ((readl(MFG_RPC_SLP_PROT_EN_STA) & GENMASK(19, 16)) != GENMASK(19, 16)) {
			udelay(10);
			if (++i > 1000) {
				__gpufreq_footprint_power_step(0xF5);
				goto timeout;
			}
		}
		/* IFR_EMISYS_PROTECT_EN_W1S_0 0x1002C104 [20:19] = 2'b11 */
		writel(GENMASK(20, 19), IFR_EMISYS_PROTECT_EN_W1S_0);
		/* IFR_EMISYS_PROTECT_EN_W1S_1 0x1002C124 [20:19] = 2'b11 */
		writel(GENMASK(20, 19), IFR_EMISYS_PROTECT_EN_W1S_1);
		/* IFR_EMISYS_PROTECT_RDY_STA_0 0x1002C10C [20:19] = 2'b11 */
		i = 0;
		while ((readl(IFR_EMISYS_PROTECT_RDY_STA_0) & GENMASK(20, 19)) != GENMASK(20, 19)) {
			udelay(10);
			if (++i > 1000) {
				__gpufreq_footprint_power_step(0xF6);
				goto timeout;
			}
		}
		/* IFR_EMISYS_PROTECT_RDY_STA_1 0x1002C12C [20:19] = 2'b11 */
		i = 0;
		while ((readl(IFR_EMISYS_PROTECT_RDY_STA_1) & GENMASK(20, 19)) != GENMASK(20, 19)) {
			udelay(10);
			if (++i > 1000) {
				__gpufreq_footprint_power_step(0xF7);
				goto timeout;
			}
		}
		/* MFG1_PWR_CON 0x13F91070 [8] MFG1_PWR_SRAM_PDN = 1'b1 */
		writel((readl(MFG_RPC_MFG1_PWR_CON) | BIT(8)), MFG_RPC_MFG1_PWR_CON);
		/* MFG1_PWR_CON 0x13F91070 [12] MFG1_PWR_SRAM_PDN_ACK = 1'b1 */
		i = 0;
		while ((readl(MFG_RPC_MFG1_PWR_CON) & BIT(12)) != BIT(12)) {
			udelay(10);
			if (++i > 1000) {
				__gpufreq_footprint_power_step(0xF8);
				goto timeout;
			}
		}
		/* MFG1_PWR_CON 0x13F91070 [1] MFG1_PWR_ISO = 1'b1 */
		writel((readl(MFG_RPC_MFG1_PWR_CON) | BIT(1)), MFG_RPC_MFG1_PWR_CON);
		/* MFG1_PWR_CON 0x13F91070 [4] MFG1_PWR_CLK_DIS = 1'b1 */
		writel((readl(MFG_RPC_MFG1_PWR_CON) | BIT(4)), MFG_RPC_MFG1_PWR_CON);
		/* MFG1_PWR_CON 0x13F91070 [0] MFG1_PWR_RST_B = 1'b0 */
		writel((readl(MFG_RPC_MFG1_PWR_CON) & ~BIT(0)), MFG_RPC_MFG1_PWR_CON);
		/* MFG1_PWR_CON 0x13F91070 [2] MFG1_PWR_ON = 1'b0 */
		writel((readl(MFG_RPC_MFG1_PWR_CON) & ~BIT(2)), MFG_RPC_MFG1_PWR_CON);
		/* MFG1_PWR_CON 0x13F91070 [3] MFG1_PWR_ON_2ND = 1'b0 */
		writel((readl(MFG_RPC_MFG1_PWR_CON) & ~BIT(3)), MFG_RPC_MFG1_PWR_CON);
		/* MFG1_PWR_CON 0x13F91070 [30] MFG1_PWR_ACK = 1'b0 */
		/* MFG1_PWR_CON 0x13F91070 [31] MFG1_PWR_ACK_2ND = 1'b0 */
		i = 0;
		while (readl(MFG_RPC_MFG1_PWR_CON) & GENMASK(31, 30)) {
			udelay(10);
			if (++i > 1000) {
				__gpufreq_footprint_power_step(0xF9);
				goto timeout;
			}
		}
	}

	return;

timeout:
	GPUFREQ_LOGE("(0x13F91070)=0x%x, (0x13F91048)=0x%x, (0x1002C100)=0x%x, (0x1002C120)=0x%x",
		readl(MFG_RPC_MFG1_PWR_CON), readl(MFG_RPC_SLP_PROT_EN_STA),
		readl(IFR_EMISYS_PROTECT_EN_STA_0), readl(IFR_EMISYS_PROTECT_EN_STA_1));
	GPUFREQ_LOGE("(0x1021C82C)=0x%x, (0x1021C830)=0x%x, (0x1021E82C)=0x%x, (0x1021E830)=0x%x",
		readl(NTH_MFG_EMI1_GALS_SLV_DBG), readl(NTH_MFG_EMI0_GALS_SLV_DBG),
		readl(STH_MFG_EMI1_GALS_SLV_DBG), readl(STH_MFG_EMI0_GALS_SLV_DBG));
	__gpufreq_abort("timeout");
}
#endif /* GPUFREQ_SELF_CTRL_MTCMOS */

static int __gpufreq_mtcmos_control(enum gpufreq_power_state power)
{
	int ret = GPUFREQ_SUCCESS;
	u32 val = 0;

	GPUFREQ_TRACE_START("power=%d", power);

	if (power == POWER_ON) {
#if GPUFREQ_SELF_CTRL_MTCMOS
		__gpufreq_mfg1_rpc_control(POWER_ON);
#else
		/* MFG1 on by CCF */
		ret = pm_runtime_get_sync(g_mtcmos->mfg1_dev);
		if (unlikely(ret < 0)) {
			__gpufreq_abort("fail to enable mfg1_dev (%d)", ret);
			goto done;
		}
#endif /* GPUFREQ_SELF_CTRL_MTCMOS */
		g_gpu.mtcmos_count++;
		g_stack.mtcmos_count++;

#if GPUFREQ_CHECK_MFG_PWR_STATUS
		val = MFG_0_1_PWR_STATUS & MFG_0_1_PWR_MASK;
		if (unlikely(val != MFG_0_1_PWR_MASK)) {
			__gpufreq_abort("incorrect MFG0-1 power on status: 0x%08x", val);
			ret = GPUFREQ_EINVAL;
			goto done;
		}
#endif /* GPUFREQ_CHECK_MFG_PWR_STATUS */

#if !GPUFREQ_PDCA_ENABLE
#if GPUFREQ_SELF_CTRL_MTCMOS
		__gpufreq_mfgx_rpc_control(POWER_ON, MFG_RPC_MFG2_PWR_CON);
		__gpufreq_mfgx_rpc_control(POWER_ON, MFG_RPC_MFG3_PWR_CON);
		__gpufreq_mfgx_rpc_control(POWER_ON, MFG_RPC_MFG4_PWR_CON);
		__gpufreq_mfgx_rpc_control(POWER_ON, MFG_RPC_MFG5_PWR_CON);
		__gpufreq_mfgx_rpc_control(POWER_ON, MFG_RPC_MFG6_PWR_CON);
		__gpufreq_mfgx_rpc_control(POWER_ON, MFG_RPC_MFG7_PWR_CON);
		__gpufreq_mfgx_rpc_control(POWER_ON, MFG_RPC_MFG8_PWR_CON);
		__gpufreq_mfgx_rpc_control(POWER_ON, MFG_RPC_MFG9_PWR_CON);
		__gpufreq_mfgx_rpc_control(POWER_ON, MFG_RPC_MFG10_PWR_CON);
		__gpufreq_mfgx_rpc_control(POWER_ON, MFG_RPC_MFG11_PWR_CON);
		__gpufreq_mfgx_rpc_control(POWER_ON, MFG_RPC_MFG12_PWR_CON);
		__gpufreq_mfgx_rpc_control(POWER_ON, MFG_RPC_MFG13_PWR_CON);
		__gpufreq_mfgx_rpc_control(POWER_ON, MFG_RPC_MFG14_PWR_CON);
		__gpufreq_mfgx_rpc_control(POWER_ON, MFG_RPC_MFG15_PWR_CON);
		__gpufreq_mfgx_rpc_control(POWER_ON, MFG_RPC_MFG16_PWR_CON);
		__gpufreq_mfgx_rpc_control(POWER_ON, MFG_RPC_MFG17_PWR_CON);
		__gpufreq_mfgx_rpc_control(POWER_ON, MFG_RPC_MFG18_PWR_CON);
		__gpufreq_mfgx_rpc_control(POWER_ON, MFG_RPC_MFG19_PWR_CON);
#else
		/* manually control MFG2-19 if PDCA is disabled */
		ret = pm_runtime_get_sync(g_mtcmos->mfg2_dev);
		if (unlikely(ret < 0)) {
			__gpufreq_abort("fail to enable mfg2_dev (%d)", ret);
			goto done;
		}
		ret = pm_runtime_get_sync(g_mtcmos->mfg3_dev);
		if (unlikely(ret < 0)) {
			__gpufreq_abort("fail to enable mfg3_dev (%d)", ret);
			goto done;
		}
		ret = pm_runtime_get_sync(g_mtcmos->mfg4_dev);
		if (unlikely(ret < 0)) {
			__gpufreq_abort("fail to enable mfg4_dev (%d)", ret);
			goto done;
		}
		ret = pm_runtime_get_sync(g_mtcmos->mfg5_dev);
		if (unlikely(ret < 0)) {
			__gpufreq_abort("fail to enable mfg5_dev (%d)", ret);
			goto done;
		}
		ret = pm_runtime_get_sync(g_mtcmos->mfg6_dev);
		if (unlikely(ret < 0)) {
			__gpufreq_abort("fail to enable mfg6_dev (%d)", ret);
			goto done;
		}
		ret = pm_runtime_get_sync(g_mtcmos->mfg7_dev);
		if (unlikely(ret < 0)) {
			__gpufreq_abort("fail to enable mfg7_dev (%d)", ret);
			goto done;
		}
		ret = pm_runtime_get_sync(g_mtcmos->mfg8_dev);
		if (unlikely(ret < 0)) {
			__gpufreq_abort("fail to enable mfg8_dev (%d)", ret);
			goto done;
		}
		ret = pm_runtime_get_sync(g_mtcmos->mfg9_dev);
		if (unlikely(ret < 0)) {
			__gpufreq_abort("fail to enable mfg9_dev (%d)", ret);
			goto done;
		}
		ret = pm_runtime_get_sync(g_mtcmos->mfg12_dev);
		if (unlikely(ret < 0)) {
			__gpufreq_abort("fail to enable mfg12_dev (%d)", ret);
			goto done;
		}
		ret = pm_runtime_get_sync(g_mtcmos->mfg10_dev);
		if (unlikely(ret < 0)) {
			__gpufreq_abort("fail to enable mfg10_dev (%d)", ret);
			goto done;
		}
		ret = pm_runtime_get_sync(g_mtcmos->mfg13_dev);
		if (unlikely(ret < 0)) {
			__gpufreq_abort("fail to enable mfg13_dev (%d)", ret);
			goto done;
		}
		ret = pm_runtime_get_sync(g_mtcmos->mfg11_dev);
		if (unlikely(ret < 0)) {
			__gpufreq_abort("fail to enable mfg11_dev (%d)", ret);
			goto done;
		}
		ret = pm_runtime_get_sync(g_mtcmos->mfg14_dev);
		if (unlikely(ret < 0)) {
			__gpufreq_abort("fail to enable mfg14_dev (%d)", ret);
			goto done;
		}
		ret = pm_runtime_get_sync(g_mtcmos->mfg15_dev);
		if (unlikely(ret < 0)) {
			__gpufreq_abort("fail to enable mfg15_dev (%d)", ret);
			goto done;
		}
		ret = pm_runtime_get_sync(g_mtcmos->mfg16_dev);
		if (unlikely(ret < 0)) {
			__gpufreq_abort("fail to enable mfg16_dev (%d)", ret);
			goto done;
		}
		ret = pm_runtime_get_sync(g_mtcmos->mfg18_dev);
		if (unlikely(ret < 0)) {
			__gpufreq_abort("fail to enable mfg18_dev (%d)", ret);
			goto done;
		}
		ret = pm_runtime_get_sync(g_mtcmos->mfg17_dev);
		if (unlikely(ret < 0)) {
			__gpufreq_abort("fail to enable mfg17_dev (%d)", ret);
			goto done;
		}
		ret = pm_runtime_get_sync(g_mtcmos->mfg19_dev);
		if (unlikely(ret < 0)) {
			__gpufreq_abort("fail to enable mfg19_dev (%d)", ret);
			goto done;
		}
#endif /* GPUFREQ_SELF_CTRL_MTCMOS */

#if GPUFREQ_CHECK_MFG_PWR_STATUS
		val = MFG_0_19_PWR_STATUS & MFG_0_19_PWR_MASK;
		if (unlikely(val != MFG_0_19_PWR_MASK)) {
			__gpufreq_abort("incorrect MFG0-19 power on status: 0x%08x", val);
			ret = GPUFREQ_EINVAL;
			goto done;
		}
#endif /* GPUFREQ_CHECK_MFG_PWR_STATUS */
#endif /* GPUFREQ_PDCA_ENABLE */
	} else {
#if !GPUFREQ_PDCA_ENABLE
#if GPUFREQ_SELF_CTRL_MTCMOS
		__gpufreq_mfgx_rpc_control(POWER_OFF, MFG_RPC_MFG19_PWR_CON);
		__gpufreq_mfgx_rpc_control(POWER_OFF, MFG_RPC_MFG18_PWR_CON);
		__gpufreq_mfgx_rpc_control(POWER_OFF, MFG_RPC_MFG17_PWR_CON);
		__gpufreq_mfgx_rpc_control(POWER_OFF, MFG_RPC_MFG16_PWR_CON);
		__gpufreq_mfgx_rpc_control(POWER_OFF, MFG_RPC_MFG15_PWR_CON);
		__gpufreq_mfgx_rpc_control(POWER_OFF, MFG_RPC_MFG14_PWR_CON);
		__gpufreq_mfgx_rpc_control(POWER_OFF, MFG_RPC_MFG13_PWR_CON);
		__gpufreq_mfgx_rpc_control(POWER_OFF, MFG_RPC_MFG12_PWR_CON);
		__gpufreq_mfgx_rpc_control(POWER_OFF, MFG_RPC_MFG11_PWR_CON);
		__gpufreq_mfgx_rpc_control(POWER_OFF, MFG_RPC_MFG10_PWR_CON);
		__gpufreq_mfgx_rpc_control(POWER_OFF, MFG_RPC_MFG9_PWR_CON);
		__gpufreq_mfgx_rpc_control(POWER_OFF, MFG_RPC_MFG8_PWR_CON);
		__gpufreq_mfgx_rpc_control(POWER_OFF, MFG_RPC_MFG7_PWR_CON);
		__gpufreq_mfgx_rpc_control(POWER_OFF, MFG_RPC_MFG6_PWR_CON);
		__gpufreq_mfgx_rpc_control(POWER_OFF, MFG_RPC_MFG5_PWR_CON);
		__gpufreq_mfgx_rpc_control(POWER_OFF, MFG_RPC_MFG4_PWR_CON);
		__gpufreq_mfgx_rpc_control(POWER_OFF, MFG_RPC_MFG3_PWR_CON);
		__gpufreq_mfgx_rpc_control(POWER_OFF, MFG_RPC_MFG2_PWR_CON);
#else
		/* manually control MFG2-19 if PDCA is disabled */
		ret = pm_runtime_put_sync(g_mtcmos->mfg19_dev);
		if (unlikely(ret < 0)) {
			__gpufreq_abort("fail to disable mfg19_dev (%d)", ret);
			goto done;
		}
		ret = pm_runtime_put_sync(g_mtcmos->mfg17_dev);
		if (unlikely(ret < 0)) {
			__gpufreq_abort("fail to disable mfg17_dev (%d)", ret);
			goto done;
		}
		ret = pm_runtime_put_sync(g_mtcmos->mfg18_dev);
		if (unlikely(ret < 0)) {
			__gpufreq_abort("fail to disable mfg18_dev (%d)", ret);
			goto done;
		}
		ret = pm_runtime_put_sync(g_mtcmos->mfg16_dev);
		if (unlikely(ret < 0)) {
			__gpufreq_abort("fail to disable mfg16_dev (%d)", ret);
			goto done;
		}
		ret = pm_runtime_put_sync(g_mtcmos->mfg15_dev);
		if (unlikely(ret < 0)) {
			__gpufreq_abort("fail to disable mfg15_dev (%d)", ret);
			goto done;
		}
		ret = pm_runtime_put_sync(g_mtcmos->mfg14_dev);
		if (unlikely(ret < 0)) {
			__gpufreq_abort("fail to disable mfg14_dev (%d)", ret);
			goto done;
		}
		ret = pm_runtime_put_sync(g_mtcmos->mfg11_dev);
		if (unlikely(ret < 0)) {
			__gpufreq_abort("fail to disable mfg11_dev (%d)", ret);
			goto done;
		}
		ret = pm_runtime_put_sync(g_mtcmos->mfg13_dev);
		if (unlikely(ret < 0)) {
			__gpufreq_abort("fail to disable mfg13_dev (%d)", ret);
			goto done;
		}
		ret = pm_runtime_put_sync(g_mtcmos->mfg10_dev);
		if (unlikely(ret < 0)) {
			__gpufreq_abort("fail to disable mfg10_dev (%d)", ret);
			goto done;
		}
		ret = pm_runtime_put_sync(g_mtcmos->mfg12_dev);
		if (unlikely(ret < 0)) {
			__gpufreq_abort("fail to disable mfg12_dev (%d)", ret);
			goto done;
		}
		ret = pm_runtime_put_sync(g_mtcmos->mfg9_dev);
		if (unlikely(ret < 0)) {
			__gpufreq_abort("fail to disable mfg9_dev (%d)", ret);
			goto done;
		}
		ret = pm_runtime_put_sync(g_mtcmos->mfg8_dev);
		if (unlikely(ret < 0)) {
			__gpufreq_abort("fail to disable mfg8_dev (%d)", ret);
			goto done;
		}
		ret = pm_runtime_put_sync(g_mtcmos->mfg7_dev);
		if (unlikely(ret < 0)) {
			__gpufreq_abort("fail to disable mfg7_dev (%d)", ret);
			goto done;
		}
		ret = pm_runtime_put_sync(g_mtcmos->mfg6_dev);
		if (unlikely(ret < 0)) {
			__gpufreq_abort("fail to disable mfg6_dev (%d)", ret);
			goto done;
		}
		ret = pm_runtime_put_sync(g_mtcmos->mfg5_dev);
		if (unlikely(ret < 0)) {
			__gpufreq_abort("fail to disable mfg5_dev (%d)", ret);
			goto done;
		}
		ret = pm_runtime_put_sync(g_mtcmos->mfg4_dev);
		if (unlikely(ret < 0)) {
			__gpufreq_abort("fail to disable mfg4_dev (%d)", ret);
			goto done;
		}
		ret = pm_runtime_put_sync(g_mtcmos->mfg3_dev);
		if (unlikely(ret < 0)) {
			__gpufreq_abort("fail to disable mfg3_dev (%d)", ret);
			goto done;
		}
		ret = pm_runtime_put_sync(g_mtcmos->mfg2_dev);
		if (unlikely(ret < 0)) {
			__gpufreq_abort("fail to disable mfg2_dev (%d)", ret);
			goto done;
		}
#endif /* GPUFREQ_SELF_CTRL_MTCMOS */
#endif /* GPUFREQ_PDCA_ENABLE */

#if GPUFREQ_SELF_CTRL_MTCMOS
		__gpufreq_mfg1_rpc_control(POWER_OFF);
#else
		/* MFG1 off by CCF */
		ret = pm_runtime_put_sync(g_mtcmos->mfg1_dev);
		if (unlikely(ret < 0)) {
			__gpufreq_abort("fail to disable mfg1_dev (%d)", ret);
			goto done;
		}
#endif /* GPUFREQ_SELF_CTRL_MTCMOS */
		g_gpu.mtcmos_count--;
		g_stack.mtcmos_count--;

#if GPUFREQ_CHECK_MFG_PWR_STATUS
		val = MFG_0_19_PWR_STATUS & MFG_1_19_PWR_MASK;
		if (unlikely(val))
			/* only print error if pwr is incorrect when mtcmos off */
			GPUFREQ_LOGE("incorrect MFG1-19 power off status: 0x%08x", val);
#endif /* GPUFREQ_CHECK_MFG_PWR_STATUS */
	}

#if !GPUFREQ_SELF_CTRL_MTCMOS
done:
#endif /* GPUFREQ_SELF_CTRL_MTCMOS */
	GPUFREQ_TRACE_END();

	return ret;
}

static int __gpufreq_buck_control(enum gpufreq_power_state power)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("power=%d", power);

	/* power on: VSRAM -> VGPU -> VSTACK */
	if (power == POWER_ON) {
		ret = regulator_enable(g_pmic->reg_vsram);
		if (unlikely(ret)) {
			__gpufreq_abort("fail to enable VSRAM (%d)", ret);
			goto done;
		}

		ret = regulator_enable(g_pmic->reg_vgpu);
		if (unlikely(ret)) {
			__gpufreq_abort("fail to enable VGPU (%d)", ret);
			goto done;
		}
		g_gpu.buck_count++;

		ret = regulator_enable(g_pmic->reg_vstack);
		if (unlikely(ret)) {
			__gpufreq_abort("fail to enable VSTACK (%d)", ret);
			goto done;
		}
		g_stack.buck_count++;

		/* clear BUCK_ISO  */
		/* SPM_SOC_BUCK_ISO_CON_CLR 0x1C001F80 [8] VGPU_BUCK_ISO = 1'b1 */
		writel(BIT(8), SPM_SOC_BUCK_ISO_CON_CLR);
		/* SPM_SOC_BUCK_ISO_CON_CLR 0x1C001F80 [16] VSTACK_BUCK_ISO = 1'b1 */
		writel(BIT(16), SPM_SOC_BUCK_ISO_CON_CLR);
	/* power off: VSTACK-> VGPU -> VSRAM */
	} else {
		/* set BUCK_ISO  */
		/* SPM_SOC_BUCK_ISO_CON_SET 0x1C001F7C [16] VSTACK_BUCK_ISO = 1'b1 */
		writel(BIT(16), SPM_SOC_BUCK_ISO_CON_SET);
		/* SPM_SOC_BUCK_ISO_CON_SET 0x1C001F7C [8] VGPU_BUCK_ISO = 1'b1 */
		writel(BIT(8), SPM_SOC_BUCK_ISO_CON_SET);

		ret = regulator_disable(g_pmic->reg_vstack);
		if (unlikely(ret)) {
			__gpufreq_abort("fail to disable VSTACK (%d)", ret);
			goto done;
		}
		g_stack.buck_count--;

		ret = regulator_disable(g_pmic->reg_vgpu);
		if (unlikely(ret)) {
			__gpufreq_abort("fail to disable VGPU (%d)", ret);
			goto done;
		}
		g_gpu.buck_count--;

		ret = regulator_disable(g_pmic->reg_vsram);
		if (unlikely(ret)) {
			__gpufreq_abort("fail to disable VSRAM (%d)", ret);
			goto done;
		}
	}

done:
	GPUFREQ_TRACE_END();

	return ret;
}

/* API: init first OPP idx by init freq set in preloader */
static int __gpufreq_init_opp_idx(void)
{
	struct gpufreq_opp_info *working_gpu = g_gpu.working_table;
	struct gpufreq_opp_info *working_stack = g_stack.working_table;
	int target_oppidx = -1;
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START();

	/* get current GPU OPP idx by freq set in preloader */
	g_gpu.cur_oppidx = __gpufreq_get_idx_by_fgpu(g_gpu.cur_freq);
	/* get current STACK OPP idx by freq set in preloader */
	g_stack.cur_oppidx = __gpufreq_get_idx_by_fstack(g_stack.cur_freq);

	/* decide first OPP idx by custom setting */
	if (__gpufreq_custom_init_enable())
		target_oppidx = GPUFREQ_CUST_INIT_OPPIDX;
	/* decide first OPP idx by preloader setting */
	else
		target_oppidx = g_stack.cur_oppidx;

	GPUFREQ_LOGI(
		"init STACK[%d] F(%d->%d) V(%d->%d), GPU[%d] F(%d->%d) V(%d->%d), VSRAM(%d->%d)",
		target_oppidx,
		g_stack.cur_freq, working_stack[target_oppidx].freq,
		g_stack.cur_volt, working_stack[target_oppidx].volt,
		target_oppidx,
		g_gpu.cur_freq, working_gpu[target_oppidx].freq,
		g_gpu.cur_volt, working_gpu[target_oppidx].volt,
		g_stack.cur_vsram,
		MAX(working_stack[target_oppidx].vsram, working_gpu[target_oppidx].vsram));

	/* init first OPP index */
	if (!__gpufreq_dvfs_enable()) {
		g_dvfs_state = DVFS_DISABLE;
		GPUFREQ_LOGI("DVFS state: 0x%x, disable DVFS", g_dvfs_state);

		/* set OPP once if DVFS is disabled but custom init is enabled */
		if (__gpufreq_custom_init_enable())
			ret = __gpufreq_generic_commit_stack(target_oppidx, DVFS_DISABLE);
	} else {
		g_dvfs_state = DVFS_FREE;
		GPUFREQ_LOGI("DVFS state: 0x%x, enable DVFS", g_dvfs_state);

		ret = __gpufreq_generic_commit_stack(target_oppidx, DVFS_FREE);
	}

#if GPUFREQ_MSSV_TEST_MODE
	/* disable DVFS when MSSV test */
	__gpufreq_set_dvfs_state(true, DVFS_MSSV_TEST);
#endif /* GPUFREQ_MSSV_TEST_MODE */

	GPUFREQ_TRACE_END();

	return ret;
}

/* API: calculate power of every OPP in working table */
static void __gpufreq_measure_power(void)
{
	struct gpufreq_opp_info *working_gpu = g_gpu.working_table;
	struct gpufreq_opp_info *working_stack = g_stack.working_table;
	unsigned int freq = 0, volt = 0;
	unsigned int p_total = 0, p_dynamic = 0, p_leakage = 0;
	int opp_num_gpu = g_gpu.opp_num;
	int opp_num_stack = g_stack.opp_num;
	int i = 0;

	for (i = 0; i < opp_num_gpu; i++) {
		freq = working_gpu[i].freq;
		volt = working_gpu[i].volt;

		p_leakage = __gpufreq_get_lkg_pgpu(volt);
		p_dynamic = __gpufreq_get_dyn_pgpu(freq, volt);

		p_total = p_dynamic + p_leakage;

		working_gpu[i].power = p_total;

		GPUFREQ_LOGD("GPU[%02d] power: %d (dynamic: %d, leakage: %d)",
			i, p_total, p_dynamic, p_leakage);
	}

	for (i = 0; i < opp_num_stack; i++) {
		freq = working_stack[i].freq;
		volt = working_stack[i].volt;

		p_leakage = __gpufreq_get_lkg_pstack(volt);
		p_dynamic = __gpufreq_get_dyn_pstack(freq, volt);

		p_total = p_dynamic + p_leakage;

		working_stack[i].power = p_total;

		GPUFREQ_LOGD("STACK[%02d] power: %d (dynamic: %d, leakage: %d)",
			i, p_total, p_dynamic, p_leakage);
	}

	/* update current status to shared memory */
	if (g_shared_status) {
		g_shared_status->cur_power_gpu = working_gpu[g_gpu.cur_oppidx].power;
		g_shared_status->max_power_gpu = working_gpu[g_gpu.max_oppidx].power;
		g_shared_status->min_power_gpu = working_gpu[g_gpu.min_oppidx].power;
		g_shared_status->cur_power_stack = working_stack[g_stack.cur_oppidx].power;
		g_shared_status->max_power_stack = working_stack[g_stack.max_oppidx].power;
		g_shared_status->min_power_stack = working_stack[g_stack.min_oppidx].power;
	}
}

/*
 * API: interpolate OPP from signoff idx.
 * step = (large - small) / range
 * vnew = large - step * j
 */
static void __gpufreq_interpolate_volt(enum gpufreq_target target)
{
	unsigned int large_volt = 0, small_volt = 0;
	unsigned int large_freq = 0, small_freq = 0;
	unsigned int inner_volt = 0, inner_freq = 0;
	unsigned int previous_volt = 0;
	int adj_num = 0, i = 0, j = 0;
	int front_idx = 0, rear_idx = 0, inner_idx = 0;
	int range = 0, slope = 0;
	const int *signed_idx = NULL;
	struct gpufreq_opp_info *signed_table = NULL;

	if (target == TARGET_STACK) {
		adj_num = NUM_STACK_SIGNED_IDX;
		signed_idx = g_stack_signed_idx;
		signed_table = g_stack.signed_table;
	} else {
		adj_num = NUM_GPU_SIGNED_IDX;
		signed_idx = g_gpu_signed_idx;
		signed_table = g_gpu.signed_table;
	}

	mutex_lock(&gpufreq_lock);

	for (i = 1; i < adj_num; i++) {
		front_idx = signed_idx[i - 1];
		rear_idx = signed_idx[i];
		range = rear_idx - front_idx;

		/* freq division to amplify slope */
		large_volt = signed_table[front_idx].volt * 100;
		large_freq = signed_table[front_idx].freq / 1000;

		small_volt = signed_table[rear_idx].volt * 100;
		small_freq = signed_table[rear_idx].freq / 1000;

		/* slope = volt / freq */
		slope = (int)(large_volt - small_volt) / (int)(large_freq - small_freq);

		if (unlikely(slope < 0))
			__gpufreq_abort("invalid slope: %d", slope);

		GPUFREQ_LOGD("%s[%02d*] Freq: %d, Volt: %d, slope: %d",
			target == TARGET_STACK ? "STACK" : "GPU",
			rear_idx, small_freq * 1000, small_volt / 100, slope);

		/* start from small V and F, and use (+) instead of (-) */
		for (j = 1; j < range; j++) {
			inner_idx = rear_idx - j;
			inner_freq = signed_table[inner_idx].freq / 1000;
			inner_volt = (small_volt + slope * (inner_freq - small_freq)) / 100;
			inner_volt = VOLT_NORMALIZATION(inner_volt);

			/* compare interpolated volt with volt of previous OPP idx */
			previous_volt = signed_table[inner_idx + 1].volt;
			if (inner_volt < previous_volt)
				__gpufreq_abort("invalid %s[%02d*] Volt: %d < [%02d*] Volt: %d",
					target == TARGET_STACK ? "STACK" : "GPU",
					inner_idx, inner_volt, inner_idx + 1, previous_volt);

			/* record margin */
			signed_table[inner_idx].margin += signed_table[inner_idx].volt - inner_volt;
			/* update to signed table */
			signed_table[inner_idx].volt = inner_volt;
			signed_table[inner_idx].vsram = __gpufreq_get_vsram_by_vlogic(inner_volt);

			GPUFREQ_LOGD("%s[%02d*] Freq: %d, Volt: %d, Vsram: %d",
				target == TARGET_STACK ? "STACK" : "GPU",
				inner_idx, inner_freq * 1000, inner_volt,
				signed_table[inner_idx].vsram);
		}
		GPUFREQ_LOGD("%s[%02d*] Freq: %d, Volt: %d",
			target == TARGET_STACK ? "STACK" : "GPU",
			front_idx, large_freq * 1000, large_volt / 100);
	}

	mutex_unlock(&gpufreq_lock);
}

#if GPUFREQ_ASENSOR_ENABLE
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
	unsigned int cur_fgpu = 0, cur_vgpu = 0, target_fgpu = 0, target_vgpu = 0;
	unsigned int cur_fstack = 0, cur_vstack = 0, target_fstack = 0, target_vstack = 0;
	unsigned int cur_vsram = 0, target_vsram = 0;

	GPUFREQ_TRACE_START();

	__gpufreq_set_dvfs_state(true, DVFS_AGING_KEEP);

	ret = __gpufreq_power_control(POWER_ON);
	if (unlikely(ret < 0)) {
		__gpufreq_set_dvfs_state(false, DVFS_AGING_KEEP);
		goto done;
	}

	mutex_lock(&gpufreq_lock);

	/* prepare OPP setting */
	cur_fgpu = g_gpu.cur_freq;
	cur_vgpu = g_gpu.cur_volt;
	cur_fstack = g_stack.cur_freq;
	cur_vstack = g_stack.cur_volt;
	cur_vsram = g_stack.cur_vsram;

	target_fstack = GPUFREQ_AGING_KEEP_FSTACK;
	target_vstack = GPUFREQ_AGING_KEEP_VSTACK;
	target_fgpu = GPUFREQ_AGING_KEEP_FGPU;
	target_vgpu = GPUFREQ_AGING_KEEP_VGPU;
	target_vsram = GPUFREQ_AGING_KEEP_VSRAM;

	GPUFREQ_LOGD(
		"begin to commit STACK F(%d->%d) V(%d->%d), GPU F(%d->%d) V(%d->%d), VSRAM(%d->%d)",
		cur_fstack, target_fstack, cur_vstack, target_vstack,
		cur_fgpu, target_fgpu, cur_vgpu, target_vgpu,
		cur_vsram, target_vsram);

	ret = __gpufreq_generic_scale(cur_fgpu, target_fgpu, cur_vgpu, target_vgpu,
		cur_fstack, target_fstack, cur_vstack, target_vstack, cur_vsram, target_vsram);
	if (ret) {
		GPUFREQ_LOGE("fail to scale to GPU F(%d) V(%d), STACK F(%d) V(%d), VSRAM(%d)",
			target_fgpu, target_vgpu, target_fstack, target_vstack, target_vsram);
		goto done_unlock;
	}

	GPUFREQ_LOGI("pause DVFS at STACK(%d, %d), GPU(%d, %d), VSRAM(%d), state: 0x%x",
		target_fstack, target_vstack, target_fgpu, target_vgpu,
		target_vsram, g_dvfs_state);

done_unlock:
	mutex_unlock(&gpufreq_lock);
done:
	GPUFREQ_TRACE_END();

	return ret;
}

/* API: get Aging sensor data from EFUSE, return if success*/
static unsigned int __gpufreq_asensor_read_efuse(u32 *a_t0_efuse1,
	u32 *a_t0_efuse2, u32 *a_t0_efuse3, u32 *efuse_error)
{
	u32 efuse1 = 0, efuse2 = 0, efuse3 = 0;

	/* efuse1: 0x11E805CC */
	efuse1 = readl(EFUSE_ASENSOR_RT);
	/* efuse2: 0x11E805D0 */
	efuse2 = readl(EFUSE_ASENSOR_HT);
	/* efuse3: 0x11E805DC */
	efuse3 = readl(EFUSE_ASENSOR_TEMPER);

	if (!efuse1)
		return false;

	GPUFREQ_LOGD("efuse1: 0x%08x, efuse2: 0x%08x, efuse3: 0x%08x", efuse1, efuse2, efuse3);

	/* A_T0_LVT_RT: 0x11E805CC [9:0] */
	*a_t0_efuse1 = (efuse1  & GENMASK(9, 0)) + 1200;
	/* A_T0_ULVT_RT: 0x11E805CC [19:10] */
	*a_t0_efuse2 = ((efuse1 & GENMASK(19, 10)) >> 10) + 1800;
	/* A_T0_LVTLL_RT: 0x11E805CC [29:20] */
	*a_t0_efuse3 = ((efuse1 & GENMASK(29, 20)) >> 20) + 800;
	/* efuse_error: 0x11E805CC [31] */
	*efuse_error = ((efuse1 & BIT(31)) >> 31);

	g_asensor_info.efuse1 = efuse1;
	g_asensor_info.efuse2 = efuse2;
	g_asensor_info.efuse3 = efuse3;
	g_asensor_info.efuse1_addr = 0x11E805CC;
	g_asensor_info.efuse2_addr = 0x11E805D0;
	g_asensor_info.efuse3_addr = 0x11E805DC;
	g_asensor_info.a_t0_efuse1 = *a_t0_efuse1;
	g_asensor_info.a_t0_efuse2 = *a_t0_efuse2;
	g_asensor_info.a_t0_efuse3 = *a_t0_efuse3;
	g_asensor_info.lvts5_0_y_temperature = ((efuse3 & GENMASK(31, 24)) >> 24);

	GPUFREQ_LOGD("a_t0_efuse1: 0x%08x, a_t0_efuse2: 0x%08x, a_t0_efuse3: 0x%08x",
		*a_t0_efuse1, *a_t0_efuse2, *a_t0_efuse3);
	GPUFREQ_LOGD("efuse_error: 0x%08x, lvts5_0_y_temperature: %d",
		*efuse_error, g_asensor_info.lvts5_0_y_temperature);

	return true;
}

static void __gpufreq_asensor_read_register(u32 *a_tn_sensor1,
	u32 *a_tn_sensor2, u32 *a_tn_sensor3)
{
	u32 aging_data1 = 0, aging_data2 = 0, a_tn_sensor4 = 0;
	int i = 0;

	/* Enable CPE CG */
	/* MFG_SENSOR_BCLK_CG 0x13FBFF98 = 0x00000001 */
	writel(0x00000001, MFG_SENSOR_BCLK_CG);
	/* Config and trigger Sensor */
	/* CPE_CTRL_MCU_REG_CPEMONCTL 0x13FB9C00 = 0x09010103 */
	writel(0x09010103, MFG_CPE_CTRL_MCU_REG_CPEMONCTL);
	/* Enable CPE */
	/* CPE_CTRL_MCU_REG_CEPEN 0x13FB9C04 = 0x0000FFFF */
	writel(0x0000FFFF, MFG_CPE_CTRL_MCU_REG_CEPEN);
	/* wait 70us */
	udelay(70);

	/* check IRQ status */
	/* MFG_CPE_CTRL_MCU_REG_CPEIRQSTS 0x13FB9C10 = 0x80000000 */
	while (readl(MFG_CPE_CTRL_MCU_REG_CPEIRQSTS) != 0x80000000) {
		udelay(10);
		if (++i > 5) {
			GPUFREQ_LOGW("wait Aging Sensor IRQ timeout: 0x%x",
				readl(MFG_CPE_CTRL_MCU_REG_CPEIRQSTS));
			*a_tn_sensor1 = 0;
			*a_tn_sensor2 = 0;
			*a_tn_sensor3 = 0;
			return;
		}
	}

	/* Read sensor data */
	/* ASENSORDATA2 0x13FB6008 */
	aging_data1 = readl(MFG_CPE_SENSOR_C0ASENSORDATA2);
	/* ASENSORDATA3 0x13FB600C */
	aging_data2 = readl(MFG_CPE_SENSOR_C0ASENSORDATA3);

	GPUFREQ_LOGD("aging_data1: 0x%08x, aging_data2: 0x%08x", aging_data1, aging_data2);

	/* clear IRQ status */
	/* MFG_CPE_CTRL_MCU_REG_CPEINTSTS 0x13FB9C28 = 0xFFFFFFFF */
	writel(0xFFFFFFFF, MFG_CPE_CTRL_MCU_REG_CPEINTSTS);

	/* A_TN_LVT_CNT 0x13FB6008 [31:16] */
	*a_tn_sensor1 = (aging_data1 & GENMASK(31, 16)) >> 16;
	/* A_TN_ULVT_CNT 0x13FB600C [15:0] */
	*a_tn_sensor2 = aging_data2 & GENMASK(15, 0);
	/* A_TN_LVTLL_CNT 0x13FB600C [31:16] */
	*a_tn_sensor3 = (aging_data2 & GENMASK(31, 16)) >> 16;
	/* 0x13FB6008 [15:0] */
	a_tn_sensor4 = aging_data1 & GENMASK(15, 0);

	g_asensor_info.a_tn_sensor1 = *a_tn_sensor1;
	g_asensor_info.a_tn_sensor2 = *a_tn_sensor2;
	g_asensor_info.a_tn_sensor3 = *a_tn_sensor3;
	g_asensor_info.a_tn_sensor4 = a_tn_sensor4;

	GPUFREQ_LOGD("a_tn_sensor1: 0x%08x, a_tn_sensor2: 0x%08x, a_tn_sensor3: 0x%08x",
		*a_tn_sensor1, *a_tn_sensor2, *a_tn_sensor3);
}

static unsigned int __gpufreq_get_aging_table_idx(
	u32 a_t0_efuse1, u32 a_t0_efuse2, u32 a_t0_efuse3,
	u32 a_tn_sensor1, u32 a_tn_sensor2, u32 a_tn_sensor3,
	u32 efuse_error, unsigned int is_efuse_read_success)
{
	unsigned int aging_table_idx = GPUFREQ_AGING_MOST_AGRRESIVE;
	int a_diff = 0, a_diff1 = 0, a_diff2 = 0, a_diff3 = 0;
	int tj = 0, tj_max = 0;
	unsigned int leakage_power = 0;

	tj_max = 30;
	tj = (((tj_max - 25) * 50) / 40);

	a_diff1 = a_t0_efuse1 + tj - a_tn_sensor1;
	a_diff2 = a_t0_efuse2 + tj - a_tn_sensor2;
	a_diff3 = a_t0_efuse3 + tj - a_tn_sensor3;
	a_diff = MAX(a_diff1, a_diff2);
	a_diff = MAX(a_diff, a_diff3);

	leakage_power = __gpufreq_get_lkg_pstack(GPUFREQ_AGING_LKG_VSTACK);

	g_asensor_info.tj_max = tj_max;
	g_asensor_info.a_diff1 = a_diff1;
	g_asensor_info.a_diff2 = a_diff2;
	g_asensor_info.a_diff3 = a_diff3;
	g_asensor_info.leakage_power = leakage_power;

	GPUFREQ_LOGD("tj_max: %d, tj: %d, a_diff1: %d, a_diff2: %d, a_diff3: %d",
		tj_max, tj, a_diff1, a_diff2, a_diff3);
	GPUFREQ_LOGD("a_diff: %d, leakage_power: %d, is_efuse_read_success: %d",
		a_diff, leakage_power, is_efuse_read_success);

	if (g_aging_load)
		aging_table_idx = GPUFREQ_AGING_MOST_AGRRESIVE;
	else if (!is_efuse_read_success)
		aging_table_idx = 3;
	else if (tj_max < 25)
		aging_table_idx = 3;
	else if (efuse_error)
		aging_table_idx = 3;
	else if (a_diff < GPUFREQ_AGING_GAP_MIN)
		aging_table_idx = 3;
	else if (leakage_power < 20)
		aging_table_idx = 3;
	else if (a_diff < GPUFREQ_AGING_GAP_1)
		aging_table_idx = 0;
	else if (a_diff < GPUFREQ_AGING_GAP_2)
		aging_table_idx = 1;
	else if (a_diff < GPUFREQ_AGING_GAP_3)
		aging_table_idx = 2;
	else if (a_diff >= GPUFREQ_AGING_GAP_3)
		aging_table_idx = 3;
	else {
		GPUFREQ_LOGW("fail to find aging_table_idx");
		aging_table_idx = 3;
	}

	/* check the MostAgrresive setting */
	aging_table_idx = MAX(aging_table_idx, GPUFREQ_AGING_MOST_AGRRESIVE);

	return aging_table_idx;
}
#endif /* GPUFREQ_ASENSOR_ENABLE */

static void __gpufreq_compute_aging(void)
{
	unsigned int aging_table_idx = GPUFREQ_AGING_MAX_TABLE_IDX;

#if GPUFREQ_ASENSOR_ENABLE
	u32 a_t0_efuse1 = 0, a_t0_efuse2 = 0, a_t0_efuse3 = 0;
	u32 a_tn_sensor1 = 0, a_tn_sensor2 = 0, a_tn_sensor3 = 0;
	u32 efuse_error = 0;
	unsigned int is_efuse_read_success = false;

	if (__gpufreq_dvfs_enable()) {
		/* keep volt for A sensor working */
		__gpufreq_pause_dvfs();

		/* get aging efuse data */
		is_efuse_read_success = __gpufreq_asensor_read_efuse(
			&a_t0_efuse1, &a_t0_efuse2, &a_t0_efuse3, &efuse_error);

		/* get aging sensor data */
		__gpufreq_asensor_read_register(&a_tn_sensor1, &a_tn_sensor2, &a_tn_sensor3);

		/* resume DVFS */
		__gpufreq_resume_dvfs();

		/* compute aging_table_idx with aging efuse data and aging sensor data */
		aging_table_idx = __gpufreq_get_aging_table_idx(
			a_t0_efuse1, a_t0_efuse2, a_t0_efuse3,
			a_tn_sensor1, a_tn_sensor2, a_tn_sensor3,
			efuse_error, is_efuse_read_success);
	}

	g_asensor_info.aging_table_idx_agrresive = GPUFREQ_AGING_MOST_AGRRESIVE;
	g_asensor_info.aging_table_idx = aging_table_idx;

	GPUFREQ_LOGI("Aging Sensor table id: %d", aging_table_idx);
#endif /* GPUFREQ_ASENSOR_ENABLE */

	if (g_aging_load)
		aging_table_idx = GPUFREQ_AGING_MOST_AGRRESIVE;

	if (aging_table_idx > GPUFREQ_AGING_MAX_TABLE_IDX)
		aging_table_idx = GPUFREQ_AGING_MAX_TABLE_IDX;

	/* Aging margin is set if any OPP is adjusted by Aging */
	if (aging_table_idx == 0)
		g_aging_margin = true;

	g_aging_table_idx = aging_table_idx;
}

static unsigned int __gpufreq_compute_avs_freq(u32 val)
{
	unsigned int freq = 0;

	freq |= (val & BIT(20)) >> 10;         /* Get freq[10]  from efuse[20]    */
	freq |= (val & GENMASK(11, 10)) >> 2;  /* Get freq[9:8] from efuse[11:10] */
	freq |= (val & GENMASK(1, 0)) << 6;    /* Get freq[7:6] from efuse[1:0]   */
	freq |= (val & GENMASK(7, 6)) >> 2;    /* Get freq[5:4] from efuse[7:6]   */
	freq |= (val & GENMASK(19, 18)) >> 16; /* Get freq[3:2] from efuse[19:18] */
	freq |= (val & GENMASK(13, 12)) >> 12; /* Get freq[1:0] from efuse[13:12] */
	/* Freq is stored in efuse with MHz unit */
	freq *= 1000;

	return freq;
}

static unsigned int __gpufreq_compute_avs_volt(u32 val)
{
	unsigned int volt = 0;

	volt |= (val & GENMASK(17, 14)) >> 14; /* Get volt[3:0] from efuse[17:14] */
	volt |= (val & GENMASK(5, 4));         /* Get volt[5:4] from efuse[5:4]   */
	volt |= (val & GENMASK(3, 2)) << 4;    /* Get volt[7:6] from efuse[3:2]   */
	/* Volt is stored in efuse with 6.25mV unit */
	volt *= 625;

	return volt;
}

static void __gpufreq_compute_avs(void)
{
	u32 val = 0, val_sn = 0;
	unsigned int temp_volt = 0, temp_freq = 0, volt_ofs = 0, temp_volt_sn = 0;
	int i = 0, oppidx = 0, adj_num_gpu = 0, adj_num_stack = 0;

	adj_num_gpu = NUM_GPU_SIGNED_IDX;
	adj_num_stack = NUM_STACK_SIGNED_IDX;

	/*
	 * Compute GPU AVS
	 *
	 * Freq (MHz) | Signoff Volt (V) | Efuse name | Efuse addr
	 * ============================================================
	 * 1000       | 0.9              | -          | -
	 * 890        | 0.75             | -          | -
	 * 700        | 0.675            | PTPOD22    | 0x11E805D8
	 * 385        | 0.575            | -          | -
	 */
	/* read AVS efuse and compute Freq and Volt */
	for (i = 0; i < adj_num_gpu; i++) {
		oppidx = g_gpu_avs_table[i].oppidx;
		if (g_mcl50_load) {
			if (i == 2)
				val = 0x0048BCF6;
			else
				val = 0;
		} else if (g_avs_enable) {
			if (i == 2)
				val = readl(EFUSE_PTPOD22_AVS);
			else
				val = 0;
		} else
			val = 0;

		/* if efuse value is not set */
		if (!val)
			continue;

		/* compute Freq from efuse */
		temp_freq = __gpufreq_compute_avs_freq(val);
		/* verify with signoff Freq */
		if (temp_freq != g_gpu.signed_table[oppidx].freq) {
			__gpufreq_abort("GPU[%02d*]: efuse[%d].freq(%d) != signed-off.freq(%d)",
				oppidx, i, temp_freq, g_gpu.signed_table[oppidx].freq);
			return;
		}
		g_gpu_avs_table[i].freq = temp_freq;

		/* compute Volt from efuse */
		temp_volt = __gpufreq_compute_avs_volt(val);
		g_gpu_avs_table[i].volt = temp_volt;

		/* AVS margin is set if any OPP is adjusted by AVS */
		g_avs_margin = true;

		/* clamp to signoff Volt */
		if (g_gpu_avs_table[i].volt > g_gpu.signed_table[oppidx].volt) {
			GPUFREQ_LOGW("GPU[%02d*]: efuse[%02d].volt(%d) > signed-off.volt(%d)",
				oppidx, i, g_gpu_avs_table[i].volt,
				g_gpu.signed_table[oppidx].volt);
			g_gpu_avs_table[i].volt = g_gpu.signed_table[oppidx].volt;
		}

		/* update Vsram */
		g_gpu_avs_table[i].vsram = __gpufreq_get_vsram_by_vlogic(g_gpu_avs_table[i].volt);
	}

	/* check AVS Volt and update Vsram */
	for (i = adj_num_gpu - 1; i >= 0; i--) {
		oppidx = g_gpu_avs_table[i].oppidx;
		/* mV * 100 */
		volt_ofs = 1250;

		/* if AVS Volt is not set */
		if (!g_gpu_avs_table[i].volt)
			continue;

		/*
		 * AVS Volt reverse check, start from adj_num -2
		 * Volt of sign-off[i] should always be larger than sign-off[i + 1]
		 * if not, add Volt offset to sign-off[i]
		 */
		if (i != (adj_num_gpu - 1)) {
			if (g_gpu_avs_table[i].volt <= g_gpu_avs_table[i + 1].volt) {
				GPUFREQ_LOGW("GPU efuse[%02d].volt(%d) <= efuse[%02d].volt(%d)",
					i, g_gpu_avs_table[i].volt,
					i + 1, g_gpu_avs_table[i + 1].volt);
				g_gpu_avs_table[i].volt = g_gpu_avs_table[i + 1].volt + volt_ofs;
			}
		}

		/* clamp to signoff Volt */
		if (g_gpu_avs_table[i].volt > g_gpu.signed_table[oppidx].volt) {
			GPUFREQ_LOGW("GPU[%02d*]: efuse[%02d].volt(%d) > signed-off.volt(%d)",
				oppidx, i, g_gpu_avs_table[i].volt,
				g_gpu.signed_table[oppidx].volt);
			g_gpu_avs_table[i].volt = g_gpu.signed_table[oppidx].volt;
		}

		/* update Vsram */
		g_gpu_avs_table[i].vsram = __gpufreq_get_vsram_by_vlogic(g_gpu_avs_table[i].volt);
	}

	/*
	 * Compute STACK AVS
	 *
	 * Freq (MHz) | Signoff Volt (V) | Efuse name | Efuse addr
	 * ============================================================
	 * 981        | 0.8              | PTPOD23    | 0x11E805DC
	 * 670        | 0.675            | PTPOD24    | 0x11E805E0
	 * 390        | 0.575            | PTPOD25    | 0x11E805E4
	 * 224        | 0.5              | PTPOD26    | 0x11E805E8
	 */
	/* read AVS efuse and compute Freq and Volt */
	for (i = 0; i < adj_num_stack; i++) {
		oppidx = g_stack_avs_table[i].oppidx;
		if (g_mcl50_load) {
			if (i == 0)
				val = 0x0048BCF6;
			else if (i == 1)
				val = 0x00C30C65;
			else if (i == 2)
				val = 0x008E1C64;
			else if (i == 3)
				val = 0x00886825;
			else
				val = 0;
		} else if (g_avs_enable) {
			if (i == 0) {
				val = readl(EFUSE_PTPOD23_AVS);
				val_sn = readl(EFUSE_PTPOD21_SN);
			} else if (i == 1) {
				val = readl(EFUSE_PTPOD24_AVS);
				val_sn = 0;
			} else if (i == 2) {
				val = readl(EFUSE_PTPOD25_AVS);
				val_sn = 0;
			} else if (i == 3) {
				val = readl(EFUSE_PTPOD26_AVS);
				val_sn = 0;
			} else {
				val = 0;
				val_sn = 0;
			}
		} else {
			val = 0;
			val_sn = 0;
		}

		/* if efuse value is not set */
		if (!val)
			continue;

		/* compute Freq from efuse */
		temp_freq = __gpufreq_compute_avs_freq(val);
		/* verify with signoff Freq */
		if (temp_freq != g_stack.signed_table[oppidx].freq) {
			__gpufreq_abort("STACK[%02d*]: efuse[%d].freq(%d) != signed-off.freq(%d)",
				oppidx, i, temp_freq, g_stack.signed_table[oppidx].freq);
			return;
		}
		g_stack_avs_table[i].freq = temp_freq;

		/* compute Volt from efuse */
		temp_volt = __gpufreq_compute_avs_volt(val);
		/* mt6985 new: Vsn on binning P1 */
		if (val_sn) {
			temp_volt_sn = __gpufreq_compute_avs_volt(val_sn);
			g_stack_avs_table[i].volt = MIN(temp_volt, temp_volt_sn);
		} else
			g_stack_avs_table[i].volt = temp_volt;

		/* AVS margin is set if any OPP is adjusted by AVS */
		g_avs_margin = true;
		/* PTPOD22 [31:24] GPU_PTP_version */
		if (i == 1)
			g_ptp_version = (val & GENMASK(31, 24)) >> 24;
	}

	/* check AVS Volt and update Vsram */
	for (i = adj_num_stack - 1; i >= 0; i--) {
		oppidx = g_stack_avs_table[i].oppidx;
		/* mV * 100 */
		volt_ofs = 1250;

		/* if AVS Volt is not set */
		if (!g_stack_avs_table[i].volt)
			continue;

		/*
		 * AVS Volt reverse check, start from adj_num -2
		 * Volt of sign-off[i] should always be larger than sign-off[i + 1]
		 * if not, add Volt offset to sign-off[i]
		 */
		if (i != (adj_num_stack - 1)) {
			if (g_stack_avs_table[i].volt <= g_stack_avs_table[i + 1].volt) {
				GPUFREQ_LOGW("STACK efuse[%02d].volt(%d) <= efuse[%02d].volt(%d)",
					i, g_stack_avs_table[i].volt,
					i + 1, g_stack_avs_table[i + 1].volt);
				g_stack_avs_table[i].volt =
					g_stack_avs_table[i + 1].volt + volt_ofs;
			}
		}

		/* clamp to signoff Volt */
		if (g_stack_avs_table[i].volt > g_stack.signed_table[oppidx].volt) {
			GPUFREQ_LOGW("STACK[%02d*]: efuse[%02d].volt(%d) > signed-off.volt(%d)",
				oppidx, i, g_stack_avs_table[i].volt,
				g_stack.signed_table[oppidx].volt);
			g_stack_avs_table[i].volt = g_stack.signed_table[oppidx].volt;
		}

		/* update Vsram */
		g_stack_avs_table[i].vsram =
			__gpufreq_get_vsram_by_vlogic(g_stack_avs_table[i].volt);
	}

	for (i = 0; i < adj_num_gpu; i++)
		GPUFREQ_LOGI("GPU[%02d*]: efuse[%02d] freq(%d), volt(%d)",
			g_gpu_avs_table[i].oppidx, i, g_gpu_avs_table[i].freq,
			g_gpu_avs_table[i].volt);

	for (i = 0; i < adj_num_stack; i++)
		GPUFREQ_LOGI("STACK[%02d*]: efuse[%02d] freq(%d), volt(%d)",
			g_stack_avs_table[i].oppidx, i, g_stack_avs_table[i].freq,
			g_stack_avs_table[i].volt);
}

static void __gpufreq_apply_adjustment(void)
{
	struct gpufreq_opp_info *signed_gpu = NULL;
	struct gpufreq_opp_info *signed_stack = NULL;
	int adj_num_gpu = 0, adj_num_stack = 0, oppidx = 0, i = 0;
	unsigned int avs_volt = 0, avs_vsram = 0, aging_volt = 0;

	signed_gpu = g_gpu.signed_table;
	signed_stack = g_stack.signed_table;
	adj_num_gpu = NUM_GPU_SIGNED_IDX;
	adj_num_stack = NUM_STACK_SIGNED_IDX;

	/* apply AVS margin */
	if (g_avs_margin) {
		/* GPU AVS */
		for (i = 0; i < adj_num_gpu; i++) {
			oppidx = g_gpu_avs_table[i].oppidx;
			avs_volt = g_gpu_avs_table[i].volt ?
				g_gpu_avs_table[i].volt : signed_gpu[oppidx].volt;
			avs_vsram = g_gpu_avs_table[i].vsram ?
				g_gpu_avs_table[i].vsram : signed_gpu[oppidx].vsram;
			/* record margin */
			signed_gpu[oppidx].margin += signed_gpu[oppidx].volt - avs_volt;
			/* update to signed table */
			signed_gpu[oppidx].volt = avs_volt;
			signed_gpu[oppidx].vsram = avs_vsram;
		}
		/* STACK AVS */
		for (i = 0; i < adj_num_stack; i++) {
			oppidx = g_stack_avs_table[i].oppidx;
			avs_volt = g_stack_avs_table[i].volt ?
				g_stack_avs_table[i].volt : signed_stack[oppidx].volt;
			avs_vsram = g_stack_avs_table[i].vsram ?
				g_stack_avs_table[i].vsram : signed_stack[oppidx].vsram;
			/* record margin */
			signed_stack[oppidx].margin += signed_stack[oppidx].volt - avs_volt;
			/* update to signed table */
			signed_stack[oppidx].volt = avs_volt;
			signed_stack[oppidx].vsram = avs_vsram;
		}
	} else
		GPUFREQ_LOGI("AVS margin is not set");

	/* apply Aging margin */
	if (g_aging_margin) {
		/* GPU Aging */
		for (i = 0; i < adj_num_gpu; i++) {
			oppidx = g_gpu_aging_table[g_aging_table_idx][i].oppidx;
			aging_volt = g_gpu_aging_table[g_aging_table_idx][i].volt;
			/* record margin */
			signed_gpu[oppidx].margin += aging_volt;
			/* update to signed table */
			signed_gpu[oppidx].volt -= aging_volt;
			signed_gpu[oppidx].vsram =
				__gpufreq_get_vsram_by_vlogic(signed_gpu[oppidx].volt);
		}
		/* STACK Aging */
		for (i = 0; i < adj_num_stack; i++) {
			oppidx = g_stack_aging_table[g_aging_table_idx][i].oppidx;
			aging_volt = g_stack_aging_table[g_aging_table_idx][i].volt;
			/* record margin */
			signed_stack[oppidx].margin += aging_volt;
			/* update to signed table */
			signed_stack[oppidx].volt -= aging_volt;
			signed_stack[oppidx].vsram =
				__gpufreq_get_vsram_by_vlogic(signed_stack[oppidx].volt);
		}
	} else
		GPUFREQ_LOGI("Aging margin is not set");

	/* compute others OPP exclude signoff idx */
	__gpufreq_interpolate_volt(TARGET_GPU);
	__gpufreq_interpolate_volt(TARGET_STACK);
}

static void __gpufreq_init_shader_present(void)
{
	unsigned int segment_id = 0;

	segment_id = g_stack.segment_id;

	switch (segment_id) {
	default:
		g_shader_present = GPU_SHADER_PRESENT_11;
	}
	GPUFREQ_LOGD("segment_id: %d, shader_present: %d", segment_id, g_shader_present);
}

/*
 * 1. init OPP segment range
 * 2. init segment/working OPP table
 * 3. init power measurement
 * 4. init springboard table
 */
static void __gpufreq_init_opp_table(void)
{
	unsigned int segment_id = 0;
	int i = 0, j = 0;

	/* init current GPU/STACK freq and volt set by preloader */
	g_gpu.cur_freq = __gpufreq_get_real_fgpu();
	g_gpu.cur_volt = __gpufreq_get_real_vgpu();
	g_gpu.cur_vsram = __gpufreq_get_real_vsram();

	g_stack.cur_freq = __gpufreq_get_real_fstack();
	g_stack.cur_volt = __gpufreq_get_real_vstack();
	g_stack.cur_vsram = __gpufreq_get_real_vsram();

	GPUFREQ_LOGI(
		"preloader init [GPU] Freq: %d, Volt: %d [STACK] Freq: %d, Volt: %d, Vsram: %d",
		g_gpu.cur_freq, g_gpu.cur_volt,
		g_stack.cur_freq, g_stack.cur_volt, g_stack.cur_vsram);

	/* init GPU OPP table */
	segment_id = g_stack.segment_id;
	if (segment_id == MT6985_SEGMENT)
		g_gpu.segment_upbound = 0;
	else
		g_gpu.segment_upbound = 0;
	g_gpu.segment_lowbound = NUM_GPU_SIGNED_OPP - 1;
	g_gpu.signed_opp_num = NUM_GPU_SIGNED_OPP;
	g_gpu.max_oppidx = 0;
	g_gpu.min_oppidx = g_gpu.segment_lowbound - g_gpu.segment_upbound;
	g_gpu.opp_num = g_gpu.min_oppidx + 1;
	g_gpu.signed_table = g_gpu_default_opp_table;

	g_gpu.working_table = kcalloc(g_gpu.opp_num, sizeof(struct gpufreq_opp_info), GFP_KERNEL);
	if (!g_gpu.working_table) {
		__gpufreq_abort("fail to alloc g_gpu.working_table (%dB)",
			g_gpu.opp_num * sizeof(struct gpufreq_opp_info));
		return;
	}

	GPUFREQ_LOGD("num of signed GPU OPP: %d, segment bound: [%d, %d]",
		g_gpu.signed_opp_num, g_gpu.segment_upbound, g_gpu.segment_lowbound);
	GPUFREQ_LOGI("num of working GPU OPP: %d", g_gpu.opp_num);

	/* init STACK OPP table */
	segment_id = g_stack.segment_id;
	if (segment_id == MT6985_SEGMENT)
		g_stack.segment_upbound = 0;
	else
		g_stack.segment_upbound = 0;
	g_stack.segment_lowbound = NUM_STACK_SIGNED_OPP - 1;
	g_stack.signed_opp_num = NUM_STACK_SIGNED_OPP;
	g_stack.max_oppidx = 0;
	g_stack.min_oppidx = g_stack.segment_lowbound - g_stack.segment_upbound;
	g_stack.opp_num = g_stack.min_oppidx + 1;
	g_stack.signed_table = g_stack_default_opp_table;

	g_stack.working_table = kcalloc(g_stack.opp_num,
		sizeof(struct gpufreq_opp_info), GFP_KERNEL);
	if (!g_stack.working_table) {
		__gpufreq_abort("fail to alloc g_stack.working_table (%dB)",
			g_stack.opp_num * sizeof(struct gpufreq_opp_info));
		return;
	}
	GPUFREQ_LOGD("num of signed STACK OPP: %d, segment bound: [%d, %d]",
		g_stack.signed_opp_num, g_stack.segment_upbound, g_stack.segment_lowbound);
	GPUFREQ_LOGI("num of working STACK OPP: %d", g_stack.opp_num);

	/* update signed OPP table from MFGSYS features */

	/* compute Aging table based on Aging Sensor */
	__gpufreq_compute_aging();
	/* compute AVS table based on EFUSE */
	__gpufreq_compute_avs();
	/* apply Segment/Aging/AVS/... adjustment to signed OPP table  */
	__gpufreq_apply_adjustment();

	/* after these, GPU/STACK signed table are settled down */

	/* init working table, based on signed table */
	for (i = 0; i < g_gpu.opp_num; i++) {
		j = i + g_gpu.segment_upbound;
		g_gpu.working_table[i].freq = g_gpu.signed_table[j].freq;
		g_gpu.working_table[i].volt = g_gpu.signed_table[j].volt;
		g_gpu.working_table[i].vsram = g_gpu.signed_table[j].vsram;
		g_gpu.working_table[i].posdiv = g_gpu.signed_table[j].posdiv;
		g_gpu.working_table[i].margin = g_gpu.signed_table[j].margin;
		g_gpu.working_table[i].power = g_gpu.signed_table[j].power;

		GPUFREQ_LOGD("GPU[%02d] Freq: %d, Volt: %d, Vsram: %d, Margin: %d",
			i, g_gpu.working_table[i].freq, g_gpu.working_table[i].volt,
			g_gpu.working_table[i].vsram, g_gpu.working_table[i].margin);
	}
	for (i = 0; i < g_stack.opp_num; i++) {
		j = i + g_stack.segment_upbound;
		g_stack.working_table[i].freq = g_stack.signed_table[j].freq;
		g_stack.working_table[i].volt = g_stack.signed_table[j].volt;
		g_stack.working_table[i].vsram = g_stack.signed_table[j].vsram;
		g_stack.working_table[i].posdiv = g_stack.signed_table[j].posdiv;
		g_stack.working_table[i].margin = g_stack.signed_table[j].margin;
		g_stack.working_table[i].power = g_stack.signed_table[j].power;

		GPUFREQ_LOGD("STACK[%02d] Freq: %d, Volt: %d, Vsram: %d, Margin: %d",
			i, g_stack.working_table[i].freq, g_stack.working_table[i].volt,
			g_stack.working_table[i].vsram, g_stack.working_table[i].margin);
	}

	/* set power info to working table */
	__gpufreq_measure_power();

	/* update DVFS constraint */
	__gpufreq_update_springboard();
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

static void __gpufreq_init_segment_id(void)
{
	unsigned int efuse_id = 0x0;
	unsigned int segment_id = 0;

	switch (efuse_id) {
	default:
		segment_id = MT6985_SEGMENT;
		break;
	}

	GPUFREQ_LOGI("efuse_id: 0x%x, segment_id: %d", efuse_id, segment_id);

	g_gpu.segment_id = segment_id;
	g_stack.segment_id = segment_id;
}

static int __gpufreq_init_mtcmos(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int ret = GPUFREQ_SUCCESS;

#if !GPUFREQ_SELF_CTRL_MTCMOS
	GPUFREQ_TRACE_START("pdev=0x%x", pdev);

	g_mtcmos = kzalloc(sizeof(struct gpufreq_mtcmos_info), GFP_KERNEL);
	if (!g_mtcmos) {
		__gpufreq_abort("fail to alloc g_mtcmos (%dB)",
			sizeof(struct gpufreq_mtcmos_info));
		ret = GPUFREQ_ENOMEM;
		goto done;
	}

	g_mtcmos->mfg1_dev = dev_pm_domain_attach_by_name(dev, "pd_mfg1");
	if (IS_ERR_OR_NULL(g_mtcmos->mfg1_dev)) {
		ret = g_mtcmos->mfg1_dev ? PTR_ERR(g_mtcmos->mfg1_dev) : GPUFREQ_ENODEV;
		__gpufreq_abort("fail to get mfg1_dev (%ld)", ret);
		goto done;
	}
	dev_pm_syscore_device(g_mtcmos->mfg1_dev, true);

#if !GPUFREQ_PDCA_ENABLE
	g_mtcmos->mfg2_dev = dev_pm_domain_attach_by_name(dev, "pd_mfg2");
	if (IS_ERR_OR_NULL(g_mtcmos->mfg2_dev)) {
		ret = g_mtcmos->mfg2_dev ? PTR_ERR(g_mtcmos->mfg2_dev) : GPUFREQ_ENODEV;
		__gpufreq_abort("fail to get mfg2_dev (%ld)", ret);
		goto done;
	}
	dev_pm_syscore_device(g_mtcmos->mfg2_dev, true);

	g_mtcmos->mfg3_dev = dev_pm_domain_attach_by_name(dev, "pd_mfg3");
	if (IS_ERR_OR_NULL(g_mtcmos->mfg3_dev)) {
		ret = g_mtcmos->mfg3_dev ? PTR_ERR(g_mtcmos->mfg3_dev) : GPUFREQ_ENODEV;
		__gpufreq_abort("fail to get mfg3_dev (%ld)", ret);
		goto done;
	}
	dev_pm_syscore_device(g_mtcmos->mfg3_dev, true);

	g_mtcmos->mfg4_dev = dev_pm_domain_attach_by_name(dev, "pd_mfg4");
	if (IS_ERR_OR_NULL(g_mtcmos->mfg4_dev)) {
		ret = g_mtcmos->mfg4_dev ? PTR_ERR(g_mtcmos->mfg4_dev) : GPUFREQ_ENODEV;
		__gpufreq_abort("fail to get mfg4_dev (%ld)", ret);
		goto done;
	}
	dev_pm_syscore_device(g_mtcmos->mfg4_dev, true);

	g_mtcmos->mfg5_dev = dev_pm_domain_attach_by_name(dev, "pd_mfg5");
	if (IS_ERR_OR_NULL(g_mtcmos->mfg5_dev)) {
		ret = g_mtcmos->mfg5_dev ? PTR_ERR(g_mtcmos->mfg5_dev) : GPUFREQ_ENODEV;
		__gpufreq_abort("fail to get mfg5_dev (%ld)", ret);
		goto done;
	}
	dev_pm_syscore_device(g_mtcmos->mfg5_dev, true);

	g_mtcmos->mfg6_dev = dev_pm_domain_attach_by_name(dev, "pd_mfg6");
	if (IS_ERR_OR_NULL(g_mtcmos->mfg6_dev)) {
		ret = g_mtcmos->mfg6_dev ? PTR_ERR(g_mtcmos->mfg6_dev) : GPUFREQ_ENODEV;
		__gpufreq_abort("fail to get mfg6_dev (%ld)", ret);
		goto done;
	}
	dev_pm_syscore_device(g_mtcmos->mfg6_dev, true);

	g_mtcmos->mfg7_dev = dev_pm_domain_attach_by_name(dev, "pd_mfg7");
	if (IS_ERR_OR_NULL(g_mtcmos->mfg7_dev)) {
		ret = g_mtcmos->mfg7_dev ? PTR_ERR(g_mtcmos->mfg7_dev) : GPUFREQ_ENODEV;
		__gpufreq_abort("fail to get mfg7_dev (%ld)", ret);
		goto done;
	}
	dev_pm_syscore_device(g_mtcmos->mfg7_dev, true);

	g_mtcmos->mfg8_dev = dev_pm_domain_attach_by_name(dev, "pd_mfg8");
	if (IS_ERR_OR_NULL(g_mtcmos->mfg8_dev)) {
		ret = g_mtcmos->mfg8_dev ? PTR_ERR(g_mtcmos->mfg8_dev) : GPUFREQ_ENODEV;
		__gpufreq_abort("fail to get mfg8_dev (%ld)", ret);
		goto done;
	}
	dev_pm_syscore_device(g_mtcmos->mfg8_dev, true);

	g_mtcmos->mfg9_dev = dev_pm_domain_attach_by_name(dev, "pd_mfg9");
	if (IS_ERR_OR_NULL(g_mtcmos->mfg9_dev)) {
		ret = g_mtcmos->mfg9_dev ? PTR_ERR(g_mtcmos->mfg9_dev) : GPUFREQ_ENODEV;
		__gpufreq_abort("fail to get mfg9_dev (%ld)", ret);
		goto done;
	}
	dev_pm_syscore_device(g_mtcmos->mfg9_dev, true);

	g_mtcmos->mfg10_dev = dev_pm_domain_attach_by_name(dev, "pd_mfg10");
	if (IS_ERR_OR_NULL(g_mtcmos->mfg10_dev)) {
		ret = g_mtcmos->mfg10_dev ? PTR_ERR(g_mtcmos->mfg10_dev) : GPUFREQ_ENODEV;
		__gpufreq_abort("fail to get mfg10_dev (%ld)", ret);
		goto done;
	}
	dev_pm_syscore_device(g_mtcmos->mfg10_dev, true);

	g_mtcmos->mfg11_dev = dev_pm_domain_attach_by_name(dev, "pd_mfg11");
	if (IS_ERR_OR_NULL(g_mtcmos->mfg11_dev)) {
		ret = g_mtcmos->mfg11_dev ? PTR_ERR(g_mtcmos->mfg11_dev) : GPUFREQ_ENODEV;
		__gpufreq_abort("fail to get mfg11_dev (%ld)", ret);
		goto done;
	}
	dev_pm_syscore_device(g_mtcmos->mfg11_dev, true);

	g_mtcmos->mfg12_dev = dev_pm_domain_attach_by_name(dev, "pd_mfg12");
	if (IS_ERR_OR_NULL(g_mtcmos->mfg12_dev)) {
		ret = g_mtcmos->mfg12_dev ? PTR_ERR(g_mtcmos->mfg12_dev) : GPUFREQ_ENODEV;
		__gpufreq_abort("fail to get mfg12_dev (%ld)", ret);
		goto done;
	}
	dev_pm_syscore_device(g_mtcmos->mfg12_dev, true);

	g_mtcmos->mfg13_dev = dev_pm_domain_attach_by_name(dev, "pd_mfg13");
	if (IS_ERR_OR_NULL(g_mtcmos->mfg13_dev)) {
		ret = g_mtcmos->mfg13_dev ? PTR_ERR(g_mtcmos->mfg13_dev) : GPUFREQ_ENODEV;
		__gpufreq_abort("fail to get mfg13_dev (%ld)", ret);
		goto done;
	}
	dev_pm_syscore_device(g_mtcmos->mfg13_dev, true);

	g_mtcmos->mfg14_dev = dev_pm_domain_attach_by_name(dev, "pd_mfg14");
	if (IS_ERR_OR_NULL(g_mtcmos->mfg14_dev)) {
		ret = g_mtcmos->mfg14_dev ? PTR_ERR(g_mtcmos->mfg14_dev) : GPUFREQ_ENODEV;
		__gpufreq_abort("fail to get mfg14_dev (%ld)", ret);
		goto done;
	}
	dev_pm_syscore_device(g_mtcmos->mfg14_dev, true);

	g_mtcmos->mfg15_dev = dev_pm_domain_attach_by_name(dev, "pd_mfg15");
	if (IS_ERR_OR_NULL(g_mtcmos->mfg15_dev)) {
		ret = g_mtcmos->mfg15_dev ? PTR_ERR(g_mtcmos->mfg15_dev) : GPUFREQ_ENODEV;
		__gpufreq_abort("fail to get mfg15_dev (%ld)", ret);
		goto done;
	}
	dev_pm_syscore_device(g_mtcmos->mfg15_dev, true);

	g_mtcmos->mfg16_dev = dev_pm_domain_attach_by_name(dev, "pd_mfg16");
	if (IS_ERR_OR_NULL(g_mtcmos->mfg16_dev)) {
		ret = g_mtcmos->mfg16_dev ? PTR_ERR(g_mtcmos->mfg16_dev) : GPUFREQ_ENODEV;
		__gpufreq_abort("fail to get mfg16_dev (%ld)", ret);
		goto done;
	}
	dev_pm_syscore_device(g_mtcmos->mfg16_dev, true);

	g_mtcmos->mfg17_dev = dev_pm_domain_attach_by_name(dev, "pd_mfg17");
	if (IS_ERR_OR_NULL(g_mtcmos->mfg17_dev)) {
		ret = g_mtcmos->mfg17_dev ? PTR_ERR(g_mtcmos->mfg17_dev) : GPUFREQ_ENODEV;
		__gpufreq_abort("fail to get mfg17_dev (%ld)", ret);
		goto done;
	}
	dev_pm_syscore_device(g_mtcmos->mfg17_dev, true);

	g_mtcmos->mfg18_dev = dev_pm_domain_attach_by_name(dev, "pd_mfg18");
	if (IS_ERR_OR_NULL(g_mtcmos->mfg18_dev)) {
		ret = g_mtcmos->mfg18_dev ? PTR_ERR(g_mtcmos->mfg18_dev) : GPUFREQ_ENODEV;
		__gpufreq_abort("fail to get mfg18_dev (%ld)", ret);
		goto done;
	}
	dev_pm_syscore_device(g_mtcmos->mfg18_dev, true);

	g_mtcmos->mfg19_dev = dev_pm_domain_attach_by_name(dev, "pd_mfg19");
	if (IS_ERR_OR_NULL(g_mtcmos->mfg19_dev)) {
		ret = g_mtcmos->mfg19_dev ? PTR_ERR(g_mtcmos->mfg19_dev) : GPUFREQ_ENODEV;
		__gpufreq_abort("fail to get mfg19_dev (%ld)", ret);
		goto done;
	}
	dev_pm_syscore_device(g_mtcmos->mfg19_dev, true);
#endif /* GPUFREQ_PDCA_ENABLE */

done:
	GPUFREQ_TRACE_END();
#endif /* GPUFREQ_SELF_CTRL_MTCMOS */

	return ret;
}

static int __gpufreq_init_clk(struct platform_device *pdev)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("pdev=0x%x", pdev);

	g_clk = kzalloc(sizeof(struct gpufreq_clk_info), GFP_KERNEL);
	if (!g_clk) {
		__gpufreq_abort("fail to alloc g_clk (%dB)",
			sizeof(struct gpufreq_clk_info));
		ret = GPUFREQ_ENOMEM;
		goto done;
	}

	g_clk->clk_mux = devm_clk_get(&pdev->dev, "clk_mux");
	if (IS_ERR(g_clk->clk_mux)) {
		ret = PTR_ERR(g_clk->clk_mux);
		__gpufreq_abort("fail to get clk_mux (%ld)", ret);
		goto done;
	}

	g_clk->clk_main_parent = devm_clk_get(&pdev->dev, "clk_main_parent");
	if (IS_ERR(g_clk->clk_main_parent)) {
		ret = PTR_ERR(g_clk->clk_main_parent);
		__gpufreq_abort("fail to get clk_main_parent (%ld)", ret);
		goto done;
	}

	g_clk->clk_sub_parent = devm_clk_get(&pdev->dev, "clk_sub_parent");
	if (IS_ERR(g_clk->clk_sub_parent)) {
		ret = PTR_ERR(g_clk->clk_sub_parent);
		__gpufreq_abort("fail to get clk_sub_parent (%ld)", ret);
		goto done;
	}

	g_clk->clk_sc_mux = devm_clk_get(&pdev->dev, "clk_sc_mux");
	if (IS_ERR(g_clk->clk_sc_mux)) {
		ret = PTR_ERR(g_clk->clk_sc_mux);
		__gpufreq_abort("fail to get clk_sc_mux (%ld)", ret);
		goto done;
	}

	g_clk->clk_sc_main_parent = devm_clk_get(&pdev->dev, "clk_sc_main_parent");
	if (IS_ERR(g_clk->clk_sc_main_parent)) {
		ret = PTR_ERR(g_clk->clk_sc_main_parent);
		__gpufreq_abort("fail to get clk_sc_main_parent (%ld)", ret);
		goto done;
	}

	g_clk->clk_sc_sub_parent = devm_clk_get(&pdev->dev, "clk_sc_sub_parent");
	if (IS_ERR(g_clk->clk_sc_sub_parent)) {
		ret = PTR_ERR(g_clk->clk_sc_sub_parent);
		__gpufreq_abort("fail to get clk_sc_sub_parent (%ld)", ret);
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
		__gpufreq_abort("fail to alloc g_pmic (%dB)",
			sizeof(struct gpufreq_pmic_info));
		ret = GPUFREQ_ENOMEM;
		goto done;
	}

	g_pmic->reg_vgpu = regulator_get_optional(&pdev->dev, "vgpu");
	if (IS_ERR(g_pmic->reg_vgpu)) {
		ret = PTR_ERR(g_pmic->reg_vgpu);
		__gpufreq_abort("fail to get VGPU (%ld)", ret);
		goto done;
	}

	g_pmic->reg_vstack = regulator_get_optional(&pdev->dev, "vstack");
	if (IS_ERR(g_pmic->reg_vstack)) {
		ret = PTR_ERR(g_pmic->reg_vstack);
		__gpufreq_abort("fail to get VSATCK (%ld)", ret);
		goto done;
	}

	g_pmic->reg_vsram = regulator_get_optional(&pdev->dev, "vsram");
	if (IS_ERR(g_pmic->reg_vsram)) {
		ret = PTR_ERR(g_pmic->reg_vsram);
		__gpufreq_abort("fail to get VSRAM (%ld)", ret);
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

	if (unlikely(!gpufreq_dev)) {
		GPUFREQ_LOGE("fail to find gpufreq device (ENOENT)");
		goto done;
	}

	of_wrapper = of_find_compatible_node(NULL, NULL, "mediatek,gpufreq_wrapper");
	if (unlikely(!of_wrapper)) {
		GPUFREQ_LOGE("fail to find gpufreq_wrapper of_node");
		goto done;
	}

	/* feature config */
	g_asensor_enable = GPUFREQ_ASENSOR_ENABLE;
	g_avs_enable = GPUFREQ_AVS_ENABLE;
	g_gpm1_mode = GPUFREQ_GPM1_ENABLE;
	g_gpm3_mode = GPUFREQ_GPM3_ENABLE;
	g_dfd_mode = GPUFREQ_DFD_ENABLE;
	/* ignore return error and use default value if property doesn't exist */
	of_property_read_u32(gpufreq_dev->of_node, "aging-load", &g_aging_load);
	of_property_read_u32(gpufreq_dev->of_node, "mcl50-load", &g_mcl50_load);
	of_property_read_u32(of_wrapper, "gpueb-support", &g_gpueb_support);

	/* 0x13000000 */
	g_mali_base = __gpufreq_of_ioremap("mediatek,mali", 0);
	if (unlikely(!g_mali_base)) {
		GPUFREQ_LOGE("fail to ioremap MALI");
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
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "nth_emicfg");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource NTH_EMICFG");
		goto done;
	}
	g_nth_emicfg_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_nth_emicfg_base)) {
		GPUFREQ_LOGE("fail to ioremap NTH_EMICFG: 0x%llx", res->start);
		goto done;
	}

	/* 0x1021E000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "sth_emicfg");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource STH_EMICFG");
		goto done;
	}
	g_sth_emicfg_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_sth_emicfg_base)) {
		GPUFREQ_LOGE("fail to ioremap STH_EMICFG: 0x%llx", res->start);
		goto done;
	}

	/* 0x10270000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "nth_emicfg_ao_mem");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource NTH_EMICFG_AO_MEM");
		goto done;
	}
	g_nth_emicfg_ao_mem_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_nth_emicfg_ao_mem_base)) {
		GPUFREQ_LOGE("fail to ioremap NTH_EMICFG_AO_MEM: 0x%llx", res->start);
		goto done;
	}

	/* 0x1030E000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "sth_emicfg_ao_mem");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource STH_EMICFG_AO_MEM");
		goto done;
	}
	g_sth_emicfg_ao_mem_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_sth_emicfg_ao_mem_base)) {
		GPUFREQ_LOGE("fail to ioremap STH_EMICFG_AO_MEM: 0x%llx", res->start);
		goto done;
	}

	/* 0x1002C000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ifrbus_ao");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource IFRBUS_AO");
		goto done;
	}
	g_ifrbus_ao_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_ifrbus_ao_base)) {
		GPUFREQ_LOGE("fail to ioremap IFRBUS_AO: 0x%llx", res->start);
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

	/* 0x10028000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "sth_emi_ao_debug_ctrl");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource STH_EMI_AO_DEBUG_CTRL");
		goto done;
	}
	g_sth_emi_ao_debug_ctrl = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_sth_emi_ao_debug_ctrl)) {
		GPUFREQ_LOGE("fail to ioremap STH_EMI_AO_DEBUG_CTRL: 0x%llx", res->start);
		goto done;
	}

	/* 0x11E80000 */
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

	/* 0x13FB9C00 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_cpe_ctrl_mcu");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource MFG_CPE_CTRL_MCU");
		goto done;
	}
	g_mfg_cpe_ctrl_mcu_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_mfg_cpe_ctrl_mcu_base)) {
		GPUFREQ_LOGE("fail to ioremap MFG_CPE_CTRL_MCU: 0x%llx", res->start);
		goto done;
	}

	/* 0x13FB6000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_cpe_sensor");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource MFG_CPE_SENSOR0");
		goto done;
	}
	g_mfg_cpe_sensor_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_mfg_cpe_sensor_base)) {
		GPUFREQ_LOGE("fail to ioremap MFG_CPE_SENSOR0: 0x%llx", res->start);
		goto done;
	}

	/* 0x13FBC000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_secure");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource MFG_SECURE");
		goto done;
	}
	g_mfg_secure_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_mfg_secure_base)) {
		GPUFREQ_LOGE("fail to ioremap MFG_SECURE: 0x%llx", res->start);
		goto done;
	}

	/* 0x1000D000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "drm_debug");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource DRM_DEBUG");
		goto done;
	}
	g_drm_debug_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_drm_debug_base)) {
		GPUFREQ_LOGE("fail to ioremap DRM_DEBUG: 0x%llx", res->start);
		goto done;
	}

	ret = GPUFREQ_SUCCESS;

done:
	return ret;
}

/* API: skip gpufreq driver probe if in bringup state */
static unsigned int __gpufreq_bringup(void)
{
	struct device_node *of_wrapper = NULL;
	unsigned int bringup_state = false;

	of_wrapper = of_find_compatible_node(NULL, NULL, "mediatek,gpufreq_wrapper");
	if (unlikely(!of_wrapper)) {
		GPUFREQ_LOGE("fail to find gpufreq_wrapper of_node, treat as bringup");
		return true;
	}

	/* check bringup state by dts */
	of_property_read_u32(of_wrapper, "gpufreq-bringup", &bringup_state);

	return bringup_state;
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

	/* init footprint */
	__gpufreq_reset_footprint();

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
		GPUFREQ_LOGI("gpufreq platform probe only init reg/pmic/fp in EB mode");
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
	__gpufreq_init_segment_id();

	/* init shader present */
	__gpufreq_init_shader_present();

	/* power on to init first OPP index */
	ret = __gpufreq_power_control(POWER_ON);
	if (unlikely(ret < 0)) {
		GPUFREQ_LOGE("fail to control power state: %d (%d)", POWER_ON, ret);
		goto done;
	}

	/* init OPP table */
	__gpufreq_init_opp_table();

	/* init first OPP index by current freq and volt */
	ret = __gpufreq_init_opp_idx();
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to init OPP index (%d)", ret);
		goto done;
	}

	/* power off after init first OPP index */
	if (__gpufreq_power_ctrl_enable())
		__gpufreq_power_control(POWER_OFF);
	/* never power off if power control is disabled */
	else
		GPUFREQ_LOGI("power control always on");

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
	ret = gpuppm_init(TARGET_STACK, g_gpueb_support, GPUPPM_DEFAULT_IDX);
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to init gpuppm (%d)", ret);
		goto done;
	}

	g_gpufreq_ready = true;
	GPUFREQ_LOGI("gpufreq platform driver probe done");

done:
	return ret;
}

/* API: gpufreq driver remove */
static int __gpufreq_pdrv_remove(struct platform_device *pdev)
{
#if !GPUFREQ_SELF_CTRL_MTCMOS
#if !GPUFREQ_PDCA_ENABLE
	dev_pm_domain_detach(g_mtcmos->mfg19_dev, true);
	dev_pm_domain_detach(g_mtcmos->mfg18_dev, true);
	dev_pm_domain_detach(g_mtcmos->mfg17_dev, true);
	dev_pm_domain_detach(g_mtcmos->mfg16_dev, true);
	dev_pm_domain_detach(g_mtcmos->mfg15_dev, true);
	dev_pm_domain_detach(g_mtcmos->mfg14_dev, true);
	dev_pm_domain_detach(g_mtcmos->mfg13_dev, true);
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
#endif /* GPUFREQ_PDCA_ENABLE */
	dev_pm_domain_detach(g_mtcmos->mfg1_dev, true);
#endif /* GPUFREQ_SELF_CTRL_MTCMOS */

	kfree(g_gpu.working_table);
	kfree(g_stack.working_table);
	kfree(g_clk);
	kfree(g_pmic);
#if !GPUFREQ_SELF_CTRL_MTCMOS
	kfree(g_mtcmos);
#endif /* GPUFREQ_SELF_CTRL_MTCMOS */

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
