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

#include <gpufreq_v2.h>
#include <gpufreq_debug.h>
#include <gpuppm.h>
#include <gpufreq_common.h>
#include <gpufreq_mt6855.h>
#include <gpudfd_mt6855.h>
#include <gpueb_debug.h>
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
static void __gpufreq_apply_adjust(struct gpufreq_adj_info *adj_table, int adj_num);
/* dvfs function */
static int __gpufreq_generic_scale_gpu(
	unsigned int freq_old, unsigned int freq_new,
	unsigned int volt_old, unsigned int volt_new,
	unsigned int vsram_old, unsigned int vsram_new);
static int __gpufreq_custom_commit_gpu(unsigned int target_freq,
	unsigned int target_volt, enum gpufreq_dvfs_state key);
static int __gpufreq_freq_scale_gpu(unsigned int freq_old, unsigned int freq_new);
static int __gpufreq_volt_scale_gpu(
	unsigned int vgpu_old, unsigned int vgpu_new,
	unsigned int vsram_old, unsigned int vsram_new);
static void __gpufreq_vsram_scale(unsigned int vsram);
static void __gpufreq_switch_clksrc(enum gpufreq_clk_src clksrc);
static void __gpufreq_switch_parksrc(enum gpufreq_clk_src clksrc);
static unsigned int __gpufreq_calculate_pcw(unsigned int freq, enum gpufreq_posdiv posdiv_power);
static unsigned int __gpufreq_settle_time_vgpu(unsigned int direction, int deltaV);
/* get function */
static unsigned int __gpufreq_get_fmeter_fgpu(void);
static unsigned int __gpufreq_execute_fmeter(enum gpufreq_clk_src clksrc);
static unsigned int __gpufreq_get_real_fgpu(void);
static unsigned int __gpufreq_get_real_vgpu(void);
static unsigned int __gpufreq_get_real_vsram(void);
static enum gpufreq_posdiv __gpufreq_get_real_posdiv_gpu(void);
static enum gpufreq_posdiv __gpufreq_get_posdiv_by_fgpu(unsigned int freq);
/* aging sensor function */
#if GPUFREQ_ASENSOR_ENABLE
static unsigned int __gpufreq_asensor_read_efuse(u32 *a_t0_efuse1, u32 *a_t0_efuse2,
	u32 *efuse_error);
static void __gpufreq_asensor_read_register(u32 *a_tn_sensor1, u32 *a_tn_sensor2);
static unsigned int __gpufreq_get_aging_table_idx(
	u32 a_t0_efuse1, u32 a_t0_efuse2, u32 a_tn_sensor1, u32 a_tn_sensor2,
	u32 efuse_error, unsigned int is_efuse_read_success);
#endif /* GPUFREQ_ASENSOR_ENABLE */
/* power control function */
static int __gpufreq_clock_control(enum gpufreq_power_state power);
static int __gpufreq_mtcmos_control(enum gpufreq_power_state power);
static int __gpufreq_buck_control(enum gpufreq_power_state power);
static void __gpufreq_aoc_control(enum gpufreq_power_state power);
static void __gpufreq_transaction_control(enum gpufreq_transaction_mode mode);
static void __gpufreq_hw_dcm_control(void);
static void __gpufreq_hwapm_control(void);
/* init function */
static void __gpufreq_segment_adjustment(void);
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
static void __iomem *g_mfg_rpc_base;
static void __iomem *g_mfg_top_base;
static void __iomem *g_sleep;
static void __iomem *g_nth_emicfg_base;
static void __iomem *g_nth_emicfg_ao_mem_base;
static void __iomem *g_nth_emi_mpu_base;
static void __iomem *g_fmem_ao_debug_ctrl;
static void __iomem *g_infracfg_ao_base;
static void __iomem *g_infra_ao_debug_ctrl;
static void __iomem *g_infra_ao1_debug_ctrl;
static void __iomem *g_efuse_base;
static void __iomem *g_mfg_cpe_sensor_base;
static void __iomem *g_rgx_base;
static struct gpufreq_pmic_info *g_pmic;
static struct gpufreq_clk_info *g_clk;
static struct gpufreq_mtcmos_info *g_mtcmos;
static struct gpufreq_status g_gpu;
static struct gpufreq_asensor_info g_asensor_info;
static unsigned int g_stress_test_enable;
static unsigned int g_gpueb_support;
static unsigned int g_aging_enable;
static unsigned int g_avs_enable;
static unsigned int g_aging_load;
static unsigned int g_mcl50_load;
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
	.check_bus_idle = __gpufreq_check_bus_idle,
	.dump_infra_status = __gpufreq_dump_infra_status,
	.set_stress_test = __gpufreq_set_stress_test,
	.set_aging_mode = __gpufreq_set_aging_mode,
	.get_asensor_info = __gpufreq_get_asensor_info,
};

static struct gpufreq_platform_fp platform_eb_fp = {
	.bringup = __gpufreq_bringup,
	.check_bus_idle = __gpufreq_check_bus_idle,
	.dump_infra_status = __gpufreq_dump_infra_status,
	.get_dyn_pgpu = __gpufreq_get_dyn_pgpu,
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
	opp_info.aging_enable = g_aging_enable;
	opp_info.avs_enable = g_avs_enable;
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

/* API: get Volt of SRAM via volt of GPU */
unsigned int __gpufreq_get_vsram_by_vgpu(unsigned int volt)
{
	if (volt > VSRAM_LEVEL_0)
		return VSRAM_LEVEL_1;
	else
		return VSRAM_LEVEL_0;
}

/* API: get leakage Power of GPU */
unsigned int __gpufreq_get_lkg_pgpu(unsigned int volt)
{
	GPUFREQ_UNREFERENCED(volt);

	return GPU_LEAKAGE_POWER;
}

/* API: get leakage Power of STACK */
unsigned int __gpufreq_get_lkg_pstack(unsigned int volt)
{
	GPUFREQ_UNREFERENCED(volt);

	return 0;
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
		__gpufreq_footprint_power_step(0x01);

		/* control AOC after MFG_0 on */
		__gpufreq_aoc_control(POWER_ON);
		__gpufreq_footprint_power_step(0x02);

		/* control Buck */
		ret = __gpufreq_buck_control(POWER_ON);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to control Buck: On (%d)", ret);
			ret = GPUFREQ_EINVAL;
			goto done_unlock;
		}
		__gpufreq_footprint_power_step(0x03);

		/* control MTCMOS */
		ret = __gpufreq_mtcmos_control(POWER_ON);
		if (unlikely(ret < 0)) {
			GPUFREQ_LOGE("fail to control MTCMOS: On (%d)", ret);
			ret = GPUFREQ_EINVAL;
			goto done_unlock;
		}
		__gpufreq_footprint_power_step(0x04);

		/* control clock */
		ret = __gpufreq_clock_control(POWER_ON);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to control CLOCK: On (%d)", ret);
			ret = GPUFREQ_EINVAL;
			goto done_unlock;
		}
		__gpufreq_footprint_power_step(0x05);

		/* set HWAPM register when power on, let GPU DDK control MTCMOS itself */
		__gpufreq_hwapm_control();
		__gpufreq_footprint_power_step(0x06);

		/* control HWDCM */
		__gpufreq_hw_dcm_control();
		__gpufreq_footprint_power_step(0x07);

		/* control bus transaction mode */
		__gpufreq_transaction_control(MERGER_LIGHT);
		__gpufreq_footprint_power_step(0x08);

		/* disable RGX secure protect */
		gpu_set_rgx_bus_secure();
		__gpufreq_footprint_power_step(0x09);

		/* free DVFS when power on */
		g_dvfs_state &= ~DVFS_POWEROFF;
		__gpufreq_footprint_power_step(0x0A);
	} else if (power == POWER_OFF && g_gpu.power_count == 0) {
		__gpufreq_footprint_power_step(0x0B);

		/* freeze DVFS when power off */
		g_dvfs_state |= DVFS_POWEROFF;
		__gpufreq_footprint_power_step(0x0C);

		/* control clock */
		ret = __gpufreq_clock_control(POWER_OFF);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to control CLOCK: Off (%d)", ret);
			ret = GPUFREQ_EINVAL;
			goto done_unlock;
		}
		__gpufreq_footprint_power_step(0x0D);

		/* control MTCMOS */
		ret = __gpufreq_mtcmos_control(POWER_OFF);
		if (unlikely(ret < 0)) {
			GPUFREQ_LOGE("fail to control MTCMOS: Off (%d)", ret);
			ret = GPUFREQ_EINVAL;
			goto done_unlock;
		}
		__gpufreq_footprint_power_step(0x0E);

		/* control Buck */
		ret = __gpufreq_buck_control(POWER_OFF);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to control Buck: Off (%d)", ret);
			ret = GPUFREQ_EINVAL;
			goto done_unlock;
		}
		__gpufreq_footprint_power_step(0x0F);

		/* control AOC before MFG_0 off */
		__gpufreq_aoc_control(POWER_OFF);
		__gpufreq_footprint_power_step(0x10);
	}

	/* return power count if successfully control power */
	ret = g_gpu.power_count;

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

	/* randomly replace target index */
	if (g_stress_test_enable) {
		get_random_bytes(&target_oppidx, sizeof(target_oppidx));
		target_oppidx = target_oppidx < 0 ?
			(target_oppidx*-1) % opp_num_gpu : target_oppidx % opp_num_gpu;
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

	GPUFREQ_LOGD("begin to commit GPU OPP index: (%d->%d)",
		cur_oppidx_gpu, target_oppidx_gpu);

	ret = __gpufreq_generic_scale_gpu(cur_fgpu, target_fgpu,
		cur_vgpu, target_vgpu, cur_vsram_gpu, target_vsram_gpu);
	if (unlikely(ret)) {
		GPUFREQ_LOGE(
			"fail to scale Idx(%d->%d), Freq(%d->%d), Volt(%d->%d), Vsram(%d->%d)",
			cur_oppidx_gpu, target_oppidx_gpu, cur_fgpu, target_fgpu,
			cur_vgpu, target_vgpu, cur_vsram_gpu, target_vsram_gpu);
		goto done_unlock;
	}

	g_gpu.cur_oppidx = target_oppidx_gpu;

	__gpufreq_footprint_oppidx(target_oppidx_gpu);

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

		ret = __gpufreq_custom_commit_gpu(freq, volt, DVFS_DEBUG_KEEP);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to commit GPU Freq: %d, Volt: %d (%d)",
				freq, volt, ret);
			__gpufreq_set_dvfs_state(false, DVFS_DEBUG_KEEP);
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
		GPUFREQ_LOGI("[Regulator] Vgpu: %d, Vsram: %d",
			__gpufreq_get_real_vgpu(), __gpufreq_get_real_vsram());
		GPUFREQ_LOGI("[Clk] MFG_PLL: %d", __gpufreq_get_real_fgpu());
	} else {
		GPUFREQ_LOGI("GPU[%d] Freq: %d, Vgpu: %d, Vsram: %d",
			g_gpu.cur_oppidx, g_gpu.cur_freq,
			g_gpu.cur_volt, g_gpu.cur_vsram);
	}

	/* 0x13F90000, 0x13000000 */
	if (g_mfg_rpc_base && g_rgx_base) {
		/* MFG_GPU_EB_SPM_RPC_SLP_PROT_EN_STA */
		/* RGX_CR_SYS_BUS_SECURE */
		GPUFREQ_LOGI("%-7s (0x%x): 0x%08x, (0x%x): 0x%08x",
			"[MFG]",
			(0x13F90000 + 0x1048), readl(g_mfg_rpc_base + 0x1048),
			(0x13000000 + 0xA100), readl(g_rgx_base + 0xA100));
	}

	/* 0x1021C000, 0x10270000 */
	if (g_nth_emicfg_base && g_nth_emicfg_ao_mem_base) {
		/* NTH_EMICFG_REG_MFG_EMI1_GALS_SLV_DBG */
		/* NTH_EMICFG_REG_MFG_EMI0_GALS_SLV_DBG */
		/* NTH_EMICFG_AO_MEM_REG_M6M7_IDLE_BIT_EN_1 */
		/* NTH_EMICFG_AO_MEM_REG_M6M7_IDLE_BIT_EN_0 */
		GPUFREQ_LOGI("%-7s (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x",
			"[EMI]",
			(0x1021C000 + 0x82C), readl(g_nth_emicfg_base + 0x82C),
			(0x1021C000 + 0x830), readl(g_nth_emicfg_base + 0x830),
			(0x10270000 + 0x228), readl(g_nth_emicfg_ao_mem_base + 0x228),
			(0x10270000 + 0x22C), readl(g_nth_emicfg_ao_mem_base + 0x22C));
	}

	/* 0x10351000, 0x10042000 */
	if (g_nth_emi_mpu_base && g_fmem_ao_debug_ctrl) {
		/* SECURERANGE0 */
		/* SECURERANGE0_1 */
		/* SECURERANGE0_2 */
		/* FMEM_AO_BUS_U_DEBUG_CTRL_AO_FMEM_AO_CTRL0 */
		GPUFREQ_LOGI("%-7s (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x",
			"[EMI]",
			(0x10351000 + 0x1D8), readl(g_nth_emi_mpu_base + 0x1D8),
			(0x10351000 + 0x3D8), readl(g_nth_emi_mpu_base + 0x3D8),
			(0x10351000 + 0x5D8), readl(g_nth_emi_mpu_base + 0x5D8),
			(0x10042000 + 0x000), readl(g_fmem_ao_debug_ctrl + 0x000));
	}

	/* 0x10001000, 0x10023000, 0x1002B000 */
	if (g_infracfg_ao_base && g_infra_ao_debug_ctrl && g_infra_ao1_debug_ctrl) {
		/* MD_MFGSYS_PROTECT_EN_STA_0 */
		/* MD_MFGSYS_PROTECT_RDY_STA_0 */
		/* INFRA_AO_BUS_U_DEBUG_CTRL_AO_INFRA_AO_CTRL0 */
		/* INFRA_QAXI_AO_BUS_SUB1_U_DEBUG_CTRL_AO_INFRA_AO1_CTRL0 */
		GPUFREQ_LOGI("%-7s (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x",
			"[INFRA]",
			(0x10001000 + 0xCA0), readl(g_infracfg_ao_base + 0xCA0),
			(0x10001000 + 0xCAC), readl(g_infracfg_ao_base + 0xCAC),
			(0x10023000 + 0x000), readl(g_infra_ao_debug_ctrl + 0x000),
			(0x1002B000 + 0x000), readl(g_infra_ao1_debug_ctrl + 0x000));
	}

	/* 0x1C001000 */
	if (g_sleep) {
		/* MFG0_PWR_CON */
		/* MFG1_PWR_CON */
		/* MFG2_PWR_CON */
		GPUFREQ_LOGI("%-7s (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x",
			"[SPM]",
			(0x1C001000 + 0xEB8), readl(g_sleep + 0xEB8),
			(0x1C001000 + 0xEBC), readl(g_sleep + 0xEBC),
			(0x1C001000 + 0xEC0), readl(g_sleep + 0xEC0));
		/* XPU_PWR_STATUS */
		/* XPU_PWR_STATUS_2ND */
		GPUFREQ_LOGI("%-7s (0x%x): 0x%08x, (0x%x): 0x%08x",
			"[SPM]",
			(0x1C001000 + PWR_STATUS_OFS), readl(g_sleep + PWR_STATUS_OFS),
			(0x1C001000 + PWR_STATUS_2ND_OFS), readl(g_sleep + PWR_STATUS_2ND_OFS));
	}
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

/* API: DVFS order control of GPU */
static int __gpufreq_generic_scale_gpu(
	unsigned int freq_old, unsigned int freq_new,
	unsigned int volt_old, unsigned int volt_new,
	unsigned int vsram_old, unsigned int vsram_new)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START(
		"freq_old=%d, freq_new=%d, volt_old=%d, volt_new=%d, vsram_old=%d, vsram_new=%d",
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
		/* GPU freq scaling */
		ret = __gpufreq_freq_scale_gpu(freq_old, freq_new);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to scale Fgpu: (%d->%d)", freq_old, freq_new);
			goto done;
		}
	/* scaling down: freq -> volt */
	} else if (freq_new < freq_old) {
		/* freq scaling */
		ret = __gpufreq_freq_scale_gpu(freq_old, freq_new);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to scale Fgpu: (%d->%d)", freq_old, freq_new);
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

/*
 * API: commit DVFS to GPU by given freq and volt
 * this is debug function and use it with caution
 */
static int __gpufreq_custom_commit_gpu(unsigned int target_freq,
	unsigned int target_volt, enum gpufreq_dvfs_state key)
{
	/* GPU */
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

	/* prepare GPU setting */
	cur_oppidx_gpu = g_gpu.cur_oppidx;
	cur_fgpu = g_gpu.cur_freq;
	cur_vgpu = g_gpu.cur_volt;
	cur_vsram_gpu = g_gpu.cur_vsram;
	target_oppidx_gpu = __gpufreq_get_idx_by_fgpu(target_freq);
	target_fgpu = target_freq;
	target_vgpu = target_volt;
	target_vsram_gpu = __gpufreq_get_vsram_by_vgpu(target_volt);

	GPUFREQ_LOGD("begin to commit GPU Freq: (%d->%d), Volt: (%d->%d)",
		cur_fgpu, target_fgpu, cur_vgpu, target_vgpu);

	ret = __gpufreq_generic_scale_gpu(cur_fgpu, target_fgpu,
		cur_vgpu, target_vgpu, cur_vsram_gpu, target_vsram_gpu);
	if (unlikely(ret)) {
		GPUFREQ_LOGE(
			"fail to scale Idx(%d->%d), Freq(%d->%d), Volt(%d->%d), Vsram(%d->%d)",
			cur_oppidx_gpu, target_oppidx_gpu, cur_fgpu, target_fgpu,
			cur_vgpu, target_vgpu, cur_vsram_gpu, target_vsram_gpu);
		goto done_unlock;
	}

	g_gpu.cur_oppidx = target_oppidx_gpu;

	__gpufreq_footprint_oppidx(target_oppidx_gpu);

done_unlock:
	mutex_unlock(&gpufreq_lock);

	GPUFREQ_TRACE_END();

	return ret;
}

static void __gpufreq_switch_clksrc(enum gpufreq_clk_src clksrc)
{
	u32 val = 0;

	/* MFG_RPC_AO_CLK_CFG 0x13F91034 */
	val = readl(g_mfg_rpc_base + CLK_MUX_OFS);

	if (clksrc == CLOCK_MAIN)
		/* MAIN = 1'b1 */
		val |= (1UL << CKMUX_SEL_REF_CORE);
	else if (clksrc == CLOCK_SUB)
		/* SUB = 1'b0 */
		val &= ~(1UL << CKMUX_SEL_REF_CORE);
	else
		GPUFREQ_LOGE("invalid clock source: %d (EINVAL)", clksrc);

	writel(val, g_mfg_rpc_base + CLK_MUX_OFS);
}

static void __gpufreq_switch_parksrc(enum gpufreq_clk_src clksrc)
{
	u32 val = 0;

	/* MFG_RPC_AO_CLK_CFG 0x13F91034 */
	val = readl(g_mfg_rpc_base + CLK_MUX_OFS);

	if (clksrc == CLOCK_MAIN)
		/* MAIN = 1'b1 */
		val |= (1UL << CKMUX_SEL_REF_PARK);
	else if (clksrc == CLOCK_SUB)
		/* SUB = 1'b0 */
		val &= ~(1UL << CKMUX_SEL_REF_PARK);
	else
		GPUFREQ_LOGE("invalid clock source: %d (EINVAL)", clksrc);

	writel(val, g_mfg_rpc_base + CLK_MUX_OFS);
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

	if ((freq >= POSDIV_8_MIN_FREQ) && (freq <= POSDIV_2_MAX_FREQ)) {
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
	/* compute CON1 with PCW and POSDIV */
	pll = (0x80000000) | (target_posdiv << POSDIV_SHIFT) | pcw;

	/* 1. switch to parking clk source */
	__gpufreq_switch_clksrc(CLOCK_SUB);
	/* 2. change PCW and POSDIV by writing CON1 */
	writel(pll, MFG_PLL_CON1);
	/* 3. wait until PLL stable */
	udelay(20);
	/* 4. switch to main clk source */
	__gpufreq_switch_clksrc(CLOCK_MAIN);
#endif /* GPUFREQ_FHCTL_ENABLE && CONFIG_COMMON_CLK_MTK_FREQ_HOPPING */

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

static unsigned int __gpufreq_settle_time_vgpu(unsigned int direction, int deltaV)
{
	/* [MT6363_VBUCK5][VGPU]
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

static void __gpufreq_vsram_scale(unsigned int vsram)
{
#ifdef CFG_SRAMRC_SUPPORT
	/* mask passive_gpu to block signal of ourselves request */
	sramrc_mask_passive_gpu();

	/* request target Vsram after unmask active_gpu */
	if (vsram == VSRAM_LEVEL_1)
		__gpufreq_sramrc_request(VSRAM_LEVEL_1);
	else
		__gpufreq_sramrc_request(VSRAM_LEVEL_0);

	/* unmask passive_gpu after request done */
	sramrc_unmask_passive_gpu();
	/* update bound in advanced to prevent Vsram violation in later DVFS */
	__gpufreq_sramrc_update_bound(false);
#else
	GPUFREQ_UNREFERENCED(vsram);
#endif /* CFG_SRAMRC_SUPPORT */
}

/* API: scale Volt of GPU via Regulator */
static int __gpufreq_volt_scale_gpu(
	unsigned int volt_old, unsigned int volt_new,
	unsigned int vsram_old, unsigned int vsram_new)
{
	unsigned int t_settle_volt = 0;
	unsigned int volt_park = SRAM_PARK_VOLT;
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("volt_old=%d, volt_new=%d, vsram_old=%d, vsram_new=%d",
		volt_old, volt_new, vsram_old, vsram_new);

	GPUFREQ_LOGD("begin to scale Vgpu: (%d->%d), Vsram_gpu: (%d->%d)",
		volt_old, volt_new, vsram_old, vsram_new);

	/* volt scaling up */
	if (volt_new > volt_old) {
		if (vsram_new != vsram_old) {
			/* Vgpu scaling to parking volt */
			t_settle_volt =  __gpufreq_settle_time_vgpu(true, (volt_park - volt_old));
			ret = regulator_set_voltage(g_pmic->reg_vgpu,
				volt_park * 10, VGPU_MAX_VOLT * 10 + 125);
			if (unlikely(ret)) {
				__gpufreq_abort(GPUFREQ_PMIC_EXCEPTION,
					"fail to set regulator VGPU (%d)", ret);
				goto done;
			}
			udelay(t_settle_volt);

			/* Vsram scaling to target volt */
			__gpufreq_vsram_scale(vsram_new);

			/* Vgpu scaling to target volt */
			t_settle_volt =  __gpufreq_settle_time_vgpu(true, (volt_new - volt_park));
			ret = regulator_set_voltage(g_pmic->reg_vgpu,
				volt_new * 10, VGPU_MAX_VOLT * 10 + 125);
			if (unlikely(ret)) {
				__gpufreq_abort(GPUFREQ_PMIC_EXCEPTION,
					"fail to set regulator VGPU (%d)", ret);
				goto done;
			}
			udelay(t_settle_volt);
		} else {
			/* Vgpu scaling to target volt */
			t_settle_volt =  __gpufreq_settle_time_vgpu(true, (volt_new - volt_old));
			ret = regulator_set_voltage(g_pmic->reg_vgpu,
				volt_new * 10, VGPU_MAX_VOLT * 10 + 125);
			if (unlikely(ret)) {
				__gpufreq_abort(GPUFREQ_PMIC_EXCEPTION,
					"fail to set regulator VGPU (%d)", ret);
				goto done;
			}
			udelay(t_settle_volt);
		}
	/* volt scaling down */
	} else if (volt_new < volt_old) {
		if (vsram_new != vsram_old) {
			/* Vgpu scaling to parking volt */
			t_settle_volt =  __gpufreq_settle_time_vgpu(false, (volt_old - volt_park));
			ret = regulator_set_voltage(g_pmic->reg_vgpu,
				volt_park * 10, VGPU_MAX_VOLT * 10 + 125);
			if (unlikely(ret)) {
				__gpufreq_abort(GPUFREQ_PMIC_EXCEPTION,
					"fail to set regulator VGPU (%d)", ret);
				goto done;
			}
			udelay(t_settle_volt);

			/* Vsram scaling to target volt */
			__gpufreq_vsram_scale(vsram_new);

			/* Vgpu scaling to target volt */
			t_settle_volt =  __gpufreq_settle_time_vgpu(false, (volt_park - volt_new));
			ret = regulator_set_voltage(g_pmic->reg_vgpu,
				volt_new * 10, VGPU_MAX_VOLT * 10 + 125);
			if (unlikely(ret)) {
				__gpufreq_abort(GPUFREQ_PMIC_EXCEPTION,
					"fail to set regulator VGPU (%d)", ret);
				goto done;
			}
			udelay(t_settle_volt);
		} else {
			/* Vgpu scaling to target volt */
			t_settle_volt =  __gpufreq_settle_time_vgpu(false, (volt_old - volt_new));
			ret = regulator_set_voltage(g_pmic->reg_vgpu,
				volt_new * 10, VGPU_MAX_VOLT * 10 + 125);
			if (unlikely(ret)) {
				__gpufreq_abort(GPUFREQ_PMIC_EXCEPTION,
					"fail to set regulator VGPU (%d)", ret);
				goto done;
			}
			udelay(t_settle_volt);
		}
	/* keep volt */
	} else {
		ret = GPUFREQ_SUCCESS;
	}

	g_gpu.cur_volt = __gpufreq_get_real_vgpu();
	g_gpu.cur_vsram = vsram_new;
	if (unlikely(g_gpu.cur_volt != volt_new))
		__gpufreq_abort(GPUFREQ_GPU_EXCEPTION,
			"inconsistent scaled Vgpu, cur_volt: %d, target_volt: %d",
			g_gpu.cur_volt, volt_new);

	GPUFREQ_LOGD("Vgpu: %d, Vsram: %d, udelay: %d",
		g_gpu.cur_volt, g_gpu.cur_vsram, t_settle_volt);

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

	/* 0x13000000 */
	g_rgx_base = __gpufreq_of_ioremap("mediatek,rgx", 0);
	if (unlikely(!g_rgx_base)) {
		GPUFREQ_LOGE("fail to ioremap RGX");
		goto done;
	}

	/*
	 * [SPM] pwr_status    : 0x1C001F3C
	 * [SPM] pwr_status_2nd: 0x1C001F40
	 * Power ON: 0000 1110 (0xE) [3:1]: MFG0-2
	 */
	GPUFREQ_LOGI("[GPU] RGX_CR_CORE_ID: (0x%08x, 0x%08x), RGX_CR_SYS_BUS_SECURE: 0x%08x",
		readl(g_rgx_base + 0x024), readl(g_rgx_base + 0x020), readl(g_rgx_base + 0xA100));
	GPUFREQ_LOGI("[MFG0-2] PWR_STATUS: 0x%08x, PWR_STATUS_2ND: 0x%08x",
		readl(g_sleep + PWR_STATUS_OFS) & MFG_0_2_PWR_MASK,
		readl(g_sleep + PWR_STATUS_2ND_OFS) & MFG_0_2_PWR_MASK);
	GPUFREQ_LOGI("[TOP] FMETER: %d, CON1: %d, (FMETER_MAIN: %d, FMETER_SUB: %d)",
		__gpufreq_get_fmeter_fgpu(), __gpufreq_get_real_fgpu(),
		__gpufreq_execute_fmeter(CLOCK_MAIN), __gpufreq_execute_fmeter(CLOCK_SUB));
	GPUFREQ_LOGI("[MUX] CKMUX_SEL_REF_CORE: 0x%08x, CKMUX_SEL_REF_PARK: 0x%08x",
		readl(g_mfg_rpc_base + CLK_MUX_OFS) & CKMUX_SEL_REF_CORE_MASK,
		readl(g_mfg_rpc_base + CLK_MUX_OFS) & CKMUX_SEL_REF_PARK_MASK);

done:
	return;
}

static unsigned int __gpufreq_get_fmeter_fgpu(void)
{
	unsigned int main_src = 0, park_src = 0;

	/* MFG_RPC_AO_CLK_CFG 0x13F91034 [4] CKMUX_SEL_REF_CORE */
	main_src = readl(g_mfg_rpc_base + CLK_MUX_OFS) & CKMUX_SEL_REF_CORE_MASK;
	/* MFG_RPC_AO_CLK_CFG 0x13F91034 [5] CKMUX_SEL_REF_PARK */
	park_src = readl(g_mfg_rpc_base + CLK_MUX_OFS) & CKMUX_SEL_REF_PARK_MASK;

	if (main_src == CKMUX_SEL_REF_CORE_MASK)
		return __gpufreq_execute_fmeter(CLOCK_MAIN);
	else if (park_src == CKMUX_SEL_REF_PARK_MASK)
		return __gpufreq_execute_fmeter(CLOCK_SUB);
	else
		return 26000; /* 26MHz */
}

static unsigned int __gpufreq_execute_fmeter(enum gpufreq_clk_src clksrc)
{
	u32 val;
	unsigned int freq = 0;
	int i = 0;

	/* de-asset FMETER reset */
	/* PLL4H_FQMTR_CON0 0x13FA0200 [15] CLK26CALI_0: 1 -> 0 -> 1 */
	writel((readl(PLL4H_FQMTR_CON0) & 0xFFFF7FFF), PLL4H_FQMTR_CON0);
	writel((readl(PLL4H_FQMTR_CON0) | 0x00008000), PLL4H_FQMTR_CON0);

	/* choose target PLL */
	/* PLL4H_FQMTR_CON0 0x13FA0200 [2:0] FQMTR_CKSEL */
	val = readl(PLL4H_FQMTR_CON0) & 0xFFFFFFF8;
	if (clksrc == CLOCK_MAIN)
		val |= FQMTR_PLL1_ID;
	else if (clksrc == CLOCK_SUB)
		val |= FQMTR_PLL4_ID;
	writel(val, PLL4H_FQMTR_CON0);

	/* PLL4H_FQMTR_CON1 0x13FA0204 [25:16] CKGEN_LOAD_CNT = 0x3FF */
	val = (readl(PLL4H_FQMTR_CON1) & 0xFC00FFFF) | (0x3FF << 16);
	writel(val, PLL4H_FQMTR_CON1);

	/* PLL4H_FQMTR_CON0 0x13FA0200 [31:24] CKGEN_K1 = 0x00 */
	writel((readl(PLL4H_FQMTR_CON0) & 0x00FFFFFF), PLL4H_FQMTR_CON0);

	/* enable FMETER */
	/* PLL4H_FQMTR_CON0 0x13FA0200 [12] FMETER_EN = 1'b1 */
	val = (readl(PLL4H_FQMTR_CON0) & 0xFFFFEFFF) | (1UL << 12);
	writel(val, PLL4H_FQMTR_CON0);

	/* trigger FMETER, auto-clear when calibration is done */
	/* PLL4H_FQMTR_CON0 0x13FA0200 [4] CKGEN_TRI_CAL = 1'b1 */
	val = (readl(PLL4H_FQMTR_CON0) & 0xFFFFFFEF) | (1UL << 4);
	writel(val, PLL4H_FQMTR_CON0);

	/* wait FMETER calibration finish */
	while (readl(PLL4H_FQMTR_CON0) & 0x10) {
		udelay(10);
		i++;
		if (i > 100) {
			GPUFREQ_LOGE("wait MFGPLL Fmeter timeout");
			return 0;
		}
	}

	/* read CAL_CNT and CKGEN_LOAD_CNT */
	val = readl(PLL4H_FQMTR_CON1) & 0xFFFF;
	/* Khz */
	freq = ((val * 26000)) / 1024;

	/* reset FMETER */
	writel(0x8000, PLL4H_FQMTR_CON0);

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
	/* MFG_GLOBAL_CON 0x13FBF0B0 [13] MFG_M0_GALS_SLPPROT_IDLE_EN = 1'b1 */
	/* MFG_GLOBAL_CON 0x13FBF0B0 [14] MFG_M1_GALS_SLPPROT_IDLE_EN = 1'b1 */
	/* MFG_GLOBAL_CON 0x13FBF0B0 [21] DVFS_HINT_CG_EN = 1'b0 */
	val = readl(g_mfg_top_base + 0xB0);
	val &= ~(1UL << 8);
	val &= ~(1UL << 10);
	val |= (1UL << 13);
	val |= (1UL << 14);
	val &= ~(1UL << 21);
	writel(val, g_mfg_top_base + 0xB0);

	/* MFG_RPC_AO_CLK_CFG 0x13F91034 [0] CG_FAXI_CK_SOC_IN_FREE_RUN = 1'b0 */
	val = readl(g_mfg_rpc_base + 0x1034);
	val &= ~(1UL << 0);
	writel(val, g_mfg_rpc_base + 0x1034);
}

/* HWAPM: GPU IP automatically control GPU shader MTCMOS */
static void __gpufreq_hwapm_control(void)
{
#if GPUFREQ_HWAPM_ENABLE
	/* MFG_APM_3D_0 0x13FBFC10 = 0x01A80000 */
	writel(0x01A80000, g_mfg_top_base + 0xC10);

	/* MFG_APM_3D_1 0x13FBFC14 = 0x00080010 */
	writel(0x00080010, g_mfg_top_base + 0xC14);

	/* MFG_APM_3D_2 0x13FBFC18 = 0x00100008 */
	writel(0x00100008, g_mfg_top_base + 0xC18);

	/* MFG_APM_3D_3 0x13FBFC1C = 0x00B800C8 */
	writel(0x00B800C8, g_mfg_top_base + 0xC1C);

	/* MFG_APM_3D_4 0x13FBFC20 = 0x00B000C0 */
	writel(0x00B000C0, g_mfg_top_base + 0xC20);

	/* MFG_APM_3D_5 0x13FBFC24 = 0x00C000C8 */
	writel(0x00C000C8, g_mfg_top_base + 0xC24);

	/* MFG_APM_3D_6 0x13FBFC28 = 0x00B000B8 */
	writel(0x00B000B8, g_mfg_top_base + 0xC28);

	/* MFG_APM_3D_7 0x13FBFC2C = 0x00D000D0 */
	writel(0x00D000D0, g_mfg_top_base + 0xC2C);

	/* MFG_APM_3D_8 0x13FBFC30 = 0x00D000D0 */
	writel(0x00D000D0, g_mfg_top_base + 0xC30);

	/* MFG_APM_3D_9 0x13FBFC34 = 0x00D00000 */
	writel(0x00D00000, g_mfg_top_base + 0xC34);

	/* MFG_ACTIVE_POWER_CON_0 0x13FBFC00 = 0x9004001B */
	writel(0x9004001B, g_mfg_top_base + 0xC00);

	/* MFG_ACTIVE_POWER_CON_0 0x13FBFC00 = 0x8004001B */
	writel(0x8004001B, g_mfg_top_base + 0xC00);
#endif /* GPUFREQ_HWAPM_ENABLE */
}

static int __gpufreq_clock_control(enum gpufreq_power_state power)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("power=%d", power);

	if (power == POWER_ON) {
		/* switch GPU MUX to main clock source */
		__gpufreq_switch_parksrc(CLOCK_MAIN);
		__gpufreq_switch_clksrc(CLOCK_MAIN);

		/* enable GPU PLL and Parking PLL */
		clk_prepare(g_clk->clk_sub_parent);
		clk_prepare(g_clk->clk_main_parent);

		g_gpu.cg_count++;
	} else {
		/* switch GPU MUX to sub clock source */
		__gpufreq_switch_clksrc(CLOCK_SUB);
		__gpufreq_switch_parksrc(CLOCK_SUB);

		/* disable GPU PLL and Parking PLL */
		clk_unprepare(g_clk->clk_main_parent);
		clk_unprepare(g_clk->clk_sub_parent);

		g_gpu.cg_count--;
	}

	GPUFREQ_TRACE_END();

	return ret;
}

/* AOC2.0: Vcore_AO can power off */
static void __gpufreq_aoc_control(enum gpufreq_power_state power)
{
	u32 val = 0;
	int i = 0;

	/* wait HW semaphore: SPM_SEMA_M4 0x1C0016AC [0] = 1'b1 */
	do {
		val = readl(g_sleep + 0x6AC);
		val |= (1UL << 0);
		writel(val, g_sleep + 0x6AC);
		udelay(10);
		if (++i > 5000) {
			/* 50ms timeout */
			goto hw_semaphore_timeout;
		}
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
	/* read back to check SPM_SEMA_M4 is truly released */
	udelay(10);
	if (readl(g_sleep + 0x6AC) & 0x1)
		__gpufreq_abort(GPUFREQ_GPU_EXCEPTION,
			"fail to release SPM_SEMA_M4: 0x%08x", readl(g_sleep + 0x6AC));

	return;

hw_semaphore_timeout:
	GPUFREQ_LOGE("M0(0x1C00169C): 0x%08x, M1(0x1C0016A0): 0x%08x",
		readl(g_sleep + 0x69C), readl(g_sleep + 0x6A0));
	GPUFREQ_LOGE("M2(0x1C0016A4): 0x%08x, M3(0x1C0016A8): 0x%08x",
		readl(g_sleep + 0x6A4), readl(g_sleep + 0x6A8));
	GPUFREQ_LOGE("M4(0x1C0016AC): 0x%08x, M5(0x1C0016B0): 0x%08x",
		readl(g_sleep + 0x6AC), readl(g_sleep + 0x6B0));
	GPUFREQ_LOGE("M6(0x1C0016B4): 0x%08x, M7(0x1C0016B8): 0x%08x",
		readl(g_sleep + 0x6B4), readl(g_sleep + 0x6B8));
	__gpufreq_abort(GPUFREQ_GPU_EXCEPTION, "acquire SPM_SEMA_M4 timeout");
}

/* Merge GPU transaction from 64byte to 128byte to maximize DRAM efficiency */
static void __gpufreq_transaction_control(enum gpufreq_transaction_mode mode)
{
	if (mode == MERGER_OFF) {
		/* merge_r */
		writel(0x1818F780, g_mfg_top_base + 0x8A0);
		/* merge_w */
		writel(0x1818F780, g_mfg_top_base + 0x8B0);
		/* QoS normal mode */
		writel(0x00000000, g_mfg_top_base + 0x700);
		/* ar_requestor */
		writel(0xA0000020, g_mfg_top_base + 0x704);
		/* aw_requestor */
		writel(0xA0000020, g_mfg_top_base + 0x708);
		/* QoS pre-ultra */
		writel(0x00400400, g_mfg_top_base + 0x70C);

		writel(0x00000000, g_mfg_top_base + 0x710);
		writel(0x00000000, g_mfg_top_base + 0x714);
		writel(0x00000000, g_mfg_top_base + 0x718);
		writel(0x00000000, g_mfg_top_base + 0x71C);
		writel(0x00000000, g_mfg_top_base + 0x720);
		writel(0x00000000, g_mfg_top_base + 0x724);
	} else if (mode == MERGER_LIGHT) {
		/* merge_r */
		writel(0x1818F783, g_mfg_top_base + 0x8A0);
		/* merge_w */
		writel(0x1818F783, g_mfg_top_base + 0x8B0);
		/* QoS normal mode */
		writel(0x00000000, g_mfg_top_base + 0x700);
		/* ar_requestor */
		writel(0xA0000020, g_mfg_top_base + 0x704);
		/* aw_requestor */
		writel(0xA0000020, g_mfg_top_base + 0x708);
		/* QoS pre-ultra */
		writel(0x00400400, g_mfg_top_base + 0x70C);

		writel(0x00000000, g_mfg_top_base + 0x710);
		writel(0x00000000, g_mfg_top_base + 0x714);
		writel(0x00000000, g_mfg_top_base + 0x718);
		writel(0x00000000, g_mfg_top_base + 0x71C);
		writel(0x00000000, g_mfg_top_base + 0x720);
		writel(0x00000000, g_mfg_top_base + 0x724);
	}
}

static int __gpufreq_mtcmos_control(enum gpufreq_power_state power)
{
	int ret = GPUFREQ_SUCCESS;
	u32 val = 0;

	GPUFREQ_TRACE_START("power=%d", power);

	if (power == POWER_ON) {
		/* MFG1 on by SPM */
		ret = pm_runtime_get_sync(g_mtcmos->mfg1_dev);
		if (unlikely(ret < 0)) {
			__gpufreq_abort(GPUFREQ_CCF_EXCEPTION,
				"fail to enable mfg1_dev (%d)", ret);
			goto done;
		}

#if !GPUFREQ_HWAPM_ENABLE
		/* manually control MFG2-5 if HWAPM is disabled */
		ret = pm_runtime_get_sync(g_mtcmos->mfg2_dev);
		if (unlikely(ret < 0)) {
			__gpufreq_abort(GPUFREQ_CCF_EXCEPTION,
				"fail to enable mfg2_dev (%d)", ret);
			goto done;
		}
#if GPUFREQ_CHECK_MTCMOS_PWR_STATUS
		/* SPM contorl, check MFG0-2 power status */
		val = readl(g_sleep + PWR_STATUS_OFS) & MFG_0_2_PWR_MASK;
		if (unlikely(val != MFG_0_2_PWR_MASK)) {
			__gpufreq_abort(GPUFREQ_GPU_EXCEPTION,
				"incorrect MFG0-2 power on status: 0x%08x", val);
			ret = GPUFREQ_EINVAL;
			goto done;
		}
#endif /* GPUFREQ_CHECK_MTCMOS_PWR_STATUS */
#else
#if GPUFREQ_CHECK_MTCMOS_PWR_STATUS
		/* HWAPM control, check MFG0-1 power status */
		val = readl(g_sleep + PWR_STATUS_OFS) & MFG_0_1_PWR_MASK;
		if (unlikely(val != MFG_0_1_PWR_MASK)) {
			__gpufreq_abort(GPUFREQ_GPU_EXCEPTION,
				"incorrect MFG0-1 power on status: 0x%08x", val);
			ret = GPUFREQ_EINVAL;
			goto done;
		}
#endif /* GPUFREQ_CHECK_MTCMOS_PWR_STATUS */
#endif /* GPUFREQ_HWAPM_ENABLE */

		g_gpu.mtcmos_count++;
	} else {
#if !GPUFREQ_HWAPM_ENABLE
		ret = pm_runtime_put_sync(g_mtcmos->mfg2_dev);
		if (unlikely(ret < 0)) {
			__gpufreq_abort(GPUFREQ_CCF_EXCEPTION,
				"fail to enable mfg2_dev (%d)", ret);
			goto done;
		}
#endif /* GPUFREQ_HWAPM_ENABLE */

		/* MFG1 off by SPM */
		ret = pm_runtime_put_sync(g_mtcmos->mfg1_dev);
		if (unlikely(ret < 0)) {
			__gpufreq_abort(GPUFREQ_CCF_EXCEPTION,
				"fail to disable mfg1_dev (%d)", ret);
			goto done;
		}

#if GPUFREQ_CHECK_MTCMOS_PWR_STATUS
		/* check MFG1-2 power status, MFG0 is off in ATF */
		val = readl(g_sleep + PWR_STATUS_OFS) & MFG_1_2_PWR_MASK;
		if (unlikely(val))
			/* only print error if pwr is incorrect when mtcmos off */
			GPUFREQ_LOGE("incorrect MFG power off status: 0x%08x", val);
#endif /* GPUFREQ_CHECK_MTCMOS_PWR_STATUS */

		g_gpu.mtcmos_count--;
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
		ret = regulator_enable(g_pmic->reg_vgpu);
		if (unlikely(ret)) {
			__gpufreq_abort(GPUFREQ_PMIC_EXCEPTION, "fail to enable VGPU (%d)", ret);
			goto done;
		}
		g_gpu.buck_count++;
	/* power off */
	} else {
		ret = regulator_disable(g_pmic->reg_vgpu);
		if (unlikely(ret)) {
			__gpufreq_abort(GPUFREQ_PMIC_EXCEPTION, "fail to disable VGPU (%d)", ret);
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
	unsigned int cur_fgpu = 0;
	int target_oppidx_gpu = -1;
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START();

	/* get current GPU OPP idx by freq set in preloader */
	cur_fgpu = g_gpu.cur_freq;
	g_gpu.cur_oppidx = __gpufreq_get_idx_by_fgpu(cur_fgpu);

	/* decide first OPP idx by custom setting */
	if (__gpufreq_custom_init_enable())
		target_oppidx_gpu = GPUFREQ_CUST_INIT_OPPIDX;
	/* decide first OPP idx by SRAMRC setting */
	else
		target_oppidx_gpu = __gpufreq_get_idx_by_vgpu(GPUFREQ_SAFE_VLOGIC);

	GPUFREQ_LOGI("init GPU idx: %d, Freq: %d, Volt: %d",
		target_oppidx_gpu, working_table[target_oppidx_gpu].freq,
		working_table[target_oppidx_gpu].volt);

	/* init first OPP index */
	if (!__gpufreq_dvfs_enable()) {
		g_dvfs_state = DVFS_DISABLE;
		GPUFREQ_LOGI("DVFS state: 0x%x, disable DVFS", g_dvfs_state);

		/* set OPP once if DVFS is disabled but custom init is enabled */
		if (__gpufreq_custom_init_enable()) {
			ret = __gpufreq_generic_commit_gpu(target_oppidx_gpu, DVFS_DISABLE);
			if (unlikely(ret)) {
				GPUFREQ_LOGE("fail to commit GPU OPP index: %d (%d)",
					target_oppidx_gpu, ret);
				goto done;
			}
		}
	} else {
		g_dvfs_state = DVFS_FREE;
		GPUFREQ_LOGI("DVFS state: 0x%x, enable DVFS", g_dvfs_state);

		ret = __gpufreq_generic_commit_gpu(target_oppidx_gpu, DVFS_FREE);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to commit GPU OPP index: %d (%d)",
				target_oppidx_gpu, ret);
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

	/* prepare GPU setting */
	cur_fgpu = g_gpu.cur_freq;
	cur_vgpu = g_gpu.cur_volt;
	cur_vsram_gpu = g_gpu.cur_vsram;
	target_fgpu = GPUFREQ_AGING_KEEP_FGPU;
	target_vgpu = GPUFREQ_AGING_KEEP_VGPU;
	target_vsram_gpu = VSRAM_LEVEL_0;

	GPUFREQ_LOGD("begin to commit GPU Freq: (%d->%d), Volt: (%d->%d)",
		cur_fgpu, target_fgpu, cur_vgpu, target_vgpu);

	ret = __gpufreq_generic_scale_gpu(cur_fgpu, target_fgpu,
		cur_vgpu, target_vgpu, cur_vsram_gpu, target_vsram_gpu);
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to scale GPU: Freq(%d->%d), Volt(%d->%d), Vsram(%d->%d)",
			cur_fgpu, target_fgpu, cur_vgpu, target_vgpu,
			cur_vsram_gpu, target_vsram_gpu);
		goto done_unlock;
	}

	GPUFREQ_LOGI("pause DVFS at GPU(%d, %d), state: 0x%x",
		target_fgpu, target_vgpu, g_dvfs_state);

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
}

/* API: apply aging volt diff to working table */
static void __gpufreq_apply_aging(unsigned int apply_aging)
{
	int i = 0;
	struct gpufreq_opp_info *working_table = g_gpu.working_table;
	struct gpufreq_opp_info *signed_table = g_gpu.signed_table;
	int working_opp_num = g_gpu.opp_num;
	int signed_opp_num = g_gpu.signed_opp_num;

	mutex_lock(&gpufreq_lock);

	for (i = 0; i < working_opp_num; i++) {
		if (apply_aging)
			working_table[i].volt -= working_table[i].vaging;
		else
			working_table[i].volt += working_table[i].vaging;

		working_table[i].vsram = __gpufreq_get_vsram_by_vgpu(working_table[i].volt);

		GPUFREQ_LOGD("apply Vaging: %d, GPU[%02d] Volt: %d, Vsram: %d",
			apply_aging, i, working_table[i].volt, working_table[i].vsram);
	}

	for (i = 0; i < signed_opp_num; i++) {
		if (apply_aging)
			signed_table[i].volt -= signed_table[i].vaging;
		else
			signed_table[i].volt += signed_table[i].vaging;

		signed_table[i].vsram = __gpufreq_get_vsram_by_vgpu(signed_table[i].volt);
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

	GPUFREQ_TRACE_START("adj_table=0x%x, adj_num=%d", adj_table, adj_num);

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

		GPUFREQ_LOGD("[%02d*] Freq: %d, Volt: %d, Vsram: %d, Vaging: %d",
			oppidx, signed_table[oppidx].freq,
			signed_table[oppidx].volt,
			signed_table[oppidx].vsram,
			signed_table[oppidx].vaging);
	}

	mutex_unlock(&gpufreq_lock);

done:
	GPUFREQ_TRACE_END();
}

#if GPUFREQ_ASENSOR_ENABLE
/* API: get Aging sensor data from EFUSE, return if success*/
static unsigned int __gpufreq_asensor_read_efuse(u32 *a_t0_efuse1, u32 *a_t0_efuse2,
	u32 *efuse_error)
{
	u32 efuse1 = 0, efuse2 = 0;
	u32 a_t0_efuse1 = 0, a_t0_efuse2 = 0, a_t0_efuse3 = 0, a_t0_efuse4 = 0;
	u32 a_t0_err1 = 0, a_t0_err2 = 0, a_t0_err3 = 0, a_t0_err4 = 0;

	/* GPU_CPE_0p65v_RT_BLOW 0x11C10B4C */
	efuse1 = readl(g_efuse_base + 0xB4C);
	/* GPU_CPE_0p65v_HT_BLOW 0x11C10B50 */
	efuse2 = readl(g_efuse_base + 0xB50);

	if (!efuse1 || !efuse2)
		return false;

	GPUFREQ_LOGD("efuse1: 0x%08x, efuse2: 0x%08x", efuse1, efuse2);

	/* A_T0_LVT_RT: 0x11C10B4C [13:0] */
	*a_t0_efuse1 = (efuse1 & 0x00003FFF);
	/* A_T0_ULVT_RT: 0x11C10B4C [29:16] */
	*a_t0_efuse2 = (efuse1 & 0x3FFF0000) >> 16;
	/* AGING_QUALITY_LVT_RT: 0x11C10B4C [15] */
	a_t0_err1 = (efuse1 & 0x00008000) >> 15;
	/* AGING_QUALITY_ULVT_RT: 0x11C10B4C [31] */
	a_t0_err2 = (efuse1 & 0x80000000) >> 31;

	/* A_T0_LVT_HT: 0x11C10B50 [13:0] */
	*a_t0_efuse3 = (efuse2 & 0x00003FFF);
	/* A_T0_ULVT_HT: 0x11C10B50 [29:16] */
	*a_t0_efuse4 = (efuse2 & 0x3FFF0000) >> 16;
	/* AGING_QUALITY_LVT_HT: 0x11C10B50 [15] */
	a_t0_err3 = (efuse2 & 0x00008000) >> 15;
	/* AGING_QUALITY_ULVT_HT: 0x11C10B50 [31] */
	a_t0_err4 = (efuse2 & 0x80000000) >> 31;

	/* efuse_error: AGING_QUALITY_RT | AGING_QUALITY_HT */
	*efuse_error  = a_t0_err1 | a_t0_err2 | a_t0_err3 | a_t0_err4;

	g_asensor_info.efuse1 = efuse1;
	g_asensor_info.efuse2 = efuse2;
	g_asensor_info.efuse1_addr = 0x11C10B4C;
	g_asensor_info.efuse2_addr = 0x11C10B50;
	g_asensor_info.a_t0_efuse1 = *a_t0_efuse1;
	g_asensor_info.a_t0_efuse2 = *a_t0_efuse2;
	g_asensor_info.a_t0_efuse3 = *a_t0_efuse3;
	g_asensor_info.a_t0_efuse4 = *a_t0_efuse4;

	GPUFREQ_LOGD("a_t0_efuse1: 0x%08x, a_t0_efuse2: 0x%08x", *a_t0_efuse1, *a_t0_efuse2);
	GPUFREQ_LOGD("a_t0_efuse3: 0x%08x, a_t0_efuse4: 0x%08x, efuse_error: 0x%08x",
		*a_t0_efuse3, *a_t0_efuse4, *efuse_error);

	return true;
}
#endif /* GPUFREQ_ASENSOR_ENABLE */

#if GPUFREQ_ASENSOR_ENABLE
static void __gpufreq_asensor_read_register(u32 *a_tn_sensor1, u32 *a_tn_sensor2)
{
	u32 aging_data1 = 0, aging_data2 = 0;

	/* Enable CPE CG */
	/* MFG_SENSOR_BCLK_CG 0x13FBFF98 = 0x00000001 */
	writel(0x00000001, g_mfg_top_base + 0xF98);

	/* Config and trigger sensor */
	/* MFG_SENCMONCTL 0x13FCF000 = 0x0008000A */
	writel(0x0008000A, g_mfg_cpe_sensor_base + 0x0);
	/* MFG_SENCMONCTL 0x13FCF000 = 0x0088000A */
	writel(0x0088000A, g_mfg_cpe_sensor_base + 0x0);

	/* wait 70us */
	udelay(70);

	/* Read sensor data */
	/* MFG_ASENSORDATA0 0x13FCF008 */
	aging_data1 = readl(g_mfg_cpe_sensor_base + 0x8);
	/* MFG_ASENSORDATA1 0x13FCF00C */
	aging_data2 = readl(g_mfg_cpe_sensor_base + 0xC);

	GPUFREQ_LOGD("aging_data1: 0x%08x, aging_data2: 0x%08x", aging_data1, aging_data2);

	/* A_TN_LVT_RT_CNT: 0x13FCF008 [15:0] */
	*a_tn_sensor1 = (aging_data1 & 0xFFFF);
	/* A_TN_ULVT_RT_CNT: 0x13FCF00C [15:0] */
	*a_tn_sensor2 = (aging_data2 & 0xFFFF);

	g_asensor_info.a_tn_sensor1 = *a_tn_sensor1;
	g_asensor_info.a_tn_sensor2 = *a_tn_sensor2;

	GPUFREQ_LOGD("a_tn_sensor1: 0x%08x, a_tn_sensor2: 0x%08x", *a_tn_sensor1, *a_tn_sensor2);
}
#endif /* GPUFREQ_ASENSOR_ENABLE */

#if GPUFREQ_ASENSOR_ENABLE
static unsigned int __gpufreq_get_aging_table_idx(
	u32 a_t0_efuse1, u32 a_t0_efuse2, u32 a_tn_sensor1, u32 a_tn_sensor2,
	u32 efuse_error, unsigned int is_efuse_read_success)
{
	int a_diff = 0, a_diff1 = 0, a_diff2 = 0;
	int tj_max = 0, tj = 0;
	unsigned int leakage_power = 0;
	unsigned int aging_table_idx = GPUFREQ_AGING_MOST_AGRRESIVE;

#ifdef CFG_THERMAL_SUPPORT
	/* unit: m'C */
	tj_max = get_gpu_max_temp(HW_REG) / 1000;
#else
	tj_max = 30;
#endif /* CFG_THERMAL_SUPPORT */
	tj = ((tj_max - 25) * 50) / 40;

	a_diff1 = a_t0_efuse1 + tj - a_tn_sensor1;
	a_diff2 = a_t0_efuse2 + tj - a_tn_sensor2;
	a_diff = MAX(a_diff1, a_diff2);

	leakage_power = __gpufreq_get_lkg_pgpu(GPUFREQ_AGING_LKG_VGPU);

	g_asensor_info.tj_max = tj_max;
	g_asensor_info.a_diff1 = a_diff1;
	g_asensor_info.a_diff2 = a_diff2;
	g_asensor_info.leakage_power = leakage_power;

	GPUFREQ_LOGD("tj_max: %d tj: %d, a_diff1: %d, a_diff2: %d", tj_max, tj, a_diff1, a_diff2);
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
	else if (leakage_power < 20)
		aging_table_idx = 3;
	else if (a_diff < GPUFREQ_AGING_GAP_MIN)
		aging_table_idx = 3;
	else if ((a_diff >= GPUFREQ_AGING_GAP_MIN) && (a_diff < GPUFREQ_AGING_GAP_1))
		aging_table_idx = 0;
	else if ((a_diff >= GPUFREQ_AGING_GAP_1) && (a_diff < GPUFREQ_AGING_GAP_2))
		aging_table_idx = 1;
	else if ((a_diff >= GPUFREQ_AGING_GAP_2) && (a_diff < GPUFREQ_AGING_GAP_3))
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

static void __gpufreq_aging_adjustment(void)
{
	struct gpufreq_adj_info *aging_adj = NULL;
	int adj_num = 0;
	int i;
	unsigned int aging_table_idx = GPUFREQ_AGING_MOST_AGRRESIVE;

#if GPUFREQ_ASENSOR_ENABLE
	u32 a_t0_efuse1 = 0, a_t0_efuse2 = 0;
	u32 a_tn_sensor1 = 0, a_tn_sensor2 = 0;
	u32 efuse_error = 0;
	unsigned int is_efuse_read_success = false;

	if (__gpufreq_dvfs_enable()) {
		/* keep volt for A sensor working */
		__gpufreq_pause_dvfs();

		/* get aging efuse data */
		is_efuse_read_success = __gpufreq_asensor_read_efuse(
			&a_t0_efuse1, &a_t0_efuse2, &efuse_error);

		/* get aging sensor data */
		__gpufreq_asensor_read_register(&a_tn_sensor1, &a_tn_sensor2);

		/* resume DVFS */
		__gpufreq_resume_dvfs();

		/* compute aging_table_idx with aging efuse data and aging sensor data */
		aging_table_idx = __gpufreq_get_aging_table_idx(
			a_t0_efuse1, a_t0_efuse2, a_tn_sensor1, a_tn_sensor2,
			efuse_error, is_efuse_read_success);
	}

	g_asensor_info.aging_table_idx_agrresive = GPUFREQ_AGING_MOST_AGRRESIVE;
	g_asensor_info.aging_table_idx = aging_table_idx;

	GPUFREQ_LOGI("Aging Sensor table id: %d", aging_table_idx);
#endif /* GPUFREQ_ASENSOR_ENABLE */

	adj_num = g_gpu.signed_opp_num;

	/* prepare aging adj */
	aging_adj = kcalloc(adj_num, sizeof(struct gpufreq_adj_info), GFP_KERNEL);
	if (!aging_adj) {
		GPUFREQ_LOGE("fail to alloc gpufreq_adj_info (ENOMEM)");
		return;
	}

	if (aging_table_idx > GPUFREQ_AGING_MAX_TABLE_IDX)
		aging_table_idx = GPUFREQ_AGING_MAX_TABLE_IDX;

	for (i = 0; i < adj_num; i++) {
		aging_adj[i].oppidx = i;
		aging_adj[i].vaging = g_aging_table[aging_table_idx][i];
	}

	/* apply aging to signed table */
	__gpufreq_apply_adjust(aging_adj, adj_num);

	kfree(aging_adj);
}

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
	 * 950        | 0.8                | PTPOD16    | 0x11C105C0
	 * 900        | 0.75               | PTPOD17    | 0x11C105C4
	 * 660        | 0.65               | PTPOD18    | 0x11C105C8
	 * 390        | 0.575              | PTPOD19    | 0x11C105CC
	 */

	for (i = 0; i < adj_num; i++) {
		oppidx = g_avs_adj[i].oppidx;
		val = readl(g_efuse_base + 0x5C0 + (i * 0x4));

		/* if efuse value is not set */
		if (!val)
			continue;

		/* compute Freq from efuse */
		temp_freq = 0;
		temp_freq |= (val & 0x00100000) >> 10; /* Get freq[10] from efuse[20]     */
		temp_freq |= (val & 0x00000C00) >> 2;  /* Get freq[9:8] from efuse[11:10] */
		temp_freq |= (val & 0x00000003) << 6;  /* Get freq[7:6] from efuse[1:0]   */
		temp_freq |= (val & 0x000000C0) >> 2;  /* Get freq[5:4] from efuse[7:6]   */
		temp_freq |= (val & 0x000C0000) >> 16; /* Get freq[3:2] from efuse[19:18] */
		temp_freq |= (val & 0x00003000) >> 12; /* Get freq[1:0] from efuse[13:12] */
		/* Freq is stored in efuse with MHz unit */
		temp_freq *= 1000;
		/* verify with signoff Freq */
		if (temp_freq != g_gpu.signed_table[oppidx].freq) {
			__gpufreq_abort(GPUFREQ_GPU_EXCEPTION,
				"OPP[%02d*]: AVS efuse[%d].freq(%d) != signed-off.freq(%d)",
				oppidx, i, temp_freq, g_gpu.signed_table[oppidx].freq);
			return;
		}

		/* compute Volt from efuse */
		temp_volt = 0;
		temp_volt |= (val & 0x0003C000) >> 14; /* Get volt[3:0] from efuse[17:14] */
		temp_volt |= (val & 0x00000030);       /* Get volt[5:4] from efuse[5:4]   */
		temp_volt |= (val & 0x0000000C) << 4;  /* Get volt[7:6] from efuse[3:2]   */
		/* Volt is stored in efuse with 6.25mV unit */
		temp_volt *= 625;
		/* clamp to signoff Volt */
		if (temp_volt > g_gpu.signed_table[oppidx].volt) {
			GPUFREQ_LOGW("OPP[%02d*]: AVS efuse[%d].volt(%d) > signed-off.volt(%d)",
				oppidx, i, temp_volt, g_gpu.signed_table[oppidx].volt);
			g_avs_adj[i].volt = g_gpu.signed_table[oppidx].volt;
		} else
			g_avs_adj[i].volt = temp_volt;
		g_avs_adj[i].vsram = __gpufreq_get_vsram_by_vstack(g_avs_adj[i].volt);

		/* AVS is enabled if any OPP is adjusted by AVS */
		g_avs_enable = true;

		GPUFREQ_LOGI("OPP[%02d*]: AVS efuse[%d] freq(%d), volt(%d)",
			oppidx, i, temp_freq, temp_volt);
	}

	/* apply AVS to signed table */
	__gpufreq_apply_adjust(g_avs_adj, adj_num);

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
		__gpufreq_apply_adjust(custom_adj, adj_num);
		GPUFREQ_LOGI("MCL50 flavor load");
	}
}

static void __gpufreq_segment_adjustment(void)
{
	struct gpufreq_adj_info *segment_adj;
	int adj_num = 0;
	unsigned int efuse_id = 0x0;

	switch (efuse_id) {
	case 0x2:
		segment_adj = g_segment_adj;
		adj_num = SEGMENT_ADJ_NUM;
		__gpufreq_apply_adjust(segment_adj, adj_num);
		break;
	default:
		break;
	}

	GPUFREQ_LOGI("efuse_id: 0x%x, adj_num: %d", efuse_id, adj_num);
}

/*
 * 1. init OPP segment range
 * 2. init segment/working OPP table
 * 3. apply aging
 * 3. init power measurement
 */
static int __gpufreq_init_opp_table(struct platform_device *pdev)
{
	unsigned int segment_id = 0;
	int i = 0, j = 0;
	int ret = GPUFREQ_SUCCESS;

	/* init current GPU freq and volt set by preloader */
	g_gpu.cur_freq = __gpufreq_get_real_fgpu();
	g_gpu.cur_volt = __gpufreq_get_real_vgpu();
	g_gpu.cur_vsram = VSRAM_LEVEL_0;

	GPUFREQ_LOGI("preloader init [GPU] Freq: %d, Volt: %d, Vsram: %d",
		g_gpu.cur_freq, g_gpu.cur_volt, g_gpu.cur_vsram);

	/* init GPU OPP table */
	/* init OPP segment range */
	segment_id = g_gpu.segment_id;
	if (segment_id == MT6855_SEGMENT)
		g_gpu.segment_upbound = 0;
	else
		g_gpu.segment_upbound = 8;
	g_gpu.segment_lowbound = SIGNED_OPP_GPU_NUM - 1;
	g_gpu.signed_opp_num = SIGNED_OPP_GPU_NUM;

	g_gpu.max_oppidx = 0;
	g_gpu.min_oppidx = g_gpu.segment_lowbound - g_gpu.segment_upbound;
	g_gpu.opp_num = g_gpu.min_oppidx + 1;

	GPUFREQ_LOGD("number of signed GPU OPP: %d, upper and lower bound: [%d, %d]",
		g_gpu.signed_opp_num, g_gpu.segment_upbound, g_gpu.segment_lowbound);
	GPUFREQ_LOGI("number of working GPU OPP: %d, max and min OPP index: [%d, %d]",
		g_gpu.opp_num, g_gpu.max_oppidx, g_gpu.min_oppidx);

	g_gpu.signed_table = g_default_gpu;
	/* apply segment adjustment to GPU signed table */
	__gpufreq_segment_adjustment();
	/* apply AVS adjustment to GPU signed table */
	__gpufreq_avs_adjustment();
	/* apply aging adjustment to GPU signed table */
	__gpufreq_aging_adjustment();
	/* apply custom adjustment to GPU signed table */
	__gpufreq_custom_adjustment();

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

	if (g_aging_enable)
		__gpufreq_apply_aging(true);

	/* set power info to working table */
	__gpufreq_measure_power();

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
		segment_id = MT6855_SEGMENT;
		break;
	}

	GPUFREQ_LOGI("efuse_id: 0x%x, segment_id: %d", efuse_id, segment_id);

	g_gpu.segment_id = segment_id;

	return ret;
}

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

#if !GPUFREQ_HWAPM_ENABLE
	g_mtcmos->mfg2_dev = dev_pm_domain_attach_by_name(dev, "pd_mfg2");
	if (IS_ERR_OR_NULL(g_mtcmos->mfg2_dev)) {
		ret = g_mtcmos->mfg2_dev ? PTR_ERR(g_mtcmos->mfg2_dev) : GPUFREQ_ENODEV;
		__gpufreq_abort(GPUFREQ_CCF_EXCEPTION, "fail to get mfg2_dev (%ld)", ret);
		goto done;
	}
	dev_pm_syscore_device(g_mtcmos->mfg2_dev, true);
#endif /* GPUFREQ_HWAPM_ENABLE */

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

	g_clk->clk_main_parent = devm_clk_get(&pdev->dev, "clk_main_parent");
	if (IS_ERR(g_clk->clk_main_parent)) {
		ret = PTR_ERR(g_clk->clk_main_parent);
		__gpufreq_abort(GPUFREQ_CCF_EXCEPTION,
			"fail to get clk_main_parent (%ld)", ret);
		goto done;
	}

	g_clk->clk_sub_parent = devm_clk_get(&pdev->dev, "clk_sub_parent");
	if (IS_ERR(g_clk->clk_sub_parent)) {
		ret = PTR_ERR(g_clk->clk_sub_parent);
		__gpufreq_abort(GPUFREQ_CCF_EXCEPTION,
			"fail to get clk_sub_parent (%ld)", ret);
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

	g_pmic->reg_vgpu = regulator_get_optional(&pdev->dev, "_vgpu");
	if (IS_ERR(g_pmic->reg_vgpu)) {
		ret = PTR_ERR(g_pmic->reg_vgpu);
		__gpufreq_abort(GPUFREQ_PMIC_EXCEPTION, "fail to get VGPU (%ld)", ret);
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

	/* 0x13000000 */
	g_rgx_base = __gpufreq_of_ioremap("mediatek,rgx", 0);
	if (unlikely(!g_rgx_base)) {
		GPUFREQ_LOGE("fail to ioremap RGX");
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

	/* 0x10351000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "nth_emi_mpu_reg");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource NTH_EMI_MPU_REG");
		goto done;
	}
	g_nth_emi_mpu_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_nth_emi_mpu_base)) {
		GPUFREQ_LOGE("fail to ioremap NTH_EMI_MPU_REG: 0x%llx", res->start);
		goto done;
	}

	/* 0x10042000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "fmem_ao_debug_ctrl");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource FMEM_AO_DEBUG_CTRL");
		goto done;
	}
	g_fmem_ao_debug_ctrl = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_fmem_ao_debug_ctrl)) {
		GPUFREQ_LOGE("fail to ioremap FMEM_AO_DEBUG_CTRL: 0x%llx", res->start);
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

	/* 0x11C10000 */
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

	/* 0x13FCF000 */
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

	/* init AEE debug */
	__gpufreq_footprint_power_step_reset();
	__gpufreq_footprint_oppidx_reset();
	__gpufreq_footprint_power_count_reset();

	/* init reg base address and flavor config of the platform in both AP and EB mode */
	ret = __gpufreq_init_platform_info(pdev);
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to init platform info (%d)", ret);
		goto done;
	}

#if GPUFREQ_ASENSOR_ENABLE
	/* apply aging volt to working table volt if Asensor works */
	g_aging_enable = true;
#else
	/* apply aging volt to working table volt depending on Aging load */
	g_aging_enable = g_aging_load;
#endif /* GPUFREQ_ASENSOR_ENABLE */

	/* init gpu dfd */
	ret = gpudfd_init(pdev);
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to init gpudfd (%d)", ret);
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
	return ret;
}

/* API: gpufreq driver remove */
static int __gpufreq_pdrv_remove(struct platform_device *pdev)
{
#if !GPUFREQ_HWAPM_ENABLE
	dev_pm_domain_detach(g_mtcmos->mfg2_dev, true);
#endif /* GPUFREQ_HWAPM_ENABLE */
	dev_pm_domain_detach(g_mtcmos->mfg1_dev, true);

	kfree(g_gpu.working_table);
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
