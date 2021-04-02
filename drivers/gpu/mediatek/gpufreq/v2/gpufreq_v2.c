// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

/**
 * @file    mtk_gpufreq_v2.c
 * @brief   GPU-DVFS Driver Common Wrapper
 */

/**
 * ===============================================
 * Include
 * ===============================================
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/export.h>
#include <linux/printk.h>
#include <linux/mutex.h>

#include <gpufreq_v2.h>
#include <gpufreq_debug.h>
#include <gpuppm.h>

#if IS_ENABLED(CONFIG_MTK_BATTERY_OC_POWER_THROTTLING)
#include <mtk_battery_oc_throttling.h>
#endif
#if IS_ENABLED(CONFIG_MTK_BATTERY_PERCENTAGE_POWER_THROTTLING)
#include <mtk_battery_percentage_throttling.h>
#endif
#if IS_ENABLED(CONFIG_MTK_LOW_BATTERY_POWER_THROTTLING)
#include <mtk_low_battery_throttling.h>
#endif

/**
 * ===============================================
 * Local Function Declaration
 * ===============================================
 */
static int gpufreq_wrapper_pdrv_probe(struct platform_device *pdev);

/**
 * ===============================================
 * Local Variable Definition
 * ===============================================
 */
static const struct of_device_id g_gpufreq_wrapper_of_match[] = {
	{ .compatible = "mediatek,gpufreq_wrapper" },
	{ /* sentinel */ }
};
static struct platform_driver g_gpufreq_wrapper_pdrv = {
	.probe = gpufreq_wrapper_pdrv_probe,
	.remove = NULL,
	.driver = {
		.name = "gpufreq_wrapper",
		.owner = THIS_MODULE,
		.of_match_table = g_gpufreq_wrapper_of_match,
	},
};

static unsigned int g_gpueb_support;

/**
 * ===============================================
 * External Function Definition
 * ===============================================
 */
unsigned int gpufreq_bringup(void)
{
	return __gpufreq_bringup();
}
EXPORT_SYMBOL(gpufreq_bringup);

unsigned int gpufreq_power_ctrl_enable(void)
{
	return __gpufreq_power_ctrl_enable();
}
EXPORT_SYMBOL(gpufreq_power_ctrl_enable);

unsigned int gpufreq_get_dvfs_state(void)
{
	return __gpufreq_get_dvfs_state();
}
EXPORT_SYMBOL(gpufreq_get_dvfs_state);

unsigned int gpufreq_get_shader_present(void)
{
	return __gpufreq_get_shader_present();
}
EXPORT_SYMBOL(gpufreq_get_shader_present);

void gpufreq_set_timestamp(void)
{
	__gpufreq_set_timestamp();
}
EXPORT_SYMBOL(gpufreq_set_timestamp);

void gpufreq_check_bus_idle(void)
{
	__gpufreq_check_bus_idle();
}
EXPORT_SYMBOL(gpufreq_check_bus_idle);

void gpufreq_dump_infra_status(void)
{
	__gpufreq_dump_infra_status();
}
EXPORT_SYMBOL(gpufreq_dump_infra_status);

void gpufreq_resume_dvfs(void)
{
	GPUFREQ_TRACE_START();

	__gpufreq_resume_dvfs();

	GPUFREQ_TRACE_END();
}
EXPORT_SYMBOL(gpufreq_resume_dvfs);

int gpufreq_pause_dvfs(void)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START();

	ret = __gpufreq_pause_dvfs();

	GPUFREQ_TRACE_END();

	return ret;
}
EXPORT_SYMBOL(gpufreq_pause_dvfs);

int gpufreq_map_avs_idx(int avsidx)
{
	return __gpufreq_map_avs_idx(avsidx);
}
EXPORT_SYMBOL(gpufreq_map_avs_idx);

void gpufreq_adjust_volt_by_avs(
	unsigned int avs_volt[], unsigned int array_size)
{
	GPUFREQ_TRACE_START("avs_volt=0x%x, array_size=%d",
		avs_volt, array_size);

	__gpufreq_adjust_volt_by_avs(avs_volt, array_size);

	GPUFREQ_TRACE_END();
}
EXPORT_SYMBOL(gpufreq_adjust_volt_by_avs);

void gpufreq_restore_opp(enum gpufreq_target target)
{
	GPUFREQ_TRACE_START("target=%d", target);

	if (target >= TARGET_INVALID || target < 0)
		GPUFREQ_LOGE("invalid OPP target: %d (EINVAL)", target);

	if (target == TARGET_GPUSTACK)
		__gpufreq_restore_opp_gstack();
	else if (target == TARGET_GPU)
		__gpufreq_restore_opp_gpu();

	GPUFREQ_TRACE_END();
}
EXPORT_SYMBOL(gpufreq_restore_opp);

unsigned int gpufreq_get_cur_freq(enum gpufreq_target target)
{
	unsigned int freq = 0;

	if (target >= TARGET_INVALID || target < 0) {
		GPUFREQ_LOGE("invalid OPP target: %d (EINVAL)", target);
		goto done;
	}

	if (target == TARGET_GPUSTACK)
		freq = __gpufreq_get_cur_fgstack();
	else if (target == TARGET_GPU)
		freq = __gpufreq_get_cur_fgpu();

	GPUFREQ_LOGD("target: %s, current freq: %d",
		target == TARGET_GPUSTACK ? "GPUSTACK" : "GPU",
		freq);

done:
	return freq;
}
EXPORT_SYMBOL(gpufreq_get_cur_freq);

unsigned int gpufreq_get_cur_volt(enum gpufreq_target target)
{
	unsigned int volt = 0;

	if (target >= TARGET_INVALID || target < 0) {
		GPUFREQ_LOGE("invalid OPP target: %d (EINVAL)", target);
		goto done;
	}

	if (target == TARGET_GPUSTACK)
		volt = __gpufreq_get_cur_vgstack();
	else if (target == TARGET_GPU)
		volt = __gpufreq_get_cur_vgpu();

	GPUFREQ_LOGD("target: %s, current volt: %d",
		target == TARGET_GPUSTACK ? "GPUSTACK" : "GPU",
		volt);

done:
	return volt;
}
EXPORT_SYMBOL(gpufreq_get_cur_volt);

int gpufreq_get_cur_oppidx(enum gpufreq_target target)
{
	int oppidx = 0;

	if (target >= TARGET_INVALID || target < 0) {
		GPUFREQ_LOGE("invalid OPP target: %d (EINVAL)", target);
		goto done;
	}

	if (target == TARGET_GPUSTACK)
		oppidx = __gpufreq_get_cur_idx_gstack();
	else if (target == TARGET_GPU)
		oppidx = __gpufreq_get_cur_idx_gpu();

	GPUFREQ_LOGD("target: %s, current OPP index: %d",
		target == TARGET_GPUSTACK ? "GPUSTACK" : "GPU",
		oppidx);

done:
	return oppidx;
}
EXPORT_SYMBOL(gpufreq_get_cur_oppidx);

int gpufreq_get_max_oppidx(enum gpufreq_target target)
{
	int oppidx = 0;

	if (target >= TARGET_INVALID || target < 0) {
		GPUFREQ_LOGE("invalid OPP target: %d (EINVAL)", target);
		goto done;
	}

	if (target == TARGET_GPUSTACK)
		oppidx = __gpufreq_get_max_idx_gstack();
	else if (target == TARGET_GPU)
		oppidx = __gpufreq_get_max_idx_gpu();

	GPUFREQ_LOGD("target: %s, max OPP index: %d",
		target == TARGET_GPUSTACK ? "GPUSTACK" : "GPU",
		oppidx);

done:
	return oppidx;
}
EXPORT_SYMBOL(gpufreq_get_max_oppidx);

int gpufreq_get_min_oppidx(enum gpufreq_target target)
{
	int oppidx = 0;

	if (target >= TARGET_INVALID || target < 0) {
		GPUFREQ_LOGE("invalid OPP target: %d (EINVAL)", target);
		goto done;
	}

	if (target == TARGET_GPUSTACK)
		oppidx = __gpufreq_get_min_idx_gstack();
	else if (target == TARGET_GPU)
		oppidx = __gpufreq_get_min_idx_gpu();

	GPUFREQ_LOGD("target: %s, min OPP index: %d",
		target == TARGET_GPUSTACK ? "GPUSTACK" : "GPU",
		oppidx);

done:
	return oppidx;
}
EXPORT_SYMBOL(gpufreq_get_min_oppidx);

unsigned int gpufreq_get_opp_num(enum gpufreq_target target)
{
	unsigned int opp_num = 0;

	if (target >= TARGET_INVALID || target < 0) {
		GPUFREQ_LOGE("invalid OPP target: %d (EINVAL)", target);
		goto done;
	}

	if (target == TARGET_GPUSTACK)
		opp_num = __gpufreq_get_opp_num_gstack();
	else if (target == TARGET_GPU)
		opp_num = __gpufreq_get_opp_num_gpu();

	GPUFREQ_LOGD("target: %s, # of OPP index: %d",
		target == TARGET_GPUSTACK ? "GPUSTACK" : "GPU",
		opp_num);

done:
	return opp_num;
}
EXPORT_SYMBOL(gpufreq_get_opp_num);

unsigned int gpufreq_get_freq_by_idx(
	enum gpufreq_target target, int oppidx)
{
	unsigned int freq = 0;

	if (target >= TARGET_INVALID || target < 0) {
		GPUFREQ_LOGE("invalid OPP target: %d (EINVAL)", target);
		goto done;
	}

	if (target == TARGET_GPUSTACK)
		freq = __gpufreq_get_fgstack_by_idx(oppidx);
	else if (target == TARGET_GPU)
		freq = __gpufreq_get_fgpu_by_idx(oppidx);

	GPUFREQ_LOGD("target: %s, freq[%d]: %d",
		target == TARGET_GPUSTACK ? "GPUSTACK" : "GPU",
		oppidx, freq);

done:
	return freq;
}
EXPORT_SYMBOL(gpufreq_get_freq_by_idx);

unsigned int gpufreq_get_volt_by_idx(
	enum gpufreq_target target, int oppidx)
{
	unsigned int volt = 0;

	if (target >= TARGET_INVALID || target < 0) {
		GPUFREQ_LOGE("invalid OPP target: %d (EINVAL)", target);
		goto done;
	}

	if (target == TARGET_GPUSTACK)
		volt = __gpufreq_get_vgstack_by_idx(oppidx);
	else if (target == TARGET_GPU)
		volt = __gpufreq_get_vgpu_by_idx(oppidx);

	GPUFREQ_LOGD("target: %s, volt[%d]: %d",
		target == TARGET_GPUSTACK ? "GPUSTACK" : "GPU",
		oppidx, volt);

done:
	return volt;
}
EXPORT_SYMBOL(gpufreq_get_volt_by_idx);

unsigned int gpufreq_get_power_by_idx(
	enum gpufreq_target target, int oppidx)
{
	unsigned int power = 0;

	if (target >= TARGET_INVALID || target < 0) {
		GPUFREQ_LOGE("invalid OPP target: %d (EINVAL)", target);
		goto done;
	}

	if (target == TARGET_GPUSTACK)
		power = __gpufreq_get_pgstack_by_idx(oppidx);
	else if (target == TARGET_GPU)
		power = __gpufreq_get_pgpu_by_idx(oppidx);

	GPUFREQ_LOGD("target: %s, power[%d]: %d",
		target == TARGET_GPUSTACK ? "GPUSTACK" : "GPU",
		oppidx, power);

done:
	return power;
}
EXPORT_SYMBOL(gpufreq_get_power_by_idx);

int gpufreq_get_oppidx_by_freq(
	enum gpufreq_target target, unsigned int freq)
{
	int oppidx = 0;

	if (target >= TARGET_INVALID || target < 0) {
		GPUFREQ_LOGE("invalid OPP target: %d (EINVAL)", target);
		goto done;
	}

	if (target == TARGET_GPUSTACK)
		oppidx = __gpufreq_get_idx_by_fgstack(freq);
	else if (target == TARGET_GPU)
		oppidx = __gpufreq_get_idx_by_fgpu(freq);

	GPUFREQ_LOGD("target: %s, oppidx[%d]: %d",
		target == TARGET_GPUSTACK ? "GPUSTACK" : "GPU",
		freq, oppidx);

done:
	return oppidx;
}
EXPORT_SYMBOL(gpufreq_get_oppidx_by_freq);

int gpufreq_get_oppidx_by_power(
	enum gpufreq_target target, unsigned int power)
{
	int oppidx = 0;

	if (target >= TARGET_INVALID || target < 0) {
		GPUFREQ_LOGE("invalid OPP target: %d (EINVAL)", target);
		goto done;
	}

	if (target == TARGET_GPUSTACK)
		oppidx = __gpufreq_get_idx_by_pgstack(power);
	else if (target == TARGET_GPU)
		oppidx = __gpufreq_get_idx_by_pgpu(power);

	GPUFREQ_LOGD("target: %s, oppidx[%d]: %d",
		target == TARGET_GPUSTACK ? "GPUSTACK" : "GPU",
		power, oppidx);

done:
	return oppidx;
}
EXPORT_SYMBOL(gpufreq_get_oppidx_by_power);

unsigned int gpufreq_get_leakage_power(
	enum gpufreq_target target, unsigned int volt)
{
	unsigned int p_leakage = 0;

	if (target >= TARGET_INVALID || target < 0) {
		GPUFREQ_LOGE("invalid OPP target: %d (EINVAL)", target);
		goto done;
	}

	if (target == TARGET_GPUSTACK)
		p_leakage = __gpufreq_get_lkg_pgstack(volt);
	else if (target == TARGET_GPU)
		p_leakage = __gpufreq_get_lkg_pgpu(volt);

	GPUFREQ_LOGD("target: %s, p_leakage[v=%d]: %d",
		target == TARGET_GPUSTACK ? "GPUSTACK" : "GPU",
		volt, p_leakage);

done:
	return p_leakage;
}
EXPORT_SYMBOL(gpufreq_get_leakage_power);

unsigned int gpufreq_get_dynamic_power(
	enum gpufreq_target target, unsigned int freq, unsigned int volt)
{
	unsigned int p_dynamic = 0;

	if (target >= TARGET_INVALID || target < 0) {
		GPUFREQ_LOGE("invalid OPP target: %d (EINVAL)", target);
		goto done;
	}

	if (target == TARGET_GPUSTACK)
		p_dynamic = __gpufreq_get_dyn_pgstack(freq, volt);
	else if (target == TARGET_GPU)
		p_dynamic = __gpufreq_get_dyn_pgpu(freq, volt);

	GPUFREQ_LOGD("target: %s, p_dynamic[f=%d, v=%d]: %d",
		target == TARGET_GPUSTACK ? "GPUSTACK" : "GPU",
		freq, volt, p_dynamic);

done:
	return p_dynamic;
}
EXPORT_SYMBOL(gpufreq_get_dynamic_power);

int gpufreq_power_control(
	enum gpufreq_power_state power, enum gpufreq_cg_state cg,
	enum gpufreq_mtcmos_state mtcmos, enum gpufreq_buck_state buck)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("power=%d, cg=%d, mtcmos=%d, buck=%d",
		power, cg, mtcmos, buck);

	if (!__gpufreq_power_ctrl_enable()) {
		GPUFREQ_LOGD("power control is disabled");
		ret = GPUFREQ_SUCCESS;
		goto done;
	}

	ret = __gpufreq_power_control(power, cg, mtcmos, buck);
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to control power state (p=%d, c=%d, m=%d, b=%d) (%d)",
			power, cg, mtcmos, buck, ret);
	}

done:
	GPUFREQ_TRACE_END();

	return ret;
}
EXPORT_SYMBOL(gpufreq_power_control);

int gpufreq_commit(enum gpufreq_target target, int oppidx)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("target=%d, oppidx=%d", target, oppidx);

	if (target >= TARGET_INVALID || target < 0) {
		GPUFREQ_LOGE("invalid OPP target: %d (EINVAL)", target);
		ret = GPUFREQ_EINVAL;
		goto done;
	}

	GPUFREQ_LOGD("target: %s, oppidx: %d",
		target == TARGET_GPUSTACK ? "GPUSTACK" : "GPU",
		oppidx);

	if (target == TARGET_GPUSTACK) {
		ret = gpuppm_limited_commit_gstack(oppidx);
		if (unlikely(ret))
			GPUFREQ_LOGE("fail to commit GPUSTACK OPP index: %d (%d)",
				oppidx, ret);
	} else if (target == TARGET_GPU) {
		ret = gpuppm_limited_commit_gpu(oppidx);
		if (unlikely(ret))
			GPUFREQ_LOGE("fail to commit GPU OPP index: %d (%d)",
				oppidx, ret);
	}

done:
	GPUFREQ_TRACE_END();

	return ret;
}
EXPORT_SYMBOL(gpufreq_commit);

int gpufreq_set_limit(
	enum gpufreq_target target, unsigned int limiter,
	int ceiling, int floor)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("target=%d, limiter=%d, ceiling=%d, floor=%d",
		target, limiter, ceiling, floor);

	if (target >= TARGET_INVALID || target < 0) {
		GPUFREQ_LOGE("invalid OPP target: %d (EINVAL)", target);
		ret = GPUFREQ_EINVAL;
		goto done;
	}

	GPUFREQ_LOGD("target: %s, limiter: %d, ceiling: %d, floor: %d",
		target == TARGET_GPUSTACK ? "GPUSTACK" : "GPU",
		limiter, ceiling, floor);

	if (target == TARGET_GPUSTACK) {
		ret = gpuppm_set_limit_gstack(limiter, ceiling, floor);
		if (unlikely(ret))
			GPUFREQ_LOGE("fail to set GPUSTACK limit (%d)", ret);
	} else if (target == TARGET_GPU) {
		ret = gpuppm_set_limit_gpu(limiter, ceiling, floor);
		if (unlikely(ret))
			GPUFREQ_LOGE("fail to set GPU limit (%d)", ret);
	}

done:
	GPUFREQ_TRACE_END();

	return ret;
}
EXPORT_SYMBOL(gpufreq_set_limit);

int gpufreq_get_cur_limit_idx(
	enum gpufreq_target target, enum gpuppm_limit_type limit)
{
	int limit_idx = 0;

	if (target >= TARGET_INVALID || target < 0) {
		GPUFREQ_LOGE("invalid OPP target: %d (EINVAL)", target);
		goto done;
	}

	if (limit >= GPUPPM_INVALID || limit < 0) {
		GPUFREQ_LOGE("invalid limit target: %d (EINVAL)", limit);
		goto done;
	}

	if (target == TARGET_GPUSTACK) {
		if (limit == GPUPPM_CEILING)
			limit_idx = gpuppm_get_ceiling_gstack();
		else if (limit == GPUPPM_FLOOR)
			limit_idx = gpuppm_get_floor_gstack();
	} else if (target == TARGET_GPU) {
		if (limit == GPUPPM_CEILING)
			limit_idx = gpuppm_get_ceiling_gpu();
		else if (limit == GPUPPM_FLOOR)
			limit_idx = gpuppm_get_floor_gpu();
	}

	GPUFREQ_LOGD("target: %s, current %s index: %d",
		target == TARGET_GPUSTACK ? "GPUSTACK" : "GPU",
		limit == GPUPPM_CEILING ? "ceiling" : "floor",
		limit_idx);

done:
	return limit_idx;
}
EXPORT_SYMBOL(gpufreq_get_cur_limit_idx);

unsigned int gpufreq_get_cur_limiter(
	enum gpufreq_target target, enum gpuppm_limit_type limit)
{
	unsigned int limiter = 0;

	if (target >= TARGET_INVALID || target < 0) {
		GPUFREQ_LOGE("invalid OPP target: %d (EINVAL)", target);
		goto done;
	}

	if (limit >= GPUPPM_INVALID || limit < 0) {
		GPUFREQ_LOGE("invalid limit target: %d (EINVAL)", limit);
		goto done;
	}

	if (target == TARGET_GPUSTACK) {
		if (limit == GPUPPM_CEILING)
			limiter = gpuppm_get_c_limiter_gstack();
		else if (limit == GPUPPM_FLOOR)
			limiter = gpuppm_get_f_limiter_gstack();
	} else if (target == TARGET_GPU) {
		if (limit == GPUPPM_CEILING)
			limiter = gpuppm_get_c_limiter_gpu();
		else if (limit == GPUPPM_FLOOR)
			limiter = gpuppm_get_f_limiter_gpu();
	}

	GPUFREQ_LOGD("target: %s, current %s limiter: %d",
		target == TARGET_GPUSTACK ? "GPUSTACK" : "GPU",
		limit == GPUPPM_CEILING ? "ceiling" : "floor",
		limiter);

done:
	return limiter;
}
EXPORT_SYMBOL(gpufreq_get_cur_limiter);

#if IS_ENABLED(CONFIG_MTK_BATTERY_OC_POWER_THROTTLING)
static void gpufreq_batt_oc_callback(BATTERY_OC_LEVEL batt_oc_level)
{
	int ceiling = 0;
	int ret = GPUFREQ_SUCCESS;
	enum gpufreq_target target = TARGET_DEFAULT;

	GPUFREQ_TRACE_START("batt_oc_level=%d", batt_oc_level);

	ceiling = __gpufreq_get_batt_oc_idx(batt_oc_level);

	GPUFREQ_LOGD("target: %s, limiter: %d, battery_oc_level: %d, ceiling: %d",
		target == TARGET_GPUSTACK ? "GPUSTACK" : "GPU",
		LIMIT_BATT_OC, batt_oc_level, ceiling);

	if (target == TARGET_GPUSTACK) {
		ret = gpuppm_set_limit_gstack(LIMIT_BATT_OC,
			ceiling, GPUPPM_KEEP_IDX);
		if (unlikely(ret))
			GPUFREQ_LOGE("fail to set GPUSTACK limit (%d)", ret);
	} else if (target == TARGET_GPU) {
		ret = gpuppm_set_limit_gpu(LIMIT_BATT_OC,
			ceiling, GPUPPM_KEEP_IDX);
		if (unlikely(ret))
			GPUFREQ_LOGE("fail to set GPU limit (%d)", ret);
	}

	GPUFREQ_TRACE_END();
}
#endif /* CONFIG_MTK_BATTERY_OC_POWER_THROTTLING */

#if IS_ENABLED(CONFIG_MTK_BATTERY_PERCENTAGE_POWER_THROTTLING)
static void gpufreq_batt_percent_callback(
	BATTERY_PERCENT_LEVEL batt_percent_level)
{
	int ceiling = 0;
	int ret = GPUFREQ_SUCCESS;
	enum gpufreq_target target = TARGET_DEFAULT;

	GPUFREQ_TRACE_START("batt_percent_level=%d", batt_percent_level);

	ceiling = __gpufreq_get_batt_percent_idx(batt_percent_level);

	GPUFREQ_LOGD("target: %s, limiter: %d, battery_percent_level: %d, ceiling: %d",
		target == TARGET_GPUSTACK ? "GPUSTACK" : "GPU",
		LIMIT_BATT_PERCENT, batt_percent_level, ceiling);

	if (target == TARGET_GPUSTACK) {
		ret = gpuppm_set_limit_gstack(LIMIT_BATT_PERCENT,
			ceiling, GPUPPM_KEEP_IDX);
		if (unlikely(ret))
			GPUFREQ_LOGE("fail to set GPUSTACK limit (%d)", ret);
	} else if (target == TARGET_GPU) {
		ret = gpuppm_set_limit_gpu(LIMIT_BATT_PERCENT,
			ceiling, GPUPPM_KEEP_IDX);
		if (unlikely(ret))
			GPUFREQ_LOGE("fail to set GPU limit (%d)", ret);
	}

	GPUFREQ_TRACE_END();
}
#endif /* CONFIG_MTK_BATTERY_PERCENTAGE_POWER_THROTTLING */

#if IS_ENABLED(CONFIG_MTK_LOW_BATTERY_POWER_THROTTLING)
static void gpufreq_low_batt_callback(LOW_BATTERY_LEVEL low_batt_level)
{
	int ceiling = 0;
	int ret = GPUFREQ_SUCCESS;
	enum gpufreq_target target = TARGET_DEFAULT;

	GPUFREQ_TRACE_START("low_batt_level=%d", low_batt_level);

	ceiling = __gpufreq_get_low_batt_idx(low_batt_level);

	GPUFREQ_LOGD("target: %s, limiter: %d, low_battery_level: %d, ceiling: %d",
		target == TARGET_GPUSTACK ? "GPUSTACK" : "GPU",
		LIMIT_LOW_BATT, low_batt_level, ceiling);

	if (target == TARGET_GPUSTACK) {
		ret = gpuppm_set_limit_gstack(LIMIT_LOW_BATT,
			ceiling, GPUPPM_KEEP_IDX);
		if (unlikely(ret))
			GPUFREQ_LOGE("fail to set GPUSTACK limit (%d)", ret);
	} else if (target == TARGET_GPU) {
		ret = gpuppm_set_limit_gpu(LIMIT_LOW_BATT,
			ceiling, GPUPPM_KEEP_IDX);
		if (unlikely(ret))
			GPUFREQ_LOGE("fail to set GPU limit (%d)", ret);
	}

	GPUFREQ_TRACE_END();
}
#endif /* CONFIG_MTK_LOW_BATTERY_POWER_THROTTLING */

/*
 * API: gpufreq wrapper driver probe
 */
static int gpufreq_wrapper_pdrv_probe(struct platform_device *pdev)
{
	struct device_node *wrapper, *gpueb;
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_LOGI("start to probe gpufreq wrapper driver");

	wrapper = of_find_matching_node(NULL, g_gpufreq_wrapper_of_match);
	if (!wrapper) {
		GPUFREQ_LOGE("fail to find gpufreq wrapper node");
		ret = GPUFREQ_ENOENT;
		goto done;
	}

	gpueb = of_find_compatible_node(NULL, NULL, "mediatek,gpueb");
	if (!gpueb) {
		GPUFREQ_LOGE("fail to find gpueb node");
		ret = GPUFREQ_ENOENT;
		goto done;
	}

	ret = of_property_read_u32(gpueb, "gpueb-support", &g_gpueb_support);
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to read gpueb_support (%d)", ret);
		goto done;
	}

	GPUFREQ_LOGI("gpufreq wrapper driver probe done, gpueb: %s",
		g_gpueb_support ? "on" : "off");

done:
	return ret;
}

/*
 * API: register gpufreq wrapper driver
 */
static int __init gpufreq_wrapper_init(void)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_LOGI("start to initialize gpufreq wrapper driver");

	/* register platform driver */
	ret = platform_driver_register(&g_gpufreq_wrapper_pdrv);
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to register gpufreq wrapper driver (%d)",
			ret);
		goto done;
	}

	ret = gpufreq_debug_init();
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to init gpufreq debug (%d)", ret);
		goto done;
	}

	/* init power throttling */
#if IS_ENABLED(CONFIG_MTK_LOW_BATTERY_POWER_THROTTLING)
	register_low_battery_notify(
			&gpufreq_low_batt_callback,
			LOW_BATTERY_PRIO_GPU);
#endif /* CONFIG_MTK_LOW_BATTERY_POWER_THROTTLING */

#if IS_ENABLED(CONFIG_MTK_BATTERY_PERCENT_THROTTLING)
	register_bp_thl_notify(
			&gpufreq_batt_percent_callback,
			BATTERY_PERCENT_PRIO_GPU);
#endif /* CONFIG_MTK_BATTERY_PERCENT_THROTTLING */

#if IS_ENABLED(CONFIG_MTK_BATTERY_OC_POWER_THROTTLING)
	register_battery_oc_notify(
			&gpufreq_batt_oc_callback,
			BATTERY_OC_PRIO_GPU);
#endif /* CONFIG_MTK_BATTERY_OC_POWER_THROTTLING */

done:
	return ret;
}

/*
 * API: unregister gpufreq wrapper driver
 */
static void __exit gpufreq_wrapper_exit(void)
{
	platform_driver_unregister(&g_gpufreq_wrapper_pdrv);
}

late_initcall(gpufreq_wrapper_init);
module_exit(gpufreq_wrapper_exit);

MODULE_DEVICE_TABLE(of, g_gpufreq_wrapper_of_match);
MODULE_DESCRIPTION("MediaTek GPU-DVFS wrapper driver");
MODULE_LICENSE("GPL");
