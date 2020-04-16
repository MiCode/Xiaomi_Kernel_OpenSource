/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __MTK_CPUFREQ_COMMON_API_H__
#define __MTK_CPUFREQ_COMMON_API_H__

#if defined(CONFIG_MACH_MT6758)
#include "mt6758/include/mach/mtk_cpufreq_api.h"
#endif

#if defined(CONFIG_MACH_MT6765)
#include "mt6765/include/mach/mtk_cpufreq_api.h"
#endif

#if defined(CONFIG_MACH_MT6761)
#include "mt6761/include/mach/mtk_cpufreq_api.h"
#endif

#if defined(CONFIG_MACH_MT3967)
#include "mt3967/include/mach/mtk_cpufreq_api.h"
#endif

#if defined(CONFIG_MACH_MT6779)
#include "mt6779/include/mach/mtk_cpufreq_api.h"
#endif

#if defined(CONFIG_MACH_MT6739)
#include "mt6739/include/mach/mtk_cpufreq_api.h"
#endif

#if defined(CONFIG_MACH_MT6771)
#include "mt6771/include/mach/mtk_cpufreq_api.h"
#endif

#if defined(CONFIG_MACH_MT6785)
#include "mt6785/include/mach/mtk_cpufreq_api.h"
#endif

#if defined(CONFIG_MACH_MT6885)
#include "mt6885/include/mach/mtk_cpufreq_api.h"
#endif

#if defined(CONFIG_MACH_MT6768)
#include "mt6768/include/mach/mtk_cpufreq_api.h"
#endif

#endif	/* __MTK_CPUFREQ_COMMON_API_H__ */
