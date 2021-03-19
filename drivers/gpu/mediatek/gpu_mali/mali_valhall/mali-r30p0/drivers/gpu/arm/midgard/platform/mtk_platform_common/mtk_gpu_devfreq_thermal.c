// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <mali_kbase.h>
#include <platform/mtk_platform_common.h>
#include "mtk_gpu_devfreq_thermal.h"
#include <mtk_gpufreq.h>

unsigned long mtk_common_get_static_power(struct devfreq *df,
                                          unsigned long voltage /* mV */)
{
	(void)(voltage);
	return mt_gpufreq_get_leakage_mw();
}

unsigned long mtk_common_get_dynamic_power(struct devfreq *df,
                                           unsigned long freq /* Hz */,
                                           unsigned long voltage /* mV */)
{
	return mt_gpufreq_get_dyn_power(freq / 1000, voltage * 100);
}

struct devfreq_cooling_power mtk_common_cooling_power_ops = {
	.get_static_power = &mtk_common_get_static_power,
	.get_dynamic_power = &mtk_common_get_dynamic_power,
};
