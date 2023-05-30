// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 MediaTek Inc.
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
#include <linux/thermal.h>
#include <linux/timekeeping.h>
#if IS_ENABLED(CONFIG_MTK_DEVINFO)
#include <linux/nvmem-consumer.h>
#endif

#include <gpufreq_v2.h>
#include <gpuppm.h>
#include <gpufreq_common.h>
#include <gpufreq_mt6835.h>
#include <gpufreq_reg_mt6835.h>
#include <gpudfd_mt6835.h>
#include <mtk_gpu_utility.h>
#include <gpufreq_history_common.h>
#include <gpufreq_history_mt6835.h>

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
#if IS_ENABLED(CONFIG_COMMON_CLK_MT6835)
#include <clk-fmeter.h>
#include <clk-mt6835-fmeter.h>
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
static void __gpufreq_set_ocl_timestamp(void);
static void __gpufreq_set_margin_mode(unsigned int mode);
static void __gpufreq_measure_power(void);
static void __iomem *__gpufreq_of_ioremap(const char *node_name, int idx);
static void __gpufreq_interpolate_volt(void);
static void __gpufreq_apply_restore_margin(unsigned int mode);
static void __gpufreq_update_shared_status_opp_table(void);
static void __gpufreq_update_shared_status_adj_table(void);
static void __gpufreq_update_shared_status_init_reg(void);
static void __gpufreq_update_shared_status_power_reg(void);
static void __gpufreq_update_shared_status_dvfs_reg(void);
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
static int __gpufreq_switch_clksrc(enum gpufreq_clk_src clksrc);
static unsigned int __gpufreq_calculate_pcw(unsigned int freq, enum gpufreq_posdiv posdiv_power);
static unsigned int __gpufreq_settle_time_vgpu(unsigned int direction, int deltaV);
static unsigned int __gpufreq_settle_time_vsram(unsigned int direction, int deltaV);
static void __gpufreq_set_temper_compensation(unsigned int instant_dvfs);
/* get function */
static unsigned int __gpufreq_get_fmeter_freq(void);
static unsigned int __gpufreq_get_real_fgpu(void);
static unsigned int __gpufreq_get_real_vgpu(void);
static unsigned int __gpufreq_get_real_vsram(void);
static unsigned int __gpufreq_get_real_vcore(void);
static enum gpufreq_posdiv __gpufreq_get_real_posdiv_gpu(void);
static enum gpufreq_posdiv __gpufreq_get_posdiv_by_fgpu(unsigned int freq);
static unsigned int __gpufreq_get_vcore_by_vsram(unsigned int volt);
/* aging sensor function */
#if GPUFREQ_ASENSOR_ENABLE
static int __gpufreq_pause_dvfs(void);
static void __gpufreq_resume_dvfs(void);
static unsigned int __gpufreq_asensor_read_efuse(u32 *a_t0_efuse1, u32 *a_t0_efuse2,
	u32 *efuse_error);
static void __gpufreq_asensor_read_register(u32 *a_tn_sensor1, u32 *a_tn_sensor2);
static unsigned int __gpufreq_get_aging_table_idx(
	u32 a_t0_efuse1, u32 a_t0_efuse2, u32 a_tn_sensor1, u32 a_tn_sensor2,
	u32 efuse_error, unsigned int is_efuse_read_success);
#endif /* GPUFREQ_ASENSOR_ENABLE */
/* power control function */
#if GPUFREQ_SELF_CTRL_MTCMOS
static void __gpufreq_mfg0_control(enum gpufreq_power_state power);
static void __gpufreq_mfg1_control(enum gpufreq_power_state power);
static void __gpufreq_mfg2_control(enum gpufreq_power_state power);
static void __gpufreq_mfg3_control(enum gpufreq_power_state power);
#endif /* GPUFREQ_SELF_CTRL_MTCMOS */
static int __gpufreq_clock_control(enum gpufreq_power_state power);
static int __gpufreq_mtcmos_control(enum gpufreq_power_state power);
static int __gpufreq_buck_control(enum gpufreq_power_state power);
static void __gpufreq_check_bus_idle(void);
static void __gpufreq_hwdcm_config(void);
static void __gpufreq_transaction_config(void);
/* bringup function */
static unsigned int __gpufreq_bringup(void);
static void __gpufreq_dump_bringup_status(struct platform_device *pdev);
/* init function */
static void __gpufreq_init_shader_present(void);
static void __gpufreq_compute_aging(void);
static void __gpufreq_compute_avs(void);
static void __gpufreq_apply_adjustment(void);
static int __gpufreq_init_opp_idx(void);
static void __gpufreq_init_opp_table(void);
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

static void __iomem *g_apmixed_base;
static void __iomem *g_mfg_top_base;
static void __iomem *g_sleep;
static void __iomem *g_topckgen_base;
static void __iomem *g_infracfg_base;
static void __iomem *g_nth_emicfg_base;
static void __iomem *g_sth_emicfg_base;
static void __iomem *g_nth_emicfg_ao_mem_base;
static void __iomem *g_sth_emicfg_ao_mem_base;
static void __iomem *g_infracfg_ao_base;
static void __iomem *g_infra_ao_debug_ctrl;
static void __iomem *g_infra_ao1_debug_ctrl;
static void __iomem *g_nth_emi_ao_debug_ctrl;
static void __iomem *g_sth_emi_ao_debug_ctrl;
static void __iomem *g_efuse_base;
static void __iomem *g_mali_base;
static struct gpufreq_pmic_info *g_pmic;
static struct gpufreq_clk_info *g_clk;
static struct gpufreq_mtcmos_info *g_mtcmos;
static struct gpufreq_status g_gpu;
static struct gpufreq_asensor_info g_asensor_info;
static unsigned int g_shader_present;
static unsigned int g_mcl50_load;
static unsigned int g_aging_table_idx;
static unsigned int g_asensor_enable;
static unsigned int g_aging_load;
static unsigned int g_aging_margin;
static unsigned int g_avs_enable;
static unsigned int g_avs_margin;
static int g_temperature;
static int g_temper_comp_nf_vgpu;
static unsigned int g_ptp_version;
static unsigned int g_gpueb_support;
static unsigned int g_mt6377_support;
static unsigned int g_gpufreq_ready;
static enum gpufreq_dvfs_state g_dvfs_state;
static struct gpufreq_shared_status *g_shared_status;
static DEFINE_MUTEX(gpufreq_lock);

static struct gpufreq_platform_fp platform_ap_fp = {
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
	.get_fgpu_by_idx = __gpufreq_get_fgpu_by_idx,
	.get_pgpu_by_idx = __gpufreq_get_pgpu_by_idx,
	.get_idx_by_fgpu = __gpufreq_get_idx_by_fgpu,
	.get_lkg_pgpu = __gpufreq_get_lkg_pgpu,
	.get_dyn_pgpu = __gpufreq_get_dyn_pgpu,
	.power_control = __gpufreq_power_control,
	.generic_commit_gpu = __gpufreq_generic_commit_gpu,
	.fix_target_oppidx_gpu = __gpufreq_fix_target_oppidx_gpu,
	.fix_custom_freq_volt_gpu = __gpufreq_fix_custom_freq_volt_gpu,
	.dump_infra_status = __gpufreq_dump_infra_status,
	.set_mfgsys_config = __gpufreq_set_mfgsys_config,
	.update_debug_opp_info = __gpufreq_update_debug_opp_info,
	.set_shared_status = __gpufreq_set_shared_status,
	.update_temperature = __gpufreq_update_temperature,
};

static struct gpufreq_platform_fp platform_eb_fp = {
	.dump_infra_status = __gpufreq_dump_infra_status,
	.get_dyn_pgpu = __gpufreq_get_dyn_pgpu,
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

/* API: get current Volt of CORE */
unsigned int __gpufreq_get_cur_vcore(void)
{
	return g_gpu.cur_vcore;
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
		return volt;
	else
		return VSRAM_LEVEL_0;
}

/* API: get Volt of CORE via volt of SRAM */
static unsigned int __gpufreq_get_vcore_by_vsram(unsigned int volt)
{
	if (volt > VSRAM_LEVEL_1)
		return VCORE_LEVEL_2;
	else if (volt > VSRAM_LEVEL_0)
		return VCORE_LEVEL_1;
	else
		return VCORE_LEVEL_0;
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
	u64 power_time = 0;

	GPUFREQ_TRACE_START("power=%d", power);

	mutex_lock(&gpufreq_lock);

	GPUFREQ_LOGD("switch power: %s (Power: %d, Buck: %d, MTCMOS: %d, CG: %d)",
		power ? "On" : "Off",
		g_gpu.power_count, g_gpu.buck_count,
		g_gpu.mtcmos_count, g_gpu.cg_count);

	if (power == POWER_ON)
		g_gpu.power_count++;
	else
		g_gpu.power_count--;
	__gpufreq_footprint_power_count(g_gpu.power_count);

#if GPUFREQ_HISTORY_ENABLE
	__gpufreq_power_history_entry();
#endif /* GPUFREQ_HISTORY_ENABLE */

	if (power == POWER_ON && g_gpu.power_count == 1) {
		__gpufreq_footprint_power_step(0x01);

		/* control Buck */
		ret = __gpufreq_buck_control(POWER_ON);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to control Buck On (%d)", ret);
			ret = GPUFREQ_EINVAL;
			goto done_unlock;
		}
		__gpufreq_footprint_power_step(0x02);

		/* control MTCMOS */
		ret = __gpufreq_mtcmos_control(POWER_ON);
		if (unlikely(ret < 0)) {
			GPUFREQ_LOGE("fail to control MTCMOS On (%d)", ret);
			ret = GPUFREQ_EINVAL;
			goto done_unlock;
		}
		__gpufreq_footprint_power_step(0x03);

		/* control clock */
		ret = __gpufreq_clock_control(POWER_ON);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to control Clock On (%d)", ret);
			ret = GPUFREQ_EINVAL;
			goto done_unlock;
		}
		__gpufreq_footprint_power_step(0x04);

		/* config HWDCM */
		__gpufreq_hwdcm_config();
		__gpufreq_footprint_power_step(0x05);

		/* config AXI transaction */
		__gpufreq_transaction_config();
		__gpufreq_footprint_power_step(0x06);

		/* free DVFS when power on */
		g_dvfs_state &= ~DVFS_POWEROFF;
		__gpufreq_footprint_power_step(0x07);
	} else if (power == POWER_OFF && g_gpu.power_count == 0) {
		__gpufreq_footprint_power_step(0x08);

		/* check all transaction complete before power off */
		__gpufreq_check_bus_idle();
		__gpufreq_footprint_power_step(0x09);

		/* freeze DVFS when power off */
		g_dvfs_state |= DVFS_POWEROFF;
		__gpufreq_footprint_power_step(0x10);

		/* control clock */
		ret = __gpufreq_clock_control(POWER_OFF);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to control Clock Off (%d)", ret);
			ret = GPUFREQ_EINVAL;
			goto done_unlock;
		}
		__gpufreq_footprint_power_step(0x11);

		/* control MTCMOS */
		ret = __gpufreq_mtcmos_control(POWER_OFF);
		if (unlikely(ret < 0)) {
			GPUFREQ_LOGE("fail to control MTCMOS Off (%d)", ret);
			ret = GPUFREQ_EINVAL;
			goto done_unlock;
		}
		__gpufreq_footprint_power_step(0x12);

		/* control Buck */
		ret = __gpufreq_buck_control(POWER_OFF);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to control Buck Off (%d)", ret);
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
		g_shared_status->cg_count = g_gpu.cg_count;
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
	GPUFREQ_LOGD("PWR_STATUS (0x%x): 0x%08x",
		(0x1C001F4C), readl(XPU_PWR_STATUS));

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
	struct gpufreq_opp_info *working_table = g_gpu.working_table;
	int cur_oppidx = 0;
	int opp_num = g_gpu.opp_num;
	unsigned int cur_freq = 0, target_freq = 0;
	unsigned int cur_volt = 0, target_volt = 0;
	unsigned int cur_vsram = 0, target_vsram = 0;
	int ret = GPUFREQ_SUCCESS;

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
	cur_freq = g_gpu.cur_freq;
	cur_volt = g_gpu.cur_volt;
	cur_vsram = g_gpu.cur_vsram;
	target_freq = working_table[target_oppidx].freq;
	target_volt = working_table[target_oppidx].volt;

	/* temperature compensation check */
	target_volt = (int)target_volt + g_temper_comp_nf_vgpu;

	/* clamp to signoff */
	if (target_volt > GPU_MAX_SIGNOFF_VOLT)
		target_volt = GPU_MAX_SIGNOFF_VOLT;

	target_vsram = __gpufreq_get_vsram_by_vgpu(target_volt);

	GPUFREQ_LOGD("begin to commit GPU OPP index: (%d->%d)",
		cur_oppidx, target_oppidx);

	ret = __gpufreq_generic_scale_gpu(cur_freq, target_freq,
		cur_volt, target_volt, cur_vsram, target_vsram);
	if (unlikely(ret)) {
		GPUFREQ_LOGE(
			"fail to scale Idx(%d->%d), Freq(%d->%d), Volt(%d->%d), Vsram(%d->%d)",
			cur_oppidx, target_oppidx, cur_freq, target_freq,
			cur_volt, target_volt, cur_vsram, target_vsram);
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
		__gpufreq_update_shared_status_dvfs_reg();
	}

#ifdef GPUFREQ_HISTORY_ENABLE
	/* update history record to shared memory */
	if (target_oppidx != cur_oppidx)
		__gpufreq_record_history_entry(HISTORY_FREE);
#endif /* GPUFREQ_HISTORY_ENABLE */

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
	if (unlikely(ret < 0))
		goto done;

	if (oppidx == -1) {
		__gpufreq_set_dvfs_state(false, DVFS_FIX_OPP);
		ret = GPUFREQ_SUCCESS;
	} else if (oppidx >= 0 && oppidx < opp_num) {
		__gpufreq_set_dvfs_state(true, DVFS_FIX_OPP);

#ifdef GPUFREQ_HISTORY_ENABLE
		gpufreq_set_history_target_opp(TARGET_GPU, oppidx);
#endif /* GPUFREQ_HISTORY_ENABLE */

		ret = __gpufreq_generic_commit_gpu(oppidx, DVFS_FIX_OPP);
		if (unlikely(ret))
			__gpufreq_set_dvfs_state(false, DVFS_FIX_OPP);
	} else
		ret = GPUFREQ_EINVAL;

	__gpufreq_power_control(POWER_OFF);

done:
	if (unlikely(ret))
		GPUFREQ_LOGE("fail to commit idx: %d", oppidx);

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
	unsigned int max_freq = 0, min_freq = 0;
	unsigned int max_volt = 0, min_volt = 0;
	int ret = GPUFREQ_SUCCESS;

	ret = __gpufreq_power_control(POWER_ON);
	if (unlikely(ret < 0))
		goto done;

	max_freq = POSDIV_2_MAX_FREQ;
	min_freq = POSDIV_4_MIN_FREQ;
	max_volt = VGPU_MAX_VOLT;
	min_volt = VGPU_MIN_VOLT;

	if (freq == 0 && volt == 0) {
		__gpufreq_set_dvfs_state(false, DVFS_FIX_FREQ_VOLT);
		ret = GPUFREQ_SUCCESS;
	} else if (freq > max_freq || freq < min_freq) {
		ret = GPUFREQ_EINVAL;
	} else if (volt > max_volt || volt < min_volt) {
		ret = GPUFREQ_EINVAL;
	} else {
		__gpufreq_set_dvfs_state(true, DVFS_FIX_FREQ_VOLT);

		ret = __gpufreq_custom_commit_gpu(freq, volt, DVFS_FIX_FREQ_VOLT);
		if (unlikely(ret))
			__gpufreq_set_dvfs_state(false, DVFS_FIX_FREQ_VOLT);
	}

	__gpufreq_power_control(POWER_OFF);

done:
	if (unlikely(ret))
		GPUFREQ_LOGE("fail to commit Freq: %d, Volt: %d", freq, volt);

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
	GPUFREQ_LOGI("[Regulator] Vgpu: %d, Vsram: %d",
		g_gpu.cur_volt, g_gpu.cur_vsram);
	GPUFREQ_LOGI("[Clk] MFG_PLL: %d, MFG_SEL: 0x%x",
		g_gpu.cur_freq, readl(TOPCK_CLK_CFG_20) & MFG_PLL_SEL_MASK);

	/* 0x13FBF000 */
	if (MFG_TOP_CFG_BASE) {
		/* MFG_QCHANNEL_CON 0x13FBF0B4 [0] MFG_ACTIVE_SEL = 1'b1 */
		writel((readl(MFG_QCHANNEL_CON) | BIT(0)), MFG_QCHANNEL_CON);
		/* MFG_DEBUG_SEL 0x13FBF170 [1:0] MFG_DEBUG_TOP_SEL = 2'b11 */
		writel((readl(MFG_DEBUG_SEL) | GENMASK(1, 0)), MFG_DEBUG_SEL);

		/* MFG_DEBUG_SEL */
		/* MFG_DEBUG_TOP */
		GPUFREQ_LOGI("%-7s (0x%x): 0x%08x, (0x%x): 0x%08x",
			"[MFG]",
			(0x13FBF170), readl(MFG_DEBUG_SEL),
			(0x13FBF178), readl(MFG_DEBUG_TOP));
	}

	/* 0x1021C000, 0x1021E000 */
	if (NTH_EMICFG_BASE && STH_EMICFG_BASE) {
		/* NTH_MFG_EMI1_GALS_SLV_DBG */
		/* NTH_MFG_EMI0_GALS_SLV_DBG */
		/* STH_MFG_EMI1_GALS_SLV_DBG */
		/* STH_MFG_EMI0_GALS_SLV_DBG */
		GPUFREQ_LOGI("%-7s (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x",
			"[EMI]",
			(0x1021C82C), readl(NTH_MFG_EMI1_GALS_SLV_DBG),
			(0x1021C830), readl(NTH_MFG_EMI0_GALS_SLV_DBG),
			(0x1021E82C), readl(STH_MFG_EMI1_GALS_SLV_DBG),
			(0x1021E830), readl(STH_MFG_EMI0_GALS_SLV_DBG));
	}

	/* 0x10270000, 0x1030E000 */
	if (NTH_EMICFG_AO_MEM_BASE && STH_EMICFG_AO_MEM_BASE) {
		/* NTH_M6M7_IDLE_BIT_EN_1 */
		/* NTH_M6M7_IDLE_BIT_EN_0 */
		/* STH_M6M7_IDLE_BIT_EN_1 */
		/* STH_M6M7_IDLE_BIT_EN_0 */
		GPUFREQ_LOGI("%-7s (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x",
			"[EMI]",
			(0x10270228), readl(NTH_M6M7_IDLE_BIT_EN_1),
			(0x1027022C), readl(NTH_M6M7_IDLE_BIT_EN_0),
			(0x1030E228), readl(STH_M6M7_IDLE_BIT_EN_1),
			(0x1030E22C), readl(STH_M6M7_IDLE_BIT_EN_0));
	}

	/* 0x10001000 */
	if (IFNFRA_AO_BASE) {
		/* INFRASYS_PROTECT_EN_STA_0 */
		/* INFRASYS_PROTECT_RDY_STA_0 */
		/* INFRASYS_PROTECT_EN_STA_1 */
		/* INFRASYS_PROTECT_RDY_STA_1 */
		GPUFREQ_LOGI("%-7s (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x",
			"[INFRA]",
			(0x10001C40), readl(INFRASYS_PROTECT_EN_STA_0),
			(0x10001C4C), readl(INFRASYS_PROTECT_RDY_STA_0),
			(0x10001C50), readl(INFRASYS_PROTECT_EN_STA_1),
			(0x10001C5C), readl(INFRASYS_PROTECT_RDY_STA_1));
		/* EMISYS_PROTECT_EN_STA_0 */
		/* EMISYS_PROTECT_RDY_STA_0 */
		/* MD_MFGSYS_PROTECT_EN_STA_0 */
		/* MD_MFGSYS_PROTECT_RDY_STA_0 */
		GPUFREQ_LOGI("%-7s (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x",
			"[INFRA]",
			(0x10001C60), readl(EMISYS_PROTECT_EN_STA_0),
			(0x10001C6C), readl(EMISYS_PROTECT_RDY_STA_0),
			(0x10001CA0), readl(MD_MFGSYS_PROTECT_EN_STA_0),
			(0x10001CAC), readl(MD_MFGSYS_PROTECT_RDY_STA_0));
	}

	/* 0x10042000, 0x10028000, 0x10023000, 0x1002B000 */
	if (NTH_EMI_AO_DEBUG_CTRL_BASE && STH_EMI_AO_DEBUG_CTRL_BASE
		&& INFRA_AO_DEBUG_CTRL_BASE && INFRA_AO1_DEBUG_CTRL_BASE) {
		/* NTH_EMI_AO_DEBUG_CTRL0 */
		/* STH_EMI_AO_DEBUG_CTRL0 */
		/* INFRA_AO_BUS0_U_DEBUG_CTRL0 */
		/* INFRA_AO1_BUS1_U_DEBUG_CTRL0 */
		GPUFREQ_LOGI("%-7s (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x",
			"[EMI_INFRA]",
			(0x10042000), readl(NTH_EMI_AO_DEBUG_CTRL0),
			(0x10028000), readl(STH_EMI_AO_DEBUG_CTRL0),
			(0x10023000), readl(INFRA_AO_BUS0_U_DEBUG_CTRL0),
			(0x1002B000), readl(INFRA_AO1_BUS1_U_DEBUG_CTRL0));
	}

	/* 0x1C001000 */
	if (SPM_BASE) {
		/* MFG0_PWR_CON - MFG3_PWR_CON */
		GPUFREQ_LOGI("%-7s (0x%x-%x): 0x%08x 0x%08x 0x%08x 0x%08x",
			"[MFG0-3]", (0x1C001EB8), 0xEC4,
			readl(MFG0_PWR_CON), readl(MFG1_PWR_CON),
			readl(MFG2_PWR_CON), readl(MFG3_PWR_CON));
		/* XPU_PWR_STATUS */
		/* XPU_PWR_STATUS_2ND */
		GPUFREQ_LOGI("%-7s (0x%x): 0x%08x, (0x%x): 0x%08x",
			"[SPM]",
			(0x1C001F4C), readl(XPU_PWR_STATUS),
			(0x1C001F50), readl(XPU_PWR_STATUS_2ND));
	}
}

/* API: get working OPP index of GPU limited by BATTERY_OC via given level */
int __gpufreq_get_batt_oc_idx(int batt_oc_level)
{
#if (GPUFREQ_BATT_OC_ENABLE && IS_ENABLED(CONFIG_MTK_BATTERY_OC_POWER_THROTTLING))
	if (batt_oc_level == 1)
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
	GPUFREQ_UNREFERENCED(batt_percent_level);

	return GPUPPM_KEEP_IDX;
}

/* API: get working OPP index of GPU limited by LOW_BATTERY via given level */
int __gpufreq_get_low_batt_idx(int low_batt_level)
{
#if (GPUFREQ_LOW_BATT_ENABLE && IS_ENABLED(CONFIG_MTK_LOW_BATTERY_POWER_THROTTLING))
	if (low_batt_level == 2)
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
	mutex_lock(&gpufreq_lock);

	/* update current status to shared memory */
	if (__gpufreq_get_power_state()) {
		g_shared_status->cur_con1_fgpu = __gpufreq_get_real_fgpu();
		g_shared_status->cur_fmeter_fgpu = __gpufreq_get_fmeter_freq();
		g_shared_status->cur_regulator_vgpu = __gpufreq_get_real_vgpu();
		g_shared_status->cur_regulator_vsram_gpu = __gpufreq_get_real_vsram();
	} else {
		g_shared_status->cur_con1_fgpu = 0;
		g_shared_status->cur_fmeter_fgpu = 0;
		g_shared_status->cur_regulator_vgpu = 0;
		g_shared_status->cur_regulator_vsram_gpu = 0;
	}

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
	case CONFIG_OCL_TIMESTAMP:
		__gpufreq_set_ocl_timestamp();
		break;
	default:
		GPUFREQ_LOGE("invalid config target: %d", target);
		break;
	}

	mutex_unlock(&gpufreq_lock);
}

/* API: update GPU temper and temper related features */
void __gpufreq_update_temperature(unsigned int instant_dvfs)
{
#if GPUFREQ_TEMPER_COMP_ENABLE
	struct thermal_zone_device *tzd;
	int cur_temper = 0, ret = 0;

#if IS_ENABLED(CONFIG_THERMAL)
	tzd = thermal_zone_get_zone_by_name("gpu2");
	ret = thermal_zone_get_temp(tzd, &cur_temper);
	cur_temper /= 1000;
#else
	cur_temper = 30;
#endif /* CONFIG_THERMAL */

	mutex_lock(&gpufreq_lock);

	g_temperature = cur_temper;

	/* compensate Volt according to temper */
	__gpufreq_set_temper_compensation(instant_dvfs);

	if (g_shared_status) {
		g_shared_status->temperature = g_temperature;
		g_shared_status->temper_comp_norm_gpu = g_temper_comp_nf_vgpu;
	}

	mutex_unlock(&gpufreq_lock);
#endif /* GPUFREQ_TEMPER_COMP_ENABLE */
}

static void __gpufreq_set_temper_compensation(unsigned int instant_dvfs)
{
#if GPUFREQ_TEMPER_COMP_ENABLE
	if (g_avs_margin && g_temperature >= 10 && g_temperature < 25)
		g_temper_comp_nf_vgpu = TEMPER_COMP_10_25_VOLT;
	else if (g_avs_margin && g_temperature < 10)
		g_temper_comp_nf_vgpu = TEMPER_COMP_10_VOLT;
	else
		g_temper_comp_nf_vgpu = TEMPER_COMP_DEFAULT_VOLT;

	GPUFREQ_LOGD("current temper: %d, vgpu: %d",
		g_temperature, g_temper_comp_nf_vgpu);

	/* additionally commit to update compensate Volt */
	if (instant_dvfs)
		__gpufreq_generic_commit_gpu(g_gpu.cur_oppidx, DVFS_FREE);
#endif /* GPUFREQ_TEMPER_COMP_ENABLE */
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
		g_shared_status->power_count = g_gpu.power_count;
		g_shared_status->buck_count = g_gpu.buck_count;
		g_shared_status->mtcmos_count = g_gpu.mtcmos_count;
		g_shared_status->cg_count = g_gpu.cg_count;
		g_shared_status->power_control = __gpufreq_power_ctrl_enable();
		g_shared_status->dvfs_state = g_dvfs_state;
		g_shared_status->shader_present = g_shader_present;
		g_shared_status->asensor_enable = g_asensor_enable;
		g_shared_status->aging_load = g_aging_load;
		g_shared_status->aging_margin = g_aging_margin;
		g_shared_status->avs_enable = g_avs_enable;
		g_shared_status->avs_margin = g_avs_margin;
		g_shared_status->ptp_version = g_ptp_version;
		g_shared_status->temper_comp_mode = GPUFREQ_TEMPER_COMP_ENABLE;
		g_shared_status->temperature = g_temperature;
		g_shared_status->temper_comp_norm_gpu = g_temper_comp_nf_vgpu;
		g_shared_status->dual_buck = false;
		g_shared_status->segment_id = g_gpu.segment_id;
#if GPUFREQ_ASENSOR_ENABLE
		g_shared_status->asensor_info = g_asensor_info;
#endif /* GPUFREQ_ASENSOR_ENABLE */
		__gpufreq_update_shared_status_opp_table();
		__gpufreq_update_shared_status_adj_table();
		__gpufreq_update_shared_status_init_reg();
	}

	mutex_unlock(&gpufreq_lock);
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
	copy_size = sizeof(struct gpufreq_adj_info) * NUM_GPU_SIGNED_IDX;
	memcpy(g_shared_status->aging_table_gpu, g_gpu_aging_table[g_aging_table_idx], copy_size);
	/* avs table */
	copy_size = sizeof(struct gpufreq_adj_info) * NUM_GPU_SIGNED_IDX;
	memcpy(g_shared_status->avs_table_gpu, g_avs_table, copy_size);
}

static void __gpufreq_update_shared_status_init_reg(void)
{
#if GPUFREQ_SHARED_STATUS_REG
	unsigned int copy_size = 0;

	if (!g_shared_status)
		return;

	/* [WARNING] GPU is power off at this moment */
	g_reg_mfgsys[IDX_EFUSE_PTPOD20_AVS].val = readl(EFUSE_PTPOD20_AVS);
	g_reg_mfgsys[IDX_EFUSE_PTPOD21_AVS].val = readl(EFUSE_PTPOD21_AVS);
	g_reg_mfgsys[IDX_EFUSE_PTPOD22_AVS].val = readl(EFUSE_PTPOD22_AVS);

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
	g_reg_mfgsys[IDX_MFG_ASYNC_CON].val = readl(MFG_ASYNC_CON);
	g_reg_mfgsys[IDX_MFG_ASYNC_CON_1].val = readl(MFG_ASYNC_CON_1);
	g_reg_mfgsys[IDX_MFG_GLOBAL_CON].val = readl(MFG_GLOBAL_CON);
	g_reg_mfgsys[IDX_MFG_I2M_PROTECTOR_CFG_00].val = readl(MFG_I2M_PROTECTOR_CFG_00);
	g_reg_mfgsys[IDX_MFG_PLL_CON0].val = readl(MFG_PLL_CON0);
	g_reg_mfgsys[IDX_MFG_PLL_CON1].val = readl(MFG_PLL_CON1);
	g_reg_mfgsys[IDX_MFG_MFG0_PWR_CON].val = readl(MFG0_PWR_CON);
	g_reg_mfgsys[IDX_MFG_MFG1_PWR_CON].val = readl(MFG1_PWR_CON);
	g_reg_mfgsys[IDX_MFG_MFG2_PWR_CON].val = readl(MFG2_PWR_CON);
	g_reg_mfgsys[IDX_MFG_MFG3_PWR_CON].val = readl(MFG3_PWR_CON);
	g_reg_mfgsys[IDX_TOPCK_CLK_CFG_13].val = readl(TOPCK_CLK_CFG_13);
	g_reg_mfgsys[IDX_TOPCK_CLK_CFG_20].val = readl(TOPCK_CLK_CFG_20);
	g_reg_mfgsys[IDX_NTH_MFG_EMI1_GALS_SLV_DBG].val = readl(NTH_MFG_EMI1_GALS_SLV_DBG);
	g_reg_mfgsys[IDX_NTH_MFG_EMI0_GALS_SLV_DBG].val = readl(NTH_MFG_EMI0_GALS_SLV_DBG);
	g_reg_mfgsys[IDX_STH_MFG_EMI1_GALS_SLV_DBG].val = readl(STH_MFG_EMI1_GALS_SLV_DBG);
	g_reg_mfgsys[IDX_STH_MFG_EMI0_GALS_SLV_DBG].val = readl(STH_MFG_EMI0_GALS_SLV_DBG);
	g_reg_mfgsys[IDX_NTH_M6M7_IDLE_BIT_EN_1].val = readl(NTH_M6M7_IDLE_BIT_EN_1);
	g_reg_mfgsys[IDX_NTH_M6M7_IDLE_BIT_EN_0].val = readl(NTH_M6M7_IDLE_BIT_EN_0);
	g_reg_mfgsys[IDX_STH_M6M7_IDLE_BIT_EN_1].val = readl(STH_M6M7_IDLE_BIT_EN_1);
	g_reg_mfgsys[IDX_STH_M6M7_IDLE_BIT_EN_0].val = readl(STH_M6M7_IDLE_BIT_EN_0);
	g_reg_mfgsys[IDX_INFRASYS_PROTECT_EN_STA_0].val = readl(INFRASYS_PROTECT_EN_STA_0);
	g_reg_mfgsys[IDX_INFRASYS_PROTECT_RDY_STA_0].val = readl(INFRASYS_PROTECT_RDY_STA_0);
	g_reg_mfgsys[IDX_INFRASYS_PROTECT_EN_STA_1].val = readl(INFRASYS_PROTECT_EN_STA_1);
	g_reg_mfgsys[IDX_INFRASYS_PROTECT_RDY_STA_1].val = readl(INFRASYS_PROTECT_RDY_STA_1);
	g_reg_mfgsys[IDX_EMISYS_PROTECT_EN_STA_0].val = readl(EMISYS_PROTECT_EN_STA_0);
	g_reg_mfgsys[IDX_EMISYS_PROTECT_RDY_STA_0].val = readl(EMISYS_PROTECT_RDY_STA_0);
	g_reg_mfgsys[IDX_MD_MFGSYS_PROTECT_EN_STA_0].val = readl(MD_MFGSYS_PROTECT_EN_STA_0);
	g_reg_mfgsys[IDX_MD_MFGSYS_PROTECT_RDY_STA_0].val = readl(MD_MFGSYS_PROTECT_RDY_STA_0);
	g_reg_mfgsys[IDX_INFRA_AO_BUS0_U_DEBUG_CTRL0].val = readl(INFRA_AO_BUS0_U_DEBUG_CTRL0);
	g_reg_mfgsys[IDX_INFRA_AO1_BUS1_U_DEBUG_CTRL0].val = readl(INFRA_AO1_BUS1_U_DEBUG_CTRL0);
	g_reg_mfgsys[IDX_NTH_EMI_AO_DEBUG_CTRL0].val = readl(NTH_EMI_AO_DEBUG_CTRL0);
	g_reg_mfgsys[IDX_STH_EMI_AO_DEBUG_CTRL0].val = readl(STH_EMI_AO_DEBUG_CTRL0);

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
	g_reg_mfgsys[IDX_TOPCK_CLK_CFG_13].val = readl(TOPCK_CLK_CFG_13);
	g_reg_mfgsys[IDX_TOPCK_CLK_CFG_20].val = readl(TOPCK_CLK_CFG_20);

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

static void __gpufreq_set_ocl_timestamp(void)
{
	/* MFG_TIMESTAMP 0x13FBF130 [0] top_tsvalueb_en = 1'b1 */
	/* MFG_TIMESTAMP 0x13FBF130 [1] timer_sel = 1'b1 */
	writel(GENMASK(1, 0), MFG_TIMESTAMP);
}

/* API: apply/restore Volt margin to OPP table */
static void __gpufreq_set_margin_mode(unsigned int mode)
{
	/* update volt margin */
	__gpufreq_apply_restore_margin(mode);

	/* update power info to working table */
	__gpufreq_measure_power();

	/* update current status to shared memory */
	if (g_shared_status) {
		g_shared_status->aging_margin = mode ? g_aging_margin : mode;
		g_shared_status->avs_margin = mode ? g_avs_margin : mode;
		__gpufreq_update_shared_status_opp_table();
	}
}

/* API: mode=true: apply marign, mode=false: restore margin */
static void __gpufreq_apply_restore_margin(unsigned int mode)
{
	struct gpufreq_opp_info *working_table = NULL;
	struct gpufreq_opp_info *signed_table = NULL;
	int working_opp_num = 0, signed_opp_num = 0, segment_upbound = 0, i = 0;

	working_table = g_gpu.working_table;
	signed_table = g_gpu.signed_table;
	working_opp_num = g_gpu.opp_num;
	signed_opp_num = g_gpu.signed_opp_num;
	segment_upbound = g_gpu.segment_upbound;

	mutex_lock(&gpufreq_lock);

	/* update margin to signed table */
	for (i = 0; i < signed_opp_num; i++) {
		if (!mode)
			signed_table[i].volt += signed_table[i].margin;
		else
			signed_table[i].volt -= signed_table[i].margin;
		signed_table[i].vsram = __gpufreq_get_vsram_by_vgpu(signed_table[i].volt);
	}

	for (i = 0; i < working_opp_num; i++) {
		working_table[i].volt = signed_table[segment_upbound + i].volt;
		working_table[i].vsram = signed_table[segment_upbound + i].vsram;

		GPUFREQ_LOGD("Margin mode: %d, GPU[%d] Volt: %d, Vsram: %d",
			mode, i, working_table[i].volt, working_table[i].vsram);
	}

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

	/* scaling up: (Vcore) -> (Vsram) -> Vgpu -> Freq */
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
	/* scaling down: freq -> Vgpu -> (Vsram) -> (Vcore) */
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

	/* update current status to shared memory */
	if (g_shared_status) {
		g_shared_status->cur_oppidx_gpu = g_gpu.cur_oppidx;
		g_shared_status->cur_fgpu = g_gpu.cur_freq;
		g_shared_status->cur_vgpu = g_gpu.cur_volt;
		g_shared_status->cur_vsram_gpu = g_gpu.cur_vsram;
		g_shared_status->cur_power_gpu = g_gpu.working_table[g_gpu.cur_oppidx].power;
		__gpufreq_update_shared_status_dvfs_reg();
	}

done_unlock:
	mutex_unlock(&gpufreq_lock);

	GPUFREQ_TRACE_END();

	return ret;
}

static int __gpufreq_switch_clksrc(enum gpufreq_clk_src clksrc)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("clksrc=%d", clksrc);

	if (clksrc == CLOCK_MAIN)
		ret = clk_set_parent(g_clk->clk_mux, g_clk->clk_main_parent);
	else if (clksrc == CLOCK_SUB)
		ret = clk_set_parent(g_clk->clk_mux, g_clk->clk_sub_parent);

	if (unlikely(ret)) {
		__gpufreq_abort("fail to switch GPU to %s (%d)",
			clksrc ==  CLOCK_MAIN ? "CLOCK_MAIN" : "CLOCK_SUB", ret);
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

	if ((freq >= POSDIV_4_MIN_FREQ) && (freq <= POSDIV_2_MAX_FREQ)) {
		pcw = (((freq / TO_MHZ_HEAD * (1 << posdiv)) << DDS_SHIFT)
			/ MFGPLL_FIN + ROUNDING_VALUE) / TO_MHZ_TAIL;
	} else {
		__gpufreq_abort("out of range Freq: %d", freq);
	}

	return pcw;
}

static enum gpufreq_posdiv __gpufreq_get_real_posdiv_gpu(void)
{
	unsigned int mfgpll = 0;
	enum gpufreq_posdiv posdiv = POSDIV_POWER_1;

	mfgpll = readl(MFG_PLL_CON1);

	posdiv = (mfgpll & GENMASK(26, 24)) >> POSDIV_SHIFT;

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
			__gpufreq_abort("fail to hopping PCW: 0x%x (%d)", pcw, ret);
			ret = GPUFREQ_EINVAL;
			goto done;
		}
	}
#else
	/* compute CON1 with PCW and POSDIV */
	pll = (0x80000000) | (target_posdiv << POSDIV_SHIFT) | pcw;

	/* 1. switch to parking clk source */
	ret = __gpufreq_switch_clksrc(CLOCK_SUB);
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to switch sub clock source (%d)", ret);
		goto done;
	}
	/* 2. change PCW and POSDIV by writing CON1 */
	writel(pll, MFG_PLL_CON1);
	/* 3. wait until PLL stable */
	udelay(20);
	/* 4. switch to main clk source */
	ret = __gpufreq_switch_clksrc(CLOCK_MAIN);
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to switch main clock source (%d)", ret);
		goto done;
	}
#endif /* GPUFREQ_FHCTL_ENABLE && CONFIG_COMMON_CLK_MTK_FREQ_HOPPING */

	g_gpu.cur_freq = __gpufreq_get_real_fgpu();

#ifdef GPUFREQ_HISTORY_ENABLE
	__gpufreq_record_history_entry(HISTORY_CHANGE_FREQ_TOP);
#endif /* GPUFREQ_HISTORY_ENABLE */

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

static unsigned int __gpufreq_settle_time_vgpu(unsigned int direction, int deltaV)
{
	/* [MT6363_VBUCK5][VGPU]
	 * [MT6319_VBUCK3][VGPU]
	 * DVFS Rising : (deltaV / 12.5(mV)) + 3.85us + 2us
	 * DVFS Falling: (deltaV / 10(mV)) + 3.85us + 2us
	 * deltaV = mV x 100
	 */
	unsigned int t_settle = 0;

	if (direction) {
		/* rising: 12.5mV/us*/
		t_settle = (deltaV / 1250) + 4 + 2;
	} else {
		/* falling: 6.25mV/us*/
		t_settle = (deltaV / 1000) + 4 + 2;
	}

	return t_settle; /* us */
}

static unsigned int __gpufreq_settle_time_vsram(unsigned int direction, int deltaV)
{
	unsigned int t_settle = 0;

	if (g_mt6377_support) {
		/* [MT6377_VSRAM_OTHERS][VSRAM_CORE]
		 * DVFS Rising : (deltaV / 12.5(mV)) + 5us
		 * DVFS Falling: (deltaV / 10(mV)) + 5us
		 * deltaV = mV x 100
		 */
		if (direction) {
			/* rising: 12.5mV/us*/
			t_settle = (deltaV / 1250) + 5;
		} else {
			/* falling: 10mV/us*/
			t_settle = (deltaV / 1000) + 5;
		}
	} else {
		/* [MT6363_VBUCK4][VSRAM_CORE]
		 * DVFS Rising : (deltaV / 12.5(mV)) + 3.85us + 2us
		 * DVFS Falling: (deltaV / 6.25(mV)) + 3.85us + 2us
		 * deltaV = mV x 100
		 */
		if (direction) {
			/* rising: 12.5mV/us*/
			t_settle = (deltaV / 1250) + 4 + 2;
		} else {
			/* falling: 6.25mV/us*/
			t_settle = (deltaV / 625) + 4 + 2;
		}
	}

	return t_settle; /* us */
}

/* API: scale Volt of GPU via Regulator */
static int __gpufreq_volt_scale_gpu(
	unsigned int volt_old, unsigned int volt_new,
	unsigned int vsram_old, unsigned int vsram_new)
{
	unsigned int t_settle = 0;
	unsigned int volt_park = SRAM_PARK_VOLT, vcore_new = 0;
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("volt_old=%d, volt_new=%d, vsram_old=%d, vsram_new=%d",
		volt_old, volt_new, vsram_old, vsram_new);

	GPUFREQ_LOGD("begin to scale Vgpu: (%d->%d), Vsram_gpu: (%d->%d)",
		volt_old, volt_new, vsram_old, vsram_new);

	/* volt scaling up */
	if (volt_new > volt_old) {
		if (vsram_new != vsram_old) {
			/* Vgpu scaling to parking volt when volt_old < 0.75V */
			if (volt_old < volt_park) {
				t_settle =  __gpufreq_settle_time_vgpu(true,
					(volt_park - volt_old));
				ret = regulator_set_voltage(g_pmic->reg_vgpu,
					volt_park * 10, VGPU_MAX_VOLT * 10 + 125);

#if GPUFREQ_HISTORY_ENABLE
				__gpufreq_record_history_entry(HISTORY_VOLT_PARK);
#endif /* GPUFREQ_HISTORY_ENABLE */

				if (unlikely(ret)) {
					__gpufreq_abort("fail to set regulator VGPU (%d)", ret);
					goto done;
				}
				udelay(t_settle);
			}

#if GPUFREQ_VCORE_DVFS_ENABLE
			/* Vcore scaling to target volt */
			vcore_new = __gpufreq_get_vcore_by_vsram(vsram_new);
			ret = regulator_set_voltage(g_pmic->reg_dvfsrc, vcore_new * 10, INT_MAX);
			if (unlikely(ret)) {
				__gpufreq_abort("fail to set regulator VCORE (%d)", ret);
				goto done;
			}
#endif /* GPUFREQ_VCORE_DVFS_ENABLE */

			/* Vsram scaling to target volt */
			t_settle =  __gpufreq_settle_time_vsram(true, (vsram_new - vsram_old));
			ret = regulator_set_voltage(g_pmic->reg_vsram,
				vsram_new * 10, VSRAM_MAX_VOLT * 10 + 125);

#if GPUFREQ_HISTORY_ENABLE
			__gpufreq_record_history_entry(HISTORY_VSRAM_PARK);
#endif /* GPUFREQ_HISTORY_ENABLE */

			if (unlikely(ret)) {
				__gpufreq_abort("fail to set regulator VSRAM (%d)", ret);
				goto done;
			}
			udelay(t_settle);

			/* Vgpu scaling to target volt */
			t_settle =  __gpufreq_settle_time_vgpu(true, (volt_new - volt_park));
			ret = regulator_set_voltage(g_pmic->reg_vgpu,
				volt_new * 10, VGPU_MAX_VOLT * 10 + 125);
			if (unlikely(ret)) {
				__gpufreq_abort("fail to set regulator VGPU (%d)", ret);
				goto done;
			}
			udelay(t_settle);
		} else {
			/* Vgpu scaling to target volt */
			t_settle =  __gpufreq_settle_time_vgpu(true, (volt_new - volt_old));
			ret = regulator_set_voltage(g_pmic->reg_vgpu,
				volt_new * 10, VGPU_MAX_VOLT * 10 + 125);
			if (unlikely(ret)) {
				__gpufreq_abort("fail to set regulator VGPU (%d)", ret);
				goto done;
			}
			udelay(t_settle);
		}
	/* volt scaling down */
	} else if (volt_new < volt_old) {
		if (vsram_new != vsram_old) {
			/* Vgpu scaling to parking volt when volt_new < 0.75V */
			if (volt_new < volt_park) {
				t_settle =  __gpufreq_settle_time_vgpu(false,
					(volt_old - volt_park));
				ret = regulator_set_voltage(g_pmic->reg_vgpu,
					volt_park * 10, VGPU_MAX_VOLT * 10 + 125);

#if GPUFREQ_HISTORY_ENABLE
				__gpufreq_record_history_entry(HISTORY_VOLT_PARK);
#endif /* GPUFREQ_HISTORY_ENABLE */

				if (unlikely(ret)) {
					__gpufreq_abort("fail to set regulator VGPU (%d)", ret);
					goto done;
				}
				udelay(t_settle);
			/* Vgpu scaling to target volt when volt_new >= 0.75V */
			} else {
				t_settle =  __gpufreq_settle_time_vgpu(false,
					(volt_old - volt_new));
				ret = regulator_set_voltage(g_pmic->reg_vgpu,
					volt_new * 10, VGPU_MAX_VOLT * 10 + 125);
				if (unlikely(ret)) {
					__gpufreq_abort("fail to set regulator VGPU (%d)", ret);
					goto done;
				}
				udelay(t_settle);
			}

			/* Vsram scaling to target volt */
			t_settle =  __gpufreq_settle_time_vsram(false, (vsram_old - vsram_new));
			ret = regulator_set_voltage(g_pmic->reg_vsram,
				vsram_new * 10, VSRAM_MAX_VOLT * 10 + 125);

#if GPUFREQ_HISTORY_ENABLE
			__gpufreq_record_history_entry(HISTORY_VSRAM_PARK);
#endif /* GPUFREQ_HISTORY_ENABLE */

			if (unlikely(ret)) {
				__gpufreq_abort("fail to set regulator VSRAM (%d)", ret);
				goto done;
			}
			udelay(t_settle);

#if GPUFREQ_VCORE_DVFS_ENABLE
			/* Vcore scaling to target volt */
			vcore_new = __gpufreq_get_vcore_by_vsram(vsram_new);
			ret = regulator_set_voltage(g_pmic->reg_dvfsrc, vcore_new * 10, INT_MAX);
			if (unlikely(ret)) {
				__gpufreq_abort("fail to set regulator VCORE (%d)", ret);
				goto done;
			}
#endif /* GPUFREQ_VCORE_DVFS_ENABLE */
			/* Vgpu scaling to target volt from parking volt */
			if (volt_new < volt_park) {
				t_settle =  __gpufreq_settle_time_vgpu(false,
					(volt_park - volt_new));
				ret = regulator_set_voltage(g_pmic->reg_vgpu,
					volt_new * 10, VGPU_MAX_VOLT * 10 + 125);
				if (unlikely(ret)) {
					__gpufreq_abort("fail to set regulator VGPU (%d)", ret);
					goto done;
				}
				udelay(t_settle);
			}
		} else {
			/* Vgpu scaling to target volt */
			t_settle =  __gpufreq_settle_time_vgpu(false, (volt_old - volt_new));
			ret = regulator_set_voltage(g_pmic->reg_vgpu,
				volt_new * 10, VGPU_MAX_VOLT * 10 + 125);
			if (unlikely(ret)) {
				__gpufreq_abort("fail to set regulator VGPU (%d)", ret);
				goto done;
			}
			udelay(t_settle);
		}
	/* keep volt */
	} else {
		ret = GPUFREQ_SUCCESS;
	}

	g_gpu.cur_volt = __gpufreq_get_real_vgpu();
	g_gpu.cur_vsram = __gpufreq_get_real_vsram();
	g_gpu.cur_vcore = __gpufreq_get_real_vcore();

#if GPUFREQ_HISTORY_ENABLE
	if (volt_new != volt_old)
		__gpufreq_record_history_entry(HISTORY_CHANGE_VOLT_TOP);
#endif /* GPUFREQ_HISTORY_ENABLE */

	if (unlikely(g_gpu.cur_volt != volt_new))
		__gpufreq_abort("inconsistent scaled Vgpu, cur_volt: %d, target_volt: %d",
			g_gpu.cur_volt, volt_new);

	GPUFREQ_LOGD("Vgpu: %d, Vsram: %d, Vcore: %d",
		g_gpu.cur_volt, g_gpu.cur_vsram, g_gpu.cur_vcore);

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

	/* 0x1000C000 */
	g_apmixed_base = __gpufreq_of_ioremap("mediatek,apmixed", 0);
	if (unlikely(!g_apmixed_base)) {
		GPUFREQ_LOGE("fail to ioremap APMIXED");
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
		GPUFREQ_LOGE("fail to get resource topckgen");
		goto done;
	}
	g_topckgen_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_topckgen_base)) {
		GPUFREQ_LOGE("fail to ioremap topckgen: 0x%llx", res->start);
		goto done;
	}

	/* 0x13000000 */
	g_mali_base = __gpufreq_of_ioremap("mediatek,mali", 0);
	if (unlikely(!g_mali_base)) {
		GPUFREQ_LOGE("fail to ioremap MALI");
		goto done;
	}

	GPUFREQ_LOGI("[MFG0-3] PWR_STATUS: 0x%08x, PWR_STATUS_2ND: 0x%08x",
		readl(XPU_PWR_STATUS) & MFG_0_3_PWR_MASK,
		readl(XPU_PWR_STATUS_2ND) & MFG_0_3_PWR_MASK);
	GPUFREQ_LOGI("[TOP] SEL: 0x%08x, REF_SEL: 0x%08x",
		readl(TOPCK_CLK_CFG_20) & MFG_PLL_SEL_MASK,
		readl(TOPCK_CLK_CFG_13) & MFG_REF_SEL_MASK);
	GPUFREQ_LOGI("[GPU] MALI_GPU_ID: 0x%08x, MFG_TOP_CONFIG: 0x%08x",
		readl(MALI_GPU_ID), readl(MFG_TOP_CFG_BASE));

done:
	return;
}

static unsigned int __gpufreq_get_fmeter_freq(void)
{
#if IS_ENABLED(CONFIG_COMMON_CLK_MT6835)
	return mt_get_abist_freq(FM_MFGPLL_OPP_CK);
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

	mfgpll = readl(MFG_PLL_CON1);

	pcw = mfgpll & (0x3FFFFF);

	posdiv_power = (mfgpll & GENMASK(26, 24)) >> POSDIV_SHIFT;

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

/* API: get real current Vcore from regulator (mV * 100) */
static unsigned int __gpufreq_get_real_vcore(void)
{
	unsigned int volt = 0;

	if (regulator_is_enabled(g_pmic->reg_vcore))
		/* regulator_get_voltage return volt with uV */
		volt = regulator_get_voltage(g_pmic->reg_vcore) / 10;

	return volt;
}

/* HWDCM: mask clock when GPU idle (dynamic clock mask) */
static void __gpufreq_hwdcm_config(void)
{
#if GPUFREQ_HWDCM_ENABLE
	u32 val;

	/* [F] MFG_ASYNC_CON 0x13FB_F020 [22] MEM0_MST_CG_ENABLE = 0x1 */
	/* [J] MFG_ASYNC_CON 0x13FB_F020 [23] MEM0_SLV_CG_ENABLE = 0x1 */
	/* [H] MFG_ASYNC_CON 0x13FB_F020 [24] MEM1_MST_CG_ENABLE = 0x1 */
	/* [L] MFG_ASYNC_CON 0x13FB_F020 [25] MEM1_SLV_CG_ENABLE = 0x1 */
	val = readl(MFG_ASYNC_CON);
	val |= GENMASK(25, 22);
	writel(val, MFG_ASYNC_CON);

	/* [D] MFG_GLOBAL_CON 0x13FB_F0B0 [10] GPU_CLK_FREE_RUN = 0x0 */
	/* [D] MFG_GLOBAL_CON 0x13FB_F0B0 [9] MFG_SOC_OUT_AXI_FREE_RUN = 0x0 */
	val = readl(MFG_GLOBAL_CON);
	val &= ~GENMASK(10, 9);
	writel(val, MFG_GLOBAL_CON);

	/* [D] MFG_QCHANNEL_CON 0x13FB_F0B4 [4] QCHANNEL_ENABLE = 0x1 */
	val = readl(MFG_QCHANNEL_CON);
	val |= BIT(4);
	writel(val, MFG_QCHANNEL_CON);

	/* [E] MFG_GLOBAL_CON 0x13FB_F0B0 [19] PWR_CG_FREE_RUN = 0x1 */
	/* [P] MFG_GLOBAL_CON 0x13FB_F0B0 [8] MFG_SOC_IN_AXI_FREE_RUN = 0x0 */
	val = readl(MFG_GLOBAL_CON);
	val |= ~BIT(19);
	val &= ~BIT(8);
	writel(val, MFG_GLOBAL_CON);

	/*[O] MFG_ASYNC_CON_1 0x13FB_F024 [0] FAXI_CK_SOC_IN_EN_ENABLE = 0x1*/
	val = readl(MFG_ASYNC_CON_1);
	val |= BIT(0);
	writel(val, MFG_ASYNC_CON_1);

	/* [M] MFG_I2M_PROTECTOR_CFG_00 0x13FB_FF60 [0] GPM_ENABLE = 0x1 */
	val = readl(MFG_I2M_PROTECTOR_CFG_00);
	val |= BIT(0);
	writel(val, MFG_I2M_PROTECTOR_CFG_00);
#endif /* GPUFREQ_HWDCM_ENABLE */
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

static int __gpufreq_clock_control(enum gpufreq_power_state power)
{
	int ret = GPUFREQ_SUCCESS;
	int i = 0;

	GPUFREQ_TRACE_START("power=%d", power);

	if (power == POWER_ON) {
		ret = clk_prepare_enable(g_clk->clk_mux);
		if (unlikely(ret)) {
			__gpufreq_abort("fail to enable clk_mux (%d)", ret);
			goto done;
		}

		__gpufreq_switch_clksrc(CLOCK_MAIN);

		if (readl(TOPCK_CLK_CFG_20) & MFG_PLL_SEL_MASK) {
			udelay(10);
		} else {
			GPUFREQ_LOGI("switch clock_main fail,switch again");

			while ((~readl(TOPCK_CLK_CFG_20)) & MFG_PLL_SEL_MASK) {
				__gpufreq_switch_clksrc(CLOCK_MAIN);
				udelay(10);

				if (++i > 5) {
					__gpufreq_abort("switch clock_main time = %d", i);
					goto done;
				}
			}
		}

		ret = clk_prepare_enable(g_clk->subsys_bg3d);
		if (unlikely(ret)) {
			__gpufreq_abort("fail to enable subsys_bg3d (%d)", ret);
			goto done;
		}

		g_gpu.cg_count++;
	} else {
		clk_disable_unprepare(g_clk->subsys_bg3d);

		__gpufreq_switch_clksrc(CLOCK_SUB);

		clk_disable_unprepare(g_clk->clk_mux);

		g_gpu.cg_count--;
	}

done:
	GPUFREQ_TRACE_END();

	return ret;
}

/*
 * when call Linux standard genpd API(ex:pm_runtime_put_sync())
 * to operate mtcmos on/off on kernel-5.10, there maybe occur
 * unexpected behavior, hence mtcmos on/off fail
 * so we change to self control mtcmos by mfgsys driver
 * ex: __gpufreq_mfg0/1/2/3_control
 */
#if GPUFREQ_SELF_CTRL_MTCMOS
static void __gpufreq_mfg0_control(enum gpufreq_power_state power)
{
	int i = 0;

	if (power == POWER_OFF) {
		/* TINFO="Set bus protect" */
		writel(MFG0_PROT_STEP0_0_MASK, MD_MFGSYS_PROTECT_EN_SET_0);
		while ((readl(MD_MFGSYS_PROTECT_RDY_STA_0) & MFG0_PROT_STEP0_0_ACK_MASK) !=
			MFG0_PROT_STEP0_0_ACK_MASK) {
			udelay(10);
			if (++i > 500) {
				__gpufreq_footprint_power_step(0xE0);
				//goto timeout;
			}
		}

		/* TINFO="Set bus protect" */
		writel(MFG0_PROT_STEP1_0_MASK, INFRASYS_PROTECT_EN_SET_0);
		while ((readl(INFRASYS_PROTECT_RDY_STA_0) & MFG0_PROT_STEP1_0_ACK_MASK) !=
			MFG0_PROT_STEP1_0_ACK_MASK) {
			udelay(10);
			if (++i > 500) {
				__gpufreq_footprint_power_step(0xE1);
				//goto timeout;
			}
		}

		/* TINFO="Set PWR_ISO = 1" */
		writel((readl(MFG0_PWR_CON) | PWR_ISO), MFG0_PWR_CON);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		writel((readl(MFG0_PWR_CON) | PWR_CLK_DIS), MFG0_PWR_CON);
		/* TINFO="Set PWR_RST_B = 0" */
		writel((readl(MFG0_PWR_CON) & ~PWR_RST_B), MFG0_PWR_CON);
		/* TINFO="Set PWR_ON = 0" */
		writel((readl(MFG0_PWR_CON) & ~PWR_ON), MFG0_PWR_CON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		writel((readl(MFG0_PWR_CON) & ~PWR_ON_2ND), MFG0_PWR_CON);
		/* TINFO="Wait until XPU_PWR_STATUS = 0 and XPU_PWR_STATUS_2ND = 0" */
		while ((readl(XPU_PWR_STATUS) & MFG0_PWR_STA_MASK) ||
			(readl(XPU_PWR_STATUS_2ND) & MFG0_PWR_STA_MASK)) {
			udelay(10);
			if (++i > 500) {
				__gpufreq_footprint_power_step(0xE2);
				//goto timeout;
			}
		}
	} else {
		/* TINFO="Set PWR_ON = 1" */
		writel((readl(MFG0_PWR_CON) | PWR_ON), MFG0_PWR_CON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		writel((readl(MFG0_PWR_CON) | PWR_ON_2ND), MFG0_PWR_CON);
		/* TINFO="Wait until XPU_PWR_STATUS = 1 and XPU_PWR_STATUS_2ND = 1" */
		while (((readl(XPU_PWR_STATUS) & MFG0_PWR_STA_MASK) != MFG0_PWR_STA_MASK) ||
			((readl(XPU_PWR_STATUS_2ND) & MFG0_PWR_STA_MASK) != MFG0_PWR_STA_MASK)) {
			udelay(10);
			if (++i > 500) {
				__gpufreq_footprint_power_step(0xE3);
				//goto timeout;
			}
		}

		udelay(100);

		/* TINFO="Set PWR_CLK_DIS = 0" */
		writel((readl(MFG0_PWR_CON) & ~PWR_CLK_DIS), MFG0_PWR_CON);
		/* TINFO="Set PWR_ISO = 0" */
		writel((readl(MFG0_PWR_CON) & ~PWR_ISO), MFG0_PWR_CON);
		/* TINFO="Set PWR_RST_B = 1" */
		writel((readl(MFG0_PWR_CON) | PWR_RST_B), MFG0_PWR_CON);

		__gpufreq_footprint_power_step(0xE4);
		/* TINFO="Release bus protect" */
		writel(MFG0_PROT_STEP1_0_MASK, INFRASYS_PROTECT_EN_CLR_0);

		__gpufreq_footprint_power_step(0xE5);
		/* TINFO="Release bus protect" */
		writel(MFG0_PROT_STEP0_0_MASK, MD_MFGSYS_PROTECT_EN_CLR_0);
	}

#if GPUFREQ_HISTORY_ENABLE
	__gpufreq_set_power_mfg(HISTORY_MFG_0, power);
	__gpufreq_power_history_entry();
#endif /* GPUFREQ_HISTORY_ENABLE */
}

static void __gpufreq_mfg1_control(enum gpufreq_power_state power)
{
	int i = 0;

	if (power == POWER_OFF) {
		/* TINFO="Set bus protect" */
		writel(MFG1_PROT_STEP0_0_MASK, MD_MFGSYS_PROTECT_EN_SET_0);
		while ((readl(MD_MFGSYS_PROTECT_RDY_STA_0) & MFG1_PROT_STEP0_0_ACK_MASK) !=
			MFG1_PROT_STEP0_0_ACK_MASK) {
			udelay(10);
			if (++i > 500) {
				__gpufreq_footprint_power_step(0xE6);
				//goto timeout;
			}
		}

		/* TINFO="Set bus protect" */
		writel(MFG1_PROT_STEP1_0_MASK, INFRASYS_PROTECT_EN_SET_1);
		while ((readl(INFRASYS_PROTECT_RDY_STA_1) & MFG1_PROT_STEP1_0_ACK_MASK) !=
			MFG1_PROT_STEP1_0_ACK_MASK) {
			udelay(10);
			if (++i > 500) {
				__gpufreq_footprint_power_step(0xE7);
				//goto timeout;
			}
		}

		/* TINFO="Set bus protect" */
		writel(MFG1_PROT_STEP2_0_MASK, MD_MFGSYS_PROTECT_EN_SET_0);
		while ((readl(MD_MFGSYS_PROTECT_RDY_STA_0) & MFG1_PROT_STEP2_0_ACK_MASK) !=
			MFG1_PROT_STEP2_0_ACK_MASK) {
			udelay(10);
			if (++i > 500) {
				__gpufreq_footprint_power_step(0xE8);
				//goto timeout;
			}
		}

		/* TINFO="Set bus protect" */
		writel(MFG1_PROT_STEP3_0_MASK, EMISYS_PROTECT_EN_SET_0);
		while ((readl(EMISYS_PROTECT_RDY_STA_0) & MFG1_PROT_STEP3_0_ACK_MASK) !=
			MFG1_PROT_STEP3_0_ACK_MASK) {
			udelay(10);
			if (++i > 500) {
				__gpufreq_footprint_power_step(0xE9);
				//goto timeout;
			}
		}

		/* TINFO="Set bus protect" */
		writel(MFG1_PROT_STEP4_0_MASK, MD_MFGSYS_PROTECT_EN_SET_0);
		while ((readl(MD_MFGSYS_PROTECT_RDY_STA_0) & MFG1_PROT_STEP4_0_ACK_MASK) !=
			MFG1_PROT_STEP4_0_ACK_MASK) {
			udelay(10);
			if (++i > 500) {
				__gpufreq_footprint_power_step(0xEA);
				//goto timeout;
			}
		}

		/* TINFO="Set bus protect" */
		writel(MFG1_PROT_STEP5_0_MASK, MD_MFGSYS_PROTECT_EN_SET_0);
		while ((readl(MD_MFGSYS_PROTECT_RDY_STA_0) & MFG1_PROT_STEP5_0_ACK_MASK) !=
			MFG1_PROT_STEP5_0_ACK_MASK) {
			udelay(10);
			if (++i > 500) {
				__gpufreq_footprint_power_step(0xEB);
				//goto timeout;
			}
		}

		/* TINFO="Set SRAM_PDN = 1" */
		writel((readl(MFG1_PWR_CON) | SRAM_PDN), MFG1_PWR_CON);
		/* TINFO="Wait until SRAM_PDN_ACK = 1" */
		while ((readl(MFG1_PWR_CON) & SRAM_PDN_ACK) != SRAM_PDN_ACK) {
			udelay(10);
			if (++i > 500) {
				__gpufreq_footprint_power_step(0xEC);
				//goto timeout;
			}
		}

		/* TINFO="Set PWR_ISO = 1" */
		writel((readl(MFG1_PWR_CON) | PWR_ISO), MFG1_PWR_CON);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		writel((readl(MFG1_PWR_CON) | PWR_CLK_DIS), MFG1_PWR_CON);
		/* TINFO="Set PWR_RST_B = 0" */
		writel((readl(MFG1_PWR_CON) & ~PWR_RST_B), MFG1_PWR_CON);
		/* TINFO="Set PWR_ON = 0" */
		writel((readl(MFG1_PWR_CON) & ~PWR_ON), MFG1_PWR_CON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		writel((readl(MFG1_PWR_CON) & ~PWR_ON_2ND), MFG1_PWR_CON);
		/* TINFO="Wait until XPU_PWR_STATUS = 0 and XPU_PWR_STATUS_2ND = 0" */
		while ((readl(XPU_PWR_STATUS) & MFG1_PWR_STA_MASK) ||
			(readl(XPU_PWR_STATUS_2ND) & MFG1_PWR_STA_MASK)) {
			udelay(10);
			if (++i > 500) {
				__gpufreq_footprint_power_step(0xED);
				//goto timeout;
			}
		}
	} else {
		/* TINFO="Set PWR_ON = 1" */
		writel((readl(MFG1_PWR_CON) | PWR_ON), MFG1_PWR_CON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		writel((readl(MFG1_PWR_CON) | PWR_ON_2ND), MFG1_PWR_CON);
		/* TINFO="Wait until XPU_PWR_STATUS = 1 and XPU_PWR_STATUS_2ND = 1" */
		while (((readl(XPU_PWR_STATUS) & MFG1_PWR_STA_MASK) != MFG1_PWR_STA_MASK) ||
			((readl(XPU_PWR_STATUS_2ND) & MFG1_PWR_STA_MASK) != MFG1_PWR_STA_MASK)) {
			udelay(10);
			if (++i > 500) {
				__gpufreq_footprint_power_step(0xEE);
				//goto timeout;
			}
		}

		udelay(100);

		/* TINFO="Set PWR_CLK_DIS = 0" */
		writel((readl(MFG1_PWR_CON) & ~PWR_CLK_DIS), MFG1_PWR_CON);
		/* TINFO="Set PWR_ISO = 0" */
		writel((readl(MFG1_PWR_CON) & ~PWR_ISO), MFG1_PWR_CON);
		/* TINFO="Set PWR_RST_B = 1" */
		writel((readl(MFG1_PWR_CON) | PWR_RST_B), MFG1_PWR_CON);
		/* TINFO="Set SRAM_PDN = 0" */
		writel((readl(MFG1_PWR_CON) & ~SRAM_PDN), MFG1_PWR_CON);
		/* TINFO="Wait until SRAM_PDN_ACK = 0" */
		while (readl(MFG1_PWR_CON) & SRAM_PDN_ACK) {
			udelay(10);
			if (++i > 500) {
				__gpufreq_footprint_power_step(0xEF);
				//goto timeout;
			}
		}

		__gpufreq_footprint_power_step(0xF0);
		/* TINFO="Release bus protect" */
		writel(MFG1_PROT_STEP5_0_MASK, MD_MFGSYS_PROTECT_EN_CLR_0);

		__gpufreq_footprint_power_step(0xF1);
		/* TINFO="Release bus protect" */
		writel(MFG1_PROT_STEP4_0_MASK, MD_MFGSYS_PROTECT_EN_CLR_0);

		__gpufreq_footprint_power_step(0xF2);
		/* TINFO="Release bus protect" */
		writel(MFG1_PROT_STEP3_0_MASK, EMISYS_PROTECT_EN_CLR_0);

		__gpufreq_footprint_power_step(0xF3);
		/* TINFO="Release bus protect" */
		writel(MFG1_PROT_STEP2_0_MASK, MD_MFGSYS_PROTECT_EN_CLR_0);

		__gpufreq_footprint_power_step(0xF4);
		/* TINFO="Release bus protect" */
		writel(MFG1_PROT_STEP1_0_MASK, INFRASYS_PROTECT_EN_CLR_1);

		__gpufreq_footprint_power_step(0xF5);
		/* TINFO="Release bus protect" */
		writel(MFG1_PROT_STEP0_0_MASK, MD_MFGSYS_PROTECT_EN_CLR_0);
	}

#if GPUFREQ_HISTORY_ENABLE
	__gpufreq_set_power_mfg(HISTORY_MFG_1, power);
	__gpufreq_power_history_entry();
#endif /* GPUFREQ_HISTORY_ENABLE */
}

static void __gpufreq_mfg2_control(enum gpufreq_power_state power)
{
	int i = 0;

	if (power == POWER_OFF) {
		/* TINFO="Set SRAM_PDN = 1" */
		writel((readl(MFG2_PWR_CON) | SRAM_PDN), MFG2_PWR_CON);
		/* TINFO="Wait until SRAM_PDN_ACK = 1" */
		while ((readl(MFG2_PWR_CON) & SRAM_PDN_ACK) != SRAM_PDN_ACK) {
			udelay(10);
			if (++i > 500) {
				__gpufreq_footprint_power_step(0xF6);
				//goto timeout;
			}
		}

		/* TINFO="Set PWR_ISO = 1" */
		writel((readl(MFG2_PWR_CON) | PWR_ISO), MFG2_PWR_CON);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		writel((readl(MFG2_PWR_CON) | PWR_CLK_DIS), MFG2_PWR_CON);
		/* TINFO="Set PWR_RST_B = 0" */
		writel((readl(MFG2_PWR_CON) & ~PWR_RST_B), MFG2_PWR_CON);
		/* TINFO="Set PWR_ON = 0" */
		writel((readl(MFG2_PWR_CON) & ~PWR_ON), MFG2_PWR_CON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		writel((readl(MFG2_PWR_CON) & ~PWR_ON_2ND), MFG2_PWR_CON);
		/* TINFO="Wait until XPU_PWR_STATUS = 0 and XPU_PWR_STATUS_2ND = 0" */
		while ((readl(XPU_PWR_STATUS) & MFG2_PWR_STA_MASK) ||
			(readl(XPU_PWR_STATUS_2ND) & MFG2_PWR_STA_MASK)) {
			udelay(10);
			if (++i > 500) {
				__gpufreq_footprint_power_step(0xF7);
				//goto timeout;
			}
		}
	} else {
		/* TINFO="Set PWR_ON = 1" */
		writel((readl(MFG2_PWR_CON) | PWR_ON), MFG2_PWR_CON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		writel((readl(MFG2_PWR_CON) | PWR_ON_2ND), MFG2_PWR_CON);
		/* TINFO="Wait until XPU_PWR_STATUS = 1 and XPU_PWR_STATUS_2ND = 1" */
		while (((readl(XPU_PWR_STATUS) & MFG2_PWR_STA_MASK) != MFG2_PWR_STA_MASK) ||
			((readl(XPU_PWR_STATUS_2ND) & MFG2_PWR_STA_MASK) != MFG2_PWR_STA_MASK)) {
			udelay(10);
			if (++i > 500) {
				__gpufreq_footprint_power_step(0xF8);
				//goto timeout;
			}
		}

		udelay(100);

		/* TINFO="Set PWR_CLK_DIS = 0" */
		writel((readl(MFG2_PWR_CON) & ~PWR_CLK_DIS), MFG2_PWR_CON);
		/* TINFO="Set PWR_ISO = 0" */
		writel((readl(MFG2_PWR_CON) & ~PWR_ISO), MFG2_PWR_CON);
		/* TINFO="Set PWR_RST_B = 1" */
		writel((readl(MFG2_PWR_CON) | PWR_RST_B), MFG2_PWR_CON);
		/* TINFO="Set SRAM_PDN = 0" */
		writel((readl(MFG2_PWR_CON) & ~SRAM_PDN), MFG2_PWR_CON);
		/* TINFO="Wait until SRAM_PDN_ACK = 0" */
		while (readl(MFG2_PWR_CON) & SRAM_PDN_ACK) {
			udelay(10);
			if (++i > 500) {
				__gpufreq_footprint_power_step(0xF9);
				//goto timeout;
			}
		}
	}

#if GPUFREQ_HISTORY_ENABLE
	__gpufreq_set_power_mfg(HISTORY_MFG_2, power);
	__gpufreq_power_history_entry();
#endif /* GPUFREQ_HISTORY_ENABLE */
}

static void __gpufreq_mfg3_control(enum gpufreq_power_state power)
{
	int i = 0;

	if (power == POWER_OFF) {
		/* TINFO="Set SRAM_PDN = 1" */
		writel((readl(MFG3_PWR_CON) | SRAM_PDN), MFG3_PWR_CON);
		/* TINFO="Wait until SRAM_PDN_ACK = 1" */
		while ((readl(MFG3_PWR_CON) & SRAM_PDN_ACK) != SRAM_PDN_ACK) {
			udelay(10);
			if (++i > 500) {
				__gpufreq_footprint_power_step(0xFA);
				//goto timeout;
			}
		}

		/* TINFO="Set PWR_ISO = 1" */
		writel((readl(MFG3_PWR_CON) | PWR_ISO), MFG3_PWR_CON);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		writel((readl(MFG3_PWR_CON) | PWR_CLK_DIS), MFG3_PWR_CON);
		/* TINFO="Set PWR_RST_B = 0" */
		writel((readl(MFG3_PWR_CON) & ~PWR_RST_B), MFG3_PWR_CON);
		/* TINFO="Set PWR_ON = 0" */
		writel((readl(MFG3_PWR_CON) & ~PWR_ON), MFG3_PWR_CON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		writel((readl(MFG3_PWR_CON) & ~PWR_ON_2ND), MFG3_PWR_CON);
		/* TINFO="Wait until XPU_PWR_STATUS = 0 and XPU_PWR_STATUS_2ND = 0" */
		while ((readl(XPU_PWR_STATUS) & MFG3_PWR_STA_MASK) ||
			(readl(XPU_PWR_STATUS_2ND) & MFG3_PWR_STA_MASK)) {
			udelay(10);
			if (++i > 500) {
				__gpufreq_footprint_power_step(0xFB);
				//goto timeout;
			}
		}

	} else {
		/* TINFO="Set PWR_ON = 1" */
		writel((readl(MFG3_PWR_CON) | PWR_ON), MFG3_PWR_CON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		writel((readl(MFG3_PWR_CON) | PWR_ON_2ND), MFG3_PWR_CON);
		/* TINFO="Wait until XPU_PWR_STATUS = 1 and XPU_PWR_STATUS_2ND = 1" */
		while (((readl(XPU_PWR_STATUS) & MFG3_PWR_STA_MASK) != MFG3_PWR_STA_MASK) ||
			((readl(XPU_PWR_STATUS_2ND) & MFG3_PWR_STA_MASK) != MFG3_PWR_STA_MASK)) {
			udelay(10);
			if (++i > 500) {
				__gpufreq_footprint_power_step(0xFC);
				//goto timeout;
			}
		}

		udelay(100);

		/* TINFO="Set PWR_CLK_DIS = 0" */
		writel((readl(MFG3_PWR_CON) & ~PWR_CLK_DIS), MFG3_PWR_CON);
		/* TINFO="Set PWR_ISO = 0" */
		writel((readl(MFG3_PWR_CON) & ~PWR_ISO), MFG3_PWR_CON);
		/* TINFO="Set PWR_RST_B = 1" */
		writel((readl(MFG3_PWR_CON) | PWR_RST_B), MFG3_PWR_CON);
		/* TINFO="Set SRAM_PDN = 0" */
		writel((readl(MFG3_PWR_CON) & ~SRAM_PDN), MFG3_PWR_CON);
		/* TINFO="Wait until SRAM_PDN_ACK = 0" */
		while (readl(MFG3_PWR_CON) & SRAM_PDN_ACK) {
			udelay(10);
			if (++i > 500) {
				__gpufreq_footprint_power_step(0xFD);
				//goto timeout;
			}
		}
	}

#if GPUFREQ_HISTORY_ENABLE
	__gpufreq_set_power_mfg(HISTORY_MFG_3, power);
	__gpufreq_power_history_entry();
#endif /* GPUFREQ_HISTORY_ENABLE */
}
#endif /* GPUFREQ_SELF_CTRL_MTCMOS */

static int __gpufreq_mtcmos_control(enum gpufreq_power_state power)
{
	int ret = GPUFREQ_SUCCESS;
	u32 val = 0;

	GPUFREQ_TRACE_START("power=%d", power);

	if (power == POWER_ON) {
#if GPUFREQ_SELF_CTRL_MTCMOS
		__gpufreq_mfg0_control(POWER_ON);
		__gpufreq_mfg1_control(POWER_ON);
		ret = clk_prepare_enable(g_clk->clk_ref_mux);
		if (unlikely(ret)) {
			__gpufreq_abort("fail to enable clk_ref_mux (%d)", ret);
			goto done;
		}
		__gpufreq_mfg2_control(POWER_ON);
		__gpufreq_mfg3_control(POWER_ON);
#else
		ret = pm_runtime_get_sync(g_mtcmos->mfg0_dev);
		if (unlikely(ret < 0)) {
			__gpufreq_abort("fail to enable mfg0_dev (%d)", ret);
			goto done;
		}

#if GPUFREQ_HISTORY_ENABLE
		__gpufreq_set_power_mfg(HISTORY_MFG_0, power);
#endif /* GPUFREQ_HISTORY_ENABLE */

		ret = pm_runtime_get_sync(g_mtcmos->mfg1_dev);
		if (unlikely(ret < 0)) {
			__gpufreq_abort("fail to enable mfg1_dev (%d)", ret);
			goto done;
		}

#if GPUFREQ_HISTORY_ENABLE
		__gpufreq_set_power_mfg(HISTORY_MFG_1, power);
#endif /* GPUFREQ_HISTORY_ENABLE */

		if (g_shader_present & MFG2_SHADER_STACK0) {
			ret = pm_runtime_get_sync(g_mtcmos->mfg2_dev);
			if (unlikely(ret < 0)) {
				__gpufreq_abort("fail to enable mfg2_dev (%d)", ret);
				goto done;
			}

#if GPUFREQ_HISTORY_ENABLE
			__gpufreq_set_power_mfg(HISTORY_MFG_2, power);
#endif /* GPUFREQ_HISTORY_ENABLE */
		}
		if (g_shader_present & MFG3_SHADER_STACK2) {
			ret = pm_runtime_get_sync(g_mtcmos->mfg3_dev);
			if (unlikely(ret < 0)) {
				__gpufreq_abort("fail to enable mfg3_dev (%d)", ret);
				goto done;
			}

#if GPUFREQ_HISTORY_ENABLE
			__gpufreq_set_power_mfg(HISTORY_MFG_3, power);
#endif /* GPUFREQ_HISTORY_ENABLE */
		}
#endif /* GPUFREQ_SELF_CTRL_MTCMOS */
#if GPUFREQ_CHECK_MTCMOS_PWR_STATUS
		/* SPM contorl, check MFG0-3 power status */
		val = readl(XPU_PWR_STATUS) & MFG_0_3_PWR_MASK;
		if (unlikely(val != MFG_0_3_PWR_MASK)) {
			__gpufreq_abort("incorrect MFG0-3 power on status: 0x%08x", val);
			ret = GPUFREQ_EINVAL;
			goto done;
		}
#endif /* GPUFREQ_CHECK_MTCMOS_PWR_STATUS */

		g_gpu.mtcmos_count++;
	} else {
#if GPUFREQ_SELF_CTRL_MTCMOS
		__gpufreq_mfg3_control(POWER_OFF);
		__gpufreq_mfg2_control(POWER_OFF);
		clk_disable_unprepare(g_clk->clk_ref_mux);
		__gpufreq_mfg1_control(POWER_OFF);
		__gpufreq_mfg0_control(POWER_OFF);
#else
		if (g_shader_present & MFG3_SHADER_STACK2) {
			ret = pm_runtime_put_sync(g_mtcmos->mfg3_dev);
			if (unlikely(ret < 0)) {
				__gpufreq_abort("fail to disable mfg3_dev (%d)", ret);
				goto done;
			}
		}

#if GPUFREQ_HISTORY_ENABLE
			__gpufreq_set_power_mfg(HISTORY_MFG_3, power);
#endif /* GPUFREQ_HISTORY_ENABLE */

		if (g_shader_present & MFG2_SHADER_STACK0) {
			ret = pm_runtime_put_sync(g_mtcmos->mfg2_dev);
			if (unlikely(ret < 0)) {
				__gpufreq_abort("fail to disable mfg2_dev (%d)", ret);
				goto done;
			}

#if GPUFREQ_HISTORY_ENABLE
			__gpufreq_set_power_mfg(HISTORY_MFG_2, power);
#endif /* GPUFREQ_HISTORY_ENABLE */
		}
		ret = pm_runtime_put_sync(g_mtcmos->mfg1_dev);
		if (unlikely(ret < 0)) {
			__gpufreq_abort("fail to disable mfg1_dev (%d)", ret);
			goto done;
		}

#if GPUFREQ_HISTORY_ENABLE
		__gpufreq_set_power_mfg(HISTORY_MFG_1, power);
#endif /* GPUFREQ_HISTORY_ENABLE */

		ret = pm_runtime_put_sync(g_mtcmos->mfg0_dev);
		if (unlikely(ret < 0)) {
			__gpufreq_abort("fail to disable mfg0_dev (%d)", ret);
			goto done;
		}

#if GPUFREQ_HISTORY_ENABLE
		__gpufreq_set_power_mfg(HISTORY_MFG_0, power);
#endif /* GPUFREQ_HISTORY_ENABLE */
#endif /* GPUFREQ_SELF_CTRL_MTCMOS */
#if GPUFREQ_CHECK_MTCMOS_PWR_STATUS
		/* check MFG0-3 power status */
		val = readl(XPU_PWR_STATUS) & MFG_0_3_PWR_MASK;
		if (unlikely(val))
			/* only print error if pwr is incorrect when mtcmos off */
			GPUFREQ_LOGE("incorrect MFG power off status: 0x%08x", val);
#endif /* GPUFREQ_CHECK_MTCMOS_PWR_STATUS */

		g_gpu.mtcmos_count--;
	}

done:
	GPUFREQ_TRACE_END();

#if GPUFREQ_HISTORY_ENABLE
	__gpufreq_power_history_entry();
#endif /* GPUFREQ_HISTORY_ENABLE */

	return ret;
}

static int __gpufreq_buck_control(enum gpufreq_power_state power)
{
	int ret = GPUFREQ_SUCCESS;
	unsigned int target_vcore = 0, target_vsram = 0, t_settle = 0;

	GPUFREQ_TRACE_START("power=%d", power);

	/* power on */
	if (power == POWER_ON) {
#if GPUFREQ_VCORE_DVFS_ENABLE
		/* vote target vcore to DVFSRC as GPU power on */
		target_vcore = __gpufreq_get_vcore_by_vsram(g_gpu.cur_vsram);
		ret = regulator_set_voltage(g_pmic->reg_dvfsrc, target_vcore * 10, INT_MAX);
		if (unlikely(ret)) {
			__gpufreq_abort("fail to enable VCORE (%d)", ret);
			goto done;
		}

#if GPUFREQ_HISTORY_ENABLE
		__gpufreq_set_power_buck(HISTORY_BUCK_VCORE, power);
#endif /* GPUFREQ_HISTORY_ENABLE */
#endif /* GPUFREQ_VCORE_DVFS_ENABLE */
		/* set target vsram to cur_vsram as GPU power on */
		target_vsram = __gpufreq_get_vsram_by_vgpu(g_gpu.cur_volt);
		t_settle = __gpufreq_settle_time_vsram(true, target_vsram - VSRAM_LEVEL_0);
		ret = regulator_set_voltage(g_pmic->reg_vsram,
			target_vsram * 10, VSRAM_MAX_VOLT * 10 + 125);
		if (unlikely(ret)) {
			__gpufreq_abort("fail to enable VSRAM (%d)", ret);
			goto done;
		}
		udelay(t_settle);

#if GPUFREQ_HISTORY_ENABLE
		__gpufreq_set_power_buck(HISTORY_BUCK_VSRAM, power);
#endif /* GPUFREQ_HISTORY_ENABLE */

		t_settle = __gpufreq_settle_time_vgpu(true, g_gpu.cur_volt - VGPU_LEVEL_0);
		ret = regulator_set_mode(g_pmic->reg_vgpu, REGULATOR_MODE_NORMAL);
		if (unlikely(ret)) {
			__gpufreq_abort("fail to set VGPU normal mode(%d)", ret);
			goto done;
		}
		udelay(t_settle);

#if GPUFREQ_HISTORY_ENABLE
		__gpufreq_set_power_buck(HISTORY_BUCK_VTOP, power);
#endif /* GPUFREQ_HISTORY_ENABLE */

		g_gpu.buck_count++;
	/* power off */
	} else {
		ret = regulator_set_mode(g_pmic->reg_vgpu, REGULATOR_MODE_IDLE);
		if (unlikely(ret)) {
			__gpufreq_abort("fail to set VGPU idle mode(%d)", ret);
			goto done;
		}

#if GPUFREQ_HISTORY_ENABLE
		__gpufreq_set_power_buck(HISTORY_BUCK_VTOP, power);
#endif /* GPUFREQ_HISTORY_ENABLE */

		/* set target vsram to cur_vsram as GPU power on */
		t_settle = __gpufreq_settle_time_vsram(false, g_gpu.cur_vsram - VSRAM_LEVEL_0);
		ret = regulator_set_voltage(g_pmic->reg_vsram,
			VSRAM_LEVEL_0 * 10, VSRAM_MAX_VOLT * 10 + 125);
		if (unlikely(ret)) {
			__gpufreq_abort("fail to disable VSRAM (%d)", ret);
			goto done;
		}
		udelay(t_settle);

#if GPUFREQ_HISTORY_ENABLE
		__gpufreq_set_power_buck(HISTORY_BUCK_VSRAM, power);
#endif /* GPUFREQ_HISTORY_ENABLE */

#if GPUFREQ_VCORE_DVFS_ENABLE
		/* vote VCORE_LEVEL_0 to DVFSRC as GPU power on */
		ret = regulator_set_voltage(g_pmic->reg_dvfsrc, VCORE_LEVEL_0 * 10, INT_MAX);
		if (unlikely(ret)) {
			__gpufreq_abort("fail to disable VCORE (%d)", ret);
			goto done;
		}

#if GPUFREQ_HISTORY_ENABLE
		__gpufreq_set_power_buck(HISTORY_BUCK_VCORE, power);
#endif /* GPUFREQ_HISTORY_ENABLE */
#endif /* GPUFREQ_VCORE_DVFS_ENABLE */
		g_gpu.buck_count--;
	}

done:
	GPUFREQ_TRACE_END();

#if GPUFREQ_HISTORY_ENABLE
	__gpufreq_power_history_entry();
#endif /* GPUFREQ_HISTORY_ENABLE */

	return ret;
}

static void __gpufreq_check_bus_idle(void)
{
	/* MFG_QCHANNEL_CON 0x13FBF0B4 [0] MFG_ACTIVE_SEL = 1'b1 */
	writel((readl(MFG_QCHANNEL_CON) | BIT(0)), MFG_QCHANNEL_CON);

	/* MFG_DEBUG_SEL 0x13FBF170 [1:0] MFG_DEBUG_TOP_SEL = 2'b11 */
	writel((readl(MFG_DEBUG_SEL) | GENMASK(1, 0)), MFG_DEBUG_SEL);

	/*
	 * polling MFG_DEBUG_TOP 0x13FBF178 [2] = 1'b1
	 * 0x0: bus busy
	 * 0x1: bus idle
	 */
	do {} while ((readl(MFG_DEBUG_TOP) & 0x4) != 0x4);
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
	if (__gpufreq_custom_init_enable())
		target_oppidx = GPUFREQ_CUST_INIT_OPPIDX;
	/* decide first OPP idx by preloader setting */
	else
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

/*
 * API: interpolate OPP from signoff idx.
 * step = (large - small) / range
 * vnew = large - step * j
 */
static void __gpufreq_interpolate_volt(void)
{
	unsigned int large_volt = 0, small_volt = 0;
	unsigned int large_freq = 0, small_freq = 0;
	unsigned int inner_volt = 0, inner_freq = 0;
	unsigned int previous_volt = 0;
	int adj_num = 0, i = 0, j = 0;
	int front_idx = 0, rear_idx = 0, inner_idx = 0;
	int range = 0, slope = 0;
	struct gpufreq_opp_info *signed_table = g_gpu.signed_table;

	adj_num = NUM_GPU_SIGNED_IDX;

	mutex_lock(&gpufreq_lock);

	for (i = 1; i < adj_num; i++) {
		front_idx = g_gpu_signed_idx[i - 1];
		rear_idx = g_gpu_signed_idx[i];
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
				__gpufreq_abort(
					"invalid GPU[%02d*] Volt: %d < [%02d*] Volt: %d",
					inner_idx, inner_volt, inner_idx + 1, previous_volt);

			/* record margin */
			signed_table[inner_idx].margin += signed_table[inner_idx].volt - inner_volt;
			/* update to signed table */
			signed_table[inner_idx].volt = inner_volt;
			signed_table[inner_idx].vsram = __gpufreq_get_vsram_by_vgpu(inner_volt);

			GPUFREQ_LOGD("GPU[%02d*] Freq: %d, Volt: %d, vsram: %d,",
				inner_idx, inner_freq * 1000, inner_volt,
				signed_table[inner_idx].vsram);
		}
		GPUFREQ_LOGD("GPU[%02d*] Freq: %d, Volt: %d",
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

static void __gpufreq_compute_aging(void)
{
	struct gpufreq_adj_info *aging_adj = NULL;
	int adj_num = 0;
	int i;
	unsigned int aging_table_idx = GPUFREQ_AGING_MAX_TABLE_IDX;

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

	if (g_aging_load)
		aging_table_idx = GPUFREQ_AGING_MOST_AGRRESIVE;

	if (aging_table_idx > GPUFREQ_AGING_MAX_TABLE_IDX)
		aging_table_idx = GPUFREQ_AGING_MAX_TABLE_IDX;

	/* Aging margin is set if any OPP is adjusted by Aging */
	if (aging_table_idx == 0)
		g_aging_margin = true;

	g_aging_table_idx = aging_table_idx;
}

static void __gpufreq_compute_avs(void)
{
#if GPUFREQ_AVS_ENABLE
	u32 val = 0;
	unsigned int temp_volt = 0, temp_freq = 0, volt_offset = 0;
	int i = 0, oppidx = 0;
	int adj_num = NUM_GPU_SIGNED_IDX;

	/*
	 * Freq (MHz) | Signedoff Volt (V) | Efuse name | Efuse address
	 * ============================================================
	 * 1100       | 0.85               | PTPOD20    | 0x11C105D0
	 * 700        | 0.7                | PTPOD21    | 0x11C105D4
	 * 390        | 0.625              | PTPOD22    | 0x11C105D8
	 */

	/* read AVS efuse and compute Freq and Volt */
	for (i = 0; i < adj_num; i++) {
		oppidx = g_avs_table[i].oppidx;

		if (g_mcl50_load) {
			if (i == 0)
				val = 0x001C4C7A;
			else if (i == 1)
				val = 0x001ABC60;
			else if (i == 2)
				val = 0x0019865C;
		} else if (g_avs_enable) {
			val = readl(EFUSE_BASE + 0x5D0 + (i * 0x4));
		} else
			val = 0;

		/* if efuse value is not set */
		if (!val)
			continue;

		/* compute Freq from efuse */
		temp_freq = 0;
		temp_freq |= (val & 0x0007FF00) >> 8; /* Get freq[10:0] from efuse[18:8] */
		/* Freq is stored in efuse with MHz unit */
		temp_freq *= 1000;
		/* verify with signoff Freq */
		if (temp_freq != g_gpu.signed_table[oppidx].freq) {
			__gpufreq_abort("OPP[%02d*]: AVS efuse[%d].freq(%d) != signed-off.freq(%d)",
				oppidx, i, temp_freq, g_gpu.signed_table[oppidx].freq);
			return;
		}
		g_avs_table[i].freq = temp_freq;

		/* compute Volt from efuse */
		temp_volt = 0;
		temp_volt |= (val & 0x000000FF) >> 0;  /* Get volt[7:0] from efuse[7:0] */
		/* Volt is stored in efuse with 6.25mV unit */
		temp_volt *= 625;
		g_avs_table[i].volt = temp_volt;

		/* AVS margin is set if any OPP is adjusted by AVS */
		g_avs_margin = true;

		/* PTPOD20 [31:24] GPU_PTP_version */
		if (i == 0)
			g_ptp_version = (val & GENMASK(31, 24)) >> 24;
	}

	/* check AVS Volt and update Vsram */
	for (i = adj_num - 1; i >= 0; i--) {
		oppidx = g_avs_table[i].oppidx;
		/* mV * 100 */
		if (i == 0)
			volt_offset = 1250;
		else if (i == 1)
			volt_offset = 1250;

		/* if AVS Volt is not set */
		if (!g_avs_table[i].volt)
			continue;

		/*
		 * AVS Volt reverse check, start from adj_num -2
		 * Volt of sign-off[i] should always be larger than sign-off[i+1]
		 * if not, add Volt offset to sign-off[i]
		 */
		if (i != (adj_num - 1)) {
			if (g_avs_table[i].volt < g_avs_table[i+1].volt) {
				GPUFREQ_LOGW("AVS efuse[%d].volt(%d) < efuse[%d].volt(%d)",
					i, g_avs_table[i].volt, i+1, g_avs_table[i+1].volt);
				g_avs_table[i].volt = g_avs_table[i+1].volt + volt_offset;
			}
		}

		/* clamp to signoff Volt */
		if (g_avs_table[i].volt > g_gpu.signed_table[oppidx].volt) {
			GPUFREQ_LOGW("OPP[%02d*]: AVS efuse[%d].volt(%d) > signed-off.volt(%d)",
				oppidx, i, g_avs_table[i].volt, g_gpu.signed_table[oppidx].volt);
			g_avs_table[i].volt = g_gpu.signed_table[oppidx].volt;
		}

		/* update Vsram */
		g_avs_table[i].vsram = __gpufreq_get_vsram_by_vgpu(g_avs_table[i].volt);
	}

	for (i = 0; i < adj_num; i++)
		GPUFREQ_LOGI("OPP[%02d*]: AVS efuse[%d] freq(%d), volt(%d)",
			g_avs_table[i].oppidx, i, g_avs_table[i].freq, g_avs_table[i].volt);
#endif /* GPUFREQ_AVS_ENABLE */
}

static void __gpufreq_apply_adjustment(void)
{
	struct gpufreq_opp_info *signed_table = NULL;
	int adj_num = 0, oppidx = 0, i = 0;
	unsigned int avs_volt = 0, avs_vsram = 0, aging_volt = 0;

	signed_table = g_gpu.signed_table;
	adj_num = NUM_GPU_SIGNED_IDX;

	/* apply AVS margin */
	if (g_avs_margin) {
		for (i = 0; i < adj_num; i++) {
			oppidx = g_avs_table[i].oppidx;
			avs_volt = g_avs_table[i].volt ?
				g_avs_table[i].volt : signed_table[oppidx].volt;
			avs_vsram = g_avs_table[i].vsram ?
				g_avs_table[i].vsram : signed_table[oppidx].vsram;
			/* record margin */
			signed_table[oppidx].margin += signed_table[oppidx].volt - avs_volt;
			/* update to signed table */
			signed_table[oppidx].volt = avs_volt;
			signed_table[oppidx].vsram = avs_vsram;
		}
	} else
		GPUFREQ_LOGI("AVS margin is not set");

	/* apply Aging margin */
	if (g_aging_margin) {
		for (i = 0; i < adj_num; i++) {
			oppidx = g_gpu_aging_table[g_aging_table_idx][i].oppidx;
			aging_volt = g_gpu_aging_table[g_aging_table_idx][i].volt;
			/* record margin */
			signed_table[oppidx].margin += aging_volt;
			/* update to signed table */
			signed_table[oppidx].volt -= aging_volt;
			signed_table[oppidx].vsram =
				__gpufreq_get_vsram_by_vgpu(signed_table[oppidx].volt);
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
		g_shader_present = GPU_SHADER_PRESENT_2;
	}
	GPUFREQ_LOGD("segment_id: %d, shader_present: %d", segment_id, g_shader_present);
}

static void __gpufreq_init_acp(void)
{
	unsigned int val;

	/* disable acp */
	val = readl(IFNFRA_AO_BASE + 0x290) | (0x1 << 9);
	writel(val, IFNFRA_AO_BASE + 0x290);
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
	g_gpu.cur_vcore = __gpufreq_get_real_vcore();

	GPUFREQ_LOGI("preloader init [GPU] Freq: %d, Volt: %d, Vsram: %d, Vcore: %d",
		g_gpu.cur_freq, g_gpu.cur_volt, g_gpu.cur_vsram, g_gpu.cur_vcore);

	/* init GPU OPP table */
	/* init OPP segment range */
	segment_id = g_gpu.segment_id;
	/* Next-C+(23E+) GPU FREQ: 962MHz */
	if (segment_id == MT6835M_SEGMENT)
		g_gpu.segment_upbound = 10;
	/* Next-C++(23E++) GPU FREQ: 1100MHz */
	else if (segment_id == MT6835T_SEGMENT)
		g_gpu.segment_upbound = 0;
	/* Next-C(23E) GPU FREQ: 570MHz */
	else if (segment_id == MT6835_SEGMENT)
		g_gpu.segment_upbound = 37;
	else
		g_gpu.segment_upbound = 10;
	g_gpu.segment_lowbound = NUM_GPU_SIGNED_OPP - 1;
	g_gpu.signed_opp_num = NUM_GPU_SIGNED_OPP;
	g_gpu.max_oppidx = 0;
	g_gpu.min_oppidx = g_gpu.segment_lowbound - g_gpu.segment_upbound;
	g_gpu.opp_num = g_gpu.min_oppidx + 1;
	g_gpu.signed_table = g_gpu_default_opp_table;

	g_gpu.working_table = kcalloc(g_gpu.opp_num, sizeof(struct gpufreq_opp_info), GFP_KERNEL);
	if (!g_gpu.working_table) {
		__gpufreq_abort("fail to alloc gpufreq_opp_info (ENOMEM)");
		return;
	}

	GPUFREQ_LOGD("num of signed GPU OPP: %d, segment bound: [%d, %d]",
		g_gpu.signed_opp_num, g_gpu.segment_upbound, g_gpu.segment_lowbound);
	GPUFREQ_LOGI("num of working GPU OPP: %d", g_gpu.opp_num);

	/* update signed OPP table from MFGSYS features */

	/* compute Aging table based on Aging Sensor */
	__gpufreq_compute_aging();
	/* compute AVS table based on EFUSE */
	__gpufreq_compute_avs();
	/* apply Segment/Aging/AVS/... adjustment to signed OPP table */
	__gpufreq_apply_adjustment();

	/* after these, GPU signed table is settled down */

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

static int __gpufreq_init_segment_id(struct platform_device *pdev)
{
	unsigned int efuse_id = 0x0;
	unsigned int segment_id = 0;
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
		segment_id = MT6835_SEGMENT;
		break;
	case 0x2:
		segment_id = MT6835M_SEGMENT;
		break;
	case 0x3:
		segment_id = MT6835T_SEGMENT;
		break;
	default:
		segment_id = ENG_SEGMENT;
		break;
	}

done:
	GPUFREQ_LOGI("efuse_id: 0x%x, segment_id: %d", efuse_id, segment_id);

	g_gpu.segment_id = segment_id;

	return ret;
}

static int __gpufreq_init_mtcmos(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int ret = GPUFREQ_SUCCESS;

#if !GPUFREQ_SELF_CTRL_MTCMOS
	GPUFREQ_TRACE_START("pdev=0x%x", pdev);

	g_mtcmos = kzalloc(sizeof(struct gpufreq_mtcmos_info), GFP_KERNEL);
	if (!g_mtcmos) {
		GPUFREQ_LOGE("fail to alloc gpufreq_mtcmos_info (ENOMEM)");
		ret = GPUFREQ_ENOMEM;
		goto done;
	}

	g_mtcmos->mfg0_dev = dev_pm_domain_attach_by_name(dev, "pd_mfg0");
	if (IS_ERR_OR_NULL(g_mtcmos->mfg0_dev)) {
		ret = g_mtcmos->mfg0_dev ? PTR_ERR(g_mtcmos->mfg0_dev) : GPUFREQ_ENODEV;
		__gpufreq_abort("fail to get mfg0_dev (%ld)", ret);
		goto done;
	}
	dev_pm_syscore_device(g_mtcmos->mfg0_dev, true);

	g_mtcmos->mfg1_dev = dev_pm_domain_attach_by_name(dev, "pd_mfg1");
	if (IS_ERR_OR_NULL(g_mtcmos->mfg1_dev)) {
		ret = g_mtcmos->mfg1_dev ? PTR_ERR(g_mtcmos->mfg1_dev) : GPUFREQ_ENODEV;
		__gpufreq_abort("fail to get mfg1_dev (%ld)", ret);
		goto done;
	}
	dev_pm_syscore_device(g_mtcmos->mfg1_dev, true);


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
		GPUFREQ_LOGE("fail to alloc gpufreq_clk_info (ENOMEM)");
		ret = GPUFREQ_ENOMEM;
		goto done;
	}

	g_clk->clk_mux = devm_clk_get(&pdev->dev, "clk_mux");
	if (IS_ERR(g_clk->clk_mux)) {
		ret = PTR_ERR(g_clk->clk_mux);
		__gpufreq_abort("fail to get clk_mux (%ld)", ret);
		goto done;
	}

	g_clk->clk_ref_mux = devm_clk_get(&pdev->dev, "clk_ref_mux");
	if (IS_ERR(g_clk->clk_ref_mux)) {
		ret = PTR_ERR(g_clk->clk_ref_mux);
		__gpufreq_abort("fail to get clk_ref_mux (%ld)", ret);
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

	g_clk->subsys_bg3d = devm_clk_get(&pdev->dev, "subsys_bg3d");
	if (IS_ERR(g_clk->subsys_bg3d)) {
		ret = PTR_ERR(g_clk->subsys_bg3d);
		__gpufreq_abort("fail to get subsys_bg3d (%ld)", ret);
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

	g_pmic->reg_vcore = regulator_get_optional(&pdev->dev, "_vcore");
	if (IS_ERR(g_pmic->reg_vcore)) {
		ret = PTR_ERR(g_pmic->reg_vcore);
		__gpufreq_abort("fail to get VCORE (%ld)", ret);
		goto done;
	}

	g_pmic->reg_vgpu = regulator_get_optional(&pdev->dev, "_vgpu");
	if (IS_ERR(g_pmic->reg_vgpu)) {
		ret = PTR_ERR(g_pmic->reg_vgpu);
		__gpufreq_abort("fail to get VGPU (%ld)", ret);
		goto done;
	}

	g_pmic->reg_vsram = regulator_get_optional(&pdev->dev, "_vsram");
	if (IS_ERR(g_pmic->reg_vsram)) {
		ret = PTR_ERR(g_pmic->reg_vsram);
		__gpufreq_abort("fail to get VSRAM (%ld)", ret);
		goto done;
	}

#if GPUFREQ_VCORE_DVFS_ENABLE
	g_pmic->reg_dvfsrc = regulator_get_optional(&pdev->dev, "_dvfsrc");
	if (IS_ERR(g_pmic->reg_dvfsrc)) {
		ret = PTR_ERR(g_pmic->reg_dvfsrc);
		__gpufreq_abort("fail to get DVFSRC (%ld)", ret);
		goto done;
	}
#endif /* GPUFREQ_VCORE_DVFS_ENABLE */

	ret = regulator_enable(g_pmic->reg_vgpu);
	if (unlikely(ret)) {
		__gpufreq_abort("fail to enable VGPU (%d)", ret);
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

	/* ignore return error and use default value if property doesn't exist */
	of_property_read_u32(gpufreq_dev->of_node, "aging-load", &g_aging_load);
	of_property_read_u32(gpufreq_dev->of_node, "mcl50-load", &g_mcl50_load);
	of_property_read_u32(gpufreq_dev->of_node, "mt6377-support", &g_mt6377_support);
	of_property_read_u32(of_wrapper, "gpueb-support", &g_gpueb_support);

	/* 0x1000C000 */
	g_apmixed_base = __gpufreq_of_ioremap("mediatek,apmixed", 0);
	if (unlikely(!g_apmixed_base)) {
		GPUFREQ_LOGE("fail to ioremap APMIXED");
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
		GPUFREQ_LOGE("fail to get resource topckgen");
		goto done;
	}
	g_topckgen_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_topckgen_base)) {
		GPUFREQ_LOGE("fail to ioremap topckgen: 0x%llx", res->start);
		goto done;
	}

	/* 0x1021C000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "nth_emicfg");
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
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "sth_emicfg");
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

	/* 0x1030E000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "sth_emicfg_ao_mem");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource STH_EMICFG_AO_MEM_REG");
		goto done;
	}
	g_sth_emicfg_ao_mem_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_sth_emicfg_ao_mem_base)) {
		GPUFREQ_LOGE("fail to ioremap STH_EMICFG_AO_MEM_REG: 0x%llx", res->start);
		goto done;
	}

	/* 0x1020E000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "infracfg");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource INFRACFG_AO");
		goto done;
	}
	g_infracfg_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_infracfg_base)) {
		GPUFREQ_LOGE("fail to ioremap INFRACFG: 0x%llx", res->start);
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

#if GPUFREQ_AVS_ENABLE || GPUFREQ_ASENSOR_ENABLE
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
#endif /* GPUFREQ_AVS_ENABLE || GPUFREQ_ASENSOR_ENABLE */

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
		GPUFREQ_LOGI("gpufreq platform probe only init reg/dfd/pmic/fp in EB mode");
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
	__gpufreq_init_segment_id(pdev);
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to init segment id (%d)", ret);
		goto done;
	}

	/* init shader present */
	__gpufreq_init_shader_present();

#if GPUFREQ_HISTORY_ENABLE
	__gpufreq_history_memory_init();
	__gpufreq_power_history_memory_init();
	__gpufreq_power_history_init_entry();
#endif /* GPUFREQ_HISTORY_ENABLE */

	/* power on to init first OPP index */
	ret = __gpufreq_power_control(POWER_ON);
	if (unlikely(ret < 0)) {
		GPUFREQ_LOGE("fail to control power state: %d (%d)", POWER_ON, ret);
		goto done;
	}

	/* init ACP */
	__gpufreq_init_acp();

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

#if GPUFREQ_HISTORY_ENABLE
	__gpufreq_history_memory_reset();
	__gpufreq_record_history_entry(HISTORY_FREE);
#endif /* GPUFREQ_HISTORY_ENABLE */

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
	dev_pm_domain_detach(g_mtcmos->mfg3_dev, true);
	dev_pm_domain_detach(g_mtcmos->mfg2_dev, true);
	dev_pm_domain_detach(g_mtcmos->mfg1_dev, true);
	dev_pm_domain_detach(g_mtcmos->mfg0_dev, true);
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

#if GPUFREQ_HISTORY_ENABLE
	__gpufreq_history_memory_uninit();
	__gpufreq_power_history_memory_uninit();
#endif /* GPUFREQ_HISTORY_ENABLE */
}

module_init(__gpufreq_init);
module_exit(__gpufreq_exit);

MODULE_DEVICE_TABLE(of, g_gpufreq_of_match);
MODULE_DESCRIPTION("MediaTek GPU-DVFS platform driver");
MODULE_LICENSE("GPL");
