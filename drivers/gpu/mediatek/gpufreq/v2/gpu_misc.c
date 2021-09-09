// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

/**
 * @file    gpu_misc.c
 * @brief   Function that called by GPU ddk
 */

/**
 * ===============================================
 * Include
 * ===============================================
 */

#include <gpufreq_v2.h>
#include <gpu_misc.h>

#ifndef CREATE_TRACE_POINTS
#define CREATE_TRACE_POINTS
#endif
#include <gpu_hardstop.h>


/**
 * ===============================================
 * Local variables definition
 * ===============================================
 */

static struct gpudfd_platform_fp *gpudfd_fp;


/**
 * ===============================================
 * External Function Definition
 * ===============================================
 */

void gpufreq_hardstop_dump_slog(void)
{

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
	unsigned int gpu_freq;
	unsigned int gpu_volt;
	unsigned int gpu_vsram;
	unsigned int stack_freq;
	unsigned int stack_volt;
	unsigned int stack_vsram;

	gpu_freq = gpufreq_get_cur_freq(TARGET_GPU);
	gpu_volt = gpufreq_get_cur_volt(TARGET_GPU);
	gpu_vsram = gpufreq_get_cur_vsram(TARGET_GPU);
	stack_freq = gpufreq_get_cur_freq(TARGET_STACK);
	stack_volt = gpufreq_get_cur_volt(TARGET_STACK);
	stack_vsram = gpufreq_get_cur_vsram(TARGET_STACK);

	trace_gpu_hardstop("gpuexp", "hs",
		gpu_freq,
		gpu_volt,
		gpu_vsram,
		stack_freq,
		stack_volt,
		stack_vsram);
#endif
}
EXPORT_SYMBOL(gpufreq_hardstop_dump_slog);

unsigned int gpufreq_get_dfd_force_dump_mode(void)
{
	if (gpudfd_fp && gpudfd_fp->get_dfd_force_dump_mode)
		return gpudfd_fp->get_dfd_force_dump_mode();
	else
		return GPUFREQ_ENOENT;
}
EXPORT_SYMBOL(gpufreq_get_dfd_force_dump_mode);

unsigned int gpufreq_set_dfd_force_dump_mode(unsigned int mode)
{
	if (gpudfd_fp && gpudfd_fp->set_dfd_force_dump_mode)
		gpudfd_fp->set_dfd_force_dump_mode(mode);
	else
		return GPUFREQ_ENOENT;

	return 0;
}
EXPORT_SYMBOL(gpufreq_set_dfd_force_dump_mode);

void gpufreq_config_dfd(unsigned int enable)
{
	if (gpudfd_fp && gpudfd_fp->config_dfd)
		gpudfd_fp->config_dfd(enable);
}
EXPORT_SYMBOL(gpufreq_config_dfd);

void gpu_misc_register_gpudfd_fp(struct gpudfd_platform_fp *dfd_platform_fp)
{
	if (!dfd_platform_fp) {
		GPUFREQ_LOGE("null gpudfd platform function pointer (EINVAL)");
		return;
	}

	gpudfd_fp = dfd_platform_fp;
}
EXPORT_SYMBOL(gpu_misc_register_gpudfd_fp);
