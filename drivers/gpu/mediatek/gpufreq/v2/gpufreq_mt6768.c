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
#include <gpufreq_debug.h>
#include <gpuppm.h>
#include <gpufreq_common.h>
#include <gpufreq_mt6768.h>
#include <mtk_gpu_utility.h>

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
#if IS_ENABLED(CONFIG_MTK_STATIC_POWER)
#include <leakage_table_v2/mtk_static_power.h>
#endif
#if IS_ENABLED(CONFIG_COMMON_CLK_MTK_FREQ_HOPPING)
#include <clk-mtk.h>
#endif

#if IS_ENABLED(CONFIG_COMMON_CLK_MT6768)
#include <clk-fmeter.h>
#include <clk-mt6768-fmeter.h>
#endif

#if IS_ENABLED(CONFIG_MTK_DEVINFO)
#include <linux/nvmem-consumer.h>
#endif

#ifdef MT_GPUFREQ_STATIC_PWR_READY2USE
#include "mtk_static_power.h"
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
static void __gpufreq_set_springboard(void);
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
static unsigned int __gpufreq_settle_time_vgpu(unsigned int mode, int deltaV);
static unsigned int __gpufreq_settle_time_vsram(unsigned int mode, int deltaV);
/* get function */
static unsigned int __gpufreq_get_fmeter_fgpu(void);
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
/*low power*/
static void __gpufreq_kick_pbm(int enable);
static unsigned int __gpufreq_get_ptpod_opp_idx(unsigned int idx);
static void __gpufreq_update_gpu_working_table(void);
/*external function*/
extern void kicker_pbm_by_gpu(bool status, unsigned int loading, int voltage);

//thermal
static void __mt_update_gpufreqs_power_table(void);

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

static unsigned int g_vgpu_sfchg_rrate;
static unsigned int g_vgpu_sfchg_frate;
static unsigned int g_vsram_sfchg_rrate;
static unsigned int g_vsram_sfchg_frate;

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
static bool g_thermal_protect_limited_ignore_state;
static int (*mt_gpufreq_wrap_fp)(void);

static enum gpufreq_dvfs_state g_dvfs_state;
static DEFINE_MUTEX(gpufreq_lock);
static DEFINE_MUTEX(ptpod_lock);

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
	.set_timestamp = __gpufreq_set_timestamp,
	.check_bus_idle = __gpufreq_check_bus_idle,
	.dump_infra_status = __gpufreq_dump_infra_status,
	.set_stress_test = __gpufreq_set_stress_test,
	.set_aging_mode = __gpufreq_set_aging_mode,
	.get_core_num = __gpufreq_get_core_num,
	.update_power_table = __mt_update_gpufreqs_power_table,
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

/* API: get opp idx in original opp tables. */
/* This is usually for ptpod use. */
unsigned int __gpufreq_get_ptpod_opp_idx(unsigned int idx)
{
	if (idx < PTPOD_OPP_GPU_NUM)
		return g_ptpod_opp_idx_table[idx];
	else
		return idx;

}

/*
 * set AUTO_MODE or PWM_MODE to PMIC(VGPU)
 * REGULATOR_MODE_FAST: PWM Mode
 * REGULATOR_MODE_NORMAL: Auto Mode
 */
static void __mt_gpufreq_vgpu_set_mode(unsigned int mode)
{
	int ret;

	ret = regulator_set_mode(g_pmic->reg_vgpu, mode);
	if (ret == 0) {
		GPUFREQ_LOGD("set AUTO_MODE(%d) or PWM_MODE(%d) to PMIC(VGPU), mode = %d\n",
				REGULATOR_MODE_NORMAL, REGULATOR_MODE_FAST, mode);
	} else {
		GPUFREQ_LOGE("failed to configure mode, ret = %d, mode = %d\n", ret, mode);
	}
}

/*
 * API : enable DVFS for PTPOD initializing
 */
void mt_gpufreq_enable_by_ptpod(void)
{
	mutex_lock(&ptpod_lock);
	/* Set GPU Buck to leave PWM mode */
	__mt_gpufreq_vgpu_set_mode(REGULATOR_MODE_NORMAL);

	__gpufreq_resume_dvfs();

	__gpufreq_power_control(POWER_OFF);

#if defined(CONFIG_ARM64) && \
	defined(CONFIG_BUILD_ARM64_DTB_OVERLAY_IMAGE_NAMES)
	GPUFREQ_LOGI("flavor name: %s\n",
				CONFIG_BUILD_ARM64_DTB_OVERLAY_IMAGE_NAMES);
	if ((strstr(CONFIG_BUILD_ARM64_DTB_OVERLAY_IMAGE_NAMES,
		"k68v1_64_aging") != NULL)) {
		GPUFREQ_LOGI("AGING flavor !!!\n");
		g_aging_enable = 1;
	}
#endif

	GPUFREQ_LOGD("DVFS is enabled by ptpod\n");
	mutex_unlock(&ptpod_lock);
}
EXPORT_SYMBOL(mt_gpufreq_enable_by_ptpod);

/*
 * API : disable DVFS for PTPOD initializing
 */
void mt_gpufreq_disable_by_ptpod(void)
{
	int i = 0;
	int target_idx = g_gpu.max_oppidx;
	int ret = GPUFREQ_SUCCESS;

	mutex_lock(&ptpod_lock);

	ret = __gpufreq_power_control(POWER_ON);
	if (unlikely(ret < 0)) {
		GPUFREQ_LOGE("fail to control power state: %d (%d)", POWER_ON, ret);
		return;
	}

	/* Fix GPU @ 0.8V */
	for (i = 0; i < g_gpu.opp_num; i++) {
		if (g_gpu.working_table[i].volt <= GPU_DVFS_PTPOD_DISABLE_VOLT) {
			target_idx = i;
			break;
		}
	}
	__gpufreq_pause_dvfs(target_idx);

	/* Set GPU Buck to enter PWM mode */
	__mt_gpufreq_vgpu_set_mode(REGULATOR_MODE_FAST);

	GPUFREQ_LOGD("DVFS is disabled by ptpod, g_DVFS_off_by_ptpod_idx: %d\n",
		target_idx);
	mutex_unlock(&ptpod_lock);
}
EXPORT_SYMBOL(mt_gpufreq_disable_by_ptpod);

static void __mt_gpufreq_volt_switch_without_vsram_volt(unsigned int volt_old, unsigned int volt_new)
{
	unsigned int vsram_volt_new, vsram_volt_old;
	int ret = GPUFREQ_SUCCESS;

	volt_new = VOLT_NORMALIZATION(volt_new);

	GPUFREQ_LOGD("volt_new = %d, volt_old = %d\n", volt_new, volt_old);

	vsram_volt_new = __gpufreq_get_vsram_by_vgpu(volt_new);
	vsram_volt_old = __gpufreq_get_vsram_by_vgpu(volt_old);

		/* voltage scaling */
	ret = __gpufreq_volt_scale_gpu(
		volt_old, volt_new, vsram_volt_old, vsram_volt_new);
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to scale Vgpu: (%d->%d), Vsram_gpu: (%d->%d)",
			volt_old, volt_new, vsram_volt_old, vsram_volt_new);
	} else {
		g_gpu.cur_volt = volt_new;
		g_gpu.cur_vsram = vsram_volt_new;
	}
}

/*
 * API : update OPP and switch back to default voltage setting
 */
void mt_gpufreq_restore_default_volt(void)
{
	int i;
	struct gpufreq_opp_info *signed_table = g_gpu.signed_table;

	mutex_lock(&ptpod_lock);

	GPUFREQ_LOGD("restore OPP table to default voltage\n");

	for (i = 0; i < g_gpu.signed_opp_num; i++) {
		signed_table[i].volt = g_default_gpu[i].volt;
		signed_table[i].vsram = g_default_gpu[i].vsram;

		GPUFREQ_LOGD("signed_table[%d].volt = %d, vsram = %d\n",
				i,
				signed_table[i].volt,
				signed_table[i].vsram);
	}

	__gpufreq_update_gpu_working_table();

	if (g_aging_enable)
		__gpufreq_apply_aging(true);

	__gpufreq_set_springboard();

	__mt_gpufreq_volt_switch_without_vsram_volt(g_gpu.cur_volt,
		g_gpu.working_table[g_gpu.cur_oppidx].volt);

	mutex_unlock(&ptpod_lock);
}
EXPORT_SYMBOL(mt_gpufreq_restore_default_volt);

/*
 * API : get current voltage
 */
unsigned int mt_gpufreq_get_cur_volt(void)
{
	return (__gpufreq_get_power_state() == POWER_ON) ? g_gpu.cur_volt : 0;
}
EXPORT_SYMBOL(mt_gpufreq_get_cur_volt);

/* API : get frequency via OPP table index */
unsigned int mt_gpufreq_get_freq_by_idx(unsigned int idx)
{
	return __gpufreq_get_fgpu_by_idx(idx);
}
EXPORT_SYMBOL(mt_gpufreq_get_freq_by_idx);

/* API: get opp idx in original opp tables */
/* This is usually for ptpod use */
unsigned int mt_gpufreq_get_ori_opp_idx(unsigned int idx)
{

	unsigned int ptpod_opp_idx_num;

	ptpod_opp_idx_num = ARRAY_SIZE(g_ptpod_opp_idx_table);

	if (idx < ptpod_opp_idx_num && idx >= 0)
		return g_ptpod_opp_idx_table[idx];
	else
		return idx;

}
EXPORT_SYMBOL(mt_gpufreq_get_ori_opp_idx);

/* API : get voltage via OPP table real index */
unsigned int mt_gpufreq_get_volt_by_real_idx(unsigned int idx)
{
	if (idx < g_gpu.signed_opp_num)
		return g_gpu.signed_table[idx].volt;
	else
		return 0;
}
EXPORT_SYMBOL(mt_gpufreq_get_volt_by_real_idx);

/* API : get frequency via OPP table real index */
unsigned int mt_gpufreq_get_freq_by_real_idx(unsigned int idx)
{
	if (idx < g_gpu.signed_opp_num)
		return g_gpu.signed_table[idx].freq;
	else
		return 0;
}
EXPORT_SYMBOL(mt_gpufreq_get_freq_by_real_idx);
/*
 * API : update OPP and set voltage because PTPOD modified voltage table by PMIC wrapper
 */
unsigned int mt_gpufreq_update_volt(unsigned int pmic_volt[], unsigned int array_size)
{
	int i;
	int target_idx;
	struct gpufreq_opp_info *signed_table = g_gpu.signed_table;

	mutex_lock(&ptpod_lock);

	GPUFREQ_LOGD("update OPP table to given voltage\n");

	for (i = 0; i < array_size; i++) {
		target_idx = __gpufreq_get_ptpod_opp_idx(i);
		signed_table[target_idx].volt = pmic_volt[i];
		signed_table[target_idx].vsram =
		__gpufreq_get_vsram_by_vgpu(pmic_volt[i]);
		if (i < array_size - 1) {
			/* interpolation for opps not for ptpod */
			int larger = pmic_volt[i];
			int smaller = pmic_volt[i + 1];
			int interpolation;

			if (target_idx == 20) {
				/* After opp 20, 2 opps need intepolation */
				interpolation =	((larger << 1) + smaller) / 3;
				signed_table[target_idx + 1].volt
					= VOLT_NORMALIZATION(interpolation);
				signed_table[target_idx + 1].vsram
				= __gpufreq_get_vsram_by_vgpu
				(signed_table[target_idx + 1].volt);

				interpolation =	(larger + (smaller << 1)) / 3;
				signed_table[target_idx + 2].volt
					= VOLT_NORMALIZATION(interpolation);
				signed_table[target_idx + 2].vsram
				= __gpufreq_get_vsram_by_vgpu
				(signed_table[target_idx + 2].volt);
			} else {
				interpolation =	(larger + smaller) >> 1;
				signed_table[target_idx + 1].volt
					= VOLT_NORMALIZATION(interpolation);
				signed_table[target_idx + 1].vsram
					= __gpufreq_get_vsram_by_vgpu
				(signed_table[target_idx + 1].volt);
				}
			}
	}
	__gpufreq_update_gpu_working_table();

	if (g_aging_enable)
		__gpufreq_apply_aging(true);

	__gpufreq_set_springboard();

	mutex_unlock(&ptpod_lock);

	return 0;
}
EXPORT_SYMBOL(mt_gpufreq_update_volt);



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
static void __mt_gpufreq_calculate_power(unsigned int idx, unsigned int freq,
			unsigned int volt, unsigned int temp)
{
	unsigned int p_total = 0;
	unsigned int p_dynamic = 0;
	unsigned int ref_freq = 0;
	unsigned int ref_volt = 0;
	int p_leakage = 0;

	p_dynamic = __gpufreq_get_dyn_pgpu(freq, volt);

#ifdef MT_GPUFREQ_STATIC_PWR_READY2USE
	p_leakage = mt_spower_get_leakage(MTK_SPOWER_GPU, (volt / 100), temp);
	if (p_leakage < 0)
		p_leakage = 0;
#else
	p_leakage = 71;
#endif /* ifdef MT_GPUFREQ_STATIC_PWR_READY2USE */

	p_total = p_dynamic + p_leakage;

	GPUFREQ_LOGD("idx = %d, p_dynamic = %d, p_leakage = %d, p_total = %d, temp = %d\n",
			idx, p_dynamic, p_leakage, p_total, temp);

	g_power_table[idx].gpufreq_power = p_total;

}

/*
 * OPP power table initialization
 */
static void __mt_gpufreq_setup_opp_power_table(int num)
{
	int i = 0;
	int temp = 0;

	g_power_table = kzalloc((num) * sizeof(struct mt_gpufreq_power_table_info), GFP_KERNEL);

	if (g_power_table == NULL)
		return;

#ifdef CONFIG_THERMAL
	if (mt_gpufreq_wrap_fp)
		temp = mt_gpufreq_wrap_fp() / 1000;
	else
		temp = 40;
#else
	temp = 40;
#endif /* ifdef CONFIG_THERMAL */

	GPUFREQ_LOGD("temp = %d\n", temp);

	if ((temp < -20) || (temp > 125)) {
		GPUFREQ_LOGD("temp < -20 or temp > 125!\n");
		temp = 65;
	}

	for (i = 0; i < num; i++) {
		g_power_table[i].gpufreq_khz = g_gpu.signed_table[i].freq;
		g_power_table[i].gpufreq_volt = g_gpu.signed_table[i].volt;

		__mt_gpufreq_calculate_power(i, g_power_table[i].gpufreq_khz,
				g_power_table[i].gpufreq_volt, temp);

		GPUFREQ_LOGD("[%d], freq_khz = %u, volt = %u, power = %u\n",
				i, g_power_table[i].gpufreq_khz,
				g_power_table[i].gpufreq_volt,
				g_power_table[i].gpufreq_power);
	}
}


/* update OPP power table */
static void __mt_update_gpufreqs_power_table(void)
{
	int i;
	int temp = 0;
	unsigned int freq = 0;
	unsigned int volt = 0;

#ifdef CONFIG_THERMAL
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

			__mt_gpufreq_calculate_power(i, freq, volt, temp);

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
	unsigned int vsram;

	if (vgpu > VSRAM_FIXED_THRESHOLD)
		vsram = vgpu + VSRAM_FIXED_DIFF;
	else
		vsram = VSRAM_FIXED_VOLT;

	return vsram;
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
#if PBM_RAEDY
	unsigned int power;
	unsigned int cur_freq;
	unsigned int cur_volt;
	unsigned int found = 0;
	int tmp_idx = -1;
	int i;
	struct gpufreq_opp_info *working_table = g_gpu.working_table;

	cur_freq = g_gpu.cur_freq;
	cur_volt = g_gpu.cur_volt;

	if (enable) {
		for (i = 0; i < g_gpu.opp_num; i++) {
			if (working_table[i].freq == cur_freq) {
				/* record idx since current voltage may not in DVFS table */
				tmp_idx = i;

				if (working_table[i].volt == cur_volt) {
					power = working_table[i].power;
					found = 1;
					kicker_pbm_by_gpu(true, power, cur_volt / 100);
					GPUFREQ_LOGD("request GPU power = %d,
						cur_volt = %d uV,
						cur_freq = %d KHz\n",
						power, cur_volt * 10, cur_freq);
					return;
				}
			}
		}

		if (!found) {
			GPUFREQ_LOGD("tmp_idx = %d\n", tmp_idx);
			if (tmp_idx != -1 && tmp_idx < g_gpu.opp_num) {
				/* use freq to found corresponding power budget */
				power = working_table[tmp_idx].power;
				kicker_pbm_by_gpu(true, power, cur_volt / 100);
				GPUFREQ_LOGD("request GPU power = %d, cur_volt = %d uV,
					cur_freq = %d KHz\n", power, cur_volt * 10, cur_freq);
			} else {
				GPUFREQ_LOGD("Cannot found request power in power table,
					cur_freq = %d KHz, cur_volt = %d uV\n",
					cur_freq, cur_volt * 10);
			}
		}
	} else {
		kicker_pbm_by_gpu(false, 0, cur_volt / 100);
	}
#endif
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
		__gpufreq_kick_pbm(1);
	} else if (power == POWER_OFF && g_gpu.power_count == 0) {
		__gpufreq_footprint_power_step(GPUFREQ_POWER_STEP_05);

		/* freeze DVFS when power off */
		g_dvfs_state |= DVFS_POWEROFF;
		__gpufreq_footprint_power_step(GPUFREQ_POWER_STEP_06);

		/* control clock */
		ret = __gpufreq_clock_control(POWER_OFF);
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
	struct gpufreq_sb_info *sb_table = g_gpu.sb_table;
	int opp_num = g_gpu.opp_num;
	int cur_oppidx = 0;
	unsigned int cur_freq = 0, target_freq = 0;
	unsigned int cur_volt = 0, target_volt = 0;
	unsigned int cur_vsram = 0, target_vsram = 0;
	int sb_idx = 0;
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
		while (target_volt != cur_volt) {
			sb_idx = target_oppidx > sb_table[cur_oppidx].up ?
				target_oppidx : sb_table[cur_oppidx].up;

			ret = __gpufreq_volt_scale_gpu(
				cur_volt, opp_table[sb_idx].volt,
				cur_vsram, opp_table[sb_idx].vsram);
			if (unlikely(ret)) {
				GPUFREQ_LOGE("fail to scale Vgpu: (%d->%d), Vsram_gpu: (%d->%d)",
					cur_volt, opp_table[sb_idx].volt,
					cur_vsram, opp_table[sb_idx].vsram);
				goto done_unlock;
			}

			cur_oppidx = sb_idx;
			cur_volt = opp_table[sb_idx].volt;
			cur_vsram = opp_table[sb_idx].vsram;
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
			sb_idx = target_oppidx < sb_table[cur_oppidx].down ?
				target_oppidx : sb_table[cur_oppidx].down;

			ret = __gpufreq_volt_scale_gpu(
				cur_volt, opp_table[sb_idx].volt,
				cur_vsram, opp_table[sb_idx].vsram);
			if (unlikely(ret)) {
				GPUFREQ_LOGE("fail to scale Vgpu: (%d->%d), Vsram_gpu: (%d->%d)",
					cur_volt, opp_table[sb_idx].volt,
					cur_vsram, opp_table[sb_idx].vsram);
				goto done_unlock;
			}

			cur_oppidx = sb_idx;
			cur_volt = opp_table[sb_idx].volt;
			cur_vsram = opp_table[sb_idx].vsram;
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
	GPUFREQ_LOGD("0x130000b4 val = 0x%x\n", readl(g_MFG_base + 0xb4));

	/* set register MFG_DEBUG_SEL (0x13000170) bit [7:0] = 0x03 */
	writel(0x00000003, g_MFG_base + 0x170);
	GPUFREQ_LOGD("0x13000170 val = 0x%x\n", readl(g_MFG_base + 0x170));

	/* polling register MFG_DEBUG_TOP (0x13000178) bit 2 = 0x1 */
	/* => 1 for GPU (BUS) idle, 0 for GPU (BUS) non-idle */
	/* do not care about 0x13000174 */
	do {
		val = readl(g_MFG_base + 0x178);
		GPUFREQ_LOGD("0x13000178 val = 0x%x\n", val);
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

/* API: get max number of shader cores */
unsigned int __gpufreq_get_core_num(void)
{
	return SHADER_CORE_NUM;
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

	mfgpll = readl(MFGPLL_CON1);

	posdiv = (mfgpll & (0x7 << POSDIV_SHIFT)) >> POSDIV_SHIFT;

	return posdiv;
}

static enum gpufreq_posdiv __gpufreq_get_posdiv_by_fgpu(unsigned int freq)
{
	/* [MT6768]
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
	unsigned int parking = false;
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("freq_old=%d, freq_new=%d", freq_old, freq_new);

	GPUFREQ_LOGD("begin to scale Fgpu: (%d->%d)", freq_old, freq_new);

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

#if (GPUFREQ_FHCTL_ENABLE && IS_ENABLED(CONFIG_COMMON_CLK_MTK_FREQ_HOPPING))
	if (target_posdiv != cur_posdiv)
		parking = true;
	else
		parking = false;

#else
	/* force parking if FHCTL isn't ready */
	parking = true;
	GPUFREQ_LOGD("Fgpu: %d, PCW: 0x%x, CON1: 0x%08x", g_gpu.cur_freq, pcw, pll);
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
#if (GPUFREQ_FHCTL_ENABLE && IS_ENABLED(CONFIG_COMMON_CLK_MTK_FREQ_HOPPING))
		if (unlikely(!mtk_fh_set_rate)) {
			__gpufreq_abort(GPUFREQ_FHCTL_EXCEPTION, "null hopping fp");
			ret = GPUFREQ_ENOENT;
			goto done;
		}

		ret = mtk_fh_set_rate(MFG_PLL_NAME, pcw, target_posdiv);
		if (unlikely(!ret)) {
			__gpufreq_abort(GPUFREQ_FHCTL_EXCEPTION,
				"fail to hopping pcw: 0x%x (%d)", pcw, ret);
			goto done;
		}
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

/* API: scale vgpu and vsram via PMIC */
static int __gpufreq_volt_scale_gpu(
	unsigned int vgpu_old, unsigned int vgpu_new,
	unsigned int vsram_old, unsigned int vsram_new)
{
	unsigned int t_settle_vgpu = 0;
	unsigned int t_settle_vsram = 0;
	unsigned int t_settle = 0;
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("vgpu_old=%d, vgpu_new=%d, vsram_old=%d, vsram_new=%d",
		vgpu_old, vgpu_new, vsram_old, vsram_new);

	GPUFREQ_LOGD("begin to scale Vgpu: (%d->%d), Vsram_gpu: (%d->%d)",
		vgpu_old, vgpu_new, vsram_old, vsram_new);

	/* volt scaling up */
	if (vgpu_new > vgpu_old) {
		/* scale-up volt */
		t_settle_vgpu =
			__gpufreq_settle_time_vgpu(
				true, (vgpu_new - vgpu_old));
		t_settle_vsram =
			__gpufreq_settle_time_vsram(
				true, (vsram_new - vsram_old));

		ret = regulator_set_voltage(
				g_pmic->reg_vsram_gpu,
				vsram_new * 10,
				VSRAM_MAX_VOLT * 10 + 125);
		if (unlikely(ret)) {
			__gpufreq_abort(GPUFREQ_PMIC_EXCEPTION, "fail to set VSRAM_G (%d)", ret);
			goto done;
		}

		ret = regulator_set_voltage(
				g_pmic->reg_vgpu,
				vgpu_new * 10,
				VGPU_MAX_VOLT * 10 + 125);
		if (unlikely(ret)) {
			__gpufreq_abort(GPUFREQ_PMIC_EXCEPTION, "fail to set VGPU (%d)", ret);
			goto done;
		}
	} else if (vgpu_new < vgpu_old) {
		/* scale-down volt */
		t_settle_vgpu =
			__gpufreq_settle_time_vgpu(
				false, (vgpu_old - vgpu_new));
		t_settle_vsram =
			__gpufreq_settle_time_vsram(
				false, (vsram_old - vsram_new));

		ret = regulator_set_voltage(
				g_pmic->reg_vgpu,
				vgpu_new * 10,
				VGPU_MAX_VOLT * 10 + 125);
		if (unlikely(ret)) {
			__gpufreq_abort(GPUFREQ_PMIC_EXCEPTION, "fail to set VGPU (%d)", ret);
			goto done;
		}

		ret = regulator_set_voltage(
				g_pmic->reg_vsram_gpu,
				vsram_new * 10,
				VSRAM_MAX_VOLT * 10 + 125);
		if (unlikely(ret)) {
			__gpufreq_abort(GPUFREQ_PMIC_EXCEPTION, "fail to set VSRAM_GPU (%d)", ret);
			goto done;
		}
	} else {
		/* keep volt */
		ret = GPUFREQ_SUCCESS;
	}

	t_settle = (t_settle_vgpu > t_settle_vsram) ?
		t_settle_vgpu : t_settle_vsram;
	udelay(t_settle);

	g_gpu.cur_volt = __gpufreq_get_real_vgpu();
	if (unlikely(g_gpu.cur_volt != vgpu_new))
		__gpufreq_abort(GPUFREQ_GPU_EXCEPTION,
			"inconsistent scaled Vgpu, cur_volt: %d, target_volt: %d",
			g_gpu.cur_volt, vgpu_new);

	g_gpu.cur_vsram = __gpufreq_get_real_vsram();
	if (unlikely(g_gpu.cur_vsram != vsram_new))
		__gpufreq_abort(GPUFREQ_GPU_EXCEPTION,
			"inconsistent scaled Vsram, cur_vsram: %d, target_vsram: %d",
			g_gpu.cur_vsram, vsram_new);

	/* todo: GED log buffer (gpufreq_pr_logbuf) */

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

GPUFREQ_LOGI("[TOP] FMETER: %d, CON1: %d",
		__gpufreq_get_fmeter_fgpu(), __gpufreq_get_real_fgpu());

}

static unsigned int __gpufreq_get_fmeter_fgpu(void)
{
#if IS_ENABLED(CONFIG_COMMON_CLK_MT6768)
	return mt_get_abist_freq(AD_MFGPLL_CK);
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

	GPUFREQ_LOGD("MFGPLL_CON1 = 0x%x", MFGPLL_CON1);

	mfgpll = readl(MFGPLL_CON1);

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

	if (regulator_is_enabled(g_pmic->reg_vsram_gpu))
		/* regulator_get_voltage return volt with uV */
		volt = regulator_get_voltage(g_pmic->reg_vsram_gpu) / 10;

	return volt;
}

static void __gpufreq_external_cg_control(void)
{
	u32 val;

	/* MFG_GLOBAL_CON: 0x1300_00b0 bit [8] = 0x0 */
	/* MFG_GLOBAL_CON: 0x1300_00b0 bit [10] = 0x0 */
	GPUFREQ_LOGD("0x1300_00b0 = 0x%x\n", readl(g_MFG_base + 0xb0));
	val = readl(g_MFG_base + 0xb0);
	val &= ~(1UL << 8);
	val &= ~(1UL << 10);
	writel(val, g_MFG_base + 0xb0);
	GPUFREQ_LOGD("0x1300_00b0 = 0x%x\n", readl(g_MFG_base + 0xb0));

	/* MFG_ASYNC_CON: 0x1300_0020 bit [25:22] = 0xF */
	GPUFREQ_LOGD("0x1300_0020 = 0x%x\n", readl(g_MFG_base + 0x20));
	writel(readl(g_MFG_base + 0x20) | (0xF << 22), g_MFG_base + 0x20);
	GPUFREQ_LOGD("0x1300_0020 = 0x%x\n", readl(g_MFG_base + 0x20));

	/* MFG_ASYNC_CON_1: 0x1300_0024 bit [0] = 0x1 */
	GPUFREQ_LOGD("0x1300_0024 = 0x%x\n", readl(g_MFG_base + 0x24));
	writel(readl(g_MFG_base + 0x24) | (1UL), g_MFG_base + 0x24);
	GPUFREQ_LOGD("0x1300_0024 = 0x%x\n", readl(g_MFG_base + 0x24));
}

static int __gpufreq_clock_control(enum gpufreq_power_state power)
{
	int ret = GPUFREQ_SUCCESS;

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
		ret = clk_prepare_enable(g_clk->mtcmos_mfg_async);
		if (ret) {
			__gpufreq_abort(GPUFREQ_CCF_EXCEPTION,
				"fail to enable mtcmos_mfg_async (%d)", ret);
			goto done;
		}

		ret = clk_prepare_enable(g_clk->mtcmos_mfg);
		if (ret) {
			__gpufreq_abort(GPUFREQ_CCF_EXCEPTION,
				"fail to enable mtcmos_mfg (%d)", ret);
			goto done;
		}

		ret = clk_prepare_enable(g_clk->mtcmos_mfg_core0);
		if (ret) {
			__gpufreq_abort(GPUFREQ_CCF_EXCEPTION,
				"fail to enable mtcmos_mfg_core0 (%d)", ret);
			goto done;
		}

		ret = clk_prepare_enable(g_clk->mtcmos_mfg_core1);
		if (ret) {
			__gpufreq_abort(GPUFREQ_CCF_EXCEPTION,
				"fail to enable mtcmos_mfg_core1 (%d)", ret);
			goto done;
		}

#ifdef MT_GPUFREQ_SRAM_DEBUG
		aee_rr_rec_gpu_dvfs_status(0x80 | (aee_rr_curr_gpu_dvfs_status() & 0x0F));
#endif
		GPUFREQ_LOGD("enable MTCMOS done\n");

#if GPUFREQ_CHECK_MTCMOS_PWR_STATUS
	//TODO:GKI check mtcmos power
#endif /* GPUFREQ_CHECK_MTCMOS_PWR_STATUS */
		g_gpu.mtcmos_count++;
	} else {

#ifdef MT_GPUFREQ_SRAM_DEBUG
		aee_rr_rec_gpu_dvfs_status(0x90 | (aee_rr_curr_gpu_dvfs_status() & 0x0F));
#endif

		clk_disable_unprepare(g_clk->mtcmos_mfg_core1);
		clk_disable_unprepare(g_clk->mtcmos_mfg_core0);
		clk_disable_unprepare(g_clk->mtcmos_mfg);
		clk_disable_unprepare(g_clk->mtcmos_mfg_async);

#ifdef MT_GPUFREQ_SRAM_DEBUG
		aee_rr_rec_gpu_dvfs_status(0xA0 | (aee_rr_curr_gpu_dvfs_status() & 0x0F));
#endif
		GPUFREQ_LOGD("disable MTCMOS done\n");
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

	GPUFREQ_TRACE_START("power=%d", power);

	/* power on */
	if (power == POWER_ON) {
		ret = regulator_enable(g_pmic->reg_vgpu);
		if (unlikely(ret)) {
			__gpufreq_abort(GPUFREQ_PMIC_EXCEPTION, "fail to enable VGPU (%d)", ret);
			goto done;
		}
		ret = regulator_enable(g_pmic->reg_vsram_gpu);
		if (unlikely(ret)) {
			__gpufreq_abort(GPUFREQ_PMIC_EXCEPTION, "fail to enable VSRAM_GPU (%d)",
			ret);
			goto done;
		}
		g_gpu.buck_count++;
	/* power off */
	} else {
		ret = regulator_disable(g_pmic->reg_vsram_gpu);
		if (unlikely(ret)) {
			__gpufreq_abort(GPUFREQ_PMIC_EXCEPTION, "fail to disable VSRAM_GPU (%d)",
			ret);
			goto done;
		}
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

/*
 * API: find the longest and valid opp idx can be reached
 * use springboard opp index to avoid buck variation,
 * as the diff between Vgpu and Vsram must be in valid range
 * that is, MIN_BUCK_DIFF <= Vsram - Vgpu <= MAX_BUCK_DIFF
 * and Vsram always >= Vgpu, so we only focus on "MAX_BUCK_DIFF"
 */
static void __gpufreq_set_springboard(void)
{
	struct gpufreq_opp_info *opp_table = g_gpu.working_table;
	int src_idx = 0, dst_idx = 0;
	unsigned int src_vgpu = 0, src_vsram = 0;
	unsigned int dst_vgpu = 0, dst_vsram = 0;

	/* build volt scale-up springboad table */
	/* when volt scale-up: Vsram -> Vgpu */
	for (src_idx = 0; src_idx < g_gpu.opp_num; src_idx++) {
		src_vgpu = opp_table[src_idx].volt;
		/* search from the beginning of opp table */
		for (dst_idx = 0; dst_idx < g_gpu.opp_num; dst_idx++) {
			dst_vsram = opp_table[dst_idx].vsram;
			/* the smallest valid opp idx can be reached */
			if (dst_vsram - src_vgpu <= MAX_BUCK_DIFF) {
				g_gpu.sb_table[src_idx].up = dst_idx;
				break;
			}
		}
		GPUFREQ_LOGD("springboard_up[%02d]: %d",
			src_idx, g_gpu.sb_table[src_idx].up);
	}

	/* build volt scale-down springboad table */
	/* when volt scale-down: Vgpu -> Vsram */
	for (src_idx = 0; src_idx < g_gpu.opp_num; src_idx++) {
		src_vsram = opp_table[src_idx].vsram;
		/* search from the end of opp table */
		for (dst_idx = g_gpu.min_oppidx; dst_idx >= 0 ; dst_idx--) {
			dst_vgpu = opp_table[dst_idx].volt;
			/* the largest valid opp idx can be reached */
			if (src_vsram - dst_vgpu <= MAX_BUCK_DIFF) {
				g_gpu.sb_table[src_idx].down = dst_idx;
				break;
			}
		}
		GPUFREQ_LOGD("springboard_down[%02d]: %d",
			src_idx, g_gpu.sb_table[src_idx].down);
	}
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
		ret =  __gpufreq_fix_freq_volt_gpu(
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

	__gpufreq_set_springboard();

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
}

static void __gpufreq_custom_adjustment(void)
{

}

static void __gpufreq_segment_adjustment(struct platform_device *pdev)
{

}

static void __gpufreq_init_shader_present(void)
{
	unsigned int segment_id = 0;

	segment_id = g_gpu.segment_id;

	switch (segment_id) {
	case MT6768_SEGMENT:
		g_shader_present = GPU_SHADER_PRESENT_2;
		break;
	default:
		g_shader_present = GPU_SHADER_PRESENT_2;
		GPUFREQ_LOGI("invalid segment id: %d", segment_id);
	}
	GPUFREQ_LOGD("segment_id: %d, shader_present: %d", segment_id, g_shader_present);
}

static void __gpufreq_update_gpu_working_table(void)
{
	int i = 0, j = 0;

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

}

/*
 * 1. init working OPP range
 * 2. init working OPP table = default + adjustment
 * 3. init springboard table
 */
static int __gpufreq_init_opp_table(struct platform_device *pdev)
{
	unsigned int segment_id = 0;
	int i = 0, j = 0;
	int ret = GPUFREQ_SUCCESS;

	/* init working OPP range */
	segment_id = g_gpu.segment_id;

	/* setup segment max/min opp_idx */
	if (segment_id == MT6767_SEGMENT)
		g_gpu.segment_upbound = 15;
	else if (segment_id == MT6769T_SEGMENT)
		g_gpu.segment_upbound = 2;
	else if (segment_id == MT6769Z_SEGMENT)
		g_gpu.segment_upbound = 0;
	else
		g_gpu.segment_upbound = 7;

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

	__gpufreq_set_springboard();
	__mt_gpufreq_setup_opp_power_table(g_gpu.signed_opp_num);

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
		GPUFREQ_LOGE("cannot get efuse_segment_cell\n");
		PTR_ERR(efuse_cell);
	}

	efuse_buf = (unsigned int *)nvmem_cell_read(efuse_cell, &efuse_len);
	nvmem_cell_put(efuse_cell);
	if (IS_ERR(efuse_buf)) {
		GPUFREQ_LOGE("cannot get efuse_buf of efuse segment\n");
	    PTR_ERR(efuse_buf);
	}

	efuse_id = (*efuse_buf & 0xFF);
	kfree(efuse_buf);

#else
	efuse_id = 0x0;
#endif /* CONFIG_MTK_DEVINFO */

	if (efuse_id == 0xC0
		|| efuse_id == 0x03
		|| efuse_id == 0x20
		|| efuse_id == 0x04) {
		segment_id = MT6767_SEGMENT;
	} else if (efuse_id == 0x80
	 || efuse_id == 0x01
	 || efuse_id == 0x40
	 || efuse_id == 0x02) {
		segment_id = MT6768_SEGMENT;
	} else if (efuse_id == 0xE0
	 || efuse_id == 0x07
	 || efuse_id == 0x10
	 || efuse_id == 0x08) {
		segment_id = MT6769_SEGMENT;
	} else if (efuse_id == 0x90
	 || efuse_id == 0x09
	 || efuse_id == 0x50
	 || efuse_id == 0x0A) {
		segment_id = MT6769T_SEGMENT;
	} else if (efuse_id == 0xA0
	 || efuse_id == 0x05
	 || efuse_id == 0x60
	 || efuse_id == 0x06) {
		segment_id = MT6769Z_SEGMENT;
	} else
		segment_id = MT6768_SEGMENT;
	GPUFREQ_LOGD("efuse_id = 0x%08X, segment_id = %d\n", efuse_id, segment_id);
	g_gpu.segment_id = segment_id;
	return 0;
}

static int __gpufreq_init_mtcmos(struct platform_device *pdev)
{
	return GPUFREQ_SUCCESS;
}

static int __gpufreq_init_clk(struct platform_device *pdev)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("pdev=0x%x", pdev);

	g_clk = kzalloc(sizeof(struct g_clk_info), GFP_KERNEL);
	if (g_clk == NULL) {
		GPUFREQ_LOGE("cannot allocate g_clk\n");
		return -ENOMEM;
	}

	g_clk->clk_mux = devm_clk_get(&pdev->dev, "clk_mux");
	if (IS_ERR(g_clk->clk_mux)) {
		GPUFREQ_LOGE("cannot get clk_mux\n");
		return PTR_ERR(g_clk->clk_mux);
	}

	g_clk->clk_main_parent = devm_clk_get(&pdev->dev, "clk_main_parent");
	if (IS_ERR(g_clk->clk_main_parent)) {
		GPUFREQ_LOGE("cannot get clk_main_parent\n");
		return PTR_ERR(g_clk->clk_main_parent);
	}

	g_clk->clk_sub_parent = devm_clk_get(&pdev->dev, "clk_sub_parent");
	if (IS_ERR(g_clk->clk_sub_parent)) {
		GPUFREQ_LOGE("cannot get clk_sub_parent\n");
		return PTR_ERR(g_clk->clk_sub_parent);
	}

	g_clk->subsys_mfg_cg = devm_clk_get(&pdev->dev, "subsys_mfg_cg");
	if (IS_ERR(g_clk->subsys_mfg_cg)) {
		GPUFREQ_LOGE("cannot get subsys_mfg_cg\n");
		return PTR_ERR(g_clk->subsys_mfg_cg);
	}

	g_clk->mtcmos_mfg_async = devm_clk_get(&pdev->dev, "mtcmos_mfg_async");
	if (IS_ERR(g_clk->mtcmos_mfg_async)) {
		GPUFREQ_LOGE("@%s: cannot get mtcmos_mfg_async\n", __func__);
		return PTR_ERR(g_clk->mtcmos_mfg_async);
	}

	g_clk->mtcmos_mfg = devm_clk_get(&pdev->dev, "mtcmos_mfg");
	if (IS_ERR(g_clk->mtcmos_mfg)) {
		GPUFREQ_LOGE("@%s: cannot get mtcmos_mfg\n", __func__);
		return PTR_ERR(g_clk->mtcmos_mfg);
	}

	g_clk->mtcmos_mfg_core0 = devm_clk_get(&pdev->dev, "mtcmos_mfg_core0");
	if (IS_ERR(g_clk->mtcmos_mfg_core0)) {
		GPUFREQ_LOGE("@%s: cannot get mtcmos_mfg_core0\n", __func__);
		return PTR_ERR(g_clk->mtcmos_mfg_core0);
	}

	g_clk->mtcmos_mfg_core1 = devm_clk_get(&pdev->dev, "mtcmos_mfg_core1");
	if (IS_ERR(g_clk->mtcmos_mfg_core1)) {
		GPUFREQ_LOGE("@%s: cannot get mtcmos_mfg_core1\n", __func__);
		return PTR_ERR(g_clk->mtcmos_mfg_core1);
	}

	GPUFREQ_LOGI("clk_mux is at 0x%p, clk_main_parent is at 0x%p, \t"
			"clk_sub_parent is at 0x%p, subsys_mfg_cg is at 0x%p, \t"
			"mtcmos_mfg_async is at 0x%p, mtcmos_mfg is at 0x%p, \t"
			"mtcmos_mfg_core0 is at 0x%p, mtcmos_mfg_core1 is at 0x%p\n",
			g_clk->clk_mux, g_clk->clk_main_parent, g_clk->clk_sub_parent,
			g_clk->subsys_mfg_cg, g_clk->mtcmos_mfg_async, g_clk->mtcmos_mfg,
			g_clk->mtcmos_mfg_core0, g_clk->mtcmos_mfg_core1);

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

	GPUFREQ_LOGD("isRising = %d, sfchg_rate_vgpu = %d\n",
			isRising, sfchg_rate_vgpu);

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

	GPUFREQ_LOGD("isRising = %d, sfchg_rate_vsram = %d\n",
			isRising, sfchg_rate_vsram);

	return sfchg_rate_vsram;
}

static int __gpufreq_init_pmic(struct platform_device *pdev)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("pdev=0x%x", pdev);

	g_pmic = kzalloc(sizeof(struct g_pmic_info), GFP_KERNEL);
	if (g_pmic == NULL) {
		GPUFREQ_LOGE("cannot allocate g_pmic\n");
		return -ENOMEM;
	}

	g_pmic->reg_vgpu = regulator_get(&pdev->dev, "vgpu");
	if (IS_ERR(g_pmic->reg_vgpu)) {
		GPUFREQ_LOGE("cannot get VGPU\n");
		return PTR_ERR(g_pmic->reg_vgpu);
	}

	g_pmic->reg_vsram_gpu = regulator_get(&pdev->dev, "vsram_gpu");
	if (IS_ERR(g_pmic->reg_vsram_gpu)) {
		GPUFREQ_LOGE("cannot get VSRAM_GPU\n");
		return PTR_ERR(g_pmic->reg_vsram_gpu);
	}

	/* setup PMIC init value */
	g_vgpu_sfchg_rrate = __calculate_vgpu_sfchg_rate(true);
	g_vgpu_sfchg_frate = __calculate_vgpu_sfchg_rate(false);
	g_vsram_sfchg_rrate = __calculate_vsram_sfchg_rate(true);
	g_vsram_sfchg_frate = __calculate_vsram_sfchg_rate(false);

	/* set VSRAM_GPU */
	regulator_set_voltage(g_pmic->reg_vsram_gpu, VSRAM_MAX_VOLT * 10, VSRAM_MAX_VOLT * 10 + 125);
	/* set VGPU */
	regulator_set_voltage(g_pmic->reg_vgpu, VGPU_MAX_VOLT * 10, VGPU_MAX_VOLT * 10 + 125);

	/* enable bucks (VGPU && VSRAM_GPU) enforcement */
	if (regulator_enable(g_pmic->reg_vsram_gpu))
		GPUFREQ_LOGE("enable VSRAM_GPU failed\n");
	if (regulator_enable(g_pmic->reg_vgpu))
		GPUFREQ_LOGE("enable VGPU failed\n");

	GPUFREQ_LOGI("VGPU sfchg raising rate: %d us, VGPU sfchg falling rate: %d us, \t"
			"VSRAM_GPU sfchg raising rate: %d us, VSRAM_GPU sfchg falling rate: %d us\n"
			, g_vgpu_sfchg_rrate, g_vgpu_sfchg_frate,
			g_vsram_sfchg_rrate, g_vsram_sfchg_frate);

	GPUFREQ_LOGI("VGPU is enabled = %d (%d mV), VSRAM_GPU is enabled = %d (%d mV)\n",
			regulator_is_enabled(g_pmic->reg_vgpu),
			(regulator_get_voltage(g_pmic->reg_vgpu) / 1000),
			regulator_is_enabled(g_pmic->reg_vsram_gpu),
			(regulator_get_voltage(g_pmic->reg_vsram_gpu) / 1000));

	udelay(80);

	if (regulator_disable(g_pmic->reg_vgpu))
		GPUFREQ_LOGE("disable VGPU failed\n");
	if (regulator_disable(g_pmic->reg_vsram_gpu))
		GPUFREQ_LOGE("disable VSRAM_GPU failed\n");

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

	g_apmixed_base = __gpufreq_of_ioremap("mediatek,apmixed", 0);
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

	/* init shader present */
	__gpufreq_init_shader_present();

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
#if IS_BUILTIN(CONFIG_MTK_GPU_MT6768_SUPPORT)
rootfs_initcall(__gpufreq_init);
#else
module_init(__gpufreq_init);
#endif
module_exit(__gpufreq_exit);

MODULE_DEVICE_TABLE(of, g_gpufreq_of_match);
MODULE_DESCRIPTION("MediaTek GPU-DVFS platform driver");
MODULE_LICENSE("GPL");
