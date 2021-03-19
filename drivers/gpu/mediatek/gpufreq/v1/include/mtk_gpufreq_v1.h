/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_GPUFREQ_V1_H__
#define __MTK_GPUFREQ_V1_H__

#if defined(CONFIG_GPU_MT6885)
#include "mt6885/mtk_gpufreq_plat.h"

#elif defined(CONFIG_GPU_MT6893)
#include "mt6893/mtk_gpufreq_plat.h"

#elif defined(CONFIG_GPU_MT6873)
#include "mt6873/mtk_gpufreq_plat.h"

#elif defined(CONFIG_GPU_MT6853)
#include "mt6853/mtk_gpufreq_plat.h"
#endif

#endif /* __MTK_GPUFREQ_V1_H__ */

