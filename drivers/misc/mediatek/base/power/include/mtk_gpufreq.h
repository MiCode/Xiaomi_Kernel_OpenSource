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

#ifndef _MTK_GPUFREQ_H_
#define _MTK_GPUFREQ_H_

#if defined(CONFIG_MACH_MT6885) \
	|| defined(CONFIG_MACH_MT6873) \
	|| defined(CONFIG_MACH_MT6853) \
	|| defined(CONFIG_MACH_MT6893)
#include "gpufreq_v1/mtk_gpufreq.h"
#elif defined(CONFIG_MACH_MT6785)
#include "../mt6785/mtk_gpufreq.h"
#elif defined(CONFIG_MACH_MT6771)
#include "../mt6771/mtk_gpufreq.h"
#elif defined(CONFIG_MACH_MT6765)
#include "../mt6765/mtk_gpufreq.h"
#elif defined(CONFIG_MACH_MT6768)
#include "../mt6768/mtk_gpufreq.h"
#elif defined(CONFIG_MACH_MT6739)
#include "../mt6739/mtk_gpufreq.h"
#endif

#endif /* _MTK_GPUFREQ_H_ */
