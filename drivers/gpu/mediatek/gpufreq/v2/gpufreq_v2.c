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
#include <mtk_gpu_utility.h>

#if IS_ENABLED(CONFIG_MTK_PBM)
#include <mtk_pbm_gpu_cb.h>
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

/**
 * ===============================================
 * Local Function Declaration
 * ===============================================
 */
static int gpufreq_wrapper_pdrv_probe(struct platform_device *pdev);
static void gpufreq_init_external_callback(void);

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

static unsigned int g_dual_buck;
static unsigned int g_gpueb_support;
static struct gpufreq_platform_fp *gpufreq_fp;
static struct gpuppm_platform_fp *gpuppm_fp;

/**
 * ===============================================
 * External Function Definition
 * ===============================================
 */
/***********************************************************************************
 Function Name      : gpufreq_bringup
 Inputs             : -
 Outputs            : -
 Returns            : status - Current status of bring-up
 Description        : Check GPUFREQ bring-up status
                      If it's bring-up status, GPUFREQ is out-of-function
************************************************************************************/
unsigned int gpufreq_bringup(void)
{
	/* if bringup fp exists, it isn't bringup state */
	if (gpufreq_fp && gpufreq_fp->bringup)
		return false;
	else
		return true;
}
EXPORT_SYMBOL(gpufreq_bringup);

/***********************************************************************************
 Function Name      : gpufreq_power_ctrl_enable
 Inputs             : -
 Outputs            : -
 Returns            : status - Current status of power control
 Description        : Check whether power control of GPU HW is enable
                      If it's disabled, GPU HW is always power-on
************************************************************************************/
unsigned int gpufreq_power_ctrl_enable(void)
{
	if (gpufreq_fp && gpufreq_fp->power_ctrl_enable)
		return gpufreq_fp->power_ctrl_enable();
	else {
		GPUFREQ_LOGE("null gpufreq platform function pointer (ENOENT)");
		return false;
	}
}
EXPORT_SYMBOL(gpufreq_power_ctrl_enable);

/***********************************************************************************
 Function Name      : gpufreq_get_dvfs_state
 Inputs             : -
 Outputs            : -
 Returns            : state - Current status of GPU DVFS
 Description        : Get current DVFS state
                      If it isn't DVFS_FREE, then DVFS is fixed in some state
************************************************************************************/
unsigned int gpufreq_get_dvfs_state(void)
{
	if (gpufreq_fp && gpufreq_fp->get_dvfs_state)
		return gpufreq_fp->get_dvfs_state();
	else {
		GPUFREQ_LOGE("null gpufreq platform function pointer (ENOENT)");
		return DVFS_DISABLE;
	}
}
EXPORT_SYMBOL(gpufreq_get_dvfs_state);

/***********************************************************************************
 Function Name      : gpufreq_get_shader_present
 Inputs             : -
 Outputs            : -
 Returns            : shader_present - Current shader cores
 Description        : Get GPU shader cores
                      This is for Mali GPU DDK to control power domain of shader cores
************************************************************************************/
unsigned int gpufreq_get_shader_present(void)
{
	if (gpufreq_fp && gpufreq_fp->get_shader_present)
		return gpufreq_fp->get_shader_present();
	else {
		GPUFREQ_LOGE("null gpufreq platform function pointer (ENOENT)");
		return 0x0;
	}
}
EXPORT_SYMBOL(gpufreq_get_shader_present);

/***********************************************************************************
 Function Name      : gpufreq_set_timestamp
 Inputs             : -
 Outputs            : -
 Returns            : -
 Description        : Set timestamp for clGetEventProfilingInfo
************************************************************************************/
void gpufreq_set_timestamp(void)
{
	if (gpufreq_fp && gpufreq_fp->set_timestamp)
		gpufreq_fp->set_timestamp();
	else
		GPUFREQ_LOGE("null gpufreq platform function pointer (ENOENT)");
}
EXPORT_SYMBOL(gpufreq_set_timestamp);

/***********************************************************************************
 Function Name      : gpufreq_check_bus_idle
 Inputs             : -
 Outputs            : -
 Returns            : -
 Description        : Check bus idle before GPU power-off
************************************************************************************/
void gpufreq_check_bus_idle(void)
{
	if (gpufreq_fp && gpufreq_fp->check_bus_idle)
		gpufreq_fp->check_bus_idle();
	else
		GPUFREQ_LOGE("null gpufreq platform function pointer (ENOENT)");
}
EXPORT_SYMBOL(gpufreq_check_bus_idle);

/***********************************************************************************
 Function Name      : gpufreq_dump_infra_status
 Inputs             : -
 Outputs            : -
 Returns            : -
 Description        : Dump GPU related infra status
************************************************************************************/
void gpufreq_dump_infra_status(void)
{
	if (gpufreq_fp && gpufreq_fp->dump_infra_status)
		gpufreq_fp->dump_infra_status();
	else
		GPUFREQ_LOGE("null gpufreq platform function pointer (ENOENT)");
}
EXPORT_SYMBOL(gpufreq_dump_infra_status);

/***********************************************************************************
 Function Name      : gpufreq_get_cur_freq
 Inputs             : target - Target of GPU DVFS (GPU, STACK, DEFAULT)
 Outputs            : -
 Returns            : freq   - Current freq of given target
 Description        : Query current frequency of the target
************************************************************************************/
unsigned int gpufreq_get_cur_freq(enum gpufreq_target target)
{
	unsigned int freq = 0;

	if (target >= TARGET_INVALID || target < 0 ||
		(target == TARGET_STACK && !g_dual_buck)) {
		GPUFREQ_LOGE("invalid OPP target: %d (EINVAL)", target);
		goto done;
	}

	if (target == TARGET_DEFAULT) {
		if (g_dual_buck)
			target = TARGET_STACK;
		else
			target = TARGET_GPU;
	}

	if (target == TARGET_STACK && gpufreq_fp && gpufreq_fp->get_cur_fstack)
		freq = gpufreq_fp->get_cur_fstack();
	else if (target == TARGET_GPU && gpufreq_fp && gpufreq_fp->get_cur_fgpu)
		freq = gpufreq_fp->get_cur_fgpu();
	else
		GPUFREQ_LOGE("null gpufreq platform function pointer (ENOENT)");

	GPUFREQ_LOGD("target: %s, current freq: %d",
		target == TARGET_STACK ? "STACK" : "GPU",
		freq);

done:
	return freq;
}
EXPORT_SYMBOL(gpufreq_get_cur_freq);

/***********************************************************************************
 Function Name      : gpufreq_get_max_freq
 Inputs             : target - Target of GPU DVFS (GPU, STACK, DEFAULT)
 Outputs            : -
 Returns            : freq   - Max freq of given target in working table
 Description        : Query maximum frequency of the target
************************************************************************************/
unsigned int gpufreq_get_max_freq(enum gpufreq_target target)
{
	unsigned int freq = 0;

	if (target >= TARGET_INVALID || target < 0 ||
		(target == TARGET_STACK && !g_dual_buck)) {
		GPUFREQ_LOGE("invalid OPP target: %d (EINVAL)", target);
		goto done;
	}

	if (target == TARGET_DEFAULT) {
		if (g_dual_buck)
			target = TARGET_STACK;
		else
			target = TARGET_GPU;
	}

	if (target == TARGET_STACK && gpufreq_fp && gpufreq_fp->get_max_fstack)
		freq = gpufreq_fp->get_max_fstack();
	else if (target == TARGET_GPU && gpufreq_fp && gpufreq_fp->get_max_fgpu)
		freq = gpufreq_fp->get_max_fgpu();
	else
		GPUFREQ_LOGE("null gpufreq platform function pointer (ENOENT)");

	GPUFREQ_LOGD("target: %s, max freq: %d",
		target == TARGET_STACK ? "STACK" : "GPU",
		freq);

done:
	return freq;
}
EXPORT_SYMBOL(gpufreq_get_max_freq);

/***********************************************************************************
 Function Name      : gpufreq_get_min_freq
 Inputs             : target - Target of GPU DVFS (GPU, STACK, DEFAULT)
 Outputs            : -
 Returns            : freq   - Min freq of given target in working table
 Description        : Query minimum frequency of the target
************************************************************************************/
unsigned int gpufreq_get_min_freq(enum gpufreq_target target)
{
	unsigned int freq = 0;

	if (target >= TARGET_INVALID || target < 0 ||
		(target == TARGET_STACK && !g_dual_buck)) {
		GPUFREQ_LOGE("invalid OPP target: %d (EINVAL)", target);
		goto done;
	}

	if (target == TARGET_DEFAULT) {
		if (g_dual_buck)
			target = TARGET_STACK;
		else
			target = TARGET_GPU;
	}

	if (target == TARGET_STACK && gpufreq_fp && gpufreq_fp->get_min_fstack)
		freq = gpufreq_fp->get_min_fstack();
	else if (target == TARGET_GPU && gpufreq_fp && gpufreq_fp->get_min_fgpu)
		freq = gpufreq_fp->get_min_fgpu();
	else
		GPUFREQ_LOGE("null gpufreq platform function pointer (ENOENT)");

	GPUFREQ_LOGD("target: %s, min freq: %d",
		target == TARGET_STACK ? "STACK" : "GPU",
		freq);

done:
	return freq;
}
EXPORT_SYMBOL(gpufreq_get_min_freq);

/***********************************************************************************
 Function Name      : gpufreq_get_cur_volt
 Inputs             : target - Target of GPU DVFS (GPU, STACK, DEFAULT)
 Outputs            : -
 Returns            : volt   - Current volt of given target
 Description        : Query current voltage of the target
************************************************************************************/
unsigned int gpufreq_get_cur_volt(enum gpufreq_target target)
{
	unsigned int volt = 0;

	if (target >= TARGET_INVALID || target < 0 ||
		(target == TARGET_STACK && !g_dual_buck)) {
		GPUFREQ_LOGE("invalid OPP target: %d (EINVAL)", target);
		goto done;
	}

	if (target == TARGET_DEFAULT) {
		if (g_dual_buck)
			target = TARGET_STACK;
		else
			target = TARGET_GPU;
	}

	if (target == TARGET_STACK && gpufreq_fp && gpufreq_fp->get_cur_vstack)
		volt = gpufreq_fp->get_cur_vstack();
	else if (target == TARGET_GPU && gpufreq_fp && gpufreq_fp->get_cur_vgpu)
		volt = gpufreq_fp->get_cur_vgpu();
	else
		GPUFREQ_LOGE("null gpufreq platform function pointer (ENOENT)");

	GPUFREQ_LOGD("target: %s, current volt: %d",
		target == TARGET_STACK ? "STACK" : "GPU",
		volt);

done:
	return volt;
}
EXPORT_SYMBOL(gpufreq_get_cur_volt);

/***********************************************************************************
 Function Name      : gpufreq_get_max_volt
 Inputs             : target - Target of GPU DVFS (GPU, STACK, DEFAULT)
 Outputs            : -
 Returns            : volt   - Max volt of given target in working table
 Description        : Query maximum voltage of the target
************************************************************************************/
unsigned int gpufreq_get_max_volt(enum gpufreq_target target)
{
	unsigned int volt = 0;

	if (target >= TARGET_INVALID || target < 0 ||
		(target == TARGET_STACK && !g_dual_buck)) {
		GPUFREQ_LOGE("invalid OPP target: %d (EINVAL)", target);
		goto done;
	}

	if (target == TARGET_DEFAULT) {
		if (g_dual_buck)
			target = TARGET_STACK;
		else
			target = TARGET_GPU;
	}

	if (target == TARGET_STACK && gpufreq_fp && gpufreq_fp->get_max_vstack)
		volt = gpufreq_fp->get_max_vstack();
	else if (target == TARGET_GPU && gpufreq_fp && gpufreq_fp->get_max_vgpu)
		volt = gpufreq_fp->get_max_vgpu();
	else
		GPUFREQ_LOGE("null gpufreq platform function pointer (ENOENT)");

	GPUFREQ_LOGD("target: %s, max volt: %d",
		target == TARGET_STACK ? "STACK" : "GPU",
		volt);

done:
	return volt;
}
EXPORT_SYMBOL(gpufreq_get_max_volt);

/***********************************************************************************
 Function Name      : gpufreq_get_min_volt
 Inputs             : target - Target of GPU DVFS (GPU, STACK, DEFAULT)
 Outputs            : -
 Returns            : volt   - Min volt of given target in working table
 Description        : Query minimum voltage of the target
************************************************************************************/
unsigned int gpufreq_get_min_volt(enum gpufreq_target target)
{
	unsigned int volt = 0;

	if (target >= TARGET_INVALID || target < 0 ||
		(target == TARGET_STACK && !g_dual_buck)) {
		GPUFREQ_LOGE("invalid OPP target: %d (EINVAL)", target);
		goto done;
	}

	if (target == TARGET_DEFAULT) {
		if (g_dual_buck)
			target = TARGET_STACK;
		else
			target = TARGET_GPU;
	}

	if (target == TARGET_STACK && gpufreq_fp && gpufreq_fp->get_min_vstack)
		volt = gpufreq_fp->get_min_vstack();
	else if (target == TARGET_GPU && gpufreq_fp && gpufreq_fp->get_min_vgpu)
		volt = gpufreq_fp->get_min_vgpu();
	else
		GPUFREQ_LOGE("null gpufreq platform function pointer (ENOENT)");

	GPUFREQ_LOGD("target: %s, min volt: %d",
		target == TARGET_STACK ? "STACK" : "GPU",
		volt);

done:
	return volt;
}
EXPORT_SYMBOL(gpufreq_get_min_volt);

/***********************************************************************************
 Function Name      : gpufreq_get_cur_vsram
 Inputs             : target - Target of GPU DVFS (GPU, STACK, DEFAULT)
 Outputs            : -
 Returns            : vsram  - Current vsram volt of given target
 Description        : Query current vsram voltage of the target
************************************************************************************/
unsigned int gpufreq_get_cur_vsram(enum gpufreq_target target)
{
	unsigned int vsram = 0;

	if (target >= TARGET_INVALID || target < 0 ||
		(target == TARGET_STACK && !g_dual_buck)) {
		GPUFREQ_LOGE("invalid OPP target: %d (EINVAL)", target);
		goto done;
	}

	if (target == TARGET_DEFAULT) {
		if (g_dual_buck)
			target = TARGET_STACK;
		else
			target = TARGET_GPU;
	}

	if (target == TARGET_STACK && gpufreq_fp && gpufreq_fp->get_cur_vsram_stack)
		vsram = gpufreq_fp->get_cur_vsram_stack();
	else if (target == TARGET_GPU && gpufreq_fp && gpufreq_fp->get_cur_vsram_gpu)
		vsram = gpufreq_fp->get_cur_vsram_gpu();
	else
		GPUFREQ_LOGE("null gpufreq platform function pointer (ENOENT)");

	GPUFREQ_LOGD("target: %s, current vsram: %d",
		target == TARGET_STACK ? "STACK" : "GPU",
		vsram);

done:
	return vsram;
}
EXPORT_SYMBOL(gpufreq_get_cur_vsram);

/***********************************************************************************
 Function Name      : gpufreq_get_cur_power
 Inputs             : target - Target of GPU DVFS (GPU, STACK, DEFAULT)
 Outputs            : -
 Returns            : power  - Current power of given target
 Description        : Query current power of the target
************************************************************************************/
unsigned int gpufreq_get_cur_power(enum gpufreq_target target)
{
	unsigned int power = 0;

	if (target >= TARGET_INVALID || target < 0 ||
		(target == TARGET_STACK && !g_dual_buck)) {
		GPUFREQ_LOGE("invalid OPP target: %d (EINVAL)", target);
		goto done;
	}

	if (target == TARGET_DEFAULT) {
		if (g_dual_buck)
			target = TARGET_STACK;
		else
			target = TARGET_GPU;
	}

	if (target == TARGET_STACK && gpufreq_fp && gpufreq_fp->get_cur_pstack)
		power = gpufreq_fp->get_cur_pstack();
	else if (target == TARGET_GPU && gpufreq_fp && gpufreq_fp->get_cur_pgpu)
		power = gpufreq_fp->get_cur_pgpu();
	else
		GPUFREQ_LOGE("null gpufreq platform function pointer (ENOENT)");

	GPUFREQ_LOGD("target: %s, current power: %d",
		target == TARGET_STACK ? "STACK" : "GPU",
		power);

done:
	return power;
}
EXPORT_SYMBOL(gpufreq_get_cur_power);

/***********************************************************************************
 Function Name      : gpufreq_get_max_power
 Inputs             : target - Target of GPU DVFS (GPU, STACK, DEFAULT)
 Outputs            : -
 Returns            : power  - Max power of given target in working table
 Description        : Query maximum power of the target
************************************************************************************/
unsigned int gpufreq_get_max_power(enum gpufreq_target target)
{
	unsigned int power = 0;

	if (target >= TARGET_INVALID || target < 0 ||
		(target == TARGET_STACK && !g_dual_buck)) {
		GPUFREQ_LOGE("invalid OPP target: %d (EINVAL)", target);
		goto done;
	}

	if (target == TARGET_DEFAULT) {
		if (g_dual_buck)
			target = TARGET_STACK;
		else
			target = TARGET_GPU;
	}

	if (target == TARGET_STACK && gpufreq_fp && gpufreq_fp->get_max_pstack)
		power = gpufreq_fp->get_max_pstack();
	else if (target == TARGET_GPU && gpufreq_fp && gpufreq_fp->get_max_pgpu)
		power = gpufreq_fp->get_max_pgpu();
	else
		GPUFREQ_LOGE("null gpufreq platform function pointer (ENOENT)");

	GPUFREQ_LOGD("target: %s, max power: %d",
		target == TARGET_STACK ? "STACK" : "GPU",
		power);

done:
	return power;
}
EXPORT_SYMBOL(gpufreq_get_max_power);

/***********************************************************************************
 Function Name      : gpufreq_get_min_power
 Inputs             : target - Target of GPU DVFS (GPU, STACK, DEFAULT)
 Outputs            : -
 Returns            : power  - Min power of given target in working table
 Description        : Query minimum power of the target
************************************************************************************/
unsigned int gpufreq_get_min_power(enum gpufreq_target target)
{
	unsigned int power = 0;

	if (target >= TARGET_INVALID || target < 0 ||
		(target == TARGET_STACK && !g_dual_buck)) {
		GPUFREQ_LOGE("invalid OPP target: %d (EINVAL)", target);
		goto done;
	}

	if (target == TARGET_DEFAULT) {
		if (g_dual_buck)
			target = TARGET_STACK;
		else
			target = TARGET_GPU;
	}

	if (target == TARGET_STACK && gpufreq_fp && gpufreq_fp->get_min_pstack)
		power = gpufreq_fp->get_min_pstack();
	else if (target == TARGET_GPU && gpufreq_fp && gpufreq_fp->get_min_pgpu)
		power = gpufreq_fp->get_min_pgpu();
	else
		GPUFREQ_LOGE("null gpufreq platform function pointer (ENOENT)");

	GPUFREQ_LOGD("target: %s, min power: %d",
		target == TARGET_STACK ? "STACK" : "GPU",
		power);

done:
	return power;
}
EXPORT_SYMBOL(gpufreq_get_min_power);

/***********************************************************************************
 Function Name      : gpufreq_get_cur_oppidx
 Inputs             : target - Target of GPU DVFS (GPU, STACK, DEFAULT)
 Outputs            : -
 Returns            : oppidx - Current working OPP index of given target
 Description        : Query current working OPP index of the target
************************************************************************************/
int gpufreq_get_cur_oppidx(enum gpufreq_target target)
{
	int oppidx = -1;

	if (target >= TARGET_INVALID || target < 0 ||
		(target == TARGET_STACK && !g_dual_buck)) {
		GPUFREQ_LOGE("invalid OPP target: %d (EINVAL)", target);
		goto done;
	}

	if (target == TARGET_DEFAULT) {
		if (g_dual_buck)
			target = TARGET_STACK;
		else
			target = TARGET_GPU;
	}

	if (target == TARGET_STACK && gpufreq_fp && gpufreq_fp->get_cur_idx_stack)
		oppidx = gpufreq_fp->get_cur_idx_stack();
	else if (target == TARGET_GPU && gpufreq_fp && gpufreq_fp->get_cur_idx_gpu)
		oppidx = gpufreq_fp->get_cur_idx_gpu();
	else
		GPUFREQ_LOGE("null gpufreq platform function pointer (ENOENT)");

	GPUFREQ_LOGD("target: %s, current OPP index: %d",
		target == TARGET_STACK ? "STACK" : "GPU",
		oppidx);

done:
	return oppidx;
}
EXPORT_SYMBOL(gpufreq_get_cur_oppidx);

/***********************************************************************************
 Function Name      : gpufreq_get_max_oppidx
 Inputs             : target - Target of GPU DVFS (GPU, STACK, DEFAULT)
 Outputs            : -
 Returns            : oppidx - Max working OPP index of given target
 Description        : Query maximum working OPP index of the target
************************************************************************************/
int gpufreq_get_max_oppidx(enum gpufreq_target target)
{
	int oppidx = -1;

	if (target >= TARGET_INVALID || target < 0 ||
		(target == TARGET_STACK && !g_dual_buck)) {
		GPUFREQ_LOGE("invalid OPP target: %d (EINVAL)", target);
		goto done;
	}

	if (target == TARGET_DEFAULT) {
		if (g_dual_buck)
			target = TARGET_STACK;
		else
			target = TARGET_GPU;
	}

	if (target == TARGET_STACK && gpufreq_fp && gpufreq_fp->get_max_idx_stack)
		oppidx = gpufreq_fp->get_max_idx_stack();
	else if (target == TARGET_GPU && gpufreq_fp && gpufreq_fp->get_max_idx_gpu)
		oppidx = gpufreq_fp->get_max_idx_gpu();
	else
		GPUFREQ_LOGE("null gpufreq platform function pointer (ENOENT)");

	GPUFREQ_LOGD("target: %s, max OPP index: %d",
		target == TARGET_STACK ? "STACK" : "GPU",
		oppidx);

done:
	return oppidx;
}
EXPORT_SYMBOL(gpufreq_get_max_oppidx);

/***********************************************************************************
 Function Name      : gpufreq_get_min_oppidx
 Inputs             : target - Target of GPU DVFS (GPU, STACK, DEFAULT)
 Outputs            : -
 Returns            : oppidx - Min working OPP index of given target
 Description        : Query minimum working OPP index of the target
************************************************************************************/
int gpufreq_get_min_oppidx(enum gpufreq_target target)
{
	int oppidx = -1;

	if (target >= TARGET_INVALID || target < 0 ||
		(target == TARGET_STACK && !g_dual_buck)) {
		GPUFREQ_LOGE("invalid OPP target: %d (EINVAL)", target);
		goto done;
	}

	if (target == TARGET_DEFAULT) {
		if (g_dual_buck)
			target = TARGET_STACK;
		else
			target = TARGET_GPU;
	}

	if (target == TARGET_STACK && gpufreq_fp && gpufreq_fp->get_min_idx_stack)
		oppidx = gpufreq_fp->get_min_idx_stack();
	else if (target == TARGET_GPU && gpufreq_fp && gpufreq_fp->get_min_idx_gpu)
		oppidx = gpufreq_fp->get_min_idx_gpu();
	else
		GPUFREQ_LOGE("null gpufreq platform function pointer (ENOENT)");

	GPUFREQ_LOGD("target: %s, min OPP index: %d",
		target == TARGET_STACK ? "STACK" : "GPU",
		oppidx);

done:
	return oppidx;
}
EXPORT_SYMBOL(gpufreq_get_min_oppidx);

/***********************************************************************************
 Function Name      : gpufreq_get_opp_num
 Inputs             : target  - Target of GPU DVFS (GPU, STACK, DEFAULT)
 Outputs            : -
 Returns            : opp_num - # of OPP index in working table of given target
 Description        : Query number of OPP index in working table of the target
************************************************************************************/
int gpufreq_get_opp_num(enum gpufreq_target target)
{
	int opp_num = 0;

	if (target >= TARGET_INVALID || target < 0 ||
		(target == TARGET_STACK && !g_dual_buck)) {
		GPUFREQ_LOGE("invalid OPP target: %d (EINVAL)", target);
		goto done;
	}

	if (target == TARGET_DEFAULT) {
		if (g_dual_buck)
			target = TARGET_STACK;
		else
			target = TARGET_GPU;
	}

	if (target == TARGET_STACK && gpufreq_fp && gpufreq_fp->get_opp_num_stack)
		opp_num = gpufreq_fp->get_opp_num_stack();
	else if (target == TARGET_GPU && gpufreq_fp && gpufreq_fp->get_opp_num_gpu)
		opp_num = gpufreq_fp->get_opp_num_gpu();
	else
		GPUFREQ_LOGE("null gpufreq platform function pointer (ENOENT)");

	GPUFREQ_LOGD("target: %s, # of OPP index: %d",
		target == TARGET_STACK ? "STACK" : "GPU",
		opp_num);

done:
	return opp_num;
}
EXPORT_SYMBOL(gpufreq_get_opp_num);

/***********************************************************************************
 Function Name      : gpufreq_get_freq_by_idx
 Inputs             : target - Target of GPU DVFS (GPU, STACK, DEFAULT)
                      oppidx - Worknig OPP index of the target
 Outputs            : -
 Returns            : freq   - Freq of given target at given working OPP index
 Description        : Query freq of the target by working OPP index
************************************************************************************/
unsigned int gpufreq_get_freq_by_idx(enum gpufreq_target target, int oppidx)
{
	unsigned int freq = 0;

	if (target >= TARGET_INVALID || target < 0 ||
		(target == TARGET_STACK && !g_dual_buck)) {
		GPUFREQ_LOGE("invalid OPP target: %d (EINVAL)", target);
		goto done;
	}

	if (target == TARGET_DEFAULT) {
		if (g_dual_buck)
			target = TARGET_STACK;
		else
			target = TARGET_GPU;
	}

	if (target == TARGET_STACK && gpufreq_fp && gpufreq_fp->get_fstack_by_idx)
		freq = gpufreq_fp->get_fstack_by_idx(oppidx);
	else if (target == TARGET_GPU && gpufreq_fp && gpufreq_fp->get_fgpu_by_idx)
		freq = gpufreq_fp->get_fgpu_by_idx(oppidx);
	else
		GPUFREQ_LOGE("null gpufreq platform function pointer (ENOENT)");

	GPUFREQ_LOGD("target: %s, freq[%d]: %d",
		target == TARGET_STACK ? "STACK" : "GPU",
		oppidx, freq);

done:
	return freq;
}
EXPORT_SYMBOL(gpufreq_get_freq_by_idx);

/***********************************************************************************
 Function Name      : gpufreq_get_volt_by_idx
 Inputs             : target - Target of GPU DVFS (GPU, STACK, DEFAULT)
                      oppidx - Worknig OPP index of the target
 Outputs            : -
 Returns            : volt   - Volt of given target at given working OPP index
 Description        : Query volt of the target by working OPP index
************************************************************************************/
unsigned int gpufreq_get_volt_by_idx(enum gpufreq_target target, int oppidx)
{
	unsigned int volt = 0;

	if (target >= TARGET_INVALID || target < 0 ||
		(target == TARGET_STACK && !g_dual_buck)) {
		GPUFREQ_LOGE("invalid OPP target: %d (EINVAL)", target);
		goto done;
	}

	if (target == TARGET_DEFAULT) {
		if (g_dual_buck)
			target = TARGET_STACK;
		else
			target = TARGET_GPU;
	}

	if (target == TARGET_STACK && gpufreq_fp && gpufreq_fp->get_vstack_by_idx)
		volt = gpufreq_fp->get_vstack_by_idx(oppidx);
	else if (target == TARGET_GPU && gpufreq_fp && gpufreq_fp->get_vgpu_by_idx)
		volt = gpufreq_fp->get_vgpu_by_idx(oppidx);
	else
		GPUFREQ_LOGE("null gpufreq platform function pointer (ENOENT)");

	GPUFREQ_LOGD("target: %s, volt[%d]: %d",
		target == TARGET_STACK ? "STACK" : "GPU",
		oppidx, volt);

done:
	return volt;
}
EXPORT_SYMBOL(gpufreq_get_volt_by_idx);


/***********************************************************************************
 Function Name      : gpufreq_get_power_by_idx
 Inputs             : target - Target of GPU DVFS (GPU, STACK, DEFAULT)
                      oppidx - Worknig OPP index of the target
 Outputs            : -
 Returns            : power  - Power of given target at given working OPP index
 Description        : Query volt of the target by working OPP index
************************************************************************************/
unsigned int gpufreq_get_power_by_idx(enum gpufreq_target target, int oppidx)
{
	unsigned int power = 0;

	if (target >= TARGET_INVALID || target < 0 ||
		(target == TARGET_STACK && !g_dual_buck)) {
		GPUFREQ_LOGE("invalid OPP target: %d (EINVAL)", target);
		goto done;
	}

	if (target == TARGET_DEFAULT) {
		if (g_dual_buck)
			target = TARGET_STACK;
		else
			target = TARGET_GPU;
	}

	if (target == TARGET_STACK && gpufreq_fp && gpufreq_fp->get_pstack_by_idx)
		power = gpufreq_fp->get_pstack_by_idx(oppidx);
	else if (target == TARGET_GPU && gpufreq_fp && gpufreq_fp->get_pgpu_by_idx)
		power = gpufreq_fp->get_pgpu_by_idx(oppidx);
	else
		GPUFREQ_LOGE("null gpufreq platform function pointer (ENOENT)");

	GPUFREQ_LOGD("target: %s, power[%d]: %d",
		target == TARGET_STACK ? "STACK" : "GPU",
		oppidx, power);

done:
	return power;
}
EXPORT_SYMBOL(gpufreq_get_power_by_idx);

/***********************************************************************************
 Function Name      : gpufreq_get_oppidx_by_freq
 Inputs             : target - Target of GPU DVFS (GPU, STACK, DEFAULT)
                      freq   - Freq of the target
 Outputs            : -
 Returns            : oppidx - Working OPP index of given target that has given freq
 Description        : Query working OPP index of the target that has the frequency
************************************************************************************/
int gpufreq_get_oppidx_by_freq(enum gpufreq_target target, unsigned int freq)
{
	int oppidx = -1;

	if (target >= TARGET_INVALID || target < 0 ||
		(target == TARGET_STACK && !g_dual_buck)) {
		GPUFREQ_LOGE("invalid OPP target: %d (EINVAL)", target);
		goto done;
	}

	if (target == TARGET_DEFAULT) {
		if (g_dual_buck)
			target = TARGET_STACK;
		else
			target = TARGET_GPU;
	}

	if (target == TARGET_STACK && gpufreq_fp && gpufreq_fp->get_idx_by_fstack)
		oppidx = gpufreq_fp->get_idx_by_fstack(freq);
	else if (target == TARGET_GPU && gpufreq_fp && gpufreq_fp->get_idx_by_fgpu)
		oppidx = gpufreq_fp->get_idx_by_fgpu(freq);
	else
		GPUFREQ_LOGE("null gpufreq platform function pointer (ENOENT)");

	GPUFREQ_LOGD("target: %s, oppidx[%d]: %d",
		target == TARGET_STACK ? "STACK" : "GPU",
		freq, oppidx);

done:
	return oppidx;
}
EXPORT_SYMBOL(gpufreq_get_oppidx_by_freq);

/***********************************************************************************
 Function Name      : gpufreq_get_oppidx_by_volt
 Inputs             : target - Target of GPU DVFS (GPU, STACK, DEFAULT)
                      volt   - Volt of the target
 Outputs            : -
 Returns            : oppidx - Working OPP index of given target that has given volt
 Description        : Query working OPP index of the target that has the voltage
************************************************************************************/
int gpufreq_get_oppidx_by_volt(enum gpufreq_target target, unsigned int volt)
{
	int oppidx = -1;

	if (target >= TARGET_INVALID || target < 0 ||
		(target == TARGET_STACK && !g_dual_buck)) {
		GPUFREQ_LOGE("invalid OPP target: %d (EINVAL)", target);
		goto done;
	}

	if (target == TARGET_DEFAULT) {
		if (g_dual_buck)
			target = TARGET_STACK;
		else
			target = TARGET_GPU;
	}

	if (target == TARGET_STACK && gpufreq_fp && gpufreq_fp->get_idx_by_vstack)
		oppidx = gpufreq_fp->get_idx_by_vstack(volt);
	else if (target == TARGET_GPU && gpufreq_fp && gpufreq_fp->get_idx_by_vgpu)
		oppidx = gpufreq_fp->get_idx_by_vgpu(volt);
	else
		GPUFREQ_LOGE("null gpufreq platform function pointer (ENOENT)");

	GPUFREQ_LOGD("target: %s, oppidx[%d]: %d",
		target == TARGET_STACK ? "STACK" : "GPU",
		volt, oppidx);

done:
	return oppidx;
}
EXPORT_SYMBOL(gpufreq_get_oppidx_by_volt);

/***********************************************************************************
 Function Name      : gpufreq_get_oppidx_by_power
 Inputs             : target - Target of GPU DVFS (GPU, STACK, DEFAULT)
                      power  - Power of the target
 Outputs            : -
 Returns            : oppidx - Working OPP index of given target that has given power
 Description        : Query working OPP index of the target that has the power
************************************************************************************/
int gpufreq_get_oppidx_by_power(enum gpufreq_target target, unsigned int power)
{
	int oppidx = -1;

	if (target >= TARGET_INVALID || target < 0 ||
		(target == TARGET_STACK && !g_dual_buck)) {
		GPUFREQ_LOGE("invalid OPP target: %d (EINVAL)", target);
		goto done;
	}

	if (target == TARGET_DEFAULT) {
		if (g_dual_buck)
			target = TARGET_STACK;
		else
			target = TARGET_GPU;
	}

	if (target == TARGET_STACK && gpufreq_fp && gpufreq_fp->get_idx_by_pstack)
		oppidx = gpufreq_fp->get_idx_by_pstack(power);
	else if (target == TARGET_GPU && gpufreq_fp && gpufreq_fp->get_idx_by_pgpu)
		oppidx = gpufreq_fp->get_idx_by_pgpu(power);
	else
		GPUFREQ_LOGE("null gpufreq platform function pointer (ENOENT)");

	GPUFREQ_LOGD("target: %s, oppidx[%d]: %d",
		target == TARGET_STACK ? "STACK" : "GPU",
		power, oppidx);

done:
	return oppidx;
}
EXPORT_SYMBOL(gpufreq_get_oppidx_by_power);

/***********************************************************************************
 Function Name      : gpufreq_get_leakage_power
 Inputs             : target    - Target of GPU DVFS (GPU, STACK, DEFAULT)
                      volt      - Voltage of the target
 Outputs            : -
 Returns            : p_leakage - Leakage power of given target computed by given volt
 Description        : Compute leakage power of the target by the voltage
************************************************************************************/
unsigned int gpufreq_get_leakage_power(enum gpufreq_target target, unsigned int volt)
{
	unsigned int p_leakage = 0;

	if (target >= TARGET_INVALID || target < 0 ||
		(target == TARGET_STACK && !g_dual_buck)) {
		GPUFREQ_LOGE("invalid OPP target: %d (EINVAL)", target);
		goto done;
	}

	if (target == TARGET_DEFAULT) {
		if (g_dual_buck)
			target = TARGET_STACK;
		else
			target = TARGET_GPU;
	}

	if (target == TARGET_STACK && gpufreq_fp && gpufreq_fp->get_lkg_pstack)
		p_leakage = gpufreq_fp->get_lkg_pstack(volt);
	else if (target == TARGET_GPU && gpufreq_fp && gpufreq_fp->get_lkg_pgpu)
		p_leakage = gpufreq_fp->get_lkg_pgpu(volt);
	else
		GPUFREQ_LOGE("null gpufreq platform function pointer (ENOENT)");

	GPUFREQ_LOGD("target: %s, p_leakage[v=%d]: %d",
		target == TARGET_STACK ? "STACK" : "GPU",
		volt, p_leakage);

done:
	return p_leakage;
}
EXPORT_SYMBOL(gpufreq_get_leakage_power);

/***********************************************************************************
 Function Name      : gpufreq_get_dynamic_power
 Inputs             : target    - Target of GPU DVFS (GPU, STACK, DEFAULT)
                      freq      - Frequency of the target
                      volt      - Voltage of the target
 Outputs            : -
 Returns            : p_dynamic - Dynamic power of given target computed by given freq
                                  and volt
 Description        : Compute dynamic power of the target by the frequency and voltage
************************************************************************************/
unsigned int gpufreq_get_dynamic_power(enum gpufreq_target target,
	unsigned int freq, unsigned int volt)
{
	unsigned int p_dynamic = 0;

	if (target >= TARGET_INVALID || target < 0 ||
		(target == TARGET_STACK && !g_dual_buck)) {
		GPUFREQ_LOGE("invalid OPP target: %d (EINVAL)", target);
		goto done;
	}

	if (target == TARGET_DEFAULT) {
		if (g_dual_buck)
			target = TARGET_STACK;
		else
			target = TARGET_GPU;
	}

	if (target == TARGET_STACK && gpufreq_fp && gpufreq_fp->get_dyn_pstack)
		p_dynamic = gpufreq_fp->get_dyn_pstack(freq, volt);
	else if (target == TARGET_GPU && gpufreq_fp && gpufreq_fp->get_dyn_pgpu)
		p_dynamic = gpufreq_fp->get_dyn_pgpu(freq, volt);
	else
		GPUFREQ_LOGE("null gpufreq platform function pointer (ENOENT)");

	GPUFREQ_LOGD("target: %s, p_dynamic[f=%d, v=%d]: %d",
		target == TARGET_STACK ? "STACK" : "GPU",
		freq, volt, p_dynamic);

done:
	return p_dynamic;
}
EXPORT_SYMBOL(gpufreq_get_dynamic_power);

/***********************************************************************************
 Function Name      : gpufreq_power_control
 Inputs             : power          - Target power state
 Outputs            : -
 Returns            : power_count    - Success
                      GPUFREQ_EINVAL - Failure
                      GPUFREQ_ENOENT - Null implementation
 Description        : Control power state of whole MFG system
************************************************************************************/
int gpufreq_power_control(enum gpufreq_power_state power)
{
	int ret = GPUFREQ_ENOENT;

	GPUFREQ_TRACE_START("power=%d", power);

	if (!gpufreq_power_ctrl_enable()) {
		GPUFREQ_LOGD("power control is disabled");
		ret = GPUFREQ_SUCCESS;
		goto done;
	}

	if (gpufreq_fp && gpufreq_fp->power_control)
		ret = gpufreq_fp->power_control(power);
	else
		GPUFREQ_LOGE("null gpufreq platform function pointer (ENOENT)");

	if (unlikely(ret < 0))
		GPUFREQ_LOGE("fail to control power state: %s (%d)",
			power ? "POWER_ON" : "POWER_OFF",
			ret);

done:
	GPUFREQ_TRACE_END();

	return ret;
}
EXPORT_SYMBOL(gpufreq_power_control);

/***********************************************************************************
 Function Name      : gpufreq_commit
 Inputs             : target          - Target of GPU DVFS (GPU, STACK, DEFAULT)
                      oppidx          - Working OPP index of the target
 Outputs            : -
 Returns            : GPUFREQ_SUCCESS - Success
                      GPUFREQ_EINVAL  - Failure
                      GPUFREQ_ENOENT  - Null implementation
 Description        : Commit DVFS request to the target with working OPP index
                      It will be constrained by the limit recorded in GPU PPM
************************************************************************************/
int gpufreq_commit(enum gpufreq_target target, int oppidx)
{
	int ret = GPUFREQ_ENOENT;

	GPUFREQ_TRACE_START("target=%d, oppidx=%d", target, oppidx);

	if (target >= TARGET_INVALID || target < 0 ||
		(target == TARGET_STACK && !g_dual_buck)) {
		GPUFREQ_LOGE("invalid OPP target: %d (EINVAL)", target);
		ret = GPUFREQ_EINVAL;
		goto done;
	}

	if (target == TARGET_DEFAULT) {
		if (g_dual_buck)
			target = TARGET_STACK;
		else
			target = TARGET_GPU;
	}

	GPUFREQ_LOGD("target: %s, oppidx: %d",
		target == TARGET_STACK ? "STACK" : "GPU",
		oppidx);

	if (target == TARGET_STACK && gpuppm_fp && gpuppm_fp->limited_commit_stack)
		ret = gpuppm_fp->limited_commit_stack(oppidx);
	else if (target == TARGET_GPU && gpuppm_fp && gpuppm_fp->limited_commit_gpu)
		ret = gpuppm_fp->limited_commit_gpu(oppidx);
	else
		GPUFREQ_LOGE("null gpuppm platform function pointer (ENOENT)");

	if (unlikely(ret))
		GPUFREQ_LOGE("fail to commit %s OPP index: %d (%d)",
			target == TARGET_STACK ? "STACK" : "GPU",
			oppidx, ret);

done:
	GPUFREQ_TRACE_END();

	return ret;
}
EXPORT_SYMBOL(gpufreq_commit);

/***********************************************************************************
 Function Name      : gpufreq_set_limit
 Inputs             : target          - Target of GPU DVFS (GPU, STACK, DEFAULT)
                      limiter         - Pre-defined user that set limit to GPU DVFS
                      ceiling_info    - Upper limit info (oppidx, freq, volt, power, ...)
                      floor_info      - Lower limit info (oppidx, freq, volt, power, ...)
 Outputs            : -
 Returns            : GPUFREQ_SUCCESS - Success
                      GPUFREQ_EINVAL  - Failure
                      GPUFREQ_ENOENT  - Null implementation
 Description        : Set ceiling and floor limit to GPU DVFS by specified limiter
                      It will immediately trigger DVFS if current OPP violates limit
************************************************************************************/
int gpufreq_set_limit(enum gpufreq_target target,
	enum gpuppm_limiter limiter, int ceiling_info, int floor_info)
{
	int ret = GPUFREQ_ENOENT;

	GPUFREQ_TRACE_START("target=%d, limiter=%d, ceiling_info=%d, floor_info=%d",
		target, limiter, ceiling_info, floor_info);

	if (target >= TARGET_INVALID || target < 0 ||
		(target == TARGET_STACK && !g_dual_buck)) {
		GPUFREQ_LOGE("invalid OPP target: %d (EINVAL)", target);
		ret = GPUFREQ_EINVAL;
		goto done;
	}

	if (target == TARGET_DEFAULT) {
		if (g_dual_buck)
			target = TARGET_STACK;
		else
			target = TARGET_GPU;
	}

	GPUFREQ_LOGD("target: %s, limiter: %d, ceiling_info: %d, floor_info: %d",
		target == TARGET_STACK ? "STACK" : "GPU",
		limiter, ceiling_info, floor_info);

	if (target == TARGET_STACK && gpuppm_fp && gpuppm_fp->set_limit_stack)
		ret = gpuppm_fp->set_limit_stack(limiter, ceiling_info, floor_info);
	else if (target == TARGET_GPU && gpuppm_fp && gpuppm_fp->set_limit_gpu)
		ret = gpuppm_fp->set_limit_gpu(limiter, ceiling_info, floor_info);
	else
		GPUFREQ_LOGE("null gpuppm platform function pointer (ENOENT)");

	if (unlikely(ret))
		GPUFREQ_LOGE("fail to set %s limiter: %d ceiling_info: %d, floor_info: %d (%d)",
			target == TARGET_STACK ? "STACK" : "GPU",
			limiter, ceiling_info, floor_info, ret);

done:
	GPUFREQ_TRACE_END();

	return ret;
}
EXPORT_SYMBOL(gpufreq_set_limit);

/***********************************************************************************
 Function Name      : gpufreq_get_cur_limit_idx
 Inputs             : target    - Target of GPU DVFS (GPU, STACK, DEFAULT)
                      limit     - Type of limit (GPUPPM_CEILING, GPUPPM_FLOOR)
 Outputs            : -
 Returns            : limit_idx - Working OPP index of ceiling or floor
 Description        : Query current working OPP index of ceiling or floor limit
************************************************************************************/
int gpufreq_get_cur_limit_idx(enum gpufreq_target target, enum gpuppm_limit_type limit)
{
	int limit_idx = -1;

	if (target >= TARGET_INVALID || target < 0 ||
		(target == TARGET_STACK && !g_dual_buck)) {
		GPUFREQ_LOGE("invalid OPP target: %d (EINVAL)", target);
		goto done;
	}

	if (limit >= GPUPPM_INVALID || limit < 0) {
		GPUFREQ_LOGE("invalid limit target: %d (EINVAL)", limit);
		goto done;
	}

	if (target == TARGET_DEFAULT) {
		if (g_dual_buck)
			target = TARGET_STACK;
		else
			target = TARGET_GPU;
	}

	if (target == TARGET_STACK) {
		if (limit == GPUPPM_CEILING && gpuppm_fp && gpuppm_fp->get_ceiling_stack)
			limit_idx = gpuppm_fp->get_ceiling_stack();
		else if (limit == GPUPPM_FLOOR && gpuppm_fp && gpuppm_fp->get_floor_stack)
			limit_idx = gpuppm_fp->get_floor_stack();
		else
			GPUFREQ_LOGE("null gpuppm platform function pointer (ENOENT)");
	} else if (target == TARGET_GPU) {
		if (limit == GPUPPM_CEILING && gpuppm_fp && gpuppm_fp->get_ceiling_gpu)
			limit_idx = gpuppm_fp->get_ceiling_gpu();
		else if (limit == GPUPPM_FLOOR && gpuppm_fp && gpuppm_fp->get_floor_gpu)
			limit_idx = gpuppm_fp->get_floor_gpu();
		else
			GPUFREQ_LOGE("null gpuppm platform function pointer (ENOENT)");
	}

	GPUFREQ_LOGD("target: %s, current %s index: %d",
		target == TARGET_STACK ? "STACK" : "GPU",
		limit == GPUPPM_CEILING ? "ceiling" : "floor",
		limit_idx);

done:
	return limit_idx;
}
EXPORT_SYMBOL(gpufreq_get_cur_limit_idx);

/***********************************************************************************
 Function Name      : gpufreq_get_cur_limiter
 Inputs             : target  - Target of GPU DVFS (GPU, STACK, DEFAULT)
                      limit   - Type of limit (GPUPPM_CEILING, GPUPPM_FLOOR)
 Outputs            : -
 Returns            : limiter - Pre-defined user that set limit to GPU DVFS
 Description        : Query which user decide current ceiling and floor OPP index
************************************************************************************/
unsigned int gpufreq_get_cur_limiter(enum gpufreq_target target, enum gpuppm_limit_type limit)
{
	unsigned int limiter = 0;

	if (target >= TARGET_INVALID || target < 0 ||
		(target == TARGET_STACK && !g_dual_buck)) {
		GPUFREQ_LOGE("invalid OPP target: %d (EINVAL)", target);
		goto done;
	}

	if (limit >= GPUPPM_INVALID || limit < 0) {
		GPUFREQ_LOGE("invalid limit target: %d (EINVAL)", limit);
		goto done;
	}

	if (target == TARGET_DEFAULT) {
		if (g_dual_buck)
			target = TARGET_STACK;
		else
			target = TARGET_GPU;
	}

	if (target == TARGET_STACK) {
		if (limit == GPUPPM_CEILING && gpuppm_fp && gpuppm_fp->get_c_limiter_stack)
			limiter = gpuppm_fp->get_c_limiter_stack();
		else if (limit == GPUPPM_FLOOR && gpuppm_fp && gpuppm_fp->get_f_limiter_stack)
			limiter = gpuppm_fp->get_f_limiter_stack();
		else
			GPUFREQ_LOGE("null gpuppm platform function pointer (ENOENT)");
	} else if (target == TARGET_GPU) {
		if (limit == GPUPPM_CEILING && gpuppm_fp && gpuppm_fp->get_c_limiter_gpu)
			limiter = gpuppm_fp->get_c_limiter_gpu();
		else if (limit == GPUPPM_FLOOR && gpuppm_fp && gpuppm_fp->get_f_limiter_gpu)
			limiter = gpuppm_fp->get_f_limiter_gpu();
		else
			GPUFREQ_LOGE("null gpuppm platform function pointer (ENOENT)");
	}

	GPUFREQ_LOGD("target: %s, current %s limiter: %d",
		target == TARGET_STACK ? "STACK" : "GPU",
		limit == GPUPPM_CEILING ? "ceiling" : "floor",
		limiter);

done:
	return limiter;
}
EXPORT_SYMBOL(gpufreq_get_cur_limiter);

#if IS_ENABLED(CONFIG_MTK_BATTERY_OC_POWER_THROTTLING)
static void gpufreq_batt_oc_callback(enum BATTERY_OC_LEVEL_TAG batt_oc_level)
{
	int ret = GPUFREQ_ENOENT;

	ret = gpufreq_set_limit(TARGET_DEFAULT, LIMIT_BATT_OC, batt_oc_level, GPUPPM_KEEP_IDX);
	if (unlikely(ret))
		GPUFREQ_LOGE("fail to set LIMIT_BATT_OC limit level: %d (%d)",
			batt_oc_level, ret);
}
#endif /* CONFIG_MTK_BATTERY_OC_POWER_THROTTLING */

#if IS_ENABLED(CONFIG_MTK_BATTERY_PERCENT_THROTTLING)
static void gpufreq_batt_percent_callback(enum BATTERY_PERCENT_LEVEL_TAG batt_percent_level)
{
	int ret = GPUFREQ_ENOENT;

	ret = gpufreq_set_limit(TARGET_DEFAULT, LIMIT_BATT_PERCENT,
		batt_percent_level, GPUPPM_KEEP_IDX);
	if (unlikely(ret))
		GPUFREQ_LOGE("fail to set LIMIT_BATT_PERCENT limit level: %d (%d)",
			batt_percent_level, ret);
}
#endif /* CONFIG_MTK_BATTERY_PERCENT_THROTTLING */

#if IS_ENABLED(CONFIG_MTK_LOW_BATTERY_POWER_THROTTLING)
static void gpufreq_low_batt_callback(enum LOW_BATTERY_LEVEL_TAG low_batt_level)
{
	int ret = GPUFREQ_ENOENT;

	ret = gpufreq_set_limit(TARGET_DEFAULT, LIMIT_LOW_BATT, low_batt_level, GPUPPM_KEEP_IDX);
	if (unlikely(ret))
		GPUFREQ_LOGE("fail to set LIMIT_LOW_BATT limit level: %d (%d)",
			low_batt_level, ret);
}
#endif /* CONFIG_MTK_LOW_BATTERY_POWER_THROTTLING */

static void gpufreq_init_external_callback(void)
{
#if IS_ENABLED(CONFIG_MTK_PBM)
	struct pbm_gpu_callback_table pbm_cb = {
		.get_max_pb = gpufreq_get_max_power,
		.get_min_pb = gpufreq_get_min_power,
		.get_cur_pb = gpufreq_get_cur_power,
		.get_cur_vol = gpufreq_get_cur_volt,
		.get_opp_by_pb = gpufreq_get_oppidx_by_power,
		.set_limit = gpufreq_set_limit,
	};
#endif /* CONFIG_MTK_PBM */

	/* hook GPU HAL callback */
	mtk_get_gpu_limit_index_fp = gpufreq_get_cur_limit_idx;
	mtk_get_gpu_limiter_fp = gpufreq_get_cur_limiter;
	mtk_get_gpu_cur_freq_fp = gpufreq_get_cur_freq;
	mtk_get_gpu_cur_oppidx_fp = gpufreq_get_cur_oppidx;

	/* register PBM callback */
#if IS_ENABLED(CONFIG_MTK_PBM)
	register_pbm_gpu_notify(&pbm_cb);
#endif /* CONFIG_MTK_PBM */

	/* register power throttling callback */
#if IS_ENABLED(CONFIG_MTK_LOW_BATTERY_POWER_THROTTLING)
	register_low_battery_notify(&gpufreq_low_batt_callback, LOW_BATTERY_PRIO_GPU);
#endif /* CONFIG_MTK_LOW_BATTERY_POWER_THROTTLING */

#if IS_ENABLED(CONFIG_MTK_BATTERY_PERCENT_THROTTLING)
	register_bp_thl_notify(&gpufreq_batt_percent_callback, BATTERY_PERCENT_PRIO_GPU);
#endif /* CONFIG_MTK_BATTERY_PERCENT_THROTTLING */

#if IS_ENABLED(CONFIG_MTK_BATTERY_OC_POWER_THROTTLING)
	register_battery_oc_notify(&gpufreq_batt_oc_callback, BATTERY_OC_PRIO_GPU);
#endif /* CONFIG_MTK_BATTERY_OC_POWER_THROTTLING */
}

void gpufreq_register_gpufreq_fp(struct gpufreq_platform_fp *platform_fp)
{
	if (!platform_fp) {
		GPUFREQ_LOGE("null gpufreq platform function pointer (EINVAL)");
		return;
	}

	gpufreq_fp = platform_fp;
}
EXPORT_SYMBOL(gpufreq_register_gpufreq_fp);

void gpufreq_register_gpuppm_fp(struct gpuppm_platform_fp *platform_fp)
{
	if (!platform_fp) {
		GPUFREQ_LOGE("null gpuppm platform function pointer (EINVAL)");
		return;
	}

	gpuppm_fp = platform_fp;

	/*
	 * register external callback function at here
	 * because platform impl. is ready after PPM func. pointer register done
	 */
	gpufreq_init_external_callback();
}
EXPORT_SYMBOL(gpufreq_register_gpuppm_fp);

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

	ret = of_property_read_u32(wrapper, "dual-buck", &g_dual_buck);
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to read dual-buck (%d)", ret);
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
		GPUFREQ_LOGE("fail to read gpueb-support (%d)", ret);
		goto done;
	}

	GPUFREQ_LOGI("gpufreq wrapper driver probe done, dual_buck: %s, gpueb: %s",
		g_dual_buck ? "true" : "false",
		g_gpueb_support ? "on" : "off");

done:
	return ret;
}

static int __init gpufreq_wrapper_init(void)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_LOGI("start to init gpufreq wrapper driver");

	/* register platform driver */
	ret = platform_driver_register(&g_gpufreq_wrapper_pdrv);
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to register gpufreq wrapper driver (%d)",
			ret);
		goto done;
	}

	ret = gpufreq_debug_init(g_dual_buck, g_gpueb_support);
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to init gpufreq debug (%d)", ret);
		goto done;
	}

	GPUFREQ_LOGI("gpufreq wrapper driver init done");

done:
	return ret;
}

static void __exit gpufreq_wrapper_exit(void)
{
	platform_driver_unregister(&g_gpufreq_wrapper_pdrv);
}

module_init(gpufreq_wrapper_init);
module_exit(gpufreq_wrapper_exit);

MODULE_DEVICE_TABLE(of, g_gpufreq_wrapper_of_match);
MODULE_DESCRIPTION("MediaTek GPU-DVFS wrapper driver");
MODULE_LICENSE("GPL");
