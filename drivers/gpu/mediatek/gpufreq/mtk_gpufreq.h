/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_GPUFREQ_H__
#define __MTK_GPUFREQ_H__

#if defined(CONFIG_MTK_GPUFREQ_V2)
#include <v2/include/gpufreq_v2.h>
#include <v2/include/gpu_misc.h>
#else
#include <v1/include/mtk_gpufreq_v1.h>
#endif

#endif /* __MTK_GPUFREQ_H__ */
