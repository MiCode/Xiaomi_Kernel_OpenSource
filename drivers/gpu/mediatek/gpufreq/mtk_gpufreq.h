/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_GPUFREQ_H__
#define __MTK_GPUFREQ_H__
#if defined(CONFIG_MACH_MT6781) || defined(CONFIG_MACH_MT6833) || defined(CONFIG_MACH_MT6877) || defined(CONFIG_MACH_MT6893)
#include <v1/include/mtk_gpufreq_v1.h>
#elif defined(CONFIG_MACH_MT6768)
#include <mt6768/mtk_gpufreq.h>
#else
#include <include/mtk_gpufreq.h>
#endif

#endif /* __MTK_GPUFREQ_H__ */
