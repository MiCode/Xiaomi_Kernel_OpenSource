/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_GPUFREQ_INTERNAL_H__
#define __MTK_GPUFREQ_INTERNAL_H__

#if defined(CONFIG_MACH_MT6885)
#include "mt6885/mtk_gpufreq_internal_plat.h"

#elif defined(CONFIG_MACH_MT6893)
#include "mt6893/mtk_gpufreq_internal_plat.h"

#elif defined(CONFIG_MACH_MT6873)
#include "mt6873/mtk_gpufreq_internal_plat.h"

#elif defined(CONFIG_MACH_MT6853)
#include "mt6853/mtk_gpufreq_internal_plat.h"

#elif defined(CONFIG_MACH_MT6833)
#include "mt6833/mtk_gpufreq_internal_plat.h"

#elif defined(CONFIG_MACH_MT6877)
#include "mt6877/mtk_gpufreq_internal_plat.h"

#elif defined(CONFIG_MACH_MT6781)
#include "mt6781/mtk_gpufreq_internal_plat.h"

#endif

#endif /* __MTK_GPUFREQ_INTERNAL_H__ */

