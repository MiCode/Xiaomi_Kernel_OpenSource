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
#include <gpufreq_mt6886.h>
#include <gpufreq_reg_mt6886.h>
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
static void __gpufreq_fake_mtcmos_control(unsigned int mode);
static void __gpufreq_set_ocl_timestamp(void);
static void __gpufreq_set_margin_mode(unsigned int val);
static void __gpufreq_set_gpm_mode(unsigned int version, unsigned int val);
static void __gpufreq_apply_restore_margin(unsigned int val);
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
/* dvfs function */
static int __gpufreq_generic_scale_gpu(
	unsigned int freq_old, unsigned int freq_new,
	unsigned int volt_old, unsigned int volt_new,
	unsigned int vsram_old, unsigned int vsram_new);
static int __gpufreq_custom_commit_gpu(unsigned int target_freq,
	unsigned int target_volt, enum gpufreq_dvfs_state key);
static int __gpufreq_freq_scale_gpu(unsigned int freq_old, unsigned int freq_new);
static int __gpufreq_freq_scale_stack(unsigned int freq_old, unsigned int freq_new);
static int __gpufreq_volt_scale_gpu(
	unsigned int vgpu_old, unsigned int vgpu_new,
	unsigned int vsram_old, unsigned int vsram_new);
static void __gpufreq_switch_clksrc(enum gpufreq_target target, enum gpufreq_clk_src clksrc);
static void __gpufreq_switch_parksrc(enum gpufreq_target target, enum gpufreq_clk_src clksrc);
static unsigned int __gpufreq_calculate_pcw(unsigned int freq, enum gpufreq_posdiv posdiv_power);
static unsigned int __gpufreq_settle_time_vgpu(enum gpufreq_opp_direct direct, int deltaV);
static unsigned int __gpufreq_settle_time_vsram(enum gpufreq_opp_direct direct, int deltaV);
/* get function */
static unsigned int __gpufreq_get_fmeter_fgpu(void);
static unsigned int __gpufreq_get_fmeter_fstack(void);
static unsigned int __gpufreq_get_fmeter_main_fgpu(void);
static unsigned int __gpufreq_get_fmeter_main_fstack(void);
static unsigned int __gpufreq_get_fmeter_sub(void);
static unsigned int __gpufreq_get_real_fgpu(void);
static unsigned int __gpufreq_get_real_fstack(void);
static unsigned int __gpufreq_get_real_vgpu(void);
static unsigned int __gpufreq_get_real_vsram(void);
static unsigned int __gpufreq_get_vsram_by_vlogic(unsigned int volt);
static enum gpufreq_posdiv __gpufreq_get_real_posdiv_gpu(void);
static enum gpufreq_posdiv __gpufreq_get_real_posdiv_stack(void);
static enum gpufreq_posdiv __gpufreq_get_posdiv_by_fgpu(unsigned int freq);
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
static void __gpufreq_check_bus_idle(void);
static void __gpufreq_aoc_config(enum gpufreq_power_state power);
static void __gpufreq_hwdcm_config(void);
static void __gpufreq_acp_config(void);
static void __gpufreq_gpm1_config(void);
static void __gpufreq_transaction_config(void);
static void __gpufreq_dfd_config(void);
/* bringup function */
static unsigned int __gpufreq_bringup(void);
static void __gpufreq_dump_bringup_status(struct platform_device *pdev);
/* init function */
static void __gpufreq_interpolate_volt(void);
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
static void __iomem *g_sensor_pll_base;
static void __iomem *g_mfgsc_pll_base;
static void __iomem *g_mfg_rpc_base;
static void __iomem *g_sleep;
static void __iomem *g_nth_emicfg_base;
static void __iomem *g_nth_emicfg_ao_mem_base;
static void __iomem *g_infracfg_ao_base;
static void __iomem *g_infra_ao_debug_ctrl;
static void __iomem *g_infra_ao1_debug_ctrl;
static void __iomem *g_nth_emi_ao_debug_ctrl;
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
static unsigned int g_shader_present;
static unsigned int g_mcl50_load;
static unsigned int g_aging_table_idx;
static unsigned int g_asensor_enable;
static unsigned int g_aging_load;
static unsigned int g_aging_margin;
static unsigned int g_avs_enable;
static unsigned int g_avs_margin;
static unsigned int g_gpm1_mode;
static unsigned int g_ptp_version;
static unsigned int g_dfd_mode;
static unsigned int g_gpueb_support;
static unsigned int g_gpufreq_ready;
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
	.generic_commit_gpu = __gpufreq_generic_commit_gpu,
	.fix_target_oppidx_gpu = __gpufreq_fix_target_oppidx_gpu,
	.fix_custom_freq_volt_gpu = __gpufreq_fix_custom_freq_volt_gpu,
	.dump_infra_status = __gpufreq_dump_infra_status,
	.set_mfgsys_config = __gpufreq_set_mfgsys_config,
	.get_core_mask_table = __gpufreq_get_core_mask_table,
	.get_core_num = __gpufreq_get_core_num,
	.pdca_config = __gpufreq_pdca_config,
	.update_debug_opp_info = __gpufreq_update_debug_opp_info,
	.set_shared_status = __gpufreq_set_shared_status,
	.mssv_commit = __gpufreq_mssv_commit,
};

static struct gpufreq_platform_fp platform_eb_fp = {
	.dump_infra_status = __gpufreq_dump_infra_status,
	.get_dyn_pgpu = __gpufreq_get_dyn_pgpu,
	.get_core_mask_table = __gpufreq_get_core_mask_table,
	.get_core_num = __gpufreq_get_core_num,
};

/**
 * ===============================================
 * External Function Definition
 * ===============================================
 */
/* API: get POWER_CTRL status */
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
	return -1;
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

/* API: get pointer of working OPP table of GPU */
const struct gpufreq_opp_info *__gpufreq_get_working_table_gpu(void)
{
	return g_gpu.working_table;
}

/* API: get pointer of working OPP table of STACK */
const struct gpufreq_opp_info *__gpufreq_get_working_table_stack(void)
{
	return NULL;
}

/* API: get pointer of signed OPP table of GPU */
const struct gpufreq_opp_info *__gpufreq_get_signed_table_gpu(void)
{
	return g_gpu.signed_table;
}

/* API: get pointer of signed OPP table of STACK */
const struct gpufreq_opp_info *__gpufreq_get_signed_table_stack(void)
{
	return NULL;
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

	return -1;
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

/* API: get leakage Power of GPU */
unsigned int __gpufreq_get_lkg_pgpu(unsigned int volt)
{
	GPUFREQ_UNREFERENCED(volt);

	return GPU_LEAKAGE_POWER;
}

/* API: get dynamic Power of GPU */
unsigned int __gpufreq_get_dyn_pgpu(unsigned int freq, unsigned int volt)
{
	unsigned long long p_dynamic = GPU_ACT_REF_POWER;
	unsigned int ref_freq = GPU_ACT_REF_FREQ;
	unsigned int ref_volt = GPU_ACT_REF_VOLT;

	p_dynamic = p_dynamic *
		((freq * 100) / ref_freq) *
		((volt * 100) / ref_volt) *
		((volt * 100) / ref_volt) /
		(100 * 100 * 100);

	return (unsigned int)p_dynamic;
}

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
	u64 power_time = 0;

	GPUFREQ_TRACE_START("power=%d", power);

	mutex_lock(&gpufreq_lock);

	GPUFREQ_LOGD("+ PWR_STATUS: 0x%08x", MFG_0_12_PWR_STATUS);
	GPUFREQ_LOGD("switch power: %s (Power: %d, Active: %d, Buck: %d, MTCMOS: %d, CG: %d)",
		power ? "On" : "Off", g_gpu.power_count,
		g_gpu.active_count, g_gpu.buck_count,
		g_gpu.mtcmos_count, g_gpu.cg_count);

	if (power == POWER_ON) {
		g_gpu.power_count++;
	} else {
		g_gpu.power_count--;
	}
	__gpufreq_footprint_power_count(g_gpu.power_count);

	if (power == POWER_ON && g_gpu.power_count == 1) {
		__gpufreq_footprint_power_step(0x01);

		/* enable Buck */
		ret = __gpufreq_buck_control(POWER_ON);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to Buck: On (%d)", ret);
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
			GPUFREQ_LOGE("fail to MTCMOS: On (%d)", ret);
			ret = GPUFREQ_EINVAL;
			goto done_unlock;
		}
		__gpufreq_footprint_power_step(0x04);

		/* enable clock */
		ret = __gpufreq_clock_control(POWER_ON);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to CLOCK: On (%d)", ret);
			ret = GPUFREQ_EINVAL;
			goto done_unlock;
		}
		__gpufreq_footprint_power_step(0x05);

		/* set PDCv2 register when power on, let GPU DDK control MTCMOS itself */
		__gpufreq_pdca_config(POWER_ON);
		__gpufreq_footprint_power_step(0x06);

		/* config ACP */
		__gpufreq_acp_config();
		__gpufreq_footprint_power_step(0x07);

		/* config HWDCM */
		__gpufreq_hwdcm_config();
		__gpufreq_footprint_power_step(0x08);

		/* config GPM 1.0 */
		if (g_gpm1_mode)
			__gpufreq_gpm1_config();
		__gpufreq_footprint_power_step(0x09);

		/* config AXI transaction */
		__gpufreq_transaction_config();
		__gpufreq_footprint_power_step(0x0A);

		/* config DFD */
		__gpufreq_dfd_config();
		__gpufreq_footprint_power_step(0x0B);

		/* free DVFS when power on */
		g_dvfs_state &= ~DVFS_POWEROFF;

		__gpufreq_footprint_power_step(0x0C);
	} else if (power == POWER_OFF && g_gpu.power_count == 0) {
		__gpufreq_footprint_power_step(0x0D);

		/* check all transaction complete before power off */
		__gpufreq_check_bus_idle();
		__gpufreq_footprint_power_step(0x0E);

		/* freeze DVFS when power off */
		g_dvfs_state |= DVFS_POWEROFF;
		__gpufreq_footprint_power_step(0x0F);

		/* disable clock */
		ret = __gpufreq_clock_control(POWER_OFF);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to control CLOCK: Off (%d)", ret);
			ret = GPUFREQ_EINVAL;
			goto done_unlock;
		}
		__gpufreq_footprint_power_step(0x10);

		/* disable MTCMOS */
		ret = __gpufreq_mtcmos_control(POWER_OFF);
		if (unlikely(ret < 0)) {
			GPUFREQ_LOGE("fail to MTCMOS: Off (%d)", ret);
			ret = GPUFREQ_EINVAL;
			goto done_unlock;
		}
		__gpufreq_footprint_power_step(0x11);

		/* set AOC ISO/LATCH before Buck off */
		__gpufreq_aoc_config(POWER_OFF);
		__gpufreq_footprint_power_step(0x12);

		/* disable Buck */
		ret = __gpufreq_buck_control(POWER_OFF);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to Buck: Off (%d)", ret);
			ret = GPUFREQ_EINVAL;
			goto done_unlock;
		}
		__gpufreq_footprint_power_step(0x13);
	}

	/* return power count if successfully control power */
	ret = g_gpu.power_count;
	/* record time of successful power control */
	power_time = ktime_get_ns();

	/* update current status to shared memory */
	if (g_shared_status) {
		g_shared_status->dvfs_state = g_dvfs_state;
		g_shared_status->power_count = g_gpu.power_count;
		g_shared_status->buck_count = g_gpu.buck_count;
		g_shared_status->mtcmos_count = g_gpu.mtcmos_count;
		g_shared_status->power_time_h = (power_time >> 32) & GENMASK(31, 0);
		g_shared_status->power_time_l = power_time & GENMASK(31, 0);
		if (power == POWER_ON)
			__gpufreq_update_shared_status_power_reg();
	}

	if (power == POWER_ON)
		__gpufreq_footprint_power_step(0x14);
	else if (power == POWER_OFF)
		__gpufreq_footprint_power_step(0x15);

done_unlock:
	GPUFREQ_LOGD("- PWR_STATUS: 0x%08x", MFG_0_12_PWR_STATUS);

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
		power ? "Active" : "Idle", g_gpu.active_count);

	/* active-idle control is only available at power-on */
	if (__gpufreq_get_power_state() == POWER_OFF)
		__gpufreq_abort("switch active-idle at power-off (%d)", g_gpu.power_count);

	if (power == POWER_ON)
		g_gpu.active_count++;
	else
		g_gpu.active_count--;

	if (power == POWER_ON && g_gpu.active_count == 1) {
		/* switch GPU MUX to PLL */
		__gpufreq_switch_clksrc(TARGET_GPU, CLOCK_MAIN);
		/* switch STACK MUX to PLL */
		__gpufreq_switch_clksrc(TARGET_STACK, CLOCK_MAIN);
		/* free DVFS when active */
		g_dvfs_state &= ~DVFS_SLEEP;
	} else if (power == POWER_OFF && g_gpu.active_count == 0) {
		/* freeze DVFS when idle */
		g_dvfs_state |= DVFS_SLEEP;
		/* switch STACK MUX to REF_SEL */
		__gpufreq_switch_clksrc(TARGET_STACK, CLOCK_SUB);
		/* switch GPU MUX to REF_SEL */
		__gpufreq_switch_clksrc(TARGET_GPU, CLOCK_SUB);
	} else if (g_gpu.active_count < 0)
		__gpufreq_abort("incorrect active count: %d", g_gpu.active_count);

	/* return active count if successfully control runtime state */
	ret = g_gpu.active_count;

	/* update current status to shared memory */
	if (g_shared_status) {
		g_shared_status->dvfs_state = g_dvfs_state;
		g_shared_status->active_count = g_gpu.active_count;
		__gpufreq_update_shared_status_active_idle_reg();
	}

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
	int ret = GPUFREQ_SUCCESS;
	int cur_oppidx = 0, opp_num = g_gpu.opp_num;
	/* GPU */
	struct gpufreq_opp_info *working_table = g_gpu.working_table;
	unsigned int cur_fgpu = 0, cur_vgpu = 0, target_fgpu = 0, target_vgpu = 0;
	/* SRAM */
	unsigned int cur_vsram = 0, target_vsram = 0;

	GPUFREQ_TRACE_START("target_oppidx=%d, key=%d", target_oppidx, key);

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
		GPUFREQ_LOGD("unavailable DVFS state (0x%x)", g_dvfs_state);
		/* still update Volt when DVFS is fixed by fix OPP cmd */
		if (g_dvfs_state == DVFS_FIX_OPP)
			target_oppidx = g_gpu.cur_oppidx;
		/* otherwise skip */
		else {
			ret = GPUFREQ_SUCCESS;
			goto done_unlock;
		}
	}

	/* prepare GPU setting */
	cur_oppidx = g_gpu.cur_oppidx;
	cur_fgpu = g_gpu.cur_freq;
	cur_vgpu = g_gpu.cur_volt;
	cur_vsram = g_gpu.cur_vsram;

	target_fgpu = working_table[target_oppidx].freq;
	target_vgpu = working_table[target_oppidx].volt;
	target_vsram = working_table[target_oppidx].vsram;

	GPUFREQ_LOGD("begin to commit GPU OPP index: (%d->%d)",
		cur_oppidx, target_oppidx);

	ret = __gpufreq_generic_scale_gpu(cur_fgpu, target_fgpu,
		cur_vgpu, target_vgpu, cur_vsram, target_vsram);
	if (unlikely(ret)) {
		GPUFREQ_LOGE(
			"fail to scale GPU: Idx(%d->%d), Freq(%d->%d), Volt(%d->%d), Vsram(%d->%d)",
			cur_oppidx, target_oppidx, cur_fgpu, target_fgpu,
			cur_vgpu, target_vgpu, cur_vsram, target_vsram);
		goto done_unlock;
	}

	g_gpu.cur_oppidx = target_oppidx;

	__gpufreq_footprint_oppidx(target_oppidx);

	/* update current status to shared memory */
	if (g_shared_status) {
		g_shared_status->cur_oppidx_gpu = g_gpu.cur_oppidx;
		g_shared_status->cur_fgpu = g_gpu.cur_freq;
		g_shared_status->cur_vgpu = g_gpu.cur_volt;
		g_shared_status->cur_vsram_gpu = g_gpu.cur_vsram;
		g_shared_status->cur_power_gpu = g_gpu.working_table[g_gpu.cur_oppidx].power;
		g_shared_status->cur_fstack = g_stack.cur_freq;
		__gpufreq_update_shared_status_dvfs_reg();
	}

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
		__gpufreq_set_dvfs_state(false, DVFS_FIX_OPP);
		ret = GPUFREQ_SUCCESS;
	} else if (oppidx >= 0 && oppidx < opp_num) {
		__gpufreq_set_dvfs_state(true, DVFS_FIX_OPP);

		ret = __gpufreq_generic_commit_gpu(oppidx, DVFS_FIX_OPP);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to commit GPU OPP index: %d (%d)", oppidx, ret);
			__gpufreq_set_dvfs_state(false, DVFS_FIX_OPP);
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
	unsigned int min_oppidx = g_gpu.min_oppidx;
	unsigned int max_freq = 0, min_freq = 0;
	unsigned int max_volt = 0, min_volt = 0;
	int ret = GPUFREQ_SUCCESS;

	ret = __gpufreq_power_control(POWER_ON);
	if (unlikely(ret < 0)) {
		GPUFREQ_LOGE("fail to control power state: %d (%d)", POWER_ON, ret);
		goto done;
	}

	max_freq = POSDIV_2_MAX_FREQ;
	min_freq = POSDIV_8_MIN_FREQ;
	max_volt = VGPU_MAX_VOLT;
	min_volt = VGPU_MIN_VOLT;

	if (freq == 0 && volt == 0) {
		__gpufreq_set_dvfs_state(false, DVFS_FIX_FREQ_VOLT);
		ret = GPUFREQ_SUCCESS;
	} else if (freq > max_freq || freq < min_freq) {
		GPUFREQ_LOGE("invalid fixed Freq: %d", freq);
		ret = GPUFREQ_EINVAL;
	} else if (volt > max_volt || volt < min_volt) {
		GPUFREQ_LOGE("invalid fixed Volt: %d", volt);
		ret = GPUFREQ_EINVAL;
	} else {
		__gpufreq_set_dvfs_state(true, DVFS_FIX_FREQ_VOLT);

		ret = __gpufreq_custom_commit_gpu(freq, volt, DVFS_FIX_FREQ_VOLT);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to commit GPU Freq: %d, Volt: %d (%d)",
				freq, volt, ret);
			__gpufreq_set_dvfs_state(false, DVFS_FIX_FREQ_VOLT);
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

void __gpufreq_dump_infra_status(void)
{
	if (!g_gpufreq_ready)
		return;

	GPUFREQ_LOGI("== [GPUFREQ INFRA STATUS] ==");
	GPUFREQ_LOGI("[Clk] MFG_PLL: %d, MFG_SEL: 0x%x, MFGSC_PLL: %d, MFGSC_SEL: 0x%x",
		__gpufreq_get_real_fgpu(), readl(MFG_RPC_AO_CLK_CFG) & BIT(CKMUX_SEL_CORE),
		__gpufreq_get_real_fstack(), readl(MFG_RPC_AO_CLK_CFG) & BIT(CKMUX_SEL_STACK));

	/* MFG_QCHANNEL_CON 0x13FBF0B4 [0] MFG_ACTIVE_SEL = 1'b1 */
	writel((readl(MFG_QCHANNEL_CON) | BIT(0)), MFG_QCHANNEL_CON);
	/* MFG_DEBUG_SEL 0x13FBF170 [1:0] MFG_DEBUG_TOP_SEL = 2'b11 */
	writel((readl(MFG_DEBUG_SEL) | GENMASK(1, 0)), MFG_DEBUG_SEL);

	/* MFG_DEBUG_SEL */
	/* MFG_DEBUG_TOP */
	GPUFREQ_LOGI("%-11s (0x%x): 0x%08x, (0x%x): 0x%08x",
		"[MFG]",
		0x13FBF170, readl(MFG_DEBUG_SEL),
		0x13FBF178, readl(MFG_DEBUG_TOP));

	/* MFG_RPC_SLP_PROT_EN_SET */
	/* MFG_RPC_SLP_PROT_EN_CLR */
	/* MFG_RPC_SLP_PROT_EN_STA */
	GPUFREQ_LOGI("%-11s (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x",
		"[RPC]",
		0x13F91040, readl(MFG_RPC_SLP_PROT_EN_SET),
		0x13F91044, readl(MFG_RPC_SLP_PROT_EN_CLR),
		0x13F91048, readl(MFG_RPC_SLP_PROT_EN_STA));

	/* NTH_MFG_EMI1_GALS_SLV_DBG */
	/* NTH_MFG_EMI0_GALS_SLV_DBG */
	/* NTH_EMICFG_AO_MEM_REG_M6M7_IDLE_BIT_EN_1 */
	/* NTH_EMICFG_AO_MEM_REG_M6M7_IDLE_BIT_EN_0 */
	GPUFREQ_LOGI("%-11s (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x",
		"[EMI]",
		0x1021C82C, readl(NTH_MFG_EMI1_GALS_SLV_DBG),
		0x1021C830, readl(NTH_MFG_EMI0_GALS_SLV_DBG),
		0x10270228, readl(NTH_M6M7_IDLE_BIT_EN_1),
		0x1027022c, readl(NTH_M6M7_IDLE_BIT_EN_0));

	/* MD_MFGSYS_PROTECT_EN_STA_0 */
	/* MD_MFGSYS_PROTECT_EN_SET_0 */
	/* MD_MFGSYS_PROTECT_EN_CLR_0 */
	/* MD_MFGSYS_PROTECT_RDY_STA_0 */
	GPUFREQ_LOGI("%-11s (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x",
		"[INFRA]",
		0x10001CA0, readl(MD_MFGSYS_PROTECT_EN_STA_0),
		0x10001CA4, readl(MD_MFGSYS_PROTECT_EN_SET_0),
		0x10001CA8, readl(MD_MFGSYS_PROTECT_EN_CLR_0),
		0x10001CAC, readl(MD_MFGSYS_PROTECT_RDY_STA_0));

	/* INFRA_PAR_BUS_INFRA_AO_BUS_U_DEBUG_CTRL_AO_INFRA_AO_CTRL0 */
	/* INFRA_PAR_BUS_INFRA_QAXI_AO_BUS_SUB1_u_debug_ctrl_ao_INFRA_AO1_debug_ctrl_ao */
	/* EMI_AO_BUS_U_DEBUG_CTRL_AO_EMI_AO_CTRL0 */
	GPUFREQ_LOGI("%-11s (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x",
		"[EMI_INFRA]",
		0x10023000, readl(INFRA_AO_BUS0_U_DEBUG_CTRL0),
		0x1002B000, readl(INFRA_AO1_DEBUG_CTRL_BASE),
		0x10042000, readl(NTH_EMI_AO_DEBUG_CTRL_BASE));

	GPUFREQ_LOGI("%-11s (0x%x): 0x%08x, (0x%x): 0x%08x",
		"[SPM]",
		0x1C001410, readl(SPM_SPM2GPUPM_CON),
		0x1C001F74, readl(SPM_SOC_BUCK_ISO_CON));

	GPUFREQ_LOGI("%-11s 0x%08x, 0x%08x, 0x%08x",
		"[MFG0-2]", readl(SPM_MFG0_PWR_CON),
		readl(MFG_RPC_MFG1_PWR_CON), readl(MFG_RPC_MFG2_PWR_CON));
	GPUFREQ_LOGI("%-11s 0x%08x, 0x%08x, 0x%08x, 0x%08x",
		"[MFG9-12]", readl(MFG_RPC_MFG9_PWR_CON),
		readl(MFG_RPC_MFG10_PWR_CON), readl(MFG_RPC_MFG11_PWR_CON),
		readl(MFG_RPC_MFG12_PWR_CON));
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
		g_shared_status->cur_fmeter_fgpu = __gpufreq_get_fmeter_fgpu();
		g_shared_status->cur_fmeter_fstack = __gpufreq_get_fmeter_fstack();
		g_shared_status->cur_regulator_vgpu = __gpufreq_get_real_vgpu();
		g_shared_status->cur_regulator_vsram_gpu = __gpufreq_get_real_vsram();
	} else {
		g_shared_status->cur_con1_fgpu = 0;
		g_shared_status->cur_con1_fstack = 0;
		g_shared_status->cur_fmeter_fgpu = 0;
		g_shared_status->cur_fmeter_fstack = 0;
		g_shared_status->cur_regulator_vgpu = 0;
		g_shared_status->cur_regulator_vsram_gpu = 0;
	}
	g_shared_status->mfg_pwr_status = MFG_0_12_PWR_STATUS;

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
	case CONFIG_OCL_TIMESTAMP:
		__gpufreq_set_ocl_timestamp();
		break;
	case CONFIG_FAKE_MTCMOS_CTRL:
		__gpufreq_fake_mtcmos_control(val);
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

/* PDCv2: GPU IP automatically control GPU shader MTCMOS */
void __gpufreq_pdca_config(enum gpufreq_power_state power)
{
#if GPUFREQ_PDCA_ENABLE
	if (power == POWER_ON) {
		// CG
		/* MFG_ACTIVE_POWER_CON_CG_06 0x13FBF100 [0] rg_cg_active_pwrctl_en = 1'b1 */
		writel((readl(MFG_ACTIVE_POWER_CON_CG_06) | BIT(0)), MFG_ACTIVE_POWER_CON_CG_06);
		// SC0
		/* MFG_ACTIVE_POWER_CON_00 0x13FBF400 [0] rg_sc0_active_pwrctl_en = 1'b1 */
		writel((readl(MFG_ACTIVE_POWER_CON_00) | BIT(0)), MFG_ACTIVE_POWER_CON_00);
		/* MFG_ACTIVE_POWER_CON_01 0x13FBF404 [31] rg_sc0_active_pwrctl_rsv = 1'b1 */
		writel((readl(MFG_ACTIVE_POWER_CON_01) | BIT(31)), MFG_ACTIVE_POWER_CON_01);
		// SC1
		/* MFG_ACTIVE_POWER_CON_06 0x13FBF418 [0] rg_sc1_active_pwrctl_en = 1'b1 */
		writel((readl(MFG_ACTIVE_POWER_CON_06) | BIT(0)), MFG_ACTIVE_POWER_CON_06);
		/* MFG_ACTIVE_POWER_CON_07 0x13FBF41C [31] rg_sc1_active_pwrctl_rsv = 1'b1 */
		writel((readl(MFG_ACTIVE_POWER_CON_07) | BIT(31)), MFG_ACTIVE_POWER_CON_07);
		// SC2
		/* MFG_ACTIVE_POWER_CON_12 0x13FBF430 [0] rg_sc2_active_pwrctl_en = 1'b1 */
		writel((readl(MFG_ACTIVE_POWER_CON_12) | BIT(0)), MFG_ACTIVE_POWER_CON_12);
		/* MFG_ACTIVE_POWER_CON_13 0x13FBF434 [31] rg_sc2_active_pwrctl_rsv = 1'b1 */
		writel((readl(MFG_ACTIVE_POWER_CON_13) | BIT(31)), MFG_ACTIVE_POWER_CON_13);
		// SC3
		/* MFG_ACTIVE_POWER_CON_18 0x13FBF448 [0] rg_sc3_active_pwrctl_en = 1'b1 */
		writel((readl(MFG_ACTIVE_POWER_CON_18) | BIT(0)), MFG_ACTIVE_POWER_CON_18);
		/* MFG_ACTIVE_POWER_CON_13 0x13FBF44C [31] rg_sc2_active_pwrctl_rsv = 1'b1 */
		writel((readl(MFG_ACTIVE_POWER_CON_19) | BIT(31)), MFG_ACTIVE_POWER_CON_19);
	} else {
		// CG
		/* MFG_ACTIVE_POWER_CON_CG_06 0x13FBF100 [0] rg_cg_active_pwrctl_en = 1'b0 */
		writel((readl(MFG_ACTIVE_POWER_CON_CG_06) & ~BIT(0)), MFG_ACTIVE_POWER_CON_CG_06);
		// SC0
		/* MFG_ACTIVE_POWER_CON_00 0x13FBF400 [0] rg_sc0_active_pwrctl_en = 1'b0 */
		writel((readl(MFG_ACTIVE_POWER_CON_00) & ~BIT(0)), MFG_ACTIVE_POWER_CON_00);
		/* MFG_ACTIVE_POWER_CON_01 0x13FBF404 [31] rg_sc0_active_pwrctl_rsv = 1'b0 */
		writel((readl(MFG_ACTIVE_POWER_CON_01) & ~BIT(31)), MFG_ACTIVE_POWER_CON_01);
		// SC1
		/* MFG_ACTIVE_POWER_CON_06 0x13FBF418 [0] rg_sc1_active_pwrctl_en = 1'b0 */
		writel((readl(MFG_ACTIVE_POWER_CON_06) & ~BIT(0)), MFG_ACTIVE_POWER_CON_06);
		/* MFG_ACTIVE_POWER_CON_07 0x13FBF41C [31] rg_sc1_active_pwrctl_rsv = 1'b0 */
		writel((readl(MFG_ACTIVE_POWER_CON_07) & ~BIT(31)), MFG_ACTIVE_POWER_CON_07);
		// SC2
		/* MFG_ACTIVE_POWER_CON_12 0x13FBF430 [0] rg_sc2_active_pwrctl_en = 1'b0 */
		writel((readl(MFG_ACTIVE_POWER_CON_12) & ~BIT(0)), MFG_ACTIVE_POWER_CON_12);
		/* MFG_ACTIVE_POWER_CON_13 0x13FBF434 [31] rg_sc2_active_pwrctl_rsv = 1'b0 */
		writel((readl(MFG_ACTIVE_POWER_CON_13) & ~BIT(31)), MFG_ACTIVE_POWER_CON_13);
		// SC3
		/* MFG_ACTIVE_POWER_CON_18 0x13FBF448 [0] rg_sc3_active_pwrctl_en = 1'b0 */
		writel((readl(MFG_ACTIVE_POWER_CON_18) & ~BIT(0)), MFG_ACTIVE_POWER_CON_18);
		/* MFG_ACTIVE_POWER_CON_13 0x13FBF44C [31] rg_sc2_active_pwrctl_rsv = 1'b0 */
		writel((readl(MFG_ACTIVE_POWER_CON_19) & ~BIT(31)), MFG_ACTIVE_POWER_CON_19);
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
		g_shared_status->magic = GPUFREQ_MAGIC_NUMBER;
		g_shared_status->cur_oppidx_gpu = g_gpu.cur_oppidx;
		g_shared_status->opp_num_gpu = g_gpu.opp_num;
		g_shared_status->signed_opp_num_gpu = g_gpu.signed_opp_num;
		g_shared_status->cur_fgpu = g_gpu.cur_freq;
		g_shared_status->cur_vgpu = g_gpu.cur_volt;
		g_shared_status->cur_vsram_gpu = g_gpu.cur_vsram;
		g_shared_status->cur_power_gpu = g_gpu.working_table[g_gpu.cur_oppidx].power;
		g_shared_status->max_power_gpu = g_gpu.working_table[g_gpu.max_oppidx].power;
		g_shared_status->min_power_gpu = g_gpu.working_table[g_gpu.min_oppidx].power;
		g_shared_status->cur_fstack = g_stack.cur_freq;
		g_shared_status->power_count = g_gpu.power_count;
		g_shared_status->buck_count = g_gpu.buck_count;
		g_shared_status->mtcmos_count = g_gpu.mtcmos_count;
		g_shared_status->cg_count = g_gpu.cg_count;
		g_shared_status->active_count = g_gpu.active_count;
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
		g_shared_status->dual_buck = false;
		g_shared_status->segment_id = g_gpu.segment_id;
		g_shared_status->test_mode = true;
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

	switch (target) {
	case TARGET_MSSV_FGPU:
		if (val > POSDIV_2_MAX_FREQ || val < POSDIV_8_MIN_FREQ)
			ret = GPUFREQ_EINVAL;
		else {
			ret = __gpufreq_freq_scale_gpu(g_gpu.cur_freq, val);
			if (unlikely(ret))
				break;
			ret = __gpufreq_freq_scale_stack(g_stack.cur_freq, val);
		}
		break;
	case TARGET_MSSV_VGPU:
		if (val > VGPU_MAX_VOLT || val < VGPU_MIN_VOLT)
			ret = GPUFREQ_EINVAL;
		else
			ret = __gpufreq_volt_scale_gpu(
				g_gpu.cur_volt, val,
				g_gpu.cur_vsram, __gpufreq_get_vsram_by_vlogic(val));
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
		}
	}

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
}

static void __gpufreq_update_shared_status_adj_table(void)
{
	unsigned int copy_size = 0;

	if (!g_shared_status)
		return;

	/* GPU */
	/* aging table */
	copy_size = sizeof(struct gpufreq_adj_info) * NUM_GPU_BINNING_IDX;
	memcpy(g_shared_status->aging_table_gpu, g_aging_table[g_aging_table_idx], copy_size);
	/* avs table */
	copy_size = sizeof(struct gpufreq_adj_info) * NUM_GPU_BINNING_IDX;
	memcpy(g_shared_status->avs_table_gpu, g_avs_table, copy_size);
}

static void __gpufreq_update_shared_status_init_reg(void)
{
#if GPUFREQ_SHARED_STATUS_REG
	unsigned int copy_size = 0;

	if (!g_shared_status)
		return;

	/* [WARNING] GPU is power off at this moment */
	g_reg_mfgsys[IDX_EFUSE_PTPOD21].val = readl(EFUSE_PTPOD21);
	g_reg_mfgsys[IDX_EFUSE_PTPOD22].val = readl(EFUSE_PTPOD22);
	g_reg_mfgsys[IDX_EFUSE_PTPOD23].val = readl(EFUSE_PTPOD23);
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

	g_reg_mfgsys[IDX_MFG_CG_CON].val = readl(MFG_CG_CON);
	g_reg_mfgsys[IDX_MFG_DCM_CON_0].val = readl(MFG_DCM_CON_0);
	g_reg_mfgsys[IDX_MFG_ASYNC_CON].val = readl(MFG_ASYNC_CON);
	g_reg_mfgsys[IDX_MFG_GLOBAL_CON].val = readl(MFG_GLOBAL_CON);
	g_reg_mfgsys[IDX_MFG_AXCOHERENCE_CON].val = readl(MFG_AXCOHERENCE_CON);
	g_reg_mfgsys[IDX_MFG_PLL_CON0].val = readl(MFG_PLL_CON0);
	g_reg_mfgsys[IDX_MFG_PLL_CON1].val = readl(MFG_PLL_CON1);
	g_reg_mfgsys[IDX_MFGSC_PLL_CON0].val = readl(MFGSC_PLL_CON0);
	g_reg_mfgsys[IDX_MFGSC_PLL_CON1].val = readl(MFGSC_PLL_CON1);
	g_reg_mfgsys[IDX_MFG_RPC_AO_CLK_CFG].val = readl(MFG_RPC_AO_CLK_CFG);
	g_reg_mfgsys[IDX_MFG_RPC_MFG1_PWR_CON].val = readl(MFG_RPC_MFG1_PWR_CON);
#if !GPUFREQ_PDCA_ENABLE
	g_reg_mfgsys[IDX_MFG_RPC_MFG2_PWR_CON].val = readl(MFG_RPC_MFG2_PWR_CON);
	g_reg_mfgsys[IDX_MFG_RPC_MFG9_PWR_CON].val = readl(MFG_RPC_MFG9_PWR_CON);
	g_reg_mfgsys[IDX_MFG_RPC_MFG10_PWR_CON].val = readl(MFG_RPC_MFG10_PWR_CON);
	g_reg_mfgsys[IDX_MFG_RPC_MFG11_PWR_CON].val = readl(MFG_RPC_MFG11_PWR_CON);
	g_reg_mfgsys[IDX_MFG_RPC_MFG12_PWR_CON].val = readl(MFG_RPC_MFG12_PWR_CON);
#endif /* GPUFREQ_PDCA_ENABLE */
	g_reg_mfgsys[IDX_MFG_RPC_SLP_PROT_EN_SET].val = readl(MFG_RPC_SLP_PROT_EN_SET);
	g_reg_mfgsys[IDX_MFG_RPC_SLP_PROT_EN_CLR].val = readl(MFG_RPC_SLP_PROT_EN_CLR);
	g_reg_mfgsys[IDX_MFG_RPC_SLP_PROT_EN_STA].val = readl(MFG_RPC_SLP_PROT_EN_STA);
	g_reg_mfgsys[IDX_SPM_MFG0_PWR_CON].val = readl(SPM_MFG0_PWR_CON);
	g_reg_mfgsys[IDX_SPM_SOC_BUCK_ISO_CON].val = readl(SPM_SOC_BUCK_ISO_CON);
	g_reg_mfgsys[IDX_NTH_MFG_EMI1_GALS_SLV_DBG].val = readl(NTH_MFG_EMI1_GALS_SLV_DBG);
	g_reg_mfgsys[IDX_NTH_MFG_EMI0_GALS_SLV_DBG].val = readl(NTH_MFG_EMI0_GALS_SLV_DBG);
	g_reg_mfgsys[IDX_NTH_M6M7_IDLE_BIT_EN_1].val = readl(NTH_M6M7_IDLE_BIT_EN_1);
	g_reg_mfgsys[IDX_NTH_M6M7_IDLE_BIT_EN_0].val = readl(NTH_M6M7_IDLE_BIT_EN_0);
	g_reg_mfgsys[IDX_EMISYS_PROTECT_EN_SET_0].val = readl(EMISYS_PROTECT_EN_SET_0);
	g_reg_mfgsys[IDX_EMISYS_PROTECT_EN_CLR_0].val = readl(EMISYS_PROTECT_EN_CLR_0);
	g_reg_mfgsys[IDX_EMISYS_PROTECT_RDY_STA_0].val = readl(EMISYS_PROTECT_RDY_STA_0);
	g_reg_mfgsys[IDX_MD_MFGSYS_PROTECT_EN_STA_0].val = readl(MD_MFGSYS_PROTECT_EN_STA_0);
	g_reg_mfgsys[IDX_MD_MFGSYS_PROTECT_EN_SET_0].val = readl(MD_MFGSYS_PROTECT_EN_SET_0);
	g_reg_mfgsys[IDX_MD_MFGSYS_PROTECT_EN_CLR_0].val = readl(MD_MFGSYS_PROTECT_EN_CLR_0);
	g_reg_mfgsys[IDX_MD_MFGSYS_PROTECT_RDY_STA_0].val = readl(MD_MFGSYS_PROTECT_RDY_STA_0);
	g_reg_mfgsys[IDX_NTH_EMI_AO_DEBUG_CTRL0].val = readl(NTH_EMI_AO_DEBUG_CTRL0);
	g_reg_mfgsys[IDX_INFRA_AO_BUS0_U_DEBUG_CTRL0].val = readl(INFRA_AO_BUS0_U_DEBUG_CTRL0);
	g_reg_mfgsys[IDX_INFRA_AO1_BUS1_U_DEBUG_CTRL0].val = readl(INFRA_AO1_BUS1_U_DEBUG_CTRL0);

	copy_size = sizeof(struct gpufreq_reg_info) * NUM_MFGSYS_REG;
	memcpy(g_shared_status->reg_mfgsys, g_reg_mfgsys, copy_size);
#endif /* GPUFREQ_SHARED_STATUS_REG */
}

static void __gpufreq_update_shared_status_active_idle_reg(void)
{
#if GPUFREQ_SHARED_STATUS_REG
	unsigned int copy_size = 0;

	if (!g_shared_status)
		return;

	g_reg_mfgsys[IDX_MFG_PLL_CON0].val = readl(MFG_PLL_CON0);
	g_reg_mfgsys[IDX_MFG_PLL_CON1].val = readl(MFG_PLL_CON1);
	g_reg_mfgsys[IDX_MFGSC_PLL_CON0].val = readl(MFGSC_PLL_CON0);
	g_reg_mfgsys[IDX_MFGSC_PLL_CON1].val = readl(MFGSC_PLL_CON1);
	g_reg_mfgsys[IDX_MFG_RPC_AO_CLK_CFG].val = readl(MFG_RPC_AO_CLK_CFG);

	copy_size = sizeof(struct gpufreq_reg_info) * NUM_MFGSYS_REG;
	memcpy(g_shared_status->reg_mfgsys, g_reg_mfgsys, copy_size);

#endif /* GPUFREQ_SHARED_STATUS_REG */
}

static void __gpufreq_update_shared_status_dvfs_reg(void)
{
#if GPUFREQ_SHARED_STATUS_REG
	unsigned int copy_size = 0;

	if (!g_shared_status)
		return;

	g_reg_mfgsys[IDX_MFG_PLL_CON0].val = readl(MFG_PLL_CON0);
	g_reg_mfgsys[IDX_MFG_PLL_CON1].val = readl(MFG_PLL_CON1);
	g_reg_mfgsys[IDX_MFGSC_PLL_CON0].val = readl(MFGSC_PLL_CON0);
	g_reg_mfgsys[IDX_MFGSC_PLL_CON1].val = readl(MFGSC_PLL_CON1);
	g_reg_mfgsys[IDX_MFG_RPC_AO_CLK_CFG].val = readl(MFG_RPC_AO_CLK_CFG);

	copy_size = sizeof(struct gpufreq_reg_info) * NUM_MFGSYS_REG;
	memcpy(g_shared_status->reg_mfgsys, g_reg_mfgsys, copy_size);
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

/* API: fake PWR_CON value to temporarily disable PDCv2 */
static void __gpufreq_fake_mtcmos_control(unsigned int mode)
{
#if GPUFREQ_PDCA_ENABLE
	if (mode == FEAT_ENABLE) {
		/* fake power on value of SPM MFG2, 9-12 */
		writel(0xC000000D, MFG_RPC_MFG2_PWR_CON);  /* MFG2  */
		writel(0xC000000D, MFG_RPC_MFG9_PWR_CON);  /* MFG9  */
		writel(0xC000000D, MFG_RPC_MFG10_PWR_CON); /* MFG10 */
		writel(0xC000000D, MFG_RPC_MFG11_PWR_CON); /* MFG11 */
		writel(0xC000000D, MFG_RPC_MFG12_PWR_CON); /* MFG12 */
	} else if (mode == FEAT_DISABLE) {
		/* fake power off value of SPM MFG2-5 */
		writel(0x1112, MFG_RPC_MFG12_PWR_CON); /* MFG12 */
		writel(0x1112, MFG_RPC_MFG11_PWR_CON); /* MFG11 */
		writel(0x1112, MFG_RPC_MFG10_PWR_CON); /* MFG10 */
		writel(0x1112, MFG_RPC_MFG9_PWR_CON);  /* MFG9  */
		writel(0x1112, MFG_RPC_MFG2_PWR_CON);  /* MFG2  */
	}
#else
	GPUFREQ_UNREFERENCED(mode);
#endif /* GPUFREQ_PDCA_ENABLE */
}

static void __gpufreq_set_ocl_timestamp(void)
{
	/* MFG_TIMESTAMP 0x13FBF130 [0] enable timestamp = 1'b1 */
	/* MFG_TIMESTAMP 0x13FBF130 [1] timer from internal module = 1'b0 */
	/* MFG_TIMESTAMP 0x13FBF130 [1] timer from soc = 1'b0 */
	writel(GENMASK(1, 0), MFG_TIMESTAMP);
}

/* API: apply/restore Vaging to working table of GPU */
static void __gpufreq_set_margin_mode(unsigned int val)
{
	/* update volt margin */
	__gpufreq_apply_restore_margin(val);

	/* update power info to working table */
	__gpufreq_measure_power();

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

/* API: apply (enable) or restore (disable) margin */
static void __gpufreq_apply_restore_margin(unsigned int val)
{
	struct gpufreq_opp_info *working_table = NULL;
	struct gpufreq_opp_info *signed_table = NULL;
	int working_opp_num = 0, signed_opp_num = 0, segment_upbound = 0, i = 0;

	working_table = g_gpu.working_table;
	signed_table = g_gpu.signed_table;
	working_opp_num = g_gpu.opp_num;
	signed_opp_num = g_gpu.signed_opp_num;
	segment_upbound = g_gpu.segment_upbound;

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

		GPUFREQ_LOGD("Margin mode: %d, GPU[%d] Volt: %d, Vsram: %d",
			val, i, working_table[i].volt, working_table[i].vsram);
	}
}

/* API: DVFS order control of GPU */
static int __gpufreq_generic_scale_gpu(
	unsigned int freq_old, unsigned int freq_new,
	unsigned int vgpu_old, unsigned int vgpu_new,
	unsigned int vsram_old, unsigned int vsram_new)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START
		("freq_old=%d, freq_new=%d, volt_old=%d, volt_new=%d, vsram_old=%d, vsram_new=%d",
		freq_old, freq_new, vgpu_old, vgpu_new, vsram_old, vsram_new);

	/* scale-up: Vsram -> Vgpu -> Freq */
	if (freq_new > freq_old) {
		/* volt scaling */
		ret = __gpufreq_volt_scale_gpu(vgpu_old, vgpu_new, vsram_old, vsram_new);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to scale Vgpu: (%d->%d), Vsram: (%d->%d)",
				vgpu_old, vgpu_new, vsram_old, vsram_new);
			goto done;
		}

		/* GPU freq scaling */
		ret = __gpufreq_freq_scale_gpu(freq_old, freq_new);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to scale Fgpu: (%d->%d)",
				freq_old, freq_new);
			goto done;
		}
		/* STACK freq scaling */
		ret = __gpufreq_freq_scale_stack(freq_old, freq_new);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to scale Fstack: (%d->%d)",
				freq_old, freq_new);
			goto done;
		}
	/* scale-down: Freq -> Vgpu -> Vsram */
	} else if (freq_new < freq_old) {
		/* freq scaling */
		ret = __gpufreq_freq_scale_gpu(freq_old, freq_new);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to scale Fgpu: (%d->%d)",
				freq_old, freq_new);
			goto done;
		}
		/* STACK freq scaling */
		ret = __gpufreq_freq_scale_stack(freq_old, freq_new);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to scale Fstack: (%d->%d)",
				freq_old, freq_new);
			goto done;
		}
		/* volt scaling */
		ret = __gpufreq_volt_scale_gpu(vgpu_old, vgpu_new, vsram_old, vsram_new);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to scale Vgpu: (%d->%d), Vsram: (%d->%d)",
				vgpu_old, vgpu_new, vsram_old, vsram_new);
			goto done;
		}
	/* keep: volt only */
	} else {
		/* volt scaling */
		ret = __gpufreq_volt_scale_gpu(vgpu_old, vgpu_new, vsram_old, vsram_new);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to scale Vgpu: (%d->%d), Vsram_gpu: (%d->%d)",
				vgpu_old, vgpu_new, vsram_old, vsram_new);
			goto done;
		}
	}

	/* freq of GPU and STACK should always be equal */
	if (g_gpu.cur_freq != g_stack.cur_freq) {
		__gpufreq_abort("unequal Fgpu: %d and Fstack: %d",
			g_gpu.cur_freq, g_stack.cur_freq);
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
	int ret = GPUFREQ_SUCCESS;
	/* GPU */
	int cur_oppidx = 0, target_oppidx = 0;
	/* GPU */
	unsigned int cur_fgpu = 0, cur_vgpu = 0, target_fgpu = 0, target_vgpu = 0;
	/* SRAM */
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

	/* prepare GPU setting */
	cur_oppidx = g_gpu.cur_oppidx;
	cur_fgpu = g_gpu.cur_freq;
	cur_vgpu = g_gpu.cur_volt;
	cur_vsram = g_gpu.cur_vsram;

	target_oppidx = __gpufreq_get_idx_by_fgpu(target_freq);
	target_fgpu = target_freq;
	target_vgpu = target_volt;
	target_vsram = __gpufreq_get_vsram_by_vlogic(target_volt);

	GPUFREQ_LOGD("begin to commit GPU Freq: (%d->%d), Volt: (%d->%d)",
		cur_fgpu, target_fgpu, cur_vgpu, target_vgpu);

	ret = __gpufreq_generic_scale_gpu(cur_fgpu, target_fgpu,
		cur_vgpu, target_vgpu, cur_vsram, target_vsram);
	if (unlikely(ret)) {
		GPUFREQ_LOGE(
			"fail to scale GPU: Idx(%d->%d), Freq(%d->%d), Volt(%d->%d), Vsram(%d->%d)",
			cur_oppidx, target_oppidx, cur_fgpu, target_fgpu,
			cur_vgpu, target_vgpu, cur_vsram, target_vsram);
		goto done_unlock;
	}

	g_gpu.cur_oppidx = target_oppidx;

	__gpufreq_footprint_oppidx(target_oppidx);

	/* update current status to shared memory */
	if (g_shared_status) {
		g_shared_status->cur_oppidx_gpu = g_gpu.cur_oppidx;
		g_shared_status->cur_fgpu = g_gpu.cur_freq;
		g_shared_status->cur_vgpu = g_gpu.cur_volt;
		g_shared_status->cur_vsram_gpu = g_gpu.cur_vsram;
		g_shared_status->cur_power_gpu = g_gpu.working_table[g_gpu.cur_oppidx].power;
		g_shared_status->cur_fstack = g_stack.cur_freq;
		__gpufreq_update_shared_status_dvfs_reg();
	}

done_unlock:
	mutex_unlock(&gpufreq_lock);

	GPUFREQ_TRACE_END();

	return ret;
}

static void __gpufreq_switch_clksrc(enum gpufreq_target target, enum gpufreq_clk_src clksrc)
{
	unsigned int offset = 0;
	u32 val = 0;

	GPUFREQ_TRACE_START("target=%d, clksrc=%d", target, clksrc);

	/* MFG_RPC_AO_CLK_CFG 0x13F91034 */
	val = readl(MFG_RPC_AO_CLK_CFG);

	if (target == TARGET_STACK)
		/* [7] CKMUX_SEL_STACK */
		offset = CKMUX_SEL_STACK;
	else
		/* [4] CKMUX_SEL_CORE */
		offset = CKMUX_SEL_CORE;

	if (clksrc == CLOCK_MAIN)
		/* MAIN = 1'b1 */
		val |= (1UL << offset);
	else if (clksrc == CLOCK_SUB)
		/* SUB = 1'b0 */
		val &= ~(1UL << offset);
	else
		GPUFREQ_LOGE("invalid clock source: %d (EINVAL)", clksrc);

	writel(val, MFG_RPC_AO_CLK_CFG);

	GPUFREQ_TRACE_END();
}

static void __gpufreq_switch_parksrc(enum gpufreq_target target, enum gpufreq_clk_src clksrc)
{
	unsigned int offset = 0;
	u32 val = 0;

	GPUFREQ_TRACE_START("target=%d, clksrc=%d", target, clksrc);

	/* MFG_RPC_AO_CLK_CFG 0x13F91034 */
	val = readl(MFG_RPC_AO_CLK_CFG);

	if (target == TARGET_STACK)
		/* [8] CKMUX_SEL_STACK_PARK */
		offset = CKMUX_SEL_STACK_PARK;
	else
		/* [5] CKMUX_SEL_PARK */
		offset = CKMUX_SEL_PARK;

	if (clksrc == CLOCK_MAIN)
		/* MAIN = 1'b1 */
		val |= BIT(offset);
	else if (clksrc == CLOCK_SUB)
		/* SUB = 1'b0 */
		val &= ~BIT(offset);
	else
		GPUFREQ_LOGE("invalid clock source: %d (EINVAL)", clksrc);

	writel(val, MFG_RPC_AO_CLK_CFG);

	GPUFREQ_TRACE_END();
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

	if ((freq >= POSDIV_8_MIN_FREQ) && (freq <= POSDIV_2_MAX_FREQ))
		pcw = (((unsigned long long)freq * (1 << posdiv)) << DDS_SHIFT) / MFGPLL_FIN / 1000;
	else
		__gpufreq_abort("out of range Freq: %d", freq);

	GPUFREQ_LOGD("target freq: %d, posdiv: %d, pcw: 0x%llx", freq, posdiv, pcw);

	return (unsigned int)pcw;
}

static enum gpufreq_posdiv __gpufreq_get_real_posdiv_gpu(void)
{
	unsigned int mfgpll = 0;
	enum gpufreq_posdiv posdiv = POSDIV_POWER_1;

	mfgpll = readl(MFG_PLL_CON1);

	posdiv = (mfgpll & GENMASK(26, 24)) >> POSDIV_SHIFT;

	return posdiv;
}

static enum gpufreq_posdiv __gpufreq_get_real_posdiv_stack(void)
{
	unsigned int mfgpll = 0;
	enum gpufreq_posdiv posdiv = POSDIV_POWER_1;

	mfgpll = readl(MFGSC_PLL_CON1);

	posdiv = (mfgpll & GENMASK(26, 24)) >> POSDIV_SHIFT;

	return posdiv;
}

static enum gpufreq_posdiv __gpufreq_get_posdiv_by_fgpu(unsigned int freq)
{
	if (freq > POSDIV_4_MAX_FREQ)
		return POSDIV_POWER_2;
	else if (freq > POSDIV_8_MAX_FREQ)
		return POSDIV_POWER_4;
	else if (freq > POSDIV_16_MAX_FREQ)
		return POSDIV_POWER_8;
	else if (freq >= POSDIV_16_MIN_FREQ)
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
	target_posdiv = __gpufreq_get_posdiv_by_fgpu(freq_new);
	/* compute PCW based on target Freq */
	pcw = __gpufreq_calculate_pcw(freq_new, target_posdiv);
	if (!pcw) {
		GPUFREQ_LOGE("invalid PCW: 0x%x", pcw);
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
	__gpufreq_switch_clksrc(TARGET_GPU, CLOCK_SUB);
	/* 2. compute CON1 with PCW and POSDIV */
	pll = BIT(31) | (target_posdiv << POSDIV_SHIFT) | pcw;
	/* 3. change PCW and POSDIV by writing CON1 */
	writel(pll, MFG_PLL_CON1);
	/* 4. wait until PLL stable */
	udelay(20);
	/* 5. switch to main clk source */
	__gpufreq_switch_clksrc(TARGET_GPU, CLOCK_MAIN);
#endif /* GPUFREQ_FHCTL_ENABLE && CONFIG_COMMON_CLK_MTK_FREQ_HOPPING */

	g_gpu.cur_freq = __gpufreq_get_real_fgpu();
	if (unlikely(g_gpu.cur_freq != freq_new))
		__gpufreq_abort("inconsistent scaled Fgpu, cur_freq: %d, target_freq: %d",
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
	/* because we only have OPP table of GPU */
	target_posdiv = __gpufreq_get_posdiv_by_fgpu(freq_new);
	/* compute PCW based on target Freq */
	pcw = __gpufreq_calculate_pcw(freq_new, target_posdiv);
	if (!pcw) {
		GPUFREQ_LOGE("invalid PCW: 0x%x", pcw);
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
	__gpufreq_switch_clksrc(TARGET_STACK, CLOCK_SUB);
	/* 2. compute CON1 with PCW and POSDIV */
	pll = BIT(31) | (target_posdiv << POSDIV_SHIFT) | pcw;
	/* 3. change PCW and POSDIV by writing CON1 */
	writel(pll, MFGSC_PLL_CON1);
	/* 4. wait until PLL stable */
	udelay(20);
	/* 5. switch to main clk source */
	__gpufreq_switch_clksrc(TARGET_STACK, CLOCK_MAIN);
#endif /* GPUFREQ_FHCTL_ENABLE && CONFIG_COMMON_CLK_MTK_FREQ_HOPPING */

	g_stack.cur_freq = __gpufreq_get_real_fstack();
	if (unlikely(g_stack.cur_freq != freq_new))
		__gpufreq_abort("inconsistent scaled Fstack, cur_freq: %d, target_freq: %d",
			g_stack.cur_freq, freq_new);

	GPUFREQ_LOGD("Fstack: %d, PCW: 0x%x, CON1: 0x%08x", g_stack.cur_freq, pcw, pll);

	/* because return value is different across the APIs */
	ret = GPUFREQ_SUCCESS;

	/* notify stack freq change to DDK  */
	mtk_notify_gpu_freq_change(1, freq_new);

done:
	GPUFREQ_TRACE_END();

	return ret;
}

static unsigned int __gpufreq_settle_time_vgpu(enum gpufreq_opp_direct direct, int deltaV)
{
	/* [MT6368_VBUCK1+2][VGPU]
	 * DVFS Rising : (deltaV / 12.5(mV)) + 3.85us + 2us + 2.64us
	 * DVFS Falling: (deltaV / 6.25(mV)) + 3.85us + 2us + 2.64us
	 * deltaV = mV x 100
	 */
	unsigned int t_settle = 0;

	if (direct == SCALE_UP) {
		/* rising */
		t_settle = (deltaV / 1250) + 4 + 2 + 3;
	} else if (direct == SCALE_DOWN) {
		/* falling */
		t_settle = (deltaV / 625) + 4 + 2 + 3;
	}

	return t_settle; /* us */
}

static unsigned int __gpufreq_settle_time_vsram(enum gpufreq_opp_direct direct, int deltaV)
{
	/*
	 * [MT6363_VSRAM_APU][VSRAM]
	 * DVFS Rising : (deltaV / 10(mV)) + 5us
	 * DVFS Falling: (deltaV / 7.5(mV)) + 5us
	 * deltaV = mV x 100
	 */
	unsigned int t_settle = 0;

	if (direct == SCALE_UP) {
		/* rising */
		t_settle = (deltaV / 1000) + 5;
	} else if (direct == SCALE_DOWN) {
		/* falling */
		t_settle = (deltaV / 750) + 5;
	}

	return t_settle; /* us */
}

/* API: scale Volt of GPU via Regulator */
static int __gpufreq_volt_scale_gpu(
	unsigned int vgpu_old, unsigned int vgpu_new,
	unsigned int vsram_old, unsigned int vsram_new)
{
	int ret = GPUFREQ_SUCCESS;
	unsigned int t_settle = 0, vgpu_target = 0;

	GPUFREQ_TRACE_START("volt_old=%d, volt_new=%d, vsram_old=%d, vsram_new=%d",
		vgpu_old, vgpu_new, vsram_old, vsram_new);

	GPUFREQ_LOGD("begin to scale Vgpu: (%d->%d)", vgpu_old, vgpu_new);
	if (vgpu_new > vgpu_old) {
		if ((vsram_new > VSRAM_THRESH) && (vgpu_old + VSRAM_VLOGIC_DIFF < vgpu_new)) {
			/* GPU to park */
			vgpu_target = VSRAM_THRESH;
			t_settle = __gpufreq_settle_time_vgpu(SCALE_UP, (vgpu_target - vgpu_old));
			ret = regulator_set_voltage(g_pmic->reg_vgpu,
				vgpu_target * 10, VGPU_MAX_VOLT * 10 + 125);
			if (unlikely(ret)) {
				__gpufreq_abort("fail to set regulator VGPU: %d (%d)",
					vgpu_target, ret);
				goto done;
			}
			udelay(t_settle);
			vgpu_old = vgpu_target;
		}

		if (vsram_new != vsram_old) {
			t_settle = __gpufreq_settle_time_vsram(SCALE_UP, (vsram_new - vsram_old));
			/* Vsram scaling*/
			ret = regulator_set_voltage(g_pmic->reg_vsram,
				vsram_new * 10, VSRAM_MAX_VOLT * 10 + 125);
			if (unlikely(ret)) {
				__gpufreq_abort("fail to set regulator VSRAM: %d (%d)",
					vsram_new, ret);
				goto done;
			}
			udelay(t_settle);
		}
		/* Vgpu scaling */
		if (vgpu_old != vgpu_new) {
			t_settle =  __gpufreq_settle_time_vgpu(SCALE_UP, (vgpu_new - vgpu_old));
			ret = regulator_set_voltage(g_pmic->reg_vgpu,
					vgpu_new * 10, VGPU_MAX_VOLT * 10 + 125);
			if (unlikely(ret)) {
				__gpufreq_abort("fail to set regulator VGPU: %d (%d)",
					vgpu_new, ret);
				goto done;
			}
			udelay(t_settle);
		}
	/* volt scaling down */
	} else if (vgpu_new < vgpu_old) {
		if ((vsram_old > VSRAM_THRESH) && (vgpu_old - vgpu_new > VSRAM_VLOGIC_DIFF)) {
			/* GPU to park */
			vgpu_target = VSRAM_THRESH;
		} else {
			vgpu_target = vgpu_new;
		}
		t_settle = __gpufreq_settle_time_vgpu(SCALE_DOWN, (vgpu_old - vgpu_target));
		ret = regulator_set_voltage(g_pmic->reg_vgpu,
			vgpu_target * 10, VGPU_MAX_VOLT * 10 + 125);
		if (unlikely(ret)) {
			__gpufreq_abort("fail to set regulator VGPU: %d (%d)", vgpu_target, ret);
			goto done;
		}
		udelay(t_settle);
		vgpu_old = vgpu_target;
		if (vsram_new != vsram_old) {
			t_settle = __gpufreq_settle_time_vsram(SCALE_DOWN, (vsram_old - vsram_new));
			/* Vsram scaling*/
			ret = regulator_set_voltage(g_pmic->reg_vsram,
				vsram_new * 10, VSRAM_MAX_VOLT * 10 + 125);
			if (unlikely(ret)) {
				__gpufreq_abort("fail to set regulator VSRAM: %d (%d)",
					vsram_new, ret);
				goto done;
			}
			udelay(t_settle);
		}
		if (vgpu_old != vgpu_new) {
			t_settle = __gpufreq_settle_time_vgpu(SCALE_DOWN, (vgpu_old - vgpu_new));
			ret = regulator_set_voltage(g_pmic->reg_vgpu,
				vgpu_new * 10, VGPU_MAX_VOLT * 10 + 125);
			if (unlikely(ret)) {
				__gpufreq_abort("fail to set regulator VGPU: %d (%d)",
					vgpu_new, ret);
				goto done;
			}
			udelay(t_settle);
		}
	/* keep volt */
	} else {
		ret = GPUFREQ_SUCCESS;
	}

	g_gpu.cur_volt = __gpufreq_get_real_vgpu();
	if (unlikely(g_gpu.cur_volt != vgpu_new))
		__gpufreq_abort("inconsistent scaled Vgpu, cur_volt: %d, target_volt: %d",
			g_gpu.cur_volt, vgpu_new);

	g_gpu.cur_vsram = __gpufreq_get_real_vsram();
	if (unlikely(g_gpu.cur_vsram != vsram_new))
		__gpufreq_abort("inconsistent scaled Vsram, cur_volt: %d, target_volt: %d",
			g_gpu.cur_vsram, vsram_new);

	GPUFREQ_LOGD("Vgpu: %d, Vsram: %d, udelay: %d",
		g_gpu.cur_volt, g_gpu.cur_vsram, t_settle);

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

	/* 0x13FA0400 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "sensor_pll");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource SENSOR_PLL");
		return;
	}
	g_sensor_pll_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_sensor_pll_base)) {
		GPUFREQ_LOGE("fail to ioremap SENSOR_PLL: 0x%llx", res->start);
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

	GPUFREQ_LOGI("[GPU] SPM_SPM2GPUPM_CON: 0x%08x", readl(SPM_SPM2GPUPM_CON));

	GPUFREQ_LOGI("[GPU] MFG_0_12_PWR_STATUS: 0x%08x, MFG_RPC_MFG1_PWR_CON: 0x%08x",
		MFG_0_12_PWR_STATUS,
		readl(MFG_RPC_MFG1_PWR_CON));

	GPUFREQ_LOGI("[TOP] CON0: 0x%08x, CON1: %d, FMETER: %d, SEL: 0x%08x, SEL_PARK: 0x%08x",
		readl(MFG_PLL_CON0),
		__gpufreq_get_real_fgpu(),
		__gpufreq_get_fmeter_main_fgpu(),
		readl(MFG_RPC_AO_CLK_CFG) & BIT(CKMUX_SEL_CORE),
		readl(MFG_RPC_AO_CLK_CFG) & BIT(CKMUX_SEL_PARK));

	GPUFREQ_LOGI("[STK] CON0: 0x%08x, CON1: %d, FMETER: %d, SEL: 0x%08x, SEL_PARK: 0x%08x",
		readl(MFGSC_PLL_CON0),
		__gpufreq_get_real_fstack(),
		__gpufreq_get_fmeter_main_fstack(),
		readl(MFG_RPC_AO_CLK_CFG) & BIT(CKMUX_SEL_STACK),
		readl(MFG_RPC_AO_CLK_CFG) & BIT(CKMUX_SEL_STACK_PARK));

	GPUFREQ_LOGI("[GPU] MALI_GPU_ID: 0x%08x", readl(MALI_GPU_ID));
}

static unsigned int __gpufreq_get_fmeter_fgpu(void)
{
	return __gpufreq_get_fmeter_main_fgpu();
}

static unsigned int __gpufreq_get_fmeter_fstack(void)
{
	return __gpufreq_get_fmeter_main_fstack();
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

static unsigned int __gpufreq_get_fmeter_sub(void)
{
	u32 val = 0, ckgen_load_cnt = 0, ckgen_k1 = 0;
	int i = 0;
	unsigned int freq = 0;

	writel(GENMASK(23, 16), SENSOR_PLL_FQMTR_CON1);
	val = readl(SENSOR_PLL_FQMTR_CON0);
	writel((val & GENMASK(23, 0)), SENSOR_PLL_FQMTR_CON0);
	writel((BIT(12) | BIT(15)), SENSOR_PLL_FQMTR_CON0);

	ckgen_load_cnt = readl(MFGSC_PLL_FQMTR_CON1) >> 16;
	ckgen_k1 = readl(SENSOR_PLL_FQMTR_CON0) >> 24;

	val = readl(SENSOR_PLL_FQMTR_CON0);
	writel((val | BIT(4) | BIT(12)), SENSOR_PLL_FQMTR_CON0);

	/* wait fmeter finish */
	while (readl(SENSOR_PLL_FQMTR_CON0) & BIT(4)) {
		udelay(10);
		i++;
		if (i > 1000) {
			GPUFREQ_LOGE("wait SENSORPLL Fmeter timeout");
			break;
		}
	}

	val = readl(SENSOR_PLL_FQMTR_CON1) & GENMASK(15, 0);
	/* KHz */
	freq = (val * 26000 * (ckgen_k1 + 1)) / (ckgen_load_cnt + 1);

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

/* API: get real current Vsram from regulator (mV * 100) */
static unsigned int __gpufreq_get_real_vsram(void)
{
	unsigned int volt = 0;

	if (regulator_is_enabled(g_pmic->reg_vsram))
		/* regulator_get_voltage return volt with uV */
		volt = regulator_get_voltage(g_pmic->reg_vsram) / 10;

	return volt;
}

/* API: get Volt of SRAM via volt of GPU */
static unsigned int __gpufreq_get_vsram_by_vlogic(unsigned int volt)
{
	unsigned int vsram = 0;

	if (volt <= VSRAM_THRESH)
		vsram =  VSRAM_THRESH;
	else
		vsram = volt;

	return vsram;
}

/* HWDCM: mask clock when GPU idle (dynamic clock mask) */
static void __gpufreq_hwdcm_config(void)
{
#if GPUFREQ_HWDCM_ENABLE
	u32 val = 0;

	/* G : MFG_DCM_CON_0 0x13FBF010 [6:0] BG3D_DBC_CNT = 7'b0111111 */
	/* G : MFG_DCM_CON_0 0x13FBF010 [15]  BG3D_DCM_EN = 1'b1 */
	val = (readl(MFG_DCM_CON_0) & ~BIT(6)) | GENMASK(5, 0) | BIT(15);
	writel(val, MFG_DCM_CON_0);

	/* B, K : MFG_ASYNC_CON 0x13FBF020 [23] MEM0_SLV_CG_ENABLE = 1'b1 */
	/* B, K : MFG_ASYNC_CON 0x13FBF020 [25] MEM1_SLV_CG_ENABLE = 1'b1 */
	val = readl(MFG_ASYNC_CON) | BIT(23) | BIT(25);
	writel(val, MFG_ASYNC_CON);

	/* A : MFG_GLOBAL_CON 0x13FBF0B0 [8]  GPU_SOCIF_MST_FREE_RUN = 1'b0 */
	/* L : MFG_GLOBAL_CON 0x13FBF0B0 [10] GPU_CLK_FREE_RUN = 1'b0 */
	/* I : MFG_GLOBAL_CON 0x13FBF0B0 [21] dvfs_hint_cg_en = 1'b0 */
	/* M : MFG_GLOBAL_CON 0x13FBF0B0 [24] stack_hd_bg3d_cg_free_run = 1'b0 */
	val = readl(MFG_GLOBAL_CON) & ~BIT(8) & ~BIT(10) & ~BIT(21) & ~BIT(24);
	writel(val, MFG_GLOBAL_CON);

	/* C : MFG_RPC_AO_CLK_CFG 0x13F91034 [0] CG_FAXI_CK_SOC_IN_FREE_RUN = 1'b0 */
	writel((readl(MFG_RPC_AO_CLK_CFG) & ~BIT(0)), MFG_RPC_AO_CLK_CFG);
#endif /* GPUFREQ_HWDCM_ENABLE */
}


/* AOC2.0: Vcore_AO can power off */
static void __gpufreq_aoc_config(enum gpufreq_power_state power)
{
	u32 val = 0;

	/* wait HW semaphore:  0x1C0016AC [0] = 1'b1 */
	do {
		writel((readl(SPM_SEMA_M4) | BIT(0)), SPM_SEMA_M4);
	} while ((readl(SPM_SEMA_M4) & BIT(0)) != BIT(0));

	/* power on: AOCISO -> AOCLHENB */
	if (power == POWER_ON) {
		udelay(1);
		/* SOC_BUCK_ISO_CON 0x1C001F74 [9] AOC_VGPU_SRAM_ISO_DIN = 1'b0 */
		writel((readl(SPM_SOC_BUCK_ISO_CON) & ~BIT(9)), SPM_SOC_BUCK_ISO_CON);
		udelay(1);
		/* SOC_BUCK_ISO_CON 0x1C001F74 [10] AOC_VGPU_SRAM_LATCH_ENB = 1'b0 */
		writel((readl(SPM_SOC_BUCK_ISO_CON) & ~BIT(10)), SPM_SOC_BUCK_ISO_CON);
	/* power off: AOCLHENB -> AOCISO */
	} else {
		/* SOC_BUCK_ISO_CON 0x1C001F74 [10] AOC_VGPU_SRAM_LATCH_ENB = 1'b1 */
		writel((readl(SPM_SOC_BUCK_ISO_CON) | BIT(10)), SPM_SOC_BUCK_ISO_CON);
		udelay(1);
		/* SOC_BUCK_ISO_CON 0x1C001F74 [9] AOC_VGPU_SRAM_ISO_DIN = 1'b1 */
		writel((readl(SPM_SOC_BUCK_ISO_CON) | BIT(9)), SPM_SOC_BUCK_ISO_CON);
		udelay(1);
	}

	GPUFREQ_LOGD("power: %d, SPM_SOC_BUCK_ISO_CON: 0x%08x",
		power, readl(SPM_SOC_BUCK_ISO_CON));

	/* signal HW semaphore: SPM_SEMA_M4 0x1C0016AC [0] = 1'b1 */
	writel((readl(SPM_SEMA_M4) | BIT(0)), SPM_SEMA_M4);
}

/* ACP: GPU can access CPU cache directly */
static void __gpufreq_acp_config(void)
{
#if GPUFREQ_ACP_ENABLE
	/* MFG_1TO2AXI_CON_00 0x13FBF8E0 [24:0] mfg_axi1to2_R_dispatch_mode = 0x855 */
	writel(0x00FFC855, MFG_1TO2AXI_CON_00);
	/* MFG_1TO2AXI_CON_02 0x13FBF8E8 [24:0] mfg_axi1to2_R_dispatch_mode = 0x855 */
	writel(0x00FFC855, MFG_1TO2AXI_CON_02);
	/* MFG_1TO2AXI_CON_04 0x13FBF910 [24:0] mfg_axi1to2_R_dispatch_mode = 0x855 */
	writel(0x00FFC855, MFG_1TO2AXI_CON_04);
	/* MFG_1TO2AXI_CON_06 0x13FBF918 [24:0] mfg_axi1to2_R_dispatch_mode = 0x855 */
	writel(0x00FFC855, MFG_1TO2AXI_CON_06);

	// Enable ACP
	/* MFG_AXCOHERENCE_CON 0x13FBF168 [0] M0_coherence_enable = 1'b1 */
	/* MFG_AXCOHERENCE_CON 0x13FBF168 [1] M1_coherence_enable = 1'b1 */
	/* MFG_AXCOHERENCE_CON 0x13FBF168 [2] M2_coherence_enable = 1'b1 */
	/* MFG_AXCOHERENCE_CON 0x13FBF168 [3] M3_coherence_enable = 1'b1 */
	writel((readl(MFG_AXCOHERENCE_CON) | GENMASK(3, 0)), MFG_AXCOHERENCE_CON);

	// Enable ACP_MPU
	/* MFG_SECURE_REG 0x13FBCFE0 [30] acp_mpu_enable = 1'b1 */
	/* MFG_SECURE_REG 0x13FBCFE0 [31] acp_mpu_rule3_disable = 1'b1 */
	writel((readl(MFG_SECURE_REG) | GENMASK(31, 30)), MFG_SECURE_REG);

	/* NTH_APU_ACP_GALS_SLV_CTRL  0x1021C600 [27:25] MFG_ACP_AR_MPAM_2_1_0 = 3'b111 */
	/* NTH_APU_EMI1_GALS_SLV_CTRL 0x1021C624 [27:25] MFG_ACP_AW_MPAM_2_1_0 = 3'b111 */
	writel((readl(NTH_APU_ACP_GALS_SLV_CTRL) | GENMASK(27, 25)), NTH_APU_ACP_GALS_SLV_CTRL);
	writel((readl(NTH_APU_EMI1_GALS_SLV_CTRL) | GENMASK(27, 25)), NTH_APU_EMI1_GALS_SLV_CTRL);
#endif /* GPUFREQ_ACP_ENABLE */
}

/* GPM1.0: di/dt reduction by slowing down speed of frequency scaling up or down */
static void __gpufreq_gpm1_config(void)
{
#if GPUFREQ_GPM1_ENABLE
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
#endif /* GPUFREQ_GPM1_ENABLE */
}

/* Merge GPU transaction to maximize DRAM efficiency */
static void __gpufreq_transaction_config(void)
{
#if GPUFREQ_MERGER_ENABLE
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

#endif /* GPUFREQ_MERGER_ENABLE */
}

static void __gpufreq_dfd_config(void)
{
#if GPUFREQ_DFD_ENABLE
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
#endif /* GPUFREQ_DFD_ENABLE */
}

static int __gpufreq_clock_control(enum gpufreq_power_state power)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("power=%d", power);

	if (power == POWER_ON) {
		/* prepare GPU PLL */
		ret = clk_prepare(g_clk->clk_main_parent);
		if (unlikely(ret)) {
			__gpufreq_abort("fail to prepare clk_main_parent (%d)", ret);
			goto done;
		}
		/* prepare STACK PLL */
		ret = clk_prepare(g_clk->clk_sc_main_parent);
		if (unlikely(ret)) {
			__gpufreq_abort("fail to prepare clk_sc_main_parent (%d)", ret);
			goto done;
		}

		/* switch GPU MUX to main_parent */
		__gpufreq_switch_clksrc(TARGET_GPU, CLOCK_MAIN);
		/* switch STACK MUX to main_parent */
		__gpufreq_switch_clksrc(TARGET_STACK, CLOCK_MAIN);
		/* switch GPU top and stack parking clock source to SENSOR PLL */
		__gpufreq_switch_parksrc(TARGET_GPU, CLOCK_MAIN);
		__gpufreq_switch_parksrc(TARGET_STACK, CLOCK_MAIN);

		g_gpu.cg_count++;
		g_gpu.active_count++;
	} else {
		/* switch GPU top and stack parking clock source to 26M */
		__gpufreq_switch_parksrc(TARGET_STACK, CLOCK_SUB);
		__gpufreq_switch_parksrc(TARGET_GPU, CLOCK_SUB);
		/* switch STACK MUX to sub_parent */
		__gpufreq_switch_clksrc(TARGET_STACK, CLOCK_SUB);
		/* switch GPU MUX to sub_parent */
		__gpufreq_switch_clksrc(TARGET_GPU, CLOCK_SUB);
		/* unprepare gpu pll and stack pll */
		clk_unprepare(g_clk->clk_sc_main_parent);
		clk_unprepare(g_clk->clk_main_parent);

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
	GPUFREQ_LOGE("(0x13F91070)=0x%x, (0x13F910A0)=0x%x",
		readl(MFG_RPC_MFG1_PWR_CON), readl(MFG_RPC_MFG2_PWR_CON));
	GPUFREQ_LOGE("(0x13F910BC)=0x%x, (0x13F910C0)=0x%x, (0x13F910C4)=0x%x, (0x13F910C8)=0x%x",
		readl(MFG_RPC_MFG9_PWR_CON), readl(MFG_RPC_MFG10_PWR_CON),
		readl(MFG_RPC_MFG11_PWR_CON), readl(MFG_RPC_MFG12_PWR_CON));
	__gpufreq_abort("timeout");
}
#endif /* GPUFREQ_SELF_CTRL_MTCMOS */

#if GPUFREQ_SELF_CTRL_MTCMOS
static void __gpufreq_mfg1_rpc_control(enum gpufreq_power_state power)
{
	int i = 0;

	if (power == POWER_ON) {
		/* MFG1_PWR_CON 0x13F91070 [2] MFG1_PWR_ON = 1'b1 */
		writel((readl(MFG_RPC_MFG1_PWR_CON) | BIT(2)), MFG_RPC_MFG1_PWR_CON);
		/* Delay after mfg1 short chain power on */
		udelay(1);
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
		/* Delay after mfg1 long chain power on */
		udelay(10);
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
		/* TX: MD_MFGSYS_PROTECT_EN_CLR_0 0x10001CA8 [1:0] = 2'b11 */
		writel(GENMASK(1, 0), MD_MFGSYS_PROTECT_EN_CLR_0);
		/* RX: MFG_RPC_SLP_PROT_EN_CLR 0x13F91044 [16] = 1'b1 */
		writel(BIT(16), MFG_RPC_SLP_PROT_EN_CLR);
		/* RX: MFG_RPC_SLP_PROT_EN_CLR 0x13F91044 [18] = 1'b1 */
		writel(BIT(18), MFG_RPC_SLP_PROT_EN_CLR);
		/* EMI: EMISYS_PROTECT_EN_CLR_0 0x10001C68 [19:18] = 1'b1 */
		writel(GENMASK(19, 18), EMISYS_PROTECT_EN_CLR_0);
	} else {
		/* TX: MD_MFGSYS_PROTECT_EN_SET_0 0x10001CA4 [1:0] = 2'b11 */
		writel(GENMASK(1, 0), MD_MFGSYS_PROTECT_EN_SET_0);
		/* TX: MD_MFGSYS_PROTECT_RDY_STA_0 0x10001CAC [1:0] = 2'b11 */
		i = 0;
		while ((readl(MD_MFGSYS_PROTECT_RDY_STA_0) & GENMASK(1, 0)) != GENMASK(1, 0)) {
			udelay(10);
			if (++i > 1000) {
				__gpufreq_footprint_power_step(0xF4);
				goto timeout;
			}
		}
		/* RX: MFG_RPC_SLP_PROT_EN_SET 0x13F91040 [16] = 1'b1 */
		writel(BIT(16), MFG_RPC_SLP_PROT_EN_SET);
		/* RX: MFG_RPC_SLP_PROT_EN_SET 0x13F91040 [18] = 1'b1 */
		writel(BIT(18), MFG_RPC_SLP_PROT_EN_SET);
		/* RX: MFG_RPC_SLP_PROT_EN_STA 0x13F91048 [16] = 1'b1 */
		/* RX: MFG_RPC_SLP_PROT_EN_STA 0x13F91048 [18] = 1'b1 */
		i = 0;
		while ((readl(MFG_RPC_SLP_PROT_EN_STA) & BIT(16)) != BIT(16) ||
				(readl(MFG_RPC_SLP_PROT_EN_STA) & BIT(18)) != BIT(18)) {
			udelay(10);
			if (++i > 1000) {
				__gpufreq_footprint_power_step(0xF5);
				goto timeout;
			}
		}
		/* EMI: EMISYS_PROTECT_EN_SET_0 0x10001C64 [19:18] = 2'b11 */
		writel(GENMASK(19, 18), EMISYS_PROTECT_EN_SET_0);
		/* EMI: EMISYS_PROTECT_RDY_STA_0 0x10001CAC [19:18] = 2'b11 */
		i = 0;
		while ((readl(EMISYS_PROTECT_RDY_STA_0) & GENMASK(19, 18)) != GENMASK(19, 18)) {
			udelay(10);
			if (++i > 1000) {
				__gpufreq_footprint_power_step(0xF6);
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
				__gpufreq_footprint_power_step(0xF7);
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
		/* Delay after mfg1 power off */
		udelay(10);
	}

	return;

timeout:
	GPUFREQ_LOGE("(0x13F91070)=0x%x, (0x10001CAC)=0x%x, (0x13F91048)=0x%x, (0x10001C6C)=0x%x",
		readl(MFG_RPC_MFG1_PWR_CON), readl(MD_MFGSYS_PROTECT_RDY_STA_0),
		readl(MFG_RPC_SLP_PROT_EN_STA), readl(EMISYS_PROTECT_RDY_STA_0));
	GPUFREQ_LOGE("(0x1021C82C)=0x%x, (0x1021C830)=0x%x",
		readl(NTH_MFG_EMI1_GALS_SLV_DBG), readl(NTH_MFG_EMI0_GALS_SLV_DBG));
	__gpufreq_abort("timeout");
}
#endif /* GPUFREQ_SELF_CTRL_MTCMOS */

static int __gpufreq_mtcmos_control(enum gpufreq_power_state power)
{
	int ret = GPUFREQ_SUCCESS;
	u32 val = 0;

	GPUFREQ_TRACE_START("power=%d", power);

	if (power == POWER_ON) {
		/* MFG1 on by CCF */
#if GPUFREQ_SELF_CTRL_MTCMOS
		__gpufreq_mfg1_rpc_control(POWER_ON);
#else
		ret = pm_runtime_get_sync(g_mtcmos->mfg1_dev);
		if (unlikely(ret < 0)) {
			__gpufreq_abort("fail to enable mfg1_dev (%d)", ret);
			goto done;
		}
#endif /* GPUFREQ_SELF_CTRL_MTCMOS */
		g_gpu.mtcmos_count++;

#if GPUFREQ_CHECK_MFG_PWR_STATUS
		/* PDC control, check MFG0-1 power status */
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
		__gpufreq_mfgx_rpc_control(POWER_ON, MFG_RPC_MFG9_PWR_CON);
		__gpufreq_mfgx_rpc_control(POWER_ON, MFG_RPC_MFG10_PWR_CON);
		__gpufreq_mfgx_rpc_control(POWER_ON, MFG_RPC_MFG11_PWR_CON);
		__gpufreq_mfgx_rpc_control(POWER_ON, MFG_RPC_MFG12_PWR_CON);
#else
		/* manually control MFG2, 9-12 if PDC is disabled */
		ret = pm_runtime_get_sync(g_mtcmos->mfg2_dev);
		if (unlikely(ret < 0)) {
			__gpufreq_abort("fail to enable mfg2_dev (%d)", ret);
			goto done;
		}

		if (g_shader_present & MFG9_SHADER_STACK0) {
			ret = pm_runtime_get_sync(g_mtcmos->mfg9_dev);
			if (unlikely(ret < 0)) {
				__gpufreq_abort("fail to enable  mfg9_dev (%d)", ret);
				goto done;
			}
		}
		if (g_shader_present & MFG10_SHADER_STACK2) {
			ret = pm_runtime_get_sync(g_mtcmos->mfg10_dev);
			if (unlikely(ret < 0)) {
				__gpufreq_abort("fail to enable  mfg10_dev (%d)", ret);
				goto done;
			}
		}
		if (g_shader_present & MFG11_SHADER_STACK4) {
			ret = pm_runtime_get_sync(g_mtcmos->mfg11_dev);
			if (unlikely(ret < 0)) {
				__gpufreq_abort("fail to enable mfg11_dev (%d)", ret);
				goto done;
			}
		}
		if (g_shader_present & MFG12_SHADER_STACK6) {
			ret = pm_runtime_get_sync(g_mtcmos->mfg12_dev);
			if (unlikely(ret < 0)) {
				__gpufreq_abort("fail to enable mfg12_dev (%d)", ret);
				goto done;
			}
		}
#endif /* GPUFREQ_SELF_CTRL_MTCMOS */

#if GPUFREQ_CHECK_MFG_PWR_STATUS
		val = MFG_0_12_PWR_STATUS & MFG_0_12_PWR_MASK;
		if (unlikely(val != MFG_0_12_PWR_MASK)) {
			__gpufreq_abort("incorrect MFG0-12 power on status: 0x%08x", val);
			ret = GPUFREQ_EINVAL;
			goto done;
		}
#endif /* GPUFREQ_CHECK_MFG_PWR_STATUS */
#endif /* GPUFREQ_PDCA_ENABLE */
	} else {
#if !GPUFREQ_PDCA_ENABLE
#if GPUFREQ_SELF_CTRL_MTCMOS
		__gpufreq_mfgx_rpc_control(POWER_OFF, MFG_RPC_MFG12_PWR_CON);
		__gpufreq_mfgx_rpc_control(POWER_OFF, MFG_RPC_MFG11_PWR_CON);
		__gpufreq_mfgx_rpc_control(POWER_OFF, MFG_RPC_MFG10_PWR_CON);
		__gpufreq_mfgx_rpc_control(POWER_OFF, MFG_RPC_MFG9_PWR_CON);
		__gpufreq_mfgx_rpc_control(POWER_OFF, MFG_RPC_MFG2_PWR_CON);
#else
		/* manually control MFG2,9-12 if PDC is disabled */
		if (g_shader_present & MFG12_SHADER_STACK6) {
			ret = pm_runtime_put_sync(g_mtcmos->mfg12_dev);
			if (unlikely(ret < 0)) {
				__gpufreq_abort("fail to disable mfg12_dev (%d)", ret);
				goto done;
			}
		}
		if (g_shader_present & MFG11_SHADER_STACK4) {
			ret = pm_runtime_put_sync(g_mtcmos->mfg11_dev);
			if (unlikely(ret < 0)) {
				__gpufreq_abort("fail to disable mfg11_dev (%d)", ret);
				goto done;
			}
		}
		if (g_shader_present & MFG10_SHADER_STACK2) {
			ret = pm_runtime_put_sync(g_mtcmos->mfg10_dev);
			if (unlikely(ret < 0)) {
				__gpufreq_abort("fail to disable  mfg10_dev (%d)", ret);
				goto done;
			}
		}
		if (g_shader_present & MFG9_SHADER_STACK0) {
			ret = pm_runtime_put_sync(g_mtcmos->mfg9_dev);
			if (unlikely(ret < 0)) {
				__gpufreq_abort("fail to disable  mfg9_dev (%d)", ret);
				goto done;
			}
		}

		ret = pm_runtime_put_sync(g_mtcmos->mfg2_dev);
		if (unlikely(ret < 0)) {
			__gpufreq_abort("fail to enable mfg2_dev (%d)", ret);
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

#if GPUFREQ_CHECK_MFG_PWR_STATUS
		val = MFG_0_12_PWR_STATUS & MFG_1_12_PWR_MASK;
		if (unlikely(val)) {
			__gpufreq_abort("incorrect MFG1-12 power on status: 0x%08x", val);
			ret = GPUFREQ_EINVAL;
			goto done;
		}
#endif /* GPUFREQ_CHECK_MFG_PWR_STATUS */
	}

#if !GPUFREQ_SELF_CTRL_MTCMOS || GPUFREQ_CHECK_MFG_PWR_STATUS
done:
#endif /* GPUFREQ_SELF_CTRL_MTCMOS */
	GPUFREQ_TRACE_END();

	return ret;
}

static int __gpufreq_buck_control(enum gpufreq_power_state power)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("power=%d", power);

	/* power on: VSRAM -> VA15 -> VGPU */
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

		/* clear BUCK_ISO  */
		udelay(3);
		/* SPM_SOC_BUCK_ISO_CON 0x1C001F80 [8] VGPU_EXT_BUCK_ISO = 1'b0 */
		writel((readl(SPM_SOC_BUCK_ISO_CON) & ~BIT(8)), SPM_SOC_BUCK_ISO_CON);
	/* power off: VGPU-> VA15 -> VSRAM */
	} else {
		/* set BUCK_ISO  */
		/* SPM_SOC_BUCK_ISO_CON 0x1C001F80 [8] VGPU_EXT_BUCK_ISO = 1'b1 */
		writel((readl(SPM_SOC_BUCK_ISO_CON) | BIT(8)), SPM_SOC_BUCK_ISO_CON);
		udelay(3);

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

static void __gpufreq_check_bus_idle(void)
{
	/* MFG_QCHANNEL_CON 0x13FBF0B4 [0] MFG_ACTIVE_SEL = 1'b1 */
	writel((readl(MFG_QCHANNEL_CON) | BIT(0)), MFG_QCHANNEL_CON);

	/* MFG_DEBUG_SEL 0x13FBF170 [1:0] MFG_DEBUG_TOP_SEL = 2'b11 */
	writel((readl(MFG_DEBUG_SEL) | GENMASK(1, 0)), MFG_DEBUG_SEL);

	/*
	 * polling MFG_DEBUG_TOP 0x13FBF178 [0] MFG_DEBUG_TOP
	 * 0x0: bus idle
	 * 0x1: bus busy
	 */
	do {} while (readl(MFG_DEBUG_TOP) & BIT(0));
}

/* API: init first OPP idx by init freq set in preloader */
static int __gpufreq_init_opp_idx(void)
{
	struct gpufreq_opp_info *working_table = g_gpu.working_table;
	int target_oppidx = -1;
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START();

	/* get current GPU OPP idx by freq set in preloader */
	g_gpu.cur_oppidx = __gpufreq_get_idx_by_fgpu(g_gpu.cur_freq);

	/* decide first OPP idx by custom setting */
	if (__gpufreq_custom_init_enable()) {
		target_oppidx = GPUFREQ_CUST_INIT_OPPIDX;
	} else
		target_oppidx = g_gpu.cur_oppidx;

	GPUFREQ_LOGI(
		"init GPU[%d] F(%d->%d) V(%d->%d), VSRAM(%d->%d)",
		target_oppidx,
		g_gpu.cur_freq, working_table[target_oppidx].freq,
		g_gpu.cur_volt, working_table[target_oppidx].volt,
		g_gpu.cur_vsram, working_table[target_oppidx].vsram);

	/* init first OPP index */
	if (!__gpufreq_dvfs_enable()) {
		g_dvfs_state = DVFS_DISABLE;
		GPUFREQ_LOGI("DVFS state: 0x%x, disable DVFS", g_dvfs_state);

		/* set OPP once if DVFS is disabled but custom init is enabled */
		if (__gpufreq_custom_init_enable())
			ret = __gpufreq_generic_commit_gpu(target_oppidx, DVFS_DISABLE);
	} else {
		g_dvfs_state = DVFS_FREE;
		GPUFREQ_LOGI("DVFS state: 0x%x, enable DVFS", g_dvfs_state);

		ret = __gpufreq_generic_commit_gpu(target_oppidx, DVFS_FREE);
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

	/* update current status to shared memory */
	if (g_shared_status) {
		g_shared_status->cur_power_gpu = working_table[g_gpu.cur_oppidx].power;
		g_shared_status->max_power_gpu = working_table[g_gpu.max_oppidx].power;
		g_shared_status->min_power_gpu = working_table[g_gpu.min_oppidx].power;
	}
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
#endif /* GPUFREQ_ASENSOR_ENABLE */

#if GPUFREQ_ASENSOR_ENABLE
/* API: pause dvfs to given freq and volt */
static int __gpufreq_pause_dvfs(void)
{
	int ret = GPUFREQ_SUCCESS;
	unsigned int cur_fgpu = 0, cur_vgpu = 0, target_fgpu = 0, target_vgpu = 0;
	unsigned int cur_vsram = 0, target_vsram = 0;

	GPUFREQ_TRACE_START();

	__gpufreq_set_dvfs_state(true, DVFS_AGING_KEEP);

	ret = __gpufreq_power_control(POWER_ON);
	if (unlikely(ret < 0)) {
		__gpufreq_set_dvfs_state(false, DVFS_AGING_KEEP);
		goto done;
	}

	mutex_lock(&gpufreq_lock);

	/* prepare GPU setting */
	cur_fgpu = g_gpu.cur_freq;
	cur_vgpu = g_gpu.cur_volt;
	cur_vsram = g_gpu.cur_vsram;

	target_fgpu = GPUFREQ_AGING_KEEP_FGPU;
	target_vgpu = GPUFREQ_AGING_KEEP_VGPU;
	target_vsram = GPUFREQ_AGING_KEEP_VSRAM;

	GPUFREQ_LOGD("begin to commit GPU Freq: (%d->%d), Volt: (%d->%d)",
		cur_fgpu, target_fgpu, cur_vgpu, target_vgpu);

	ret = __gpufreq_generic_scale_gpu(cur_fgpu, target_fgpu,
		cur_vgpu, target_vgpu, cur_vsram, target_vsram);
	if (ret) {
		GPUFREQ_LOGE("fail to scale GPU: Freq(%d->%d), Volt(%d->%d), Vsram(%d->%d)",
			cur_fgpu, target_fgpu, cur_vgpu, target_vgpu,
			cur_vsram, target_vsram);
		goto done_unlock;
	}

	GPUFREQ_LOGI("pause DVFS at GPU(%d, %d), VSRAM(%d), state: 0x%x",
		target_fgpu, target_vgpu, target_vsram, g_dvfs_state);

done_unlock:
	mutex_unlock(&gpufreq_lock);
done:
	GPUFREQ_TRACE_END();

	return ret;
}
#endif /* GPUFREQ_ASENSOR_ENABLE */

/*
 * API: interpolate OPP of none AVS idx.
 * step = (large - small) / range
 * vnew = large - step * j
 */
static void __gpufreq_interpolate_volt(void)
{
	unsigned int large_volt = 0, small_volt = 0;
	unsigned int large_freq = 0, small_freq = 0;
	unsigned int inner_volt = 0, inner_freq = 0;
	unsigned int previous_volt = 0;
	int i = 0, j = 0;
	int front_idx = 0, rear_idx = 0, inner_idx = 0;
	int range = 0, slope = 0;
	int adj_num = NUM_GPU_BINNING_IDX;
	const int *binning_idx = g_gpu_binning_idx;
	struct gpufreq_opp_info *signed_table = g_gpu.signed_table;

	mutex_lock(&gpufreq_lock);

	for (i = 1; i < adj_num; i++) {
		front_idx = binning_idx[i - 1];
		rear_idx = binning_idx[i];
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

		GPUFREQ_LOGD("GPU[%02d*] Freq: %d, Volt: %d, slope: %d",
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
				__gpufreq_abort("invalid GPU[%02d*] Volt: %d < [%02d*] Volt: %d",
					inner_idx, inner_volt, inner_idx + 1, previous_volt);

			/* record margin */
			signed_table[inner_idx].margin += signed_table[inner_idx].volt - inner_volt;
			/* update to signed table */
			signed_table[inner_idx].volt = inner_volt;
			signed_table[inner_idx].vsram = __gpufreq_get_vsram_by_vlogic(inner_volt);

			GPUFREQ_LOGD("GPU[%02d*] Freq: %d, Volt: %d, Vsram: %d",
				inner_idx, inner_freq * 1000, inner_volt,
				signed_table[inner_idx].vsram);
		}
		GPUFREQ_LOGD("GPU[%02d*] Freq: %d, Volt: %d",
			front_idx, large_freq * 1000, large_volt / 100);
	}

	mutex_unlock(&gpufreq_lock);
}

#if GPUFREQ_ASENSOR_ENABLE
/* API: get Aging sensor data from EFUSE, return if success*/
static unsigned int __gpufreq_asensor_read_efuse(u32 *a_t0_efuse1,
	u32 *a_t0_efuse2, u32 *a_t0_efuse3, u32 *efuse_error)
{
	u32 efuse1 = 0, efuse2 = 0, efuse3 = 0;

	/* efuse1: 0x11E30A6C */
	efuse1 = readl(EFUSE_ASENSOR_RT);
	/* efuse2: 0x11E30A70 */
	efuse2 = readl(EFUSE_ASENSOR_HT);
	/* efuse3: 0x11E305C4 */
	efuse3 = readl(EFUSE_ASENSOR_TEMPER);

	if (!efuse1)
		return false;

	GPUFREQ_LOGD("efuse1: 0x%08x, efuse2: 0x%08x, efuse3: 0x%08x", efuse1, efuse2, efuse3);

	/* A_T0_LVT_RT: 0x11E30A6C [9:0], shift value: 1200 */
	*a_t0_efuse1 = (efuse1  & GENMASK(9, 0)) + 0x4B0;
	/* A_T0_ULVT_RT: 0x11E30A6C [19:10], shift value: 1800 */
	*a_t0_efuse2 = ((efuse1 & GENMASK(19, 10)) >> 10) + 0x708;
	/* A_T0_ULVTLL_RT: 0x11E30A6C [29:20], shift value:800 */
	*a_t0_efuse3 = ((efuse1 & GENMASK(29, 20)) >> 20) + 0x320;
	/* efuse_error   : 0x11E30A6C [31] */
	*efuse_error = ((efuse1 & BIT(31)) >> 31);

	g_asensor_info.efuse1 = efuse1;
	g_asensor_info.efuse2 = efuse2;
	g_asensor_info.efuse3 = efuse3;
	g_asensor_info.efuse1_addr = 0x11E30A6C;
	g_asensor_info.efuse2_addr = 0x11E30A70;
	g_asensor_info.efuse3_addr = 0x11E305C4;
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
#endif /* GPUFREQ_ASENSOR_ENABLE */

#if GPUFREQ_ASENSOR_ENABLE
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
	while (readl(MFG_CPE_CTRL_MCU_REG_CPEIRQSTS) != BIT(31)) {
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
	*a_tn_sensor2 = (aging_data2 & GENMASK(15, 0));
	/* A_TN_ULVTLL_CNT 0x13FB600C [31:16] */
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
#endif /* GPUFREQ_ASENSOR_ENABLE */

#if GPUFREQ_ASENSOR_ENABLE
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

	leakage_power = __gpufreq_get_lkg_pstack(GPUFREQ_AGING_LKG_VGPU);

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

	GPUFREQ_LOGI("Aging Sensor choose aging table id: %d", aging_table_idx);
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
	u32 val = 0;
	int i = 0, oppidx = 0;
	unsigned int temp_freq = 0, volt_ofs = 0;
	int adj_num = NUM_GPU_BINNING_IDX;

	/*
	 * Read AVS efuse
	 *
	 * Freq (MHz) | Signedoff Volt (V) | Efuse name | Efuse address
	 * ============================================================
	 * 1130       | 0.85               | PTPOD21    | 0x11E3_05C0
	 * 675        | 0.675              | PTPOD22    | 0x11E3_05C4
	 * 330        | 0.575              | PTPOD23    | 0x11E3_05C8
	 *
	 * 0.575 will not be lowered, but will increase according to
	 * the binning result for rescue yeild.
	 */
	/* read AVS efuse and compute Freq and Volt */
	for (i = 0; i < adj_num; i++) {
		oppidx = g_avs_table[i].oppidx;
		if (g_mcl50_load) {
			if (i == 0)
				val = 0x00D9A089;
			else if (i == 1)
				val = 0x00C2B8A6;
			else if (i == 2)
				val = 0x00CB0495;
			else
				val = 0;
		} else if (g_avs_enable) {
			if (i == 0)
				val = readl(EFUSE_PTPOD21);
			else if (i == 1)
				val = readl(EFUSE_PTPOD22);
			else if (i == 2)
				val = readl(EFUSE_PTPOD23);
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
			__gpufreq_abort("OPP[%02d*]: AVS efuse[%d].freq(%d) != signed-off.freq(%d)",
				oppidx, i, g_avs_table[i].freq, g_gpu.signed_table[oppidx].freq);
			return;
		}
		g_avs_table[i].freq = temp_freq;

		/* compute Volt from efuse */
		g_avs_table[i].volt = __gpufreq_compute_avs_volt(val);

		/* AVS margin is set if any OPP is adjusted by AVS */
		g_avs_margin = true;

		/* update Vsram */
		g_avs_table[i].vsram = __gpufreq_get_vsram_by_vlogic(g_avs_table[i].volt);

		/* PTPOD21 [31:24] GPU_PTP_version */
		if (i == 0)
			g_ptp_version = (val & GENMASK(31, 24)) >> 24;
	}

	/* check AVS Volt and update Vsram */
	for (i = adj_num - 1; i >= 0; i--) {
		/* if AVS Volt is not set */
		if (!g_avs_table[i].volt)
			continue;

		oppidx = g_avs_table[i].oppidx;
		/* mV * 100 */
		volt_ofs = 1250;

		/*
		 * AVS Volt reverse check, start from adj_num -2
		 * Volt of sign-off[i] should always be larger than sign-off[i + 1]
		 * if not, add Volt offset to sign-off[i]
		 */
		if (i != (adj_num - 1)) {
			if (g_avs_table[i].volt <= g_avs_table[i + 1].volt) {
				GPUFREQ_LOGW("AVS efuse[%02d].volt(%d) <= efuse[%02d].volt(%d)",
					i, g_avs_table[i].volt, i + 1, g_avs_table[i + 1].volt);
				g_avs_table[i].volt = g_avs_table[i + 1].volt + volt_ofs;
			}
		}

		/* clamp to signoff Volt */
		if (g_avs_table[i].volt > g_gpu.signed_table[oppidx].volt) {
			GPUFREQ_LOGW("OPP[%02d*]: AVS efuse[%d].volt(%d) > signed-off.volt(%d)",
				oppidx, i, g_avs_table[i].volt, g_gpu.signed_table[oppidx].volt);
			g_avs_table[i].volt = g_gpu.signed_table[oppidx].volt;
		}
	}

	for (i = 0; i < adj_num; i++)
		GPUFREQ_LOGI("GPU OPP[%02d*]: efuse[%02d] freq(%d), volt(%d)",
			g_avs_table[i].oppidx, i, g_avs_table[i].freq,
			g_avs_table[i].volt);
}

static void __gpufreq_apply_adjustment(void)
{
	struct gpufreq_opp_info *signed_gpu = NULL;
	int adj_num_gpu = 0, oppidx = 0, i = 0;
	unsigned int avs_volt = 0, avs_vsram = 0, aging_volt = 0;

	signed_gpu = g_gpu.signed_table;
	adj_num_gpu = NUM_GPU_BINNING_IDX;

	/* apply AVS margin */
	if (g_avs_margin) {
		/* GPU AVS */
		for (i = 0; i < adj_num_gpu; i++) {
			oppidx = g_avs_table[i].oppidx;
			avs_volt = g_avs_table[i].volt ?
				g_avs_table[i].volt : signed_gpu[oppidx].volt;
			avs_vsram = g_avs_table[i].vsram ?
				g_avs_table[i].vsram : signed_gpu[oppidx].vsram;
			/* record margin */
			signed_gpu[oppidx].margin += signed_gpu[oppidx].volt - avs_volt;
			/* update to signed table */
			signed_gpu[oppidx].volt = avs_volt;
			signed_gpu[oppidx].vsram = avs_vsram;
		}
	} else
		GPUFREQ_LOGI("AVS margin is not set");

	/* apply Aging margin */
	if (g_aging_margin) {
		/* GPU Aging */
		for (i = 0; i < adj_num_gpu; i++) {
			oppidx = g_aging_table[g_aging_table_idx][i].oppidx;
			aging_volt = g_aging_table[g_aging_table_idx][i].volt;
			/* record margin */
			signed_gpu[oppidx].margin += aging_volt;
			/* update to signed table */
			signed_gpu[oppidx].volt -= aging_volt;
			signed_gpu[oppidx].vsram =
				__gpufreq_get_vsram_by_vlogic(signed_gpu[oppidx].volt);
		}
	} else
		GPUFREQ_LOGI("Aging margin is not set");

	/* compute others OPP exclude signoff idx */
	__gpufreq_interpolate_volt();
}

static void __gpufreq_init_shader_present(void)
{
	unsigned int segment_id = 0;

	segment_id = g_gpu.segment_id;

	switch (segment_id) {
	default:
		g_shader_present = GPU_SHADER_PRESENT_4;
	}
	GPUFREQ_LOGD("segment_id: %d, shader_present: %d", segment_id, g_shader_present);
}

/*
 * 1. init OPP segment range
 * 2. init segment/working OPP table
 * 3. apply aging
 * 3. init power measurement
 */
static void __gpufreq_init_opp_table(void)
{
	unsigned int segment_id = 0;
	int i = 0, j = 0;

	/* init current GPU freq and volt set by preloader */
	g_gpu.cur_freq = __gpufreq_get_real_fgpu();
	g_gpu.cur_volt = __gpufreq_get_real_vgpu();
	g_gpu.cur_vsram = __gpufreq_get_real_vsram();
	g_stack.cur_freq = __gpufreq_get_real_fstack();

	GPUFREQ_LOGI("preloader init [GPU] Freq: %d, Volt: %d, Vsram: %d",
		g_gpu.cur_freq, g_gpu.cur_volt, g_gpu.cur_vsram);

	/* init GPU OPP table */
	/* init OPP segment range */
	segment_id = g_gpu.segment_id;
	if (segment_id == MT6886_SEGMENT)
		g_gpu.segment_upbound = 0;
	else
		g_gpu.segment_upbound = 8;
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
	GPUFREQ_LOGD("number of signed GPU OPP: %d, upper and lower bound: [%d, %d]",
		g_gpu.signed_opp_num, g_gpu.segment_upbound, g_gpu.segment_lowbound);
	GPUFREQ_LOGI("number of working GPU OPP: %d, max and min OPP index: [%d, %d]",
		g_gpu.opp_num, g_gpu.max_oppidx, g_gpu.min_oppidx);

	/* update signed OPP table from MFGSYS features */

	/* compute Aging table based on Aging Sensor */
	__gpufreq_compute_aging();
	/* compute AVS table based on EFUSE */
	__gpufreq_compute_avs();
	/* apply Segment/Aging/AVS/... adjustment to signed OPP table  */
	__gpufreq_apply_adjustment();

	/* after these, signed table is settled down */

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

	/* set power info to working table */
	__gpufreq_measure_power();
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
		segment_id = MT6886_SEGMENT;
		GPUFREQ_LOGW("unknown efuse id: 0x%x", efuse_id);
		break;
	}

	GPUFREQ_LOGI("efuse_id: 0x%x, segment_id: %d", efuse_id, segment_id);

	g_gpu.segment_id = segment_id;
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
		__gpufreq_abort("fail to get  mfg10_dev (%ld)", ret);
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

	g_clk->clk_main_parent = devm_clk_get(&pdev->dev, "clk_main_parent");
	if (IS_ERR(g_clk->clk_main_parent)) {
		ret = PTR_ERR(g_clk->clk_main_parent);
		__gpufreq_abort("fail to get clk_main_parent (%ld)", ret);
		goto done;
	}

	g_clk->clk_sc_main_parent = devm_clk_get(&pdev->dev, "clk_sc_main_parent");
	if (IS_ERR(g_clk->clk_sc_main_parent)) {
		ret = PTR_ERR(g_clk->clk_sc_main_parent);
		__gpufreq_abort("fail to get clk_sc_main_parent (%ld)", ret);
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

	g_pmic->reg_vgpu = regulator_get_optional(&pdev->dev, "vgpu");
	if (IS_ERR(g_pmic->reg_vgpu)) {
		ret = PTR_ERR(g_pmic->reg_vgpu);
		__gpufreq_abort("fail to get VGPU (%ld)", ret);
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

	/* feature config */
	g_asensor_enable = GPUFREQ_ASENSOR_ENABLE;
	g_avs_enable = GPUFREQ_AVS_ENABLE;
	g_gpm1_mode = GPUFREQ_GPM1_ENABLE;
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

	/* 0x13FA0400 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "sensor_pll");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource SENSOR_PLL");
		goto done;
	}
	g_sensor_pll_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_sensor_pll_base)) {
		GPUFREQ_LOGE("fail to ioremap SENSOR_PLL: 0x%llx", res->start);
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

	/* 0x10270000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "nth_emicfg_ao_mem");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource NTH_EMICFG_AO_MEM_REG");
		goto done;
	}
	g_nth_emicfg_ao_mem_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_nth_emicfg_ao_mem_base)) {
		GPUFREQ_LOGE("fail to ioremap NTH_EMICFG_AO_MEM_REG: 0x%llx", res->start);
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

	/* 0x11E30000 */
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
		GPUFREQ_LOGE("fail to get resource MFG_CPE_SENSOR");
		goto done;
	}
	g_mfg_cpe_sensor_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_mfg_cpe_sensor_base)) {
		GPUFREQ_LOGE("fail to ioremap MFG_CPE_SENSOR: 0x%llx", res->start);
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
	GPUFREQ_TRACE_END();

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
		GPUFREQ_LOGI("gpufreq platform probe only init reg_base/pmic/fp in EB mode");
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
	ret = gpuppm_init(TARGET_GPU, g_gpueb_support, GPUPPM_DEFAULT_IDX);
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
	dev_pm_domain_detach(g_mtcmos->mfg12_dev, true);
	dev_pm_domain_detach(g_mtcmos->mfg11_dev, true);
	dev_pm_domain_detach(g_mtcmos->mfg10_dev, true);
	dev_pm_domain_detach(g_mtcmos->mfg9_dev, true);
	dev_pm_domain_detach(g_mtcmos->mfg2_dev, true);
#endif /* GPUFREQ_PDCA_ENABLE */
	dev_pm_domain_detach(g_mtcmos->mfg1_dev, true);
#endif /* GPUFREQ_SELF_CTRL_MTCMOS */

	kfree(g_gpu.working_table);
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
