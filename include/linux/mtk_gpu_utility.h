/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __MTK_GPU_UTILITY_H__
#define __MTK_GPU_UTILITY_H__

#include <linux/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* returning false indicated no implement */

/* unit: x bytes */
bool mtk_get_gpu_memory_usage(unsigned int *pMemUsage);

/* unit: 0~100 % */
bool mtk_get_gpu_loading(unsigned int *pLoading);
bool mtk_get_gpu_block(unsigned int *pBlock);
bool mtk_get_gpu_idle(unsigned int *pIlde);

bool mtk_get_gpu_GP_loading(unsigned int *pLoading);
bool mtk_get_gpu_PP_loading(unsigned int *pLoading);
bool mtk_get_gpu_power_loading(unsigned int *pLoading);

bool mtk_enable_gpu_dvfs_timer(bool bEnable);
bool mtk_boost_gpu_freq(void);
bool mtk_set_bottom_gpu_freq(unsigned int ui32FreqLevel);

unsigned int mt_gpufreq_get_cur_freq(void);
unsigned int mt_gpufreq_get_cur_volt(void);

extern unsigned int (*mtk_get_gpu_memory_usage_fp)(void);
extern unsigned int (*mtk_get_gpu_loading_fp)(void);
extern unsigned int (*mtk_get_gpu_block_fp)(void);
extern unsigned int (*mtk_get_gpu_idle_fp)(void);
extern unsigned int (*mtk_get_gpu_freq_fp)(void);
extern unsigned int (*mtk_get_gpu_power_loading_fp)(void);
extern void         (*mtk_enable_gpu_dvfs_timer_fp)(bool bEnable);
extern void         (*mtk_boost_gpu_freq_fp)(void);
extern void         (*mtk_set_bottom_gpu_freq_fp)(unsigned int);
extern unsigned int (*mtk_custom_get_gpu_freq_level_count_fp)(void);
extern void         (*mtk_custom_boost_gpu_freq_fp)(unsigned int ui32FreqLevel);
extern void         (*mtk_custom_upbound_gpu_freq_fp)(unsigned int ui32FreqLevel);
extern unsigned int (*mtk_get_custom_boost_gpu_freq_fp)(void);
extern unsigned int (*mtk_get_custom_upbound_gpu_freq_fp)(void);

#ifdef __cplusplus
}
#endif

#endif
