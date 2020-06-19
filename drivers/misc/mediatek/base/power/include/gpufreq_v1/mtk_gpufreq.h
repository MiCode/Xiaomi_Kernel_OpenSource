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

#ifndef __MTK_GPUFREQ_H__
#define __MTK_GPUFREQ_H__

#if defined(CONFIG_MACH_MT6885)
#include "mt6885/mtk_gpufreq_plat.h"

#elif defined(CONFIG_MACH_MT6893)
#include "mt6893/mtk_gpufreq_plat.h"

#elif defined(CONFIG_MACH_MT6873)
#include "mt6873/mtk_gpufreq_plat.h"

#elif defined(CONFIG_MACH_MT6853)
#include "mt6853/mtk_gpufreq_plat.h"

#endif

#endif /* __MTK_GPUFREQ_H__ */

